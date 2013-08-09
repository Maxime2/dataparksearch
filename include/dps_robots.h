/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 Datapark corp. All rights reserved.
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

#ifndef _DPS_ROBOTS_H
#define _DPS_ROBOTS_H

extern DPS_ROBOT* DpsRobotFind(DPS_ROBOTS *Robots, const char *hostinfo);
extern DPS_ROBOT_RULE* DpsRobotRuleFind(DPS_AGENT *Indexer, DPS_SERVER *Server, DPS_DOCUMENT *Doc, DPS_URL * URL, int make_pause, int aliased);
extern int DpsRobotListFree(DPS_ROBOTS *Robots);
extern int DpsRobotParse(DPS_AGENT *Indexer, DPS_SERVER *Srv, const char *content, const char *hostinfo, int hops);
extern void DpsRobotClean(DPS_AGENT *Indexer);

#endif
