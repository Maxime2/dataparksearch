/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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
#include "dps_boolean.h"
#include "dps_utils.h"
#include "dps_hash.h"
#include "dps_word.h"
#include "dps_searchtool.h"
#include "dps_stopwords.h"
#include "dps_log.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <strings.h>

/*
#define DEBUG_BOOL
*/

#ifdef DEBUG_BOOL
static char item_type(int cmd) {
	switch(cmd){
		case DPS_STACK_LEFT:	return('(');
		case DPS_STACK_RIGHT:	return(')');
		case DPS_STACK_OR:	return('|');
		case DPS_STACK_AND:	return('&');
		case DPS_STACK_NEAR:	return('.');
		case DPS_STACK_ANYWORD:	return('*');
		case DPS_STACK_NOT:	return('~');
		case DPS_STACK_PHRASE_LEFT:	return('<');
		case DPS_STACK_PHRASE_RIGHT:	return('>');
		case DPS_STACK_WORD:	return('w');
		default:		return('?');
	}
}
#endif


int DpsAddStackItem(DPS_AGENT *query, DPS_RESULT *Res, DPS_PREPARE_STATE *state, char *word, dpsunicode_t *uword) {
  int origin, loose = (query->flags & DPS_FLAG_STOPWORDS_LOOSE);
  size_t      i; 
  size_t wlen = (uword == NULL) ? 0 : DpsUniLen(uword);
  dpshash32_t crcword = (word == NULL) ? 0 : DpsStrHash32(word);

#ifdef DEBUG_BOOL
  DpsLog(query, DPS_LOG_EXTRA, "0[%d].%x %c -- %s [%x] .secno:%d(%d)  wlen:%d\n", state->order, state->origin, item_type(state->cmd), 
	 (word == NULL) ? "<NULL>" : word, crcword, state->secno[state->p_secno], state->p_secno, wlen);
#endif

  if((uword != NULL) && ( DpsStopListFind(&query->Conf->StopWords, uword, (loose) ? state->qlang : "") ||
			  (query->WordParam.min_word_len > wlen) ||
			  (query->WordParam.max_word_len < wlen)) ) {

    origin = state->origin | DPS_WORD_ORIGIN_STOP;
  } else {
    origin = state->origin;
  }
  /* FIX ME: it's better remove such words in another way, mey by DPS_WORDS_ORIGIN_HARDSTOP ?
  if (state->cmd == DPS_STACK_WORD && !(origin & DPS_WORD_ORIGIN_QUERY)) {
    for (i = 0; i < Res->nitems; i++) {
      if ((Res->items[i].order == state->order) && (Res->items[i].crcword == crcword)) return DPS_OK;
    }
  }
  */
  if (Res->nitems >= Res->mitems - 2) {
    Res->mitems += DPS_MAXSTACK;
    Res->items = (DPS_STACK_ITEM*)DpsRealloc(Res->items, Res->mitems * sizeof(DPS_STACK_ITEM));
    if (Res->items == NULL) {
      DpsLog(query, DPS_LOG_ERROR, "Can't alloc %d bytes for %d mitems", Res->mitems * sizeof(DPS_STACK_ITEM), Res->mitems);
      return DPS_ERROR;
    }
    bzero(Res->items + Res->nitems, (Res->mitems - Res->nitems) * sizeof(Res->items[0]));
  }
      
  if (Res->nitems > 0) {
    if (state->cmd == DPS_STACK_OR || state->cmd == DPS_STACK_AND || state->cmd == DPS_STACK_NEAR || state->cmd == DPS_STACK_ANYWORD) {
      if (Res->items[Res->nitems-1].cmd == DPS_STACK_AND || Res->items[Res->nitems-1].cmd == DPS_STACK_OR 
	  || Res->items[Res->nitems-1].cmd == DPS_STACK_NEAR ||  Res->items[Res->nitems-1].cmd == DPS_STACK_ANYWORD) {
	return DPS_OK;
      }
    }

    if ((Res->nitems > 0) && (state->cmd == DPS_STACK_WORD) 
	&& (
	    (Res->items[Res->nitems-1].cmd == DPS_STACK_WORD)
	    || (Res->items[Res->nitems-1].cmd == DPS_STACK_RIGHT)
	    || (Res->items[Res->nitems-1].cmd == DPS_STACK_PHRASE_RIGHT)
	    )) {
      Res->items[Res->nitems].cmd = DPS_STACK_OR;
      Res->items[Res->nitems].order = 0;
      Res->items[Res->nitems].origin = 0;
      Res->items[Res->nitems].count = 0;
      Res->items[Res->nitems].len = 0;
      Res->items[Res->nitems].crcword = 0;
      Res->items[Res->nitems].word = NULL;
      Res->items[Res->nitems].ulen = 0;
      Res->items[Res->nitems].uword = NULL;
      Res->items[Res->nitems].pbegin = NULL;
      Res->items[Res->nitems].db_pbegin = NULL;
      Res->items[Res->nitems].order_origin = 0;
      Res->items[Res->nitems].secno = state->secno[state->p_secno];
      Res->nitems++;
      Res->ncmds++;
#ifdef DEBUG_BOOL
  DpsLog(query, DPS_LOG_EXTRA, "1[%d].%x %c -- %s", 0, 0, item_type(DPS_STACK_OR), "<NULL>");
#endif
    }
    if ((Res->nitems > 0) && (state->cmd == DPS_STACK_LEFT) 
	&& (
	    (Res->items[Res->nitems-1].cmd == DPS_STACK_RIGHT)
	    || (Res->items[Res->nitems-1].cmd == DPS_STACK_PHRASE_RIGHT)
	    )) {
      Res->items[Res->nitems].cmd = state->add_cmd;
      Res->items[Res->nitems].order = 0;
      Res->items[Res->nitems].origin = 0;
      Res->items[Res->nitems].count = 0;
      Res->items[Res->nitems].len = 0;
      Res->items[Res->nitems].crcword = 0;
      Res->items[Res->nitems].word = NULL;
      Res->items[Res->nitems].ulen = 0;
      Res->items[Res->nitems].uword = NULL;
      Res->items[Res->nitems].pbegin = NULL;
      Res->items[Res->nitems].db_pbegin = NULL;
      Res->items[Res->nitems].order_origin = 0;
      Res->items[Res->nitems].secno = state->secno[state->p_secno];
      Res->nitems++;
      Res->ncmds++;
#ifdef DEBUG_BOOL
  DpsLog(query, DPS_LOG_EXTRA, "1[%d].%x %c -- %s", 0, 0, item_type(state->add_cmd), "<NULL>");
#endif
    }
  }

  Res->items[Res->nitems].cmd = state->cmd;
  Res->items[Res->nitems].order = state->order;
  Res->items[Res->nitems].order_inquery = state->order_inquery;
  Res->items[Res->nitems].origin = origin;
  Res->items[Res->nitems].count = 0;
  Res->items[Res->nitems].len = (word == NULL) ? 0 : dps_strlen(word);
  Res->items[Res->nitems].crcword = crcword;
  Res->items[Res->nitems].word = (word == NULL) ? NULL : DpsStrdup(word);
  Res->items[Res->nitems].ulen = wlen;
  Res->items[Res->nitems].uword = (uword == NULL) ? NULL : DpsUniDup(uword);
  Res->items[Res->nitems].pbegin = NULL;
  Res->items[Res->nitems].db_pbegin = NULL;
  Res->items[Res->nitems].order_origin = 0;
  Res->items[Res->nitems].wordnum = Res->nitems;
  Res->items[Res->nitems].secno = state->secno[state->p_secno];
  Res->nitems++;
  if (state->cmd != DPS_STACK_WORD) {
    Res->ncmds++;
  } else {
    Res->items[state->order].order_origin |= origin;
  if (state->order > Res->max_order) Res->max_order = state->order;
  if (state->order_inquery > Res->max_order_inquery) Res->max_order_inquery = state->order;
  }
/*  if ((state->cmd == DPS_STACK_WORD) && state->order > Res->max_order) Res->max_order = state->order;*/
#ifdef DEBUG_BOOL
  DpsLog(query, DPS_LOG_EXTRA, "1[%d,%d].%x %c -- %s", state->order, state->order_inquery, state->origin, item_type(state->cmd), 
	 (word == NULL) ? "<NULL>" : word);
#endif

  return DPS_OK;
}

