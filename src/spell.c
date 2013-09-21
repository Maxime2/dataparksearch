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

#include "dps_common.h"
#include "dps_utils.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_spell.h"
#include "dps_xmalloc.h"
#include "dps_word.h"
#include "dps_synonym.h"
#include "dps_hash.h"
#include "dps_vars.h"
#include "dps_charsetutils.h"
#include "dps_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include <sys/mman.h>

#define MAXNORMLEN 256
#define MAX_NORM 512
#define ERRSTRSIZE 100

/*
#define DEBUG_UNIREG
*/

static void DpsAllFormsWord (DPS_AGENT *Indexer, DPS_SPELL *word, DPS_WIDEWORDLIST *result, size_t order, size_t order_inquery);


/* Unicode regex lite BEGIN */

static const dpsunicode_t *DpsUniRegTok(const dpsunicode_t *s, const dpsunicode_t **last) {
  int skip;
	if(s == NULL && (s=*last) == NULL)
		return NULL;

	skip = (*s == '\\');

	switch(*s){
		case 0:
			return(NULL);
			break;
		case '[':
		        if (!skip) {
			  for(*last=s+1;(**last)&&(**last!=']');(*last)++);
			  if(**last==']')(*last)++;
			  break;
			}
		case '$':
		case '^':
		        if (!skip) {
			  *last=s+1;
			  break;
			}
		default:
		        for(*last = s + 1; (**last) && (skip || ( (**last!=']')&&(**last!='[')&&(**last!='^')&&(**last!='$')) ); (*last)++) {
			  skip = (**last == '\\');
			}
			break;
	}
	return s;
}

int DpsUniRegComp(DPS_UNIREG_EXP *reg, const dpsunicode_t *pattern) {
	const dpsunicode_t *tok, *lt;
	size_t len;
#ifdef DEBUG_UNIREG
	size_t i, p;
	DPS_CHARSET *k = DpsGetCharSet("koi8-r");
	DPS_CHARSET *sy = DpsGetCharSet("sys-int");
	DPS_CONV fromuni;
	char sstr[1024];
	char rstr[1024];
	
	DpsConvInit(&fromuni, sy, k, NULL, 0);
#endif

	reg->ntokens=0;
	reg->Token=NULL;

#ifdef DEBUG_UNIREG

	DpsConv(&fromuni, sstr, sizeof(sstr), (char*)pattern, DpsUniLen(pattern) * sizeof(dpsunicode_t));
	printf(" -- pattern='%s'\n", sstr);
#endif

	tok=DpsUniRegTok(pattern,&lt);
	while(tok){
#ifdef DEBUG_UNIREG

	    DpsConv(&fromuni, sstr, sizeof(sstr), (char*)tok, dps_strlen(tok));
	    DpsConv(&fromuni, rstr, sizeof(rstr), (char*)lt, dps_strlen(lt));
	    printf(" -- tok:'%s' lt:'%s'\n", sstr, rstr);
#endif
		reg->Token=(DPS_UNIREG_TOK*)DpsRealloc(reg->Token,sizeof(*reg->Token)*(reg->ntokens+1));
		if (reg->Token == NULL) {
		  reg->ntokens = 0;
		  return DPS_ERROR;
		}
		len=lt-tok;
		reg->Token[reg->ntokens].str = (dpsunicode_t*)DpsMalloc((len+1)*sizeof(dpsunicode_t));
		dps_memcpy(reg->Token[reg->ntokens].str, tok, len * sizeof(dpsunicode_t));
                reg->Token[reg->ntokens].str[len]=0;

		reg->ntokens++;
		tok=DpsUniRegTok(NULL,&lt);
	}
#ifdef DEBUG_UNIREG
	for (i = 0; i < reg->ntokens; i++) {
	    DpsConv(&fromuni, sstr, sizeof(sstr), (char*)reg->Token[i].str, dps_strlen((char*)reg->Token[i].str));
	    printf(" -- str.%d:'%s'\n", i, sstr);
	}
	printf(" -- reg:%x ntokens:%d\n", reg, reg->ntokens);
#endif
	return DPS_OK;
}


int DpsUniRegExec(const DPS_UNIREG_EXP *reg, const dpsunicode_t *string) {
	const dpsunicode_t *start = string;
	int match=0;
#ifdef DEBUG_UNIREG
	DPS_CHARSET *k = DpsGetCharSet("koi8-r");
	DPS_CHARSET *sy = DpsGetCharSet("sys-int");
	DPS_CONV fromuni;
	char sstr[1024];
	char rstr[1024];
	
	DpsConvInit(&fromuni, sy, k, NULL, 0);
	printf(" -- reg:%x ntokens:%d\n", reg, reg->ntokens);
#endif
	
	for(start=string;*start;start++){
		const dpsunicode_t *tstart=start;
		size_t i;
		
		for(i=0;i<reg->ntokens;i++){
			const dpsunicode_t *s;
			int inc=DPS_UNIREG_INC;
#ifdef DEBUG_UNIREG

			DpsConv(&fromuni, sstr, sizeof(sstr), (char*)tstart, dps_strlen(tstart));
			DpsConv(&fromuni, rstr, sizeof(rstr), (char*)reg->Token[i].str, dps_strlen((char*)reg->Token[i].str));
			printf(" -- t:%d tstart='%s'\ttok='%s'\t", i, sstr, rstr);
#endif
			reg->Token[i].rm_so = tstart - start;
			switch(reg->Token[i].str[0]){
				case '^':
					if(string!=tstart){
					  return 0/*match=0*/;
					}else{	
						match=1;
					}
					break;
				case '[':
					match=0;
					for(s=reg->Token[i].str+1;*s;s++){
						if(*s==']'){
						}else
						if(*s=='^'){
							inc=DPS_UNIREG_EXC;
							match=1;
						}else{
							if((*tstart==*s)&&(inc==DPS_UNIREG_EXC)){
								match=0;
								break;
							}
							if((*tstart==*s)&&(inc==DPS_UNIREG_INC)){
								match=1;
								break;
							}
						}
					}
					tstart++;
					break;
				case '$':
					if(*tstart!=0){
						match=0;
					}else{
						match=1;
					}
					break;
				default:
					match=1;
					for(s=reg->Token[i].str;(*s)&&(*tstart);s++,tstart++){
						if(*s=='.'){
							/* Any char */
						}else {
						  if(*s == '\\') if (*(++s) == '\0') break;
						  if((*s)!=(*tstart)){
							match=0;
							break;
						  }
						}
					}
					if((*s)&&(!*tstart))match=0;
					break;
			}
#ifdef DEBUG_UNIREG
			printf("%d -- match=%d\n", __LINE__, match);
#endif
			reg->Token[i].rm_eo = tstart - start;
			if(!match)break;
		}
		if(match) break;
	}

#ifdef DEBUG_UNIREG
	printf("%d -- return match=%d\n", __LINE__, match);
#endif
	return match;

}


void DpsUniRegFree(DPS_UNIREG_EXP *reg){
	size_t i;
	
	for(i=0;i<reg->ntokens;i++)
		if(reg->Token[i].str)
			DPS_FREE(reg->Token[i].str);

	DPS_FREE(reg->Token);
}


/* Unicode regex lite END */


void DpsUniRegCompileAll(DPS_ENV *Conf) {
        size_t i;
	
	for(i = 0; i < Conf->Affixes.naffixes; i++) {
	  if(!DpsUniRegComp(&Conf->Affixes.Affix[i].reg, Conf->Affixes.Affix[i].mask)) {
	    Conf->Affixes.Affix[i].compile = 0;
	  }
	}
	for(i = 0; i < Conf->Quffixes.nrecs; i++) {
	  if(!DpsUniRegComp(&Conf->Quffixes.Quffix[i].reg, Conf->Quffixes.Quffix[i].mask)) {
	    Conf->Quffixes.Quffix[i].compile = 0;
	  }
	}
}

static int cmpspellword(dpsunicode_t *w1, const dpsunicode_t *w2) {
    register dpsunicode_t u1 = (*w1 & 255), u2 = (*w2 & 255);
    if (u1 < u2) return -1;
    if (u1 > u2) return 1;
    if (u1 == 0) return 0;
    return DpsUniStrCmp(w1 + 1, w2 + 1);
}

static int cmpspell(const void *s1, const void *s2) {
  int lc;
  lc = strcmp(((const DPS_SPELL*)s1)->lang,((const DPS_SPELL*)s2)->lang);
  if (lc == 0) {
    lc = cmpspellword(((const DPS_SPELL*)s1)->word, ((const DPS_SPELL*)s2)->word );
  }
  return lc;
}

