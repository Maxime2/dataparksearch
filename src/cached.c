/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
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
#include "dps_cache.h"
#include "dps_utils.h"
#include "dps_db.h"
#include "dps_db_int.h"
#include "dps_services.h"
#include "dps_log.h"
#include "dps_env.h"
#include "dps_conf.h"
#include "dps_agent.h"
#include "dps_mkind.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_signals.h"
#include "dps_base.h"
#include "dps_socket.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif

/*#define DEBUG_LOGD 1*/
/*#define DEBUG_LOGD_CMD 1*/

#define SPLDIR  "splitter"

#define STATE_CMD 1
#define STATE_WRD 2
#define DPS_CACHED_MAXCLNTS 128

/************************ type defenitions ************************/ 
typedef struct {
	int  recv_fd, send_fd;	/* Socket descriptors    */
	DPS_LOGD_CMD cmd;
        DPS_LOGD_WRD *wrd;
} DPS_LOGD_CL;


#ifdef HAVE_PTHREAD
static int total_threads = 0;
#endif
static int in_flush = 0;


#ifdef DEBUG_LOGD_CMD
static void printcmd(DPS_LOGD_CMD *c){
	fprintf(stderr, "cmd=%d nwords=%d stamp=%d url_id=%d\n", c->cmd, c->nwords, (int)c->stamp, c->url_id);
}
#endif

static void init_client(DPS_LOGD_CL * client) {
        client->recv_fd = client->send_fd = 0;
	client->wrd = NULL;
}

static void deinit_client(DPS_LOGD_CL * client) {
        DPS_FREE(client->wrd);
}

/********************* SIG Handlers ****************/

static void sighandler(int sign);

