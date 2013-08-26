/* Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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
#include "dps_parser.h"
#include "dps_xmalloc.h"
#include "dps_log.h"
#include "dps_vars.h"
#include "dps_wild.h"
#include "dps_signals.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif

__C_LINK int __DPSCALL DpsParserAdd(DPS_PARSERLIST *List,DPS_PARSER *P){
	List->Parser=(DPS_PARSER*)(DpsRealloc(List->Parser,(List->nparsers+1)*sizeof(DPS_PARSER)));
	if (List->Parser == NULL) {
	  List->nparsers = 0;
	  return DPS_ERROR;
	}
	List->Parser[List->nparsers].from_mime = (char*)DpsStrdup(P->from_mime);
	List->Parser[List->nparsers].to_mime = (char*)DpsStrdup(P->to_mime);
	List->Parser[List->nparsers].cmd = (char*)DpsStrdup(P->cmd);
	List->nparsers++;
	return 0;
}

__C_LINK void __DPSCALL DpsParserListFree(DPS_PARSERLIST *List){
	size_t i;
	for(i=0;i<List->nparsers;i++){
		DPS_FREE(List->Parser[i].from_mime);
		DPS_FREE(List->Parser[i].to_mime);
		DPS_FREE(List->Parser[i].cmd);
	}
	DPS_FREE(List->Parser)
	List->nparsers=0;
}


#ifdef WITH_PARSER

/* Parser1: from STDIN to STDOUT */


static void sighandler(int sign);

static void init_signals(void){
	/* Set up signals handler*/
	DpsSignal(SIGALRM, sighandler);
}

static void sighandler(int sign){
	switch(sign){
	case SIGTERM:
	case SIGINT:
	case SIGALRM:
	  _exit(0);
	  break;
	default:
	  break;
	}
	init_signals();
}

/* Parser1: from STDIN to STDOUT */
static char *parse1(DPS_AGENT * Agent, DPS_DOCUMENT *Doc, const char *url, const char *cmd) {
	int wr[2];
	int rd[2];    
	pid_t pid;    
	size_t gap = (size_t)(Doc->Buf.content - Doc->Buf.buf);

	/* Create write and read pipes */
	if (pipe(wr) == -1){
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot make a pipe for a write");
		return NULL;
	}
	if (pipe(rd) == -1){
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot make a pipe for a read");
		return NULL;
	}    

	/* Fork a clild */
	if ((pid = fork()) == -1) {
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot spawn a child");
		return NULL;
	}

	if (pid > 0) {
		/* Parent process */
		ssize_t rs;

		/* Close other pipe ends */
		close(wr[0]);
		close(wr[1]);
		close(rd[1]);

		Doc->Buf.size = gap;
		if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
		  Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
		  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		    Doc->Buf.allocated_size = 0;
		    close(rd[0]);
		    wait(NULL);
		    return NULL;
		  }
		}

		while ( (rs = read(rd[0], Doc->Buf.buf + Doc->Buf.size, DPS_NET_BUF_SIZE)) > 0) {
		  Doc->Buf.size += (size_t)rs;
		  if (Doc->Buf.size >= Doc->Buf.max_size) break;
		  if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
		    Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
		    if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		      Doc->Buf.allocated_size = 0;
		      close(rd[0]);
		      wait(NULL);
		      return NULL;
		    }
		  }
		}
		Doc->Buf.buf[Doc->Buf.size] = '\0';
		close(rd[0]);
		wait(NULL);
	} else {
		/* Child process */
		/* Fork a clild */
		if ((pid = fork()) == -1){
			DpsLog(Agent,DPS_LOG_ERROR,"Cannot spawn a child");
			return NULL;
		}

		if (pid > 0) {
		/* Parent process */
			/* Close other pipe ends */
			close(wr[0]);
			close(rd[0]);
			close(rd[1]);

			/* Send string to be parsed */
			(void)write(wr[1], Doc->Buf.content, Doc->Buf.size - gap);
			close(wr[1]);

			_exit(0);
		}else{
			/* Child process */
			/* Close other pipe ends */
			close (wr[1]);
			close (rd[0]);

			/* Connect pipe to stdout */
			dup2(rd[1], STDOUT_FILENO);

			/* Connect pipe to stdin */
			dup2(wr[0], STDIN_FILENO);

			DpsSetEnv("DPS_URL",url);
			init_signals();
			alarm((unsigned int) DpsVarListFindInt(&Agent->Vars, "ParserTimeOut", 300) );

			(void)system(cmd);
			DpsUnsetEnv("DPS_URL");
			_exit(0);
		}
	}
	Doc->Buf.content = Doc->Buf.buf + gap;
	return Doc->Buf.content;
}

