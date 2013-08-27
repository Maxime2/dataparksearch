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
#include "dps_db.h"
#include "dps_db_int.h"
#include "dps_sqldbms.h"
#include "dps_cache.h"
#include "dps_utils.h"
#include "dps_charsetutils.h"
#include "dps_url.h"
#include "dps_sdp.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_searchtool.h"
#include "dps_result.h"
#include "dps_log.h"
#include "dps_proto.h"
#include "dps_host.h"
#include "dps_hash.h"
#include "dps_doc.h"
#include "dps_cache.h"
#include "dps_services.h"
#include "dps_xmalloc.h"
#include "dps_searchcache.h"
#include "dps_store.h"
#include "dps_agent.h"
#include "dps_match.h"
#include "dps_template.h"
#include "dps_conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
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
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_MSG_H
#include <sys/msg.h>
#endif
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#include <assert.h>

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif


#define DPS_THREADINFO(A,s,m)	if(A->Conf->ThreadInfo)A->Conf->ThreadInfo(A,s,m)


#define DEBUG

/*#define NEWMERGE*/


void *DpsDBInit(void *vdb){
	DPS_DB *db=vdb;
	size_t	nbytes=sizeof(DPS_DB);
	
	if(!db){
		db=(DPS_DB*)DpsMalloc(nbytes);
		if (db == NULL) return NULL;
		bzero((void*)db, nbytes);
		db->freeme=1;
	}else{
		bzero((void*)db, nbytes);
	}
	db->numtables=32;

	DpsURLInit(&db->addrURL);
	
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_SAPDB || HAVE_DB2)
	db->hDbc=SQL_NULL_HDBC;
	db->hEnv=SQL_NULL_HENV;
	db->hstmt=SQL_NULL_HSTMT;
#endif
#if (HAVE_IBASE)
	db->DBH=NULL;
#endif
#if (HAVE_ORACLE8)
	db->par = DpsXmalloc(sizeof(struct param_struct));
	if (db->par == NULL) {
	  DpsDBFree(db);
	  return NULL;
	}
#endif
	return db;
}


void DpsDBFree(void *vdb){
	DPS_DB	*db=vdb;

#ifdef HAVE_SQL
	DpsSQLFree(&db->Res);
#endif
	
	DpsURLFree(&db->addrURL);
	DPS_FREE(db->DBADDR);
	DPS_FREE(db->DBName);
	DPS_FREE(db->DBUser);
	DPS_FREE(db->DBPass);
	DPS_FREE(db->DBSock);
	DPS_FREE(db->DBCharset);
	DPS_FREE(db->where);
	DPS_FREE(db->from);
	DPS_FREE(db->label);
	DPS_FREE(db->vardir);

	/*if (db->searchd[0] || db->searchd[1])*/ DpsSearchdClose(db);
	
	if(!db->connected)goto ret;

#ifdef HAVE_SQL
	
#if HAVE_DP_MYSQL
	if(db->DBDriver==DPS_DB_MYSQL){
		DpsSQLClose(db);
		goto ret;
	}
#endif
	
#if HAVE_DP_PGSQL
	if(db->DBDriver==DPS_DB_PGSQL){
		DpsSQLClose(db);
		goto ret;
	}
#endif
	
#if HAVE_DP_MSQL
	if(db->DBDriver==DPS_DB_MSQL){
		DpsSQLClose(db);
		goto ret;
	}
#endif
	
#if HAVE_IBASE
	if(db->DBDriver==DPS_DB_IBASE){
		DpsSQLClose(db);
		goto ret;
	}
#endif

#if HAVE_ORACLE8
	if(db->DBDriver==DPS_DB_ORACLE8){
		DpsSQLClose(db);
		goto ret;
	}
#endif
	
#if HAVE_ORACLE7
	if(db->DBDriver==DPS_DB_ORACLE7){
		DpsSQLClose(db);
		goto ret;
	}
#endif
	
#if HAVE_CTLIB
	if(db->DBDriver==DPS_DB_MSSQL){
		DpsSQLClose(db);
		goto ret;
	}
#endif
	
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_SAPDB || HAVE_DB2)
	DpsSQLClose(db);
	goto ret;
#endif

#endif

ret:
	DpsVarListFree(&db->Vars);
	if(db->freeme)DPS_FREE(vdb);
	return;
}



__C_LINK int __DPSCALL DpsLimit8(DPS_AGENT *A, DPS_UINT8URLIDLIST *L,const char *field,int type, void *vdb){
	DPS_DB	*db=vdb;
	int	rc=DPS_OK;

	TRACE_IN(A, "DpsLimit8");

#ifdef HAVE_SQL
	if (strcasecmp(field, "category"))
	  rc = DpsLimit8SQL(A, L, field, type, db);
	else 
	  rc = DpsLimitCategorySQL(A, L, field, type, db);
#endif
	dps_strcpy(A->Conf->errstr, db->errstr);
	TRACE_OUT(A);
	return rc;
}

__C_LINK int __DPSCALL DpsLimit4(DPS_AGENT *A, DPS_UINT4URLIDLIST *L,const char *field, int type, void *vdb){
	DPS_DB	*db=vdb;
	int	rc=DPS_OK;
	
	TRACE_IN(A, "DpsLimit4");

#ifdef HAVE_SQL
	if (!strcasecmp(field, "link")) rc = DpsLimitLinkSQL(A, L, field, type, db);
	else if (!strcasecmp(field, "tag")) rc = DpsLimitTagSQL(A, L, db);
	else rc = DpsLimit4SQL(A, L, field, type, db);
#endif
	dps_strcpy(A->Conf->errstr, db->errstr);
	TRACE_OUT(A);
	return rc;
}



__C_LINK int __DPSCALL DpsClearDatabase(DPS_AGENT *A) {
	int	res=DPS_ERROR;
	DPS_DB	*db;
	size_t i, dbto;

	TRACE_IN(A, "DpsClearDatabase");

	dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	for (i = 0; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
#ifdef HAVE_SQL
	  res = DpsClearDBSQL(A, db);
	  DPS_FREE(db->where);          /* clear db->where for next parameters */
#endif
	  if (res != DPS_OK) break;
	}
	if(res!=DPS_OK){
		dps_strcpy(A->Conf->errstr,db->errstr);
	}
	TRACE_OUT(A);
	return res;
}


int DpsExecActions(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, char action) {
  DPS_MATCH       *Alias;
  DPS_MATCH_PART  Parts[10];
  size_t nparts = 10;
  DPS_VAR *Sec, *dSec;
  char *buf;
  DPS_TEXTITEM *Item;
  size_t i, z, buf_len;
  int notdone;

#ifdef HAVE_SQL

  if (Indexer->Conf->ActionSQLMatch.nmatches == 0) return DPS_OK;

  buf = (char*)DpsMalloc(buf_len = (Doc->Buf.size + 1024));
  if (buf == NULL) return DPS_OK;

  {
    char qbuf[16384];
    DPS_TEMPLATE t;
    DPS_DBLIST      dbl;
    DPS_DB *db;
    bzero(&t, sizeof(t));
    t.HlBeg = t.HlEnd = t.GrBeg = t.GrEnd = t.ExcerptMark = NULL;
    t.Env_Vars = &Doc->Sections;

    for (i = 0; i < Indexer->Conf->ActionSQLMatch.nmatches; i++) {
      DPS_TEXTLIST	*tlist = &Doc->TextList;
      Alias = &Indexer->Conf->ActionSQLMatch.Match[i];

      if (Alias->subsection[0] != action) continue; 

      dSec = DpsVarListFind(&Doc->Sections, Alias->section);
      Sec = DpsVarListFind(&Indexer->Conf->Sections, Alias->section);
      if (Sec == NULL && dSec == NULL) continue;

      if (Alias->dbaddr != NULL) {
	DpsDBListInit(&dbl);
	DpsDBListAdd(&dbl, Alias->dbaddr, DPS_OPEN_MODE_READ);
	db = &dbl.db[0];
      } else {
	db = (Indexer->flags & DPS_FLAG_UNOCON) ? &Indexer->Conf->dbl.db[0] :  &Indexer->dbl.db[0];
      }

      notdone = 1;

      if (Sec != NULL) {
	for(z = 0; z < tlist->nitems; z++) {
	  Item = &tlist->Items[z];
	  if (Item->section != Sec->section) continue;
	  if (strcasecmp(Item->section_name, Alias->section)) continue;
	  notdone = 0;
	  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	  if (DpsMatchExec(Alias, Item->str, Item->str, NULL, nparts, Parts)) {
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	    continue;
	  }
	  DpsMatchApply(buf, buf_len - 1, Item->str, Alias->arg, Alias, nparts, Parts);

	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  qbuf[0] = '\0';
	  DpsPrintTextTemplate(Indexer, NULL, NULL, qbuf, sizeof(qbuf), &t, buf /*cbuf*/);
	  DpsLog(Indexer, DPS_LOG_DEBUG, "ActionSQL.%c: %s", action, qbuf);
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	  if (DPS_OK != DpsSQLAsyncQuery(db, NULL, qbuf)) DpsLog(Indexer, DPS_ERROR, "ActionSQL error");
	  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	}
      }
      if (notdone && dSec != NULL && dSec->val != NULL) {
	DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	if (DpsMatchExec(Alias, dSec->val, dSec->val, NULL, nparts, Parts)) {
	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  continue;
	}
	DpsMatchApply(buf, buf_len - 1, dSec->val, Alias->arg, Alias, nparts, Parts);

	DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	qbuf[0] = '\0';
	DpsPrintTextTemplate(Indexer, NULL, NULL, qbuf, sizeof(qbuf), &t, buf /*cbuf*/);
	DpsLog(Indexer, DPS_LOG_DEBUG, "ActionSQL.%c: %s", action, qbuf);
	if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	if (DPS_OK != DpsSQLAsyncQuery(db, NULL, qbuf)) DpsLog(Indexer, DPS_ERROR, "ActionSQL error");
	if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
      }

      if (Alias->dbaddr != NULL) DpsDBListFree(&dbl);
    }  
    DpsTemplateFree(&t);
  }

  DPS_FREE(buf);
#endif
  return DPS_OK;
}


