/* Copyright (C) 2013-2014 Maxim Zakharov. All rights reserved.
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
#include "dps_textlist.h"
#include "dps_parsehtml.h"
#include "dps_utils.h"
#include "dps_charsetutils.h"
#include "dps_url.h"
#include "dps_match.h"
#include "dps_log.h"
#include "dps_xmalloc.h"
#include "dps_server.h"
#include "dps_hrefs.h"
#include "dps_word.h"
#include "dps_crossword.h"
#include "dps_spell.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_uniconv.h"
#include "dps_sgml.h"
#include "dps_guesser.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_searchtool.h"
#include "dps_sea.h"
#include "dps_hash.h"
#include "dps_utils.h"
#include "dps_indexer.h"
#include "dps_conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <ctype.h>

#define TAG_WITH_CROSSATTRIBUTE (!strcmp(name, "img") || !strcmp(name, "a") || !strcmp(name, "link") )


/****************************************************************/

static void DpsUniDesegment(dpsunicode_t *s) {
  register dpsunicode_t *d = s;
  while(*s != 0) {
    switch(*s) {
    case 0x0008:
    case 0x000A:
    case 0x000D:
    case 0x0020:
    case 0x00A0:
    case 0x1680:
    case 0x202F:
    case 0x2420:
    case 0x3000:
    case 0x303F:
    case 0xFeFF: break;
    default: if ((*s >= 0x2000) && (*s <= 0x200B)) break;
      *d++ = *s;
    }
    s++;
  }
  *d = *s;
}

#ifdef HAVE_ASPELL
static void DpsSpellSuggest(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_TEXTITEM *Item,
			    dpsunicode_t *uword, size_t uwlen, int crossec,
			    AspellSpeller *speller, DPS_DSTR *suggest,
			    int *spelling
) {
    DPS_WORD Word;
    register int ii;
    char          *utf_str = NULL, *asug = NULL;
    dpsunicode_t  *aword = NULL;
    const AspellWordList *suggestions;
    AspellStringEnumeration *elements;
    size_t tlen;
    static dpsunicode_t COLON[] = { ':', ' ', 0};
    static dpsunicode_t COMMA[] = { ',', ' ', 0};
    static dpsunicode_t PERIOD[] = { '.', 0};
    int res = DPS_OK;

    TRACE_IN(Indexer, "DpsSpellSuggest");

    ii = 1;
    for (aword = uword; *aword; aword++) {
	if (DpsUniCType(*aword) <= DPS_UNI_BUKVA_FORTE) {
	    ii = 0; break;
	}
    }
    if (ii) {
	TRACE_OUT(Indexer);
	return; 
    }

    if ((utf_str = (char*)DpsRealloc(utf_str, 16 * uwlen + 1)) == NULL) {
	TRACE_OUT(Indexer);
	return; 
    }
    if ((aword = (dpsunicode_t*)DpsMalloc((2 * uwlen + 1) * sizeof(dpsunicode_t))) == NULL) {
	DPS_FREE(utf_str); TRACE_OUT(Indexer);
	return; 
    }
    DpsUniStrCpy(aword, uword);
    DpsUniAspellSimplify(aword);
    DpsConv(&Indexer->uni_utf, utf_str, 16 * uwlen, (char*)aword, (int)(sizeof(dpsunicode_t) * (uwlen + 1)));
    DPS_GETLOCK(Indexer, DPS_LOCK_ASPELL);
    ii = aspell_speller_check(speller, (const char *)utf_str, (int)(tlen = dps_strlen(utf_str)));
    if ( ii == 0) {
	if (aspell_speller_error(speller) != 0) {
	    DpsLog(Indexer, DPS_LOG_DEBUG, "aspell error: %s\n", aspell_speller_error_message(speller));
	}
	suggestions = aspell_speller_suggest(speller, (const char *)utf_str, (int)tlen);
	elements = aspell_word_list_elements(suggestions);
	for (ii = 0; 
	     (ii < 2) && ((asug = (char*)aspell_string_enumeration_next(elements)) != NULL);
	     ii++ ) { 

	    TRACE_LINE(Indexer);
	    DpsConv(&Indexer->utf_uni, (char*)aword, (2 * uwlen + 1) * sizeof(*aword), (char*)(asug), sizeof(asug[0])*(tlen + 1));
      
	    Word.uword = aword;
	    Word.ulen = DpsUniLen(aword);

	    res = DpsWordListAddFantom(Doc, &Word, Item->section);
	    if (res != DPS_OK) break;
	    *spelling = 1;
	    if(Item->href != NULL && crossec != 0) {
		DPS_CROSSWORD cw;
		cw.url = Item->href;
		cw.weight = crossec;
		cw.pos = Doc->CrossWords.wordpos;
		cw.uword = aword;
		cw.ulen = Word.ulen;
		DpsCrossListAddFantom(Doc, &cw);
	    }
	    if (suggest != NULL) {
		if (ii == 0) {
		    DpsDSTRAppendUniWithSpace(suggest, uword);
		    DpsDSTRAppendUniStr(suggest, COLON);
		} else {
		    DpsDSTRAppendUniStr(suggest, COMMA);
		}
		DpsDSTRAppendUniStr(suggest, aword);
	    }
	}
	if (ii > 0 && suggest != NULL) DpsDSTRAppendUniStr(suggest, PERIOD);
	delete_aspell_string_enumeration(elements);
    }
    DPS_RELEASELOCK(Indexer, DPS_LOCK_ASPELL);
    DPS_FREE(utf_str); DPS_FREE(aword);
    TRACE_OUT(Indexer);
    return;
}
#endif

static void DpsProcessFantoms(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_TEXTITEM *Item, size_t min_word_len, int crossec, int have_bukva_forte, 
			      dpsunicode_t *uword, int make_prefixes, int make_suffixes, int strict
#ifdef HAVE_ASPELL
			      , int have_speller, AspellSpeller *speller, DPS_DSTR *suggest
#endif
			      ) {
  DPS_WORD Word;
  dpsunicode_t    *af_uword; /* Word in UNICODE with accents striped */
  dpsunicode_t    *de_uword; /* Word in UNICODE with german replaces */
  size_t  uwlen;
  int res = DPS_OK;
#ifdef HAVE_ASPELL
  int spelling = 0;
#endif

  TRACE_IN(Indexer, "DpsProcessFantoms");

  if (Indexer->Flags.use_accentext != 0) {
    af_uword = DpsUniAccentStrip(uword);
    if (DpsUniStrCmp(af_uword, uword) != 0) {

      Word.uword = af_uword;
      Word.ulen = (uwlen = DpsUniLen(af_uword));

      res = DpsWordListAddFantom(Doc, &Word, Item->section);
      if (res != DPS_OK) { 
	DPS_FREE(af_uword);
	TRACE_OUT(Indexer); return; 
      }
      if(Item->href != NULL && crossec != 0) {
	DPS_CROSSWORD cw;
	cw.url = Item->href;
	cw.weight = (short)(crossec & 255);
	cw.pos = Doc->CrossWords.wordpos;
	cw.uword = af_uword;
	cw.ulen = uwlen;
	DpsCrossListAddFantom(Doc, &cw);
      }
    }
    DPS_FREE(af_uword);
    de_uword = DpsUniGermanReplace(uword);
    if (DpsUniStrCmp(de_uword, uword) != 0) {

      Word.uword = de_uword;
      Word.ulen = DpsUniLen(de_uword);

      res = DpsWordListAddFantom(Doc, &Word, Item->section);
      if (res != DPS_OK) { 
	DPS_FREE(de_uword);
	TRACE_OUT(Indexer); return; 
      }
      if(Item->href != NULL && crossec != 0) {
	DPS_CROSSWORD cw;
	cw.url = Item->href;
	cw.weight = (short)(crossec & 255);
	cw.pos = Doc->CrossWords.wordpos;
	cw.uword = de_uword;
	cw.ulen = Word.ulen;
	DpsCrossListAddFantom(Doc, &cw);
      }
    }
    DPS_FREE(de_uword);
  }


#ifdef HAVE_ASPELL
  uwlen = DpsUniLen(uword);
  if (strict && 
      have_speller && have_bukva_forte && Indexer->Flags.use_aspellext && (uwlen > 2)
      && (DpsUniStrChr(uword, (dpsunicode_t) '&') == NULL) /* aspell trap workaround */
      ) DpsSpellSuggest(Indexer, Doc, Item, uword, uwlen, crossec, speller, suggest, &spelling);
#endif

  if (strict == 0) {
    dpsunicode_t *dword = DpsUniDup(uword);
    dpsunicode_t *nword = NULL;
    dpsunicode_t *lt, *tok;
    size_t	tlen, nlen = 0;
    int dw_have_bukva_forte, n;

    tok = DpsUniGetToken(dword, &lt, &dw_have_bukva_forte, !strict); 
    if (tok) {
      tlen = (size_t)(lt - tok);
      if (tlen + 1 > nlen || nword == NULL) {
	nword = (dpsunicode_t*)DpsRealloc(nword, (tlen + 1) * sizeof(dpsunicode_t));
	nlen = tlen;
      }
      dps_memcpy(nword, tok, tlen * sizeof(dpsunicode_t)); /* was: dps_memmove */
      nword[tlen] = 0;
      if (DpsUniStrCmp(uword, nword)) {
	for(n = 0 ;
	    tok ; 
	    tok = DpsUniGetToken(NULL, &lt, &dw_have_bukva_forte, !strict), n++ ) {

	  tlen = (size_t)(lt - tok);
	  if (tlen + 1 > nlen) {
	    nword = (dpsunicode_t*)DpsRealloc(nword, (tlen + 1) * sizeof(dpsunicode_t));
	    nlen = tlen;
	  }
	  dps_memcpy(nword, tok, tlen * sizeof(dpsunicode_t)); /* was: dps_memmove */
	  nword[tlen] = 0;

	  Word.uword = nword;
	  Word.ulen = DpsUniLen(nword);

	  res = DpsWordListAddFantom(Doc, &Word, Item->section);
	  if (res != DPS_OK) break;
	  if(Item->href && crossec){
	    DPS_CROSSWORD cw;
	    cw.url = Item->href;
	    cw.weight = (short)crossec;
	    cw.pos = Doc->CrossWords.wordpos;
	    cw.uword = nword;
	    cw.ulen = Word.ulen;
	    DpsCrossListAddFantom(Doc, &cw);
	  }

	  DpsProcessFantoms(Indexer, Doc, Item, min_word_len, crossec, dw_have_bukva_forte, nword, (n) ? Indexer->Flags.make_prefixes : 0, 
			    (n) ? Indexer->Flags.make_suffixes : 0, !strict
#ifdef HAVE_ASPELL
			    , have_speller, speller, suggest
#endif
			    );
	}
      }
#ifdef HAVE_ASPELL
      else if (have_speller) {
	  DpsSpellSuggest(Indexer, Doc, Item, uword, uwlen, crossec, speller, suggest, &spelling);
      }
#endif
    }
    DPS_FREE(dword); DPS_FREE(nword);
  }
  
  if (make_prefixes || make_suffixes) {
    size_t ul = DpsUniLen(uword), ml;

    if (make_suffixes && ul > min_word_len) {
      for (ml = ul - ((min_word_len) ? min_word_len : 1); ml > 0; ml--) {
	Word.uword = uword + ml;
	Word.ulen = ul - ml;
	res = DpsWordListAddFantom(Doc, &Word, Item->section);
	if (res != DPS_OK) break;
      }
    }

    if (make_prefixes) {
      Word.uword = uword;
      for (ml = ul - 1; ml >= min_word_len; ml--) {
	Word.uword[ml] = 0;
	Word.ulen = ml;
	res = DpsWordListAddFantom(Doc, &Word, Item->section);
	if (res != DPS_OK) break;
      }
    }
  }
#ifdef HAVE_ASPELL
  if (spelling) DpsVarListReplaceStr(&Doc->Sections, "spelling", "1");
#endif
  TRACE_OUT(Indexer);
}


