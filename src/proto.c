/* Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
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
#include "dps_proto.h"
#include "dps_utils.h"
#include "dps_mutex.h"
#include "dps_socket.h"
#include "dps_http.h"
#include "dps_ftp.h"
#include "dps_xmalloc.h"
#include "dps_host.h"
#include "dps_execget.h"
#include "dps_id3.h"
#include "dps_log.h"
#include "dps_vars.h"
#include "dps_db.h"
#include "dps_charsetutils.h"
#include "dps_hash.h"
#include "dps_url.h"
#include "dps_store.h"
#include "dps_hrefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef WITH_HTTPS
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif

/*#define DEBUG*/
/*#define DEBUG_REQUEST 1*/
/*
#define DEBUG_NNTP 
*/

/************** connect with timeout **********/

int connect_tm(int s, const struct sockaddr *name, unsigned int namelen, unsigned int to){
	int flags, res, s_err;
	socklen_t s_err_size = sizeof(int);
	fd_set sfds;
	struct timeval tv;
#ifdef DEBUG
	struct sockaddr_in *inet = (struct sockaddr_in *)name;
	struct sockaddr_in6 *inet6 = (struct sockaddr_in6 *)name;
#endif


	if (!to)return(connect(s,name, namelen));

#ifdef DEBUG
	{
#if INET_ADDRSTRLEN > INET6_ADDRSTRLEN
	  char abuf[INET_ADDRSTRLEN];
#else
	  char abuf[INET6_ADDRSTRLEN];
#endif
	  if (name->sa_family == AF_INET) {
	    if (inet_ntop(AF_INET, &inet->sin_addr, abuf, sizeof(abuf)) == NULL) {
	      dps_snprintf(abuf, sizeof(abuf), "<unknow>");
	    }
	  } else if (name->sa_family == AF_INET6) {
	    if (inet_ntop(AF_INET6, &inet6->sin6_addr, abuf, sizeof(abuf)) == NULL) {
	      dps_snprintf(abuf, sizeof(abuf), "<unknow>");
	    }
	  } else dps_snprintf(abuf, sizeof(abuf), "<unknow>");
	  fprintf(stderr, "Trying connect to: %s with timeout %d\n", abuf, to);
	}
#endif


	flags = fcntl(s, F_GETFL, 0);	 	/* Save the flags */
#ifdef O_NONBLOCK
	fcntl(s, F_SETFL, flags | O_NONBLOCK);  /* Set socket to non-blocked */
#endif

	res = connect(s, name, namelen);
	s_err = errno;			/* Save errno */
	fcntl(s, F_SETFL, flags);
	if ((res != 0) && (s_err != EINPROGRESS)){
	  errno = s_err;		/* Restore errno              */
#ifdef DEBUG
	  dps_strerror(NULL, 0, "exit at %s[%d]", __FILE__, __LINE__);
#endif
	  return(-1);		/* Unknown error, not timeout */
	}
	if(!res) {
#ifdef DEBUG
	    fprintf(stderr, "connected, exit at %s[%d]\n", __FILE__, __LINE__);
#endif
	    return(0);		/* Quickly connected */
	}

	FD_ZERO(&sfds);
	FD_SET(s,&sfds);

	tv.tv_sec = (long) to;
	tv.tv_usec = 0;
	
	while(1){
		res = select(s+1, NULL, &sfds, NULL, &tv);
		if(res==0)return(-1); /* Timeout */
		if(res<0){
			if (errno == EINTR) /* Signal */
				continue;
			else
				return(-1); /* Error */
		}
		break;
	}

	s_err=0;
	if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0) {
#ifdef DEBUG
	    fprintf(stderr, "not connected, exit at %s[%d]\n", __FILE__, __LINE__);
#endif
		return(-1);		/* Can't get sock options */
	}

	if (s_err){
		errno = s_err;
#ifdef DEBUG
		fprintf(stderr, "not connected, exit at %s[%d]\n", __FILE__, __LINE__);
#endif
		return(-1);
	}
#ifdef DEBUG
	fprintf(stderr, "connected, exit at %s[%d]\n", __FILE__, __LINE__);
#endif
	return(0);			/* We have a connection! */
}


/************** Open TCP Connect **************/


static int open_host(DPS_AGENT *Agent, DPS_DOCUMENT *Doc) {
        size_t i;
        int net;
	
	net = socket(AF_INET, SOCK_STREAM, 0);
	DpsSockOpt(Agent, net);
	if (bind(net, (struct sockaddr *)&Agent->Flags.bind_addr, sizeof(Agent->Flags.bind_addr)) == -1) {
	  char abuf[INET_ADDRSTRLEN];
	  if (inet_ntop(AF_INET, &Agent->Flags.bind_addr.sin_addr, abuf, sizeof(abuf)) == NULL) {
	    dps_snprintf(abuf, sizeof(abuf), "<unknow>");
	  }
	  dps_strerror(Agent, DPS_LOG_ERROR, "bind() to %s error", abuf);
	  dps_closesocket(net);
	  return DPS_NET_CANT_CONNECT;
	} 
	Doc->connp.sin.sin_family=AF_INET;
	for (i = 0; i < Doc->connp.n_sinaddr; i++) {
	  dps_memcpy(&Doc->connp.sin.sin_addr, &Doc->connp.sinaddr[i].sin_addr, sizeof(Doc->connp.sin.sin_addr));
	  if (DpsNeedLog(DPS_LOG_DEBUG)) {
	        char abuf[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &Agent->Flags.bind_addr.sin_addr, abuf, sizeof(abuf)) == NULL) {
		  dps_snprintf(abuf, sizeof(abuf), "<unknow>");
		}
		DpsLog(Agent, DPS_LOG_DEBUG, "connecting %dth addr for %s", i, abuf);
	  }
	  if(!connect_tm(net, (struct sockaddr *)&Doc->connp.sin, sizeof (struct sockaddr_in),(unsigned int)Doc->Spider.read_timeout)) {
	    return(net);
	  }
	}
	dps_closesocket(net);
	DpsLog(Agent, DPS_LOG_DEBUG, "Can't connect (%d addresses resolved)", Doc->connp.n_sinaddr);
	return(DPS_NET_CANT_CONNECT);
}

/* 
int DpsCheckAddr(struct sockaddr_in *addr, unsigned int read_timeout) {
  int net, rc;

	net = socket(AF_INET, SOCK_STREAM, 0);
	addr->sin_family = AF_INET;
	
	rc = connect_tm(net, (struct sockaddr *)addr, sizeof (struct sockaddr_in), read_timeout ? read_timeout : DPS_READ_TIMEOUT);
	if (rc) {
	  rc = DPS_NET_CANT_CONNECT;
	} else {
	  rc = DPS_OK;
	}
	dps_closesocket(net);
	return rc;
}
*/

/**************** HTTP codes and messages ************/

 __C_LINK const char * __DPSCALL DpsHTTPErrMsg(int code){
	switch(code){
	case 0:   return("Not indexed yet");
	case 100: return "Continue";
	case 101: return "Switching Protocols";
	case 200: return("OK");
	case 201: return("Created");
	case 202: return("Accepted");
	case 203: return("Non-Authoritative Information");
	case 204: return("No content");
	case 205: return("Reset Content");
	case 206: return("Partial OK");
	case 300: return("Multiple Choices");
	case 301: return("Moved Permanently");
	case 302: return("Moved Temporarily");
	case 303: return("See Other");
	case 304: return("Not Modified");
	case 305: return("Use Proxy (proxy redirect)");
	case 307: return "Temporary Redirect";
	case 400: return("Bad Request");
	case 401: return("Unauthorized");
	case 402: return("Payment Required");
	case 403: return("Forbidden");
	case 404: return("Not found");
	case 405: return("Method Not Allowed");
	case 406: return("Not Acceptable");
	case 407: return("Proxy Authentication Required");
	case 408: return("Request Timeout");
	case 409: return("Conflict");
	case 410: return("Gone");
	case 411: return("Length Required");
	case 412: return("Precondition Failed");
	case 413: return("Request Entity Too Large");
	case 414: return("Request-URI Too Long");
	case 415: return("Unsupported Media Type");
	case 416: return "Requested range not satisfiable";
	case 417: return "Expectation failed";
	case 450: return "Can't read file";
	case 451: return "SSI Error(s)";
	case 500: return("Internal Server Error");
	case 501: return("Not Implemented");
	case 502: return("Bad Gateway");
	case 503: return("Service Unavailable");
	case 504: return("Gateway Timeout");
	case 505: return("HTTP Version not supported");
	case 510: return("Not Extended");
	case 600: return "Can't create socket";
	case 601: return "Connection timed out";
	case 602: return "Incomplete response";
	case 603: return "Incomplete chunked response";
	case 2200:return "Clones, OK";
	case 2206:return "Clones, Patial OK";
	case 2304:return "Clones, Not modified";
	default:  return "Unknown status";
	}
}


