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
#include "dps_utils.h"
#include "dps_spell.h"
#include "dps_cache.h"
#include "dps_boolean.h"
#include "dps_searchtool.h"
#include "dps_agent.h"
#include "dps_xmalloc.h"
#include "dps_stopwords.h"
#include "dps_proto.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_conf.h"
#include "dps_doc.h"
#include "dps_db.h"
#include "dps_db_int.h"
#include "dps_vars.h"
#include "dps_log.h"
#include "dps_mkind.h"
#include "dps_store.h"
#include "dps_hash.h"
#include "dps_sqldbms.h"
#include "dps_base.h"
#include "dps_signals.h"
#include "dps_socket.h"
#include "dps_url.h"
#include "dps_charsetutils.h"
#include "dps_word.h"
#include "dps_crossword.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif

#include <signal.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif

/*
#define DEBUG_SEARCH 1
*/
#define WAIT_TIME 36000 /* 10 hours */

/* uncomment this to enable MODE_ALL realisation via search limits */
/*#define MODE_ALL_VIA_LIMITS*/


typedef struct{
	urlid_t url_id;
	dps_uint4   wrd_id;
	dps_uint4   coord;
} T_URL_WRD_CRD;

typedef struct{
	dps_uint4	wrd_id;
	off_t	pos;
	size_t	len;
} T_DAT_IND;

static int cmp_urlid_t(const urlid_t *u1, const urlid_t *u2){
	if(*u1<*u2) return(-1);
	if(*u1>*u2) return(1);
	return(0);
}

/*
static int DpsCmpURL_CRD(const DPS_URL_CRD *cr1, const DPS_URL_CRD *cr2) {
  if (cr1->url_id < cr2->url_id) return -1;
  if (cr1->url_id > cr2->url_id) return 1;
  return 0;
}
*/

/******** Convert category string into 32 bit number *************/
void DpsDecodeHex8Str(const char *hex_str, dps_uint4 *hi, dps_uint4 *lo, dps_uint4 *fhi, dps_uint4 *flo){
  char str[33],str_hi[17],str_lo[17], *s;

        dps_strncpy(str, hex_str, 13);
	str[12] = '\0';
	dps_strcat(str,"000000000000");
	for(s = str; *s; s++) if (*s == '@') *s = '0';
	for(s = str; *s == '0'; s++) *s = ' ';
	dps_strncpy(str_hi,&str[0],6); str_hi[6]=0;
	dps_strncpy(str_lo,&str[6],6); str_lo[6]=0;
	
	*hi = (dps_uint4)strtol(str_hi, (char **)NULL, 36);
	*lo = (dps_uint4)strtol(str_lo, (char **)NULL, 36);

	if ((fhi != NULL) && (flo != NULL)) {
	  dps_strncpy(str, hex_str, 13);
	  str[12] = '\0';
	  dps_strcat(str,"ZZZZZZZZZZZZ");
	  dps_strncpy(str_hi, &str[0], 6); str_hi[6] = 0;
	  dps_strncpy(str_lo, &str[6], 6); str_lo[6] = 0;
	
	  *fhi = strtol(str_hi, (char **)NULL, 36);
	  *flo = strtol(str_lo, (char **)NULL, 36);

	}
}

/*************************** Sort functions **************************/
/* Function to sort LOGWORD list in (wrd_id,url_id,coord,time_stamp) order */
int DpsCmplog(const DPS_LOGWORD *s1, const DPS_LOGWORD *s2) {
/*	if(s1->wrd_id<s2->wrd_id)return(-1);
	if(s1->wrd_id>s2->wrd_id)return(1);
*/
	if(s1->url_id<s2->url_id)return(-1);
	if(s1->url_id>s2->url_id)return(1);
/*
	if(s1->coord<s2->coord)return(-1);
	if(s1->coord>s2->coord)return(1);
*/
	if(s2->stamp<s1->stamp)return(-1);
	if(s2->stamp>s1->stamp)return(1);

	return(0);
}
/* Function to sort LOGWORD list in (wrd_id,url_id) order */
int DpsCmplog_wrd(const DPS_LOGWORD *s1, const DPS_LOGWORD *s2) {
	if(s1->wrd_id < s2->wrd_id) return(-1);
	if(s1->wrd_id > s2->wrd_id) return(1);

	if(s1->url_id < s2->url_id) return(-1);
	if(s1->url_id > s2->url_id) return(1);

	if(s1->coord < s2->coord) return(-1);
	if(s1->coord > s2->coord) return(1);

	return(0);
}

/**
   Function to sort LOGDEL list in URL_ID order 
*/
int DpsCmpurldellog(const void *s1,const void *s2) {
  const DPS_LOGDEL *d1 = (const DPS_LOGDEL*)s1;
  const DPS_LOGDEL *d2 = (const DPS_LOGDEL*)s2;

  if (d1->url_id < d2->url_id) return -1;
  if (d1->url_id > d2->url_id) return 1;
  if (d1->stamp < d2->stamp) return -1;
  if (d1->stamp > d2->stamp) return 1;

  return 0;
}

/*
static int DpsCmp_urlid_t(const urlid_t *s1, const urlid_t *s2) {
  if (*s1 < *s2) return -1;
  if (*s1 > *s2) return 1;
  return 0;
}
*/


static int DpsLogdInit(DPS_AGENT *Agent, DPS_DB *db, char* var_dir, size_t i, int shared);

int DpsOpenCache(DPS_AGENT *A, int shared) {
	DPS_DB		*db;
	const char	*vardir = DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
	size_t i, dbfrom = 0, dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;

	DpsLog(A, DPS_LOG_DEBUG, "DpsOpenCache:");

	if (A->Demons.Demon == NULL) {
	  A->Demons.nitems = (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	  A->Demons.Demon = (DPS_DEMONCONN*)DpsXmalloc(A->Demons.nitems * sizeof(DPS_DEMONCONN) + 1);
	  if(A->Demons.Demon == NULL) {
	    DpsLog(A, DPS_LOG_ERROR, "CacheD can't alloc at %s:%d", __FILE__, __LINE__);
	    return DPS_ERROR;
	  }
	}

	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  if(db->DBMode != DPS_DBMODE_CACHE) continue;

	  DpsLog(A, DPS_LOG_DEBUG, "i:%d  cached_sd:%d  sin_port:%d", i, A->Demons.Demon[i].cached_sd, db->cached_addr.sin_port);
	
	  if(A->Demons.Demon[i].cached_sd == 0) {
	    if (db->cached_addr.sin_port != 0) {
	      char port_str[16];
	      struct sockaddr_in dps_addr;
	      unsigned char *p = (unsigned char*)&dps_addr.sin_port;
	      unsigned int ip[2];
      
	      if((A->Demons.Demon[i].cached_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "CacheD ERR socket_sd");
		return DPS_ERROR;
	      }
  
	      if((A->Demons.Demon[i].cached_rv = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "CacheD ERR socket_rv");
		return DPS_ERROR;
	      }

	      DpsSockOpt(A, A->Demons.Demon[i].cached_sd);
	      DpsSockOpt(A, A->Demons.Demon[i].cached_rv);
  
	      if(connect(A->Demons.Demon[i].cached_sd, (struct sockaddr *)&db->cached_addr, sizeof(db->cached_addr)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "CacheD ERR connect to %s", inet_ntoa(db->cached_addr.sin_addr));
		return DPS_ERROR;
	      }

	      /* revert connection */

	      if (sizeof(port_str) != DpsRecvall(A->Demons.Demon[i].cached_sd, port_str, sizeof(port_str), 360)) {
		dps_strerror(A, DPS_LOG_ERROR, "CacheD ERR receiving port data");
		return DPS_ERROR;
	      }
	      dps_addr = db->cached_addr;
	      dps_addr.sin_port = 0;
	      sscanf(port_str, "%u,%u", ip, ip + 1);
	      p[0] = (unsigned char)(ip[0] & 255);
	      p[1] = (unsigned char)(ip[1] & 255);

	      DpsLog(A, DPS_LOG_DEBUG, "[%s] PORT: %s, decimal:%d", inet_ntoa(db->cached_addr.sin_addr), port_str, ntohs(dps_addr.sin_port));

	      if(connect(A->Demons.Demon[i].cached_rv, (struct sockaddr *)&dps_addr, sizeof(dps_addr)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Cached ERR revert connect to %s:%d", inet_ntoa(dps_addr.sin_addr), ntohs(dps_addr.sin_port));
		return DPS_ERROR;
	      }

/********************/
/************************/

	    } else {
		if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
		if(DPS_OK != DpsLogdInit(A, db, (db->vardir) ? db->vardir : vardir, i, shared)) {
		    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
		    DpsLog(A, DPS_LOG_ERROR, "OpenCache error");
		    return DPS_ERROR;
		}
		if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    }
	  }
	  DpsLog(A, DPS_LOG_DEBUG, "wrd_buf: %x", db->LOGD.wrd_buf);
	}
	DpsLog(A, DPS_LOG_DEBUG, "Done.");

	return(DPS_OK);
}

static int DpsLogdCloseLogs(DPS_AGENT *Agent);

int DpsCloseCache(DPS_AGENT *A, int shared, int last) {
  DPS_DB *db;
  const char	*vardir = DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
  size_t i, dbfrom = 0, dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
  int res;

  TRACE_IN(A, "DpsCloseCache");
  res = DpsLogdCloseLogs(A);

  for (i = dbfrom; i < dbto; i++) {
      db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
      if(db->DBMode!=DPS_DBMODE_CACHE) continue;
      if(db->logd_fd > 0) {
	  dps_closesocket(db->logd_fd);
	  res =  DPS_OK;
      } else if (last || !(A->flags & DPS_FLAG_UNOCON)) {
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	  res = DpsLogdClose(A, db, (db->vardir) ? db->vardir : vardir, i, shared);
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
      }
      if (res != DPS_OK) break;
  }
  TRACE_OUT(A);
  return res;
}


void DpsRotateDelLog(DPS_AGENT *A) {
  DPS_DB *db;
  char del_log_name[PATH_MAX];
  int split_fd, nbytes, log_fd;
  size_t i, dbfrom = 0, dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
  size_t NFiles, log_num;

  TRACE_IN(A, "DpsRotateDelLog");

  for (i = dbfrom; i < dbto; i++) {
    db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
    if(db->DBMode != DPS_DBMODE_CACHE) continue;
    if (db->del_fd <= 0) continue;

    NFiles = (db->WrdFiles > 0) ? (int)db->WrdFiles : DpsVarListFindInt(&A->Vars, "WrdFiles", 0x300);
    for(log_num = 0; log_num < NFiles; log_num++) {

      dps_snprintf(del_log_name, sizeof(del_log_name), "%s%s%03X-split.log", db->log_dir, DPSSLASHSTR, log_num);
	if((split_fd = DpsOpen3(del_log_name, O_WRONLY | O_CREAT | O_APPEND | DPS_BINARY, DPS_IWRITE)) == -1) {
	    if (errno == ENOENT) { /* So, the *split.log file dowsn't exists, we move it for fast work, but there may be some collisions */
		char old_log_name[PATH_MAX];
		dps_snprintf(old_log_name, sizeof(old_log_name), "%s%s%03X.log", db->log_dir, DPSSLASHSTR, log_num);
		if (-1 == rename(old_log_name, del_log_name)) {
		    if (errno != ENOENT) {
			dps_strerror(A, DPS_LOG_ERROR, "Can't rename '%s' into '%s'", old_log_name, del_log_name);
			return;
		    }
		}
	    } else {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s' for writing", del_log_name);
		return;
	    }
	} else {

	    dps_snprintf(del_log_name, sizeof(del_log_name), "%s%s%03X.log", db->log_dir, DPSSLASHSTR, log_num);
	    if((log_fd = DpsOpen3(del_log_name, O_RDWR | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s' for writing", del_log_name);
		return;
	    }

	    DpsWriteLock(log_fd);
  
	    lseek(log_fd, (off_t)0, SEEK_SET);
	    while((nbytes = read(log_fd, del_log_name, PATH_MAX)) > 0) {
		(void)write(split_fd, del_log_name, (size_t)nbytes);
	    }
	    DpsClose(split_fd);
	    lseek(log_fd, (off_t)0, SEEK_SET);
	    (void)ftruncate(log_fd, (off_t)0);
  
	    DpsUnLock(log_fd);
	    DpsClose(log_fd);
	}
    }

    dps_snprintf(del_log_name, sizeof(del_log_name), "%s%s%s", db->log_dir, DPSSLASHSTR, "del-split.log");

    if((split_fd = DpsOpen3(del_log_name, O_WRONLY | O_CREAT | O_APPEND | DPS_BINARY, DPS_IWRITE)) == -1) {
      dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s' for writing", del_log_name);
      return;
    }

    DpsWriteLock(db->del_fd);
  
    lseek(db->del_fd, (off_t)0, SEEK_SET);
    while((nbytes = read(db->del_fd, del_log_name, PATH_MAX)) > 0) {
      (void)write(split_fd, del_log_name, (size_t)nbytes);
    }
    DpsClose(split_fd);
    lseek(db->del_fd, (off_t)0, SEEK_SET);
    (void)ftruncate(db->del_fd, (off_t)0);
  
    DpsUnLock(db->del_fd);
  }
  TRACE_OUT(A);
}


static int PresentInDelLog(DPS_LOGDEL *buf, size_t buf_count, size_t *start, const urlid_t url_id) {
	register size_t m, l, r;
	urlid_t umin, umax;
	
	if (buf == NULL || buf_count == 0) return 0;
	if (start) l = *start;
	else l = 0;
	r = buf_count - 1;

	umin = buf[l].url_id;
	umax = buf[r].url_id;

	while(1) {
	  if (l > r || url_id > umax) {
	    if(start) *start = r;
	    return 0;
	  }
	  if (url_id < umin) {
	    return 0;
	  }
	  if (l == r) m = r;
	  else m = l + (urlid_t)((r - l - 1) * ((dps_uint8)(url_id - umin)) / (umax -  umin));
	  if (buf[m].url_id > url_id) {
	    r = m - 1;
	    umax = buf[r].url_id;
	  } else if (buf[m].url_id < url_id) {
	    l = m + 1;
	    umin = buf[l].url_id;
	  } else {
	    if(start) *start = m;
	    return buf[m].stamp;
	  }
	  
	}
	return 0;
}

/***** Write a marker that the old content of url_id should be deleted ****/
int DpsDeleteURLFromCache(DPS_AGENT * Indexer, urlid_t url_id, DPS_DB *db){
	int sent, cached_sd, cached_rv;
	ssize_t recvt;
	DPS_LOGD_CMD cmd;
	char reply;

	TRACE_IN(Indexer, "DpsDeleteURLFromCache");

	cmd.stamp = Indexer->now;
	cmd.url_id = url_id;
	cmd.cmd = DPS_LOGD_CMD_DELETE;
	cmd.nwords=0;

	cached_sd = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_sd : 0;
	cached_rv = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_rv : 0;

	if(cached_sd){

		sent = DpsSend(cached_sd, &cmd, sizeof(cmd), 0);
		if(sent!=sizeof(cmd)){
		  dps_strerror(Indexer, DPS_LOG_ERROR, "%s [%d] Can't write to cached", __FILE__, __LINE__);
		  TRACE_OUT(Indexer);
		  return DPS_ERROR;
		}
		while ((recvt = DpsRecvall(cached_rv, &reply, sizeof(char), WAIT_TIME)) != 1) {
		  if (recvt <= 0) {
		    dps_strerror(Indexer, DPS_LOG_ERROR, "Can't receive from cached [%d] %d", __LINE__, recvt);
		    TRACE_OUT(Indexer);
		    return DPS_ERROR;
		  }
		  DPSSLEEP(0);
		}
		if (reply != 'O') {
		  DpsLog(Indexer, DPS_LOG_ERROR, "Incorrect reply from cached %s:%d",__FILE__, __LINE__);
		  TRACE_OUT(Indexer);
		  return DPS_ERROR;
		}
	}else{
		if(DPS_OK != DpsLogdStoreDoc(Indexer, cmd, NULL, db)) {
			TRACE_OUT(Indexer);
			return(DPS_ERROR);
		}
	}
	TRACE_OUT(Indexer);
	return(DPS_OK);
}

int DpsStoreWordsCache(DPS_AGENT * Indexer, DPS_DOCUMENT *Doc, DPS_DB *db) {
#ifdef HAVE_SQL
	DPS_LOGD_CMD	cmd;
	DPS_LOGD_WRD	*wrd;
	DPS_SQLRES	SQLres;
	size_t		sent;
	ssize_t         recvt;
	size_t		i, curwrd, lcslen;
	urlid_t		url_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	int             cached_sd, cached_rv, rc;
	int		prev_status = DpsVarListFindInt(&Doc->Sections, "PrevStatus", 0);
	char            *lcsword;
	char		reply;

	TRACE_IN(Indexer, "DpsStoreWordsCache");
/*	fprintf(stderr, "[%d] DpsStoreWordsCache: %d words, urlid_t: %d\n", Indexer->handle, Doc->Words.nwords, url_id);*/
	
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	fprintf(Indexer->TR, "[%d] DpsStoreWordsCache: %d words, urlid_t: %d\n", Indexer->handle, Doc->Words.nwords, url_id);
	fflush(Indexer->TR);
#endif
	/* Mark that old content should be deleted    */
	/* Note that this should be done every time   */
	/* even if previous status=0, i.e. first time */
	/* indexing                                   */
	
/*	DpsLog(Indexer, DPS_LOG_DEBUG, "DpsStoreWordsCache: %d words, urlid_t: %d\n", Indexer->handle, Doc->Words.nwords, url_id);*/

	cmd.nwords = Doc->Words.nwords;
	if (Indexer->Flags.use_crosswords) {
	  char buf[128];
	  DpsSQLResInit(&SQLres);
	  dps_snprintf(buf, sizeof(buf), "SELECT word_id,intag FROM ncrossdict WHERE url_id=%d", url_id);
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	  rc = DpsSQLQuery(db, &SQLres, buf);
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	  if (rc != DPS_OK) {
	    DpsSQLFree(&SQLres);
	    TRACE_OUT(Indexer);
	    return (DPS_OK);
	  }
	  cmd.nwords += DpsSQLNumRows(&SQLres);
	}

	cmd.stamp = Indexer->now;
	cmd.url_id = url_id;
	cmd.cmd = (prev_status == 0) ? DPS_LOGD_CMD_NEWORD : DPS_LOGD_CMD_WORD;

	wrd = (DPS_LOGD_WRD*)DpsMalloc((cmd.nwords + 1) * sizeof(DPS_LOGD_WRD));
	if (wrd == NULL) {
	  DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc memory for %d words (%d bytes) [%s:%d]", 
		 cmd.nwords, cmd.nwords * sizeof(DPS_LOGD_WRD), __FILE__, __LINE__);
	  if (Indexer->Flags.use_crosswords) DpsSQLFree(&SQLres);
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}

	lcslen = 12 * Indexer->WordParam.max_word_len;
	if ((lcsword = (char*)DpsMalloc(lcslen + 1)) == NULL) { 
	  DPS_FREE(wrd);
	  if (Indexer->Flags.use_crosswords) DpsSQLFree(&SQLres);
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}
	lcsword[lcslen] = '\0';

	for(curwrd = i = 0; i < Doc->Words.nwords; i++) {
	        if ((wrd[curwrd].coord = Doc->Words.Word[i].coord) == 0) continue;

		DpsConv(&Indexer->uni_lc, lcsword, lcslen, 
			(char*)Doc->Words.Word[i].uword, sizeof(dpsunicode_t) * (Doc->Words.Word[i].ulen + 1));

		wrd[curwrd].wrd_id = DpsStrHash32(lcsword);

/*		DpsLog(Indexer, DPS_LOG_DEBUG, " -- url_id:%d coord:%x - %s %d(%x) 0x%x", 
		       url_id, wrd[curwrd].coord,
		       lcsword, wrd[curwrd].wrd_id, wrd[curwrd].wrd_id,
		       DPS_FILENO(wrd[curwrd].wrd_id, 0x300));*/
		curwrd++;
	}
	if (Indexer->Flags.use_crosswords) {
	  dps_uint4 posadd = (curwrd) ? (wrd[curwrd-1].coord & 0xFFFF0000) : 0;
	  for (i = 0; i < DpsSQLNumRows(&SQLres); i++) {
	    wrd[curwrd].coord = DPS_ATOI(DpsSQLValue(&SQLres, i, 1));
	    if (wrd[curwrd].coord == 0) continue;
	    wrd[curwrd].coord += posadd;
	    wrd[curwrd].wrd_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 0));
	    curwrd++;
	  }
	  DpsSQLFree(&SQLres);
	}
	cmd.nwords = curwrd;

	cached_sd = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_sd : 0;
	cached_rv = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_rv : 0;

#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	fprintf(Indexer->TR, "[%d] DpsStoreWordsCache: cached_sd %d\n", Indexer->handle, cached_sd);
	fflush(Indexer->TR);
#endif
	if(cached_sd){
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(Indexer->TR, "[%d] DpsStoreWordsCache: sending cmd\n", Indexer->handle);
	  fflush(Indexer->TR);
#endif
		sent = DpsSend(cached_sd, &cmd, sizeof(cmd), 0);
		if(sent!=sizeof(cmd)){
		  dps_strerror(Indexer, DPS_LOG_ERROR, "%s [%d] Can't write to cached", __FILE__, __LINE__);
		  DPS_FREE(wrd); DPS_FREE(lcsword);
		  TRACE_OUT(Indexer);
		  return DPS_ERROR;
		}
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(Indexer->TR, "[%d] DpsStoreWordsCache: receiving reply for cmd\n", Indexer->handle);
	  fflush(Indexer->TR);
#endif
	  while ((recvt = DpsRecvall(cached_rv, &reply, 1, WAIT_TIME)) != 1) {
	    if (recvt <= 0) {
	      dps_strerror(Indexer, DPS_LOG_ERROR, "Can't receive from cached [%d] %d", __LINE__, recvt);
	      DPS_FREE(wrd); DPS_FREE(lcsword);
	      TRACE_OUT(Indexer);
	      return DPS_ERROR;
	    }
	    DPSSLEEP(0);
	  }
	  if (reply != 'O') {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Incorrect reply [%c] received from cached %s:%d", reply, __FILE__, __LINE__);
	    DPS_FREE(wrd); DPS_FREE(lcsword);
	    TRACE_OUT(Indexer);
	    return DPS_ERROR;
	  }
	  if (cmd.nwords > 0) {
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	    fprintf(Indexer->TR, "[%d] DpsStoreWordsCache: sending wrd\n", Indexer->handle);
	    fflush(Indexer->TR);
#endif
		  sent = DpsSend(cached_sd, wrd, cmd.nwords * sizeof(DPS_LOGD_WRD), 0);
		  if(sent != cmd.nwords * sizeof(DPS_LOGD_WRD)){
		    dps_strerror(Indexer, DPS_LOG_ERROR, "[%s:%d] Can't write (%d of %d) to cached", 
			   __FILE__, __LINE__, sent, cmd.nwords * sizeof(DPS_LOGD_WRD));
		    DPS_FREE(wrd); DPS_FREE(lcsword);
		    TRACE_OUT(Indexer);
		    return DPS_ERROR;
		  }
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(Indexer->TR, "[%d] DpsStoreWordsCache: receiving reply for wrd\n", Indexer->handle);
	  fflush(Indexer->TR);
#endif
	  while ((recvt = DpsRecvall(cached_rv, &reply, sizeof(char), WAIT_TIME)) != 1) {
	    if (recvt <= 0) {
	      DpsLog(Indexer, DPS_LOG_ERROR, "Can't receive from cached %s:%d", __FILE__, __LINE__);
	      DPS_FREE(wrd); DPS_FREE(lcsword);
	      TRACE_OUT(Indexer);
	      return DPS_ERROR;
	    }
	    DPSSLEEP(0);
	  }
	  if (reply != 'O') {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Incorrect reply received from cached %s:%d", __FILE__, __LINE__);
	    DPS_FREE(wrd); DPS_FREE(lcsword);
	    TRACE_OUT(Indexer);
	    return DPS_ERROR;
	  }
	  }
	}else{
		if(DPS_OK != DpsLogdStoreDoc(Indexer, cmd, wrd, db)) {
			DPS_FREE(wrd); DPS_FREE(lcsword);
			TRACE_OUT(Indexer);
			return(DPS_ERROR);
		}
	}
	DPS_FREE(wrd); DPS_FREE(lcsword);
	TRACE_OUT(Indexer);
#endif
	return(DPS_OK);
}

