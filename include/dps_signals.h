/* Copyright (C) 2003 Datapark corp. All rights reserved.
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

#ifndef _DPS_SIGNALS_H
#define _DPS_SIGNALS_H

#include "dps_common.h"

/*
 * udm_signals.h from UdmSearch
 * (C) 2000 Kir <kir@sever.net>, UdmSearch Developers Team
 */

extern int DpsSigHandlersInit(DPS_AGENT *Indexer);
extern void (*DpsSignal(int signo, void (*handler)(int)))(int);

extern int have_sighup;
extern int have_sigterm;
extern int have_sigpipe;
extern int have_sigint;
extern int have_sigusr1;
extern int have_sigusr2;
extern int have_sigalrm;

#endif /* _DPS_SIGNALS_H */