static int DocUpdate(DPS_AGENT * Indexer, DPS_DOCUMENT *Doc) {
  int		result = DPS_OK, rc = DPS_OK;
	const char	*c;
	int		status=DpsVarListFindInt(&Doc->Sections,"Status",0);
	int		prev_status = DpsVarListFindInt(&Doc->Sections, "PrevStatus", 0);
	int		hops=DpsVarListFindInt(&Doc->Sections,"Hops",0);
	urlid_t		origin_id = 0;
	urlid_t		url_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	time_t		next_index_time;
	char		dbuf[64];
	int             use_crosswords, use_newsext, collect_links;

	TRACE_IN(Indexer, "DocUpdate");

	use_crosswords = Indexer->Flags.use_crosswords;
	use_newsext    = Indexer->Flags.use_newsext;
	collect_links  = Indexer->Flags.collect_links;
	
	/* First of all check that URL must be delated */
	
	if(Doc->method==DPS_METHOD_DISALLOW){
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_DELETE);
		TRACE_OUT(Indexer);
		return result;
	}

	if (hops >= DPS_DEFAULT_MAX_HOPS) hops = DPS_DEFAULT_MAX_HOPS - 1;
	if (Doc->Spider.ExpireAt.eight != 0) {
	  struct tm nowtime;
	  int n;
	  time_t t = Indexer->now;
#ifdef HAVE_GMTIME_R
	  gmtime_r(&t, &nowtime);
#else
	  nowtime = *gmtime(&t);
#endif
	  nowtime.tm_sec = 0;
	  if (Doc->Spider.ExpireAt.cron.min != 0) { 
	    n = Doc->Spider.ExpireAt.cron.min - 1; 
	    if (n < nowtime.tm_min) nowtime.tm_hour++;
	    nowtime.tm_min = n;
	  }
	  if (Doc->Spider.ExpireAt.cron.hour != 0) { 
	    n = Doc->Spider.ExpireAt.cron.hour - 1; 
	    if (n < nowtime.tm_hour) nowtime.tm_mday++;
	    nowtime.tm_hour = n;
	  }
	  if (Doc->Spider.ExpireAt.cron.day != 0) { 
	    n = Doc->Spider.ExpireAt.cron.day - 1; 
	    if (n < nowtime.tm_mday) nowtime.tm_mon++;
	    nowtime.tm_mday = n;
	  }
	  if (Doc->Spider.ExpireAt.cron.month != 0) { 
	    n = Doc->Spider.ExpireAt.cron.month - 1; 
	    if (n < nowtime.tm_mon) nowtime.tm_year++;
	    nowtime.tm_mon = n;
	  }
	  if (Doc->Spider.ExpireAt.cron.wday != 0) { 
	    n = Doc->Spider.ExpireAt.cron.wday - 1; 
	    if (n < nowtime.tm_wday) nowtime.tm_mday += 7 - (nowtime.tm_wday - n);
	  }
	  next_index_time = mktime(&nowtime);
	} else {
	  if (Doc->Spider.Server->crawl_delay > 0) Indexer->now += (Doc->Spider.Server->crawl_delay / 1000) + 1;
	  next_index_time = Indexer->now + Doc->Spider.Server->period[hops];
	}

	dps_snprintf(dbuf, sizeof(dbuf), "%lu", (next_index_time & 0x80000000) ? 0x7fffffff : next_index_time);
	DpsVarListReplaceStr(&Doc->Sections,"Next-Index-Time",dbuf);
	
        DpsLog(Indexer, DPS_LOG_DEBUG, "Next-Index-Time: %s", dbuf);

	switch(status){
	
	case 0: /* No HTTP code */
	  if (Doc->method != DPS_METHOD_VISITLATER && Doc->method == DPS_METHOD_CRAWLDELAY) {
/*		if (Doc->connp.Host != NULL) Doc->connp.Host->net_errors++;*/
		DpsLog(Indexer, DPS_LOG_ERROR, "No HTTP response status");
		next_index_time = Indexer->now + Doc->Spider.net_error_delay_time;
	  }
	  if (Doc->method != DPS_METHOD_CRAWLDELAY) {
		dps_snprintf(dbuf, sizeof(dbuf), "%lu", (next_index_time & 0x80000000) ? 0x7fffffff : next_index_time);
		DpsVarListReplaceStr(&Doc->Sections,"Next-Index-Time",dbuf);
		DpsLog(Indexer, DPS_LOG_DEBUG, "Next-Index-Time: %s", dbuf);
	  }
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_SUPDATE);
		TRACE_OUT(Indexer);
		return result;
	
	case DPS_HTTP_STATUS_OK:				/*  200 */
	case DPS_HTTP_STATUS_PARTIAL_OK:			/*  206 */
	case DPS_HTTP_STATUS_MOVED_TEMPORARILY:			/*  302 */
	case DPS_HTTP_STATUS_SEE_OTHER:				/*  303 */
	case DPS_HTTP_STATUS_TEMPORARY_REDIRECT:                /*  307 */
		if(!DpsVarListFind(&Doc->Sections,"Content-Type")){
		        if (Doc->method != DPS_METHOD_VISITLATER && Doc->method == DPS_METHOD_CRAWLDELAY) {
			  if (Doc->connp.Host != NULL) Doc->connp.Host->net_errors++;
			  DpsLog(Indexer,DPS_LOG_ERROR,"No Content-type header");
			  DpsVarListReplaceInt(&Doc->Sections,"Status", status = DPS_HTTP_STATUS_INTERNAL_SERVER_ERROR);
			}
			if (Doc->method != DPS_METHOD_CRAWLDELAY) {
			  next_index_time = Indexer->now + Doc->Spider.net_error_delay_time;
			  dps_snprintf(dbuf, sizeof(dbuf), "%lu", (next_index_time & 0x80000000) ? 0x7fffffff : next_index_time);
			  DpsVarListReplaceStr(&Doc->Sections,"Next-Index-Time",dbuf);
			  DpsLog(Indexer, DPS_LOG_DEBUG, "Next-Index-Time: %s", dbuf);
			}
			result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_SUPDATE);
			TRACE_OUT(Indexer);
			return result;
		} else {
				if (Doc->connp.Host != NULL) Doc->connp.Host->net_errors = 0;
		}
		break;
	
	case DPS_HTTP_STATUS_MULTIPLE_CHOICES:			/*  300 */
	case DPS_HTTP_STATUS_MOVED_PARMANENTLY:			/*  301 */
	case DPS_HTTP_STATUS_NOT_MODIFIED:			/*  304 */
	case DPS_HTTP_STATUS_OK_CLONES:                         /* 2200 */
	case DPS_HTTP_STATUS_PARTIAL_OK_CLONES:                 /* 2206 */
	case DPS_HTTP_STATUS_NOT_MODIFIED_CLONES:               /* 2304 */
		/* FIXME: check that status is changed and remove words if necessary */
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_SUPDATE);
		TRACE_OUT(Indexer);
		return result;
		break;
	
	case DPS_HTTP_STATUS_USE_PROXY:				/* 305 */
	case DPS_HTTP_STATUS_BAD_REQUEST:			/* 400 */
	case DPS_HTTP_STATUS_UNAUTHORIZED:			/* 401 */
	case DPS_HTTP_STATUS_PAYMENT_REQUIRED:			/* 402 */
	case DPS_HTTP_STATUS_FORBIDDEN:				/* 403 */
	case DPS_HTTP_STATUS_NOT_FOUND:				/* 404 */
	case DPS_HTTP_STATUS_METHOD_NOT_ALLOWED:		/* 405 */
	case DPS_HTTP_STATUS_NOT_ACCEPTABLE:			/* 406 */
	case DPS_HTTP_STATUS_PROXY_AUTHORIZATION_REQUIRED:	/* 407 */
	case DPS_HTTP_STATUS_REQUEST_TIMEOUT:			/* 408 */
	case DPS_HTTP_STATUS_CONFLICT:				/* 409 */
	case DPS_HTTP_STATUS_GONE:				/* 410 */
	case DPS_HTTP_STATUS_LENGTH_REQUIRED:			/* 411 */
	case DPS_HTTP_STATUS_PRECONDITION_FAILED:		/* 412 */
	case DPS_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE:		/* 413 */
	case DPS_HTTP_STATUS_REQUEST_URI_TOO_LONG:		/* 414 */	
	case DPS_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:		/* 415 */
	case DPS_HTTP_STATUS_NOT_IMPLEMENTED:			/* 501 */
	case DPS_HTTP_STATUS_BAD_GATEWAY:			/* 502 */
	case DPS_HTTP_STATUS_NOT_SUPPORTED:			/* 505 */
	
		/*
		  FIXME: remove words from database
		  Check last reffering time to remove when
		  there are no links to this document anymore
		  DpsLog(Indexer,DPS_LOG_EXTRA,"Deleting URL");
		*/
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_SUPDATE);
		TRACE_OUT(Indexer);
		return result;
	
	case DPS_HTTP_STATUS_INTERNAL_SERVER_ERROR:		/* 500 */
	case DPS_HTTP_STATUS_SERVICE_UNAVAILABLE:		/* 503 */
	case DPS_HTTP_STATUS_GATEWAY_TIMEOUT:			/* 504 */
	
		/* Keep words in database                */
		/* We'll retry later, maybe host is down */
		if (Doc->connp.Host != NULL) Doc->connp.Host->net_errors++;
		next_index_time = Indexer->now + Doc->Spider.net_error_delay_time;
		dps_snprintf(dbuf, sizeof(dbuf), "%lu", (next_index_time & 0x80000000) ? 0x7fffffff : next_index_time);
		DpsVarListReplaceStr(&Doc->Sections,"Next-Index-Time",dbuf);
		DpsLog(Indexer, DPS_LOG_DEBUG, "Next-Index-Time: %s", dbuf);
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_SUPDATE);
		TRACE_OUT(Indexer);
		return result;
	
	default: /* Unknown status, retry later */
		DpsLog(Indexer,DPS_LOG_WARN,"HTTP %d We don't yet know how to handle it, skipped",status);
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_SUPDATE);
		TRACE_OUT(Indexer);
		return result;
	}
	
	
	if(Doc->method==DPS_METHOD_GET && Doc->Spider.use_clones){
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_FINDORIG);
		if(result!=DPS_OK) {
		  TRACE_OUT(Indexer);
		  return result;
		}
		origin_id = (urlid_t)DpsVarListFindInt(&Doc->Sections,"Origin-ID",0);
	}
	
	
	/* Check clones */
	if((origin_id)&&(origin_id!=url_id)){
	        if (DpsNeedLog(DPS_LOG_EXTRA))
		  DpsLog(Indexer, DPS_LOG_EXTRA, "Duplicate Document %s, #%d with #%d", 
			 DpsVarListFindStr(&Doc->Sections, "URL", ""), url_id, origin_id);
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_DELWORDS);
		if(use_crosswords){
			if(result == DPS_OK) result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_DELCWORDS);
		}
		if(result == DPS_OK) {
		  if (status == 200 || status == 206 || status == 304) {
		    status += 2000;
		    DpsVarListReplaceInt(&Doc->Sections, "Status", status);
		  }
		  result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_UPDCLONE);
		}
		TRACE_OUT(Indexer);
		return result;
	}
	
	/* Check that document wasn't modified since last indexing */
	if( (DpsVarListFindInt(&Doc->Sections,"crc32", 0) != 0) 
	    &&  (DpsVarListFindInt(&Doc->Sections,"crc32old",0)==DpsVarListFindInt(&Doc->Sections,"crc32",0)) 
	    &&  (!(Indexer->flags&DPS_FLAG_REINDEX))) {
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_SUPDATE);
		TRACE_OUT(Indexer);
		return result;
	}
	
	/* Compose site_id string and calc it's Hash32 */
/*	sprintf(site_id_str,"%s://%s/",Doc->CurURL.schema,Doc->CurURL.hostinfo);
	DpsVarListReplaceInt(&Doc->Sections, "Site-ID", DpsStrHash32(site_id_str)); for what this need ? */
	
	/* Copy default languages, if not given by server and not guessed */
	if(!(c=DpsVarListFindStr(&Doc->Sections,"Content-Language",NULL))){
		if((c=DpsVarListFindStr(&Doc->Sections,"DefaultLang",NULL)))
			DpsVarListReplaceStr(&Doc->Sections,"Content-Language",c);
	}
	
	/* For NEWS extension: get rec_id from my */
	/* parent out of db (if I have one...)    */
	if(use_newsext){
		DPS_VAR		*Sec;
		const char	*parent=NULL;
		int		parent_id=0;
		
		if((Sec=DpsVarListFind(&Doc->Sections,"Header.References")) && Sec->val){
			/* References contains all message IDs of my */
			/* predecessors, space separated             */
			/* my direct parent is the last in the list  */
			if((parent = strrchr(Sec->val,' '))){
				/* parent now points to the space */
				/* character skip it              */
				++parent;
			}else{
				/* there is only one entry in */
				/* references, so this is my parent */
				parent=Sec->val;
			}	
		}
		
		/* get parent from database */
		if(parent && *parent != '\0' && strchr(parent,'@')){
			DPS_DOCUMENT Msg;
			
			DpsDocInit(&Msg);
			DpsVarListReplaceStr(&Msg.Sections,"Header.Message-ID",parent);
			result = DpsURLAction(Indexer, &Msg, DPS_URL_ACTION_FINDBYMSG);
			parent_id = DpsVarListFindInt(&Msg.Sections,"DP_ID",0);
			DpsVarListReplaceInt(&Doc->Sections,"Header.Parent-ID",parent_id);
			DpsDocFree(&Msg);
		}
		
		/* Now register me with my parent  */
		if (collect_links && parent_id) result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_REGCHILD);
		
		if(result!=DPS_OK) {
		  TRACE_OUT(Indexer);
		  return result;
		}
	}
	
	if (prev_status != 0) {
		DpsExecActions(Indexer, Doc, 'u');
	} else {
		DpsExecActions(Indexer, Doc, 'i');
	}
/*	rc = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_LUPDATE);*/

	/* Now store words and crosswords */
	switch(Doc->method) {
	case DPS_METHOD_UNKNOWN:
	case DPS_METHOD_GET:
	case DPS_METHOD_INDEX:
	case DPS_METHOD_HEAD:
	case DPS_METHOD_CHECKMP3:
	case DPS_METHOD_CHECKMP3ONLY:
	  if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_INSWORDS))) {
	    TRACE_OUT(Indexer);
	    return result;
	  }
	  if(use_crosswords)
	    if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_INSCWORDS))) {
	      TRACE_OUT(Indexer);
	      return result;
	    }
	  break;
	case DPS_METHOD_HREFONLY:
	  if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_DELWORDS))) {
	    TRACE_OUT(Indexer);
	    return result;
	  }
	  if(use_crosswords)
	    if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_DELCWORDS))) {
	      TRACE_OUT(Indexer);
	      return result;
	    }
	default:
	  break;
	}

	rc = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_LUPDATE);
	
	TRACE_OUT(Indexer);
	return rc;
}