static int cmpaffix(const void *s1,const void *s2){
  int lc;
  if (((const DPS_AFFIX*)s1)->type < ((const DPS_AFFIX*)s2)->type) {
    return -1;
  }
  if (((const DPS_AFFIX*)s1)->type > ((const DPS_AFFIX*)s2)->type) {
    return 1;
  }
  lc = strcmp(((const DPS_AFFIX*)s1)->lang,((const DPS_AFFIX*)s2)->lang);
  if (lc == 0) {
    if ( (((const DPS_AFFIX*)s1)->replen == 0) && (((const DPS_AFFIX*)s2)->replen == 0) ) {
      return 0;
    }
    if (((const DPS_AFFIX*)s1)->replen == 0) {
      return -1;
    }
    if (((const DPS_AFFIX*)s2)->replen == 0) {
      return 1;
    }
    {
      dpsunicode_t u1[BUFSIZ], u2[BUFSIZ];
      DpsUniStrCpy(u1,((const DPS_AFFIX*)s1)->repl); 
      DpsUniStrCpy(u2,((const DPS_AFFIX*)s2)->repl); 
      if (((const DPS_AFFIX*)s1)->type == 'p') {
	*u1 &= 255; *u2 &= 255;
	return DpsUniStrCmp(u1, u2);
      } else {
	u1[((const DPS_AFFIX*)s1)->replen - 1] &= 255; u2[((const DPS_AFFIX*)s2)->replen -1] &= 255;
	return DpsUniStrBCmp(u1, u2);
      }
    }
  }
  return lc;
}

static int cmpquffix(const void *s1,const void *s2){
  int lc;
  lc = strcmp(((const DPS_QUFFIX*)s1)->lang,((const DPS_QUFFIX*)s2)->lang);
  if (lc == 0) {
    if ( (((const DPS_QUFFIX*)s1)->findlen == 0) && (((const DPS_QUFFIX*)s2)->findlen == 0) ) {
      return 0;
    }
    if (((const DPS_QUFFIX*)s1)->findlen == 0) {
      return -1;
    }
    if (((const DPS_QUFFIX*)s2)->findlen == 0) {
      return 1;
    }
    {
      dpsunicode_t u1[BUFSIZ], u2[BUFSIZ];
      DpsUniStrCpy(u1,((const DPS_QUFFIX*)s1)->find); 
      DpsUniStrCpy(u2,((const DPS_QUFFIX*)s2)->find); 
      *u1 &= 255; *u2 &= 255;
      return DpsUniStrBCmp(u1, u2);
    }
  }
  return lc;
}


int DpsSpellAdd(DPS_SPELLLIST *List, const dpsunicode_t *word, const char *flag, const char *lang) {
	if(List->nspell>=List->mspell){
		List->mspell += 1024; /* was: 1024 * 20 */
		List->Spell=(DPS_SPELL *)DpsXrealloc(List->Spell,List->mspell*sizeof(DPS_SPELL));
		if (List->Spell == NULL) return DPS_ERROR;
	}
	List->Spell[List->nspell].word = DpsUniRDup(word);
	dps_strncpy(List->Spell[List->nspell].flag,flag,10);
	dps_strncpy(List->Spell[List->nspell].lang,lang,5);
	List->Spell[List->nspell].lang[5] = List->Spell[List->nspell].flag[10] = '\0';
	List->nspell++;
	List->sorted = 0;
	return DPS_OK;
}


__C_LINK int __DPSCALL DpsImportDictionary(DPS_AGENT *query, const char *lang, const char *charset,
				   const char *filename, int skip_noflag, const char *first_letters){
        struct stat     sb;
	char *str, *data = NULL, *cur_n = NULL;	
	char *lstr;	
	dpsunicode_t *ustr;
	DPS_ENV  *Conf = query->Conf;	
	DPS_CHARSET *sys_int;
	DPS_CHARSET *dict_charset;
	DPS_CONV touni;
	DPS_CONV fromuni;
	int             fd;
	char            savebyte;
#if defined HAVE_ASPELL
	DPS_CHARSET *utf8;
	DPS_CONV toutf8;
	AspellCanHaveError *ret;
	AspellSpeller *speller = NULL;
	int use_aspellext = Conf->Flags.use_aspellext;
	size_t naffixes = query->Conf->Affixes.naffixes;
	DPS_AFFIX *Affix = (DPS_AFFIX *)query->Conf->Affixes.Affix;

	if (use_aspellext) {
	  aspell_config_replace(query->aspell_config, "lang", lang);
	  /*
	  fprintf(stderr, " -- Using: %s\n", aspell_config_retrieve(query->aspell_config, "lang"));
	  fprintf(stderr, " -- Using: %s\n", aspell_config_retrieve(query->aspell_config, "encoding"));
	  fprintf(stderr, " -- Using: %s\n", aspell_config_retrieve(query->aspell_config, "home-dir"));
	  fprintf(stderr, " -- Using: %s\n", aspell_config_retrieve(query->aspell_config, "personal-path"));
	  */
	  ret = new_aspell_speller(query->aspell_config);
	  if (aspell_error(ret) != 0) {
	    DpsLog(query, DPS_LOG_ERROR, "ImportDictionary: aspell error: %s", aspell_error_message(ret));
	    delete_aspell_can_have_error(ret);
	    use_aspellext = 0;
	  } else {
	    speller = to_aspell_speller(ret);
	  }
	}
#endif

	if ((lstr = (char*) DpsMalloc(2048)) == NULL) {
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
	  return DPS_ERROR; 
	}
	if ((ustr = (dpsunicode_t*) DpsMalloc(8192)) == NULL) {
	  DPS_FREE(lstr);
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
	  return DPS_ERROR; 
	}

	dict_charset = DpsGetCharSet(charset);
	sys_int = DpsGetCharSet("sys-int");
	if ((dict_charset == NULL) || (sys_int == NULL)) {
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
	  return DPS_ERROR;
	}
#ifdef HAVE_ASPELL
	utf8 = DpsGetCharSet("UTF-8");
	if (utf8 == NULL) {
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
	  return DPS_ERROR;
	}
	DpsConvInit(&toutf8, sys_int, utf8, Conf->CharsToEscape, 0);
#endif
	
	DpsConvInit(&touni, dict_charset, sys_int, Conf->CharsToEscape, 0);
	DpsConvInit(&fromuni, sys_int, dict_charset, Conf->CharsToEscape, 0);
	
	if (stat(filename, &sb)) {
	  dps_strerror(NULL, 0, "Unable to stat synonyms file '%s'", filename);
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
	  return DPS_ERROR;
	}
	if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
	  dps_strerror(NULL, 0, "Unable to open synonyms file '%s'", filename);
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
	  return DPS_ERROR;
	}
	if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
	  fprintf(stderr, "Unable to alloc %ld bytes", (long)sb.st_size);
	  DpsClose(fd);
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
	  return DPS_ERROR;
	}
	if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
	  dps_strerror(NULL, 0, "Unable to read synonym file '%s'", filename);
	  DPS_FREE(data);
	  DpsClose(fd);
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	  if (use_aspellext) {
	    delete_aspell_speller(speller);
	  }
