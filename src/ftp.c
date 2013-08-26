/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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

#include "dps_common.h"
#include "dps_socket.h"
#include "dps_host.h"   
#include "dps_utils.h"   
#include "dps_ftp.h"   
#include "dps_log.h"   
#include "dps_proto.h"   
#include "dps_xmalloc.h"   
#include "dps_conf.h"   
#include "dps_charsetutils.h"

#ifdef WITH_FTP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <ctype.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

/*
#define DEBUG_FTP
*/

int Dps_ftp_get_reply(DPS_CONN *connp){
	if (!connp->buf)
		return -1;
	
        return (atoi(connp->buf)/100);
}

int Dps_ftp_read_line(DPS_CONN *connp){

	if (socket_select(connp, DPS_NET_READ_TIMEOUT, 'r')){
#ifdef DEBUG_FTP
	  fprintf(stderr, "ftp://%s (ftp_read_line-timeout-err): ", connp->hostname);
		/*DpsLog(connp->indexer, DPS_LOG_DEBUG, "ftp://%s (ftp_read_line-timeout-err): ", connp->hostname);*/
#endif
		return -1;
	}
	
		
	do {
		if (socket_read_line(connp) < 0)
			return -1;
		if (((connp->buf[0] =='1')||(connp->buf[0] =='2')||
		    (connp->buf[0] =='3')||(connp->buf[0] =='4')||
		    (connp->buf[0] =='5')) && (connp->buf[3] == ' '))
			break;
	}while( 1 );
	return 0;
}

int Dps_ftp_connect(DPS_AGENT *Agent, DPS_CONN *connp, const char *hostname, int port, const char *user, const char *passwd, int timeout) {
	size_t len;
	
	if (!connp)
		return -1;

	if (connp->connected == DPS_NET_CONNECTED)
		Dps_ftp_close(connp);	
	connp->connected = DPS_NET_NOTCONNECTED;

	if (!port) {
		connp->port = 21;
		connp->connp->port = 20;
	} else {
		connp->port = port;
		connp->connp->port = port - 1;
	}

	connp->timeout = (size_t)timeout;
	
	if (!hostname)
		return -1;
	len = dps_strlen(hostname);
	connp->hostname = DpsXrealloc(connp->hostname, len+1);
	if (connp->hostname == NULL) return -1;
	dps_snprintf(connp->hostname, len+1, "%s", hostname);
	
	if (Dps_ftp_open_control_port(Agent, connp))
		return -1;
	if (Dps_ftp_login(connp, user, passwd))
		return -1;
	Dps_ftp_set_binary(connp);
	connp->connected = DPS_NET_CONNECTED;
	return 0;
}

int Dps_ftp_open_control_port(DPS_AGENT *Agent, DPS_CONN *connp) {
	int code;

        if (DpsHostLookup(Agent, connp))
		return -1;
        if (socket_open(connp))
		return -1;
	if (socket_connect(connp))
		return -1;
	/* Read server response */
	Dps_ftp_read_line(connp);
	code = Dps_ftp_get_reply(connp);
	if (code != 2)
		return -1;
	return 0;
}

int Dps_ftp_open_data_port( DPS_CONN *c, DPS_CONN *d){
	char buf[64];	
	unsigned char *a, *p;
	
	int code;
	
	if (!d)
		return -1;
	if (socket_getname(c, &d->sin) == -1)
		return -1;

	if (d->port) {
	  d->sin.sin_port = htons((uint16_t)d->port);
	}

	if (socket_open(d))
		return -1;

	if (socket_listen(d)){
		return -1;
	}
	if (socket_getname(d, &d->sin) == -1){
		return -1;
	}

        a = (unsigned char *)&d->sin.sin_addr;
        p = (unsigned char *)&d->sin.sin_port;

	dps_snprintf(buf, 64, "PORT %d,%d,%d,%d,%d,%d",
                a[0], a[1], a[2], a[3], p[0], p[1]);
	code = Dps_ftp_send_cmd(c, buf);
	if ((code < 0) || strncasecmp(c->buf, "200 ", 4)){
		return -1;
	}
	d->user = c->user;
	d->pass = c->pass;
	return 0;
}


int Dps_ftp_send_cmd(DPS_CONN *connp, const char *cmd) {
        char *buf;
	size_t len;

    	connp->err = 0;
	len = dps_strlen(cmd)+2;    
        buf = DpsXmalloc(len+1);
	if (buf == NULL) return -1;
	dps_snprintf(buf, len+1, "%s\r\n", cmd);
	socket_buf_clear(connp);
	if (socket_write(connp, buf)){
		DPS_FREE( buf);
		return -1;
	}
#ifdef DEBUG_FTP
	fprintf(stderr, "ftp://%s (cmd) : %s", connp->hostname, buf);
	/*DpsLog(connp->indexer, DPS_LOG_DEBUG, "ftp://%s (cmd) : %s", connp->hostname, buf);*/
#endif
	DPS_FREE(buf);
	if (Dps_ftp_read_line(connp))
		return -1;
#ifdef DEBUG_FTP
	fprintf(stderr, "ftp://%s (reply): %s", connp->hostname, connp->buf);
	/*DpsLog(connp->indexer, DPS_LOG_DEBUG, "ftp://%s (reply): %s", connp->hostname, connp->buf);*/
#endif
    	return(Dps_ftp_get_reply(connp));
}

