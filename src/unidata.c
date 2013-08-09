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
#include <stdlib.h>
#include <string.h>
#include "dps_unidata.h"
#include "dps_unicode.h"
#include "dps_charsetutils.h"

typedef struct {
	dpsunicode_t tolower;
	unsigned char ctype;
} DPS_UNICODE;
 
typedef struct {
	DPS_UNICODE *table;
	unsigned short ctype;
} DPS_UNI_PLANE;

typedef unsigned char DPS_UNI_COMB;

typedef struct {
        DPS_UNI_COMB *table;
        unsigned char comb;
} DPS_UNI_COMB_PLANE;

typedef struct {
        unsigned short decomp[2];
} DPS_UNI_DECOMP;

typedef DPS_UNI_DECOMP * DPS_UNI_DECOMP_PLANE;

typedef int DPS_UNI_COMPO;
typedef DPS_UNI_COMPO * DPS_UNI_COMPO_PLANE;
typedef DPS_UNI_COMPO_PLANE * DPS_UNI_COMPO_MATRIX;
typedef DPS_UNI_COMPO_MATRIX * DPS_UNI_COMPO_MATRIX_PLANE;


#include "unidata.ch"


/* Returns lower case of argument "uni "*/

inline dpsunicode_t DpsUniToLower(dpsunicode_t uni) {
	register unsigned int plane;

	plane = ((uni) >> 8) & 0xFF;
	if (dps_uni_plane[plane].table) {
		return dps_uni_plane[plane].table[uni & 0xFF].tolower;
	}
	return(uni);
}


/* Function converts NULL terminated */
/* unistr array to lower case        */

void __DPSCALL DpsUniStrToLower(dpsunicode_t *unistr) {
	register unsigned int plane;
	for( ;*unistr; unistr++ ){
		plane = ((*unistr) >> 8) & 0xFF;
		if(dps_uni_plane[plane].table)
			(*unistr)=dps_uni_plane[plane].table[(*unistr)&0xFF].tolower;
	}
}

/* Parses null terminated  UNICODE string */
/* Returns tokens without separators      */
dpsunicode_t * DpsUniGetToken(dpsunicode_t *s, dpsunicode_t ** last, int *have_bukva_forte, int loose) {
	int ctype0 = DPS_UNI_UNDEF, ctype, plane, ctype_1, plane_1;
	dpsunicode_t *beg = NULL;
	dpsunicode_t *pattern_beg = NULL;

	if(s == NULL && (s=*last) == NULL)
		return NULL;

	/* Skip leading separators */
	for(;*s;s++){
		plane = ((*s) >> 8) & 0xFF;
		if(dps_uni_plane[plane].table){
		        ctype = dps_uni_plane[plane].table[(*s) &0xFF].ctype;
		}else{
			ctype=dps_uni_plane[plane].ctype;
		}
		/*fprintf(stderr,"TOK %04X %d\n",*s,ctype);*/
		if (ctype > DPS_UNI_BUKVA) {
		  if (dps_isPattern_Syntax(*s)) {
		    if (pattern_beg == NULL) pattern_beg = s;
		  } else {
		    pattern_beg = NULL;
		  }
		  continue;
		}
		ctype0 = ctype;
		beg = s;
		if (beg) break;
	}

	if(!*s)return NULL;
	*last=NULL;

	*have_bukva_forte = (ctype0 <= DPS_UNI_BUKVA_FORTE);

	/* Skip non-separators */
	for(;(*s);s++){
		plane = ((*s) >> 8) & 0xFF;
		if(dps_uni_plane[plane].table){
			ctype=dps_uni_plane[plane].table[(*s)&0xFF].ctype;
		}else{
			ctype=dps_uni_plane[plane].ctype;
		}
		
		if (*s == 0x27 || *s == 0x2019) {
		  if (dps_isApostropheBreak(*(s+1), (*(s+1)==0) ? 0 : *(s+2) )) {
		    s++;
		    *last = s;
		    return (pattern_beg && !loose) ? pattern_beg : beg;
		  }
		  plane_1 = ((*(s+1)) >> 8) & 0xFF;
		  if(dps_uni_plane[plane_1].table){
		    ctype_1 = dps_uni_plane[plane_1].table[(*(s+1)) & 0xFF].ctype;
		  }else{
		    ctype_1 = dps_uni_plane[plane_1].ctype;
		  }
/*		  fprintf(stderr," -- TOK %04X %d\n",*(s+1), ctype_1 > DPS_UNI_BUKVA);*/
		  if (ctype_1 > DPS_UNI_BUKVA && (loose || !dps_isPattern_Syntax(*(s+1)))) {
		    *last = s + 1;
		    return (pattern_beg && !loose) ? pattern_beg : beg;
		  }
		  s++; continue;
		}
		
/*		fprintf(stderr," -- TOK %04X %d  loose:%d\n",*(s), ctype > DPS_UNI_BUKVA, loose);*/
		if (ctype > DPS_UNI_BUKVA && (loose || !dps_isPattern_Syntax(*s))) {
		  *last = s;
		  return (pattern_beg && !loose) ? pattern_beg : beg;
		}
		if (ctype > DPS_UNI_BUKVA_FORTE) {
		  *have_bukva_forte = 0;
		}
	}
	/*fprintf(stderr,"*beg=%04X *s=%04X beg=%d s=%d *last=%d\n",*beg,*s,beg,s,*last);*/

	/* Done because of end-of-line */ 
	*last=s;
	return((pattern_beg && !loose) ? pattern_beg : beg);
}