#endif
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
	while(str != NULL) {
		char *s;
		const char *flag;
		int res;
		
	        flag = NULL;
		s = str;
		while(*s){
			if(*s == CR_CHAR) *s = '\0';
			if(*s == NL_CHAR) *s = '\0';
			s++;
		}
		if((s=strchr(str,'/'))){
			*s=0;
			s++;flag=s;
			while(*s){
				if(((*s>='A')&&(*s<='Z'))||((*s>='a')&&(*s<='z')))s++;
				else{
					*s=0;
					break;
				}
			}
		}else{
			if(skip_noflag)	goto loop_continue;
			flag="";
		}

		res = DpsConv(&touni, (char*)ustr, 8192, str, dps_strlen(str));
		DpsUniStrToLower(ustr);

		/* Dont load words if first letter is not required */
		/* It allows to optimize loading at  search time   */
		if(*first_letters) {
			DpsConv(&fromuni, lstr, 2048, ((const char*)ustr),(size_t)4096);
			if(!strchr(first_letters,lstr[0]))
				goto loop_continue;
		}
#ifdef HAVE_ASPELL
		if (use_aspellext) {
		  DPS_SPELL word;
		  DPS_WIDEWORDLIST forms;
		  DPS_WIDEWORD ww;
		  size_t frm;
		  word.word = DpsUniRDup(ustr);
		  dps_strncpy(word.lang, lang, sizeof(word.lang));
		  dps_strncpy(word.flag, flag, sizeof(word.flag));
		  bzero(&forms, sizeof(forms));
		  bzero(&ww, sizeof(ww));

		  ww.uword = ustr;
		  DpsWideWordListAdd(&forms, &ww, DPS_WWL_LOOSE);
		  DpsAllFormsWord(query, &word, &forms, 0, 0);
		  DpsFree(word.word);
		  for (frm = 0; frm < forms.nwords; frm++) {
		  
		      DpsConv(&toutf8, lstr, 2048, ((const char*)forms.Word[frm].uword), sizeof(forms.Word[frm].uword[0]) * DpsUniLen(forms.Word[frm].uword));
		    /*		    fprintf(stderr, " -- frm:%d - %s\n", frm, forms.Word[frm].word);*/
		    if (aspell_speller_check(speller, lstr, -1) == 0) {
		      aspell_speller_add_to_personal(speller, lstr, -1);
		    }
		  }
		  DpsWideWordListFree(&forms);
		}
#endif
		res = DpsSpellAdd(&Conf->Spells,ustr,flag,lang);
		if (res != DPS_OK) {
		  DPS_FREE(lstr);
		  DPS_FREE(ustr); DPS_FREE(data);
#ifdef HAVE_ASPELL
		  if (use_aspellext) {
		    aspell_speller_save_all_word_lists(speller);
		    delete_aspell_speller(speller);
		  }
#endif
		  return res;
		}
		if (Conf->Flags.use_accentext) {
		  dpsunicode_t *af_uwrd = DpsUniAccentStrip(ustr);
		  if (DpsUniStrCmp(af_uwrd, ustr) != 0) {
		    res = DpsSpellAdd(&Conf->Spells, af_uwrd, flag, lang);
		    if (res != DPS_OK) {
		      DPS_FREE(lstr);
		      DPS_FREE(ustr); DPS_FREE(data); DPS_FREE(af_uwrd);
#ifdef HAVE_ASPELL
		      if (use_aspellext) {
			aspell_speller_save_all_word_lists(speller);
			delete_aspell_speller(speller);
		      }
#endif
		      return res;
		    }
		  }
		  DPS_FREE(af_uwrd);
		  if (strncasecmp(lang, "de", 2) == 0) {
		    dpsunicode_t *de_uwrd = DpsUniGermanReplace(ustr);
		    if (DpsUniStrCmp(de_uwrd, ustr) != 0) {
		      res = DpsSpellAdd(&Conf->Spells, de_uwrd, flag, lang);
		      if (res != DPS_OK) {
			DPS_FREE(lstr);
			DPS_FREE(ustr); DPS_FREE(data); DPS_FREE(de_uwrd);
#ifdef HAVE_ASPELL
			if (use_aspellext) {
			  aspell_speller_save_all_word_lists(speller);
			  delete_aspell_speller(speller);
			}
#endif
			return res;
		      }
		    }
		    DPS_FREE(de_uwrd);
		  }
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
	DPS_FREE(lstr);
	DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	if (use_aspellext) {
	  aspell_speller_save_all_word_lists(speller);
	  delete_aspell_speller(speller);
	}
#endif
	return DPS_OK;
}




static DPS_SPELL ** DpsFindWord(DPS_AGENT * Indexer, const dpsunicode_t *p_word, const char *affixflag, DPS_PSPELL *PS, DPS_PSPELL *FZ) {
  int l,c,r,resc,nlang = /*Indexer->SpellLang*/ -1 /*Indexer->spellang FIXME: if Language limit issued in query */, 
    li, li_from, li_to, i, ii;
  DPS_SPELLLIST *SpellList=&Indexer->Conf->Spells;
  dpsunicode_t *word = DpsUniRDup(p_word);

  if (nlang == -1) {
    li_from = 0; li_to = SpellList->nLang;
  } else {
    li_from = nlang; li_to = li_from + 1;
  }
  if (Indexer->Conf->Spells.nspell) {
    i = (int)(*word) & 255;
    for(li = li_from; li < li_to; li++) {
      l = SpellList->SpellTree[li].Left[i];
      r = SpellList->SpellTree[li].Right[i];
      if (l == -1) continue;
      while(l<=r){
	c = l + ((r - l) >> 1);
	resc = cmpspellword(SpellList->Spell[c].word, word);
	if( (resc == 0) && 
	    ((affixflag == NULL) || (strstr(SpellList->Spell[c].flag, affixflag) != NULL)) ) {
	  if (PS->nspell < MAX_NORM - 1) {
	    PS->cur[PS->nspell] = &SpellList->Spell[c];
	    PS->nspell++;
	    PS->cur[PS->nspell] = NULL;
	  }
	  break;
	}
	if(resc < 0){
	  l = c + 1;
	} else {
	  r = c - 1;
	}
      }

      for (ii = c - 1; ii >= l; ii--) {
	resc = cmpspellword(SpellList->Spell[ii].word, word);
	if(resc == 0) {
	  if ((affixflag == NULL) || (strstr(SpellList->Spell[ii].flag, affixflag) != NULL))  {
	    if (PS->nspell < MAX_NORM - 1) {
	      PS->cur[PS->nspell] = &SpellList->Spell[ii];
	      PS->nspell++;
	      PS->cur[PS->nspell] = NULL;
	    }
	  }
	} else break;
      }
      for (ii = c + 1; ii <= r; ii++) {
	resc = cmpspellword(SpellList->Spell[ii].word, word);
	if(resc == 0) {
	  if ((affixflag == NULL) || (strstr(SpellList->Spell[ii].flag, affixflag) != NULL))  {
	    if (PS->nspell < MAX_NORM - 1) {
	      PS->cur[PS->nspell] = &SpellList->Spell[ii];
	      PS->nspell++;
	      PS->cur[PS->nspell] = NULL;
	    }
	  }
	} else break;
      }

      if ((FZ != NULL) && (PS->nspell == 0) && ((affixflag == NULL) || (strstr(SpellList->Spell[c].flag, affixflag) != NULL)) ) {
	register size_t z; /*, lw = DpUniLen(word), ls = DpsUniLen(SpellList->Spell[c].word[z]);*/
	for (z = 0; word[z] && SpellList->Spell[c].word[z] == word[z]; z++);
	if (z > 3) { /* FIXME: min. 4 letters from the end, - add config parameter for that */ 
	  if (z > FZ->nspell) {
	    DPS_FREE(FZ->cur[0]->word);
	    dps_memcpy(FZ->cur[0]->flag, SpellList->Spell[c].flag, sizeof(FZ->cur[0]->flag));
	    dps_memcpy(FZ->cur[0]->lang, SpellList->Spell[c].lang, sizeof(FZ->cur[0]->lang));
	    FZ->cur[0]->word = DpsUniDup(word);
	    FZ->nspell = z;
	  } else if (z == FZ->nspell) {
	    size_t flen0 = dps_strlen(FZ->cur[0]->flag);
	    dps_strncat(FZ->cur[0]->flag + flen0, SpellList->Spell[c].flag, sizeof(FZ->cur[0]->flag) - flen0);
	  }
	}
	{
	  register int q;
	  for (q = c - 1; q >= 0; q--) {
	    register size_t y;
	    for (y = 0; word[y] && SpellList->Spell[q].word[y] == word[y]; y++);
	    if (y < 3) break;
	    if (y < z) continue;
	    if (dps_strlen(FZ->cur[0]->flag) < dps_strlen(SpellList->Spell[q].flag)) {
	      DPS_FREE(FZ->cur[0]->word);
	      dps_memcpy(FZ->cur[0]->flag, SpellList->Spell[q].flag, sizeof(FZ->cur[0]->flag));
	      dps_memcpy(FZ->cur[0]->lang, SpellList->Spell[q].lang, sizeof(FZ->cur[0]->lang));
	      FZ->cur[0]->word = DpsUniDup(word);
	      FZ->nspell = z;
	    }
	  }
	  for (q = c + 1; q <= SpellList->SpellTree[li].Right[i]; q++) {
	    register size_t y;
	    for (y = 0; word[y] && SpellList->Spell[q].word[y] == word[y]; y++);
	    if (y < 3) break;
	    if (y < z) continue;
	    if (dps_strlen(FZ->cur[0]->flag) <= dps_strlen(SpellList->Spell[q].flag)) {
	      DPS_FREE(FZ->cur[0]->word);
	      dps_memcpy(FZ->cur[0]->flag, SpellList->Spell[q].flag, sizeof(FZ->cur[0]->flag));
	      dps_memcpy(FZ->cur[0]->lang, SpellList->Spell[q].lang, sizeof(FZ->cur[0]->lang));
	      FZ->cur[0]->word = DpsUniDup(word);
	      FZ->nspell = z;
	    }
	  }
	}
	
      }
      

    }
  }

  DPS_FREE(word);
  return PS->cur;
}


int DpsAffixAdd(DPS_AFFIXLIST *List, const char *flag, const char * lang, const dpsunicode_t *mask, const dpsunicode_t *find, 
		const dpsunicode_t *repl, int type) {

	if(List->naffixes>=List->maffixes){
		List->maffixes+=16;
		List->Affix = DpsXrealloc(List->Affix,List->maffixes*sizeof(DPS_AFFIX));
		if (List->Affix == NULL) return DPS_ERROR;
	}

	List->Affix[List->naffixes].compile = 1;
	List->Affix[List->naffixes].flag[0] = flag[0];
	List->Affix[List->naffixes].flag[1] = flag[1];
	List->Affix[List->naffixes].flag[2] = '\0';
	List->Affix[List->naffixes].type = (char)type;
	dps_strncpy(List->Affix[List->naffixes].lang, lang, 5);
	List->Affix[List->naffixes].lang[5] = 0;
	
	DpsUniStrNCpy(List->Affix[List->naffixes].mask, mask, 40);
	DpsUniStrNCpy(List->Affix[List->naffixes].find,find, 15);
	DpsUniStrNCpy(List->Affix[List->naffixes].repl,repl, 15);
	
	List->Affix[List->naffixes].replen  = DpsUniLen(repl);
	List->Affix[List->naffixes].findlen = DpsUniLen(find);
	List->naffixes++;
	List->sorted = 0;
	return DPS_OK;
}


int DpsQuffixAdd(DPS_QUFFIXLIST *List, const char *flag, const char * lang, const dpsunicode_t *mask, const dpsunicode_t *find, const dpsunicode_t *repl) {

	if(List->nrecs >= List->mrecs) {
		List->mrecs += 16;
		List->Quffix = (DPS_QUFFIX*)DpsXrealloc(List->Quffix, List->mrecs * sizeof(DPS_QUFFIX));
		if (List->Quffix == NULL) return DPS_ERROR;
	}

	List->Quffix[List->nrecs].compile = 1;
	List->Quffix[List->nrecs].flag[0] = flag[0];
	List->Quffix[List->nrecs].flag[1] = flag[1];
	List->Quffix[List->nrecs].flag[2] = '\0';
	dps_strncpy(List->Quffix[List->nrecs].lang, lang, 5);
	List->Quffix[List->nrecs].lang[5] = 0;
	
	DpsUniStrNCpy(List->Quffix[List->nrecs].mask, mask, 40);
	DpsUniStrNCpy(List->Quffix[List->nrecs].find,find, 15);
	DpsUniStrNCpy(List->Quffix[List->nrecs].repl,repl, 15);
	
	List->Quffix[List->nrecs].replen  = DpsUniLen(repl);
	List->Quffix[List->nrecs].findlen = DpsUniLen(find);
	List->nrecs++;
	List->sorted = 0;
	return DPS_OK;
}


static char * remove_spaces(char *dist,char *src){
char *d,*s;
	d=dist;
	s=src;
	while(*s){
		if((*s != ' ')&& (*s != '-') && (*s != HT_CHAR)){
			*d=*s;
			d++;
		}
		s++;
	}
	*d=0;
	return(dist);
}


__C_LINK int __DPSCALL DpsImportAffixes(DPS_ENV * Conf,const char *lang, const char*charset, 
				const char *filename) {
  struct stat     sb;
  char      *str, *data = NULL, *cur_n = NULL;
  char flag[2]="";
  char mstr[14*BUFSIZ]="";
  char mask[14*BUFSIZ]="";
  char find[14*BUFSIZ]="";
  char repl[14*BUFSIZ]="";
  char *s;
  int i;
  int suffixes=0;
  int prefixes=0;
  int IspellUsePrefixes;
  DPS_CHARSET *affix_charset = NULL;
  dpsunicode_t unimask[BUFSIZ];
  dpsunicode_t ufind[BUFSIZ];
  dpsunicode_t urepl[BUFSIZ];
  size_t len;
  DPS_CHARSET *sys_int;
  DPS_CONV touni;
  int             fd;
  char            savebyte;
#ifdef DEBUG_UNIREG
  DPS_CONV fromuni;
#endif
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif

  if (stat(filename, &sb)) {
    dps_strerror(NULL, 0, "Unable to stat affixes file '%s': %s", filename);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
    dps_strerror(NULL, 0, "Unable to open affixes file '%s'", filename);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "Unable to alloc %d bytes", sb.st_size);
    DpsClose(fd);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
    dps_strerror(NULL, 0, "Unable to read affixes file '%s'", filename);
    DPS_FREE(data);
    DpsClose(fd);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
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

  affix_charset = DpsGetCharSet(charset);
  if (affix_charset == NULL) {
      DPS_FREE(data);
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return 1;
  }
  sys_int = DpsGetCharSet("sys-int");
  if (sys_int == NULL) {
      DPS_FREE(data);
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return 1;
  }
	    
  DpsConvInit(&touni, affix_charset, sys_int, Conf->CharsToEscape, 0);
#ifdef DEBUG_UNIREG
  DpsConvInit(&fromuni, sys_int, affix_charset, Conf->CharsToEscape, 0);
#endif

  IspellUsePrefixes = strcasecmp(DpsVarListFindStr(&Conf->Vars,"IspellUsePrefixes","no"),"no");

  while(str != NULL) {
    str = DpsTrim(str, "\t ");
    if(!strncasecmp(str,"suffixes",8)){
      suffixes=1;
      prefixes=0;
      goto loop2_continue;
    }
    if(!strncasecmp(str,"prefixes",8)){
      suffixes=0;
      prefixes=1;
      goto loop2_continue;
    }
    if(!strncasecmp(str,"flag ",5)){
      s=str+5;
      while(strchr("* ",*s))s++;
      flag[0] = s[0];
      flag[1] = (s[1] >= 'A') ? s[1] : (char)'\0';
      goto loop2_continue;
    }
    if((!suffixes)&&(!prefixes)) goto loop2_continue;
    if((prefixes)&&(!IspellUsePrefixes)) goto loop2_continue;
		
    if((s=strchr(str,'#')))*s=0;
    if(!*str) goto loop2_continue;

    dps_strcpy(mask,"");
    dps_strcpy(find,"");
    dps_strcpy(repl,"");

    i=sscanf(str,"%[^>\n]>%[^,\n],%[^\n]",mask,find,repl);

    remove_spaces(mstr, repl); dps_strcpy(repl, mstr);
    remove_spaces(mstr, find); dps_strcpy(find, mstr);
    remove_spaces(mstr, mask); dps_strcpy(mask, mstr);

    switch(i){
    case 3:break;
    case 2:
      if(*find != '\0'){
	dps_strcpy(repl,find);
	dps_strcpy(find,"");
      }
      break;
    default:
      goto loop2_continue;
    }

    len=DpsConv(&touni,(char*)urepl,sizeof(urepl),repl, dps_strlen(repl)+1);
    DpsUniStrToLower(urepl);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni,repl,sizeof(repl),(char *)urepl,len);
#endif
		    
    len=DpsConv(&touni,(char*)ufind,sizeof(ufind),find, dps_strlen(find)+1);
    DpsUniStrToLower(ufind);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni,find,sizeof(find),(char*)ufind,len);
#endif

    if (suffixes) {
      sprintf(mstr, "%s$", mask);
    } else {
      sprintf(mstr, "^%s", mask);
    }

    len = DpsConv(&touni, (char*)unimask, sizeof(unimask), mstr, dps_strlen(mstr) + 1);
    DpsUniStrToLower(unimask);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni, mask, sizeof(mask), (char*)unimask, len);
