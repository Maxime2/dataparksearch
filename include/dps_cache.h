/* Copyright (C) 2003-2006 Datapark corp. All rights reserved.
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

#ifndef _DPS_CACHE_H
#define _DPS_CACHE_H

#include "dps_common.h"


extern int DpsOpenCache(DPS_AGENT *A, int shared, int light);
extern int DpsCloseCache(DPS_AGENT  *A, int shared, int light);
extern void DpsRotateDelLog(DPS_AGENT *A);
extern void DpsFlushAllBufs(DPS_AGENT *Agent, int rotate_logs_flag);

extern __C_LINK int __DPSCALL DpsURLActionCache(DPS_AGENT *A, DPS_DOCUMENT *D, int cmd, DPS_DB *db);
extern int DpsStatActionCache(DPS_AGENT *A, DPS_STATLIST *S, DPS_DB *db);
extern int DpsResActionCache(DPS_AGENT *Agent, DPS_RESULT *Res, int cmd, DPS_DB *db, size_t dbnum);

extern int DpsResAddDocInfoCache(DPS_AGENT *query, DPS_DB *db, DPS_RESULT *Res, size_t dbnum);
extern int DpsAddURLCache(DPS_AGENT *A, DPS_DOCUMENT *Doc, DPS_DB *db);

extern int DpsCachedCheck(DPS_AGENT *A, int level);
extern int DpsCacheConvert(DPS_AGENT *A);


#endif