/***************** split one log into cache *****************/

/* 
This function removes duplicate
(wrd_id,url_id) pairs using timestamps.
Most recent pairs will remane.
*/

__C_LINK int __DPSCALL DpsRemoveDelLogDups(DPS_LOGDEL *words, size_t n) {
	size_t i, j;
	
	j = 0;
	for(i = 1; i < n; i++) {
		if(words[j].url_id != words[i].url_id) {
			j++;
		}
		if(i != j) words[j] = words[i];
	}
	return(j+1);
}

/**< 
This function removes non-fresh  
words from DPS_LOGWORD array using timestamps
from delete log. Only those (word_id,url_id)
pairs which are newer than (url_id) from 
delete log will remane in "words" array 
*/

#if 0

size_t DpsRemoveOldWords(DPS_LOGWORD *words, size_t n, DPS_LOGDEL *del, size_t del_count) {
  size_t i, j = 0;
  size_t start = 0;
/*  int u;*/
  urlid_t curl;
  long curstamp;

  if (del_count == 0) return n;

  for (i = 0; i < n; ) {
  
/*    u = (PresentInDelLog(del, del_count, &start, words[i].url_id) <= words[i].stamp);*/
    curstamp = PresentInDelLog(del, del_count, &start/*NULL*/, words[i].url_id);
    curl = words[i].url_id;
/*    curstamp = words[i].stamp;*/
    for( ; (i < n) && (words[i].url_id == curl) /*&& (words[i].stamp == curstamp)*/; i++) {
      if (words[i].stamp >= curstamp) words[j++] = words[i];
    }
  }

  return j;
}

#else

size_t DpsRemoveOldWords(DPS_LOGWORD *words, size_t n, DPS_LOGDEL *del, size_t del_count) {
  register size_t i, j = 0;
  size_t start = 0;

  if (del_count == 0 || n == 0) return n;

  while(j < n && words[j].url_id < del[start].url_id) j++;
  i = j;
  do {
    while(i < n && words[i].url_id == del[start].url_id) {
      if (words[i].stamp >= del[start].stamp) {
	if (i != j) words[j] = words[i];
	j++;
      } 
      i++;
    }
    if (i == n) break;
    if (++start == del_count) break;
    while(i < n && words[i].url_id < del[start].url_id) {
      if (i != j) words[j] = words[i];
      j++;
      i++;
    }
  } while(1);
  if (i < n) { 
    if (j != i) dps_memmove(words + j, words + i, (n - i) * sizeof(DPS_URL_CRD));
    j += (n - i);
  }

  return j;
}
#endif


#if 1

static size_t RemoveOldCrds(DPS_URL_CRD *words, size_t n, DPS_LOGDEL *del, size_t del_count) {
  register size_t i, j = 0;
  size_t start = 0;

  if (del_count == 0 || n == 0) return n;

  while(j < n && words[j].url_id < del[start].url_id) j++;
  i = j;
  do {
    while(i < n && words[i].url_id == del[start].url_id) i++;
    if (i == n) break;
    if (++start == del_count) break;
    while(i < n && words[i].url_id < del[start].url_id) {
      if (i != j) words[j] = words[i];
      j++;
      i++;
    }
  } while(1);
  if (i < n) {
    if (j != i)dps_memmove(words + j, words + i, (n - i) * sizeof(DPS_URL_CRD));
    j += (n - i);
  }

  return j;
}

#else

