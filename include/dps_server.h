/* Copyright (C) 2003-2008 Datapark corp. All rights reserved.
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

#ifndef _DPS_SERVER_H
#define _DPS_SERVER_H

/* Spider follow types */
#define DPS_FOLLOW_UNKNOWN	-1

#define DPS_FOLLOW_NO		0
#define DPS_FOLLOW_PATH		1
#define DPS_FOLLOW_SITE		2
#define DPS_FOLLOW_WORLD	3

extern int		DpsSpiderParamInit(DPS_SPIDERPARAM *);
extern int cmpsrvpnt(const void *p1,const void *p2);
extern __C_LINK int	__DPSCALL DpsServerInit(DPS_SERVER * srv);
extern __C_LINK void __DPSCALL DpsServerFree(DPS_SERVER * srv);
extern __C_LINK int	__DPSCALL DpsServerAdd(DPS_AGENT * A, DPS_SERVER * srv);
extern DPS_SERVER *	DpsServerFind(DPS_AGENT *A, urlid_t server_id, const char * url, int charset_id, char **alias);
extern void		DpsServerListFree(DPS_SERVERLIST *);
extern void		DpsServerListSort(DPS_SERVERLIST *);
extern urlid_t          DpsServerGetSiteId(DPS_AGENT *Indexer, DPS_SERVER *srv, DPS_DOCUMENT *Doc);

#endif
