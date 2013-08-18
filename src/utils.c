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

#include "dps_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#ifndef HAVE_INET_NET_PTON_PROTO
#include <assert.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <errno.h>

#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef CHASEN
#include <chasen.h>
#endif

#ifdef WITH_HTTPS
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include "dps_common.h"
#include "dps_unicode.h"
#include "dps_utils.h"
#include "dps_charsetutils.h"
#include "dps_xmalloc.h"
#include "dps_log.h"


char dps_pid_name[PATH_MAX];    /* EXT */
long tz_offset = 0;	        /* EXT */
unsigned int milliseconds = 0;  /* To sleep between documents    */

/*** str functions implementation which may be missing on some OS ***/

#ifndef HAVE_BZERO
__C_LINK void __DPSCALL bzero(void *b, size_t len) {
	memset(b,0,len);
}
#endif

#ifndef HAVE_STRCASECMP
__C_LINK int __DPSCALL strcasecmp(const char *s1, const char *s2) {
	register const unsigned char
			*us1 = (const unsigned char *)s1,
			*us2 = (const unsigned char *)s2;

	while (dps_tolower(*us1) == dps_tolower(*us2++))
		if (*us1++ == '\0')
			return (0);
	return (dps_tolower(*us1) - dps_tolower(*--us2));
}
#endif

