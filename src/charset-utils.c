/* Copyright (C) 2013 Maxim Zakharov. Al rights reserved.
   Copyright (C) 2005-2012 Datapark corp. All rights reserved.

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
#include "dps_charsetutils.h"
#include "dps_unidata.h"
#include "dps_xmalloc.h"
#include "dps_unicode.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>


__C_LINK int dps_tolower(int c) {
  if (c < 0x80) return (int)DpsUniToLower((dpsunicode_t)c);
  return tolower(c);
}

inline char *dps_strtolower(char *str) {
  register char *s = str;
  if (s != NULL) {
    while(*s) {
      *s = (char*)dps_tolower((int)*s);
      s++;
    }
  }
  return str;
}


void dps_mstr(char *s, const char *src, size_t l1, size_t l2) {
  l1 = l1 < l2 ? l1 : l2;
  dps_memmove(s, src, l1);
  s[l1] = '\0';
}


/* <DSTR> */

DPS_DSTR *DpsDSTRInit(DPS_DSTR *dstr, size_t page_size) {
        if (page_size == 0) return NULL;

        if (dstr == NULL) {
                dstr = DpsXmalloc(sizeof(DPS_DSTR));
                if (! dstr) return(NULL);
                dstr->freeme = 1;
        } else {
                dstr->freeme = 0;
        }

        dstr->data = DpsXmalloc(page_size);
        if (dstr->data == NULL) {
                if (dstr->freeme) DpsFree(dstr);
                return NULL;
        }
        dstr->page_size = page_size;
        dstr->data_size = 0;
        dstr->allocated_size = page_size;
        return dstr;
}

void DpsDSTRFree(DPS_DSTR *dstr) {
        if (dstr->data) DpsFree(dstr->data);
	if (dstr->freeme) DpsFree(dstr);
}

size_t DpsDSTRAppend(DPS_DSTR *dstr, const void *data, size_t append_size) {
        size_t bytes_left = (dstr->allocated_size - dstr->data_size);
        size_t asize;
	char *dstr_data;

        if (data == NULL || append_size == 0) return 0;

        if (bytes_left <= append_size + 2 * sizeof(dpsunicode_t)) {
	  asize = dstr->allocated_size + ((append_size + 2 * sizeof(dpsunicode_t) - bytes_left) / dstr->page_size + 1) * dstr->page_size;
	  dstr->data = DpsRealloc(dstr->data, asize);
	  if (dstr->data == NULL) { dstr->allocated_size = dstr->data_size = 0; return 0; }
	  dstr->allocated_size = asize;
        }
	dstr_data = dstr->data;
        dps_memcpy(dstr_data + dstr->data_size, data, append_size);  /* was: dps_memmove */
        dstr->data_size += append_size;
	dstr_data += dstr->data_size;
	bzero(dstr_data, 2 * sizeof(dpsunicode_t));
        return append_size;
}

size_t DpsDSTRAppendStr(DPS_DSTR *dstr, const char *data) {
  return DpsDSTRAppend(dstr, data, dps_strlen(data));
}

size_t DpsDSTRAppendStrWithSpace(DPS_DSTR *dstr, const char *data) {
  char space[] = { 0x20, 0 };
  size_t rc;
/*  fprintf(stderr, "size:%d Append:%s|\n", dstr->data_size, data);*/
  rc = (dstr->data_size) ? DpsDSTRAppend(dstr, space, 1) : 0;
  rc += DpsDSTRAppend(dstr, data, dps_strlen(data));
  return rc;
}

size_t DpsDSTRAppendUniStr(DPS_DSTR *dstr, const dpsunicode_t *data) {
  return DpsDSTRAppend(dstr, data, sizeof(dpsunicode_t) * DpsUniLen(data));
}

size_t DpsDSTRAppendUni(DPS_DSTR *dstr, const dpsunicode_t data) {
  register dpsunicode_t *dstr_data;
  if (dstr->data_size + sizeof(dpsunicode_t) >= dstr->allocated_size) {
    if ((dstr->data = DpsRealloc(dstr->data, dstr->allocated_size += dstr->page_size)) == NULL) {
      dstr->data_size = dstr->allocated_size = 0;
      return 0;
    }
  }
  dstr_data = (dpsunicode_t*)dstr->data;
  dstr_data[ dstr->data_size / sizeof(dpsunicode_t) ] = data;
  dstr->data_size += sizeof(dpsunicode_t);
  
  return sizeof(dpsunicode_t);
}

size_t DpsDSTRAppendUniWithSpace(DPS_DSTR *dstr, const dpsunicode_t *data) {
  dpsunicode_t space[] = {0x20, 0 };
  size_t rc;
  rc = (dstr->data_size) ? DpsDSTRAppend(dstr, space, sizeof(dpsunicode_t)) : 0;
  rc += DpsDSTRAppend(dstr, data, sizeof(dpsunicode_t) * DpsUniLen(data) );
  return rc;
}



/* </DSTR> */


/*
0009 0000 0000 0000 1001
000A 0000 0000 0000 1010
000D 0000 0000 0000 1101
0020 0000 0000 0010 0000
00A0 0000 0000 1010 0000
1680 0001 0110 1000 0000
2000 0010 0000 0000 0000
2001 0010 0000 0000 0001
2002 0010 0000 0000 0010
2003 0010 0000 0000 0011
2004 0010 0000 0000 0100
2005 0010 0000 0000 0101
2006 0010 0000 0000 0110
2007 0010 0000 0000 0111
2008 0010 0000 0000 1000
2009 0010 0000 0000 1001
200A 0010 0000 0000 1010
200B 0010 0000 0000 1011
202F 0010 0000 0010 1111
2420 0010 0100 0010 0000
3000 0011 0000 0000 0000

303F 0011 0000 0011 1111
---- -------------------
CB50 11?? 1?11 ?1?1 ???? - not space bits

*/
int DpsUniNSpace(dpsunicode_t c) {
     if (c == 0x303F) return 0;
     if (c == 0xFEFF) return 0;
     if (c  & 0xCB50) return 1;
     if (c == 0x0009) return 0;
     if (c == 0x000A) return 0;
     if (c == 0x000D) return 0;
     if (c == 0x0020) return 0;
/*     if (c == 0x0026) return 0;
     if (c == 0x002C) return 0;*/
     if (c == 0x00A0) return 0;
     if (c == 0x1680) return 0;
     if ((c >= 0x2000) && (c <= 0x200B)) return 0;
     if (c == 0x202F) return 0;
     if (c == 0x2420) return 0;
     if (c == 0x3000) return 0;
     return 1;
}

