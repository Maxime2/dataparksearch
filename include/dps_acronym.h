/* Copyright (C) 2005-2012 DataPark Ltd. All rights reserved.

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

#ifndef _DPS_ACRONYM_H
#define _DPS_ACRONYM_H

#include "dps_config.h"
#include "dps_common.h"



extern void DpsAcronymListInit(DPS_ACRONYMLIST *List);
/*extern size_t DpsAcronymListAdd(DPS_ACRONUMLIST *List, DPS_ACRONYM *Acronym);*/
extern void DpsAcronymListFree(DPS_ACRONYMLIST *List);
extern int DpsAcronymListLoad(DPS_AGENT *query, const char *filename);
extern void DpsAcronymListSort(DPS_ACRONYMLIST *List);
extern DPS_ACRONYM *DpsAcronymListFind(const DPS_ACRONYMLIST *List, DPS_WIDEWORD *wword, DPS_ACRONYM **last);

#endif /* _DPS_ACRONYM */