static int DpsDocUpdate(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc){
	size_t		maxsize;
	size_t		sec, r;
	int		flush=0;
	int		rc=DPS_OK;
	DPS_RESULT	*I = &Indexer->Indexed;

	TRACE_IN(Indexer, "DpsDocUpdate");

	maxsize = DpsVarListFindInt(&Indexer->Vars, "DocMemCacheSize", 0) * 1024 * 1024;

	if (maxsize > 0 && I->memused > 0) DpsLog(Indexer, DPS_LOG_EXTRA, "DocCacheSize: %d/%d", I->memused, maxsize);
	if (Doc) {
		I->memused += sizeof(DPS_DOCUMENT);
		/* Aproximation for Words memory usage  */
		I->memused += Doc->Words.nwords * (sizeof(DPS_WORD) + 5);
		/* Aproximation for CrossWords memory usage */
		I->memused += Doc->CrossWords.ncrosswords * (sizeof(DPS_CROSSWORD) + 35);
		/* Aproximation for Sections memory usage */
		for (r = 0; r < 256; r++)
		for(sec = 0; sec < Doc->Sections.Root[r].nvars; sec++) {
			I->memused += sizeof(DPS_VAR);
			I->memused += Doc->Sections.Root[r].Var[sec].curlen * 3 + 10;
		}
		I->memused += (sizeof(DPS_HREF) + 35) * Doc->Hrefs.nhrefs;
		if (I->memused >= maxsize) flush = 1;
		if (I->num_rows >= 1024) flush = 1;
	} else flush = 1;

	if (flush) {
		size_t	docnum;

		if (I->num_rows)
			DpsLog(Indexer, DPS_LOG_EXTRA, "Flush %d document(s)", I->num_rows + ((Doc != NULL) ? 1 : 0));
		
		if (Doc) {
			DPS_THREADINFO(Indexer, "Updating", DpsVarListFindStr(&Doc->Sections, "URL", ""));
			if(DPS_OK != (rc = DocUpdate(Indexer, Doc))) {
			  TRACE_OUT(Indexer);
			  return rc;
			}
			DpsDocFree(Doc);
		}
		
		for (docnum = 0; docnum < I->num_rows; docnum++) {
			/* Flush all hrefs from cache in-memory    */
			/* cache into database. Note, this must    */
			/* be done before call of  StoreCrossWords */
			/* because we need to know all new URL IDs */
			
			DPS_THREADINFO(Indexer, "Updating", DpsVarListFindStr(&I->Doc[docnum].Sections, "URL", ""));
			if(DPS_OK != (rc = DocUpdate(Indexer, &I->Doc[docnum]))) {
			        TRACE_OUT(Indexer);
				return rc;
			}
		}
		if (Indexer->Indexed.num_rows) DpsResultFree(&Indexer->Indexed);
	} else {
		/* Add document into cache */
		I->Doc=(DPS_DOCUMENT*)DpsRealloc(I->Doc, (I->num_rows + 1) * sizeof(DPS_DOCUMENT));
		if (I->Doc == NULL) {
		  rc = DPS_ERROR;
		  I->num_rows = 0;
		} else {
		  I->Doc[I->num_rows] = Doc[0];
		  I->Doc[I->num_rows].freeme = 0;
		  if (Doc->freeme) DPS_FREE(Doc);
		  I->num_rows++;
		}
	}
	TRACE_OUT(Indexer);
	return rc;
}

#if defined(WITH_TRACE) && defined(DEBUG)

static const char *DpsURLActionStr(int cmd) {
  switch(cmd) {
  case DPS_URL_ACTION_DELETE:  return "URL Action Delete";
  case DPS_URL_ACTION_ADD:     return "URL Action Add";
  case DPS_URL_ACTION_SUPDATE: return "URL Action Supdate";
  case DPS_URL_ACTION_LUPDATE: return "URL Action Lupdate";
  case DPS_URL_ACTION_INSWORDS:return "URL Action InsWords";
  case DPS_URL_ACTION_INSCWORDS:return "URL Action InsCWords";
  case DPS_URL_ACTION_DELWORDS:return "URLAction DelWords";
  case DPS_URL_ACTION_DELCWORDS:return "URL Action DelCWords";
  case DPS_URL_ACTION_UPDCLONE:return "URL Action UpdClone";
  case DPS_URL_ACTION_REGCHILD:return "URL Action RegChild";
  case DPS_URL_ACTION_FINDBYURL:return "URL Action FindByURL";
  case DPS_URL_ACTION_FINDBYMSG:return "URL Action FindByMsg";
  case DPS_URL_ACTION_FINDORIG: return "URL Action FindOrigin";
  case DPS_URL_ACTION_EXPIRE:   return "URL Action Expire";
  case DPS_URL_ACTION_REFERERS: return "URL Action Referers";
  case DPS_URL_ACTION_SERVERTABLE:return "URL Action ServerTable";
  case DPS_URL_ACTION_DOCCOUNT: return "URL Action DocCount";
  case DPS_URL_ACTION_FLUSH:    return "URL Action Flush";
  case DPS_URL_ACTION_WRITEDATA:return "URL Action WriteDate";
  case DPS_URL_ACTION_LINKS_DELETE:return "URL Action Links Delete";
  case DPS_URL_ACTION_ADD_LINK: return "URL Action Add Link";
/*  case DPS_URL_ACTION_FLUSH:    return "URL Action flush";*/
  case DPS_URL_ACTION_FLUSHCACHED:    return "URL Action cached flush";
  case DPS_URL_ACTION_LINKS_MARKTODEL: return "URL Action Links mark to del";
  case DPS_URL_ACTION_REFRESHDOCINFO: return "URL Action Refresh DocInfo";
  case DPS_URL_ACTION_PASNEO: return "URL Action Pas Neo";
  case DPS_URL_ACTION_REFERER: return "URL Action Referer";
  case DPS_URL_ACTION_RESORT: return "URL Action Resort Cached";
  case DPS_URL_ACTION_REHASHSTORED: return "URL Action Rehash Stored DB";
  case DPS_URL_ACTION_POSTPONE_ON_ERR: return "URL Action Postpone On Errors";

  }
  return "<unknown URLAction>";
}
#endif


__C_LINK int __DPSCALL DpsURLAction(DPS_AGENT *A, DPS_DOCUMENT *D, int cmd) {
	DPS_DB	*db;
	int res=DPS_ERROR, execflag = 0;
	size_t i, dbfrom = 0, dbto;

	TRACE_IN(A, "DpsURLAction");

	if(cmd == DPS_URL_ACTION_FLUSH) {
	  res = DpsDocUpdate(A, D);
	  TRACE_OUT(A);
	  return res;
	}

	if (cmd == DPS_URL_ACTION_DELETE) {
	    DpsLog(A, DPS_LOG_WARN, "Deleting %s", DpsVarListFindStr(&D->Sections, "URL", ""));
	    DpsExecActions(A, D, 'd');
	}

	if (A->flags & DPS_FLAG_UNOCON) {
	  DPS_GETLOCK(A, DPS_LOCK_CONF);
	  if (A->Conf->dbl.cnt_db) {
	    dbfrom = A->Conf->dbl.dbfrom; dbto = A->Conf->dbl.dbto;
	  } else {
	    dbto =  A->Conf->dbl.nitems;
	    if (D != NULL) {
	      /* we don't need to use DpsURL_ID() here, since the sparse between databases is base on URL only */
	      if (D->id == 0) D->id = DpsStrHash32(DpsVarListFindStr(&D->Sections, "URL", ""));
	      dbfrom = dbto = D->id % A->Conf->dbl.nitems;
	      dbto++;
	    }
	  }
	  DPS_RELEASELOCK(A, DPS_LOCK_CONF);
	} else {
	  if (A->dbl.cnt_db) {
	    dbfrom = A->dbl.dbfrom; dbto = A->dbl.dbto;
	  } else {
	    dbto =  A->dbl.nitems;
	    if (D != NULL) {
	      /* we don't need to use DpsURL_ID() here, since the sparse between databases is base on URL only */
	      if (D->id == 0) D->id = DpsStrHash32(DpsVarListFindStr(&D->Sections, "URL", ""));
	      dbfrom = dbto = D->id % A->dbl.nitems;
	      dbto++;
	    }
	  }
	}

#if defined(WITH_TRACE) && defined(DEBUG)
	{
	  register unsigned long ticks = DpsStartTimer();
	  fprintf(A->TR, "%lu [%d] URLAction: (%d) %s   dbfrom: %d, dbto: %d  n.dbl: %d\n", 
		  ticks, A->handle, cmd, DpsURLActionStr(cmd), dbfrom, dbto, A->Conf->dbl.nitems);
	  fflush(A->TR);
	}
#endif
	
	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];

/*#ifdef WITH_TRACE
		  fprintf(A->TR, "[%d] URLAction: db->DBDriver %d\n", A->handle, db->DBDriver);
#endif*/
	  switch(db->DBDriver) {
	  case DPS_DB_SEARCHD:
#if defined(WITH_TRACE) && defined(DEBUG)
		  fprintf(A->TR, "[%d] URLAction Searchd: %d (%s)\n", A->handle, cmd, DpsURLActionStr(cmd));
		  fflush(A->TR);
#endif
	    res = DpsSearchdURLAction(A, D, cmd, db);
	    execflag = 1;
	    break;
	  case DPS_DB_CACHE:
#if defined(WITH_TRACE) && defined(DEBUG)
		  fprintf(A->TR, "[%d] URLAction DBCache: %d (%s)\n", A->handle, cmd, DpsURLActionStr(cmd));
		  fflush(A->TR);
#endif
	    res = DpsURLActionCache(A, D, cmd, db);
	    execflag = 1;
	    break;
#ifdef HAVE_SQL
	  default:
	        if (db->DBMode == DPS_DBMODE_CACHE) {
#if defined(WITH_TRACE) && defined(DEBUG)
		  {
		    register unsigned long ticks = DpsStartTimer();
		    fprintf(A->TR, "%lu [%d] URLActionCache: (%d) %s\n", ticks, A->handle, cmd, DpsURLActionStr(cmd));
		    fflush(A->TR);
		  }
#endif
		  res = DpsURLActionCache(A, D, cmd, db);
/*		  if (res != DPS_OK) break;*/
		  if ((cmd == DPS_URL_ACTION_INSWORDS || cmd == DPS_URL_ACTION_DELWORDS) && !A->Flags.fill_dictionary) break;
		}
		if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
		  
#if defined(WITH_TRACE) && defined(DEBUG)
		{
		  register unsigned long ticks = DpsStartTimer();
		  fprintf(A->TR, "%lu [%d] URLActionSQL: %d (%s)\n", ticks, A->handle, cmd, DpsURLActionStr(cmd));
		  fflush(A->TR);
		}
#endif
		res = DpsURLActionSQL(A,D,cmd,db);
		if (cmd == DPS_URL_ACTION_EXPIRE) DPS_FREE(db->where);  /* clear db->where for next parameters */
		execflag = 1;
		if (res != DPS_OK && execflag) {
		  DpsLog (A, DPS_LOG_ERROR, db->errstr);
		}
		if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
		break;
#endif
	  }
	  if (res != DPS_OK) break;
	}
	if ((res != DPS_OK) && !execflag) {
	  DpsLog(A, DPS_LOG_ERROR, "no supported DBAddr specified");
	}
	TRACE_OUT(A);
	return res;
}


__C_LINK int __DPSCALL DpsTargets(DPS_AGENT *A) {
	int	res=DPS_ERROR;
	DPS_DB	*db;
	size_t i, dbfrom = 0, dbto;

	TRACE_IN(A, "DpsTargets");

/*	DPS_GETLOCK(A, DPS_LOCK_CONF);*/
	dbto = (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems ;
	DpsResultFree(&A->Conf->Targets);
/*	DPS_RELEASELOCK(A, DPS_LOCK_CONF);*/

	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
#ifdef HAVE_SQL
	  res = DpsTargetsSQL(A, db);
#endif
	  if(res != DPS_OK){
		DpsLog(A, DPS_LOG_ERROR, db->errstr);
	  }
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	  if (res != DPS_OK) break;
	}
	TRACE_OUT(A);
	return res;
}

__C_LINK int __DPSCALL DpsResAction(DPS_AGENT *A, DPS_RESULT *R, int cmd){
	int	res=DPS_ERROR;
	DPS_DB	*db;
	size_t i, dbfrom = 0, dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	const char      *label = DpsVarListFindStr(&A->Vars, "label", NULL);

	TRACE_IN(A, "DpsResAction");

	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  if (label != NULL && db->label == NULL) continue;
	  if (label == NULL && db->label != NULL) continue;
	  if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);

	  if (db->DBMode == DPS_DBMODE_CACHE)
	    res = DpsResActionCache(A, R, cmd, db, i);
#ifdef HAVE_SQL
	  if ((db->DBType != DPS_DB_CACHE) && A->Flags.URLInfoSQL)
	    res = DpsResActionSQL(A, R, cmd, db, i);
#endif
	  if(res != DPS_OK){
		DpsLog(A, DPS_LOG_ERROR, db->errstr);
	  }
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	  if (res != DPS_OK) break;
	}
	TRACE_OUT(A);
	return res;
}