/*******************************************************/
#ifdef WITH_HTTP

static int DpsHTTPGet(DPS_AGENT *Agent, DPS_DOCUMENT *Doc) {
    int fd,res=0;
    fd_set sfds;
    time_t start_time, now_time, prev_time;
    int status;
    ssize_t recv_size;
    size_t buf_size = DPS_NET_BUF_SIZE;
    struct timeval tv;


    /* Connect to HTTP or PROXY server */
    if( (fd=open_host(Agent,Doc)) < 0 )
       return(fd);

    /* Send HTTP request */
    if( DpsSend(fd, Doc->Buf.buf, dps_strlen(Doc->Buf.buf), 0) < 0 )
       return -1;

    Doc->Buf.size=0;
    start_time = now_time = time(NULL);

    while( 1 ) {
      /* Retrieve response */
      tv.tv_sec = (long) Doc->Spider.read_timeout;
      tv.tv_usec = 0;

       FD_ZERO( &sfds );
       FD_SET( fd, &sfds );

       status = select(FD_SETSIZE, &sfds, NULL, NULL, &tv);
       if( status == -1 ) {
               res=DPS_NET_ERROR;
               break;
       }else if( status == 0 ) {
               res=DPS_NET_TIMEOUT;
               break;
       } else {

	 if(FD_ISSET(fd,&sfds)) {
	   if(Doc->Buf.size + buf_size > Doc->Buf.max_size)
	     buf_size = Doc->Buf.max_size - Doc->Buf.size;
	   else
	     buf_size = DPS_NET_BUF_SIZE;
	  
	   if (Doc->Buf.allocated_size <= Doc->Buf.size + buf_size) {
	     Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
	     if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
	       Doc->Buf.allocated_size = 0;
	       res = DPS_NET_ALLOC_ERROR;
	       break;
	     }
	   }

/*	   recv_size = recv(fd, Doc->Buf.buf + Doc->Buf.size, buf_size, 0);*/
	   prev_time = now_time;
	   recv_size = read(fd, Doc->Buf.buf + Doc->Buf.size, buf_size);
	   now_time = time(NULL);
	   if( recv_size < 0 ){
	     res = DPS_NET_ERROR; /* recv_size;*/
	     break;
	   } else if( recv_size == 0 ){
	     if(Doc->Spider.doc_timeout < (size_t)(now_time - start_time)) {
	       res = DPS_NET_TIMEOUT;
	     }
	     if(Doc->Spider.read_timeout < (size_t)(now_time - prev_time)) {
	       res = DPS_NET_TIMEOUT;
	     }
	     break;
	   } else {
	     Doc->Buf.size += (size_t)recv_size;
	     if( Doc->Buf.size >= Doc->Buf.max_size ) {
	       res = DPS_NET_FILE_TL;
	       break;
	     }
	     if(Doc->Spider.doc_timeout < (size_t)(now_time - start_time)) {
	       res = DPS_NET_TIMEOUT;
	       break;
	     }
	   }
	 } else {
	   break;
	 }
       }
    }
    dps_closesocket(fd);
    if (res == 0) Doc->Buf.buf[Doc->Buf.size] = '\0';
    return(res);
}
#endif

#ifdef WITH_HTTPS

#define sslcleanup dps_closesocket(fd); SSL_free (ssl); SSL_CTX_free (ctx)

#ifndef OPENSSL_VERSION_NUMBER
#define OPENSSL_VERSION_NUMBER 0x0000
#endif

static int DpsHTTPSGet(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc)
{
    int fd;
    int res = 0, status;
    SSL_CTX* ctx;
    SSL*     ssl=NULL;
    const SSL_METHOD *meth;
    time_t start_time;
    size_t buf_size = DPS_NET_BUF_SIZE;

    /* Connect to HTTPS server */
    if( (fd=open_host(Indexer,Doc)) < 0 )
       return(fd);

    meth = SSLv23_client_method();

    if ((ctx = SSL_CTX_new (meth))==NULL){
      sslcleanup;
      return DPS_NET_ERROR;
    }

    /* -------------------------------------------------- */
    /* Now we have TCP connection. Start SSL negotiation. */

    if ((ssl=SSL_new(ctx))==NULL){
      sslcleanup;
      return DPS_NET_ERROR;
    }

    SSL_set_verify(ssl, SSL_VERIFY_NONE, NULL);

    SSL_set_fd (ssl, fd);

    if (SSL_connect(ssl)==-1){
      sslcleanup;
      return DPS_NET_ERROR;
    }

    /* Send HTTP request */
    if ((SSL_write(ssl, Doc->Buf.buf, (int)dps_strlen(Doc->Buf.buf))) == -1){
      sslcleanup;
      return DPS_NET_ERROR;
    }

    /* Get result */
    Doc->Buf.size = 0;
    start_time = time(NULL);
    while(1) {
		if(Doc->Buf.size + buf_size > Doc->Buf.max_size)
			buf_size = Doc->Buf.max_size - Doc->Buf.size;
		else
			buf_size = DPS_NET_BUF_SIZE;
		
		if (Doc->Buf.allocated_size <= Doc->Buf.size + buf_size) {
		  Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
		  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		    Doc->Buf.allocated_size = 0;
		    res = DPS_NET_ALLOC_ERROR;
		    break;
		  }
		}
		status = SSL_read(ssl, Doc->Buf.buf + Doc->Buf.size, (int)buf_size);
               if( status < 0 ){
                   res = status;
                   break;
               }else 
               if( status == 0 ){
                   break;
               }else{
		 Doc->Buf.size += (size_t)status;
                   if(Doc->Spider.doc_timeout < (size_t)(time(NULL) - start_time)) {
                   	res=DPS_NET_TIMEOUT;
                   	break;
                   }
                   if( Doc->Buf.size == Doc->Buf.max_size )
                       break;
               }
    }
/*    nread = SSL_read(ssl, Doc->buf, (int)(Doc->Buf.maxsize - 1));*/
/*
    if(nread <= 0){
      sslcleanup;
      return DPS_NET_ERROR;
    }
*/    
    SSL_shutdown (ssl);  /* send SSL/TLS close_notify */

/*    Doc->Buf.size=nread;*/
    if (res == 0) Doc->Buf.buf[Doc->Buf.size] = '\0';

    /* Clean up. */
    sslcleanup;
    return res;
}

#endif

