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

#ifndef _DPS_DB_INT_H
#define _DPS_DB_INT_H

#include <limits.h>

typedef struct {
  dps_uint8 stamp;
  dps_uint4 cmd;
  dps_uint4 nwords;
  urlid_t   url_id;
} DPS_LOGD_CMD;

typedef struct {
	dps_uint4	wrd_id;
	dps_uint4	coord;
} DPS_LOGD_WRD;

typedef struct {
	time_t	stamp;
	urlid_t	url_id;
	dps_uint4	wrd_id;
	dps_uint4	coord;
} DPS_LOGWORD;

typedef struct {
	time_t	stamp;
	urlid_t	url_id; 
} DPS_LOGDEL;

/************************** Logd stuff **************************/
/*#define DPS_MAX_LOG 4096*/  /* should be power of 2 */
/*#define DPS_MAX_WRD 1024*/   /* 256 => 4Kb */
/*#define DPS_MAX_DEL 1024*/
#define DPS_INF_DEL 11

#define open_flags O_WRONLY|O_CREAT|O_APPEND|DPS_BINARY
#define open_perm  DPS_IWRITE

typedef struct{
	DPS_LOGWORD *data;
        DPS_LOGDEL  *del_buf;
	size_t nrec;
        size_t ndel;
} dps_wrd_buf;

typedef struct {
	dps_wrd_buf	*wrd_buf;
        size_t          cur_del_buf;
} DPS_LOGD;


#if (HAVE_DP_MYSQL)

#include <mysql.h>
#define ER_DUP_ENTRY            1062
#define ER_DUP_KEY		1022
#define CR_SERVER_LOST          2013
#define CR_SERVER_GONE_ERROR    2006
#define ER_SERVER_SHUTDOWN      1053
#endif

#if (HAVE_DP_PGSQL)
#include "libpq-fe.h"
#endif

#if HAVE_DP_MSQL
#include "msql.h"
#endif

#if HAVE_IODBC
#include "sql.h"
#include "sqlext.h"
#endif

#if HAVE_EASYSOFT
#include "sql.h"
#include "sqlext.h"
#endif

#if HAVE_VIRT
#include "iodbc.h"
#include "isql.h"
#include "isqlext.h"
#endif

#if HAVE_UNIXODBC
#include "sql.h"
#include "sqlext.h"
#endif

#if HAVE_SAPDB
#include "WINDOWS.H"
#include "sql.h"
#include "sqlext.h"
#endif

#if HAVE_SOLID
#include "cli0cli.h"
#endif

#if HAVE_DB2
#include "sqlcli1.h"
#endif

#if HAVE_IBASE
#include "ibase.h"
#endif

#if HAVE_CTLIB
#include <ctpublic.h>
#endif

#if HAVE_SQLITE
#include <sqlite.h>
#endif

#if HAVE_SQLITE3
#include <sqlite3.h>
#endif

#if HAVE_ORACLE8
#include "oci.h"
#endif

#if HAVE_ORACLE7
 #include "ocidfn.h"
 #include "oratypes.h"
 #ifdef __STDC__
  #include <ociapr.h>
 #else
  #include <ocikpr.h>
 #endif
#endif



#ifdef HAVE_ORACLE8

#define	MAX_COLS_IN_TABLE	32
#define MAX_BIND_PARAM		4
#define BUF_OUT_SIZE		128
struct param_struct{
	int out_rec;
	int out_pos[MAX_BIND_PARAM];
	int out_pos_val[MAX_BIND_PARAM][BUF_OUT_SIZE];
};
#endif


/* Multi-dict mode defines */
#define MAXMULTI	32
#define NDICTS		18
#define MINDICT		2
#define MAXDICT		NDICTS
#define	DICTNUM(l)	(((l) > 16) ? dictlen[17] : dictlen[(l)])

static const size_t dictlen[NDICTS]={2,2,2,3,4,5,6,7,8,9,10,11,12,16,16,16,16,32};


#define	DPS_URL_DELETE_CACHE_SIZE	128
#define	DPS_URL_SELECT_CACHE_SIZE	1024
#define	DPS_URL_DUMP_CACHE_SIZE		100000
#define URL_LOCK_TIME			4*60*60
#define DPS_MAX_MULTI_INSERT_QSIZE	60*1024
#define NDOCS_QUERY			"SELECT count(*) FROM url"

#define DPS_SQL_UNKNOWN	0
#define DPS_SQL_SELECT	1
#define DPS_SQL_UPDATE	2

typedef struct {
	char	*sqlname;
	int	sqltype;
	int	sqllen;
} DPS_SQLFIELD;

typedef struct {
	size_t	len;
	char	*val;
} DPS_PSTR;

