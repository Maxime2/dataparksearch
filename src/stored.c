/* Copyright (C) 2013 Maxim Zakharov. All right reserved.
   Copyright (C) 2003-2012 DataPark Ltd. All right reserved.
   Copyright (C) 2000-2002 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA 
*/
#include "dps_common.h"
#include "dps_utils.h"
#include "dps_env.h"
#include "dps_agent.h"
#include "dps_conf.h"
#include "dps_log.h"
#include "dps_services.h"
#include "dps_store.h"
#include "dps_vars.h"
#include "dps_xmalloc.h"
#include "dps_vars.h"
#include "dps_parsehtml.h"
#include "dps_searchtool.h"
#include "dps_utils.h"
#include "dps_cache.h"
#include "dps_signals.h"
#include "dps_base.h"
#include "dps_socket.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_ZLIB
#include <zlib.h>

#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <math.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif


#define DPS_STORED_MAXCLNTS 128

static char time_pid[100];
static DPS_ENV * Conf = NULL;
static DPS_AGENT * Agent = NULL;
static int verb = DPS_LOG_EXTRA;
static int DpsARGC;
static char **DpsARGV;
static int nport;
static char var_dir[PATH_MAX] = DPS_VAR_DIR DPSSLASHSTR;
static int demonize = 1;


static char * time_pid_info(void){
        time_t t = time(NULL);
#ifdef HAVE_PTHREAD
	struct tm l_tim;
	struct tm *tim = localtime_r(&t, &l_tim);
#else
	struct tm *tim = localtime(&t);
#endif
	strftime(time_pid,sizeof(time_pid),"%a %d %H:%M:%S",tim);
	sprintf(time_pid+dps_strlen(time_pid)," [%d]",(int)getpid());
	return(time_pid);
}

/********************* SIG Handlers ****************/

static void sighandler(int sign);
static void Optimize_sighandler(int sign);

static void init_signals(void){
	/* Set up signals handler*/
	DpsSignal(SIGPIPE, sighandler);
	DpsSignal(SIGCHLD, sighandler);
	DpsSignal(SIGALRM, sighandler);
	DpsSignal(SIGUSR1, sighandler);
	DpsSignal(SIGUSR2, sighandler);
	DpsSignal(SIGHUP, sighandler);
	DpsSignal(SIGINT, sighandler);
	DpsSignal(SIGTERM, sighandler);
}
static void Optimize_init_signals(void){
	/* Set up signals handler*/
	DpsSignal(SIGPIPE, Optimize_sighandler);
	DpsSignal(SIGCHLD, Optimize_sighandler);
	DpsSignal(SIGALRM, Optimize_sighandler);
	DpsSignal(SIGUSR1, Optimize_sighandler);
	DpsSignal(SIGUSR2, Optimize_sighandler);
	DpsSignal(SIGHUP, Optimize_sighandler);
	DpsSignal(SIGINT, Optimize_sighandler);
	DpsSignal(SIGTERM, Optimize_sighandler);
}

static void sighandler(int sign){
#ifdef UNIONWAIT
	union wait status;
#else
	int status;
#endif

	switch(sign){
		case SIGPIPE:
		  have_sigpipe = 1;
		  break;
		case SIGHUP:
		  have_sighup = 1;
		  break;
	        case SIGCHLD:
		  while (waitpid(-1, &status, WNOHANG) > 0);
		  break;
		case SIGINT:
		  have_sigint=1;
		  break;
		case SIGTERM:
		  have_sigterm=1;
		  break;
	        case SIGALRM:
		  _exit(0);
		  break;
		case SIGUSR1:
		  have_sigusr1 = 1;
		  break;
		case SIGUSR2:
		  have_sigusr2 = 1;
		  break;
		default: ;
	}
}

static void Optimize_sighandler(int sign){
#ifdef UNIONWAIT
	union wait status;
#else
	int status;
#endif

	switch(sign){
		case SIGPIPE:
		  have_sigpipe = 1;
		  break;
		case SIGHUP:
		  have_sighup = 1;
		  break;
	        case SIGCHLD:
		  wait(&status);
		  break;
		case SIGINT:
		  have_sigint = 1;
		  break;
		case SIGTERM:
		  have_sigterm = 1;
		  break;
	        case SIGALRM:
		  have_sigalrm = 1;
		  break;
		case SIGUSR1:
		  have_sigusr1 = 1;
		  break;
		case SIGUSR2:
		  have_sigusr2 = 1;
		  break;
		default: ;
	}
}




