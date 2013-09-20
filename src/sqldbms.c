/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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
#include "dps_utils.h"
#include "dps_sqldbms.h"
#include "dps_xmalloc.h"
#include "dps_charsetutils.h"

#ifdef HAVE_SQL

/*
#define DEBUG_SQL
*/

#define DEBUG_ERR_QUERY


#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/***************************************************************/

#define sql_val(x,y,z)	((x)->items[(y)*((x)->nCols)+(z)])
#define sql_value(x,y,z) ((x)?(sql_val((x),y,z)?sql_val((x),y,z):""):"")


static int udb_free_result(DPS_SQLRES *res){
	size_t i;
	if(res){
		if(res->items){
			for(i=0;i<res->nCols*res->nRows;i++)
				if(res->items[i])
					DPS_FREE(res->items[i]);
			DPS_FREE(res->items);
		}
		
		if(res->Items){
			for(i=0;i<res->nCols*res->nRows;i++)
				if(res->Items[i].val)
					DPS_FREE(res->Items[i].val);
			DPS_FREE(res->Items);
		}
	}
	return(0);
}




/************************ MYSQL **************************************/
#if   (HAVE_DP_MYSQL)

static int DpsMySQLInit(DPS_DB *db){
	mysql_init(&(db->mysql));
	if(!(mysql_real_connect(&(db->mysql),db->addrURL.hostname,db->DBUser,db->DBPass,db->DBName?db->DBName:"search",(unsigned)db->addrURL.port,db->DBSock,0))){
		db->errcode=1;
		sprintf(db->errstr,"MySQL driver: #%d: %s",mysql_errno(&db->mysql),mysql_error(&db->mysql));
		return DPS_ERROR;
	}
	db->connected=1;
	if (db->DBCharset) {
	  char query[64];
	  dps_snprintf(query, sizeof(query), "SET NAMES '%s'", db->DBCharset);
	  DpsSQLAsyncQuery(db, NULL, query);
	}
	DpsSQLAsyncQuery(db, NULL, "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED");
	return DPS_OK;
}

static int DpsMySQLQuery(DPS_DB *db,DPS_SQLRES *R,const char *query){
	size_t	i;
	
	db->errcode=0;
	if (db->connected && db->async_in_process) {
	  if (mysql_read_query_result(&db->mysql)) {
	    if((mysql_errno(&db->mysql)==CR_SERVER_LOST)||
	       (mysql_errno(&db->mysql)==CR_SERVER_GONE_ERROR)||
	       (mysql_errno(&db->mysql)==ER_SERVER_SHUTDOWN)) {
	      mysql_close(&db->mysql);
	      db->connected = 0;
	    } else if((mysql_errno(&(db->mysql))!=ER_DUP_ENTRY) &&
		      (mysql_errno(&(db->mysql))!=ER_DUP_KEY)){
	      sprintf(db->errstr,"AsyncQuery Error\n\tMySQL driver: #%d: %s", mysql_errno(&db->mysql), mysql_error(&db->mysql));
	    }
	  }
	  db->async_in_process = 0;
	}
	
	for(i = 0; i < 3; i++) {
	        if(!db->connected) {
		  int rc = DpsMySQLInit(db);
		  if (rc != DPS_OK || !db->connected) {
		    mysql_close(&db->mysql);
		    db->connected = 0;
		    DPSSLEEP(20);
		    continue;
		  }
		  if (db->async_in_process) {
		    mysql_read_query_result(&db->mysql);
		    db->async_in_process = 0;
		  }
		}
		
		if((mysql_query(&db->mysql,query))){
			if((mysql_errno(&db->mysql)==CR_SERVER_LOST)||
			   (mysql_errno(&db->mysql)==CR_SERVER_GONE_ERROR)||
			   (mysql_errno(&db->mysql)==ER_SERVER_SHUTDOWN)){
			        mysql_close(&db->mysql);
				db->connected = 0;
				DPSSLEEP(5);
			}else{
				sprintf(db->errstr,"MySQL driver: #%d: %s",mysql_errno(&db->mysql),mysql_error(&db->mysql));
				if((mysql_errno(&(db->mysql))!=ER_DUP_ENTRY) &&
				   (mysql_errno(&(db->mysql))!=ER_DUP_KEY)){
					db->errcode=1;
					return DPS_ERROR;
				}
				db->errcode=0;
				return DPS_OK;
			}
		}else{
			MYSQL_RES	*mysqlres;
			
			if((mysqlres=mysql_use_result(&db->mysql))){
				MYSQL_FIELD	*field;
				MYSQL_ROW	mysqlrow;
				size_t		mitems=0;
				size_t		nfields;
				
				R->nCols=mysql_num_fields(mysqlres);
				R->nRows=0;
				R->Items=NULL;
				R->Fields = (DPS_SQLFIELD*)DpsMalloc((R->nCols + 1) * sizeof(DPS_SQLFIELD));
				if (R->Fields == NULL) {
				  mysql_free_result(mysqlres);
				  return DPS_ERROR;
				}
				bzero(R->Fields,R->nCols*sizeof(DPS_SQLFIELD));
				
				for(nfields=0; (field=mysql_fetch_field(mysqlres)); nfields++){
					R->Fields[nfields].sqlname = (char*)DpsStrdup(field->name);
					R->Fields[nfields].sqllen=field->length;
				}
				
				while((mysqlrow=mysql_fetch_row(mysqlres))){
					size_t		col;
					unsigned long	*lengths=mysql_fetch_lengths(mysqlres);
					
					for(col=0;col<R->nCols;col++){
						size_t offs=R->nRows*R->nCols+col;
						size_t len;
						
						if(offs>=mitems){
							mitems+=256;
							R->Items=(DPS_PSTR*)DpsRealloc(R->Items,mitems*sizeof(DPS_PSTR));
							if (R->Items == NULL) {
							  mysql_free_result(mysqlres);
							  return DPS_ERROR;
							}
						}
						
						len=R->Items[offs].len=lengths[col];
						R->Items[offs].val=(char*)DpsMalloc(len+1);
						if (R->Items[offs].val == NULL) {
						  mysql_free_result(mysqlres);
						  return DPS_ERROR;
						}
						dps_memcpy(R->Items[offs].val, mysqlrow[col], len); /*was: dps_memmove */
						R->Items[offs].val[len]='\0';
					}
					R->nRows++;
				}
				mysql_free_result(mysqlres);
			}
			return DPS_OK;
		}
	}
	db->errcode = 1;
	sprintf(db->errstr, "MySQL driver: #%d: %s", mysql_errno(&db->mysql), mysql_error(&db->mysql));
	return DPS_ERROR;
}

static int DpsMySQLAsyncQuery(DPS_DB *db, DPS_SQLRES *R, const char *query) {
	size_t	i;
	
	db->errcode=0;
	if (db->connected && db->async_in_process) {
	  if (mysql_read_query_result(&db->mysql)) {
	    if((mysql_errno(&db->mysql)==CR_SERVER_LOST)||
	       (mysql_errno(&db->mysql)==CR_SERVER_GONE_ERROR)||
	       (mysql_errno(&db->mysql)==ER_SERVER_SHUTDOWN)) {
	      mysql_close(&db->mysql);
	      db->connected = 0;
	    } else if((mysql_errno(&(db->mysql)) != ER_DUP_ENTRY) &&
	       (mysql_errno(&(db->mysql)) != ER_DUP_KEY)){
	      sprintf(db->errstr,"AsyncQuery Error\n\tMySQL driver: #%d: %s", mysql_errno(&db->mysql), mysql_error(&db->mysql));
	    }
	  }
	  db->async_in_process = 0;
	}
	
	for(i = 0; i < 3; i++) {
	  if(!db->connected) {
		int rc = DpsMySQLInit(db);
		if(rc!=DPS_OK)return rc;
		if (rc != DPS_OK || !db->connected) {
		  mysql_close(&db->mysql);
		  db->connected = 0;
		  DPSSLEEP(20);
		  continue;
		}
		if (db->async_in_process) {
		  mysql_read_query_result(&db->mysql);
		  db->async_in_process = 0;
		}
	  }
	  if((mysql_send_query(&db->mysql, query, dps_strlen(query)))){
			if((mysql_errno(&db->mysql)==CR_SERVER_LOST)||
			   (mysql_errno(&db->mysql)==CR_SERVER_GONE_ERROR)||
			   (mysql_errno(&db->mysql)==ER_SERVER_SHUTDOWN)){
			        mysql_close(&db->mysql);
				db->connected = 0;
				DPSSLEEP(5);
			}else{
				sprintf(db->errstr,"MySQL driver: #%d: %s",mysql_errno(&db->mysql),mysql_error(&db->mysql));
				if((mysql_errno(&(db->mysql))!=ER_DUP_ENTRY) &&
				   (mysql_errno(&(db->mysql))!=ER_DUP_KEY)){
					db->errcode=1;
					return DPS_ERROR;
				}
				db->errcode=0;
				return DPS_OK;
			}
	  } else {
	    db->async_in_process = 1;
	    return DPS_OK;
	  }
	}
}

#endif


/*********************************** POSTGRESQL *********************/
#if (HAVE_DP_PGSQL)

static int DpsPgSQLInitDB(DPS_DB *db){
	char port[8];
	int rc;

	sprintf(port,"%d",db->addrURL.port);
	db->pgsql = PQsetdbLogin(db->DBSock ? db->DBSock:db->addrURL.hostname, db->addrURL.port?port:0, 0, 0, db->DBName, db->DBUser, db->DBPass);
	if (PQstatus(db->pgsql) == CONNECTION_BAD){
		db->errcode = 1;
		return DPS_ERROR;
	}
	db->connected=1;
	if (db->DBCharset) {
	  if (PQsetClientEncoding(db->pgsql, db->DBCharset) != 0) {
	    fprintf(stderr, "Can't set encoding: %s\n", db->DBCharset);
		db->errcode = 1;
		return DPS_ERROR;
	  }
	}
	rc = DpsSQLAsyncQuery(db, NULL, "set standard_conforming_strings to on");
	return rc;
}

static int DpsPgSQLQuery(DPS_DB *db, DPS_SQLRES *res, const char *q){
	size_t i;
	
	db->errcode=0;
	if (db->connected /*&& db->async_in_process*/) {
	  /* Wait async query in process */
	  while ((res->pgsqlres = PQgetResult(db->pgsql))) {
	    if (PQstatus(db->pgsql) == CONNECTION_BAD) { 
	      PQfinish(db->pgsql); /* db error occured, trying reconnect */
	      db->connected=0;
	      break;
	    }
	    if (PQresultStatus(res->pgsqlres)!=PGRES_COMMAND_OK) {
		if (PQerrorMessage(db->pgsql)) {
		  sprintf(db->errstr, "%s", PQerrorMessage(db->pgsql));
		} else {
		  sprintf(db->errstr, "<empty>, status: %d", PQresultStatus(res->pgsqlres));
		}
		if(!(strcasestr(db->errstr, "duplicate") /* en */
		   || strcasestr(db->errstr, "") /* ru */
		   || strcasestr(db->errstr, "duplizierter") /* de */
		   || strcasestr(db->errstr, "doppelter") /* de */
		   || strcasestr(db->errstr, "duplicada") /* es */
		   || strcasestr(db->errstr, "duplique") /* fr */
		   || strcasestr(db->errstr, "duplicirani") /* hr */
		   || strcasestr(db->errstr, "una chiave duplicata") /* it */
		   || strcasestr(db->errstr, "chave duplicada") /* pt-BR */
		     )) {
		  fprintf(stderr, "AsyncQuery Error\n\tPgSQL-server message: %s\n\n", db->errstr);
		}
	    }
	    PQclear(res->pgsqlres);
	  }
	  db->async_in_process = 0;
	}

	for (i = 0; i < 3; i++) {
	  if(!db->connected){
	        DpsPgSQLInitDB(db);
		if(db->errcode) {
		  PQfinish(db->pgsql);
		  db->connected=0;
		  res->pgsqlres = NULL;
		  DPSSLEEP(20);
		  continue;
		}
	  }
	
	  if(db->connected)
		res->pgsqlres = PQexec(db->pgsql, q);
	  if (res->pgsqlres != NULL) break;
	  sprintf(db->errstr, "%s", PQerrorMessage(db->pgsql) ? PQerrorMessage(db->pgsql) : "<empty>");

	  PQfinish(db->pgsql); /* db error occured, trying reconnect */
	  db->connected=0;
	  DPSSLEEP(20);
	}
	
	if(!res->pgsqlres){
		db->errcode=1;
		return DPS_ERROR;
	}
	if(PQresultStatus(res->pgsqlres)==PGRES_COMMAND_OK){
		/* Free non-SELECT query */
		PQclear(res->pgsqlres);
		res->pgsqlres=NULL;
		return DPS_OK;
	}
	if((PQresultStatus(res->pgsqlres)!=PGRES_COMMAND_OK)&&(PQresultStatus(res->pgsqlres)!=PGRES_TUPLES_OK)){
		PQclear(res->pgsqlres);
		res->pgsqlres=NULL;
		if (PQerrorMessage(db->pgsql)) {
		  sprintf(db->errstr, "%s", PQerrorMessage(db->pgsql));
		} else {
		  sprintf(db->errstr, "<empty>, status: %d", PQresultStatus(res->pgsqlres));
		}
	
		if(strcasestr(db->errstr, "duplicate") /* en */
		   || strcasestr(db->errstr, "") /* ru */
		   || strcasestr(db->errstr, "duplizierter") /* de */
		   || strcasestr(db->errstr, "doppelter") /* de */
		   || strcasestr(db->errstr, "duplicada") /* es */
		   || strcasestr(db->errstr, "duplique") /* fr */
		   || strcasestr(db->errstr, "duplicirani") /* hr */
		   || strcasestr(db->errstr, "una chiave duplicata") /* it */
		   || strcasestr(db->errstr, "chave duplicada") /* pt-BR */
		   ) {
			return DPS_OK;
		}else{
			db->errcode = 1;
			return DPS_ERROR;
		}
	}
	res->nCols=(size_t)PQnfields(res->pgsqlres);
	res->nRows=(size_t)PQntuples(res->pgsqlres);
	res->Fields=(DPS_SQLFIELD*)DpsMalloc(res->nCols*sizeof(DPS_SQLFIELD) + 1);
	if (res->Fields == NULL) return DPS_ERROR;
	for(i=0;i<res->nCols;i++){
		res->Fields[i].sqlname = (char*)DpsStrdup(PQfname(res->pgsqlres,(int)i));
	}
	return DPS_OK;
}


