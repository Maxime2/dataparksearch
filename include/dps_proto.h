/* Copyright (C) 2003-2007 Datapark corp. All rights reserved.
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

#ifndef _DPS_PROTO_H
#define _DPS_PROTO_H

/* some of NNTP status codes */
#define NNTP_GROUP_OK                             211
#define NNTP_XOVER_OK                             224

/* HTTP status codes */
#define DPS_HTTP_STATUS_UNKNOWN              0
#define DPS_HTTP_STATUS_OK                   200
#define DPS_HTTP_STATUS_OK_CLONES            2200
#define DPS_HTTP_STATUS_NO_CONTENT           204
#define DPS_HTTP_STATUS_PARTIAL_OK           206
#define DPS_HTTP_STATUS_PARTIAL_OK_CLONES    2206

#define DPS_HTTP_STATUS_MULTIPLE_CHOICES          300
#define DPS_HTTP_STATUS_MOVED_PARMANENTLY         301
#define DPS_HTTP_STATUS_MOVED_TEMPORARILY         302
#define DPS_HTTP_STATUS_SEE_OTHER                 303
#define DPS_HTTP_STATUS_NOT_MODIFIED              304
#define DPS_HTTP_STATUS_NOT_MODIFIED_CLONES       2304
#define DPS_HTTP_STATUS_USE_PROXY                 305
#define DPS_HTTP_STATUS_TEMPORARY_REDIRECT        307

#define DPS_HTTP_STATUS_BAD_REQUEST               400
#define DPS_HTTP_STATUS_UNAUTHORIZED              401
#define DPS_HTTP_STATUS_PAYMENT_REQUIRED          402
#define DPS_HTTP_STATUS_FORBIDDEN            403
#define DPS_HTTP_STATUS_NOT_FOUND            404
#define DPS_HTTP_STATUS_METHOD_NOT_ALLOWED        405
#define DPS_HTTP_STATUS_NOT_ACCEPTABLE            406
#define DPS_HTTP_STATUS_PROXY_AUTHORIZATION_REQUIRED   407
#define DPS_HTTP_STATUS_REQUEST_TIMEOUT           408
#define DPS_HTTP_STATUS_CONFLICT             409
#define DPS_HTTP_STATUS_GONE                 410
#define DPS_HTTP_STATUS_LENGTH_REQUIRED           411
#define DPS_HTTP_STATUS_PRECONDITION_FAILED       412
#define DPS_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE  413
#define DPS_HTTP_STATUS_REQUEST_URI_TOO_LONG      414
#define DPS_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE         415

#define DPS_HTTP_STATUS_INTERNAL_SERVER_ERROR          500
#define DPS_HTTP_STATUS_NOT_IMPLEMENTED           501
#define DPS_HTTP_STATUS_BAD_GATEWAY               502
#define DPS_HTTP_STATUS_SERVICE_UNAVAILABLE       503
#define DPS_HTTP_STATUS_GATEWAY_TIMEOUT           504
#define DPS_HTTP_STATUS_NOT_SUPPORTED             505


/* DpsSearch network error codes */
#define DPS_NET_ERROR              -1
#define DPS_NET_TIMEOUT            -2
#define DPS_NET_CANT_CONNECT       -3
#define DPS_NET_CANT_RESOLVE       -4
#define DPS_NET_UNKNOWN            -5
#define DPS_NET_FILE_TL            -6
#define DPS_NET_ALLOC_ERROR        -7

/* Mirror parameters and error codes */
#define DPS_MIRROR_NO              -1
#define DPS_MIRROR_YES             0
#define DPS_MIRROR_NOT_FOUND       -1
#define DPS_MIRROR_EXPIRED         -2
#define DPS_MIRROR_CANT_BUILD      -3
#define DPS_MIRROR_CANT_OPEN       -4

/* HTTP codes */
extern __C_LINK const char * __DPSCALL DpsHTTPErrMsg(int code);

/* Single routine for various protocols */
extern int DpsGetURL(DPS_AGENT * Indexer,DPS_DOCUMENT * Doc, const char *origurl);
/*extern int DpsCheckAddr(struct sockaddr_in *addr, unsigned int read_timeout);*/

/* Mirroring features */
extern __C_LINK int __DPSCALL DpsMirrorPUT(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_URL *url, const char *suffix);
extern __C_LINK int __DPSCALL DpsMirrorGET(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_URL *url);

int connect_tm(int s, const struct sockaddr *name, unsigned int namelen, unsigned int to);

#endif