static void init_signals(void){
	/* Set up signals handler*/
	DpsSignal(SIGPIPE, sighandler);
	DpsSignal(SIGCHLD, sighandler);
	DpsSignal(SIGALRM, sighandler);
	DpsSignal(SIGUSR1, sighandler);
	DpsSignal(SIGUSR2, sighandler);
	DpsSignal(SIGHUP,sighandler);
	DpsSignal(SIGINT,sighandler);
	DpsSignal(SIGTERM,sighandler);
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

/*************************************************************/

static int demonize = 1;
static char time_pid[128];

static char *Logd_time_pid_info(void) {
	time_t t=time(NULL);
#ifdef HAVE_PTHREAD
	struct tm l_tim;
	struct tm *tim = localtime_r(&t, &l_tim);
#else
	struct tm *tim = localtime(&t);
#endif
	
	strftime(time_pid,sizeof(time_pid), "%a %d %H:%M:%S", tim);
	t = strlen(time_pid);
	dps_snprintf(time_pid + t, sizeof(time_pid) - t, " [%d]", (int)getpid());
	return(time_pid);
}


static int client_action(DPS_AGENT *Agent, DPS_LOGD_CL * client){

        size_t i;
	ssize_t         recvt;

#ifdef DEBUG_LOGD_CMD
	printcmd(&client->cmd);
#endif

#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Agent->TR, "[%d] client_action: cmd received\n", Agent->handle);
	  fprintf(Agent->TR, "cmd=%d nwords=%d stamp=%d url_id=%d\n", client->cmd.cmd, client->cmd.nwords, (int)client->cmd.stamp, 
		  client->cmd.url_id);
	  fflush(Agent->TR);
#endif

	if (client->cmd.nwords > 0) {

	  client->wrd = (DPS_LOGD_WRD*)DpsRealloc(client->wrd, (client->cmd.nwords + 1) * sizeof(DPS_LOGD_WRD));
	  if (client->wrd == NULL) {
	    DpsLog(Agent, DPS_LOG_ERROR, "Can't realloc memory for %d nwords (%u bytes) ", client->cmd.nwords, 
		   (client->cmd.nwords + 1) * sizeof(DPS_LOGD_WRD));  
	    if (DpsSend(client->send_fd, "F", 1, 0) != 1) return DPS_ERROR;
	    return DPS_ERROR;
	  }
	  if (DpsSend(client->send_fd, "O", 1, 0) != 1) return DPS_ERROR;
	  recvt = DpsRecvall(client->recv_fd, client->wrd, client->cmd.nwords * sizeof(DPS_LOGD_WRD), 360);
	  if (recvt != (ssize_t)(client->cmd.nwords * sizeof(DPS_LOGD_WRD))) {
	    DpsLog(Agent, DPS_LOG_ERROR, "Incorrect wrd received %d of %d (signal: %s) at %s:%d", 
		   recvt, client->cmd.nwords * sizeof(DPS_LOGD_WRD), 
		   (have_sigint) ? "SIGINT" :
		   (have_sigterm) ? "SIGTERM" :
		   (have_sigpipe) ? "SIGPIPE" : 
		   (have_sigalrm) ? "SIGALRM" : "UNKNOWN",
		   __FILE__, __LINE__);
	    if (DpsSend(client->send_fd, "F", 1, 0) != 1) return DPS_ERROR;
	    return DPS_ERROR;
	  }
	}

	if (client->cmd.cmd == DPS_LOGD_CMD_CHECK) {
	  DpsFlushAllBufs(Agent, 0); /* Don't rotate del logs */
	  DpsCachedCheck(Agent, client->cmd.url_id);
	}

#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	fprintf(Agent->TR, "[%d] client_action: sending OK [%d]\n", Agent->handle, __LINE__);
	fflush(Agent->TR);
#endif
	if (DpsSend(client->send_fd, "O", 1, 0) != 1) {
#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Agent->TR, "[%d] client_action: OK failed ?! [%d]\n", Agent->handle, __LINE__);
	  fflush(Agent->TR);
#endif
	  return DPS_ERROR;
	}
	
	if (client->cmd.cmd == DPS_LOGD_CMD_URLINFO) {
	  DPS_BASE_PARAM P;
	  urlid_t rec_id = client->cmd.url_id;
	  dps_uint4 tlen;
	  char *textbuf;

#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Agent->TR, "[%d] client_action: receinving tlen\n", Agent->handle);
	  fflush(Agent->TR);
#endif
	  if (DpsRecvall(client->recv_fd, &tlen, sizeof(tlen), 60) != sizeof(tlen)) {
	    DpsLog(Agent, DPS_LOG_ERROR, "DpsRecvall error %s:%d ", __FILE__, __LINE__);  
	    return DPS_ERROR;
	  }
	  if ((textbuf = (char*)DpsMalloc((size_t)tlen+1)) == NULL) {
	    DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc memory (%u bytes) %s:%d ", tlen+1, __FILE__, __LINE__);  
	    if (DpsSend(client->send_fd, "F", 1, 0) != 1) return DPS_ERROR;
	    return DPS_ERROR;
	  }
#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Agent->TR, "[%d] client_action: sending OK [%d]\n", Agent->handle, __LINE__);
	  fflush(Agent->TR);
#endif
	  if (DpsSend(client->send_fd, "O", 1, 0) != 1) {
	    DpsLog(Agent, DPS_LOG_ERROR, "DpsSend error %s:%d ", __FILE__, __LINE__);  
	    DPS_FREE(textbuf);
	    return DPS_ERROR;
	  }
	  if (DpsRecvall(client->recv_fd, textbuf, tlen, 360) != (ssize_t)tlen) {
	    DpsLog(Agent, DPS_LOG_ERROR, "DpsRecvall error %s:%d ", __FILE__, __LINE__);  
	    DPS_FREE(textbuf);
	    return DPS_ERROR;
	  }
#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Agent->TR, "[%d] client_action: done\n", Agent->handle);
	  fflush(Agent->TR);
#endif

	  bzero(&P, sizeof(P));
	  P.subdir = DPS_URLDIR;
	  P.basename = "info";
	  P.indname = "info";
#ifdef HAVE_ZLIB
	  P.zlib_method = Z_DEFLATED;
	  P.zlib_level = 9;
	  P.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
	  P.zlib_memLevel = 9;
	  P.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif
	  P.NFiles = DpsVarListFindInt(&Agent->Vars, "URLDataFiles", 0x300);
	  P.vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
	  P.A = Agent;
	  P.mode = DPS_WRITE_LOCK;
	  P.rec_id = rec_id;
	  DpsBaseWrite(&P, textbuf, tlen);
	  DpsBaseClose(&P);
	  DPS_FREE(textbuf);

	  if (DpsSend(client->send_fd, "O", 1, 0) != 1) {
	    DpsLog(Agent, DPS_LOG_ERROR, "DpsSend error %s:%d ", __FILE__, __LINE__);  
	    return DPS_ERROR;
	  }

	} else
	if (client->cmd.cmd == DPS_LOGD_CMD_DATA) {
	  size_t dbto;
	  DPS_DB *db;

	  while(in_flush) DPSSLEEP(1);
	  in_flush++;
	  DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	  dbto =  Agent->Conf->dbl.nitems;
	  DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
	  for (i = 0; i < dbto; i++) {
	    db = &Agent->Conf->dbl.db[i];
	    DpsURLDataWrite(Agent, db);
	  }
	  in_flush--;
	} else if (client->cmd.cmd == DPS_LOGD_CMD_FLUSH) {
	  while(in_flush) DPSSLEEP(1);
	  in_flush++;
	  DpsFlushAllBufs(Agent, 1); /* Rotate del logs */
	  in_flush--;
	} else if (client->cmd.cmd == DPS_LOGD_CMD_WORD 
		   || client->cmd.cmd == DPS_LOGD_CMD_NEWORD 
		   || client->cmd.cmd == DPS_LOGD_CMD_DELETE) {
	  register size_t dbnum;
	  register int rc;
	  DPS_DB *db;
/*	  DpsLog(Agent, DPS_LOG_EXTRA, "DPS_LOGD_CMD_WORD: %d  nwords:%d", client->cmd.url_id, client->cmd.nwords);*/
	  DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	  dbnum = client->cmd.url_id % Agent->Conf->dbl.nitems;
	  db = &Agent->Conf->dbl.db[dbnum];
	  DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
	  rc = DpsLogdStoreDoc(Agent, client->cmd, client->wrd, db);
	  if(rc != DPS_OK) {
	    DpsLog(Agent, DPS_LOG_ERROR, "DpsLogdStoreDoc error %s:%d ", __FILE__, __LINE__);  
	    return rc;
	  }
	}

	return DPS_OK;
}

static void usage(void){

  fprintf(stderr, "\ncached from %s-%s-%s\
\n(C)1998-2003, LavTech Corp.\n(C)2003-2011, DataPark Ltd.\nUsage: cached [OPTIONS] [configfile]\n\n\
Options are:\n\
  -w /path      choose alternative working /var directory\n\
  -p xxx        listen port xxx\n\
  -v n          verbose level, 0-5\n\
  -l            write logs only mode\n\
  -s k          sleep k sec. at start-up\n\
  -f            run foreground, don't demonize\n\
  -h,-?         print this help page and exit\n\n\n",

	  PACKAGE,VERSION,DPS_DBTYPE);
  return;
}

static void exitproc(void){
}

typedef struct {
  DPS_AGENT *Agent;
  int recv_fd, send_fd;
} DPS_CACHED_CFG;

static int next_thread = 200;

static void * thread_child(void *arg){

  DPS_CACHED_CFG *C = (DPS_CACHED_CFG*)arg;
  int lbytes;
  DPS_LOGD_CL cl;
  DPS_AGENT *Indexer = C->Agent;
#if defined(HAVE_PTHREAD)
  sigset_t mask;

  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
#endif

#ifdef HAVE_PTHREAD
/*  pthread_detach(pthread_self());*/
#endif

  init_client(&cl);
  cl.recv_fd = C->recv_fd;
  cl.send_fd = C->send_fd;

  DpsLog(Indexer, DPS_LOG_INFO, "%s Client thread started", Logd_time_pid_info());

  while (1) {

#ifdef FILENCE
     DpsFilenceCheckLeaks(
#ifdef WITH_TRACE
			  Indexer->TR
#else
			  NULL
#endif
			  );
#endif
#ifdef DEBUG_LOGD

    DpsLog(Indexer, DPS_LOG_DEBUG, "%s try read cmd (%d bytes)", Logd_time_pid_info(), sizeof(DPS_LOGD_CMD));

#endif

    lbytes = DpsRecvall(cl.recv_fd, &cl.cmd, sizeof(DPS_LOGD_CMD), 60);

#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Indexer->TR, "%s [%d] thread_child: cmd received (%d of %d)\n", 
		  Logd_time_pid_info(), Indexer->handle, lbytes, sizeof(DPS_LOGD_CMD));
	  fprintf(Indexer->TR, "cmd=%d nwords=%d stamp=%d url_id=%d\n", cl.cmd.cmd, cl.cmd.nwords, (int)cl.cmd.stamp, cl.cmd.url_id);
	  fflush(Indexer->TR);
#endif

    if (lbytes == sizeof(DPS_LOGD_CMD)) {

      if (cl.cmd.cmd == DPS_LOGD_CMD_BYE) {
	DpsLog(Indexer, DPS_LOG_INFO, "%s Client action BYE received.", Logd_time_pid_info());
	close (cl.recv_fd);
	close (cl.send_fd);
	deinit_client(&cl);
	init_client(&cl);
	break;
      }

#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Indexer->TR, "%s [%d] thread_child: before client action  Agent %x  Agent.Conf %x\n", 
		  Logd_time_pid_info(), Indexer->handle, Indexer, Indexer->Conf);
	  fflush(Indexer->TR);
#endif

      if (client_action(Indexer, &cl) != DPS_OK) {

#if defined(WITH_TRACE) && defined(DEBUG_LOGD)
	  fprintf(Indexer->TR, "%s [%d] thread_child: failed client action  Agent %x\n", 
		  Logd_time_pid_info(), Indexer->handle, Indexer);
	  fflush(Indexer->TR);
	  fprintf(Indexer->TR, "%s [%d] thread_child: before client action  Agent.Conf %x\n", 
		  Logd_time_pid_info(), Indexer->handle, Indexer->Conf);
	  fflush(Indexer->TR);
#endif

	DpsLog(Indexer, DPS_LOG_ERROR, "%s Client action failed", Logd_time_pid_info());
	close (cl.send_fd);
	close (cl.recv_fd);
	deinit_client(&cl);
	init_client(&cl);
	break;
      }
      DpsLog(Indexer, DPS_LOG_DEBUG, "%s Client action done", Logd_time_pid_info());
    } else {
      DpsLog(Indexer, DPS_LOG_INFO, "%s Client left", Logd_time_pid_info());
      close (cl.recv_fd);
      close (cl.send_fd);
      deinit_client(&cl);
      init_client(&cl);
      break;
    }
  }
