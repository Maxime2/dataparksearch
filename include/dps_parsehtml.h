/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 Datapark corp. All rights reserved.
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

#ifndef _DPS_PARSE_HTML_H
#define _DPS_PARSE_HTML_H

#include "dps_charsetutils.h"

/* HTML parser states */
#define DPS_HTML_TAG	1
#define DPS_HTML_TXT	2
#define DPS_HTML_COM	3

typedef struct dps_langstack_struct {
	char *tag;
	char lang[4];
	struct dps_langstack_struct *prev;
} DPS_LANGSTACK;

#define ISTAG(n,s)	(!strncasecmp(tag->toks[n].name,s,tag->toks[n].nlen)&&(tag->toks[n].nlen==strlen(s)))

extern int    DpsHTMLParse(DPS_AGENT*, DPS_DOCUMENT*);
extern int DpsHTMLParseBuf(DPS_AGENT*, DPS_DOCUMENT*, const char *, const char *);
extern int DpsParseURLText(DPS_AGENT*, DPS_DOCUMENT*);
extern int    DpsParseText(DPS_AGENT*, DPS_DOCUMENT*);
extern int DpsParseHeaders(DPS_AGENT*, DPS_DOCUMENT*);
extern int DpsPrepareWords(DPS_AGENT*, DPS_DOCUMENT*);
extern int DpsPrepareItem(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_TEXTITEM *Item, dpsunicode_t *ustr, dpsunicode_t *Ustr, 
			  const char *content_lang, size_t *indexed_size, size_t *indexed_limit, 
			  size_t max_word_len, size_t min_word_len, int crossec
#ifdef HAVE_ASPELL
			  , int have_speller, AspellSpeller *speller, DPS_DSTR *suggest
#endif
			  );

extern const char * DpsHTMLToken(const char * s, const char ** lt,DPS_HTMLTOK *t);
extern int DpsHTMLParseTag(DPS_AGENT *Indexer, DPS_HTMLTOK * tag, DPS_DOCUMENT * Doc, DPS_VAR *CrosSec);
extern void DpsHTMLTOKInit(DPS_HTMLTOK *t);
extern void DpsHTMLTOKFree(DPS_HTMLTOK *t);
extern int dps_itemptr_cmp(DPS_TEXTITEM **p1, DPS_TEXTITEM **p2);

#endif