#ifdef WITH_FTP
static int DpsFTPGet(DPS_AGENT * Indexer,DPS_DOCUMENT * Doc)
{
	int	last_mod_tm, code, res=0;
	char	buf[256], *full_path=NULL;
	size_t	len,buf_size;
	time_t	last_mod_time=DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections,"Last-Modified",""));
	
	Doc->Buf.size=0;
	
	/* Check for changing host */
	if (!Doc->connp.hostname || 
	    strcmp(Doc->connp.hostname, DPS_NULL2EMPTY(Doc->CurURL.hostname)) || 
	    (Doc->connp.connected == DPS_NET_NOTCONNECTED)){
		char *authstr = NULL;
		char * user=NULL;
		char * pass=NULL;
		
		if (Doc->CurURL.auth != NULL) {
			authstr = (char*)DpsStrdup(Doc->CurURL.auth);
			user=authstr;
			if((pass=strchr(authstr,':'))){
				*pass='\0';
				pass++;
			}
		}
		
		Doc->connp.charset_id = Doc->charset_id;
		DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
		code = Dps_ftp_connect(Indexer, &Doc->connp, DPS_NULL2EMPTY(Doc->CurURL.hostname),
				     Doc->CurURL.port?Doc->CurURL.port:Doc->CurURL.default_port, user, pass,
				     Doc->Spider.read_timeout); /* locked */
		DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);
		
		if (code == -1){
			if (Doc->connp.err >0){
				sprintf(Doc->Buf.buf, "HTTP/1.1 401 OK\r\n\r\n  ");
				Doc->Buf.size = dps_strlen(Doc->Buf.buf);
			}else{
				res = Doc->connp.err;
			}
		}
		DPS_FREE(authstr);
	}
	
	if (Doc->connp.connected == DPS_NET_CONNECTED){
		/* Make listing */
		if (Doc->CurURL.filename == NULL) {
			code = Dps_ftp_cwd(&Doc->connp, DPS_NULL2EMPTY(Doc->CurURL.path + 1));
			if (code != -1){
				code = Dps_ftp_list(&Doc->connp, Doc->connp.connp, DPS_NULL2EMPTY(Doc->CurURL.path + 1),
						    NULL, Doc->Buf.max_size);
				if (code == -1){
					if (Doc->connp.err >0){
						sprintf(Doc->Buf.buf, "HTTP/1.1 403 OK\r\n\r\n");
						Doc->Buf.size = dps_strlen(Doc->Buf.buf);
					}else{
						res = Doc->connp.err;
					}
				}else{
				  if (Doc->Buf.allocated_size < (size_t)Doc->connp.connp->buf_len + 128) {
				    Doc->Buf.allocated_size = Doc->connp.connp->buf_len + 128;
				    if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
				      Doc->Buf.allocated_size = 0;
				      res = DPS_NET_ALLOC_ERROR;
				    }
				  } 
				  if (res != DPS_NET_ALLOC_ERROR) {
				    dps_snprintf(Doc->Buf.buf, Doc->Buf.allocated_size, 
						 "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body>%s</body></html>", 
						 Doc->connp.connp->buf);
				    Doc->Buf.size = dps_strlen(Doc->Buf.buf);
				  }
				}
			}else{
				if (Doc->connp.err >0){
					sprintf(Doc->Buf.buf, "HTTP/1.1 403 OK\r\n\r\n");
					Doc->Buf.size = dps_strlen(Doc->Buf.buf);
				}else{
					res = Doc->connp.err;
				}
			}
		}else{
			len = dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.path)) + dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.filename));
			full_path = DpsMalloc(len+1);
			if (full_path == NULL) {
			  return DPS_ERROR;
			}
			dps_snprintf(full_path,len + 1, "%s%s", DPS_NULL2EMPTY(Doc->CurURL.path + 1), DPS_NULL2EMPTY(Doc->CurURL.filename));
			full_path[len]='\0';
			last_mod_tm = Dps_ftp_mdtm(&Doc->connp,full_path);
			if(last_mod_tm == -1 && Doc->connp.err){
				if (Doc->connp.err >0){
					sprintf(Doc->Buf.buf, "HTTP/1.1 404 OK\r\n\r\n");
					Doc->Buf.size = dps_strlen(Doc->Buf.buf);
				}else
					res = Doc->connp.err;

			}else if (((Indexer->flags & DPS_FLAG_REINDEX) == 0) && (last_mod_tm == last_mod_time)) {
				sprintf(Doc->Buf.buf, "HTTP/1.1 304 OK\r\n\r\n");
				Doc->Buf.size = dps_strlen(Doc->Buf.buf);
			}else{
				DpsTime_t2HttpStr(last_mod_tm, buf);
				if (Doc->method!=DPS_METHOD_HEAD){
				        int s, f = -1;
					size_t fsize = Doc->Buf.max_size;
					ssize_t dfs;
					sscanf(DpsVarListFindStr(&Doc->RequestHeaders, "Range", "bytes=0-0"), "bytes=%d-%d", &s, &f);
				        if (f != 0) {
					  if (s < 0) {
					    dfs = Dps_ftp_size(&Doc->connp, full_path);
					    if (dfs >= 0) s += dfs;
					  } else if (f > 0) {
					    fsize = f - s;
					  }
					  if (s > 0) Dps_ftp_rest(&Doc->connp, (size_t)s);
					}
					if (!Dps_ftp_get(&Doc->connp, Doc->connp.connp, full_path, fsize)) {
					  if (Doc->Buf.allocated_size < (size_t)Doc->connp.connp->buf_len + 128) {
					    Doc->Buf.allocated_size = Doc->connp.connp->buf_len + 128;
					    if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
					      Doc->Buf.allocated_size = 0;
					      res = DPS_NET_ALLOC_ERROR;
					    }
					  } 
					  if (res != DPS_NET_ALLOC_ERROR) {
					    dps_snprintf(Doc->Buf.buf, Doc->Buf.allocated_size, 
							 "HTTP/1.1 20%c OK\r\nLast-Modified: %s\r\n\r\n", 
							 (Doc->connp.connp->err == DPS_NET_FILE_TL) ? '6' : '0',
							 buf);
					    Doc->Buf.size = dps_strlen(Doc->Buf.buf);
					    if (Doc->connp.connp->buf_len + Doc->Buf.size >= Doc->Buf.max_size)
					      buf_size = Doc->Buf.max_size - Doc->Buf.size;
					    else
					      buf_size = Doc->connp.connp->buf_len;
					    
					    dps_memcpy(Doc->Buf.buf + Doc->Buf.size, Doc->connp.connp->buf, buf_size);
					    Doc->Buf.size += buf_size; 
					  }
					}else{
						if (Doc->connp.err > 0) {
							sprintf(Doc->Buf.buf, "HTTP/1.1 403 OK\r\n\r\n");
							Doc->Buf.size = dps_strlen(Doc->Buf.buf);
						}else{
							res = Doc->connp.err;
						}
					}
				}else{
				    	sprintf(Doc->Buf.buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nLast-Modified: %s\r\n\r\n", buf);
					Doc->Buf.size = dps_strlen(Doc->Buf.buf);
				}
			}
		}
		Dps_ftp_close(&Doc->connp);
	}
	
	DPS_FREE(full_path);
	DPS_FREE(Doc->connp.buf);
	if (Doc->connp.connp && Doc->connp.connp->buf){
		DPS_FREE(Doc->connp.connp->buf);
		Doc->connp.connp->buf=NULL;
	}
	if (res == 0) Doc->Buf.buf[Doc->Buf.size] = '\0';
	return res;
}
#endif  

#ifdef WITH_NEWS
static ssize_t fdgets(char *str, size_t size, int fd) {
	size_t nbytes = 0;	/* Bytes read from fd and stored to str */
	int done=0;		/* Finish flag */

	while(!done){
		if (nbytes + 1 >= size) {
			break;
		}
		if (!recv(fd, str + nbytes, 1, 0)) {
			break;
		} else {
			if(str[nbytes]==NL_CHAR){
				done=1;
			}
			nbytes++;
		}
	}
/*	if(!nbytes){
		return 0;
	}else{*/
		str[nbytes]='\0';
		return nbytes;
/*	}*/
}



static ssize_t NNTPRecv(char *str, size_t sz, int fd) {
	ssize_t res;
	res=fdgets(str,sz,fd);
#ifdef DEBUG_NNTP
	{
		char * s, ch;
		for(s=str; *s && *s!=CR_CHAR && *s!=NL_CHAR && (s-str<sz) ; s++);
		ch=*s;*s='\0';
		fprintf(stderr,"NNTPGet : NNTPRecv: '%s'\n",str);
		*s=ch;
	}
#endif
	return res;
}