static int DpsPgSQLAsyncQuery(DPS_DB *db, DPS_SQLRES *res, const char *q) {
  size_t i, rc = 0;

  db->errcode = 0;
  for (i = 0; i < 3; i++) {
    if(!db->connected){
      DpsPgSQLInitDB(db);
      if(db->errcode) {
	PQfinish(db->pgsql);
	db->connected=0;
	res->pgsqlres = NULL;
	DPSSLEEP(20);
	continue;
      }
    }
	
    if (db->connected /*&& db->async_in_process*/) {
      /* Wait async query in progress */
      while ((res->pgsqlres = PQgetResult(db->pgsql))) {
	if (PQstatus(db->pgsql) == CONNECTION_BAD) { 
	  PQfinish(db->pgsql); /* db error occured, trying reconnect */
	  db->connected=0;
	  break;
	}
	if (PQresultStatus(res->pgsqlres)!=PGRES_COMMAND_OK) {
		if (PQerrorMessage(db->pgsql)) {
		  sprintf(db->errstr, "%s", PQerrorMessage(db->pgsql));
		} else {
		  sprintf(db->errstr, "<empty>, status: %d", PQresultStatus(res->pgsqlres));
		}
		if(!(strcasestr(db->errstr, "duplicate") /* en */
		   || strcasestr(db->errstr, "") /* ru */
		   || strcasestr(db->errstr, "duplizierter") /* de */
		   || strcasestr(db->errstr, "doppelter") /* de */
		   || strcasestr(db->errstr, "duplicada") /* es */
		   || strcasestr(db->errstr, "duplique") /* fr */
		   || strcasestr(db->errstr, "duplicirani") /* hr */
		   || strcasestr(db->errstr, "una chiave duplicata") /* it */
		   || strcasestr(db->errstr, "chave duplicada") /* pt-BR */
		     )) {
		  fprintf(stderr, "AsyncQuery Error\n\tPgSQL-server message: %s\n\n", db->errstr);
		}
	}
	PQclear(res->pgsqlres);
      }
      db->async_in_process = 0;
    }

    if(db->connected)
      rc = PQsendQuery(db->pgsql, q);
    if (rc) break;
    sprintf(db->errstr, "%s", PQerrorMessage(db->pgsql) ? PQerrorMessage(db->pgsql) : "<empty>");
    fprintf(stderr, "PgAsyncQuery error - %s\n", db->errstr);

    PQfinish(db->pgsql); /* db error occured, trying reconnect */
    db->connected = 0;
    DPSSLEEP(20);
  }
  if (!rc) return DPS_ERROR;
  db->async_in_process = 1;
  return DPS_OK;
}

#endif


/*****************************  miniSQL *******************************/
#if (HAVE_DP_MSQL)

static const char * msql_value(m_result *res,size_t i,size_t j){
	m_row row;

	if(i>=msqlNumRows(res))return(NULL);
	msqlDataSeek(res,(int)i);
	row=msqlFetchRow(res);
	if(row)return(row[j]?row[j]:"");
	else	return(0);
}
static int DpsMSQLInitDB(DPS_DB *db){
	char *host;
	
	/* To hide "never used" warning */
	if(msqlTypeNames[0][0]);
	
	if((host=db->addrURL.hostname))
		if(!strcmp(db->addrURL.hostname,"localhost"))
			host=NULL;
	if((db->msql=msqlConnect(host))<0){
		db->errcode=1;
		return(1);
	}
	if(msqlSelectDB(db->msql,db->DBName)==-1){
		db->errcode=1;
		return(1);
	}
	db->connected=1;
	return(0);
}
static void DpsMSQLDisplayError(DPS_DB *db){
	sprintf(db->errstr,"%s",msqlErrMsg);
}
static int safe_msql_query(DPS_DB *db,const char *query){
	int err;
	char * q;

	if(!(db->connected)){
		DpsMSQLInitDB(db);
		if(db->errcode)
			return(db->errcode);
	}
	q = (char*)DpsStrdup(query);
	if((err=msqlQuery(db->msql,q)==-1)){
		if(!strstr(msqlErrMsg,"Non unique")){
			DPS_FREE(q);
			db->errcode=1;
			return(err);
		}
		return(0);
	}
	DPS_FREE(q);
	return(0);
}
static m_result * DpsMSQLQuery(DPS_DB *db,const char *query){
	int err;

	db->errcode=0;
	if((err=safe_msql_query(db,query))){
		DpsMSQLDisplayError(db);
		return(NULL);
	}
	return(msqlStoreResult());
}
#endif


/************************** ODBC ***********************************/
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_SAPDB || HAVE_DB2)


#define SQL_OK(rc)	((rc==SQL_SUCCESS)||(rc==SQL_SUCCESS_WITH_INFO))

static int DpsODBCDisplayError(DPS_DB *db){
	UCHAR szSqlState[6];
	UCHAR szErrMsg[256];
	SDWORD naterr;
	SWORD length;
	RETCODE rc=SQL_SUCCESS;
	
	db->errstr[0]=0;
	while ((SQL_SUCCESS==rc) || (SQL_SUCCESS_WITH_INFO==rc)){
		rc = SQLError(db->hEnv,db->hDbc,db->hstmt,szSqlState,&naterr,szErrMsg,sizeof(szErrMsg),&length);
		dps_strcat(db->errstr, szErrMsg);
	}
	return(0);
}

