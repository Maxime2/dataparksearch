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

#include "dps_config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "dps_uniconv.h"
#include "dps_sgml.h"
#include "dps_charsetutils.h"

/* UTF8 RFC 3629 (was: 2279) */

int dps_mb_wc_utf8 (DPS_CONV *conv, DPS_CHARSET *cs, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *end) {
  unsigned char c = s[0];
  int n = end - s;
  const unsigned char *p;
  unsigned char *e, z;
  unsigned int sw;
  int nn;
  
  conv->icodes = conv->ocodes = 1;

  if (c < 0x80) {
      if ( (*s == '&' && ((conv->flags & DPS_RECODE_HTML_FROM) || (conv->flags & DPS_RECODE_URL_FROM)) ) ||
	   (*s == '!' && (conv->flags & DPS_RECODE_URL_FROM)) ) {
      /*if ((p = strchr(s, ';')) != NULL)*/ {
	      if (s + 1 >= end)
		  return DPS_CHARSET_TOOFEW(0);
	      if (s[1] == '#') {
		  if (s + 2 >= end)
		      return DPS_CHARSET_TOOFEW(0);
		  p = s + 2;
		  if (s[2] == 'x' || s[2] == 'X') sscanf((const char*)s + 3, "%x", &sw);
		  else  sscanf((const char*)s + 2, "%d", &sw);
		  *pwc = (dpsunicode_t)sw;
	      } else {
		  p = s + 1;
		  if (!(conv->flags & DPS_RECODE_TEXT_FROM)) {
		      for(e = (unsigned char*)s + 1 ; (e - s < DPS_MAX_SGML_LEN) && (((*e<='z')&&(*e>='a'))||((*e<='Z')&&(*e>='A'))); e++);
		      if (/*!(conv->flags & DPS_RECODE_URL_FROM) ||*/ (*e == ';')) {
			  z = *e;
			  *e = '\0';
			  nn = DpsSgmlToUni((const char*)s + 1, pwc);
			  if (nn == 0) *pwc = 0;
			  else conv->ocodes = nn;
			  *e = z;
		      } else *pwc = 0;
		  } else *pwc = 0;
	      }
	      if (*pwc) {
		  for (; isalpha(*p) || isdigit(*p); p++);
		  if (*p == ';') p++;
		  return conv->icodes = (p - s /*+ 1*/);
	      }
	  }
      }
      if ( *s == '\\' && (conv->flags & DPS_RECODE_JSON_FROM)) {
	  if (s + 1 >= end)
	      return DPS_CHARSET_TOOFEW(0);
	  n = DpsJSONToUni((const char*)s + 1, pwc, &conv->icodes);
	  if (n) {
	      conv->ocodes = n;
	      return ++conv->icodes;
	  }
      }
      *pwc = c;
      return 1;
  } else if (c < 0xc2) {
    return DPS_CHARSET_ILSEQ;
  } else if ((c & 0xE0) == 0xC0) {
    if (n < 2) return DPS_CHARSET_TOOFEW(0);
    if ((s[1] & 0xC0) != 0x80) return DPS_CHARSET_ILSEQ2;
    *pwc = ((dpsunicode_t) (c & 0x1f) << 6) | (dpsunicode_t) (s[1] & 0x3F);
    return conv->icodes = 2;
  } else if ((c & 0xF0) == 0xE0) {
    if (n < 3) return DPS_CHARSET_TOOFEW(0);
    if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return DPS_CHARSET_ILSEQ3;
    *pwc = ((dpsunicode_t) (c & 0x0F) << 12) | ((dpsunicode_t) (s[1] & 0x3F) << 6) | (dpsunicode_t) (s[2] & 0x3F);
    return conv->icodes = 3;
  } else if (c < 0xf8 && sizeof(dpsunicode_t)*8 >= 32) {
    if (n < 4)return DPS_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (c >= 0xf1 || s[1] >= 0x90))) {
      return DPS_CHARSET_ILSEQ4;
    }
    *pwc = ((dpsunicode_t) (c & 0x07) << 18) | ((dpsunicode_t) (s[1] ^ 0x80) << 12) | ((dpsunicode_t) (s[2] ^ 0x80) << 6) | (dpsunicode_t) (s[3] ^ 0x80);
    return conv->icodes = 4;
  } else if (c < 0xfc && sizeof(dpsunicode_t)*8 >= 32) {
    if (n < 5)return DPS_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40 && (c >= 0xf9 || s[1] >= 0x88)))
      return DPS_CHARSET_ILSEQ5;
    *pwc = ((dpsunicode_t) (c & 0x03) << 24)
      | ((dpsunicode_t) (s[1] ^ 0x80) << 18)
      | ((dpsunicode_t) (s[2] ^ 0x80) << 12)
      | ((dpsunicode_t) (s[3] ^ 0x80) << 6)
      | (dpsunicode_t) (s[4] ^ 0x80);
    return conv->icodes = 5;
  } else if (c < 0xfe && sizeof(dpsunicode_t)*8 >= 32) {
     if (n < 6)return DPS_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
      && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40
      && (s[5] ^ 0x80) < 0x40
      && (c >= 0xfd || s[1] >= 0x84)))
      return DPS_CHARSET_ILSEQ6;
    *pwc = ((dpsunicode_t) (c & 0x01) << 30)
      | ((dpsunicode_t) (s[1] ^ 0x80) << 24)
      | ((dpsunicode_t) (s[2] ^ 0x80) << 18)
      | ((dpsunicode_t) (s[3] ^ 0x80) << 12)
      | ((dpsunicode_t) (s[4] ^ 0x80) << 6)
      | (dpsunicode_t) (s[5] ^ 0x80);
    return conv->icodes = 6;
  } else
    return DPS_CHARSET_ILSEQ;
}



