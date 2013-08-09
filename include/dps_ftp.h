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

#ifndef _DPS_FTP_H
#define _DPS_FTP_H

#define FTP_FREE	0
#define FTP_BUSY	1

extern int Dps_ftp_connect(DPS_AGENT * Agent, DPS_CONN *connp, 
			   const char *hostname, int port, const char *user, const char *passwd, int timeout);
extern int Dps_ftp_open_control_port(DPS_AGENT * Agent, DPS_CONN *connp);

extern int Dps_ftp_get_reply(DPS_CONN *connp);
extern int Dps_ftp_read_line(DPS_CONN *connp);
extern int Dps_ftp_open_data_port(DPS_CONN *c, DPS_CONN *d);
extern int Dps_ftp_send_cmd(DPS_CONN *connp, const char *cmd);
extern int Dps_ftp_send_data_cmd(DPS_CONN *c, DPS_CONN *d, char *cmd, size_t max_doc_size);
extern int Dps_ftp_login(DPS_CONN *connp, const char *user, const char *passwd);
extern int Dps_ftp_cwd(DPS_CONN *c, const char *path);
extern int Dps_ftp_list(DPS_CONN *c, DPS_CONN *d, const char *path, const char *filename, size_t max_doc_size);
extern int Dps_ftp_get(DPS_CONN *c, DPS_CONN *d, char *path,size_t max_doc_size);
extern int Dps_ftp_mdtm(DPS_CONN *c, char *path);
extern int Dps_ftp_close(DPS_CONN *connp);
extern int Dps_ftp_abort(DPS_CONN *connp);
extern int Dps_ftp_set_binary(DPS_CONN *c);
extern ssize_t Dps_ftp_size(DPS_CONN *c, char *path);
extern int Dps_ftp_rest(DPS_CONN *c, size_t rest);

#endif
