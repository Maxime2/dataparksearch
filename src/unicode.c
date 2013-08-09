/* Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
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
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_charsetutils.h"

/*static const dpsunicode_t dps_uninullstr[] = {0};*/

/* Calculates UNICODE string length */

size_t __DPSCALL DpsUniLen(register const dpsunicode_t *u) {
#if 1
  register const dpsunicode_t *s;
  for(s = u; *s != (dpsunicode_t)0; s++);
  return (size_t)(s - u);
#else
	register size_t ulen=0;
	while(*u++)ulen++;
	return(ulen);
#endif
}

/* Allocates a copy of unicode string */

dpsunicode_t *DpsUniDup(const dpsunicode_t *s) {
	dpsunicode_t *res;
	size_t size;
	
	size = (DpsUniLen(s)+1)*sizeof(*s);
	if((res=(dpsunicode_t*)DpsMalloc(size)) == NULL)
		return(NULL);
	dps_memcpy(res, s, size); /* was: dps_memmove */
	return res;
}

dpsunicode_t *DpsUniNDup(const dpsunicode_t *s, size_t len) {
	dpsunicode_t *res;
	size_t size = DpsUniLen(s);
	if (size > len) size = len;
	if((res = (dpsunicode_t*)DpsMalloc((size + 1) * sizeof(*s))) == NULL) return(NULL);
	dps_memcpy(res, s, size * sizeof(*s)); /* was: dps_memmove */
	res[size] = 0;
	return res;
}

dpsunicode_t *DpsUniRDup(const dpsunicode_t *s) {
	dpsunicode_t *res;
	size_t size, len;
	
	size = ((len = DpsUniLen(s)) + 1) * sizeof(*s);
	if((res=(dpsunicode_t*)DpsMalloc(size)) == NULL)
		return(NULL);
	{
	  register size_t z;
	  size = len - 1;
	  for (z = 0; z < len; z++) {
	    res[z] = s[size - z];
	  }
	  res[len] = 0;
	}
	return res;
}

/* Compare unicode strings */

int DpsUniStrCmp(const dpsunicode_t * u1, const dpsunicode_t * u2) {
  register dpsunicode_t *s1 = u1, *s2 = u2;
  while (*s1 == *s2) {
    if (*s1 == 0)
      return (0);
    s1++; s2++;
  }
  if (*s1 < *s2) return -1;
  return 1;
}

int DpsUniStrCaseCmp(const dpsunicode_t *u1, const dpsunicode_t * u2) {
  register dpsunicode_t d1, d2, *s1 = u1, *s2 = u2;
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL) return -1;
  if (s2 == NULL) return 1;
  
  do {
    d1 = DpsUniToLower(*s1++);
    d2 = DpsUniToLower(*s2++);
  } while ((d1 != (dpsunicode_t)0) && (d1 == d2));
  if (d1 < d2) return -1;
  if (d1 > d2) return 1;
  return 0;
 }


/* backward unicode string compaire */
int DpsUniStrBCmp(const dpsunicode_t *s1, const dpsunicode_t *s2) { 
  register ssize_t l1 = (ssize_t)DpsUniLen(s1)-1, 
                   l2 = (ssize_t)DpsUniLen(s2)-1;
  while (l1 >= 0 && l2 >= 0) {
    if (s1[l1] < s2[l2]) return -1;
    if (s1[l1] > s2[l2]) return 1;
    l1--;
    l2--;
  }
  if (l1 < l2) return -1;
  if (l1 > l2) return 1;
/*  if (*s1 < *s2) return -1;
  if (*s1 > *s2) return 1;*/
  return 0;
}

int DpsUniStrBNCmp(const dpsunicode_t *s1, const dpsunicode_t *s2, size_t count) { 
  register ssize_t l1 = (ssize_t)DpsUniLen(s1) - 1, 
                   l2 = (ssize_t)DpsUniLen(s2) - 1, 
                    l = (ssize_t)count;
  while (l1 >= 0 && l2 >= 0 && l > 0) {
    if (s1[l1] < s2[l2]) return -1;
    if (s1[l1] > s2[l2]) return 1;
    l1--;
    l2--;
    l--;
  }
  if (l == 0) return 0;
  if (l1 < l2) return -1;
  if (l1 > l2) return 1;
  if (*s1 < *s2) return -1;
  if (*s1 > *s2) return 1;
  return 0;
}

