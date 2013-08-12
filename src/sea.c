/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2005-2012 DataPark Ltd. All rights reserved.

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
/*
#define DEBUG 1
*/
#include "dps_common.h"
#include "dps_sea.h"
#include "dps_guesser.h"
#include "dps_log.h"
#include "dps_parsehtml.h"
#include "dps_uniconv.h"
#include "dps_unidata.h"
#include "dps_vars.h"

#include <string.h>
#include <strings.h>
#include <math.h>

#define TOP_SENTENCES 3
#define f(x) (1.0 / (1.0 + exp(-1.0 * (x))))
#define f2(x) (1.0 / (1.0 + exp(-1.15 * (x))))
#define EPS 0.00001
#define LOW_BORDER_EPS  0.000001
#define LOW_BORDER_EPS2 0.000001
#define HI_BORDER_EPS  (1.0 - 0.000001)
#define HI_BORDER_EPS2 (1.0 - 0.000001)


static int SentCmp(const DPS_SENTENCE *s1, const DPS_SENTENCE *s2) {
  register double r1 = (s1->di + s1->Oi), r2 = (s2->di + s2->Oi);
  if (r1 < r2) return 1;
  if (r1 > r2) return -1;
  return 0;
}

static int SentOrderCmp(const DPS_SENTENCE *s1, const DPS_SENTENCE *s2) {
  if (s1->order < s2->order) return -1;
  if (s1->order > s2->order) return 1;
  return 0;
}

