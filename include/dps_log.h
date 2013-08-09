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

#ifndef _DPS_LOG_H
#define _DPS_LOG_H
/*
#define DEBUG_LOG 1
*/
#include "dps_config.h"
#include "dps_common.h" /* for DPS_ENV etc. */
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

/* Define default log facility */
#ifdef HAVE_SYSLOG_H
#ifndef LOG_FACILITY
#define LOG_FACILITY LOG_LOCAL7
#endif /* LOG_FACILITY */
#else
#define LOG_FACILITY 0
#endif

/* According to some recommendations, try not to exceed about 800 bytes 
   or you might have problems with old syslog implementations */
#define DPS_LOG_BUF_LEN 480

/* Verbose levels */
#define DPS_LOG_NONE	0
#define DPS_LOG_ERROR	1
#define DPS_LOG_WARN	2
#define DPS_LOG_INFO	3
#define DPS_LOG_EXTRA	4
#define DPS_LOG_DEBUG	5

extern int DpsOpenLog(const char * appname,DPS_ENV *Env, int log2stderr_fl);

extern void DpsLog(DPS_AGENT *Agent, int level, const char *fmt, ...);
/* if you do not have DPS_AGENT struct yet, use DpsLog_noagent */
extern void DpsLog_noagent(DPS_ENV *Env, int level, const char *fmt, ...);

extern __C_LINK void __DPSCALL DpsSetLogLevel(DPS_AGENT *A, int level);
extern __C_LINK void __DPSCALL DpsIncLogLevel(DPS_AGENT *A);
extern __C_LINK void __DPSCALL DpsDecLogLevel(DPS_AGENT *A);
extern __C_LINK int  __DPSCALL DpsSetThreadProc(DPS_ENV * Conf,void (*_ThreadInfo)(DPS_AGENT* A,const char *state, const char* str));
extern __C_LINK int  __DPSCALL DpsSetRefProc(DPS_ENV * Conf,void (*_RefProc)(int code,const char *url, const char *ref));
extern __C_LINK int  __DPSCALL DpsNeedLog(int level);

#endif /* _DPS_LOG_H */
