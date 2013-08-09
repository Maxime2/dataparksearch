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

#ifndef _DPS_UTILS_H
#define _DPS_UTILS_H

#include "dps_config.h"

#include <stdio.h>

/* for time_t */
#include <time.h>

/* for va_list */
#include <stdarg.h>

#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
extern char **environ;
#endif

#include "dps_common.h"
#include "dps_charsetutils.h"

/* This is used in DpsTime_t2Str and in its callers */
#define DPS_MAXTIMESTRLEN	35


/* Some useful MACROs */
#define DPS_STREND(s)		(s+strlen(s))
#define DPS_FREE(x)		{if((x)!=NULL){DpsFree((void*)(x));x=NULL;}}
#define DPS_SKIP(s,set)		while((*s)&&(strchr(set,*s)))s++;
#define DPS_SKIPN(s,set)	while((*s)&&(!strchr(set,*s)))s++;



extern int ARGC;
extern char **ARGV;
extern char **ENVP;
#ifdef HAVE_SETPROCTITLE
#define dps_setproctitle setproctitle
#else
void dps_setproctitle(const char *fmt, ...);
#endif


/* Misc functions */
extern int    DpsInit(int argc, char **argv, char **envp);
extern void   DpsDeInit(void);
extern char * DpsGetStrToken(char * s, char ** last);
extern char * DpsTrim(char * p, const char * delim);
extern char * DpsRTrim(char* p, const char * delim);
extern char * DpsUnescapeCGIQuery(char *d, const char *s);
extern char * DpsEscapeURL(char *d,const char *s);
extern char * DpsEscapeURI(char *d,const char *s);
extern char * DpsRemove2Dot(char *path);
extern char * DpsBuildParamStr(char * dst,size_t len,const char * src,char ** argv,size_t argc);
extern char * DpsStrRemoveChars(char * str, const char * sep);
extern char * DpsStrRemoveDoubleChars(char * str, const char * sep);

/* This should convert Last-Modified time returned by webserver
 * to time_t (seconds since the Epoch). -kir
 */
extern time_t DpsHttpDate2Time_t(const char * date);

extern time_t DpsFTPDate2Time_t(char *date);

/***********************************************************
 * converts time_str to time_t (seconds)
 * time_str can be exactly number of seconds
 * or in the form 'xxxA[yyyB[zzzC]]'
 * (Spaces are allowed between xxx and A and yyy and so on)
 *   there xxx, yyy, zzz are numbers (can be negative!)
 *         A, B, C can be one of the following:
 *		s - second
 *		M - minute	
 *		h - hour
 *		d - day
 *		m - month
 *		y - year
 *	(these letters are as in strptime/strftime functions)
 *
 * Examples:
 * 1234 - 1234 seconds
 * 4h30M - 4 hours and 30 minutes (will return 9000 seconds)
 * 1y6m-15d - 1 year and six month minus 15 days (will return 45792000 s)
 * 1h-60M+1s - 1 hour minus 60 minutes plus 1 second (will return 1 s)
 */
time_t Dps_dp2time_t(const char * time_str);


/* This one for printing HTTP Last-Modified: header */
extern void DpsTime_t2HttpStr(time_t t, char * time_str);
/* This one deals with timezone offset */
extern int DpsInitTZ(void);
extern unsigned long DpsStartTimer(void);


/* Probably string missing functions */

#ifndef HAVE_BZERO
extern __C_LINK void __DPSCALL bzero(void *b, size_t len);
#endif

#ifndef HAVE_STRCASECMP
extern __C_LINK int __DPSCALL strcasecmp(const char *s1, const char *s2);
#endif

#ifndef HAVE_STRNCASECMP
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifndef HAVE_STRCASESTR
extern char * strcasestr(register const char *s1, register const char *s2);
#endif

#ifndef HAVE_VSNPRINTF
extern int vsnprintf(char *str, size_t size, const char  *fmt,  va_list ap);
#endif

extern __C_LINK int __DPSCALL dps_snprintf(char *str, size_t size, const char *fmt, ...);
#ifndef HAVE_SNPRINTF
#define snprintf dps_snprintf
#endif

extern char *dps_strtok_r(char *s, const char *delim, char **last, char *save);
#ifndef HAVE_STRTOK_R
#define strtok_r(x,y,z) dps_rstok_r((x), (y), (z), NULL);
#endif

extern double dps_strtod (const char *nptr, char **endptr);
#define dps_atof(x)         ((x) ? dps_strtod((x), (char **)NULL):0.0)

extern void dps_strerror(DPS_AGENT *Agent, int level, const char *fmt, ...);