__C_LINK int __DPSCALL DpsCatAction(DPS_AGENT *A, DPS_CATEGORY *C, int cmd){
	DPS_DB	*db;
	int	res=DPS_ERROR;
	size_t i, dbfrom = 0, dbto;

	TRACE_IN(A, "DpsCatAction");

	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_CONF);
	dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_CONF);

	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	  switch(db->DBDriver) {
	  case DPS_DB_SEARCHD:
	        res = DpsSearchdCatAction(A, C, cmd, db);
		break;
#ifdef HAVE_SQL
	  default:
	    if (db->DBType != DPS_DB_CACHE)
	      res = DpsCatActionSQL(A, C, cmd, db);
#endif
	  }
	  if(res != DPS_OK){
		DpsLog(A, DPS_LOG_ERROR, db->errstr);
	  }
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	  if (res != DPS_OK) break;
	}
	TRACE_OUT(A);
	return res;
}

__C_LINK int __DPSCALL DpsSrvAction(DPS_AGENT *A, DPS_SERVER *S, int cmd) {
	DPS_DB	*db;
	int	res = DPS_OK;
	size_t i, dbfrom = 0, dbto;

	TRACE_IN(A, "DpsSrvAction");

	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_CONF);
	dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_CONF);

	dps_strcpy(A->Conf->errstr, "An error in DpsSRVAction (does appropriate storage support compiled in?)");
	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];

	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB); 
#ifdef HAVE_SQL
	  if (db->DBType != DPS_DB_CACHE)
	    res = DpsSrvActionSQL(A, S, cmd, db);
#endif
	  if(res != DPS_OK){
		DpsLog(A, DPS_LOG_ERROR, db->errstr);
	  }
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	  if (res != DPS_OK) break;
	}
	TRACE_OUT(A);
	return res;
}

int DpsFindWords(DPS_AGENT *A, DPS_RESULT *Res) {
	const char      *cache_mode  = DpsVarListFindStr(&A->Vars, "Cache", "no");
	const char      *label = DpsVarListFindStr(&A->Vars, "label", NULL);
	DPS_DB		*db;
	size_t          i, nitems =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	size_t          dbfrom = nitems, dbto = 0;
	int             res = DPS_OK;
	size_t	        nwrd = 0;
	size_t	        nwrdX[256];
#ifdef NEWMERGE
	size_t          iwrdX[256], nskipped;
	urlid_t cur_url_id;
#endif
	size_t          *PerSite[256], *persite, *curpersite;
	size_t          ResultsLimit = DpsVarListFindUnsigned(&A->Vars, "ResultsLimit", 0);
	size_t          total_found = 0, grand_total = 0, cnt_db = 0;
	DPS_URL_CRD_DB	*wrdX[256];
	DPS_URL_CRD_DB	*wrd = NULL, *curwrd = NULL;
	DPS_URLDATA     *udtX[256];
	DPS_URLDATA     *udt = NULL, *curudt = NULL;
#ifdef WITH_REL_TRACK
	DPS_URLTRACK    *trkX[256];
	DPS_URLTRACK    *trk = NULL, *curtrk = NULL;
#endif

	TRACE_IN(A, "DpsFindWords");

	if( strcasecmp(cache_mode, "yes") || DpsSearchCacheFind(A, Res) ){
		/* If not found in search cache       */
		/* Let's do actual search now:        */
		/* Get groupped by url_id words array */
	
	  for (i = 0; i < nitems; i++) {
	    db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	    if (label != NULL && db->label == NULL) continue;
	    if (label == NULL && db->label != NULL) continue;
	    if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
	    if (i < dbfrom) dbfrom = i;
	    if (i > dbto) dbto = i;
	    DpsLog(A, DPS_LOG_DEBUG, "DpsFind for %s", db->DBADDR);
	    switch(db->DBDriver){
		case DPS_DB_SEARCHD:
		  res = DpsFindWordsSearchd(A, Res, db);
		  break;
	        default:
		  res = DPS_OK;
#ifdef HAVE_SQL
		  DPS_FREE(db->where)
#endif
		  ;
	    }
	    if (DPS_OK != res) DpsLog(A, DPS_LOG_ERROR, "FindWordsSearchd err: %s", A->Conf->errstr); 
	  }
	  dbto++;
	  if (A->flags & DPS_FLAG_UNOCON) {
	    A->Conf->dbl.dbfrom = dbfrom; A->Conf->dbl.dbto = dbto;
	    A->Conf->dbl.cnt_db = dbto - dbfrom;
	  } else {
	  }
	  for (i = dbfrom; i < dbto; i++) {
	    db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	    if (label != NULL && db->label == NULL) continue;
	    if (label == NULL && db->label != NULL) continue;
	    if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
	    DpsLog(A, DPS_LOG_DEBUG, "DpsGetWords for %s", db->DBADDR);
	    Res->CoordList.Coords = NULL;
	    Res->CoordList.Data = NULL;
	    Res->PerSite = NULL;
#ifdef WITH_REL_TRACK
	    Res->CoordList.Track = NULL;
#endif
	    Res->CoordList.ncoords = 0;
	    Res->total_found = 0;
	    switch(db->DBDriver){
		case DPS_DB_SEARCHD:
		  res = DpsSearchdGetWordResponse(A, Res, db);
		  Res->offset = (nitems == 1) ? 0 : 1;
		  break;
		default:
		  if (db->DBMode == DPS_DBMODE_CACHE) {
		    res = DpsFindWordsCache(A, Res, db);
		  } else {
#ifdef HAVE_SQL
		    res = DpsFindWordsSQL(A, Res, db);
#else
		    res = DPS_OK;
#endif
		  }
		  Res->offset = 1;
		  break;
	    }
	    if (DPS_OK != res) DpsLog(A, DPS_LOG_ERROR, "2FindWordsSearchd err: %s", A->Conf->errstr); 
	    wrdX[i] = Res->CoordList.Coords;
	    udtX[i] = Res->CoordList.Data;
#ifdef WITH_REL_TRACK
	    trkX[i] = Res->CoordList.Track;
#endif
	    nwrdX[i] = (Res->offset || !Res->num_rows) ? Res->total_found : Res->num_rows;
#ifdef NEWMERGE
	    iwrdX[i] = 0;
	    if (label != NULL && db->label == NULL) nwrdX[i] = 0;
	    else if (label == NULL && db->label != NULL) nwrdX[i] = 0;
	    else if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) nwrdX[i] = 0;
	    
#endif
/*	    nwrd += (Res->offset || !Res->num_rows) ? Res->total_found : Res->num_rows;*/
	    nwrd += nwrdX[i];

	    total_found += Res->total_found;
	    grand_total += Res->grand_total;
	    if ((PerSite[i] = Res->PerSite) == NULL) {
	      if (nwrdX[i]) {
		PerSite[i] = (size_t*)DpsXmalloc(sizeof(size_t) * nwrdX[i]);
		if (PerSite[i] == NULL) {
		  while(i > 0) {
		    DPS_FREE(PerSite[i - 1]);
		    i--;
		  }
		  TRACE_OUT(A);
		  return DPS_ERROR;
		}
	      }
	    }
	  }

	  if (nwrd > 0) {
	    curwrd=wrd=(DPS_URL_CRD_DB*)DpsMalloc(sizeof(*wrd)*nwrd);
	    curudt=udt=(DPS_URLDATA*)DpsMalloc(sizeof(*udt)*nwrd);
#ifdef WITH_REL_TRACK
	    curtrk=trk=(DPS_URLTRACK*)DpsMalloc(sizeof(*trk)*nwrd);
#endif
	    Res->PerSite = persite = curpersite = (size_t*)DpsMalloc(sizeof(size_t) * nwrd);

	    if (curwrd == NULL || curudt == NULL || Res->PerSite == NULL
#ifdef WITH_REL_TRACK
		|| curtrk == NULL
#endif
		) {
	      DPS_FREE(curwrd); DPS_FREE(curudt); DPS_FREE(Res->PerSite);
#ifdef WITH_REL_TRACK
	      DPS_FREE(curtrk);
#endif
	      for (i = dbfrom; i < dbto; i++) {
		DPS_FREE(udtX[i]); DPS_FREE(wrdX[i]);DPS_FREE(PerSite[i]);
#ifdef WITH_REL_TRACK
		DPS_FREE(trkX[i]);
#endif
	      }
	      TRACE_OUT(A);
	      return DPS_ERROR;
	    }

	    if (A->flags & DPS_FLAG_UNOCON) { A->Conf->dbl.cnt_db = 0; } else { A->dbl.cnt_db = 0; }
#ifdef NEWMERGE
	    for (i = dbfrom; i < dbto; i++) {
	      int zz;
	      for (zz = 0 ; zz < nwrdX[i]; zz++) {
		fprintf(stderr, "%d -- db:%d wrd:%d url_id:%d\n", __LINE__, i, zz, wrdX[i][zz].url_id);
	      }
	    }
	    while (1) {
	      nskipped = 0;
	      for (i = dbfrom; i < dbto; i++) {
		if (iwrdX[i] < nwrdX[i]) {
		  cur_url_id = wrdX[i][iwrdX[i]].url_id; i++; break;
		}
		nskipped++;
	      }
	      fprintf(stderr, "%d -- nskipped: %d\n", __LINE__, nskipped);
	      if (nskipped >= (dbto - dbfrom)) break;
	      for (; i < dbto; i++) {
		if (iwrdX[i] < nwrdX[i] && cur_url_id > wrdX[i][iwrdX[i]].url_id) {
		  cur_url_id = wrdX[i][iwrdX[i]].url_id;
		}
	      }
	      fprintf(stderr, "%d -- cur_url_id: %d\n", __LINE__, cur_url_id);
	      for (i = dbfrom; i < dbto; i++) {
		while ((iwrdX[i] < nwrdX[i]) && (wrdX[i][iwrdX[i]].url_id == cur_url_id)) {
		  *curwrd = wrdX[i][iwrdX[i]];
		  *curpersite = PerSite[i][iwrdX[i]];
		  if (udtX[i] != NULL) {
		    *curudt = udtX[i][iwrdX[i]];
		  } else bzero(curudt, sizeof(*curudt));
#ifdef WITH_REL_TRACK
		  if (trkX[i] != NULL) {
		    *curtrk = trkX[i][iwrdX[i]];
		  } else bzero(curtrk, sizeof(*curtrk));
#endif
		  iwrdX[i]++;
		}
	      }
	      curwrd++; curpersite++; curudt++;
	    }
	    nwrd = curwrd = wrd;
	    for (i = dbfrom; i < dbto; i++) {
	      DPS_FREE(wrdX[i]);
	      DPS_FREE(PerSite[i]);
	      DPS_FREE(udtX[i]);
#ifdef WITH_REL_TRACK
	      DPS_FREE(trkX[i]);
#endif
	    }

#else
	    for (i = dbfrom; i < dbto; i++) {
	      db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	      if (label != NULL && db->label == NULL) continue;
	      if (label == NULL && db->label != NULL) continue;
	      if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
		if(wrdX[i]){
		        if (A->flags & DPS_FLAG_UNOCON) A->Conf->dbl.cnt_db++; else A->dbl.cnt_db++;
/*			size_t j;*/
			/* Set machine number */
/*			for(j=0;j<nwrdX[i];j++){
				wrdX[i][j].coord = (wrdX[i][j].coord << 8) + (i & 255);
			}*/

			dps_memcpy(curwrd, wrdX[i], sizeof(*curwrd)*nwrdX[i]); /* was: dps_memmove */
#ifdef WITH_MULTIDBADDR
			if (db->DBDriver == DPS_DB_SEARCHD && nwrdX[i] > 0 ) {
			  register size_t length = nwrdX[i];
			  register size_t n = (nwrdX[i] + 7) / 8;
			  switch(length % 8) {
			  case 0: do { curwrd->dbnum = i; curwrd++;
			  case 7:      curwrd->dbnum = i; curwrd++;
			  case 6:      curwrd->dbnum = i; curwrd++;
			  case 5:      curwrd->dbnum = i; curwrd++;
			  case 4:      curwrd->dbnum = i; curwrd++;
			  case 3:      curwrd->dbnum = i; curwrd++;
			  case 2:      curwrd->dbnum = i; curwrd++;
			  case 1:      curwrd->dbnum = i; curwrd++;
			    } while (--n > 0);
			  }
			} else curwrd += nwrdX[i];
#else
			curwrd+=nwrdX[i];
#endif
			DPS_FREE(wrdX[i]);
			dps_memcpy(curpersite, PerSite[i], sizeof(size_t) * nwrdX[i]); /* was: dps_memmove */
			curpersite += nwrdX[i];
			DPS_FREE(PerSite[i]);
			if (udtX[i] != NULL) {
			  dps_memcpy(curudt, udtX[i], sizeof(*curudt) * nwrdX[i]); /* was: dps_memmove */
			} else {
			  bzero(curudt, sizeof(*curudt) * nwrdX[i]);
			}
			curudt += nwrdX[i];
			DPS_FREE(udtX[i]);
#ifdef WITH_REL_TRACK
			if (trkX[i] != NULL) {
			  dps_memcpy(curtrk, trkX[i], sizeof(*curtrk) * nwrdX[i]); /* was: dps_memmove */
			} else {
			  bzero(curtrk, sizeof(*curtrk) * nwrdX[i]);
			}
			curtrk += nwrdX[i];
			DPS_FREE(trkX[i]);
#endif
		}
	    }
#endif /* NEWMERGE */
/*	    if (Res->offset) DpsSortSearchWordsByWeight(wrd,nwrd);*/
	  } else for (i = dbfrom; i < dbto; i++) {
			DPS_FREE(wrdX[i]);
			DPS_FREE(PerSite[i]);
			DPS_FREE(udtX[i]);
#ifdef WITH_REL_TRACK
			DPS_FREE(trkX[i]);
#endif
	  }

          Res->total_found = total_found /*nwrd*/;
	  Res->grand_total = grand_total;
          Res->num_rows = Res->CoordList.ncoords = (size_t)nwrd;
	  Res->CoordList.Coords = wrd;
	  Res->CoordList.Data = udt;
#ifdef WITH_REL_TRACK
	  Res->CoordList.Track = trk;
#endif

	  cnt_db = (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.cnt_db : A->dbl.cnt_db;
	  if (cnt_db > 1 /*dbto - dbfrom > 1*/) {
	    int group_by_site =  DpsGroupBySiteMode(DpsVarListFindStr(&A->Vars, "GroupBySite", NULL));
	    int	use_site_id = (group_by_site && (DpsVarListFindInt(&A->Vars, "site", 0) == 0));
	    size_t n, p;
	    
	    /* Removing duplictes */
	    DpsSortSearchWordsByPattern(Res, &Res->CoordList, Res->CoordList.ncoords, "U");
	    for(n = 1, p = 0; n < Res->CoordList.ncoords; n++) {
	      if (Res->CoordList.Coords[p].url_id == Res->CoordList.Coords[n].url_id) continue;
	      p++;
	      if (p != n) {
		Res->CoordList.Coords[p] = Res->CoordList.Coords[n];
		Res->CoordList.Data[p] = Res->CoordList.Data[n];
#ifdef WITH_REL_TRACK
		Res->CoordList.Track[p] = Res->CoordList.Track[n];
#endif
	      }
	    }
	    if (p + 1 < Res->CoordList.ncoords) Res->CoordList.ncoords = Res->total_found = p + 1;
	    
	    if (use_site_id && (group_by_site == DPS_GROUP_FULL)) {
	      DpsSortSearchWordsBySite(Res, &Res->CoordList, Res->CoordList.ncoords, DpsVarListFindStr(&A->Vars, "s", "RP"));
	      DpsGroupBySite(A, Res);
	    }
	    DpsSortSearchWordsByPattern(Res, &Res->CoordList, Res->CoordList.ncoords, DpsVarListFindStr(&A->Vars, "s", "RP"));
	    if (use_site_id && (group_by_site == DPS_GROUP_YES)) {
	      DpsGroupBySite(A, Res);
	    }
	    Res->total_found = Res->CoordList.ncoords;
	  }

	  if (ResultsLimit > 0 && ResultsLimit < Res->total_found) {
	    if(Res->offset) {
	      Res->total_found = Res->CoordList.ncoords = ResultsLimit;
	    } else {
	      Res->total_found = ResultsLimit;
	    }
	  }

	  if(strcasecmp(cache_mode,"yes") == 0) {
/*			fflush(stdout);
			fflush(stderr);*/
			DpsSearchCacheStore(A, Res);
	  }
	}

	TRACE_OUT(A);
	return res;
}

