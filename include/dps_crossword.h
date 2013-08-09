/* Copyright (C) 2003-2004 Datapark corp. All rights reserved.
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

#ifndef _DPS_CROSSWORD_H
#define _DPS_CROSSWORD_H

extern int DpsCrossListAdd(DPS_DOCUMENT * Doc, DPS_CROSSWORD * CrossWord);
extern int DpsCrossListAddFantom(DPS_DOCUMENT * Doc, DPS_CROSSWORD * CrossWord);
extern void DpsCrossListFree(DPS_CROSSLIST * List);
extern DPS_CROSSLIST * DpsCrossListInit(DPS_CROSSLIST * List);
extern int DpsCrossCmp(const void *p1, const void *p2);
extern int DpsCrossListSort(DPS_CROSSLIST *List);


#endif
