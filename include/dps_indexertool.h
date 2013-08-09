/* Copyright (C) 2004 Datapark corp. All rights reserved.

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

#ifndef _DPS_INDEXERTOOL_H
#define _DPS_INDEXERTOOL_H

#include "dps_config.h"

extern int extended_stats;
int DpsShowStatistics(DPS_AGENT *Indexer);
const char *DpsVersion(void);
void DpsAppendTarget(DPS_AGENT *Indexer, const char *url, const char *lang, const int hops, int parent);


#endif
