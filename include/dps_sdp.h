/* Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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

#ifndef _DPS_SDP_H
#define _DPS_SDP_H

#include "dps_db_int.h"

extern int DpsFindWordsSearchd(DPS_AGENT *query, DPS_RESULT *Res, DPS_DB *searchd);
extern int DpsSearchdGetWordResponse(DPS_AGENT *query, DPS_RESULT *Res, DPS_DB *cl);
extern int DpsSearchdCatAction(DPS_AGENT *A, DPS_CATEGORY *C, int cmd, void *vdb);
extern int DpsSearchdURLAction(DPS_AGENT *A, DPS_DOCUMENT *D, int cmd, void *vdb);
extern int DpsResAddDocInfoSearchd(DPS_AGENT * query, DPS_DB *cl, DPS_RESULT * Res, size_t clnum);
extern int DpsSearchdConnect(DPS_AGENT *A, DPS_DB *cl);
extern void DpsSearchdClose(DPS_DB *cl);
extern int DpsCloneListSearchd(DPS_AGENT * Indexer, DPS_DOCUMENT *Doc, DPS_RESULT *Res, DPS_DB *db);
extern ssize_t DpsSearchdSendPacket(int fd, const DPS_SEARCHD_PACKET_HEADER *hdr, const void *data);

#endif