static size_t RemoveOldCrds(DPS_URL_CRD_DB *words0, size_t n, DPS_LOGDEL *del0, size_t del_count) {
  register DPS_URL_CRD_DB *words = words0, *c = words0, *wordsn = words0 + n;
  register DPS_LOGDEL *del = del0, *deln = del0 + del_count;

  if ((n == 0) || (del_count == 0)) return n;

  while((del < deln) && (del->url_id < words->url_id)) del++;
  while((del < deln) && (words < wordsn)) {
    while((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
    while((words < wordsn) && (words->url_id == del->url_id)) words++;
    del++;
  }

/*
  if (del_count) {
    register size_t w = (del_count + 7) / 8;
    switch (del_count % 8) {
    case 0: do { while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
     	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
    case 7:      while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
    case 6:      while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
    case 5:      while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
    case 4:      while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
    case 3:      while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
    case 2:      while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
    case 1:      while ((words < wordsn) && (words->url_id < del->url_id)) *c++ = *words++;
	         while ((words < wordsn) && (words->url_id == del->url_id)) words++;
	         if (words == wordsn) break;
	         del++;
            } while (--w > 0);
    }
  }
*/
  if (n && (c != words) && (words < wordsn)) {
    register size_t w = wordsn - words;
    register l = (w + 7) / 8;
    switch(w % 8) {
    case 0: do { *c++ = *words++;
    case 7:      *c++ = *words++;
    case 6:      *c++ = *words++;
    case 5:      *c++ = *words++;
    case 4:      *c++ = *words++;
    case 3:      *c++ = *words++;
    case 2:      *c++ = *words++;
    case 1:      *c++ = *words++;
            } while (--l > 0);
    }
  }
  return c - words0;
}
#endif


/**< 
This function removes non-fresh records form DPS_URL_CRD array using timestamps from delete log.
*/
/*
static size_t RemoveOldURLCRD(DPS_URL_CRD *Coords, size_t n, DPS_LOGDEL *del, size_t del_count) {
  size_t i, j = 0;
  int u;
  urlid_t curl;

  if (del_count == 0) return n;

  for (i = 0; i < n; ) {
    u = (PresentInDelLog(del, del_count, NULL, Coords[i].url_id) == 0);
    curl = Coords[i].url_id;
    for (; (i < n) && (Coords[i].url_id == curl); i++) {
      if (u) Coords[j++] = Coords[i];
    }
  }

  return j;
}
*/

int DpsClearCacheTree(DPS_ENV * Conf){
	int		i;
	char		fname[PATH_MAX];
	const char	*vardir=DpsVarListFindStr(&Conf->Vars,"VarDir", DPS_VAR_DIR);
	const int       NFiles = DpsVarListFindInt(&Conf->Vars, "WrdFiles", 0x300);
	const int       URLFiles = DpsVarListFindInt(&Conf->Vars, "URLDataFiles", 0x300);

/* FixME: add removal for cache mode limits also */

/* for old mnogo versions	
	for(i = 0; i < DPS_MAX_LOG; i++){
		dps_snprintf(fname,sizeof(fname),"%s%s%s%c%03X.dat",vardir,DPSSLASHSTR,DPS_TREEDIR,DPSSLASH,i);
		unlink(fname);
		dps_snprintf(fname,sizeof(fname),"%s%s%s%c%03X.ind",vardir,DPSSLASHSTR,DPS_TREEDIR,DPSSLASH,i);
		unlink(fname);
	}
*/
	for(i = 0; i < NFiles; i++){
		dps_snprintf(fname,sizeof(fname),"%s%s%s%cwrd%04x.s", vardir, DPSSLASHSTR, DPS_TREEDIR, DPSSLASH, i);
		unlink(fname);
		dps_snprintf(fname,sizeof(fname),"%s%s%s%cwrd%04x.i", vardir, DPSSLASHSTR, DPS_TREEDIR, DPSSLASH, i);
		unlink(fname);
	}
	for(i = 0; i < URLFiles; i++){
		dps_snprintf(fname,sizeof(fname),"%s%s%s%cinfo%04x.s", vardir, DPSSLASHSTR, DPS_URLDIR, DPSSLASH, i);
		unlink(fname);
		dps_snprintf(fname,sizeof(fname),"%s%s%s%cinfo%04x.i", vardir, DPSSLASHSTR, DPS_URLDIR, DPSSLASH, i);
		unlink(fname);
		dps_snprintf(fname,sizeof(fname),"%s%s%s%cdata%04x.s", vardir, DPSSLASHSTR, DPS_URLDIR, DPSSLASH, i);
		unlink(fname);
		dps_snprintf(fname,sizeof(fname),"%s%s%s%cdata%04x.i", vardir, DPSSLASHSTR, DPS_URLDIR, DPSSLASH, i);
		unlink(fname);
		dps_snprintf(fname, sizeof(fname), "%s%c%s%cdata%04x.dat", vardir, DPSSLASH, DPS_URLDIR, DPSSLASH, i);
		unlink(fname);
	}
	return(0);
}

typedef struct {
  urlid_t rec_id;
  int flag;
} DPS_TODEL;

static int cmp_todel(const DPS_TODEL *t1, const DPS_TODEL *t2) {
  if((unsigned long)t1->rec_id < (unsigned long)t2->rec_id) return -1;
  if((unsigned long)t1->rec_id > (unsigned long)t2->rec_id) return 1;
	return 0;
}



__C_LINK int __DPSCALL DpsProcessBuf(DPS_AGENT *Indexer, DPS_BASE_PARAM *P, size_t log_num,
				     DPS_LOGWORD *log_buf, size_t n, DPS_LOGDEL *del_buf, size_t del_count) {
  DPS_URL_CRD *data;
  size_t i, j, p, z, len, n_add, n_old, n_old_new;
  DPS_TODEL *todel = (DPS_TODEL*)DpsMalloc(1024 * sizeof(*todel)), *startdel, *enddel;
  size_t ndel = 0, mdel = 1024;

#ifdef DEBUG_SEARCH
  unsigned long total_ticks = DpsStartTimer();
#endif

  TRACE_IN(Indexer, "DpsProcessBuf");
  if ((n == 0) && (del_count == 0)) {
    DPS_FREE(todel);
    TRACE_OUT(Indexer);
    return DPS_OK;
  }

  P->rec_id = (log_num << DPS_BASE_BITS);
  if (DpsBaseSeek(P, DPS_WRITE_LOCK) != DPS_OK) {
      DpsLog(Indexer, DPS_LOG_ERROR, "Can't open base %s/%s {%s:%d}", P->subdir, P->basename, __FILE__, __LINE__);
      DpsBaseClose(P);
      DPS_FREE(todel);
      TRACE_OUT(Indexer);
      return DPS_ERROR;
  }

    if (lseek(P->Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
      DpsLog(Indexer, DPS_LOG_ERROR, "Can't seeek for file %s at %s[%d]", P->Ifilename, __FILE__, __LINE__);
      DpsBaseClose(P);
      DPS_FREE(todel);
      TRACE_OUT(Indexer);
      return DPS_ERROR;
    }
    while (read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) == sizeof(DPS_BASEITEM)) {
      if ((P->Item.rec_id != 0) && (P->Item.size > 0)) {
	if (ndel >= mdel) {
	  mdel += 1024;
	  todel = (DPS_TODEL*)DpsRealloc(todel, mdel * sizeof(*todel));
	  if (todel == NULL) { 
	    DpsBaseClose(P);
	    DPS_FREE(todel);
	    TRACE_OUT(Indexer);
	    return DPS_ERROR;
	  }
	}
	todel[ndel].rec_id = P->Item.rec_id;
	todel[ndel].flag = 0;
	ndel++;
      }
    }

#ifdef DEBUG_SEARCH
    DpsLog(Indexer, DPS_LOG_EXTRA, "-- del read in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif

    if (ndel > 1) {
      DpsSort(todel, ndel, sizeof(*todel), (qsort_cmp)cmp_todel);
      for (j = i = 1; i < ndel; i++) {
	if (todel[i].rec_id != todel[i-1].rec_id) {
	  if (i != j) todel[j] = todel[i];
	  j++;
	}
      }
      ndel = j;
    }

#ifdef DEBUG_SEARCH
    DpsLog(Indexer, DPS_LOG_EXTRA, "-- del sorted in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif

    startdel = todel;
    enddel = (ndel > 1) ? todel + (ndel - 1) : todel; 
    for (i = 0; i < n; i += n_add) {
      for(n_add = 1; (i + n_add < n) && (log_buf[i].wrd_id == log_buf[i + n_add].wrd_id); n_add++);
/**************/
      P->rec_id = log_buf[i].wrd_id;
      if (ndel > 0) {
	while(startdel < enddel && (unsigned long)startdel->rec_id < (unsigned long)P->rec_id) startdel++;
	if (startdel->rec_id == P->rec_id) startdel->flag = 1;
      }
      if ((data = (DPS_URL_CRD*)DpsBaseARead(P, &len)) == NULL) {
	len = 0;
	data = (DPS_URL_CRD*)DpsMalloc(n_add * sizeof(DPS_URL_CRD));
	if (data == NULL) {
	  DPS_FREE(todel);
	  DpsBaseClose(P);
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}
	n_old = 0;
      } else {
#ifdef DEBUG_SEARCH
	DpsLog(Indexer, DPS_LOG_EXTRA, "-- old read in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif
	data = (DPS_URL_CRD*)DpsRealloc(data, len + n_add * sizeof(DPS_URL_CRD));
	if (data == NULL) {
	  DPS_FREE(todel);
	  DpsBaseClose(P);
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}
	n_old = len / sizeof(DPS_URL_CRD);
	n_old = RemoveOldCrds(data, n_old, del_buf, del_count);
      }
#if 0
      fprintf(stderr, "==== LOG_BUF %x ====\n", log_buf[i].wrd_id);

      {
	size_t q;
	for (q = 0; q < n_add; q++) {
	  fprintf(stderr, "l.url_id:%d  .coord:%d\n", log_buf[i + q].url_id, log_buf[i + q].coord);
	}
      }
#endif

      for(p = n_add + n_old - 1, j = n_old, z = n_add; (j > 0) && (z > 0); ) {
	if ( (data[j - 1].url_id > log_buf[i + z - 1].url_id) ||
	     ((data[j - 1].url_id == log_buf[i + z - 1].url_id) && (data[j - 1].coord > log_buf[i + z - 1].coord)) ) {
	  data[p--] = data[j - 1]; j--;
	} else {
	  data[p].url_id = log_buf[i + z - 1].url_id;
	  data[p--].coord = log_buf[i + z - 1].coord; z--;
	}
      }
      while(z > 0) {
	data[p].url_id = log_buf[i + z - 1].url_id; 
	data[p--].coord = log_buf[i + z - 1].coord; z--;
      }

#if 0
      fprintf(stderr, "==== MERGED ====\n");

      {
	DPS_URL_CRD *w = data;
	size_t q;
	for (q = 0; q < n_add + n_old; q++) {
	  fprintf(stderr, ".url_id:%d  .coord:%d\n", w[q].url_id, w[q].coord);
	}
      }
#endif

#ifdef DEBUG_SEARCH
    DpsLog(Indexer, DPS_LOG_EXTRA, "-- merged in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif

      P->rec_id = log_buf[i].wrd_id;
      if (DPS_OK != DpsBaseWrite(P, data, (n_old + n_add) * sizeof(DPS_URL_CRD))) {
	DPS_FREE(data);
	DpsLog(Indexer, DPS_LOG_ERROR, "Can't write base %s/%s {%s:%d}", P->subdir, P->basename, __FILE__, __LINE__);
	DpsBaseClose(P);
	DPS_FREE(todel);
	TRACE_OUT(Indexer);
	return DPS_ERROR;
      }
      DPS_FREE(data);

#ifdef DEBUG_SEARCH
      DpsLog(Indexer, DPS_LOG_EXTRA, "-- written in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif

    }
/*************/

    if (del_count > 0)
      for (i = 0; i < ndel; i++) {
/**************/
      if (todel[i].flag > 0) continue;
      P->rec_id = todel[i].rec_id;
      if ((data = (DPS_URL_CRD*)DpsBaseARead(P, &len)) == NULL) {
	P->rec_id = todel[i].rec_id;
	DpsBaseDelete(P);
#ifdef DEBUG_SEARCH
	DpsLog(Indexer, DPS_LOG_EXTRA, "-- flaged read/deleted in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif
      } else {
	n_old = len / sizeof(DPS_URL_CRD);
/*	DpsSortSearchWordsByURL(data, n_old);*/
	n_old_new = RemoveOldCrds(data, n_old, del_buf, del_count);
#ifdef DEBUG_SEARCH
	DpsLog(Indexer, DPS_LOG_EXTRA, "-- flaged read/cleaned in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif
	P->rec_id = todel[i].rec_id;
	if (n_old_new != n_old) {
	  if (n_old_new) DpsBaseWrite(P, data, n_old_new * sizeof(DPS_URL_CRD));
	  else DpsBaseDelete(P);
#ifdef DEBUG_SEARCH
	  DpsLog(Indexer, DPS_LOG_EXTRA, "-- flaged written in %.4f sec.", (float)(DpsStartTimer() - total_ticks) / 1000);
#endif
	}
      }
      DPS_FREE(data);
/*************/
    }
    DpsBaseClose(P);
    DPS_FREE(todel);


#ifdef DEBUG_SEARCH
    total_ticks = DpsStartTimer() - total_ticks;
    DpsLog(Indexer, DPS_LOG_EXTRA, "Log %03X updated in %.2f sec., nwrd:%d, ndel:%d", log_num, (float)total_ticks / 1000, n, ndel);
    if (DpsNeedLog(DPS_LOG_EXTRA)) dps_setproctitle("Log %03X updated in %.2f sec., ndel:%d", log_num, (float)total_ticks / 1000, ndel);
#else
    DpsLog(Indexer, DPS_LOG_EXTRA, "Log %03X updated, nwrd:%d, ndel:%d", log_num, n, ndel);
    if (DpsNeedLog(DPS_LOG_EXTRA)) dps_setproctitle("Log %03X updated", log_num);
#endif

    TRACE_OUT(Indexer);
    return DPS_OK;
}





/****************** Search stuff ************************************/
/*
static int cmp_hex8_ind(const DPS_UINT8_POS_LEN *c1, const DPS_UINT8_POS_LEN *c2){
	dps_uint4 n1=c1->hi;
	dps_uint4 n2=c2->hi;

	if(n1==n2){
		n1=c1->lo; 
		n2=c2->lo;
	}
	if(n1<n2) return(-1);
	if(n1>n2) return(1);
	return(0);
}
*/

static int cmp_hex4_ind(const DPS_UINT4_POS_LEN *c1, const DPS_UINT4_POS_LEN *c2){
	if(c1->val<c2->val) return(-1);
	if(c1->val>c2->val) return(1);
	return(0);
}

int __DPSCALL DpsAddSearchLimit(DPS_AGENT *Agent, int type, const char *file_name, const char *val){
	dps_uint4 hi, lo, f_hi, f_lo;
	char *str = (char *)DpsMalloc(dps_strlen(val) + 7);
	
	TRACE_IN(Agent, "DpsAddSearchLimit");

	if ((Agent->limits = (DPS_SEARCH_LIMIT*)DpsRealloc(Agent->limits, (Agent->nlimits + 1) * sizeof(DPS_SEARCH_LIMIT))) == NULL) {
	  DPS_FREE(str);
	  TRACE_OUT(Agent);
	  return DPS_ERROR;
	}

	DpsUnescapeCGIQuery(str, val);
	
	Agent->limits[Agent->nlimits].type = type;
	dps_strncpy(Agent->limits[Agent->nlimits].file_name, file_name, PATH_MAX);
	Agent->limits[Agent->nlimits].file_name[PATH_MAX-1] = '\0';
	switch(type){
		case DPS_LIMTYPE_NESTED: DpsDecodeHex8Str(str, &hi, &lo, &f_hi, &f_lo); break;
		case DPS_LIMTYPE_TIME: f_hi = hi = 0; f_lo = lo = 0; break;
		case DPS_LIMTYPE_LINEAR_INT: hi = atoi(str); lo=0; f_hi = hi; f_lo = lo; break;
		case DPS_LIMTYPE_LINEAR_CRC: hi = DpsStrHash32(str); lo = 0; f_hi = hi; f_lo = 0; break;
	}	
	Agent->limits[Agent->nlimits].hi = hi;
	Agent->limits[Agent->nlimits].lo = lo;
	Agent->limits[Agent->nlimits].f_hi = f_hi;
	Agent->limits[Agent->nlimits].f_lo = f_lo;
	
	Agent->nlimits++;

	DpsLog(Agent, DPS_LOG_DEBUG, "val: %s[%s]  %x %x   %x %x", str, val,  hi, lo, f_hi, f_lo);

	DPS_FREE(str);
	TRACE_OUT(Agent);
	return DPS_OK;
}

urlid_t* LoadNestedLimit(DPS_AGENT *Agent, DPS_DB *db, size_t lnum, size_t *size) {
	char	fname[PATH_MAX];
	int	ind_fd,dat_fd;
	DPS_UINT8_POS_LEN *ind=NULL;
	struct	stat sb;
	size_t	num;
	urlid_t	*data;
	size_t	start = (size_t)-1, stop = (size_t)-1, len;
	dps_uint4   hi = Agent->limits[lnum].hi, lo = Agent->limits[lnum].lo, 
	  f_hi = Agent->limits[lnum].f_hi, f_lo = Agent->limits[lnum].f_lo;
	const char *name = Agent->limits[lnum].file_name;
	const char	*vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);

	TRACE_IN(Agent, "LoadNestedLimit");
	
	DpsLog(Agent, DPS_LOG_DEBUG, "%08x %08x - %08x %08x", hi, lo, f_hi, f_lo);
	if(hi==0&&lo==0) {
	  TRACE_OUT(Agent);
	  return(NULL);
	}

	dps_snprintf(fname, sizeof(fname), "%s%c%s%c%s.ind", vardir, DPSSLASH, DPS_TREEDIR, DPSSLASH, name);
	if((ind_fd = DpsOpen2(fname, O_RDONLY | DPS_BINARY)) < 0) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "Can't open '%s'", fname);
	  goto err1;
	}
	fstat(ind_fd, &sb);
	if ((ind = (DPS_UINT8_POS_LEN*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
	        DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d, file:%s", sb.st_size, __FILE__, __LINE__, fname);
		DpsClose(ind_fd);
		goto err1;
	}
	if (sb.st_size != 0) {
	  if(sb.st_size!=read(ind_fd,ind,(size_t)sb.st_size)){
	    dps_strerror(Agent, DPS_LOG_ERROR, "Can't read '%s'", fname);
	    DpsClose(ind_fd);
	    goto err1;
	  }
	}
	DpsClose(ind_fd);
	num = (size_t)(sb.st_size / sizeof(DPS_UINT8_POS_LEN));
	DpsLog(Agent, DPS_LOG_DEBUG, " num: %d", num);

	{
		size_t l=0,r=num,m;

		while(l<r){
			m = (l + r) / 2;
			DpsLog(Agent, DPS_LOG_DEBUG, "m: %d  .hi: %08x  .lo: %08x", m, ind[m].hi, ind[m].lo);
			if (ind[m].hi < hi) l = m + 1;
			else
			if((ind[m].hi == hi) && (ind[m].lo < lo)) l=m+1;
			else r = m;
		}
		if(r == num) goto err2;
		start = r;
/*		if (ind[r].hi > f_hi || (ind[r].hi == f_hi && ind[r].lo > f_lo ) ) start--;*/
		DpsLog(Agent, DPS_LOG_DEBUG, "start:%d   r: %d  .hi: %08x  .lo: %08x", start, r, ind[r].hi, ind[r].lo);
		if (ind[r].hi > f_hi || (ind[r].hi == f_hi && ind[r].lo > f_lo ) ) goto err2;
	}	
	if (start != (size_t)-1) {
		size_t l = start, r = num, m;

		while(l < r){
			m = (l + r) / 2;
			DpsLog(Agent, DPS_LOG_DEBUG, "m: %d  .hi: %08x  .lo: %08x", m, ind[m].hi, ind[m].lo);
			if(ind[m].hi < f_hi) l = m + 1;
			else
			if((ind[m].hi == f_hi) && (ind[m].lo < f_lo)) l = m + 1;
			else r = m;
		}
		if (r == num) stop = num - 1;
		else stop = r;
		if (ind[stop].hi > f_hi || (ind[stop].hi == f_hi && ind[stop].lo > f_lo)) stop--;
	}	

	DpsLog(Agent, DPS_LOG_DEBUG, "num: %d  start: %d [%08x %08x]   stop: %d [%08x %08x]", num, start, ind[start].hi, ind[start].lo,
	       stop, ind[stop].hi, ind[stop].lo);

	if (start != (size_t)-1) {
	  dps_snprintf(fname, sizeof(fname), "%s%c%s%c%s.dat", vardir, DPSSLASH, DPS_TREEDIR, DPSSLASH, name);
	  if((dat_fd = DpsOpen2(fname, O_RDONLY | DPS_BINARY)) < 0) {
	    dps_strerror(Agent, DPS_LOG_ERROR, "Can't open '%s'", fname);
	    goto err1;
	  }
	  if(ind[start].pos != (dps_uint8)lseek(dat_fd, (off_t)ind[start].pos, SEEK_SET)){
	    dps_strerror(Agent, DPS_LOG_ERROR, "Can't seek '%s'", fname);
	    DpsClose(dat_fd);
	    goto err1;
	  }
	  len = (size_t)(ind[stop].pos + ind[stop].len - ind[start].pos);
	  DpsLog(Agent, DPS_LOG_DEBUG, "len: %d", len);
	  data = (urlid_t*)DpsMalloc(len + 1);
	  if (data == NULL) {
		DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", len, __FILE__, __LINE__);
		DpsClose(dat_fd);
		goto err1;
	  }
	  if(len != (size_t)read(dat_fd,data,len)){
	    dps_strerror(Agent, DPS_LOG_ERROR, "Can't read '%s'", fname);
	    DpsClose(dat_fd);
	    goto err1;
	  }
	  if ((stop > start) && ((len / sizeof(*data)) > 1)) DpsSort(data, len / sizeof(*data), sizeof(urlid_t), (qsort_cmp)cmp_urlid_t);
	} else {
	  DpsClose(dat_fd);
	  goto err2;
	}

	DpsClose(dat_fd);
	DPS_FREE(ind);

	*size = len / sizeof(*data);
	TRACE_OUT(Agent);
	return(data);

err1:
	/*if (ind)*/ DPS_FREE(ind);
	TRACE_OUT(Agent);
	return(NULL);
err2:
	len = sizeof(*data);
	data = (urlid_t*)DpsMalloc(len + 1);
	if (data == NULL) {
	  DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", len + 1, __FILE__, __LINE__);
	  goto err1;
	}
	data[0] = 0;
	*size = 1;
	TRACE_OUT(Agent);
	return(data);

}

urlid_t* LoadLinearLimit(DPS_AGENT *Agent, DPS_DB *db, const char *name, dps_uint4 val, size_t *size) {
	char	fname[PATH_MAX];
	int	ind_fd,dat_fd;
	DPS_UINT4_POS_LEN key,*found,*ind=NULL;
	struct	stat sb;
	size_t	num;
	urlid_t	*data;
	const char	*vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
	
	TRACE_IN(Agent, "LoadLinearLimit");
	DpsLog(Agent, DPS_LOG_DEBUG, "Linear limit for: %08x", val);

	dps_snprintf(fname, sizeof(fname), "%s%c%s%c%s.ind", vardir, DPSSLASH, DPS_TREEDIR, DPSSLASH, name);
	if((ind_fd = DpsOpen2(fname, O_RDONLY | DPS_BINARY)) < 0) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "Can't open '%s'", fname);
	  goto err1;
	}
	fstat(ind_fd, &sb);
	if ((ind = (DPS_UINT4_POS_LEN*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
		DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", sb.st_size, __FILE__, __LINE__);
		DpsClose(ind_fd);
		goto err1;
	}
	if (sb.st_size != 0) {
	  if(sb.st_size!=read(ind_fd,ind,(size_t)sb.st_size)){
	    dps_strerror(Agent, DPS_LOG_ERROR, "Can't read '%s'", fname);
	    DpsClose(ind_fd);
	    goto err1;
	  }
	}
	DpsClose(ind_fd);

	num = (size_t)(sb.st_size/sizeof(DPS_UINT4_POS_LEN));
	key.val=val;
	if(!(found = dps_bsearch(&key, ind, num, sizeof(DPS_UINT4_POS_LEN), (qsort_cmp)cmp_hex4_ind))) {
	  data = (urlid_t*)DpsMalloc(sizeof(*data) + 1);
	  if (data == NULL) {
	    DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", sizeof(*data), __FILE__, __LINE__);
	    goto err1;
	  }
	  data[0] = 0;
	  *size = 1;
	  DPS_FREE(ind);
	  TRACE_OUT(Agent);
	  return data;
	}
	dps_snprintf(fname, sizeof(fname), "%s%c%s%c%s.dat", vardir, DPSSLASH, DPS_TREEDIR, DPSSLASH, name);
	if((dat_fd = DpsOpen2(fname, O_RDONLY | DPS_BINARY)) < 0) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "Can't open '%s'", fname);
	  goto err1;
	}
	if(found->pos != (dps_uint8)lseek(dat_fd, (off_t)found->pos, SEEK_SET)) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "Can't seek '%s'", fname);
	  DpsClose(dat_fd);
	  goto err1;
	}
	if ((found->len == 0) || (data = (urlid_t*)DpsMalloc(found->len)) == NULL) {
	  DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", found->len, __FILE__, __LINE__);
	  DpsClose(dat_fd);
	  goto err1;
	}
	if(found->len != (size_t)read(dat_fd,data,found->len)){
	  dps_strerror(Agent, DPS_LOG_ERROR, "Can't read '%s'", fname);
	  DpsClose(dat_fd);
	  goto err1;
	}
	DpsClose(dat_fd);

	*size = found->len / sizeof(*data);

	DPS_FREE(ind);
	TRACE_OUT(Agent);
	return(data);

err1:
	DPS_FREE(ind);
	TRACE_OUT(Agent);
	return(NULL);
}

#ifndef HAVE_TIMEGM
#define timegm mktime
#endif

urlid_t* LoadTimeLimit(DPS_AGENT *Agent, DPS_DB *db, const char *name, dps_uint4 from, dps_uint4 to, size_t *size) {
	char	fname[PATH_MAX];
	int	ind_fd,dat_fd;
	DPS_UINT4_POS_LEN *found1,*found2,*ind=NULL;
	struct	stat sb;
	size_t	num,len;
	urlid_t	*data;
	const char *dt = DpsVarListFindStr(&Agent->Vars, "dt", "");
	dps_uint4 dp = 1;
	struct tm tm;
	const char	*vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);

	TRACE_IN(Agent, "LoadTimeLimit");

	bzero((void*)&tm, sizeof(struct tm));
	if (!strcasecmp(dt, "back")) {
	  dp = Dps_dp2time_t(DpsVarListFindStr(&Agent->Vars, "dp", "")) / 3600;
	  to = Agent->now / 3600;
	  from = to - dp;
	} else if (!strcasecmp(dt, "er")) {
	  tm.tm_mday = DpsVarListFindInt(&Agent->Vars, "dd", 1);
	  tm.tm_mon = DpsVarListFindInt(&Agent->Vars, "dm", 0);
	  tm.tm_year = DpsVarListFindInt(&Agent->Vars, "dy", 1970) - 1900;
	  if (DpsVarListFindInt(&Agent->Vars, "dx", 1) == -1) {
	    from = 0;
	    to = timegm(&tm) / 3600;
	  } else {
	    from = timegm(&tm) / 3600;
	    to = INT_MAX;
	  }
	} else if (!strcasecmp(dt, "range")) {
	  sscanf(DpsVarListFindStr(&Agent->Vars, "db", "01/01/1970"), "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year);
	  tm.tm_year -= 1900;
	  tm.tm_mon--;
	  tm.tm_hour = 0;
	  tm.tm_min = 0;
	  tm.tm_sec = 0;
	  from = timegm(&tm) / 3600;
	  bzero((void*)&tm, sizeof(struct tm));
	  sscanf(DpsVarListFindStr(&Agent->Vars, "de", "01/01/1970"), "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year);
	  tm.tm_year -= 1900;
	  tm.tm_mon--;
	  tm.tm_hour = 23;
	  tm.tm_min = 59;
	  tm.tm_sec = 59;
	  to = timegm(&tm) / 3600;
	} else {
	  TRACE_OUT(Agent);
	  return NULL;
	}

	DpsLog(Agent, DPS_LOG_DEBUG, "Time limit: from:%d  to:%d", from, to);

	if(((from==0)&&(to==0))||(from>to)||(dp==0)) { TRACE_OUT(Agent); return(NULL); }

	dps_snprintf(fname, sizeof(fname), "%s%c%s%c%s.ind", vardir, DPSSLASH, DPS_TREEDIR, DPSSLASH, name);
	if((ind_fd = DpsOpen2(fname, O_RDONLY | DPS_BINARY)) < 0) {
	  dps_strerror(Agent, DPS_LOG_ERROR, "Can't open '%s'", fname);
	  goto err1;
	}
	fstat(ind_fd, &sb);
	if ((ind = (DPS_UINT4_POS_LEN*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
		DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", sb.st_size, __FILE__, __LINE__);
		DpsClose(ind_fd);
		goto err1;
	}
	if (sb.st_size != 0) {
	  if(sb.st_size!=read(ind_fd,ind,(size_t)sb.st_size)){
	    dps_strerror(Agent, DPS_LOG_ERROR, "Can't read '%s'", fname);
	    DpsClose(ind_fd);
	    goto err1;
	  }
	}
	DpsClose(ind_fd);

	num = (size_t)(sb.st_size/sizeof(DPS_UINT4_POS_LEN));

	if(!from){
		found1=&ind[0];
	}else{
		dps_uint4 l=0,r=num,m;

		while(l<r){
			m=(l+r)/2;
			if(ind[m].val<from) l=m+1;
			else r=m;
		}
		if(r == num && ind[r].val != from) found1 = NULL;
		else found1=&ind[r];
	}	
	if(!to){
		found2=&ind[num-1];
	}else{
		dps_uint4 l=0,r=num,m;

		while(l<r){
			m=(l+r)/2;
			if(ind[m].val<to) l=m+1;
			else r=m;
		}
		if(r==num) found2=&ind[num-1];
		else if(ind[r].val==to) found2=&ind[r];
		else if(l>0) found2=&ind[l-1];
		else found2=NULL;
	}	
	if(!found1||!found2) {
	  data = (urlid_t*)DpsMalloc(sizeof(*data) + 1);
	  if (data == NULL) {
	    DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", sizeof(*data), __FILE__, __LINE__);
	    goto err1;
	  }
	  data[0] = 0;
	  *size = 1;
	  TRACE_OUT(Agent);
	  return data;
	}

	dps_snprintf(fname, sizeof(fname), "%s%c%s%c%s.dat", vardir, DPSSLASH, DPS_TREEDIR, DPSSLASH, name);
	if((dat_fd = DpsOpen2(fname, O_RDONLY | DPS_BINARY)) < 0) {
	  dps_strerror(Agent, DPS_ERROR, "Can't open '%s'", fname);
	  goto err1;
	}
	if(found1->pos != (dps_uint8)lseek(dat_fd, (off_t)found1->pos, SEEK_SET)) {
	  dps_strerror(Agent, DPS_ERROR, "Can't seek '%s'", fname);
	  DpsClose(dat_fd);
	  goto err1;
	}
	len = (size_t)(found2->pos + found2->len - found1->pos);
	if (len == 0) {
	  data = (urlid_t*)DpsMalloc(sizeof(*data) + 1);
	  if (data == NULL) {
	    DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", sizeof(*data), __FILE__, __LINE__);
	    goto err1;
	  }
	  data[0] = 0;
	  *size = 1;
	} else {
	  if ((data = (urlid_t*)DpsMalloc(len)) == NULL) {
	        DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", len, __FILE__, __LINE__);
		DpsClose(dat_fd);
		goto err1;
	  }
	  if(len != (size_t)read(dat_fd,data,len)){
	    dps_strerror(Agent, DPS_LOG_ERROR, "Can't read '%s'", fname);
	    DpsClose(dat_fd);
	    goto err1;
	  }
	  *size = len / sizeof(*data);
	}
	DpsClose(dat_fd);
	
	DPS_FREE(ind);
	
	if (*size > 1) DpsSort(data, *size, sizeof(*data), (qsort_cmp)cmp_urlid_t);
	TRACE_OUT(Agent);
	return(data);

err1:
	if (ind) DPS_FREE(ind);
	TRACE_OUT(Agent);
	return(NULL);
}


static int PresentInLimit(urlid_t *buf, size_t buf_count, size_t *start, const urlid_t url_id) {
	register size_t m;
	register size_t l;
	register size_t r;

	if (start) l = *start;
	else l = 0;
	r = buf_count;
	while(l < r) {
		m = ( l + r) / 2;
/*		fprintf(stderr, "PIL: %d ? %d\n", url_id, buf[m]);*/
		if(buf[m] == url_id) {
		  if (start) *start = m;
		  return 1;
		}
		if(buf[m]<url_id) l=m+1;
		else r=m;
	}
/*	fprintf(stderr, "r: %d  buf_count: %d  buf[r]: %d  m: %d  buf[m]: %d\n", r, buf_count, buf[r], m, buf[m]);*/
	if (start) *start = r;
	if(r == buf_count) return(0);
	else
		if(buf[r]==url_id) return(1);
		else return(0);
}


int DpsCmpURLData(DPS_URLDATA *d1, DPS_URLDATA *d2) {
  if (d1->url_id < d2->url_id) return -1;
  if (d1->url_id > d2->url_id) return 1;
  return 0;
}

int DpsURLDataLoadCache(DPS_AGENT *A, DPS_RESULT *R, DPS_DB *db) {
	DPS_URLDATA *Dat, *D = NULL, K, *F;
	DPS_URL_CRD_DB *Crd;
#ifdef WITH_REL_TRACK
	DPS_URLTRACK *Trk = R->CoordList.Track;
#endif
	struct stat sb;
        size_t i, j, count, nrec = 0, first = 0;
	const char	*vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&A->Conf->Vars, "VarDir", DPS_VAR_DIR);
	int NFiles = (db->URLDataFiles > 0) ? (int)db->URLDataFiles : DpsVarListFindInt(&A->Conf->Vars, "URLDataFiles", 0x300);
	int filenum, fd = -1, prevfilenum = - 1;
	char fname[PATH_MAX];

	TRACE_IN(A, "DpsURLDataLoadCache");

	count = R->CoordList.ncoords;
	if (count == 0) { TRACE_OUT(A); return DPS_OK; }
	Dat = R->CoordList.Data = (DPS_URLDATA*)DpsRealloc(R->CoordList.Data, count * sizeof(DPS_URLDATA));
	if (Dat == NULL) { TRACE_OUT(A); return DPS_ERROR; }
	Crd = R->CoordList.Coords;

	if (A->Flags.PreloadURLData) {

	  DPS_GETLOCK(A, DPS_LOCK_CONF);
	  for (i = j = 0; i < count; i++) {
	    filenum = DPS_FILENO(Crd[i].url_id, NFiles);
	    if (filenum != prevfilenum) {
	      prevfilenum = filenum;
	      nrec = A->Conf->URLDataFile[db->dbnum][filenum].nrec;
	      D = A->Conf->URLDataFile[db->dbnum][filenum].URLData;
	      first = 0;
	    }
	    K.url_id = Crd[i].url_id;
	    if ((nrec > 0) 
		&& (F = (DPS_URLDATA*)dps_bsearch(&K, &D[first], nrec - first, sizeof(DPS_URLDATA),(qsort_cmp) DpsCmpURLData)) != NULL) {
	      Dat[j] = *F;
	      first = (F - D);
	      if (i != j) {
		Crd[j] = Crd[i];
#ifdef WITH_REL_TRACK
		Trk[j] = Trk[i];
#endif
	      }
	      j++;
	    }
	  }
	  DPS_RELEASELOCK(A, DPS_LOCK_CONF);
	  R->CoordList.ncoords = j;

	} else {
	
	  for (i = j = 0; i < count; i++) {
	    filenum = DPS_FILENO(Crd[i].url_id, NFiles);
/*	  DpsLog(A, DPS_LOG_DEBUG, "LoadCache: id: %d (%x) - filenum: %d", Crd[i].url_id, Crd[i].url_id, filenum);*/
	    if (filenum != prevfilenum) {
	      if (fd > 0) DpsClose(fd);
	      dps_snprintf(fname, sizeof(fname), "%s%c%s%cdata%04x.dat", vardir, DPSSLASH, DPS_URLDIR, DPSSLASH, filenum);
	      fd = DpsOpen3(fname, O_RDONLY | DPS_BINARY, 0644);
	      prevfilenum = filenum;
	      nrec = 0;
	      DpsLog(A, DPS_LOG_DEBUG, "Open %s %s", fname, (fd > 0) ? "OK" : "FAIL");
	      if (fd > 0) {
		DpsReadLock(fd); 
		fstat(fd, &sb);
		if ((sb.st_size == 0) || (D = (DPS_URLDATA*)DpsRealloc(D, (size_t)sb.st_size)) == NULL) {
		  DpsLog(A, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", sb.st_size, __FILE__, __LINE__);
		  TRACE_OUT(A);
		  return DPS_ERROR;
		}
		(void)read(fd, D, (size_t)sb.st_size);
		nrec = (size_t)(sb.st_size / sizeof(DPS_URLDATA));
		first = 0;
		DpsUnLock(fd);
		DpsLog(A, DPS_LOG_DEBUG, "%d records readed", nrec);
	      }
	    }
	    K.url_id = Crd[i].url_id;
/*	  fprintf(stderr, "K.url_id: %d  filenum: %d  first: %d\n", Crd[i].url_id, filenum, first);*/
	    if ((nrec > 0) 
		&& (F = (DPS_URLDATA*)dps_bsearch(&K, &D[first], nrec - first, sizeof(DPS_URLDATA),(qsort_cmp) DpsCmpURLData)) != NULL) {
	      Dat[j] = *F;
	      first = (F - D);
	      if (i != j) {
		Crd[j] = Crd[i];
#ifdef WITH_REL_TRACK
		Trk[j] = Trk[i];
#endif
	      }
	      j++;
	    }
	  }
	  R->CoordList.ncoords = j;
	  DPS_FREE(D);
	  if (fd > 0) DpsClose(fd);

	}

	TRACE_OUT(A);
	return DPS_OK;
}


int DpsURLDataPreloadCache(DPS_AGENT *Agent, DPS_DB *db) {
	DPS_URLDATA_FILE *DF;
	struct stat sb;
        size_t nrec = 0, mem_used = 0;
	const char	*vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Conf->Vars, "VarDir", DPS_VAR_DIR); /* should be fetched from Conf->Vars */
	int NFiles = (db->URLDataFiles > 0) ? (int)db->URLDataFiles : DpsVarListFindInt(&Agent->Conf->Vars, "URLDataFiles", 0x300);
	int filenum, fd = -1;
	char fname[PATH_MAX];

	TRACE_IN(Agent, "DpsURLDataPreloadCache");

	if (Agent->Conf->URLDataFile == NULL) {
	  size_t nitems = (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
	  if ((Agent->Conf->URLDataFile = (DPS_URLDATA_FILE**)DpsXmalloc(nitems * sizeof(DPS_URLDATA_FILE*))) == NULL) {
	    DpsLog(Agent, DPS_LOG_ERROR, " DpsURLDataPreloadCache %d", __LINE__);
	    TRACE_OUT(Agent);
	    return DPS_ERROR;
	  }
	}
	if (Agent->Conf->URLDataFile[db->dbnum] == NULL) {
	  if ((Agent->Conf->URLDataFile[db->dbnum] = (DPS_URLDATA_FILE*)DpsXmalloc(NFiles * sizeof(DPS_URLDATA_FILE))) == NULL) {
	    DpsLog(Agent, DPS_LOG_ERROR, " DpsURLDataPreloadCache %d", __LINE__);
	    TRACE_OUT(Agent);
	    return DPS_ERROR;
	  }
	  mem_used += NFiles * sizeof(DPS_URLDATA_FILE);
	}
	DF = Agent->Conf->URLDataFile[db->dbnum];

	for (filenum = 0 ; filenum < NFiles; filenum++) {
	    dps_snprintf(fname, sizeof(fname), "%s%c%s%cdata%04x.dat", vardir, DPSSLASH, DPS_URLDIR, DPSSLASH, filenum);
	    fd = DpsOpen2(fname, O_RDONLY | DPS_BINARY);
	    nrec = 0;
	    DpsLog(Agent, DPS_LOG_DEBUG, "Open %s %s", fname, (fd > 0) ? "OK" : "FAIL");
	    if (fd > 0) {
	      DpsReadLock(fd); 
	      fstat(fd, &sb);
	      if ((nrec = (size_t)(sb.st_size / sizeof(DPS_URLDATA))) > 0) {
	      
		DF[filenum].URLData = (DPS_URLDATA*)DpsRealloc(DF[filenum].URLData, (DF[filenum].nrec + nrec) * sizeof(DPS_URLDATA));
		if (DF[filenum].URLData == NULL) {
		  DpsLog(Agent, DPS_LOG_ERROR, "Can't realloc %d bytes at %s:%d", 
			 (DF[filenum].nrec + nrec) * sizeof(DPS_URLDATA), __FILE__, __LINE__);
		  TRACE_OUT(Agent);
		  return DPS_ERROR;
		}
/*		DF[filenum].mtime = sb.st_mtime;*/
		(void)read(fd, &DF[filenum].URLData[DF[filenum].nrec], (size_t)sb.st_size);
		DpsUnLock(fd);
		DF[filenum].nrec += nrec;
		mem_used += nrec *  sizeof(DPS_URLDATA);
		DpsSort(DF[filenum].URLData, DF[filenum].nrec, sizeof(DPS_URLDATA), (qsort_cmp) DpsCmpURLData);
		DpsLog(Agent, DPS_LOG_DEBUG, "%d records readed", nrec);
		DpsClose(fd);
	      }
	    }
	}

	DpsLog(Agent, DPS_LOG_INFO, "URL data preloaded. %u bytes of memory used", mem_used);
	TRACE_OUT(Agent);
	return DPS_OK;
}

/*
typedef struct {
	DPS_URL_CRD *plast;
	DPS_URL_CRD *pcur;
	DPS_URL_CRD *pbegin;
	int num;
	int count;
} DPS_PMERG;
*/
/*
typedef struct {
  urlid_t *data;
  size_t  size;
  size_t  start;
  int     origin;
} DPS_FINDWORD_LIMIT;

static int cmp_findword_limit(const DPS_FINDWORD_LIMIT *l1, const DPS_FINDWORD_LIMIT *l2) {
  if (l1->size < l2->size) return -1;
  if (l1->size > l2->size) return 1;
  return 0;
}
*/
static int cmp_search_limit(const DPS_SEARCH_LIMIT *l1, const DPS_SEARCH_LIMIT *l2) {
  if (l1->size < l2->size) return -1;
  if (l1->size > l2->size) return 1;
  return 0;
}



/*#define MAXMERGE 256*/ /* <= one byte */

static void dps_read_word(void *param) {
  DPS_WRD_CFG *Cfg = (DPS_WRD_CFG*)param;
  size_t num;
  size_t orig_size;

  /*  bzero(&BASEP, sizeof(BASEP));*/ /* must be zeroed already */
  Cfg->BASEP.subdir = DPS_TREEDIR;
  Cfg->BASEP.basename = "wrd";
  Cfg->BASEP.indname = "wrd";
  /*  BASEP.NFiles = (db->WrdFiles > 0) ? (int)db->WrdFiles : DpsVarListFindInt(&Indexer->Vars, "WrdFiles", 0x300);
      BASEP.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Indexer->Vars, "VarDir", DPS_VAR_DIR);*/ /* must be set already */
  /*Cfg->BASEP.A = Indexer;*/ /* must be set already */
  Cfg->BASEP.mode = DPS_READ_LOCK;
#ifdef HAVE_ZLIB
  Cfg->BASEP.zlib_method = Z_DEFLATED;
  Cfg->BASEP.zlib_level = 9;
  Cfg->BASEP.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
  Cfg->BASEP.zlib_memLevel = 9;
  Cfg->BASEP.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif

  Cfg->BASEP.rec_id = Cfg->pmerg[0]->crcword;
  Cfg->pmerg[0]->db_pcur = Cfg->pmerg[0]->db_pbegin = Cfg->pmerg[0]->db_pchecked = (DPS_URL_CRD*)DpsBaseARead(&Cfg->BASEP, &orig_size);

  if (Cfg->pmerg[0]->db_pbegin != NULL) {
    Cfg->pmerg[0]->count = num = RemoveOldCrds(Cfg->pmerg[0]->db_pcur, orig_size / sizeof(DPS_URL_CRD), Cfg->del_buf, Cfg->del_count);
    Cfg->pmerg[0]->pcur = Cfg->pmerg[0]->pchecked = Cfg->pmerg[0]->pbegin = (DPS_URL_CRD_DB*)DpsMalloc(num * sizeof(DPS_URL_CRD_DB));
    if (Cfg->pmerg[0]->pbegin != NULL) {

      if (Cfg->flag_null_wf) {
	Cfg->pmerg[0]->count = DpsRemoveNullSections(Cfg->pmerg[0]->db_pcur, num, Cfg->wf);
	num = Cfg->pmerg[0]->count;
      }

      Cfg->pmerg[0]->plast = &Cfg->pmerg[0]->pcur[num];
      Cfg->pmerg[0]->db_plast = &Cfg->pmerg[0]->db_pcur[num];
    }
  } else Cfg->pmerg[0]->count = 0;

  DpsBaseClose(&Cfg->BASEP);

}


int DpsFindWordsCache(DPS_AGENT * Indexer, DPS_RESULT *Res, DPS_DB *db) {
        size_t i, j;
	DPS_BASE_PARAM BASEP;
	DPS_STACK_ITEM **pmerg = NULL;
	int wf[256], present;
	size_t z, npmerge;
	size_t del_count = 0, nwords;
	DPS_LOGDEL *del_buf=NULL;
	
	DPS_SEARCH_LIMIT *lims = NULL;
	size_t nlims = 0, num, nskipped, orig_size;
	urlid_t cur_url_id;
	int flag_null_wf, group_by_site, use_site_id;
	int use_empty = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "empty", "yes"), "yes");
	
#ifdef DEBUG_SEARCH
	unsigned long ticks, seek_ticks;
	unsigned long total_ticks=DpsStartTimer();
	
	DpsLog(Indexer, DPS_LOG_DEBUG, "Start DpsFindWordsCache()");
#endif

	TRACE_IN(Indexer, "DpsFindWordsCache");

#ifdef DEBUG_SEARCH
	ticks = DpsStartTimer();
#endif

	DpsPrepare(Indexer, Res);	/* Prepare query    */
	DpsWWLBoolItems(Res);
	
#ifdef DEBUG_SEARCH
	ticks = DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "    Query prepared in ... %.4f sec.", (float)ticks / 1000);
	DpsLog(Indexer, DPS_LOG_EXTRA, "    wf=%s", DpsVarListFindStr(&Indexer->Vars, "wf", ""));
#endif

	flag_null_wf = DpsWeightFactorsInit(DpsVarListFindStr(&Indexer->Vars, "wf", ""), wf);

#ifdef DEBUG_SEARCH
	ticks = DpsStartTimer();
#endif

#if 0

	/* Open del log file */
	dps_snprintf(dname, sizeof(dname), "%s%c%s%cdel.log", vardir, DPSSLASH, DPS_SPLDIR, DPSSLASH);
	if((dd = DpsOpen2(dname, O_RDONLY | DPS_BINARY)) < 0) {
	  dps_strerror(Indexer, DPS_LOG_ERROR, "Can't open del log '%s'", dname);
 	} else {

	  /* Allocate del buffer */
	  fstat(dd, &sb);
	  if (sb.st_size != 0) {
	    del_buf = (DPS_LOGDEL*)DpsMalloc((size_t)sb.st_size);
	    if (del_buf == NULL) {
	      DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", sb.st_size, __FILE__, __LINE__);
	      DPS_FREE(merg);
	      TRACE_OUT(Indexer);
	      return DPS_ERROR;
	    }
	    del_count = read(dd,del_buf, (size_t)sb.st_size) / sizeof(DPS_LOGDEL);
	  }
	  DpsClose(dd);

	  /* Remove duplicates URLs in DEL log     */
	  /* Keep only oldest records for each URL */
	  if (del_count > 0) {
	    if (del_count > 1) DpsSort(del_buf, (size_t)del_count, sizeof(DPS_LOGDEL), DpsCmpurldellog);
	    del_count = DpsRemoveDelLogDups(del_buf, del_count);
	  }
	
#ifdef DEBUG_SEARCH
	  ticks = DpsStartTimer() - ticks;
	  DpsLog(Indexer, DPS_LOG_DEBUG, "    Del log loaded (%d)... %.4fs", del_count, (float)ticks / 1000);
#endif
	}

#endif

#ifdef DEBUG_SEARCH
	DpsLog(Indexer, DPS_LOG_EXTRA, "    Reading limits (%d, loaded:%d)... ", Indexer->nlimits, Indexer->loaded_limits);
	ticks=DpsStartTimer();
#endif

	lims = (DPS_SEARCH_LIMIT*)DpsRealloc(lims, (Indexer->nlimits + 1) * sizeof(DPS_SEARCH_LIMIT));
	if (lims == NULL) {
	  DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", (nlims+1)*sizeof(DPS_SEARCH_LIMIT), __FILE__, __LINE__);
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}

	for(i = Indexer->loaded_limits; i < Indexer->nlimits; i++){
	        int not_loaded = 1;

		lims[nlims].start = 0;
		lims[nlims].origin = -1;

		lims[nlims].need_free = 1;
		for (j = 0; j < Indexer->loaded_limits; j++) {
		  if ((Indexer->limits[j].type == Indexer->limits[i].type) &&
		      (Indexer->limits[j].hi == Indexer->limits[i].hi) &&
		      (Indexer->limits[j].lo == Indexer->limits[i].lo) &&
		      (Indexer->limits[j].f_hi == Indexer->limits[i].f_hi) &&
		      (Indexer->limits[j].f_lo == Indexer->limits[i].f_lo)) {
		    lims[nlims] = Indexer->limits[j];
		    lims[nlims].need_free = 0;
		    nlims++;
		    not_loaded = 0; break;
		  }
		}

		if (not_loaded)
		switch(Indexer->limits[i].type){
			case DPS_LIMTYPE_NESTED:
			  if((lims[nlims].data = LoadNestedLimit(Indexer, db, i, &lims[nlims].size))) nlims++;
				break;
			case DPS_LIMTYPE_TIME:
			  if((lims[nlims].data = LoadTimeLimit(Indexer, db, Indexer->limits[i].file_name,
								Indexer->limits[i].hi,
								Indexer->limits[i].lo,
								&lims[nlims].size))) nlims++;
				break;
			case DPS_LIMTYPE_LINEAR_INT:
			case DPS_LIMTYPE_LINEAR_CRC:
			  if((lims[nlims].data = LoadLinearLimit(Indexer, db, Indexer->limits[i].file_name,
								Indexer->limits[i].hi,
								&lims[nlims].size))) nlims++;
				break;
		}
		DpsLog(Indexer, DPS_LOG_DEBUG, "\t\tlims.%d.size:%d", nlims - 1, lims[nlims - 1].size);
	}

	nwords = Res->nitems - Res->ncmds;

#ifdef DEBUG_SEARCH
	ticks=DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "\t\t\tDone (%.2f)", (float)ticks / 1000);
	  ticks = DpsStartTimer();
	  DpsLog(Indexer, DPS_LOG_EXTRA, "    Sorting %d limits...", nlims);
#endif
	if (/*(Indexer->nlimits > 0) &&*/ (nlims > 1)) {

	  DpsSort(lims, nlims, sizeof(DPS_SEARCH_LIMIT), (qsort_cmp)cmp_search_limit);

	}
#ifdef DEBUG_SEARCH
	ticks = DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "\tDone (%.2f)", (float)ticks / 1000);
	DpsLog(Indexer, DPS_LOG_EXTRA, "    Reading .wrd files (%d words)... ", nwords);
	ticks=DpsStartTimer();
#endif

	if (nwords == 0 && use_empty && nlims > 0) {
	  DPS_URL_CRD *p;

	  if ((pmerg = (DPS_STACK_ITEM**)DpsXmalloc(2 * sizeof(DPS_STACK_ITEM *))) == NULL) {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", 2 * sizeof(DPS_STACK_ITEM *), __FILE__, __LINE__);
	    TRACE_OUT(Indexer);
	    return DPS_ERROR;
	  }
	  pmerg[0] = &Res->items[0];
	  pmerg[0]->pcur = pmerg[0]->pbegin = pmerg[0]->pchecked = (DPS_URL_CRD_DB*)DpsMalloc((lims[0].size + 1) * sizeof(DPS_URL_CRD_DB));
	  pmerg[0]->db_pcur = pmerg[0]->db_pbegin = pmerg[0]->db_pchecked = p = (DPS_URL_CRD*)DpsMalloc((lims[0].size + 1) * sizeof(DPS_URL_CRD));

	  if (pmerg[0]->db_pbegin != NULL) {
	    for (i = 0; i < lims[0].size; i++) {
	      p[i].url_id = lims[0].data[i];
	      p[i].coord = 0;
	    }
	    pmerg[0]->count = num = RemoveOldCrds(pmerg[0]->db_pcur, lims[0].size, del_buf, del_count);
	    if (flag_null_wf) {
	      pmerg[0]->count = DpsRemoveNullSections(pmerg[0]->db_pcur, num, wf);
	      num = pmerg[0]->count;
	    }
	    pmerg[0]->plast = &pmerg[0]->pcur[num];
	    pmerg[0]->db_plast = &pmerg[0]->db_pcur[num];
	    npmerge = 1;
	    Res->CoordList.ncoords += num;
	  } else pmerg[0]->count = 0;

	} else {


#ifdef HAVE_PTHREAD

#ifdef HAVE_PTHREAD_SETCONCURRENCY_PROT
	  if (pthread_setconcurrency(nwords + 1) != 0) {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Can't set %d concurrency threads", nwords + 1);
	    return DPS_ERROR;
	  }
#elif HAVE_THR_SETCONCURRENCY_PROT
	  if (thr_setconcurrency(nwords + 1) != NULL) {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Can't set %d concurrency threads", nwords + 1);
	    return DPS_ERROR;
	  }
#endif

#endif /* HAVE_PTHREAD */


	bzero(&BASEP, sizeof(BASEP));
	BASEP.subdir = DPS_TREEDIR;
	BASEP.basename = "wrd";
	BASEP.indname = "wrd";
	BASEP.NFiles = (db->WrdFiles > 0) ? (int)db->WrdFiles : DpsVarListFindInt(&Indexer->Vars, "WrdFiles", 0x300);
	BASEP.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Indexer->Vars, "VarDir", DPS_VAR_DIR);
	BASEP.A = Indexer;
	BASEP.mode = DPS_READ_LOCK;
#ifdef HAVE_ZLIB
	BASEP.zlib_method = Z_DEFLATED;
	BASEP.zlib_level = 9;
	BASEP.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
	BASEP.zlib_memLevel = 9;
	BASEP.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif

	if ((pmerg = (DPS_STACK_ITEM**)DpsXmalloc((nwords + 1) * sizeof(DPS_STACK_ITEM *))) == NULL) {
	  DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", (nwords + 1) * sizeof(DPS_STACK_ITEM *), __FILE__, __LINE__);
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}

	npmerge = 0;
	for (i = 0; i < Res->nitems; i++) {
	  if (Res->items[i].cmd != DPS_STACK_WORD) continue;
/*	  if (Res->items[i].origin & DPS_WORD_ORIGIN_STOP) continue;*/ /* FIX: add a command to skip reading */

	  for (z = 0 ; z < npmerge; z++) 
	    if (pmerg[z]->crcword == Res->items[i].crcword && (pmerg[z]->secno == 0 || pmerg[z]->secno == Res->items[i].secno)) break;
	  if (z < npmerge) continue;

	  BASEP.rec_id = Res->items[i].crcword;

	  DpsBaseSeek(&BASEP, DPS_READ_LOCK);

	  if (BASEP.rec_id == BASEP.Item.rec_id) {

	    pmerg[npmerge] = &Res->items[i];

	    pmerg[npmerge]->db_pcur = pmerg[npmerge]->db_pbegin = pmerg[npmerge]->db_pchecked = (DPS_URL_CRD*)DpsBaseARead(&BASEP, &orig_size);

	    if (pmerg[npmerge]->db_pbegin == NULL) {

	      pmerg[npmerge]->count = 0;
	      DpsLog(Indexer, DPS_LOG_EXTRA, "No data for %dth word or error occured (%s:%d)", i, __FILE__, __LINE__);
	      continue;
	    }
	    pmerg[npmerge]->count = num = RemoveOldCrds(pmerg[npmerge]->db_pcur, orig_size / sizeof(DPS_URL_CRD), del_buf, del_count);
	    pmerg[npmerge]->pcur = pmerg[npmerge]->pchecked = pmerg[npmerge]->pbegin = (DPS_URL_CRD_DB*)DpsMalloc(num * sizeof(DPS_URL_CRD_DB));
	    if (pmerg[npmerge]->pbegin == NULL) {

	      pmerg[npmerge]->count = 0;
	      DpsLog(Indexer, DPS_LOG_EXTRA, "No data for %dth word or error occured (%s:%d)", i, __FILE__, __LINE__);
	      continue;
	    }
		
	    if (flag_null_wf) {
	      pmerg[npmerge]->count = DpsRemoveNullSections(pmerg[npmerge]->db_pcur, num, wf);
	      num = pmerg[npmerge]->count;
	    }

	    pmerg[npmerge]->plast = &pmerg[npmerge]->pcur[num];
	    pmerg[npmerge]->db_plast = &pmerg[npmerge]->db_pcur[num];
	    npmerge++;
	    Res->CoordList.ncoords += num;

	  }

	}

	DpsBaseClose(&BASEP);
	}
	
#ifdef DEBUG_SEARCH
	ticks = DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "\tDone (%.2f)", (float)ticks / 1000);
	DpsLog(Indexer, DPS_LOG_EXTRA, "    Merging (%d groups, %d urls)... ", npmerge, Res->CoordList.ncoords);
	ticks=DpsStartTimer();
#endif

	if (Res->CoordList.ncoords == 0) { DPS_FREE(lims); goto zero_exit; }
/* will make this later
	Res->CoordList.Coords = (DPS_URL_CRD*)DpsRealloc(Res->CoordList.Coords, Res->CoordList.ncoords * sizeof(DPS_URL_CRD));
	if (Res->CoordList.Coords == NULL) {
	  DPS_FREE(pmerge)
	  DPS_FREE(del_buf);
	  DpsLog(Indexer, DPS_LOG_ERROR, "Can't realloc %d bytes at %s:%d", Res->CoordList.ncoords*sizeof(DPS_URL_CRD), __FILE__, __LINE__);
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}
*/
	Res->CoordList.ncoords = 0;
	for(i = 0; i < (size_t)npmerge; i++) {
	  pmerg[i]->count = 0;
	}

	while (1) {
	  nskipped = 0;
	  for (i = 0; i < npmerge; i++) {
	    if ((pmerg[i]->db_pcur != NULL) && (pmerg[i]->db_pcur < pmerg[i]->db_plast)) {
	      cur_url_id = pmerg[i]->db_pcur->url_id;
	      i++;
	      break;
	    }
	    nskipped++;
	  }

	  if (nskipped >= npmerge) break;
	  for(; i < npmerge; i++) {
	    if ((pmerg[i]->db_pcur != NULL) && (pmerg[i]->db_pcur < pmerg[i]->db_plast)) {
	      if (cur_url_id > pmerg[i]->db_pcur->url_id) cur_url_id = pmerg[i]->db_pcur->url_id;
	    }
	  }
	  
	  present = 1;
	  for (i = 0; i < nlims; i++) {
	    if (!PresentInLimit(lims[i].data, lims[i].size, &lims[i].start, cur_url_id)) {
	      present = 0;
	      break;
	    }
	  }

	  for(i = 0; i < npmerge; i++) {
	    if (pmerg[i]->pcur != NULL) {
	      if (present) {
		register DPS_URL_CRD_DB *Crd;
		while ((pmerg[i]->db_pcur < pmerg[i]->db_plast) && (pmerg[i]->db_pcur->url_id == cur_url_id) ) {
		  Crd = pmerg[i]->pchecked;
		  Crd->coord = pmerg[i]->db_pcur->coord;
		  if (pmerg[i]->secno == 0 || DPS_WRDSEC(Crd->coord) == pmerg[i]->secno) {
		    register size_t mlen = (Crd->coord & 0xFF);
		    if (mlen == 0 || mlen == pmerg[i]->ulen) {
		      Crd->url_id = pmerg[i]->db_pcur->url_id;
		      Crd->coord &= 0xFFFFFF00;
		      Crd->coord += (pmerg[i]->wordnum /*order*/ & 0xFF);
#ifdef WITH_MULTIDBADDR
		      Crd->dbnum = db->dbnum;
#endif		  
		      pmerg[i]->pchecked++;
		      pmerg[i]->count++;
		    }
		  }
		  pmerg[i]->db_pcur++;
		}
	      } else {
		while ((pmerg[i]->db_pcur < pmerg[i]->db_plast) && (pmerg[i]->db_pcur->url_id == cur_url_id) ) {
		  pmerg[i]->db_pcur++;
		}
	      }
	    }
	  }

	}
	for(i = 0; i < (size_t)npmerge; i++) {
	  Res->CoordList.ncoords += pmerg[i]->count;
	}

	for(i=0;i<nlims;i++) if (lims[i].need_free) DPS_FREE(lims[i].data);
	DPS_FREE(lims);

#ifdef DEBUG_SEARCH
	ticks=DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "\tDone (%.2f) Merged ncoords1=%d", (float)ticks / 1000, Res->CoordList.ncoords);
	DpsLog(Indexer, DPS_LOG_EXTRA, "    Grouping by url_id... ");
	ticks=DpsStartTimer();