/*****************************************************************/

static int WordInfo(DPS_VARLIST *Env_Vars, DPS_RESULT *Res) { 
	size_t	len, i, j, wsize;
	char	*wordinfo = NULL;
	size_t	corder = (size_t)-1, ccount = 0;
	
	for(len = i = 0; i < Res->WWList.nwords; i++) 
		len += Res->WWList.Word[i].len + 48;
	
	wsize=(1+len*15)*sizeof(char);
	wordinfo = (char*) DpsMalloc(wsize);
	if (wordinfo == NULL) return DPS_ERROR;

	*wordinfo = '\0';
	for(i = 0; i < Res->WWList.nwords; i++){
	        if(wordinfo[0]) dps_strcat(wordinfo,", ");
		sprintf(DPS_STREND(wordinfo)," %s: %d", Res->WWList.Word[i].word, Res->WWList.Word[i].count);
	}
	
	DpsVarListReplaceStr(Env_Vars, "WA", wordinfo);
	

	*wordinfo = '\0';
	for(i = 0; i < Res->WWList.nwords; i++){
	  if ((Res->WWList.Word[i].count > 0) || (Res->WWList.Word[i].origin & DPS_WORD_ORIGIN_QUERY)) {
			if(wordinfo[0]) dps_strcat(wordinfo,", ");
			sprintf(DPS_STREND(wordinfo)," %s: %d", Res->WWList.Word[i].word, Res->WWList.Word[i].count);
		} else if ((Res->WWList.Word[i].origin & (DPS_WORD_ORIGIN_STOP | DPS_WORD_ORIGIN_QUERY)) == 
			   (DPS_WORD_ORIGIN_STOP | DPS_WORD_ORIGIN_QUERY)) {
			if(wordinfo[0]) dps_strcat(wordinfo,", ");
			sprintf(DPS_STREND(wordinfo)," %s: stopword", Res->WWList.Word[i].word);
		}
	}
	
	DpsVarListReplaceStr(Env_Vars, "WE", wordinfo);
	

	*wordinfo = '\0';
	for(i = 0; i < Res->WWList.nwords; i++) {

	  if ((Res->WWList.Word[i].origin & DPS_WORD_ORIGIN_QUERY) == 0) continue;

	  corder = Res->WWList.Word[i].order;
	  ccount = 0;
	  for(j = 0; j < Res->WWList.nwords; j++) {
		if (Res->WWList.Word[j].order == corder) {
			ccount += Res->WWList.Word[j].count;
		}
	  }
	  if (Res->WWList.Word[i].origin & DPS_WORD_ORIGIN_STOP) {
	    sprintf(DPS_STREND(wordinfo),"%s%s: stopword", (*wordinfo) ? ", " : "",  Res->WWList.Word[i].word);
	  } else {
	    sprintf(DPS_STREND(wordinfo),"%s%s: %d / %zu",  (*wordinfo) ? ", " : "", Res->WWList.Word[i].word, Res->WWList.Word[i].count, ccount );
	  }
	}
	
	DpsVarListReplaceStr(Env_Vars, "W", wordinfo);


	*wordinfo = '\0';
	for(i = 0; i < Res->WWList.nwords; i++) {

	  if ((Res->WWList.Word[i].origin & DPS_WORD_ORIGIN_QUERY) == 0) continue;

	  corder = Res->WWList.Word[i].order;
	  ccount = 0;
	  for(j = 0; j < Res->WWList.nwords; j++) {
		if (Res->WWList.Word[j].order == corder) {
			ccount += Res->WWList.Word[j].count;
		}
	  }
	  if (Res->WWList.Word[i].origin & DPS_WORD_ORIGIN_STOP) {
		  sprintf(DPS_STREND(wordinfo),"%s%s: stopword", (*wordinfo) ? ", " : "",  Res->WWList.Word[i].word);
	  } else {
		  sprintf(DPS_STREND(wordinfo),"%s%s: %zu", 
			  (*wordinfo) ? ", " : "", Res->WWList.Word[i].word, ccount );
	  }
	}
	
	DpsVarListReplaceStr(Env_Vars, "WS", wordinfo);


	DPS_FREE(wordinfo);
	return DPS_OK;
}

/*
static float DpsTimeDiff(struct timeval *tm1, struct timeval *tm2) {
  return (float)(tm2->tv_sec - tm1->tv_sec) + ((float)(tm2->tv_usec - tm1->tv_usec)) / 1000000.0;
}
*/

/*****************************************************************/
#define MAX_PS 1000

DPS_RESULT * __DPSCALL DpsFind(DPS_AGENT *A) {
	DPS_DB		*db;
	DPS_RESULT	*Res;
	DPS_EXCERPT_CFG Cfg;
	int		res;
	unsigned long	ticks=DpsStartTimer(), ticks_;
	size_t i, dbfrom = 0, dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems, num;
	int		page_number;
	int		page_size;
	size_t          ExcerptSize = (size_t)DpsVarListFindInt(&A->Vars, "ExcerptSize", 256);
	size_t          ExcerptPadding = (size_t)DpsVarListFindInt(&A->Vars, "ExcerptPadding", 40);
	const char      *label = DpsVarListFindStr(&A->Vars, "label", NULL);
	char		str[256];
/*	struct timeval stime, etime;*/
	
	TRACE_IN(A, "DpsFind");
	DpsLog(A,DPS_LOG_DEBUG,"Start DpsFind");
/*	gettimeofday(&stime, NULL);*/

	res         = DpsVarListFindInt(&A->Vars, "ps", DPS_DEFAULT_PS);
	page_size   = dps_min(res, MAX_PS);
	res = DPS_OK;
	page_number = DpsVarListFindInt(&A->Vars, "p", 0);
	if (page_number == 0) {
	  page_number = DpsVarListFindInt(&A->Vars, "np", 0);
	} else page_number--;
	
	/* Allocate result */
	Res=DpsResultInit(NULL);
	if (Res == NULL) {
	  A->Res = NULL;
	  TRACE_OUT(A);
	  return NULL;
	}

	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  if (label != NULL && db->label == NULL) continue;
	  if (label == NULL && db->label != NULL) continue;
	  if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
	  switch(db->DBType) {
		case DPS_DB_SEARCHD:
		  res = DpsSearchdConnect(A, db);
		  break;
		default:
			break;
	  }
	}

	if (res != DPS_OK) {
	  DpsResultFree(Res);
	  A->Res = NULL;
	  TRACE_OUT(A);
	  return NULL;
	}

	DpsFindWords(A, Res);

	Res->first = page_number * page_size;	
	if(Res->first >= Res->total_found) Res->first = (Res->total_found)? (Res->total_found - 1) : 0;
	if((Res->first+page_size)>Res->total_found){
		Res->num_rows=Res->total_found-Res->first;
	}else{
		Res->num_rows=page_size;
	}
	Res->last=Res->first+Res->num_rows-1;

	/* Allocate an array for documents information */
	if (Res->num_rows > 0) {
	  Res->Doc = (DPS_DOCUMENT*)DpsMalloc(sizeof(DPS_DOCUMENT) * (Res->num_rows));
	  if (Res->Doc == NULL) {
	    DpsResultFree(Res);
	    A->Res = NULL;
	    TRACE_OUT(A);
	    return NULL;
	  }
	}
	
	/* Copy url_id and coord to result */
	for(i=0;i<Res->num_rows;i++){
	  dps_uint4	score = Res->CoordList.Coords[i + Res->first * Res->offset].coord;
	  double  pr = Res->CoordList.Data[i + Res->first * Res->offset].pop_rank;
	  DpsDocInit(&Res->Doc[i]);
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "DP_ID", Res->CoordList.Coords[i + Res->first * Res->offset].url_id);
	  dps_snprintf(str, sizeof(str), "%.3f%%", ((double)(score /*>> 8*/)) / 1000);
	  DpsVarListReplaceStr(&Res->Doc[i].Sections, "Score", str);
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Order", (int)(i + Res->first + 1));
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Pos", (int)(i + 1));
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "sdnum", (int)(score & 0xFF));
	  dps_snprintf(str, sizeof(str), "%.5f", pr);
	  DpsVarListReplaceStr(&Res->Doc[i].Sections, "Pop_Rank", str);
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Site_id", Res->CoordList.Data[i + Res->first * Res->offset].site_id);
#ifdef WITH_MULTIDBADDR
	  Res->Doc[i].dbnum = Res->CoordList.Coords[i + Res->first * Res->offset].dbnum;
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "dbnum", (int)(Res->Doc[i].dbnum));
#endif
#ifdef WITH_REL_TRACK
	  DpsVarListReplaceDouble(&Res->Doc[i].Sections, "Rel.x", Res->CoordList.Track[i + Res->first * Res->offset].x);
	  DpsVarListReplaceDouble(&Res->Doc[i].Sections, "Rel.xy", Res->CoordList.Track[i + Res->first * Res->offset].xy);
	  DpsVarListReplaceDouble(&Res->Doc[i].Sections, "Rel.y", Res->CoordList.Track[i + Res->first * Res->offset].y);