#endif

/*    fprintf(stderr, "-- %s | %c | %s : %s > %s\n", flag, suffixes ? 's' : 'p', mask, find, repl);*/

    DpsAffixAdd(&Conf->Affixes, flag, lang, unimask, ufind, urepl, suffixes ? 's' : 'p');
    if (Conf->Flags.use_accentext) {
      dpsunicode_t *af_unimask = DpsUniAccentStrip(unimask);
      dpsunicode_t *af_ufind = DpsUniAccentStrip(ufind);
      dpsunicode_t *af_urepl = DpsUniAccentStrip(urepl);
      size_t zz;
      for (zz = 0; zz < 2; zz++) {
	if (DpsUniStrCmp(af_unimask, unimask) || DpsUniStrCmp(af_ufind, ufind) || DpsUniStrCmp(af_urepl, urepl)) {
	  size_t w_len = DpsUniLen(af_unimask);
	  dpsunicode_t *new_wrd = DpsMalloc(2 * sizeof(dpsunicode_t) * w_len);
	  if (new_wrd == NULL) {
	    DpsAffixAdd(&Conf->Affixes, flag, lang, af_unimask, af_ufind, af_urepl, suffixes ? 's' : 'p');
	  } else {
	    size_t ii, pnew = 0;
	    int in_pattern = 0;
	    for (ii = 0; ii < w_len; ii++) {
	      if ((af_unimask[ii] == (dpsunicode_t)'[') && (af_unimask[ii + 1] == (dpsunicode_t)'^') ) {
		in_pattern = 1;
	      } else if (in_pattern && (af_unimask[ii] == (dpsunicode_t)']')) {
		in_pattern = 0;
	      } else if (in_pattern && (af_unimask[ii] != unimask[ii])) {
		new_wrd[pnew++] = unimask[ii];
	      }
	      new_wrd[pnew++] = af_unimask[ii];
	    }
	    new_wrd[pnew] = 0;
	    DpsAffixAdd(&Conf->Affixes, flag, lang, new_wrd, af_ufind, af_urepl, suffixes ? 's' : 'p');
	    DPS_FREE(new_wrd);
	  }
	}
	DPS_FREE(af_unimask); DPS_FREE(af_ufind); DPS_FREE(af_urepl);
	if ((zz == 0) && (strncasecmp(lang, "de", 2) == 0)) {
	  af_unimask = DpsUniGermanReplace(unimask);
	  af_ufind = DpsUniGermanReplace(ufind);
	  af_urepl = DpsUniGermanReplace(urepl);
	} else zz = 2;
      }
    }
  loop2_continue:
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
  /*	
  if (Conf->Affixes.naffixes > 1)
    DpsSort((void*)Conf->Affixes.Affix,Conf->Affixes.naffixes,sizeof(DPS_AFFIX),cmpaffix);
  */
#ifdef WITH_PARANOIA
  DpsViolationExit(-1, paran);
