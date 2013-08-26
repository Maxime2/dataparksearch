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

#ifndef _DPS_STORE_H
#define _DPS_STORE_H

#include <stdio.h>
#include "dps_common.h"

#define DPS_DOCHUNKSIZE (32 * 1024)

#define DPS_STOREIND_SIG "msINDEX\0"
#define DPS_STORE_SIG    "msSTORE\0"
#define DPS_URL_SIG      "msURLTB\0"
#define DPS_URLINFO_SIG  "msURLIN\0"
#define DPS_SERVER_SIG   "msSERVR\0"
#define DPS_LINKS_SIG    "msLINKS\0"
#define DPS_SIG_LEN 8
#define DPS_STORE_BITS 0xFFFF      /* bits of rec_id for file no. */

#define DPS_READ_LOCK  0
#define DPS_WRITE_LOCK 1


typedef struct {
  urlid_t    rec_id;
  size_t     docsize;
  time_t     next_index_time;
  time_t     last_mod_time;
  urlid_t    referrer;
  dpshash32_t crc32;
  time_t     bad_since_time;
  size_t     site_id;
  size_t     server_id;
  float      pop_rank;
  size_t     infoseek;
  short      status;
  short      hops;
  short      seed;
} DPS_CACHE_URL; /* url */

typedef struct {
  size_t rec_id;
  time_t period;
  int    proxy_port;
  int    gindex;
  int    follow;
  time_t net_delay_time;
  time_t read_timeout;
  int    match_type;
  size_t ordre;
  size_t parent;
  size_t weight;
  float  pop_weight;
  short  enabled;
  short  maxhops;
  short  deletebad;
  short  use_robots;
  short  use_clones;
  short  max_net_errors;
  char   command;
  char   case_sense;
  char   nomatch;
} DPS_CACHE_SERVER; /* url alias tag category charset lang basic_auth proxy proxy_auth */

typedef struct {
  size_t ot;
  size_t k;
  float weight;
} DPS_CACHE_LINK;


extern __C_LINK int __DPSCALL DpsStoreDoc(DPS_AGENT *A, DPS_DOCUMENT *Doc, const char *origurl);
extern __C_LINK int __DPSCALL DpsUnStoreDoc(DPS_AGENT *A, DPS_DOCUMENT *Doc, const char *origurl);
extern __C_LINK int __DPSCALL DpsStoreDeleteDoc(DPS_AGENT *A, DPS_DOCUMENT *Doc);
extern __C_LINK int __DPSCALL DpsStoreCheckUp(DPS_AGENT *Agent, int level);

typedef struct {
  DPS_AGENT *query;
  DPS_RESULT *Res;
  DPS_DOCUMENT *Doc;
  size_t size;
  size_t padding;
} DPS_EXCERPT_CFG;

/*extern void DpsExcerptDoc(DPS_AGENT *query, DPS_RESULT *Res, DPS_DOCUMENT *Doc, size_t size, size_t padding);*/
extern void * DpsExcerptDoc(void *excerpt_cfg);
extern char * DpsExcerptString(DPS_AGENT *query, DPS_RESULT *Res, const char *value, size_t size, size_t padding);
extern int DpsStoreSave(DPS_AGENT *Agent, int ns, const char *Client);
extern int DpsStoreGet(DPS_AGENT *Agent, int ns, int sd, const char *Client);
extern int DpsStoreGetByChunks(DPS_AGENT *Agent, int ns, int sd, const char *Client);
extern int DpsStoreFind(DPS_AGENT *Agent, int ns, int sd, const char *Client);
extern int DpsStoreDelete(DPS_AGENT *Agent, int ns, int sd, const char *Client);
extern int DpsStoredCheck(DPS_AGENT *Agent, int ns, int sd, const char *Client);
extern int DpsStoredOptimize(DPS_AGENT *Agent, int ns, const char *Client);
extern urlid_t DpsURL_ID(DPS_DOCUMENT *Doc, const char *url);

#endif
