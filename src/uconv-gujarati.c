/* Copyright (C) 2003-2010 Datapark corp. All rights reserved.
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

#ifdef HAVE_CHARSET_gujarati

static unsigned short tab_gujarati[]={
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x00 */
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x08 */
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x10 */
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x18 */
  0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,  /* 0x20 */
  0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,  /* 0x28 */
  0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,  /* 0x30 */
  0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,  /* 0x38 */
  0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,  /* 0x40 */
  0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,  /* 0x48 */
  0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,  /* 0x50 */
  0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,  /* 0x58 */
  0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,  /* 0x60 */
  0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,  /* 0x68 */
  0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,  /* 0x70 */
  0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0000,  /* 0x78 */
  0x00D7, 0x2212, 0x2013, 0x2014, 0x2018, 0x2019, 0x2026, 0x2022,  /* 0x80 */
  0x00A9, 0x00AE, 0x2122, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0x88 */
  0x0965, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0x90 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0x98 */
  0x0000, 0x0A81, 0x0A82, 0x0A83, 0x0A85, 0x0A86, 0x0A87, 0x0A88,  /* 0xA0 */
  0x0A89, 0x0A8A, 0x0A8B, 0x0000, 0x0A8F, 0x0A90, 0x0A8D, 0x0000,  /* 0xA8 */
  0x0A93, 0x0A94, 0x0A91, 0x0A95, 0x0A96, 0x0A97, 0x0A98, 0x0A99,  /* 0xB0 */
  0x0A9A, 0x0A9B, 0x0A9C, 0x0A9D, 0x0A9E, 0x0A9F, 0x0AA0, 0x0AA1,  /* 0xB8 */
  0x0AA2, 0x0AA3, 0x0AA4, 0x0AA5, 0x0AA6, 0x0AA7, 0x0AA8, 0x0000,  /* 0xC0 */
  0x0AAA, 0x0AAB, 0x0AAC, 0x0AAD, 0x0AAE, 0x0AAF, 0x0000, 0x0AB0,  /* 0xC8 */
  0x0000, 0x0AB2, 0x0AB3, 0x0000, 0x0AB5, 0x0AB6, 0x0AB7, 0x0AB8,  /* 0xD0 */
  0x0AB9, 0x200E, 0x0ABE, 0x0ABF, 0x0AC0, 0x0AC1, 0x0AC2, 0x0AC3,  /* 0xD8 */
  0x0000, 0x0AC7, 0x0AC8, 0x0AC5, 0x0000, 0x0ACB, 0x0ACC, 0x0AC9,  /* 0xE0 */
  0x0ACD, 0x0ABC, 0x0964, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0xE8 */
  0x0000, 0x0AE6, 0x0AE7, 0x0AE8, 0x0AE9, 0x0AEA, 0x0AEB, 0x0AEC,  /* 0xF0 */
  0x0AED, 0x0AEE, 0x0AEF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0xF8 */
/*   0       1       2       3       4       5       6       7      */
/*   8       9       A       B       C       D       E       F      */
};

static unsigned char tab_0A8F[] = {0xAC, 0xAD, 0xB2};
static unsigned char tab_0A93[] = {
  0xB0, 0xB1, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
  0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0,
  0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6
};
static unsigned char tab_0AC7[] = {0xE1, 0xE2, 0xE7};