#endif
  return 0;
}



__C_LINK int __DPSCALL DpsImportQuffixes(DPS_ENV * Conf,const char *lang, const char*charset, const char *filename) {
  struct stat     sb;
  char      *str, *data = NULL, *cur_n = NULL;
  char flag[2]="";
  char mstr[14*BUFSIZ]="";
  char mask[14*BUFSIZ]="";
  char find[14*BUFSIZ]="";
  char repl[14*BUFSIZ]="";
  char *s;
  int i;
  DPS_CHARSET *qreg_charset = NULL;
  dpsunicode_t unimask[BUFSIZ];
  dpsunicode_t ufind[BUFSIZ];
  dpsunicode_t urepl[BUFSIZ];
  size_t len;
  DPS_CHARSET *sys_int;
  DPS_CONV touni;
  int             fd;
  char            savebyte;
#ifdef DEBUG_UNIREG
  DPS_CONV fromuni;
#endif
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif

  if (stat(filename, &sb)) {
    dps_strerror(NULL, 0, "Unable to stat query regs file '%s'", filename);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
    dps_strerror(NULL, 0, "Unable to open query regs file '%s'", filename);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "Unable to alloc %d bytes", sb.st_size);
    DpsClose(fd);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
    dps_strerror(NULL, 0, "Unable to read query regs file '%s'", filename);
    DPS_FREE(data);
    DpsClose(fd);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
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

  qreg_charset = DpsGetCharSet(charset);
  if (qreg_charset == NULL) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  sys_int = DpsGetCharSet("sys-int");
  if (sys_int == NULL) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
	    
  DpsConvInit(&touni, qreg_charset, sys_int, Conf->CharsToEscape, 0);
#ifdef DEBUG_UNIREG
  DpsConvInit(&fromuni, sys_int, qreg_charset, Conf->CharsToEscape, 0);
#endif

  while(str != NULL) {
    str = DpsTrim(str, "\t ");
    if(!strncasecmp(str,"flag ",5)){
      s=str+5;
      while(strchr("* ",*s))s++;
      flag[0] = s[0];
      flag[1] = (s[1] >= 'A') ? s[1] : '\0';
      goto loop2_continue;
    }
		
    if((s=strchr(str,'#')))*s=0;
    if(!*str) goto loop2_continue;

    dps_strcpy(mask,"");
    dps_strcpy(repl,"");

    i=sscanf(str,"%[^>\n]>%[^,\n],%[^\n]",mask,find,repl);
/*    i=sscanf(str,"%[^>\n]>%[^\n]",mask,repl);*/

    remove_spaces(mstr, repl); dps_strcpy(repl, mstr);
    remove_spaces(mstr, find); dps_strcpy(find, mstr);
    remove_spaces(mstr, mask); dps_strcpy(mask, mstr);

    switch(i){
    case 3:break;
    case 2:
      if(*find != '\0'){
	dps_strcpy(repl,find);
	dps_strcpy(find,"");
      }
      break;
    default:
      goto loop2_continue;
    }

    len=DpsConv(&touni,(char*)urepl,sizeof(urepl),repl, dps_strlen(repl)+1);
    DpsUniStrToLower(urepl);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni,repl,sizeof(repl),(char *)urepl,len);
#endif
		    
    len=DpsConv(&touni,(char*)ufind,sizeof(ufind),find, dps_strlen(find)+1);
    DpsUniStrToLower(ufind);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni,find,sizeof(find),(char*)ufind,len);
#endif

    dps_snprintf(mstr, sizeof(mstr), "%s$", mask);

    len = DpsConv(&touni, (char*)unimask, sizeof(unimask), mstr, dps_strlen(mstr) + 1);
    DpsUniStrToLower(unimask);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni, mask, sizeof(mask), (char*)unimask, len);
#endif

/*    fprintf(stderr, "-- %s | %c | %s : %s > %s\n", flag, suffixes ? 's' : 'p', mask, find, repl);*/

    DpsQuffixAdd(&Conf->Quffixes, flag, lang, unimask, ufind, urepl);
    if (Conf->Flags.use_accentext) {
      dpsunicode_t *af_unimask = DpsUniAccentStrip(unimask);
      dpsunicode_t *af_ufind = DpsUniAccentStrip(ufind);
      dpsunicode_t *af_urepl = DpsUniAccentStrip(urepl);
      size_t zz;
      for (zz = 0; zz < 2; zz++) {
	if (DpsUniStrCmp(af_unimask, unimask)  || DpsUniStrCmp(af_ufind, ufind) || DpsUniStrCmp(af_urepl, urepl)) {
	  size_t w_len = DpsUniLen(af_unimask);
	  dpsunicode_t *new_wrd = DpsMalloc(2 * sizeof(dpsunicode_t) * w_len);
	  if (new_wrd == NULL) {
	    DpsQuffixAdd(&Conf->Quffixes, flag, lang, af_unimask, af_ufind, af_urepl);
	  } else {
	    size_t ii, pnew = 0;
	    int in_pattern = 0;
	    for (ii = 0; ii < w_len; ii++) {
	      if ((af_unimask[ii] == (dpsunicode_t)'[') && (af_unimask[ii + 1] == (dpsunicode_t)'^') ) {
		in_pattern = 1;
	      } else if (in_pattern && (af_unimask[ii] == (dpsunicode_t)']')) {
		in_pattern = 0;
	      } else if (in_pattern && (af_unimask[ii] != unimask[ii])) {
		new_wrd[pnew++] = unimask[ii];
	      }
	      new_wrd[pnew++] = af_unimask[ii];
	    }
	    new_wrd[pnew] = 0;
	    DpsQuffixAdd(&Conf->Quffixes, flag, lang, new_wrd, af_ufind, af_urepl);
	    DPS_FREE(new_wrd);
	  }
	}
	DPS_FREE(af_unimask); DPS_FREE(af_ufind); DPS_FREE(af_urepl);
	if ((zz == 0) && (strncasecmp(lang, "de", 2) == 0)) {
	  af_unimask = DpsUniGermanReplace(unimask);
	  af_ufind = DpsUniGermanReplace(ufind);
	  af_urepl = DpsUniGermanReplace(urepl);
	} else zz = 2;
      }
    }
  loop2_continue:
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
	    
#ifdef WITH_PARANOIA
  DpsViolationExit(-1, paran);
#endif
  return 0;
}




__C_LINK void __DPSCALL DpsSortDictionary(DPS_SPELLLIST * List){
  int  j, CurLet = -1, Let;size_t i;
  char *CurLang = NULL;

  if (List->sorted) return;
        if (List->nspell > 1) DpsSort((void*)List->Spell, List->nspell, sizeof(DPS_SPELL), cmpspell);
	for(i = 0; i < List->nspell; i++) {
	  if (CurLang == NULL || strncmp(CurLang, List->Spell[i].lang, 2) != 0) {
	    CurLang = List->Spell[i].lang;
	    dps_strncpy(List->SpellTree[List->nLang].lang, CurLang, 2);
	    List->SpellTree[List->nLang].lang[3] = 0;
	    for(j = 0; j < 256; j++)
	      List->SpellTree[List->nLang].Left[j] =
		List->SpellTree[List->nLang].Right[j] = -1;
	    if (List->nLang > 0) {
	      CurLet = -1;
	    }
	    List->nLang++;
	  }
	  Let = (int)(*(List->Spell[i].word)) & 255;
	  if (CurLet != Let) {
	    List->SpellTree[List->nLang-1].Left[Let] = i;
	    CurLet = Let;
	  }
	  List->SpellTree[List->nLang-1].Right[Let] = i;
	}
	List->sorted = 1;
}


__C_LINK void __DPSCALL DpsSortAffixes(DPS_AFFIXLIST *List, DPS_SPELLLIST *SL) {
  int  CurLetP = -1, CurLetS = -1, Let, cl = -1;
  char *CurLangP = NULL, *CurLangS = NULL;
  DPS_AFFIX *Affix; size_t i, j;

  if (List->sorted) return;
  if (List->naffixes > 1)
    DpsSort((void*)List->Affix,List->naffixes,sizeof(DPS_AFFIX),cmpaffix);

  for(i = 0; i < SL->nLang; i++)
    for(j = 0; j < 256; j++) {
      List->PrefixTree[i].Left[j] = List->PrefixTree[i].Right[j] = -1;
      List->SuffixTree[i].Left[j] = List->SuffixTree[i].Right[j] = -1;
    }

  for(i = 0; i < List->naffixes; i++) {
    Affix = &(((DPS_AFFIX*)List->Affix)[i]);
    if(Affix->type == 'p') {
      if (CurLangP == NULL || strcmp(CurLangP, Affix->lang) != 0) {
	cl = -1;
	for (j = 0; j < SL->nLang; j++) {
	  if (strncmp(SL->SpellTree[j].lang, Affix->lang, 2) == 0) {
	    cl = j;
	    break;
	  }
	}
	CurLangP = Affix->lang;
	dps_strcpy(List->PrefixTree[cl].lang, CurLangP);
	CurLetP = -1;
      }
      if (cl < 0) continue; /* we have affixes without spell for this lang */
      Let = (int)(*(Affix->repl)) & 255;
      if (CurLetP != Let) {
	List->PrefixTree[cl].Left[Let] = i;
	CurLetP = Let;
      }
      List->PrefixTree[cl].Right[Let] = i;
    } else {
      if (CurLangS == NULL || strcmp(CurLangS, Affix->lang) != 0) {
	cl = -1;
	for (j = 0; j < SL->nLang; j++) {
	  if (strcmp(SL->SpellTree[j].lang, Affix->lang) == 0) {
	    cl = j;
	    break;
	  }
	}
	CurLangS = Affix->lang;
	dps_strcpy(List->SuffixTree[cl].lang, CurLangS);
	CurLetS = -1;
      }
      if (cl < 0) continue; /* we have affixes without spell for this lang */
      Let = (Affix->replen) ? (int)(Affix->repl[Affix->replen-1]) & 255 : 0;
      if (CurLetS != Let) {
	List->SuffixTree[cl].Left[Let] = i;
	CurLetS = Let;
      }
      List->SuffixTree[cl].Right[Let] = i;
    }
  }
  List->sorted = 1;
}