/* Parses null terminated  UNICODE string  */
/* Returns all tokens including separators */

#if 1

dpsunicode_t * __DPSCALL DpsUniGetSepToken(dpsunicode_t *s, dpsunicode_t **last, int *ctype0, int *have_bukva_forte, int cmd_mode, int inphrase) {
  dpsunicode_t *beg;
  dpsunicode_t *pattern_beg = NULL;
  int pattern_prefix, pattern_prefix0;
  int plane, ctype, ctype_forte, plane_1, ctype_1, ctype_forte_1;
  int Pc0, Pc_1;
  /*
  fprintf(stderr, " -- cmd_mode: %d\n", cmd_mode);
  */
  if ((s == NULL) && ((s = *last) == NULL)) {
    return NULL;
  }
  /*
  cmd_mode = 1;
  */
  beg = s;
  if (!(*beg)) {
    return NULL;
  }

  plane = ((*s) >> 8) & 0xFF;
  if(dps_uni_plane[plane].table){
    *ctype0 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].table[(*s)&0xFF].ctype);
    Pc0 = (dps_uni_plane[plane].table[(*s)&0xFF].ctype == DPS_UNI_PUNCT_C);
    *have_bukva_forte = (dps_uni_plane[plane].table[(*s)&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
  }else{
    *ctype0 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].ctype);
    Pc0 = (dps_uni_plane[plane].ctype == DPS_UNI_PUNCT_C);
    *have_bukva_forte = (dps_uni_plane[plane].ctype <= DPS_UNI_BUKVA_FORTE);
  }
  pattern_prefix0 = dps_isPattern_Syntax(*s) || Pc0;
  if (pattern_prefix0) {
    if (!inphrase && dps_isQuotation_Mark(*s)) {
      *last = s + 1;
      return beg;
    }
    pattern_beg = s;
  }

  s++;
  plane = ((*s) >> 8) & 0xFF;
  if(dps_uni_plane[plane].table){
    ctype_1 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].table[(*s)&0xFF].ctype);
    Pc_1 = (dps_uni_plane[plane].table[(*s)&0xFF].ctype == DPS_UNI_PUNCT_C);
    ctype_forte_1 = (dps_uni_plane[plane].table[(*s)&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
  }else{
    ctype_1 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].ctype);
    Pc_1 = (dps_uni_plane[plane].ctype == DPS_UNI_PUNCT_C);
    ctype_forte_1 = (dps_uni_plane[plane].ctype <= DPS_UNI_BUKVA_FORTE);
  }

  for(; *s; s++) {

    pattern_prefix = dps_isPattern_Syntax(*s) || Pc_1;
    if (inphrase && dps_isQuotation_Mark(*s)) {
      *last = s;
      return beg;
    }
    ctype = ctype_1;
    ctype_forte = ctype_forte_1;
    
    *have_bukva_forte &= ctype_forte;

    plane = ((*(s+1)) >> 8) & 0xFF;
    if(dps_uni_plane[plane].table){
      ctype_1 = dps_uni_plane[plane].table[(*(s+1)) & 0xFF].ctype;
      Pc_1 = (dps_uni_plane[plane].table[(*(s+1))&0xFF].ctype == DPS_UNI_PUNCT_C);
      ctype_forte_1 = (dps_uni_plane[plane].table[(*(s+1))&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
    }else{
      ctype_1 = dps_uni_plane[plane].ctype;
      Pc_1 = (dps_uni_plane[plane].ctype == DPS_UNI_PUNCT_C);
      ctype_forte_1 = (dps_uni_plane[plane].ctype <= DPS_UNI_BUKVA_FORTE);
    }

    if (*s == 0x27 || *s == 0x2019) {
      if (dps_isApostropheBreak(*(s+1), (*(s+1)==0) ? 0 : *(s+2) )) {
	s++;
	*last = s;
	return beg;
      }
      if (ctype_1 > DPS_UNI_BUKVA && (!cmd_mode || !(dps_isPattern_Syntax(*(s+1)) || Pc_1)) ) {
	*last = s + 1;
	return beg;
      }
      s++; 
      plane = ((*(s+1)) >> 8) & 0xFF;
      if(dps_uni_plane[plane].table){
	ctype_1 = dps_uni_plane[plane].table[(*(s+1)) & 0xFF].ctype;
	Pc_1 = (dps_uni_plane[plane].table[(*(s+1))&0xFF].ctype == DPS_UNI_PUNCT_C);
	ctype_forte_1 = (dps_uni_plane[plane].table[(*(s+1))&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
      }else{
	ctype_1 = dps_uni_plane[plane].ctype;
	Pc_1 = (dps_uni_plane[plane].ctype == DPS_UNI_PUNCT_C);
	ctype_forte_1 = (dps_uni_plane[plane].ctype <= DPS_UNI_BUKVA_FORTE);
      }
      continue;
    }

    if ((ctype <= DPS_UNI_BUKVA) == (*ctype0 <= DPS_UNI_BUKVA)) {

      if (pattern_prefix0 && (ctype <= DPS_UNI_BUKVA)) {
	*ctype0 = DPS_UNI_BUKVA; /* force it into a word */
	continue;
      }
      if (pattern_prefix == pattern_prefix0) {
	continue;
      }
      if ((*ctype0 <= DPS_UNI_BUKVA) && pattern_prefix) {
	continue;
      }
      /*
      fprintf(stderr, " ++ ctype0: %d pattern_prefix0: %d\n", (*ctype0<= DPS_UNI_BUKVA), pattern_prefix0);
      fprintf(stderr, " ++ ctype:  %d pattern_prefix:  %d\n", (ctype<= DPS_UNI_BUKVA), pattern_prefix);
      fprintf(stderr, " ++ ctype_1:%d\n", (ctype_1<= DPS_UNI_BUKVA));
      */
     *last = s;
      return beg;
    }
    if (!cmd_mode && (*s != 0x2d /*'-'*/) && (*s != 0x2e /*'.'*/) && (*s != 0x5f/*'_'*/)) break;
    if ((*ctype0 <= DPS_UNI_BUKVA) && pattern_prefix) continue;
    if (pattern_prefix0 || *ctype0 <= DPS_UNI_BUKVA) {
      if (ctype <= DPS_UNI_BUKVA) {
	*ctype0 = DPS_UNI_BUKVA; /* force it into a word */
	continue;
      }
      if (pattern_prefix && ctype_1 <= DPS_UNI_BUKVA) continue;
    }
    /*
    fprintf(stderr, " ** ctype0: %d pattern_prefix0: %d\n", (*ctype0<= DPS_UNI_BUKVA), pattern_prefix0);
    fprintf(stderr, " ** ctype:  %d pattern_prefix:  %d\n", (ctype<= DPS_UNI_BUKVA), pattern_prefix);
    fprintf(stderr, " ** ctype_1:%d\n", (ctype_1<= DPS_UNI_BUKVA));
    */
    *last = s;
    return beg;
  }

  *last = s;
  return beg;
}

#else

dpsunicode_t * __DPSCALL DpsUniGetSepToken(dpsunicode_t *s, dpsunicode_t **last, int *ctype0, int *have_bukva_forte, int cmd_mode, int inphrase) {
  int ctype, plane, ctype_1, plane_1, ctype_forte, ctype_forte_1;
  dpsunicode_t *beg;
  int pattern_prefix;

	if(s == NULL && (s=*last) == NULL)
		return NULL;

	beg=s;

	if(!(*beg))return NULL;

	plane = ((*s) >> 8) & 0xFF;
	pattern_prefix = dps_isPattern_Syntax(*s);
	if(dps_uni_plane[plane].table){
		*ctype0 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].table[(*s)&0xFF].ctype);
		*have_bukva_forte = (dps_uni_plane[plane].table[(*s)&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
	}else{
		*ctype0 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].ctype);
		*have_bukva_forte = (dps_uni_plane[plane].ctype <= DPS_UNI_BUKVA_FORTE);
	}

	while(*s){
		plane = ((*s) >> 8) & 0xFF;
		if(dps_uni_plane[plane].table){
			ctype = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].table[(*s)&0xFF].ctype);
			ctype_forte = (dps_uni_plane[plane].table[(*s)&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
		}else{
			ctype = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].ctype);
			ctype_forte = (dps_uni_plane[plane].ctype <= DPS_UNI_BUKVA_FORTE);
		}

		/*		if (((*s == 0x27 || *s == 0x2019) && (*ctype0 <= DPS_UNI_BUKVA)) || cmd_mode) {*/
		  plane_1 = ((*(s+1)) >> 8) & 0xFF;
		  if(dps_uni_plane[plane_1].table){
		    ctype_1 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane_1].table[(*(s+1)) & 0xFF].ctype);
		  }else{
		    ctype_1 = DPS_UNI_CTYPECLASS(dps_uni_plane[plane_1].ctype);
		  }
		  /*}*/


		if ((*s == 0x27 || *s == 0x2019) && (*ctype0 <= DPS_UNI_BUKVA)) {
		  if (dps_isApostropheBreak(*(s+1), (*(s+1)==0) ? 0 : *(s+2) )) { s++; break; }
		  if(dps_uni_plane[plane_1].table){
		    ctype_forte_1 = (dps_uni_plane[plane_1].table[(*s)&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
		  }else{
		    ctype_forte_1 = (dps_uni_plane[plane_1].ctype <= DPS_UNI_BUKVA_FORTE);
		  }
		  if (ctype_1 <= DPS_UNI_BUKVA) {
		    ctype = ctype_1;
		    ctype_forte = ctype_forte_1;
		    s++;
		  }
		}
		/*
		if (pattern_prefix && ctype <= DPS_UNI_BUKVA) {
		  s++;
		  plane = ((*s) >> 8) & 0xFF;
		  if(dps_uni_plane[plane].table){
		    ctype = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].table[(*s)&0xFF].ctype);
		    ctype_forte = (dps_uni_plane[plane].table[(*s)&0xFF].ctype <= DPS_UNI_BUKVA_FORTE);
		  }else{
		    ctype = DPS_UNI_CTYPECLASS(dps_uni_plane[plane].ctype);
		    ctype_forte = (dps_uni_plane[plane].ctype <= DPS_UNI_BUKVA_FORTE);
		  }
		}
		*/

			
		fprintf(stderr, " -- ct0:%02d ct:%02d *s:'%c'%02x(%03d)%d - ct1:%02d *(s+1):'%c'%02x(%03d) ct1>:%d  cmd:%d  pp:%d\n", 
			*ctype0, ctype, (char)*s, *s, *s, dps_isPattern_Syntax(*s), ctype_1, (char)*(s+1), *(s+1), *(s+1), (ctype_1 > DPS_UNI_BUKVA), cmd_mode, pattern_prefix);
		
		
		fprintf(stderr, " -- cbb   %d ((%d %d) || %d \n --       %d %d %d\n --      %d %d %d %d\n", 
			(*ctype0 > DPS_UNI_BUKVA), (ctype_1 <= DPS_UNI_BUKVA), (!pattern_prefix), dps_isPattern_Syntax(*(s+1)),
			(*ctype0 <= DPS_UNI_BUKVA), (ctype_1 > DPS_UNI_BUKVA), (!dps_isPattern_Syntax(*s)),
			(!cmd_mode), (*s == 0), (*(s+1) == 0), ( (ctype_1 > DPS_UNI_BUKVA))
			);
		
		if ((*ctype0 > DPS_UNI_BUKVA && ((ctype_1 <= DPS_UNI_BUKVA && !pattern_prefix) || dps_isPattern_Syntax(*(s+1)))) 
		    || (*ctype0 <= DPS_UNI_BUKVA && ctype_1 > DPS_UNI_BUKVA && !dps_isPattern_Syntax(*s))) {
#if 0
		  if (!cmd_mode || *(s+1) == 0 || /* *s == 0 ||*/ ( /*((*s != 0x2d) && (*s != 0x2e) && (*s != 0x5f))|| */ (ctype_1 > DPS_UNI_BUKVA)) ) {
#endif
		       
		    fprintf(stderr, " -- break %d ((%d %d) || %d\n --       %d %d %d\n --      %d %d %d %d\n", 
			    (*ctype0 > DPS_UNI_BUKVA), (ctype_1 <= DPS_UNI_BUKVA), (!pattern_prefix),dps_isPattern_Syntax(*(s+1)),
			    (*ctype0 <= DPS_UNI_BUKVA), (ctype_1 > DPS_UNI_BUKVA), (!dps_isPattern_Syntax(*s)),
			    (!cmd_mode), (*s == 0), (*(s+1) == 0), ( (ctype_1 > DPS_UNI_BUKVA))
			    );
		    
		    s++;
		    break;
#if 0
		  }
#endif
		}

		if (pattern_prefix) {
		  pattern_prefix = dps_isPattern_Syntax(*s);
		  if (!pattern_prefix && (ctype <= DPS_UNI_BUKVA)) *ctype0 = DPS_UNI_BUKVA;
		}

		*have_bukva_forte &= ctype_forte;

		s++;

	}

	*last=s;
	return(beg);
}
#endif


