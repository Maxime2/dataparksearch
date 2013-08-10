/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 Datapark corp. All rights reserved.
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

#ifndef _DPS_DBMS_H
#define _DPS_DBMS_H

#include "dps_config.h"
#include "dps_base.h"

/* Database types and drivers */
#define DPS_DB_UNK			0
#define DPS_DB_MSQL			1
#define DPS_DB_MYSQL			2
#define DPS_DB_PGSQL			3
#define DPS_DB_SOLID			4
#define DPS_DB_VIRT			6
#define DPS_DB_IBASE			7
#define DPS_DB_ORACLE8			8
#define DPS_DB_ORACLE7			9
#define DPS_DB_MSSQL			10
#define DPS_DB_SAPDB			11
#define DPS_DB_DB2			12
#define DPS_DB_SQLITE			13
#define DPS_DB_ACCESS			14
#define DPS_DB_MIMER			15
#define DPS_DB_SQLITE3                  16
#define DPS_DB_SEARCHD			200
#define DPS_DB_CACHED                   400
#define DPS_DB_CACHE                    401
#define DPS_DB_ODBC                     500


#define DPS_URL_ACTION_DELETE		1
#define DPS_URL_ACTION_ADD		2
#define DPS_URL_ACTION_SUPDATE		3
#define DPS_URL_ACTION_LUPDATE		4
#define DPS_URL_ACTION_INSWORDS		5
#define DPS_URL_ACTION_INSCWORDS	6
#define DPS_URL_ACTION_DELWORDS		7
#define DPS_URL_ACTION_DELCWORDS	8
#define DPS_URL_ACTION_UPDCLONE		9
#define DPS_URL_ACTION_REGCHILD		10
#define DPS_URL_ACTION_FINDBYURL	11
#define DPS_URL_ACTION_FINDBYMSG	12
#define DPS_URL_ACTION_FINDORIG		13
#define DPS_URL_ACTION_EXPIRE		14
#define DPS_URL_ACTION_REFERERS		15
#define DPS_URL_ACTION_SERVERTABLE	17
#define DPS_URL_ACTION_DOCCOUNT		18
#define DPS_URL_ACTION_FLUSH		19
#define DPS_URL_ACTION_WRITEDATA        20
#define DPS_URL_ACTION_LINKS_DELETE     21
#define DPS_URL_ACTION_ADD_LINK		22
#define DPS_URL_ACTION_FLUSHCACHED	23
#define DPS_URL_ACTION_LINKS_MARKTODEL  24
#define DPS_URL_ACTION_REFRESHDOCINFO   25
#define DPS_URL_ACTION_PASNEO           26
#define DPS_URL_ACTION_REFERER          27
#define DPS_URL_ACTION_RESORT		28
#define DPS_URL_ACTION_REHASHSTORED	29
#define DPS_URL_ACTION_POSTPONE_ON_ERR  30
#define DPS_URL_ACTION_SITEMAP          31

#define DPS_RES_ACTION_DOCINFO		1

#define DPS_CAT_ACTION_PATH		1
#define DPS_CAT_ACTION_LIST		2

#define DPS_SRV_ACTION_TABLE		1
#define DPS_SRV_ACTION_FLUSH            2
#define DPS_SRV_ACTION_ADD              3
#define DPS_SRV_ACTION_ID               4
#define DPS_SRV_ACTION_POPRANK          5
#define DPS_SRV_ACTION_URLDB            6
#define DPS_SRV_ACTION_SERVERDB         7
#define DPS_SRV_ACTION_REALMDB          8
#define DPS_SRV_ACTION_SUBNETDB         9
#define DPS_SRV_ACTION_CLEAN            10
#define DPS_SRV_ACTION_CATTABLE		11
#define DPS_SRV_ACTION_CATFLUSH         12

#define DPS_IFIELD_TYPE_HOUR      0
#define DPS_IFIELD_TYPE_MIN       1
#define DPS_IFIELD_TYPE_HOSTNAME  2
#define DPS_IFIELD_TYPE_STRCRC32  3
#define DPS_IFIELD_TYPE_INT       4
#define DPS_IFIELD_TYPE_HEX8STR   5
#define DPS_IFIELD_TYPE_STR2CRC32 6

#define DPS_LIMTYPE_NESTED     0
#define DPS_LIMTYPE_TIME       1
#define DPS_LIMTYPE_LINEAR_INT 2
#define DPS_LIMTYPE_LINEAR_CRC 3

#define DPS_LIMFNAME_CAT   "lim_cat"
#define DPS_LIMFNAME_TAG   "lim_tag"
#define DPS_LIMFNAME_TIME  "lim_time"
#define DPS_LIMFNAME_HOST  "lim_host"
#define DPS_LIMFNAME_LANG  "lim_lang"
#define DPS_LIMFNAME_CTYPE "lim_ctype"
#define DPS_LIMFNAME_SITE  "lim_site"
#define DPS_LIMFNAME_LINK  "lim_link"