#ifdef HAVE_PTHREAD
  DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
  total_threads--;
  DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
#endif
  DpsAgentFree(Indexer);
  DPS_FREE(C);
  return NULL;
}

static void * thread_flush_limits(void *arg) {
  DPS_AGENT *Indexer = (DPS_AGENT*)arg;
  size_t i, dbto;
  DPS_DB *db;

#ifdef HAVE_PTHREAD
/*  pthread_detach(pthread_self());*/
#endif

  while(in_flush) DPSSLEEP(1);

/*  if (have_sighup == 0) DpsURLAction(Indexer,  NULL, DPS_URL_ACTION_WRITEDATA);*/

  in_flush++;
  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
  dbto =  Indexer->Conf->dbl.nitems;
  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
  for (i = 0; i < dbto; i++) {
    db = &Indexer->Conf->dbl.db[i];
    DpsURLDataWrite(Indexer, db);
  }
  DpsFlushAllBufs(Indexer, 1); /* Rotate del logs */
#ifdef HAVE_PTHREAD
  DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
  total_threads--;
  DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
#endif
  DpsAgentFree(Indexer);
  in_flush--;
  return NULL;
}



static void * thread_optimize(void *arg) {
  DPS_AGENT *Agent = (DPS_AGENT*)arg;
  DPS_BASE_PARAM P, I;
  size_t WrdFiles, URLDataFiles;
  int OptimizeInterval, current_base, current_urlbase, res;
#if defined(HAVE_PTHREAD)
  sigset_t mask;

  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGTERM);