/* string copy */
dpsunicode_t *DpsUniStrCpy(dpsunicode_t *dst, const dpsunicode_t *src) {
/*
  register dpsunicode_t *d = dst; register const dpsunicode_t *s = src;
  while (*s) {
    *d = *s; d++; s++;
  }
  *d = *s;
  return dst;
*/
  register size_t n = DpsUniLen(src) + 1;
  return dps_memmove(dst, src, n * sizeof(*src));
}

dpsunicode_t *DpsUniStrNCpy(dpsunicode_t *dst, const dpsunicode_t *src, size_t len) {
/*
  register dpsunicode_t *d = dst; register const dpsunicode_t *s = src; register size_t l = len;
  while (*s && l) {
    *d = *s; d++; s++;
    l--;
  }
  if (l) *d = *s;
  return dst;
*/
  register size_t n = DpsUniLen(src) + 1;
  return dps_memmove(dst, src, sizeof(*src) * ((n < len) ? n : len));
}

dpsunicode_t *DpsUniStrRCpy(dpsunicode_t *dst, const dpsunicode_t *src) {
  register size_t l = DpsUniLen(src);
  register size_t i; 
  dpsunicode_t *d = dst + l; 
  *d = 0; 
  for (i = 0; i < l; i++) {
    *--d = src[i];
  }
  return dst;
}

/* string append */
dpsunicode_t *DpsUniStrCat(dpsunicode_t *s, const dpsunicode_t *append) {
  size_t len = DpsUniLen(s);
  (void)DpsUniStrCpy(&s[len], append);
  return s;
}

/* Compares two unicode strings, ignore case */
/* Not more than len characters are compared */

int DpsUniStrNCaseCmp(const dpsunicode_t *s1, const dpsunicode_t * s2, size_t len) {
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL) return -1;
  if (s2 == NULL) return 1;
  if(len != 0) {
    register dpsunicode_t d1, d2;
    do {
      d1 = DpsUniToLower(*s1);
      d2 = DpsUniToLower(*s2);
      if (d1 < d2) return -1;
      if (d1 > d2) return 1;
      if (d1 == 0) return 0;
      s1++;
      s2++;
    } while (--len != 0);
  }
  return 0;
}

int DpsUniStrNCmp(const dpsunicode_t *s1, const dpsunicode_t * s2, size_t len) {
  if(len != 0) {
    register dpsunicode_t d1, d2;
    do {
      d1 = *s1;
      d2 = *s2;
      if (d1 < d2) return -1;
      if (d1 > d2) return 1;
      if (d1 == 0) return 0;
      s1++;
      s2++;
    } while (--len != 0);
  }
  return 0;
}


dpsunicode_t *DpsUniAccentStrip(dpsunicode_t *str) {
  dpsunicode_t *nfd, *s, *d;

  s = d = nfd = DpsUniNormalizeNFD(NULL, str);
  while (*s != 0) {
    switch(DpsUniCType(*s)) {
    case DPS_UNI_MARK_N: /*@switchbreak@*/ break;
    default:
      if (s != d) *d = *s;
      d++;
    }
    s++;
  }
  *d = *s;
  return nfd;
}