enum {
  DPS_LIMIT_CAT = 1,
  DPS_LIMIT_TAG = 2,
  DPS_LIMIT_TIME = 4,
  DPS_LIMIT_LANG = 8,
  DPS_LIMIT_CTYPE = 16,
  DPS_LIMIT_SITE = 32
};

extern void	*DpsDBInit(void *db);
extern void	DpsDBFree(void *db);

extern __C_LINK int __DPSCALL DpsURLAction(DPS_AGENT *A, DPS_DOCUMENT   *D, int cmd);
extern __C_LINK int __DPSCALL DpsResAction(DPS_AGENT *A, DPS_RESULT     *R, int cmd);
extern __C_LINK int __DPSCALL DpsCatAction(DPS_AGENT *A, DPS_CATEGORY   *C, int cmd);
extern __C_LINK int __DPSCALL DpsSrvAction(DPS_AGENT *A, DPS_SERVER     *S, int cmd);
extern __C_LINK int __DPSCALL DpsTargets(DPS_AGENT *Indexer);

extern __C_LINK int __DPSCALL DpsLimit8(DPS_AGENT *A, DPS_UINT8URLIDLIST *L,const char *field, int type, void *db);
extern __C_LINK int __DPSCALL DpsLimit4(DPS_AGENT *A, DPS_UINT4URLIDLIST *L,const char *field, int type, void *db);
extern __C_LINK int __DPSCALL DpsSQLLimit8(DPS_AGENT *A, DPS_UINT8URLIDLIST *L, const char *req, int type, DPS_DB *db);
extern __C_LINK int __DPSCALL DpsSQLLimit4(DPS_AGENT *A, DPS_UINT4URLIDLIST *L, const char *req, int type, DPS_DB *db);

extern __C_LINK DPS_RESULT * __DPSCALL DpsFind(DPS_AGENT *A);
extern __C_LINK int __DPSCALL DpsClearDatabase(DPS_AGENT *A);
extern __C_LINK int __DPSCALL DpsStatAction(DPS_AGENT *A, DPS_STATLIST *S);

extern __C_LINK int __DPSCALL DpsTrack(DPS_AGENT * query, DPS_RESULT *Res);
#ifdef HAVE_SYS_MSG_H
extern __C_LINK int __DPSCALL DpsTrackSearchd(DPS_AGENT * query, DPS_RESULT *Res);
#else
#define DpsTrackSearchd DpsTrack
#endif

extern __C_LINK DPS_RESULT * __DPSCALL DpsCloneList(DPS_AGENT * Indexer, DPS_VARLIST *Env_Vars, DPS_DOCUMENT *Doc);

extern DPS_DBLIST	*DpsDBListInit(DPS_DBLIST *List);
extern size_t		DpsDBListAdd(DPS_DBLIST *List, const char * addr, int mode);
extern void		DpsDBListFree(DPS_DBLIST *List);
extern int DpsDBSetAddr(DPS_DB *db, const char *dbaddr, int mode);
extern unsigned int     DpsGetCategoryId(DPS_ENV *Conf, char *category);

extern int DpsFindWords(DPS_AGENT *A, DPS_RESULT *Res);
extern int DpsHTDBGet(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc, int flag_short);

extern int DpsCheckUrlid(DPS_AGENT *A, urlid_t id);
extern int DpsCheckReferrer(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc);

extern int DpsURLDataPreload(DPS_AGENT *Agent);
extern int DpsURLDataDePreload(DPS_AGENT *Agent);
extern int DpsURLDataLoad(DPS_AGENT *Indexer, DPS_RESULT *Res, DPS_DB *db);

/******************/

