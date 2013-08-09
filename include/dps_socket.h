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

#ifndef _DPS_SOCKET_H
#define _DPS_SOCKET_H

/* Net status flags */
#define	DPS_NET_NOTCONNECTED	        0
#define	DPS_NET_CONNECTED		1
/* Net timeouts */
#define	DPS_NET_ACC_TIMEOUT		20
#define	DPS_NET_READ_TIMEOUT		20


int dps_closesocket(int socket);

void socket_close(DPS_CONN *connp );
int socket_open(DPS_CONN *connp );
int socket_connect(DPS_CONN *connp );
int socket_select(DPS_CONN *connp, size_t timeout, int mode );
int socket_read(DPS_CONN *connp, size_t maxsize);
int socket_read_line(DPS_CONN *connp);
int socket_write(DPS_CONN *connp, const char *buf);
int socket_listen(DPS_CONN *connp );
int socket_accept(DPS_CONN *connp );
int socket_getname(DPS_CONN *connp, struct sockaddr_in *sa_in);
void socket_buf_clear(DPS_CONN *connp);

extern ssize_t DpsRecvall(int s, void *buf, size_t len, size_t time_to_wait);
extern ssize_t DpsRecvstr(int s, void *buf, size_t len, size_t time_to_wait);
extern ssize_t DpsSend(int s, const void *msg, size_t len, int flags);
extern void DpsSockOpt(DPS_AGENT *A, int socket);
extern int DpsSockPrintf(int *socket, const char *fmt, ...);

#endif
