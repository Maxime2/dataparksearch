/* Copyright (C) 2005-2012 Datapark corp. All rights reserved.

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

#ifndef DPS_SEA_H
#define DPS_SEA_H

#include "dps_uniconv.h"
#include "dps_utils.h"

typedef struct {
  dpsunicode_t *sentence;
  DPS_LANGMAP  LangMap;
  double       pas;
  double       Oi;
  double       di;
  size_t       len;
  size_t       order; /* original order of the sentence in the document */
} DPS_SENTENCE;

typedef struct {
  size_t nitems, mitems;
  DPS_SENTENCE *Sent;
} DPS_SENTENCELIST;


extern int DpsSEAMake(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DSTR *excerpt,  
		      const char *content_lang, size_t *indexed_size, size_t *indexed_limit, 
		      size_t max_word_len, size_t min_word_len, int crossec, int seasec
#ifdef HAVE_ASPELL
		      , int have_speller, AspellSpeller *speller
#endif
		      );


#endif