static int ftp_expect_bytes(char *buf){
	char *ch, *ch1;
	int bytes;
	
        ch = strstr(buf, " bytes");
        ch1 = strrchr(buf, '(');
        if (ch==0 || ch1 ==0)
                return -1;
        bytes = atol(ch1+1);

	return bytes;
}

int Dps_ftp_send_data_cmd(DPS_CONN *c, DPS_CONN *d, char *cmd, size_t max_doc_size){
	int code, bytes=0;
	
	if (!d)
		return -1;

	d->timeout = c->timeout;
	d->hostname = c->hostname;
	c->err = 0;
	
	if (Dps_ftp_open_data_port(c, d)){
		socket_close(d);
		return -1;
	}

	code = Dps_ftp_send_cmd(c, cmd);
	if (code == -1){
                socket_close(d);
                return -1;
	}else if ( code >3 ){
		c->err = code;
		socket_close(d);
		return -1;
	}
	bytes = ftp_expect_bytes(c->buf);	
	if (socket_accept(d)){
		socket_close(d);
		return -1;
	}

	if (socket_read(d, max_doc_size) < 0){
		/*DpsLog(c->indexer, DPS_LOG_DEBUG, "ftp://%s (socket_read-err):", c->hostname);*/
		socket_close(d);
		Dps_ftp_read_line(c);
		return -1;
	}
        socket_close(d);
 
	/* Check if file too lage ABORT */
	if (d->err == DPS_NET_FILE_TL){
		if (Dps_ftp_abort(c)) {
			socket_buf_clear(d);
			return -1;
		}
	} 
	

	/* 226 Transfer complete. */
	if (Dps_ftp_read_line(c)){
		/* Fixme: What to do if not receive "226 Transfer complete."? */
		/*DpsLog(c->indexer, DPS_LOG_DEBUG, "ftp://%s (data-end-err): %d", d->hostname, d->buf_len);*/
		Dps_ftp_close(c);
		if (bytes == d->buf_len )
			return 0;
		return -1;
	}
	code = Dps_ftp_get_reply(c);
	if (code == -1){
                return -1;
	}else if ( code >3 ){
		c->err = code;
		return -1;
	}
	return 0;		
}

int Dps_ftp_login(DPS_CONN *connp, const char *user, const char *passwd){
	char *buf;
	char user_tmp[32], passwd_tmp[64];
	int code;
	size_t len;
	
	DPS_FREE(connp->user);
	DPS_FREE(connp->pass);
	if (!user)
		dps_snprintf(user_tmp, 32, "anonymous");
	else {
		dps_snprintf(user_tmp, 32, "%s", user);
		connp->user = (char*)DpsStrdup(user);
	}
	    
	if (!passwd)
		dps_snprintf(passwd_tmp, 64, "%s-%s@dataparksearch.org", PACKAGE, VERSION);
	else {
		dps_snprintf(passwd_tmp, 32, "%s", passwd);
		connp->pass = (char*)DpsStrdup(passwd);
	}
	
	len = dps_strlen(user_tmp) + 5 /*dps_strlen("USER ")*/;
	buf = (char *) DpsXmalloc(len+1);
	if (buf == NULL) return -1;
	dps_snprintf(buf, len+1, "USER %s", user_tmp);
	code = Dps_ftp_send_cmd(connp, buf);
	DPS_FREE(buf);
	if (code == -1)
		return -1;
	else if (code == 2) /* Don't need password */
		return 0;
		
	len = dps_strlen(passwd_tmp) + 5 /*dps_strlen("PASS ")*/;
	buf = (char *) DpsXmalloc(len+1);
	if (buf == NULL) return -1;
	dps_snprintf(buf, len+1, "PASS %s", passwd_tmp);
	code = Dps_ftp_send_cmd( connp, buf);
	DPS_FREE(buf);
	if (code > 3)
		return -1;
	return 0;
}