static ssize_t NNTPSend(char * str,size_t sz, int fd){
	ssize_t res;
	res = DpsSend(fd, str, sz, 0);
#ifdef DEBUG_NNTP
	{
		char * s, ch;
		for(s=str; *s && *s!=CR_CHAR && *s!=NL_CHAR && (s-str<sz) ; s++);
		ch=*s;*s='\0';
		fprintf(stderr,"NNTPGet : NNTPSend: '%s'\n",str);
		*s=ch;
	}
#endif
	return res;
}


#define DPS_NNTP_SEND		10
#define DPS_NNTP_RECV		11
#define DPS_NNTP_RECVCMD	12

#define DPS_NNTP_CONNECT	21
#define DPS_NNTP_HELLO		22
#define DPS_NNTP_READER		23
#define DPS_NNTP_BREAK		24
#define DPS_NNTP_QUIT		25

#define DPS_NNTP_USER		31
#define DPS_NNTP_USERRES	32
#define DPS_NNTP_PASS		33
#define DPS_NNTP_PASSRES	34

#define DPS_NNTP_LIST		41
#define DPS_NNTP_LISTRES	42
#define DPS_NNTP_LISTDAT	43

#define DPS_NNTP_STAT		51
#define DPS_NNTP_STATRES	52

#define DPS_NNTP_GROUP		61
#define DPS_NNTP_GROUPRES	62

#define DPS_NNTP_XOVER		71
#define DPS_NNTP_XOVERRES	72
#define DPS_NNTP_XOVERDAT	73

#define DPS_NNTP_ARTICLE	81
#define DPS_NNTP_ARTICLERES	82
#define DPS_NNTP_ARTICLEDAT	83



#define DPS_TERM(x)		(x)[sizeof(x)-1]='\0'

#define DPS_NNTP_MAXCMD		100

#ifdef DEBUG_NNTP
static const char * nntpcmd(int cmd){
	switch(cmd){
		case DPS_NNTP_CONNECT:		return("connect");
		case DPS_NNTP_QUIT:		return("quit");
		case DPS_NNTP_BREAK:		return("break");
		case DPS_NNTP_HELLO:		return("hello");
		case DPS_NNTP_READER:		return("reader");
		case DPS_NNTP_RECV:		return("recv");
		case DPS_NNTP_RECVCMD:		return("recvcmd");
		case DPS_NNTP_SEND:		return("send");
		
		case DPS_NNTP_USER:		return("user");
		case DPS_NNTP_USERRES:		return("userres");
		
		case DPS_NNTP_PASS:		return("pass");
		case DPS_NNTP_PASSRES:		return("passres");
		
		case DPS_NNTP_LIST:		return("list");
		case DPS_NNTP_LISTRES:		return("listres");
		case DPS_NNTP_LISTDAT:		return("listdat");
		
		case DPS_NNTP_GROUP:		return("group");
		case DPS_NNTP_GROUPRES:		return("groupres");
		
		case DPS_NNTP_XOVER:		return("xover");
		case DPS_NNTP_XOVERRES:		return("xoverres");
		case DPS_NNTP_XOVERDAT:		return("xoverdat");
	
		case DPS_NNTP_STAT:		return("stat");
		case DPS_NNTP_STATRES:		return("statres");
	
		case DPS_NNTP_ARTICLE:		return("article");
		case DPS_NNTP_ARTICLERES:	return("articleres");
		case DPS_NNTP_ARTICLEDAT:	return("articledat");
	}
	return("UNKNOWN");
}
#endif
static void inscmd(int * commands,size_t * n,int command){
	if(((*n)+1)<DPS_NNTP_MAXCMD){
		commands[*n]=command;
		(*n)++;
	}
}
static int delcmd(int * cmd,size_t * n){
	if(*n>0){
		(*n)--;
		return cmd[*n];
	}else{
		return DPS_NNTP_BREAK;
	}
}

static int DpsNNTPGet(DPS_AGENT * Indexer,DPS_DOCUMENT *Doc){
	char str[2048]="";
	char grp[256]="";
	char msg[256]="";
	char user[256]="";
	char pass[256]="";
	char *tok,*lt,*end;
	int  cmd[DPS_NNTP_MAXCMD];
	size_t ncmd = 0, gap;
	
	int  fd=0;
	int  status=0;
	int  lastcmd=0;
	int  has_if_modified=0;
	int  a=0;
	int  b=0;
	int  from=0;
	int  to=0;
	int  headers=1;
	int  has_content=0;
	ssize_t nbytes;
	char savec;
	
	memset(cmd,0,sizeof(cmd));
	end=Doc->Buf.buf;
	Doc->Buf.size=0;
	*end='\0';

	/* We'll send user/password given in URL first         */
	/* Only after that we'll use them from "header" field  */
	if (Doc->CurURL.auth != NULL) {
		dps_strncpy(user,Doc->CurURL.auth,sizeof(user)-1);
		DPS_TERM(user);
		if((tok=strchr(user,':'))){
			*tok++='\0';
			dps_strncpy(pass,tok,sizeof(pass)-1);
			DPS_TERM(pass);
		}
	}
	
	
	inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
	
	/* Parse request headers */
	tok = dps_strtok_r(Doc->Buf.buf, "\r\n", &lt, &savec);
	while(tok){
#ifdef DEBUG_NNTP
		fprintf(stderr,"HEADER: '%s'\n",tok);
#endif
		if(!strncasecmp(tok,"If-Modified-Since: ",19))has_if_modified=1;
		if(!strncasecmp(tok,"Authorization: ",15)){
			size_t l;
			char * auth=DpsTrim(tok+15," \t\r\n");
			dps_strncpy(str,auth,sizeof(str)-1);
			DPS_TERM(str);
			l=dps_base64_decode(user,str,sizeof(user)-1);
			if((auth=strchr(user,':'))){
				*auth++='\0';
				dps_strcpy(pass,auth);
			}
		}
		tok = dps_strtok_r(NULL, "\r\n", &lt, &savec);
	}


	/* Find in which "url" field group and message are */
	/* It depends on protocol being used               */

	/* First bad case, make redirect */
	if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "news") && strcmp(DPS_NULL2EMPTY(Doc->CurURL.path), "/")) {
		/* news://news.server.com/group/ */
		/* In news:// group should not   */
		/* have trailing slash !!!!!!!!  */
		/* Let's    redirect to the same */
		/* URL without slashes           */
		char *s;
		dps_strncpy(grp,Doc->CurURL.path,sizeof(grp)-1);
		DPS_TERM(grp);
		/* Remove trailing slash */
		if((s=strrchr(grp+1,'/')))*s='\0';
		status=301;
		dps_snprintf(str, sizeof(str)-1, "%s://%s%s", Doc->CurURL.schema,Doc->CurURL.hostinfo,grp);
		DPS_TERM(str);
		sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nLocation: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
		Doc->Buf.size=dps_strlen(Doc->Buf.buf);
		return 0;
	}
	
	
	/* Second bad case, make redirect */
	if(!strcasecmp(Doc->CurURL.schema,"nntp") && !strcmp(Doc->CurURL.path,"/") && Doc->CurURL.filename != NULL) {
		/* nntp://news.server.com/something        */
		/* This is wrong URL, assume that          */
		/* "something" is a group name, redirect   */
		/* to the same URL but with trailing slash */
		status=301;
		dps_snprintf(str,sizeof(str)-1,"%s://%s/%s/", Doc->CurURL.schema, DPS_NULL2EMPTY(Doc->CurURL.hostinfo),
			 DPS_NULL2EMPTY(Doc->CurURL.filename));
		DPS_TERM(str);
		sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nLocation: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
		Doc->Buf.size=dps_strlen(Doc->Buf.buf);
		return 0;
	}



	if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "news") && (Doc->CurURL.filename != NULL)) {
		/* url->path here is always "/"     */
		/* So there are two possible cases: */
		/* news://news.server.com/group     */
		/* news://news.server.com/messg     */
		/* url->filename contains either    */
		/* message ID  or group name        */
		/* and doesn't have any slashes     */
		/* Check whether @ does present     */
		if(strchr(Doc->CurURL.filename,'@')){
			/* We have a message */
			dps_strncpy(msg,Doc->CurURL.filename,sizeof(grp)-1);
			DPS_TERM(msg);
			if(has_if_modified){
				inscmd(cmd,&ncmd,DPS_NNTP_STAT);
			}else{
				inscmd(cmd,&ncmd,DPS_NNTP_ARTICLE);
			}
		}else{
			/* We have a news group */
			dps_strncpy(grp,Doc->CurURL.filename,sizeof(grp)-1);
			DPS_TERM(grp);
			inscmd(cmd,&ncmd,DPS_NNTP_XOVER);
			inscmd(cmd,&ncmd,DPS_NNTP_GROUP);
		}
	}else
	if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "nntp") && strcmp(DPS_NULL2EMPTY(Doc->CurURL.path), "/")) {
		/* There are two possible cases:         */
		/*   nntp://news.server.com/group/       */
		/*   nntp://news.server.com/group/msg    */
		/* Group is in url->path and contains    */
		/* both leading and trailing slashes     */ 
		char * s;
		
		/* Copy without leading slash */
		dps_strncpy(grp,Doc->CurURL.path+1,sizeof(grp)-1);
		DPS_TERM(grp);
		
		/* Remove trailing slash */
		if((s=strchr(grp,'/')))*s='\0';
		if(Doc->CurURL.filename != NULL) {
			dps_strncpy(msg,Doc->CurURL.filename,sizeof(msg)-1);
			DPS_TERM(msg);
			if(has_if_modified){
				inscmd(cmd,&ncmd,DPS_NNTP_STAT);
				inscmd(cmd,&ncmd,DPS_NNTP_GROUP);
			}else{
				inscmd(cmd,&ncmd,DPS_NNTP_ARTICLE);
				inscmd(cmd,&ncmd,DPS_NNTP_GROUP);
			}
		}else{
			inscmd(cmd,&ncmd,DPS_NNTP_XOVER);
			inscmd(cmd,&ncmd,DPS_NNTP_GROUP);
		}
	}else{
		inscmd(cmd,&ncmd,DPS_NNTP_LIST);
	}
	inscmd(cmd,&ncmd,DPS_NNTP_READER);
	inscmd(cmd,&ncmd,DPS_NNTP_CONNECT);