void DpsSortQuffixes(DPS_QUFFIXLIST *List, DPS_SPELLLIST *SL) {
  int  CurLetP = -1, Let, cl = -1;
  char *CurLangP = NULL;
  DPS_QUFFIX *Qreg; size_t i, j;

  if (List->sorted) return;
  if (List->nrecs > 1)
    DpsSort((void*)List->Quffix, List->nrecs, sizeof(DPS_QUFFIX), cmpquffix);

  for(i = 0; i < SL->nLang; i++)
    for(j = 0; j < 256; j++) {
      List->PrefixTree[i].Left[j] = List->PrefixTree[i].Right[j] = -1;
      List->SuffixTree[i].Left[j] = List->SuffixTree[i].Right[j] = -1;
    }

  for(i = 0; i < List->nrecs; i++) {
    Qreg = &(((DPS_QUFFIX*)List->Quffix)[i]);
    if (CurLangP == NULL || strcmp(CurLangP, Qreg->lang) != 0) {
      cl = -1;
      for (j = 0; j < SL->nLang; j++) {
	if (strncmp(SL->SpellTree[j].lang, Qreg->lang, 2) == 0) {
	  cl = j;
	  break;
	}
      }
      CurLangP = Qreg->lang;
      dps_strcpy(List->PrefixTree[cl].lang, CurLangP);
      CurLetP = -1;
    }
    if (cl < 0) continue; /* we have affixes without spell for this lang */
    Let = (int)(*(Qreg->repl)) & 255;
    if (CurLetP != Let) {
      List->PrefixTree[cl].Left[Let] = i;
      CurLetP = Let;
    }
    List->PrefixTree[cl].Right[Let] = i;
  }
  List->sorted = 1;
}


static void CheckSuffix(const dpsunicode_t *word, size_t len, DPS_AFFIX *Affix, int *res, DPS_AGENT *Indexer, 
			DPS_PSPELL *PS, DPS_PSPELL * FZ) {
  dpsunicode_t newword[2*MAXNORMLEN] = {0};
  int err;
/*3.1  int curlang, curspellang;*/
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif
  
  *res = DpsUniStrBNCmp(word, Affix->repl, Affix->replen);
  if (*res < 0) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return;
  }
  if (*res > 0) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return;
  }
  DpsUniStrCpy(newword, word);
  DpsUniStrCpy(newword+len-Affix->replen, Affix->find);

  if (Affix->compile) {
    err = DpsUniRegComp(&(Affix->reg), Affix->mask);
    if(err){
      DpsUniRegFree(&(Affix->reg));
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return;
    }
    Affix->compile = 0;
  }
  if((err=DpsUniRegExec(&(Affix->reg),newword))){
    DPS_SPELL **curspell;

    if((curspell = DpsFindWord(Indexer, newword, Affix->flag, PS, FZ))) {

/*3.1 FIXME: language statistics collection while normalizing       
      curlang = Indexer->curlang;
      curspellang = Indexer->spellang;
      DpsSelectLang(Indexer, curspell->lang);
      Indexer->lang[Indexer->curlang].count++;
      Indexer->curlang = curlang;
      Indexer->spellang = curspellang;
*/
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return;
    }
  }
#ifdef WITH_PARANOIA
  DpsViolationExit(-1, paran);
#endif
  return;
}


static int CheckPrefix(const dpsunicode_t *word, DPS_AFFIX *Affix, DPS_AGENT *Indexer, int li, int pi, DPS_PSPELL *PS, DPS_PSPELL *FZ) {
  dpsunicode_t newword[2*MAXNORMLEN] = {0};
  int err, ls, rs, res;
  /*  int lres,rres;*/
  size_t newlen;
  DPS_AFFIX *CAffix = Indexer->Conf->Affixes.Affix;
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif
  
  res = DpsUniStrNCaseCmp(word, Affix->repl, Affix->replen);
  if (res != 0) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return res;
  }
  DpsUniStrCpy(newword, Affix->find);
  DpsUniStrCat(newword, word+Affix->replen);

  if (Affix->compile) {
    err = DpsUniRegComp(&(Affix->reg),Affix->mask);
    if(err){
      DpsUniRegFree(&(Affix->reg));
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return (0);
    }
    Affix->compile = 0;
  }
  if((err=DpsUniRegExec(&(Affix->reg),newword))){
    DPS_SPELL **curspell;

    if((curspell = DpsFindWord(Indexer, newword, Affix->flag, PS, FZ))) {
    } 
    newlen = DpsUniLen(newword);
    ls = Indexer->Conf->Affixes.SuffixTree[li].Left[pi];
    if (ls >= 0) {
      for (rs = Indexer->Conf->Affixes.SuffixTree[li].Right[pi]; ls <= rs; ls++) {
	CheckSuffix(newword, newlen, &CAffix[ls], &res, Indexer, PS, FZ);
      }
    }
/*
    while (ls >= 0 && ls <= rs) {
      CheckSuffix(newword, newlen, &CAffix[ls], &lres, Indexer, PS, FZ);
      if (rs > ls) {
	CheckSuffix(newword, newlen, &CAffix[rs], &rres, Indexer, PS, FZ);
      }
      ls++;
      rs--;
    }
*/
  }
#ifdef WITH_PARANOIA
  DpsViolationExit(-1, paran);
#endif
  return 0;
}


__C_LINK DPS_SPELL ** __DPSCALL DpsNormalizeWord(DPS_AGENT * Indexer, DPS_WIDEWORD *wword, DPS_PSPELL *FZ) {
	size_t len;
	DPS_SPELL **forms;
	DPS_SPELL **cur;
	DPS_AFFIX * Affix;
	int ri, pi, ipi, lp, rp, cp, ls, rs, nlang = /*Indexer->SpellLang*/ -1 /*Indexer->spellang  FIXME: search form limit by lang  */ ;
	int li, li_from, li_to, lres, rres, cres = 0;
	dpsunicode_t *uword = wword->uword;
	DPS_PSPELL PS;

	len=DpsUniLen(uword);
	if (len < Indexer->WordParam.min_word_len 
		|| len > MAXNORMLEN
		|| len > Indexer->WordParam.max_word_len
		)
		return(NULL);
	
	PS.nspell = 0;
	forms = (DPS_SPELL **) DpsXmalloc(MAX_NORM*sizeof(DPS_SPELL *));
	if (forms == NULL) return NULL;
	PS.cur = cur = forms; *cur=NULL;

	ri = (int)((*uword) & 255);
	pi = (int)((uword[DpsUniLen(uword)-1]) & 255);
	if (nlang == -1) {
	  li_from = 0; li_to = Indexer->Conf->Spells.nLang;
	} else {
	  li_from  = nlang;
	  li_to = nlang + 1;
	}
	Affix = (DPS_AFFIX*)Indexer->Conf->Affixes.Affix;
	
	/* Check that the word itself is normal form */
	DpsFindWord(Indexer, uword, 0, &PS, FZ);

	/* Find all other NORMAL forms of the 'word' */
	
	for (ipi = 0; ipi <= pi; ipi += pi ? pi : 1) {

	  for (li = li_from; li < li_to; li++) {
	    /* check prefix */
	    lp = Indexer->Conf->Affixes.PrefixTree[li].Left[ri];
	    rp = Indexer->Conf->Affixes.PrefixTree[li].Right[ri];
	    while (lp >= 0 && lp <= rp) {
	      cp = (lp + rp) >> 1;
	      cres = 0;
	      if (PS.nspell < (MAX_NORM-1)) {
		cres = CheckPrefix(uword, &Affix[cp], Indexer, li, ipi, &PS, FZ);
	      }
	      if ((lp < cp) && ((cur - forms) < (MAX_NORM-1)) ) {
		lres = CheckPrefix(uword, &Affix[lp], Indexer, li, ipi, &PS, FZ);
	      }
	      if ( (rp > cp) && ((cur - forms) < (MAX_NORM-1)) ) {
		rres = CheckPrefix(uword, &Affix[rp], Indexer, li, ipi, &PS, FZ);
	      }
	      if (cres < 0) {
		rp = cp - 1;
		lp++;
	      } else if (cres > 0) {
		lp = cp + 1;
		rp--;
	      } else {
		lp++;
		rp--;
	      }
	    }

	    /* check suffix */
	    ls = Indexer->Conf->Affixes.SuffixTree[li].Left[ipi];
	    rs = Indexer->Conf->Affixes.SuffixTree[li].Right[ipi];
	    while (ls >= 0 && ls <= rs) {
	      CheckSuffix(uword, len, &Affix[ls], &lres, Indexer, &PS, FZ);
	      if ( rs > ls ) {
		CheckSuffix(uword, len, &Affix[rs], &rres, Indexer, &PS, FZ);
	      }
	      ls++;
	      rs--;
	    } /* end while */
	  
	  } /* for li */
	} /* for ipi */

	if(PS.nspell == 0) {
		DPS_FREE(forms);
		return NULL;
	}

	return forms;
}



