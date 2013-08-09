/* Copyright (C) 2003-2009 Datapark corp. All rights reserved.
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

#ifndef _DPS_URL_H
#define _DPS_URL_H

#define DPS_URL_OK	0
#define DPS_URL_LONG	1
#define DPS_URL_BAD	2
/*
#define DEBUG_URL
*/
extern __C_LINK DPS_URL * __DPSCALL DpsURLInit(DPS_URL *url);
extern __C_LINK void __DPSCALL DpsURLFree(DPS_URL *url);
#ifdef DEBUG_URL
extern int _DpsURLParse(DPS_URL *url, const char *s, const char *file, int line);
#define DpsURLParse(u, s) _DpsURLParse(u, s, __FILE__, __LINE__)
#else
extern int DpsURLParse(DPS_URL *url,const char *s);
#endif
extern char * DpsURLNormalizePath(char * path);
extern void RelLink(DPS_AGENT *Indexer, DPS_URL *curURL, DPS_URL *newURL, char **str, int ReverseAliasFlag);

#endif