int dps_wc_mb_utf8(DPS_CONV *conv, DPS_CHARSET *cs, const dpsunicode_t *pwc, unsigned char *r, unsigned char *e) {
  int count;
  dpsunicode_t wc = *pwc;

  conv->icodes = conv->ocodes = 1;

  if (wc < 0x80) {
    if ((conv->flags & DPS_RECODE_JSON_TO) && ( (wc > 0 && wc < 0x20) || wc == 0x22 || wc == 0x5C )) {
      return DPS_CHARSET_ILUNI;
    }
    r[0] = (unsigned char)wc;
    if ((conv->flags & DPS_RECODE_HTML_TO) && (strchr(DPS_NULL2EMPTY(conv->CharsToEscape), (int)r[0]) != NULL))
      return DPS_CHARSET_ILUNI;
    if ((conv->flags & DPS_RECODE_URL_TO) && (r[0] == '!')) 
      return DPS_CHARSET_ILUNI;
    return 1;
  }
  else if (wc < 0x800) 
    count = 2;
  else if (wc < 0x10000) 
    count = 3;
  else if (wc < 0x200000) 
    count = 4;
  else if (wc < 0x4000000) 
    count = 5;
  else if (wc <= 0x7fffffff) 
    count = 6;
  else 
    return DPS_CHARSET_ILUNI;
  
  if ( r+count > e)
    return DPS_CHARSET_TOOSMALL;
  
  switch (count) { /* Fall through all cases. */
  case 6: r[5] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x4000000;
  case 5: r[4] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x200000;
  case 4: r[3] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x10000;
  case 3: r[2] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x800;
  case 2: r[1] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0xC0;
  case 1: r[0] = (unsigned char)wc;
  }
  return conv->ocodes = count;
}


/* UTF-16LE RFC2781 */

int dps_mb_wc_utf16le(DPS_CONV *conv, DPS_CHARSET *cs, dpsunicode_t *pwc, const unsigned char *r, const unsigned char *end) {

  dpsunicode_t f, s;

  if (r + 1 >= end) return DPS_CHARSET_TOOFEW(0);
  
  conv->ocodes = 1;
  f = (((dpsunicode_t)r[1]) << 8) + (dpsunicode_t)r[0];

  if ((f & 0xFC00) != 0xD800) {
    *pwc = f;
    return conv->icodes = 2;
  }

  if (r + 3 >= end) return DPS_CHARSET_ILUNI;
  s = (r[3] << 8) + r[2];
  *pwc = ((f & 0x03FF) << 10) + 0x010000;
  if ((s & 0xFC00) != 0xDC00) return DPS_CHARSET_ILUNI;
  *pwc += s & 0x03FF;
  return conv->icodes = 4;

}

