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
#include "dpsearch.h"
#include "dps_db_int.h"
#include "dps_base.h"
#include "dps_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <errno.h> 
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif


/** 
   splitter usage
 */
static void usage(void) {

	fprintf(stderr, "\nsplitter from %s-%s-%s\n(C)1998-2003, LavTech Corp.\
\n(C)2003-2011, DataPark Ltd.\n\
\n\
Usage: splitter [OPTIONS] [configfile]\n\
\n\
Options are:\n\
  -w /path      choose alternative working /var directory\n\
  -f xxx        start at xxx.log, where xxx is a hex number\n\
  -t xxx        stop  at xxx.log, where xxx is a hex number\n\
  -v n          verbose level, 0-5\n\
  -p n          sleep n seconds after each buffer update. (Default 1)\n\
  -b            optimize before update (to check-up database before update)\n\
  -o            optimize after update\n\
  -h,-?         print this help page and exit\n\n\n",
	PACKAGE,VERSION,DPS_DBTYPE);

	return;
}

int main(int argc,char **argv, char **envp) {
  int ch, sleeps = 1, optimize = 0, obi = 0;
  unsigned int from = 0, to = 0xFFF, p_to = 0;
	DPS_ENV * Env;
	const char * config_name = DPS_CONF_DIR "/cached.conf";

	DpsInit(argc, argv, envp); /* Initialize library */
	
	DpsInitMutexes();
	Env=DpsEnvInit(NULL);
	if (Env == NULL) exit(1);
	DpsSetLockProc(Env, DpsLockProc);

/*#ifndef HAVE_SETPROCTITLE*/
	ARGV = argv;
	ARGC = argc;
/*#endif*/
	while ((ch = getopt(argc, argv, "blt:f:op:w:v:h?")) != -1){
		switch (ch) {
			case 'f':
				sscanf(optarg, "%x", &from);
				break;	
			case 't': 
				sscanf(optarg, "%x", &p_to);
				break;
			case 'w':
			        DpsVarListReplaceStr(&Env->Vars, "VarDir", optarg);
				break;
                        case 'v': DpsSetLogLevel(NULL, atoi(optarg)); break;
                        case 'b': obi++; break;
                        case 'o': optimize++; break;
                        case 'p': sleeps = atoi(optarg); break;
			case 'h':
			case '?':
			default:
			  usage();
			  DpsEnvFree(Env);
			  DpsDestroyMutexes();
			  DpsDeInit();
				return 1;
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if(argc > 1) {
		usage();
		DpsEnvFree(Env);
		DpsDestroyMutexes();
		DpsDeInit();
		return 1;
	} else if (argc == 1) {
	        config_name = argv[0];
	}
	{
		DPS_LOGDEL *del_buf=NULL;
		size_t del_count = 0, log, bytes, n = 0;
		int dd, log_fd;
		struct stat sb;
		char dname[PATH_MAX] = "";
		DPS_BASE_PARAM P;
		DPS_LOGWORD *log_buf = NULL;
		DPS_AGENT *Indexer = DpsAgentInit(NULL, Env, 0);

		log2stderr = 1;
		if (Indexer == NULL) {
		  fprintf(stderr, "Can't alloc Agent at %s:%d\n", __FILE__, __LINE__);
		  exit(DPS_ERROR);
		}
		
		if(DPS_OK != DpsEnvLoad(Indexer, config_name, (dps_uint8)0)){
		  fprintf(stderr, "%s\n", DpsEnvErrMsg(Env));
		  DpsEnvFree(Env);
		  DpsDestroyMutexes();
		  DpsDeInit();
		  return DPS_ERROR;
		}
		DpsOpenLog("splitter", Env, log2stderr);
		Indexer->flags = Env->flags = DPS_FLAG_UNOCON;
		DpsVarListAddLst(&Indexer->Vars, &Env->Vars, NULL, "*");

		bzero(&P, sizeof(P));
		P.subdir = DPS_TREEDIR;
		P.basename = "wrd";
		P.indname = "wrd";
		P.mode = DPS_WRITE_LOCK;
		P.NFiles = DpsVarListFindInt(&Indexer->Conf->Vars, "WrdFiles", 0x300);
		P.vardir = DpsStrdup(DpsVarListFindStr(&Indexer->Conf->Vars, "VarDir", DPS_VAR_DIR));
		P.A = Indexer;
		if (p_to != 0) to = p_to;
		else to = P.NFiles - 1;
#ifdef HAVE_ZLIB
		P.zlib_method = Z_DEFLATED;
		P.zlib_level = 9;
		P.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
		P.zlib_memLevel = 9;
		P.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif

		/* Open del log file */
		dps_snprintf(dname,sizeof(dname),"%s%c%s%cdel-split.log", P.vardir, DPSSLASH, DPS_SPLDIR, DPSSLASH);
		if((dd = DpsOpen2(dname, O_RDONLY | DPS_BINARY)) < 0) {
		  dps_strerror(NULL, 0, "Can't open del log '%s'", dname);
		  exit(DPS_ERROR);
		}

		DpsLog(Indexer, DPS_LOG_DEBUG, "VarDir: %s, WrdFiles: %d [%x]", P.vardir, P.NFiles, P.NFiles);

		/* Allocate del buffer */
		fstat(dd, &sb);
		if (sb.st_size != 0) {
		  del_buf=(DPS_LOGDEL*)DpsMalloc((size_t)sb.st_size + 1);
		  if (del_buf == NULL) {
		    fprintf(stderr, "Can't alloc %d bytes at %s:%d\n", (int)sb.st_size, __FILE__, __LINE__);
		    exit(0);
		  }
		  del_count=read(dd,del_buf,(size_t)sb.st_size)/sizeof(DPS_LOGDEL);
		}
		DpsClose(dd);

		/* Remove duplicates URLs in DEL log     */
		/* Keep only oldest records for each URL */
		if (del_count > 0) {
		  DpsLog(Indexer, DPS_LOG_DEBUG, "Sorting del_buf: %d items", del_count);
		  if (del_count > 1) DpsSort(del_buf, (size_t)del_count, sizeof(DPS_LOGDEL), DpsCmpurldellog);
		    DpsLog(Indexer, DPS_LOG_DEBUG, "Removing DelLogDups");
		  del_count = DpsRemoveDelLogDups(del_buf, del_count);
		}

		DpsLog(Indexer, DPS_LOG_DEBUG, "Processing Bufs from %d [%x] to %d [%x]", from, from, to, to);

		for(log = from; log <= to; log++) {

		  /* Open log file */
		  dps_snprintf(dname, sizeof(dname), "%s%c%s%c%03X-split.log", P.vardir, DPSSLASH, DPS_SPLDIR, DPSSLASH, log);
		  if((log_fd = DpsOpen2(dname, O_RDWR|DPS_BINARY)) < 0){
		    if (errno == ENOENT) {
		      dps_strerror(Indexer, DPS_LOG_DEBUG, "Can't open '%s'", dname);
		      n = 0;
/*		      continue;*/
		    } else {
		      dps_strerror(Indexer, DPS_LOG_ERROR, "Can't open '%s'", dname);
		      continue;
		    }
		  } else {
		    DpsWriteLock(log_fd); 
		    DpsLog(Indexer, DPS_LOG_DEBUG, "Processing Log: %x", log);
		    fstat(log_fd, &sb);
		    log_buf = (sb.st_size > 0) ? (DPS_LOGWORD*)DpsMalloc((size_t)sb.st_size + 1) : NULL;
		    if (log_buf != NULL) {
		      unlink(dname);
		      bytes = read(log_fd,log_buf,(size_t)sb.st_size);
		      (void)ftruncate(log_fd, (off_t)0);
		      DpsUnLock(log_fd);
		      DpsClose(log_fd);
		      
		      n = bytes / sizeof(DPS_LOGWORD);
		      DpsLog(Indexer, DPS_LOG_DEBUG, "Sorting log_buf: %d items", n);
		      if (n > 1) DpsSort(log_buf, n, sizeof(DPS_LOGWORD), (qsort_cmp)DpsCmplog);
		      DpsLog(Indexer, DPS_LOG_DEBUG, "Removing OldWords");
		      n = DpsRemoveOldWords(log_buf, n, del_buf, del_count);
		      if (n > 1) DpsSort(log_buf, n, sizeof(DPS_LOGWORD), (qsort_cmp)DpsCmplog_wrd);
		      
		    } else {
		      n = 0;
		      DpsUnLock(log_fd);
		      DpsClose(log_fd);
		    }
		  }

		  DpsLog(Indexer, DPS_LOG_DEBUG, "Processing Buf, optimize: %d", optimize);
		  if (obi) DpsBaseOptimize(&P, log);
		  DpsProcessBuf(Indexer, &P, log, log_buf, n, del_buf, del_count);
		  if (optimize) DpsBaseOptimize(&P, log);
		  DpsBaseClose(&P);
		  DPS_FREE(log_buf);

		  DpsLog(Indexer, DPS_LOG_DEBUG, "pas done: %d from %d to %d", log, from, to);
		  DPSSLEEP(sleeps);
		}
		DPS_FREE(del_buf);
		DpsAgentFree(Indexer);
		DPS_FREE(P.vardir);
	}

	fprintf(stderr, "Splitting done.\n");
	
	DpsEnvFree(Env);
	DpsDestroyMutexes();
	DpsDeInit();

#ifdef EFENCE
	fprintf(stderr, "Memory leaks checking\n");
	DpsEfenceCheckLeaks();
#endif
#ifdef FILENCE
	fprintf(stderr, "FD leaks checking\n");
	DpsFilenceCheckLeaks(NULL);
#endif
	return 0;
}