int DpsSEAMake(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DSTR *excerpt,  
	       const char *content_lang, size_t *indexed_size, size_t *indexed_limit, 
	       size_t max_word_len, size_t min_word_len, int crossec, int seasec
#ifdef HAVE_ASPELL
	       , int have_speller, AspellSpeller *speller
#endif
	       ) {
  DPS_SENTENCELIST List;
  DPS_MAPSTAT MapStat;
  DPS_TEXTITEM Item;
  DPS_VAR	*Sec;
  dpsunicode_t *sentence, *lt, savec;
  double *links, *lang_cs, w;
  double delta, pdiv, cur_div;
  size_t l, sent_len, order;
  size_t min_len = 10000000, min_pos = 0;
  int  it;
  register size_t i, j;
#ifdef DEBUG
  char lcstr[4096];

#endif

  TRACE_IN(Indexer, "DpsSEAMake");

  if((Sec = DpsVarListFind(&Doc->Sections, "sea"))) { /* set SEA section to NULL */
    DPS_FREE(Sec->val);
    DPS_FREE(Sec->txt_val);
    Sec->curlen = 0;
  }
  
  bzero(&List, sizeof(List));
  order = 0;
  sentence = DpsUniStrTok_SEA((dpsunicode_t*)excerpt->data, &lt);
  while(sentence) {
    if (lt != NULL) { savec = *lt; *lt = 0; }
#ifdef DEBUG
    DpsConv(&Indexer->uni_lc, lcstr, sizeof(lcstr), (char*)sentence, sizeof(dpsunicode_t) * (DpsUniLen(sentence) + 1));
    fprintf(stderr, "Sentence.%d: %s\n", List.nitems, lcstr);
#endif
    if ((sent_len = DpsUniLen(sentence)) >= Indexer->Flags.SEASentenceMinLength) {
      j = 1;
      for (i = 0; i < List.nitems; i++) {
	if (DpsUniStrCmp(sentence, List.Sent[i].sentence) == 0) {
	  j = 0; break;
	}
      }
      if (j) {
	if ( List.nitems < Indexer->Flags.SEASentences ) {
	  if (List.nitems == List.mitems) {
	    List.mitems += 16;
	    List.Sent = (DPS_SENTENCE*)DpsRealloc(List.Sent, List.mitems * sizeof(DPS_SENTENCE));
	    if (List.Sent == NULL) { TRACE_OUT(Indexer); return DPS_ERROR;}
	  }
	  List.Sent[List.nitems].sentence = DpsUniDup(sentence);
	  List.Sent[List.nitems].len = sent_len;
	  List.Sent[List.nitems].order = order++;
	  sentence = DpsUniDup(sentence);
	  DpsUniStrToLower(sentence);
	  bzero(&List.Sent[List.nitems].LangMap, sizeof(DPS_LANGMAP));
	  DpsBuildLangMap(&List.Sent[List.nitems].LangMap, (char*)sentence, sent_len * sizeof(dpsunicode_t), 0, 0);
	  if (sent_len < min_len) { min_len = sent_len; min_pos = List.nitems; }
	  List.nitems++;
	  DPS_FREE(sentence);
	} else if (sent_len > min_len) {
	  DPS_FREE(List.Sent[min_pos].sentence);
	  List.Sent[min_pos].sentence = DpsUniDup(sentence);
	  List.Sent[min_pos].len = sent_len;
	  List.Sent[min_pos].order = order++;
	  sentence = DpsUniDup(sentence);
	  DpsUniStrToLower(sentence);
	  bzero(&List.Sent[min_pos].LangMap, sizeof(DPS_LANGMAP));
	  DpsBuildLangMap(&List.Sent[min_pos].LangMap, (char*)sentence, sent_len * sizeof(dpsunicode_t), 0, 0);
	  DPS_FREE(sentence);
	  min_len = List.Sent[0].len; min_pos = 0;
	  for(i = 1; i < List.nitems; i++) if (List.Sent[i].len < min_len) { min_len = List.Sent[i].len; min_pos = i; }
	}
      }
    }
#ifdef DEBUG
    fprintf(stderr, "Sent. len.:%d, Min.allowed: %d\n", sent_len, Indexer->Flags.SEASentenceMinLength);
#endif
    if (lt != NULL) *lt = savec;
    sentence = DpsUniStrTok_SEA(NULL, &lt);
  }
  DpsLog(Indexer, DPS_LOG_DEBUG, "SEA sentences: %d", List.nitems);
  if (List.nitems < 4) {
    for (i = 0; i < List.nitems; i++) DPS_FREE(List.Sent[i].sentence);
    DPS_FREE(List.Sent); 
    TRACE_OUT(Indexer);
    return DPS_OK; 
  }

  links = (double*)DpsMalloc(sizeof(double) * List.nitems * List.nitems);
  lang_cs = (double*)DpsMalloc(sizeof(double) * List.nitems);
/*
        k                 ot
  links[i * List.nitems + j] 
*/

  if (links != NULL && lang_cs != NULL) {

    for (i = 0; i < List.nitems; i++) {
      DpsPrepareLangMap(&List.Sent[i].LangMap);
    }

    for (i = 0; i < List.nitems; i++) {
      List.Sent[i].Oi =  List.Sent[i].di = 0.5;
      if (Doc->lang_cs_map == NULL) {
	  links[i * List.nitems + i] = 0.0;
      } else {
	MapStat.map = &List.Sent[i].LangMap;
	DpsCheckLangMap6(Doc->lang_cs_map, &List.Sent[i].LangMap, &MapStat, DPS_LM_TOPCNT * DPS_LM_TOPCNT, 2 * DPS_LM_TOPCNT);
	links[i * List.nitems + i] = (double)MapStat.hits / (2.0 * DPS_LM_TOPCNT) / (List.nitems + 1);
      }
#ifdef DEBUG
      DpsLog(Indexer, DPS_LOG_INFO, "Link %u->%u: %f [hits:%d miss:%d]", i, i, links[i * List.nitems + i], MapStat.hits, MapStat.miss);
#endif
      for (j = 0; j < List.nitems; j++) {
	  if (j == i) continue;
	MapStat.map = &List.Sent[j].LangMap;
	DpsCheckLangMap6(&List.Sent[j].LangMap, &List.Sent[i].LangMap, &MapStat, DPS_LM_TOPCNT * DPS_LM_TOPCNT, 2 * DPS_LM_TOPCNT);

	links[i * List.nitems + j] = (double)MapStat.hits / (2.0 * DPS_LM_TOPCNT) / (List.nitems + 1);
#ifdef DEBUG
	DpsLog(Indexer, DPS_LOG_INFO, "Link %u->%u: %f [hits:%d miss:%d]", i, j, links[i * List.nitems + j], MapStat.hits, MapStat.miss);
#endif
      }
    }

    for (l = 0; l < List.nitems; l++) {
	w = 0.0;
	for (i = 0; i < List.nitems; i++) { 
	    w += links[l * List.nitems + i] * List.Sent[i].Oi;
	}
	w = f(w);
	if (w < LOW_BORDER_EPS2) w = LOW_BORDER_EPS2;
	else if (w > HI_BORDER_EPS2) w = HI_BORDER_EPS2;
	List.Sent[l].di = w;
    }

    DpsSort(List.Sent, List.nitems, sizeof(DPS_SENTENCE), (qsort_cmp)SentCmp);

#ifdef DEBUG
    DpsConv(&Indexer->uni_lc, lcstr, sizeof(lcstr), (char*)List.Sent[0].sentence, sizeof(dpsunicode_t) * (DpsUniLen(List.Sent[0].sentence) + 1));
    fprintf(stderr, "Sent.0: %f %f -- %s\n", List.Sent[0].di, List.Sent[0].Oi, lcstr);
    DpsConv(&Indexer->uni_lc, lcstr, sizeof(lcstr), (char*)List.Sent[1].sentence, sizeof(dpsunicode_t) * (DpsUniLen(List.Sent[1].sentence) + 1));
    fprintf(stderr, "Sent.1: %f %f -- %s\n", List.Sent[1].di, List.Sent[1].Oi, lcstr);
    DpsConv(&Indexer->uni_lc, lcstr, sizeof(lcstr), (char*)List.Sent[2].sentence, sizeof(dpsunicode_t) * (DpsUniLen(List.Sent[2].sentence) + 1));
    fprintf(stderr, "Sent.2: %f %f -- %s\n", List.Sent[2].di, List.Sent[2].Oi, lcstr);
    DpsConv(&Indexer->uni_lc, lcstr, sizeof(lcstr), (char*)List.Sent[3].sentence, sizeof(dpsunicode_t) * (DpsUniLen(List.Sent[3].sentence) + 1));
    fprintf(stderr, "Sent.3: %f %f -- %s\n", List.Sent[3].di, List.Sent[3].Oi, lcstr);
    DpsConv(&Indexer->uni_lc, lcstr, sizeof(lcstr), (char*)List.Sent[4].sentence, sizeof(dpsunicode_t) * (DpsUniLen(List.Sent[4].sentence) + 1));
    fprintf(stderr, "Sent.4: %f %f -- %s\n", List.Sent[4].di, List.Sent[4].Oi, lcstr);
#endif
    DpsSort(List.Sent, TOP_SENTENCES, sizeof(DPS_SENTENCE), (qsort_cmp)SentOrderCmp);

    bzero(&Item, sizeof(Item));
    Item.section = seasec;
    Item.href = NULL;
    Item.section_name = "sea";
    for (i = 0; i < TOP_SENTENCES; i++) {
      dpsunicode_t *UStr = DpsUniDup(List.Sent[i].sentence);
      DpsPrepareItem(Indexer, Doc, &Item, List.Sent[i].sentence, UStr, content_lang, indexed_size, indexed_limit,
		     max_word_len, min_word_len, crossec
#ifdef HAVE_ASPELL
		     , have_speller, speller, NULL
#endif
		     );
      DPS_FREE(UStr);
    }
  }
  DPS_FREE(lang_cs);
  DPS_FREE(links);
  for (i = 0; i < List.nitems; i++) DPS_FREE(List.Sent[i].sentence);
  DPS_FREE(List.Sent);

  TRACE_OUT(Indexer);
  return DPS_OK;
}