int dps_wc_mb_utf16le(DPS_CONV *conv, DPS_CHARSET *cs, const dpsunicode_t *pwc, unsigned char *r, unsigned char *e) {
  dpsunicode_t wc = *pwc, c;

  conv->icodes = 1;

  if (wc < 0x010000) {
    if (r + 2 > e) return DPS_CHARSET_TOOSMALL;
    r[0] =  (unsigned char)(wc & 0xFF);
    r[1] =  (unsigned char)(wc >> 8);
    conv->ocodes = 1;
    return conv->obytes = sizeof(dpsunicode_t);
  }
  if (wc < 0x200000) {
    if (r + 4 > e) return DPS_CHARSET_TOOSMALL;
    c = (0xD800 + (((wc >> 16) - 1) << 6) + ((wc & 0x00FC00) >> 2));
    r[0] = (unsigned char)(c & 0xFF);
    r[1] =  (unsigned char)(c >> 8);
    c   = (0xDC00 + (wc & 0x0003FF));
    r[2] = (unsigned char)(c & 0xFF);
    r[3] =  (unsigned char)(c >> 8);
    conv->ocodes = 2;
    return conv->obytes = 2 * sizeof(dpsunicode_t);
  }
  return DPS_CHARSET_ILUNI;

}


/* UTF-16BE RFC2781 */

int dps_mb_wc_utf16be(DPS_CONV *conv, DPS_CHARSET *cs, dpsunicode_t *pwc, const unsigned char *r, const unsigned char *end) {

  dpsunicode_t f, s;

  if (r + 1 >= end) return DPS_CHARSET_ILUNI;
  
  conv->ocodes = 1;
  f = (((dpsunicode_t)r[0]) << 8) + (dpsunicode_t)r[1];

  if ((f & 0xFC00) != 0xD800) {
    *pwc = f;
    return conv->icodes = 2;
  }

  if (r + 3 >= end) return DPS_CHARSET_ILUNI;
  s = (r[2] << 8) + r[3];
  *pwc = ((f & 0x03FF) << 10) + 0x010000;
  if ((s & 0xFC00) != 0xDC00) return DPS_CHARSET_ILUNI;
  *pwc += s & 0x03FF;
  return conv->icodes = 4;

}

int dps_wc_mb_utf16be(DPS_CONV *conv, DPS_CHARSET *cs, const dpsunicode_t *pwc, unsigned char *r, unsigned char *e) {
  dpsunicode_t wc = *pwc, c;

  conv->icodes = 1;

  if (wc < 0x010000) {
    if (r + 2 > e) return DPS_CHARSET_TOOSMALL;
    r[0] =  (unsigned char)(wc >> 8);
    r[1] =  (unsigned char)(wc & 0xFF);
    conv->ocodes = 1;
    return conv->obytes = sizeof(dpsunicode_t);
  }
  if (wc < 0x200000) {
    if (r + 4 > e) return DPS_CHARSET_TOOSMALL;
    c = (0xD800 + (((wc >> 16) - 1) << 6) + ((wc & 0x00FC00) >> 2));
    r[0] =  (unsigned char)(c >> 8);
    r[1] = (unsigned char)(c & 0xFF);
    c   = (0xDC00 + (wc & 0x0003FF));
    r[2] =  (unsigned char)(c >> 8);
    r[3] = (unsigned char)(c & 0xFF);
    conv->ocodes = 2;
    return conv->obytes = 2 * sizeof(dpsunicode_t);
  }
  return DPS_CHARSET_ILUNI;

}


/* UTF-7 RFC2152 
  Based on: */