static void execDB(DPS_DB*db, DPS_SQLRES *result, const char* sqlstr) {
	RETCODE rc;
	SWORD iResColumns;
	SDWORD iRowCount;
	int i,res_count;
	char* p;
	UCHAR szColName[32];
	SWORD pcbColName;
	SWORD pfSQLType;
	UDWORD pcbColDef;
	SWORD pibScale;
	SWORD pfNullable;
	SDWORD	pcbValue;
	static char	bindbuf[(int)(32L * 1024L - 16L)];

	/* -------- 
	p=sqlstr;
	while(*p){
		if(*p=='?')*p='.';
		p++;
	}
	*/
	
	if(!strcmp(sqlstr,"DPS_COMMIT")){
		rc=SQLTransact(db->hEnv,db->hDbc,SQL_COMMIT);
		if(!SQL_OK(rc))	db->errcode=1;
		else db->errcode=0;
		return;
	}

	rc=SQLAllocStmt(db->hDbc, &(db->hstmt));
	if (!SQL_OK(rc)){
		db->errcode=1;
		return;
	}
	rc=SQLExecDirect(db->hstmt,(SQLCHAR *)sqlstr, SQL_NTS);
	if (!SQL_OK(rc)){
		if(rc==SQL_NO_DATA) goto ND;
		db->errcode=1;
		return; 
	}
	rc=SQLNumResultCols(db->hstmt, &iResColumns);
	if(!SQL_OK(rc)){
		db->errcode=1;
		return;
	}
	if(!iResColumns) {
		rc=SQLRowCount(db->hstmt, &iRowCount);
		if (!SQL_OK(rc)){
			db->errcode=1;
			return;
		}
	}else{
		result->nRows = 0;
		result->nCols = iResColumns;
/*		result->items=NULL;*/

		rc = SQL_NO_DATA_FOUND;
		for (res_count=0;(db->res_limit?(res_count<db->res_limit):1);res_count++){
			rc=SQLFetch(db->hstmt);
			if (!SQL_OK(rc)) {
				if (rc!=SQL_NO_DATA_FOUND){
					db->errcode=1;
				}
				break;
			}
			if(!result->nRows){
				result->items=(char **)DpsXmalloc(1*iResColumns*sizeof(char*) + 1);
				if (result->items == NULL) return;
			}else{
				result->items=(char **)DpsXrealloc(result->items,((result->nRows+1)*iResColumns*sizeof(char*)));
				if (result->items == NULL) return;
			}
			for (i = 0; i < iResColumns; i++) {
				SQLDescribeCol(db->hstmt, i+1, szColName, sizeof(szColName),
					&pcbColName, &pfSQLType, &pcbColDef,&pibScale, &pfNullable);
				SQLGetData(db->hstmt,i+1,SQL_C_CHAR,bindbuf,sizeof(bindbuf),&pcbValue);
				if (pcbValue==SQL_NULL_DATA) p = NULL;
				else p = bindbuf;
				if(p)DpsRTrim(p," ");
				sql_val(result,result->nRows,i) = (char*)DpsStrdup(p?p:"");
			}
			result->nRows++;
		}
	}
ND:
	if(db->commit_fl==0){
		rc=SQLTransact(db->hEnv,db->hDbc,SQL_COMMIT);
		if(!SQL_OK(rc)){
			db->errcode=1;
			return;
		}
	}
	
	SQLFreeStmt(db->hstmt, SQL_DROP);
	
	db->res_limit=0;
	db->errcode=0;
	return;
}
static int DpsODBCInitDB(DPS_DB *db){
	char DSN[512]="";

#if (HAVE_SOLID)
	dps_snprintf(DSN,sizeof(DSN)-1,"tcp %s %d",db->addrURL.hostname?db->addrURL.hostname:"localhost",db->addrURL.port?db->addrURL.port:1313);
#elif (HAVE_SAPDB)
	dps_snprintf(DSN,sizeof(DSN)-1,"%s:%s",db->addrURL.hostname?db->addrURL.hostname:"localhost",db->DBName?db->DBName:"");
#else
	dps_strncpy(DSN,db->DBName?db->DBName:"",sizeof(DSN)-1);
#endif

	db->errcode = SQLAllocEnv( &(db->hEnv) );
	if( SQL_SUCCESS != db->errcode )return -2;
	db->errcode = SQLAllocConnect( db->hEnv, &(db->hDbc) );
	if( SQL_SUCCESS != db->errcode )return -3;
	db->errcode = SQLSetConnectOption( db->hDbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
	if( SQL_SUCCESS != db->errcode )return -4;
	db->errcode = SQLConnect( db->hDbc, DSN, SQL_NTS, db->DBUser?(db->DBUser[0]?db->DBUser:NULL):NULL, SQL_NTS, db->DBPass?(db->DBPass[0]?db->DBPass:NULL):NULL, SQL_NTS);
	if( !SQL_OK(db->errcode)) return(-5);
	else db->errcode=0;
	db->connected=1;
	return 0;
}

static void DpsODBCCloseDB(DPS_DB *db){
	if(db->connected){
		db->connected=0;
		db->errcode = SQLTransact( db->hEnv, db->hDbc, SQL_COMMIT);
		if( SQL_SUCCESS != db->errcode )return;
		db->errcode = SQLDisconnect( db->hDbc );
		if( SQL_SUCCESS != db->errcode )return;
		db->errcode = SQLFreeConnect( db->hDbc );
		if( SQL_SUCCESS != db->errcode )return;
		else	db->hDbc = SQL_NULL_HDBC;
		db->errcode = SQLFreeEnv( db->hEnv );
		if( SQL_SUCCESS != db->errcode )return;
		else	db->hEnv = SQL_NULL_HENV;
	}
}

static int DpsODBCQuery(DPS_DB *db, DPS_SQLRES *res, const char *qbuf) {
	int rc = DPS_OK;

	if(!db->connected){
		DpsODBCInitDB(db);
		if(db->errcode){
			DpsODBCDisplayError(db);
			return DPS_ERROR;
		}else{
			db->connected=1;
		}
	}
	execDB(db, res, qbuf);
	if((db->errcode)){
		DpsODBCDisplayError(db);
		if(strstr(db->errstr,"uplicat")){ /* PgSQL,MySQL*/
			db->errcode=0;
		}else
		if(strstr(db->errstr,"nique")){ /* Solid, Virtuoso */
			db->errcode=0;
		}
		else if(strstr(db->errstr,"UNIQUE")){ /* Mimer */
			db->errcode=0;
		}else{
			db->errcode=1;
			rc = DPS_ERROR;
		}
		SQLFreeStmt(db->hstmt, SQL_DROP);
	}
	return rc;
}

#endif

/*******************************************************/

#if HAVE_IBASE

#define SQL_VARCHAR(len) struct {short vary_length; char vary_string[(len)+1];}
typedef struct {
	short	len;
	char	str[1];
} DPS_IBASE_VARY;

static void DpsIBaseDisplayError(DPS_DB *db){
	char * s = db->errstr;
	ISC_STATUS * ibstatus=db->status;
	
	while(isc_interprete(s ,&ibstatus)){
		dps_strcat(s," ");
		s=db->errstr+dps_strlen(db->errstr);
	}
}

static int DpsIBaseInitDB(DPS_DB *db){
	char dpb_buffer[256], *dpb, *p, *e;
	int dpb_length, len;
	char connect_string[256];

	dpb = dpb_buffer;
	*dpb++ = isc_dpb_version1;

	if (db->DBUser != NULL && (len = dps_strlen(db->DBUser))) {
		*dpb++ = isc_dpb_user_name;
		*dpb++ = len;
		for (p = db->DBUser; *p;) {
			*dpb++ = *p++;
		}
	}
	if (db->DBPass != NULL && (len = dps_strlen(db->DBPass))) {
		*dpb++ = isc_dpb_password;
		*dpb++ = dps_strlen(db->DBPass);
		for (p = db->DBPass; *p;) {
			*dpb++ = *p++;
		}
	}
	/*
	if (charset != NULL && (len = dps_strlen(charset))) {
		*dpb++ = isc_dpb_lc_ctype;
		*dpb++ = dps_strlen(charset);
		for (p = charset; *p;) {
			*dpb++ = *p++;
		}
	}
#ifdef isc_dpb_sql_role_name
	if (role != NULL && (len = dps_strlen(role))) {
		*dpb++ = isc_dpb_sql_role_name;
		*dpb++ = dps_strlen(role);
		for (p = role; *p;) {
			*dpb++ = *p++;
		}
	}
#endif
	*/

	dpb_length = dpb - dpb_buffer;
	
	if(strcmp(db->addrURL.hostname,"localhost"))
		dps_snprintf(connect_string,sizeof(connect_string)-1,"%s:%s",db->addrURL.hostname, db->DBName);
	else
		dps_snprintf(connect_string,sizeof(connect_string)-1,"%s",db->DBName);
	
	/* Remove possible trailing slash */
	e= connect_string+dps_strlen(connect_string);
	if (e>connect_string && e[-1]=='/')
		e[-1]='\0';
	
#ifdef DEBUG_SQL
	fprintf(stderr, "SQL Connect to: '%s'\n",connect_string);
#endif	
	if(isc_attach_database(db->status, dps_strlen(connect_string), connect_string, &(db->DBH), dpb_length, dpb_buffer)){
		db->errcode=1;
		return(1);
	}
	return(0);
}
static void DpsIBaseCloseDB(DPS_DB*db){
	if(db->connected){
		if (isc_detach_database(db->status, &(db->DBH))){
			db->errcode=1;
		}
	}
}

static void sql_ibase_query(DPS_DB *db, DPS_SQLRES *result, const char *query) {
	ISC_STATUS	status[20]; /* To not override db->status */
	isc_stmt_handle	query_handle = NULL;
	char		query_info[] = { isc_info_sql_stmt_type };
	char		info_buffer[18];
	long		query_type;
	int		autocommit=1;
	char		shortdata[64];
	short		sqlind_array[128];
	
	if(!db->connected){
		DpsIBaseInitDB(db);
		if(db->errcode){
			DpsIBaseDisplayError(db);
			db->errcode=1;
			return;
		}else{
			db->connected=1;
		}
	}
	
	if(!strcmp(query,"BEGIN")){
		if(!db->tr_handle){
			if (isc_start_transaction(db->status, &db->tr_handle, 1, &(db->DBH), 0, NULL))
				db->errcode=1;
		}else{
			db->errcode=1;
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"Wrong calls order: begin");
		}
		return;
	}
	
	if(!strcmp(query,"COMMIT")){
		if(db->tr_handle){
			if (isc_commit_transaction(db->status, &db->tr_handle))
				db->errcode=1;
			
			db->tr_handle=NULL;
		}else{
			db->errcode=1;
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"Wrong calls order: commit");
		}
		return;
	}
	
	if(!db->tr_handle){
		if (isc_start_transaction(db->status, &db->tr_handle, 1, &(db->DBH), 0, NULL)){
			db->errcode=1;
			return;
		}
		autocommit=1;
	}else{
		autocommit=0;
	}
	
	if (isc_dsql_allocate_statement(db->status, &(db->DBH), &query_handle)){
		db->errcode=1; 
		return;
	}
	if (isc_dsql_prepare(db->status, &db->tr_handle, &query_handle, 0, query, 1, NULL)){
		db->errcode=1; 
		return;
	}
	if (!isc_dsql_sql_info(db->status, &query_handle, sizeof(query_info), query_info, sizeof(info_buffer), info_buffer)) {
		short l;
		l = (short) isc_vax_integer((char ISC_FAR *) info_buffer + 1, 2);
		query_type = isc_vax_integer((char ISC_FAR *) info_buffer + 3, l);
	}
	
	/* Find out what kind of query is to be executed */
	if (query_type == isc_info_sql_stmt_select || query_type == isc_info_sql_stmt_select_for_upd) {
		XSQLDA *osqlda=NULL;
		long fetch_stat;
		int i;
		
		/*
		 * Select, need to allocate output sqlda and and prepare it for use.
		 */
		osqlda = (XSQLDA *) DpsXmalloc(XSQLDA_LENGTH(0));
		if (osqlda == NULL) {
		        db->errcode=1;
			isc_rollback_transaction(status, &db->tr_handle);
			return;
		}
		osqlda->sqln = 0;
		osqlda->version = SQLDA_VERSION1;
		
		/* Fetch column information */
		if (isc_dsql_describe(db->status, &query_handle, 1, osqlda)) {
			DPS_FREE(osqlda);
			db->errcode=1;
			isc_rollback_transaction(status, &db->tr_handle);
			return;
		}
		
		if (osqlda->sqld) {
			osqlda = (XSQLDA *) DpsXrealloc(osqlda, XSQLDA_LENGTH(osqlda->sqld));
			if (osqlda == NULL) {
				db->errcode=1;
				isc_rollback_transaction(status, &db->tr_handle);
				return;
			}
			osqlda->sqln = osqlda->sqld;
			osqlda->version = SQLDA_VERSION1;
			if (isc_dsql_describe(db->status, &query_handle, 1, osqlda)) {
				DPS_FREE(osqlda);
				db->errcode=1;
				isc_rollback_transaction(status, &db->tr_handle);
				return;
			}
		}
		
		result->nCols = osqlda->sqld;
		result->Fields= (DPS_SQLFIELD*)DpsXmalloc(result->nCols*sizeof(DPS_SQLFIELD) + 1);
		if (result->Fields == NULL) {
		  db->errcode=1;
		  isc_rollback_transaction(status, &db->tr_handle);
		  return;
		}
		
		for (i = 0; i < osqlda->sqld; i++) {
			XSQLVAR *var=&osqlda->sqlvar[i];
			int coltype = osqlda->sqlvar[i].sqltype & ~1;
			
			osqlda->sqlvar[i].sqlind=&sqlind_array[i];
			osqlda->sqlvar[i].sqlind[0]=0;
			result->Fields[i].sqlname = (char*)DpsStrdup(osqlda->sqlvar[i].sqlname);
			result->Fields[i].sqllen  = osqlda->sqlvar[i].sqllen;
			
			
			switch(coltype){
				case SQL_SHORT:
					var->sqldata = (char*)DpsMalloc(sizeof(short));
					break;
				case SQL_LONG:
					var->sqldata = (char*)DpsMalloc(sizeof(long));
					break;
				case SQL_FLOAT:
					var->sqldata = (char*)DpsMalloc(sizeof(float));
					break;
				case SQL_DOUBLE:
					var->sqldata = (char*)DpsMalloc(sizeof(double));
					break;
				case SQL_DATE:
				case SQL_BLOB:
				case SQL_ARRAY:
					var->sqldata = (char*)DpsMalloc(sizeof(ISC_QUAD));
					break;
				case SQL_TEXT:
					var->sqldata = (char*)DpsMalloc((size_t)(osqlda->sqlvar[i].sqllen));
					break;
				case SQL_VARYING:
					osqlda->sqlvar[i].sqldata = (char*)DpsMalloc((size_t)(osqlda->sqlvar[i].sqllen+sizeof(short)));
					break;
			}
		}
		if (isc_dsql_execute(db->status, &db->tr_handle, &query_handle, 1, NULL)) {
			DPS_FREE(osqlda);
			db->errcode=1;
			isc_rollback_transaction(status, &db->tr_handle);
			return;
		}
		
		while ((fetch_stat = isc_dsql_fetch(db->status, &query_handle, 1, osqlda)) == 0){
			result->items=(char **)DpsRealloc(result->items,(result->nRows+1)*(result->nCols)*sizeof(char*));
			if (result->items == NULL) {
			  DPS_FREE(osqlda);
			  db->errcode=1;
			  isc_rollback_transaction(status, &db->tr_handle);
			  return;
			}
			
			for(i=0;i<osqlda->sqld; i++){
				DPS_IBASE_VARY *vary;
				XSQLVAR *var=osqlda->sqlvar+i;
				char *p=NULL;
				
				if(*var->sqlind==-1)
					/* NULL data */
					p = (char*)DpsStrdup("");
				else
				switch(var->sqltype & ~1){
				case SQL_TEXT:
					p=(char*)DpsMalloc((size_t)(var->sqllen+1));
					dps_strncpy(p,(char*)var->sqldata,(size_t)(var->sqllen));
					p[var->sqllen]='\0';
					break;
				case SQL_VARYING:
					vary=(DPS_IBASE_VARY*)var->sqldata;
					p=(char*)DpsMalloc((size_t)(vary->len+1));
					dps_strncpy(p,vary->str,(size_t)(vary->len));
					p[vary->len]='\0';
					break;
				case SQL_LONG:
					sprintf(shortdata,"%ld",*(long*)(var->sqldata));
					p = (char*)DpsStrdup(shortdata);
					break;
				case SQL_SHORT:
					sprintf(shortdata,"%d",*(short*)(var->sqldata));
					p = (char*)DpsStrdup(shortdata);
					break;
				case SQL_FLOAT:
					sprintf(shortdata,"%f",*(float*)(var->sqldata));
					p = (char*)DpsStrdup(shortdata);
					break;
				case SQL_DOUBLE:
					sprintf(shortdata,"%f",*(double*)(var->sqldata));
					p = (char*)DpsStrdup(shortdata);
					break;
				default:
					sprintf(shortdata,"Unknown SQL type");
					p = (char*)DpsStrdup(shortdata);
					break; 
				}
				DpsRTrim(p," ");
				result->items[result->nRows*result->nCols+i]=p;
			}
			result->nRows++;
		}
		/* Free fetch buffer */
		for (i = 0; i < osqlda->sqld; i++) {
			DPS_FREE(osqlda->sqlvar[i].sqldata);
		}
		DPS_FREE(osqlda);
		
		if (fetch_stat != 100L){
			db->errcode=1; 
			isc_rollback_transaction(status, &db->tr_handle);
			return;
		}
		
	} else {
		/* Not select */
		if (isc_dsql_execute(db->status, &db->tr_handle, &query_handle, 1, NULL)) {
			db->errcode=1;
			isc_rollback_transaction(status, &db->tr_handle);
			isc_dsql_free_statement(status, &query_handle, DSQL_drop);
			return;
		}
/*		result=NULL;*/
	}
	if (isc_dsql_free_statement(db->status, &query_handle, DSQL_drop)){
		db->errcode=1; 
		isc_rollback_transaction(status, &db->tr_handle);
		return;
	}
	if(autocommit){
		if (isc_commit_transaction(db->status, &db->tr_handle)){
			db->errcode=1; 
			db->tr_handle=NULL;
			return;
		}
		db->tr_handle=NULL;
	}
	return;
}

static int DpsIBaseQuery(DPS_DB *db, DPS_SQLRES *res, const char *query) {

	sql_ibase_query(db, res, query);
	if(db->errcode){
	
		DpsIBaseDisplayError(db);
	
		if(strstr(db->errstr,"uplicat") || strstr(db->errstr,"UNIQUE")){
			db->errcode=0;
			return DPS_OK;
		}else{
			/*dps_strcat(db->errstr," ");
			dps_strcat(db->errstr,query);*/
		}
		return DPS_ERROR;
	}
	return DPS_OK;
}

#endif


/******************** Oracle8 OCI - native support driver ******************/

#if HAVE_ORACLE8

/* (C) copyleft 2000 Anton Zemlyanov, az@hotmail.ru */
/* TODO: efficient transactions, multi-row fetch, limits stuff */




#define SQL_OK(rc)	((rc==OCI_SUCCESS)||(rc==OCI_SUCCESS_WITH_INFO))

static int oci_free_result(DPS_SQLRES *res)
{
	int i;
	if(res){
		if(res->items){
			for(i=0;i<res->nCols*res->nRows;i++)
				if(res->items[i])
					DPS_FREE(res->items[i]);
			DPS_FREE(res->items);
		}
		if(res->defbuff){
			for(i=0;i<res->nCols;i++)
				if(res->defbuff[i])
					DPS_FREE(res->defbuff[i]);
		}
	}
	return(0);
}