#ifdef DEBUG_NNTP
	fprintf(stderr,"NNTPGet : Enter with proto='%s' host='%s' port=%d path='%s' filename='%s'\n",
		Doc->CurURL.schema, Doc->CurURL.hostname, Doc->CurURL.port ? Doc->CurURL.port : Doc->CurURL.default_port,
		Doc->CurURL.path, Doc->CurURL.filename);
	fprintf(stderr,"NNTPGet : Assume grp='%s' msg='%s'\n", grp, msg);
#endif


	while(ncmd){
#ifdef DEBUG_NNTP
		fprintf(stderr,"NCMD=%d\n",ncmd);
#endif
		lastcmd=delcmd(cmd,&ncmd);
#ifdef DEBUG_NNTP
		fprintf(stderr,"CMD=%s\n",nntpcmd(lastcmd));
#endif

		gap = end - Doc->Buf.buf;
		if ((size_t)gap + 4096 > Doc->Buf.allocated_size) {
		  Doc->Buf.allocated_size += 4096 + DPS_NET_BUF_SIZE;
		  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		    Doc->Buf.allocated_size = 0;
		    break;
		  }
		  end = Doc->Buf.buf + gap;
		}

		switch(lastcmd){
		case DPS_NNTP_CONNECT:
			/* Connect to NNTP server */
			fd=open_host(Indexer,Doc);
			if(fd<0)return(fd);		/* Could not connect */
			inscmd(cmd,&ncmd,DPS_NNTP_HELLO);
			inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
			break;
			
		case DPS_NNTP_RECV:
			nbytes=NNTPRecv(str,sizeof(str),fd);
			break;
			
		case DPS_NNTP_RECVCMD:
			nbytes=NNTPRecv(str,sizeof(str),fd);
			status=atoi(str);
			break;
			
		case DPS_NNTP_SEND:
			nbytes=NNTPSend(str,dps_strlen(str),fd);
			break;
			
		case DPS_NNTP_READER:
			inscmd(cmd,&ncmd,DPS_NNTP_HELLO);
			inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
			inscmd(cmd,&ncmd,DPS_NNTP_SEND);
			sprintf(str,"mode reader\r\n");
			break;
			
		case DPS_NNTP_HELLO:
			if(status!=200){
				status=500;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
			}
			break;
			
		case DPS_NNTP_USER:
			if(user[0]){
				inscmd(cmd,&ncmd,DPS_NNTP_USERRES);
				inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
				inscmd(cmd,&ncmd,DPS_NNTP_SEND);
				sprintf(str,"authinfo user %s\r\n",user);
			}else{
				status=401;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
			}
			break;
			
		case DPS_NNTP_USERRES:
			if(status==381){
				inscmd(cmd,&ncmd,DPS_NNTP_PASS);
			}else{
				status=500;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
			}
			break;
			
		case DPS_NNTP_PASS:
			if(pass[0]){
				inscmd(cmd,&ncmd,DPS_NNTP_PASSRES);
				inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
				inscmd(cmd,&ncmd,DPS_NNTP_SEND);
				sprintf(str,"authinfo pass %s\r\n",pass);
			}else{
				status=401;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
			}
			break;
			
		case DPS_NNTP_PASSRES:
			if((status!=200) && (status!=281)){
				if(status==502)status=401; /* Unauthorzed */
				else	status=500;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
				break;
			}
			break;
			
		case DPS_NNTP_LIST:
			inscmd(cmd,&ncmd,DPS_NNTP_LISTRES);
			inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
			inscmd(cmd,&ncmd,DPS_NNTP_SEND);
			sprintf(str,"list\r\n");
			break;
			
		case DPS_NNTP_LISTRES:
			if(status==480){
				inscmd(cmd,&ncmd,DPS_NNTP_LIST);
				inscmd(cmd,&ncmd,DPS_NNTP_USER);
				break;
			}
			if(status!=215){
				status=500;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
				break;
			}
			sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<HTML><BODY>\n");
			end=Doc->Buf.buf+dps_strlen(Doc->Buf.buf);
			inscmd(cmd,&ncmd,DPS_NNTP_LISTDAT);
			inscmd(cmd,&ncmd,DPS_NNTP_RECV);
			break;
			
		case DPS_NNTP_LISTDAT:
			if(str[0]=='.'){
				dps_snprintf(end, Doc->Buf.allocated_size - dps_strlen(Doc->Buf.buf), "</BODY></HTML>\n");
				break;
			}
			if((tok=strchr(str,' ')))*tok=0;
			if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "nntp"))
				dps_snprintf(end, Doc->Buf.allocated_size - dps_strlen(Doc->Buf.buf), "<A HREF=\"/%s/\"></A>\n", str);
			else
				dps_snprintf(end, Doc->Buf.allocated_size - dps_strlen(Doc->Buf.buf), "<A HREF=\"/%s\"></A>\n", str);
			end=end+dps_strlen(end);
			inscmd(cmd,&ncmd,DPS_NNTP_LISTDAT);
			inscmd(cmd,&ncmd,DPS_NNTP_RECV);
			break;
			
		case DPS_NNTP_XOVER:
			/* List of the articles in one news group */
			inscmd(cmd,&ncmd,DPS_NNTP_XOVERRES);
			inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
			inscmd(cmd,&ncmd,DPS_NNTP_SEND);
			sprintf(str,"xover %d-%d\r\n",from,to);
			break;
			
		case DPS_NNTP_XOVERRES:
			if(status==224){/* 224 data follows */
				sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<HTML><BODY>\n");
				end=Doc->Buf.buf+dps_strlen(Doc->Buf.buf);
				inscmd(cmd,&ncmd,DPS_NNTP_XOVERDAT);
				inscmd(cmd,&ncmd,DPS_NNTP_RECV);
			}else{
				sprintf(Doc->Buf.buf,"HTTP/1.0 404 Not found\r\nNNTP-Server-Response: %s\r\n\r\n",str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
			}
			break;
			
		case DPS_NNTP_XOVERDAT:
			if(str[0]=='.'){
				dps_snprintf(end, Doc->Buf.allocated_size - dps_strlen(Doc->Buf.buf), "</BODY></HTML>\n");
				break;
			}else
			if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "nntp")) {
				char *e;
				if((e = dps_strtok_r(str, "\t\r\n", &lt, NULL)))
					dps_snprintf(end, Doc->Buf.allocated_size - dps_strlen(Doc->Buf.buf), 
						     "<A HREF=\"%s%s\"></A>\n", DPS_NULL2EMPTY(Doc->CurURL.path), e);
				end=end+dps_strlen(end);
				inscmd(cmd,&ncmd,DPS_NNTP_XOVERDAT);
				inscmd(cmd,&ncmd,DPS_NNTP_RECV);
			}else{
				int n=0;
				char * field[10];
				memset(field,sizeof(field),0);
				
				tok = dps_strtok_r(str, "\t\r\n", &lt, NULL);
				while(tok){
#ifdef DEBUG_NNTP
					fprintf(stderr,"NNTPGet : field[%d]: '%s'\n",n,tok);
#endif
					if(n<10)field[n]=tok;
					tok = dps_strtok_r(NULL, "\t\r\n", &lt, NULL);
					n++;
				}
				if(field[4]){
					char * msgb=field[4];
					char * msge;
					if(*msgb=='<')msgb++;
					for(msge=msgb ; *msge!='\0' && *msge!='>' ; msge++);
					if(*msge=='>')*msge='\0';
#ifdef DEBUG_NNTP
					fprintf(stderr,"NNTPGet : messg_id: '%s'\n",msgb);
#endif
					if( ((end-Doc->Buf.buf) + dps_strlen(msgb) + 50) < Doc->Buf.allocated_size) {
						dps_snprintf(end, Doc->Buf.allocated_size - dps_strlen(Doc->Buf.buf), "<A HREF=\"/%s\"></A>\n", msgb);
						end+=dps_strlen(end);
					}
				}
				inscmd(cmd,&ncmd,DPS_NNTP_XOVERDAT);
				inscmd(cmd,&ncmd,DPS_NNTP_RECV);
			}
			break;
			
		case DPS_NNTP_GROUP:
			inscmd(cmd,&ncmd,DPS_NNTP_GROUPRES);
			inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
			inscmd(cmd,&ncmd,DPS_NNTP_SEND);
			sprintf(str, "group %s\r\n",grp);
			break;
			
		case DPS_NNTP_GROUPRES:
			if(status==480){ /* Authorization required */
				inscmd(cmd,&ncmd,DPS_NNTP_GROUP);
				inscmd(cmd,&ncmd,DPS_NNTP_USER);
			}else
			if(status==211){ /* Group selected */
				sscanf(str,"%d%d%d%d\n",&a,&b,&from,&to);
			}else{
				status=404;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
				break;
			}
			break;
			
		case DPS_NNTP_STAT:
			inscmd(cmd,&ncmd,DPS_NNTP_STATRES);
			inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
			inscmd(cmd,&ncmd,DPS_NNTP_SEND);
			if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "nntp")) {
				dps_snprintf(str,sizeof(str)-1,"stat %s\r\n",msg);
			}else{
				dps_snprintf(str,sizeof(str)-1,"stat <%s>\r\n",msg);
			}
			break;
			
		case DPS_NNTP_STATRES:
			if(status==480){ /* Authorization required */
				inscmd(cmd,&ncmd,DPS_NNTP_STAT);
				inscmd(cmd,&ncmd,DPS_NNTP_USER);
			}else
			if(status==223){	/* Article exists             */
				status=304;	/* return 304 Not modified    */
			}else
			if(status==423){	/* No such article            */
				status=404;	/* return 404 Not found       */
			}else{			/* Unknown status code        */
				status=500;	/* Return 505 Server error    */
			}
			sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
			inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
			break;
			
		case DPS_NNTP_ARTICLE:
			inscmd(cmd,&ncmd,DPS_NNTP_ARTICLERES);
			inscmd(cmd,&ncmd,DPS_NNTP_RECVCMD);
			inscmd(cmd,&ncmd,DPS_NNTP_SEND);
			if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "nntp")) {
				dps_snprintf(str,sizeof(str),"article %s\r\n",msg);
			}else{
				dps_snprintf(str,sizeof(str),"article <%s>\r\n",msg);
			}
			break;
			
		case DPS_NNTP_ARTICLERES:
			if(status==480){ /* Authorization required */
				inscmd(cmd,&ncmd,DPS_NNTP_ARTICLE);
				inscmd(cmd,&ncmd,DPS_NNTP_USER);
			}else
			if(status==220){ /* Article follows */
				sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");
				end=Doc->Buf.buf+dps_strlen(Doc->Buf.buf);
				inscmd(cmd,&ncmd,DPS_NNTP_ARTICLEDAT);
				inscmd(cmd,&ncmd,DPS_NNTP_RECV);
			}else{
				status=404;
				sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,DpsHTTPErrMsg(status),str);
				inscmd(cmd,&ncmd,DPS_NNTP_QUIT);
			}
			break;
			
		case DPS_NNTP_ARTICLEDAT:
			if(str[0]=='.')break;
			if(headers){
				if(!strncasecmp(str,"Content-Type:",13))has_content=1;
				if((!strcmp(str,"\r\n"))||(!strcmp(str,"\n"))){
					headers=0;
					if(!has_content)dps_strcat(Doc->Buf.buf,"Content-Type: text/plain\r\n");
				}
			}
			if((dps_strlen(Doc->Buf.buf)+dps_strlen(str)) < Doc->Buf.allocated_size){
				dps_strcat(end,str);
				end=end+dps_strlen(end);
			}
			inscmd(cmd,&ncmd,DPS_NNTP_ARTICLEDAT);
			inscmd(cmd,&ncmd,DPS_NNTP_RECV);
			break;
			
		case DPS_NNTP_QUIT:
			if(fd>0){
				sprintf(str,"quit\r\n");
				NNTPSend(str,dps_strlen(str),fd);
				dps_closesocket(fd);
				fd=0;
			}
			inscmd(cmd,&ncmd,DPS_NNTP_BREAK);
			break;
		
		case DPS_NNTP_BREAK:	
		default:
			ncmd=0;
			break;
		}
	}
	if(fd>0){
		/* This never should happen */
		/* Error in state machine   */
		dps_closesocket(fd);
	}
	/* Return length of buffer */
	Doc->Buf.size=dps_strlen(Doc->Buf.buf);
	return(0);
}