/* ================================================================ */
/*
File:   ConvertUTF7.c
Author: David B. Goldsmith
Copyright (C) 1994 Taligent, Inc. All rights reserved.

This code is copyrighted. Under the copyright laws, this code may not
be copied, in whole or part, without prior written consent of Taligent.

Taligent grants the right to use this code as long as this ENTIRE
copyright notice is reproduced in the code.  The code is provided
AS-IS, AND TALIGENT DISCLAIMS ALL WARRANTIES, EITHER EXPRESS OR
IMPLIED, INCLUDING, BUT NOT LIMITED TO IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT
WILL TALIGENT BE LIABLE FOR ANY DAMAGES WHATSOEVER (INCLUDING,
WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS
INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PECUNIARY
LOSS) ARISING OUT OF THE USE OR INABILITY TO USE THIS CODE, EVEN
IF TALIGENT HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
BECAUSE SOME STATES DO NOT ALLOW THE EXCLUSION OR LIMITATION OF
LIABILITY FOR CONSEQUENTIAL OR INCIDENTAL DAMAGES, THE ABOVE
LIMITATION MAY NOT APPLY TO YOU.

RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
government is subject to restrictions as set forth in subparagraph
(c)(l)(ii) of the Rights in Technical Data and Computer Software
clause at DFARS 252.227-7013 and FAR 52.227-19.

This code may be protected by one or more U.S. and International
Patents.

TRADEMARKS: Taligent and the Taligent Design Mark are registered
trademarks of Taligent, Inc.
*/
static char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char direct[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'(),-./:?";
static char optional[] = "!\"#$%&*;<=>@[]^_`{|}";
static char spaces[] = " \x09\x0d\x0a";

static char invbase64[128] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59,
        60, 61, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,
         7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32,
        33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, -1, -1, -1, -1, -1
};
static char mustshiftopt[128] = {
         1,  1,  1,  1,  1,  1,  1,  1,
         1,  0,  0,  1,  1,  0,  1,  1,
         1,  1,  1,  1,  1,  1,  1,  1,
         1,  1,  1,  1,  1,  1,  1,  1,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  1,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  1,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  1,  1
};
static char mustshiftsafe[128] = {
         1,  1,  1,  1,  1,  1,  1,  1,
         1,  0,  0,  1,  1,  0,  1,  1,
         1,  1,  1,  1,  1,  1,  1,  1,
         1,  1,  1,  1,  1,  1,  1,  1,
         0,  1,  1,  1,  1,  1,  1,  0,
         0,  0,  1,  1,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  1,  1,  1,  1,  0,
         1,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  1,  1,  1,  1,  1,
         1,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  1,  1,  1,  1,  1
};


#define SHIFT_IN '+'
#define SHIFT_OUT '-'

#define DECLARE_BIT_BUFFER register dps_uint4 BITbuffer = 0, buffertemp= 0; int bufferbits = 0
#define BITS_IN_BUFFER bufferbits
#define WRITE_N_BITS(x, n) ((BITbuffer |= ( ((x) & ~(-1L<<(n))) << (32-(n)-bufferbits) ) ), bufferbits += (n) )
#define READ_N_BITS(n) ((buffertemp = (BITbuffer >> (32-(n)))), (BITbuffer <<= (n)), (bufferbits -= (n)), buffertemp)
#define TARGETCHECK  {if (r >= e) {return DPS_CHARSET_TOOSMALL;}}

#define verbose 1