int DpsUniCType(dpsunicode_t uni) {
	register unsigned int plane;

	plane = ((uni) >> 8) & 0xFF;

	if(dps_uni_plane[plane].table) {
		return dps_uni_plane[plane].table[ uni & 0xFF].ctype;
	}
	return dps_uni_plane[plane].ctype;
}


void DpsUniAspellSimplify(dpsunicode_t *ustr) {
  register dpsunicode_t *u = ustr;
  while (*u) {
    switch(*u) {
    case 0x02BC: /* modifier letter apostrophe */
    case 0x2019: /* right single quotation mark */
    case 0x275C: /* heavy single comma quotation mark */
      *u = (u[1] != 0) ? 0x27 : 0;
      break;
    default:
      break;
    }
    u++;
  }
  return;
}


/* Normalizations based on: */
/*
 * Uninorm - A free ANSI C Implementation of Unicode
 * Normalization Forms NFD and NFC.
 *
 * You may use this library on either the terms of the
 * GNU General Public Licence or the Artistic Licence.
 *
 * The project is maintained at
 * http://sourceforge.net/projects/uninorm
 *
 * Copyright (c) 2001 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 *
 * $Id: uninorm.c,v 1.2 2001/08/29 03:58:40 hoehrmann Exp $
 *
 */