dpsunicode_t *DpsUniGermanReplace(dpsunicode_t *str) {
  size_t l = DpsUniLen(str);
  dpsunicode_t *german = DpsMalloc((3 * l + 1) * sizeof(*str));
  if (german !=NULL) {
    dpsunicode_t *s = str, *d = german;
    while(*s != 0) {
      switch(*s) {
      case 0x00DF: /* eszett, or scharfes s, small */
	*d++ = (dpsunicode_t)'s'; *d++ = (dpsunicode_t)'s'; break;
      case 0x1E9E: /* eszett, or scharfes s, big */
	*d++ = (dpsunicode_t)'S'; *d++ = (dpsunicode_t)'S'; break;
      case 0x00D6: *d++ = (dpsunicode_t)'O'; *d++ = (dpsunicode_t)'E'; break;
      case 0x00F6: *d++ = (dpsunicode_t)'o'; *d++ = (dpsunicode_t)'e'; break;

      case 0x00DC: *d++ = (dpsunicode_t)'U'; *d++ = (dpsunicode_t)'E'; break;
      case 0x00FC: *d++ = (dpsunicode_t)'u'; *d++ = (dpsunicode_t)'e'; break;

      case 0x00C4: *d++ = (dpsunicode_t)'A'; *d++ = (dpsunicode_t)'E'; break;
      case 0x00E4: *d++ = (dpsunicode_t)'a'; *d++ = (dpsunicode_t)'e'; break;

/* AE */

      case 0x00C6: *d++ = (dpsunicode_t)'A'; *d++ = (dpsunicode_t)'E'; break;
      case 0x00E6: *d++ = (dpsunicode_t)'a'; *d++ = (dpsunicode_t)'e'; break;

/* Long S */
      case 0x017F: *d++ = (dpsunicode_t)'s'; break;


/* Unicode SpecialCasing.txt */

	/*# Preserve canonical equivalence for I with dot. Turkic is handled below.*/
      case 0x0130: *d++ = 0x0069; *d++ = 0x0307; break; /*# LATIN CAPITAL LETTER I WITH DOT ABOVE */

	/*# Ligatures */

      case 0xFB00: *d++ = (dpsunicode_t)'f'; *d++ = (dpsunicode_t)'f'; break; /* # LATIN SMALL LIGATURE FF */
      case 0xFB01: *d++ = (dpsunicode_t)'f'; *d++ = (dpsunicode_t)'i'; break; /* # LATIN SMALL LIGATURE FI */
      case 0xFB02: *d++ = (dpsunicode_t)'f'; *d++ = (dpsunicode_t)'l'; break; /* # LATIN SMALL LIGATURE FL */
      case 0xFB03: *d++ = (dpsunicode_t)'f'; *d++ = (dpsunicode_t)'f'; *d++ = (dpsunicode_t)'i'; break; /* # LATIN SMALL LIGATURE FFI */
      case 0xFB04: *d++ = (dpsunicode_t)'f'; *d++ = (dpsunicode_t)'f'; *d++ = (dpsunicode_t)'l'; break; /* # LATIN SMALL LIGATURE FFL */
      case 0xFB05: *d++ = (dpsunicode_t)'s'; *d++ = (dpsunicode_t)'t'; break; /* # LATIN SMALL LIGATURE LONG S T */
      case 0xFB06: *d++ = (dpsunicode_t)'s'; *d++ = (dpsunicode_t)'t'; break; /* # LATIN SMALL LIGATURE ST */

      case 0x0587: *d++ = 0x0565; *d++ = 0x0582; break; /* # ARMENIAN SMALL LIGATURE ECH YIWN */
      case 0xFB13: *d++ = 0x0574; *d++ = 0x0576; break; /* # ARMENIAN SMALL LIGATURE MEN NOW */
      case 0xFB14: *d++ = 0x0574; *d++ = 0x0565; break; /* # ARMENIAN SMALL LIGATURE MEN ECH */
      case 0xFB15: *d++ = 0x0574; *d++ = 0x056B; break; /* # ARMENIAN SMALL LIGATURE MEN INI */
      case 0xFB16: *d++ = 0x057E; *d++ = 0x0576; break; /* # ARMENIAN SMALL LIGATURE VEW NOW */
      case 0xFB17: *d++ = 0x0574; *d++ = 0x056D; break; /* # ARMENIAN SMALL LIGATURE MEN XEH */


/*# No corresponding uppercase precomposed character*/

      case 0x0149: *d++ = 0x02BC; *d++ = 0x006E; break; /* # LATIN SMALL LETTER N PRECEDED BY APOSTROPHE */
      case 0x0390: *d++ = 0x03B9; *d++ = 0x0308; *d++ = 0x0301; break; /* # GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS */
/*
03B0; 03B0; 03A5 0308 0301; 03A5 0308 0301; # GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS
01F0; 01F0; 004A 030C; 004A 030C; # LATIN SMALL LETTER J WITH CARON
1E96; 1E96; 0048 0331; 0048 0331; # LATIN SMALL LETTER H WITH LINE BELOW
1E97; 1E97; 0054 0308; 0054 0308; # LATIN SMALL LETTER T WITH DIAERESIS
1E98; 1E98; 0057 030A; 0057 030A; # LATIN SMALL LETTER W WITH RING ABOVE
1E99; 1E99; 0059 030A; 0059 030A; # LATIN SMALL LETTER Y WITH RING ABOVE
1E9A; 1E9A; 0041 02BE; 0041 02BE; # LATIN SMALL LETTER A WITH RIGHT HALF RING
1F50; 1F50; 03A5 0313; 03A5 0313; # GREEK SMALL LETTER UPSILON WITH PSILI
1F52; 1F52; 03A5 0313 0300; 03A5 0313 0300; # GREEK SMALL LETTER UPSILON WITH PSILI AND VARIA
1F54; 1F54; 03A5 0313 0301; 03A5 0313 0301; # GREEK SMALL LETTER UPSILON WITH PSILI AND OXIA
1F56; 1F56; 03A5 0313 0342; 03A5 0313 0342; # GREEK SMALL LETTER UPSILON WITH PSILI AND PERISPOMENI
1FB6; 1FB6; 0391 0342; 0391 0342; # GREEK SMALL LETTER ALPHA WITH PERISPOMENI
1FC6; 1FC6; 0397 0342; 0397 0342; # GREEK SMALL LETTER ETA WITH PERISPOMENI
1FD2; 1FD2; 0399 0308 0300; 0399 0308 0300; # GREEK SMALL LETTER IOTA WITH DIALYTIKA AND VARIA
1FD3; 1FD3; 0399 0308 0301; 0399 0308 0301; # GREEK SMALL LETTER IOTA WITH DIALYTIKA AND OXIA
1FD6; 1FD6; 0399 0342; 0399 0342; # GREEK SMALL LETTER IOTA WITH PERISPOMENI
1FD7; 1FD7; 0399 0308 0342; 0399 0308 0342; # GREEK SMALL LETTER IOTA WITH DIALYTIKA AND PERISPOMENI
1FE2; 1FE2; 03A5 0308 0300; 03A5 0308 0300; # GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND VARIA
1FE3; 1FE3; 03A5 0308 0301; 03A5 0308 0301; # GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND OXIA
1FE4; 1FE4; 03A1 0313; 03A1 0313; # GREEK SMALL LETTER RHO WITH PSILI
1FE6; 1FE6; 03A5 0342; 03A5 0342; # GREEK SMALL LETTER UPSILON WITH PERISPOMENI
1FE7; 1FE7; 03A5 0308 0342; 03A5 0308 0342; # GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND PERISPOMENI
1FF6; 1FF6; 03A9 0342; 03A9 0342; # GREEK SMALL LETTER OMEGA WITH PERISPOMENI


# All letters with YPOGEGRAMMENI (iota-subscript) or PROSGEGRAMMENI (iota adscript)
# have special uppercases.
# Note: characters with PROSGEGRAMMENI are actually titlecase, not uppercase!

1F80; 1F80; 1F88; 1F08 0399; # GREEK SMALL LETTER ALPHA WITH PSILI AND YPOGEGRAMMENI
1F81; 1F81; 1F89; 1F09 0399; # GREEK SMALL LETTER ALPHA WITH DASIA AND YPOGEGRAMMENI
1F82; 1F82; 1F8A; 1F0A 0399; # GREEK SMALL LETTER ALPHA WITH PSILI AND VARIA AND YPOGEGRAMMENI
1F83; 1F83; 1F8B; 1F0B 0399; # GREEK SMALL LETTER ALPHA WITH DASIA AND VARIA AND YPOGEGRAMMENI
1F84; 1F84; 1F8C; 1F0C 0399; # GREEK SMALL LETTER ALPHA WITH PSILI AND OXIA AND YPOGEGRAMMENI
1F85; 1F85; 1F8D; 1F0D 0399; # GREEK SMALL LETTER ALPHA WITH DASIA AND OXIA AND YPOGEGRAMMENI
1F86; 1F86; 1F8E; 1F0E 0399; # GREEK SMALL LETTER ALPHA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI
1F87; 1F87; 1F8F; 1F0F 0399; # GREEK SMALL LETTER ALPHA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI
1F88; 1F80; 1F88; 1F08 0399; # GREEK CAPITAL LETTER ALPHA WITH PSILI AND PROSGEGRAMMENI
1F89; 1F81; 1F89; 1F09 0399; # GREEK CAPITAL LETTER ALPHA WITH DASIA AND PROSGEGRAMMENI
1F8A; 1F82; 1F8A; 1F0A 0399; # GREEK CAPITAL LETTER ALPHA WITH PSILI AND VARIA AND PROSGEGRAMMENI
1F8B; 1F83; 1F8B; 1F0B 0399; # GREEK CAPITAL LETTER ALPHA WITH DASIA AND VARIA AND PROSGEGRAMMENI
1F8C; 1F84; 1F8C; 1F0C 0399; # GREEK CAPITAL LETTER ALPHA WITH PSILI AND OXIA AND PROSGEGRAMMENI
1F8D; 1F85; 1F8D; 1F0D 0399; # GREEK CAPITAL LETTER ALPHA WITH DASIA AND OXIA AND PROSGEGRAMMENI
1F8E; 1F86; 1F8E; 1F0E 0399; # GREEK CAPITAL LETTER ALPHA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI
1F8F; 1F87; 1F8F; 1F0F 0399; # GREEK CAPITAL LETTER ALPHA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI
1F90; 1F90; 1F98; 1F28 0399; # GREEK SMALL LETTER ETA WITH PSILI AND YPOGEGRAMMENI
1F91; 1F91; 1F99; 1F29 0399; # GREEK SMALL LETTER ETA WITH DASIA AND YPOGEGRAMMENI
1F92; 1F92; 1F9A; 1F2A 0399; # GREEK SMALL LETTER ETA WITH PSILI AND VARIA AND YPOGEGRAMMENI
1F93; 1F93; 1F9B; 1F2B 0399; # GREEK SMALL LETTER ETA WITH DASIA AND VARIA AND YPOGEGRAMMENI
1F94; 1F94; 1F9C; 1F2C 0399; # GREEK SMALL LETTER ETA WITH PSILI AND OXIA AND YPOGEGRAMMENI
1F95; 1F95; 1F9D; 1F2D 0399; # GREEK SMALL LETTER ETA WITH DASIA AND OXIA AND YPOGEGRAMMENI
1F96; 1F96; 1F9E; 1F2E 0399; # GREEK SMALL LETTER ETA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI
1F97; 1F97; 1F9F; 1F2F 0399; # GREEK SMALL LETTER ETA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI
1F98; 1F90; 1F98; 1F28 0399; # GREEK CAPITAL LETTER ETA WITH PSILI AND PROSGEGRAMMENI
1F99; 1F91; 1F99; 1F29 0399; # GREEK CAPITAL LETTER ETA WITH DASIA AND PROSGEGRAMMENI
1F9A; 1F92; 1F9A; 1F2A 0399; # GREEK CAPITAL LETTER ETA WITH PSILI AND VARIA AND PROSGEGRAMMENI
1F9B; 1F93; 1F9B; 1F2B 0399; # GREEK CAPITAL LETTER ETA WITH DASIA AND VARIA AND PROSGEGRAMMENI
1F9C; 1F94; 1F9C; 1F2C 0399; # GREEK CAPITAL LETTER ETA WITH PSILI AND OXIA AND PROSGEGRAMMENI
1F9D; 1F95; 1F9D; 1F2D 0399; # GREEK CAPITAL LETTER ETA WITH DASIA AND OXIA AND PROSGEGRAMMENI
1F9E; 1F96; 1F9E; 1F2E 0399; # GREEK CAPITAL LETTER ETA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI
1F9F; 1F97; 1F9F; 1F2F 0399; # GREEK CAPITAL LETTER ETA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI
1FA0; 1FA0; 1FA8; 1F68 0399; # GREEK SMALL LETTER OMEGA WITH PSILI AND YPOGEGRAMMENI
1FA1; 1FA1; 1FA9; 1F69 0399; # GREEK SMALL LETTER OMEGA WITH DASIA AND YPOGEGRAMMENI
1FA2; 1FA2; 1FAA; 1F6A 0399; # GREEK SMALL LETTER OMEGA WITH PSILI AND VARIA AND YPOGEGRAMMENI
1FA3; 1FA3; 1FAB; 1F6B 0399; # GREEK SMALL LETTER OMEGA WITH DASIA AND VARIA AND YPOGEGRAMMENI
1FA4; 1FA4; 1FAC; 1F6C 0399; # GREEK SMALL LETTER OMEGA WITH PSILI AND OXIA AND YPOGEGRAMMENI
1FA5; 1FA5; 1FAD; 1F6D 0399; # GREEK SMALL LETTER OMEGA WITH DASIA AND OXIA AND YPOGEGRAMMENI
1FA6; 1FA6; 1FAE; 1F6E 0399; # GREEK SMALL LETTER OMEGA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI
1FA7; 1FA7; 1FAF; 1F6F 0399; # GREEK SMALL LETTER OMEGA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI
1FA8; 1FA0; 1FA8; 1F68 0399; # GREEK CAPITAL LETTER OMEGA WITH PSILI AND PROSGEGRAMMENI
1FA9; 1FA1; 1FA9; 1F69 0399; # GREEK CAPITAL LETTER OMEGA WITH DASIA AND PROSGEGRAMMENI
1FAA; 1FA2; 1FAA; 1F6A 0399; # GREEK CAPITAL LETTER OMEGA WITH PSILI AND VARIA AND PROSGEGRAMMENI
1FAB; 1FA3; 1FAB; 1F6B 0399; # GREEK CAPITAL LETTER OMEGA WITH DASIA AND VARIA AND PROSGEGRAMMENI
1FAC; 1FA4; 1FAC; 1F6C 0399; # GREEK CAPITAL LETTER OMEGA WITH PSILI AND OXIA AND PROSGEGRAMMENI
1FAD; 1FA5; 1FAD; 1F6D 0399; # GREEK CAPITAL LETTER OMEGA WITH DASIA AND OXIA AND PROSGEGRAMMENI
1FAE; 1FA6; 1FAE; 1F6E 0399; # GREEK CAPITAL LETTER OMEGA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI
1FAF; 1FA7; 1FAF; 1F6F 0399; # GREEK CAPITAL LETTER OMEGA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI
1FB3; 1FB3; 1FBC; 0391 0399; # GREEK SMALL LETTER ALPHA WITH YPOGEGRAMMENI
1FBC; 1FB3; 1FBC; 0391 0399; # GREEK CAPITAL LETTER ALPHA WITH PROSGEGRAMMENI
1FC3; 1FC3; 1FCC; 0397 0399; # GREEK SMALL LETTER ETA WITH YPOGEGRAMMENI
1FCC; 1FC3; 1FCC; 0397 0399; # GREEK CAPITAL LETTER ETA WITH PROSGEGRAMMENI
1FF3; 1FF3; 1FFC; 03A9 0399; # GREEK SMALL LETTER OMEGA WITH YPOGEGRAMMENI
1FFC; 1FF3; 1FFC; 03A9 0399; # GREEK CAPITAL LETTER OMEGA WITH PROSGEGRAMMENI

# Some characters with YPOGEGRAMMENI also have no corresponding titlecases

1FB2; 1FB2; 1FBA 0345; 1FBA 0399; # GREEK SMALL LETTER ALPHA WITH VARIA AND YPOGEGRAMMENI
1FB4; 1FB4; 0386 0345; 0386 0399; # GREEK SMALL LETTER ALPHA WITH OXIA AND YPOGEGRAMMENI
1FC2; 1FC2; 1FCA 0345; 1FCA 0399; # GREEK SMALL LETTER ETA WITH VARIA AND YPOGEGRAMMENI
1FC4; 1FC4; 0389 0345; 0389 0399; # GREEK SMALL LETTER ETA WITH OXIA AND YPOGEGRAMMENI
1FF2; 1FF2; 1FFA 0345; 1FFA 0399; # GREEK SMALL LETTER OMEGA WITH VARIA AND YPOGEGRAMMENI
1FF4; 1FF4; 038F 0345; 038F 0399; # GREEK SMALL LETTER OMEGA WITH OXIA AND YPOGEGRAMMENI

1FB7; 1FB7; 0391 0342 0345; 0391 0342 0399; # GREEK SMALL LETTER ALPHA WITH PERISPOMENI AND YPOGEGRAMMENI
1FC7; 1FC7; 0397 0342 0345; 0397 0342 0399; # GREEK SMALL LETTER ETA WITH PERISPOMENI AND YPOGEGRAMMENI
1FF7; 1FF7; 03A9 0342 0345; 03A9 0342 0399; # GREEK SMALL LETTER OMEGA WITH PERISPOMENI AND YPOGEGRAMMENI


# Lithuanian

# Introduce an explicit dot above when lowercasing capital I's and J's
# whenever there are more accents above.
# (of the accents used in Lithuanian: grave, acute, tilde above, and ogonek)

0049; 0069 0307; 0049; 0049; lt More_Above; # LATIN CAPITAL LETTER I
004A; 006A 0307; 004A; 004A; lt More_Above; # LATIN CAPITAL LETTER J
012E; 012F 0307; 012E; 012E; lt More_Above; # LATIN CAPITAL LETTER I WITH OGONEK
00CC; 0069 0307 0300; 00CC; 00CC; lt; # LATIN CAPITAL LETTER I WITH GRAVE
00CD; 0069 0307 0301; 00CD; 00CD; lt; # LATIN CAPITAL LETTER I WITH ACUTE
0128; 0069 0307 0303; 0128; 0128; lt; # LATIN CAPITAL LETTER I WITH TILDE


# Turkish and Azeri

# I and i-dotless; I-dot and i are case pairs in Turkish and Azeri
# The following rules handle those cases.

0130; 0069; 0130; 0130; tr; # LATIN CAPITAL LETTER I WITH DOT ABOVE
0130; 0069; 0130; 0130; az; # LATIN CAPITAL LETTER I WITH DOT ABOVE

# When lowercasing, remove dot_above in the sequence I + dot_above, which will turn into i.
# This matches the behavior of the canonically equivalent I-dot_above

0307; ; 0307; 0307; tr After_I; # COMBINING DOT ABOVE
0307; ; 0307; 0307; az After_I; # COMBINING DOT ABOVE

# When lowercasing, unless an I is before a dot_above, it turns into a dotless i.

0049; 0131; 0049; 0049; tr Not_Before_Dot; # LATIN CAPITAL LETTER I
0049; 0131; 0049; 0049; az Not_Before_Dot; # LATIN CAPITAL LETTER I

# When uppercasing, i turns into a dotted capital I

0069; 0069; 0130; 0130; tr; # LATIN SMALL LETTER I
0069; 0069; 0130; 0130; az; # LATIN SMALL LETTER I

# Note: the following case is already in the UnicodeData file.

# 0131; 0131; 0049; 0049; tr; # LATIN SMALL LETTER DOTLESS I

*/

      default: *d++ = *s;
      }
      s++;
    }
    *d = 0;
  }
  return german;
}