static int ftp_parse_list(DPS_CONN *connp, const char *path) {
  char *line, *buf_in, *ch, *buf_out, *tok, *fname, *line_tok;
        int len_h, len_f,len_p, i;
	char *dir, savec, line_savec;
	size_t len,buf_len,cur_len;
	
	if (!connp->buf || !connp->buf_len)
		return 0;
        buf_in = connp->buf;
	/* 22 = dps_strlen(<a href=\"ftp://%s%s%s/\"></a>)*/
        len_h = dps_strlen(connp->hostname) + ((connp->user) ? dps_strlen(connp->user) : 0) + ((connp->pass) ? dps_strlen(connp->pass) : 0) + 2 + 22;
        len_p = dps_strlen(path);
        cur_len = 0;
        buf_len = connp->buf_len;
        buf_out = DpsXmalloc(buf_len + 1);
	if (buf_out == NULL) return -1;
	buf_out[0] = '\0';
	line = dps_strtok_r(buf_in, "\r\n", &tok, &savec);
        do{
	        if (!(fname = dps_strtok_r(line, " ", &line_tok, &line_savec)))
			continue;
		/* drwxrwxrwx x user group size month date time file_name */
		for(i=0; i<7; i++)
		        if (!(fname = dps_strtok_r(NULL, " ", &line_tok, &line_savec)))
				break;
		if (!(fname = dps_strtok_r(NULL, "", &line_tok, &line_savec)))
			continue;
		len = 0 ;
		len_f = len_h + len_p + dps_strlen(fname);
	        if ((cur_len+len_f) >= buf_len){
			buf_len += DPS_NET_BUF_SIZE;
			buf_out = DpsXrealloc(buf_out, buf_len + 1);
		}
					
		switch (line[0]){
			case 'd':
				if (!fname || !strcmp(fname, ".") || !strcmp(fname, ".."))
				        break;
				len = len_f;
    				dps_snprintf(DPS_STREND(buf_out) /*buf_out+cur_len*/, len+1, "<a href=\"ftp://%s%s%s%s%s/%s%s/\"></a>\n",
					 (connp->user) ? connp->user : "", (connp->user) ? ":" : "",
					 (connp->pass) ? connp->pass : "", (connp->user || connp->pass) ? "@" : "",
					    connp->hostname, path, fname);
				break;
    	    	        case 'l':
				ch = strstr (fname, " -> ");
				if (!ch)
				    break;
				len = ch - fname;
				dir = DpsMalloc(len+1);
				if (dir == NULL) return -1;
				dps_snprintf(dir, len+1, "%s", fname);
				len = len_h + len_p + dps_strlen(dir);
				dps_snprintf(DPS_STREND(buf_out)/*buf_out+cur_len*/, len+1, "<a href=\"ftp://%s%s%s%s%s/%s%s/\"></a>\n", 
					 (connp->user) ? connp->user : "", (connp->user) ? ":" : "",
					 (connp->pass) ? connp->pass : "", (connp->user || connp->pass) ? "@" : "",
					    connp->hostname, path, dir);
				DPS_FREE(dir);
				/*ch +=4;*/
				/* Check if it is absolute link */
/*				if ((ch[0] == '/') || (ch[0] == '\\') ||
					 ((isalpha(ch[0]) && (ch[1]==':')))){
					len = len_h+dps_strlen(ch);
					dps_snprintf(buf_out+cur_len, len+1, "<a href=\"ftp://%s%s/\"></a>", 
						    connp->hostname, ch);
				}else{
					len = len_h+len_p+dps_strlen(ch);
	    				dps_snprintf(buf_out+cur_len, len+1, "<a href=\"ftp://%s%s%s/\"></a>", 
						    connp->hostname, path, ch);
				}
*/
				break;
    	    	        case '-':
				len =  len_f; 
		    	        dps_snprintf(DPS_STREND(buf_out)/*buf_out+cur_len*/, len+1, "<a  href=\"ftp://%s%s%s%s%s/%s%s\"></a>\n", 
					 (connp->user) ? connp->user : "", (connp->user) ? ":" : "",
					 (connp->pass) ? connp->pass : "", (connp->user || connp->pass) ? "@" : "",
					    connp->hostname, path, fname);

				break;
		}
		cur_len += len;
		
	}while( (line = dps_strtok_r(NULL, "\r\n", &tok, &savec)));

	if (cur_len+1 > connp->buf_len_total){
		connp->buf_len_total = cur_len;  
		connp->buf = DpsXrealloc(connp->buf, (size_t)connp->buf_len_total+1);
		if (connp->buf == NULL) return -1;
	}
	bzero(connp->buf, ((size_t)connp->buf_len_total+1));
	dps_memcpy(connp->buf, buf_out, cur_len); /* was: dps_memmove */
	DPS_FREE(buf_out);
	return 0;
}