/*  sigaddset(&mask, SIGALRM);*/
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
#endif

/*  pthread_detach(pthread_self());*/

  WrdFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "WrdFiles", 0x300);
  URLDataFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "URLDataFiles", 0x300);
  OptimizeInterval = DpsVarListFindInt(&Agent->Vars, "OptimizeInterval", 300);

  DpsLog(Agent, DPS_LOG_EXTRA, "Optimize thread started: Nfiles: 0x%x Interval: %d", WrdFiles, OptimizeInterval);
  bzero(&P, sizeof(P));
  P.subdir = DPS_TREEDIR;
  P.basename = "wrd";
  P.indname = "wrd";
  P.NFiles = WrdFiles;
  P.vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
  P.A = Agent;
#ifdef HAVE_ZLIB
  P.zlib_method = Z_DEFLATED;
  P.zlib_level = 9;
  P.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
  P.zlib_memLevel = 9;
  P.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif

  bzero(&I, sizeof(I));
  I.subdir = DPS_URLDIR;
  I.basename = "info";
  I.indname = "info";
  I.NFiles = URLDataFiles;
  I.vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
  I.A = Agent;
#ifdef HAVE_ZLIB
  I.zlib_method = Z_DEFLATED;
  I.zlib_level = 9;
  I.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
  I.zlib_memLevel = 9;
  I.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif

  current_base = (int)time(NULL) % P.NFiles;
  current_urlbase = (int)time(NULL) % I.NFiles;

  while(1) {
    if (have_sighup) {
/*      size_t i;
      for (i = 0; i < P.NFiles; i++) {
	P.rec_id = current_base;
	DpsBaseOptimize(Agent, &P, current_base);
	DpsBaseClose(Agent, &P);
	current_base++;
	current_base %= P.NFiles;
	DPSSLEEP(0);
      }*/
    } else if (have_sigint || have_sigterm || have_sigpipe || have_sigalrm) { 
      break;
    } else {
/* 
   need parametrization on update strategy, if optimize is not done after every update then we need this code
*/
      P.rec_id = current_base;

      if (!Agent->Conf->logs_only) {
	DPS_GETLOCK(Agent, DPS_LOCK_CACHED_N(current_base));
	res = DpsLogdSaveBuf(Agent, Agent->Conf, current_base);
	DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED_N(current_base));
      }
      if (!Agent->Flags.OptimizeAtUpdate) DpsBaseOptimize(&P, current_base);
      DpsBaseClose(&P);
      current_base++;
      current_base %= P.NFiles;

      I.rec_id = current_urlbase;
      DpsBaseOptimize(&I, current_urlbase);
      DpsBaseClose(&I);

      current_urlbase++;
      current_urlbase %= I.NFiles;
    }
    DPSSLEEP(OptimizeInterval);
  }
  DpsAgentFree(Agent);
  return NULL;
}