#endif


#ifdef WITH_FILE
static int DpsFILEGet(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc){
	int fd,size,l,status=0,is_dir;
	struct stat sb,sb1;
	DIR * dir;
	struct dirent *rec;
	char filename[1024];
	char newfilename[1024];
	char openname[1024];
	char mystatname[1024];
	char command[32]="";
	char proto[32]="";
	char *lt,*s;
	time_t ims=0;
	int  cvs_ignore= Indexer->Flags.CVS_ignore;
	size_t size_to_read;

	Doc->Buf.size=0;
	sscanf(Doc->Buf.buf,"%s%s%s",command,filename,proto);
	dps_strcpy(newfilename,filename);
	DpsUnescapeCGIQuery(openname,newfilename);


	/* Remember If-Modified-Since timestamp */
	s = dps_strtok_r(Doc->Buf.buf, "\r\n", &lt, NULL);
	while(s){
		if(!strncasecmp(s,"If-Modified-Since: ",19)){
			ims=DpsHttpDate2Time_t(s+19);
		}
		s = dps_strtok_r(NULL, "\r\n", &lt, NULL);
	}

	dps_strcpy(mystatname,openname);
	
	if(stat(mystatname,&sb)){
	  int err_no = errno;
#ifdef HAVE_PTHREAD
	  char err_str[128];
	  (void)strerror_r(err_no, err_str, sizeof(err_str));
#else
	  char *err_str = DPS_NULL2EMPTY(strerror(errno));
#endif
	  switch(err_no) {
	  case ENOENT: status = 404; break; /* Not found */
	  case EACCES: status = 403; break; /* Forbidden*/
	  default: status = 500;
	  }
	  sprintf(Doc->Buf.buf, "HTTP/1.0 %d %s\r\nX-Reason: %s\r\n\r\n", status, DpsHTTPErrMsg(status), err_str);
	  Doc->Buf.size = dps_strlen(Doc->Buf.buf);
	  return 0;
	}

	/* If directory is given without ending "/"   */
	/* we must redirect to the same URL with "/"  */
	if((sb.st_mode&S_IFDIR)&&(filename[dps_strlen(filename)-1]!='/')){
		status=301;
		sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nLocation: file:%s/\r\n\r\n",status,DpsHTTPErrMsg(status),filename);
		Doc->Buf.size=dps_strlen(Doc->Buf.buf);
		return 0;
	}
	
	if(sb.st_mode&S_IFDIR){

		if((dir=opendir(openname))){
		        DPS_HREF Href;
			char *stre, *lname = DpsVarListFindStr(&Doc->Sections, "ORIG_URL", NULL);
			int hops = 1 + DpsVarListFindInt(&Doc->Sections, "Hops", 0);
			sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n<HTML><TITLE>%s</TITLE><BODY>\n",
				(lname != NULL) ? lname : DpsVarListFindStr(&Doc->Sections, "URL", openname));
			
			stre=DPS_STREND(Doc->Buf.buf);
			while((rec = readdir (dir))){
				char escaped_name[1024]="";
				char *src,*e;
				size_t gap;
				/*
				This does not work on Solaris
				is_dir=(rec->d_type==DT_DIR);
				*/
				if (cvs_ignore && (!strcmp(rec->d_name, "CVS") || !strcmp(rec->d_name, ".svn")))
					continue;

				sprintf(newfilename, "%s%s", openname, rec->d_name);
				if(stat(newfilename,&sb1)){
					DpsLog(Indexer, DPS_LOG_EXTRA, "Can't stat '%s'", newfilename);
					is_dir = 0;
				} else { 
					is_dir = ((sb1.st_mode&S_IFDIR) > 0);
				}
				
				e=escaped_name;
				for(src=rec->d_name;*src;src++){
					if(strchr(" %&<>+[](){}/?#'\"\\;,",*src)){
						sprintf(e,"%%%X",(int)*src);
						e+=3;
					}else{
						*e=*src;
						e++;
					}
				}
				*e=0;
				DpsHrefInit(&Href);
				Href.url = escaped_name;
				Href.method = DPS_METHOD_GET;
				Href.hops = hops;
				DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
				if (Doc->Buf.allocated_size > Doc->Buf.max_size) continue;

				gap = stre - Doc->Buf.buf + 1;
				if (gap >= Doc->Buf.allocated_size - 2*PATH_MAX + 32) {
				  Doc->Buf.allocated_size += DPS_NET_BUF_SIZE;
				  if ((e = (char*)DpsRealloc(Doc->Buf.buf, (size_t)Doc->Buf.allocated_size + 1)) == NULL) {
				    Doc->Buf.allocated_size = 0;
				    break;
				  }
				  Doc->Buf.buf = e;
				  stre = e + gap;
				}
				
				dps_snprintf(stre,2*PATH_MAX+32,"<A HREF=\"%s%s\">%s</A>\n",escaped_name, is_dir ? "/" : "", rec->d_name);
				stre=DPS_STREND(stre);
			}
			closedir(dir);
			dps_strcpy(DPS_STREND(Doc->Buf.buf),"</BODY><HTML>\n");
			Doc->Buf.size=dps_strlen(Doc->Buf.buf);
			return 0;
		}else{
			switch(errno){
				case ENOENT: status=404;break; /* Not found */
				case EACCES: status=403;break; /* Forbidden*/
				default: status=500;
			}
			sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n\r\n",status,DpsHTTPErrMsg(status));
			Doc->Buf.size=dps_strlen(Doc->Buf.buf);
			return 0;
		}
	}else{
		/* Lets compare last modification date */
		if(ims>=sb.st_mtime){
			/* Document seems to be unchanged */
			status=304;
			sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n\r\n",status,DpsHTTPErrMsg(status));
			Doc->Buf.size=dps_strlen(Doc->Buf.buf);
			return 0;
		}

		if((fd = DpsOpen2(openname, O_RDONLY | DPS_BINARY)) < 0) {
			switch(errno){
				case ENOENT: status=404;break;
				case EACCES: status=403;break;
				default: status=1;
			}
			sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n\r\n",status,DpsHTTPErrMsg(status));
			Doc->Buf.size=dps_strlen(Doc->Buf.buf);
			return 0;
		}
		sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");

		dps_strcpy(DPS_STREND(Doc->Buf.buf),"Last-Modified: ");
		DpsTime_t2HttpStr(sb.st_mtime, DPS_STREND(Doc->Buf.buf));
		dps_strcpy(DPS_STREND(Doc->Buf.buf),"\r\n");

		sprintf(DPS_STREND(Doc->Buf.buf),"\r\n");
		l = dps_strlen(Doc->Buf.buf);
		if ((size_t)sb.st_size + l >= Doc->Buf.allocated_size) {
		  if ((size_t)sb.st_size >= Doc->Buf.max_size) {
		    Doc->Buf.allocated_size = Doc->Buf.max_size + l;
		    size_to_read = (size_t)Doc->Buf.max_size;
		  } else {
		    Doc->Buf.allocated_size = size_to_read = (size_t)(sb.st_size + l);
		  }
		  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, (size_t)Doc->Buf.allocated_size + 1)) == NULL) {
		    Doc->Buf.allocated_size = 0;
		    DpsClose(fd);
		    return -1;
		  }
		} else {
		  size_to_read = (size_t)sb.st_size;
		}
		size = read(fd, Doc->Buf.buf + l, size_to_read);
		DpsClose(fd);
		if(size>0){
			Doc->Buf.size = l + size;
			Doc->Buf.buf[Doc->Buf.size] = '\0';
			return 0;
		}else{
			return(size);
		}
	}
}
#endif