int dps_mb_wc_gujarati(DPS_CONV *conv, DPS_CHARSET *cs, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *end) {

  int hi;
  const unsigned char *p;
  unsigned char *e, z;
  unsigned int sw;
  int n;
  
  hi = s[0];
  conv->icodes = conv->ocodes = 1;
  
  if(hi < 0x80) {
    if ( (*s == '&' && ((conv->flags & DPS_RECODE_HTML_FROM) || (conv->flags & DPS_RECODE_URL_FROM)) ) ||
	     (*s == '!' && (conv->flags & DPS_RECODE_URL_FROM)) ) {
	  /*if ((p = strchr(s, ';')) != NULL)*/ {
	    if (s[1] == '#') {
	      p = s + 2;
	      if (s[2] == 'x' || s[2] == 'X') sscanf(s + 3, "%x", &sw);
	      else sscanf(s + 2, "%d", &sw);
	      *pwc = (dpsunicode_t)sw;
	    } else {
	      p = s + 1;
	      if (!(conv->flags & DPS_RECODE_TEXT_FROM)) {
		for(e = s + 1 ; (e - s < DPS_MAX_SGML_LEN) && (((*e<='z')&&(*e>='a'))||((*e<='Z')&&(*e>='A'))); e++);
		if (/*!(conv->flags & DPS_RECODE_URL_FROM) ||*/ (*e == ';')) {
		  z = *e;
		  *e = '\0';
		  n = DpsSgmlToUni(s + 1, pwc);
		  if (n == 0) *pwc = 0;
		  else conv->ocodes = n;
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
    pwc[0] = hi;
    return 1;
  }

  if (hi == 0xA1) {
    if ((s + 2 > end) || (s[1] != 0xE9)) {
      pwc[0] = tab_gujarati[0xA1];
      return 1;
    }
    pwc[0] = 0x0AD0;
    return conv->icodes = 2;
  }

  if (hi == 0xAA) {
    if ((s + 2 > end) || (s[1] != 0xE9)) {
      pwc[0] = tab_gujarati[0xAA];
      return 1;
    }
    pwc[0] = 0x0AE0;
    return conv->icodes = 2;
  }

  if (hi == 0xDF) {
    if ((s + 2 > end) || (s[1] != 0xE9)) {
      pwc[0] = tab_gujarati[0xDF];
      return 1;
    }
    pwc[0] = 0x0AC4;
    return conv->icodes = 2;
  }

  if (hi == 0xE8) {
    if ((s + 2 > end) || ((s[1] != 0xE8) &&(s[1] != 0xE9))   ) {
      pwc[0] = tab_gujarati[0xE8];
      return 1;
    }
    pwc[0] = 0x0ACD;
    pwc[1] = (s[1] == 0xE8) ? 0x200C : 0x200D;
    return conv->icodes = conv->ocodes = 2;
  }

  pwc[0] = tab_gujarati[hi];
  return 1;
}

int dps_wc_mb_gujarati(DPS_CONV *conv, DPS_CHARSET *cs, const dpsunicode_t *wc, unsigned char *s, unsigned char *e) {

  conv->icodes = 1;
  conv->ocodes = 1;

  if (*wc < 0x7F) {
    s[0] = *wc;
    if ((conv->flags & DPS_RECODE_HTML_TO) && (strchr(DPS_NULL2EMPTY(conv->CharsToEscape), (int)s[0]) != NULL))
      return DPS_CHARSET_ILUNI;
    if ((conv->flags & DPS_RECODE_URL_TO) && (s[0] == '!')) 
      return DPS_CHARSET_ILUNI;
    return conv->ocodes;
  }

  if (*wc == 0x00A9) {
    s[0] = 0x88;
    return conv->ocodes;
  }
  if (*wc == 0x00AE) {
    s[0] = 0x89;
    return conv->ocodes;
  }
  if (*wc == 0x00D7) {
    s[0] = 0x80;
    return conv->ocodes;
  }
  if (*wc == 0x0964) {
    s[0] = 0xEA;
    return conv->ocodes;
  }
  if (*wc == 0x0965) {
    s[0] = 0x90;
    return conv->ocodes;
  }
  if (*wc >= 0x0A81 && *wc <= 0xA83) {
    s[0] = 0xA1 + (*wc - 0x0A81);
    return conv->ocodes;
  }
  if (*wc >= 0x0A85 && *wc <= 0xA8B) {
    s[0] = 0xA4 + (*wc - 0x0A85);
    return conv->ocodes;
  }
  if (*wc == 0x0A8D) {
    s[0] = 0xAE;
    return conv->ocodes;
  }
  if (*wc >= 0x0A8F && *wc <= 0xA91) {
    s[0] = tab_0A8F[*wc - 0x0A8F];
    return conv->ocodes;
  }
  if (*wc >= 0x0A93 && *wc <= 0xAA8) {
    s[0] = tab_0A93[*wc - 0x0A93];
    return conv->ocodes;
  }
  if (*wc >= 0x0AAA && *wc <= 0xAAF) {
    s[0] = 0xC8 + (*wc - 0x0AAA);
    return conv->ocodes;
  }
  if (*wc == 0x0AB0) {
    s[0] = 0xCF;
    return conv->ocodes;
  }
  if (*wc >= 0x0AB2 && *wc <= 0xAB3) {
    s[0] = 0xD1 + (*wc - 0x0AB2);
    return conv->ocodes;
  }
  if (*wc >= 0x0AB5 && *wc <= 0xAB9) {
    s[0] = 0xD4 + (*wc - 0x0AB5);
    return conv->ocodes;
  }
  if (*wc == 0x0ABC) {
    s[0] = 0xE9;
    return conv->ocodes;
  }
  if (*wc >= 0x0ABE && *wc <= 0xAC3) {
    s[0] = 0xDA + (*wc - 0x0ABE);
    return conv->ocodes;
  }
  
  if (*wc == 0x0AC4) {
    if(s + 2 > e)
      return DPS_CHARSET_TOOSMALL;
    s[0] = 0xAA;
    s[1] = 0xE9;
    return conv->ocodes = 2;
  }

  if (*wc == 0x0AC5) {
    s[0] = 0xE3;
    return conv->ocodes;
  }

  if (*wc >= 0x0AC7 && *wc <= 0xAC9) {
    s[0] = tab_0AC7[*wc - 0x0AC7];
    return conv->ocodes;
  }

  if (*wc >= 0x0ACB && *wc <= 0xACC) {
    s[0] = 0xE5 + (*wc - 0x0ACB);
    return conv->ocodes;
  }

  if (*wc == 0x0ACD) {
    if (wc[1] == 0x200C) {
      if(s + 2 > e)
	return DPS_CHARSET_TOOSMALL;
      conv->icodes = 2;
      s[0] = 0xE8;
      s[1] = 0xE8;
      return conv->ocodes = 2;
    }
    if (wc[1] == 0x200D) {
      if(s + 2 > e)
	return DPS_CHARSET_TOOSMALL;
      conv->icodes = 2;
      s[0] = 0xE8;
      s[1] = 0xE9;
      return conv->ocodes = 2;
    }
    s[0] = 0xE8;
    return conv->ocodes;
  }

  if (*wc == 0x0AD0) {
    if(s + 2 > e)
      return DPS_CHARSET_TOOSMALL;
    s[0] = 0xA1;
    s[1] = 0xE9;
    return conv->ocodes = 2;
  }

  if (*wc == 0x0AE0) {
    if(s + 2 > e)
      return DPS_CHARSET_TOOSMALL;
    s[0] = 0xAA;
    s[1] = 0xE9;
    return conv->ocodes = 2;
  }

  if (*wc >= 0x0AE6 && *wc <= 0xAEF) {
    s[0] = 0xF1 + (*wc - 0x0AE6);
    return conv->ocodes;
  }

  if (*wc == 0x200E) {
    s[0] = 0xD9;
    return conv->ocodes;
  }
  if (*wc == 0x2013) {
    s[0] = 0x82;
    return conv->ocodes;
  }
  if (*wc == 0x2014) {
    s[0] = 0x83;
    return conv->ocodes;
  }
  if (*wc == 0x2018) {
    s[0] = 0x84;
    return conv->ocodes;
  }
  if (*wc == 0x2019) {
    s[0] = 0x85;
    return conv->ocodes;
  }
  if (*wc == 0x2022) {
    s[0] = 0x87;
    return conv->ocodes;
  }
  if (*wc == 0x2026) {
    s[0] = 0x86;
    return conv->ocodes;
  }
  if (*wc == 0x2122) {
    s[0] = 0x8A;
    return conv->ocodes;
  }
  if (*wc == 0x2212) {
    s[0] = 0x81;
  } else {
    s[0] = '?';
    return DPS_CHARSET_ILUNI;
  }

  return conv->ocodes;
}


#endif

#if 0

typedef struct back {
  unsigned char c;
  int u;
} B;

static int cmpB(const void *s1,const void *s2){
  B *b1 = (B*)s1;
  B *b2 = (B*)s2;
  return b1->u - b2->u;
}


int main() {
  B table[256];
  int i;

  for (i = 0; i < 256; i++) {
    table[i].c = i;
    table[i].u = tab_gujarati[i];
  }
  DpsSort((void*)table, 256, sizeof(B), cmpB);

  for (i = 0; i < 256; i++) {
    if (table[i].u)
      printf("%04x %02x %03d\n", table[i].u, table[i].c, table[i].c);
  }
  
}

#endif