static int DpsOracle8DisplayError(DPS_DB *db)
{
	sb4	errcode=0;
	text	errbuf[512];
	char	*ptr;

	db->errstr[0]='\0';

	switch (db->errcode)
	{
	case OCI_SUCCESS:
		sprintf(db->errstr,"Oracle - OCI_SUCCESS");
		break;
	case OCI_SUCCESS_WITH_INFO:
		sprintf(db->errstr,"Oracle - OCI_SUCCESS_WITH_INFO");
		break;
	case OCI_NEED_DATA:
		sprintf(db->errstr,"Oracle - OCI_NEED_DATA");
		break;
	case OCI_NO_DATA:
		sprintf(db->errstr,"Oracle - OCI_NODATA");
		break;
	case OCI_ERROR:
		OCIErrorGet((dvoid *)db->errhp, (ub4) 1, (text *) NULL, &errcode,
			errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		ptr=errbuf;
		while(*ptr) {
			if(*ptr==NL_CHAR)
				*ptr='!';
			++ptr;
		}
		sprintf(db->errstr,"Oracle - %.*s", 512, errbuf);
		break;
	case OCI_INVALID_HANDLE:
		sprintf(db->errstr,"Oracle - OCI_INVALID_HANDLE");
		break;
	case OCI_STILL_EXECUTING:
		sprintf(db->errstr,"Oracle - OCI_STILL_EXECUTE");
		break;
	case OCI_CONTINUE:
		sprintf(db->errstr,"Oracle - OCI_CONTINUE");
		break;
	default:
		sprintf(db->errstr,"Oracle - unknown internal bug");
		break;
	}
	return 0;
}


static void DpsOracle8CloseDB(DPS_DB *db)
{
	if(!db->connected)
		return;
	db->errcode=OCILogoff(db->svchp,db->errhp);
	db->connected=0;
}

static int oci_bind(DPS_DB *db){
	int i,pos;
	sword   rc;

        for (i=0; i<MAX_BIND_PARAM; i++)
                db->bndhp[i] = (OCIBind *) 0;

        for( i=0; i<MAX_BIND_PARAM; i++){
                if ((pos=db->par->out_pos[i])>0){
                        if ( (rc = OCIBindByPos(db->stmthp, &db->bndhp[pos-1], db->errhp, (ub4) pos,
				(dvoid *) &db->par->out_pos_val[pos-1][0],
                                (sb4) sizeof(db->par->out_pos_val[pos-1][0]), SQLT_INT,
				(dvoid *) 0, (ub2 *)0, (ub2 *)0,
                                (ub4) 0, (ub4 *) 0, (ub4) OCI_DEFAULT))){

                                return rc;
                        }
                        /*  bind array  */
                        if ((rc = OCIBindArrayOfStruct(db->bndhp[pos-1], db->errhp,                                
				sizeof(db->par->out_pos_val[pos-1][0]),0, 0, 0))){

                                return rc;
                        }
                }
        }
        return 0;
}

static void param_free(DPS_DB *db){
        db->par->out_rec = 0;
}


static int DpsOracle8InitDB(DPS_DB *db){

	OCIInitialize( OCI_DEFAULT, NULL, NULL, NULL, NULL );
	OCIEnvInit( &(db->envhp), OCI_DEFAULT, 0, NULL );
	OCIHandleAlloc( db->envhp, (dvoid **)&(db->errhp), OCI_HTYPE_ERROR,0, NULL );
	db->errcode=OCILogon(db->envhp, db->errhp, &(db->svchp),
		db->DBUser, dps_strlen(db->DBUser),
		db->DBPass, dps_strlen(db->DBPass),
		db->DBName, dps_strlen(db->DBName) );
	if(db->errcode!=OCI_SUCCESS)
		return -1;

	db->errcode=0;
	db->connected=1;
	db->commit_fl=0;
	param_free(db);

/*        sql_query(((DPS_DB*)(db)), "ALTER SESSION SET SQL_TRACE=TRUE"); */

	return 0;
}

static DPS_SQLRES* sql_oracle_query(DPS_DB *db, char *qbuf)
{
	DPS_SQLRES *result;
	sword	rc;
	ub2	stmt_type;
	int	cnt, buf_nRows, row;

	int	colcnt;
	ub2	coltype;
	ub2	colsize;
	int	oci_fl;
	sb4	errcode=0;
	text	errbuf[512];
	int	num_rec;

	db->errcode=0;
	rc=OCIHandleAlloc(db->envhp, (dvoid *)&(db->stmthp), OCI_HTYPE_STMT,0,NULL);
	if(!SQL_OK(rc)){
		db->errcode=rc;
		return NULL;
	}
	rc=OCIStmtPrepare(db->stmthp,db->errhp,qbuf,dps_strlen(qbuf),
			OCI_NTV_SYNTAX,OCI_DEFAULT);
	if(!SQL_OK(rc)){
		db->errcode=rc;
		return NULL;
	}

        if (db->par->out_rec){
                oci_bind(db);
                num_rec = db->par->out_rec;
                param_free(db);
        }else{
                num_rec = 1;
        }
    
	rc=OCIAttrGet(db->stmthp,OCI_HTYPE_STMT,&stmt_type,0,
			OCI_ATTR_STMT_TYPE,db->errhp);
	if(!SQL_OK(rc)){
		db->errcode=rc;
		return NULL;
	}

	if(stmt_type!=OCI_STMT_SELECT) {
		/* non-select statements */
		/* COMMIT_ON_SUCCESS in inefficient */
		if (db->commit_fl)
			oci_fl=OCI_DEFAULT;
		else
			oci_fl=OCI_COMMIT_ON_SUCCESS;

		rc=OCIStmtExecute(db->svchp,db->stmthp,db->errhp, num_rec,0,
				NULL,NULL,oci_fl);

		if (num_rec>1)
			  (void) OCITransCommit(db->svchp, db->errhp, (ub4) 0);

		if(!SQL_OK(rc)){
			db->errcode=rc;
			OCIErrorGet((dvoid *)db->errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
			if(strncmp(errbuf,"ORA-00001",9)) /* ignore ORA-00001 */
				return NULL;
			else 
				db->errcode=OCI_SUCCESS;
		}
		/* OCI_ATTR_ROW_COUNT of db->stmthp - Rows affected */
		rc=OCIHandleFree(db->stmthp,OCI_HTYPE_STMT);
		if(!SQL_OK(rc)){
			db->errcode=rc;
			return NULL;
		}
		return NULL;
	}

	/* select statements */
	/*Allocate result Set*/
	result=(DPS_SQLRES*)DpsXmalloc(sizeof(DPS_SQLRES));
	bzero((void*)result,sizeof(*result));

	rc=OCIStmtExecute(db->svchp,db->stmthp,db->errhp,0,0,
			NULL,NULL,OCI_DEFAULT);
	if(!SQL_OK(rc)){
		db->errcode=rc;
		return NULL;
	}

	/*describe the select list and define buffers for the select list */
	colcnt=0;
	while(OCIParamGet(db->stmthp,OCI_HTYPE_STMT,db->errhp,(dvoid *)&(db->param),
			colcnt+1 ) == OCI_SUCCESS) {
		
		ub4 str_len;
		text *namep;
		
		result->Fields=(DPS_SQLFIELD*)DpsRealloc(result->Fields,(colcnt+1)*sizeof(DPS_SQLFIELD));
		bzero((void*)&result->Fields[colcnt],sizeof(DPS_SQLFIELD));
						
		rc=OCIAttrGet(db->param,OCI_DTYPE_PARAM,(dvoid*) &namep, (ub4*) &str_len,
				(ub4) OCI_ATTR_NAME, db->errhp);
		if(!SQL_OK(rc)){
			db->errcode=rc;
			return NULL;
		}
		namep[str_len]= '\0';
		result->Fields[colcnt].sqlname = (char*)DpsStrdup(namep);
		
		rc=OCIAttrGet(db->param,OCI_DTYPE_PARAM,&(coltype),0,
				OCI_ATTR_DATA_TYPE,db->errhp);
		if(!SQL_OK(rc)){
			db->errcode=rc;
			return NULL;
		}
		rc=OCIAttrGet(db->param,OCI_DTYPE_PARAM,&colsize,0,
			OCI_ATTR_DATA_SIZE,db->errhp);
		if(!SQL_OK(rc)){
			db->errcode=rc;
			return NULL;
		}

		/* OCIStmtFetch do not terminate data with \0 -
		add a byte for terminator to define buffers - insurance*/

		switch(coltype) {
		case SQLT_CHR: /* variable length string */
		case SQLT_AFC: /* fixed length string */
                        result->col_size[colcnt]=colsize;
                        result->defbuff[colcnt]=(char *)DpsXmalloc(result->col_size[colcnt]*BUF_OUT_SIZE+1);
			break;
		case SQLT_NUM: /* numbers up to 14 digits now */
                        result->col_size[colcnt]=14;
                        result->defbuff[colcnt]=(char *)DpsXmalloc(result->col_size[colcnt]*BUF_OUT_SIZE+1);
			break;
		default:
			printf("<P>Unknown datatype: %d\n",coltype);
			return (DPS_SQLRES*)NULL;
		}
		rc=OCIDefineByPos(db->stmthp,&(db->defb[colcnt]),db->errhp,
			colcnt+1,result->defbuff[colcnt],result->col_size[colcnt],SQLT_CHR,
			&(result->indbuff[colcnt]),0,0,OCI_DEFAULT);
		if(!SQL_OK(rc)){
			db->errcode=rc;
	 		return NULL;
		}
                rc=OCIDefineArrayOfStruct(db->defb[colcnt], db->errhp,
                         result->col_size[colcnt], sizeof(result->indbuff[0][0]), 0, 0);
		if(!SQL_OK(rc)){
                        db->errcode=rc;
                        return NULL;
                }
		colcnt++;
	}
	result->nCols=colcnt;

	/* Now fetching the selected rows into the memory */
	while(1) {
		if (db->res_limit)
			if (db->res_limit == result->nRows)
				break;
                /*Fix me: Process of determine real fetched rows
                 Fill indicator buffer for value -6 to */
                for(row=0; row<BUF_OUT_SIZE; row++)
                        result->indbuff[result->nCols-1][row]=-6;

		rc=OCIStmtFetch(db->stmthp,db->errhp,BUF_OUT_SIZE,
			OCI_FETCH_NEXT,OCI_DEFAULT);

                if((rc!=OCI_NO_DATA) && !SQL_OK(rc)) {
                        db->errcode=rc;
                        return NULL;
                }

                /* Find number of fetched rows */
                for(buf_nRows=0; buf_nRows<BUF_OUT_SIZE; buf_nRows++){
                        if(result->indbuff[result->nCols-1][buf_nRows]==-6)
                                break;
                }

                if(!buf_nRows)
                        break;

		if(!result->nRows)  /* first row - allocate */
			result->items=(char **)DpsXmalloc(buf_nRows*result->nCols*sizeof(char*) + 1);
		else
			result->items=(char **)DpsXrealloc(result->items,
				((result->nRows+buf_nRows + 1) * result->nCols*sizeof(char*)) );
		/* Limit rows */	
	        if (db->res_limit)
            		buf_nRows=result->nRows+buf_nRows<=db->res_limit?buf_nRows:(db->res_limit-result->nRows);
		for(row=0; row<buf_nRows; row++){
			for(cnt=0; cnt<result->nCols; ++cnt) {
                    		if(result->indbuff[cnt][row]==OCI_IND_NULL) {
                            		sql_val(result,(result->nRows+row),cnt) = (char*)DpsStrdup("");
                                }else{
                                        char *val=DpsXmalloc(result->col_size[cnt]+1);
                                        int offset=row*result->col_size[cnt];

                                        dps_snprintf(val, result->col_size[cnt]+1, "%s",
						result->defbuff[cnt]+offset);
                                        sql_val(result,(result->nRows+row),cnt) = (char*)DpsStrdup( DpsRTrim(val," "));
                                        DPS_FREE(val);
                                }
                        }
                }
		result->nRows+=row;
		if(rc==OCI_NO_DATA)
                        break;
                if(!SQL_OK(rc)) {
                        db->errcode=rc;
                        return NULL;
                }

	}
	rc=OCIHandleFree(db->stmthp,OCI_HTYPE_STMT);
	db->res_limit = 0;
	if(!SQL_OK(rc)){
		db->errcode=rc;
		return NULL;
	}

	return (DPS_SQLRES*)result;
}

static DPS_SQLRES* DpsOracle8Query(DPS_DB *db, const char *qbuf) {
	DPS_SQLRES *res;

	if(!db->connected) {
		DpsOracle8InitDB(db);
		if(db->errcode) {
			DpsOracle8DisplayError(db);
			return NULL;
		} else {
			db->connected=1;
		}
 	}
	res=sql_oracle_query(db,qbuf);

	if(db->errcode) {
		DpsOracle8DisplayError(db);
		return NULL;
	}
	return (DPS_SQLRES*)res;
}

#endif



/******************** Oracle7 OCI - native support driver ******************/

#if HAVE_ORACLE7

 * (C) 2000 Kir Maximov, maxkir@email.com 
 * 
 * Without transactions ...
 */

#define MAX_COLS_IN_TABLE    32
#define HDA_SIZE 256
#define MAX_ITEM_BUFFER_SIZE 32

#define DPS_OCI_UNIQUE 1
#define ORA_VERSION_7 2

/*  some SQL and OCI function codes */
#define FT_INSERT                3
#define FT_SELECT                4
#define FT_UPDATE                5
#define FT_DELETE                9

/* Declare structures for query information. */
struct describe
{
    sb4             dbsize;
    sb2             dbtype;
    sb1             buf[MAX_ITEM_BUFFER_SIZE];
    sb4             buflen;
    sb4             dsize;
};

struct define 
{
    ub1             buf[1024*5];	/* FIXME unsafe */
    sb2             indp;
    ub2             col_retlen, col_retcode;
};


/*-------------------------------------------------------*/
static int oci_free_result(DPS_SQLRES *res)
{
    int i;

    if(res)
    {
        if(res->items)
        {
            for(i=0;i<res->nCols*res->nRows;i++)
            {
                if(res->items[i])
                    DPS_FREE(res->items[i]);
            }
            DPS_FREE(res->items);
        }
    }
    return(0);
}
/*-------------------------------------------------------*/
static int DpsOracle7DisplayError(DPS_DB *db)
{
    sword n;
    text msg[512];

    db->errstr[0]='\0';
    if (!db->errcode) 
        return 0; 

    n = oerhms(&db->lda, db->cursor.rc, msg, (sword) sizeof msg);
    
    dps_strcpy(db->errstr, "ORACLE - ");
    dps_strcat(db->errstr, msg);

    printf("displayError: %p %s\n", db, msg);

    return 0;
}
/*-------------------------------------------------------*/
static int DpsOracle7InitDB(DPS_DB *db)
{
    Cda_Def *cda = &db->cursor;
    Lda_Def *lda = &db->lda;
    char buff[1024];

    dps_snprintf(buff, sizeof(buff)-1, "%s/%s@%s",db->DBUser,db->DBPass,db->DBName);
    
    db->errcode = olog(lda, db->hda,buff, -1,(text *)0, -1,0, -1, OCI_LM_DEF);

    if ( db->errcode )
    {
        /* Error happened */
        return -1;
    }

    /* Now, open a cursor to be used in all queries*/
    oopen(cda, lda, (text*)0, -1, -1, (text*)0, -1);
    db->errcode = cda->rc;
    if (db->errcode)
    {
        DpsOracle7DisplayError(db);
        return 0;
    }
    
    db->errcode = 0;
    db->connected = 1;

    return 0;
}
/*-------------------------------------------------------*/
static void DpsOracle7CloseDB(DPS_DB *db)
{
    if(!db->connected)
        return;
    
    /* First, close a cursor */
    oclose(&db->cursor);

    /* Logout */
    db->errcode = ologof(&db->lda);
    db->connected = 0;
}
/*-------------------------------------------------------*/
static DPS_SQLRES* fetch_data(DPS_DB *db)
{
    /* Real feching of data for SELECT statements */
    DPS_SQLRES *res = NULL;
    int col, deflen;
    struct describe desc[MAX_COLS_IN_TABLE];
    struct define   def[MAX_COLS_IN_TABLE];
    Cda_Def *cda = &db->cursor;
    char *buf_ptr;
   

    /* Now, get column desriptions */
    for(col = 0; col < MAX_COLS_IN_TABLE; col ++)
    {
        desc[col].buflen = MAX_ITEM_BUFFER_SIZE;
        if ( odescr( cda, col+1, &desc[col].dbsize,
                     &desc[col].dbtype, desc[col].buf,
                     &desc[col].buflen, &desc[col].dsize,
                     0,0,0
                   )
           )
        {
            if ( cda->rc == 1007 ) /* No more variables */
                break;
            db->errcode = cda->rc;
            if (db->errcode)
            {
                DpsOracle7DisplayError(db);
                return NULL;
            }
            
        }
        
        switch(desc[col].dbtype)
        {
        case 2: /* NUMBER */
            deflen = 14;
            break;
        case 96: /* CHAR */
        case 1:  /* VARCHAR2 */
        default:
            deflen = (desc[col].dbsize > 1024 ? 1024 : desc[col].dbsize + 1);
        }
       
        if (odefin(cda, col + 1,
                    def[col].buf, deflen, 5, /* 5 - null terminated string */
                    -1, &def[col].indp, (text*)0, -1, -1,
                    &def[col].col_retlen,
                    &def[col].col_retcode ))
        {
            db->errcode = cda->rc;
            if (db->errcode)
            {
                DpsOracle7DisplayError(db);
                return NULL;
            }
        }
    }
    /* Now col contains a number of columns */

    /* Get memory for resulting data */
    res = (DPS_SQLRES*)DpsXmalloc(sizeof(DPS_SQLRES));
    if (!res)
        return NULL;

    /* Clear all structure data */
    memset(res, 0, sizeof(DPS_SQLRES));

    res->nCols = col;

    /* Now, fetching the data */
    for (;;)
    {
        /* Fetch a row, break on end of fetch, */
        /* disregard null fetch "error" */
        
        if (ofetch(cda))
        {
            if ( cda->rc == 1403 ) /* No data found */
                break;
            if ( cda->rc != 1405 && cda->rc ) /* Null value returned */
            {
               SQL_FREE(res);
               db->errcode = cda->rc;
               DpsOracle7DisplayError(db);
               return NULL;
            }
        }
        /* Next row: */
        
        /* [Re]allocate memory for data */
        if (!res->items)
        {
	  res->items = (char**) DpsXmalloc( (res->nCols + 1) * sizeof(char*));
        }
        else
        {
            res->items = (char**) DpsXrealloc(res->items, 
                             (res->nRows + 1) * res->nCols * sizeof(char*));
        }
                
        for (col = 0; col < res->nCols; col++)
        {
            if (def[col].indp < 0) /* NULL value */
            {
                buf_ptr = (char*)DpsStrdup("");
            }
            else
            {
                buf_ptr = (char*)DpsStrdup((char*)def[col].buf);
                DpsRTrim(buf_ptr, " ");
            }
            /* sql_val(res, res->nRows, col)*/
            res->items[res->nRows*res->nCols + col] = buf_ptr;
            /*printf( "\n<P>Field %d Val '%s' Null '%d'",col,sql_value(res,res->nRows,col), def[col].indp );*/

        }
        res->nRows ++;
    }

    return res;    
}
/*-------------------------------------------------------*/
static DPS_SQLRES* sql_oracle_query(DPS_DB *db, char *query)
{
    /* Make real query and store result */
    DPS_SQLRES* res = NULL;
    Cda_Def *cda = &db->cursor;
    
    /* Parse SQL statement */
    oparse(cda, (text*) query, (sb4)-1, (sword) 0, (ub4)ORA_VERSION_7 );
    if ( cda->rc )
    {
        db->errcode = cda->rc;
        DpsOracle7DisplayError(db);
        return NULL;
    }
    /* Save sql function */

    /* Exec */
    oexec(cda);
    if (cda->rc)
    {
        if (cda->rc != DPS_OCI_UNIQUE)  /* ignore unique constraint violation */
        {
            DpsOracle7DisplayError(db);
            return NULL;
        }
        db->errcode = 0;
    }
    
    switch (cda->ft)
     {
        case FT_DELETE:
        case FT_INSERT:
        case FT_UPDATE:
            /* Empty result */
            res = NULL;

            /* commit */
            ocom(&db->lda);

            break;
        case FT_SELECT:
            /* Fetch data needed */
            res = fetch_data(db);
            break;
        default:
            res = NULL;
            break;
    }
    
    return res;
}

/*-------------------------------------------------------*/
static DPS_SQLRES* DpsOracle7Query(DPS_DB *db, char *qbuf) 
{
    DPS_SQLRES *res;

    if(!db->connected) 
    {
        DpsOracle7InitDB(db);
        if(db->errcode) 
        {
            DpsOracle7DisplayError(db);
            return NULL;
        }
    }
    
    /* Make real query */
    res = sql_oracle_query(db, qbuf);
    
    if(!res) 
    {
        if (db->errcode)
        {
            DpsOracle7DisplayError(db);
        }
        else
        {
            fprintf(stderr, "sql_query: this should never happen.\n");   
        }
    }
    return res;
}

#endif



/***********************************************************************/

#if HAVE_CTLIB

#ifndef MAX
#define MAX(X,Y)	(((X) > (Y)) ? (X) : (Y))
#endif

#ifndef MIN
#define MIN(X,Y)	(((X) < (Y)) ? (X) : (Y))
#endif

#define MAX_CHAR_BUF	1024

typedef struct _ex_column_data
{
	CS_INT		indicator;
	CS_CHAR		*value;
	CS_INT		valuelen;
} EX_COLUMN_DATA;

static char sybsrvmsg[2048]="";

static CS_RETCODE CS_PUBLIC
ex_clientmsg_cb(CS_CONTEXT *context,CS_CONNECTION *connection,CS_CLIENTMSG *errmsg)
{
	fprintf(stderr, "\nOpen Client Message:\n");
	fprintf(stderr, "Message number: LAYER = (%ld) ORIGIN = (%ld) ",(long)CS_LAYER(errmsg->msgnumber), (long)CS_ORIGIN(errmsg->msgnumber));
	fprintf(stderr, "SEVERITY = (%ld) NUMBER = (%ld)\n",(long)CS_SEVERITY(errmsg->msgnumber), (long)CS_NUMBER(errmsg->msgnumber));
	fprintf(stderr, "Message String: %s\n", errmsg->msgstring);
	if (errmsg->osstringlen > 0)
	{
		fprintf(stderr, "Operating System Error: %s\n",errmsg->osstring);
	}
	return CS_SUCCEED;
}

static CS_RETCODE CS_PUBLIC
ex_servermsg_cb(CS_CONTEXT *context,CS_CONNECTION *connection,CS_SERVERMSG *srvmsg)
{
#ifdef DEBUG_SQL
	char msg[1024];
	dps_snprintf(msg, sizeof(msg)-1, "Server message: Message number: %ld, Severity %ld, State: %ld, Line: %ld, Msg: '%s'",(long)srvmsg->msgnumber, (long)srvmsg->severity, (long)srvmsg->state, (long)srvmsg->line,srvmsg->text);
	fprintf(stderr, "%s\n",msg);
#endif	

	/*
	if (srvmsg->svrnlen > 0){fprintf(stderr, "Server '%s'\n", srvmsg->svrname);}
	if (srvmsg->proclen > 0){fprintf(stderr, " Procedure '%s'\n", srvmsg->proc);}
	*/

	if(srvmsg->severity>1){
		dps_snprintf(sybsrvmsg, sizeof(sybsrvmsg)-1, "Message number: %ld, Severity %ld, State: %ld, Line: %ld, Msg: '%s'",(long)srvmsg->msgnumber, (long)srvmsg->severity, (long)srvmsg->state, (long)srvmsg->line,srvmsg->text);
	}
	
	return CS_SUCCEED;
}

static int
ex_execute_cmd(DPS_DB *db,CS_CHAR *cmdbuf)
{
	CS_RETCODE      retcode;
	CS_INT          restype;
	CS_COMMAND      *cmd;
	CS_RETCODE      query_code=CS_SUCCEED;

	if ((retcode = ct_cmd_alloc(db->conn, &cmd)) != CS_SUCCEED)
	{
		dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_cmd_alloc() failed: %s",sybsrvmsg);
		query_code=retcode;
		goto unlock_ret;
	}
	if ((retcode = ct_command(cmd, CS_LANG_CMD, cmdbuf, CS_NULLTERM,CS_UNUSED)) != CS_SUCCEED)
	{
		dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_command() failed: %s",sybsrvmsg);
		ct_cmd_drop(cmd);
		query_code=retcode;
		goto unlock_ret;
	}
	if ((retcode = ct_send(cmd)) != CS_SUCCEED)
	{
		dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_send() failed: %s",sybsrvmsg);
		ct_cmd_drop(cmd);
		query_code=retcode;
		goto unlock_ret;
	}
	while ((retcode = ct_results(cmd, &restype)) == CS_SUCCEED)
	{
		switch((int)restype)
                {
                    case CS_CMD_SUCCEED:
                    case CS_CMD_DONE:
                        break;

                    case CS_CMD_FAIL:
                        query_code = CS_FAIL;
                        break;

                    case CS_STATUS_RESULT:
                        retcode = ct_cancel(NULL, cmd, CS_CANCEL_CURRENT);
                        if (retcode != CS_SUCCEED)
                        {
                                dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_cancel() failed: %s",sybsrvmsg);
                                query_code = CS_FAIL;
                        }
                        break;

                    default:
                        query_code = CS_FAIL;
                        break;
                }
                if (query_code == CS_FAIL)
                {
                        retcode = ct_cancel(NULL, cmd, CS_CANCEL_ALL);
                        if (retcode != CS_SUCCEED)
                        {
                                dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_cancel() failed: %s",sybsrvmsg);
                        }
                        break;
                }
        }
        if (retcode == CS_END_RESULTS)
        {
                retcode = ct_cmd_drop(cmd);
                if (retcode != CS_SUCCEED)
                {
                        query_code = CS_FAIL;
                }
        }else{
                (void)ct_cmd_drop(cmd);
                query_code = CS_FAIL;
        }
       
unlock_ret:
        return query_code;
}


static int DpsCTLIBInitDB(DPS_DB *db)
{
	CS_RETCODE	retcode;
	CS_INT		netio_type = CS_SYNC_IO;
	CS_INT		len;
	CS_CHAR         *cmdbuf;

	retcode = cs_ctx_alloc(CS_VERSION_100, &db->ctx);
	if (retcode != CS_SUCCEED)
	{
		dps_snprintf(db->errstr,sizeof(db->errstr)-1,"cs_ctx_alloc() failed: %s",sybsrvmsg);
		db->errcode=1;
		goto unlock_ex;
	}
	retcode = ct_init(db->ctx, CS_VERSION_100);
	if (retcode != CS_SUCCEED)
	{
		dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ex_init: ct_init() failed: %s",sybsrvmsg);
		cs_ctx_drop(db->ctx);
		db->ctx = NULL;
		db->errcode=1;
		goto unlock_ex;
	}

#ifdef EX_API_DEBUG
	retcode = ct_debug(db->ctx, NULL, CS_SET_FLAG, CS_DBG_API_STATES,NULL, CS_UNUSED);
	if (retcode != CS_SUCCEED)
	{
		sprintf(db->errstr,"ex_init: ct_debug() failed");
	}
#endif

	if (retcode == CS_SUCCEED)
	{
		retcode = ct_callback(db->ctx, NULL, CS_SET, CS_CLIENTMSG_CB,(CS_VOID *)ex_clientmsg_cb);
		if (retcode != CS_SUCCEED)
		{
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_callback(clientmsg) failed: %s",sybsrvmsg);
			db->errcode=1;
		}
	}
	if (retcode == CS_SUCCEED)
	{
		retcode = ct_callback(db->ctx, NULL, CS_SET, CS_SERVERMSG_CB,(CS_VOID *)ex_servermsg_cb);
		if (retcode != CS_SUCCEED)
		{
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_callback(servermsg) failed: %s",sybsrvmsg);
			db->errcode=1;
		}
	}
	if (retcode == CS_SUCCEED)
	{
		retcode = ct_config(db->ctx, CS_SET, CS_NETIO, &netio_type, CS_UNUSED, NULL);
		if (retcode != CS_SUCCEED)
		{
			sprintf(db->errstr,"ct_config(netio) failed: %s",sybsrvmsg);
			db->errcode=1;
		}
	}
	if (retcode != CS_SUCCEED)
	{
		ct_exit(db->ctx, CS_FORCE_EXIT);
		cs_ctx_drop(db->ctx);
		db->ctx = NULL;
		db->errcode=1;
		goto unlock_ex;
	}
	if (retcode==CS_SUCCEED){
		retcode = ct_con_alloc(db->ctx, &db->conn);
	}
	if (retcode != CS_SUCCEED)
	{
		dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_con_alloc failed: %s",sybsrvmsg);
		db->errcode=1;
		goto unlock_ex;
	}
	if (retcode == CS_SUCCEED && db->DBUser != NULL)
	{
		if ((retcode = ct_con_props(db->conn, CS_SET, CS_USERNAME, db->DBUser, CS_NULLTERM, NULL)) != CS_SUCCEED)
		{
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_con_props(username) failed: %s",sybsrvmsg);
			db->errcode=1;
		}
	}
	if (retcode == CS_SUCCEED && db->DBPass != NULL)
	{
		if ((retcode = ct_con_props(db->conn, CS_SET, CS_PASSWORD, db->DBPass, CS_NULLTERM, NULL)) != CS_SUCCEED)
		{
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_con_props(password) failed: %s",sybsrvmsg);
			db->errcode=1;
		}
	}
	if (retcode == CS_SUCCEED)
	{
		if ((retcode = ct_con_props(db->conn, CS_SET, CS_APPNAME, "indexer", CS_NULLTERM, NULL)) != CS_SUCCEED)
		{
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_con_props(appname) failed: %s",sybsrvmsg);
			db->errcode=1;
		}
	}
	if (retcode == CS_SUCCEED)
	{
		len = (db->addrURL.hostname == NULL) ? 0 : CS_NULLTERM;
		retcode = ct_connect(db->conn,db->addrURL.hostname,len);
		if (retcode != CS_SUCCEED)
		{
			dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ct_connect failed: %s",sybsrvmsg);
			db->errcode=1;
		}
	}
	if (retcode != CS_SUCCEED)
	{
		ct_con_drop(db->conn);
		db->conn = NULL;
		db->errcode=1;
		goto unlock_ex;
	}
	
	db->connected=1;
        cmdbuf = (CS_CHAR *) DpsMalloc(1024);
	sprintf(cmdbuf, "use %s\n", db->DBName);
	
	if ((retcode = ex_execute_cmd(db, cmdbuf)) != CS_SUCCEED)
	{
		dps_snprintf(db->errstr,sizeof(db->errstr)-1,"ex_execute_cmd(use db) failed: %s",sybsrvmsg);
		db->errcode=1;
	}
	DPS_FREE(cmdbuf);
	
unlock_ex:
	return retcode;
}


static void DpsCTLIBCloseDB(DPS_DB *db)
{
	CS_RETCODE	retcode=CS_SUCCEED;
	CS_INT		close_option;
	CS_INT		exit_option;
	CS_RETCODE	status=CS_SUCCEED; /* FIXME: previous retcode was here */

	if(db->conn){
		close_option = (status != CS_SUCCEED) ? CS_FORCE_CLOSE : CS_UNUSED;
		retcode = ct_close(db->conn, close_option);
		if (retcode != CS_SUCCEED)
		{
			sprintf(db->errstr,"ex_con_cleanup: ct_close() failed");
			db->errcode=1;
			goto unlock_ret;
		}
		retcode = ct_con_drop(db->conn);
		if (retcode != CS_SUCCEED)
		{
			sprintf(db->errstr,"ex_con_cleanup: ct_con_drop() failed");
			db->errcode=1;
			goto unlock_ret;
		}
	}	
	if(db->ctx){
		exit_option = (status != CS_SUCCEED) ? CS_FORCE_EXIT : CS_UNUSED;
		retcode = ct_exit(db->ctx, exit_option);
		if (retcode != CS_SUCCEED)
		{
			sprintf(db->errstr,"ex_ctx_cleanup: ct_exit() failed");
			db->errcode=1;
			goto unlock_ret;
		}
		retcode = cs_ctx_drop(db->ctx);
		if (retcode != CS_SUCCEED)
		{
			sprintf(db->errstr,"ex_ctx_cleanup: cs_ctx_drop() failed");
			db->errcode=1;
			goto unlock_ret;
		}
	}
unlock_ret:
	return;
}

static CS_INT CS_PUBLIC
ex_display_dlen(CS_DATAFMT * column)
{
	CS_INT		len;

	switch ((int) column->datatype)
	{
		case CS_CHAR_TYPE:
		case CS_VARCHAR_TYPE:
		case CS_TEXT_TYPE:
		case CS_IMAGE_TYPE:
			len = MIN(column->maxlength, MAX_CHAR_BUF);
			break;

		case CS_BINARY_TYPE:
		case CS_VARBINARY_TYPE:
			len = MIN((2 * column->maxlength) + 2, MAX_CHAR_BUF);
			break;

		case CS_BIT_TYPE:
		case CS_TINYINT_TYPE:
			len = 3;
			break;

		case CS_SMALLINT_TYPE:
			len = 6;
			break;

		case CS_INT_TYPE:
			len = 11;
			break;

		case CS_REAL_TYPE:
		case CS_FLOAT_TYPE:
			len = 20;
			break;

		case CS_MONEY_TYPE:
		case CS_MONEY4_TYPE:
			len = 24;
			break;

		case CS_DATETIME_TYPE:
		case CS_DATETIME4_TYPE:
			len = 30;
			break;

		case CS_NUMERIC_TYPE:
		case CS_DECIMAL_TYPE:
			len = (CS_MAX_PREC + 2);
			break;

		default:
			len = 12;
			break;
	}
	return MAX((CS_INT)(dps_strlen(column->name) + 1), len);
}

static DPS_SQLRES *
ex_fetch_data(DPS_DB *db,CS_COMMAND *cmd)
{
	CS_RETCODE		retcode;
	CS_INT			num_cols;
	CS_INT			i;
	CS_INT			j;
	CS_INT			row_count = 0;
	CS_INT			rows_read;
	CS_DATAFMT		*datafmt;
	EX_COLUMN_DATA		*coldata;
	DPS_SQLRES		*result=NULL;

	retcode = ct_res_info(cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL);
	if (retcode != CS_SUCCEED)
	{
		sprintf(db->errstr,"ex_fetch_data: ct_res_info() failed");
		db->errcode=1;
		return NULL;
	}
	if (num_cols <= 0)
	{
		sprintf(db->errstr,"ex_fetch_data: ct_res_info() returned zero columns");
		db->errcode=1;
		return NULL;
	}

	coldata = (EX_COLUMN_DATA *)DpsMalloc(num_cols * sizeof (EX_COLUMN_DATA));
	datafmt = (CS_DATAFMT *)DpsMalloc(num_cols * sizeof (CS_DATAFMT));

	for (i = 0; i < num_cols; i++)
	{
		retcode = ct_describe(cmd, (i + 1), &datafmt[i]);
		if (retcode != CS_SUCCEED)
		{
			sprintf(db->errstr,"ex_fetch_data: ct_describe() failed");
			db->errcode=1;
			break;
		}
		datafmt[i].maxlength = ex_display_dlen(&datafmt[i]) + 1;
		datafmt[i].datatype = CS_CHAR_TYPE;
		datafmt[i].format   = CS_FMT_NULLTERM;
		coldata[i].value = (CS_CHAR *)DpsMalloc((size_t)datafmt[i].maxlength + 1);

		retcode = ct_bind(cmd, (i + 1), &datafmt[i],coldata[i].value, &coldata[i].valuelen,(CS_SMALLINT *)&coldata[i].indicator);

		if (retcode != CS_SUCCEED)
		{
			sprintf(db->errstr,"ex_fetch_data: ct_bind() failed");
			db->errcode=1;
			break;
		}
	}
	if (retcode != CS_SUCCEED)
	{
		for (j = 0; j < i; j++)
		{
			DPS_FREE(coldata[j].value);
		}
		DPS_FREE(coldata);
		DPS_FREE(datafmt);
		return NULL;
	}

	result=(DPS_SQLRES*)DpsMalloc(sizeof(DPS_SQLRES));
	bzero((void*)result,sizeof(*result));
	result->nCols=num_cols;
	if (result->nCols){
		result->Fields=(DPS_SQLFIELD*)DpsMalloc(result->nCols*sizeof(DPS_SQLFIELD) + 1);
		bzero(result->Fields,result->nCols*sizeof(DPS_SQLFIELD));
		for (i=0 ; i < result->nCols; i++){
			result->Fields[i].sqlname = (char*)DpsStrdup(datafmt[i].name);
			result->Fields[i].sqllen= 0;
			result->Fields[i].sqltype= 0;
		}
	}
	
	while (((retcode = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED,&rows_read)) == CS_SUCCEED) || (retcode == CS_ROW_FAIL))
	{
		row_count = row_count + rows_read;
		if (retcode == CS_ROW_FAIL)
		{
			sprintf(db->errstr, "Error on row %d.\n", (int)row_count);
			db->errcode=1;
		}
		result->nRows++;
		result->items=(char**)DpsXrealloc(result->items,result->nRows*result->nCols*sizeof(char*) + 1);
		
		for (i = 0; i < num_cols; i++)
		{
			char * p;
			p=coldata[i].value;
			DpsTrim(p," ");
			sql_val(result,(result->nRows-1),(i)) = (char*)DpsStrdup(p?p:"");
		} 
	}

	for (i = 0; i < num_cols; i++)
	{
		DPS_FREE(coldata[i].value);
	}
	DPS_FREE(coldata);
	DPS_FREE(datafmt);

	switch ((int)retcode)
	{
		case CS_END_DATA:
			retcode = CS_SUCCEED;
			db->errcode=0;
			break;

		case CS_FAIL:
			sprintf(db->errstr,"ex_fetch_data: ct_fetch() failed");
			db->errcode=1;
			return NULL;
			break;

		default:
			sprintf(db->errstr,"ex_fetch_data: ct_fetch() returned an expected retcode");
			db->errcode=1;
			return NULL;
			break;
	}
	return result;
}


static DPS_SQLRES *
DpsCTLIBQuery(DPS_DB *db, const char * query)
{
	CS_RETCODE	retcode;
	CS_COMMAND	*cmd;
	CS_INT		res_type;
	DPS_SQLRES	*res=NULL;

	db->errcode=0;
	db->errstr[0]='\0';

	if(!db->connected){
		DpsCTLIBInitDB(db);
		if(db->errcode)
			goto unlock_ret;
	}

        if ((retcode = ct_cmd_alloc(db->conn, &cmd)) != CS_SUCCEED)
        {
                sprintf(db->errstr,"ct_cmd_alloc() failed");
                db->errcode=1;
                goto unlock_ret;
        }

	retcode = ct_command(cmd, CS_LANG_CMD, query, CS_NULLTERM, CS_UNUSED);
	if (retcode != CS_SUCCEED)
	{
                sprintf(db->errstr,"ct_command() failed");
                db->errcode=1;
                goto unlock_ret;
	}

	if (ct_send(cmd) != CS_SUCCEED)
	{
                sprintf(db->errstr,"ct_send() failed");
                db->errcode=1;
                goto unlock_ret;
	}

	while ((retcode = ct_results(cmd, &res_type)) == CS_SUCCEED)
	{
		switch ((int)res_type)
		{
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
   			break;

		case CS_STATUS_RESULT:
			break;

		case CS_CMD_FAIL:
			if(!strstr(sybsrvmsg,"UNIQUE KEY") && !strstr(sybsrvmsg,"PRIMARY KEY")){
				dps_snprintf(db->errstr,sizeof(db->errstr)-1,"%s",sybsrvmsg);
				db->errcode=1;
			}
			break;

		case CS_ROW_RESULT:
			res = ex_fetch_data(db,cmd);
			if (retcode != CS_SUCCEED)
			{
				sprintf(db->errstr,"ex_fetch_data() failed");
				db->errcode=1;
				goto unlock_ret;
			}
			break;

		case CS_COMPUTE_RESULT:
			/* FIXME: add checking */
			break; 

		default:
			sprintf(db->errstr,"DoCompute: ct_results() returned unexpected result type (%d)",(int)res_type);
			db->errcode=1;
			goto unlock_ret;
		}
	}

	switch ((int)retcode)
	{
		case CS_END_RESULTS:
			break;

		case CS_FAIL:
			sprintf(db->errstr,"DoCompute: ct_results() failed");
			db->errcode=1;
			goto unlock_ret;

		default:
			sprintf(db->errstr,"DoCompute: ct_results() returned unexpected result code");
			db->errcode=1;
			goto unlock_ret;
	}

	if ((retcode = ct_cmd_drop(cmd)) != CS_SUCCEED)
	{
                sprintf(db->errstr,"DoCompute: ct_cmd_drop() failed");
                db->errcode=1;
                if(res){
                	udb_free_result(res);
                	res=NULL;
                }
                goto unlock_ret;
	}

unlock_ret:

	return res;
}

#endif


/*********************************** SQLITE *********************/
#if (HAVE_SQLITE)

static int DpsSQLiteInitDB(DPS_DB *db) {
  char dbname[1024], *e, *errmsg;
  dps_strncpy(dbname,db->DBName,sizeof(dbname));
  dbname[sizeof(dbname)-1]='\0';
  /* Remove possible trailing slash */
  e= dbname+dps_strlen(dbname);
  if (e>dbname && e[-1]=='/')
    e[-1]='\0';
  
  if (!(db->sqlt= sqlite_open(dbname, 0, &errmsg)))
  {
    sprintf(db->errstr, "sqlite driver: %s", errmsg ? errmsg : "<NOERROR>");
    DPS_FREE(errmsg);
    db->errcode=1;
    return DPS_ERROR;
  }
  sqlite_busy_timeout(db->sqlt, 1000000000);
  db->connected=1;
  return DPS_OK;
}

static int xCallBack(void *pArg, int argc, char **argv, char **name)
{
  DPS_SQLRES *res= (DPS_SQLRES*) pArg;
  int i;

  if (res->nCols == 0)
  {
    res->nCols = argc;
    res->Fields = (DPS_SQLFIELD*)DpsMalloc(res->nCols * sizeof(DPS_SQLFIELD) + 1);
    bzero(res->Fields, res->nCols * sizeof(DPS_SQLFIELD));
    for (i = 0 ; i < argc; i++) {
	res->Fields[i].sqlname = (char*)DpsStrdup(name[i]);
	res->Fields[i].sqllen= 0;
	res->Fields[i].sqltype= 0;
    }
  }
  res->nRows++;
  res->items=(char**)DpsXrealloc(res->items,res->nRows*res->nCols*sizeof(char*) + 1);
  
  for (i = 0; i < argc; i++)
  {
    sql_val(res,(res->nRows-1),(i)) = (char*)DpsStrdup(DPS_NULL2EMPTY(argv[i]));
  } 
  return 0;
}


static int DpsSQLiteQuery(DPS_DB *db, DPS_SQLRES *res, const char *q) {
  char *errmsg;
  int rc;
  
  db->errcode=0;
  db->errstr[0]='\0';
  if(!db->connected && DPS_OK != DpsSQLiteInitDB(db))
    return DPS_ERROR;
  
  if ((rc= sqlite_exec(db->sqlt, q, xCallBack, (void*)res, &errmsg)))
  {
    sprintf(db->errstr,"sqlite driver: %s",errmsg ? errmsg : "<NOERROR>");
	sqlite_freemem(errmsg);
    if (!strstr(db->errstr,"unique"))
    {
      db->errcode=1;
      return DPS_ERROR;
    }
  }
  return DPS_OK;
}

#endif	/* HAVE_SQLITE */


/*********************************** SQLITE3 *********************/
#if (HAVE_SQLITE3)

static int DpsSQLite3InitDB(DPS_DB *db) {
  char dbname[1024], *e;
  dps_strncpy(dbname,db->DBName,sizeof(dbname));
  dbname[sizeof(dbname)-1]='\0';
  /* Remove possible trailing slash */
  e= dbname+dps_strlen(dbname);
  if (e>dbname && e[-1]=='/')
    e[-1]='\0';
  
  if ( SQLITE_OK != sqlite3_open(dbname, &db->sqlt3)) {
    const char *errmsg = sqlite3_errmsg(db->sqlt3);
    dps_snprintf(db->errstr, sizeof(db->errstr), "sqlite3 driver (dbname:%s): %s", dbname, errmsg ? errmsg : "<NOERROR>");
    sqlite3_close(db->sqlt3);
    db->errcode = 1;
    return DPS_ERROR;
  }
  if ( SQLITE_OK != sqlite3_busy_timeout(db->sqlt3, 1000000000)) {
    const char *errmsg = sqlite3_errmsg(db->sqlt3);
    dps_snprintf(db->errstr, sizeof(db->errstr), "sqlite3 driver: %s", errmsg ? errmsg : "<NOERROR>");
    sqlite3_close(db->sqlt3);
    db->errcode = 1;
    return DPS_ERROR;
  }
  db->connected = 1;
  return DPS_OK;
}

static int xCallBack3(void *pArg, int argc, char **argv, char **name)
{
  DPS_SQLRES *res = (DPS_SQLRES*) pArg;
  int i;

  if (res->nCols == 0)
  {
    res->nCols= argc;
    res->Fields = (DPS_SQLFIELD*)DpsMalloc(res->nCols * sizeof(DPS_SQLFIELD) + 1);
    bzero(res->Fields, res->nCols * sizeof(DPS_SQLFIELD));
    for (i = 0 ; i < argc; i++) {
	res->Fields[i].sqlname = (char*)DpsStrdup(name[i]);
	res->Fields[i].sqllen= 0;
	res->Fields[i].sqltype= 0;
    }
  }
  res->nRows++;
  res->items=(char**)DpsXrealloc(res->items,res->nRows*res->nCols*sizeof(char*) + 1);
  
  for (i = 0; i < argc; i++)
  {
    sql_val(res,(res->nRows-1),(i)) = (char*)DpsStrdup(DPS_NULL2EMPTY(argv[i]));
  } 
  return 0;
}


static int DpsSQLite3Query(DPS_DB *db, DPS_SQLRES *res, const char *q) {
  char *errmsg;
  int rc;
  
  db->errcode=0;
  db->errstr[0]='\0';
  if(!db->connected && DPS_OK != DpsSQLite3InitDB(db))
    return DPS_ERROR;
  
  if ((rc= sqlite3_exec(db->sqlt3, q, xCallBack3, (void*)res, &errmsg)))
  {
    dps_snprintf(db->errstr, sizeof(db->errstr), "sqlite3 driver: %s", errmsg ? errmsg : "<NOERROR>");
	sqlite3_free(errmsg);
    if (!strstr(db->errstr,"unique"))
    {
      db->errcode=1;
      return DPS_ERROR;
    }
  }
  return DPS_OK;
}

#endif	/* HAVE_SQLITE3 */


/***********************************************************************/

/*
 *   Wrappers for different databases
 *
 *   DpsDBEscDoubleStr();
 *   DpsDBEscStr();
 *   DpsSQLQuery();
 *   DpsSQLValue();
 *   DpsSQLLen();
 *   DpsSQLFree();
 *   DpsSQLClose();
 */  


char * DpsDBEscDoubleStr(char *from) {
  char *p = from;
  while((p = strchr(p, (int)','))) {
    *p = '.';
  }
  return from;
}


char * DpsDBEscStr(DPS_DB *db, char *to, const char *from, size_t len) {
	char *s;
#if defined(HAVE_DP_MYSQL) || defined(HAVE_DP_PGSQL)
	size_t	i;
#endif

	if(!from)return(NULL);
	if(!to)to=(char*)DpsMalloc(len*2+1);

#ifdef HAVE_DP_MYSQL
	if(db->DBType == DPS_DB_MYSQL) {
	  for(i = 0; i < 3; i++) {
	    if(!db->connected) {
	      int rc = DpsMySQLInit(db);
	      if (DPS_OK == rc && db->connected) break;
	      mysql_close(&db->mysql);
	      db->connected = 0;
	      DPSSLEEP(20);
	    }
	  }
	  if (db->connected)
	        mysql_real_escape_string(&db->mysql, to, from, len);
	  else
	        mysql_escape_string(to, from, len);
		return(to);
	}
#endif
#ifdef HAVE_DP_PGSQL
	if(db->DBType == DPS_DB_PGSQL) {
	  for (i = 0; i < 3; i++) {
	    if(!db->connected) {
	      DpsPgSQLInitDB(db);
	      if(db->errcode) {
		PQfinish(db->pgsql);
		db->connected = 0;
		DPSSLEEP(20);
	      } else break;
	    }
	  }
	  if (db->connected)
	        PQescapeStringConn(db->pgsql, to, from, len, NULL);
	  else
	        PQescapeString(to, from, len);
		return(to);
	}
#endif
	s=to;
	if (db->DBType == DPS_DB_ORACLE7 || db->DBType == DPS_DB_ORACLE8  ||
	    db->DBType == DPS_DB_MSSQL || db->DBType == DPS_DB_DB2 ||
	    db->DBType == DPS_DB_IBASE || db->DBType == DPS_DB_SAPDB ||
	    db->DBType == DPS_DB_SQLITE || db->DBType == DPS_DB_ACCESS ||
	    db->DBType == DPS_DB_SQLITE3 || db->DBType == DPS_DB_MIMER)
	 {
	    while(*from){
		switch(*from){
			case '\'':
				/* Note that no break here!*/
				*to=*from;to++;
			default:
				*to=*from;
		}
		to++;from++;
	    }
	} else {
	    while(*from){
		switch(*from){
			case '\'':
			case '\\':
				*to='\\';to++;
			default:*to=*from;
		}
		to++;from++;
	    }
	}
	*to=0;return(s);
}



int __DPSCALL DpsSQLBegin(DPS_DB *db) {
  int rc = DPS_OK;

  switch(db->DBDriver) {

  case DPS_DB_PGSQL:
    rc = DpsSQLAsyncQuery(db, NULL, "BEGIN WORK");
    break;

  case DPS_DB_SQLITE:
  case DPS_DB_SQLITE3:
  case DPS_DB_MSSQL:
    rc = DpsSQLAsyncQuery(db, NULL, "BEGIN TRANSACTION");
    break;

  case DPS_DB_ORACLE7:
  case DPS_DB_ORACLE8:
  case DPS_DB_SAPDB:
    rc = DpsSQLAsyncQuery(db, NULL, "COMMIT");
    db->commit_fl = 1;
    break;

  default:
    db->commit_fl = 1;
    break;
  }
  return rc;
}

int __DPSCALL DpsSQLEnd(DPS_DB *db) {
  int rc = DPS_OK;

  switch(db->DBDriver) {

  case DPS_DB_ORACLE7:
  case DPS_DB_ORACLE8:
  case DPS_DB_SAPDB:
    db->commit_fl = 0;

  case DPS_DB_PGSQL:
  case DPS_DB_SQLITE:
  case DPS_DB_SQLITE3:
  case DPS_DB_MSSQL:
    rc = DpsSQLAsyncQuery(db, NULL, "COMMIT");
    break;

  default:
    db->commit_fl = 0;
    break;
  }
  return rc;
}

int __DPSCALL DpsSQLAbort(DPS_DB *db) {
  int rc = DPS_OK;

  switch(db->DBDriver) {

  case DPS_DB_ORACLE7:
  case DPS_DB_ORACLE8:
  case DPS_DB_SAPDB:
    db->commit_fl = 0;

  case DPS_DB_PGSQL:
  case DPS_DB_SQLITE:
  case DPS_DB_SQLITE3:
  case DPS_DB_MSSQL:
    rc = DpsSQLAsyncQuery(db, NULL, "ROLLBACK");
    break;

  default:
    db->commit_fl = 0;
    break;
  }
  return rc;
}



void DpsSQLResInit(DPS_SQLRES *SQLRes) {
  bzero((void*)SQLRes, sizeof(*SQLRes));
}


int __DPSCALL _DpsSQLQuery(DPS_DB *db, DPS_SQLRES *SQLRes, const char * query, const char *file, const int line) {
  DPS_SQLRES *res = (SQLRes == NULL) ? &db->Res : SQLRes;
	
#ifdef DEBUG_SQL
	unsigned long ticks;
	ticks=DpsStartTimer();
#endif
	
	if(SQLRes) {
	  DpsSQLFree(SQLRes);
/*	  bzero((void*)SQLRes, sizeof(*SQLRes));*/
	}
	
#if HAVE_DP_MYSQL
	if(db->DBDriver==DPS_DB_MYSQL){
/*		res=(DPS_SQLRES*)DpsMalloc(sizeof(*res));
		bzero((void*)res, sizeof(*res));*/
		DpsMySQLQuery(db,res,query);
		res->DBDriver=db->DBDriver;
		goto ret;
	}
#endif
	
#if HAVE_SQLITE
	if(db->DBDriver==DPS_DB_SQLITE){
/*		res=(DPS_SQLRES*)DpsMalloc(sizeof(*res));
		bzero((void*)res, sizeof(*res));*/
		DpsSQLiteQuery(db,res,query);
		res->DBDriver=db->DBDriver;
		goto ret;
	}
#endif
	
#if HAVE_SQLITE3
	if(db->DBDriver==DPS_DB_SQLITE3){
/*		res=(DPS_SQLRES*)DpsMalloc(sizeof(*res));
		bzero((void*)res, sizeof(*res));*/
		DpsSQLite3Query(db,res,query);
		res->DBDriver=db->DBDriver;
		goto ret;
	}
#endif

#if HAVE_DP_PGSQL
	if(db->DBDriver==DPS_DB_PGSQL){
/*		res=(DPS_SQLRES*)DpsMalloc(sizeof(*res));
		bzero((void*)res, sizeof(*res));*/
		DpsPgSQLQuery(db,res,query);
		res->DBDriver=db->DBDriver;
		if(!res->pgsqlres){
/*			DPS_FREE(res);*/
			res=NULL;
		}
		goto ret;
	}
#endif
	
#if HAVE_DP_MSQL
	if(db->DBDriver==DPS_DB_MSQL){
/*		res=(DPS_SQLRES*)DpsMalloc(sizeof(*res));
		bzero((void*)res, sizeof(*res));*/
		res->msqlres=DpsMSQLQuery(db,query);
		if(!res->msqlres){
/*			DPS_FREE(res);*/
			res=NULL;
		}else{
			res->DBDriver=db->DBDriver;
		}
		goto ret;
	}
#endif
	
#if HAVE_IBASE
	if(db->DBDriver==DPS_DB_IBASE){
	  DpsIBaseQuery(db, res, query);
		res->DBDriver = db->DBDriver;
		goto ret;
	}
#endif
	
#if HAVE_ORACLE8
	if(db->DBDriver==DPS_DB_ORACLE8){
		res=DpsOracle8Query(db,query);
		if(res)res->DBDriver=db->DBDriver;
		goto ret;
	}
#endif

#if HAVE_ORACLE7
	if(db->DBDriver==DPS_DB_ORACLE7){
		res=DpsOracle7Query(db,query);
		if(res)res->DBDriver=db->DBDriver;
		return res;
	}
#endif
	
#if HAVE_CTLIB
	if(db->DBDriver==DPS_DB_MSSQL){
		res=DpsCTLIBQuery(db,query);
		if(res)res->DBDriver=db->DBDriver;
		goto ret;
	}
#endif

#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_SAPDB || HAVE_DB2)
	if(db->DBDriver == DPS_DB_ODBC) {
	        DpsODBCQuery(db, res, query);
		if(res) res->DBDriver = db->DBDriver;
		goto ret;
	}
#endif
	db->errcode=1;
	dps_snprintf(db->errstr,sizeof(db->errstr)-1,"Unsupported SQL database type %d @ %s:%d", db->DBDriver, file, line);
ret:
#ifdef DEBUG_SQL
	ticks=DpsStartTimer()-ticks;
/*	fprintf(stderr,"[%d,%x] %.2fs SQL {%s:%d}: %s\n", getpid(), db, (float)ticks/1000, file, line, query);*/
	fprintf(stderr,"%.2fs SQL {%s:%d}: %s\n", (float)ticks/1000, file, line, query);
#endif
	
#ifdef DEBUG_ERR_QUERY
	if (db->errcode == 1) {
	  fprintf(stderr, "{%s:%d} Query: %s\n", file, line, query);
	  fprintf(stderr, "\tSQL-server message: %s\n\n", db->errstr);
	}
#endif

	if(res){
	  if(SQLRes) {
/*	    SQLRes[0] = res[0];*/
	  } else {
	    DpsSQLFree(res);
	  }
/*	  DPS_FREE(res);*/
	}
	return db->errcode ? DPS_ERROR : DPS_OK;
}


#if defined(HAVE_DP_PGSQL) || defined(HAVE_DP_MYSQL)

int __DPSCALL _DpsSQLAsyncQuery(DPS_DB *db, DPS_SQLRES *SQLRes, const char * query, const char *file, const int line) {
  DPS_SQLRES *res = (SQLRes == NULL) ? &db->Res : SQLRes;
  int rc, done = 0;
	
#ifdef DEBUG_SQL
	unsigned long ticks;
	ticks = DpsStartTimer();
#endif

	if(SQLRes) {
	  DpsSQLFree(SQLRes);
/*	  bzero((void*)SQLRes, sizeof(*SQLRes));*/
	}

#if HAVE_DP_PGSQL
	if(db->DBDriver==DPS_DB_PGSQL){
/*		res=(DPS_SQLRES*)DpsMalloc(sizeof(*res));
		bzero((void*)res, sizeof(*res));*/
		DpsPgSQLAsyncQuery(db, res, query);
		res->DBDriver = db->DBDriver;
		if(!res->pgsqlres){
/*			DPS_FREE(res);*/
			res = NULL;
		}
		done = 1;
		goto asyncret;
	}
#endif
#if HAVE_DP_MYSQL
	if(db->DBDriver == DPS_DB_MYSQL) {
/*		res = (DPS_SQLRES*)DpsMalloc(sizeof(*res));
		bzero((void*)res, sizeof(*res));*/
		DpsMySQLAsyncQuery(db, res, query);
		res->DBDriver = db->DBDriver;
		done = 1;
		goto asyncret;
	}
#endif

	rc = _DpsSQLQuery(db, res, query, file, line);
	
asyncret:
#ifdef DEBUG_SQL
	ticks=DpsStartTimer()-ticks;
/*	fprintf(stderr,"[%d,%x] %.2fs SQL {%s:%d}: %s\n", getpid(), db, (float)ticks/1000, file, line, query);*/
	if (done) fprintf(stderr,"%.2fs AsSQL {%s:%d}: %s\n", (float)ticks/1000, file, line, query);
#endif
	
#ifdef DEBUG_ERR_QUERY
	if (db->errcode == 1) {
	  fprintf(stderr, "{%s:%d} Query: %s\n", file, line, query);
	  fprintf(stderr, "\tSQL-server message: %s\n\n", db->errstr);
	}
#endif

	if(res){
	  if(SQLRes) {
/*	    SQLRes[0] = res[0];*/
	  } else {
	    DpsSQLFree(res);
	  }
/*	  DPS_FREE(res);*/
	}
	return db->errcode ? DPS_ERROR : DPS_OK;
}

#endif



size_t __DPSCALL DpsSQLNumRows(DPS_SQLRES * res){
#if HAVE_DP_PGSQL
	if(res->DBDriver==DPS_DB_PGSQL)
		return(PQntuples(res->pgsqlres));
#endif

#if HAVE_DP_MSQL
	if(res->DBDriver==DPS_DB_MSQL)
		return(msqlNumRows(res->msqlres));
#endif
	return(res?res->nRows:0);
}

size_t DpsSQLNumCols(DPS_SQLRES * res){
#if HAVE_DP_PGSQL
	if(res->DBDriver==DPS_DB_PGSQL)
		return 0;
#endif

#if HAVE_DP_MSQL
	if(res->DBDriver==DPS_DB_MSQL)
		return 0;
#endif
	return(res?res->nCols:0);
}

char * DpsSQLValue(DPS_SQLRES *res, size_t i, size_t j) {
	char * v = NULL;
#if HAVE_DP_MYSQL
	if(res->DBDriver==DPS_DB_MYSQL){
		if (i<res->nRows){
			size_t offs=res->nCols*i+j;
			return res->Items[offs].val;
		}else{
			return NULL;
		}
	}
#endif

#if HAVE_DP_PGSQL
	if(res->DBDriver==DPS_DB_PGSQL){
		return(PQgetvalue(res->pgsqlres,(int)(i),(int)(j)));
	}
#endif

#if HAVE_DP_MSQL
	if(res->DBDriver==DPS_DB_MSQL){
		return(msql_value(res->msqlres,i,j));
	}
#endif
	if (i < res->nRows) v = sql_value(res, i, j);
	return v;

}

void __DPSCALL DpsSQLFree(DPS_SQLRES * res){

	if (res->Fields){
		size_t i;
		for(i=0;i<res->nCols;i++){
			/*
			printf("%s(%d)\n",res->Fields[i].sqlname,res->Fields[i].sqllen);
			*/
			DPS_FREE(res->Fields[i].sqlname);
		}
		DPS_FREE(res->Fields);
	}
	
#if HAVE_DP_PGSQL
	if(res->DBDriver==DPS_DB_PGSQL){
	  if (res->pgsqlres) PQclear(res->pgsqlres);
	  bzero((void*)res, sizeof(*res));
	  return;
	}
#endif

#if HAVE_DP_MSQL
	if(res->DBDriver==DPS_DB_MSQL){
	  if (res->msqlres) msqlFreeResult(res->msqlres);
	  bzero((void*)res, sizeof(*res));
	  return;
	}
#endif

#if HAVE_ORACLE7
	if(res->DBDriver==DPS_DB_ORACLE7){
		oci_free_result(res)
		bzero((void*)res, sizeof(*res));
		return;
	}
#endif

#if HAVE_ORACLE8
	if(res->DBDriver==DPS_DB_ORACLE8){
		oci_free_result(res);
		bzero((void*)res, sizeof(*res));
		return;
	}
#endif
	udb_free_result(res);
	bzero((void*)res, sizeof(*res));
}


size_t DpsSQLLen(DPS_SQLRES * res,size_t i,size_t j){
#if HAVE_DP_MYSQL
	if(res->DBDriver==DPS_DB_MYSQL){
		if (i<res->nRows){
			size_t offs=res->nCols*i+j;
			return res->Items[offs].len;
		}else{
			return 0;
		}
	}
#endif
	return 0;
}




void DpsSQLClose(DPS_DB *db){

	if(!db->connected)return;

#if defined(HAVE_DP_PGSQL)
	if(db->DBDriver==DPS_DB_PGSQL){
	  if (1 /*db->async_in_process*/) {
	    /* Wait async query in process */
	    PGresult  *pgsqlres;
	    while ((pgsqlres = PQgetResult(db->pgsql))) {
	      if (PQstatus(db->pgsql) == CONNECTION_BAD) { 
		PQfinish(db->pgsql); /* db error occured, trying reconnect */
		db->connected=0;
		break;
	      }
	      PQclear(pgsqlres);
	    }
	    db->async_in_process = 0;
	  }
	  PQfinish(db->pgsql);
	  goto ret;
	}
#endif


#if HAVE_DP_MYSQL
	if(db->DBDriver==DPS_DB_MYSQL){
		mysql_close(&db->mysql);
		goto ret;
	}
#endif
	
#if HAVE_DP_MSQL
	if(db->DBDriver==DPS_DB_MSQL){
		msqlClose(db->msql);
		goto ret;
	}
#endif
	
#if HAVE_IBASE
	if(db->DBDriver==DPS_DB_IBASE){
		DpsIBaseCloseDB(db);
		goto ret;
	}
#endif

#if HAVE_ORACLE8
	if(db->DBDriver==DPS_DB_ORACLE8){
		DpsOracle8CloseDB(db);
		goto ret;
	}
#endif
	
#if HAVE_ORACLE7
	if(db->DBDriver==DPS_DB_ORACLE7){
		DpsOracle7CloseDB(db);
		goto ret;
	}
#endif
	
#if HAVE_CTLIB
	if(db->DBDriver==DPS_DB_MSSQL){
		DpsCTLIBCloseDB(db);
		goto ret;
	}
#endif
	
#if HAVE_SQLITE
	if(db->DBDriver==DPS_DB_SQLITE){
		sqlite_close(db->sqlt);
		goto ret;
	}
#endif

#if HAVE_SQLITE3
	if(db->DBDriver==DPS_DB_SQLITE3){
		sqlite3_close(db->sqlt3);
		goto ret;
	}
#endif
	
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_SAPDB || HAVE_DB2)
	DpsODBCCloseDB(db);
	goto ret;
#endif
ret:
	db->connected=0;
	return;
}