void DpsSpellListFree(DPS_SPELLLIST *List){
	size_t i;

	for ( i = 0; i < List->nspell; i++) {
		DPS_FREE(List->Spell[i].word);
	}
	DPS_FREE(List->Spell);
	List->nspell = 0;
}

void DpsAffixListFree (DPS_AFFIXLIST *List) {
	size_t i;

	for (i = 0; i < List->naffixes; i++)
		if (List->Affix[i].compile == 0)
			DpsUniRegFree(&(List->Affix[i].reg));

	DPS_FREE(List->Affix);
	List->naffixes = 0;
}

void DpsQuffixListFree (DPS_QUFFIXLIST *List) {
	size_t i;

	for (i = 0; i < List->nrecs; i++)
		if (List->Quffix[i].compile == 0)
			DpsUniRegFree(&(List->Quffix[i].reg));

	DPS_FREE(List->Quffix);
	List->nrecs = 0;
}


static void DpsAllFormsWord (DPS_AGENT *Indexer, DPS_SPELL *word, DPS_WIDEWORDLIST *result, size_t order, size_t order_inquery) {
  DPS_WIDEWORD w;
  size_t i;
  size_t naffixes = Indexer->Conf->Affixes.naffixes;
  DPS_AFFIX *Affix = (DPS_AFFIX *)Indexer->Conf->Affixes.Affix;
  int err;
  DPS_CHARSET *local_charset;
  DPS_CHARSET *sys_int;
  DPS_CONV fromuni;
  dpsunicode_t *r_word;
  
  local_charset = Indexer->Conf->lcs;
  if (local_charset == NULL) return;
  if (NULL==(sys_int=DpsGetCharSet("sys-int"))) return;
  DpsConvInit(&fromuni, sys_int, local_charset, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);

#ifdef DEBUG_UNIREG
  printf("start AllFormsWord\n");
#endif
  
  w.word = NULL;
  w.uword = NULL;

  r_word = DpsUniRDup(word->word);

  for (i = 0; i < naffixes; i++) {
    if ( (word->flag != NULL)
	 && (strcmp(word->lang, Affix[i].lang) == 0 )
	 && (strstr(word->flag, Affix[i].flag) != NULL)
	 ) {
      if (Affix[i].compile) {
	err = DpsUniRegComp(&(Affix[i].reg), Affix[i].mask);
	if(err){
	  DpsUniRegFree(&(Affix[i].reg));
	  DPS_FREE(r_word);
	  return;
	}
	Affix[i].compile = 0;
      }

      err = DpsUniRegExec(&(Affix[i].reg), r_word);
      if ( err
	   && (err = (Affix[i].type == 'p') ? (DpsUniStrNCaseCmp(r_word, Affix[i].find, Affix[i].findlen) == 0) :
	       (DpsUniStrBNCmp(r_word, Affix[i].find, Affix[i].findlen) == 0)
	       )
	   )  {
	
	w.len = DpsUniLen(r_word) - Affix[i].findlen + Affix[i].replen;
	if ( ( (w.word = DpsRealloc(w.word, 14 * w.len + 1)) == NULL) ||
	     ( (w.uword = DpsRealloc(w.uword, (w.len + 1) * sizeof(dpsunicode_t))) == NULL)) {
	  DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(r_word);
	  return;
	}

	bzero((void*)w.uword, (w.len + 1) * sizeof(dpsunicode_t));

	if (Affix[i].type == 'p') {
	  DpsUniStrCpy(w.uword, Affix[i].repl);
	  DpsUniStrCat(w.uword, &(r_word[Affix[i].findlen]));
	} else {
	  DpsUniStrNCpy(w.uword, r_word, DpsUniLen(r_word) - Affix[i].findlen);
	  DpsUniStrCat(w.uword, Affix[i].repl);
	}
	  
	DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(dpsunicode_t) * (w.len + 1));
	w.crcword = DpsStrHash32(w.word);
	w.order = order;
	w.order_inquery = order_inquery;
	w.count = 0;
	w.origin = DPS_WORD_ORIGIN_SPELL;
	DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE);
      }

    }
  }
  DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(r_word);
}


static void DpsQuffixWord(DPS_AGENT *Indexer, DPS_WIDEWORDLIST *result, DPS_SPELL *norm, DPS_WIDEWORD *wword) {
  DPS_WIDEWORD w;
  size_t i, j, nrecs = Indexer->Conf->Quffixes.nrecs, len;
  DPS_QUFFIX *Quffix = (DPS_QUFFIX*)Indexer->Conf->Quffixes.Quffix;
  int err;
  DPS_CHARSET *local_charset;
  DPS_CHARSET *sys_int;
  DPS_CONV fromuni;
  DPS_PSPELL PS;
  
  local_charset = Indexer->Conf->lcs;
  if (local_charset == NULL) return;
  if (NULL==(sys_int=DpsGetCharSet("sys-int"))) return;
  if ((PS.cur = (DPS_SPELL **) DpsXmalloc(MAX_NORM*sizeof(DPS_SPELL *))) == NULL) return;
  PS.nspell = 0;
  DpsConvInit(&fromuni, sys_int, local_charset, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);

  w.word = NULL;
  w.uword = NULL;

  len = DpsUniLen(wword->uword);

  for (i = 0; i < nrecs; i++) {
    if ( ((norm->flag != NULL) && (strcmp(norm->lang, Quffix[i].lang) == 0 ) && (strstr(norm->flag, Quffix[i].flag) != NULL))
	 || ((norm->flag == NULL) && (strcmp(norm->lang, Quffix[i].lang) == 0 ) && (strchr(Quffix[i].flag, (int)'.') != NULL))
	 ) {
      if (Quffix[i].compile) {
	err = DpsUniRegComp(&(Quffix[i].reg), Quffix[i].mask);
	if(err){
	  DpsUniRegFree(&(Quffix[i].reg));
	  return;
	}
	Quffix[i].compile = 0;
      }
      err = DpsUniRegExec(&(Quffix[i].reg), wword->uword);
      if (err) {
	w.len = len - Quffix[i].findlen + Quffix[i].replen;
	if ( ( (w.word = DpsRealloc(w.word, 14 * w.len + 1)) == NULL) ||
	     ( (w.uword = DpsRealloc(w.uword, (w.len + 1) * sizeof(dpsunicode_t))) == NULL)) {
	  DPS_FREE(w.word); DPS_FREE(w.uword);
	  return;
	}
	  
	bzero((void*)w.uword, (w.len + 1) * sizeof(dpsunicode_t));

	DpsUniStrNCpy(w.uword, wword->uword, len - Quffix[i].findlen);
	DpsUniStrCat(w.uword, Quffix[i].repl);

	DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(dpsunicode_t) * (w.len + 1));
	w.crcword = DpsStrHash32(w.word);
	w.order = wword->order;
	w.order_inquery = wword->order_inquery;
	w.count = 0;
	w.origin = DPS_WORD_ORIGIN_SPELL;
	DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE);

	PS.nspell = 0;
	DpsFindWord(Indexer, w.uword, 0, &PS, NULL);
	for (j = 0; PS.cur[j] != NULL; j++) DpsAllFormsWord(Indexer, PS.cur[j], result, wword->order, wword->order_inquery);
      }
    }
  }
  DPS_FREE(PS.cur);
  DPS_FREE(w.word); DPS_FREE(w.uword);
}