void DpsStackItemFree(DPS_STACK_ITEM *item) {
  if (item == NULL) return;
  DPS_FREE(item->pbegin); item->pbegin = item->plast = item->pcur = NULL;
  DPS_FREE(item->db_pbegin); item->db_pbegin = NULL;
/*  item->count = 0;*/
/*  DPS_FREE(item->word); item->word = NULL;
  DPS_FREE(item->uword); item->uword = NULL;*/
  return;
}

/*
void DpsStackFree(DPS_RESULT *Res) {
  size_t i;
  for (i = 0; i < Res->nitems; i++) {
    DPS_FREE(Res->items[i].word);
    DPS_FREE(Res->items[i].uword);
    DPS_FREE(Res->items[i].pbegin);
  }
  DPS_FREE(Res->items);
}
*/


DPS_BOOLSTACK *DpsBoolStackInit(DPS_BOOLSTACK *s) {
	if(s == NULL) {
		s = (DPS_BOOLSTACK*)DpsMalloc(sizeof(DPS_BOOLSTACK));
		if (s == NULL) return NULL;
		bzero((void*)s, sizeof(*s));
		s->freeme = 1;
	}else{
		bzero((void*)s, sizeof(*s));
	}
	s->ncstack = 0;
	s->nastack = 0;
	s->mcstack = s->mastack = DPS_MAXSTACK;
	s->cstack = (int*)DpsMalloc(DPS_MAXSTACK * sizeof(int));
	if (s->cstack == NULL) { if (s->freeme) DPS_FREE(s); return NULL; }
	s->astack = (DPS_STACK_ITEM*)DpsMalloc(DPS_MAXSTACK * sizeof(DPS_STACK_ITEM));
	if (s->astack == NULL) {
	  DPS_FREE(s->cstack);
	  if (s->freeme) DPS_FREE(s); 
	  return NULL;
	}
	return s;
}


void DpsBoolStackFree(DPS_BOOLSTACK *s) {
	DPS_FREE(s->cstack); DPS_FREE(s->astack);
	if (s->freeme) DPS_FREE(s); 
}


static int TOPCMD(DPS_BOOLSTACK * s){
	if(s->ncstack)
		return(s->cstack[s->ncstack-1]);
	else
		return(DPS_STACK_BOT);
}

static DPS_STACK_ITEM* POPARG(DPS_BOOLSTACK * s) {
	if(s->nastack > 0) {
		s->nastack--;
		return(&s->astack[s->nastack]);
	}else{
	  return NULL; /*(DPS_STACK_BOT);*/
	}
}

static int POPCMD(DPS_BOOLSTACK * s){
	if(s->ncstack>0){
		s->ncstack--;
		return(s->cstack[s->ncstack]);
	}else{
		return(DPS_STACK_BOT);
	}
}

static int PUSHARG(DPS_BOOLSTACK * s, DPS_STACK_ITEM *arg) {
	s->astack[s->nastack] = *arg;
	s->nastack++;
	if (s->nastack >= s->mastack) {
	  s->mastack += DPS_MAXSTACK;
	  s->astack = (DPS_STACK_ITEM*)DpsRealloc(s->astack, s->mastack * sizeof(DPS_STACK_ITEM));
	  if (s->astack == NULL) return DPS_ERROR;
	}
	return DPS_OK;
}

static int PUSHCMD(DPS_BOOLSTACK * s, int arg) {
	s->cstack[s->ncstack] = arg;
	s->ncstack++;
	if (s->ncstack >= s->mcstack) {
	  s->mcstack += DPS_MAXSTACK;
	  s->cstack = (int*)DpsRealloc(s->cstack, s->mcstack * sizeof(int));
	  if (s->cstack == NULL) return DPS_ERROR;
	}
	return DPS_OK;
}

static int proceedOR(DPS_AGENT *query, DPS_STACK_ITEM *res, DPS_STACK_ITEM *x1, DPS_STACK_ITEM *x2) {

  res->pbegin = res->pcur = (DPS_URL_CRD_DB*)DpsMalloc((x1->count + x2->count + 1) * sizeof(DPS_URL_CRD_DB));
  if (res->pbegin == NULL) {
    DpsLog(query, DPS_LOG_ERROR, "Can't alloc %d bytes for %d results", (x1->count + x2->count + 1) * sizeof(DPS_URL_CRD_DB),
	   (x1->count + x2->count + 1));
    return DPS_ERROR;
  }
  x1->pcur = x1->pbegin; x1->plast = x1->pbegin + x1->count;
  x2->pcur = x2->pbegin; x2->plast = x2->pbegin + x2->count;
  while (x1->pcur < x1->plast && x2->pcur < x2->plast) {
    while((x1->pcur < x1->plast) && (DpsCmpUrlid(x1->pcur, x2->pcur) <= 0)) { 
      *res->pcur = *x1->pcur;
      res->pcur++; x1->pcur++;
    }
    {
      register DPS_STACK_ITEM *t = x1;
      x1 = x2; x2 = t;
    }
  }
  while (x1->pcur < x1->plast) {
    *res->pcur = *x1->pcur;
    res->pcur++; x1->pcur++;
  }
  while (x2->pcur < x2->plast) {
    *res->pcur = *x2->pcur;
    res->pcur++; x2->pcur++;
  }
  res->count = res->pcur - res->pbegin;
  return DPS_OK;
}

