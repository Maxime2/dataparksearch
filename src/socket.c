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

#include "dps_common.h"
#include "dps_socket.h"
#include "dps_utils.h"
#include "dps_proto.h"
#include "dps_log.h"
#include "dps_xmalloc.h"
#include "dps_proto.h"
#include "dps_signals.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
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
#ifdef __linux__
#include <linux/version.h>
#endif


int dps_closesocket(int socket) {
  char junk[2048];
  fd_set fds;
  struct timeval tv;
  int rfd, z;

  (void)shutdown(socket, SHUT_WR);
  tv.tv_sec = (long)2;
  tv.tv_usec = 0;
  for(z = 0; z < 90; z++) {
    FD_ZERO(&fds);
    FD_SET(socket, &fds);
    rfd = select(socket+1, &fds, 0, 0, &tv);
    if (rfd <= 0) break;
    if (read(socket, junk, sizeof(junk)) <= 0) break;
  }
  (void)close(socket);
}


void socket_close( DPS_CONN *connp ){
	if (!connp)
		return;

	if (connp->conn_fd > 0){
		dps_closesocket(connp->conn_fd);
		connp->conn_fd = 0;
	}    
	return;
}


int socket_open( DPS_CONN *connp ) {
	int op, len;
	
	op=1;
	len = sizeof(struct sockaddr_in);
	
	/* Create cocket */
	connp->conn_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connp->conn_fd == -1){
		connp->err = DPS_NET_ERROR;
		return -1;
	}

	if (setsockopt(connp->conn_fd, SOL_SOCKET, SO_REUSEADDR,
					(char *)&op,sizeof(op))==-1) {
		connp->err = DPS_NET_ERROR;
		return -1;
	}

	connp->sin.sin_family = AF_INET;
	return 0;
}

int socket_connect( DPS_CONN *connp){
        socklen_t len;
	size_t i;

	for (i = 0; i < connp->n_sinaddr; i++) {
	  dps_memcpy(&connp->sin.sin_addr, &connp->sinaddr[i].sin_addr, sizeof(connp->sin.sin_addr));
	  connp->sin.sin_port = htons((uint16_t)connp->port);
	  connp->sin.sin_family = AF_INET;
/*	  connp->sin.sin_len = sizeof(struct sockaddr_in);*/
	  if (connect(connp->conn_fd, (struct sockaddr *)&connp->sin, sizeof(connp->sin)) == 0) {
	    len = sizeof(struct sockaddr_in);
	    if (getsockname(connp->conn_fd, (struct sockaddr *)&connp->sin, &len) == -1){
		connp->err = DPS_NET_ERROR;
                return -1;
	    }
	    connp->connected = DPS_NET_CONNECTED;
	    return 0;
	    
	  }
	  dps_strerror(NULL, 0, "connecting for %s:%d error", inet_ntoa(connp->sin.sin_addr), connp->port);
	}

	connp->err = DPS_NET_CANT_CONNECT;
	return -1;
}


int socket_select( DPS_CONN *connp, size_t timeout, int mode) {
        fd_set fds;
        struct timeval tv;
	int rfd;

        FD_ZERO(&fds);

        tv.tv_sec = (long)timeout;
        tv.tv_usec = 0;
    do{
        FD_ZERO(&fds);
	FD_SET(connp->conn_fd, &fds);
	if (mode == 'r')
    		rfd = select(connp->conn_fd+1, &fds, 0, 0, &tv);
	else
    		rfd = select(connp->conn_fd+1, 0, &fds, 0, &tv);
	
	if (rfd == 0 ) {
		/* timeout */
		if (timeout)
			connp->err = DPS_NET_TIMEOUT;
		return -1;
	
	}
    }while( rfd == -1 && errno == EINTR);
	return 0;	
}