#ifdef WITH_REL_DISTANCE
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Rel.distance", Res->CoordList.Track[i + Res->first * Res->offset].D_distance);
#endif
#ifdef WITH_REL_POSITION
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Rel.position", Res->CoordList.Track[i + Res->first * Res->offset].D_position);
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Rel.firstpos", Res->CoordList.Track[i + Res->first * Res->offset].D_firstpos);
#endif
#ifdef WITH_REL_WRDCOUNT
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Rel.wrdcount", Res->CoordList.Track[i + Res->first * Res->offset].D_wrdcount);
#endif
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Rel.n_count", Res->CoordList.Track[i + Res->first * Res->offset].D_n_count);
	  DpsVarListReplaceInt(&Res->Doc[i].Sections, "Rel.n_origin", Res->CoordList.Track[i + Res->first * Res->offset].D_n_origin);
#endif
	}
	
	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  if (label != NULL && db->label == NULL) continue;
	  if (label == NULL && db->label != NULL) continue;
	  if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
	  switch(db->DBDriver){
		case DPS_DB_SEARCHD:
		  res = DpsResAddDocInfoSearchd(A, db, Res, i);
		  break;
		default:
		  if (db->DBMode == DPS_DBMODE_CACHE) {
		    res = DpsResAddDocInfoCache(A, db, Res, i);
		  }
#ifdef HAVE_SQL
		  if (db->DBType != DPS_DB_CACHE)
		    res = DpsResAddDocInfoSQL(A, db, Res, i);
			break;
#endif
	  }
	}

	num = Res->num_rows;

	if(!strcasecmp(DpsVarListFindStr(&A->Vars, "DetectClones", "no"), "yes")) {
	        ticks_=DpsStartTimer();
		DpsLog(A, DPS_LOG_DEBUG, "Start Clones");
		for(i=0;i<num;i++){
			DPS_RESULT *Cl = DpsCloneList(A, &A->Vars, &Res->Doc[i]);
			if(Cl){
				Res->Doc=(DPS_DOCUMENT*)DpsRealloc(Res->Doc,sizeof(DPS_DOCUMENT)*(Res->num_rows+Cl->num_rows+1));
				if (Res->Doc == NULL) {
				  DpsResultFree(Res);
				  A->Res = NULL;
				  TRACE_OUT(A);
				  return NULL;
				}
				dps_memcpy(&Res->Doc[Res->num_rows], Cl->Doc, sizeof(DPS_DOCUMENT)*Cl->num_rows); /* was: dps_memmove */
				Res->num_rows+=Cl->num_rows;
				DPS_FREE(Cl->Doc);
				DpsResultFree(Cl);
			}
		}
		ticks_ = DpsStartTimer() - ticks_;
		DpsLog(A, DPS_LOG_DEBUG, "Stop  Clones: %.2f", (float)ticks_/1000);
	}

	/* first and last begins from 0, make it begin from 1 */
	Res->first++;
	Res->last++;

	DpsAgentStoredConnect(A);
	
	ticks_=DpsStartTimer();
	DpsLog(A, DPS_LOG_DEBUG, "Start Order, Last-Modified and Excerpts");

	Cfg.query = A;
	Cfg.Res = Res;
	Cfg.size = ExcerptSize;
	Cfg.padding = ExcerptPadding;

	for(i = 0; i < num; i++) {
	  DPS_DOCUMENT *D = &Res->Doc[i];
	  DPS_MATCH    *Alias = NULL;
	  char	       *aliastr = NULL;
	  char         *url = DpsVarListFindStrTxt(&D->Sections, "URL", "");
	  char	       *alcopy = NULL;

	  alcopy = DpsRemoveHiLightDup(url);
	  if (alcopy != NULL) {
	    if((Alias = DpsMatchListFind(&A->Conf->Aliases, alcopy, 0, NULL))) {
	      aliastr = (char*)DpsMalloc(dps_strlen(Alias->arg) + dps_strlen(alcopy) + 1);
	      if (aliastr != NULL) {
		sprintf(aliastr, "%s%s", Alias->arg, alcopy + dps_strlen(Alias->pattern));
		DpsVarListReplaceStr(&D->Sections, "Alias-of", url);
		DpsVarListReplaceStr(&D->Sections, "URL", url = aliastr);
	      }
	    }
	  }

	  if (*url != '\0') {
	    if (!DpsURLParse(&D->CurURL, url)) {
	      DpsVarListInsStr(&D->Sections, "url.host", DPS_NULL2EMPTY(D->CurURL.hostname));
	      DpsVarListInsStr(&D->Sections, "url.path", DPS_NULL2EMPTY(D->CurURL.path));
	      DpsVarListInsStr(&D->Sections, "url.directory", DPS_NULL2EMPTY(D->CurURL.directory));
	      DpsVarListInsStr(&D->Sections, "url.file", DPS_NULL2EMPTY(D->CurURL.filename));
	      DpsVarListInsStr(&D->Sections, "url.query_string", DPS_NULL2EMPTY(D->CurURL.query_string));
	      D->fetched = 1;
	      Res->fetched++;
	    }
	  }
	  DpsFree(aliastr);
	  DpsFree(alcopy);

	  if (DpsVarListFindInt(&Res->Doc[i].Sections, "ST", -1) == -1) {
	    if (A->Flags.do_excerpt) {
	      Cfg.Doc = &Res->Doc[i];
	      DpsExcerptDoc(&Cfg);
	    }
	    if (DpsVarListFindStr(&Res->Doc[i].Sections, "Z", NULL) != NULL) {
	      DpsVarListReplaceInt(&Res->Doc[i].Sections, "ST", 0);
	    }
	  }
	}

	DpsConvert(A->Conf, &A->Vars, Res, A->Conf->lcs, A->Conf->bcs);

	{
	  const char	*format = DpsVarListFindStrTxt(&A->Vars, "DateFormat", "%a, %d %b %Y, %X %Z");
#ifdef HAVE_PTHREAD
	  struct tm l_tim;
#endif

	  for(i = 0; i < num; i++) {
	    time_t	last_mod_time = Res->CoordList.Data[i + (Res->first - 1) * Res->offset].last_mod_time;

		DpsVarListReplaceInt(&Res->Doc[i].Sections,"Order",(int)(Res->first+i));
		if (last_mod_time > 0) {
		  if (strftime(str, sizeof(str), format, 
#ifdef HAVE_PTHREAD
			       localtime_r(&last_mod_time, &l_tim)
#else
			       localtime(&last_mod_time)
#endif
			       ) == 0) {
		    DpsTime_t2HttpStr(last_mod_time, str);
		  }
		  DpsVarListReplaceStr(&Res->Doc[i].Sections, "Last-Modified", str);
		}
		
	  }
	}
	ticks_ = DpsStartTimer() - ticks_;
	DpsLog(A, DPS_LOG_DEBUG, "Stop  Order, Last-Modified and Excerpts: %.2f", (float)ticks_/1000);
/******************************/

	Res->work_time=ticks=DpsStartTimer()-ticks;
	dps_snprintf(str, sizeof(str), "%.3f", ((double)Res->work_time)/1000.0);
	DpsVarListReplaceStr(&A->Vars, "SearchTime", str);
	WordInfo(&A->Vars, Res);

	DpsLog(A, DPS_LOG_DEBUG, "Start DpsTrack");
	ticks_ = DpsStartTimer();
	DpsTrack(A, Res);
	ticks_ = DpsStartTimer() - ticks_;
	DpsLog(A, DPS_LOG_DEBUG, "Stop  DpsTrack: %.2f", (float)ticks_/1000);
	
/*	DpsLog(A,DPS_LOG_DEBUG,"Done  DpsFind %.3f [%.5f]", (float)ticks/1000, DpsTimeDiff(&stime, &etime));*/
	DpsLog(A,DPS_LOG_DEBUG,"Done  DpsFind %.3f", (float)ticks/1000);

	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
	  if (label != NULL && db->label == NULL) continue;
	  if (label == NULL && db->label != NULL) continue;
	  if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
	  switch(db->DBType) {
		case DPS_DB_SEARCHD:
		  DpsSearchdClose(db);
		  break;
		default:
			break;
	  }
	}

	if(res!=DPS_OK){
		DpsResultFree(Res);
		Res=NULL;
	}
	A->Res = Res;
	/*	DpsLog(A,DPS_LOG_DEBUG,"Exit DpsFind");*/
	TRACE_OUT(A);
	return Res;
}


static int DpsStr2DBMode(const char * str1){
	int m = -1;
	if(!strncasecmp(str1,"multi-crc",9))m=DPS_DBMODE_MULTI_CRC;
	else
	if(!strncasecmp(str1,"crc-multi",9))m=DPS_DBMODE_MULTI_CRC;
	else
	if(!strncasecmp(str1,"single",6))m=DPS_DBMODE_SINGLE;
	else
	if(!strncasecmp(str1,"crc",3))m=DPS_DBMODE_SINGLE_CRC;
	else
	if(!strncasecmp(str1,"multi",5))m=DPS_DBMODE_MULTI;
	else
	if(!strncasecmp(str1,"cache",5))m=DPS_DBMODE_CACHE;
	return(m);
}

__C_LINK const char* __DPSCALL DpsDBTypeToStr(int dbtype)
{
  switch(dbtype)
  {
    case DPS_DB_MYSQL:   return "mysql";
    case DPS_DB_PGSQL:   return "pgsql";
    case DPS_DB_IBASE:   return "ibase";
    case DPS_DB_MSSQL:   return "mssql";
    case DPS_DB_ORACLE8: return "oracle";
    case DPS_DB_SQLITE:  return "sqlite";
    case DPS_DB_SQLITE3: return "sqlite";
    case DPS_DB_MIMER:   return "mimer";
    case DPS_DB_ACCESS:  return "access";
  }
  return "unknown_dbtype";
}

__C_LINK const char* __DPSCALL DpsDBModeToStr(int dbmode) {
  switch(dbmode) 
  {
    case DPS_DBMODE_SINGLE:     return "single";
    case DPS_DBMODE_CACHE:	return "cache";
    case DPS_DBMODE_SINGLE_CRC:	return "crc";
    case DPS_DBMODE_MULTI:	return "multi";
    case DPS_DBMODE_MULTI_CRC:	return "crc-multi";
  }
  return "unknown_dbmode";
}

int DpsDBSetAddr(DPS_DB *db, const char *dbaddr, int mode){
	char	*s, *cport;
	char *stored_host = NULL, *cached_host = NULL;
	struct hostent *hp;
	int nport;
	char savec;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	
	if (!dbaddr) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}

	if(DpsURLParse(&db->addrURL, dbaddr)) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	DPS_FREE(db->DBADDR);
	DPS_FREE(db->DBName);
	DPS_FREE(db->DBUser);
	DPS_FREE(db->DBPass);
	DPS_FREE(db->DBSock);
	DPS_FREE(db->DBCharset);
	DPS_FREE(db->where);
	DPS_FREE(db->from);
	DPS_FREE(db->label);

	db->open_mode=mode;
	db->DBMode = DPS_DBMODE_CACHE;
	db->DBADDR = (char*)DpsStrdup(dbaddr);

	if (db->addrURL.schema == NULL){
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	if(!strcasecmp(db->addrURL.schema,"cached")){
		db->DBType   = DPS_DB_CACHED;
		db->DBDriver = DPS_DB_CACHED;
	}
	else if(!strcasecmp(db->addrURL.schema,"cache")){
		db->DBType   = DPS_DB_CACHE;
		db->DBDriver = DPS_DB_CACHE;
	}
	else if(!strcasecmp(db->addrURL.schema,"searchd")){
		db->DBType   = DPS_DB_SEARCHD;
		db->DBDriver = DPS_DB_SEARCHD;
	}
#if (HAVE_DP_MSQL||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"msql")){
		db->DBType = DPS_DB_MSQL;
#if defined(HAVE_DP_MSQL)
		db->DBDriver = DPS_DB_MSQL;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_LIMIT=1;
	}
#endif
#if (HAVE_SOLID||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"solid")){
		db->DBType=DPS_DB_SOLID;
#if defined(HAVE_SOLID)
		db->DBDriver = DPS_DB_SOLID;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
	}
#endif
#if (HAVE_ORACLE7||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"oracle7")){
		db->DBType=DPS_DB_ORACLE7;
#if defined(HAVE_ORACLE7)
		db->DBDriver = DPS_DB_ORACLE7;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_TRUNCATE=1;
		db->DBSQL_SUBSELECT = 1;
	}
#endif
#if (HAVE_ORACLE8||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"oracle8")){
		db->DBType=DPS_DB_ORACLE8;
