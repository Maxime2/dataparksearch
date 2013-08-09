/* Copyright (C) 2008-2011 DataPark Ltd. All rights reserved.
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

#ifndef _DPS_AGENT_H
#define _DPS_AGENT_H

#include "dps_common.h"

extern __C_LINK DPS_AGENT *	__DPSCALL DpsAgentInit(DPS_AGENT *Indexer, DPS_ENV *, int handle);
extern __C_LINK void		__DPSCALL DpsAgentFree(DPS_AGENT *Indexer);
extern __C_LINK void		__DPSCALL DpsAgentSetAction(DPS_AGENT *Indexer, int action);
extern __C_LINK int DpsAgentStoredConnect(DPS_AGENT *Indexer);
extern DPS_DBLIST * DpsAgentDBLSet(DPS_AGENT *result, DPS_ENV *Env);

#endif