#include "dps_mutex.h"

/*

  DpsSQLMonitor()
  
  - Executes SQL queries arriving from some virtual input stream
  - Displays query results to some virtual output streams
  
  The incoming strings are checked against:
    
    - SQL comments, i.e. lines starting with "--".
      The comment lines are skipped.
    
    - A set of end-of-query markers, i.e. 
      * MSSQL   style: 'GO' keyword written in the beginning
      * mysql   style: ';'  character written in the end of the string
      * minisql style: '\g' characters written in the end of the string
      
      As soon as a end-of-query marker has found, current collected query
      is executed.
    
    - In other case, the next line is concatenated to the previously
      collected data thus helping to run multi-line queries.
  
  
  The particular ways of handling input and output streams are
  passed in the DPS_SQLMON_PARAM argument. This approach makes 
  it possible to use DpsSQLMonitor() for many purposes:
  
  - executing a bundle of queries written in a text file.
  - running a command-line SQL monitor ("mysql", "pgsql" or "sqlplus" alike),
    i.e. executing queries arriving from the keyboard.
    
  - running a GUI SQL monitor,
    i.e. executing querues arriving from the keyboard in some GUI application.
  
  
  DPS_SQLMON_PARAM has got this structure:
  
  typedef struct dps_sqlmon_param_st
  {
    int flags;
    FILE *infile;
    FILE *outfile;
    char *(*gets)(struct dps_sqlmon_param_st *prm, char *str, size_t size);
    int (*display)(struct dps_sqlmon_param_st *, DPS_SQLRES *sqlres);
    int (*prompt)(struct dps_sqlmon_param_st *, const char *msg);
	void* tag_ptr;
  } DPS_SQLMON_PARAM;
  
  
  gets():     a function to get one string from the incoming stream.
  display():  a function to display an SQL query results
  prompt():   a function to display some prompts:
                - an invitation to enter data, for example: "SQL>".
                - an error message
  
  flags:      sets various aspects of the monitor behaviour:
              the only one so far:
                - whether to display field names in the output

  infile:     if the caller wants to input SQL queries from some FILE stream,
              for example, from STDIN or from a textual file opened via fopen(),
              it can use this field for this purpose
              
  outfile:    if the caller wants to output results to some FILE stream,
              for example, to STDOUT or to a textial file opened via fopen(),
              it can use this field for this purpose

  context_ptr:    extra pointer parameter for various purposes.
  
  To start using DpsSQLMonitor() one have to set
    - gets()
    - diplay()
    - prompt()
  
  If diplay() or prompt() use some FILE streams, the caller should
  also set infile and outfile with appropriative stream pointers.
  
*/


