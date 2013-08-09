/* Copyright (C) 2003-2009 Datapark corp. All rights reserved.
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
#include "dps_vars.h"
#include "dps_log.h"
#include "dps_mutex.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

/*
#define DEBUG_TIME 1
*/

#if defined(APACHE1) || defined(APACHE2)

static int DpsLogLevel2Apache(int level) {
  switch(level) {
  case DPS_LOG_DEBUG: return APLOG_DEBUG;
  case DPS_LOG_EXTRA: return APLOG_INFO;
  case DPS_LOG_INFO:  return APLOG_NOTICE;
  case DPS_LOG_WARN:  return APLOG_WARNING;
  case  DPS_LOG_ERROR:
  default:
    return APLOG_CRIT;
  }
}

#endif


int log2stderr = 1;
static int DpsLogLevel = DPS_LOG_INFO;


__C_LINK int __DPSCALL DpsSetRefProc(DPS_ENV * Conf,void (*_RefProc)(int code,const char *url, const char *ref)){
	Conf->RefInfo=_RefProc;
	return(0);
}

__C_LINK int __DPSCALL DpsSetThreadProc(DPS_ENV * Conf,void (*_ThreadInfo)(DPS_AGENT *A,const char *state, const char* str)){
	Conf->ThreadInfo=_ThreadInfo;
	return(0);
}


typedef struct _code {
        const char *c_name;
        int     c_val;
} CODE;

#if defined HAVE_SYSLOG_H && defined WITH_SYSLOG
static const CODE facilitynames[] ={
#ifdef LOG_AUTH
    {"auth",	LOG_AUTH},
#endif
#ifdef LOG_AUTHPRIV
    {"authpriv",LOG_AUTHPRIV},
#endif
#ifdef LOG_CRON
    {"cron", 	LOG_CRON},
#endif
#ifdef LOG_DAEMON
    {"daemon",	LOG_DAEMON},
#endif
#ifdef LOG_FTP
    {"ftp",	LOG_FTP},
#endif
#ifdef LOG_KERN
    {"kern",	LOG_KERN},
#endif
#ifdef LOG_LPR
    {"lpr",	LOG_LPR},
#endif
#ifdef LOG_MAIL
    {"mail",	LOG_MAIL},
#endif
#ifdef LOG_NEWS
    {"news",	LOG_NEWS},
#endif
#ifdef LOG_SYSLOG
    {"syslog",	LOG_SYSLOG},
#endif
#ifdef LOG_USER
    {"user",	LOG_USER},
#endif
#ifdef LOG_UUCP
    {"uucp",	LOG_UUCP},
#endif
#ifdef LOG_LOCAL0
    {"local0",	LOG_LOCAL0},
#endif
#ifdef LOG_LOCAL1
    {"local1",	LOG_LOCAL1},
#endif
#ifdef LOG_LOCAL2
    {"local2",	LOG_LOCAL2},
#endif
#ifdef LOG_LOCAL3
    {"local3",	LOG_LOCAL3},
#endif
#ifdef LOG_LOCAL4
    {"local4",	LOG_LOCAL4},
#endif
#ifdef LOG_LOCAL5
    {"local5",	LOG_LOCAL5},
#endif
#ifdef LOG_LOCAL6
    {"local6",	LOG_LOCAL6},
#endif
#ifdef LOG_LOCAL7
    {"local7",	LOG_LOCAL7},
#endif
    {NULL,		-1},
};
static int syslog_facility(const char *f){
	const CODE *fn=facilitynames;
	if( !f || !f[0])return LOG_FACILITY;
	while (fn->c_name!=NULL){
		if (strcasecmp(f, fn->c_name)==0)
			return fn->c_val;
		fn++;
	}
	fprintf(stderr, "Config file error: unknown facility given: '%s'\n\r", f);
	fprintf(stderr, "Will continue with default facility\n\r");
	return LOG_FACILITY;
}

#endif /* HAVE_SYSLOG_H */


__C_LINK void __DPSCALL DpsSetLogLevel(DPS_AGENT *A, int level) {
        if (A) DPS_GETLOCK(A, DPS_LOCK_THREAD);
	DpsLogLevel = level;
	if (A) DPS_RELEASELOCK(A, DPS_LOCK_THREAD);
}
__C_LINK void __DPSCALL DpsIncLogLevel(DPS_AGENT *A) {
        DPS_GETLOCK(A, DPS_LOCK_THREAD);
	if (DpsLogLevel < DPS_LOG_DEBUG) DpsLogLevel++;
	DPS_RELEASELOCK(A, DPS_LOCK_THREAD);
}
__C_LINK void __DPSCALL DpsDecLogLevel(DPS_AGENT *A) {
        DPS_GETLOCK(A, DPS_LOCK_THREAD);
	if (DpsLogLevel > 0) DpsLogLevel--;
	DPS_RELEASELOCK(A, DPS_LOCK_THREAD);
}