/* Hangul constants */
#define SBase  0xAC00
#define LBase  0x1100
#define VBase  0x1161
#define TBase  0x11A7
#define HLast  0xD7A3
#define LCount 19
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
#define SCount (LCount * NCount)


static int DpsUniIsExcluded(dpsunicode_t c) {
  register int i = 0;
  while (uni_CompositionExclusions[i] != 0) {
    if (uni_CompositionExclusions[i] == c) {
      return 1;
    }
    i++;
  }
  return 0;
}


static int DpsUniGetCombiningClass(dpsunicode_t c) {
  register int plane = (c >> 8) & 0xFF;

  if (uni_comb_plane[plane].table == NULL) return uni_comb_plane[plane].comb;
  return uni_comb_plane[plane].table[c & 0xFF];
}


static int DpsUniGetRecombinedCodepoint(dpsunicode_t c1, dpsunicode_t c2) {
  register int matrix_plane = (c2 >> 8) & 0xFF;
  register dpsunicode_t r = 0;

  if (uni_compo[matrix_plane] != NULL) {
    register int matrix_character = c2 & 0xFF;
    if ( (uni_compo[matrix_plane])[matrix_character] != NULL) {
      register int plane = (c1 >> 8) & 0xFF;
      if ( ((uni_compo[matrix_plane])[matrix_character])[plane] != NULL) {
	r = (((uni_compo[matrix_plane])[matrix_character])[plane])[c1 & 0xFF];
      }
    }
  }

  if (r != 0 && !DpsUniIsExcluded(r)) return r;

  return -1;
}


