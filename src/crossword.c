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
#include "dps_crossword.h"
#include "dps_unicode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#define RESORT_WORDS	256
#define WSIZE		1024


int DpsCrossListAddFantom(DPS_DOCUMENT * Doc, DPS_CROSSWORD * CrossWord) {
	CrossWord->pos=Doc->CrossWords.wordpos;

	/* Realloc memory when required  */
	if(Doc->CrossWords.ncrosswords>=Doc->CrossWords.mcrosswords){
		Doc->CrossWords.mcrosswords+=WSIZE;
		Doc->CrossWords.CrossWord=(DPS_CROSSWORD *)DpsRealloc(Doc->CrossWords.CrossWord,
								      Doc->CrossWords.mcrosswords*sizeof(DPS_CROSSWORD));
		if (Doc->CrossWords.CrossWord == NULL) {
		  Doc->CrossWords.ncrosswords = Doc->CrossWords.mcrosswords = 0;
		  return DPS_ERROR;
		}
	}

	/* Add new word */
/*	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].word = (char*)DpsStrdup(CrossWord->word);*/
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].uword = DpsUniDup(CrossWord->uword);
/*	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].len = CrossWord->len;*/
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].ulen = CrossWord->ulen;
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].url = (char*)DpsStrdup(CrossWord->url);
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].weight = CrossWord->weight;
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].pos = Doc->CrossWords.wordpos; /*CrossWord->pos;*/
	Doc->CrossWords.ncrosswords++;
	return DPS_OK;
}

/* This function adds a normalized word form(s) into list using Ispell */
int DpsCrossListAdd(DPS_DOCUMENT * Doc,DPS_CROSSWORD * CrossWord){

	Doc->CrossWords.wordpos++;
	DpsCrossListAddFantom(Doc, CrossWord);
	return(0);
}

void DpsCrossListFree(DPS_CROSSLIST * CrossList) {
	size_t i;
	for(i=0;i<CrossList->ncrosswords;i++){
/*		DPS_FREE(CrossList->CrossWord[i].word);*/
		DPS_FREE(CrossList->CrossWord[i].uword);
		DPS_FREE(CrossList->CrossWord[i].url);
	}
	CrossList->ncrosswords=0;
	CrossList->mcrosswords=0;
	DPS_FREE(CrossList->CrossWord);
	return;
}

DPS_CROSSLIST * DpsCrossListInit(DPS_CROSSLIST * List){
	bzero((void*)List, sizeof(*List));
	return(List);
}


int DpsCrossCmp(const void *p1, const void *p2) {
  const DPS_CROSSWORD *w1 = (DPS_CROSSWORD*)p1;
  const DPS_CROSSWORD *w2 = (DPS_CROSSWORD*)p2;
  if (w1->ulen < w2->ulen) return -1;
  if (w1->ulen > w2->ulen) return 1;
  
  return DpsUniStrCaseCmp(w1->uword, w2->uword);
}

int DpsCrossListSort(DPS_CROSSLIST *List) {
  if (List->ncrosswords > 1) DpsSort(List->CrossWord, List->ncrosswords, sizeof(DPS_CROSSWORD), (qsort_cmp)DpsCrossCmp);
}