__C_LINK int __DPSCALL DpsSQLMonitor(DPS_AGENT *A, DPS_ENV *Env, DPS_SQLMON_PARAM *prm){
     char str[10*1024];
     char *snd=str;
     int  rc=DPS_OK;
     char delimiter = ';';
     
     str[sizeof(str)-1]='\0';
     while(1){
          int  res;
          int  exec=0;
          char *send0;
          size_t  rbytes= (sizeof(str)-1) - (snd - str);

	  if (!prm->gets(prm,snd,rbytes))
	    break;

          if(snd[0]=='#' || !strncmp(snd, "--", 2)){
               continue;
          }
          send0 = snd;
          snd = snd + dps_strlen(snd);
          while(snd > send0 && strchr(" \r\n\t", snd[-1])){
               *--snd='\0';
          }
		  if(snd == send0)
			  continue;
          
          if(snd[-1] == delimiter) {
               exec=1;
               *--snd='\0';
          }else
          if(snd - 2 >= str && snd[-1]=='g' && snd[-2]=='\\'){
               exec=1;
               snd-=2;
               *snd='\0';
          }else
          if(snd-2 >= str && strchr("oO", snd[-1]) && strchr("gG",snd[-2])){
               exec=1;
               snd-=2;
               *snd='\0';
          }else
          if((size_t)(snd - str + 1) >= sizeof(str)){
               exec=1;
          }
          
          if(exec){
          
               prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, "'");
               prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, str);
               prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, "'");
               prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, "\n");
               
               if (!strncasecmp(str,"connection",10)){
                    size_t newnum= atoi(str+10);
                    char msg[255];
                    if (newnum < (A->flags & DPS_FLAG_UNOCON) ? Env->dbl.nitems : A->dbl.nitems) {
		      if (A->flags & DPS_FLAG_UNOCON) {
                         Env->dbl.currdbnum= newnum;
		      } else {
			A->dbl.currdbnum= newnum;
		      }
                         sprintf(msg,"Connection changed to #%d", (int)((A->flags & DPS_FLAG_UNOCON) ? Env->dbl.currdbnum : A->dbl.currdbnum));
                         prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, msg);
                         prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, "\n");
                    }else{
                         sprintf(msg,"Wrong connection number %d", (int)newnum);
                         prm->prompt(prm, DPS_SQLMON_MSG_ERROR, msg);
                         prm->prompt(prm, DPS_SQLMON_MSG_ERROR, "\n");
                    }
                    
               }
               else if (!strncasecmp(str, "delimiter=", 10)) {
                    delimiter = str[10];
               }
               else if (!strcasecmp(str,"fields=off")){
                    prm->flags=0;
               }
               else if (!strcasecmp(str,"fields=on")){
                    prm->flags=1;
               }
               else{
                    DPS_SQLRES sqlres;
                    DPS_DB *db = (A->flags & DPS_FLAG_UNOCON) ? Env->dbl.db[Env->dbl.currdbnum] : A->dbl.db[A->dbl.currdbnum];
                    prm->nqueries++;
		    DpsSQLResInit(&sqlres);
		    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
		    res = DpsSQLQuery(db, &sqlres, str);
		    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
		    if (res!=DPS_OK)
		    {
		      prm->nbad++;
		      rc=DPS_ERROR;
		      prm->prompt(prm, DPS_SQLMON_MSG_ERROR, db->errstr);
		      prm->prompt(prm, DPS_SQLMON_MSG_ERROR, "\n");
		    }
		    else
		    {
		      prm->ngood++;
                      res=prm->display(prm,&sqlres);
                    }
                    DpsSQLFree(&sqlres);
               }
               snd=str;
               str[0]='\0';
          }else{
               if(send0!=snd){
                    *snd=' ';
                    snd++;
                    *snd='\0';
               }
          }
     }
     prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, "\n");
     return rc;
}


#endif
