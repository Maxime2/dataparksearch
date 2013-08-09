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

#ifndef DPS_UNICODE_H
#define DPS_UNICODE_H

static const dpsunicode_t dps_uninullstr[] = {0};

#define DPS_UNINULL2EMPTY(x)	((x)?(x):dps_uninullstr)

extern size_t __DPSCALL DpsUniLen(register const dpsunicode_t *u);
int    DpsUniStrCmp(register const dpsunicode_t * s1, register const dpsunicode_t *s2);
int    DpsUniStrCaseCmp(register const dpsunicode_t * s1, register const dpsunicode_t *s2);
int    DpsUniStrBCmp(const dpsunicode_t *s1, const dpsunicode_t *s2);
int    DpsUniStrBNCmp(const dpsunicode_t *s1, const dpsunicode_t *s2, size_t count);
int    DpsUniStrNCaseCmp(const dpsunicode_t *s1, const dpsunicode_t *s2, size_t len);
int    DpsUniStrNCmp(const dpsunicode_t *s1, const dpsunicode_t *s2, size_t len);

dpsunicode_t  *DpsUniStrCpy(dpsunicode_t *dst, const dpsunicode_t *src);
dpsunicode_t  *DpsUniStrRCpy(dpsunicode_t *dst, const dpsunicode_t *src);
dpsunicode_t  *DpsUniStrNCpy(dpsunicode_t *dst, const dpsunicode_t *src, size_t len);
dpsunicode_t  *DpsUniStrCat(dpsunicode_t *s, const dpsunicode_t *append);
dpsunicode_t  *DpsUniDup(const dpsunicode_t *s);
dpsunicode_t  *DpsUniRDup(const dpsunicode_t *s);
dpsunicode_t  *DpsUniNDup(const dpsunicode_t *s, size_t len);
dpsunicode_t  *DpsUniAccentStrip(dpsunicode_t *str);
dpsunicode_t  *DpsUniGermanReplace(dpsunicode_t *str);
dpsunicode_t  *DpsUniStrChr(const dpsunicode_t *str, dpsunicode_t ch);
dpsunicode_t  *DpsUniStrChrLower(const dpsunicode_t *str, dpsunicode_t ch);
dpsunicode_t  *DpsUniRTrim(dpsunicode_t *p, dpsunicode_t *delim);

dpsunicode_t *DpsUniStrTok_SEA(dpsunicode_t *s, dpsunicode_t **last);

#endif