extern char *_DpsStrndup(const char *str, size_t len);
#ifndef EFENCE
extern char *_DpsStrdup(const char *str);
#endif

#if !defined(HAVE_STRNDUP) || defined(EFENCE)
#define strndup _DpsStrndup
#define DpsStrndup _DpsStrndup
#endif

#ifndef HAVE_HSTRERROR
extern const char *h_errlist[];
# define hstrerror(err)  ((err) <= 4 ? h_errlist[(err)] : "unknown error")
#endif

#ifndef HAVE_INET_NET_PTON_PROTO
int inet_net_pton(int af, const char *src, void *dst, size_t size);
#endif

extern __C_LINK int __DPSCALL DpsHex2Int(int h);
extern __C_LINK int __DPSCALL DpsInt2Hex(int i);


#define BASE64_LEN(len) (4 * (((len) + 2) / 3) +2)
extern __C_LINK size_t __DPSCALL dps_base64_encode (const char *s, char *store, size_t length);
extern __C_LINK size_t __DPSCALL dps_base64_decode (char * dst, const char * src, size_t len);
extern char * dps_rfc1522_decode(char * dst, const char *src);

/* Build directory */
extern int   DpsBuild(char * path, int mode);

/* SetEnv */
extern int DpsSetEnv(const char * name,const char * value);
extern void DpsUnsetEnv(const char * name);

extern void DpsUniRemoveDoubleSpaces(dpsunicode_t * ustr);
extern void DpsUniPrint(const char *head, dpsunicode_t * ustr);

extern void DpsWriteLock(int fd);
extern void DpsUnLock(int fd);
extern void DpsReadLock(int fd);
extern void DpsReadLockFILE(FILE *f);
extern __C_LINK void __DPSCALL DpsWriteLockFILE(FILE *f);
extern __C_LINK void __DPSCALL DpsUnLockFILE(FILE *f);

extern FILE * dps_fopen(const char *path, const char *mode);
extern int dps_demonize(void);
extern void * dps_bsearch(const void *key, const void *base0, size_t nmemb0, size_t size, int (*compar)(const void *, const void *));
extern int dps_heapsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));


/* NULL safe atoi*/
/*#define DPS_ATOI(x)		((x)?atoi(x):0)*/
#define DPS_ATOI(x)		((x)?(int)strtol((x), (char **)NULL, 0):0)
#define DPS_HTOI(x)             ((x)?(int)strtol((x), (char **)NULL, 16):0)
/*#define DPS_ATOF(x)		((x)?atof(x):0.0)*/
#define DPS_ATOF(x)		((x)?strtod(x, (char **)NULL):0.0)
#define DPS_ATOU(x)		((x)?(urlid_t)strtoll((x), (char**)NULL,0):0)
#define DPS_NULL2EMPTY(x)	((x)?(x):"")
#define DPD_NULL2STR(x)         ((x)?(x):"<NULL>")



#ifdef WITH_TRACE


#define TRACE_IN(A, fn)  {						\
  register int trace_i;							\
  register unsigned long trace_ticks = DpsStartTimer();                       \
  fprintf(A->TR, "%lu [%d] in ", trace_ticks, A->handle);		        \
  for (trace_i = 0; trace_i < A->level; trace_i++) fprintf(A->TR, "-"); \
  A->level++;								\
  fprintf(A->TR, "%s at %s:%d\n", fn, __FILE__, __LINE__);		\
  fflush(A->TR);							\
}
#define TRACE_OUT(A)   {						\
  register int trace_i;		 				        \
  register unsigned long trace_ticks = DpsStartTimer();                       \
  fprintf(A->TR, "%lu [%d] out", trace_ticks, A->handle);		        \
  if (A->level) A->level--;						\
  for (trace_i = 0; trace_i < A->level; trace_i++) fprintf(A->TR, "-"); \
  fprintf(A->TR, "at %s:%d\n", __FILE__, __LINE__);		        \
  fflush(A->TR);							\
}

#define TRACE_LINE(A) {                                                 \
  register int trace_i;						        \
  register unsigned long trace_ticks = DpsStartTimer();                       \
  fprintf(A->TR, "%lu [%d] got", trace_ticks, A->handle);		\
  for (trace_i = 0; trace_i < A->level; trace_i++) fprintf(A->TR, "-"); \
  fprintf(A->TR, "the %s:%d\n", __FILE__, __LINE__);	        	\
  fflush(A->TR);							\
}


#else

#define TRACE_IN(A, fn)
#define TRACE_OUT(A)
#define TRACE_LINE(A)

#endif


#define __ fprintf(stderr, " == %s:%d\n", __FILE__, __LINE__);


#endif /* _DPS_UTILS_H */