__C_LINK DPS_WIDEWORDLIST * __DPSCALL DpsAllForms (DPS_AGENT *Indexer, DPS_WIDEWORD *wword) {
  DPS_SPELL **norm, **cur;
  DPS_WIDEWORDLIST *result, *syn = NULL;
  DPS_WIDEWORD w;
  size_t i, j;
  DPS_CHARSET *local_charset;
  DPS_CHARSET *sys_int;
  DPS_CONV fromuni;
  int sy   = DpsVarListFindInt(&Indexer->Vars, "sy", 1);
  int sp   = DpsVarListFindInt(&Indexer->Vars, "sp", 1);
  DPS_PSPELL PS, FZ;
  DPS_SPELL s_p, *p_sp = &s_p;
  
  PS.cur = NULL;

  FZ.nspell = 0;
  FZ.cur = &p_sp;
  s_p.word = NULL;

  if ((wword->ulen < Indexer->WordParam.min_word_len) || (wword->ulen == 1)) return NULL;
  local_charset = Indexer->Conf->lcs;
  if (local_charset == NULL) return NULL;
  if (NULL==(sys_int=DpsGetCharSet("sys-int"))) return NULL;
  DpsConvInit(&fromuni, sys_int, local_charset, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);

  if ((result = DpsXmalloc(sizeof(DPS_WIDEWORDLIST))) == NULL) {
    return NULL;
  }
  w.word = NULL;
  w.uword = NULL;

  if ((PS.cur = (DPS_SPELL **) DpsXmalloc(MAX_NORM*sizeof(DPS_SPELL *))) == NULL) return NULL;
  PS.nspell = 0;
  DpsWideWordListInit(result);

/*  mprotect(&(FZ.cur), 4096, PROT_READ);*/

  cur = norm = DpsNormalizeWord(Indexer, wword, &FZ);

  if (cur != NULL) {
    while (*cur != NULL) {

      w.len = DpsUniLen((*cur)->word);
      if ( ( (w.word = DpsRealloc(w.word, 14 * w.len + 1)) == NULL) ||
	   ( (w.uword = DpsRealloc(w.uword, (3 * w.len + 1) * sizeof(dpsunicode_t))) == NULL)) {
	
/*	mprotect(&FZ.cur, 4096, PROT_READ | PROT_WRITE);*/

	DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(s_p.word);
	return NULL;
      }
      DpsUniStrRCpy(w.uword,(*cur)->word); 
      DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
      w.crcword = DpsStrHash32(w.word);
      w.order = wword->order;
      w.order_inquery = wword->order_inquery;
      w.count = 0;
      w.origin = wword->origin | DPS_WORD_ORIGIN_SPELL;
      if (sp) { DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE); }

      if (sy) syn = DpsSynonymListFind(&(Indexer->Conf->Synonyms), &w);

      if (syn != NULL) {
	DPS_WIDEWORD sw;
	bzero(&sw, sizeof(sw));
	for(i = 0; i < syn->nwords; i++) {
	  sw = syn->Word[i];
	  sw.order = wword->order;
	  sw.order_inquery = wword->order_inquery;
	  sw.count = 0;
	  sw.origin = wword->origin | DPS_WORD_ORIGIN_SYNONYM | DPS_WORD_ORIGIN_SPELL;
	  DpsWideWordListAdd(result, &sw, DPS_WWL_LOOSE);
	}
      }

      if (sp) { DpsAllFormsWord(Indexer, *cur, result, wword->order, wword->order_inquery); 
	if (wword->origin & DPS_WORD_ORIGIN_QUERY) DpsQuffixWord(Indexer, result, *cur, wword);
      }
      if (syn != NULL) {
	for(i = 0; i < syn->nwords; i++) {
	  PS.nspell = 0;
	  DpsFindWord(Indexer, syn->Word[i].uword, 0, &PS, NULL);
	  for (j = 0; PS.cur[j] != NULL; j++) 
	    DpsAllFormsWord(Indexer, PS.cur[j], result, wword->order, wword->order_inquery);
	}
      }
      if (Indexer->Flags.use_accentext) {
	dpsunicode_t *aw = DpsUniAccentStrip((*cur)->word);
	DPS_SPELL asp = **cur;
	size_t zz;
	for (zz = 0; zz < 2; zz++) {
	  if (DpsUniStrCmp(aw, (*cur)->word)) {
	    DpsUniStrRCpy(w.uword, aw); 
	    DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
	    w.crcword = DpsStrHash32(w.word);
	    w.origin = wword->origin | DPS_WORD_ORIGIN_ACCENT;
	    w.order = wword->order;
	    w.order_inquery = wword->order_inquery;
	    DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE);
	    asp.word = aw;
	    if (sp) { DpsAllFormsWord(Indexer, &asp, result, wword->order, wword->order_inquery);
	      if (wword->origin & DPS_WORD_ORIGIN_QUERY) DpsQuffixWord(Indexer, result, &asp, wword);
	    }
	  }
	  DPS_FREE(aw);
	  if (zz == 0) {
	    aw = DpsUniGermanReplace((*cur)->word);
	  }
	}
	DPS_FREE(aw);
      }
      cur++;
    }
  } else if (/*FZ.nspell > 0 && */FZ.nspell > Indexer->WordParam.min_word_len) {

      w.len = DpsUniLen(s_p.word);
      if ( ( (w.word = DpsRealloc(w.word, 14 * w.len + 1)) == NULL) ||
	   ( (w.uword = DpsRealloc(w.uword, (3 * w.len + 1) * sizeof(dpsunicode_t))) == NULL)) {
	
/*	mprotect(&FZ.cur, 4096, PROT_READ | PROT_WRITE);*/

	DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(s_p.word);
	return NULL;
      }
      DpsUniStrRCpy(w.uword, s_p.word); 
      DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
      w.crcword = DpsStrHash32(w.word);
      w.order = wword->order;
      w.order_inquery = wword->order_inquery;
      w.count = 0;
      w.origin = wword->origin | DPS_WORD_ORIGIN_SPELL;
      if (sp) { 
	DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE); 
      }

      if (sy) syn = DpsSynonymListFind(&(Indexer->Conf->Synonyms), &w);

      if (syn != NULL) {
	DPS_WIDEWORD sw;
	bzero(&sw, sizeof(sw));
	for(i = 0; i < syn->nwords; i++) {
	  sw = syn->Word[i];
	  sw.order = wword->order;
	  sw.order_inquery = wword->order_inquery;
	  sw.count = 0;
	  sw.origin = wword->origin | DPS_WORD_ORIGIN_SYNONYM | DPS_WORD_ORIGIN_SPELL;
	  DpsWideWordListAdd(result, &sw, DPS_WWL_LOOSE);
	}
      }
    
      if (sp) { DpsAllFormsWord(Indexer, p_sp, result, wword->order, wword->order_inquery); 
	if (wword->origin & DPS_WORD_ORIGIN_QUERY) DpsQuffixWord(Indexer, result, p_sp, wword);
      }
      if (syn != NULL) {
	for(i = 0; i < syn->nwords; i++) {
	  PS.nspell = 0;
	  DpsFindWord(Indexer, syn->Word[i].uword, 0, &PS, NULL);
	  for (j = 0; PS.cur[j] != NULL; j++) 
	    DpsAllFormsWord(Indexer, PS.cur[j], result, wword->order, wword->order_inquery);
	}
      }
      if (Indexer->Flags.use_accentext) {
	dpsunicode_t *aw = DpsUniAccentStrip(p_sp->word);
	DPS_SPELL asp = *p_sp;
	size_t zz;
	for (zz = 0; zz < 2; zz++) {
	  if (DpsUniStrCmp(aw, p_sp->word)) {
	    DpsUniStrRCpy(w.uword, aw); 
	    DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
	    w.crcword = DpsStrHash32(w.word);
	    w.origin = wword->origin | DPS_WORD_ORIGIN_ACCENT; 
	    w.order = wword->order;
	    w.order_inquery = wword->order_inquery;
	    w.count = 0;
	    DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE);
	    asp.word = aw;
	    if (sp) { DpsAllFormsWord(Indexer, &asp, result, wword->order, wword->order_inquery);
	      if (wword->origin & DPS_WORD_ORIGIN_QUERY) DpsQuffixWord(Indexer, result, p_sp, wword);
	    }
	  }
	  DPS_FREE(aw);
	  if (zz == 0) {
	    aw = DpsUniGermanReplace(p_sp->word);
	  }
	}
	DPS_FREE(aw);
      }
  } else {

    /*DpsWideWordListAdd(result, wword);*/
    if (sy) syn = DpsSynonymListFind(&(Indexer->Conf->Synonyms), wword);

    if (syn != NULL) {
      DPS_WIDEWORD sw;
      bzero(&sw, sizeof(sw));
      for(i = 0; i < syn->nwords; i++) {
	sw = syn->Word[i];
	sw.order = wword->order;
	sw.order_inquery = wword->order_inquery;
	sw.count = 0;
	sw.origin = wword->origin | DPS_WORD_ORIGIN_SYNONYM;
	DpsWideWordListAdd(result, &sw, DPS_WWL_LOOSE);
      }
    
      for(i = 0; i < syn->nwords; i++) {
	PS.nspell = 0;
	DpsFindWord(Indexer, syn->Word[i].uword, 0, &PS, NULL);
	for (j = 0; PS.cur[j] != NULL; j++) 
	  DpsAllFormsWord(Indexer, PS.cur[j], result, wword->order, wword->order_inquery);
      }
    }
  }
	
/*  mprotect(&FZ.cur, 4096, PROT_READ | PROT_WRITE);*/

  DPS_FREE(w.word); DPS_FREE(w.uword);
  DPS_FREE(norm);
  DPS_FREE(PS.cur);
  DPS_FREE(s_p.word);

  return result;
}
