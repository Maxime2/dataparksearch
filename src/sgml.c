/* Copyright (C) 2003-2011 DataPark Ltd. All right reserved.
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
#include <string.h>

#include "dps_sgml.h"
#include "dps_unicode.h"
#include "dps_charsetutils.h"

typedef struct {
  const char *sgml;
  dpsunicode_t unicode_1;
  dpsunicode_t unicode_2;
} dps_sgml_char;

typedef int (*qsort_cmp)(const void*, const void*);

/*
static const struct dps_sgml_chars {
     const char     *sgml;
     int       unicode;
}
*/

static int dps_sgml_cmp(dps_sgml_char *s1, dps_sgml_char *s2) {
  return strcmp(s1->sgml, s2->sgml);
}

static dps_sgml_char SGMLChars[] = {

#include "sgml.inc"

/* END Marker */
/*{    "",       0    }*/
};  


int DpsSgmlToUni(const char *sgml, dpsunicode_t *wc) {
/*     int i;
     int res;*/
     dps_sgml_char k, *found;
     
     k.sgml = sgml;

     found = bsearch(&k, SGMLChars, sizeof(SGMLChars) / sizeof(dps_sgml_char), sizeof(dps_sgml_char), (qsort_cmp)dps_sgml_cmp);
     if (found == NULL) return 0;
     *wc = found->unicode_1;
     if (found->unicode_2 == 0) return 1;
     wc[1] = found->unicode_2;
     return 2;
}

/** This function replaces SGML entities
    With their character equivalents     
*/

__C_LINK char * __DPSCALL DpsSGMLUnescape(char * str){
  char *s = str,*e = str, c, z;
  int n;
  dpsunicode_t wc[2];

/*****************/     
     while(*s){
          if(*s=='&'){
               if(*(s+1)=='#'){
                    for(e=s+2;(e-s<DPS_MAX_SGML_LEN)&&(*e<='9')&&(*e>='0');e++);
                    if(*e==';'){
                         int v=atoi(s+2);
                         if(v>=0&&v<=255) {
                           *s=(char)v;
                         } else {
                           *s = ' ';
                         }
                         dps_memmove(s+1, e+1, dps_strlen(e + 1) + 1);
                    }
               }else{
                    for(e=s+1;(e-s<DPS_MAX_SGML_LEN)&&(((*e<='z')&&(*e>='a'))||((*e<='Z')&&(*e>='A')));e++);
		    z = *e;
		    *e = '\0';
                    if( z == ';') {
		      if ((n =  DpsSgmlToUni(s+1, wc)) == 1) {
			 c = (char)wc[0];
                         *s = c;
                         dps_memmove(s+1, e+1, dps_strlen(e + 1) + 1);
		      }
                    }
		    if (z != ';') *e = z;
		    else s++;
               }
          }
          s++;
     }
     return(str);
}

/** This function replaces SGML entities
    With their UNICODE   equivalents     
*/
void DpsSGMLUniUnescape(dpsunicode_t *ustr) {
  char sgml[DPS_MAX_SGML_LEN+1];
  int n;
  dpsunicode_t *s = ustr, *e, c[2];

  while (*s){
          if(*s=='&'){
               int i = 0;
               if(*(s+1)=='#'){
                    for(e = s + 2; (e - s < DPS_MAX_SGML_LEN) && (*e <= '9') && (*e >= '0'); e++);
                    if(*e==';'){
                         for(i = 2; s + i < e; i++)
			   sgml[i-2] = (char)s[i];
                         sgml[i-2] = '\0';
                         *s = atoi(sgml);
                         dps_memmove(s + 1, e + 1, sizeof(dpsunicode_t) * (DpsUniLen(e + 1) + 1));
                    }
               }else{
		    for(e=s+1;(e-s<DPS_MAX_SGML_LEN)&&(((*e<='z')&&(*e>='a'))||((*e<='Z')&&(*e>='A')));e++) {
                      sgml[i] = (char)*e;
                      i++;
                    }
		    sgml[i] = '\0';
                    if( *e == ';') {
		      if ((n = DpsSgmlToUni(sgml, c)) > 0) {
                         s[0] = c[0];
			 if (n == 2) s[1] = c[1];
                         dps_memmove(s + n, e + 1, sizeof(dpsunicode_t) * (DpsUniLen(e + 1) + 1));
		      }  
                    }
               }
          }
          s++;
  }
}


int DpsJSONToUni(const char *json, dpsunicode_t *wc, size_t *icodes) {
  switch(*json) {
  case 'b': *wc = 0x08; if (icodes) *icodes = 1; break;
  case 't': *wc = 0x09; if (icodes) *icodes = 1; break;
  case 'n': *wc = 0x0A; if (icodes) *icodes = 1; break;
  case 'f': *wc = 0x0C; if (icodes) *icodes = 1; break;
  case 'r': *wc = 0x0D; if (icodes) *icodes = 1; break;
  case '"': *wc = 0x22; if (icodes) *icodes = 1; break;
  case '/': *wc = 0x2F; if (icodes) *icodes = 1; break;
  case '\\': *wc = 0x5C;if (icodes) *icodes = 1;  break;
  case 'u': {
    register dpsunicode_t dig;
    *wc = 0;
    dig = json[1] - ((json[1] > 0x39) ? 0x36 : 0x30);
    if (dig > 16) return 0;
    *wc += dig * 0x1000;
    dig = json[2] - ((json[2] > 0x39) ? 0x36 : 0x30);
    if (dig > 16) return 0;
    *wc += dig * 0x100;
    dig = json[3] - ((json[3] > 0x39) ? 0x36 : 0x30);
    if (dig > 16) return 0;
    *wc += dig * 0x10;
    dig = json[4] - ((json[4] > 0x39) ? 0x36 : 0x30);
    if (dig > 16) return 0;
    *wc += dig; 
  } if (icodes) *icodes = 5; break;
  default: return 0;
  }
  return 1;
}