size_t  DocSize = 0, filenamelen;
Byte *Doc = NULL, *CDoc = NULL;
int ns, sd, StoredFiles, OptimizeInterval, OptimizeRatio;
DPS_BASE_PARAM P;


static void Optimize(DPS_ENV *C) {
  int current_base = 0;

	Optimize_init_signals();
	DpsLog(Agent, DPS_LOG_INFO, "Optimization child started.");

	/* To see the URL being indexed in "ps" output on xBSD */
	dps_setproctitle("base optimizer. Interval: %d", OptimizeInterval);

	alarm((unsigned int) OptimizeInterval); /* try optimize one base every OptimizeInterval sec. */
	while(1) {
	  sleep((unsigned)OptimizeInterval);
/*	  DpsLog(Agent, DPS_LOG_INFO, "int:%d  term:%d pipe:%d  usr1:%d  usr2:%d  alrm:%d",
		 have_sigint, have_sigterm, have_sigpipe, have_sigusr1, have_sigusr2, have_sigalrm);*/
		if(have_sigint){
			DpsLog(Agent, verb, "Optimize: SIGINT arrived");
			have_sigint = 0;
			break;
		}
		if(have_sigterm){
			DpsLog(Agent, verb, "Optimize: SIGTERM arrived");
			have_sigterm = 0;
			break;
		}
		if(have_sigpipe){
			DpsLog(Agent,verb,"Optimize: SIGPIPE arrived. Broken pipe !");
			have_sigpipe = 0;
			break;
		}
		if (have_sigusr1) {
		  DpsIncLogLevel(Agent);
		  have_sigusr1 = 0;
		}
		if (have_sigusr2) {
		  DpsDecLogLevel(Agent);
		  have_sigusr2 = 0;
		}
		if (have_sigalrm) {
			bzero(&P, sizeof(P));
			P.subdir = "store";
			P.basename = "doc";
			P.indname = "doc";
			P.rec_id = current_base;
			P.mode = DPS_WRITE_LOCK;
			P.NFiles = (size_t)DpsVarListFindUnsigned(&C->Vars, "StoredFiles", 0x100);
			P.vardir = DpsVarListFindStr(&C->Vars, "VarDir", DPS_VAR_DIR);
			P.A = Agent;

/*			if (!Agent->Flags.OptimizeAtUpdate) {*/
			  if (DpsBaseOptimize(&P, current_base) != DPS_OK) {
			    DpsSend(ns, &DocSize, sizeof(DocSize), 0); 
			    dps_closesocket(ns);
			    DpsBaseClose(&P); 
			    exit(0);
			  }
/*			}*/
			DpsBaseClose(&P);
			current_base++;
			current_base %= P.NFiles;
			have_sigalrm = 0;
			alarm((unsigned int) OptimizeInterval);
		}
	}
}

static void usage(void){

  fprintf(stderr, "\nstored from %s-%s-%s\n(C)1998-2003, LavTech Corp.\
\n(C)2003-2007, Datapark Corp.\n\nUsage: stored [OPTIONS] [configfile]\n\nOptions are:\
\n\t-w /path      choose alternative working /var directory\n\
\t-p xxx        listen port xxx\n\
\t-v n          verbose level, 0-5\n\
\t-f            run foreground, don't demonize\n\
\t-h,-?         print this help page and exit\n\n",  
	  PACKAGE,VERSION,DPS_DBTYPE);
  
  return;
}

static void DpsParseCmdLine(void) {
  int ch;

	while ((ch = getopt(DpsARGC, DpsARGV, "fhv:w:p:?")) != -1){
		switch (ch) {
			case 'p':
				nport = atoi(optarg);
				break;
		        case 'v': DpsSetLogLevel(NULL, atoi(optarg)); break;
			case 'w':
			  dps_strncpy(var_dir, optarg, PATH_MAX);
			  var_dir[PATH_MAX-1] = '\0';
				break;
		        case 'l':
			        Conf->logs_only = 1;
				break;
		        case 'f':
			        demonize = 0;
				break;
			case 'h':
			case '?':
			default:
				usage();
				exit(1);
				break;
		}
	}
}


#endif


