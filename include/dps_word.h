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

#ifndef _DPS_WORD_H
#define _DPS_WORD_H

enum {
  DPS_WWL_LOOSE  = 0,
  DPS_WWL_STRICT = 1
};

extern int DpsWordListAdd(DPS_DOCUMENT* Doc, DPS_WORD *word, int where);
extern int DpsWordListAddFantom(DPS_DOCUMENT* Doc, DPS_WORD *word, int where);
extern int DpsWordListFree(DPS_WORDLIST * List);
extern DPS_WORDLIST * DpsWordListInit(DPS_WORDLIST * List);
extern int DpsWordListSort(DPS_WORDLIST * List);
extern int DpsWordCmp(const void *p1, const void *p2);


extern DPS_WIDEWORDLIST * DpsWideWordListInit(DPS_WIDEWORDLIST * List);
extern size_t DpsWideWordListAdd(DPS_WIDEWORDLIST *List, DPS_WIDEWORD *Word, int strictness);
extern void DpsWideWordListFree(DPS_WIDEWORDLIST * List);
/*extern int DpsWideWordListBuildPi(DPS_WIDEWORDLIST *List);*/

extern int DpsResCollectWords(DPS_RESULT *, DPS_URLCRDLISTLIST *);

#endif
