/* Copyright (C) 2003-2004 Datapark corp. AlL rights reserved.
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

#ifndef _DPS_STOPWORDS_H
#define _DPS_STOPWORDS_H

extern DPS_STOPWORD  *DpsStopListFind(DPS_STOPLIST *, const dpsunicode_t *word, const char *lang);
extern void           DpsStopListFree(DPS_STOPLIST *);
extern void           DpsStopListSort(DPS_STOPLIST *);
extern __C_LINK int __DPSCALL DpsStopListLoad(DPS_ENV *, const char *fname);
extern int            DpsStopListAdd(DPS_STOPLIST *, DPS_STOPWORD *);

#endif