/* Parser2: from FILE to STDOUT */
static char *parse2(DPS_AGENT * Agent, DPS_DOCUMENT *Doc, const char *url, const char *cmd) {
	size_t gap = Doc->Buf.content - Doc->Buf.buf;
	ssize_t rs;
	int rd[2];    
	pid_t pid;    

	if (pipe(rd) == -1){
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot make a pipe for a read");
		return NULL;
	}    

	/* Fork a clild */
	if ((pid = fork()) == -1) {
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot spawn a child");
		return NULL;
	}

	if (pid > 0) {
		/* Parent process */
		close(rd[1]);
		Doc->Buf.size = gap;
		if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
		  Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
		  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		    Doc->Buf.allocated_size = 0;
		    close(rd[0]);
		    wait(NULL);
		    return NULL;
		  }
		}

		while ( (rs = read(rd[0], Doc->Buf.buf + Doc->Buf.size, DPS_NET_BUF_SIZE)) > 0) {
		  Doc->Buf.size += rs;
		  if (Doc->Buf.size >= Doc->Buf.max_size) break;
		  if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
		    Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
		    if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		      Doc->Buf.allocated_size = 0;
		      close(rd[0]);
		      wait(NULL);
		      return NULL;
		    }
		  }
		}
		Doc->Buf.buf[Doc->Buf.size] = '\0';
		close(rd[0]);
		wait(NULL);
	} else {
	        int fd, rc = 0;;

		if (setsid() == -1) rc = 128;

		/* Child process */
	        close (rd[0]);
			
		/* Connect pipe to stdout */
		if (dup2(rd[1], STDOUT_FILENO) == -1) rc |= 1;

		if ((fd = open("/dev/null", O_RDONLY)) == -1)    rc |= 2;
		if (dup2(fd, STDIN_FILENO) == -1)    rc |= 4;
		if (close(fd) == -1)    rc |= 8;

		if ((fd = open("/dev/null", O_WRONLY)) == -1)    rc |= 16;
		if (dup2(fd, STDERR_FILENO) == -1)    rc |= 32;
		if (close(fd) == -1)    rc |= 64;
		
		DpsSetEnv("DPS_URL",url);
		init_signals();
		alarm((unsigned int) DpsVarListFindInt(&Agent->Vars, "ParserTimeOut", 300) );

		(void)system(cmd);
		DpsUnsetEnv("DPS_URL");
		close (rd[1]);
		_exit(rc);
	}

	Doc->Buf.buf[Doc->Buf.size] = '\0';
	Doc->Buf.content = Doc->Buf.buf + gap;
	return Doc->Buf.content;
}

/* Parser3: from FILE to FILE */
static char *parse3(DPS_AGENT * Agent, DPS_DOCUMENT *Doc, const char *url, const char *cmd, const char *to_file) {
	int fd;
	pid_t pid;    
	size_t gap = Doc->Buf.content - Doc->Buf.buf;
	ssize_t rs;
#ifdef UNIONWAIT
	union wait status;
#else
	int status;
#endif

	/* Fork a clild */
	if ((pid = fork()) == -1) {
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot spawn a child");
		return NULL;
	}
	if (pid > 0) {
	  waitpid(pid, &status, 0);
	} else {
	  DpsSetEnv("DPS_URL",url);
	  init_signals();
	  alarm((unsigned int) DpsVarListFindInt(&Agent->Vars, "ParserTimeOut", 300) );
	  (void)system(cmd);
	  DpsUnsetEnv("DPS_URL");
	  _exit(0);
	}
	
	if((fd = DpsOpen2(to_file, O_RDONLY | DPS_BINARY))) {
		Doc->Buf.size = gap;
		if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
		  Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
		  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		    Doc->Buf.allocated_size = 0;
		    return NULL;
		  }
		}
		while ( (rs = read(fd, Doc->Buf.buf + Doc->Buf.size, DPS_NET_BUF_SIZE)) > 0) {
		  Doc->Buf.size += rs;
		  if (Doc->Buf.size >= Doc->Buf.max_size) break;
		  if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
		    Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
		    if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		      Doc->Buf.allocated_size = 0;
		      return NULL;
		    }
		  }
		}
		DpsClose(fd);
	}else{
		DpsLog(Agent,DPS_LOG_ERROR,"Can't open output file (parse3)");
		return NULL;
	}
	Doc->Buf.buf[Doc->Buf.size] = '\0';
	Doc->Buf.content = Doc->Buf.buf + gap;
	return Doc->Buf.content;
}