#ifndef HAVE_STRNCASECMP
int strncasecmp(const char *s1, const char *s2, size_t n)
{
	if (n != 0) {
		register const unsigned char
				*us1 = (const unsigned char *)s1,
				*us2 = (const unsigned char *)s2;

		do {
			if (dps_tolower(*us1) != dps_tolower(*us2++))
				return (dps_tolower(*us1) - dps_tolower(*--us2));
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}
#endif

char * _DpsStrndup(const char * str, size_t len) {
	char * res;
	res=(char*)DpsMalloc(len+1);
	if (res == NULL) return NULL;
	dps_strncpy(res, DPS_NULL2EMPTY(str), len);
	res[len]='\0';
	return res;
}

#ifndef EFENCE
char * _DpsStrdup(const char *str) {
        size_t len;
        char *copy;

        len = dps_strlen(DPS_NULL2EMPTY(str)) + 1;
        if ((copy = DpsMalloc(len)) == NULL) return NULL;
        if (len > 1) dps_memcpy(copy, DPS_NULL2EMPTY(str), len); /* was: dps_memmove */
	copy[len - 1] = '\0';
/*	fprintf(stderr, " -- dup len:%d copy:%s -- str:%s\n", len, copy, str);*/
        return (copy);
}
#endif

#ifndef HAVE_STRCASESTR
char * strcasestr(register const char *s, register const char *find) {
        register char c, sc;
        register size_t len;

        if ((c = *find++) != 0) {
                c = dps_tolower((unsigned char)c);
                len = dps_strlen(find);
                do {
                        do {
                                if ((sc = *s++) == 0)
                                        return (NULL);
                        } while ((char)dps_tolower((unsigned char)sc) != c);
                } while (strncasecmp(s, find, len) != 0);
                s--;
        }
        return ((char *)s);
}
#endif

/**************** RFC 1522 ******************************************/
static char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int __DPSCALL DpsHex2Int(int h){
	if((h>='0')&&(h<='9'))return(h-'0');
	if((h>='A')&&(h<='F'))return(h-'A'+10);
	if((h>='a')&&(h<='f'))return(h-'a'+10);
	return(0);
}

int __DPSCALL DpsInt2Hex(int i){
	if((i>=0)&&(i<=9))return(i+'0');
	if((i>=10)&&(i<=15))return(i+'A'-10);
	return '0';
}	

char * dps_rfc1522_decode(char * dst, const char *src){
	const char *s;
	char  *d;

	s=src;
	d=dst;
	*dst=0;

	while(*s){
		char *e;

		if((e=strstr(s,"=?"))){
			char *schema;
			char *data;

			if(e>s){
				/* Copy last plain text buffer */
				dps_strncpy(d, s, (size_t)(e - s));
				d+=(e-s);
				*d=0;
			}
			e+=2;
			if(!(schema=strchr(e,'?'))){
				/* Error */
				break;
			}
			schema++;
			data=schema+2;

			if(!(e=strstr(data,"?="))){
				/* This is an error */
				break;
			}
			switch(*schema){
				case 'Q':
				case 'q':
					while(data<e){
						char c;
						if(*data=='='){
						  c=(char)(DpsHex2Int((int)data[1])*16+DpsHex2Int((int)data[2]));
							data+=3;
						}else{
							c=data[0];
							data++;
						}
						/* Copy one char to dst */
						*d=c;d++;*d=0;
					}
					break;
				case 'B':
				case 'b':
					while(data<e){
						char *p;
						int x0,x1,x2,x3,res;

						p=strchr(base64,data[0]);x0=p?p-base64:0;
						p=strchr(base64,data[1]);x1=p?p-base64:0;
						p=strchr(base64,data[2]);x2=p?p-base64:0;
						p=strchr(base64,data[3]);x3=p?p-base64:0;

						res=x3+x2*64+x1*64*64+x0*64*64*64;

						p=(char*)(&res);

						if(p[2])*d=p[2];d++;*d=0;
						if(p[1])*d=p[1];d++;*d=0;
						if(p[0])*d=p[0];d++;*d=0;

						data+=4;
					}
					break;
				default:
					/* Error */
					schema=NULL;
					break;
			}
			if(schema==NULL){
				/* Error */
				break;
			}
			s=e+2;
		}else{
			/* Copy plain text tail */
			dps_strcpy(d,s);
			break;
		}
	}
	return(dst);
}


/************* Base 64 *********************************************/
/* BASE64 encoding converts  3x8 bits into 4x6 bits                */

__C_LINK size_t __DPSCALL dps_base64_encode (const char *src, char *store, size_t length){
	size_t i;
	unsigned char *p = (unsigned char *)store;
	unsigned const char *s = (unsigned const char *)src;

	for (i = 0; i < length; i += 3){
		*p++ = base64[s[0] >> 2];
		*p++ = base64[((s[0] & 3) << 4) + (s[1] >> 4)];
		*p++ = base64[((s[1] & 0xf) << 2) + (s[2] >> 6)];
		*p++ = base64[s[2] & 0x3f];
		s += 3;
	}
	/*Pad the result*/
	if (i == length + 1) *(p - 1) = '=';
	else if (i == length + 2) *(p - 1) = *(p - 2) = '=';
	*p = '\0';
	return p- (unsigned char*)store;
}


__C_LINK size_t __DPSCALL dps_base64_decode(char * dst, const char * src, size_t len){
	int count=0;
	int b[4];
	char * dst0=dst;
	
	for( ; (*src)&&(len>3); src++){
		char * p=strchr(base64,*src);
		
		b[count]=p?p-base64:0;
		if(++count==4){
			int res;
			res=b[3]+b[2]*64+b[1]*64*64+b[0]*64*64*64;
			*dst++=(res>>16)&0xFF;
			*dst++=(res>>8)&0xFF;
			*dst++= res&0xFF;
			count=0;
			len-=3;
		}
	}
	*dst='\0';
	return dst-dst0;
}


unsigned long DpsStartTimer(void){
	struct timeval tv;
	gettimeofday(&tv, NULL);
/*	return ((unsigned long)tv.tv_usec);*/
	return (((unsigned long)tv.tv_sec * (unsigned long)1000) + (unsigned long)tv.tv_usec / 1000);

/*
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC (sysconf(_SC_CLK_TCK))
	struct tms tms_tmp;
	return (float)times(&tms_tmp)*1000/CLOCKS_PER_SEC;
*/
}

char * DpsTrim(char *p, const char *delim){
int len;
	len = dps_strlen(p);
	while ((len > 0) && strchr(delim, p[len - 1] )) {
		p[len - 1] = '\0';
		len--;
	}
	while((*p)&&(strchr(delim,*p)))p++;
	return(p);
}

char * DpsRTrim(char* p, const char *delim){
int len;
	len = dps_strlen(p);
	while ((len > 0) && strchr(delim, p[len - 1] )) {
		p[len - 1] = '\0';
		len--;
	}
	return(p);
}


/* strtok_r clone */
char * dps_strtok_r(char *s, const char *delim, char **last, char *save) {
    const char *spanp;
    int c, sc;
    char *tok;

    if (s == NULL) {
      if ((s = *last) == NULL)
	return NULL;
      if (save && *save) *(*last - 1) = *save;
    } else if (save) *save = 0;

cont:
    c = *s++;
    for (spanp = delim; (sc = *spanp++) != 0; )
    {
	if (c == sc)
	{
	    goto cont;
	}
    }

    if (c == 0)		/* no non-delimiter characters */
    {
	*last = NULL;
	return NULL;
    }
    tok = s - 1;

    for (;;)
    {
	c = *s++;
	spanp = delim;
	do
	{
	    if ((sc = *spanp++) == c)
	    {
		if (c == 0)
		{
		    s = NULL;
		}
		else
		{
		    char *w = s - 1;
		    if (save) *save = *w;
		    *w = '\0';
		}
		*last = s;
		return tok;
	    }
	}
	while (sc != 0);
    }
}

/* This function parses string tokens      */
/* It understands: text 'text' "text"      */
/* I.e. Words, tokens enclosed in ' and "  */
/* Behavior is the same with strtok_r()    */
char * DpsGetStrToken(char * s, char ** last) {
	char * tbeg,lch;
	if (s == NULL && (s = *last) == NULL)
		return NULL;

	/* Find the beginning of token */
	for(;(*s)&&(strchr(" \r\n\t",*s));s++);

	if(!*s)return(NULL);

	lch=*s;
	if((lch=='\'')||(lch=='"'))s++;
	else	lch=' ';
	tbeg=s;

	while(1){
		switch(*s){
			case '\0': *last=NULL;break;
		        case '\\': if (s[1] == lch) dps_memmove(s, s + 1, dps_strlen(s)); break;
			case '"':
			case '\'':
				if(lch==*s){
					*s='\0';
					*last=s+1;	
				}
				break;
			case ' ':
			case CR_CHAR:
			case NL_CHAR:
			case HT_CHAR:
				if(lch==' '){
					*s='\0';
					*last=s+1;
				}
				break;
			default:;
		}
		if(*s)s++;
		else break;
	}
	return(tbeg);
}



char * DpsUnescapeCGIQuery(char *d, const char *s) {
  int hi, lo = 0;
  char *dd;
	if((d==NULL)||(s==NULL))return(NULL);
	dd=d;
	while(*s){
		if(*s=='%'){
		    if (*(++s) == '\0') break;
		    if (strchr("0123456789",*s) != NULL) hi = *s - '0';
		    else hi = (dps_tolower((int)*s) - 'a' + 10) & 15;
		    if (*(++s) == '\0') break;
		    if (strchr("0123456789",*s) != NULL) lo = *s - '0';
		    else lo = (dps_tolower((int)*s) -'a' + 10) & 15;
		    *d = (char)(hi * 16 + lo);
		}else
		if(*s=='+'){
			*d=' ';
		}else{
			*d=*s;
		}
		s++; d++;
	}
	*d=0;return(dd);
}

char * DpsEscapeURL(char *d, const char *s) {
	char *dd;
	unsigned const char *ss = (unsigned const char *)s;
	if((d==NULL)||(s==NULL))return(0);
	dd=d;
	while(*ss){
	  if ((*ss == 2) || (*ss == 3)) {
	    ss++; continue;
	  } else if((*ss & 0x80) || (*ss == 0x7f) || (*ss < 0x20) || strchr("%&<>+[](){}/?#'\"\\;,:@=",*ss)) {
/*			sprintf(d,"%%%X",(int)*ss);*/
	                register int dig = (int)(((*ss) & 0xF0) >> 4);
			*d = '%';
			d[1] = (char)((dig < 10) ? '0' + dig : 'A' + (dig - 10));
			dig = (int)((*ss) & 0xF);
			d[2] = (char)((dig < 10) ? '0' + dig : 'A' + (dig - 10));
			d+=2;
	  } else
	  if(*ss==' ') {
			*d='+';
	  } else{
			*d=*ss;
	  }
	  ss++;d++;
	}
	*d=0;
	return(dd);
}

char * DpsEscapeURI(char *d,const char *s){
	char *dd;
	unsigned const char *ss = (unsigned const char *)s;
	if((d==NULL)||(s==NULL))return(0);
	dd=d;
	while(*ss){
/*		if(strchr(" ", *ss)) {*/
	        if((*ss & 0x80) || (*ss == 0x7f) || (*ss < 0x20)) {
/*			sprintf(d,"%%%X",(int)*s);*/
	                register int dig = (int)(((*ss) & 0xF0) >> 4);
			*d = '%';
			d[1] = (char)((dig < 10) ? '0' + dig : 'A' + (dig - 10));
			dig = (int)((*ss) & 0xF);
			d[2] = (char)((dig < 10) ? '0' + dig : 'A' + (dig - 10));
			d+=2;
		}else{
			*d=*ss;
		}
		ss++;d++;
	}
	*d=0;
	return(dd);
}


/* Oh,no! We have to remove parent level like /parent/../index.html from path
   Let's do it recursively! */
char * DpsRemove2Dot(char *path){
        char *ptr;
	char *tail;
    
        if(!(ptr=strstr(path,"../"))) return path;
	if(ptr==path) return path; /* How could it be? */
        tail=ptr+2;
	ptr--;
        *ptr=0;
	if(!(ptr=strrchr(path,'/'))) *path=0; else *ptr=0;
        path = dps_strcat(path, tail);
	return DpsRemove2Dot(path);
}

/* Function to convert date returned by web-server to time_t
 *
 * Earlier we used strptime, but it seems to be completely broken
 * on Solaris 2.6. Thanks to Maciek Uhlig <muhlig@us.edu.pl> for pointing
 * out this and bugfix proposal (to use Apache's ap_parseHTTPdate).
 *
 * 10 July 2000 kir.
 */

#define BAD_DATE 0

typedef struct time_zone_st {
  const char *name;
  int        sign;
  time_t     offset;
} DPS_TZ_OFFSET;

static int dps_tz_cmp(DPS_TZ_OFFSET *z1, DPS_TZ_OFFSET *z2) {
  return strcasecmp(z1->name, z2->name);
}

static DPS_TZ_OFFSET time_zones[] = {

#include "timezones.inc"

/* END Marker */
/*	{NULL, 0, 0}*/
};

static time_t dps_tz_adjust(time_t timevalue, const char *tz_name) {
  DPS_TZ_OFFSET key, *tz;
  time_t add = 0;

  if (tz_name == NULL) return timevalue;
  if (*tz_name == '+' || *tz_name == '-') {
    add = (tz_name[1] - '0') * 36000 + (tz_name[2] - '0') * 3600 + (tz_name[4] - '0') * 600 + (tz_name[5] - '0') * 60;
    if (*tz_name == '+') return timevalue - add;
    return timevalue + add;
  }else
  if (strncasecmp(tz_name, "PM ", 3) == 0) {
    add = 12 * 3600;
    key.name = tz_name + 3;
  } else if (strncasecmp(tz_name, "AM ", 3) == 0) {
    key.name = tz_name + 3;
  } else  key.name = tz_name;
  tz = bsearch(&key, time_zones, sizeof(time_zones) / sizeof(time_zones[0]), sizeof(time_zones[0]), (qsort_cmp)dps_tz_cmp);
  if (tz == NULL) {
    return timevalue + add;
  }
  if (tz->sign == 1) return timevalue + tz->offset + add;
  return timevalue - tz->offset + add;
}


/*****
 *  BEGIN: Below is taken from Apache's util_date.c
 */

/* ====================================================================
 * Copyright (c) 1996-1999 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/********
 * BEGIN
 * This is taken from ap_ctype.h
 * --kir.
 */

#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/* These macros allow correct support of 8-bit characters on systems which
 * support 8-bit characters.  Pretty dumb how the cast is required, but
 * that's legacy libc for ya.  These new macros do not support EOF like
 * the standard macros do.  Tough.
 */
#define ap_isalnum(c) (isalnum(((unsigned char)(c))))
#define ap_isalpha(c) (isalpha(((unsigned char)(c))))
#define ap_iscntrl(c) (iscntrl(((unsigned char)(c))))
#define ap_isdigit(c) (isdigit(((unsigned char)(c))))
#define ap_isgraph(c) (isgraph(((unsigned char)(c))))
#define ap_islower(c) (islower(((unsigned char)(c))))
#define ap_isprint(c) (isprint(((unsigned char)(c))))
#define ap_ispunct(c) (ispunct(((unsigned char)(c))))
#define ap_isspace(c) (isspace(((unsigned char)(c))))
#define ap_isupper(c) (isupper(((unsigned char)(c))))
#define ap_isxdigit(c) (isxdigit(((unsigned char)(c))))
#define ap_tolower(c) (tolower(((unsigned char)(c))))
#define ap_toupper(c) (toupper(((unsigned char)(c))))

#ifdef __cplusplus
}
#endif

/*******
 * END of taken from ap_ctype.h
 */

/*
 * Compare a string to a mask
 * Mask characters (arbitrary maximum is 256 characters, just in case):
 *   @ - uppercase letter
 *   $ - lowercase letter
 *   & - hex digit
 *   # - digit
 *   ~ - digit or space
 *   * - swallow remaining characters 
 *   + - plus or minus sign
 *  <x> - exact match for any other character
 */
static int ap_checkmask(const char *data, const char *mask) {
    int i;
    char d;

    for (i = 0; i < 256; i++) {
	d = data[i];
	switch (mask[i]) {
	case '\0':
	    return (d == '\0');

	case '*':
	    return 1;

	case '@':
	    if (!ap_isupper(d)) {
		return 0;
	    }
	    break;
	case '$':
	    if (!ap_islower(d)) {
		return 0;
	    }
	    break;
	case '#':
	    if (!ap_isdigit(d)) {
		return 0;
	    }
	    break;
	case '&':
	    if (!ap_isxdigit(d)) {
		return 0;
	    }
	    break;
	case '~':
	    if ((d != ' ') && !ap_isdigit(d)) {
		return 0;
	    }
	    break;
        case '+':
	    if ((d != '+') && (d != '-')) {
	        return 0;
	    }
	    break;
	default:
	    if (mask[i] != d) {
		return 0;
	    }
	    break;
	}
    }
    return 0;			/* We only get here if mask is corrupted (exceeds 256) */
}

/*
 * tm2sec converts a GMT tm structure into the number of seconds since
 * 1st January 1970 UT.  Note that we ignore tm_wday, tm_yday, and tm_dst.
 * 
 * The return value is always a valid time_t value -- (time_t)0 is returned
 * if the input date is outside that capable of being represented by time(),
 * i.e., before Thu, 01 Jan 1970 00:00:00 for all systems and 
 * beyond 2038 for 32bit systems.
 *
 * This routine is intended to be very fast, much faster than mktime().
 */
static time_t ap_tm2sec(const struct tm * t)
{
    int year;
    time_t days;
    static const int dayoffset[12] =
    {306, 337, 0, 31, 61, 92, 122, 153, 184, 214, 245, 275};

    year = t->tm_year;

    if (year < 70 || ((sizeof(time_t) <= 4) && (year >= 138)))
	return BAD_DATE;

    /* shift new year to 1st March in order to make leap year calc easy */

    if (t->tm_mon < 2)
	year--;

    /* Find number of days since 1st March 1900 (in the Gregorian calendar). */

    days = year * 365 + year / 4 - year / 100 + (year / 100 + 3) / 4;
    days += dayoffset[t->tm_mon] + t->tm_mday - 1;
    days -= 25508;		/* 1 jan 1970 is 25508 days since 1 mar 1900 */

    days = ((days * 24 + t->tm_hour) * 60 + t->tm_min) * 60 + t->tm_sec;

    if (days < 0)
	return BAD_DATE;	/* must have overflowed */
    else
	return days;		/* must be a valid time */
}

/*
 * Parses an HTTP date in one of these forms:
 *
 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
 *     Sat, 11-Feb-2006 18:54:55 GMT  ; "fixed RFC 850"/cookie format (?)
 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 *     17 Jul 2013                    ; e.g.: Youtube upload date (not as HTTP header)
 *     Jul 25, 2013                   ; US style, e.g.: Youtube upload date (not as HTTP header)
 *     08.07.2013                     ; Widely used date stamp
 *     2013-08-06T12:40:12            ; ISO8601 timestamp
 *
 * and returns the time_t number of seconds since 1 Jan 1970 GMT, or
 * 0 if this would be out of range or if the date is invalid.
 *
 * The restricted HTTP syntax is
 * 
 *     HTTP-date    = rfc1123-date | rfc850-date | asctime-date
 *
 *     rfc1123-date = wkday "," SP date1 SP time SP "GMT"
 *     rfc850-date  = weekday "," SP date2 SP time SP "GMT"
 *     asctime-date = wkday SP date3 SP time SP 4DIGIT
 *
 *     date1        = 2DIGIT SP month SP 4DIGIT
 *                    ; day month year (e.g., 02 Jun 1982)
 *     date2        = 2DIGIT "-" month "-" 2DIGIT
 *                    ; day-month-year (e.g., 02-Jun-82)
 *     date3        = month SP ( 2DIGIT | ( SP 1DIGIT ))
 *                    ; month day (e.g., Jun  2)
 *
 *     time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
 *                    ; 00:00:00 - 23:59:59
 *
 *     wkday        = "Mon" | "Tue" | "Wed"
 *                  | "Thu" | "Fri" | "Sat" | "Sun"
 *
 *     weekday      = "Monday" | "Tuesday" | "Wednesday"
 *                  | "Thursday" | "Friday" | "Saturday" | "Sunday"
 *
 *     month        = "Jan" | "Feb" | "Mar" | "Apr"
 *                  | "May" | "Jun" | "Jul" | "Aug"
 *                  | "Sep" | "Oct" | "Nov" | "Dec"
 *
 * However, for the sake of robustness (and Netscapeness), we ignore the
 * weekday and anything after the time field (including the timezone).
 *
 * This routine is intended to be very fast; 10x faster than using sscanf.
 *
 * Originally from Andrew Daviel <andrew@vancouver-webpages.com>, 29 Jul 96
 * but many changes since then.
 *
 */
time_t DpsHttpDate2Time_t(const char *date){
    struct tm ds;
    int mint, mon;
    const char *monstr = NULL, *timstr = NULL, *tz_str = NULL;
    static const int months[12] =
    {
	('J' << 16) | ('a' << 8) | 'n', ('F' << 16) | ('e' << 8) | 'b',
	('M' << 16) | ('a' << 8) | 'r', ('A' << 16) | ('p' << 8) | 'r',
	('M' << 16) | ('a' << 8) | 'y', ('J' << 16) | ('u' << 8) | 'n',
	('J' << 16) | ('u' << 8) | 'l', ('A' << 16) | ('u' << 8) | 'g',
	('S' << 16) | ('e' << 8) | 'p', ('O' << 16) | ('c' << 8) | 't',
	('N' << 16) | ('o' << 8) | 'v', ('D' << 16) | ('e' << 8) | 'c'};

    if (!date)
	return BAD_DATE;

    while (*date && ap_isspace(*date))	/* Find first non-whitespace char */
	++date;

    if (*date == '\0')
	return BAD_DATE;

    bzero(&ds, sizeof(ds));

    if (ap_checkmask(date, "####-##-##T##:##:##+##:##*")) {	/* ISO 8601 format */
      ds.tm_year = ((date[0] - '0') * 10 + (date[1] - '0') - 19) * 100;
      if (ds.tm_year < 0)
	return BAD_DATE;
      ds.tm_year += ((date[2] - '0') * 10) + (date[3] - '0');

      ds.tm_mon = mon = (date[5] - '0') * 10 + (date[6] - '0') - 1;
      ds.tm_mday = (date[8] - '0') * 10 + (date[9] - '0');
      timstr = date + 11;
      tz_str = date + 19;
    } 
    else if (ap_checkmask(date, "####-##-##T##:##:##*")) {	/* ISO 8601 format, UTC  */
      ds.tm_year = ((date[0] - '0') * 10 + (date[1] - '0') - 19) * 100;
      if (ds.tm_year < 0)
	return BAD_DATE;
      ds.tm_year += ((date[2] - '0') * 10) + (date[3] - '0');

      ds.tm_mon = mon = (date[5] - '0') * 10 + (date[6] - '0') - 1;
      ds.tm_mday = (date[8] - '0') * 10 + (date[9] - '0');
      timstr = date + 11;
      if (date[19] == 'Z') tz_str = "UTC";
    }
    else if (ap_checkmask(date, "# @$$ ####*")) { /* As 7 Jul 2013  */
	ds.tm_year = ((date[6] - '0') * 10 + (date[7] - '0') - 19) * 100;
	if (ds.tm_year < 0)
	    return BAD_DATE;

	ds.tm_year += ((date[8] - '0') * 10) + (date[9] - '0');
	ds.tm_mday = (date[0] - '0');
	
	monstr = date + 2;
    }
    else if (ap_checkmask(date, "## @$$ ####*")) { /* As 17 Jul 2013  */
	ds.tm_year = ((date[7] - '0') * 10 + (date[8] - '0') - 19) * 100;
	if (ds.tm_year < 0)
	    return BAD_DATE;

	ds.tm_year += ((date[9] - '0') * 10) + (date[10] - '0');
	ds.tm_mday = (date[0] - '0') * 10 + (date[1] - '0');
	monstr = date + 3;
    }
    else if (ap_checkmask(date, "@$$ ~#, ####*")) { /* As Jul 25, 2013  */
	ds.tm_year = ((date[8] - '0') * 10 + (date[9] - '0') - 19) * 100;
	if (ds.tm_year < 0)
	    return BAD_DATE;

	ds.tm_year += ((date[10] - '0') * 10) + (date[11] - '0');
	if (date[4] == ' ')
	    ds.tm_mday = 0;
	else
	    ds.tm_mday = (date[4] - '0') * 10;

	ds.tm_mday += (date[5] - '0');
	monstr = date;
    }
    else if (ap_checkmask(date, "##.##.####*") || ap_checkmask(date, "##/##/####*")) { /* As 08.07.2013 or 08/07/2013  */
	ds.tm_year = ((date[6] - '0') * 10 + (date[7] - '0') - 19) * 100;
	if (ds.tm_year < 0)
	    return BAD_DATE;

	ds.tm_year += ((date[8] - '0') * 10) + (date[9] - '0');
	ds.tm_mon = (date[3] - '0') * 10 + (date[4] - '0');
	if (ds.tm_mon < 1)
	    return BAD_DATE;
	if (ds.tm_mon > 12) {
	    ds.tm_mday = ds.tm_mon;
	    ds.tm_mon = (date[0] - '0') * 10 + (date[1] - '0') - 1;
	    if (ds.tm_mon > 11)
		return BAD_DATE;
	} else {
	    ds.tm_mon--;
	    ds.tm_mday = (date[0] - '0') * 10 + (date[1] - '0');
	}
	monstr = NULL;
    }
    else {


      if ((date = strchr(date, ' ')) == NULL)	/* Find space after weekday */
	return BAD_DATE;

      ++date;			/* Now pointing to first char after space, which should be */
    /* start of the actual date information for all 3 formats. */

      if (ap_checkmask(date, "## @$$ #### ##:##:## *")) {	/* RFC 1123 format */
	ds.tm_year = ((date[7] - '0') * 10 + (date[8] - '0') - 19) * 100;
	if (ds.tm_year < 0)
	    return BAD_DATE;

	ds.tm_year += ((date[9] - '0') * 10) + (date[10] - '0');

	ds.tm_mday = ((date[0] - '0') * 10) + (date[1] - '0');

	monstr = date + 3;
	timstr = date + 12;
	tz_str = date + 21;
      }
      else if (ap_checkmask(date, "##-@$$-## ##:##:## *")) {		/* RFC 850 format  */
	ds.tm_year = ((date[7] - '0') * 10) + (date[8] - '0');
	if (ds.tm_year < 70)
	    ds.tm_year += 100;

	ds.tm_mday = ((date[0] - '0') * 10) + (date[1] - '0');

	monstr = date + 3;
	timstr = date + 10;
	tz_str = date + 19;
      }
      else if (ap_checkmask(date, "##-@$$-#### ##:##:## *")) {		/* RFC 850 "fixed" format (HTTP Cookie format ?)  */
        ds.tm_year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');
	ds.tm_year -= 1900;

	ds.tm_mday = ((date[0] - '0') * 10) + (date[1] - '0');

	monstr = date + 3;
	timstr = date + 12;
	tz_str = date + 21;
      }
      else if (ap_checkmask(date, "@$$ ~# ##:##:## ####*")) {	/* asctime format  */
	ds.tm_year = ((date[16] - '0') * 10 + (date[17] - '0') - 19) * 100;
	if (ds.tm_year < 0)
	    return BAD_DATE;

	ds.tm_year += ((date[18] - '0') * 10) + (date[19] - '0');

	if (date[4] == ' ')
	    ds.tm_mday = 0;
	else
	    ds.tm_mday = (date[4] - '0') * 10;

	ds.tm_mday += (date[5] - '0');

	monstr = date;
	timstr = date + 7;
      }
      else
	return BAD_DATE;
    }

    if (ds.tm_mday <= 0 || ds.tm_mday > 31)
	return BAD_DATE;

    if (timstr != NULL) {
	ds.tm_hour = ((timstr[0] - '0') * 10) + (timstr[1] - '0');
	ds.tm_min = ((timstr[3] - '0') * 10) + (timstr[4] - '0');
	ds.tm_sec = ((timstr[6] - '0') * 10) + (timstr[7] - '0');

	if ((ds.tm_hour > 23) || (ds.tm_min > 59) || (ds.tm_sec > 61))
	    return BAD_DATE;
    }

    if (monstr != NULL) {
      mint = (monstr[0] << 16) | (monstr[1] << 8) | monstr[2];
      for (mon = 0; mon < 12; mon++)
	if (mint == months[mon])
	  break;
      if (mon == 12)
	return BAD_DATE;

      ds.tm_mon = mon;
    }

    if ((ds.tm_mday == 31) && (mon == 3 || mon == 5 || mon == 8 || mon == 10))
      return BAD_DATE;

    /* February gets special check for leapyear */

    if ((mon == 1) &&
	((ds.tm_mday > 29)
	 || ((ds.tm_mday == 29)
	     && ((ds.tm_year & 3)
		 || (((ds.tm_year % 100) == 0)
		     && (((ds.tm_year % 400) != 100)))))))
      return BAD_DATE;

/*    fprintf(stderr, " -- parseHTTPdate: %s is %lu adjusted to %lu\n", date, ap_tm2sec(&ds), dps_tz_adjust(ap_tm2sec(&ds), tz_str));*/

    return dps_tz_adjust(ap_tm2sec(&ds), tz_str);
}

/********
 * END: Above is taken from Apache's util_date.c
 */

time_t DpsFTPDate2Time_t(char *date){
	struct tm ds;

	if (!(ap_checkmask(date+4, "##############*")))
		return BAD_DATE;

/*        strptime(date+6, "%y%m%d%H%M%S", &tm_s); */

	ds.tm_year = (((date[4] - '0') * 10) + (date[5] - '0') - 19)*100;
	ds.tm_year += ((date[6] - '0') * 10) + (date[7] - '0');

	ds.tm_mon = ((date[8] - '0') * 10) + (date[9] - '0') - 1;
	ds.tm_mday = ((date[10] - '0') * 10) + (date[11] - '0');

	ds.tm_hour = ((date[12] - '0') * 10) + (date[13] - '0');
	ds.tm_min = ((date[14] - '0') * 10) + (date[15] - '0');
	ds.tm_sec = ((date[16] - '0') * 10) + (date[17] - '0');

	/* printf("parseFTPdate: %s is %lu\n", date, ap_tm2sec(&ds)); */

	return ap_tm2sec(&ds);
}

time_t Dps_dp2time_t(const char * time_str){
	time_t t=0;
	long i;
	char *s;
	const char *ts;

	if (time_str != NULL && *time_str != '\0') {

	  /* flag telling us that time_str is exactly <num> without any char
	   * so we think it's seconds
	   * flag==0 means not defined yet
	   * flag==1 means ordinary format (XXXmYYYs)
	   * flag==2 means seconds only
	   */
	  int flag=0;
	
	  ts = time_str;
	  do {
		i=strtol(ts, &s, 10);
		if (s==ts){ /* falied to find a number */
			/* FIXME: report error */
			return -1;
		}
		
		/* ignore spaces */
		while (isspace(*s))
			s++;
	    
		switch(*s){
			case 's': /* seconds */
				t+=i; flag=1; break;
			case 'M': /* minutes */
				t+=i*60; flag=1; break;
			case 'h': /* hours */
				t+=i*60*60; flag=1; break;
			case 'd': /* days */
				t+=i*60*60*24; flag=1; break;
			case 'm': /* months */
				/* I assume month is 30 days */
				t+=i*60*60*24*30; flag=1; break;
			case 'y': /* years */
				t+=i*60*60*24*365; flag=1; break;
			case '\0': /* end of string */
				if (flag==1)
					return -1;
				else{
					t=i;
					flag=2;
					return t;
				}    
			default:
				/* FIXME: report error here! */
				return -1;
		}
		/* go to next char */
		ts=++s;
	  /* is it end? */
	  } while (*s);
	}
	
	return t;
}

/* * * * * * * * * * * * *
 * DEBUG CRAP...
 */

/*
#define DEBUG_DP2TIME
*/

#ifdef DEBUG_DP2TIME
int main(int argc, char **argv){

	char *dp;
	
	if (argc>1)
		dp=argv[1];
	else{
		printf("Usage: %s time_string\n", argv[0]);
		return 1;
	}
	
	printf("str='%s'\ttime=%li\n", dp, Dps_dp2time_t(dp));
	return 0;
}
#endif /* DEBUG_DP2TIME */


static char *dps_wday[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *dps_mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void DpsTime_t2HttpStr(time_t t, char *str) {
  struct tm nowtime, *tim;
  register char *s = str;
#ifdef HAVE_GMTIME_R
  gmtime_r(&t, &nowtime);
  tim = &nowtime;
#else
  tim = gmtime(&t);
#endif
/*
  if (!strftime(str, DPS_MAXTIMESTRLEN, "%a, %d %b %Y %H:%M:%S %Z", tim))
   *str='\0';
*/

  if (tim->tm_wday >= 0 && tim->tm_wday < 7) {
    dps_strcpy(s, dps_wday[tim->tm_wday]); s += 3;
  } else {
    *s = '?'; s++;
  }
  dps_strcpy(s, ", "); s += 2;

  if (tim->tm_mday > 0 && tim->tm_mday < 32) {
    register int dig = tim->tm_mday / 10;
    register int dig2 = tim->tm_mday - (dig * 10);
    s[0] = (char)(0x30 + dig);
    s[1] = (char)(0x30 + dig2);
    s[2] = ' ';
    s += 3;
  } else {
    dps_strcpy(s, "?? "); s += 3;
  }

  if (tim->tm_mon >= 0 && tim->tm_mon < 12) {
    dps_strcpy(s, dps_mon[tim->tm_mon]); s += 3;
    *s = ' '; s++;
  } else {
    dps_strcpy(s, "??? "); s += 4;
  }

  {
    register int year = 1900 + tim->tm_year;
    register int dig = year / 1000, dig2, dig3;
    s[0] = (char)(0x30 + dig);
    year -= dig * 1000;
    dig2 = year / 100;
    s[1] = (char)(0x30 + dig2);
    year -= dig2 * 100;
    dig3 = year / 10;
    s[2] = (char)(0x30 + dig3);
    year -= dig3 * 10;
    s[3] = (char)(0x30 + year);
    s[4] = ' '; s += 5;
  }

  if (tim->tm_hour >= 0 && tim->tm_hour < 24) {
    register int dig = tim->tm_hour / 10;
    register int dig2 = tim->tm_hour - (dig * 10);
    s[0] = (char)(0x30 + dig);
    s[1] = (char)(0x30 + dig2);
    s[2] = ':'; s += 3;
  } else {
    dps_strcpy(s, "??:"); s += 3;
  }

  if (tim->tm_min >= 0 && tim->tm_min < 60) {
    register int dig = tim->tm_min / 10;
    register int dig2 = tim->tm_min - (dig * 10);
    s[0] = (char)(0x30 + dig);
    s[1] = (char)(0x30 + dig2);
    s[2] = ':'; s += 3;
  } else {
    dps_strcpy(s, "??:"); s += 3;
  }

  if (tim->tm_sec >= 0 && tim->tm_sec < 60) {
    register int dig = tim->tm_sec / 10;
    register int dig2 = tim->tm_sec - (dig * 10);
    s[0] = (char)(0x30 + dig);
    s[1] = (char)(0x30 + dig2);
    s[2] = ' '; s += 3;
  } else {
    dps_strcpy(s, "?? "); s += 3;
  }

  dps_strcpy(s, "GMT");
}


/* Find out tz_offset for the functions above
 */
int DpsInitTZ(void){

#ifdef HAVE_TM_GMTOFF
	time_t tclock;
	struct tm *tim;
#ifdef HAVE_PTHREAD
	struct tm l_tim;
#endif
        
	tzset();
	tclock = time(NULL);
#ifdef HAVE_PTHREAD
	tim = localtime_r(&tclock, &l_tim);
#else
	tim = localtime(&tclock);
#endif
        tz_offset = (long)tim->tm_gmtoff;
	return 0;
#else      
	time_t tt;
	struct tm t, gmt;
	int days, hours, minutes;

	tzset();
	tt = time(NULL);
#ifdef HAVE_PTHREAD
	(void)gmtime_r(&tt, &gmt);
	(void)localtime_r(&tt, &t);
#else
        gmt = *gmtime(&tt);
        t = *localtime(&tt);
#endif
        days = t.tm_yday - gmt.tm_yday;
        hours = ((days < -1 ? 24 : 1 < days ? -24 : days * 24)
                + t.tm_hour - gmt.tm_hour);
        minutes = hours * 60 + t.tm_min - gmt.tm_min;
        tz_offset = 60L * minutes;
	return 0;
#endif /* HAVE_TM_GMTOFF */

}



__C_LINK int __DPSCALL DpsInit(int argc, char **argv, char **envp) {
#ifdef CHASEN
     char            *chasen_argv[] = { "chasen", /*"-b", "-f",*/ "-F", "%m ", NULL };
     chasen_getopt_argv(chasen_argv, NULL);
#endif
#if defined(WITH_IDNKIT) && !defined(APACHE1) && !defined(APACHE2)
     idn_nameinit(0);
#endif
     ARGV = argv;
     ARGC = argc;
     ENVP = envp;
     DpsInitTZ();
     srandom((unsigned long)time(NULL));
#ifdef WITH_HTTPS
     {
       time_t start_time;
       pid_t pid;
#if OPENSSL_VERSION_NUMBER >= 0x00905100
       while (RAND_status() != 1) {
#endif
	 /* seed in the current time and process id
	  * these are normally 4 bytes each, which should be enough
	  * for our necessary 128-bit minimum seed for the PRNG */
	 start_time = time(NULL);
	 RAND_seed((unsigned char *)&start_time, sizeof(time_t));
	 pid = getpid();
	 RAND_seed((unsigned char *)&pid, sizeof(pid_t));
#if OPENSSL_VERSION_NUMBER >= 0x00905100
       }
#endif
       SSL_library_init();
       SSL_load_error_strings(); 
     }
#endif
     return(0);
}


#ifndef HAVE_VSNPRINTF

#ifndef HAVE_STRNLEN
static int strnlen(const char *s, int max){
	const char *e;
	e=s;
	if (max>=0) {
	    while((*e)&&((e-s)<max))e++;
	} else {
	    while(*e)e++;
	}	
	return(e-s);
}
#endif /* ifndef HAVE_STRNLEN */


/* we use this so that we can do without the ctype library */
#define is_digit(c)	((c) >= '0' && (c) <= '9')
#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })

static int skip_atoi(const char **s){
	int i=0;

	while (is_digit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

static int number(long num, int base, int size, int precision ,int type)
{
	int buflen=0;
	char c,sign,tmp[66];
	const char *digits="0123456789abcdefghijklmnopqrstuvwxyz";
	int i;

	if (type & LARGE)
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num != 0){
		tmp[i++] = digits[(unsigned long) num % (unsigned) base];
                num /= (unsigned) base;
             }
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))
		while(size-->0)
			buflen++;
	if (sign)
		buflen++;
	if (type & SPECIAL) {
		if (base==8)
			buflen++;
		else if (base==16) {
			buflen++;
			buflen++;
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			buflen++;
	while (i < precision--)
		buflen++;
	while (i-- > 0)
		buflen++;
	while (size-- > 0)
		buflen++;
	return buflen;
}

static size_t dps_vsnprintf_len(const char *fmt, va_list args)
{
	size_t buflen=0;
	int len;
	unsigned long num;
	int i, base;
	const char *s;
	int ichar;
	double ifloat;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for (; *fmt ; ++fmt) {
		if (*fmt != '%') {
			buflen++;
			continue;
		}
			
		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}
		
		/* get field width */
		field_width = -1;
		if (is_digit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;	
			if (is_digit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		/* default base */
		base = 10;

		switch (*fmt) {
		case 'f':
                        if (precision < 0)
                                precision = 6;
                        buflen += precision;
                        if ((flags & SPECIAL) && (precision == 0))
                                buflen++;
                        if (flags & (PLUS || SPACE))
                                buflen++;
                        ifloat = va_arg(args, double);
                        while ((ifloat >= 1.) || (field_width-- > 0)) {
                                ifloat /= base;
                                buflen++;
                        };
 
                        continue;
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					buflen++;
			ichar = va_arg(args, int);
			buflen+= sizeof((unsigned char) ichar);
			while (--field_width > 0)
				buflen++;
			continue;

		case 's':
			s = va_arg(args, char *);
			if (!s)
				s = "<NULL>";

			len = strnlen(s, precision);

			if (!(flags & LEFT))
				while (len < field_width--)
					buflen++;
			for (i = 0; i < len; ++i){
				buflen++;
				s++;
			}
			while (len < field_width--)
				buflen++;
			continue;

		case 'p':
			if (field_width == -1) {
				field_width = 2*sizeof(void *);
				flags |= ZEROPAD;
			}
			buflen += number(
				(long) va_arg(args, void *), 16,
				field_width, precision, flags);
			continue;


		case '%':
			buflen++;
			continue;

		/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;

		case 'X':
			flags |= LARGE;
		case 'x':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			buflen++;
			if (*fmt)
				buflen++;
			else
				--fmt;
			continue;
		}
		if (qualifier == 'l')
			num = va_arg(args, unsigned long);
		else if (qualifier == 'h') {
			num = (unsigned short) va_arg(args, int);
			if (flags & SIGN)
				num = (short) num;
		} else if (flags & SIGN)
			num = va_arg(args, int);
		else
			num = va_arg(args, unsigned int);
		buflen += number((long)num, base, field_width, precision, flags);
	}
	return buflen;
}

int vsnprintf(char *str, size_t size, const char  *fmt,  va_list ap){
	if (dps_vsnprintf_len(fmt, ap)<size){
                return(vsprintf(str, fmt, ap));
        }else{
                dps_strncpy(str, "Oops! Buffer overflow in vsnprintf()", size);
                return(-1);
        }
}
#endif /* ifndef HAVE_VSNPRINTF */



/* This snprintf function has been written
 * by Murray Jensen <Murray.Jensen@cmst.csiro.au>
 * So if it works for you - praise him, 
 * and if it doesn't - blame him...  
 *   --kir.
 */

__C_LINK int __DPSCALL dps_snprintf(char *buf, size_t len, const char *fmt, ...){
#ifndef HAVE_VSNPRINTF
       char *tmpbuf;
       size_t need;
#endif /* ifndef HAVE_VSNPRINTF */
       va_list ap;
       int ret;

       va_start(ap, fmt);

#ifdef HAVE_VSNPRINTF
       ret = vsnprintf(buf, len, fmt, ap);
       buf[len-1] = '\0';
#else

       need = dps_vsnprintf_len(fmt, ap);

       if ((tmpbuf = (char *) DpsMalloc(need + 1)) == NULL)
               dps_strncpy(buf, "Oops! Out of memory in snprintf", len-1);
       else {
               vsprintf(tmpbuf, fmt, ap);
               dps_strncpy(buf, tmpbuf, len-1);
               DPS_FREE(tmpbuf);
       }

       ret = dps_strlen(buf);
#endif /* HAVE_VSNPRINTF */

       va_end(ap);

       return ret;
}

void dps_strerror(DPS_AGENT *Agent, int level, const char *fmt, ...) {
  char buf[1024];
#ifndef HAVE_VSNPRINTF
  char *tmpbuf;
  size_t need;
#endif
  va_list ap;
  int ret;
  int err_no = errno;
#ifdef HAVE_PTHREAD
  char err_str[128];
  (void)strerror_r(err_no, err_str, sizeof(err_str));
#else
  char *err_str = DPS_NULL2EMPTY(strerror(errno));
#endif
       va_start(ap, fmt);

#ifdef HAVE_VSNPRINTF
       ret = vsnprintf(buf, sizeof(buf), fmt, ap);
       buf[sizeof(buf)-1] = '\0';
#else

       need = dps_vsnprintf_len(fmt, ap);

       if ((tmpbuf = (char *) DpsMalloc(need + 1)) == NULL)
	 dps_strncpy(buf, "Oops! Out of memory in dps_strerror", sizeof(buf)-1);
       else {
               vsprintf(tmpbuf, fmt, ap);
               dps_strncpy(buf, tmpbuf, sizeof(buf)-1);
               DPS_FREE(tmpbuf);
       }

#endif /* HAVE_VSNPRINTF */

       va_end(ap);
       
       if (Agent) {
	 DpsLog(Agent, level, "%s - (%d) %s", buf, err_no, err_str);
       } else {
	 fprintf(stderr, "%s - (%d) %s", buf, err_no, err_str);
       }
}


/* Convert NPTR to a double.  If ENDPTR is not NULL, a pointer to the
   character after the last one used in the number is put in *ENDPTR.  */
double dps_strtod (const char *nptr, char **endptr) {
  register const char *s;
  short int sign;

  /* The number so far.  */
  double num;

  int got_dot;			/* Found a decimal point.  */
  int got_digit;		/* Seen any digits.  */

  /* The exponent of the number.  */
  long int exponent;

  if (nptr == NULL) {
      errno = EINVAL;
      goto noconv;
  }

  s = nptr;

  /* Eat whitespace.  */
  while (isspace(*s))  ++s;

  /* Get the sign.  */
  sign = *s == '-' ? -1 : 1;
  if (*s == '-' || *s == '+')
    ++s;

  num = 0.0;
  got_dot = 0;
  got_digit = 0;
  exponent = 0;
  for (;; ++s) {
      if (isdigit(*s)) {
	  got_digit = 1;

	  /* Make sure that multiplication by 10 will not overflow.  */
	  if (num > DBL_MAX * 0.1)
	    /* The value of the digit doesn't matter, since we have already
	       gotten as many digits as can be represented in a `double'.
	       This doesn't necessarily mean the result will overflow.
	       The exponent may reduce it to within range.

	       We just need to record that there was another
	       digit so that we can multiply by 10 later.  */
	    ++exponent;
	  else
	    num = (num * 10.0) + (*s - '0');

	  /* Keep track of the number of digits after the decimal point.
	     If we just divided by 10 here, we would lose precision.  */
	  if (got_dot)
	    --exponent;
      }
      else if (!got_dot && (*s == '.' || *s == ','))
	/* Record that we have found the decimal point.  */
	got_dot = 1;
      else
	/* Any other character terminates the number.  */
	break;
    }

  if (!got_digit)
    goto noconv;

  if (dps_tolower(*s) == 'e') {
      /* Get the exponent specified after the `e' or `E'.  */
      int save = errno;
      char *end;
      long int exp;

      errno = 0;
      ++s;
      exp = strtol (s, &end, 10);
      if (errno == ERANGE) {
	  /* The exponent overflowed a `long int'.  It is probably a safe
	     assumption that an exponent that cannot be represented by
	     a `long int' exceeds the limits of a `double'.  */
	  if (endptr != NULL)
	    *endptr = end;
	  if (exp < 0)
	    goto underflow;
	  else
	    goto overflow;
      }
      else if (end == s)
	/* There was no exponent.  Reset END to point to
	   the 'e' or 'E', so *ENDPTR will be set there.  */
	end = (char *) s - 1;
      errno = save;
      s = end;
      exponent += exp;
  }

  if (endptr != NULL)
    *endptr = (char *) s;

  if (num == 0.0)
    return 0.0;

  /* Multiply NUM by 10 to the EXPONENT power,
     checking for overflow and underflow.  */

  if (exponent < 0) {
      if (num < DBL_MIN * pow (10.0, (double) -exponent))
	goto underflow;
  }
  else if (exponent > 0) {
      if (num > DBL_MAX * pow (10.0, (double) -exponent))
	goto overflow;
  }

  num *= pow (10.0, (double) exponent);

  return num * sign;

overflow:
  /* Return an overflow error.  */
  errno = ERANGE;
  return HUGE_VAL * sign;

underflow:
  /* Return an underflow error.  */
  if (endptr != NULL)
    *endptr = (char *) nptr;
  errno = ERANGE;
  return 0.0;

noconv:
  /* There was no number.  */
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0.0;
}



#ifndef HAVE_HSTRERROR
const char *h_errlist[] = {
    "Error 0",
    "Unknown host",
    "Host name lookup failure",
    "Unknown server error",
    "No address associated with name"
};
#endif





int DpsBuild(char *path, int omode) {
	struct stat sb;
	mode_t numask, oumask;
	int first, last, retval;
	char *p, *pp = DpsStrdup(path);

	p = pp;
	oumask = 0;
	retval = 0;
	if (p[0] == DPSSLASH)		/* Skip leading slashes. */
		++p;
	for (first = 1, last = 0; !last ; ++p) {
		if (p[0] == '\0')
			last = 1;
		else if (p[0] != DPSSLASH)
			continue;
		*p = '\0';
		if (last == 0 && p[1] == '\0')
			last = 1;
		if (first) {
			/*
			 * POSIX 1003.2:
			 * For each dir operand that does not name an existing
			 * directory, effects equivalent to those cased by the
			 * following command shall occcur:
			 *
			 * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
			 *    mkdir [-m mode] dir
			 *
			 * We change the user's umask and then restore it,
			 * instead of doing chmod's.
			 */
			oumask = umask(0);
			numask = oumask & ~((mode_t)(S_IWUSR | S_IXUSR));
			(void)umask(numask);
			first = 0;
		}
		if (last)
			(void)umask(oumask);
		if (stat(path, &sb)) {
			if (errno != ENOENT ||
			    mkdir(path, last ? omode :
				  S_IRWXU | S_IRWXG | S_IRWXO) < 0
				 ){
				/* warn("%s", path); */
				retval = 1;
				break;
			}
		}
		else if ((sb.st_mode & S_IFMT) != S_IFDIR) {
			if (last)
				errno = EEXIST;
			else
				errno = ENOTDIR;
			/* warn("%s", path); */
			retval = 1;
			break;
		}
		if (!last)
			*p = DPSSLASH;
	}
	if (!first && !last)
		(void)umask(oumask);
	DPS_FREE(pp);
	return (retval);
}



char * DpsBuildParamStr(char * dst,size_t len,const char * src,char ** argv,size_t argc){
	const char * s;
	char * d;
	size_t argn;
	size_t curlen;

	*dst='\0';
	s=src;
	d=dst;
	curlen=0;
	
	while(*s){
		if(*s=='$'){
			argn=atoi(s+1);
			if((argn<=argc)&&(argn>0)){
				size_t arglen;
				arglen=dps_strlen(argv[argn-1]);
				if(arglen+curlen+1<len){
					dps_strcpy(d,argv[argn-1]);
					d+=dps_strlen(d);
					curlen+=arglen;
				}else{
					break;
				}
			}
			s++;
			while(*s>='0'&&*s<='9')
				s++;
		}else
		if(*s=='\\'){
			s++;
			if(*s){
				if(curlen+2<len){
					*d=*s;
					s++;
					d++;
					*d='\0';
					curlen++;
				}else{
					break;
				}
			}
		}else
		{
			if(curlen+2<len){
				*d=*s;
				s++;
				d++;
				*d='\0';
				curlen++;
			}else{
				break;
			}
		}
	}
	return(dst);
}

/************************************************/

int DpsSetEnv(const char * name,const char * value){
#ifdef HAVE_SETENV
	return(setenv(name,value,1));
#else
#ifdef HAVE_PUTENV
	int res;
	char * s;
	s=(char*)DpsMalloc(dps_strlen(name)+dps_strlen(value)+3);
	if (s == NULL) return 0;
	sprintf(s,"%s=%s",name,value);
	res=putenv(s);
	DPS_FREE(s);
	return res;
#else
	return 0;
#endif
#endif

}

void DpsUnsetEnv(const char * name){
#ifdef HAVE_UNSETENV
	unsetenv(name);
#else
	DpsSetEnv(name,"");
#endif
}

/****************************************************/

char * DpsStrRemoveChars(char * str, const char * sep){
	char * s, *e;
	int has_sep=0;

	e=s=str;  
	while(*s){
		if(strchr(sep,*s)){
			if(!has_sep){
				e=s;
				has_sep=1;
			}
		}else{
			if(has_sep){
				dps_memmove(e,s,dps_strlen(s)+1);
				s=e;
				has_sep=0;
			}
		}
		s++;
	}
	/* End spaces */
	if(has_sep)*e='\0';
	
	return(str);
}


char * DpsStrRemoveDoubleChars(char * str, const char * sep){
	char * s, *e;
	int has_sep=0;
	
	/* Initial spaces */
	for(s=str;(*s)&&(strchr(sep,*s));s++);
	if (s != str) dps_memmove(str, s, dps_strlen(s) + 1);
	e=s=str;
	
	/* Middle spaces */
	while(*s){
		if(strchr(sep,*s)){
			if(!has_sep){
				e=s;
				has_sep=1;
			}
		}else{
			if(has_sep){
				*e=' ';
				dps_memmove(e + 1, s, dps_strlen(s) + 1);
				s=e+1;
				has_sep=0;
			}
		}
		s++;
	}
	
	/* End spaces */
	if(has_sep)*e='\0';
	
	return(str);
}


void DpsUniRemoveDoubleSpaces(dpsunicode_t *ustr) {
  dpsunicode_t * u, *e;
  int addspace=0;
				
	for(u = e = ustr; *u; u++){
		switch(*u){
			case ' ':
			case HT_INT:
			case CR_INT:
			case NL_INT:
			case 0xA0: /* nbsp */
				addspace = 1;
				break;
			default:
				if(addspace){
					if(e>ustr){
						*e=' ';
						e++;
					}
					addspace=0;
				}
				*e=*u;
				e++;
				break;
		}
	}
	*e=0;
}

void DpsUniPrint(const char *head, dpsunicode_t *ustr) {
	dpsunicode_t * lt;
	fprintf(stderr, "%s: ", head);
	for(lt = ustr; *lt; lt++) {
	  fprintf(stderr, "%04X ", (unsigned int)*lt);
	}
	fprintf(stderr,"\n");
}





static struct flock* file_lock(struct flock *ret, int type, int whence) {
  ret->l_type = (short)type ;
  ret->l_start = 0 ;
  ret->l_whence = (short)whence ;
  ret->l_len = 0 ;
  ret->l_pid = getpid() ;
  return ret ;
}


void DpsWriteLock(int fd) {  /* an exclusive lock on an entire file */


    static struct flock ret ;
    fcntl(fd, F_SETLKW, file_lock(&ret, F_WRLCK, SEEK_SET));

}

void DpsUnLock(int fd) { /* ulock an entire file */

    
	static struct flock ret ;
    fcntl(fd, F_SETLKW, file_lock(&ret, F_UNLCK, SEEK_SET));


}

void DpsReadLock(int fd) {   /* a shared lock on an entire file */

    
    static struct flock ret ;
    fcntl(fd, F_SETLKW, file_lock(&ret, F_RDLCK, SEEK_SET));

}

void DpsReadLockFILE(FILE *f) {   /* a shared lock on an entire file */

    
	static struct flock ret ;
    fcntl(fileno(f), F_SETLKW, file_lock(&ret, F_RDLCK, SEEK_SET));

}

void __DPSCALL DpsWriteLockFILE(FILE *f) {  /* an exclusive lock on an entire file */

    static struct flock ret ;
    fcntl(fileno(f), F_SETLKW, file_lock(&ret, F_WRLCK, SEEK_SET));

}

void __DPSCALL DpsUnLockFILE(FILE *f) { /* ulock an entire file */


    static struct flock ret ;
    fcntl(fileno(f), F_SETLKW, file_lock(&ret, F_UNLCK, SEEK_SET));


}


int ARGC;
char **ARGV;
char **ENVP;

static char *endargv = (char *)NULL, *beginargv = (char*)NULL, *start;
static char **new_environ = NULL;

void DpsDeInit(void) {

#ifdef WITH_HTTPS
    CONF_modules_free();
    ERR_remove_state(0);
    ENGINE_cleanup();
    CONF_modules_unload(1);
    ERR_free_strings();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
#endif
  if (new_environ) {
    size_t i;
    for (i = 0; new_environ[i]; i++) DPS_FREE(new_environ[i]);
    DPS_FREE(new_environ);
  }

}


#ifndef HAVE_SETPROCTITLE

void dps_setproctitle( const char *fmt, ... ) {
  char	*s;
  int		i;
  char	buf[ PATH_MAX + 1024 ];
  va_list	ap;

  if (!ARGC) return;
  
  va_start(ap, fmt);
  buf[sizeof(buf) - 1] = '\0';
  vsnprintf( buf, sizeof(buf) - 1, fmt, ap );
  va_end(ap);

  if ( endargv == (char *)NULL ) {
    for (i = 0; i < ARGC; i++) {
      if (!beginargv) beginargv = ARGV[i];
      if (!endargv || endargv + 1 == ARGV[i]) endargv = ARGV[i] + dps_strlen(ARGV[i]);
    }
    for (i = 0; ENVP[i]; i++) {
      if (!beginargv) beginargv = ENVP[i];
      if (!endargv || endargv + 1 == ENVP[i]) endargv = ENVP[i] + dps_strlen(ENVP[i]);
    }
    new_environ = (char**)DpsMalloc((i + 1) * sizeof(ENVP[0]));
    if (!new_environ) return;
    for (i = 0; ENVP[i]; i++) {
      new_environ[i] = DpsStrdup(ENVP[i]);
    }
    new_environ[i] = NULL;
    s = strrchr(ARGV[0], '/');
    if (s == NULL) s = ARGV[0];
    start = beginargv + dps_snprintf(beginargv, endargv - beginargv, "%s:", s);
    environ = new_environ;
    /* set pointer to end of original argv */
    /*endargv = ARGV[ARGC - 1] + dps_strlen(ARGV[ARGC - 1]) - 1;*/
  }
  s = start;
/*  *s++ = '-';*/
  i = dps_strlen( buf );
  if ( i > endargv - s) {
    i = endargv - s;
    buf[ i ] = '\0';
  }
  dps_strcpy( s, buf );
  s += i;
  while ( s <= endargv ) *s++ = '\0';
}
#endif


/* BEGIN OF inet_net_pton implementation */

#ifndef HAVE_INET_NET_PTON_PROTO

/*
 * Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
static int	inet_net_pton_ipv4(const char *src, u_char *dst, size_t size);

/*
 * static int
 * inet_net_pton(af, src, dst, size)
 *	convert network number from presentation to network format.
 *	accepts hex octets, hex strings, decimal octets, and /CIDR.
 *	"size" is in bytes and describes "dst".
 * return:
 *	number of bits, either imputed classfully or specified with /CIDR,
 *	or -1 if some failure occurred (check errno).  ENOENT means it was
 *	not a valid network specification.
 * author:
 *	Paul Vixie (ISC), June 1996
 */
int inet_net_pton(int af, const char *src, void *dst, size_t size) {
	switch (af) {
	case AF_INET:
		return (inet_net_pton_ipv4(src, dst, size));
	default:
		errno = EAFNOSUPPORT;
		return (-1);
	}
}

/*
 * static int
 * inet_net_pton_ipv4(src, dst, size)
 *	convert IPv4 network number from presentation to network format.
 *	accepts hex octets, hex strings, decimal octets, and /CIDR.
 *	"size" is in bytes and describes "dst".
 * return:
 *	number of bits, either imputed classfully or specified with /CIDR,
 *	or -1 if some failure occurred (check errno).  ENOENT means it was
 *	not an IPv4 network specification.
 * note:
 *	network byte order assumed.  this means 192.5.5.240/28 has
 *	0x11110000 in its fourth octet.
 * author:
 *	Paul Vixie (ISC), June 1996
 */
static int inet_net_pton_ipv4(const char *src, u_char *dst, size_t size) {
	static const char
		xdigits[] = "0123456789abcdef",
		digits[] = "0123456789";
	int n, ch, tmp, dirty, bits;
	const u_char *odst = dst;

	ch = *src++;
	if (ch == '0' && (src[0] == 'x' || src[0] == 'X')
	    && isascii(src[1]) && isxdigit(src[1])) {
		/* Hexadecimal: Eat nybble string. */
		if (size <= 0)
			goto emsgsize;
		*dst = 0, dirty = 0;
		src++;	/* skip x or X. */
		while ((ch = *src++) != '\0' &&
		       isascii(ch) && isxdigit(ch)) {
			if (isupper(ch))
				ch = dps_tolower(ch);
			n = strchr(xdigits, ch) - xdigits;
			assert(n >= 0 && n <= 15);
			*dst |= n;
			if (!dirty++)
				*dst <<= 4;
			else if (size-- > 0)
				*++dst = 0, dirty = 0;
			else
				goto emsgsize;
		}
		if (dirty)
			size--;
	} else if (isascii(ch) && isdigit(ch)) {
		/* Decimal: eat dotted digit string. */
		for (;;) {
			tmp = 0;
			do {
				n = strchr(digits, ch) - digits;
				assert(n >= 0 && n <= 9);
				tmp *= 10;
				tmp += n;
				if (tmp > 255)
					goto enoent;
			} while ((ch = *src++) != '\0' &&
				 isascii(ch) && isdigit(ch));
			if (size-- <= 0)
				goto emsgsize;
			*dst++ = (u_char) tmp;
			if (ch == '\0' || ch == '/')
				break;
			if (ch != '.')
				goto enoent;
			ch = *src++;
			if (!isascii(ch) || !isdigit(ch))
				goto enoent;
		}
	} else
		goto enoent;

	bits = -1;
	if (ch == '/' && isascii(src[0]) && isdigit(src[0]) && dst > odst) {
		/* CIDR width specifier.  Nothing can follow it. */
		ch = *src++;	/* Skip over the /. */
		bits = 0;
		do {
			n = strchr(digits, ch) - digits;
			assert(n >= 0 && n <= 9);
			bits *= 10;
			bits += n;
		} while ((ch = *src++) != '\0' && isascii(ch) && isdigit(ch));
		if (ch != '\0')
			goto enoent;
		if (bits > 32)
			goto emsgsize;
	}

	/* Firey death and destruction unless we prefetched EOS. */
	if (ch != '\0')
		goto enoent;

	/* If nothing was written to the destination, we found no address. */
	if (dst == odst)
		goto enoent;
	/* If no CIDR spec was given, infer width from net class. */
	if (bits == -1) {
		if (*odst >= 240)	/* Class E */
			bits = 32;
		else if (*odst >= 224)	/* Class D */
			bits = 4;
		else if (*odst >= 192)	/* Class C */
			bits = 24;
		else if (*odst >= 128)	/* Class B */
			bits = 16;
		else			/* Class A */
			bits = 8;
		/* If imputed mask is narrower than specified octets, widen. */
		if (bits < ((dst - odst) * 8))
			bits = (dst - odst) * 8;
	}
	/* Extend network to cover the actual mask. */
	while (bits > ((dst - odst) * 8)) {
		if (size-- <= 0)
			goto emsgsize;
		*dst++ = '\0';
	}
	return (bits);

 enoent:
	errno = ENOENT;
	return (-1);

 emsgsize:
	errno = EMSGSIZE;
	return (-1);
}

/*
 * Weak aliases for applications that use certain private entry points,
 * and fail to include <arpa/inet.h>.
 */
#undef inet_net_pton
__weak_reference(__inet_net_pton, inet_net_pton);


#endif

/* END OF inet_net_pton implementation */






FILE * dps_fopen(const char *path, const char *mode) {
  struct stat sb;
  FILE *f = fopen(path, mode);
  if (f != NULL) {
    fstat(fileno(f), &sb);
    setvbuf(f, NULL, _IOFBF, (size_t)sb.st_size);
  }
  return f;
}


int dps_demonize(void) {
  char *ptty0;
  char *ptty1;
  char *ptty2;
  char *dev_null = "/dev/null";
  int rc = 0;
#ifndef HAVE_DAEMON
  int fd;
#endif

  if ((ptty0 = ttyname(0)) == NULL) ptty0 = dev_null;
  if ((ptty1 = ttyname(1)) == NULL) ptty1 = dev_null;
  if ((ptty2 = ttyname(2)) == NULL) ptty2 = dev_null;

  signal(SIGHUP, SIG_IGN);
#ifdef HAVE_DAEMON
  rc = daemon(1, 0);
#else
 
  if (fork() != 0)
    exit(0);
  close(0);
  close(1);
  close(2);
  if (setsid() == -1) rc = 1;

  if ((fd = open(dev_null, O_RDONLY)) == -1)    rc |= 2;

  if (dup2(fd, 0) == -1)    rc |= 4;

/*  if (close(fd) == -1)    rc |= 8;*/

  if ((fd = open(ptty1, O_WRONLY)) == -1)    rc |= 16;

  if (dup2(fd, 1) == -1)    rc |= 32;

  if (close(fd) == -1)    rc |= 64;

  if ((fd = open(ptty2, O_WRONLY)) == -1)    rc |= 128;

  if (dup2(fd, 2) == -1)    rc |= 256;

  if (close(fd) == -1)    rc |= 512;
#endif

  return rc;
}


/*
 * Perform a binary search.
 *
*/
void * dps_bsearch(const void *key, const void *base0, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
	const char *base = base0;
	size_t lim;
	int cmp;
	register const void *p;

	if (nmemb == 0) return NULL;
	if ((*compar)(key, base0) < 0) return NULL;
	p = base + (nmemb - 1) * size;
	if ((*compar)(key, p) > 0) return NULL;

	for (lim = nmemb; lim != 0; lim >>= 1) {
		p = base + (lim >> 1) * size;
		cmp = (*compar)(key, p);
		if (cmp == 0)
			return ((void *)p);
		if (cmp > 0) {	/* key > p: move right */
			base = (char *)p + size;
			lim--;
		}		/* else move left */
	}
	return (NULL);
}




/*
 * Heapsort.
 *
*/

int dps_heapsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
  size_t n = nmemb, i = (n / 2), parent, child;
  char *t;
  unsigned char *cbase = (unsigned char*)base;

  if (nmemb < 1 || size < 1) return -1;
  t = DpsMalloc(size + 1);
  if (t == NULL) return -1;

  while(1) {
    if (i > 0) {
      i--;
      dps_memcpy(t, cbase + i * size, size);
    } else {
      n--;
      if (n == 0) {
	DPS_FREE(t);
	return 0;
      }
      dps_memcpy(t, cbase + n * size, size);
      dps_memcpy(cbase + n * size, cbase, size);
    }

    parent = i;
    child = (i * 2) + 1;

    while(child < n) {
      if (child + 1 < n && compar(cbase + (child + 1) * size, cbase + child * size) > 0) {
	child ++;
      }
      if (compar(cbase + (child * size), t) > 0) {
	dps_memcpy(cbase + parent * size, cbase + child * size, size);
	parent = child;
	child = parent * 2 + 1;
      } else {
	break;
      }
    }
    dps_memcpy(cbase + parent * size, t, size);
  }
}




#ifdef WITH_PARANOIA

/* NOTE: gcc optimization should be switched off */

#if defined(NO_FRAME_POINTER)
#error --enable-paranoia requires the binary to be compiled with frame pointers
#endif

#define NSAVE 20
/*#define PRINTF_DEBUGINFO*/

#ifdef WITH_SYSLOG
extern char * __progname;

#define SUICIDE(h,i,X, file, line)						\
/*	openlog(__progname,LOG_NDELAY|LOG_PERROR|LOG_PID|LOG_CONS,LOG_USER);*/ \
  syslog(LOG_ERR,"{%d} Stack violation %d:%s - exiting [%s:%d]", h, i, X, file, line); \
        closelog();\
        kill(SIGSEGV,getpid());\
        exit(1) ;
#else
#define SUICIDE(h,i,X,file,line)			\
        kill(SIGSEGV,getpid());\
        exit(1) ;
#endif

typedef struct {
  void * save[NSAVE];
  void * saveip[NSAVE];
  unsigned invflag;

#ifdef PARANOIDAL_ROOT
  int paranoidal_check;
#endif

} DPS_PARANOIA_PARAM;


static void **DpsStackFrameNext(void **old_bp) {
    void **new_bp = (void**) *old_bp;
    /* check for bogus new frame pointer */
    if (new_bp <= old_bp) return NULL;
    if ((uintptr_t)new_bp & (sizeof(void*) - 1)) return NULL;
#ifdef __i386__
    if ((uintptr_t)new_bp >= 0xffffe000) return NULL;
#endif
    if ((uintptr_t)new_bp - (uintptr_t)old_bp > 100000) return NULL;
    return new_bp;
}


void * DpsViolationEnter(void *param) { 
    void **bp;
    int i = 0;
    DPS_PARANOIA_PARAM *p = (DPS_PARANOIA_PARAM*)DpsMalloc(sizeof(DPS_PARANOIA_PARAM));
#ifdef __i386__
    bp = (void**)&param - 2;
#elif defined __x86_64__
    bp = (void**) __builtin_frame_address (0);
#else
#error Frame pointer detection is not defined for your architecture
#endif

#if defined(PRINTF_DEBUGINFO)
	printf("\nEnter p: %p  bp:%p  param:%p\n", p, bp, &param);
#endif
	if (p == NULL) exit(2);

	bzero(p, sizeof(DPS_PARANOIA_PARAM));
#ifdef PARANOIDAL_ROOT
	p->paranoidal_check = -1;
#endif

#ifdef PARANOIDAL_ROOT
	if(p->paranoidal_check == -1) 
	  p->paranoidal_check = issetugid() || (!geteuid()) || (!getuid());
	if(!p->paranoidal_check) return (void*)p; 
#endif

	p->invflag++;
	if(p->invflag > 1) return (void*)p;
	bzero(p->save, sizeof(p->save));
	bzero(p->saveip, sizeof(p->saveip));
	bp = DpsStackFrameNext(bp);
	for(i = 0; i < NSAVE && bp != NULL; i++) { 
#if defined(PRINTF_DEBUGINFO)
		printf("saving %p (%i,%d)\n", bp, p->invflag, i);
#endif
		p->save[i] = bp;
		if(!bp) break;
		/* this is bogus, but... Sometimes we got stack entries
		 * points to low addresses. 0x20000000 - is a shared 
		 * library maping start */ 
		if(bp < (void**)0x20000000) {
			p->save[i] = 0;
			break;
		};
		p->saveip[i] = *(bp + 1);
#if defined(PRINTF_DEBUGINFO)
		printf("storing   ip %p : %p \n", p->saveip[i], *(bp + 1));
#endif
		bp = DpsStackFrameNext(bp);
	}
	return (void *)p;
}


void _DpsViolationExit(void *v, const char *filename, int line, int handle) { 
    void **bp;
    int i = 2;
    DPS_PARANOIA_PARAM *p = (DPS_PARANOIA_PARAM*)v;

#ifdef __i386__
    bp = (void**)&v - 2;
#elif defined __x86_64__
    bp = (void**) __builtin_frame_address (0);
#else
#error Frame pointer detection is not defined for your architecture
#endif

#if defined(PRINTF_DEBUGINFO)
	printf("Exit p: %p\n", p);
#endif
#ifdef PARANOIDAL_ROOT
	/* at exit_violation we always have paranoidal_check set to 0 or 1 */
	if(!p->paranoidal_check) {
	  DPS_FREE(p);
	  return; 
	}
#endif

	if(p->invflag > 1) { 
		p->invflag--;
		return;
	};
	bp = DpsStackFrameNext(bp);
	for(i = 0; i < NSAVE && bp != NULL; i++) { 
#if defined(PRINTF_DEBUGINFO)
		printf("processing %p (%i, %d)\n", bp, p->invflag, i);
#endif
		if(!p->save[i]) break;
		if(bp != p->save[i]) { 
		  SUICIDE(handle, 0, "bp", filename, line);
		};
		if(!bp) break;
#if defined(PRINTF_DEBUGINFO)
		printf("restoring ip %p : %x \n", p->saveip[i], *(bp + 1));
#endif
		if(p->saveip[i] != *(bp + 1)) { 
		  SUICIDE(handle, i, "ip", filename, line);
		};
		bp = DpsStackFrameNext(bp);
	};
	p->invflag--;
	if (p->invflag == 0) DPS_FREE(p);
	return;
}


void _DpsViolationCheck(void *v, const char *filename, int line, int handle) { 
    void **bp;
    int i = 2;
    DPS_PARANOIA_PARAM *p = (DPS_PARANOIA_PARAM*)v;
#ifdef __i386__
    bp = (void**)&v - 2;
#elif defined __x86_64__
    bp = (void**) __builtin_frame_address (0);
#else
#error Frame pointer detection is not defined for your architecture
#endif

#if defined(PRINTF_DEBUGINFO)
	printf("Check p: %p\n", p);
#endif
#ifdef PARANOIDAL_ROOT
	/* at exit_violation we always have paranoidal_check set to 0 or 1 */
	if(!p->paranoidal_check) {
	  DPS_FREE(p);
	  return; 
	}
#endif

	bp = DpsStackFrameNext(bp);
	for(i = 0; i < NSAVE; i++) { 
#if defined(PRINTF_DEBUGINFO)
		printf("processing %p (%i, %d)\n", bp, p->invflag, i);
#endif
		if(!p->save[i]) break;
		if(bp != p->save[i]) { 
		  SUICIDE(handle, 0, "bp", filename, line);
		};
		if(!bp) break;
#if defined(PRINTF_DEBUGINFO)
		printf("restoring ip %p : %p \n", p->saveip[i], *(bp + 1));
#endif
		if(p->saveip[i] != *(bp + 1)) { 
		  SUICIDE(handle, i, "ip", filename, line);
		};
		bp = DpsStackFrameNext(bp);
	};
	return;
}



#endif /* WITH_PARANOIA */