static int DpsBuildHTTPRequest(DPS_DOCUMENT *Doc){
	const char	*method=(Doc->method==DPS_METHOD_HEAD)?"HEAD ":"GET ";
	const char	*proxy=DpsVarListFindStr(&Doc->RequestHeaders,"Proxy",NULL);
	size_t		i, r;
	char            *url = (char*)DpsMalloc((i = dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.path)) + 
						 dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.filename)) + 
						 dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.query_string)) + 1));
	char            *eurl = (char*)DpsMalloc(3 * i);

	if (url == NULL || eurl == NULL) return DPS_ERROR;

/*	sprintf(url, "%s%s%s", DPS_NULL2EMPTY(Doc->CurURL.path), DPS_NULL2EMPTY(Doc->CurURL.filename),
		DPS_NULL2EMPTY(Doc->CurURL.query_string)); */
	dps_strcpy(url, DPS_NULL2EMPTY(Doc->CurURL.path));
	dps_strcat(url, DPS_NULL2EMPTY(Doc->CurURL.filename));
	dps_strcat(url, DPS_NULL2EMPTY(Doc->CurURL.query_string));
	DpsEscapeURI(eurl, url);

	Doc->Buf.allocated_size = 3 * i + DPS_NET_BUF_SIZE;
	
	Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1);
	if (Doc->Buf.buf == NULL) {
	  Doc->Buf.allocated_size = 0;
	  DPS_FREE(url); DPS_FREE(eurl);
	  return DPS_ERROR;
	}
	
	if(proxy && strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "file")) {
/*		sprintf(Doc->Buf.buf,"%s%s://%s%s HTTP/1.0\r\n", method, DPS_NULL2EMPTY(Doc->CurURL.schema), 
			DPS_NULL2EMPTY(Doc->CurURL.hostinfo), eurl);*/
		dps_strcpy(Doc->Buf.buf, method);
		dps_strcat(Doc->Buf.buf, DPS_NULL2EMPTY(Doc->CurURL.schema));
		dps_strcat(Doc->Buf.buf, "://");
		dps_strcat(Doc->Buf.buf, DPS_NULL2EMPTY(Doc->CurURL.hostinfo));
		dps_strcat(Doc->Buf.buf, eurl);
		dps_strcat(Doc->Buf.buf, " HTTP/1.0\r\n");
	}else{
/*		sprintf(Doc->Buf.buf,"%s %s HTTP/1.0\r\n", method, eurl);*/
		dps_strcpy(Doc->Buf.buf, method);
		dps_strcat(Doc->Buf.buf, eurl);
		dps_strcat(Doc->Buf.buf, " HTTP/1.0\r\n");
	}
	DPS_FREE(eurl);DPS_FREE(url);
	
	/* Add user defined headers */
	for (r = 0; r < 256; r++)
	for(i = 0; i < Doc->RequestHeaders.Root[r].nvars; i++) {
		DPS_VAR *Hdr = &Doc->RequestHeaders.Root[r].Var[i];
/*		sprintf(DPS_STREND(Doc->Buf.buf), "%s: %s\r\n", Hdr->name, Hdr->val);*/
		dps_strcpy(DPS_STREND(Doc->Buf.buf), Hdr->name);
		dps_strcpy(DPS_STREND(Doc->Buf.buf), ": ");
		dps_strcpy(DPS_STREND(Doc->Buf.buf), Hdr->val);
		dps_strcpy(DPS_STREND(Doc->Buf.buf), "\r\n");
	}
	
	/* Empty line is the end of HTTP header */
	dps_strcat(Doc->Buf.buf,"\r\n");