static dpsunicode_t *DpsUniGetDecomposition(dpsunicode_t *buf, dpsunicode_t c) {
  int plane = (c >> 8) & 0xFF;
  int character = c & 0xFF;

  if (uni_decomp_plane[plane] != NULL) {
    buf[0] = (uni_decomp_plane[plane])[character].decomp[0];
    buf[1] = (uni_decomp_plane[plane])[character].decomp[1];
    if (buf[0] != 0) return buf;
  }
  return 0;
}

int dps_isApostropheBreak(dpsunicode_t c, dpsunicode_t n) {
  int plane = (c >> 8) & 0xFF;
  if (uni_decomp_plane[plane] != NULL) {
    int character = c & 0xFF;
    dpsunicode_t dec = (uni_decomp_plane[plane])[character].decomp[0];
    if (dec == 0) dec = (dpsunicode_t) character;
    if (dec == 'h' && (n != 0)) {
      plane = (n >> 8) & 0xFF;
      if (uni_decomp_plane[plane] == NULL) return 0;
      character = n & 0xFF;
      dec = (uni_decomp_plane[plane])[character].decomp[0];
      if (dec == 0) dec = (dpsunicode_t) character;
    }
    switch(dec) {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
    case 'w':
    case 'y': return 1;
    default: return 0;
    }
  }
  return 0;
}

