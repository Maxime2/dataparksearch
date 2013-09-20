/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2006-2012 DataPark Ltd. All rights reserved.

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
#include "dps_cookies.h"
#include "dps_utils.h"
#include "dps_hash.h"
#include "dps_sqldbms.h"
#include "dps_mutex.h"
#include "dps_log.h"
#include "dps_utils.h"
#include "dps_vars.h"
#include "dps_charsetutils.h"

#include <string.h>


int DpsCookiesAdd(DPS_AGENT *Indexer, const char *domain, const char * path, const char *name, const char *value, const char secure,
		  dps_uint4 expires, int insert_flag) {
#ifdef HAVE_SQL

  char buf[3*PATH_MAX];
  char path_esc[2*PATH_MAX+1];
  DPS_COOKIES *Cookies = &Indexer->Cookies;
  DPS_COOKIE *Coo;
  DPS_DB *db;
  dpshash32_t url_id = DpsStrHash32(domain);
  size_t i;
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif

  if (Indexer->flags & DPS_FLAG_UNOCON) {
    if (Indexer->Conf->dbl.nitems == 0) return DPS_OK;
    DPS_GETLOCK(Indexer, DPS_LOCK_DB);
    db = Indexer->Conf->dbl.db[url_id % Indexer->Conf->dbl.nitems];
  } else {
    if (Indexer->dbl.nitems == 0) return DPS_OK;
    db = Indexer->dbl.db[url_id % Indexer->dbl.nitems];
  }
  (void)DpsDBEscStr(db, path_esc, DPS_NULL2EMPTY(path), dps_min(PATH_MAX,dps_strlen(DPS_NULL2EMPTY(path))));

  for (i = 0; i < Cookies->ncookies; i++) {
    Coo = &Cookies->Cookie[i];
    if (!strcasecmp(Coo->domain, domain) && !strcasecmp(Coo->path, path) && !strcasecmp(Coo->name, name) && (Coo->secure == secure) ) {
      DPS_FREE(Coo->value);
      Coo->value = DpsStrdup(value);
/*      Coo->expires = expires;*/
      if (insert_flag) {
	dps_snprintf(buf, sizeof(buf), "UPDATE cookies SET value='%s',expires=%d WHERE domain='%s' AND path='%s' AND name='%s' AND secure='%c'",
		     value, expires, domain, path_esc, name, secure);
	DpsSQLAsyncQuery(db, NULL, buf);
      }
      if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_OK;
    }
  }

  Cookies->Cookie = (DPS_COOKIE*)DpsRealloc(Cookies->Cookie, (Cookies->ncookies + 1) * sizeof(DPS_COOKIE));
  if(Cookies->Cookie == NULL) {
    Cookies->ncookies = 0;
    if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
#ifdef WITH_PARANOIA
    DpsViolationExit(Indexer->handle, paran);
#endif
    return DPS_ERROR;
  }
  Coo = &Cookies->Cookie[Cookies->ncookies];
/*  Coo->expires = expires;*/
  Coo->secure = secure;
  Coo->domain = DpsStrdup(domain);
  Coo->path = DpsStrdup(path);
  Coo->name = DpsStrdup(name);
  Coo->value = DpsStrdup(value);
  if (insert_flag) {
    if (Indexer->Flags.CheckInsertSQL) {
      dps_snprintf(buf, sizeof(buf), "DELETE FROM cookies WHERE domain='%s' AND path='%s' AND name='%s' AND secure='%c'",
		   domain, path_esc, name, secure);
      DpsSQLAsyncQuery(db, NULL, buf);
    }
    dps_snprintf(buf, sizeof(buf), "INSERT INTO cookies(expires,secure,domain,path,name,value)VALUES(%d,'%c','%s','%s','%s','%s')",
		 expires, secure, domain, path_esc, name, value);
    DpsSQLAsyncQuery(db, NULL, buf);
  }
  Cookies->ncookies++;
  if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
#ifdef WITH_PARANOIA
  DpsViolationExit(Indexer->handle, paran);
#endif

#endif /*HAVE_SQL*/
  return DPS_OK;
}

void DpsCookiesFree(DPS_COOKIES *Cookies) {
  size_t i;
  DPS_COOKIE *Coo;

   for (i = 0; i < Cookies->ncookies; i++) {
    Coo = &Cookies->Cookie[i];
    DPS_FREE(Coo->domain);
    DPS_FREE(Coo->path);
    DPS_FREE(Coo->name);
    DPS_FREE(Coo->value);
   }
   DPS_FREE(Cookies->Cookie);
   Cookies->ncookies = 0;
}