#ifdef DEBUG_REQUEST
	fprintf(stderr,"Request:'%s'\n",Doc->Buf.buf);
#endif
	return DPS_OK;
}



int DpsGetURL(DPS_AGENT * Indexer, DPS_DOCUMENT * Doc, const char *origurl) {
	int		res=0;
	const char	*proxy;
	int             mirror_period, found_in_mirror = 0, status;

	if ((strcmp(DPS_NULL2EMPTY(Doc->CurURL.filename), "robots.txt")) && (Indexer->flags & DPS_FLAG_FROM_STORED)) {
	  DpsVarListReplaceInt(&Doc->Sections, "Status", 0);
/*	  if(origurl != NULL) {
	    DpsVarListReplaceStr(&Doc->Sections, "URL", origurl);
	    DpsVarListDel(&Doc->Sections, "E_URL");
	    DpsVarListDel(&Doc->Sections, "URL_ID");
	    DpsURLParse(&Doc->CurURL,origurl);
	  }*/
	  res = DpsUnStoreDoc(Indexer, Doc, origurl);
	  status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
	  if ((status > 0) || (!(Indexer->flags & DPS_FLAG_REINDEX))) return (res == DPS_OK) ? DPS_OK : DPS_ERROR;
	}

	proxy = DpsVarListFindStr(&Doc->RequestHeaders, "Proxy", NULL);
	mirror_period = DpsVarListFindInt(&Doc->Sections, "MirrorPeriod", -1);

	DpsBuildHTTPRequest(Doc);
	
	/* If mirroring is enabled */
	if (mirror_period >= 0) {
		/* on u_m==0 it returned by mtime from mirrorget */
		/* but we knew that it should be put in mirror  */
		
		res = DpsMirrorGET(Indexer, Doc, &Doc->CurURL);
		
		if(!res){
			DpsLog(Indexer, DPS_LOG_DEBUG, "Taken from mirror");
			found_in_mirror = 1;
		}
	}

	if (!found_in_mirror) {
	  if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "exec")) {
		res = DpsExecGet(Indexer,Doc);
	  }else	if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "cgi")) {
		res = DpsExecGet(Indexer,Doc);
	  }
#ifdef HAVE_SQL
	  else if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "htdb")) {
	        res = DpsHTDBGet(Indexer, Doc, 0);
	  }
#endif
#ifdef WITH_FILE
	  else if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "file")) {
		res = DpsFILEGet(Indexer,Doc);
	  }
#endif
#ifdef WITH_NEWS
	  else if((!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "news"))) {
		res = DpsNNTPGet(Indexer,Doc);
	  }else if((!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "nntp"))) {
		res = DpsNNTPGet(Indexer,Doc);
	  }
#endif
#ifdef WITH_HTTPS
	  else if((!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "https"))) {
		res = DpsHTTPSGet(Indexer,Doc);
	  }
#endif
#ifdef WITH_HTTP
	  else if((!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "http")) ||
	   (!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "ftp") && (proxy))) {
		res = DpsHTTPGet(Indexer,Doc);
	  }
#endif
#ifdef WITH_FTP
	  else if ((!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "ftp")) && (!proxy)) {
		res = DpsFTPGet(Indexer,Doc);
	  }
#endif
	  DpsVarListReplaceStr(&Doc->Sections, "IP", inet_ntoa(Doc->connp.sin.sin_addr));
	}
#ifdef HAVE_SQL
	if(strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "htdb")) {/* Get HTDBText sections for non htdb: documents if they're defined */
	  res = DpsHTDBGet(Indexer, Doc, 1);
	}
#endif

	/* Add NULL terminator */
	if (Doc->Buf.buf) {
	  Doc->Buf.buf[Doc->Buf.size]='\0';
	  /* Increment indexer's download statistics */
	  Indexer->nbytes += Doc->Buf.size;
	}
	Indexer->ndocs++;
	
	switch(res){
		case DPS_NET_UNKNOWN:
			DpsLog(Indexer,DPS_LOG_WARN,"Protocol not supported");
			DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_NOT_SUPPORTED);
			break;
		case DPS_NET_ERROR:
			DpsLog(Indexer,DPS_LOG_WARN,"Network error");
			DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_SERVICE_UNAVAILABLE);
			break;
		case DPS_NET_TIMEOUT:
			DpsLog(Indexer,DPS_LOG_WARN,"Download timeout");
			DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_GATEWAY_TIMEOUT);
			break;
		case DPS_NET_CANT_CONNECT:
			DpsLog(Indexer,DPS_LOG_WARN,"Can't connect to host %s:%d",Doc->connp.hostname,Doc->connp.port);
			DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_SERVICE_UNAVAILABLE);
			break;
		case DPS_NET_CANT_RESOLVE:
			DpsLog(Indexer,DPS_LOG_WARN,"Unknown %shost '%s'",proxy?"proxy ":"",Doc->connp.hostname);
			DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_SERVICE_UNAVAILABLE);
			break;
		case DPS_NET_FILE_TL:
			DpsLog(Indexer, DPS_LOG_WARN, "File too large");
			DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_PARTIAL_OK);
			break;
		default:
			if(res<0){	/* No connection */
			  DpsLog(Indexer,DPS_LOG_WARN,"Can't connect to host %s:%d, res:%d",Doc->connp.hostname,Doc->connp.port,res);
			  DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_SERVICE_UNAVAILABLE);
			  break;
			}
	}
	/* Put into mirror if required */
	if ((mirror_period >= 0) && (!found_in_mirror)){
	  if(DpsMirrorPUT(Indexer, Doc, &Doc->CurURL, NULL)){
			return DPS_ERROR;
		}
	}
	DpsParseHTTPResponse(Indexer, Doc);
	return DPS_OK;
}