static int proceedSTOP(DPS_AGENT *query, DPS_STACK_ITEM *res, DPS_STACK_ITEM *x, DPS_STACK_ITEM *stop) {

  res->pbegin = res->pcur = (DPS_URL_CRD_DB*)DpsMalloc((x->count + stop->count + 1) * sizeof(DPS_URL_CRD_DB));
  if (res->pbegin == NULL) {
    DpsLog(query, DPS_LOG_ERROR, "Can't alloc %d bytes for %d results", (x->count + stop->count + 1) * sizeof(DPS_URL_CRD_DB),
	   (x->count + stop->count + 1));
    return DPS_ERROR;
  }
  x->pcur = x->pbegin; x->plast = x->pbegin + x->count;
  stop->pcur = stop->pbegin; stop->plast = stop->pbegin + stop->count;

  if (stop->pcur < stop->plast) {
    while (x->pcur < x->plast) {
      while (stop->pcur < stop->plast && stop->pcur->url_id < x->pcur->url_id) stop->pcur++;
      while (stop->pcur < stop->plast && DpsCmpUrlid(stop->pcur, x->pcur) <= 0) { 
	if (stop->pcur->url_id == x->pcur->url_id) {
	  *res->pcur = *stop->pcur; res->pcur++; 
	}
	stop->pcur++;
      }
      if (stop->pcur >= stop->plast) break;
      while (x->pcur < x->plast && DpsCmpUrlid(x->pcur, stop->pcur) <= 0) {
	*res->pcur = *x->pcur;
	res->pcur++; x->pcur++;
      }
    }
  }
  while (x->pcur < x->plast) {
    *res->pcur = *x->pcur;
    res->pcur++; x->pcur++;
  }
  res->count = res->pcur - res->pbegin;
/*
  if ((x->origin & DPS_WORD_ORIGIN_QUERY) == 0)
    if (stop->origin & (DPS_WORD_ORIGIN_STOP|DPS_WORD_ORIGIN_QUERY) == (DPS_WORD_ORIGIN_STOP|DPS_WORD_ORIGIN_QUERY)) res->origin = DPS_WORD_ORIGIN_STOP|DPS_WORD_ORIGIN_QUERY;
*/
  return DPS_OK;
}

#ifdef DEBUG_BOOL

static void printBoolRes(DPS_AGENT *query, DPS_STACK_ITEM *res) {
  size_t i;;
  for (i = 0; i < res->count; i++) {
    DpsLog(query, DPS_LOG_EXTRA, "url_id: %03x  coord: %08x", res->pbegin[i].url_id, res->pbegin[i].coord);
  }
}
#endif