int socket_read( DPS_CONN *connp, size_t maxsize){
        int num_read;
	size_t num_read_total;
	time_t t;

	num_read_total = 0;

	if (connp->buf)
		DPS_FREE(connp->buf);

	connp->buf_len_total = 0;
	connp->buf_len = 0;
	connp->err = 0;
	
	t = time(NULL);
	do {
	  if (socket_select(connp, connp->timeout, 'r') == -1){
			return -1;
		}

		if (connp->buf_len_total <= num_read_total+DPS_NET_BUF_SIZE){
			connp->buf_len_total += DPS_NET_BUF_SIZE;
			connp->buf = DpsXrealloc(connp->buf, (size_t)(connp->buf_len_total+1));
			if (connp->buf == NULL) return -1;
		}
		
/*		num_read = recv(connp->conn_fd, connp->buf + num_read_total, 
				(DPS_NET_BUF_SIZE <= maxsize - num_read_total) ? DPS_NET_BUF_SIZE : (maxsize - num_read_total),
				0);*/
		num_read = read(connp->conn_fd, connp->buf + num_read_total, 
				(DPS_NET_BUF_SIZE <= maxsize - num_read_total) ? DPS_NET_BUF_SIZE : (maxsize - num_read_total)
				);

		num_read_total += num_read;		

		if (num_read < 0){
			connp->err = DPS_NET_ERROR;
			return -1;
		}else if (num_read == 0) {
		  if ((size_t)(time(NULL) - t) > connp->timeout) break;
		} else t = time(NULL);
		if (num_read_total >=maxsize ){
			connp->err = DPS_NET_FILE_TL;
			break;
		}
	}while (num_read!=0);
	connp->buf_len = num_read_total;
	return num_read_total;	
}


int socket_read_line( DPS_CONN *connp ){
	size_t num_read_total, buf_size;
	
        buf_size = DPS_NET_BUF_SIZE;
	num_read_total = 0;

	if (connp->buf)
		DPS_FREE(connp->buf);
	
	connp->buf_len_total = 0;
	connp->buf_len = 0;

	while(1){
		if (connp->buf_len_total <= num_read_total+DPS_NET_BUF_SIZE) {
			connp->buf_len_total += DPS_NET_BUF_SIZE;
			connp->buf = DpsXrealloc(connp->buf, (size_t)(connp->buf_len_total +1));
			if (connp->buf == NULL) return -1;
		}
/*		if(!recv(connp->conn_fd,&(connp->buf[num_read_total]),1,0))
			return -1;*/
		
		if(read(connp->conn_fd, &(connp->buf[num_read_total]), 1) <= 0)
			return -1;
		if (connp->buf[num_read_total] == NL_CHAR)
			break;
		else if (connp->buf[num_read_total] == 0)
			break;
		num_read_total++;
        }
	connp->buf_len = dps_strlen(connp->buf);
	return num_read_total;	
}

int socket_write(DPS_CONN *connp, const char *buf){
	
	if (socket_select(connp, DPS_NET_READ_TIMEOUT, 'w') == -1)
		return -1;

	if (DpsSend(connp->conn_fd, buf, dps_strlen(buf), 0) == -1){
		connp->err = DPS_NET_ERROR;
		return -1;
	}
	return 0;
}

int socket_accept(DPS_CONN *connp){
	struct sockaddr sa;
	int sfd;
	socklen_t len;
        if (socket_select(connp, DPS_NET_ACC_TIMEOUT, 'r') == -1)
		return -1;

	len = sizeof(struct sockaddr);
						
	sfd = accept(connp->conn_fd, &sa, &len);
	socket_close(connp);

	if (sfd == -1){
		connp->err = DPS_NET_ERROR;
		return -1;
	}
	connp->conn_fd = sfd;
	
	dps_memcpy(&connp->sin, &sa, sizeof(connp->sin));
	return 0;
}

int socket_listen(DPS_CONN *connp){

        /*if (!connp->port)*/ connp->sin.sin_port = 0; 

        if (bind(connp->conn_fd, (struct sockaddr *)&connp->sin, 
				    sizeof(struct sockaddr_in)) == -1){
		connp->err = DPS_NET_ERROR;
	        return -1;
	}
	if (socket_getname(connp, &connp->sin) == -1)
	    return -1;	
								
        if (listen(connp->conn_fd, 1) == -1){
		connp->err = DPS_NET_ERROR;
	        return -1;
	}
        return 0;
}

int socket_getname(DPS_CONN *connp, struct sockaddr_in *sa_in){
        socklen_t len;
	
	len = sizeof(struct sockaddr_in);
        if (getsockname(connp->conn_fd, (struct sockaddr *)sa_in, &len) == -1){
		connp->err = DPS_NET_ERROR;
                return -1;
	}
	return 0;
}

void socket_buf_clear(DPS_CONN *connp){
	char buf[1024];
	int len;
	do {
    		if (socket_select(connp, 0, 'r')==-1)
			return;
/*		len = recv(connp->conn_fd, buf, 1024,0);*/
		len = read(connp->conn_fd, buf, 1024);
	} while(len > 0);
}



/* To bust performance you may increase this value, but be sure, that kernel can handle it */
#define DPS_BLKLEN 8192

#if 0

