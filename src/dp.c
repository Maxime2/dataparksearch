/* Copyright (C) 2012-2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2010-2011 DataPark Ltd. All rights reserved.

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

#ifdef DPS_CONFIGURE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

#if (SIZEOF_VOIDP == SIZEOF_INT)
#define DPS_PNTYPE unsigned int
#elif (SIZEOF_VOIDP == SIZEOF_LONG)
#define DPS_PNTYPE unsigned long
#elif (SIZEOF_VOIDP == SIZEOF_SHORT)
#define DPS_PNTYPE short
#else
#define DPS_PNTYPE unsigned long long
#endif

static double ticks;
static double getTime(void);

static double getTime(void) {
  struct timeval tv; 
  gettimeofday(&tv, 0); 
  return tv.tv_sec + tv.tv_usec * 1e-6;
}
static void TimerStart(void);
static void TimerStart(void) {
  ticks = getTime();
}
static double TimerEnd(void);
static double TimerEnd(void) {
  return getTime() - ticks;
}
char* copyarr(char*, int);
char* copyarr(char* a0, int N) {
    char* a = (char*)malloc(sizeof(char) * N);
    memcpy(a, a0, sizeof(char) * N);
    return a;
}
char* zeroarr(int);
char* zeroarr(int N) {
    char* a = (char*)malloc(sizeof(char)*N);
    bzero(a, sizeof(char) * N);
    return a;
}

char *dps_strcpy1(char *dst0, const char *src0);
char *dps_strcpy2(char *dst0, const char *src0);
size_t dps_strlen(const char *src);

#else /* DPS_CONFIGURE */

#include "dps_config.h"
#include <stdlib.h>
#include <string.h>
#include "dp.inc"

#define dps_memmove memmove
#define dps_memcpy  memcpy
#define dps_strcpy  strcpy
#define dps_strncpy strncpy
#define dps_strcat  strcat
#define dps_strncat strncat
#define dps_strlen  strlen
#define dps_bsearch bsearch

#endif /* DPS_CONFIGURE */





#undef DPS_MEMMOVE

#include "dps_memcpy.inc"

#define DPS_MEMMOVE

#include "dps_memcpy.inc"


#if defined DPS_USE_STRCPY1_UNALIGNED || defined DPS_CONFIGURE

#if defined DPS_CONFIGURE
char *dps_strcpy1(char *dst0, const char *src0) 
#else
char *dps_strcpy(char *dst0, const char *src0) 
#endif
{
  register char *dst = dst0;
  register const char *src = src0;
  while ((*dst++ = *src++));
  return dst0;
}
#endif /* DPS_USE_STRCPY_ALIGNED */


#if defined DPS_USE_STRCPY2_UNALIGNED || defined DPS_CONFIGURE

#if defined DPS_CONFIGURE
char *dps_strcpy2(char *dst0, const char *src0) 
#else
char *dps_strcpy(char *dst0, const char *src0)
#endif
{
  register size_t n = strlen(src0);
  return memmove(dst0, src0, n + 1);
}
#endif /* DPS_USE_STRCPY_ALIGNED */



#if defined DPS_USE_STRNCPY1_UNALIGNED || defined DPS_CONFIGURE

#if defined DPS_CONFIGURE
char * dps_strncpy1(char *dst0, const char *src0, size_t length)
#else
char * dps_strncpy(char *dst0, const char *src0, size_t length)
#endif
{
  register size_t n = strlen(src0);
  register char *dst = dps_memmove(dst0, src0, (n < length) ? n : length);
  if (n < length) bzero(dst + n, (length - n));
  return dst;
}

#endif



#if defined DPS_USE_STRNCPY2_UNALIGNED || defined DPS_CONFIGURE

typedef long long dps_strncpy_word;  /* up to 32 bytes long */
#define strncpy_wsize sizeof(dps_strncpy_word)
#define strncpy_wmask (strncpy_wsize - 1)

