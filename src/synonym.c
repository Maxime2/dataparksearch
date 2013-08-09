/* Copyright (C) 2003-2012 DataPark Ltd. All right reserved.
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
#include "dps_common.h"
#include "dps_env.h"
#include "dps_utils.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_word.h"
#include "dps_synonym.h"
#include "dps_conf.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>


void DpsSynonymListInit(DPS_SYNONYMLIST * List){
     bzero((void*)List, sizeof(*List));
}

__C_LINK int __DPSCALL DpsSynonymListLoad(DPS_ENV * Env,const char * filename){
     struct stat     sb;
     char      *str, *data = NULL, *cur_n = NULL, *sharp;
     char      lang[64]="";
     DPS_CHARSET    *cs=NULL;
     DPS_CHARSET    *sys_int=DpsGetCharSet("sys-int");
     DPS_CONV  file_uni;
     DPS_WIDEWORD    *ww = NULL;
     size_t key = 1;
     int flag_th = 0;
     int             fd;
     char            savebyte;
     
     if (stat(filename, &sb)) {
       dps_strerror(NULL, 0, "Unable to stat synonyms file '%s'", filename);
       return DPS_ERROR;
     }
     if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
       dps_strerror(NULL, 0, "Unable to open synonyms file '%s'", filename);
       return DPS_ERROR;
     }
     if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
       dps_snprintf(Env->errstr,sizeof(Env->errstr)-1, "Unable to alloc %d bytes", sb.st_size);
       DpsClose(fd);
       return DPS_ERROR;
     }
     if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
       dps_strerror(NULL, 0, "Unable to read synonym file '%s'", filename);
       DPS_FREE(data);
       DpsClose(fd);
       return DPS_ERROR;
     }
     data[sb.st_size] = '\0';
     str = data;
     cur_n = strchr(str, NL_INT);
     if (cur_n != NULL) {
       cur_n++;
       savebyte = *cur_n;
       *cur_n = '\0';
     }

     while(str != NULL) {
          if(str[0]=='#'||str[0]==' '||str[0]==HT_CHAR||str[0]==CR_CHAR||str[0]==NL_CHAR) goto loop_continue;
	  sharp = strchr(str, (int)'#');
	  while (sharp != NULL) 
	    if (*(sharp - 1) != '\\') {
	      *sharp = '\0';
	      break;
	    } else {
	      sharp = strchr(sharp + 1, (int)'#');
	    }
          
          if(!strncasecmp(str,"Charset:",8)){
               char * lasttok;
               char * charset;
               if((charset = dps_strtok_r(str + 8, " \t\n\r", &lasttok, NULL))) {
                    cs=DpsGetCharSet(charset);
                    if(!cs){
                         dps_snprintf(Env->errstr, sizeof(Env->errstr), "Unknown charset '%s' in synonyms file '%s'",
                                   charset, filename);
                         DPS_FREE(data);
			 DpsClose(fd);
                         return DPS_ERROR;
                    }
                    DpsConvInit(&file_uni, cs, sys_int, Env->CharsToEscape, 0);
               }
          }else
          if(!strncasecmp(str,"Language:",9)){
               char * lasttok;
               char * l;
               if((l = dps_strtok_r(str + 9, " \t\n\r", &lasttok, NULL))) {
                    dps_strncpy(lang, l, sizeof(lang)-1);
               }
          }else
          if(!strncasecmp(str, "Thesaurus:", 10)) {
               char * lasttok;
	       char *tok = dps_strtok_r(str + 10, " \t\n\r", &lasttok, NULL);
	       flag_th = (strncasecmp(tok, "yes", 3) == 0) ? 1 : 0;
          }else{
               char      *av[255];
               size_t         ac, i, j;
	       dpsunicode_t *t;

               if(!cs){
                    dps_snprintf(Env->errstr,sizeof(Env->errstr)-1,"No Charset command in synonyms file '%s'",filename);
                    DpsClose(fd); DPS_FREE(data);
                    return DPS_ERROR;
               }
               if(!lang[0]){
                    dps_snprintf(Env->errstr,sizeof(Env->errstr)-1,"No Language command in synonyms file '%s'",filename);
                    DpsClose(fd); DPS_FREE(data);
                    return DPS_ERROR;
               }

               ac = DpsGetArgs(str, av, 255);
               if (ac < 2) goto loop_continue;

               if ((ww = (DPS_WIDEWORD*)DpsRealloc(ww, ac * sizeof(DPS_WIDEWORD))) == NULL) return DPS_ERROR;

               for (i = 0; i < ac; i++) {
                 ww[i].word = av[i];
                 ww[i].len = dps_strlen(av[i]);
		 ww[i].uword = t = (dpsunicode_t*)DpsMalloc((3 * ww[i].len + 1) * sizeof(dpsunicode_t));
		 if (ww[i].uword == NULL) return DPS_ERROR;
                 DpsConv(&file_uni, (char*)ww[i].uword, sizeof(dpsunicode_t) * (3 * ww[i].len + 1), av[i], ww[i].len + 1);
                 DpsUniStrToLower(ww[i].uword);
		 ww[i].uword = DpsUniNormalizeNFC(NULL, ww[i].uword);
		 DPS_FREE(t);
               }

               for (i = 0; i < ac - 1; i++) {
                 for (j = i + 1; j < ac; j++) {

                   if((Env->Synonyms.nsynonyms + 1) >= Env->Synonyms.msynonyms){
                    Env->Synonyms.msynonyms += 64;
                    Env->Synonyms.Synonym = (DPS_SYNONYM*)DpsRealloc(Env->Synonyms.Synonym, 
                                                   sizeof(DPS_SYNONYM)*Env->Synonyms.msynonyms);
		    if (Env->Synonyms.Synonym == NULL) {
		      Env->Synonyms.msynonyms = Env->Synonyms.nsynonyms = 0;
		      return DPS_ERROR;
  		    }
                   }
               
                   bzero((void*)&Env->Synonyms.Synonym[Env->Synonyms.nsynonyms], sizeof(DPS_SYNONYM));
               
                   /* Add direct order */
                   Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].p.uword = DpsUniDup(ww[i].uword);
                   Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].s.uword = DpsUniDup(ww[j].uword);
		   Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].p.count = 
		     Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].s.count = (size_t)((flag_th) ? key : 0);
                   Env->Synonyms.nsynonyms++;
               
                   bzero((void*)&Env->Synonyms.Synonym[Env->Synonyms.nsynonyms], sizeof(DPS_SYNONYM));
               
                   /* Add reverse order */
                   Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].p.uword = DpsUniDup(ww[j].uword);
                   Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].s.uword = DpsUniDup(ww[i].uword);
		   Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].p.count = 
		     Env->Synonyms.Synonym[Env->Synonyms.nsynonyms].s.count = (size_t)((flag_th) ? key : 0);
                   Env->Synonyms.nsynonyms++;

		   Env->Synonyms.sorted = 0;
                 }
               }

               for (i = 0; i < ac; i++) {
                 DPS_FREE(ww[i].uword);
               }
               do { key++; } while (key == 0);
          }
     loop_continue:
	  str = cur_n;
	  if (str != NULL) {
	    *str = savebyte;
	    cur_n = strchr(str, NL_INT);
	    if (cur_n != NULL) {
	      cur_n++;
	      savebyte = *cur_n;
	      *cur_n = '\0';
	    }
	  }
     }
     DPS_FREE(data);
     DPS_FREE(ww);
     DpsClose(fd);
     return DPS_OK;
}

