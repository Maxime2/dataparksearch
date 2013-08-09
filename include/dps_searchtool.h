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

#ifndef _DPS_SEARCH_TOOL_H
#define _DPS_SEARCH_TOOL_H

extern void DpsWrdTopSort(DPS_URL_CRD *wrd, size_t nwrd,size_t topcount);
extern void DpsSortSearchWordsByURL(DPS_URL_CRD_DB *wrd, size_t num);
extern void DpsSortSearchWordsByURL0(DPS_URL_CRD *wrd, size_t num);
extern void DpsSortSearchWordsByWeight(DPS_URL_CRD *wrd,size_t num);
extern void DpsSortSearchWordsBySite(DPS_RESULT *Res, DPS_URLCRDLIST *L, size_t num, const char *pattern);
extern void DpsSortSearchWordsByPattern(DPS_RESULT *Res, DPS_URLCRDLIST *L, size_t num, const char *pattern);

extern void DpsGroupByURL(DPS_AGENT *Agent, DPS_RESULT *Res);
extern void DpsGroupBySite(DPS_AGENT *Agent, DPS_RESULT *Res);

extern int  DpsPrepare(DPS_AGENT *query, DPS_RESULT *res);
extern int  DpsParseQueryString(DPS_AGENT * Agent,DPS_VARLIST * vars,char * query_string);

extern char *DpsHlConvert(DPS_WIDEWORDLIST *L,const char * src, DPS_CONV *lc_uni, DPS_CONV *uni_bc, int NOprefixHL);
extern int  DpsConvert(DPS_ENV *Conf, DPS_VARLIST *Env_Vars, DPS_RESULT *Res, DPS_CHARSET *lcs, DPS_CHARSET *bcs);
extern char* DpsRemoveHiLightDup(const char *s);

extern int DpsCatToTextBuf(DPS_CATEGORY *C, char *textbuf, size_t len);
extern int DpsCatFromTextBuf(DPS_CATEGORY *C, char *textbuf);

extern int dps_need2segment(dpsunicode_t *uwrd);
extern dpsunicode_t   *DpsUniSegment(DPS_AGENT *Indexer, dpsunicode_t *s, const char *lang);

extern char * DpsBuildPageURL(DPS_VARLIST * vars, char **dst);
extern void DpsParseQStringUnescaped(DPS_VARLIST *vars, const char *qstring);

extern int DpsCmpUrlid(const void *v1, const void *v2);
extern size_t DpsRemoveNullSections(DPS_URL_CRD *words, size_t n, int *wf);
extern size_t DpsRemoveNullSectionsDB(DPS_URL_CRD_DB *words, size_t n, int *wf, int secno);


#endif