int DpsPrepareItem(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_TEXTITEM *Item, dpsunicode_t *ustr, dpsunicode_t *UStr, 
		   const char *content_lang, size_t *indexed_size, size_t *indexed_limit, 
		   size_t max_word_len, size_t min_word_len, int crossec
#ifdef HAVE_ASPELL
		   , int have_speller, AspellSpeller *speller, DPS_DSTR *suggest
#endif
		   ) {
  dpsunicode_t	uspace[2] = {0x20, 0};
  DPS_VAR	*Sec;
  dpsunicode_t *nfc = NULL;
  dpsunicode_t *lt, *tok;
  dpsunicode_t    *uword = NULL;    /* Word in UNICODE      */
  char		*src;
  int have_bukva_forte, res = DPS_OK;
  size_t uwordlen = 0/*DPS_MAXWORDSIZE*/;
  size_t	srclen;
  size_t	dstlen;

  TRACE_IN(Indexer, "DpsPrepareItem");

  Sec = DpsVarListFind(&Doc->Sections, Item->section_name);
  if (Sec) {
    if (Sec->single && (Sec->val != NULL) && (Sec->curlen != 0)) { /* Already have single valued section, skip the rest from processing */
      TRACE_OUT(Indexer);
      return DPS_OK;
    }
  }
  DpsUniStrToLower(ustr);
  nfc = DpsUniNormalizeNFC(NULL, ustr);
  if (dps_need2segment(nfc)) {
    if      ((Indexer->Flags.Resegment & DPS_RESEGMENT_CHINESE)  && !strncasecmp(content_lang, "zh", 2)) DpsUniDesegment(nfc);
    else if ((Indexer->Flags.Resegment & DPS_RESEGMENT_JAPANESE) && !strncasecmp(content_lang, "ja", 2)) DpsUniDesegment(nfc);
    else if ((Indexer->Flags.Resegment & DPS_RESEGMENT_KOREAN)   && !strncasecmp(content_lang, "ko", 2)) DpsUniDesegment(nfc);
    else if ((Indexer->Flags.Resegment & DPS_RESEGMENT_THAI)     && !strncasecmp(content_lang, "th", 2)) DpsUniDesegment(nfc);
    lt = DpsUniSegment(Indexer, nfc, content_lang);
    DPS_FREE(nfc);
    nfc = lt;
  }

  TRACE_LINE(Indexer);
  if (nfc != NULL && Item->section && ((Indexer->Flags.LongestTextItems == 0) || (Item->marked)) )
    for(tok = DpsUniGetToken(nfc, &lt, &have_bukva_forte, Item->strict); 
	tok ; 
	tok = DpsUniGetToken(NULL, &lt, &have_bukva_forte, Item->strict) ) {

      size_t	tlen;				/* Word length          */ 
      DPS_WORD Word;
				
      tlen = (size_t)(lt-tok);
				
      if (tlen <= max_word_len && tlen >= min_word_len && (*indexed_limit == 0 || *indexed_size < *indexed_limit )) {

	*indexed_size += tlen;
				
	if (tlen > uwordlen || uword == NULL) {
	  uwordlen = tlen;
	  if ((uword = (dpsunicode_t*)DpsRealloc(uword, 2 * (uwordlen + 1) * sizeof(dpsunicode_t))) == NULL) { 
	    TRACE_OUT(Indexer);
	    return DPS_ERROR;
	  }
	}

	dps_memcpy(uword, tok, tlen * sizeof(dpsunicode_t)); /* was: dps_memmove */
	uword[tlen]=0;

	
	Word.uword = uword;
	Word.ulen = tlen;

	res = DpsWordListAdd(Doc, &Word, Item->section);
	if(res!=DPS_OK)break;

	if(Item->href && crossec){
	  DPS_CROSSWORD cw;
	  cw.url=Item->href;
	  cw.weight = (short)crossec;
	  cw.pos=Doc->CrossWords.wordpos;
	  cw.uword = uword;
	  cw.ulen = tlen;
	  DpsCrossListAdd(Doc, &cw);
	}

	DpsProcessFantoms(Indexer, Doc, Item, min_word_len, crossec, have_bukva_forte, uword, Indexer->Flags.make_prefixes, Indexer->Flags.make_suffixes, Item->strict
#ifdef HAVE_ASPELL
			  , have_speller, speller, suggest
#endif
			  );
			
      }
    }
  DPS_FREE(nfc);
  DPS_FREE(uword);

    if((Sec) && (strncasecmp(Item->section_name, "url.", 4) != 0) && (strcasecmp(Item->section_name, "url") != 0) ) {
      int cnvres;

      /* +4 to avoid attempts to fill the only one  */
      /* last byte with multibyte sequence          */
      /* as well as to add a space between portions */

      if(Sec->curlen < Sec->maxlen || Sec->maxlen == 0) {
				
	src = (char*)UStr;
	srclen = DpsUniLen(UStr) * sizeof(dpsunicode_t);
	if(Sec->val == NULL) {
	  dstlen = (Sec->maxlen) ? dps_min(Sec->maxlen, 24 * srclen) : 24 * srclen;
	  Sec->val = (char*)DpsMalloc( dstlen + 32 );
	  if (Sec->val == NULL) {
	    Sec->curlen = 0;
	    TRACE_OUT(Indexer);
	    return DPS_ERROR;
	  }
	  Sec->curlen = 0;
	} else {
	  if (Sec->single != 0) {
	    TRACE_OUT(Indexer);
	    return DPS_OK;
	  }
	  if (Sec->maxlen) dstlen = Sec->maxlen - Sec->curlen;
	  else dstlen = 24 * srclen;
	  if ((Sec->val = DpsRealloc(Sec->val, Sec->curlen + dstlen + 32)) == NULL) {
	      Sec->curlen = 0;
	      TRACE_OUT(Indexer);
	      return DPS_ERROR;
	  }
	  /* Add space */
	  DpsConv(&Indexer->uni_lc, Sec->val + Sec->curlen, 24, (char*)(uspace), sizeof(uspace));
	  Sec->curlen += Indexer->uni_lc.obytes;
	  Sec->val[Sec->curlen] = '\0';
	}
	cnvres = DpsConv(&Indexer->uni_lc, Sec->val + Sec->curlen, dstlen, src, srclen);
	Sec->curlen += Indexer->uni_lc.obytes;
	Sec->val[Sec->curlen] = '\0';
				
	if (cnvres < 0 && Sec->maxlen) {
	  Sec->curlen = 0 /*Sec->maxlen*/;
	}
      }
    }
    TRACE_OUT(Indexer);
    return DPS_OK;
}


int dps_itemptr_cmp(DPS_TEXTITEM **p1, DPS_TEXTITEM **p2) {
  if ((*p1)->len > (*p2)->len) return -1;
  if ((*p1)->len < (*p2)->len) return 1;
  return 0;
}