int main(int argc,char **argv, char **envp) {
	struct	sockaddr_in server_addr;
	struct	sockaddr_in his_addr;
	struct	in_addr bind_address;
	const char	*var_dir = NULL;
	int	on = 1, rc = 0;
	int	ctl_sock, sel;
	struct timeval tval;
	int	pid_fd,ch,port=DPS_LOGD_PORT;
	char	pidbuf[1024];
	DPS_LOGD_CL cl;
	DPS_ENV * Conf = NULL;
	DPS_AGENT *Agent;
	const char * config_name = DPS_CONF_DIR "/cached.conf";
	const char *lstn;
	fd_set	mask;
#if defined(HAVE_PTHREAD)
	pthread_t optimize_tid;
#endif

	log2stderr = 1;
	init_client(&cl);

	DpsInit(argc, argv, envp); /* Initialize library */

	DpsInitMutexes();
	Conf = DpsEnvInit(NULL);
	if (Conf == NULL) {
	  DpsDestroyMutexes();
	  DpsDeInit();
	  exit(1);
	}
	DpsSetLockProc(Conf, DpsLockProc);
		
	while ((ch = getopt(argc, argv, "fhlv:w:p:s:?")) != -1){
		switch (ch) {
			case 'p':
				port=atoi(optarg);
				break;
		        case 'v': DpsSetLogLevel(NULL, atoi(optarg)); break;
			case 'w':
				var_dir = optarg;
				break;
		        case 'l':
			        Conf->logs_only = 1;
				break;
		        case 's':
			        sleep((unsigned int)atoi(optarg));
				break;
		        case 'f':
			        demonize = 0;
				break;
			case 'h':
			case '?':
			default:
				usage();
			        DpsEnvFree(Conf);
				DpsDestroyMutexes();
				DpsDeInit();
				return 1;
				break;
		}
	}
	argc -= optind;argv += optind;
	
	if(argc > 1){
		usage();
		goto err1;
	} else if (argc == 1) {
	        config_name = argv[0];
	}

	if (demonize) {
	  if ((rc = dps_demonize()) != 0) {
	    fprintf(stderr, "Can't demonize x%x (%d).\n", rc, rc);
	    goto err1;
	  }
	}
	
	Agent = DpsAgentInit(NULL, Conf, 0);
	if (Agent == NULL) {
	  fprintf(stderr, "Can't alloc Agent at %s:%d", __FILE__, __LINE__);
	  DpsEnvFree(Conf);
	  DpsDestroyMutexes();
	  DpsDeInit();
	  return DPS_ERROR;
	}

	Agent->flags = Conf->flags = DPS_FLAG_UNOCON;

	if(DPS_OK != DpsEnvLoad(Agent, config_name, (dps_uint8)0)){
		fprintf(stderr, "%s\n", DpsEnvErrMsg(Conf));
		DpsEnvFree(Conf);
		DpsDestroyMutexes();
		DpsDeInit();
		return DPS_ERROR;
	}
		
	DpsOpenLog("cached", Conf, log2stderr);
	Agent->Flags = Conf->Flags;
	Agent->WordParam = Conf->WordParam;
	DpsVarListAddLst(&Agent->Vars, &Conf->Vars, NULL, "*");

	if (var_dir == NULL) var_dir = DpsVarListFindStr(&Conf->Vars, "VarDir", DPS_VAR_DIR);

	/* Check that another instance isn't running and create PID file */
	dps_snprintf(dps_pid_name, PATH_MAX, "%s%s%s", var_dir, DPSSLASHSTR, "cached.pid");
	pid_fd=open(dps_pid_name, O_CREAT|O_EXCL|O_WRONLY, 0644);
	if(pid_fd<0){

	  dps_strerror(NULL, 0, "Can't create '%s'", dps_pid_name);
	  if(errno == EEXIST){
	    int pid = 0;
	    pid_fd = DpsOpen3(dps_pid_name, O_RDWR, 0644);
	    if (pid_fd < 0) {
	      dps_strerror(NULL, 0, "Can't open '%s'", dps_pid_name);
	      goto err2;
	    }
	    (void)read(pid_fd, pidbuf, sizeof(pidbuf));
	    if (1 > sscanf(pidbuf, "%d", &pid)) {
	      dps_strerror(NULL, 0, "Can't read pid from '%s'", dps_pid_name);
	      close(pid_fd);
	      goto err2;
	    }
	    pid = kill((pid_t)pid, 0);
	    if (pid == 0) {
	      fprintf(stderr, "It seems that another indexer is already running!\n");
	      fprintf(stderr, "Remove '%s' if it is not true.\n", dps_pid_name);
	      close(pid_fd);
	      goto err2;
	    }
	    if (errno == EPERM) {
	      fprintf(stderr, "Can't check if another indexer is already running!\n");
	      fprintf(stderr, "Remove '%s' if it is not true.\n", dps_pid_name);
	      close(pid_fd);
	      goto err2;
	    }
	    dps_strerror(NULL, 0, "Process %s seems to be dead. Flushing '%s'", pidbuf, dps_pid_name);
	    lseek(pid_fd, 0L, SEEK_SET);
	    ftruncate(pid_fd, 0L);
	  }
	}
	sprintf(pidbuf,"%d\n",(int)getpid());
	(void)write(pid_fd, &pidbuf, strlen(pidbuf));
	close(pid_fd);
	
	/* Initialize variables */

	DpsLog(Agent, DPS_LOG_INFO, "Starting in %s mode", (Conf->logs_only) ? "old" : "new");
	
	if(DPS_OK != DpsOpenCache(Agent, 
#ifdef HAVE_PTHREAD
				  0
#else
				  0 /*1*/
#endif
	       )) {
		DpsLog(Agent, DPS_LOG_ERROR, "Error: %s", Conf->errstr);
		goto err1;
	}

	atexit(&exitproc);
	init_signals();
	
#ifdef HAVE_PTHREAD  

#ifdef HAVE_PTHREAD_SETCONCURRENCY_PROT
	  if (pthread_setconcurrency(FD_SETSIZE + 3) != 0) {
	    DpsLog(A, DPS_LOG_ERROR, "Can't set %d concurrency threads", FD_SETSIZE + 3);
	    DpsEnvFree(Conf);
	    DpsDestroyMutexes();
	    DpsDeInit();
	    return DPS_ERROR;
	  }
#elif HAVE_THR_SETCONCURRENCY_PROT
	  if (thr_setconcurrency(FD_SETSIZE + 3) != NULL) {
	    DpsLog(A, DPS_LOG_ERROR, "Can't set %d concurrency threads", FD_SETSIZE + 3);
	    DpsEnvFree(Conf);
	    DpsDestroyMutexes();
	    DpsDeInit();
	    return DPS_ERROR;
	  }
#endif

	{
	  pthread_attr_t attr;
	  size_t stksize = 1024 * 1024;
	  DPS_AGENT *I;
		  
	  DPS_GETLOCK(Agent, DPS_LOCK_THREAD);
	  DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	  I = DpsAgentInit(NULL, Agent->Conf, 0);
	  DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
	  DPS_RELEASELOCK(Agent, DPS_LOCK_THREAD);

	  if (I == NULL) {
		DpsLog(Agent, DPS_LOG_ERROR , "Can't alloc Agent at %s:%d", __FILE__, __LINE__);
		goto err1;
	  }

	  pthread_attr_init(&attr);
	  pthread_attr_setstacksize(&attr, stksize);
	  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
/*	  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#ifdef SCHED_FIFO
	  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
#endif*/
	  pthread_create(&optimize_tid, &attr, &thread_optimize, I);
	  pthread_attr_destroy(&attr);
	}
#endif

	ctl_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ctl_sock < 0) {
		DpsLog(Agent, DPS_LOG_ERROR , "%s socket() error %d", Logd_time_pid_info(), errno);
		goto err1;
	}
	if (setsockopt(ctl_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0){
		DpsLog(Agent, DPS_LOG_ERROR, "%s setsockopt() error %d", Logd_time_pid_info(), errno);
		goto err1;
	}
	DpsSockOpt(Agent, ctl_sock);
	
	/* Prepare to start TCP server */
	
	server_addr.sin_family	= AF_INET;
	if((lstn = DpsVarListFindStr(&Agent->Conf->Vars, "Listen", NULL))) {
	  char * cport;
			
	  if((cport = strchr(lstn, ':'))) {
	    *cport = '\0';
	    server_addr.sin_addr.s_addr = inet_addr(lstn);
	    port = atoi(cport + 1);
	  }else if (strchr(lstn, '.')) {
	    server_addr.sin_addr.s_addr = inet_addr(lstn);
	  }else {
	    port = atoi(lstn);
	    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	  }
	  DpsLog_noagent(Conf, DPS_LOG_INFO, "Listening port %d", port);
	}else{
	  DpsLog_noagent(Conf, DPS_LOG_INFO, "Listening port %d", port);
	  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	server_addr.sin_port	= htons(port);
	
	if (bind(ctl_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "%s bind() port %d error", Logd_time_pid_info(), port);
	  goto err1;
	}
	if (listen(ctl_sock, DPS_CACHED_MAXCLNTS) < 0) {
		dps_strerror(Agent, DPS_LOG_ERROR, "%s listen() error", Logd_time_pid_info());
		goto err1;
	}

	DpsLog(Agent, DPS_LOG_INFO, "%s Started on port %d. Accepting %d connections", Logd_time_pid_info(), port, FD_SETSIZE);
	
	while (1) {
#ifdef __irix__
 	        int addrlen;
#else
		socklen_t addrlen;
#endif
#if !defined(HAVE_PTHREAD) && (defined(HAVE_SEMAPHORE_H) || defined(HAVE_SYS_SEM_H))
		int pid;
#endif		
		if (have_sighup) {
		  have_sighup = 0;
		  DpsLog(Agent, DPS_LOG_INFO, "%s SIGHUP arrived. Flushing buffers and making indexes.", Logd_time_pid_info());

		  {
		    DPS_AGENT *Indexer;
		  
		    DPS_GETLOCK(Agent, DPS_LOCK_THREAD);
		    DPS_GETLOCK(Agent, DPS_LOCK_CONF);
		    Indexer = DpsAgentInit(NULL, Agent->Conf, next_thread++);
		    if (next_thread == 500) next_thread = 200;
#ifdef HAVE_PTHREAD
		    total_threads++;
#endif
		    DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
		    DPS_RELEASELOCK(Agent, DPS_LOCK_THREAD);

		    if (Indexer == NULL) {
		      DpsLog(Agent, DPS_LOG_ERROR , "Can't alloc Agent at %s:%d", __FILE__, __LINE__);
		      goto err1;
		    }
#ifdef HAVE_PTHREAD  
		    {
		      pthread_attr_t attr;
		      size_t stksize = 1024 * 1024;
		      pthread_t tid;
		  
		      pthread_attr_init(&attr);
		      pthread_attr_setstacksize(&attr, stksize);
		      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
/*		      pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#ifdef SCHED_FIFO
		      pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
#endif*/
		      pthread_create(&tid, &attr, &thread_flush_limits, Indexer);
		      pthread_attr_destroy(&attr);
		    }
#else

#if defined(HAVE_SEMAPHORE_H) || defined(HAVE_SYS_SEM_H)
		    if ((pid = fork() ) == -1) {
		      dps_strerror(Agent, DPS_LOG_ERROR, "%s fork() error", Logd_time_pid_info());
		      unlink(dps_pid_name);
		      DpsEnvFree(Conf);
		      DpsDestroyMutexes();
		      DpsDeInit();
		      exit(1);
		    }
		    if (pid == 0) { /* child process */
#endif

		      thread_flush_limits(Indexer);
		    
		      DpsEnvFree(Conf);
		      DpsDestroyMutexes();
		      DpsDeInit();
#if defined(HAVE_SEMAPHORE_H) || defined(HAVE_SYS_SEM_H)
		      exit(0);
		    }
#endif

#endif

		  }
		}
		if (have_sigpipe) {
		        have_sigpipe = 0;
			DpsLog(Agent, DPS_LOG_ERROR, "%s SIGPIPE arrived. Broken pipe.", Logd_time_pid_info());
			DpsFlushAllBufs(Agent, 0); /* Don't rotate del logs */
			continue;
		}
		if (have_sigint) {
			DpsLog(Agent, DPS_LOG_INFO, "%s SIGINT arrived. Shutdown.", Logd_time_pid_info());
			DpsFlushAllBufs(Agent, 0); /* Don't rotate del logs */
/*			DpsURLAction(Agent, NULL, DPS_URL_ACTION_WRITEDATA);*/
			break;
		}
		if (have_sigusr1) {
		  have_sigusr1 = 0;
		  DpsIncLogLevel(Agent);
		}
		if (have_sigusr2) {
		  have_sigusr2 = 0;
		  DpsDecLogLevel(Agent);
		}
		if(have_sigterm){
			DpsLog(Agent, DPS_LOG_INFO, "SIGTERM arrived. Shutdown.");
			DpsFlushAllBufs(Agent, 0); /* Don't rotate del logs */
/*			DpsURLAction(Agent, NULL, DPS_URL_ACTION_WRITEDATA);*/
			break;
		}
		if(have_sigalrm){
			DpsLog(Agent, DPS_LOG_INFO, "SIGALRM arrived. Shutdown.");
			DpsFlushAllBufs(Agent, 0); /* Don't rotate del logs */
			break;
		}
		
		if (have_sighup) continue;
		
		tval.tv_sec = 60;
		tval.tv_usec = 0;
		FD_ZERO(&mask);
		FD_SET(ctl_sock, &mask);

		sel = select(FD_SETSIZE, &mask, 0, 0, &tval);

		if(sel==0){
			/* Time limit expired */
 			continue;
		}
		if(sel<1){
			/* FIXME: add errno checking */
			continue;
		}


		if (FD_ISSET(ctl_sock, &mask)) {
		  char port_str[16];
		  struct sockaddr_in dps_addr;
		  unsigned char *p = (unsigned char*)&dps_addr.sin_port;
		  ssize_t sent;


		  bzero((void*)&his_addr, addrlen = sizeof(his_addr));
		  cl.recv_fd = accept(ctl_sock, (struct sockaddr *)&his_addr, &addrlen);
		  if (cl.recv_fd <= 0) {
		    continue;
		  }


		  /* revert connection */

		  bind_address.s_addr 	= htonl(INADDR_ANY);
		  server_addr.sin_family	= AF_INET;
		  server_addr.sin_addr	= bind_address;
		  server_addr.sin_port	= 0; /* any free port */
		  p = (unsigned char*) &server_addr.sin_port;
	
		  if ((cl.send_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		    dps_strerror(Agent, DPS_LOG_ERROR, "CacheD socket() ERR");
		    close(cl.recv_fd);
/*		    close(cl.send_fd);*/
		    continue;
		  }
		  DpsSockOpt(Agent, cl.send_fd);
		  if (bind(cl.send_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		    dps_strerror(Agent, DPS_LOG_ERROR, "CacheD ERR bind() error");
		    close(cl.recv_fd);
		    continue;
		  }
		  if (listen(cl.send_fd, 1) < 0) {
		    dps_strerror(Agent, DPS_LOG_ERROR, "CacheD ERR listen() error");
		    close(cl.recv_fd);
		    close(cl.send_fd);
		    continue;
		  }

		  addrlen = sizeof(server_addr);
		  if (getsockname(cl.send_fd, (struct sockaddr *)&server_addr, &addrlen) == -1) {
		    dps_strerror(Agent, DPS_LOG_ERROR, "CacheD ERR getsockname at %s:%d", __FILE__, __LINE__);
		    close(cl.recv_fd);
		    close(cl.send_fd);
		    continue;
		  }
		  dps_snprintf(port_str, 15, "%d,%d", p[0], p[1]);

		  sent = DpsSend(cl.recv_fd, port_str, sizeof(port_str), 0);

		  if (sent != sizeof(port_str)) {
		    DpsLog(Agent, DPS_LOG_ERROR, "CacheD ERR port sent %d of %d bytes\n", sent, sizeof(port_str));
		    close(cl.recv_fd);
		    close(cl.send_fd);
		    continue;
		  }
		  
		  bzero((void*)&his_addr, addrlen = sizeof(his_addr));
		  if ((cl.send_fd = accept(cl.send_fd, (struct sockaddr *)&his_addr, &addrlen)) <= 0) {
		    dps_strerror(Agent, DPS_LOG_ERROR, "CacheD ERR revert accept on port %d error", ntohs(server_addr.sin_port));
		    close(cl.recv_fd);
		    close(cl.send_fd);
		    continue;
		  }
#define Client inet_ntoa(his_addr.sin_addr)

/*********************************/
/******************************/

		  DpsLog(Agent, DPS_LOG_INFO, "[%s] Connected. PORT: %s", Client, port_str);

		  {
		    DPS_AGENT *Indexer;
		    DPS_CACHED_CFG *C;

		    if ((C = (DPS_CACHED_CFG*)DpsMalloc(sizeof(DPS_CACHED_CFG))) == NULL) {
		      DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc DPS_CACHED_CFG %s:%d", __FILE__, __LINE__);
		      break;
		    }
		  
		    DPS_GETLOCK(Agent, DPS_LOCK_THREAD);
		    DPS_GETLOCK(Agent, DPS_LOCK_CONF);
		    Indexer = DpsAgentInit(NULL, Conf, next_thread++);
		    if (next_thread == 500) next_thread = 200;
#ifdef HAVE_PTHREAD
		    total_threads++;
#endif
		    DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
		    DPS_RELEASELOCK(Agent, DPS_LOCK_THREAD);
		    
		    if (Indexer == NULL) {
		      DpsLog(Agent, DPS_LOG_ERROR , "Can't alloc Agent at %s:%d", __FILE__, __LINE__);
		      goto err1;
		    }

		    C->Agent = Indexer;
		    C->recv_fd = cl.recv_fd;
		    C->send_fd = cl.send_fd;

#ifdef HAVE_PTHREAD  

		    {
		      pthread_attr_t attr;
		      size_t stksize = 512 * 1024;
		      pthread_t tid;
		  
		      pthread_attr_init(&attr);
		      pthread_attr_setstacksize(&attr, stksize);
		      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
/*		      pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#ifdef SCHED_FIFO
		      pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
#endif*/
		      pthread_create(&tid, &attr, &thread_child, C);
		      pthread_attr_destroy(&attr);
		    }

#else

#if defined(HAVE_SEMAPHORE_H) || defined(HAVE_SYS_SEM_H)
		    if ((pid = fork() ) == -1) {
		      dps_strerror(Agent, DPS_LOG_ERROR, "%s fork() error", Logd_time_pid_info());
		      unlink(dps_pid_name);
		      DpsAgentFree(Agent);
		      DpsEnvFree(Conf);
		      DpsDestroyMutexes();
		      deinit_client(&cl);
		      DpsDeInit();
		      exit(1);
		    }
		    if (pid == 0) { /* child process */
#endif

		      thread_child(C);

		      DpsAgentFree(Agent);
		      DpsEnvFree(Conf);
		      DpsDestroyMutexes();
		      deinit_client(&cl);
		      DpsDeInit();
#if defined(HAVE_SEMAPHORE_H) || defined(HAVE_SYS_SEM_H)
		      exit(0);
		    }
		    DpsAgentFree(Indexer);
		    DPS_FREE(C);
#endif

#endif
		    DPSSLEEP(0);
		  }
		}
		
	}
#ifdef HAVE_PTHREAD
	DpsLog(Agent, DPS_LOG_INFO, "Waiting working threads");
	while (1) {
	  int num;
	  DPS_GETLOCK(Agent, DPS_LOCK_THREAD);
	  num = total_threads;
	  DPS_RELEASELOCK(Agent, DPS_LOCK_THREAD);
	  if (num == 0) break;
	  DPSSLEEP(1);
	}
	DpsLog(Agent, DPS_LOG_INFO, "Waiting done.");
#endif
	if (have_sigpipe || have_sigint || have_sigterm || have_sigalrm) {
	  DpsFlushAllBufs(Agent, 0); /* Don't rotate del logs */
	}
/*	if (have_sigint || have_sigterm) {
	  DpsURLAction(Agent, NULL, DPS_URL_ACTION_WRITEDATA);
	}*/
#ifdef HAVE_PTHREAD  
	pthread_kill(optimize_tid, SIGALRM);
	pthread_join(optimize_tid, NULL);
#endif


	DpsCloseCache(Agent, 
#ifdef HAVE_PTHREAD
		      0
#else
		      0 /*1*/
#endif
		      , 1
	    );
	DpsAgentFree(Agent);
	DpsEnvFree(Conf);
	DpsDestroyMutexes();
	deinit_client(&cl);
	DpsDeInit();
	unlink(dps_pid_name);
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
	exit(0);
err1:
	unlink(dps_pid_name);
err2:
	if (Agent) DpsCloseCache(Agent, 
#ifdef HAVE_PTHREAD
				 0
#else
				 0 /*1*/
#endif
				 , 1
	    );
	DpsAgentFree(Agent);
	DpsEnvFree(Conf);
	DpsDestroyMutexes();
	deinit_client(&cl);
	DpsDeInit();
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
	exit(rc);

}