int Dps_ftp_list(DPS_CONN *c, DPS_CONN *d, const char *path, const char *filename, size_t max_doc_size){
	char *cmd;
	size_t len;
	
	if (!filename){
		cmd = DpsXmalloc(16);
		if (cmd == NULL) return -1;
		sprintf(cmd, "LIST");
	}else{
    		len = dps_strlen(filename) + 16;
		cmd = DpsXmalloc(len+1);
		if (cmd == NULL) return -1;
    		dps_snprintf(cmd, len+1, "LIST %s", filename);
	}

	if (Dps_ftp_send_data_cmd(c, d, cmd, max_doc_size)== -1) {
		DPS_FREE(cmd);
		/*DpsLog(c->indexer, DPS_LOG_DEBUG, "(ftp_list-err)->%s", d->buf);*/
		return -1;
	}
	DPS_FREE(cmd);
	return ftp_parse_list(d, path);
}

int Dps_ftp_get(DPS_CONN *c, DPS_CONN *d, char *path, size_t max_doc_size){
	char *cmd;
	size_t len;
	
        if (!path)
		return -1;

        len = dps_strlen(path) + 16;
	cmd = DpsXmalloc(len+1);
	if (cmd == NULL) return -1;
        dps_snprintf(cmd, len+1, "RETR %s", path);

	if (Dps_ftp_send_data_cmd(c, d, cmd, max_doc_size) == -1 && d->err != DPS_NET_FILE_TL) {
		DPS_FREE(cmd);
		return -1;
	}
	DPS_FREE(cmd);
	return 0;
}

int Dps_ftp_mdtm(DPS_CONN *c, char *path){
	char *cmd;
	int code;
	size_t len; 
	
	if (!path)
		return -1;
		
    	len = dps_strlen(path) + 16;
	cmd = DpsXmalloc(len+1);
	if (cmd == NULL) return -1;
	dps_snprintf(cmd, len+1, "MDTM %s", path);

	code = Dps_ftp_send_cmd(c, cmd);
	DPS_FREE(cmd);
	if (code == -1){
                return -1;
	}else if ( code >3 ){
		c->err = code;
		return -1;
	}
	return (DpsFTPDate2Time_t(c->buf));
}

ssize_t Dps_ftp_size(DPS_CONN *c, char *path) {
	char *cmd;
	int code;
	size_t len; 
	
	if (!path)
		return -1;
		
    	len = dps_strlen(path) + 16;
	cmd = DpsXmalloc(len + 1);
	if (cmd == NULL) return -1;
	dps_snprintf(cmd, len + 1, "SIZE %s", path);

	code = Dps_ftp_send_cmd(c, cmd);
	DPS_FREE(cmd);
	if (code == -1){
                return -1;
	}else if ( code >3 ){
		c->err = code;
		return -1;
	}
	sscanf(c->buf, "213 %zu", &len);
	return (ssize_t)len;
}

int Dps_ftp_rest(DPS_CONN *c, size_t rest) {
	char cmd[64];
	int code;
	
	dps_snprintf(cmd, 63, "REST %u", rest);

	code = Dps_ftp_send_cmd(c, cmd);
	if (code == -1){
                return -1;
	}else if ( code >3 ){
		c->err = code;
		return -1;
	}
	return 0;
}

int Dps_ftp_set_binary(DPS_CONN *c){
	char *cmd;
	int code;
	
	cmd = DpsXmalloc(7);
	if (cmd == NULL) return -1;
	sprintf(cmd, "TYPE I");

	code = Dps_ftp_send_cmd(c, cmd);
	DPS_FREE(cmd);
	if (code == -1){
                return -1;
	}else if ( code >3 ){
		c->err = code;
		return -1;
	}
	return 0;
}

int Dps_ftp_cwd(DPS_CONN *c, const char *path){
	char *cmd;
	int code;
	size_t len;
	
	if (!path)
		return -1;
	if (*path == '\0') return 0;
		
	len = dps_strlen(path) + 16;
	cmd = DpsXmalloc(len+1);
	if (cmd == NULL) return -1;
	dps_snprintf(cmd, len+1, "CWD %s", path);

	code = Dps_ftp_send_cmd(c, cmd);
	DPS_FREE(cmd);
	if (code == -1){
                return -1;
	}else if ( code >3 ){
		c->err = code;
		return -1;
	}
	return 0;
}

int Dps_ftp_close(DPS_CONN *connp){
	int code;

	if (connp->connected == DPS_NET_CONNECTED){
		code = Dps_ftp_send_cmd(connp, "QUIT");
	}
	connp->connected = DPS_NET_NOTCONNECTED;
	socket_close(connp);
	if (connp->connp)
		socket_close(connp->connp);
	return 0;
}

int Dps_ftp_abort(DPS_CONN *connp){
	int code;

	socket_buf_clear(connp->connp);
	if (send(connp->conn_fd, "\xFF\xF4\xFF", 3, MSG_OOB)==-1)
		return -1;
	
	if (socket_write(connp, "\xF2"))
		return -1;
	
	code = Dps_ftp_send_cmd(connp, "ABOR");
	socket_buf_clear(connp->connp);

	if (code !=4)
		return -1;
	return 0;
}

#endif