void DpsSynonymListFree(DPS_SYNONYMLIST * List){
     size_t i;
     
     for(i=0;i<List->nsynonyms;i++){
          DPS_FREE(List->Synonym[i].p.word);
          DPS_FREE(List->Synonym[i].p.uword);
          DPS_FREE(List->Synonym[i].s.word);
          DPS_FREE(List->Synonym[i].s.uword);
     }
     DPS_FREE(List->Synonym);
     DPS_FREE(List->Back);
}

static int cmpsyn(const void * v1,const void * v2){
     const DPS_SYNONYM * s1=(const DPS_SYNONYM*)v1;
     const DPS_SYNONYM * s2=(const DPS_SYNONYM*)v2;
     return(DpsUniStrCmp(s1->p.uword,s2->p.uword));
}

static int cmpsynback(const void * v1,const void * v2){
     const DPS_SYNONYM **s1 = (const DPS_SYNONYM**)v1;
     const DPS_SYNONYM **s2 = (const DPS_SYNONYM**)v2;
     return(DpsUniStrCmp((*s1)->s.uword, (*s2)->s.uword));
}


__C_LINK void __DPSCALL DpsSynonymListSort(DPS_SYNONYMLIST * List){
  if (List->Synonym != NULL && List->sorted == 0) {
    if (List->nsynonyms > 1)
      DpsSort(List->Synonym, List->nsynonyms, sizeof(DPS_SYNONYM), &cmpsyn);
    if ((List->Back = (DPS_SYNONYM**)DpsRealloc(List->Back, (List->nsynonyms + 1) * sizeof(DPS_SYNONYM*))) != NULL) {
      register size_t i;
      for (i = 0; i < List->nsynonyms; i++) List->Back[i] = &List->Synonym[i];
      if (List->nsynonyms > 1)
	DpsSort(List->Back, List->nsynonyms, sizeof(DPS_SYNONYM*), &cmpsynback);
    }
    List->sorted = 1;
  }
}