void DpsCookiesClean(DPS_AGENT *A) {
    char buf[256];
    DPS_DB	*db;
    size_t i, dbfrom = 0, dbto;
    int res;

    if (A->Flags.robots_period == 0) return;

    dps_snprintf(buf, sizeof(buf), "DELETE FROM cookies WHERE expires < %d", A->now);

    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_CONF);
    dbto = DPS_DBL_TO(A);
    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_CONF);

    for (i = dbfrom; i < dbto; i++) {
	db = DPS_DBL_DB(A, i);
	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
#ifdef HAVE_SQL
	res = DpsSQLAsyncQuery(db, NULL, buf);
#endif
	if (res != DPS_OK) {
	    DpsLog(A, DPS_LOG_ERROR, db->errstr);
	}
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	if (res != DPS_OK) break;
    }
}


void DpsCookiesFind(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, const char *hostinfo) {
#ifdef HAVE_SQL
  DPS_DSTR cookie;
  DPS_COOKIES *Cookies = &Indexer->Cookies;
  DPS_COOKIE *Coo;
  size_t i, blen = dps_strlen(hostinfo), slen;
  int have_no_cookies = 1;

  DpsDSTRInit(&cookie, 1024);
  for(i = 0; i < Cookies->ncookies; i++) {
    Coo = &Cookies->Cookie[i];
    slen = dps_strlen(Coo->domain);
    if (slen > blen) continue;
    if (Coo->secure == 'y' && strcasecmp(Doc->CurURL.schema, "https")) continue;
    if (Coo->secure == 'n' && !strcasecmp(Doc->CurURL.schema, "https")) continue;
    if (strncasecmp(Coo->path, Doc->CurURL.path, dps_strlen(Coo->path))) continue;
    if (strcasecmp(Coo->domain, hostinfo + (blen - slen))) continue;
    have_no_cookies = 0;
    if (Coo->name[0] == '\0' && Coo->value[0] == '\0') continue;
    if (cookie.data_size)
      DpsDSTRAppend(&cookie, "; ", 2);
    DpsDSTRAppendStr(&cookie, Coo->name);
    DpsDSTRAppend(&cookie, "=", 1);
    DpsDSTRAppendStr(&cookie, Coo->value);
  }
  if (have_no_cookies) {
    char buf[2*PATH_MAX];
    dpshash32_t url_id;
    DPS_DB *db;
    DPS_SQLRES Res;
    size_t rows;
    int rc;

    while(hostinfo != NULL) {
      url_id = DpsStrHash32(hostinfo);
      DpsSQLResInit(&Res);
      dps_snprintf(buf, sizeof(buf), "SELECT name,value,path,secure FROM cookies WHERE domain='%s'", hostinfo);
      if (Indexer->flags & DPS_FLAG_UNOCON) {
	DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	db = Indexer->Conf->dbl.db[url_id % Indexer->Conf->dbl.nitems];
      } else {
	db = Indexer->dbl.db[url_id % Indexer->dbl.nitems];
      }
      if(DPS_OK == (rc = DpsSQLQuery(db, &Res, buf))) {
	rows = DpsSQLNumRows(&Res);
	for(i = 0; i < rows; i++) {
	  DpsCookiesAdd(Indexer, hostinfo, DpsSQLValue(&Res, i, 2), DpsSQLValue(&Res, i, 0), DpsSQLValue(&Res, i, 1), 
			*DpsSQLValue(&Res, i, 3), 0, 0);
	  if (*DpsSQLValue(&Res, i, 3) == 'y' && strcasecmp(Doc->CurURL.schema, "https")) continue;
	  if (*DpsSQLValue(&Res, i, 3) == 'n' && !strcasecmp(Doc->CurURL.schema, "https")) continue;
	  if (strncasecmp(DpsSQLValue(&Res, i, 2), Doc->CurURL.path, dps_strlen(DpsSQLValue(&Res, i, 2)))) continue;
	  if (cookie.data_size)
	    DpsDSTRAppend(&cookie, "; ", 2);
	  DpsDSTRAppendStr(&cookie, DpsSQLValue(&Res, i, 0));
	  DpsDSTRAppend(&cookie, "=", 1);
	  DpsDSTRAppendStr(&cookie, DpsSQLValue(&Res, i, 1));
	}
	if (rows == 0) {
	  DpsCookiesAdd(Indexer, hostinfo, "/", "", "", 'n', 0, 0);
	}
      }
      DpsSQLFree(&Res);
      if (Indexer->flags & DPS_FLAG_UNOCON) {
	DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
      }	  
      hostinfo = strchr(hostinfo, '.');
      if (hostinfo != NULL) hostinfo++;
    }
  }
  if (cookie.data_size) {
    DpsVarListReplaceStr(&Doc->RequestHeaders, "Cookie", cookie.data);
  }
  DpsDSTRFree(&cookie);
#endif
}