int dps_mb_wc_utf7(DPS_CONV *conv, DPS_CHARSET *cs, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *end) {
  int n = end - s;
  const unsigned char *p;
  unsigned char *e, z;
  unsigned int sw;
  int nn;

  DECLARE_BIT_BUFFER;
  int shifted = 0, first = 0, wroteone = 0, base64EOF, base64value, done;
  unsigned int c, prevc;
  unsigned long junk;
  
  conv->icodes = 0; 
  conv->ocodes = 0;
	
  do {
    /* read an ASCII character c */
    if (!(done = (s > end))) {
      c = *s++;
      conv->icodes++;
    }
    if (shifted) {
      /* We're done with a base64 string if we hit EOF, it's not a valid
	 ASCII character, or it's not in the base64 set.
      */
      base64EOF = done || (c > 0x7f) || (base64value = invbase64[c]) < 0;
      if (base64EOF) {
	shifted = 0;
	/* If the character causing us to drop out was SHIFT_IN or
	   SHIFT_OUT, it may be a special escape for SHIFT_IN. The
	   test for SHIFT_IN is not necessary, but allows an alternate
	   form of UTF-7 where SHIFT_IN is escaped by SHIFT_IN. This
	   only works for some values of SHIFT_IN.
	*/
	if (!done && (c == SHIFT_IN || c == SHIFT_OUT)) {
	  /* get another character c */
	  prevc = c;
	  if (!(done = (s > end))) {
	    c = *s++;
	    conv->icodes++;
	  }
	  /* If no base64 characters were encountered, and the
	     character terminating the shift sequence was
	     SHIFT_OUT, then it's a special escape for SHIFT_IN.
	  */
	  if (first && prevc == SHIFT_OUT) {
	    /* write SHIFT_IN unicode */
/*	    TARGETCHECK;*/
	    *pwc++ = (dpsunicode_t)SHIFT_IN;
	    conv->ocodes++;
	  } else if (!wroteone) {
/*	    return conv->icodes = 1;*/
	    return DPS_CHARSET_ILSEQ;
/*	    result = sourceCorrupt;*/
	    /* fprintf(stderr, "UTF7: empty sequence near byte %ld in input\n",
	       source-sourceStart) */;
	  }
	} else if (!wroteone) {
/*	  return conv->icodes = 1;*/
	  return DPS_CHARSET_ILSEQ;
	  /*result = sourceCorrupt;*/
	  /* fprintf(stderr, "UTF7: empty sequence near byte %ld in input\n",
	     source-sourceStart) */;
	}
      } else {
	/* Add another 6 bits of base64 to the bit buffer. */
	WRITE_N_BITS(base64value, 6);
	first = 0;
      }

      /* Extract as many full 16 bit characters as possible from the
	 bit buffer.
      */
      while (BITS_IN_BUFFER >= 16 /*&& (target < targetEnd)*/) {
	/* write a unicode */
	*pwc++ = READ_N_BITS(16);
	conv->ocodes++;
	wroteone = 1;
      }

/*      if (BITS_IN_BUFFER >= 16)
	TARGETCHECK;*/
    
      if (base64EOF) {
	junk = READ_N_BITS(BITS_IN_BUFFER);
	if (junk) {
/*	  return conv->icodes = 1;*/
	  return DPS_CHARSET_ILSEQ;
/*	  result = sourceCorrupt;*/
	  /* fprintf(stderr, "UTF7: non-zero pad bits near byte %ld in input\n",
	     source-sourceStart) */;
	}
      }
    }

    if (!shifted && !done) {
      if (c == SHIFT_IN) {
	shifted = 1;
	first = 1;
	wroteone = 0;
      } else {
	/* It must be a directly encoded character. */
	if (c > 0x7f) {
/*	  return conv->icodes = 1;*/
	  return DPS_CHARSET_ILSEQ;
	  /* fprintf(stderr, "UTF7: non-ASCII character near byte %ld in
	     input\n", source-sourceStart) */;
	}
	/* write a unicode */
	if ( (*s == '&' && ((conv->flags & DPS_RECODE_HTML_FROM) || (conv->flags & DPS_RECODE_URL_FROM)) ) ||
	     (*s == '!' && (conv->flags & DPS_RECODE_URL_FROM)) ) {
	  /*if ((p = strchr(s, ';')) != NULL)*/ {
	    if (s[1] == '#') {
	      p = s + 2;
	      if (s[2] == 'x' || s[2] == 'X') sscanf((const char*)s + 3, "%x", &sw);
	      else  sscanf((const char*)s + 2, "%d", &sw);
	      *pwc = (dpsunicode_t)sw;
	    } else {
	      p = s + 1;
	      if (!(conv->flags & DPS_RECODE_TEXT_FROM)) {
		for(e = (unsigned char*)s + 1 ; (e - s < DPS_MAX_SGML_LEN) && (((*e<='z')&&(*e>='a'))||((*e<='Z')&&(*e>='A'))); e++);
		if (/*!(conv->flags & DPS_RECODE_URL_FROM) ||*/ (*e == ';')) {
		  z = *e;
		  *e = '\0';
		  nn = DpsSgmlToUni((const char*)s + 1, pwc);
		  if (nn == 0) *pwc = 0;
		  else conv->ocodes = nn;
		  *e = z;
		} else *pwc = 0;
	      } else *pwc = 0;
	    }
	    if (*pwc) {
	      for (; isalpha(*p) || isdigit(*p); p++);
	      if (*p == ';') p++;
	      return conv->icodes += (p - s /*+ 1*/);
	    }
	  }
	}
	*pwc++ = c;
	conv->ocodes++;
	return conv->icodes;
/*	TARGETCHECK;
	*target++ = c;*/
      }
    }
  } while (!done);

  return conv->icodes;
}