DPS_WIDEWORDLIST * DpsSynonymListFind(const DPS_SYNONYMLIST * List,DPS_WIDEWORD * wword){
     DPS_SYNONYM syn,*res,*first,*last;
     DPS_SYNONYM *psyn, **pres, **pfirst, **plast;
     DPS_WIDEWORDLIST *Res = NULL;
     size_t nnorm,i;

     if(!List->nsynonyms)return NULL;

     syn.p.uword = wword->uword;

     res = dps_bsearch(&syn, List->Synonym, List->nsynonyms, sizeof(DPS_SYNONYM), &cmpsyn);

     if(res){

          Res = (DPS_WIDEWORDLIST *)DpsMalloc(sizeof(*Res));
	  if (Res == NULL) return NULL;
          DpsWideWordListInit(Res);

          /* Find first and last synonym */
          for(first = res; first >= List->Synonym; first--) {
               if(DpsUniStrCmp(wword->uword,first->p.uword)){
                    break;
               }else{
                    first->s.order = wword->order;
                    first->s.origin = DPS_WORD_ORIGIN_SYNONYM;
                    DpsWideWordListAdd(Res,&first->s, DPS_WWL_LOOSE);
               }
          }
          for(last=res+1;last<List->Synonym+List->nsynonyms;last++){
               if(DpsUniStrCmp(wword->uword,last->p.uword)){
                    break;
               }else{
                    last->s.order=wword->order;
                    last->s.origin = DPS_WORD_ORIGIN_SYNONYM;
                    DpsWideWordListAdd(Res,&last->s, DPS_WWL_LOOSE);
               }
          }
     }

     syn.s.uword = wword->uword;
     psyn = &syn;
     pres = dps_bsearch(&psyn, List->Back, List->nsynonyms, sizeof(DPS_SYNONYM*), &cmpsynback);

     if(pres) {

          if (Res == NULL) {
	    Res = (DPS_WIDEWORDLIST *)DpsMalloc(sizeof(*Res));
	    if (Res == NULL) return NULL;
	    DpsWideWordListInit(Res);
	  }

          /* Find first and last synonym */
          for(pfirst = pres; pfirst >= List->Back; pfirst--) {
	    if(DpsUniStrCmp(wword->uword, (*pfirst)->s.uword)) {
                    break;
	    }else{
	      (*pfirst)->p.order = wword->order;
	      (*pfirst)->p.origin = DPS_WORD_ORIGIN_SYNONYM;
	      DpsWideWordListAdd(Res, &((*pfirst)->p), DPS_WWL_LOOSE);
	    }
          }
          for(plast = pres + 1; plast < List->Back + List->nsynonyms; plast++) {
	    if(DpsUniStrCmp(wword->uword, (*plast)->s.uword)) {
                    break;
	    } else {
	      (*plast)->p.order = wword->order;
	      (*plast)->p.origin = DPS_WORD_ORIGIN_SYNONYM;
	      DpsWideWordListAdd(Res, &((*plast)->p), DPS_WWL_LOOSE);
	    }
          }
     }

     if (Res == NULL) return NULL;

     /* Now find each of them in reverse order */
     if ((nnorm = Res->nwords) > 0) {
          for(i = 0; i < nnorm; i++) {

	    syn.p.uword = Res->Word[i].uword;

	    res = dps_bsearch(&syn, List->Synonym, List->nsynonyms, sizeof(DPS_SYNONYM), &cmpsyn);

	    if(res){

	      /* Find first and last synonym */
	      for(first = res; first >= List->Synonym; first--) {
		if(DpsUniStrCmp(Res->Word[i].uword, first->p.uword)){
		  break;
		}else{
		  if ((Res->Word[i].count != 0) && (first->p.count != Res->Word[i].count)) continue;
		  first->s.order = wword->order;
		  first->s.origin = DPS_WORD_ORIGIN_SYNONYM;
		  DpsWideWordListAdd(Res, &first->s, DPS_WWL_LOOSE);
		}
	      }
	      for(last=res+1;last<List->Synonym+List->nsynonyms;last++){
		if(DpsUniStrCmp(Res->Word[i].uword, last->p.uword)) {
		  break;
		}else{
		  if ((Res->Word[i].count != 0) && (last->p.count != Res->Word[i].count)) continue;
		  last->s.order=wword->order;
		  last->s.origin = DPS_WORD_ORIGIN_SYNONYM;
		  DpsWideWordListAdd(Res, &last->s, DPS_WWL_LOOSE);
		}
	      }
	    }

	    syn.s.uword = Res->Word[i].uword;
	    pres = dps_bsearch(&psyn, List->Back, List->nsynonyms, sizeof(DPS_SYNONYM*), &cmpsynback);
               
	    if(pres) {
                    /* Find first and last synonym */
                    for(pfirst = pres; pfirst >= List->Back; pfirst--) {
		      if(DpsUniStrCmp(syn.s.uword, (*pfirst)->s.uword)) {
                              break;
		      } else {
			if ((Res->Word[i].count != 0) && ((*pfirst)->s.count != Res->Word[i].count)) continue;
			(*pfirst)->s.order = wword->order;
			(*pfirst)->s.origin = DPS_WORD_ORIGIN_SYNONYM;
			DpsWideWordListAdd(Res, &((*pfirst)->s), DPS_WWL_LOOSE);
		      }
                    }
                    for(plast = pres + 1; plast < List->Back + List->nsynonyms; plast++) {
		      if(DpsUniStrCmp(syn.s.uword, (*plast)->s.uword)) {
                              break;
		      } else {
			if ((Res->Word[i].count != 0) && ((*plast)->s.count != Res->Word[i].count)) continue;
			(*plast)->s.order = wword->order;
			(*plast)->s.origin = DPS_WORD_ORIGIN_SYNONYM;
			DpsWideWordListAdd(Res, &((*plast)->s), DPS_WWL_LOOSE);
		      }
                    }
	    }
          }
     }
     return(Res);
}