#define dps_minibzero(dst, t) \
	if (t) { dst[0] = '\0'; \
	if (t > 1) { dst[1] = '\0'; \
	if (t > 2) { dst[2] = '\0'; \
	if (t > 3) { dst[3] = '\0'; \
	if (t > 4) { dst[4] = '\0'; \
	if (t > 5) { dst[5] = '\0'; \
	if (t > 6) { dst[6] = '\0'; \
	if (t > 7) { dst[7] = '\0'; \
	if (t > 8 ) { dst[8] = '\0'; \
	if (t > 9) { dst[9] = '\0'; \
	if (t > 10) { dst[10] = '\0'; \
	if (t > 11) { dst[11] = '\0'; \
	if (t > 12) { dst[12] = '\0'; \
	if (t > 13) { dst[13] = '\0'; \
	if (t > 14) { dst[14] = '\0'; \
	if (t > 15) { dst[15] = '\0'; \
	if (t > 16) { dst[16] = '\0'; \
	if (t > 17) { dst[17] = '\0'; \
	if (t > 18) { dst[18] = '\0'; \
	if (t > 19) { dst[19] = '\0'; \
	if (t > 20) { dst[20] = '\0'; \
	if (t > 21) { dst[21] = '\0'; \
	if (t > 22) { dst[22] = '\0'; \
	if (t > 23) { dst[23] = '\0'; \
	if (t > 24) { dst[24] = '\0'; \
	if (t > 25) { dst[25] = '\0'; \
	if (t > 26) { dst[26] = '\0'; \
	if (t > 27) { dst[27] = '\0'; \
	if (t > 28) { dst[28] = '\0'; \
	if (t > 29) { dst[29] = '\0'; \
	if (t > 30) { dst[30] = '\0'; \
	}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}

#if defined DPS_CONFIGURE
char * dps_strncpy2(char *dst0, const char *src0, size_t length)
#else
char * dps_strncpy(char *dst0, const char *src0, size_t length)
#endif
{
  if (length) {
    register size_t n = length / 8;
    register size_t r = (length % 8);
    register char *dst = dst0;
    register const char *src = src0;
    if (r == 0) r = 8; else n++;
    if (!(dst[0] = src[0])) { dst++; src++; goto dps_strncpy_second_pas; }
    if (r > 1) { if (!(dst[1] = src[1])) { dst += 2; src += 2; goto dps_strncpy_second_pas; }
    if (r > 2) { if (!(dst[2] = src[2])) { dst += 3; src += 3; goto dps_strncpy_second_pas; }
    if (r > 3) { if (!(dst[3] = src[3])) { dst += 4; src += 4; goto dps_strncpy_second_pas; }
    if (r > 4) { if (!(dst[4] = src[4])) { dst += 5; src += 5; goto dps_strncpy_second_pas; }
    if (r > 5) { if (!(dst[5] = src[5])) { dst += 6; src += 6; goto dps_strncpy_second_pas; }
    if (r > 6) { if (!(dst[6] = src[6])) { dst += 7; src += 7; goto dps_strncpy_second_pas; }
    if (r > 7) { if (!(dst[7] = src[7])) { dst += 8; src += 8; goto dps_strncpy_second_pas; }
    }}}}}}}
    src += r; dst += r;
    while (--n > 0) {
      if (!(dst[0] = src[0])) { dst++; src++; goto dps_strncpy_second_pas; }
      if (!(dst[1] = src[1])) { dst += 2; src += 2; goto dps_strncpy_second_pas; }
      if (!(dst[2] = src[2])) { dst += 3; src += 3; goto dps_strncpy_second_pas; }
      if (!(dst[3] = src[3])) { dst += 4; src += 4; goto dps_strncpy_second_pas; }
      if (!(dst[4] = src[4])) { dst += 5; src += 5; goto dps_strncpy_second_pas; }
      if (!(dst[5] = src[5])) { dst += 6; src += 6; goto dps_strncpy_second_pas; }
      if (!(dst[6] = src[6])) { dst += 7; src += 7; goto dps_strncpy_second_pas; }
      if (!(dst[7] = src[7])) { dst += 8; src += 8; goto dps_strncpy_second_pas; }
      src += 8; dst += 8;
    }
dps_strncpy_second_pas:
    if (dst < dst0 + length) {
      size_t t, restlen = length - (size_t)(dst - dst0);
      t = (size_t)dst & strncpy_wmask;
      if (t) {
    	if (restlen < strncpy_wsize) {
		t = restlen;
    	} else {
		t = strncpy_wsize - t;
    	}
	dps_minibzero(dst, t);
	restlen -= t;
	dst += t;
      }
      t = restlen / strncpy_wsize;
      if (t) {
	register dps_strncpy_word *wdst = (dps_strncpy_word*)dst;
	n = t / 8;
    	r = (t % 8 );
    	if (r == 0) r = 8; else n++;
    	wdst[0] = (dps_strncpy_word)0;
    	if (r > 1) { wdst[1] = (dps_strncpy_word)0;
    	if (r > 2) { wdst[2] = (dps_strncpy_word)0;
    	if (r > 3) { wdst[3] = (dps_strncpy_word)0;
    	if (r > 4) { wdst[4] = (dps_strncpy_word)0;
    	if (r > 5) { wdst[5] = (dps_strncpy_word)0;
    	if (r > 6) { wdst[6] = (dps_strncpy_word)0;
    	if (r > 7) { wdst[7] = (dps_strncpy_word)0;
	}}}}}}}
    	wdst += r;
    	while (--n > 0) {
    		wdst[0] = (dps_strncpy_word)0;
    		wdst[1] = (dps_strncpy_word)0;
    		wdst[2] = (dps_strncpy_word)0;
    		wdst[3] = (dps_strncpy_word)0;
    		wdst[4] = (dps_strncpy_word)0;
    		wdst[5] = (dps_strncpy_word)0;
    		wdst[6] = (dps_strncpy_word)0;
    		wdst[7] = (dps_strncpy_word)0;
    		wdst += 8;
    	}
 	dst = (char*)wdst;
      }
      if ( (t = (restlen & strncpy_wmask)) ) {
	dps_minibzero(dst, t);
      }
    }
  }
  return dst0;
}

#endif /* DPS_USE_STRNCPY2_ALIGNED */


#if defined DPS_USE_STRCAT_UNALIGNED || defined DPS_CONFIGURE

char * dps_strcat(char *dst0, const char *src0) {
  register size_t n = dps_strlen(dst0);
  strcpy((char*)dst0 + n, src0);
  return dst0;
}
#endif /* DPS_USE_STRCAT_ALIGNED */


#if defined DPS_USE_STRNCAT_UNALIGNED || defined DPS_CONFIGURE

char * dps_strncat(char *dst0, const char *src0, size_t length) {
  register size_t n = dps_strlen(dst0);
  strncpy((char*)dst0 + n, src0, length);
  return dst0;
}
#endif /* DPS_USE_STRNCAT_ALIGNED */


#if defined DPS_USE_STRLEN_UNALIGNED || defined DPS_CONFIGURE

size_t dps_strlen(const char *src) {
#if 1
  register const char *s = src;

  if (s) for (; *s; s++);
  return(s - src);

#else
    size_t len = 0;
    switch(
#if SIZEOF_CHARP == 4
	   (dps_uint4)src & 3
#else
	   (dps_uint8)src & 3
#endif
	   ) {
    case 1: if (*src++ == 0) return len; len++;
    case 2: if (*src++ == 0) return len; len++;
    case 3: if (*src++ == 0) return len; len++;
    default:
      for(;;) {
        dps_uint4 x = *(dps_uint4*)src;
#ifdef WORDS_BIGENDIAN
        if((x & 0xFF000000) == 0) return len;
        if((x & 0xFF0000) == 0) return len + 1;
        if((x & 0xFF00) == 0) return len + 2;
        if((x & 0xFF) == 0) return len + 3;
#else
        if((x & 0xFF) == 0) return len;
        if((x & 0xFF00) == 0) return len + 1;
        if((x & 0xFF0000) == 0) return len + 2;
        if((x & 0xFF000000) == 0) return len + 3;
#endif
        src += 4, len += 4;
      }
    }
    /* this point is reachless. possible compilation warning is OK */
#endif
}

#endif /* DPS_USE_STRLEN_ALIGNED */


#if defined DPS_USE_BSEARCH || defined DPS_CONFIGURE

#ifdef DPS_CONFIGURE
int char_cmp (const void *v1, const void*v2) {
    const char *c1 = (const char *)v1;
    const char *c2 = (const char *)v2;
    if (*c1 < *c2) return -1;
    if (*c1 > *c2) return 1;
    return 0;
}
#endif

void * dps_bsearch(const void *key, const void *base0, size_t nmemb, size_t size, 
		   int (*compar)(const void *, const void*))
{
    const char *base = base0;
    size_t lim;
    int cmp;
    const void *p;

    for (lim = nmemb; lim != 0; lim >>= 1 ) {
	p = base + (lim >> 1) * size;
	cmp = (*compar)(key, p);
	if (cmp == 0)
	    return (void *)p;
	if (cmp > 0) {	/* key > p: move right */
	    base = (char *)p + size;
	    lim--;
	}		/* else move left */
    }
    return NULL;
}

#endif /* DPS_USE_BSEARCH */





#if defined DPS_USE_STRPBRK_UNALIGNED || defined DPS_CONFIGURE

#define UC(a) ((unsigned int)(unsigned char)(a))
#define ADD_NEW_TO_SET(i) (set[inv[i] = idx++] = (i))
#define IS_IN_SET(i) (inv[i] < idx && set[inv[i]] == (i))
#define ADD_TO_SET(i) (void)(IS_IN_SET(i) || ADD_NEW_TO_SET(i))

#if defined DPS_CONFIGURE
char *dps_strpbrk1( const char *s, const char *accept)
#else
char *dps_strpbrk( const char *s, const char *accept)
#endif
{
    unsigned int set[256], inv[256], idx = 0;

    if (s == NULL || accept == NULL) return NULL;
    if (accept[0] == '\0') return NULL;
    if (accept[1] == '\0') return strchr(s, accept[0]);

    for (; *accept != '\0'; accept++) {
	if (IS_IN_SET(UC(*accept))) continue;
	if (ADD_NEW_TO_SET(UC(*accept)) == UC(*s))
	    return ((char *)(void *)(unsigned long)(const void *)(s));
    }

    for (s++; *s != '\0'; s++)
	if (IS_IN_SET(UC(*s)))
	    return ((char *)(void *)(unsigned long)(const void *)(s));
    return NULL;

}

#endif



#ifdef DPS_CONFIGURE


#define STARTLEN 5
#define NOTALIGN 1
#define DPS_MIN(x,y) ((x)<(y)?(x):(y))

#define N 4000 /* array size */
char a0[N + 8];

int main() {
  size_t i, j, z = 0;
  FILE *cfg = fopen("src/dp.inc", "w");
  double t_dps, t_dps2, t_lib;

  for (i=0; i<N; i++) {
    a0[i] = (char)(1 + (rand() % 255));
  }

  {
    char* d = zeroarr(N + 8);
    char* a = copyarr(a0, N + 1);
    a[N] = 0;

    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      dps_memcpy(d, a, i);
      dps_memcpy(a, d, i);
    }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      memcpy(d, a, i);
      memcpy(a, d, i);
    }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_MEMCPY_ALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\n\tmemcpy aligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);

    /***************/

    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	dps_memcpy(d+z, a, i);
	dps_memcpy(a, d+z, i);
      }
    t_dps = TimerEnd();

    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	memcpy(d+z, a, i);
	memcpy(a, d+z, i);
      }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_MEMCPY_UNALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tmemcpy unaligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);

    /* ###################################### */

    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      dps_memmove(d, a, i);
      dps_memmove(a, d, i);
    }
    t_dps = TimerEnd();

    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      memmove(d, a, i);
      memmove(a, d, i);
    }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_MEMMOVE_ALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tmemmove aligned: %s (%g vs %g)\n",  (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);

    /***************/

    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	dps_memmove(d+z, a, i);
	dps_memmove(a, d+z, i);
      }
    t_dps = TimerEnd();

    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	memmove(d+z, a, i);
	memmove(a, d+z, i);
      }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_MEMMOVE_UNALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tmemmove unaligned: %s (%g vs %g)\n",  (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);



    free(d);
    free(a);
  }

  /* ###################################### */

  {
    char* d = zeroarr(N + 8);
    char* a = copyarr(a0, N + 1);
    a[N] = 0;


    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      dps_strcpy1(d, a);
      dps_strcpy1(a, d);
    }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      dps_strcpy2(d, a);
      dps_strcpy2(a, d);
    }
    t_dps2 = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      strcpy(d, a);
      strcpy(a, d);
    }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRCPY%d_ALIGNED%s\n\n", DPS_MIN(t_dps,t_dps2), t_lib,
	    (t_lib < DPS_MIN(t_dps,t_dps2)) ? "/*" : "", (t_dps < t_dps2) ? 1 : 2,
	    (t_lib < DPS_MIN(t_dps,t_dps2)) ? "*/" : ""
	    );
    printf("\tstrcpy aligned: %s (%g vs %g vs %g)\n", 
	   t_lib < DPS_MIN(t_dps,t_dps2) ? "lib" : ((t_dps < t_dps2) ? "dps1" : "dps2"), 
	   t_dps, t_dps2, t_lib);

    /***************/

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	dps_strcpy1(d+z, a+z);
	dps_strcpy1(a+z, d+z);
      }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	dps_strcpy2(d+z, a+z);
	dps_strcpy2(a+z, d+z);
      }
    t_dps2 = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	strcpy(d+z, a+z);
	strcpy(a+z, d+z);
      }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRCPY%d_UNALIGNED%s\n\n", DPS_MIN(t_dps,t_dps2), t_lib,
	    (t_lib < DPS_MIN(t_dps,t_dps2)) ? "/*" : "", (t_dps < t_dps2) ? 1 : 2,
	    (t_lib < DPS_MIN(t_dps,t_dps2)) ? "*/" : ""
	    );

    printf("\tstrcpy unaligned: %s (%g vs %g vs %g)\n", 
	   t_lib < DPS_MIN(t_dps,t_dps2) ? "lib" : ((t_dps < t_dps2) ? "dps1" : "dps2"), 
	   t_dps, t_dps2, t_lib);

    /* ###################################### */

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      dps_strncpy1(d, a, i);
      dps_strncpy1(a, d, i);
    }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      dps_strncpy2(d, a, i);
      dps_strncpy2(a, d, i);
    }
    t_dps2 = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      strncpy(d, a, i);
      strncpy(a, d, i);
    }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRNCPY%d_ALIGNED%s\n\n", DPS_MIN(t_dps,t_dps2), t_lib,
	    (t_lib < DPS_MIN(t_dps,t_dps2)) ? "/*" : "", (t_dps < t_dps2) ? 1 : 2,
	    (t_lib < DPS_MIN(t_dps,t_dps2)) ? "*/" : ""
	    );
    printf("\tstrncpy aligned: %s (%g vs %g vs %g)\n", 
	   t_lib < DPS_MIN(t_dps,t_dps2) ? "lib" : ((t_dps < t_dps2) ? "dps1" : "dps2"), 
	   t_dps, t_dps2, t_lib);

    /***************/

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	dps_strncpy1(d+z, a, i);
	dps_strncpy1(a, d+z, i);
      }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	dps_strncpy2(d+z, a, i);
	dps_strncpy2(a, d+z, i);
      }
    t_dps2 = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N; i > STARTLEN; i--) {
	strncpy(d+z, a, i);
	strncpy(a, d+z, i);
      }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRNCPY%d_UNALIGNED%s\n\n", DPS_MIN(t_dps,t_dps2), t_lib,
	    (t_lib < DPS_MIN(t_dps2,t_dps)) ? "/*" : "", (t_dps < t_dps2) ? 1 : 2,
	    (t_lib < DPS_MIN(t_dps,t_dps2)) ? "*/" : ""
	    );
    printf("\tstrncpy unaligned: %s (%g vs %g vs %g)\n", 
	   t_lib < DPS_MIN(t_dps,t_dps2) ? "lib" : ((t_dps < t_dps2) ? "dps1" : "dps2"), 
	   t_dps, t_dps2, t_lib);








    /* ###################################### */

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N-1; i > STARTLEN; i--) {
      d[0] = '\0';
      dps_strcat(d, a+i);
      a[0] = '\0';
      dps_strcat(a, d + i);
    }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N-1; i > STARTLEN; i--) {
      d[0] = '\0';
      strcat(d, a+i);
      a[0] = '\0';
      strcat(a, d + i);
    }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRCAT_ALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tstrcat aligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);

    /***************/

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N - z - 1; i > STARTLEN; i--) {
	d[0] = '\0';
	dps_strcat(d, a + z + i);
	a[0] = '\0';
	dps_strcat(a, d + z + i);
      }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N - z - 1; i > STARTLEN; i--) {
	d[0] = '\0';
	strcat(d, a + z + i);
	a[0] = '\0';
	strcat(a, d + z + i);
      }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRCAT_UNALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tstrcat unaligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);




    /* ###################################### */

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      d[0] = '\0';
      dps_strncat(d, a, i);
      a[0] = '\0';
      dps_strncat(a, d, i);
    }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (i = N; i > STARTLEN; i--) {
      d[0] = '\0';
      strncat(d, a, i);
      a[0] = '\0';
      strncat(a, d, i);
    }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRNCAT_ALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tstrncat aligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);

    /***************/

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N - z; i > STARTLEN; i--) {
	d[0] = '\0';
	dps_strncat(d, a + z, i);
	a[0] = '\0';
	dps_strncat(a, d+z, i);
      }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    for (z =0; z < 8; z++)
      for (i = N - z; i > STARTLEN; i--) {
	d[0] = '\0';
	strncat(d, a + z, i);
	a[0] = '\0';
	strncat(a, d+z, i);
      }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRNCAT_UNALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tstrncat unaligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);





    /* ###################################### */

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    /*    for (z =0; z < N/8; z++)*/
    for (i = N-1; i > STARTLEN; i--) {
      dps_strlen(a+i);
      /*dps_strlen(d+i);*/
    }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    /*for (z =0; z < N/8; z++)*/
    for (i = N-1; i > STARTLEN; i--) {
      strlen(a+i);
      /*strlen(d+i);*/
    }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRLEN_ALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tstrlen aligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);


    /* ###################################### */

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    char set[4] = { 13, 14, 15, 0}, *res;
    TimerStart();
    /*    for (z =0; z < N/8; z++)*/
    for (i = N-1; i > STARTLEN; i--) {
	res = dps_strpbrk1(a+i, set);
 	res = dps_strpbrk1(set, a+i);
   }
    t_dps = TimerEnd();

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    /*for (z =0; z < N/8; z++)*/
    for (i = N-1; i > STARTLEN; i--) {
	res = strpbrk(a+i, set);
 	res = strpbrk(set, a+i);
   }
    t_lib = TimerEnd();

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_STRPBRK_UNALIGNED%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tstrbprk aligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);


    /* ###################################### */

    free(d); free(a);
    char w[5] = { 1, 2, 3, 4, 5};
    qsort(a0, N, sizeof(*a0), char_cmp);
    d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    /*    for (z =0; z < N/8; z++)*/
    for (i = N-1; i > STARTLEN; i--) {
	dps_bsearch(a+i, a, N, sizeof(*a), char_cmp);
    }
    t_dps = TimerEnd();
    if (dps_bsearch(w+4, w, 5, sizeof(*w), char_cmp) != w+4 || dps_bsearch(w+3, w, 4, sizeof(*w), char_cmp) != w+3) printf("dps_bsearch() is not correct!\n");

    free(d); free(a); d = zeroarr(N + 8); a = copyarr(a0, N + 1);
    TimerStart();
    /*for (z =0; z < N/8; z++)*/
    for (i = N-1; i > STARTLEN; i--) {
	bsearch(a+i, a, N, sizeof(*a), char_cmp);
    }
    t_lib = TimerEnd();
    if (bsearch(w+4, w, 5, sizeof(*w), char_cmp) != w+4 || bsearch(w+3, w, 4, sizeof(*w), char_cmp) != w+3) printf("bsearch() is not correct!\n");

    fprintf(cfg, "/* dps:%g vs. lib:%g */\n%s#define DPS_USE_BSEARCH%s\n\n", t_dps, t_lib,
	    (t_lib < t_dps) ? "/*" : "",
	    (t_lib < t_dps) ? "*/" : ""
	    );
    printf("\tbsearch aligned: %s (%g vs %g)\n", (t_dps < t_lib) ? "dps" : "lib", t_dps, t_lib);




    free(d);
    free(a);
  }

  printf("\t");
  fclose(cfg);
  return 0;
}

#endif



