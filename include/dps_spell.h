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

#ifndef _DPS_SPELL_H
#define _DPS_SPELL_H

#include "dps_services.h"

typedef struct {
  DPS_SPELL **cur;
/*  char buff[4096 - sizeof(DPS_SPELL**)];*/
  size_t nspell;
} DPS_PSPELL;

extern __C_LINK DPS_SPELL ** __DPSCALL DpsNormalizeWord(DPS_AGENT *Indexer, DPS_WIDEWORD *word, DPS_PSPELL *FZ);
extern __C_LINK DPS_WIDEWORDLIST * __DPSCALL DpsAllForms(DPS_AGENT *Indexer, DPS_WIDEWORD *word);
extern __C_LINK int __DPSCALL DpsImportAffixes(DPS_ENV *Conf, const char *lang, const char *charset, const char *filename);
extern __C_LINK int __DPSCALL DpsImportDictionary(DPS_AGENT *query, const char *lang, const char *charset, 
						  const char *filename, int skip_noflag, const char *first_letters);
extern __C_LINK int __DPSCALL DpsImportQuffixes(DPS_ENV *Conf, const char *lang, const char *charset, const char *filename);

extern __C_LINK void __DPSCALL DpsSortDictionary(DPS_SPELLLIST *);
extern __C_LINK void __DPSCALL DpsSortAffixes(DPS_AFFIXLIST *, DPS_SPELLLIST *);
extern __C_LINK void __DPSCALL DpsSortQuffixes(DPS_QUFFIXLIST *, DPS_SPELLLIST *);

extern int  DpsSpellAdd(DPS_SPELLLIST *, const dpsunicode_t *word, const char *flag, const char *lang);
extern int  DpsAffixAdd(DPS_AFFIXLIST *, const char *flag, const char * lang, const dpsunicode_t *mask, 
			const dpsunicode_t *find, const dpsunicode_t *repl, int type);
extern int  DpsQuffixAdd(DPS_QUFFIXLIST *, const char *flag, const char * lang, const dpsunicode_t *mask, const dpsunicode_t *find, const dpsunicode_t *repl);

extern int  DpsSelectSpellLang(DPS_ENV *Conf, char *lang);
extern void DpsSelectLang(DPS_AGENT *Indexer, char *lang);

extern void DpsSpellListFree(DPS_SPELLLIST *);
extern void DpsAffixListFree(DPS_AFFIXLIST *);
extern void DpsQuffixListFree(DPS_QUFFIXLIST *);

extern void DpsUniRegCompileAll(DPS_ENV *Conf);
extern int DpsUniRegComp(DPS_UNIREG_EXP *reg, const dpsunicode_t *pattern);
extern int DpsUniRegExec(const DPS_UNIREG_EXP *reg, const dpsunicode_t *string);
extern void DpsUniRegFree(DPS_UNIREG_EXP *reg);


#endif