#ifdef HAVE_SQL
extern int   DpsClearDBSQL(DPS_AGENT    *A, DPS_DB *db);
extern int   DpsURLActionSQL(DPS_AGENT  *A, DPS_DOCUMENT   *D, int cmd, DPS_DB *db);
extern int   DpsResActionSQL(DPS_AGENT  *A, DPS_RESULT     *R, int cmd, DPS_DB *db, size_t dbnum);
extern int   DpsCatActionSQL(DPS_AGENT  *A, DPS_CATEGORY   *C, int cmd, DPS_DB *db);
extern int   DpsSrvActionSQL(DPS_AGENT  *A, DPS_SERVER     *S, int cmd, DPS_DB *db);
extern int   DpsStatActionSQL(DPS_AGENT *A, DPS_STATLIST   *S, DPS_DB *db);
extern int   DpsFindWordsSQL(DPS_AGENT *, DPS_RESULT *Res, DPS_DB *db);
extern int   DpsLimit8SQL(DPS_AGENT *A, DPS_UINT8URLIDLIST *L, const char *field, int type, DPS_DB *db);
extern int   DpsLimitCategorySQL(DPS_AGENT *A, DPS_UINT8URLIDLIST *L, const char *field, int type, DPS_DB *db);
extern int   DpsLimitLinkSQL(DPS_AGENT *A, DPS_UINT4URLIDLIST *L, const char *field, int type, DPS_DB *db);
extern int   DpsLimit4SQL(DPS_AGENT *A, DPS_UINT4URLIDLIST *L,const char *field, int type, DPS_DB *db);
extern int   DpsLimitTagSQL(DPS_AGENT *A, DPS_UINT4URLIDLIST *L, DPS_DB *db);
extern unsigned int   DpsGetCategoryIdSQL(DPS_ENV *Conf, char *category, DPS_DB *db);
extern int DpsResAddDocInfoSQL(DPS_AGENT *query, DPS_DB *db, DPS_RESULT *Res, size_t i);
extern int DpsTrackSQL(DPS_AGENT * query, DPS_RESULT *Res, DPS_DB *db);
extern int DpsCloneListSQL(DPS_AGENT * Indexer, DPS_VARLIST *Env_Vars, DPS_DOCUMENT *Doc, DPS_RESULT *Res, DPS_DB *db);
extern int DpsLoadURLDataSQL(DPS_AGENT *A, DPS_RESULT *R, DPS_DB *db);
extern int DpsTargetsSQL(DPS_AGENT *Indexer, DPS_DB *db);
extern int DpsCheckUrlidSQL(DPS_AGENT *A, DPS_DB *db, urlid_t id);
extern int DpsCheckReferrerSQL(DPS_AGENT *Indexer, DPS_DB *db, urlid_t id);
extern int DpsURLDataPreloadSQL(DPS_AGENT *Agent, DPS_DB *db);
extern int DpsURLDataLoadSQL(DPS_AGENT *Indexer, DPS_RESULT *Res, DPS_DB *db);
#endif


extern int LogdInit(DPS_DB *db,const char *var_dir);
extern int LogdClose(DPS_DB *db);
extern int LogdSaveAllBufs(DPS_DB *db);
extern int DpsLogdSaveBuf(DPS_AGENT *Indexer, DPS_ENV * Env, size_t log_num);
extern int LogdStoreDoc(DPS_DB *db,DPS_LOGD_CMD cmd,DPS_LOGD_WRD *wrd);

extern int DpsLogdClose(DPS_AGENT *Agent, DPS_DB *db, const char *var_dir, size_t i, int shared);
extern int DpsLogdSaveAllBufs(DPS_AGENT *Agent);
extern int DpsLogdStoreDoc(DPS_AGENT *Agent, DPS_LOGD_CMD cmd, DPS_LOGD_WRD *wrd, DPS_DB *db);


extern int DpsClearCacheTree(DPS_ENV * Env);
extern __C_LINK int __DPSCALL DpsProcessBuf(DPS_AGENT *Indexer, DPS_BASE_PARAM *P, size_t log_num,
					    DPS_LOGWORD *log_buf, size_t n, DPS_LOGDEL *del_buf, size_t del_count);
extern __C_LINK int __DPSCALL DpsRemoveDelLogDups(DPS_LOGDEL *words, size_t n);
extern __C_LINK size_t __DPSCALL DpsRemoveOldWords(DPS_LOGWORD *words, size_t n, DPS_LOGDEL *del, size_t del_count);

extern int DpsStoreWordsCache(DPS_AGENT * Indexer,DPS_DOCUMENT *Doc, DPS_DB *db);
extern int DpsFindWordsCache(DPS_AGENT * Indexer, DPS_RESULT *Res, DPS_DB *db);
extern int DpsDeleteURLFromCache(DPS_AGENT *Indexer, urlid_t url_id, DPS_DB *db);
extern int DpsURLDataWrite(DPS_AGENT *Indexer, DPS_DB *db);
extern int DpsCachedFlush(DPS_AGENT *Indexer, DPS_DB *db);

extern void DpsDecodeHex8Str(const char *hex_str, dps_uint4 *hi, dps_uint4 *lo, dps_uint4 *fhi, dps_uint4 *flo);

extern __C_LINK int __DPSCALL DpsAddSearchLimit(DPS_AGENT *Agent, int type, const char *file_name, const char *val);
extern __C_LINK urlid_t* LoadNestedLimit(DPS_AGENT *Agent, DPS_DB *db, size_t lnum, size_t *size);
extern __C_LINK urlid_t* LoadLinearLimit(DPS_AGENT *Agent, DPS_DB *db, const char *name, dps_uint4 val, size_t *size);
extern __C_LINK urlid_t* LoadTimeLimit(DPS_AGENT *Agent, DPS_DB *db, const char *name, dps_uint4 from, dps_uint4 to, size_t *size);

extern int DpsURLDataPreloadCache(DPS_AGENT *Agent, DPS_DB *db);
extern int DpsURLDataLoadCache(DPS_AGENT *Indexer, DPS_RESULT *Res, DPS_DB *db);


extern __C_LINK const char* __DPSCALL DpsDBTypeToStr(int dbtype);
extern __C_LINK const char* __DPSCALL DpsDBModeToStr(int dbmode);

extern int DpsExecActions(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, char action);

#endif