ssize_t DpsRecvall(int s, void *buf, size_t len){

	ssize_t received = 0, r = 0;
	char *b = buf;
#if !defined(MSG_WAITALL)
	time_t start = time(NULL);
#endif
	if (len == 0) return received;
	while ( ((size_t)received < len) && ((r = recv(s, b + received, dps_min(len-received, DPS_BLKLEN),
#ifdef MSG_WAITALL
						       MSG_WAITALL)) > 0)) {
#else
						       0)) >= 0)) {
#endif

		received += r;
		if ( 0  /* || have_sigint || have_sigterm */
		    || have_sigpipe
		    ) break;
#if !defined(MSG_WAITALL)
		if (time(NULL) - start > 300) break;
#endif
	}
	return (r < 0) ? r : received;
}

#else

ssize_t DpsRecvall(int s, void *buf, size_t len, size_t time_to_wait) {

	ssize_t received = 0, r = 0;
	char *b = buf;
	time_t start = time(NULL);

	if (len == 0) return received;
	while ((size_t)received < len) {

	  if ((r = read(s, b + received, dps_min(len - received, DPS_BLKLEN))) > 0) {
		received += r;
	  }
	  if ( ((r < 0) && (errno != EINTR))  /* || have_sigint || have_sigterm */
	       || have_sigpipe
	       ) break;
	  if (r == 0) {
	    if (time_to_wait > 0 && ((size_t)(time(NULL) - start) > time_to_wait)) break;
	    DPS_MSLEEP(1);
	  }
	}
	return (r < 0) ? r : received;
}

ssize_t DpsRecvstr(int s, void *buf, size_t len, size_t time_to_wait) {

	ssize_t received = 0, r = 0;
	char *b = buf;
	time_t start = time(NULL);
	int notdone = 1;
	size_t i;

	if (len == 0) return received;
	while (notdone && (size_t)received < len) {

	  if ((r = read(s, b + received, dps_min(len - received, DPS_BLKLEN))) > 0) {
	        for (i = 0; i < r; i++) if(b[received + i] == '\0' || b[received + i] == '\n') notdone = 0;
		received += r;
	  }
	  if ( ((r < 0) && (errno != EINTR))  /* || have_sigint || have_sigterm */
	       || have_sigpipe
	       ) break;
	  if (r == 0) {
	    if (time_to_wait > 0 && ((size_t)(time(NULL) - start) > time_to_wait)) break;
	    DPS_MSLEEP(1);
	  }
	}
	return (r < 0) ? r : received;
}


#endif

ssize_t DpsSend(int s, const void *msg, size_t len, int flags) {
  ssize_t o = 0;
  size_t olen, slen;
  const char *p = msg;

  while(len) {
    slen = (len < DPS_BLKLEN) ? len : DPS_BLKLEN;
    olen = send(s, p, slen, flags);
    if (olen == (size_t)-1) return olen;
    len -= olen;
    p += olen;
    o += olen;
  }
  return o;
}


void DpsSockOpt(DPS_AGENT *A, int dps_socket) {
  const int lowat = 1;
  struct timeval so_tval;
  so_tval.tv_sec = 300;
  so_tval.tv_usec = 0;

#if !defined(sgi) && !defined(__sgi) && !defined(__irix__) && !defined(sun) && !defined(__sun) /* && !defined(__FreeBSD__)*/
  if (setsockopt(dps_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&so_tval, sizeof(so_tval)) != 0) {
    dps_strerror(A, DPS_LOG_EXTRA, "%s [%d] setsockopt error", __FILE__, __LINE__);
  }
#endif
#if defined(SO_SNDLOWAT) && !defined(__linux__)
  if (setsockopt(dps_socket, SOL_SOCKET, SO_SNDLOWAT, (char *)&lowat, sizeof(lowat)) != 0) {
    dps_strerror(A, DPS_LOG_EXTRA, "%s [%d] setsockopt error", __FILE__, __LINE__);
  }
#endif
#if defined(SO_RCVLOWAT) && (!defined(__linux__) || LINUX_VERSION_CODE >= 0x20400)
  if (setsockopt(dps_socket, SOL_SOCKET, SO_RCVLOWAT, (char *)&lowat, sizeof(lowat)) != 0) {
    dps_strerror(A, DPS_LOG_EXTRA, "%s [%d] setsockopt error", __FILE__, __LINE__);
  }
#endif
}


int DpsSockPrintf(int *socket, const char *fmt, ...) {
	char buf[4096];
	va_list ap;
	size_t len;

	va_start(ap,fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	len = dps_strlen(buf);
	if ((ssize_t)len != DpsSend(*socket, buf, len, 0)) return DPS_ERROR;
	return DPS_OK;
}


/***********/