typedef struct {
	size_t nRows;
	size_t nCols;
	int DBDriver;
	int qtype;
	char ** items;
	
	DPS_SQLFIELD	*Fields;
	DPS_PSTR	*Items;
	
#ifdef HAVE_DP_PGSQL
	PGresult  *pgsqlres;
#endif
	
#ifdef HAVE_DP_MSQL
	m_result  *msqlres;
#endif
	
#ifdef HAVE_ORACLE8
	char	*defbuff[MAX_COLS_IN_TABLE]; /* Buffers for OCIStmtFetch */
	int	col_size[MAX_COLS_IN_TABLE]; /* Size of column */
	sb2	indbuff[MAX_COLS_IN_TABLE][BUF_OUT_SIZE];  /* Indicators for NULLs */
#endif

} DPS_SQLRES;




typedef struct struct_dps_db {
        DPS_SQLRES Res;
	DPS_URL    addrURL;
        size_t     dbnum;
	int	   freeme;
	char	   *DBADDR;
	char	   *DBName;
	char	   *DBUser;
	char	   *DBPass;
	char	   *DBSock;
        char       *DBCharset;
	int	   DBMode;
	char	   *where;
        char       *from;
        char       *label;
	int	   DBType;
	int	   DBDriver;
	
	int	   DBSQL_IN;
	int	   DBSQL_LIMIT;
	int	   DBSQL_GROUP;
	int	   DBSQL_TRUNCATE;
	int	   DBSQL_SELECT_FROM_DELETE;
        int        DBSQL_SUBSELECT;
        int        DBSQL_MULTINSERT;
	
	int	connected;
        int     TrackQuery;             /* =1, if track queries into this db */
	int	open_mode;
	int	res_limit;
	int	commit_fl;
	unsigned int	numtables;
	int	errcode;
	char	errstr[2048];
	
/*	int		urld_fd;*/	/* URL server socket descriptor      */
	int		searchd;	/* Searchd daemon descriptors         */
	int		del_fd;		/* Cache mode dellog descriptor      */
        int             cat_fd;         /* Cache mode category log descriptor */
        int             tag_fd;         /* Cache mode tag log descriptor */
        int             time_fd;        /* Cache mode time log descriptor */
        int             lang_fd;        /* Cache mode language log descriptor */
        int             ctype_fd;       /* Cache mode content-type log descriptor */
        int             site_fd;        /* Cache mode site log descriptor */
	char		log_dir[PATH_MAX];	/* Where to store logs               */
	DPS_LOGD	LOGD;		/* Cache mode local descriptors      */
        int             logd_fd;        /* connection to cached file descriptor */
  struct sockaddr_in    stored_addr;    /* stored address                    */
  struct sockaddr_in    cached_addr;    /* cached address                    */
        DPS_VARLIST     Vars;           /* optional parameters and variables*/

        char            *vardir;
        size_t          WrdFiles, StoredFiles, URLDataFiles;

	/** Cache mode limits */
	DPS_SEARCH_LIMIT	*limits;
        size_t		nlimits;

#if defined(HAVE_DP_PGSQL) || defined(HAVE_DP_MYSQL)
        int             async_in_process;
#endif

#ifdef HAVE_DP_MYSQL
	MYSQL  mysql;
#endif

#ifdef HAVE_DP_PGSQL
	PGconn	*pgsql;
#endif

#ifdef HAVE_DP_MSQL
	int	msql;
#endif

#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_SAPDB)
	HDBC	hDbc;
	HENV	hEnv;
	HSTMT	hstmt;
#endif

#if HAVE_DB2
	SQLHANDLE	hDbc;
	SQLHANDLE	hEnv;
	SQLHANDLE	hstmt;
#endif

#ifdef HAVE_IBASE
	isc_db_handle	DBH;		/* database handle    */
	ISC_STATUS	status[20];	/* status vector      */
	isc_tr_handle   tr_handle;	/* transaction handle */ 
#endif

#ifdef HAVE_ORACLE7
	Lda_Def     lda;
	ub1 hda[HDA_SIZE];
	Cda_Def     cursor;
#endif

#ifdef HAVE_CTLIB
	CS_CONTEXT	*ctx;
	CS_CONNECTION	*conn;
#endif

#ifdef HAVE_SQLITE
	struct sqlite	*sqlt;
#endif

#ifdef HAVE_SQLITE3
	struct sqlite3	*sqlt3;
#endif

#ifdef HAVE_ORACLE8
	OCIEnv     *envhp;
	OCIError   *errhp;
	OCISvcCtx  *svchp;
	OCIStmt	   *stmthp;
	OCIParam   *param;
	OCIDefine  *defb[MAX_COLS_IN_TABLE];
	OCIBind    *bndhp[MAX_BIND_PARAM];
	struct param_struct *par;
#endif


} DPS_DB;


extern int DpsCmpurldellog(const void *s1, const void *s2);
extern int DpsCmplog(const DPS_LOGWORD *s1, const DPS_LOGWORD *s2);
extern int DpsCmplog_wrd(const DPS_LOGWORD *s1, const DPS_LOGWORD *s2);
extern int DpsCmpURLData(DPS_URLDATA *d1, DPS_URLDATA *d2);

#endif