int DpsPrepareWords(DPS_AGENT * Indexer, DPS_DOCUMENT * Doc) {
  size_t		i;
  const char	*doccset;
  DPS_CHARSET	*doccs;
  DPS_CHARSET	*loccs;
  DPS_CHARSET	*sys_int;
  DPS_CONV	dc_uni;
  DPS_CONV      utf8_uni;
  DPS_TEXTLIST	*extractor_tlist = &Doc->ExtractorList;
  DPS_TEXTLIST	*tlist = &Doc->TextList;
  DPS_VAR	*Sec;
  int		res = DPS_OK;
  dpshash32_t	crc32 = 0;
  int		crossec, seasec, makesea;
  dpsunicode_t    *uword = NULL;    /* Word in UNICODE      */
  char            *lcsword = NULL;  /* Word in LocalCharset */
  size_t          max_word_len, min_word_len, uwordlen = DPS_MAXWORDSIZE;
  size_t          indexed_size = 0, indexed_limit = (size_t)DpsVarListFindInt(&Doc->Sections, "IndexDocSizeLimit", 0);
  const char      *content_lang = DpsVarListFindStr(&Doc->Sections, "Content-Language", "");
  const char      *SEASections = DpsVarListFindStr(&Indexer->Vars, "SEASections", "body");
  DPS_DSTR        exrpt;
#ifdef HAVE_ASPELL
  AspellCanHaveError *ret;
  AspellSpeller *speller = NULL;
  int have_speller = 0;
  DPS_DSTR suggest;
#endif
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif

  TRACE_IN(Indexer, "DpsPrepareWords");
  DpsLog(Indexer, DPS_LOG_DEBUG, "Preparing words");

  if (DpsDSTRInit(&exrpt, dps_max( 4096, Doc->Buf.size >> 2 )) == NULL) {
    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
    DpsViolationExit(Indexer->handle, paran);
#endif
    return DPS_ERROR;
  }

  if ((uword = (dpsunicode_t*)DpsMalloc((uwordlen + 1) * sizeof(dpsunicode_t))) == NULL) {
    DpsDSTRFree(&exrpt);
    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
    DpsViolationExit(Indexer->handle, paran);
#endif
    return DPS_ERROR;
  }
  if ((lcsword = (char*)DpsMalloc(12 * uwordlen + 1)) == NULL) { DPS_FREE(uword); 
    DpsDSTRFree(&exrpt);
    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
    DpsViolationExit(Indexer->handle, paran);
#endif
    return DPS_ERROR; 
  }

  Sec = DpsVarListFind(&Doc->Sections,"crosswords");
  crossec = Sec ? Sec->section : 0;
  Sec = DpsVarListFind(&Doc->Sections, "sea");
  seasec = Sec ? Sec->section : 0;
  makesea = Sec ? 1 : 0;
	
  doccset=DpsVarListFindStr(&Doc->Sections,"Charset",NULL);
  if(!doccset||!*doccset)doccset=DpsVarListFindStr(&Doc->Sections,"RemoteCharset","iso-8859-1");
  doccs=DpsGetCharSet(doccset);
  if(!doccs)doccs=DpsGetCharSet("iso-8859-1");
  loccs = Doc->lcs;
  if (!loccs) loccs = Indexer->Conf->lcs;
  if (!loccs) loccs = DpsGetCharSet("iso-8859-1");
  sys_int=DpsGetCharSet("sys-int");

  DpsConvInit(&dc_uni, loccs /*doccs*/, sys_int, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
  DpsConvInit(&utf8_uni, DpsGetCharSet("utf-8"), sys_int, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
  max_word_len = Indexer->WordParam.max_word_len;
  min_word_len = Indexer->WordParam.min_word_len;
	
#ifdef HAVE_ASPELL
  if (Indexer->Flags.use_aspellext) {
    AspellConfig * spell_config2;
    DPS_GETLOCK(Indexer, DPS_LOCK_ASPELL);
    spell_config2 = aspell_config_clone(Indexer->aspell_config);
    aspell_config_replace(spell_config2, "lang", (*content_lang != '\0') ? content_lang : "en");
    ret = new_aspell_speller(spell_config2);
    if (aspell_error(ret) != 0) {
      DpsLog(Indexer, DPS_LOG_ERROR, " aspell error: %s", aspell_error_message(ret));
      delete_aspell_can_have_error(ret);
    } else {
      speller = to_aspell_speller(ret);
      DpsDSTRInit(&suggest, 1024);
      have_speller = 1;
    }
    delete_aspell_config(spell_config2);
    DPS_RELEASELOCK(Indexer, DPS_LOCK_ASPELL);
  }
#endif				

  if (Indexer->Flags.LongestTextItems > 0) {
    DPS_TEXTITEM **items = (DPS_TEXTITEM**)DpsMalloc((tlist->nitems + 1) * sizeof(DPS_TEXTITEM*));
    if (items != NULL) {
      for(i = 0; i < tlist->nitems; i++) items[i] = &tlist->Items[i];
      DpsSort(items, tlist->nitems, sizeof(DPS_TEXTITEM*), (qsort_cmp) dps_itemptr_cmp);
      for(i = 0; (i < tlist->nitems) && (i < (size_t)Indexer->Flags.LongestTextItems); i++) items[i]->marked = 1;
    }
    DPS_FREE(items);
  }

/* Now convert everything to UNICODE format and calculate CRC32 */

  for(i = 0; i < tlist->nitems; i++) {
    size_t		srclen;
    size_t		dstlen;
    size_t		reslen;
    char		*src,*dst;
    dpsunicode_t	*ustr = NULL, *UStr = NULL;
    DPS_TEXTITEM	*Item = &tlist->Items[i];
		
		
    srclen = ((Item->len) ? Item->len : (dps_strlen(Item->str)) + 1);	/* with '\0' */
    dstlen = (16 * (srclen + 1)) * sizeof(dpsunicode_t);	/* with '\0' */
		
    if ((ustr = (dpsunicode_t*)DpsMalloc(dstlen + 1)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't alloc %u bytes", __FILE__, __LINE__, dstlen);
      DPS_FREE(uword); DPS_FREE(lcsword); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
		
    src = Item->str;
    dst = (char*)ustr;

    DpsConv(&dc_uni, dst, dstlen, src, srclen);
/*		DpsSGMLUniUnescape(ustr);*/
    DpsUniRemoveDoubleSpaces(ustr);
    if ((UStr = DpsUniDup(ustr)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't DpsUniDup", __FILE__, __LINE__);
      DPS_FREE(uword); DPS_FREE(lcsword); DPS_FREE(ustr); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
    reslen = DpsUniLen(ustr);
		
    /*
      TODO for clones detection:
      Replace any separators into space to ignore 
      various pseudo-graphics, commas, semicolons
      and so on to improve clone detection quality
    */
    if (strncasecmp(DPS_NULL2EMPTY(Item->section_name), "url", 3) != 0) /* do not calculate crc32  on url* sections */
      crc32 = DpsHash32Update(crc32, (char*)ustr, reslen * sizeof(dpsunicode_t));

    /* Collect links from HrefSections */
    if(Doc->Spider.follow != DPS_FOLLOW_NO &&
	(Sec = DpsVarListFind(&Indexer->Conf->HrefSections, Item->section_name))) {
      DPS_HREF	Href;
      DpsHrefInit(&Href);
      Href.referrer = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
      Href.hops = 1 + (dps_uint4)DpsVarListFindInt(&Doc->Sections, "Hops", 0);
      Href.site_id = 0; /*DpsVarListFindInt(&Doc->Sections, "Site_id", 0);*/
      Href.url = Item->str;
      Href.method = DPS_METHOD_GET;
      DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
    }


    if (makesea && strstr(SEASections, Item->section_name)) {
      DpsDSTRAppendUniWithSpace(&exrpt, UStr);
    }

    if (DPS_OK != DpsPrepareItem(Indexer, Doc, Item, ustr, UStr, 
				 content_lang, &indexed_size, &indexed_limit, max_word_len, min_word_len, crossec
#ifdef HAVE_ASPELL
				 , have_speller, speller, &suggest
#endif
				 )) {
      DPS_FREE(uword); DPS_FREE(lcsword); DPS_FREE(ustr); DPS_FREE(UStr); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
		
		
    DPS_FREE(ustr);
    DPS_FREE(UStr);
    if (res != DPS_OK) break;
  }

 /* do the same for libextrated meta-data, which is always encoded in UTF-8 */
  for(i = 0; i < extractor_tlist->nitems; i++) {
    size_t		srclen;
    size_t		dstlen;
    size_t		reslen;
    char		*src,*dst;
    dpsunicode_t	*ustr = NULL, *UStr = NULL;
    DPS_TEXTITEM	*Item = &extractor_tlist->Items[i];

    srclen = ((Item->len) ? Item->len : (dps_strlen(Item->str)) + 1);	/* with '\0' */
    dstlen = (16 * (srclen + 1)) * sizeof(dpsunicode_t);	/* with '\0' */
		
    if ((ustr = (dpsunicode_t*)DpsMalloc(dstlen + 1)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't alloc %u bytes", __FILE__, __LINE__, dstlen);
      DPS_FREE(uword); DPS_FREE(lcsword); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
		
    src = Item->str;
    dst = (char*)ustr;

    DpsConv(&utf8_uni, dst, dstlen, src, srclen);
    DpsUniRemoveDoubleSpaces(ustr);
    if ((UStr = DpsUniDup(ustr)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't DpsUniDup", __FILE__, __LINE__);
      DPS_FREE(uword); DPS_FREE(lcsword); DPS_FREE(ustr); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
    reslen = DpsUniLen(ustr);

    /*
      TODO for clones detection:
      Replace any separators into space to ignore 
      various pseudo-graphics, commas, semicolons
      and so on to improve clone detection quality
    */
    if (strncasecmp(DPS_NULL2EMPTY(Item->section_name), "url", 3) != 0) /* do not calculate crc32  on url* sections */
      crc32 = DpsHash32Update(crc32, (char*)ustr, reslen * sizeof(dpsunicode_t));

    /* Collect links from HrefSections */
    if(Doc->Spider.follow != DPS_FOLLOW_NO &&
       (Sec = DpsVarListFind(&Indexer->Conf->HrefSections, Item->section_name))) {
      DPS_HREF	Href;
      DpsHrefInit(&Href);
      Href.referrer = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
      Href.hops = 1 + (dps_uint4)DpsVarListFindInt(&Doc->Sections, "Hops", 0);
      Href.site_id = 0; /*DpsVarListFindInt(&Doc->Sections, "Site_id", 0);*/
      Href.url = Item->str;
      Href.method = DPS_METHOD_GET;
      DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
    }

    if (makesea && strstr(SEASections, Item->section_name)) {
      DpsDSTRAppendUniWithSpace(&exrpt, UStr);
    }

    if (DPS_OK != DpsPrepareItem(Indexer, Doc, Item, ustr, UStr, 
				 content_lang, &indexed_size, &indexed_limit, max_word_len, min_word_len, crossec
#ifdef HAVE_ASPELL
				 , have_speller, speller, &suggest
#endif
				 )) {
      DPS_FREE(uword); DPS_FREE(lcsword); DPS_FREE(ustr); DPS_FREE(UStr); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
		
		
    DPS_FREE(ustr);
    DPS_FREE(UStr);
    if (res != DPS_OK) break;
  }



  DpsVarListReplaceInt(&Doc->Sections,"crc32", (int)crc32);

  if (makesea) {
    DpsDSTRAppendUni(&exrpt, 0);
    DpsSEAMake(Indexer, Doc, &exrpt, content_lang, &indexed_size, &indexed_limit, max_word_len, min_word_len, crossec, seasec
#ifdef HAVE_ASPELL
	       , have_speller, speller
#endif
	       );
  }

#ifdef HAVE_ASPELL
  if (have_speller && Indexer->Flags.use_aspellext) {
    DPS_GETLOCK(Indexer, DPS_LOCK_ASPELL);
    delete_aspell_speller(speller);
    DPS_RELEASELOCK(Indexer, DPS_LOCK_ASPELL);
    if (suggest.data_size > 0) {
      char *spelling = DpsMalloc(24 * (suggest.data_size + 2));
      DpsDSTRAppendUni(&suggest, 0);
      if (spelling != NULL) {
	DpsConv(&Indexer->uni_lc, spelling, 24 * (suggest.data_size + 1), (char*)suggest.data, suggest.data_size);
	DpsVarListReplaceStr(&Doc->Sections, "spelling", spelling);
	DPS_FREE(spelling);
      }
    }
  }
  if (have_speller) DpsDSTRFree(&suggest); 
#endif
	
  DPS_FREE(uword); DPS_FREE(lcsword); 
  DpsDSTRFree(&exrpt);
  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
  DpsViolationExit(Indexer->handle, paran);
#endif
  return res;
}


/**************************** Built-in Parsers ***************************/

int DpsParseURLText(DPS_AGENT *A, DPS_DOCUMENT *Doc) {
	DPS_TEXTITEM	Item;
	DPS_URL         dcURL;
	DPS_CONV        lc_dc;
	DPS_VAR		*Sec;
	DPS_CHARSET	*doccs, *loccs;
	const char      *lc_url = DpsVarListFindStr(&Doc->Sections, "E_URL", NULL);
	char            *dc_url;
	size_t          len;


	if (lc_url == NULL) {
	  lc_url = DpsVarListFindStr(&Doc->Sections, "URL", "");
	}
	len = dps_strlen(lc_url);
	dc_url = (char*)DpsMalloc(24 * len + sizeof(dpsunicode_t));
	if (dc_url == NULL) return DPS_ERROR;

	loccs = A->Conf->lcs;
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");
	doccs = DpsGetCharSetByID(Doc->charset_id);
	if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
	DpsConvInit(&lc_dc, loccs, doccs, A->Conf->CharsToEscape, DPS_RECODE_URL_FROM);
	DpsConv(&lc_dc, dc_url, (size_t)24 * len,  lc_url, (size_t)(len + 1));

	dcURL.freeme = 0;
	DpsURLInit(&dcURL);
	if (DpsURLParse(&dcURL, dc_url)) { DPS_FREE(dc_url); return DPS_ERROR; }
	
	bzero((void*)&Item, sizeof(Item));
	Item.href = NULL;
	
	if((Sec = DpsVarListFind(&Doc->Sections, "url"))) {
		char sc[] = "url\0";
		Item.str = DPS_NULL2EMPTY(dc_url);
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.section_name = sc;
		Item.len = 0;
		(void)DpsTextListAdd(&Doc->TextList, &Item);
	}
	if((Sec=DpsVarListFind(&Doc->Sections,"url.proto"))) {
		char sc[] = "url.proto\0";
		Item.str = DPS_NULL2EMPTY(dcURL.schema);
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.section_name = sc;
		Item.len = 0;
		(void)DpsTextListAdd(&Doc->TextList, &Item);
	}
	DpsVarListReplaceStr(&Doc->Sections, "url.proto", DPS_NULL2EMPTY(dcURL.schema));
	if((Sec=DpsVarListFind(&Doc->Sections,"url.host"))) {
		char sc[] = "url.host\0";
		Item.str = DPS_NULL2EMPTY(dcURL.hostname);
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.section_name = sc;
		Item.len = 0;
		(void)DpsTextListAdd(&Doc->TextList, &Item);
	}
	DpsVarListReplaceStr(&Doc->Sections, "url.host", DPS_NULL2EMPTY(dcURL.hostname));
	if((Sec=DpsVarListFind(&Doc->Sections,"url.path"))) {
		char sc[] = "url.path\0";
		Item.str = DPS_NULL2EMPTY(dcURL.path);
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.section_name = sc;
		Item.len = 0;
		(void)DpsTextListAdd(&Doc->TextList, &Item);
	}
	DpsVarListReplaceStr(&Doc->Sections, "url.path", DPS_NULL2EMPTY(dcURL.path));
	if((Sec=DpsVarListFind(&Doc->Sections,"url.directory"))) {
		char sc[]="url.directory\0";
		Item.str = DPS_NULL2EMPTY(dcURL.directory);
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.section_name = sc;
		Item.len = 0;
		(void)DpsTextListAdd(&Doc->TextList, &Item);
	}
	DpsVarListReplaceStr(&Doc->Sections, "url.directory", DPS_NULL2EMPTY(dcURL.directory));
	{
	  char * str;
	  size_t flen;
	  str = (char*)DpsMalloc((flen = dps_strlen(DPS_NULL2EMPTY(dcURL.filename))) + 1);
	  if (str != NULL) {
	    DpsUnescapeCGIQuery(str, DPS_NULL2EMPTY(dcURL.filename));
	    if((Sec = DpsVarListFind(&Doc->Sections, "url.file"))) {
	        char sc[] = "url.file\0";
		Item.str = str;
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.section_name = sc;
		Item.len = flen;
		(void)DpsTextListAdd(&Doc->TextList, &Item);
	    }
	    DpsVarListReplaceStr(&Doc->Sections, "url.file", str);
	    DPS_FREE(str);
	  }
	}
	DpsURLFree(&dcURL);
	DPS_FREE(dc_url);
	return DPS_OK;
}
/*
int DpsParseHeaders(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc){
	size_t i;
	DPS_TEXTITEM Item;
	
	bzero((void*)&Item, sizeof(Item));
	Item.href=NULL;
	for(i=0;i<Doc->Sections.nvars;i++){
		char	secname[128];
		DPS_VAR	*Sec;
		dps_snprintf(secname,sizeof(secname),"header.%s",Doc->Sections.Var[i].name);
		secname[sizeof(secname)-1]='\0';
		if((Sec=DpsVarListFind(&Doc->Sections,secname))){
			Item.str=Doc->Sections.Var[i].val;
			Item.section = Sec->section;
			Item.strict = Sec->strict;
			Item.section_name=secname;
			(void)DpsTextListAdd(&Doc->TextList,&Item);
		}
	}
	return DPS_OK;
}
*/
int DpsParseText(DPS_AGENT * Indexer,DPS_DOCUMENT * Doc){
	DPS_TEXTITEM	Item;
	DPS_VAR		*BSec=DpsVarListFind(&Doc->Sections,"body");
	char            *buf_content = (Doc->Buf.pattern == NULL) ? Doc->Buf.content : Doc->Buf.pattern;
	char savec;

	DpsLog(Indexer, DPS_LOG_DEBUG, "Executing Text parser");
	
	if (BSec == NULL) return DPS_OK;
	Item.href = NULL;
	
	if(BSec && buf_content && Doc->Spider.index){
		char *lt;
                bzero((void*)&Item, sizeof(Item));
		Item.section = BSec->section;
		Item.strict = BSec->strict;
		Item.str = dps_strtok_r(buf_content, "\r\n", &lt, &savec);
		Item.section_name = BSec->name; /*"body";*/
		while(Item.str){
		        Item.len = (lt!=NULL) ? (size_t)(lt-Item.str) : dps_strlen(Item.str);
			(void)DpsTextListAdd(&Doc->TextList, &Item);
			Item.str = dps_strtok_r(NULL, "\r\n", &lt, &savec);
		}
	}
	return(DPS_OK);
}


static void DpsNextCharB(void *d) {
  DPS_HTMLTOK *t = (DPS_HTMLTOK *)d;
  (t->b)++;
}

static void DpsNextCharE(void *d) {
  DPS_HTMLTOK *t = (DPS_HTMLTOK *)d;
  (t->e)++;
}


void DpsHTMLTOKInit(DPS_HTMLTOK *tag) {
  bzero((void*)tag, sizeof(*tag));
  tag->next_b = &DpsNextCharB;
  tag->next_e = &DpsNextCharE;
  tag->trailend = tag->trail;
  tag->visible[0] = 1;
}

void DpsHTMLTOKFree(DPS_HTMLTOK *tag) {
/*  register size_t i;
  for (i = 0; i < sizeof(tag->section); i++) DPS_FREE(tag->section_name[i]);*/
}


const char * DpsHTMLToken(const char * s, const char ** lt,DPS_HTMLTOK *t){

	t->ntoks=0;
	t->s = s;
	t->lt = lt;
	
	if(t->s == NULL && (t->s = *lt) == NULL)
		return NULL;

	if(!*t->s) return NULL;

	if(!strncmp(t->s,"<!--",4)) t->type = DPS_HTML_COM;
	else	
	if(*t->s=='<' && t->s[1] != ' ' && t->s[1] != '<' && t->s[1] != '>') t->type = DPS_HTML_TAG;
	else	t->type = DPS_HTML_TXT;

	switch(t->type){
		case DPS_HTML_TAG:

			for(*lt = t->b = t->s + 1; *t->b; ) {
				const char * valbeg=NULL;
				const char * valend=NULL;
				size_t nt=t->ntoks;
				
				
				/* Skip leading spaces */
				while((*t->b)&&strchr(" \t\r\n",*t->b)) (*t->next_b)(t);

				if(*t->b=='>'){
					*lt = t->b + 1;
					return(t->s);
				}

				if(*t->b=='<'){ /* Probably broken tag occure */
					*lt = t->b;
					return(t->s);
				}

				/* Skip non-spaces, i.e. name */
				for(t->e = t->b; (*t->e) && !strchr(" =>\t\r\n", *t->e); (*t->next_e)(t));
				
				if(t->ntoks<DPS_MAXTAGVAL)
					t->ntoks++;
				
				t->toks[nt].val=0;
				t->toks[nt].vlen=0;
				t->toks[nt].name = t->b;
				t->toks[nt].nlen = (size_t)(t->e - t->b);

				if (nt == 0) {
				  if(!strncasecmp(t->b,"script",6)) t->script = 1;
				  else if(!strncasecmp(t->b,"/script",7)) { t->script = 0; if (t->comment_inside) { t->comment = t->comment_inside = 0; }}
				  else if(!strncasecmp(t->b, "style", 5)) t->style = 1;
				  else if(!strncasecmp(t->b, "/style", 6)) { t->style = 0; if (t->comment_inside) { t->comment = t->comment_inside = 0; }}
				  else if(!strncasecmp(t->b, "select", 4)) t->select = 1;
				  else if(!strncasecmp(t->b, "/select", 5)) t->select = 0;
				  else if(!strncasecmp(t->b, "body", 4)) t->body = 1;
				  else if(!strncasecmp(t->b, "/body", 5)) t->body = 0;
				  else if(!strncasecmp(t->b, "title", 5)) t->title = 1;
				  else if(!strncasecmp(t->b, "/title", 6)) t->title = 0;
				  else if(!strncasecmp(t->b, "noindex", 7)) t->noindex = 1;
				  else if(!strncasecmp(t->b, "/noindex", 8)) t->noindex = 0;
				  else if(!strncasecmp(t->b, "frameset", 8)) t->frameset++;
				  else if(!strncasecmp(t->b, "/frameset", 9)) { if (t->frameset) t->frameset--; }
				  else if(!strncasecmp(t->b, "br", 2)) { t->br++; }
				}

				if(*t->e=='>'){
					*lt = t->e + 1;
					return(t->s);
				}

				if(!(*t->e)){
					*lt = t->e;
					return(t->s);
				}
				
				/* Skip spaces */
				while((*t->e) && strchr(" \t\r\n",*t->e))(*t->next_e)(t);
				
				if(*t->e != '='){
					t->b = t->e;
				       *lt = t->b;        /* bug when hang on broken inside tag pages fix */
					continue;
				}
				
				/* Skip spaces */
				for(t->b = t->e + 1; (*t->b) && strchr(" \r\n\t", *t->b); (*t->next_b)(t));
				
				if(*t->b == '"'){
					t->b++;
					
					valbeg = t->b;
					for(t->e = t->b; (*t->e) && (*t->e != '"'); (*t->next_e)(t));
					valend = t->e;
					
					t->b = t->e;
					if(*t->b == '"')(*t->next_b)(t);
				}else
				if(*t->b == '\''){
					t->b++;
					
					valbeg = t->b;
					for(t->e = t->b; (*t->e) && (*t->e != '\''); (*t->next_e)(t));
					valend = t->e;
					
					t->b = t->e;
					if(*t->b == '\'')(*t->next_b)(t);
				}else{
					valbeg = t->b;
					for(t->e = t->b; (*t->e) && !strchr(" >\t\r\n", *t->e);(*t->next_e)(t));
					valend = t->e;
					
					t->b = t->e;
				}
				*lt = t->b;
				t->toks[nt].val = valbeg;
				t->toks[nt].vlen = (size_t)(valend - valbeg);
			}
			break;

		case DPS_HTML_COM: /* comment */
			
			if(!strncasecmp(t->s, "<!--DpsComment-->", 17))	t->noindex = 1;
			else
			if(!strncasecmp(t->s, "<!--/DpsComment-->", 18)) t->noindex = 0;
			else
			if(!strncasecmp(t->s, "<!--UdmComment-->", 17)) t->noindex = 1;
			else
			if(!strncasecmp(t->s, "<!--/UdmComment-->", 18)) t->noindex = 0;
			else
			if(!strncasecmp(t->s, "<!--noindex-->", 14)) t->noindex = 1;
			else
			if(!strncasecmp(t->s, "<!--/noindex-->", 15)) t->noindex = 0;
			else
			if(!strncasecmp(t->s, "<!--Comment-->", 14)) t->noindex = 1;
			else
			if(!strncasecmp(t->s, "<!--/Comment-->", 15)) t->noindex = 0;
			else
			if(!strncasecmp(t->s, "<!--htdig_noindex-->", 20)) t->noindex = 1;
			else
			if(!strncasecmp(t->s, "<!--/htdig_noindex-->", 21)) t->noindex = 0;
			else
			if(!strncasecmp(t->s, "<!-- google_ad_section_start -->", 32)) t->noindex = 0;
			else
			if(!strncasecmp(t->s, "<!-- google_ad_section_start(weight=ignore) -->", 47)) t->noindex = 1;
			else
			if(!strncasecmp(t->s, "<!-- google_ad_section_end -->", 30)) t->noindex = !(t->noindex);

			for(t->e = t->s; (*t->e) && (strncmp(t->e, "-->", 3)); (*t->next_e)(t));
			if(!strncmp(t->e, "-->", 3)) *lt = t->e + 3;
			else {
/*				*lt = t->e;*/
			    *lt = t->e = t->s + 3;
			    if (t->style || t->script) t->comment_inside = 1;
			    t->comment = 1;
			}
			break;

		case DPS_HTML_TXT: /* text */
		default:
			/* Special case when script  */
			/* body is not commented:    */
			/* <script> x="<"; </script> */
			/* We should find </script>  */
			
			for(t->e = t->s; *t->e; (*t->next_e)(t)){
				if(*t->e == '<'){
					if(t->script){
						if(!strncasecmp(t->e, "</script>",9)){
							/* This is when script body  */
							/* is not hidden using <!--  */
							break;
						}else
						if(!strncmp(t->e, "<!--",4)){
							/* This is when script body  */
							/* is hidden but there are   */
							/* several spaces between    */
							/* <SCRIPT> and <!--         */
							break;
						}
					}else{
					  if (t->e == t->s) continue;
						break;
					}
				}
			}
			
			*lt = t->e;
			break;
	}
	
	return t->s;
}



static int DpsHasEndTag(const char *name) {
  if (!strcasecmp(name, "img")) return 0;
  if (!strcasecmp(name, "br")) return 0;
  if (!strcasecmp(name, "hr")) return 0;
  if (!strcasecmp(name, "link")) return 0;
  if (!strcasecmp(name, "meta")) return 0;

  if (!strcasecmp(name, "area")) return 0;
  if (!strcasecmp(name, "base")) return 0;
  if (!strcasecmp(name, "basefont")) return 0;
  if (!strcasecmp(name, "col")) return 0;
  if (!strcasecmp(name, "frame")) return 0;
  if (!strcasecmp(name, "input")) return 0;
  if (!strcasecmp(name, "isindex")) return 0;
  if (!strcasecmp(name, "param")) return 0;

  return 1;
}


static void putItem(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_TEXTITEM *Item) {
  DPS_TEXTITEM *Last;
  if (Doc->TextList.nitems == 0) {
    (void)DpsTextListAdd(&Doc->TextList, Item);
  } else {
    Last = Doc->TextList.Items + Doc->TextList.nitems - 1;
    if (Item->section == Last->section 
	&& ((Item->href == NULL && Last->href == NULL) || strcmp(DPS_NULL2EMPTY(Item->href), DPS_NULL2EMPTY(Last->href)) == 0)
	) {
      if ((Last->str = (char*)DpsRealloc(Last->str, Last->len + Item->len + 1)) == NULL) return;

      dps_memcpy(Last->str + Last->len, Item->str, Item->len);
      Last->len += Item->len;
      Last->str[Last->len] = '\0';
      /*return;*/
    } else {
      DpsTextListAdd(&Doc->TextList, Item); /* temporary, need to be implemented when Items will be in unicode */
    }
  }
  return;
}


int DpsHTMLParseTag(DPS_AGENT *Indexer, DPS_HTMLTOK * tag, DPS_DOCUMENT * Doc, DPS_VAR *CrosSec) {
	DPS_TEXTITEM Item;
	DPS_VAR	*Sec;
	int opening, visible = 0;
	char name[128];
	register char * n;
	char *metaname = NULL;
	char *metacont = NULL;
	char *href = NULL;
	char *alt = NULL, *title = NULL;
	char *data_expanded_url = NULL;
	char *data_ultimate_url = NULL;
	char *base = NULL;
	char *lang = NULL;
	char *secname = NULL;
	char *rel = NULL;
	size_t i, j, seclen = 128, metacont_len = 0;
	char savec;

#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif

	if(!tag->ntoks) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return(0);
	}
	if(!tag->toks[0].name) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return(0);
	}
	if(tag->toks[0].nlen>sizeof(name)-1) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return(0);
	}

	secname = (char*)DpsMalloc(seclen + 1);
	if (secname == NULL) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return(0);
	}
	dps_strncpy(name, tag->toks[0].name, (i = dps_min(sizeof(name) - 1, tag->toks[0].nlen)) );
	name[i]='\0';
	
	for(n=name; *n; *n = (char)dps_tolower((int)*n), n++);
	
	if(name[0]=='/'){
	        char *e;
		size_t glen, slen;
		opening = 0;
		dps_memmove(name, name + 1, (slen = dps_strlen(name+1)) + 1);
		if ((strcasecmp(name, "p")) && (strcasecmp(name, "br")) && (strcasecmp(name, "option")) && (strcasecmp(name, "input")) && (strcasecmp(name, "font"))) {
		  do {
		    /* Find previous '.' or beginning */
		    for(e = tag->trailend; (e > tag->trail) && (e[0] != '.'); e--);
		    glen = (size_t)((e[0] == '.') ? (tag->trailend - e-1) : (tag->trailend - e));
		    *e = '\0';
		    tag->trailend = e;
		    if (tag->level) { tag->level--; tag->section[tag->level] = 0; }
		  } while ((strcmp(e + 1, name) != 0) && tag->trailend > tag->trail);
		}
	}else{
	        size_t name_len = dps_strlen(name);
		opening = 1;
		if ((strcasecmp(name, "p")) && (strcasecmp(name, "br")) && (strcasecmp(name, "option")) && (strcasecmp(name, "input")) && (strcasecmp(name, "font"))) {
		  Sec = DpsVarListFind(&Doc->Sections, name);
		  if (tag->level < sizeof(tag->visible) - 1) visible = tag->visible[tag->level + 1] = tag->visible[tag->level];
		  tag->section[tag->level] = (Sec) ? (unsigned char)Sec->section : 0;
		  tag->strict[tag->level] = (Sec) ? (unsigned char)Sec->strict : 0;
		  tag->section_name[tag->level] = (Sec) ? Sec->name : NULL;
		  if ((tag->level + 2 >= sizeof(tag->visible)) || (((tag->trailend - tag->trail) + name_len + 2) > sizeof(tag->trail)) ) {
		    DpsLog(Indexer, DPS_LOG_WARN, "Too deep or incorrect HTML, level:%d, trailsize:%d", tag->level, (tag->trailend - tag->trail) + name_len);
		    DpsLog(Indexer, DPS_LOG_DEBUG, " -- trail: %s", tag->trail);
#ifdef WITH_PARANOIA
		    DpsViolationExit(Indexer->handle, paran);
#endif
		    return 0;
		  }
		  tag->level++;
		  if (tag->trailend > tag->trail) {
		    tag->trailend[0] = '.';
		    tag->trailend++;
		  }
		  dps_memcpy(tag->trailend, name, name_len); /* was: dps_memmove */
		  tag->trailend += name_len;
		  tag->trailend[0] = '\0';
		}
	}

	tag->follow = Doc->Spider.follow;
	tag->index = Doc->Spider.index;

	for(i=0;i<tag->ntoks;i++){
		if(ISTAG(i,"name") || ISTAG(i,"http-equiv") || ISTAG(i,"property")) {
		  DPS_FREE(metaname);
		  if (tag->toks[i].vlen) metaname = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		}else
		if(ISTAG(i,"content")){
		  DPS_FREE(metacont);
		  if (tag->toks[i].vlen) metacont = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  metacont_len = tag->toks[i].vlen;
		}else
		if(ISTAG(i,"href")){
		  /* A, LINK, AREA*/
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(href);
		  href = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  DPS_FREE(y);
		}else
		if(ISTAG(i,"rel")){
		  /* A, LINK*/
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(rel);
		  rel = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  DPS_FREE(y);
		}else
		if(ISTAG(i,"alt")){
		  /* IMG, AREA, APPLET, and INPUT */
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(alt);
		  alt = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  DPS_FREE(y);
		}else
		if(ISTAG(i,"title")){
		  /* All elements but BASE, BASEFONT, HEAD, HTML, META, PARAM, SCRIPT, TITLE */
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(title);
		  title = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  DPS_FREE(y);
		}else
		if(ISTAG(i,"data-expanded-url")){
		  /* Twitter's A */
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(data_expanded_url);
		  data_expanded_url = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  DPS_FREE(y);
		}else
		if(ISTAG(i,"data-ultimate-url")){
		  /* Twitter's A */
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(data_ultimate_url);
		  data_ultimate_url = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  DPS_FREE(y);
		}else
		if(ISTAG(i,"base")) {
		  /* OBJECT, EMBED*/
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(base);
		  base = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  DPS_FREE(y);
		}else
		if(Indexer->Flags.rel_nofollow && ISTAG(i, "rel")) {
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  if (strcasestr(y, "nofollow") != NULL) {
		    tag->follow = DPS_FOLLOW_NO;
		  }
		  DPS_FREE(y);
		}else
		  if(ISTAG(i, "style") && DpsHasEndTag(name)) {
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  char *z = strcasestr(y, "visibility:");
		  if (z != NULL) {
		    char *p = strchr(z, (int)';');
		    char *x = strcasestr(z, "visible");
		    visible = tag->visible[tag->level];
		    if (x && ((p == NULL) || (p > x))) {
		      visible = 1;
		    }
		    x = strcasestr(z, "hidden");
		    if (x && ((p == NULL) || (p > x))) {
		      visible = 0;
		    }
		    x = strcasestr(z, "none");
		    if (x && ((p == NULL) || (p > x))) {
		      visible = 0;
		    }
		    tag->visible[tag->level] = (char)visible;
		  }
		  z = strcasestr(y, "display:");
		  if (z != NULL) {
		    char *p = strchr(z, (int)';');
		    char *x = strcasestr(z, "none");
		    visible = tag->visible[tag->level];
		    if (x && ((p == NULL) || (p > x))) {
		      visible = 0;
		    } else visible = 1;
		    tag->visible[tag->level] = (char)visible;
		  }
		  DPS_FREE(y);
		}else
		if(ISTAG(i, "src")) {
			/* IMG, FRAME, IFRAME, EMBED */
		  if (href == NULL && data_expanded_url == NULL && data_ultimate_url == NULL) {
		    char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		    href = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		    DPS_FREE(y);
		  }
		}else
		if (ISTAG(i, "lang")) {
		  char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		  DPS_FREE(lang);
		  lang = (char*)DpsStrdup(DpsTrim(y, " \t\r\n"));
		  for(n = lang; *n; *n = (char)dps_tolower((int)*n),n++);
		  DPS_FREE(y);
		} /*else*/ if (Doc->Spider.index && visible) {
		  j = dps_max(tag->toks[i].nlen + 12, dps_strlen(name) + tag->toks[i].nlen + 2);
		  if (j > seclen) {
		    secname = (char*)DpsRealloc(secname, seclen = j);
		    if (secname == NULL) {
#ifdef WITH_PARANOIA
		      DpsViolationExit(Indexer->handle, paran);
#endif
		      return(0);
		    }
		  }
			
		  for ( 
		       j = 0,
			 dps_strcpy(secname, "attribute."),
			 dps_strncat(secname + 10, tag->toks[i].name, tag->toks[i].nlen),
			 secname[seclen - 1]='\0';
		       j < 2;
		    
		       j++,
			 dps_strcpy(secname, name), dps_strcat(secname, "."),
			 dps_strncat(secname, tag->toks[i].name, tag->toks[i].nlen),
			 secname[seclen - 1]='\0'
			) {

		    if ((Sec = DpsVarListFind(&Doc->Sections, secname))) {
		      char *y = DpsStrndup(DPS_NULL2EMPTY(tag->toks[i].val), tag->toks[i].vlen);
		      bzero((void*)&Item, sizeof(Item));
		      Item.str = y;
		      Item.section = Sec->section;
		      Item.strict = Sec->strict;
		      Item.section_name = Sec->name;
		      Item.href = NULL;
		      Item.len = tag->toks[i].vlen;
		      (void)DpsTextListAdd(&Doc->TextList, &Item);
		      DPS_FREE(y);
		    }
		  }
		}
	}
	
	/* Let's find tag name in order of frequency */

	if(!strcmp(name,"a")){
		DPS_FREE(tag->lasthref);			/*117941*/
	}else
	if(!strcmp(name,"title"))	tag->title=opening;	/*6192*/
	else
	if(!strcmp(name,"html") && opening && (lang != NULL)) {
		DpsVarListReplaceStr(&Doc->Sections, "Meta-Language", lang);
	}else
	if(!strcmp(name,"body")) {
		tag->body=opening;	/*5146*/
		if (opening && (lang != NULL)) {
			DpsVarListReplaceStr(&Doc->Sections, "Meta-Language", lang);
		}
	}else
	if((!strcmp(name,"meta"))&&(metaname)&&(metacont)){ 
		
		dps_strcpy(secname,"meta.");
		dps_strncat(secname + 5 ,metaname, seclen - 5);
		secname[seclen - 1]='\0';
		
		if((tag->comment + tag->noindex == 0) && (Sec=DpsVarListFind(&Doc->Sections,secname)) && Doc->Spider.index && visible) {
/*			DpsSGMLUnescape(metacont);   we do this later */
		        bzero((void*)&Item, sizeof(Item));
			Item.str=metacont;
			Item.section = Sec->section;
			Item.strict = Sec->strict;
			Item.section_name=secname;
			Item.href = NULL;
			Item.len = metacont_len;
			(void)DpsTextListAdd(&Doc->TextList,&Item);
		}
		
		if(!strcasecmp(metaname,"Content-Type")){
			char *p;
			if((p = strcasestr(metacont, "charset="))) {
				const char *cs = DpsCharsetCanonicalName(DpsTrim(p + 8, " \t;\"'"));
				const char *prev_cs = DpsVarListFindStr(&Doc->Sections, "Meta-Charset", NULL);
				if (prev_cs == NULL) {
				    DpsVarListReplaceStr(&Doc->Sections, "Meta-Charset", cs ? cs : p + 8);
				} else if (strcasecmp(prev_cs, cs ? cs : p + 8) != 0) {
				    DpsVarListReplaceStr(&Doc->Sections, "Meta-Charset", ""); /* nil it to let Guesser to decide */
				}
			}
		}else
		if(!strcasecmp(metaname, "Content-Language") || !strcasecmp(metaname, "DC.Language")) {
			char *l;
			l = (char*)DpsStrdup(metacont);
			for(n = l; *n; *n = (char)dps_tolower((int)*n), n++);
/*			if (dps_strlen(l) > 2) {
			  if (l[2] != '_') {
			    l[2] = '\0';
			  } else {
			    if (dps_strlen(l) > 5) {
			      l[5] = '\0';
			    }
			  }
			}*/
			DpsVarListReplaceStr(&Doc->Sections, "Meta-Language", l);
			DPS_FREE(l);
		}else
		if(!strcasecmp(metaname,"refresh")){
			/* Format: "10; Url=http://something/" */
			/* URL can be written in different     */
			/* forms: URL, url, Url and so on      */
		        char *p;
			
			if((p = strchr(metacont, '='))){
				if((p >= metacont + 3) && (!strncasecmp(p-3,"URL=",4))){
				  DPS_FREE(href);
				  href = (char*)DpsStrdup(p + 1);
				}
				/* noindex if redirect on refresh */
				tag->index = 0;
				Doc->Spider.index = 0;
			}
 		}else
		if(!strcasecmp(metaname,"robots")&&(Doc->Spider.use_robots)&&(metacont)){
			char * lt;
			char * rtok;
					
			rtok = dps_strtok_r(metacont, " ,\r\n\t", &lt, &savec);
			while(rtok){
				if(!strcasecmp(rtok,"ALL")){
					/* Set Server parameters */
					tag->follow=Doc->Spider.follow;
					tag->index=Doc->Spider.index;
				}else if(!strcasecmp(rtok,"NONE")){
					tag->follow=DPS_FOLLOW_NO;
					tag->index=0;
					Doc->Spider.follow = DPS_FOLLOW_NO;
					Doc->Spider.index = 0;
					Doc->method = DPS_METHOD_DISALLOW;
					if (DpsNeedLog(DPS_LOG_DEBUG)) {
					  DpsVarListReplaceInt(&Doc->Sections, "Index", 0);
					  DpsVarListReplaceInt(&Doc->Sections, "Follow", DPS_FOLLOW_NO);
					  DpsLog(Indexer, DPS_LOG_DEBUG, "Disallow by meta.robots");
					}
				}else if(!strcasecmp(rtok,"NOINDEX")) {
					tag->index=0;
					Doc->Spider.index = 0;
					Doc->method = DPS_METHOD_DISALLOW;
					if (DpsNeedLog(DPS_LOG_DEBUG)) {
					    DpsVarListReplaceInt(&Doc->Sections, "Index", 0);
					    DpsLog(Indexer, DPS_LOG_DEBUG, "Disallow by meta.robots");
					}
				}else if(!strcasecmp(rtok,"NOFOLLOW")) {
					tag->follow=DPS_FOLLOW_NO;
					Doc->Spider.follow = DPS_FOLLOW_NO;
					if (DpsNeedLog(DPS_LOG_DEBUG)) DpsVarListReplaceInt(&Doc->Sections, "Follow", DPS_FOLLOW_NO);
				}else if(!strcasecmp(rtok,"NOARCHIVE")) {
				        DpsVarListReplaceStr(&Doc->Sections, "Z", "");
				}else if(!strcasecmp(rtok,"INDEX")) {
				        tag->index = Doc->Spider.index;
					if (DpsNeedLog(DPS_LOG_DEBUG)) DpsVarListReplaceInt(&Doc->Sections, "Index", Doc->Spider.index);
				}else if(!strcasecmp(rtok,"FOLLOW")) {
					tag->follow=Doc->Spider.follow;
					if (DpsNeedLog(DPS_LOG_DEBUG)) DpsVarListReplaceInt(&Doc->Sections, "Follow", Doc->Spider.follow);
				}
				rtok = dps_strtok_r(NULL, " \r\n\t", &lt, &savec);
			}
		}else
		if(!strcasecmp(metaname, "DP.PopRank")) {
		  char pstr[32];
		  double pop_rank = dps_atof(metacont);
		  dps_snprintf(pstr, sizeof(pstr), "%f", pop_rank);
		  DpsVarListReplaceStr(&Doc->Sections, "Pop_Rank", pstr);
		}else
		if(!strcasecmp(metaname, "Last-Modified")) {
		  DpsVarListReplaceStr(&Doc->Sections, metaname, metacont);
		}else
		if(!strcasecmp(metaname, "Date")) {
		  DpsVarListReplaceStr(&Doc->Sections, metaname, metacont);
		}else
		if(!strcasecmp(metaname, "geo.position")) {
		  double lat, lon;
		  char *l = strchr(metacont, (int)';');
		  if (l != NULL) {
		    lat = dps_atof(metacont);
		    lon = dps_atof(l + 1);
		    DpsVarListReplaceDouble(&Doc->Sections, "geo.lat", lat);
		    DpsVarListReplaceDouble(&Doc->Sections, "geo.lon", lon);
		  }
		}else
		if(!strcasecmp(metaname, "ICBM")) {
		  double lat, lon;
		  char *l = strchr(metacont, (int)',');
		  if (l != NULL) {
		    lat = dps_atof(metacont);
		    lon = dps_atof(l + 1);
		    DpsVarListReplaceDouble(&Doc->Sections, "geo.lat", lat);
		    DpsVarListReplaceDouble(&Doc->Sections, "geo.lon", lon);
		  }
		} else if (strcasecmp(metaname, "title") && strcasecmp(metaname, "category") && strcasecmp(metaname, "tag")
			   && strcasecmp(metaname, "url")) {
		  if ((Sec = DpsVarListFind(&Doc->Sections, metaname)) != NULL) {
			DpsVarListReplaceStr(&Doc->Sections, metaname, metacont);
		  }
		}
	}
	else	if(!strcmp(name,"script"))	tag->script=opening;
	else	if(!strcmp(name,"style"))	tag->style=opening;
	else	if(!strcmp(name,"noindex"))	tag->noindex = opening;
	else	if(!strcmp(name,"frameset"))	{
	  if (opening) tag->frameset++;
	  else if (tag->frameset) tag->frameset--;
	}
	else	
	if((!strcmp(name,"base"))&&(href)){
		
		DpsVarListReplaceStr(&Doc->Sections,"base.href",href);
		
		/* Do not add BASE HREF itself into database.      */
		/* It will be used only to compose relative links. */
		DPS_FREE(href);
	}
	if(( !strcmp(name, "link")) && (href) && (rel) && !strcasecmp(rel, "canonical")) {

	  DPS_HREF	Href;
	  char *URL = DpsVarListFindStr(&Doc->Sections, "URL", NULL);
	  char *newhref = NULL;

	  DpsHrefInit(&Href);
	  Href.referrer = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
	  Href.hops = 1 + DpsVarListFindInt(&Doc->Sections, "Hops", 0);
	  Href.site_id = 0;
	  Href.url = DpsStrdup(href);
	  Href.method = DPS_METHOD_GET;
	  Href.charset_id = Doc->charset_id;
	  DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
	  
	  DpsConvertHref(Indexer, &Doc->CurURL, &Href);

	  if (!strcasecmp(DpsVarListFindStr(&Indexer->Vars, "DetectClones", DPS_DETECTCLONES), "yes") && strcasecmp(Href.url, URL)) {
		
		Doc->method = DPS_METHOD_DISALLOW;
		DpsLog(Indexer, DPS_LOG_DEBUG, "Disallow not canonical URL");
	  }
	  DPS_FREE(Href.url); /* this is freed and reallocated inside DpsConvertHref */
	  /* Do not add LINK HREF into database, we already do it above. */
	  DPS_FREE(href);
	}
	else if (href && CrosSec && alt != NULL && TAG_WITH_CROSSATTRIBUTE ) {
	    Item.href = href;
	    Item.section = CrosSec->section;
	    Item.section_name = CrosSec->name;
	    Item.strict = CrosSec->strict;
	    Item.str = alt;
	    Item.len = dps_strlen(alt);
	    putItem(Indexer, Doc, &Item);
	}
	else if (href && CrosSec && title != NULL && TAG_WITH_CROSSATTRIBUTE ) {
	    Item.href = href;
	    Item.section = CrosSec->section;
	    Item.section_name = CrosSec->name;
	    Item.strict = CrosSec->strict;
	    Item.str = title;
	    Item.len = dps_strlen(title);
	    putItem(Indexer, Doc, &Item);
	}

	if((href || data_expanded_url || data_ultimate_url) && visible && (Doc->Spider.follow != DPS_FOLLOW_NO) && (tag->follow != DPS_FOLLOW_NO) && !(Indexer->Flags.SkipHrefIn & DpsHrefFrom(name)) ) {
		DPS_HREF	Href;
		char *url_pointer = data_ultimate_url;
		
		if (url_pointer == NULL) url_pointer = data_expanded_url;
		if (url_pointer == NULL) url_pointer = href;

		if ((Doc->subdoc < Indexer->Flags.SubDocLevel) && (Doc->sd_cnt < Indexer->Flags.SubDocCnt)
		    && (!strcasecmp(name, "IFRAME") || !strcasecmp(name, "EMBED") || !strcasecmp(name, "FRAME") 
					   || !strcasecmp(name, "META"))) {
		  DpsSGMLUnescape(url_pointer);
		  DpsIndexSubDoc(Indexer, Doc, base, NULL, url_pointer);
		} else {
/*		DpsSGMLUnescape(href); why we need do this ? */
		  DpsHrefInit(&Href);
		  Href.referrer = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
		  Href.hops = 1 + DpsVarListFindInt(&Doc->Sections, "Hops", 0);
		  Href.site_id = 0; /*DpsVarListFindInt(&Doc->Sections, "Site_id", 0);*/
		  Href.url = url_pointer;
		  Href.method=DPS_METHOD_GET;
		  Href.charset_id = Doc->charset_id;
		  DpsHrefListAdd(Indexer, &Doc->Hrefs,&Href);
		  
		  /* For crosswords */
		  DPS_FREE(tag->lasthref);
		  tag->lasthref = (char*)DpsStrdup(url_pointer);
		}
	}
	DPS_FREE(metaname);
	DPS_FREE(metacont);
	DPS_FREE(href);
	DPS_FREE(data_expanded_url);
	DPS_FREE(data_ultimate_url);
	DPS_FREE(base);
	DPS_FREE(lang);
	DPS_FREE(secname);
	DPS_FREE(rel);
	
#ifdef WITH_PARANOIA
	DpsViolationExit(Indexer->handle, paran);
#endif
	return 0;
}


#define MAXSTACK	1024

typedef struct {
	size_t len;
	char * ofs;
} DPS_TAGSTACK;


int DpsHTMLParseBuf(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, const char *section_name, const char *content) {
	DPS_HTMLTOK	tag;
	DPS_TEXTITEM	Item;
	const char	*htok;
	const char	*last;
	DPS_VAR		*BSec = DpsVarListFind(&Doc->Sections, (section_name) ? section_name : "body");
	DPS_VAR		*TSec = DpsVarListFind(&Doc->Sections, "title");
	int		body_sec  = BSec ? BSec->section : 0;
	int		title_sec = TSec ? TSec->section : 0;
	int		body_strict  = BSec ? BSec->strict : 0;
	int		title_strict = TSec ? TSec->strict : 0;
	int             status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
	int             skip = (status > 299 && status < 600 && status != 304);
	DPS_VAR         *CrosSec = DpsVarListFind(&Doc->Sections, "crosswords");

#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif
	
	bzero((void*)&Item, sizeof(Item));
	DpsHTMLTOKInit(&tag);
	tag.follow = Doc->Spider.follow;
	tag.index = Doc->Spider.index;
	tag.body = 1; /* for the case when the BodyPattern is applied */
	tag.noindex = (strstr(content, "<!-- google_ad_section_start -->") == NULL) ? 0 : 2;

	htok=DpsHTMLToken(content, &last, &tag);
	
	while(htok){
	  char       *tmp=NULL;
	  const char *tmpbeg;
	  const char *tmpend;
	  /*
	  { char hbuf[33];

	    strncpy(hbuf, htok, 32);
	    hbuf[32] = 0;
	    
	    fprintf(stderr, " -- tag.scr:%d .com:%d .noi:%d .tit:%d .bod:%d sty:%d fra:%d ind:%d sel:%d vis:%d br:%d-- htok: %32s\n", 
		    tag.script, tag.comment, tag.noindex, tag.title, tag.body, tag.style, tag.frameset, tag.index, 
		    tag.select, tag.visible[tag.level], tag.br, hbuf);
	  }
	  */
	  switch(tag.type){
			
	  case DPS_HTML_COM:
	    break;

	  case DPS_HTML_TXT:
	    if (skip) break;

	    /*	    for( tmpbeg=htok;   tmpbeg<last && strchr(" \r\n\t", tmpbeg[0]); tmpbeg++);
		    for( tmpend=last-1; htok<tmpend && strchr(" \r\n\t", tmpend[0]); tmpend--);*/
	    tmpbeg = htok;
	    tmpend = last-1;
	    if(tmpbeg > tmpend)break;

	    tmp = DpsStrndup(tmpbeg,(size_t)(tmpend-tmpbeg+1));

	    if (BSec && (tag.comment + tag.noindex == 0) && !tag.title && (tag.body || tag.frameset) && !tag.script && !tag.style && tag.index && !tag.select 
		&& tag.visible[tag.level]) {
	      int z;
	      for(z = (int)tag.level - 1; z >= 0 && tag.section[z] == 0; z--);
	      bzero((void*)&Item, sizeof(Item));
	      Item.href=tag.lasthref;
	      if (z >= 0) {
		Item.section = tag.section[z];
		Item.strict = tag.strict[z];
		Item.section_name = (section_name) ? section_name : tag.section_name[z];
	      } else {
		Item.section = body_sec;
		Item.strict = body_strict;
		Item.section_name = (section_name) ? section_name : "body";
	      }
	      while (tag.br) {
		Item.str = "\n";
		Item.len = 1;
		putItem(Indexer, Doc, &Item);
		tag.br--;
	      }
	      Item.str=tmp;
	      Item.len = (size_t)(tmpend-tmpbeg+1);
	      /*	      DpsTextListAdd(&Doc->TextList,&Item);*/
	      putItem(Indexer, Doc, &Item);
	    }
	    if (TSec && (tag.comment != 1) && (tag.noindex != 1) && tag.title && tag.index && !tag.select && tag.visible[tag.level]) {
	      bzero((void*)&Item, sizeof(Item));
	      Item.href=NULL;
	      Item.str=tmp;
	      Item.section = title_sec;
	      Item.strict = title_strict;
	      Item.section_name = "title";
	      Item.len = (size_t)(tmpend-tmpbeg+1);
	      /*	      DpsTextListAdd(&Doc->TextList,&Item);*/
	      putItem(Indexer, Doc, &Item);
	    }
	    DPS_FREE(tmp);
	    break;
		
	  case DPS_HTML_TAG:
	      DpsHTMLParseTag(Indexer, &tag, Doc, CrosSec);
	    break;
	  }
	  htok = DpsHTMLToken(NULL, &last, &tag);
		
	}
	DPS_FREE(tag.lasthref);	
	DpsHTMLTOKFree(&tag);
#ifdef WITH_PARANOIA
	DpsViolationExit(Indexer->handle, paran);
#endif
	return DPS_OK;
}

int DpsHTMLParse(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
  DpsLog(Indexer, DPS_LOG_DEBUG, "Executing HTML parser");
  return DpsHTMLParseBuf(Indexer, Doc, NULL, (Doc->Buf.pattern == NULL) ? Doc->Buf.content : Doc->Buf.pattern);
}