#endif
/********/
#if 0
	DpsSortSearchWordsByURL(Res->CoordList.Coords, Res->CoordList.ncoords);
#endif
	DpsGroupByURL(Indexer, Res);
	
	
#ifdef DEBUG_SEARCH
	ticks=DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "\t\tDone (%.2f)", (float)ticks / 1000);
#endif

#ifdef DEBUG_SEARCH
	DpsLog(Indexer, DPS_LOG_EXTRA, "Start load url data %d docs", Res->CoordList.ncoords);
	ticks = DpsStartTimer();
#endif
	DpsURLDataLoadCache(Indexer, Res, db);
#ifdef DEBUG_SEARCH
	ticks = DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "Stop load url data %d docs in (%.2f)", Res->CoordList.ncoords, (float)ticks / 1000);
#endif

	Res->grand_total = Res->CoordList.ncoords;


	group_by_site = DpsGroupBySiteMode(DpsVarListFindStr(&Indexer->Vars, "GroupBySite", NULL));
	use_site_id = group_by_site && (DpsVarListFindInt(&Indexer->Vars, "site", 0) == 0);

	if (use_site_id && (group_by_site == DPS_GROUP_FULL)) {

#ifdef DEBUG_SEARCH
	  DpsLog(Indexer, DPS_LOG_EXTRA, "    Sorting by site_id... ");
	  ticks = DpsStartTimer();
#endif
	  if (Res->CoordList.ncoords > 1) 
	    DpsSortSearchWordsBySite(Res, &Res->CoordList, Res->CoordList.ncoords, DpsVarListFindStr(&Indexer->Vars, "s", "RP"));

#ifdef DEBUG_SEARCH
	  ticks=DpsStartTimer() - ticks;
	  DpsLog(Indexer, DPS_LOG_EXTRA, "\t\tDone (%.2f)", (float)ticks / 1000);
#endif

#ifdef DEBUG_SEARCH
	  DpsLog(Indexer, DPS_LOG_EXTRA, "    Grouping by site_id... ");
	  ticks = DpsStartTimer();
#endif
	  DpsGroupBySite(Indexer, Res);

#ifdef DEBUG_SEARCH
	  ticks=DpsStartTimer() - ticks;
	  DpsLog(Indexer, DPS_LOG_EXTRA, "\t\tDone (%.2f)", (float)ticks / 1000);
#endif
	}