static void DpsUniDecomposeRecursive(DPS_DSTR *buf, dpsunicode_t c) {
  dpsunicode_t decomp[3];

  if (c >= SBase && c <= HLast) {
    int SIndex = c - SBase;
    if (SIndex < 0 || SIndex >= SCount) { DpsDSTRAppend(buf, &c, sizeof(dpsunicode_t)); return; }
    else {
      dpsunicode_t L = LBase + (dpsunicode_t)SIndex / NCount;
      dpsunicode_t V = VBase + (SIndex % NCount) / TCount;
      dpsunicode_t T = TBase + SIndex % TCount;
      DpsDSTRAppendUni(buf, L);
      DpsDSTRAppendUni(buf, V);
      if (T != TBase) DpsDSTRAppendUni(buf, T);
    }
  } else {
    if (DpsUniGetDecomposition(decomp, c) != 0) {
      DpsUniDecomposeRecursive(buf, decomp[0]);
      if (decomp[1]) {
	DpsDSTRAppendUni(buf, decomp[1]);
      }
    } else DpsDSTRAppendUni(buf, c);
  }

}


static dpsunicode_t *DpsUniCanonicalOrder(dpsunicode_t *str) {
  int i;
  int len = DpsUniLen(str);

  for (i = 0; i < len - 1; i++) {
    int first = DpsUniGetCombiningClass(str[i]);
    int second = DpsUniGetCombiningClass(str[i + 1]);

    if ((first > second) && (second != 0)) {
      register dpsunicode_t c = str[i];

      str[i] = str[i+1];
      str[i+1] = c;
      if (i == 0) i--;
      else i -= 2;
    }
  }
  return str;
}