dpsunicode_t *DpsUniStrChr(const dpsunicode_t *str, dpsunicode_t ch) {
        register dpsunicode_t *p = (dpsunicode_t *)str;
        for (;; ++p) {
                if (*p == 0)
                        return (NULL);
                if (*p == ch)
                        return (p);
        }
        /* NOTREACHED */
}


dpsunicode_t *DpsUniStrChrLower(const dpsunicode_t *str, dpsunicode_t ch) { /* ch must be in lower register */
        register dpsunicode_t *p = (dpsunicode_t *)str;

        for (;; ++p) {
                if (*p == 0)
                        return (NULL);
  	        if (DpsUniToLower(*p) == ch)
                        return (p);
        }
        /* NOTREACHED */
}


dpsunicode_t *DpsUniRTrim(dpsunicode_t *p, dpsunicode_t *delim) {
  ssize_t len = (ssize_t)DpsUniLen(p);
  while((len > 0) && (DpsUniStrChr(delim, p[len - 1] ) != NULL)) {
    p[len - 1] = 0;
    len--;
  }
  return p;
}


static int dps_is_delim(const dpsunicode_t *delim, dpsunicode_t c) {
    register const dpsunicode_t *spanp;
    register dpsunicode_t sc;
    for (spanp = delim; (sc = *spanp++) != 0; ) {
	if (c == sc) {
	    return 1;
	}
    }
    return 0;
}