#ifdef DEBUG_SEARCH
	DpsLog(Indexer, DPS_LOG_EXTRA, "    Sort by relevancy,pop_rank... ");
	ticks = DpsStartTimer();
#endif

	if (Res->CoordList.ncoords > 1)
	  DpsSortSearchWordsByPattern(Res, &Res->CoordList, Res->CoordList.ncoords, DpsVarListFindStr(&Indexer->Vars, "s", "RP"));

#ifdef DEBUG_SEARCH
	ticks=DpsStartTimer() - ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "\t\tDone (%.2f)", (float)ticks / 1000);
#endif

	if (use_site_id && (group_by_site == DPS_GROUP_YES)) {

#ifdef DEBUG_SEARCH
	  DpsLog(Indexer, DPS_LOG_EXTRA, "    Grouping by site_id... ");
	  ticks = DpsStartTimer();
#endif
	  DpsGroupBySite(Indexer, Res);

#ifdef DEBUG_SEARCH
	  ticks=DpsStartTimer() - ticks;
	  DpsLog(Indexer, DPS_LOG_EXTRA, "\t\tDone (%.2f)", (float)ticks / 1000);
#endif
	}
	

	Res->total_found = Res->CoordList.ncoords;
#ifdef DEBUG_SEARCH
	total_ticks=DpsStartTimer() - total_ticks;
	DpsLog(Indexer, DPS_LOG_EXTRA, "Stop DpsFindWordsCache() - %.2f\n", (float)total_ticks / 1000);
#endif

 zero_exit:

	DPS_FREE(pmerg);
	DPS_FREE(del_buf);
	DpsWWLBoolItems(Res);
	TRACE_OUT(Indexer);
	return DPS_OK;
}





/*************************************/


int DpsLogdSaveBuf(DPS_AGENT *Indexer, DPS_ENV * Env, size_t log_num) { /* Should be DPS_LOCK_CACHED locked */
  DPS_DB *db;
  DPS_LOGDEL *del_buf = NULL;
  DPS_LOGWORD *log_buf;
  DPS_BASE_PARAM P;
  const char *vardir;
  DPS_LOGD *logd;
  size_t z, dbfrom = 0, dbto, n, del_count;
  int res = DPS_OK;

  TRACE_IN(Indexer, "DpsLogdSaveBuf");

  bzero(&P, sizeof(P));
  P.subdir = DPS_TREEDIR;
  P.basename = "wrd";
  P.indname = "wrd";
  P.mode = DPS_WRITE_LOCK;
/*  P.vardir = DpsVarListFindStr(&Indexer->Vars, "VarDir", DPS_VAR_DIR);*/
  P.A = Indexer;
#ifdef HAVE_ZLIB
  P.zlib_method = Z_DEFLATED;
  P.zlib_level = 9;
  P.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
  P.zlib_memLevel = 9;
  P.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif

  vardir = DpsVarListFindStr(&Indexer->Vars, "VarDir", DPS_VAR_DIR);
  dbto =  (Indexer->flags & DPS_FLAG_UNOCON) ? Indexer->Conf->dbl.nitems : Indexer->dbl.nitems;

  for (z = dbfrom; z < dbto; z++) {
    db = (Indexer->flags & DPS_FLAG_UNOCON) ? &Indexer->Conf->dbl.db[z] : &Indexer->dbl.db[z];
    if (db->DBMode != DPS_DBMODE_CACHE) continue;
    P.vardir = (db->vardir) ? db->vardir : vardir;
    P.NFiles = (db->WrdFiles > 0) ? (int)db->WrdFiles : DpsVarListFindInt(&Indexer->Vars, "WrdFiles", 0x300);

/*    DPS_GETLOCK(Indexer, DPS_LOCK_CACHED);*/
    logd = &db->LOGD;

    if (Env->logs_only) {
      char   fname[PATH_MAX];
      int    fd;
      size_t nbytes = logd->wrd_buf[log_num].nrec * sizeof(DPS_LOGWORD);

      if (nbytes > 0) {
	dps_snprintf(fname, sizeof(fname), "%s%s%03X.log", db->log_dir, DPSSLASHSTR, log_num);
	
/*	DPS_GETLOCK(Indexer, DPS_LOCK_CACHED_N(log_num));*/
	if((fd = DpsOpen3(fname, open_flags, open_perm)) != -1) {
	  nbytes = logd->wrd_buf[log_num].nrec * sizeof(DPS_LOGWORD);
	  DpsWriteLock(fd);
	  if(nbytes != (size_t)write(fd, logd->wrd_buf[log_num].data,nbytes)){
	    dps_strerror(Indexer, DPS_LOG_ERROR, "Can't write %d nbytes to '%s'", nbytes, fname);
	    DpsUnLock(fd);
	    DpsClose(fd);
	    DpsBaseClose(&P);
/*	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED_N(log_num));*/
/*	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED);*/
	    TRACE_OUT(Indexer);
	    return DPS_ERROR;
	  }
	  DpsUnLock(fd);
	  DpsClose(fd);
/*	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED_N(log_num));*/
	  logd->wrd_buf[log_num].nrec = 0;
	}else{
	  dps_strerror(Indexer, DPS_LOG_ERROR, "Can't open '%s'", fname);
	  DpsBaseClose(&P);
/*	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED_N(log_num));*/
/*	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED);*/
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}
      }
      DpsWriteLock(db->del_fd);
      if (logd->wrd_buf[log_num].ndel) 
	lseek(db->del_fd, (off_t)0, SEEK_END);
	if((ssize_t)(logd->wrd_buf[log_num].ndel * sizeof(DPS_LOGDEL)) != 
	   write(db->del_fd, logd->wrd_buf[log_num].del_buf, logd->wrd_buf[log_num].ndel * sizeof(DPS_LOGDEL))) {
	  dps_strerror(Indexer, DPS_LOG_ERROR, "Can't write to del.log");
	  db->errcode = 1;
	  DpsUnLock(db->del_fd);
	  DpsBaseClose(&P);
/*	  DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED);*/
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}
      logd->wrd_buf[log_num].ndel = 0;
      DpsUnLock(db->del_fd);
      
/*      DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED);*/
      continue;
    }

/*    if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_CACHED_N(log_num));*/
    /* Open del log file */
    del_buf = logd->wrd_buf[log_num].del_buf;
    del_count = logd->wrd_buf[log_num].ndel;

    /* Remove duplicates URLs in DEL log     */
    /* Keep only oldest records for each URL */
    if (del_count > 1) {
      DpsSort(del_buf, (size_t)del_count, sizeof(DPS_LOGDEL), DpsCmpurldellog);
      del_count = DpsRemoveDelLogDups(del_buf, del_count);
    }

    /* Allocate buffer for log */
    log_buf = logd->wrd_buf[log_num].data;
    n = logd->wrd_buf[log_num].nrec;
    if (n > 1) DpsSort(log_buf, n, sizeof(DPS_LOGWORD), (qsort_cmp)DpsCmplog);
    n = DpsRemoveOldWords(log_buf, n, del_buf, del_count);
    if (n > 1) DpsSort(log_buf, n, sizeof(DPS_LOGWORD), (qsort_cmp)DpsCmplog_wrd);

/*    if (!(Indexer->flags & DPS_FLAG_UNOCON)) DPS_GETLOCK(Indexer, DPS_LOCK_CACHED_N(log_num));*/
    if (n || del_count) res = DpsProcessBuf(Indexer, &P, log_num, log_buf, n, del_buf, del_count);

    logd->wrd_buf[log_num].nrec = 0;
    logd->wrd_buf[log_num].ndel = 0;
/*    DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED);*/

    if (Indexer->Flags.OptimizeAtUpdate && (res == DPS_OK) && (n > 0)) {
      res = DpsBaseOptimize(&P, (int)log_num);    /* disk space save strategy: optimize after every update, 
							but this slow down indexing. */
    }
    DpsBaseClose(&P);
/*    DPS_RELEASELOCK(Indexer, DPS_LOCK_CACHED_N(log_num));*/
  }
  
  TRACE_OUT(Indexer);
  return res;
}




int DpsLogdSaveAllBufs(DPS_AGENT *Agent) {
  DPS_DB *db;
  DPS_ENV *Env = Agent->Conf;
	size_t i;
	size_t j, dbfrom = 0, dbto, pi, shift;
	int res = DPS_OK, u;
	size_t NWrdFiles = (size_t)DpsVarListFindInt(&Env->Vars, "WrdFiles", 0x300);

	TRACE_IN(Agent, "DpsLogdSaveAllBufs");

	DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
	DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
	
	for (j = dbfrom; j < dbto; j++) {
	  DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	  db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[j] : &Agent->dbl.db[j];
	  DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
/*	  if (Agent->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_DB);*/

/**	  DPS_GETLOCK(Agent, DPS_LOCK_CACHED);**/
	  u = (db->LOGD.wrd_buf == NULL);
/**	  DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED);**/
	  if (u) continue;
	  shift = (Agent->handle * 321) % ((db->WrdFiles) ? db->WrdFiles : NWrdFiles);
	  for(i = 0; i < ((db->WrdFiles) ? db->WrdFiles : NWrdFiles); i++) {
	    pi = (shift + i) % ((db->WrdFiles) ? db->WrdFiles : NWrdFiles);
	    DPS_GETLOCK(Agent, DPS_LOCK_CACHED_N(pi));
	    u = (db->LOGD.wrd_buf[pi].nrec || db->LOGD.wrd_buf[pi].ndel);
	    if (u) {
	      res = DpsLogdSaveBuf(Agent, Env, pi);
	    }
	    DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED_N(pi));
	    if (res != DPS_OK) break;
/*	    DPSSLEEP(0);*/
	  }
	  db->LOGD.cur_del_buf = 0;
/*	  if (Agent->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_DB);*/
	  if (res != DPS_OK) break;
	}
	TRACE_OUT(Agent);
	return res;
}

static int DpsLogdCloseLogs(DPS_AGENT *Agent) {
  DPS_ENV *Env = Agent->Conf;
  size_t j, dbfrom = 0, dbto;
  DPS_DB *db;
  int res;

  TRACE_IN(Agent, "DpsLogdCloseLogs");

  res = DpsLogdSaveAllBufs(Agent);

  dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;

  for (j = dbfrom; j < dbto; j++) {
    db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[j] : &Agent->dbl.db[j];
    if (Env->logs_only) {
	if(db->del_fd){ DpsClose(db->del_fd); db->del_fd = 0; }
	if(db->cat_fd){ DpsClose(db->cat_fd); db->cat_fd = 0; }
	if(db->tag_fd){ DpsClose(db->tag_fd); db->tag_fd = 0; }
	if(db->time_fd){ DpsClose(db->time_fd); db->time_fd = 0; }
	if(db->lang_fd){ DpsClose(db->lang_fd); db->lang_fd = 0; }
	if(db->ctype_fd){ DpsClose(db->ctype_fd); db->ctype_fd = 0; }
	if(db->site_fd){ DpsClose(db->site_fd); db->site_fd = 0; }
    }
  }
  TRACE_OUT(Agent);
  return res;
}