/* Compute one operation and store result */
static int perform(DPS_AGENT *query, DPS_RESULT *Res, DPS_BOOLSTACK *s, int com) {
	DPS_STACK_ITEM res, *x1, *x2;
	int rc = DPS_OK, found, flag1;

	bzero(&res, sizeof(res));
	switch(com){
	        case DPS_STACK_PHRASE_LEFT:
		  x1 = POPARG(s);
		  if (x1 == NULL) {
		    bzero(&res, sizeof(res));
		    
		  } else {
		    res = *x1; /* FIXME: add checking ? */
		    if (res.order_from != res.order_to) {
		      DPS_URL_CRD_DB *w;
		      dps_uint4 *pos_real, *order_ideal, *order_real, *gap_ahead/*, *gap_back*/;
		      urlid_t curlid;
		      size_t nwords = res.order_to - res.order_from + 1, nonstop_words;
		      size_t p_cmp, p_ins;
		      res.plast = res.pbegin + res.count;
		      w = res.pcur = res.pchecked = res.pbegin;
		      if ((pos_real = (dps_uint4*)DpsMalloc(5 * nwords * sizeof(dps_uint4) + 1)) == NULL) {
			DpsLog(query, DPS_LOG_ERROR, "Can't alloc %d bytes %s:%d",(5 * nwords * sizeof(dps_uint4) + 1), __FILE__, __LINE__);
			return DPS_ERROR;
		      }
		      order_real = pos_real + nwords;
		      gap_ahead = order_real + nwords;
		      order_ideal = gap_ahead + nwords;
/*		    gap_back = order_ideal + nwords;*/
		      nonstop_words = 0;
		      { register size_t tt;
			for (tt = res.order_from; tt <= res.order_to; tt++) {
#ifdef DEBUG_BOOL
			  
#endif
			  if ((Res->items[tt].order_origin & DPS_WORD_ORIGIN_STOP) == 0) {
			    order_ideal[nonstop_words] = tt;
			    gap_ahead[nonstop_words] = (tt == res.order_to) ? 0 : 1;
			    { register size_t zz;
			      for (zz = tt + 1; zz <= res.order_to; zz++) {
				if (Res->items[zz].order_origin & DPS_WORD_ORIGIN_STOP) gap_ahead[nonstop_words]++;
				else break;
			      }
			    }
			    nonstop_words++;
			  }
			}
		      }

#ifdef DEBUG_BOOL
		      DpsLog(query, DPS_LOG_EXTRA, "nonstopwords: %d  nwords:%d", nonstop_words, nwords);
		      { register size_t tt;
			for (tt = 0; tt < nonstop_words; tt++) {
			  DpsLog(query, DPS_LOG_EXTRA, "%d:order_ideal:%d  gap_ahead:%d", tt, order_ideal[tt], gap_ahead[tt]);
			}
		      }
#endif
		      if (nonstop_words != 0) {
			while (res.pcur < res.plast) {
			  register size_t tt;
			  curlid = res.pcur->url_id;
			  found = 0;
			  p_ins = 0;
			  p_cmp = nwords - nonstop_words;
			  res.pchecked = res.pcur; /******* ? *****/
			  for (tt = 0; tt < nwords - nonstop_words; tt++) pos_real[tt] = 0;
			  for (tt = 0; (tt < nonstop_words) && (res.pcur < res.plast) && (res.pcur->url_id == curlid) ; tt++) {
			    order_real[p_ins] = Res->WWList.Word[DPS_WRDNUM(res.pcur->coord)].order;
			    pos_real[p_ins] = DPS_WRDPOS(res.pcur->coord);
			    while((res.pcur < res.plast) && (pos_real[p_ins] == DPS_WRDPOS(res.pcur->coord))) res.pcur++;
			    p_ins++; p_cmp++;
			    p_ins %= nwords;
			    p_cmp %= nwords;
			  }
			  if (tt == nonstop_words) {
/* [[[[[[ */
			    found = 1;
			    for (tt = 0; tt < nonstop_words; tt++) {
			      if (order_real[(p_cmp + tt) % nwords] != order_ideal[tt]) {
				found = 0; break;
			      }
			      if (gap_ahead[tt] && (tt + gap_ahead[tt] < nwords) && 
				  (pos_real[(p_cmp + tt) % nwords] + gap_ahead[tt] != pos_real[(p_cmp + tt + 1) % nwords])) {
				found = 0; break;
			      }
			    }
			    if (found) {
			      while((res.pchecked < res.pcur) /*&& (res.pchecked->url_id == curlid)*/) {
				*w = *res.pchecked;
				w++; res.pchecked++;
			      }
			      res.pcur = res.pchecked;
			    } else {
			      res.pchecked = res.pcur;
			    }
/* ]]]]]] */			    
			  }
			  while (/*(found == 0) &&*/ (res.pcur < res.plast) && (res.pcur->url_id == curlid)) {
			    order_real[p_ins] = Res->WWList.Word[DPS_WRDNUM(res.pcur->coord)].order;
			    pos_real[p_ins] = DPS_WRDPOS(res.pcur->coord);
			    while((res.pcur < res.plast) && (pos_real[p_ins] == DPS_WRDPOS(res.pcur->coord))) res.pcur++;
			    p_ins++; p_cmp++;
			    p_ins %= nwords;
			    p_cmp %= nwords;
/* [[[[[[ */
			    found = 1;
			    for (tt = 0; tt < nonstop_words; tt++) {
			      if (order_real[(p_cmp + tt) % nwords] != order_ideal[tt]) {
				found = 0; break;
			      }
			      if (gap_ahead[tt] && (tt + gap_ahead[tt] < nwords) && 
				  (pos_real[(p_cmp + tt) % nwords] + gap_ahead[tt] != pos_real[(p_cmp + tt + 1) % nwords])) {
				found = 0; break;
			      }
			    }
			    if (found) {
			      while((res.pchecked < res.pcur) /*&& (res.pchecked->url_id == curlid)*/) {
				*w = *res.pchecked;
				w++; res.pchecked++;
			      }
			      res.pcur = res.pchecked;
			    } else {
			      res.pchecked = res.pcur;
			    }
/* ]]]]]] */
			  }
			
			}
		      }
		      res.count = w - res.pbegin;
		      DPS_FREE(pos_real);
		    }
		  }
#if defined(DEBUG_BOOL)
		  DpsLog(query, DPS_LOG_INFO, "Perform <{%d}:%d:%d> ->{%d}", x1 ? x1->count : 0, x1 ? x1->order_from : 0, x1 ? x1->order_to : 0, res.count);
#endif
		  rc = PUSHARG(s, &res);
		  break;
		case DPS_STACK_OR:
			x1 = POPARG(s);
			x2 = POPARG(s);
			if (x2 == NULL || x1 == NULL) {
			  if (x1 != NULL) { res = *x1; x1 = NULL; }
			  if (x2 != NULL) { res = *x2; x2 = NULL; }
			} else {
#ifdef DEBUG_BOOL
/*			  printBoolRes(query, x1);*/
			  DpsLog(query, DPS_LOG_EXTRA, "^^^");
/*			  printBoolRes(query, x2);*/
#endif
			res.order_from = (x1->order_from <= x2->order_from) ? x1->order_from : x2->order_from;
			res.order_to = (x1->order_to >= x2->order_to) ? x1->order_to : x2->order_to;
			
			if (DPS_OK != proceedOR(query, &res, x1, x2)) return DPS_ERROR;
			DpsStackItemFree(x1); DpsStackItemFree(x2);
			res.count = res.pcur - res.pbegin;
			{ register size_t tt; int x1origin=0, x2origin=0;
			  for (tt = x1->order_from; tt <= x1->order_to; tt++) {
#ifdef DEBUG_BOOL
			    DpsLog(query, DPS_LOG_EXTRA, "\t\t\t\tx1order_origin[%d].%x ", tt, Res->items[tt].origin /*order_origin*/);
#endif
			    if (Res->items[tt].origin /*order_origin*/ & DPS_WORD_ORIGIN_STOP) {
			      x1origin = Res->items[tt].origin /*order_origin*/;
			      break;
			    }
			  }
			  for (tt = x2->order_from; tt <= x2->order_to; tt++) {
#ifdef DEBUG_BOOL
			    DpsLog(query, DPS_LOG_EXTRA, "\t\t\t\tx2order_origin[%d].%x ", tt, Res->items[tt].origin /*order_origin*/);
#endif
			    if (Res->items[tt].origin /*order_origin*/ & DPS_WORD_ORIGIN_STOP) {
			      x2origin = Res->items[tt].origin /*order_origin*/;
			      break;
			    }
			  }
			  
			  x1origin = x1->origin; x2origin = x2->origin;
#ifdef DEBUG_BOOL
			  DpsLog(query, DPS_LOG_EXTRA, "\t\t\t\tx1origin.%x x2origin.%x", x1origin, x2origin);
#endif


			  if (((x1origin & (DPS_WORD_ORIGIN_STOP|DPS_WORD_ORIGIN_QUERY))==(DPS_WORD_ORIGIN_STOP|DPS_WORD_ORIGIN_QUERY))) {
			    res.origin = x1origin;
			  } else
			  if (((x2origin & (DPS_WORD_ORIGIN_STOP|DPS_WORD_ORIGIN_QUERY))==(DPS_WORD_ORIGIN_STOP|DPS_WORD_ORIGIN_QUERY))) {
			    res.origin = x2origin;
			  } else
			  if (((x1origin & DPS_WORD_ORIGIN_STOP) && (x2origin & DPS_WORD_ORIGIN_STOP)) ||
			      ( (res.count == 0) && 
				((x1origin & DPS_WORD_ORIGIN_STOP) || (x2origin & DPS_WORD_ORIGIN_STOP))))
			    res.origin = DPS_WORD_ORIGIN_STOP;

			  if ((x1origin & DPS_WORD_ORIGIN_ACRONYM) && (x1origin & DPS_WORD_ORIGIN_ACRONYM)) 
			    res.origin |= DPS_WORD_ORIGIN_ACRONYM;
			}
			}
#if defined(DEBUG_BOOL)
			DpsLog(query, DPS_LOG_INFO, "Perform {%d}.%x | {%d}.%x -> {%d}.%x",
			       (x1) ? x1->count:-1, (x1)?x1->origin:-1, (x2)?x2->count : -1, (x2) ? x2->origin : -1, res.count, res.origin);
/*			printBoolRes(query, &res);*/
			DpsLog(query, DPS_LOG_EXTRA, "===");
#endif
			rc = PUSHARG(s, &res);
			break;
	        case DPS_STACK_NEAR:
			x1 = POPARG(s);
			x2 = POPARG(s);
			if (x2 == NULL || x1 == NULL) {
			  if (x1 != NULL) { res = *x1; x1 = NULL; }
			  if (x2 != NULL) { res = *x2; x2 = NULL; }
			} else {
			  res.order_from = (x1->order_from <= x2->order_from) ? x1->order_from : x2->order_from;
			  res.order_to = (x1->order_to >= x2->order_to) ? x1->order_to : x2->order_to;
			  if ((x1->origin & DPS_WORD_ORIGIN_STOP) && (x2->origin & DPS_WORD_ORIGIN_STOP) ) {
			    if (DPS_OK != proceedOR(query, &res, x1, x2)) return DPS_ERROR;
			    res.origin = DPS_WORD_ORIGIN_STOP;
			  } else if (x2->origin & DPS_WORD_ORIGIN_STOP) {
			    if (DPS_OK != proceedSTOP(query, &res, x1, x2)) return DPS_ERROR;
			  } else if (x1->origin & DPS_WORD_ORIGIN_STOP ) {
			    if (DPS_OK != proceedSTOP(query, &res, x2, x1)) return DPS_ERROR;
			  } else if (!((x1->cmd & DPS_STACK_WORD_NOT) && (x2->cmd & DPS_STACK_WORD_NOT))) {
#ifdef DEBUG_BOOL
/*			    printBoolRes(query, x1);*/
			    DpsLog(query, DPS_LOG_EXTRA, "^^^");
			    DpsLog(query, DPS_LOG_DEBUG, "x1.NOT: %d  x2.NOT: %d", x1->cmd & DPS_STACK_WORD_NOT, x2->cmd & DPS_STACK_WORD_NOT);
/*			    printBoolRes(query, x2);*/
#endif
			    res.pbegin = res.pcur = (DPS_URL_CRD_DB*)DpsMalloc((x1->count + x2->count + 1) * sizeof(DPS_URL_CRD_DB));
			    if (res.pbegin == NULL) return DPS_ERROR;
			    x1->pcur = x1->pbegin; x1->plast = x1->pbegin + x1->count;
			    x2->pcur = x2->pbegin; x2->plast = x2->pbegin + x2->count;
			    if (x1->cmd & DPS_STACK_WORD_NOT) {
			      register DPS_STACK_ITEM *t = x1;
			      x1 = x2; x2 = t;
			    }
			    while (x1->pcur < x1->plast && x2->pcur < x2->plast) {
			      while ((x2->pcur < x2->plast) && (x2->pcur->url_id < x1->pcur->url_id)) x2->pcur++;
			      if (x2->pcur >= x2->plast) break;
			      if (x2->pcur->url_id == x1->pcur->url_id) {
				dps_uint4 pos1 = DPS_WRDPOS(x1->pcur->coord);
				dps_uint4 pos2 = DPS_WRDPOS(x2->pcur->coord);
				register urlid_t curlid = x1->pcur->url_id;
				if (pos1 > pos2) { found = ((pos2 + 16) >= pos1);
				} else { found = ((pos1 + 16) >= pos2);
				}
				x1->pchecked = x1->pcur; x2->pchecked = x2->pcur;
				while (!found) {
				  if (x1->pchecked->coord <= x2->pchecked->coord) {
				    x1->pchecked++;
				    pos1 = DPS_WRDPOS(x1->pchecked->coord);
				    if (x1->pchecked >= x1->plast || x1->pchecked->url_id != curlid) break;
				  } else {
				    x2->pchecked++;
				    pos2 = DPS_WRDPOS(x2->pchecked->coord);
				    if (x2->pchecked >= x2->plast || x2->pchecked->url_id != curlid) break;
				  }
				  if (pos1 > pos2) { found = ((pos2 + 16) >= pos1);
				  } else { found = ((pos1 + 16) >= pos2);
				  }
				}
				if (x2->cmd & DPS_STACK_WORD_NOT || x1->cmd & DPS_STACK_WORD_NOT) found = !found;
				if (found) {

				  while (1 /* (x1->pcur < x1->plast) && (x2->pcur < x2->plast) && (x1->pcur->url_id == x2->pcur->url_id)*/) {
				    if (x1->pcur->coord <= x2->pcur->coord) {
				      *res.pcur = *x1->pcur;
				      res.pcur++; x1->pcur++; 
				      if (x1->pcur >= x1->plast || x1->pcur->url_id != curlid) break;
				    } else {
				      *res.pcur = *x2->pcur;
				      res.pcur++; x2->pcur++; 
				      if (x2->pcur >= x2->plast || x2->pcur->url_id != curlid) break;
				    }
				  }
				  while ((x1->pcur < x1->plast) && (x1->pcur->url_id == curlid)) {
				    *res.pcur = *x1->pcur;
				    res.pcur++; x1->pcur++;
				  }
				  while ((x2->pcur < x2->plast) && (x2->pcur->url_id == curlid)) {
				    *res.pcur = *x2->pcur;
				    res.pcur++; x2->pcur++;
				  }

				} else {
				  x1->pcur = x1->pchecked; x2->pcur = x2->pchecked;
				  while ((x1->pcur < x1->plast) && (x1->pcur->url_id == curlid)) x1->pcur++;
				  while ((x2->pcur < x2->plast) && (x2->pcur->url_id == curlid)) x2->pcur++;
				}
			      } else {
				register DPS_STACK_ITEM *t = x1;
				x1 = x2; x2 = t;
			      }
			    }
			  }
			}
			DpsStackItemFree(x1); DpsStackItemFree(x2);
			res.count = res.pcur - res.pbegin;
#if defined(DEBUG_BOOL)
			DpsLog(query, DPS_LOG_INFO, "Perform {%d}.%x NEAR {%d}.%x - > %d.%d", 
			       (x1)?x1->count:-1, (x1)?x1->origin:-1, (x2) ? x2->count : -1, (x2) ? x2->origin: - 1, res.count, res.origin);
/*			printBoolRes(query, &res);*/
			DpsLog(query, DPS_LOG_EXTRA, "===");
#endif
			rc = PUSHARG(s, &res);
			break;
	        case DPS_STACK_ANYWORD:
			x1 = POPARG(s);
			x2 = POPARG(s); flag1 = 0;
			if (x2 == NULL || x1 == NULL) {
			  if (x1 != NULL) { res = *x1; x1 = NULL; }
			  if (x2 != NULL) { res = *x2; x2 = NULL; }
			} else {
			  res.order_from = (x1->order_from <= x2->order_from) ? x1->order_from : x2->order_from;
			  res.order_to = (x1->order_to >= x2->order_to) ? x1->order_to : x2->order_to;
			  if ((x1->origin & DPS_WORD_ORIGIN_STOP) && (x2->origin & DPS_WORD_ORIGIN_STOP) ) {
			    if (DPS_OK != proceedOR(query, &res, x1, x2)) return DPS_ERROR;
			    res.origin = DPS_WORD_ORIGIN_STOP;
			  } else if (x2->origin & DPS_WORD_ORIGIN_STOP) {
			    if (DPS_OK != proceedSTOP(query, &res, x1, x2)) return DPS_ERROR;
			  } else if (x1->origin & DPS_WORD_ORIGIN_STOP ) {
			    if (DPS_OK != proceedSTOP(query, &res, x2, x1)) return DPS_ERROR;
			  } else if (!((x1->cmd & DPS_STACK_WORD_NOT) && (x2->cmd & DPS_STACK_WORD_NOT))) {
			    res.pbegin = res.pcur = (DPS_URL_CRD_DB*)DpsMalloc((x1->count + x2->count + 1) * sizeof(DPS_URL_CRD_DB));
			    if (res.pbegin == NULL) return DPS_ERROR;
			    x1->pcur = x1->pbegin; x1->plast = x1->pbegin + x1->count;
			    x2->pcur = x2->pbegin; x2->plast = x2->pbegin + x2->count;
			    if (x1->cmd & DPS_STACK_WORD_NOT) {
			      register DPS_STACK_ITEM *t = x1;
			      x1 = x2; x2 = t; flag1 = !flag1;
			    }
			    while (x1->pcur < x1->plast && x2->pcur < x2->plast) {
			      while ((x2->pcur < x2->plast) && (x2->pcur->url_id < x1->pcur->url_id)) x2->pcur++;
			      if (x2->pcur >= x2->plast) break;
			      if (x2->pcur->url_id == x1->pcur->url_id) {
				dps_int4 pos1 = (dps_int4)DPS_WRDPOS(x1->pcur->coord);
				dps_int4 pos2 = (dps_int4)DPS_WRDPOS(x2->pcur->coord);
				register urlid_t curlid = x1->pcur->url_id;
				found = ((flag1) ? ((pos1 + 2) == pos2) : ((pos2 + 2) == pos1));
				x1->pchecked = x1->pcur; x2->pchecked = x2->pcur;
				while ((!found) /*&& (x1->pchecked < x1->plast) && (x2->pchecked < x2->plast)*/ ) {
				  if (x1->pchecked->coord <= x2->pchecked->coord) {
				    x1->pchecked++;
				    if (x1->pchecked >= x1->plast || x1->pchecked->url_id != curlid) break;
				    pos1 = (dps_int4)DPS_WRDPOS(x1->pchecked->coord);
				  } else {
				    x2->pchecked++;
				    if (x2->pchecked >= x2->plast || x2->pchecked->url_id != curlid) break;
				    pos2 = (dps_int4)DPS_WRDPOS(x1->pchecked->coord);
				  }
				  found = ((flag1) ? ((pos1 + 2) == pos2) : ((pos2 + 2) == pos1));
				}
				if (x2->cmd & DPS_STACK_WORD_NOT || x1->cmd & DPS_STACK_WORD_NOT) found = !found;
				if (found) {
				  while (1 /* (x1->pcur < x1->plast) && (x2->pcur < x2->plast) && (x1->pcur->url_id == x2->pcur->url_id) && (x1->pcur->url_id == curlid) */) {
				    if (x1->pcur->coord <= x2->pcur->coord) {
				      *res.pcur = *x1->pcur;
				      res.pcur++; x1->pcur++;
				      if (x1->pcur >= x1->plast || x1->pcur->url_id != curlid) break;
				    } else {
				      *res.pcur = *x2->pcur;
				      res.pcur++; x2->pcur++;
				      if (x2->pcur >= x2->plast || x2->pcur->url_id != curlid) break;
				    }
				  }
				  while ((x1->pcur < x1->plast) && (x1->pcur->url_id == curlid)) {
				    *res.pcur = *x1->pcur;
				    res.pcur++; x1->pcur++;
				  }
				  while ((x2->pcur < x2->plast) && (x2->pcur->url_id == curlid)) {
				    *res.pcur = *x2->pcur;
				    res.pcur++; x2->pcur++;
				  }
				} else {
				  x1->pcur = x1->pchecked; x2->pcur = x2->pchecked;
				  while ((x1->pcur < x1->plast) && (x1->pcur->url_id == curlid)) x1->pcur++;
				  while ((x2->pcur < x2->plast) && (x2->pcur->url_id == curlid)) x2->pcur++;
				}
			      } else {
				register DPS_STACK_ITEM *t = x1;
				x1 = x2; x2 = t; flag1 = !flag1;
			      }
			    }
			  }
			}
			DpsStackItemFree(x1); DpsStackItemFree(x2);
			res.count = res.pcur - res.pbegin;
#if defined(DEBUG_BOOL)
			DpsLog(query, DPS_LOG_INFO, "Perform {%d} ANYWORD {%d} - > %d", (x1) ? x1->count : -1, (x2) ? x2->count : -1, res.count);
#endif
			rc = PUSHARG(s, &res);
			break;
		case DPS_STACK_AND:
			x1 = POPARG(s);
			x2 = POPARG(s);
			if (x2 == NULL || x1 == NULL) {
			  if (x1 != NULL) { res = *x1; x1 = NULL; }
			  if (x2 != NULL) { res = *x2; x2 = NULL; }
			} else {
#ifdef DEBUG_BOOL
/*			  printBoolRes(query, x1);*/
			  DpsLog(query, DPS_LOG_EXTRA, "^^^");
/*			  printBoolRes(query, x2);*/
#endif
			  res.order_from = (x1->order_from <= x2->order_from) ? x1->order_from : x2->order_from;
			  res.order_to = (x1->order_to >= x2->order_to) ? x1->order_to : x2->order_to;
			  if ((x1->origin & DPS_WORD_ORIGIN_STOP) && (x2->origin & DPS_WORD_ORIGIN_STOP) ) {
			    if (DPS_OK != proceedOR(query, &res, x1, x2)) return DPS_ERROR;
			    res.origin = DPS_WORD_ORIGIN_STOP;
			  } else if (x2->origin & DPS_WORD_ORIGIN_STOP) {
			    if (DPS_OK != proceedSTOP(query, &res, x1, x2)) return DPS_ERROR;
			  } else if (x1->origin & DPS_WORD_ORIGIN_STOP ) {
			    if (DPS_OK != proceedSTOP(query, &res, x2, x1)) return DPS_ERROR;
			  } else if (!((x1->cmd & DPS_STACK_WORD_NOT) && (x2->cmd & DPS_STACK_WORD_NOT))) {
			    res.pbegin = res.pcur = (DPS_URL_CRD_DB*)DpsMalloc((x1->count + x2->count + 1) * sizeof(DPS_URL_CRD_DB));
			    if (res.pbegin == NULL) return DPS_ERROR;
			    x1->pcur = x1->pbegin; x1->plast = x1->pbegin + x1->count;
			    x2->pcur = x2->pbegin; x2->plast = x2->pbegin + x2->count;
			    if (x1->cmd & DPS_STACK_WORD_NOT) {
			      register DPS_STACK_ITEM *t = x1;
			      x1 = x2; x2 = t;
			    }
			    if (x2->cmd & DPS_STACK_WORD_NOT) {
			      while (x1->pcur < x1->plast && x2->pcur < x2->plast) {
				while ((x1->pcur < x1->plast) && (x1->pcur->url_id < x2->pcur->url_id)) {
				  *res.pcur = *x1->pcur;
				  res.pcur++; x1->pcur++;
				}
				while ((x2->pcur < x2->plast) && (x2->pcur->url_id < x1->pcur->url_id)) x2->pcur++;
			    
				if (x2->pcur->url_id == x1->pcur->url_id) {
				  register urlid_t curlid = x1->pcur->url_id;
				  while ((x1->pcur < x1->plast) && (x1->pcur->url_id == curlid)) x1->pcur++;
				  while ((x2->pcur < x2->plast) && (x2->pcur->url_id == curlid)) x2->pcur++;
				}
			      }
			      while (x1->pcur < x1->plast) {
				*res.pcur = *x1->pcur;
				res.pcur++; x1->pcur++;
			      }
			    } else {
#if 0
			      {
				DPS_URL_CRD_DB *w;
				for (w = x1->pcur; w < x1->plast; w++) {
				  fprintf(stderr, "x1.url_id:%d  .coord:%d\n", w->url_id, w->coord);
				}
				for (w = x2->pcur; w < x2->plast; w++) {
				  fprintf(stderr, "x2.url_id:%d  .coord:%d\n", w->url_id, w->coord);
				}
			      }
#endif

			      while (x1->pcur < x1->plast && x2->pcur < x2->plast) {
				while ((x2->pcur < x2->plast) && (x2->pcur->url_id < x1->pcur->url_id)) x2->pcur++;
				if (x2->pcur >= x2->plast) break;
				if (x2->pcur->url_id == x1->pcur->url_id) {
				  register urlid_t curlid = x1->pcur->url_id;
				  while ((x1->pcur < x1->plast) && (x2->pcur < x2->plast) && (x1->pcur->url_id == x2->pcur->url_id)) {
				    if (x1->pcur->coord <= x2->pcur->coord) {
				      *res.pcur = *x1->pcur;
				      res.pcur++; x1->pcur++;
				    } else {
				      *res.pcur = *x2->pcur;
				      res.pcur++; x2->pcur++;
				    }
				  }
				  while ((x1->pcur < x1->plast) && (x1->pcur->url_id == curlid)) {
				    *res.pcur = *x1->pcur;
				    res.pcur++; x1->pcur++;
				  }
				  while ((x2->pcur < x2->plast) && (x2->pcur->url_id == curlid)) {
				    *res.pcur = *x2->pcur;
				    res.pcur++; x2->pcur++;
				  }
				} else {
				  register DPS_STACK_ITEM *t = x1;
				  x1 = x2; x2 = t;
				}
			      }
			    }
			  }
			}
			DpsStackItemFree(x1); DpsStackItemFree(x2);
			res.count = res.pcur - res.pbegin;
#if defined(DEBUG_BOOL)
#if 0
			{
			  DPS_URL_CRD_DB *w = res.pbegin;
			  size_t q;
			  for (q = 0; q < res.count; q++) {
			    fprintf(stderr, "res.url_id:%d  .coord:%d\n", w[q].url_id, w[q].coord);
			  }
			}
#endif
			DpsLog(query, DPS_LOG_INFO, "Perform {%d}.%x & {%d}.%x - > {%d}.%x", 
	       (x1) ? x1->count : -1, (x1) ? x1->origin : -1, (x2) ? x2->count : -1 , (x2) ? x2->origin : -1, res.count, res.origin);
/*			printBoolRes(query, &res);*/
			DpsLog(query, DPS_LOG_EXTRA, "===");
#endif
			rc = PUSHARG(s, &res);
			break;
		case DPS_STACK_NOT:
		        x1 = POPARG(s);
			/* res = x1 ? 0 : 1; */
			if (x1 != NULL) {
#ifdef DEBUG_BOOL
/*			  printBoolRes(query, x1);*/
			  DpsLog(query, DPS_LOG_EXTRA, "^^^");
#endif
			  x1->cmd ^= DPS_STACK_WORD_NOT;
			  rc = PUSHARG(s, x1);
			}
#if defined(DEBUG_BOOL)
			DpsLog(query, DPS_LOG_INFO, "Perform ~ {%d}", (x1) ? x1->count : -1);
/*			printBoolRes(query, &x1);*/
			DpsLog(query, DPS_LOG_EXTRA, "===");
#endif
			break;
	}
	return rc;
}