int main(int argc, char **argv, char **envp) {

#ifdef HAVE_ZLIB

        fd_set mask;
	struct sockaddr_in server_addr, client_addr;
	const char *config_name= DPS_CONF_DIR "/stored.conf";
	int pid, pid_fd, s, on = 1;
	char pidbuf[1024];
	char buf[1024];
	const char *lstn;
	pid_t opt_pid;

	log2stderr = 1;
	nport = DPS_STORED_PORT;

	DpsARGC = argc;

	DpsARGV = (char**)DpsXmalloc((size_t)(argc + 1) * sizeof(char*));
	if (DpsARGV == NULL) {
	  fprintf(stderr, "Can't allocate DpsARGV\n");
	  exit(-1);
	}
	{
	  size_t i;
	  for (i = 0; i < (size_t)argc; i++) {
	    if ((DpsARGV[i] = DpsStrdup(argv[i])) == NULL) {
	      fprintf(stderr, "Can't duplicate DpsARGV[%d]\n", (int)i);
	      exit(-1);
	    }
	  }
	  DpsARGV[DpsARGC = argc] = NULL;
	}

	DpsParseCmdLine();

	argc -= optind;argv += optind;
	
	if(argc > 1){
		usage();
		exit(1);
	} else if (argc == 1) {
	        config_name = argv[0];
	}

	if (demonize) {
	  if (dps_demonize() != 0) {
	    fprintf(stderr, "Can't demonize");
	    exit(1);
	  }
	}

	/* Check that another instance isn't running */
	/* and create PID file.                      */

	sprintf(dps_pid_name,"%s/%s", var_dir, "stored.pid");
	pid_fd = DpsOpen3(dps_pid_name, O_CREAT | O_EXCL | O_WRONLY, 0644);
	if(pid_fd < 0){

	  dps_strerror(NULL, 0, "Can't create '%s'", dps_pid_name);
	  if(errno == EEXIST){
	    int other_pid = 0;
	    pid_fd = DpsOpen3(dps_pid_name, O_RDWR, 0644);
	    if (pid_fd < 0) {
	      dps_strerror(NULL, 0, "Can't open '%s'", dps_pid_name);
	      exit(1);
	    }
	    (void)read(pid_fd, pidbuf, sizeof(pidbuf));
	    if (1 > sscanf(pidbuf, "%d", &other_pid)) {
	      dps_strerror(NULL, 0, "Can't read pid from '%s'", dps_pid_name);
	      close(pid_fd);
	      exit(1);
	    }
	    other_pid = kill((pid_t)other_pid, 0);
	    if (other_pid == 0) {
	      fprintf(stderr, "It seems that another indexer is already running!\n");
	      fprintf(stderr, "Remove '%s' if it is not true.\n", dps_pid_name);
	      close(pid_fd);
	      exit(1);
	    }
	    if (errno == EPERM) {
	      fprintf(stderr, "Can't check if another indexer is already running!\n");
	      fprintf(stderr, "Remove '%s' if it is not true.\n", dps_pid_name);
	      close(pid_fd);
	      exit(1);
	    }
	    dps_strerror(NULL, 0, "Process %s seems to be dead. Flushing '%s'", pidbuf, dps_pid_name);
	    lseek(pid_fd, 0L, SEEK_SET);
	    ftruncate(pid_fd, 0L);
	  }
	}

	sprintf(pidbuf,"%d\n",(int)getpid());
	(void)write(pid_fd,&pidbuf,dps_strlen(pidbuf));
	DpsClose(pid_fd);

	init_signals();
	DpsInit(argc, argv, envp); /* Initialize library */

	Conf = DpsEnvInit(NULL);
	if (Conf == NULL) exit(1);

	
	Agent = DpsAgentInit(NULL, Conf, 0);
	if (Agent == NULL) {
	        fprintf(stderr, "Can't alloc Agent at %s:%d\n", __FILE__, __LINE__);
		DpsEnvFree(Conf);
		unlink(dps_pid_name);
		exit(1);
	}

	if (DPS_OK != DpsEnvLoad(Agent, config_name, (dps_uint8)0)) {
		fprintf(stderr,"%s\n",DpsEnvErrMsg(Conf));
		DpsEnvFree(Conf);
		unlink(dps_pid_name);
		exit(1);
	}

	StoredFiles = DpsVarListFindInt(&Conf->Vars, "StoredFiles", 0x100);
	OptimizeInterval = DpsVarListFindInt(&Conf->Vars, "OptimizeInterval", 600);
	OptimizeRatio = DpsVarListFindInt(&Conf->Vars, "OptimizeRatio", 15);

	DpsOpenLog("stored", Conf, log2stderr);
	Agent->flags = Conf->flags = DPS_FLAG_UNOCON;
	Agent->Flags = Conf->Flags;
	Agent->WordParam = Conf->WordParam;
	DpsVarListAddLst(&Agent->Vars, &Conf->Vars, NULL, "*");

	DpsLog(Agent, DPS_LOG_ERROR, "stored started using config file %s", config_name);
	DpsLog(Agent, verb, "StoredFiles     : %d (0x%x)", StoredFiles, StoredFiles);
	DpsLog(Agent, verb, "OptimizeInterval: %d sec.", OptimizeInterval);
	DpsLog(Agent, verb, "OptimizeRatio   : %d %%", OptimizeRatio);


	if ((opt_pid = fork() ) == -1) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "%s fork() error", time_pid_info());
	  unlink(dps_pid_name);
	  exit(1);
	}
	if (opt_pid == 0) { /* child process */
	  Optimize(Conf);
	  DpsAgentFree(Agent);
	  DpsEnvFree(Conf);
	  exit(0);
	}


	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "%s socket() error",time_pid_info());
	  DpsAgentFree(Agent);
	  DpsEnvFree(Conf);
	  unlink(dps_pid_name);
	  exit(1);
	}
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0){
		DpsLog(Agent, DPS_LOG_ERROR, "%s setsockopt() error %d",time_pid_info(),errno);
		DpsAgentFree(Agent);
		DpsEnvFree(Conf);
		unlink(dps_pid_name);
		exit(1);
	}
	DpsSockOpt(Agent, s);

	bzero((void*)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	if((lstn = DpsVarListFindStr(&Agent->Conf->Vars, "Listen", NULL))) {
	  char * cport;
			
	  if((cport=strchr(lstn,':'))){
	    *cport='\0';
	    server_addr.sin_addr.s_addr = inet_addr(lstn);
	    nport=atoi(cport+1);
	  }else if (strchr(lstn, '.')) {
	    server_addr.sin_addr.s_addr = inet_addr(lstn);
	  }else{
	    nport=atoi(lstn);
	    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	  }
	  DpsLog(Agent,verb,"Listening port %d",nport);
	}else{
	  DpsLog(Agent,verb,"Listening port %d",nport);
	  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	server_addr.sin_port = htons((u_short)nport);
	

	if (bind(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		dps_strerror(Agent, DPS_LOG_ERROR, "%s bind() error", time_pid_info());
		unlink(dps_pid_name);
		DpsAgentFree(Agent);
		DpsEnvFree(Conf);
		exit(1);
	}

	if (listen(s, DPS_STORED_MAXCLNTS) == -1) {
		dps_strerror(Agent, DPS_LOG_ERROR, "%s listen() error", time_pid_info());
		unlink(dps_pid_name);
		DpsAgentFree(Agent);
		DpsEnvFree(Conf);
		exit(1);
	}
	
	FD_ZERO(&mask);
	FD_SET(s, &mask);


	while(1) {
#ifdef __irix__
 	        int addrlen;
#else
		socklen_t addrlen;
#endif
		int sel;
		struct timeval tval;
		fd_set msk;
	  	
		tval.tv_sec = 300;
		tval.tv_usec = 0;
		msk = mask;
		sel = select(DPS_STORED_MAXCLNTS, &msk, 0, 0, &tval);
		
		if(have_sighup){
			DpsLog(Agent,verb,"SIGHUP arrived. Reloading config.");
			have_sighup=0;
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			Conf = DpsEnvInit(NULL);
			if (Conf == NULL) {
			  fprintf(stderr, "Can't alloc Env at %s:%d\n", __FILE__, __LINE__);
			  unlink(dps_pid_name);
			  exit(1);
			}

			Agent = DpsAgentInit(NULL, Conf, 0);
			if (Agent == NULL) {
			  fprintf(stderr, "Can't alloc Agent at %s:%d\n", __FILE__, __LINE__);
			  DpsEnvFree(Conf);
			  unlink(dps_pid_name);
			  exit(1);
			}
			if (DPS_OK != DpsEnvLoad(Agent, config_name, (dps_uint8)0)) {
				DpsLog(Agent, DPS_LOG_ERROR, "%s", DpsEnvErrMsg(Conf));
				DpsAgentFree(Agent);
				DpsEnvFree(Conf);
				unlink(dps_pid_name);
				exit(1);
			}
			DpsParseCmdLine();
			DpsOpenLog("stored", Conf, log2stderr);
			Agent->flags = Conf->flags = DPS_FLAG_UNOCON;
			Agent->Flags = Conf->Flags;
			Agent->WordParam = Conf->WordParam;
			DpsVarListAddLst(&Agent->Vars, &Conf->Vars, NULL, "*");
		}
		if(have_sigint){
			DpsLog(Agent,verb,"SIGINT arrived");
			have_sigint=0;
			break;
		}
		if(have_sigterm){
			DpsLog(Agent,verb,"SIGTERM arrived");
			have_sigterm=0;
			break;
		}
		if(have_sigpipe){
			DpsLog(Agent,verb,"SIGPIPE arrived. Broken pipe !");
			have_sigpipe=0;
			break;
		}
		if (have_sigusr1) {
		  DpsIncLogLevel(Agent);
		  have_sigusr1 = 0;
		}
		if (have_sigusr2) {
		  DpsDecLogLevel(Agent);
		  have_sigusr2 = 0;
		}
		
		if(sel==0)continue;
		if(sel==-1){
			switch(errno){
				case EINTR:	/* Child */
					break;
			        default:
					dps_strerror(Agent, verb, "FIXME select error");
			}
			continue;
		}
		
		bzero((void*)&client_addr, addrlen = sizeof(client_addr));
	
	  if ((ns = accept(s, (struct sockaddr *) &client_addr, &addrlen)) == -1) {
	    dps_strerror(Agent, DPS_LOG_ERROR, "%s accept() error", time_pid_info());
	    unlink(dps_pid_name);
	    exit(1);
	  }

#define Client inet_ntoa(client_addr.sin_addr)

	  DpsLog(Agent, verb, "[%s] Accept", Client);
    
	  if ((pid = fork() ) == -1) {
	    dps_strerror(Agent, DPS_LOG_ERROR, "%s fork() error", time_pid_info());
	    unlink(dps_pid_name);
	    exit(1);
	  }

	  if (FD_ISSET(s, &msk)) {

	    if (pid == 0) { /* child process */
	      char port_str[16];
	      struct sockaddr_in dps_addr;
	      struct	sockaddr_in his_addr;
	      struct	in_addr bind_address;
	      unsigned char *p = (unsigned char*)&dps_addr.sin_port;
	      ssize_t sent;

	      close(s);

	      alarm(3600); /* 60 min. - maximum time of child execution */
	      init_signals();

      /* revert connection */

	      bind_address.s_addr 	= htonl(INADDR_ANY);
	      server_addr.sin_family	= AF_INET;
	      server_addr.sin_addr	= bind_address;
	      server_addr.sin_port	= 0; /* any free port */
	      p = (unsigned char*) &server_addr.sin_port;
	
	      if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dps_strerror(Agent, DPS_LOG_ERROR, "StoreD socket() ERR:");
                close(ns);
                exit(0);
              }
	      DpsSockOpt(Agent, sd);
	      if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		dps_strerror(Agent, DPS_LOG_ERROR, "StoreD ERR bind() error");
		close(ns);
		exit(0);
	      }
	      if (listen(sd, 1) < 0) {
		dps_strerror(Agent, DPS_LOG_ERROR, "StoreD ERR listen() error");
		close(ns);
		exit(0);
	      }

	      addrlen = sizeof(server_addr);
	      if (getsockname(sd, (struct sockaddr *)&server_addr, &addrlen) == -1) {
		dps_strerror(Agent, DPS_LOG_ERROR, "StoreD ERR getsockname %s:%d\n", __FILE__, __LINE__);
		close(ns);
		exit(0);
	      }
	      dps_snprintf(port_str, 15, "%d,%d", p[0], p[1]);

	      sent = DpsSend(ns, port_str, sizeof(port_str), 0);

	      if (sent != sizeof(port_str)) {
		DpsLog(Agent, DPS_LOG_ERROR, "StoreD ERR port sent %d of %d bytes\n", sent, sizeof(port_str));
		close(ns);
		exit(0);
	      }
		  
	      bzero((void*)&his_addr, addrlen = sizeof(his_addr));
	      if ((sd = accept(sd, (struct sockaddr *)&his_addr, &addrlen)) <= 0) {
		dps_strerror(Agent, DPS_LOG_ERROR, "StoreD ERR revert accept on port %d", ntohs(server_addr.sin_port));
		close(ns);
		exit(0);
	      }

/******************************************/


/**************************/

	      DpsLog(Agent, DPS_LOG_INFO, "[%s] Connected. PORT: %s", Client, port_str);

	      while (1) {
      
		if(have_sigpipe){
			DpsLog(Agent,verb,"Optimize: SIGPIPE arrived. Broken pipe !");
			have_sigpipe = 0;
			break;
		}
		if (DpsRecvall(ns, buf, 1/*, 0*/, 60)) {

		  switch(*buf) {
		  case 'S': /* Store document */
		    DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Store received.", Client);
		    if (DpsStoreSave(Agent, ns, Client) == DPS_OK) {
/*		      DpsSend (sd, "O", 1, 0); */
		    } else {
		      DpsLog(Agent, DPS_LOG_ERROR, "Store error");
/*		      DpsSend (sd, "E", 1, 0); */
		    }
		    break;
		  case 'G': /* retrieve document */
		    DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Get received.", Client);
		    if (DpsStoreGet(Agent, ns, sd, Client) != DPS_OK) {
		      DpsLog(Agent, DPS_LOG_ERROR, "Retrieve error");
		    }
		    break;
		  case 'E': /* retrieve by chuncks for excerption */
		    DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Excerpts received.", Client);
		    if (DpsStoreGetByChunks(Agent, ns, sd, Client) != DPS_OK) {
		      DpsLog(Agent, DPS_LOG_ERROR, "Retrieve by chunks error");
		    }
		    break;
		  case 'F': /* check documents presence */
		    DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Find received.", Client);
		    if (DpsStoreFind(Agent, ns, sd, Client) != DPS_OK) {
		      DpsLog(Agent, DPS_LOG_ERROR, "Find error");
		    }
		    break;
		  case 'D': /* delete document */
		    DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Delete received.", Client);
		    if (DpsStoreDelete(Agent, ns, sd, Client) != DPS_OK) {
		      DpsLog(Agent, DPS_LOG_ERROR, "Delete error");
		    }
		    break;
		  case 'C': /* check-up stored database */
		    DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Checkup received.", Client);
		    if (DpsStoredCheck(Agent, ns, sd, Client) != DPS_OK) {
		      DpsLog(Agent, DPS_LOG_ERROR, "Stored check-up error");
		    }
		    break;
		  case 'O': /* Optimize database */
		    DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Optimize received.", Client);
		    if (DpsStoredOptimize(Agent, ns, Client) != DPS_OK) {
		      DpsLog(Agent, DPS_LOG_ERROR, "Stored optimize error");
		    }
		    break;
		  case 'B': /* Bye */
		    DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Bye received.", Client);
		    close(ns);
		    exit(0);
		  default:
		    DpsLog(Agent, DPS_LOG_ERROR, "%s <hello> error %d %s", time_pid_info(), errno, buf);
		    continue;
		  }

		  alarm(3600); /* 60 min. - maximum time of child execution reset */

		} else {
		  DpsLog(Agent, DPS_LOG_DEBUG, "%s client silent.", time_pid_info());
		  sleep(0);
		}

	      }
	      close(ns);
	      exit(0);
	    }
	  }
	  /* parent process */
	  close(ns);
	}

	if (opt_pid != 0) {
	  kill(opt_pid, SIGTERM);
	}

	unlink(dps_pid_name);
	DpsAgentFree(Agent);
	DpsEnvFree(Conf);

	{
	  size_t i;
	  for (i = 0; i < (size_t)DpsARGC; i++) {
	    DPS_FREE(DpsARGV[i]);
	  }
	  DPS_FREE(DpsARGV);
	}

#else
	fprintf(stderr, "zlib support required. Please rebuild with --with-zlib option\n");

#endif
#ifdef EFENCE
     fprintf(stderr, "Memory leaks checking\n");
     DpsEfenceCheckLeaks();
#endif

#ifdef FILENCE
     fprintf(stderr, "FD leaks checking\n");
     DpsFilenceCheckLeaks(NULL);
#endif
#ifdef BOEHMGC
     CHECK_LEAKS();
#endif
	return 0;
}