int DpsOpenLog(const char *appname, DPS_ENV *Env, int log_to_stderr) {

  if (Env->is_log_open == 0) {

#if defined HAVE_SYSLOG_H && defined WITH_SYSLOG
/* LOG_PERROR supported by 4.3BSD Reno releases and later */
	int facility=syslog_facility(DpsVarListFindStr(&Env->Vars,"SyslogFacility",""));

#ifdef LOG_PERROR
	openlog(appname?appname:"<NULL>", (log_to_stderr) ? LOG_PERROR|LOG_PID : LOG_PID, facility);
#else
	openlog(appname?appname:"<NULL>",LOG_PID,facility);
	if (log_to_stderr) Env->logFD = stderr;
#endif /* LOG_PERROR */

#else
	/* If syslog not found or not used, use stderr. */
	if (log_to_stderr) Env->logFD = stderr;
	
#endif /* HAVE_SYSLOG_H */
	Env->is_log_open=1;
  } else {
#if defined HAVE_SYSLOG_H && defined WITH_SYSLOG
    /* LOG_PERROR supported by 4.3BSD Reno releases and later */
	int facility = syslog_facility(DpsVarListFindStr(&Env->Vars,"SyslogFacility",""));

	closelog();
#ifdef LOG_PERROR
	openlog(appname?appname:"<NULL>", (log_to_stderr) ? LOG_PERROR|LOG_PID : LOG_PID, facility);
#else
	openlog(appname?appname:"<NULL>", LOG_PID, facility);
	if (log_to_stderr) Env->logFD = stderr;
#endif /* LOG_PERROR */
#endif /* HAVE_SYSLOG_H */
  }
  if (appname) DpsVarListReplaceStr(&Env->Vars, "appname", appname);
  return DPS_OK;
}

__C_LINK int __DPSCALL DpsNeedLog(int level) {
  if (DpsLogLevel < level) return 0;
  return 1;
}

static int dps_logger(DPS_AGENT *Agent, DPS_ENV *Env, int handle, int level, const char *fmt, va_list ap){
	char buf[DPS_LOG_BUF_LEN+1];
	char logFMT[DPS_LOG_BUF_LEN+1];
#ifdef DEBUG_TIME
	unsigned long ticks = DpsStartTimer();
#endif
#if defined(APACHE1) || defined(APACHE2)
	request_rec *r = (request_rec*) ((Agent != NULL) ? Agent->request : NULL);
#endif


#if defined(HAVE_SYSLOG_H) && defined(WITH_SYSLOG)
#ifdef DEBUG_TIME
	dps_snprintf(logFMT, DPS_LOG_BUF_LEN, "{%02d} %lu %s", handle, ticks, fmt);
#else
	dps_snprintf(logFMT, DPS_LOG_BUF_LEN, "{%02d} %s", handle, fmt);
#endif
#else
	dps_snprintf(logFMT, DPS_LOG_BUF_LEN, "[%d]{%02d} %s", (int)getpid(), handle, fmt);
#endif

	vsnprintf(buf, DPS_LOG_BUF_LEN, logFMT, ap);

#if defined HAVE_SYSLOG_H && defined WITH_SYSLOG
	syslog((level!=DPS_LOG_ERROR)?LOG_INFO:LOG_ERR,"%s",buf);
#endif
	if (Env->is_log_open) {


#if defined(APACHE1) || defined(APACHE2)

	  if (r != NULL)

#ifdef APACHE1
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO | DpsLogLevel2Apache(level), r->server, "%s", buf);
#else
	  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | DpsLogLevel2Apache(level), 0, r, "%s", buf);
#endif

#else /* defined(APACHE1) || defined(APACHE2) */


	  if (Env->logFD)
	    fprintf(Env->logFD,"%s\n",buf); 

#endif /* defined(APACHE1) || defined(APACHE2) */


	} else {
	  dps_snprintf(Env->errstr, sizeof(Env->errstr), "%s", buf);
	}
	return 1;
}

void DpsLog(DPS_AGENT *Agent, int level, const char *fmt, ...){
	va_list ap;

	if (!Agent){
	    fprintf(stderr, "BUG IN LOG - blame Kir\n");
	    return;
	}

	if (!DpsNeedLog(level)) return;
	DPS_GETLOCK(Agent, DPS_LOCK_CONF);

	if(1 /*Agent->Conf->is_log_open*/){
		va_start(ap,fmt);
		dps_logger(Agent, Agent->Conf, Agent->handle,level,fmt,ap);
		va_end(ap);
	}else{
		fprintf(stderr,"Log has not been opened\n");
	}

	DPS_RELEASELOCK(Agent, DPS_LOCK_CONF);
	return;
}

void DpsLog_noagent(DPS_ENV * Env, int level, const char *fmt, ...){
	va_list ap;

	if(Env->is_log_open) {
	  if (DpsNeedLog(level)) {
		va_start(ap,fmt);
		dps_logger(NULL, Env, 0, level, fmt, ap);
		va_end(ap);
	  }
	}else
		fprintf(stderr,"Log has not been opened\n");
	return;
}