static int DpsLogdInit(DPS_AGENT *A, DPS_DB *db, char* var_dir, size_t i, int shared) {
        char shm_name[PATH_MAX];
	DPS_ENV *Env = A->Conf;
	int fd;
	size_t NWrdFiles = (db->WrdFiles) ? db->WrdFiles : (size_t)DpsVarListFindInt(&Env->Vars, "WrdFiles", 0x300);
	size_t CacheLogWords = (size_t)DpsVarListFindInt(&Env->Vars, "CacheLogWords", 1024);
	size_t CacheLogDels = (size_t)DpsVarListFindInt(&Env->Vars, "CacheLogDels", 10240);

	size_t WrdBufSize = NWrdFiles * (sizeof(dps_wrd_buf) + CacheLogWords * sizeof(DPS_LOGWORD) + CacheLogDels * sizeof(DPS_LOGDEL));

/*	fprintf(stderr, "LogdInit. NWrdFiles:%d  CacheLogWords:%d  CacheLogDels:%d  shared: %d\n", 
		NWrdFiles, CacheLogWords, CacheLogDels, shared);*/

	if (DpsBuild(var_dir, 0755) != 0) {
	  dps_strerror(A, DPS_LOG_ERROR, "Can't create VarDir %s", var_dir);
	  return DPS_ERROR;
	}

        dps_snprintf(db->log_dir, PATH_MAX, "%s%s%s%s", var_dir, DPSSLASHSTR, DPS_SPLDIR, DPSSLASHSTR);
	db->errstr[0] = 0;

	if (DpsBuild(db->log_dir, 0755) != 0) {
	  dps_strerror(A, DPS_LOG_ERROR, "Can't create directory %s", var_dir);
	  return DPS_ERROR;
	}

	if (shared) {
	  

	    dps_snprintf(shm_name, PATH_MAX, "%s%sLOGD.%d", var_dir, DPSSLASHSTR, i);
	    if ((fd = DpsOpen3(shm_name, O_RDWR | O_CREAT, (mode_t)0644)) < 0) {
	      dps_strerror(A, DPS_LOG_ERROR, "%s open failed", shm_name);
	      return DPS_ERROR;
	    }
	    DpsClose(fd);
#ifdef HAVE_SHAREDMEM_POSIX
	    if ((fd = shm_open(shm_name, O_RDWR | O_CREAT, (mode_t)0644)) < 0) {
	      dps_snprintf(shm_name, PATH_MAX, "%sLOGD.%d", DPSSLASHSTR, i);
	      if ((fd = shm_open(shm_name, O_RDWR | O_CREAT, (mode_t)0644)) < 0) {
		dps_strerror(A, DPS_LOG_ERROR, "shm_open (%s)", shm_name);
		return DPS_ERROR;
	      }
	    }
/*	    fprintf(stderr, "Open LOGD shared: %s\n", shm_name);*/
	    if ((db->LOGD.wrd_buf = (dps_wrd_buf*)
		 mmap( NULL, WrdBufSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0)) == NULL) {
	      dps_strerror(A, DPS_LOG_ERROR, "mmap: %s:%d", __FILE__, __LINE__);
	      return DPS_ERROR;
	    }
	    (void)ftruncate(fd, (off_t) WrdBufSize);
	    DpsClose(fd);
#elif defined(HAVE_SHAREDMEM_SYSV)
	    if ((fd = shmget(ftok(shm_name, 0), WrdBufSize, IPC_CREAT | SHM_R | SHM_W | (SHM_R>>3) | (SHM_R>>6) )) < 0) {
	      dps_strerror(A, DPS_LOG_ERROR, "shmget (%s): %s:%s", shm_name, __FILE__, __LINE__);
	      return DPS_ERROR;
	    }
	    if ((db->LOGD.wrd_buf = (dps_wrd_buf*)shmat( fd, NULL, 0)) == (void*)-1) {
	      dps_strerror(A, DPS_LOG_ERROR, "shmat: %s:%d", __FILE__, __LINE__);
	      return DPS_ERROR;
	    }
#else
#error No shared memory support found
#endif


	} else {
	  if ((db->LOGD.wrd_buf = (dps_wrd_buf*)DpsXmalloc(WrdBufSize + 1)) == NULL) {
	    DpsLog(A, DPS_LOG_ERROR, "Out of memory, %d at %s:%d", (int)WrdBufSize, __FILE__, __LINE__);
	    return DPS_ERROR;
	  }
	}

	for(i = 0; i < NWrdFiles; i++) {
	    db->LOGD.wrd_buf[i].nrec = 0;
	    db->LOGD.wrd_buf[i].ndel = 0;
	    db->LOGD.wrd_buf[i].data = (DPS_LOGWORD*)(((char*)db->LOGD.wrd_buf) + NWrdFiles * sizeof(dps_wrd_buf) + 
						      i * (CacheLogWords * sizeof(DPS_LOGWORD) + CacheLogDels * sizeof(DPS_LOGDEL)));
	    db->LOGD.wrd_buf[i].del_buf = /*(Env->logs_only) ? NULL 
	      :*/ (DPS_LOGDEL*)(((char*)db->LOGD.wrd_buf[i].data) + CacheLogWords * sizeof(DPS_LOGWORD));
	}

	if (Env->logs_only) {
	    char	log_name[PATH_MAX];

	    dps_snprintf(log_name, sizeof(log_name), "%s%s%s", db->log_dir, DPSSLASHSTR, "del.log");
	    if((db->del_fd = DpsOpen3(log_name, O_RDWR | O_APPEND | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
	      dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s'", log_name);
	      return DPS_ERROR;
	    }
	    lseek(db->del_fd, (off_t)0, SEEK_END);

	    if (Env->Flags.limits & DPS_LIMIT_CAT) {
	      dps_snprintf(log_name, sizeof(log_name),"%s%s%s.log", db->log_dir, DPSSLASHSTR, DPS_LIMFNAME_CAT);
	      if((db->cat_fd = DpsOpen3(log_name, O_RDWR | O_APPEND | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s'", log_name);
		return DPS_ERROR;
	      }
	      lseek(db->cat_fd, (off_t)0, SEEK_END);
	    }
	    if (Env->Flags.limits & DPS_LIMIT_TAG) {
	      dps_snprintf(log_name, sizeof(log_name),"%s%s%s.log", db->log_dir, DPSSLASHSTR, DPS_LIMFNAME_TAG);
	      if((db->tag_fd = DpsOpen3(log_name, O_RDWR | O_APPEND | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s'", log_name);
		return DPS_ERROR;
	      }
	      lseek(db->tag_fd, (off_t)0, SEEK_END);
	    }
	    if (Env->Flags.limits & DPS_LIMIT_TIME) {
	      dps_snprintf(log_name, sizeof(log_name),"%s%s%s.log", db->log_dir, DPSSLASHSTR, DPS_LIMFNAME_TIME);
	      if((db->time_fd = DpsOpen3(log_name, O_RDWR | O_APPEND | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s'", log_name);
		return DPS_ERROR;
	      }
	      lseek(db->time_fd, (off_t)0, SEEK_END);
	    }
	    if (Env->Flags.limits & DPS_LIMIT_LANG) {
	      dps_snprintf(log_name, sizeof(log_name),"%s%s%s.log", db->log_dir, DPSSLASHSTR, DPS_LIMFNAME_LANG);
	      if((db->lang_fd = DpsOpen3(log_name, O_RDWR | O_APPEND | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s'", log_name);
		return DPS_ERROR;
	      }
	      lseek(db->lang_fd, (off_t)0, SEEK_END);
	    }
	    if (Env->Flags.limits & DPS_LIMIT_CTYPE) {
	      dps_snprintf(log_name, sizeof(log_name),"%s%s%s.log", db->log_dir, DPSSLASHSTR, DPS_LIMFNAME_CTYPE);
	      if((db->ctype_fd = DpsOpen3(log_name, O_RDWR | O_APPEND | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s'", log_name);
		return DPS_ERROR;
	      }
	      lseek(db->ctype_fd, (off_t)0, SEEK_END);
	    }
	    if (Env->Flags.limits & DPS_LIMIT_SITE) {
	      dps_snprintf(log_name, sizeof(log_name),"%s%s%s.log", db->log_dir, DPSSLASHSTR, DPS_LIMFNAME_SITE);
	      if((db->site_fd = DpsOpen3(log_name, O_RDWR | O_APPEND | O_CREAT | DPS_BINARY, DPS_IWRITE)) == -1) {
		dps_strerror(A, DPS_LOG_ERROR, "Can't open '%s'", log_name);
		return DPS_ERROR;
	      }
	      lseek(db->site_fd, (off_t)0, SEEK_END);
	    }
	}
	return DPS_OK;
}

int DpsLogdClose(DPS_AGENT *Agent, DPS_DB *db, const char *var_dir, size_t i, int shared) {
        char shm_name[PATH_MAX];

	TRACE_IN(Agent, "DpsLogdClose");

        dps_snprintf(shm_name, PATH_MAX, "%s%sLOGD.%d", (db->vardir) ? db->vardir : var_dir, DPSSLASHSTR, i);
/*	unlink(shm_name);*/
	
	if (!shared) DPS_FREE(db->LOGD.wrd_buf);
	TRACE_OUT(Agent);
	return DPS_OK;
}


static int URLDataWrite(DPS_AGENT *Indexer, DPS_DB *db) {
        DPS_SQLRES     SQLres, Res;
	DPS_URLDATA    Item;
        size_t i, offset = 0, nitems;
	urlid_t rec_id = 0;
	int filenum, fd = -1, prevfilenum = - 1;
	int rc = DPS_OK, u = 1, recs, status;
	int NFiles;
	int *FF = NULL;
	int upper_status = (Indexer->Flags.SubDocLevel > 0) ? 400 : 300;
	int max_shows, use_showcnt = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "PopRankUseShowCnt", "no"), "yes");
	double min_weight, scale_weight;
	const char	*vardir;
	char fname[PATH_MAX];
	char str[512];

	TRACE_IN(Indexer, "URLDataWrite");

#ifdef HAVE_SQL

	DpsSQLResInit(&SQLres);
	DpsSQLResInit(&Res);

	recs = DpsVarListFindInt(&Indexer->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	NFiles = (db->URLDataFiles) ? db->URLDataFiles : DpsVarListFindUnsigned(&Indexer->Vars, "URLDataFiles", 0x300);
	vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Indexer->Vars, "VarDir", DPS_VAR_DIR);

	FF = (int*)DpsXmalloc(NFiles * sizeof(int));
	if (FF == NULL) { TRACE_OUT(Indexer); return DPS_ERROR;}

	if (db->DBType != DPS_DB_CACHE) {
	  dps_snprintf(str, sizeof(str), "SELECT MIN(weight), MAX(weight) FROM server WHERE command='S'");
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	  rc = DpsSQLQuery(db, &SQLres, str);
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	  if (rc != DPS_OK) {
	    goto URLDataWrite_exit;
	  }
	  min_weight = DPS_ATOF(DpsSQLValue(&SQLres, 0, 0));
	  scale_weight = DPS_ATOF(DpsSQLValue(&SQLres, 0, 1)) - min_weight + 0.001;
	  min_weight -= 0.001;
	  DpsSQLFree(&SQLres);
	

	  if (use_showcnt) {
	    dps_snprintf(str, sizeof(str), "SELECT MAX(shows),MIN(rec_id) FROM url");
	  }else {
	    dps_snprintf(str, sizeof(str), "SELECT 0, MIN(rec_id) FROM url");
	  }
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	  rc = DpsSQLQuery(db, &SQLres, str);
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	  if (rc != DPS_OK) {
	    goto URLDataWrite_exit;
	  }
	  max_shows = DPS_ATOI(DpsSQLValue(&SQLres, 0, 0)) + 1;
	  rec_id = DPS_ATOI(DpsSQLValue(&SQLres, 0, 1));
	  DpsSQLFree(&SQLres);
	}

	while(u) {
	  if (use_showcnt) {
	    dps_snprintf(str, sizeof(str), 
 "SELECT u.rec_id,u.site_id,u.pop_rank,u.last_mod_time,u.since,u.status,u.crc32,s.weight,u.shows FROM url u,server s WHERE u.rec_id>=%d AND s.rec_id=u.site_id ORDER by u.rec_id LIMIT %d",
			 rec_id, recs);
	  } else {
	    dps_snprintf(str, sizeof(str), 
 "SELECT u.rec_id,u.site_id,u.pop_rank,u.last_mod_time,u.since,u.status,u.crc32,s.weight FROM url u,server s WHERE u.rec_id>=%d AND s.rec_id=u.site_id ORDER by u.rec_id LIMIT %d",
			 rec_id, recs);
	  }
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	  rc = DpsSQLQuery(db, &SQLres, str);
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	  if (rc != DPS_OK) {
	    goto URLDataWrite_exit;
	  }
	  nitems = DpsSQLNumRows(&SQLres);
	  for (i = 0; i < nitems; i++) {
	    status = DPS_ATOI(DpsSQLValue(&SQLres, i, 5));
	    
	    if ((status < 200 || status >= upper_status) && (status != 304)) continue;

	    Item.url_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 0));
	    Item.site_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 1));
	    if (use_showcnt) {
	      Item.pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, i, 2)) * log(2.8 + DPS_ATOF(DpsSQLValue(&SQLres, i, 8))) / log(2.8 + max_shows) 
		* log(2.8 + DPS_ATOF(DpsSQLValue(&SQLres, i, 7)) - min_weight) / log(2.8 + scale_weight);
	    } else {
	      
	      Item.pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, i, 2)) * log(2.8 + DPS_ATOF(DpsSQLValue(&SQLres, i, 7)) - min_weight) / log(2.8 + scale_weight);
	      /*
	      Item.pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, i, 2));
	      */
	    }
	    if ((Item.last_mod_time = DPS_ATOU(DpsSQLValue(&SQLres, i, 3))) == 0) {
	      Item.last_mod_time = DPS_ATOU(DpsSQLValue(&SQLres, i, 4));
	    }

	    filenum = DPS_FILENO(Item.url_id, NFiles);
	    if (filenum != prevfilenum) {
	      if (fd > 0) DpsClose(fd);
	      dps_snprintf(fname, sizeof(fname), "%s%c%s%cdata%04x.dat", vardir, DPSSLASH, DPS_URLDIR, DPSSLASH, filenum);
	      if (FF[filenum] == 0) unlink(fname);
	      fd = DpsOpen3(fname, O_WRONLY|O_APPEND|O_CREAT|DPS_BINARY, 0644);
	      prevfilenum = filenum;
	      FF[filenum]++;
	    }
	    if (fd > 0) {
	      DpsWriteLock(fd);
	      (void)write(fd, &Item, sizeof(DPS_URLDATA));
	      DpsUnLock(fd);
	    }
	  }
	  offset += nitems;

	  /* To see the URL being indexed in "ps" output on xBSD */
	  if (DpsNeedLog(DPS_LOG_EXTRA)) dps_setproctitle("[%d] url data: %d records processed", Indexer->handle, offset);
	  DpsLog(Indexer, DPS_LOG_EXTRA, "%d records of url data written, at %d", offset, rec_id);
	  rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, nitems - 1, 0)) + 1;
	  DpsSQLFree(&SQLres);
	  u = (nitems == (size_t)recs);
/*	  if (u) DPSSLEEP(0);*/
	}
 URLDataWrite_exit:
	DpsSQLFree(&SQLres);
	DpsSQLFree(&Res);
	if (fd > 0) DpsClose(fd);
	if (rc == DPS_OK) for(i = 0; i < (size_t)NFiles; i++) {
	  if (FF[i]) continue;
	  dps_snprintf(fname, sizeof(fname), "%s%c%s%cdata%04x.dat", vardir, DPSSLASH, DPS_URLDIR, DPSSLASH, i);
	  unlink(fname);
	}
	DPS_FREE(FF);

#endif
	if (DpsNeedLog(DPS_LOG_EXTRA)) dps_setproctitle("[%d] url data: done", Indexer->handle);

	TRACE_OUT(Indexer);
	return rc;
}


int DpsURLDataWrite(DPS_AGENT *Indexer, DPS_DB *db) {
	size_t sent;
	ssize_t recvt;
	DPS_LOGD_CMD cmd;
	char reply;
	int cached_sd, cached_rv;
	const char	*vardir;
	char fname[PATH_MAX];
	FILE *F;

	TRACE_IN(Indexer, "DpsURLDataWrite");
	if (db->DBMode != DPS_DBMODE_CACHE) {TRACE_OUT(Indexer); return DPS_OK;}

	DpsLog(Indexer, DPS_LOG_INFO, "Writing url data and limits for %s... ", db->DBADDR);

	cmd.stamp = Indexer->now;
	cmd.url_id = 0;
	cmd.cmd = DPS_LOGD_CMD_DATA;
	cmd.nwords = 0;

	cached_sd = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_sd : 0;
	cached_rv = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_rv : 0;

	  if(cached_sd){
		sent = DpsSend(cached_sd, &cmd, sizeof(cmd), 0);
		if(sent!=sizeof(cmd)){
			dps_strerror(Indexer, DPS_LOG_ERROR, "[%s:%d] Can't write to cached", __FILE__, __LINE__);
			TRACE_OUT(Indexer);
			return DPS_ERROR;
		}
		while ((recvt = DpsRecvall(cached_rv, &reply, sizeof(char), WAIT_TIME)) != 1) {
		  if (recvt <= 0) {
			dps_strerror(Indexer, DPS_LOG_ERROR, "Can't receive from cached [%s:%d], %d", __FILE__, __LINE__, recvt);
			TRACE_OUT(Indexer);
			return DPS_ERROR;
		  }
		  DPSSLEEP(0);
		}
		if (reply != 'O') {
			DpsLog(Indexer, DPS_LOG_ERROR, "Can't incorrect reply from cached %s:%d", __FILE__, __LINE__);
			TRACE_OUT(Indexer);
			return DPS_ERROR;
		}
	  }else{
		if (DpsCacheMakeIndexes(Indexer, db)) {
		  TRACE_OUT(Indexer);
		  return DPS_ERROR;
		}
		if(URLDataWrite(Indexer, db)){
		        TRACE_OUT(Indexer);
			return DPS_ERROR;
		}
	  }
	  /* sending -HUP signal to searchd, if need */
	  vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Indexer->Vars, "VarDir", DPS_VAR_DIR);
	  dps_snprintf(fname, PATH_MAX, "%s%s%s", vardir, DPSSLASHSTR, "searchd.pid");
	  F = fopen(fname, "r");
	  if (F != NULL) {
	    int searchd_pid;

	    (void)fscanf(F, "%d", &searchd_pid);
	    fclose(F);
	  
	    DpsLog(Indexer, DPS_LOG_EXTRA, "Sending HUP signal to searchd, pid:%d", searchd_pid);
	    kill((pid_t)searchd_pid, SIGHUP);
	  }

	  DpsLog(Indexer, DPS_LOG_INFO, "url data and limits Done");
	  TRACE_OUT(Indexer);
	  return DPS_OK;
}



void DpsFlushAllBufs(DPS_AGENT *Agent, int rotate_logs) {
  char time_pid[128];
  DPS_DB *db;
  size_t i, dbfrom = 0, dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
  time_t t = time(NULL);
#ifdef HAVE_PTHREAD
  struct tm l_tim;
  struct tm *tim = localtime_r(&t, &l_tim);
#else
  struct tm *tim = localtime(&t);
#endif

  strftime(time_pid, sizeof(time_pid), "%a %d %H:%M:%S", tim);
  t = dps_strlen(time_pid);
  dps_snprintf(time_pid + t, sizeof(time_pid) - t, " [%d]", (int)getpid());
  DpsLog(Agent, DPS_LOG_INFO, "%s Flushing all buffers... ", time_pid);

  if(DPS_OK != DpsLogdSaveAllBufs(Agent)) {
    for (i = dbfrom; i < dbto; i++) {
      DPS_GETLOCK(Agent, DPS_LOCK_DB);
      db = &Agent->Conf->dbl.db[i];
      if (db->errcode) {
	t = time(NULL);
#ifdef HAVE_PTHREAD
	tim = localtime_r(&t, &l_tim);
#else
	tim = localtime(&t);
#endif
	strftime(time_pid, sizeof(time_pid), "%a %d %H:%M:%S", tim);
	t = dps_strlen(time_pid);
	dps_snprintf(time_pid + t, sizeof(time_pid) - t, " [%d]", (int)getpid());
	DpsLog(Agent, DPS_LOG_ERROR, "%s Error: %s", time_pid, db->errstr);
      }
      DPS_RELEASELOCK(Agent, DPS_LOCK_DB);
    }
    t = time(NULL);
#ifdef HAVE_PTHREAD
    tim = localtime_r(&t, &l_tim);
#else
    tim = localtime(&t);
#endif
    strftime(time_pid, sizeof(time_pid), "%a %d %H:%M:%S", tim);
    t = dps_strlen(time_pid);
    dps_snprintf(time_pid + t, sizeof(time_pid) - t, " [%d]", (int)getpid());
    DpsLog(Agent, DPS_LOG_ERROR, "%s Shutdown", time_pid);
  }
  if (Agent->Conf->logs_only && rotate_logs) DpsRotateDelLog(Agent);
  DpsLog(Agent, DPS_LOG_INFO, "Done");

}



int DpsCachedFlush(DPS_AGENT *Indexer, DPS_DB *db) {
	size_t sent;
	ssize_t recvt;
	DPS_LOGD_CMD cmd;
	char reply;
	int cached_sd, cached_rv;
	int flush_buffers = DpsVarListFindInt(&Indexer->Vars, "FlushBuffers", 0);

	TRACE_IN(Indexer, "DpsCachedFlush");
	if (db->DBMode != DPS_DBMODE_CACHE) {TRACE_OUT(Indexer); return DPS_OK;}

	DpsLog(Indexer, DPS_LOG_DEBUG, "Flushing cached buffers for %s... ", db->DBADDR);

	cmd.stamp = Indexer->now;
	cmd.url_id = 0;
	cmd.cmd = DPS_LOGD_CMD_FLUSH;
	cmd.nwords = 0;

	cached_sd = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_sd : 0;
	cached_rv = Indexer->Demons.nitems ? Indexer->Demons.Demon[db->dbnum].cached_rv : 0;

	if(cached_sd){
	  if (flush_buffers) {
		sent = DpsSend(cached_sd, &cmd, sizeof(cmd), 0);
		if(sent!=sizeof(cmd)){
			dps_strerror(Indexer, DPS_LOG_ERROR, "[%s:%d] Can't write to cached", __FILE__, __LINE__);
			TRACE_OUT(Indexer);
			return DPS_ERROR;
		}
		while ((recvt = DpsRecvall(cached_rv, &reply, sizeof(char), WAIT_TIME)) != 1) {
		  if (recvt <= 0) {
			dps_strerror(Indexer, DPS_LOG_ERROR, "Can't receive from cached [%s:%d], %d", __FILE__, __LINE__, recvt);
			TRACE_OUT(Indexer);
			return DPS_ERROR;
		  }
		  DPSSLEEP(0);
		}
		if (reply != 'O') {
			DpsLog(Indexer, DPS_LOG_ERROR, "Can't incorrect reply from cached %s:%d", __FILE__, __LINE__);
			TRACE_OUT(Indexer);
			return DPS_ERROR;
		}
	  }
	} else {
	  DpsFlushAllBufs(Indexer, flush_buffers); /* rotate logs if needed */
	}
	if (flush_buffers) DpsLog(Indexer, DPS_LOG_INFO, "Cached buffers flush Done");
	TRACE_OUT(Indexer);
	return DPS_OK;
}


static int DpsDeleteURLinfoCache(DPS_AGENT *A, DPS_DB *db, urlid_t rec_id) {
  DPS_BASE_PARAM P;
  int res;

  TRACE_IN(A, "DpsDeleteURLinfoCache");

  bzero(&P, sizeof(P));
  P.subdir = DPS_URLDIR;
  P.basename = "info";
  P.indname = "info";
  P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
  P.A = A;
#ifdef HAVE_ZLIB
  P.zlib_method = Z_DEFLATED;
  P.zlib_level = 9;
  P.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
  P.zlib_memLevel = 9;
  P.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif
  P.NFiles = (db->URLDataFiles) ? db->URLDataFiles : DpsVarListFindUnsigned(&A->Vars, "URLDataFiles", 0x300);
  P.mode = DPS_WRITE_LOCK;
  P.rec_id = rec_id;
  DpsBaseDelete(&P);
  res = DpsBaseClose(&P);
  TRACE_OUT(A);
  return res;
}