#if defined(HAVE_ORACLE8)
		db->DBDriver = DPS_DB_ORACLE8;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_TRUNCATE=1;
		db->DBSQL_SUBSELECT = 1;
	}
	else if(!strcasecmp(db->addrURL.schema,"oracle")){
		db->DBType=DPS_DB_ORACLE8;
#if defined(HAVE_ORACLE8)
		db->DBDriver = DPS_DB_ORACLE8;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_TRUNCATE=1;
		db->DBSQL_SUBSELECT = 1;
	}
#endif
#if (HAVE_CTLIB||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"mssql")){
		db->DBType=DPS_DB_MSSQL;
#if defined(HAVE_CTLIB)
		db->DBDriver = DPS_DB_MSSQL;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_TRUNCATE=1;
		db->DBSQL_SUBSELECT = 1;
	}
#endif
#if (HAVE_DP_MYSQL||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"mysql")){
		db->DBType=DPS_DB_MYSQL;
#if defined(HAVE_DP_MYSQL)
		db->DBDriver = DPS_DB_MYSQL;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_LIMIT=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_MULTINSERT = 1;
	}
#endif
#if (HAVE_DP_PGSQL||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"pgsql")){
		db->DBType=DPS_DB_PGSQL;
#if defined(HAVE_DP_PGSQL)
		db->DBDriver = DPS_DB_PGSQL;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_LIMIT=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_SELECT_FROM_DELETE=0;
		db->DBSQL_SUBSELECT = 1;
		db->DBSQL_MULTINSERT = 1;
	}
#endif
#if (HAVE_IBASE||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"ibase")){
		db->DBType=DPS_DB_IBASE;
#if defined(HAVE_IBASE)
		db->DBDriver = DPS_DB_IBASE;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		/* 
		while indexing large sites and using the SQL in statement 
		interbase will fail when the items in the in IN statements
		are more then 1500. We'd better have to fix code to avoid 
		big INs instead of hidding DBSQL_IN.
		*/
		
		/* db->DBSQL_IN=1; */ 
		db->DBSQL_GROUP=1;
	}
#endif
#if (HAVE_SQLITE)
	else if (!strcasecmp(db->addrURL.schema,"sqlite")){
		db->DBType=DPS_DB_SQLITE;
		db->DBDriver = DPS_DB_SQLITE;
		db->DBSQL_IN=1;
		db->DBSQL_LIMIT=1;
		db->DBSQL_GROUP=1;
	}
#endif
#if (HAVE_SQLITE3)
	else if (!strcasecmp(db->addrURL.schema, "sqlite3")){
		db->DBType = DPS_DB_SQLITE;
		db->DBDriver = DPS_DB_SQLITE3;
		db->DBSQL_IN = 1;
		db->DBSQL_LIMIT = 1;
		db->DBSQL_GROUP = 1;
		db->DBSQL_SUBSELECT = 1;
	}
#endif
#if (HAVE_SAPDB||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"sapdb")){
		db->DBType=DPS_DB_SAPDB;
#if defined(HAVE_SAPDB)
		db->DBDriver = DPS_DB_SAPDB;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
	}
#endif
#if (HAVE_DB2||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema,"db2")){
		db->DBType=DPS_DB_DB2;
#if defined(HAVE_DB2)
		db->DBDriver = DPS_DB_DB2;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
	}
#endif
#if (HAVE_DB_ACCESS||HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema, "access")) {
		db->DBType=DPS_DB_ACCESS;
#if defined(HAVE_DP_ACCESS)
		db->DBDriver = DPS_DB_ACCESS;
#else
		db->DBDriver = DPS_DB_ODBC;
#endif
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_SUBSELECT = 1;
	}
#endif
#if (HAVE_ODBC)
	else if(!strcasecmp(db->addrURL.schema, "mimer")) {
		db->DBType = DPS_DB_MIMER;
		db->DBDriver = DPS_DB_ODBC;
		db->DBSQL_IN=1;
		db->DBSQL_GROUP=1;
		db->DBSQL_SUBSELECT = 1;
	}
#endif
	else {
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	
	if((s = strchr(DPS_NULL2EMPTY(db->addrURL.query_string), '?'))) {
		char * tok, *lt;
		
		*s++='\0';
		tok = dps_strtok_r(s, "&", &lt, &savec);
		while(tok){
			char * val;
			
			if((val=strchr(tok,'='))){
				*val++='\0';
				if(!strcasecmp(tok,"socket")&&val[0]){
					DPS_FREE(db->DBSock);
					db->DBSock = (char*)DpsStrdup(val);
				}else
				if(!strcasecmp(tok, "charset") && val[0]) {
					DPS_FREE(db->DBCharset);
					db->DBCharset = (char*)DpsStrdup(val);
				}else
				if(!strcasecmp(tok, "label") && val[0]) {
					DPS_FREE(db->label);
					db->label = (char*)DpsStrdup(val);
				}else
				if(!strcasecmp(tok,"numtables")&&val[0]){
					db->numtables=atoi(val);
					if(!db->numtables)
						db->numtables=1;
				}else
				if(!strcasecmp(tok,"dbmode")&&val[0]){
				  if ((db->DBMode=DpsStr2DBMode(val)) < 0) {
#ifdef WITH_PARANOIA
				    DpsViolationExit(-1, paran);
#endif
				    return DPS_ERROR;
				  }
				}else
				if(!strcasecmp(tok, "stored") && val[0]){
				  stored_host = (char*)DpsStrdup(val);
				}else
				if(!strcasecmp(tok, "cached") && val[0]){
				  cached_host = (char*)DpsStrdup(val);
				}else
				if(!strcasecmp(tok, "trackquery") && val[0]){
				  db->TrackQuery = !strcasecmp(val, "yes");
				}else
				if(!strcasecmp(tok, "VarDir") && val[0]){
				  DPS_FREE(db->vardir);
				  db->vardir = (char*)DpsStrdup(val);
				}else
				if(!strcasecmp(tok, "WrdFiles") && val[0]){
				  db->WrdFiles = DPS_ATOI(val);
				}else
				if(!strcasecmp(tok, "StoredFiles") && val[0]){
				  db->StoredFiles = DPS_ATOI(val);
				}else
				if(!strcasecmp(tok, "URLDataFiles") && val[0]){
				  db->URLDataFiles = DPS_ATOI(val);
				}else {
				  DpsVarListReplaceStr(&db->Vars, tok, val);
				}
			}else
			if (!strcasecmp(tok, "trackquery")) {
			  db->TrackQuery = 1;
			}else {
			  DpsVarListReplaceStr(&db->Vars, tok, "");
			}
			tok = dps_strtok_r(NULL, "&", &lt, &savec);
		}
	}
	
	if(db->DBType==DPS_DB_IBASE || db->DBType==DPS_DB_SQLITE || db->DBType == DPS_DB_SQLITE3){
		/* Ibase is a special case        */
		/* It's database name consists of */
		/* full path and file name        */ 
		db->DBName = (char*)DpsStrdup(DPS_NULL2EMPTY(db->addrURL.path));
	}else{
		db->DBName = (char*)DpsStrdup(DPS_NULL2EMPTY(db->addrURL.path));
		sscanf(DPS_NULL2EMPTY(db->addrURL.path), "/%[^/]s", db->DBName);
	}
	if((s=strchr(DPS_NULL2EMPTY(db->addrURL.auth),':'))){
		*s=0;
		db->DBUser = (char*)DpsStrdup(db->addrURL.auth);
		db->DBPass = (char*)DpsStrdup(s+1);
		DpsUnescapeCGIQuery(db->DBUser, db->DBUser);
		DpsUnescapeCGIQuery(db->DBPass, db->DBPass);
		*s=':';
	}else{
		db->DBUser = (char*)DpsStrdup(DPS_NULL2EMPTY(db->addrURL.auth));
	}

	bzero((void*)&db->stored_addr, sizeof(db->stored_addr));
	bzero((void*)&db->cached_addr, sizeof(db->cached_addr));

	if (stored_host) {

	  if((cport = strchr(stored_host, ':'))) {
	    *cport = '\0';
	    nport = atoi(cport + 1);
	  } else {
	    nport = DPS_STORED_PORT;
	  }

	  if ((hp = gethostbyname(stored_host)) == 0 ) {
	    DPS_FREE(stored_host); DPS_FREE(cached_host);
#ifdef WITH_PARANOIA
	    DpsViolationExit(-1, paran);
#endif
	    return DPS_ERROR;
	  }
	  dps_memcpy(&db->stored_addr.sin_addr, hp->h_addr, (size_t)hp->h_length); /* was: dps_memmove */
	  db->stored_addr.sin_family = (sa_family_t)hp->h_addrtype;
	  db->stored_addr.sin_port = htons((u_short)nport);
	}

	if (cached_host) {

	  if((cport = strchr(cached_host, ':')) !=NULL ) {
	    *cport = '\0';
	    nport = atoi(cport + 1);
	  } else {
	    nport = DPS_LOGD_PORT;
	  }

	  if ((hp = gethostbyname(cached_host)) == 0 ) {
	    DPS_FREE(stored_host); DPS_FREE(cached_host);
#ifdef WITH_PARANOIA
	    DpsViolationExit(-1, paran);
#endif
	    return DPS_ERROR;
	  }
	  dps_memcpy(&db->cached_addr.sin_addr, hp->h_addr, (size_t)hp->h_length); /* was: dps_memmove */
	  db->cached_addr.sin_family = (sa_family_t)hp->h_addrtype;
	  db->cached_addr.sin_port = htons((u_short)nport);
	}

	DPS_FREE(stored_host); 
	DPS_FREE(cached_host);
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_OK;
}