#if 1

dpsunicode_t *DpsUniStrTok_SEA(dpsunicode_t *s, dpsunicode_t **last) {
  dpsunicode_t *tok, prev = 0;

    if (s == NULL && (s = *last) == NULL) return NULL;
    if (*s == 0) return NULL;

    while(*s && (*s == 0x09 || *s == 0x20 || *s == 0xA0)) s++; /* skip leading spaces */
    tok = s;
    while(*s) {
      /*
                  See Unicode UAX#29
    0.2) sot ÷
    0.3) ÷ eot
    3.0) CR × LF
    4.0) (Sep | CR | LF) ÷
    5.0) × [Format Extend]
    6.0) ATerm × Numeric
    7.0) Upper ATerm × Upper
    8.0) ATerm Close* Sp* × [^ OLetter Upper Lower Sep CR LF STerm ATerm]* Lower
    8.1) (STerm | ATerm) Close* Sp* × (SContinue | STerm | ATerm)
    9.0) ( STerm | ATerm ) Close* × ( Close | Sp | Sep | CR | LF )
    10.0) ( STerm | ATerm ) Close* Sp* × ( Sp | Sep | CR | LF )
    11.0) ( STerm | ATerm ) Close* Sp* (Sep | CR | LF)? ÷
    12.0) × Any
    999.0) ÷ Any

      */

      if (*s == 0x0A || dps_isSep(*s)) { /* 4.0), except we break only after 2nd, 3rd, etc. Sep or LF */
	prev = *s++;
	*last = s;
	while(*s && (*s == 0x0A || dps_isSep(*s))) prev = *s++;
	if (s != *last) break;
      } else if (s[1] != '\0' && (dps_isFormat(s[1]) || dps_isExtend(s[1]))) { /* 5.0) */
	prev = *s++;
      } else if (dps_isATerm(*s)) {
	if (dps_isNumeric(s[1])) { /* 6.0) */
	  prev = *s++;
	} else if (dps_isUpper(prev) && dps_isUpper(s[1])) { /* 7.0) */
	  prev = *s++;
	} else {
          prev = *s++;
	  while(*s && dps_isClose(*s)) prev = *s++;
	  while(*s && dps_isSp(*s)) prev = *s++;
	  {
	    register dpsunicode_t *f = s; /* 8.0) */
	    while(*f && !(*f == 0x0A 
		    || *f == 0x0D 
		    || dps_isOLetter(*f) 
		    || dps_isSep(*f) 
		    || dps_isSTerm(*f) 
		    || dps_isATerm(*f) 
		    || dps_isUpper(*f) 
		    || dps_isLower(*f))) 
	      f++;
	    if (dps_isLower(*f)) {
	      prev = *s++; continue;
	    }
	  }
	  if (dps_isSContinue(*s) || dps_isSTerm(*s) || dps_isATerm(*s)) { /* 8.1) */
	    prev = *s++;
	  } else {
	    if (*s == 0x0A || *s == 0x0D || dps_isSep(*s)) s++; /* 11.0) */
	    break;
	  }
	}
      } else if (dps_isSTerm(*s)) {
        prev = *s++;
	while(*s && dps_isClose(*s)) prev = *s++;
	while(*s && dps_isSp(*s)) prev = *s++;
	if (dps_isSContinue(*s) || dps_isSTerm(*s) || dps_isATerm(*s)) { /* 8.1) */
	  prev = *s++;
	} else {
	  if (*s == 0x0A || *s == 0x0D || dps_isSep(*s)) s++; /* 11.0) */
	  break;
	}
      } else {
	prev = *s++;
      }
    }
    *last = s;
    return tok;
}

#else

const dpsunicode_t SentDelim[] = { '.', '!', '?', 0 };


dpsunicode_t *DpsUniStrTok_SEA(dpsunicode_t *s, dpsunicode_t **last) {
    dpsunicode_t *tok;

    if (s == NULL && (s = *last) == NULL) return NULL;


    while(dps_is_delim(delim, *s) == 1) {
      s++;
    }
    if (*s == 0) return *last = NULL; /* no non-delimiter characters */

    tok = s;
    while((*s != 0) && (dps_is_delim(delim, *s) == 0)) {
      s++;
    }
    if (*s != 0) {
      while (dps_is_delim(delim, *s) == 1) {
	s++;
      }
    }
    *last = s;
    return tok;

}
#endif