int DpsLogdStoreDoc(DPS_AGENT *Agent, DPS_LOGD_CMD cmd, DPS_LOGD_WRD *wrd, DPS_DB *db) {
	DPS_LOGDEL logdel;
	DPS_LOGD *logd = &db->LOGD;
	DPS_ENV *Env = Agent->Conf;
	register size_t i;
	size_t NWrdFiles, nrec, num, CacheLogDels, CacheLogWords;

	TRACE_IN(Agent, "DpsLogdStoreDoc");

	if (db->DBMode != DPS_DBMODE_CACHE) {TRACE_OUT(Agent); return DPS_OK; }

	if (cmd.cmd == DPS_LOGD_CMD_DATA) {
	  URLDataWrite(Agent, db);
	  TRACE_OUT(Agent);
	  return DPS_OK;
	}

	logdel.stamp = cmd.stamp;
	logdel.url_id = cmd.url_id;
	NWrdFiles = (db->WrdFiles) ? db->WrdFiles : (size_t)DpsVarListFindInt(&Agent->Vars, "WrdFiles", 0x300);
	CacheLogDels = (size_t)DpsVarListFindInt(&Agent->Vars, "CacheLogDels", 10240);
	CacheLogWords = (size_t)DpsVarListFindInt(&Agent->Vars, "CacheLogWords", 1024);

	if (cmd.cmd != DPS_LOGD_CMD_NEWORD) {
	  if (Env->logs_only) {

	    if (logd->wrd_buf[logd->cur_del_buf].ndel == CacheLogDels) logd->cur_del_buf++;
	    if (logd->cur_del_buf == NWrdFiles) {

	      DpsWriteLock(db->del_fd);
	      lseek(db->del_fd, (off_t)0, SEEK_END);
	      for (i = 0; i < NWrdFiles; i++) {
		if (logd->wrd_buf[i].ndel == 0) continue;
		if((ssize_t)(logd->wrd_buf[i].ndel * sizeof(DPS_LOGDEL)) != write(db->del_fd, logd->wrd_buf[i].del_buf, logd->wrd_buf[i].ndel * sizeof(DPS_LOGDEL))) {
		  dps_strerror(Agent, DPS_LOG_ERROR, "Can't write to del.log");
		  DpsUnLock(db->del_fd);
		  TRACE_OUT(Agent);
		  return DPS_ERROR;
		}
		logd->wrd_buf[i].ndel = 0;
	      }
	      DpsUnLock(db->del_fd);
	      logd->cur_del_buf = 0;
	    }
	    nrec = logd->wrd_buf[logd->cur_del_buf].ndel;
	    logd->wrd_buf[logd->cur_del_buf].del_buf[nrec] = logdel;
	    logd->wrd_buf[logd->cur_del_buf].ndel++;

	  } else {
	      
	    for (i = 0; i < NWrdFiles; i++) {
	      DPS_GETLOCK(Agent, DPS_LOCK_CACHED_N(i));
	      nrec = logd->wrd_buf[i].ndel;
	      if (nrec == CacheLogDels) {
		DpsLog(Agent, DPS_LOG_DEBUG, "num: %03x\t: nrec:%d ndel:%d", 
		       i, logd->wrd_buf[i].nrec, logd->wrd_buf[i].ndel);
		if(DPS_OK != DpsLogdSaveBuf(Agent, Env, i)) {
		  DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED_N(i));
		  TRACE_OUT(Agent);
		  return DPS_ERROR;
		}
		nrec = 0;
	      }
	      logd->wrd_buf[i].del_buf[nrec] = logdel;
	      logd->wrd_buf[i].ndel++;
	      DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED_N(i));
/*	      DPSSLEEP(0);*/
	    }
	  }
	}

	if(cmd.nwords == 0 && cmd.cmd == DPS_LOGD_CMD_DELETE) {
	  DpsDeleteURLinfoCache(Agent, db, cmd.url_id);
	  TRACE_OUT(Agent);
	  return DPS_OK;
	}

	for(i = 0; i < cmd.nwords; i++) {
	  if (wrd[i].coord == 0) continue;
	  num = DPS_FILENO(wrd[i].wrd_id, NWrdFiles);

	  DPS_GETLOCK(Agent, DPS_LOCK_CACHED_N(num));
	  if(logd->wrd_buf[num].nrec == CacheLogWords) {
	    DpsLog(Agent, DPS_LOG_DEBUG, "num: %03x\t: nrec:%d ndel:%d", 
		   num, logd->wrd_buf[num].nrec, logd->wrd_buf[num].ndel);
	    if (DPS_OK != DpsLogdSaveBuf(Agent, Env, num)) {
	      DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED_N(num));
	      TRACE_OUT(Agent);
	      return DPS_ERROR;
	    }
	  }
	  nrec = logd->wrd_buf[num].nrec;
	  logd->wrd_buf[num].data[nrec].stamp = cmd.stamp;
	  logd->wrd_buf[num].data[nrec].url_id = cmd.url_id;
	  logd->wrd_buf[num].data[nrec].wrd_id = wrd[i].wrd_id;
	  logd->wrd_buf[num].data[nrec].coord = wrd[i].coord;
	  logd->wrd_buf[num].nrec++;
	  DPS_RELEASELOCK(Agent, DPS_LOCK_CACHED_N(num));
/*	  DPSSLEEP(0);*/
	}
	TRACE_OUT(Agent);
	return DPS_OK;
}


static int DpsLogdCachedCheck(DPS_AGENT *A, DPS_DB *db, int level) {
#ifdef HAVE_SQL
  size_t b, i, j, ncrd, z, bstart;
  char dat_name[PATH_MAX];
  int dat_fd, res, recs, u = 1;
  DPS_URL_CRD *crd = NULL;
  DPS_SQLRES	SQLRes;
  size_t ndel = 0, mdel = 2048, WrdFiles, nitems, total_deleted = 0;
  unsigned long offset = 0;
  urlid_t *todel, prev_urlid;
  DPS_LOGD_CMD	cmd;
  DPS_BASEITEM Item;
  DPS_BASE_PARAM Q, I;

  TRACE_IN(A, "DpsLogdCachedCheck");

  DpsSQLResInit(&SQLRes);

  todel = (int*)DpsMalloc(mdel * sizeof(urlid_t));
  if (todel == NULL) {TRACE_OUT(A); return DPS_ERROR; }

  bzero(&I, sizeof(I));
  I.subdir = DPS_URLDIR;
  I.basename = "info";
  I.indname = "info";
  I.mode = DPS_WRITE_LOCK;
  I.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
  I.A = A;
#ifdef HAVE_ZLIB
  I.zlib_method = Z_DEFLATED;
  I.zlib_level = 9;
  I.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
  I.zlib_memLevel = 9;
  I.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif
  bzero(&Q, sizeof(Q));
  Q.subdir = DPS_TREEDIR;
  Q.basename = "wrd";
  Q.indname = "wrd";
  Q.mode = DPS_WRITE_LOCK;
  Q.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
  Q.A = A;
#ifdef HAVE_ZLIB
  Q.zlib_method = Z_DEFLATED;
  Q.zlib_level = 9;
  Q.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
  Q.zlib_memLevel = 9;
  Q.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif

  I.NFiles = (db->URLDataFiles) ? (int)db->URLDataFiles : DpsVarListFindInt(&A->Vars, "URLDataFiles", 0x300);
  Q.NFiles = WrdFiles = (db->WrdFiles) ? (int)db->WrdFiles : DpsVarListFindInt(&A->Vars, "WrdFiles", 0x300);

  DpsLog(A, DPS_LOG_INFO, "Cached database checkup started. 0x%x files to check.", WrdFiles);

  recs = DpsVarListFindInt(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);

  if (level > 2) {

    DpsLog(A, DPS_LOG_EXTRA, "update cachedchk2 table");

    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
    res = DpsSQLAsyncQuery(db, NULL, "DELETE FROM cachedchk2");
    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
    if(DPS_OK != res) { TRACE_OUT(A); return res; }

    while (u) {
      dps_snprintf(dat_name,sizeof(dat_name),"SELECT rec_id FROM url ORDER BY rec_id LIMIT %d OFFSET %ld",
		 recs, offset);

      if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
      res = DpsSQLQuery(db, &SQLRes, dat_name);
      if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
      if(DPS_OK != res) { TRACE_OUT(A); return res; }
      nitems = DpsSQLNumRows(&SQLRes);
      for( i = 0; i < nitems; i++) {
	dps_snprintf(dat_name, sizeof(dat_name), "INSERT INTO cachedchk2 (url_id) VALUES (%s)", DpsSQLValue(&SQLRes, i, 0));

	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	res = DpsSQLAsyncQuery(db, NULL, dat_name);
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	if(DPS_OK != res) {
	  DpsBaseClose(&I);
	  DpsBaseClose(&Q);
	  DpsSQLFree(&SQLRes);
	  TRACE_OUT(A);
	  return res;
	}
      }
      DpsSQLFree(&SQLRes);
      offset += nitems;
      u = (nitems == (size_t)recs);
    }
  }


  DpsLog(A, DPS_LOG_EXTRA, "update cachedchk table");

  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
  res = DpsSQLAsyncQuery(db, NULL, "DELETE FROM cachedchk");
  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
  if(DPS_OK != res) { TRACE_OUT(A); return res; }

  bstart = (size_t)A->now % WrdFiles;
  for (z = 0; z < WrdFiles; z++) {
    b = (z + bstart) % WrdFiles;

    /* To see the URL being indexed in "ps" output on xBSD */
    if (DpsNeedLog(DPS_LOG_EXTRA)) dps_setproctitle("[%d] cached checkup %04x, %d of %d", A->handle, b, z + 1, WrdFiles);
    DpsLog(A, DPS_LOG_EXTRA, "cached checkup %04x, %d of %d", b, z + 1, WrdFiles);

    DpsBaseOptimize(&Q, (int)b);
    DpsBaseClose(&Q);

    if (level == 1) continue;

    dps_snprintf(dat_name, sizeof(dat_name), "%s/tree/wrd%04x.i", Q.vardir, b);
    dat_fd = DpsOpen2(dat_name, O_RDONLY | DPS_BINARY);
    if (dat_fd <= 0) {
      dat_fd = DpsOpen2(dat_name, O_RDONLY | DPS_BINARY);
      if (dat_fd <= 0) {
	dps_strerror(A, DPS_LOG_ERROR, "Can't open file '%s'", dat_name);
	continue;
      }
    }
    DpsReadLock(dat_fd);
     while (read(dat_fd, &Item, sizeof(Item)) == sizeof(Item)) {
       if (Item.rec_id != 0) {
	if (ndel >= mdel) {
	  mdel += 1024;
	  todel = (urlid_t*)DpsRealloc(todel, mdel * sizeof(urlid_t));
	  if (todel == NULL) {
	    DpsBaseClose(&I);
	    DpsBaseClose(&Q);
	    DpsClose(dat_fd);
	    DpsLog(A, DPS_LOG_ERROR, "Can't realloc %d bytes (mdel:%d) at %s:%d", mdel*sizeof(urlid_t), mdel, __FILE__, __LINE__);
	    TRACE_OUT(A);
	    return DPS_ERROR;
	  }
	}
	todel[ndel++] = Item.rec_id;
      }
    }
    DpsUnLock(dat_fd);
    DpsClose(dat_fd);


    for (i = 0; i < ndel; i++) {
      Q.rec_id = todel[i];
      crd = (DPS_URL_CRD*)DpsBaseARead(&Q, &ncrd);
      DpsBaseClose(&Q);
      if (crd == NULL) continue;
      ncrd /= sizeof(DPS_URL_CRD);

/*      DpsSort(crd, (size_t)ncrd, sizeof(DPS_URL_CRD), (qsort_cmp)DpsCmpURL_CRD);*/ /* it's sorted already */

      prev_urlid = 0;
      for (j = 0; j < ncrd; j++) {

	if (crd[j].url_id == prev_urlid) continue;
	prev_urlid = crd[j].url_id;

	if (level > 2) {
	  dps_snprintf(dat_name, sizeof(dat_name), "DELETE FROM cachedchk2 WHERE url_id=%d", crd[j].url_id);
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	  res = DpsSQLAsyncQuery(db, NULL, dat_name);
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	}

	dps_snprintf(dat_name, sizeof(dat_name), "SELECT COUNT(*) FROM url WHERE rec_id=%d", crd[j].url_id);
	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	res = DpsSQLQuery(db, &SQLRes, dat_name);
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	if(DPS_OK != res) {
	  DpsBaseClose(&I);
	  DpsBaseClose(&Q);
	  DpsLog(A, DPS_LOG_ERROR, "SQL req. \"%s\" error", dat_name);
	  DPS_FREE(todel);
	  DPS_FREE(crd);
	  TRACE_OUT(A);
	  return res;
	}
	if (DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0)) == 0) {

	  DpsSQLFree(&SQLRes);
	  dps_snprintf(dat_name, sizeof(dat_name), "SELECT COUNT(*) FROM cachedchk WHERE rec_id=%d", crd[j].url_id);
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	  res = DpsSQLQuery(db, &SQLRes, dat_name);
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	  if(DPS_OK != res) {
	    DpsBaseClose(&I);
	    DpsBaseClose(&Q);
	    DpsLog(A, DPS_LOG_ERROR, "SQL req. \"%s\" error", dat_name);
	    DPS_FREE(todel);
	    DPS_FREE(crd);
	    TRACE_OUT(A);
	    DpsSQLFree(&SQLRes);
	    return res;
	  }
	  if (DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0)) == 0) {

	    total_deleted++;
	    dps_snprintf(dat_name, sizeof(dat_name), "INSERT INTO cachedchk VALUES(%d)", crd[j].url_id);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    res = DpsSQLAsyncQuery(db, NULL, dat_name);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    if(DPS_OK != res) {
	      DpsBaseClose(&I);
	      DpsBaseClose(&Q);
	      DpsLog(A, DPS_LOG_ERROR, "SQL req. \"%s\" error", dat_name);
	      DPS_FREE(todel);
	      DPS_FREE(crd);
	      TRACE_OUT(A);
	      DpsSQLFree(&SQLRes);
	      return res;
	    }
	    I.rec_id = cmd.url_id = crd[j].url_id;
	    DpsBaseDelete(&I);
	    DpsBaseClose(&I);

	    cmd.stamp = A->now;
	    cmd.cmd = DPS_LOGD_CMD_WORD;
	    cmd.nwords = 0;

	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    if (DPS_OK != DpsDeleteURLFromCache(A, crd[j].url_id, db)) {
	      if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	      DpsBaseClose(&I);
	      DpsBaseClose(&Q);
	      DpsSQLFree(&SQLRes);
	      DpsLog(A, DPS_LOG_ERROR, "StoreDoc error %s:%d", __FILE__, __LINE__ );
	      DPS_FREE(todel);
	      DPS_FREE(crd);
	      TRACE_OUT(A);
	      return DPS_ERROR;
	    }
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);

	  }
	}
	DpsSQLFree(&SQLRes);
	
	if (have_sigterm || have_sigint || have_sigalrm) {
	  DpsBaseClose(&I);
	  DpsBaseClose(&Q);
	  DpsLog(A, DPS_LOG_EXTRA, "%s signal received. Exiting chackup", (have_sigterm) ? "SIGTERM" :
		 (have_sigint) ? "SIGINT" : "SIGALRM");
	  DPS_FREE(todel);
	  DPS_FREE(crd);
	  TRACE_OUT(A);
	  return DPS_OK;
	}
      }
      DPS_FREE(crd);
    }
    DpsBaseClose(&I);
    DpsBaseClose(&Q);

    DpsLog(A, DPS_LOG_INFO, "tree/wrd%04X, total %d lost records found", b, total_deleted);

    ndel = 0;
  }

#if 0
  u = 1;
  offset = 0;
  DpsLog(A, DPS_LOG_EXTRA, "delete lost urls found");

  while (u) {
    dps_snprintf(dat_name, sizeof(dat_name), "SELECT rec_id FROM cachedchk ORDER BY rec_id LIMIT %d OFFSET %ld", recs, offset);
    if(DPS_OK != (res = DpsSQLQuery(db, &SQLRes, dat_name))) { TRACE_OUT(A); return res; }
    nitems = DpsSQLNumRows(&SQLRes);
    for( i = 0; i < nitems; i++) {

      I.rec_id = cmd.url_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLRes, i, 0));

      DpsBaseDelete(&I);
      DpsBaseClose(&I);

      cmd.stamp = A->now;
      cmd.cmd = DPS_LOGD_CMD_WORD;
      cmd.nwords = 0;

      if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
      if(DPS_OK != DpsLogdStoreDoc(A, cmd, NULL, db)) {
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	DpsBaseClose(&I);
	DpsBaseClose(&Q);
	DpsSQLFree(&SQLRes);
	DpsLog(A, DPS_LOG_ERROR, "StoreDoc error %s:%d", __FILE__, __LINE__ );
	DPS_FREE(todel);
	DPS_FREE(crd);
	TRACE_OUT(A);
	return DPS_ERROR;
      }
      if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);

    }
    DpsSQLFree(&SQLRes);
    offset += nitems;
    u = (nitems == (size_t)recs);
  }
#endif

  if (level > 2) {
    u = 1;
    offset = 0;
    DpsLog(A, DPS_LOG_EXTRA, "force reindexing for urls without words");

    while (u) {
      dps_snprintf(dat_name, sizeof(dat_name), "SELECT url_id FROM cachedchk2 ORDER BY url_id LIMIT %d OFFSET %ld", recs, offset);
      if(DPS_OK != (res = DpsSQLQuery(db, &SQLRes, dat_name))) { TRACE_OUT(A); return res; }
      nitems = DpsSQLNumRows(&SQLRes);
      for( i = 0; i < nitems; i++) {
	dps_snprintf(dat_name,sizeof(dat_name), "UPDATE url SET last_mod_time=0,next_index_time=0 WHERE rec_id=%s AND status IN (200,206,302,304)", 
		     DpsSQLValue(&SQLRes, i, 0));

	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	res = DpsSQLAsyncQuery(db, NULL, dat_name);
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	if(DPS_OK != res) {
	  DpsSQLFree(&SQLRes);
	  TRACE_OUT(A);
	  return res;
	}
      }
      DpsSQLFree(&SQLRes);
      offset += nitems;
      u = (nitems == (size_t)recs);
    }
  }

  DPS_FREE(todel);

  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
  res = DpsSQLAsyncQuery(db, NULL, "DELETE FROM cachedchk2");
  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
  if(DPS_OK != res) { TRACE_OUT(A); return res; }

  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
  res = DpsSQLAsyncQuery(db, NULL, "DELETE FROM cachedchk");
  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
  if(DPS_OK != res) { TRACE_OUT(A); return res; }

  DpsLog(A, DPS_LOG_INFO, "Cached database checkup finished.");

  TRACE_OUT(A);
#endif
  return DPS_OK;
}

int DpsCachedCheck(DPS_AGENT *A, int level) {
	size_t i, dbfrom = 0, dbto;
	DPS_DB	*db;
	int sent, cached_sd, cached_rv;
	ssize_t recvt;
	char reply;
	DPS_LOGD_CMD cmd;
	DPS_BASE_PARAM I;

	TRACE_IN(A, "DpsCached");

	bzero(&I, sizeof(I));
	I.subdir = DPS_URLDIR;
	I.basename = "info";
	I.indname = "info";
	I.mode = DPS_WRITE_LOCK;
	I.vardir = DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
	I.A = A;
#ifdef HAVE_ZLIB
	I.zlib_method = Z_DEFLATED;
	I.zlib_level = 9;
	I.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
	I.zlib_memLevel = 9;
	I.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif
	DPS_GETLOCK(A, DPS_LOCK_CONF);
	dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	DPS_RELEASELOCK(A, DPS_LOCK_CONF);
	I.NFiles = DpsVarListFindInt(&A->Vars, "URLDataFiles", 0x300);

	for (i = dbfrom; i < dbto; i++) {
	  DPS_GETLOCK(A, DPS_LOCK_CONF);
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  DPS_RELEASELOCK(A, DPS_LOCK_CONF);
	  if (db->DBMode != DPS_DBMODE_CACHE) {
	    continue;
	  }

	  cmd.nwords = 0;
	  cmd.stamp = A->now;
	  cmd.url_id = level;
	  cmd.cmd = DPS_LOGD_CMD_CHECK;

	  cached_sd = A->Demons.nitems ? A->Demons.Demon[db->dbnum].cached_sd : 0;
	  cached_rv = A->Demons.nitems ? A->Demons.Demon[db->dbnum].cached_rv : 0;

	  if(0/*cached_sd*/) {
		sent = DpsSend(cached_sd, &cmd, sizeof(cmd), 0);
		if(sent!=sizeof(cmd)){
			dps_strerror(A, DPS_LOG_ERROR, "[%s:%d] Can't write to cached", __FILE__, __LINE__);
			TRACE_OUT(A);
			return(DPS_ERROR);
		}
		while ((recvt = DpsRecvall(cached_rv, &reply, sizeof(char), WAIT_TIME)) != 1) {
		  if (recvt <= 0) {
			dps_strerror(A, DPS_LOG_ERROR, "Can't receive from cached [%s:%d]: %d",__FILE__,__LINE__, recvt);
			TRACE_OUT(A);
			return(DPS_ERROR);
		  }
		  DPSSLEEP(0);
		}
		if (reply != 'O') {
			DpsLog(A, DPS_LOG_ERROR, "Incorrect reply received from cached %s:%d", __FILE__, __LINE__);
			TRACE_OUT(A);
			return DPS_ERROR;
		}
	  } else {
	    DpsLogdCachedCheck(A, db, level);

	    if (level > 1) DpsBaseCheckup(&I, DpsCheckUrlid);
	    DpsBaseOptimize(&I, -1);
	    DpsBaseClose(&I);

	  }
	}
	TRACE_OUT(A);
	return DPS_OK;
}