__C_LINK int __DPSCALL DpsStatAction(DPS_AGENT *A, DPS_STATLIST *S){
	DPS_DB	*db;
	int	res=DPS_ERROR;
	size_t i, dbfrom = 0, dbto =  (A->flags & DPS_FLAG_UNOCON) ? A->Conf->dbl.nitems : A->dbl.nitems;
	
	TRACE_IN(A, "DpsStatAction");

	bzero((void*)S, sizeof(S[0]));

	for (i = dbfrom; i < dbto; i++) {
	  db = (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl.db[i] : &A->dbl.db[i];
/*	  if(db->DBDriver == DPS_DB_CACHE)
		res = DpsStatActionCache(A, S, db); */ /* FIXME: usr this after it implementation */
#ifdef HAVE_SQL
	  res = DpsStatActionSQL(A, S, db);
#endif
	  if (res != DPS_OK) break;
	}
	if(res!=DPS_OK){
		dps_strcpy(A->Conf->errstr, db->errstr);
	}
	TRACE_OUT(A);
	return res;
}

unsigned int DpsGetCategoryId(DPS_ENV *Conf, char *category) {
	DPS_DB	*db;
	unsigned int rc = 0;
	size_t i, dbfrom = 0, dbto =  Conf->dbl.nitems;

	for (i = dbfrom; i < dbto; i++) {
	  db = &Conf->dbl.db[i];
#ifdef HAVE_SQL
	  rc = DpsGetCategoryIdSQL(Conf, category, db);
	  if (rc != 0) return rc;
#endif
	}
	return rc;
}


int DpsTrack(DPS_AGENT *query, DPS_RESULT *Res) {
  int rc = DPS_OK;
  size_t i, dbfrom = 0, dbto =  (query->flags & DPS_FLAG_UNOCON) ? query->Conf->dbl.nitems : query->dbl.nitems; 
  DPS_DB *db;

  TRACE_IN(query, "DpsTrack");

  for (i = dbfrom; i < dbto; i++) {
    db = (query->flags & DPS_FLAG_UNOCON) ? &query->Conf->dbl.db[i] : &query->dbl.db[i];
    if(db->TrackQuery) {
#ifdef HAVE_SQL
      rc = DpsTrackSQL(query, Res, db);
#endif
    }
  }
  TRACE_OUT(query);
  return rc;
}


int DpsTrackSearchd(DPS_AGENT * query, DPS_RESULT *Res) {
  DPS_DB *db;
  size_t i, dbfrom = 0, dbto =  (query->flags & DPS_FLAG_UNOCON) ? query->Conf->dbl.nitems : query->dbl.nitems; 
  int rc = DPS_OK;
  int fd, qtime;
  const char	*words = DpsVarListFindStr(&query->Vars, "q", ""); /* "q-lc" was here */
  const char      *IP = DpsVarListFindStr(&query->Vars, "IP", "localhost");
  size_t r, escaped_len, qbuf_len;
  char		*qbuf, *text_escaped, *z;
  char fullname[PATH_MAX]="";

#ifdef HAVE_SQL

  TRACE_IN(query, "DpsTrackSearchd");

  if (*words == '\0') {
    TRACE_OUT(query);
    return DPS_OK; /* do not store empty queries */
  }

  escaped_len = 4 * dps_strlen(words) + 1;
  qbuf_len = escaped_len + 4096;

  if ((qbuf = (char*)DpsMalloc(qbuf_len)) == NULL) {
    TRACE_OUT(query);
    return DPS_ERROR;
  }
  if ((text_escaped = (char*)DpsMalloc(escaped_len + 1)) == NULL) { 
    DPS_FREE(qbuf); 
    TRACE_OUT(query);
    return DPS_ERROR; 
  }

  memset( qbuf, 0x20, sizeof( long ));
	
  for (i = dbfrom; (i < dbto); i++) {
    db = (query->flags & DPS_FLAG_UNOCON) ? &query->Conf->dbl.db[i] : &query->dbl.db[i];
    if(db->TrackQuery) {
      const char      *vardir = (db->vardir != NULL) ? db->vardir : DpsVarListFindStr(&query->Vars, "VarDir", DPS_VAR_DIR);

      dps_snprintf(fullname, sizeof(fullname), "%s%strack.%d.%d.%d", vardir, DPSSLASHSTR, query->handle, i, time(NULL));
      if ((fd = open(fullname, O_WRONLY | O_CREAT | DPS_BINARY, DPS_IWRITE)) <= 0) {
	char errstr[1024];
	dps_strerror(query, DPS_LOG_ERROR, "DpsTrackSearchd: couldn't open track file (%s) for writing", fullname);
	DpsLog(query, DPS_LOG_ERROR, errstr );
	return DPS_ERROR;
      }

      /* Escape text to track it  */
      (void)DpsDBEscStr(db, text_escaped, words, dps_strlen(words));

      dps_snprintf(qbuf + sizeof(long), qbuf_len - sizeof(long), "%s\2%s\2%d\2%d\2%d",
		   IP, text_escaped, qtime = (int)time(NULL), Res->total_found, Res->work_time
		   );
	
      r = (size_t)'q';
      for (i = 0; i < query->Vars.Root[r].nvars; i++) {
	DPS_VAR *Var = &query->Vars.Root[r].Var[i];
	if (strncasecmp(Var->name, "query.",6)==0 && strcasecmp(Var->name, "query.q") && strcasecmp(Var->name, "query.BrowserCharset")
	    && strcasecmp(Var->name, "query.g-lc") && strncasecmp(Var->name, "query.Excerpt", 13) 
	    && strcasecmp(Var->name, "query.IP") && strcasecmp(Var->name, "query.DateFormat") && Var->val != NULL && *Var->val != '\0') {
	  
	  z = DPS_STREND(qbuf + sizeof(long));
	  dps_snprintf(z, qbuf_len - (z - qbuf), "\2%s\2%s", &Var->name[6], Var->val);
	}
      }

      if (write(fd, qbuf,  sizeof(long) + dps_strlen(qbuf + sizeof(long))) < (ssize_t)(sizeof(long) + dps_strlen(qbuf + sizeof(long)))) {
	rc = DPS_ERROR;
	DpsLog(query, DPS_LOG_ERROR, "DpsTrackSearchd: couldn't write to file %s [%s:%d]", fullname, __FILE__, __LINE__ );
      } else rc = DPS_OK;

      DpsLog(query, DPS_LOG_DEBUG, "DpsTrackSearchd: qbuf[%d]: %s", dps_strlen(qbuf), qbuf );

      close(fd);
    }
  }
  DPS_FREE(text_escaped);
  DPS_FREE(qbuf);
  TRACE_OUT(query);
#endif /*HAVE_SQL*/
  return rc;
}




DPS_RESULT * DpsCloneList(DPS_AGENT * Indexer, DPS_VARLIST *Env_Vars, DPS_DOCUMENT *Doc) {
	size_t i, dbfrom = 0, dbto =  (Indexer->flags & DPS_FLAG_UNOCON) ? Indexer->Conf->dbl.nitems : Indexer->dbl.nitems;
	const char      *label = DpsVarListFindStr(&Indexer->Vars, "label", NULL);
	DPS_DB		*db;
	DPS_RESULT	*Res;
	int		rc = DPS_OK;

	TRACE_IN(Indexer, "DpsCloneList");

	Res = DpsResultInit(NULL);
	if (Res == NULL) {
	  TRACE_OUT(Indexer);
	  return NULL;
	}

	for (i = dbfrom; i < dbto; i++) {
	    db = (Indexer->flags & DPS_FLAG_UNOCON) ? &Indexer->Conf->dbl.db[i] : &Indexer->dbl.db[i];
	    if (label != NULL && db->label == NULL) continue;
	    if (label == NULL && db->label != NULL) continue;
	    if (label != NULL && db->label != NULL && strcasecmp(db->label, label)) continue;
	    switch(db->DBDriver) {
	    case DPS_DB_SEARCHD:
		  rc = DpsCloneListSearchd(Indexer, Doc, Res, db);
			break;
#ifdef HAVE_SQL
	    default:
	      if (db->DBType != DPS_DB_CACHE)
		rc = DpsCloneListSQL(Indexer, Env_Vars, Doc, Res, db);
	      break;
#endif
	    }
	    if (rc != DPS_OK) break;
	}
	if (Res->num_rows > 0) {
	    TRACE_OUT(Indexer);
	    return Res;
	}
	DpsResultFree(Res);
	TRACE_OUT(Indexer);
	return NULL;
}


int DpsCheckUrlid(DPS_AGENT *Agent, urlid_t id) {
	size_t i, dbfrom = 0, dbto;
	DPS_DB		*db;
	int		rc = 0;

	TRACE_IN(Agent, "DpsCheckUrlid");

	if (Agent->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
	if (Agent->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);

	for (i = dbfrom; i < dbto; i++) {
	    db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] :  &Agent->dbl.db[i];
	    if (Agent->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_DB); 
	    switch(db->DBDriver) {
#ifdef HAVE_SQL
	    default:
	      rc = DpsCheckUrlidSQL(Agent, db, id);
	      break;
#endif
	    }
	    if (Agent->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_DB);
	    if (rc != DPS_OK) break;
	}
	TRACE_OUT(Agent);
	return rc;
}


int DpsCheckReferrer(DPS_AGENT *Agent, DPS_DOCUMENT *Doc) {
	size_t i, dbfrom = 0, dbto;
	DPS_DB		*db;
	int		rc = DPS_ERROR;
	urlid_t id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);

	TRACE_IN(Agent, "DpsCheckReferrer");

	if (Agent->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
	if (Agent->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);

	for (i = dbfrom; i < dbto; i++) {
	    db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] :  &Agent->dbl.db[i];
	    if (Agent->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_DB); 
	    switch(db->DBDriver) {
#ifdef HAVE_SQL
	    default:
	      rc = DpsCheckReferrerSQL(Agent, db, id);
	      break;
#endif
	    }
	    if (Agent->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_DB);
	    if (rc == DPS_OK) break;
	}
	TRACE_OUT(Agent);
	return rc;
}


/********************************************************/


DPS_DBLIST * DpsDBListInit(DPS_DBLIST * List){
	bzero((void*)List, sizeof(*List));
	return(List);
}

size_t DpsDBListAdd(DPS_DBLIST *List, const char * addr, int mode) {
	DPS_DB	*db;
	int res;
	size_t i;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	for (i = 0; i < List->nitems; i++) {
	  if (strcasecmp(List->db[i].DBADDR, addr) == 0) {
#ifdef WITH_PARANOIA
	    DpsViolationExit(-1, paran);
#endif
	    return DPS_OK;
	  }
	}
	db=List->db=(DPS_DB*)DpsRealloc(List->db,(List->nitems+1)*sizeof(DPS_DB));
	if (db == NULL) {
	  List->nitems = 0;
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	db+=List->nitems;
	if (DpsDBInit(db) != NULL) res = DpsDBSetAddr(db, addr, mode);
	else res = DPS_ERROR;
	if (res == DPS_OK) {
	  db->dbnum = List->nitems;
	  List->nitems++;
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}

void DpsDBListFree(DPS_DBLIST *List){
	size_t	i;
	DPS_DB	*db=List->db;
	
	for(i = 0; i < List->nitems; i++){
		DpsDBFree(&db[i]);
	}
	DPS_FREE(List->db);
	DpsDBListInit(List);
}


/**************************************************/

int DpsURLDataPreload(DPS_AGENT *Agent) {
	size_t i, dbfrom = 0, dbto;
	DPS_DB		*db;
	int		rc = 0;

	TRACE_IN(Agent, "DpsURLDataPreload");

	if (Agent->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
	if (Agent->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);

	for (i = dbfrom; i < dbto; i++) {
	    db = (Agent->Conf->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] : &Agent->dbl.db[i];
	    if (Agent->Conf->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_DB);

	    if (db->DBMode == DPS_DBMODE_CACHE) {
	      rc = DpsURLDataPreloadCache(Agent, db);
	    } else {
#ifdef HAVE_SQL
	      rc = DpsURLDataPreloadSQL(Agent, db);
#endif
	    }
	    if (Agent->Conf->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_DB);
	    if (rc != DPS_OK) break;
	}
	TRACE_OUT(Agent);
	return rc;
}

int DpsURLDataDePreload(DPS_AGENT *Agent) {
	DPS_DB		*db;
	DPS_URLDATA_FILE *DF;
	int		rc = 0, filenum, NFiles;
	size_t i, dbfrom = 0, dbto;

	TRACE_IN(Agent, "DpsURLDataPreload");

	DPS_GETLOCK(Agent, DPS_LOCK_CONF);
	if (Agent->Conf->Flags.PreloadURLData) {

/*	  if (Agent->Conf->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Agent, DPS_LOCK_CONF);*/
	  dbto =  (Agent->Conf->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
/*	  if (Agent->Conf->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);*/

	  for (i = dbfrom; i < dbto; i++) {

	    db = (Agent->Conf->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] : &Agent->dbl.db[i];
	    NFiles = (db->URLDataFiles) ? db->URLDataFiles : DpsVarListFindUnsigned(&Agent->Conf->Vars, "URLDataFiles", 0x300);
	    DF = Agent->Conf->URLDataFile[db->dbnum];
	    for (filenum = 0 ; filenum < NFiles; filenum++) DPS_FREE(DF[filenum].URLData);

	    DPS_FREE(Agent->Conf->URLDataFile[i]);
	  }
	  DPS_FREE(Agent->Conf->URLDataFile);
	}
	DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
	TRACE_OUT(Agent);
	return rc;
}

int DpsURLDataLoad(DPS_AGENT *Indexer, DPS_RESULT *R, DPS_DB *db) {
  DPS_URLDATA *Dat, *D = NULL, K, *F;
  DPS_URL_CRD_DB *Crd;
  size_t i, j, count, nrec = 0, first = 0;
  int NFiles = (db->URLDataFiles) ? db->URLDataFiles : DpsVarListFindUnsigned(&Indexer->Conf->Vars, "URLDataFiles", 0x300);
  int filenum, prevfilenum = - 1, rc = DPS_OK;

  TRACE_IN(Indexer, "DpsURLDataLoad");

  if (Indexer->Flags.PreloadURLData) {
    count = R->CoordList.ncoords;
/*    DpsLog(Indexer, DPS_LOG_DEBUG, "DpsURLDataLoad: count: %d", count);*/
    if (count == 0) {
      TRACE_OUT(Indexer);
      return DPS_OK;
    }
    Dat = R->CoordList.Data = (DPS_URLDATA*)DpsRealloc(R->CoordList.Data, count * sizeof(DPS_URLDATA));
    if (Dat == NULL) { TRACE_OUT(Indexer); return DPS_ERROR; }
    Crd = R->CoordList.Coords;

    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
    for (i = j = 0; i < count; i++) {
      filenum = DPS_FILENO(Crd[i].url_id, NFiles);
      if (filenum != prevfilenum) {
	prevfilenum = filenum;
	nrec = Indexer->Conf->URLDataFile[db->dbnum][filenum].nrec;
	D = Indexer->Conf->URLDataFile[db->dbnum][filenum].URLData;
	first = 0;
      }
      K.url_id = Crd[i].url_id;
      if ((nrec > 0) 
	  && (F = (DPS_URLDATA*)dps_bsearch(&K, &D[first], nrec - first, sizeof(DPS_URLDATA),(qsort_cmp) DpsCmpURLData)) != NULL) {
	Dat[j] = *F;
	first = (F - D);
	if (i != j) Crd[j] = Crd[i];
	j++;
      }
    }
    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
    R->CoordList.ncoords = j;
/*    DpsLog(Indexer, DPS_LOG_DEBUG, "DpsURLDataLoad: ncoords: %d", j);*/
    
    TRACE_OUT(Indexer);
    return DPS_OK;
  }

  if (db == NULL) {
/*    rc = DpsURLDataLoadCache(Indexer, R);*/
  } else {
#ifdef HAVE_SQL
    rc = DpsURLDataLoadSQL(Indexer, R, db);
#endif
  }

  TRACE_OUT(Indexer);
  return rc;

}
