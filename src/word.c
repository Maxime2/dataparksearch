/* Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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
#include "dps_word.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>


#define WSIZE		1024
#define BSIZE		10

int DpsWordListAddFantom(DPS_DOCUMENT *Doc, DPS_WORD *word, int section) {
	/* Realloc memory when required  */
	if(Doc->Words.nwords>=Doc->Words.mwords){
		Doc->Words.mwords+=WSIZE;
		Doc->Words.Word=(DPS_WORD *)DpsRealloc(Doc->Words.Word,Doc->Words.mwords*sizeof(DPS_WORD));
		if (Doc->Words.Word == NULL) {
		  Doc->Words.mwords = Doc->Words.nwords = 0;
		  return DPS_ERROR;
		}
	}
	
	/* Add new word */
	Doc->Words.Word[Doc->Words.nwords].uword = DpsUniDup(word->uword);
	Doc->Words.Word[Doc->Words.nwords].coord = DPS_WRDCOORDL(Doc->Words.wordpos, section, word->ulen);
	Doc->Words.Word[Doc->Words.nwords].ulen = word->ulen;

	Doc->Words.nwords++;
	
	return(DPS_OK);
}

int DpsWordListAdd(DPS_DOCUMENT * Doc, DPS_WORD *word, int section) {

	Doc->Words.wordpos++;
	return DpsWordListAddFantom(Doc, word, section);
}

int DpsWordListFree(DPS_WORDLIST * List){
	size_t i;
	for(i=0;i<List->nwords;i++) {
		DPS_FREE(List->Word[i].uword);
	}
	List->nwords=0;
	List->swords=0;
	DPS_FREE(List->Word);
	return(0);
}

DPS_WORDLIST * DpsWordListInit(DPS_WORDLIST * List){
	bzero((void*)List, sizeof(*List));
	return(List);
}


int DpsWordCmp(const void *p1, const void *p2) {
  const DPS_WORD *w1 = (DPS_WORD*)p1;
  const DPS_WORD *w2 = (DPS_WORD*)p2;
  if (w1->ulen < w2->ulen) return -1;
  if (w1->ulen > w2->ulen) return 1;
  
  return DpsUniStrCmp(w1->uword, w2->uword); /* we assume all words are in lower case in the list here */
}

int DpsWordListSort(DPS_WORDLIST *List) {
  if (List->nwords > 1) DpsSort(List->Word, List->nwords, sizeof(DPS_WORD), (qsort_cmp)DpsWordCmp);
}


DPS_WIDEWORDLIST * DpsWideWordListInit(DPS_WIDEWORDLIST * List){
	bzero((void*)List, sizeof(*List));
	return(List);
}


size_t DpsWideWordListAdd(DPS_WIDEWORDLIST * List,DPS_WIDEWORD * Word, int strictness) {
        size_t i;

/*	if ((Word->origin != DPS_WORD_ORIGIN_QUERY) && (Word->origin != DPS_WORD_ORIGIN_STOP))*/ /* FIXME: Was commented, why ? */
	if (strictness == DPS_WWL_STRICT || (Word->origin & (DPS_WORD_ORIGIN_QUERY | DPS_WORD_ORIGIN_STOP)) == 0) {
	  for (i = 0; i < List->nwords; i++) {
	    if (List->Word[i].len == Word->len && (DpsUniStrCmp(List->Word[i].uword, Word->uword) == 0)) {
	      List->Word[i].count += Word->count;
	      if (Word->origin & DPS_WORD_ORIGIN_QUERY) {
		if ((List->Word[i].origin & DPS_WORD_ORIGIN_STOP) == 0) {
		  List->Word[i].order = Word->order;
		  List->nuniq++;
/*		  List->Word[i].origin |= Word->origin;*/
		  List->Word[i].origin = Word->origin;
		}
	      } else if (Word->origin & DPS_WORD_ORIGIN_STOP) {
		List->Word[i].origin |= Word->origin;
	      }

/*	    List->Word[i].order = Word->order;*/
	      return i; 
	    }
	  }
	}
	
	/* Realloc memory */
	List->Word=(DPS_WIDEWORD*)DpsRealloc(List->Word,sizeof(*(List->Word))*(List->nwords+1));
	if (List->Word == NULL) return DPS_ERROR;
	bzero((void*)&List->Word[List->nwords], sizeof(*(List->Word)));
	
	/* Copy data */
	List->Word[List->nwords].order = Word->order;
	List->Word[List->nwords].order_inquery = Word->order_inquery;
	List->Word[List->nwords].count=Word->count;
	List->Word[List->nwords].crcword=Word->crcword;
	List->Word[List->nwords].word = Word->word ? (char*)DpsStrdup(Word->word) : NULL;
	List->Word[List->nwords].uword=Word->uword?DpsUniDup(Word->uword):NULL;
	List->Word[List->nwords].origin = Word->origin;
	List->Word[List->nwords].len = dps_strlen(DPS_NULL2EMPTY(List->Word[List->nwords].word));
	List->Word[List->nwords].ulen = Word->uword ? DpsUniLen(List->Word[List->nwords].uword) : 0;
	if (List->Word[List->nwords].ulen > List->maxulen) List->maxulen = List->Word[List->nwords].ulen;
	List->nwords++;
/*	if (Word->origin & (DPS_WORD_ORIGIN_QUERY | DPS_WORD_ORIGIN_STOP)) List->nuniq++;*/
	if (Word->origin & DPS_WORD_ORIGIN_QUERY) List->nuniq++;

	return(List->nwords - 1);
}

void DpsWideWordListFree(DPS_WIDEWORDLIST * List){
	size_t i;
	for(i=0;i<List->nwords;i++){
		DPS_FREE(List->Word[i].word);
		DPS_FREE(List->Word[i].uword);
	}
	DPS_FREE(List->Word);
	DpsWideWordListInit(List);
}