void DpsWWLBoolItems(DPS_RESULT *Res) {
  DPS_WIDEWORD Word;
  DPS_STACK_ITEM *items = Res->items;
  size_t i;
  if (Res->WWList.nwords == 0) {
    bzero(&Word, sizeof(Word));
    for (i = 0; i < Res->nitems; i++) {
      if (items[i].cmd != DPS_STACK_WORD) continue;
      Word.order = items[i].order;
      Word.order_inquery = items[i].order_inquery;
      Word.count = items[i].count;
      Word.crcword = items[i].crcword;
      Word.word = items[i].word;
      Word.uword = items[i].uword;
      Word.origin = items[i].origin;
      items[i].wordnum = DpsWideWordListAdd(&Res->WWList, &Word, DPS_WWL_LOOSE);
      items[i].count = 0;
    }
  } else {
    for (i = 0; i < Res->nitems; i++) {
      if (items[i].cmd != DPS_STACK_WORD) continue;
      Res->WWList.Word[items[i].wordnum].count += items[i].count;
      items[i].count = 0;
    }
  }
}


/* Main function to calculate items sequence */
int DpsCalcBoolItems(DPS_AGENT *query, DPS_RESULT *Res) {
  DPS_BOOLSTACK *s = DpsBoolStackInit(NULL);
  DPS_STACK_ITEM *items = Res->items, *res = NULL;
  DPS_WIDEWORD Word;
  size_t nitems = Res->nitems;
  size_t i, j;
  int first_time = (Res->WWList.nwords == 0);

  if (s == NULL) return DPS_STACK_ERR;
  if (nitems == 0) {
    Res->CoordList.Coords = Res->items[0].pbegin;
    Res->items[0].pbegin = NULL;
    Res->items[0].count = 0;
/*    Res->CoordList.ncoords = Res->items[0]->count;*/
    DpsBoolStackFree(s);
    return DPS_OK;
  }

  bzero(&Word, sizeof(Word));

  for (i = 0; i < nitems; i++) {
    if (items[i].cmd != DPS_STACK_WORD) continue;
    if ((items[i].pbegin == NULL) && ((items[i].origin & DPS_WORD_ORIGIN_STOP) == 0)) {
      for(j = 0; j < i; j++) 
	  if (items[j].crcword == items[i].crcword && (items[j].pbegin != NULL) && (items[j].secno == 0 || items[j].secno == items[i].secno)) break;
      if (j < i) {
	items[i].count = items[j].count;
	items[i].pbegin = (DPS_URL_CRD_DB*)DpsMalloc((items[j].count + 1) * sizeof(DPS_URL_CRD_DB));
	if (items[i].pbegin == NULL) {
	  DpsLog(query, DPS_LOG_ERROR, "Can't alloc %d bytes %s:%d", (items[j].count + 1) * sizeof(DPS_URL_CRD_DB), __FILE__, __LINE__);
	  DpsBoolStackFree(s);
	  return DPS_STACK_ERR;
	}
	if (items[i].secno == 0) {
	  register size_t z;
	  for (z = 0; z < items[i].count; z++) {
	    items[i].pbegin[z] = items[j].pbegin[z];
	    items[i].pbegin[z].coord &= 0xFFFFFF00;
	    items[i].pbegin[z].coord += (items[i].wordnum & 0xFF);
	  }
	} else {
	  register size_t z, n = 0;
	  for (z = 0; z < items[i].count; z++) {
	    if (DPS_WRDSEC(items[j].pbegin[z].coord) == items[i].secno) {
	      items[i].pbegin[n] = items[j].pbegin[z];
	      items[i].pbegin[n].coord &= 0xFFFFFF00;
	      items[i].pbegin[n].coord += (items[i].wordnum & 0xFF);
	      n++;
	    }
	  }
	  items[i].count = n;
	}
      }
    }
    if (first_time) {
      Word.order = items[i].order;
      Word.order_inquery = items[i].order_inquery;
      Word.count = items[i].count;
      Word.crcword = items[i].crcword;
      Word.word = items[i].word;
      Word.uword = items[i].uword;
      Word.origin = items[i].origin;
      DpsWideWordListAdd(&Res->WWList, &Word, DPS_WWL_LOOSE);
    }
  }

#ifdef DEBUG_BOOL
  DpsLog(query, DPS_LOG_EXTRA, "--------");
  for(i=0;i<nitems;i++){
    DpsLog(query, DPS_LOG_EXTRA, "[%d].%d %c : %d -- %s", 
	   i, items[i].wordnum, item_type(items[i].cmd), items[i].count, (items[i].word == NULL) ? "<NULL>" : items[i].word);
  }
  DpsLog(query, DPS_LOG_EXTRA, "--------");
#endif

  for(i = 0; i < nitems; i++) {
    int c;

#ifdef DEBUG_BOOL
    DpsLog(query, DPS_LOG_EXTRA,
	   ".[%d].%d %c : %d -- %s, (order_origin:%d)", i, items[i].wordnum, item_type(items[i].cmd), items[i].count, (items[i].word == NULL) ? "<NULL>" : items[i].word, items[i].order_origin);
#endif

    switch(c = items[i].cmd) {
    case DPS_STACK_RIGHT:
      /* Perform till LEFT bracket */
      while((TOPCMD(s) != DPS_STACK_LEFT) && (TOPCMD(s) != DPS_STACK_BOT))
	if (DPS_OK != perform(query, Res, s, POPCMD(s))) {
	  DpsBoolStackFree(s);
	  return DPS_STACK_ERR;
	}
      /* Pop LEFT bracket itself */
      if(TOPCMD(s) == DPS_STACK_LEFT)
	POPCMD(s);
      break;
    case DPS_STACK_OR:
    case DPS_STACK_AND:
    case DPS_STACK_NEAR:
    case DPS_STACK_ANYWORD:
      if (s->nastack > 1)
	while(c <= TOPCMD(s)) {
	  if (DPS_OK != perform(query, Res, s, POPCMD(s))) {
	    DpsBoolStackFree(s);
	    return DPS_STACK_ERR;
	  }
	}
      /* IMPORTANT! No break here! That's OK*/
      /* Так надо ! */
    case DPS_STACK_LEFT:
    case DPS_STACK_PHRASE_LEFT:
    case DPS_STACK_NOT:
      if (PUSHCMD(s,c) != DPS_OK) {
	DpsBoolStackFree(s);
	return DPS_STACK_ERR;
      }
      break;
    case DPS_STACK_PHRASE_RIGHT:
      /* perform till RIGHT phrase quote */
      while((TOPCMD(s) != DPS_STACK_PHRASE_LEFT) && (TOPCMD(s) != DPS_STACK_BOT))
	if (DPS_OK != perform(query, Res, s, POPCMD(s))) {
	  DpsBoolStackFree(s);
	  return DPS_STACK_ERR;
	}
      if (TOPCMD(s) == DPS_STACK_PHRASE_LEFT) perform(query, Res, s, POPCMD(s));
#ifdef DEBUG_BOOL
      DpsLog(query, DPS_LOG_EXTRA, "[%d] %c", i, item_type(items[i].cmd));
#endif
      break;
    case DPS_STACK_WORD:
      items[i].order_from = items[i].order_to = items[i].order;
    default:
      if (DPS_OK != PUSHARG(s, &items[i])) {
	DpsBoolStackFree(s);
	return DPS_STACK_ERR;
      }
      items[i].pbegin = items[i].plast = NULL;
      items[i].db_pbegin = items[i].db_plast = NULL;
/*      items[i].count = 0;*/

/*				DpsStackItemFree(&items[i]);*/
/*			        if (DPS_OK != PUSHARG(s, (count[items[i].order]) ? 1UL : 0UL)) {
				  DpsBoolStackFree(s);
				  return DPS_STACK_ERR;
				}*/
      break;
    }
  }
  while(TOPCMD(s) != DPS_STACK_BOT) {
    if (DPS_OK != perform(query, Res, s, POPCMD(s))) {
      DpsBoolStackFree(s);
      return DPS_STACK_ERR;
    }
  }
  res = POPARG(s);
  if (res != NULL) {
    Res->CoordList.Coords = res->pbegin;
    Res->CoordList.ncoords = res->count;
    res->pbegin = res->plast = NULL;
/*    res->count = 0;*/
    DpsStackItemFree(res);
  }
#ifdef DEBUG_BOOL
  DpsLog(query, DPS_LOG_EXTRA, "result: %x", res);
#endif
  DpsBoolStackFree(s);
  return DPS_OK;
}
