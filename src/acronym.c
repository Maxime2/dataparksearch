/* Copyright (C) 2005-2012 DataPark Ltd. All right reserved.

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
#include "dps_acronym.h"
#include "dps_conf.h"
#include "dps_charsetutils.h"
#include "dps_match.h"
#include "dps_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>


void DpsAcronymListInit(DPS_ACRONYMLIST *List) {
     bzero((void*)List, sizeof(*List));
}

int __DPSCALL DpsAcronymListLoad(DPS_AGENT * query, const char * filename) {
     DPS_MATCH	Alias;
     DPS_ENV *Env = query->Conf;
     struct stat     sb;
     char      *str, *data = NULL, *cur_n = NULL, *sharp;
     char      lang[64]="";
     DPS_CHARSET    *cs=NULL;
     DPS_CHARSET    *sys_int=DpsGetCharSet("sys-int");
     DPS_CONV  file_uni, uni_lc;
     DPS_WIDEWORD    *ww = NULL;
     int             fd;
     char            savebyte;
     char            *av[255];
     size_t          ac, i;
     dpsunicode_t    *t;
#if defined HAVE_ASPELL
     DPS_CHARSET *utf8 = DpsGetCharSet("UTF-8");
     DPS_CONV toutf8;
     AspellCanHaveError *ret;
     AspellSpeller *speller = NULL;
     int use_aspellext = Env->Flags.use_aspellext;
     char lstr[2048];

     if (utf8 == NULL) {
       dps_strerror(NULL, 0, "%s: Unable to get UTF-8 charset!", __FUNCTION__, filename);
       return DPS_ERROR;
     }
     DpsConvInit(&toutf8, sys_int, utf8, Env->CharsToEscape, 0);
#endif
     
     if (stat(filename, &sb)) {
       dps_strerror(NULL, 0, "Unable to stat acronyms file '%s'", filename);
       return DPS_ERROR;
     }
     if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
       dps_strerror(NULL, 0, "Unable to open acronyms file '%s'", filename);
       return DPS_ERROR;
     }
     if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
       dps_snprintf(Env->errstr,sizeof(Env->errstr)-1, "Unable to alloc %d bytes", sb.st_size);
       DpsClose(fd);
       return DPS_ERROR;
     }
     if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
       dps_strerror(NULL, 0, "Unable to read acronym file '%s'", filename);
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
     DpsClose(fd);

     DpsConvInit(&uni_lc, sys_int, Env->lcs, Env->CharsToEscape, DPS_RECODE_HTML);

     while(str != NULL) {

          if (str[0] == '#' && str[1] == '*') {
	    ac = DpsGetArgs(str + 2, av, 255);
	    if (ac < 2) goto loop_continue;
	    DpsMatchInit(&Alias);
	    Alias.match_type = DPS_MATCH_BEGIN;
	    for(i = 0; i < ac; i++) {
		if(!strcasecmp(av[i],"regex"))
			Alias.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"regexp"))
			Alias.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"case"))
			Alias.case_sense=1;
		else
		if(!strcasecmp(av[i],"nocase"))
			Alias.case_sense=0;
		else
		if(!strcasecmp(av[i],"last"))
			Alias.last = 1;
		else
		if(!Alias.pattern){
			Alias.pattern=av[i];
		}else{
			char	    err[120] = "";
			
			Alias.arg = av[i];
			
			if(DPS_OK != DpsMatchListAdd(NULL, &Env->QAliases, &Alias, err, sizeof(err), 0)) {
				dps_snprintf(Env->errstr, sizeof(Env->errstr)-1, "%s", err);
#ifdef HAVE_ASPELL
				if (use_aspellext && speller != NULL) {
				  delete_aspell_speller(speller);
				}
#endif
				return DPS_ERROR;
			}
		}
	    }
	    if(!Alias.arg){
	      dps_snprintf(Env->errstr, sizeof(Env->errstr)-1, "too few arguments in file '%s'", filename);
#ifdef HAVE_ASPELL
	      if (use_aspellext && speller != NULL) {
		delete_aspell_speller(speller);
	      }
#endif
	      return DPS_ERROR;
	    }
	  }
          if(str[0]=='#'||str[0]==' '||str[0]==HT_CHAR||str[0]==CR_CHAR||str[0]==NL_CHAR) goto loop_continue;
	  sharp = strchr(str, (int)'#');
	  while (sharp != NULL) 
	    if (*(sharp - 1) != '\\') {
	      *sharp = '\0';
	      break;
	    } else {
	      sharp = strchr(sharp + 1, (int)'#');
	    }

         if(!strncmp(str,"Charset:",8)){
               char * lasttok;
               char * charset;
               if((charset = dps_strtok_r(str + 8, " \t\n\r", &lasttok, NULL))) {
                    cs=DpsGetCharSet(charset);
                    if(!cs){
                         dps_snprintf(Env->errstr, sizeof(Env->errstr), "Unknown charset '%s' in acronyms file '%s'",
                                   charset, filename);
                         DPS_FREE(data);
#ifdef HAVE_ASPELL
			 if (use_aspellext && speller != NULL) {
			   delete_aspell_speller(speller);
			 }
#endif
                         return DPS_ERROR;
                    }
                    DpsConvInit(&file_uni, cs, sys_int, Env->CharsToEscape, DPS_RECODE_HTML);
               }
          }else
          if(!strncmp(str,"Language:",9)){
               char * lasttok;
               char * l;
               if((l = dps_strtok_r(str + 9, " \t\n\r", &lasttok, NULL))) {
                    dps_strncpy(lang, l, sizeof(lang)-1);
               }
#if defined HAVE_ASPELL
	       if (use_aspellext) {
		 aspell_config_replace(query->aspell_config, "lang", lang);
		 ret = new_aspell_speller(query->aspell_config);
		 if (aspell_error(ret) != 0) {
		   DpsLog(query, DPS_LOG_ERROR, "%s: aspell error: %s", __FUNCTION__, aspell_error_message(ret));
		   delete_aspell_can_have_error(ret);
		   use_aspellext = 0;
		 } else {
		   speller = to_aspell_speller(ret);
		 }
	       }
#endif
         }else{

               if(!cs){
                    dps_snprintf(Env->errstr,sizeof(Env->errstr)-1,"No Charset command in acronyms file '%s'",filename);
                    DPS_FREE(data);
#ifdef HAVE_ASPELL
		    if (use_aspellext && speller != NULL) {
		      delete_aspell_speller(speller);
		    }
#endif
                    return DPS_ERROR;
               }
               if(!lang[0]){
                    dps_snprintf(Env->errstr,sizeof(Env->errstr)-1,"No Language command in acronyms file '%s'",filename);
                    DPS_FREE(data);
#ifdef HAVE_ASPELL
		    if (use_aspellext && speller != NULL) {
		      delete_aspell_speller(speller);
		    }
#endif
                    return DPS_ERROR;
               }

               ac = DpsGetArgs(str, av, 255);
               if (ac < 2) goto loop_continue;

               if ((ww = (DPS_WIDEWORD*)DpsRealloc(ww, ac * sizeof(DPS_WIDEWORD))) == NULL) { 
		 DPS_FREE(data); 
#ifdef HAVE_ASPELL
		 if (use_aspellext && speller != NULL) {
		   delete_aspell_speller(speller);
		 }
#endif
		 return DPS_ERROR;
	       }

               for (i = 0; i < ac; i++) {
		 ww[i].word = av[i];
                 ww[i].len = dps_strlen(av[i]);
		 ww[i].uword = t = (dpsunicode_t*)DpsMalloc((5 * ww[i].len + 1) * sizeof(dpsunicode_t));
		 if (ww[i].uword == NULL) {
		   DPS_FREE(data); 
#ifdef HAVE_ASPELL
		   if (use_aspellext && speller != NULL) {
		     delete_aspell_speller(speller);
		   }
#endif
		   return DPS_ERROR;
		 }
                 ww[i].word = (char*)DpsMalloc((15 * ww[i].len + 1) * sizeof(char));
		 if (ww[i].word == NULL) {
		   DPS_FREE(data); 
#ifdef HAVE_ASPELL
		   if (use_aspellext && speller != NULL) {
		     delete_aspell_speller(speller);
		   }
#endif
		   return DPS_ERROR; 
		 }
                 DpsConv(&file_uni, (char*)ww[i].uword, sizeof(dpsunicode_t) * (5 * ww[i].len + 1), av[i], ww[i].len + 1);
                 DpsUniStrToLower(ww[i].uword);
		 ww[i].uword = DpsUniNormalizeNFC(NULL, ww[i].uword);
		 DPS_FREE(t);
		 DpsConv(&uni_lc, ww[i].word, (15*ww[i].len+1)*sizeof(char), (char*)ww[i].uword, sizeof(dpsunicode_t)*(5*ww[i].len+1));
               }

	       if((Env->Acronyms.nacronyms + 1) >= Env->Acronyms.macronyms){
		 Env->Acronyms.macronyms += 64;
		 Env->Acronyms.Acronym = (DPS_ACRONYM*)DpsRealloc(Env->Acronyms.Acronym, 
								  sizeof(DPS_ACRONYM)*Env->Acronyms.macronyms);
		 if (Env->Acronyms.Acronym == NULL) {
		   Env->Acronyms.macronyms = Env->Acronyms.nacronyms = 0;
		   DPS_FREE(data);
#ifdef HAVE_ASPELL
		   if (use_aspellext && speller != NULL) {
		     delete_aspell_speller(speller);
		   }
#endif
		   return DPS_ERROR;
		 }
	       }

	       bzero((void*)&Env->Acronyms.Acronym[Env->Acronyms.nacronyms], sizeof(DPS_ACRONYM));
	       Env->Acronyms.Acronym[Env->Acronyms.nacronyms].a = ww[0];
               
               for (i = 1; i < ac; i++) {
		 DpsWideWordListAdd(&Env->Acronyms.Acronym[Env->Acronyms.nacronyms].unroll, &ww[i], DPS_WWL_LOOSE);
                 DPS_FREE(ww[i].uword);
		 DPS_FREE(ww[i].word);
               }

	       Env->Acronyms.nacronyms++;
	       Env->Acronyms.sorted = 0;

#ifdef HAVE_ASPELL
		if (use_aspellext) {
		  DpsConv(&toutf8, lstr, sizeof(lstr), ((const char*)ww[0].uword),(size_t)ww[0].len * sizeof(ww[0].uword[0]));
		  if (0 == aspell_speller_check(speller, lstr, -1)) {
		    aspell_speller_add_to_personal(speller, lstr, -1);
		  }
		}
#endif

               
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
#ifdef HAVE_ASPELL
     if (use_aspellext && speller != NULL) {
       aspell_speller_save_all_word_lists(speller);
       delete_aspell_speller(speller);
     }
#endif
     return DPS_OK;
}

void DpsAcronymListFree(DPS_ACRONYMLIST * List) {
     size_t i;
     
     for(i = 0; i < List->nacronyms; i++) {
          DPS_FREE(List->Acronym[i].a.word);
          DPS_FREE(List->Acronym[i].a.uword);
	  DpsWideWordListFree(&List->Acronym[i].unroll);
     }
     DPS_FREE(List->Acronym);
}

static int cmpacr(const void * v1, const void * v2) {
     const DPS_ACRONYM * s1=(const DPS_ACRONYM*)v1;
     const DPS_ACRONYM * s2=(const DPS_ACRONYM*)v2;
     return(DpsUniStrCmp(s1->a.uword, s2->a.uword));
}

__C_LINK void __DPSCALL DpsAcronymListSort(DPS_ACRONYMLIST * List) {

  if(List->Acronym != NULL && List->sorted == 0 && List->nacronyms > 1) {
          DpsSort(List->Acronym, List->nacronyms, sizeof(DPS_ACRONYM), &cmpacr);
	  List->sorted = 1;
  }
}


DPS_ACRONYM *DpsAcronymListFind(const DPS_ACRONYMLIST * List, DPS_WIDEWORD * wword, DPS_ACRONYM **store_last) {
     DPS_ACRONYM acr, *res, *first, *last;

     if (!List->nacronyms) return NULL;

     acr.a.uword = wword->uword;

     res = dps_bsearch(&acr, List->Acronym, List->nacronyms, sizeof(DPS_ACRONYM), &cmpacr);

     if (res == NULL) return NULL;

     /* Find first and last acronym */
     for(first = res; first >= List->Acronym; first--) {
       if(DpsUniStrCmp(wword->uword, first->a.uword)) {
	 first++;
	 break;
       }
     }
     for(last = res + 1; last < List->Acronym + List->nacronyms; last++) {
       if(DpsUniStrCmp(wword->uword,last->a.uword)) {
	 last--;
	 break;
       }
     }
     if (last >= List->Acronym + List->nacronyms) last--;
     *store_last = last;
     return first;
}