int dps_wc_mb_utf7(DPS_CONV *conv, DPS_CHARSET *cs, const dpsunicode_t *pwc, unsigned char *r, unsigned char *e) {
/*  int count;*/
  dpsunicode_t wc = *pwc;
  DECLARE_BIT_BUFFER;
  int shifted = 0, needshift = 0, done = 0;
  char *mustshift;

  conv->icodes = conv->ocodes = 0;
/*  count = 0;*/

  if ((conv->flags & DPS_RECODE_HTML_TO) || (conv->flags & DPS_RECODE_URL_TO))
    mustshift = mustshiftsafe;
  else
    mustshift = mustshiftopt;

  do {
    if (!(done = (*pwc != 0))) {
      wc = *pwc++;
      conv->icodes++;
    }

    needshift = (!done && ((wc > 0x7f) || mustshift[wc]));

    if (needshift && !shifted) {
      TARGETCHECK;
      *r++ = SHIFT_IN;
      conv->ocodes++;
      /* Special case handling of the SHIFT_IN character */
      if (wc == (dpsunicode_t)SHIFT_IN) {
	TARGETCHECK;
	*r++ = SHIFT_OUT;
	conv->ocodes++;
      } else shifted = 1;
    }
    
    if (shifted) {
      /* Either write the character to the bit buffer, or pad
	 the bit buffer out to a full base64 character.
      */
      if (needshift)
	WRITE_N_BITS(wc, sizeof(dpsunicode_t));
      else
	WRITE_N_BITS(0, (6 - (BITS_IN_BUFFER % 6))%6);

      /* Flush out as many full base64 characters as possible
	 from the bit buffer.
      */
      while ((r < e) && BITS_IN_BUFFER >= 6) {
	*r++ = base64[READ_N_BITS(6)];
	conv->ocodes++;
      }

      if (BITS_IN_BUFFER >= 6)
	TARGETCHECK;

      if (!needshift) {
	/* Write the explicit shift out character if
	   1) The caller has requested we always do it, or
	   2) The directly encoded character is in the
	   base64 set.
	*/
	if (verbose || ((!done) && invbase64[wc] >= 0)) {
	  TARGETCHECK;
	  *r++ = SHIFT_OUT;
	  conv->ocodes++;
	}
	shifted = 0;
      }
    }

    /* The character can be directly encoded as ASCII. */
    if (!needshift && !done) {
      TARGETCHECK;
      *r++ = (unsigned char) wc;
      conv->ocodes++;
      if ((conv->flags & DPS_RECODE_HTML_TO) && (strchr(DPS_NULL2EMPTY(conv->CharsToEscape), (int)r[0]) != NULL))
	return DPS_CHARSET_ILUNI;
      if ((conv->flags & DPS_RECODE_URL_TO) && (r[0] == '!')) 
	return DPS_CHARSET_ILUNI;
    }
  } while (!done);

  return conv->ocodes;
}
