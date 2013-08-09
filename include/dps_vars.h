/* Copyright (C) 2003-2005 Datapark corp. All rights reserved.
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

#ifndef _DPS_VARS_H
#define _DPS_VARS_H

extern DPS_VARLIST *DpsVarListInit(DPS_VARLIST * vars);
extern void DpsVarListFree(DPS_VARLIST * vars);
extern void DpsVarFree(DPS_VAR *);

extern int DpsVarListAddEnviron(DPS_VARLIST *vars, const char *name);

extern int DpsVarListAddStr(DPS_VARLIST * vars,const char * name, const char * val);
extern int DpsVarListAddInt(DPS_VARLIST * vars,const char * name, int val);
extern int DpsVarListAddUnsigned(DPS_VARLIST * vars, const char * name, dps_uint4 val);
extern int DpsVarListAddDouble(DPS_VARLIST * vars, const char * name, double val);
extern int DpsVarListAdd(DPS_VARLIST * Lst,DPS_VAR * S);
extern int DpsVarListDel(DPS_VARLIST * Lst,const char *name);
extern int DpsVarListReplace(DPS_VARLIST * Lst,DPS_VAR * S);

extern int DpsVarListAddLst(DPS_VARLIST * Lst,DPS_VARLIST *Src, const char *name, const char *w);
extern int DpsVarListInsLst(DPS_VARLIST * Lst,DPS_VARLIST *Src, const char *name, const char *w);
extern int DpsVarListReplaceLst(DPS_VARLIST * Lst,DPS_VARLIST *Src, const char *name, const char *w);
extern int DpsVarListDelLst(DPS_VARLIST *D, DPS_VARLIST *S, const char *name, const char *mask);

extern __C_LINK int __DPSCALL DpsVarListReplaceStr(DPS_VARLIST * vars,const char * name,const char * val);
extern __C_LINK int __DPSCALL DpsVarListReplaceInt(DPS_VARLIST * vars,const char * name,int val);
extern __C_LINK int __DPSCALL DpsVarListReplaceUnsigned(DPS_VARLIST * vars, const char * name, unsigned val);
extern int DpsVarListReplaceDouble(DPS_VARLIST * vars, const char * name, double val);

extern int DpsVarListInsStr(DPS_VARLIST * vars,const char * name,const char * val);
extern int DpsVarListInsInt(DPS_VARLIST * vars,const char * name,int val);

extern DPS_VAR *DpsVarListFind(DPS_VARLIST * vars,const char * name);
extern DPS_VAR *DpsVarListFindWithValue(DPS_VARLIST * vars,const char * name,const char * val);

extern char *       DpsVarListFindStr(DPS_VARLIST * vars,const char * name,const char * defval);
extern char *       DpsVarListFindStrTxt(DPS_VARLIST * vars,const char * name,const char * defval);
extern int          DpsVarListFindInt(DPS_VARLIST * vars,const char * name,int defval);
extern unsigned     DpsVarListFindUnsigned(DPS_VARLIST * vars, const char * name, unsigned defval);
extern double       DpsVarListFindDouble(DPS_VARLIST * vars, const char * name, double defval);

extern int DpsVarListLog(DPS_AGENT *A,DPS_VARLIST *V,int l,const char *pre);


#endif