/* convert from 4.23 to 4.24 */
int DpsCacheConvert(DPS_AGENT *Indexer) {
  DPS_BASEITEM New;
  DPS_BASEITEM_4_23 Old;
  const char *vardir = DpsVarListFindStr(&Indexer->Vars, "VarDir", DPS_VAR_DIR); 
  const size_t WrdFiles = (size_t)DpsVarListFindInt(&Indexer->Vars, "WrdFiles", 0x300);
  const size_t URLFiles = (size_t)DpsVarListFindInt(&Indexer->Vars, "URLDataFiles", 0x300);
  const size_t StoreFiles = (size_t)DpsVarListFindInt(&Indexer->Vars, "StoredFiles", 0x100);
  size_t b, n;
  int new_fd, old_fd;
  char filename[PATH_MAX];
  char command[2*PATH_MAX];
			  
  /*********************************/

  for (b = 0; b < WrdFiles; b++) {
    dps_snprintf(filename, sizeof(filename), "%s/%s/%s%04x.i", vardir, "tree", "wrd", DPS_FILENO(b << DPS_BASE_BITS, WrdFiles));
    DpsLog(Indexer, DPS_LOG_INFO, "Converting %s", filename);
    if ((new_fd = DpsOpen3("dps_tmp", O_RDWR | O_CREAT | DPS_BINARY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
      DpsLog(Indexer, DPS_LOG_ERROR, "Can't open dps_tmp file");
      continue;
    }
    if ((old_fd = DpsOpen2(filename, O_RDONLY | DPS_BINARY)) < 0) {
      DpsLog(Indexer, DPS_LOG_ERROR, "Can't open '%s' file", filename);
      DpsClose(new_fd);
      continue;
    }
    DpsWriteLock(old_fd);
    while(read(old_fd, &Old, sizeof(DPS_BASEITEM_4_23)) == sizeof(DPS_BASEITEM_4_23)) {
      New.orig_size = 0;
      New.rec_id = Old.rec_id;
      New.offset = Old.offset;
      New.next = Old.next / sizeof(DPS_BASEITEM_4_23) * sizeof(DPS_BASEITEM);
      New.size = Old.size;
      if (write(new_fd, &New, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
	continue;
      }
      n++;
    }
    dps_snprintf(command, sizeof(command), "mv dps_tmp %s", filename);
    DpsClose(new_fd);
    DpsUnLock(old_fd);
    DpsClose(old_fd);
    (void)system(command);
    DpsLog(Indexer, DPS_LOG_INFO, "Done %s", filename);
  }

/*********************************/

  for (b = 0; b < URLFiles; b++) {
    dps_snprintf(filename, sizeof(filename), "%s/%s/%s%04x.i", vardir, "url", "info", DPS_FILENO(b << DPS_BASE_BITS, URLFiles));
    DpsLog(Indexer, DPS_LOG_INFO, "Converting %s", filename);
    if ((new_fd = DpsOpen3("dps_tmp", O_RDWR | O_CREAT | DPS_BINARY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
      continue;
    }
    if ((old_fd = DpsOpen2(filename, O_RDONLY | DPS_BINARY)) < 0) {
      DpsClose(new_fd);
      continue;
    }
    DpsWriteLock(old_fd);
    while(read(old_fd, &Old, sizeof(DPS_BASEITEM_4_23)) == sizeof(DPS_BASEITEM_4_23)) {
      New.orig_size = 0;
      New.rec_id = Old.rec_id;
      New.offset = Old.offset;
      New.next = Old.next / sizeof(DPS_BASEITEM_4_23) * sizeof(DPS_BASEITEM);
      New.size = Old.size;
      if (write(new_fd, &New, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
	continue;
      }
    }
    dps_snprintf(command, sizeof(command), "mv dps_tmp %s", filename);
    DpsClose(new_fd);
    DpsUnLock(old_fd);
    DpsClose(old_fd);
    (void)system(command);
    DpsLog(Indexer, DPS_LOG_INFO, "Done %s", filename);
  }

/*********************************/

  for (b = 0; b < StoreFiles; b++) {
    dps_snprintf(filename, sizeof(filename), "%s/%s/%s%04x.i", vardir, "store", "doc", DPS_FILENO(b << DPS_BASE_BITS, StoreFiles));
    DpsLog(Indexer, DPS_LOG_INFO, "Converting %s", filename);
    if ((new_fd = DpsOpen3("dps_tmp", O_RDWR | O_CREAT | DPS_BINARY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
      continue;
    }
    if ((old_fd = DpsOpen2(filename, O_RDONLY | DPS_BINARY)) < 0) {
      DpsClose(new_fd);
      continue;
    }
    DpsWriteLock(old_fd);
    while(read(old_fd, &Old, sizeof(DPS_BASEITEM_4_23)) == sizeof(DPS_BASEITEM_4_23)) {
      New.orig_size = 0;
      New.rec_id = Old.rec_id;
      New.offset = Old.offset;
      New.next = Old.next / sizeof(DPS_BASEITEM_4_23) * sizeof(DPS_BASEITEM);
      New.size = Old.size;
      if (write(new_fd, &New, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
	continue;
      }
    }
    dps_snprintf(command, sizeof(command), "mv dps_tmp %s", filename);
    DpsClose(new_fd);
    DpsUnLock(old_fd);
    DpsClose(old_fd);
    (void)system(command);
    DpsLog(Indexer, DPS_LOG_INFO, "Done %s", filename);
  }
			  
  return DPS_OK;

}


/* cache:// */


int DpsAddURLCache(DPS_AGENT *A, DPS_DOCUMENT *Doc, DPS_DB *db) {
  DPS_BASE_PARAM P;
  DPS_LOGD_CMD cmd;
  urlid_t rec_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
  int sent, cached_sd, cached_rv, res;
  ssize_t recvt;
  dps_uint4 tlen;
  char    *textbuf;
  char reply;

  TRACE_IN(A, "DpsAddURLCache");

  textbuf = DpsDocToTextBuf(Doc, 0, 1);
  if (textbuf == NULL) { TRACE_OUT(A); return DPS_ERROR; }
  tlen = (dps_uint4)dps_strlen(textbuf) + 1;

  cached_sd = A->Demons.nitems ? A->Demons.Demon[db->dbnum].cached_sd : 0;
  cached_rv = A->Demons.nitems ? A->Demons.Demon[db->dbnum].cached_rv : 0;

  if (cached_sd) {
    cmd.stamp = A->now;
    cmd.url_id = rec_id;
    cmd.cmd = DPS_LOGD_CMD_URLINFO;
    cmd.nwords = 0;

#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(A->TR, "[%d] DpsAddURLCache: sending cmd\n", A->handle);
	  fprintf(A->TR, "cmd=%d nwords=%d stamp=%d url_id=%d\n", cmd.cmd, cmd.nwords, (int)cmd.stamp, cmd.url_id);
	  fflush(A->TR);
#endif
		sent = DpsSend(cached_sd, &cmd, sizeof(DPS_LOGD_CMD), 0);
		if(sent!=sizeof(DPS_LOGD_CMD)) {
			dps_strerror(A, DPS_LOG_ERROR, "%s [%d] Can't write to cached", __FILE__, __LINE__);
			DpsFree(textbuf);
			TRACE_OUT(A);
			return DPS_ERROR;
		}
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(A->TR, "cmd=%d nwords=%d stamp=%d url_id=%d\n", cmd.cmd, cmd.nwords, (int)cmd.stamp, cmd.url_id);
	  fprintf(A->TR, "[%d] DpsAddURLCache: receiving reply for cmd\n", A->handle);
	  fflush(A->TR);
#endif
	  while ((recvt = DpsRecvall(cached_rv, &reply, 1, WAIT_TIME)) != 1) {
		  if (recvt <= 0) {
			DpsLog(A, DPS_LOG_ERROR, "Can't receive from cached [%s:%d] %d",__FILE__,__LINE__, recvt);
			DpsFree(textbuf);
			TRACE_OUT(A);
			return DPS_ERROR;
		  }
		  DPSSLEEP(0);
		}
		if (reply != 'O') {
			DpsLog(A, DPS_LOG_ERROR, "Can't incorrect reply from cached %s:%d",__FILE__, __LINE__);
			DpsFree(textbuf);
			TRACE_OUT(A);
			return DPS_ERROR;
		}




#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(A->TR, "[%d] DpsAddURLCache: sending tlen: %d\n", A->handle, tlen);
	  fflush(A->TR);
#endif
		sent = DpsSend(cached_sd, &tlen, sizeof(tlen), 0);
		if(sent != sizeof(tlen)){
			dps_strerror(A, DPS_LOG_ERROR, "%s [%d] Can't write to cached", __FILE__, __LINE__);
			DpsFree(textbuf);
			TRACE_OUT(A);
			return DPS_ERROR;
		}
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(A->TR, "[%d] DpsAddURLCache: receiving reply for tlen\n", A->handle);
	  fflush(A->TR);
#endif
	  while ((recvt = DpsRecvall(cached_rv, &reply, 1, WAIT_TIME)) != 1) {
		  if (recvt <= 0) {
			dps_strerror(A, DPS_LOG_ERROR, "Can't receive from cached [%s:%d] %d",__FILE__, __LINE__, recvt);
			DpsFree(textbuf);
			TRACE_OUT(A);
			return DPS_ERROR;
		  }
		  DPSSLEEP(0);
		}
		if (reply != 'O') {
			DpsLog(A, DPS_LOG_ERROR, "Can't incorrect reply from cached %s:%d",__FILE__, __LINE__);
			DpsFree(textbuf);
			TRACE_OUT(A);
			return DPS_ERROR;
		}

#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(A->TR, "[%d] DpsAddURLCache: sending tbuf\n", A->handle);
	  fflush(A->TR);
#endif
		sent = DpsSend(cached_sd, textbuf, tlen, 0);
		if(sent != (ssize_t)tlen){
			dps_strerror(A, DPS_LOG_ERROR, "%s [%d] Can't write to cached", __FILE__, __LINE__);
			DpsFree(textbuf);
			TRACE_OUT(A);
			return DPS_ERROR;
		}
#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
	  fprintf(A->TR, "[%d] DpsAddURLCache: done\n", A->handle);
	  fflush(A->TR);
#endif

	  while ((recvt = DpsRecvall(cached_rv, &reply, sizeof(char), WAIT_TIME)) != 1) {
	    if (recvt <= 0) {
	      dps_strerror(A, DPS_LOG_ERROR, "Can't receive from cached [%s:%d] %d",__FILE__, __LINE__, recvt);
	      DpsFree(textbuf);
	      TRACE_OUT(A);
	      return DPS_ERROR;
	    }
	    DPSSLEEP(0);
	  }
	  if (reply != 'O') {
	    DpsLog(A, DPS_LOG_ERROR, "Can't incorrect reply from cached %s:%d", __FILE__, __LINE__);
	    DpsFree(textbuf);
	    TRACE_OUT(A);
	    return DPS_ERROR;
	  }


  } else {

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
    P.NFiles = (db->URLDataFiles) ? (int)db->URLDataFiles : DpsVarListFindInt(&A->Vars, "URLDataFiles", 0x300);
    P.mode = DPS_WRITE_LOCK;
    P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
    P.A = A;
    P.rec_id = rec_id;
    if ((res = DpsBaseWrite(&P, textbuf, tlen)) != DPS_OK) DpsBaseClose(&P);
    else res =  DpsBaseClose(&P);
    DpsFree(textbuf);
    TRACE_OUT(A);
    return res;
  }
  DpsFree(textbuf);
  TRACE_OUT(A);
  return DPS_OK;
}

static int DpsDeleteURLCache(DPS_AGENT *A, DPS_DOCUMENT *Doc, DPS_DB *db) {
  urlid_t rec_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
  int res;

  TRACE_IN(A, "DpsDeleteURLCache");

#if defined(WITH_TRACE) && defined(DEBUG_SEARCH)
  fprintf(A->TR, "[%d] DpsDeleteURLCache\n", A->handle);
  fflush(A->TR);
#endif
  res = DpsDeleteURLFromCache(A, rec_id, db);
  TRACE_OUT(A);
  return res;
}


int DpsResAddDocInfoCache(DPS_AGENT *query, DPS_DB *db, DPS_RESULT *Res, size_t dbnum) {
  DPS_BASE_PARAM P;
  char   *docinfo;
  int	 res, use_showcnt = !strcasecmp(DpsVarListFindStr(&query->Vars, "PopRankUseShowCnt", "no"), "yes");
  double pr, ratio = 0.0;
  size_t i, len;
  char qbuf[128];
  const char *url;

  TRACE_IN(query, "DpsResAddDocInfoCache");

  if(!Res->num_rows) {TRACE_OUT(query); return DPS_OK;}
  if (use_showcnt) ratio = DpsVarListFindDouble(&query->Vars, "PopRankShowCntRatio", 25.0);
  DpsLog(query, DPS_LOG_DEBUG, "use_showcnt: %d  ratio: %f", use_showcnt, ratio);

  bzero(&P, sizeof(P));
  P.subdir = DPS_URLDIR;
  P.basename = "info";
  P.indname = "info";
  P.NFiles = (db->URLDataFiles) ? (int)db->URLDataFiles : DpsVarListFindInt(&query->Vars, "URLDataFiles", 0x300);
  P.mode = DPS_READ_LOCK;
  P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&query->Vars, "VarDir", DPS_VAR_DIR);
  P.A = query;
#ifdef HAVE_ZLIB
  P.zlib_method = Z_DEFLATED;
  P.zlib_level = 9;
  P.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
  P.zlib_memLevel = 9;
  P.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif

  for(i = 0; i < Res->num_rows; i++) {
    DPS_DOCUMENT *D = &Res->Doc[i];
    urlid_t	 url_id = DpsVarListFindInt(&D->Sections, "DP_ID", 0);
    
#ifdef WITH_MULTIDBADDR
    if (Res->Doc[i].dbnum != db->dbnum) continue;
#endif		  
    P.rec_id = url_id;
    if ((docinfo = (char*)DpsBaseARead(&P, &len)) == NULL) continue;

    if (P.Item.rec_id != url_id) { DPS_FREE(docinfo); continue; }

/*    fprintf(stderr, " -- DocInfoCache: %s\n", docinfo);*/
    DpsDocFromTextBuf(D, docinfo);
    DPS_FREE(docinfo);
    if ((url = DpsVarListFindStr(&D->Sections, "URL", NULL)) != NULL) {
      if (!DpsURLParse(&D->CurURL, url)) {
/*	DpsVarListInsStr(&D->Sections, "url.host", DPS_NULL2EMPTY(D->CurURL.hostname));
	DpsVarListInsStr(&D->Sections, "url.path", DPS_NULL2EMPTY(D->CurURL.path));
	DpsVarListInsStr(&D->Sections, "url.directory", DPS_NULL2EMPTY(D->CurURL.directory));
	DpsVarListInsStr(&D->Sections, "url.file", DPS_NULL2EMPTY(D->CurURL.filename));*/
	D->fetched = 1;
	Res->fetched++;
      }
    }
#ifdef HAVE_SQL
    if (use_showcnt && (db->DBType != DPS_DB_CACHE)) {
      pr = DPS_ATOF(DpsVarListFindStr(&D->Sections, "Score", "0.0"));
      if (pr >= ratio) {
	dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET shows=shows+1 WHERE rec_id=%i", url_id);
	DpsSQLAsyncQuery(db, NULL, qbuf);
      }
    }
#endif
  }
  res = DpsBaseClose(&P);
  TRACE_OUT(query);
  return res;
}


static int DpsCachedResort(DPS_AGENT *A, DPS_DB *db) {
  DPS_BASE_PARAM P;
  DPS_URL_CRD *data;
  urlid_t *recs;
  urlid_t base;
  size_t i, n, m = 4096, n_data, len;

  recs = (urlid_t*)DpsMalloc(m * sizeof(urlid_t));
  if (recs == NULL) return DPS_ERROR;

  bzero(&P, sizeof(P));

  P.subdir = DPS_TREEDIR;
  P.basename = "wrd";
  P.indname = "wrd";
  P.mode = DPS_WRITE_LOCK;
  P.NFiles = (db->WrdFiles) ? db->WrdFiles : DpsVarListFindUnsigned(&A->Vars, "WrdFiles", 0x300);
  P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
  P.A = A;
#ifdef HAVE_ZLIB
  P.zlib_method = Z_DEFLATED;
  P.zlib_level = 9;
  P.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
  P.zlib_memLevel = 9;
  P.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif

  for(base = 0; base < (urlid_t)P.NFiles; base++) {
    P.rec_id = (base << DPS_BASE_BITS);
    DpsLog(A, DPS_LOG_EXTRA, "Resorting base: %d [0x%x]", base, base);
    if (DpsBaseSeek(&P, DPS_WRITE_LOCK) != DPS_OK) {
      DpsLog(A, DPS_LOG_ERROR, "Can't open base %s/%s {%s:%d}", P.subdir, P.basename, __FILE__, __LINE__);
      DpsBaseClose(&P);
      DPS_FREE(recs);
      return DPS_ERROR;
    }
    if (lseek(P.Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
      DpsLog(A, DPS_LOG_ERROR, "Can't seek %s {%s:%d}", P.Ifilename, __FILE__, __LINE__);
      DpsBaseClose(&P);
      DPS_FREE(recs);
      return DPS_ERROR;
    }
    n = 0;
    while(read(P.Ifd, &P.Item, sizeof(DPS_BASEITEM)) == sizeof(DPS_BASEITEM)) {
      if ((P.Item.rec_id != 0) && (P.Item.size > 0)) {
	if (n >= m) {
	  m += 1024;
	  recs = (urlid_t*)DpsRealloc(recs, m * sizeof(urlid_t));
	  if (recs == NULL) { DpsBaseClose(&P); DPS_FREE(recs); return DPS_ERROR; }
	}
	recs[n] = P.Item.rec_id;
	n++;
      }
    }
    DpsLog(A, DPS_LOG_EXTRA, " - number of records: %d\n", n);
    for (i = 0; i < n; i++) {
      P.rec_id = recs[i];
      DpsLog(A, DPS_LOG_DEBUG, " - resorting record: %d [%x]", recs[i], recs[i]);
      if ((data = (DPS_URL_CRD*)DpsBaseARead(&P, &len)) == NULL) continue;
      n_data = len / sizeof(DPS_URL_CRD);
      DpsSortSearchWordsByURL0(data, n_data);
      DpsBaseWrite(&P, data, n_data * sizeof(DPS_URL_CRD));
      DPS_FREE(data);
    }
  }
  DpsLog(A, DPS_LOG_EXTRA, "Resorting done.");
  DpsBaseClose(&P);
  DPS_FREE(recs);
  return DPS_OK;
}


int __DPSCALL DpsURLActionCache(DPS_AGENT *A, DPS_DOCUMENT *D, int cmd, DPS_DB *db) {
	int res;

	TRACE_IN(A, "DpsURLActionCache");

	switch(cmd){
/*		case DPS_URL_ACTION_ADD:*/
		case DPS_URL_ACTION_LUPDATE:
		case DPS_URL_ACTION_UPDCLONE:
			res = DpsAddURLCache(A, D, db);
			break;
			
		case DPS_URL_ACTION_DELETE:
			res = DpsDeleteURLCache(A, D, db);
			break;
	        case DPS_URL_ACTION_DELWORDS:
		        DpsWordListFree(&D->Words);
			DpsCrossListFree(&D->CrossWords);
	        case DPS_URL_ACTION_INSWORDS:
			res = DpsStoreWordsCache(A, D, db);
			break;
	        case DPS_URL_ACTION_RESORT:
			res = DpsCachedResort(A, db);
			break;
	        case DPS_URL_ACTION_REHASHSTORED:
		        res = DPS_OK;
			break;
		default:
 			res=DPS_OK;
	}
	TRACE_OUT(A);
	return res;
}

int DpsResActionCache(DPS_AGENT *Agent, DPS_RESULT *Res, int cmd, DPS_DB *db, size_t dbnum) {
  int res;
  TRACE_IN(Agent, "DpsResActionCache");
  switch(cmd){
  case DPS_RES_ACTION_DOCINFO:
    res = DpsResAddDocInfoCache(Agent, db, Res, dbnum);
    break;
  default:
    res = DPS_OK;
  }
  TRACE_OUT(Agent);
  return res;
}

int DpsStatActionCache(DPS_AGENT *A, DPS_STATLIST *S, DPS_DB *db) { /* FIXME: need to implemet */
  int res = DPS_OK;
  TRACE_IN(A, "DpsStatActionCache");

  TRACE_OUT(A);
  return res;
}