/* Parser4: from STDIN to FILE */
static char *parse4(DPS_AGENT * Agent, DPS_DOCUMENT *Doc, const char *url, const char *cmd, const char *to_file) {
	size_t gap = Doc->Buf.content - Doc->Buf.buf;
	ssize_t rs;
	int wr[2], fd;
	pid_t pid;    
#ifdef UNIONWAIT
	union wait status;
#else
	int status;
#endif

	if (pipe(wr) == -1){
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot make a pipe for a write");
		return NULL;
	}
	/* Fork a clild */
	if ((pid = fork()) == -1) {
		DpsLog(Agent,DPS_LOG_ERROR,"Cannot spawn a child");
		return NULL;
	}

	if (pid > 0) {
	  /* Parent process */
	  close(wr[0]);
	  /* Send string to be parsed */
	  (void)write(wr[1], Doc->Buf.content, Doc->Buf.size - gap);
	  close(wr[1]);

	  waitpid(pid, &status, 0);
	} else {
	  /* Child process */
	  close (wr[1]);

	  /* Connect pipe to stdin */
	  dup2(wr[0], STDIN_FILENO);
	  
	  DpsSetEnv("DPS_URL",url);
	  init_signals();
	  alarm((unsigned int) DpsVarListFindInt(&Agent->Vars, "ParserTimeOut", 300) );

	  (void)system(cmd);
	  DpsUnsetEnv("DPS_URL");
	  _exit(0);
	}

	if((fd = DpsOpen2(to_file, O_RDONLY | DPS_BINARY))) {
	  Doc->Buf.size = gap;
	  if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
	    Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
	    if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
	      Doc->Buf.allocated_size = 0;
	      return NULL;
	    }
	  }
	  while ( (rs = read(fd, Doc->Buf.buf + Doc->Buf.size, DPS_NET_BUF_SIZE)) > 0) {
	    Doc->Buf.size += rs;
	    if (Doc->Buf.size >= Doc->Buf.max_size) break;
	    if (Doc->Buf.size + DPS_NET_BUF_SIZE > Doc->Buf.allocated_size) {
	      Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
	      if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		Doc->Buf.allocated_size = 0;
		return NULL;
	      }
	    }
	  }
	  DpsClose(fd);
	} else {
	  DpsLog(Agent, DPS_LOG_ERROR, "Can't open output file (parse4)");
	  return NULL;
	}
	Doc->Buf.buf[Doc->Buf.size] = '\0';
	Doc->Buf.content = Doc->Buf.buf + gap;
	return Doc->Buf.content;
}

static char *parse_file (DPS_AGENT * Agent, DPS_PARSER * parser, DPS_DOCUMENT *Doc, const char * url) {
	char cmd[1024 + 2 * PATH_MAX]="";
	char *result=NULL;
	char *arg1pos, *arg2pos, *p;
	int parser_type;
	char fn0[PATH_MAX]="";
	char fn1[PATH_MAX]="";
	char * fnames[2];
	size_t gap = Doc->Buf.content - Doc->Buf.buf;

	arg1pos = strstr(parser->cmd, "$1");
	arg2pos = strstr(parser->cmd, "$2");

	/* Build temp file names and command line */
	dps_snprintf(fn0, sizeof(fn0) - 4, "/tmp/ind.%d.%d", Agent->handle, getpid());
	dps_strcpy(fn1, fn0);
	fnames[0] = dps_strcat(fn0, ".in");
	if (Doc->CurURL.filename != NULL) {
	  p = strrchr(Doc->CurURL.filename, (int)'.');
	  if (p != NULL) {
	    fnames[0] = dps_strcat(fn0, p);
	  }
	}
	fnames[1] = dps_strcat(fn1, ".out");
	DpsBuildParamStr(cmd,sizeof(cmd),parser->cmd,fnames,2);

	if(arg1pos){
		int fd;
		ssize_t res;
		size_t bytes_written = 0;

		/* Create temporary file */
		umask((mode_t)022);
		fd = DpsOpen3(fnames[0], O_RDWR | O_CREAT | DPS_BINARY, DPS_IWRITE);
		/* Write to the temporary file */
		while ((res = write(fd, Doc->Buf.content + bytes_written, Doc->Buf.size - gap - bytes_written)) > 0) {
		  bytes_written += (size_t)res;
		  if (bytes_written == Doc->Buf.size - gap) break;
		}
		DpsClose(fd);
	}

	if(arg1pos&&arg2pos)parser_type=3;
	else{
		if(arg1pos)parser_type=2;
		else
		if(arg2pos)parser_type=4; 
		else parser_type=1;
	}
	
	/*fprintf(stderr,"cmd='%s' parser_type=%d\n",cmd,parser_type);*/
	DpsLog(Agent, DPS_LOG_EXTRA, "Starting external parser: '%s'", cmd);
	switch(parser_type){
	case 1: result = parse1(Agent, Doc, url, cmd); break;
	case 2: result = parse2(Agent, Doc, url, cmd); break;
	case 3: result = parse3(Agent, Doc, url, cmd, fnames[1]); break;
	case 4: result = parse4(Agent, Doc, url, cmd, fnames[1]); break;
	}

	/* Remove temporary file */
	if(arg1pos)unlink(fnames[0]);
	if(arg2pos)unlink(fnames[1]);

	return result;
}

__C_LINK DPS_PARSER * __DPSCALL DpsParserFind(DPS_PARSERLIST *List,const char *mime_type) {
	size_t i;
	for(i=0;i<List->nparsers;i++){
		if(!DpsWildCaseCmp(mime_type, List->Parser[i].from_mime))
			return &List->Parser[i];
	}
    	return NULL;
}

char *DpsParserExec(DPS_AGENT *Agent, DPS_PARSER *P, DPS_DOCUMENT *Doc) {
        char *result;
	if (P->cmd[0] == (char)'\0') return Doc->Buf.content;
	result = parse_file(Agent, P, Doc, DpsVarListFindStr(&Doc->Sections, "URL", ""));
	Doc->Buf.size = dps_strlen(Doc->Buf.content) + (Doc->Buf.content - Doc->Buf.buf);
	return result;
}

#endif