static dpsunicode_t *DpsUniCanonicalComposition(dpsunicode_t *str) {
  int ipos = 0, /* position of initial */
      cpos = 0, /* current position    */
      opos = 0; /* writing position    */
  int len;
  int SIndex;
  int initial = -1;
  int c;

  if (str == NULL) return NULL;
  len = DpsUniLen(str);
  if (len == 0) return str;

  while (cpos < len ) {
    int this_class = DpsUniGetCombiningClass(str[cpos]);
    if (initial >= LBase && initial < (LBase + LCount) && str[cpos] >= VBase && str[cpos] < (VBase + VCount)) {
      initial = str[ipos] = ((initial - LBase) * VCount + str[cpos] - VBase) * TCount + SBase;
      cpos++;
    } else if (0 <= (SIndex = initial - SBase) && SIndex < SCount && SIndex % TCount == 0) {
	int TIndex = (int)((long)str[cpos] - TBase);
      if (0 <= TIndex && TIndex < TCount) {
	str[ipos] = str[cpos++] - TBase + initial;
      } else {
	str[opos++] = str[cpos++];
      }
    } else if ((initial != -1) && (initial == (int)str[opos - 1] || DpsUniGetCombiningClass(str[opos - 1]) != this_class)
	       && (c = DpsUniGetRecombinedCodepoint(initial, str[cpos])) != -1) {
      initial = str[ipos] = c;
      cpos++;
    } else if (this_class == 0) {
      ipos = opos++;
      str[ipos] = initial = str[cpos++];
    } else {
      str[opos++] = str[cpos++];
    }
  }
  str[opos++] = 0;

  str = DpsRealloc(str, sizeof(dpsunicode_t) * opos + 1);

  return str;
}


static dpsunicode_t *DpsUniCanonicalDecomposition(dpsunicode_t *buf, dpsunicode_t *str) {
  int i, pos = 0, len = 0, bulen;
  int length = bulen = DpsUniLen(str) + 4;
  DPS_DSTR temp;

  DpsDSTRInit(&temp, length);

  buf = (dpsunicode_t*)DpsRealloc(buf, sizeof(dpsunicode_t) * (bulen));
  if (buf == NULL) return str;
  buf[0] = 0;

  for(i = 0; i < length - 4; i++) {
    temp.data_size = 0;
    DpsUniDecomposeRecursive(&temp, str[i]);
    len = temp.data_size / sizeof(dpsunicode_t);
    if (pos + len >= bulen)
      buf = (dpsunicode_t*)DpsRealloc(buf, sizeof(dpsunicode_t) * (bulen += len) );
    dps_memcpy(buf + pos, temp.data, temp.data_size);  /* was: dps_memmove */
    pos += len;
  }
  buf[pos++] = 0;
  DpsDSTRFree(&temp);

  return buf;
}


/* Unicode Normalization Form D */
dpsunicode_t *DpsUniNormalizeNFD(dpsunicode_t *buf, dpsunicode_t *str) {
  
  buf = DpsUniCanonicalDecomposition(buf, str);
  buf = DpsUniCanonicalOrder(buf);
  return buf;
}

/* Unicode Normalization Form C */
dpsunicode_t *DpsUniNormalizeNFC(dpsunicode_t *buf, dpsunicode_t *str) {
  buf = DpsUniNormalizeNFD(buf, str);
  buf = DpsUniCanonicalComposition(buf);
  return buf;
}
