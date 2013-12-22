/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
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
#include "dps_utils.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_uniconv.h"
#include "dps_searchtool.h"
#include "dps_boolean.h"
#include "dps_xmalloc.h"
#include "dps_spell.h"
#include "dps_stopwords.h"
#include "dps_word.h"
#include "dps_vars.h"
#include "dps_db.h"
#include "dps_db_int.h"
#include "dps_url.h"
#include "dps_hash.h"
#include "dps_parsehtml.h"
#include "dps_store.h"
#include "dps_doc.h"
#include "dps_conf.h"
#include "dps_result.h"
#include "dps_log.h"
#include "dps_sgml.h"
#include "dps_mutex.h"
#include "dps_chinese.h"
#include "dps_acronym.h"
#include "dps_charsetutils.h"
#include "dps_guesser.h"
#include "dps_match.h"
#include "dps_signals.h"

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#ifdef CHASEN
#include <chasen.h>
#endif

#ifdef MECAB
#include <mecab.h>
#endif

/*#define DEBUG_MEM 1*/

#ifdef DEBUG_MEM
#include <sys/mman.h>
#endif

#define DEBUG_CACHE

#define MULTITHREADED_SORT


#define DEBUG_PHRASES


/********************* SIG Handlers ****************/
static void SpellSighandler(int sign);

static void init_signals(void){
	/* Set up signals handler*/
	DpsSignal(SIGPIPE, SpellSighandler);
	DpsSignal(SIGCHLD, SpellSighandler);
	DpsSignal(SIGALRM, SpellSighandler);
	DpsSignal(SIGUSR1, SpellSighandler);
	DpsSignal(SIGUSR2, SpellSighandler);
	DpsSignal(SIGHUP, SpellSighandler);
	DpsSignal(SIGINT, SpellSighandler);
	DpsSignal(SIGTERM, SpellSighandler);
}

static void SpellSighandler(int sign) {
        pid_t chpid;
#ifdef UNIONWAIT
	union wait status;
#else
	int status;
#endif
	switch(sign){
		case SIGPIPE:
			have_sigpipe = 1;
			break;
		case SIGCHLD:
		  while ((chpid = waitpid(-1, &status, WNOHANG)) > 0);
			break;
		case SIGHUP:
			have_sighup=1;
			break;
		case SIGINT:
			have_sigint=1;
			break;
		case SIGTERM:
			have_sigterm=1;
			break;
	        case SIGALRM:
		        _exit(0);
			break;
		case SIGUSR1:
		  have_sigusr1 = 1;
		  break;
		case SIGUSR2:
		  have_sigusr2 = 1;
		  break;
		default:
			break;
	}
	init_signals();
}


/********** QSORT functions *******************************/
/*
static int cmpword(DPS_URL_CRD *s1,DPS_URL_CRD *s2){
        if (s1->coord > s2->coord) return -1;
	if (s1->coord < s2->coord) return 1;
	if (s1->url_id > s2->url_id) return 1;
	if (s1->url_id < s2->url_id) return -1;
	return 0;
}
*/
int DpsCmpUrlid(const void *v1, const void *v2) {
  const DPS_URL_CRD_DB *s1 = v1;
  const DPS_URL_CRD_DB *s2 = v2;
	if (s1->url_id < s2->url_id) return -1;
	if (s1->url_id > s2->url_id) return 1;
	if (s1->coord < s2->coord) return -1;
	if (s1->coord > s2->coord) return 1;
	return 0;
}

static int DpsCmpUrlid0(const void *v1, const void *v2) {
  const DPS_URL_CRD *s1 = v1;
  const DPS_URL_CRD *s2 = v2;
	if (s1->url_id < s2->url_id) return -1;
	if (s1->url_id > s2->url_id) return 1;
	if (s1->coord < s2->coord) return -1;
	if (s1->coord > s2->coord) return 1;
	return 0;
}

static int (*DpsCmpPattern)(DPS_URLCRDLIST *, size_t , size_t , const char *);
static int (*DpsCmpPattern_T)(DPS_URLCRDLIST *, size_t , 
#ifdef WITH_MULTIDBADDR
			      DPS_URL_CRD_DB  *Coords,
#else
			      DPS_URL_CRD     *Coords,
#endif
			      DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
			      DPS_URLTRACK    *Track,
#endif
			      const char *);

static int DpsCmpPattern_U(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {
  if (L->Coords[i].url_id > L->Coords[j].url_id) return -1;
  if (L->Coords[i].url_id < L->Coords[j].url_id) return 1;
  return 0;
}
static int DpsCmpPattern_U_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
			     DPS_URL_CRD_DB  *Coords,
#else
			     DPS_URL_CRD     *Coords,
#endif
			     DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
			     DPS_URLTRACK    *Track,
#endif
			     const char *pattern) {
  if (L->Coords[i].url_id > Coords->url_id) return -1;
  if (L->Coords[i].url_id < Coords->url_id) return 1;
  return 0;
}

static int DpsCmpPattern_DRP(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {
  if (L->Data[i].last_mod_time > L->Data[j].last_mod_time) return -1;
  if (L->Data[i].last_mod_time < L->Data[j].last_mod_time) return 1;
  if (L->Coords[i].coord > L->Coords[j].coord) return -1;
  if (L->Coords[i].coord < L->Coords[j].coord) return 1;
  if (L->Data[i].pop_rank > L->Data[j].pop_rank) return -1;
  if (L->Data[i].pop_rank < L->Data[j].pop_rank) return 1;
  return 0;
}

static int DpsCmpPattern_DRP_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
			       DPS_URL_CRD_DB  *Coords,
#else
			       DPS_URL_CRD     *Coords,
#endif
			       DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
			       DPS_URLTRACK    *Track,
#endif
			       const char *pattern) {
  if (L->Data[i].last_mod_time > Data->last_mod_time) return -1;
  if (L->Data[i].last_mod_time < Data->last_mod_time) return 1;
  if (L->Coords[i].coord > Coords->coord) return -1;
  if (L->Coords[i].coord < Coords->coord) return 1;
  if (L->Data[i].pop_rank > Data->pop_rank) return -1;
  if (L->Data[i].pop_rank < Data->pop_rank) return 1;
  return 0;
}

static int DpsCmpPattern_PRD(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {
  if (L->Data[i].pop_rank > L->Data[j].pop_rank) return -1;
  if (L->Data[i].pop_rank < L->Data[j].pop_rank) return 1;
  if (L->Coords[i].coord > L->Coords[j].coord) return -1;
  if (L->Coords[i].coord < L->Coords[j].coord) return 1;
  if (L->Data[i].last_mod_time > L->Data[j].last_mod_time) return -1;
  if (L->Data[i].last_mod_time < L->Data[j].last_mod_time) return 1;
  return 0;
}

static int DpsCmpPattern_PRD_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
			       DPS_URL_CRD_DB  *Coords,
#else
			       DPS_URL_CRD     *Coords,
#endif
			       DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
			       DPS_URLTRACK    *Track,
#endif
			       const char *pattern) {
  if (L->Data[i].pop_rank > Data->pop_rank) return -1;
  if (L->Data[i].pop_rank < Data->pop_rank) return 1;
  if (L->Coords[i].coord > Coords->coord) return -1;
  if (L->Coords[i].coord < Coords->coord) return 1;
  if (L->Data[i].last_mod_time > Data->last_mod_time) return -1;
  if (L->Data[i].last_mod_time < Data->last_mod_time) return 1;
  return 0;
}

static int DpsCmpPattern_RP(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {
  if (L->Coords[i].coord > L->Coords[j].coord) return -1;
  if (L->Coords[i].coord < L->Coords[j].coord) return 1;
  if (L->Data[i].pop_rank > L->Data[j].pop_rank) return -1;
  if (L->Data[i].pop_rank < L->Data[j].pop_rank) return 1;
  return 0;
}

static int DpsCmpPattern_RP_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
			      DPS_URL_CRD_DB  *Coords,
#else
			      DPS_URL_CRD     *Coords,
#endif
			      DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
			      DPS_URLTRACK    *Track,
#endif
			      const char *pattern) {
  if (L->Coords[i].coord > Coords->coord) return -1;
  if (L->Coords[i].coord < Coords->coord) return 1;
  if (L->Data[i].pop_rank > Data->pop_rank) return -1;
  if (L->Data[i].pop_rank < Data->pop_rank) return 1;
  return 0;
}

static int DpsCmpPattern_RPD(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {
  if (L->Coords[i].coord > L->Coords[j].coord) return -1;
  if (L->Coords[i].coord < L->Coords[j].coord) return 1;
  if (L->Data[i].pop_rank > L->Data[j].pop_rank) return -1;
  if (L->Data[i].pop_rank < L->Data[j].pop_rank) return 1;
  if (L->Data[i].last_mod_time > L->Data[j].last_mod_time) return -1;
  if (L->Data[i].last_mod_time < L->Data[j].last_mod_time) return 1;
  return 0;
}

static int DpsCmpPattern_RPD_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
			       DPS_URL_CRD_DB  *Coords,
#else
			       DPS_URL_CRD     *Coords,
#endif
			       DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
			       DPS_URLTRACK    *Track,
#endif
			       const char *pattern) {
  if (L->Coords[i].coord > Coords->coord) return -1;
  if (L->Coords[i].coord < Coords->coord) return 1;
  if (L->Data[i].pop_rank > Data->pop_rank) return -1;
  if (L->Data[i].pop_rank < Data->pop_rank) return 1;
  if (L->Data[i].last_mod_time > Data->last_mod_time) return -1;
  if (L->Data[i].last_mod_time < Data->last_mod_time) return 1;
  return 0;
}

static int DpsCmpPattern_IRPD(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {
  double m1 = L->Data[i].pop_rank, m2 = L->Data[j].pop_rank;
  m1 *= (double)L->Coords[i].coord; m2 *= (double)L->Coords[j].coord;
  if (m1 > m2) return -1;
  if (m1 < m2) return 1;
  if (L->Coords[i].coord > L->Coords[j].coord) return -1;
  if (L->Coords[i].coord < L->Coords[j].coord) return 1;
  if (L->Data[i].pop_rank > L->Data[j].pop_rank) return -1;
  if (L->Data[i].pop_rank < L->Data[j].pop_rank) return 1;
  if (L->Data[i].last_mod_time > L->Data[j].last_mod_time) return -1;
  if (L->Data[i].last_mod_time < L->Data[j].last_mod_time) return 1;
  return 0;
}

static int DpsCmpPattern_IRPD_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
				DPS_URL_CRD_DB  *Coords,
#else
				DPS_URL_CRD     *Coords,
#endif
				DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
				DPS_URLTRACK    *Track,
#endif
				const char *pattern) {
  double m1 = L->Data[i].pop_rank, m2 = Data->pop_rank;
  m1 *= (double)L->Coords[i].coord; m2 *= (double)Coords->coord;
  if (m1 > m2) return -1;
  if (m1 < m2) return 1;
  if (L->Coords[i].coord > Coords->coord) return -1;
  if (L->Coords[i].coord < Coords->coord) return 1;
  if (L->Data[i].pop_rank > Data->pop_rank) return -1;
  if (L->Data[i].pop_rank < Data->pop_rank) return 1;
  if (L->Data[i].last_mod_time > Data->last_mod_time) return -1;
  if (L->Data[i].last_mod_time < Data->last_mod_time) return 1;
  return 0;
}

static inline int DpsCmpPattern_full(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {
    double m1, m2;
  
  for(; *pattern != '\0'; pattern++) {
    switch(*pattern) {
    case 'U':
      if (L->Coords[i].url_id > L->Coords[j].url_id) return -1;
      if (L->Coords[i].url_id < L->Coords[j].url_id) return 1;
      break;
    case 'u':
      if (L->Coords[i].url_id < L->Coords[j].url_id) return -1;
      if (L->Coords[i].url_id > L->Coords[j].url_id) return 1;
      break;
    case 'R':
      if (L->Coords[i].coord > L->Coords[j].coord) return -1;
      if (L->Coords[i].coord < L->Coords[j].coord) return 1;
      break;
    case 'r':
      if (L->Coords[i].coord > L->Coords[j].coord) return 1;
      if (L->Coords[i].coord < L->Coords[j].coord) return -1;
      break;
    case 'P':
      if (L->Data[i].pop_rank > L->Data[j].pop_rank) return -1;
      if (L->Data[i].pop_rank < L->Data[j].pop_rank) return 1;
      break;
    case 'p':
      if (L->Data[i].pop_rank > L->Data[j].pop_rank) return 1;
      if (L->Data[i].pop_rank < L->Data[j].pop_rank) return -1;
      break;
    case 'D':
      if (L->Data[i].last_mod_time > L->Data[j].last_mod_time) return -1;
      if (L->Data[i].last_mod_time < L->Data[j].last_mod_time) return 1;
      break;
    case 'd':
      if (L->Data[i].last_mod_time > L->Data[j].last_mod_time) return 1;
      if (L->Data[i].last_mod_time < L->Data[j].last_mod_time) return -1;
      break;
    case 'I':
    	m1 = L->Data[i].pop_rank; m2 = L->Data[j].pop_rank;
	m1 *= (double)L->Coords[i].coord; m2 *= (double)L->Coords[j].coord;
	if (m1 > m2) return -1;
	if (m1 < m2) return 1;
        break;
    case 'i':
    	m1 = L->Data[i].pop_rank; m2 = L->Data[j].pop_rank;
	m1 *= (double)L->Coords[i].coord; m2 *= (double)L->Coords[j].coord;
	if (m1 > m2) return 1;
	if (m1 < m2) return -1;
        break;
    case 'A':
    	m1 = L->Data[i].pop_rank * 1000.0; m2 = L->Data[j].pop_rank * 1000.0;
	m1 += (double)L->Coords[i].coord; m2 += (double)L->Coords[j].coord;
	if (m1 > m2) return -1;
	if (m1 < m2) return 1;
        break;
    case 'a':
    	m1 = L->Data[i].pop_rank * 1000.0, m2 = L->Data[j].pop_rank * 1000.0;
	m1 += (double)L->Coords[i].coord; m2 += (double)L->Coords[j].coord;
	if (m1 > m2) return 1;
	if (m1 < m2) return -1;
        break;
    }
  }
  return 0;
}

static inline int DpsCmpPattern_full_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
				       DPS_URL_CRD_DB  *Coords,
#else
				       DPS_URL_CRD     *Coords,
#endif
				       DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
				       DPS_URLTRACK    *Track,
#endif
				       const char *pattern) {
    double m1, m2;
  
  for(; *pattern != '\0'; pattern++) {
    switch(*pattern) {
    case 'U':
      if (L->Coords[i].url_id > Coords->url_id) return -1;
      if (L->Coords[i].url_id < Coords->url_id) return 1;
      break;
    case 'u':
      if (L->Coords[i].url_id < Coords->url_id) return -1;
      if (L->Coords[i].url_id > Coords->url_id) return 1;
      break;
    case 'R':
      if (L->Coords[i].coord > Coords->coord) return -1;
      if (L->Coords[i].coord < Coords->coord) return 1;
      break;
    case 'r':
      if (L->Coords[i].coord > Coords->coord) return 1;
      if (L->Coords[i].coord < Coords->coord) return -1;
      break;
    case 'P':
      if (L->Data[i].pop_rank > Data->pop_rank) return -1;
      if (L->Data[i].pop_rank < Data->pop_rank) return 1;
      break;
    case 'p':
      if (L->Data[i].pop_rank > Data->pop_rank) return 1;
      if (L->Data[i].pop_rank < Data->pop_rank) return -1;
      break;
    case 'D':
      if (L->Data[i].last_mod_time > Data->last_mod_time) return -1;
      if (L->Data[i].last_mod_time < Data->last_mod_time) return 1;
      break;
    case 'd':
      if (L->Data[i].last_mod_time > Data->last_mod_time) return 1;
      if (L->Data[i].last_mod_time < Data->last_mod_time) return -1;
      break;
    case 'I':
      	m1 = L->Data[i].pop_rank; m2 = Data->pop_rank;
	m1 *= (double)L->Coords[i].coord; m2 *= (double)Coords->coord;
	if (m1 > m2) return -1;
	if (m1 < m2) return 1;
      	break;
    case 'i':
	m1 = L->Data[i].pop_rank; m2 = Data->pop_rank;
	m1 *= (double)L->Coords[i].coord; m2 *= (double)Coords->coord;
	if (m1 > m2) return 1;
	if (m1 < m2) return -1;
      	break;
    case 'A':
	m1 = L->Data[i].pop_rank * 1000.0, m2 = Data->pop_rank * 1000.0;
	m1 += (double)L->Coords[i].coord; m2 += (double)Coords->coord;
	if (m1 > m2) return -1;
	if (m1 < m2) return 1;
      	break;
    case 'a':
	m1 = L->Data[i].pop_rank * 1000.0, m2 = Data->pop_rank * 1000.0;
	m1 += (double)L->Coords[i].coord; m2 += (double)Coords->coord;
	if (m1 > m2) return 1;
	if (m1 < m2) return -1;
      	break;
    }
  }
  return 0;
}

static int DpsCmpSiteid(DPS_URLCRDLIST *L, size_t i, size_t j, const char *pattern) {

        if (L->Data[i].site_id < L->Data[j].site_id) return -1;
	if (L->Data[i].site_id > L->Data[j].site_id) return 1;
	return DpsCmpPattern(L, i, j, pattern);
}

static int DpsCmpSiteid_T(DPS_URLCRDLIST *L, size_t i, 
#ifdef WITH_MULTIDBADDR
			  DPS_URL_CRD_DB  *Coords,
#else
			  DPS_URL_CRD     *Coords,
#endif
			  DPS_URLDATA     *Data,
#ifdef WITH_REL_TRACK
			  DPS_URLTRACK    *Track,
#endif
			  const char *pattern) {

        if (L->Data[i].site_id < Data->site_id) return -1;
	if (L->Data[i].site_id > Data->site_id) return 1;
	return DpsCmpPattern_T(L, i, Coords, Data,
#ifdef WITH_REL_TRACK
			       Track,
#endif
			       pattern);
}


/****************************************************/
/*
void DpsSortSearchWordsByWeight(DPS_URL_CRD *wrd,size_t num){
	if (wrd != NULL && num > 1) DpsSort((void*)wrd, num, sizeof(*wrd), (qsort_cmp)cmpword);
	return;
}
*/
void DpsSortSearchWordsByURL(DPS_URL_CRD_DB *wrd, size_t num){
	if(wrd != NULL && num > 1) DpsPreSort((void*)wrd, num, sizeof(*wrd),( qsort_cmp)DpsCmpUrlid);
	return;
}

void DpsSortSearchWordsByURL0(DPS_URL_CRD *wrd, size_t num){
	if(wrd != NULL && num > 1) DpsPreSort((void*)wrd, num, sizeof(*wrd),( qsort_cmp)DpsCmpUrlid0);
	return;
}

#define med3a(func, a, b, c, L, pattern)  ((func(L, a, b, pattern) < 0) ? \
					  ((func(L, b, c, pattern) < 0) ? (b) : ((func(L, a, c, pattern) < 0) ? (c) : (a))) \
					  : ((func(L, b, c, pattern) > 0) ? (b) : ((func(L, a, c, pattern) <= 0) ? (a) : (c))))

#define med3b(func, a, b, c, L, pattern)  ((func(L, a, b, pattern) < 0) ? \
					  ((func(L, b, c, pattern) <= 0) ? (b) : ((func(L, a, c, pattern) < 0) ? (c) : (a))) \
					  : ((func(L, b, c, pattern) >= 0) ? (b) : ((func(L, a, c, pattern) < 0) ? (a) : (c))))

#define med3c(func, a, b, c, L, pattern)  ((func(L, a, b, pattern) < 0) ? \
					  ((func(L, b, c, pattern) < 0) ? (b) : ((func(L, a, c, pattern) <= 0) ? (c) : (a))) \
					  : ((func(L, b, c, pattern) > 0) ? (b) : ((func(L, a, c, pattern) < 0) ? (a) : (c))))

#define MIN_SLICE 9
/*#define MIDDLE_SLICE 40*/

#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)
#define THREAD_SLICE 10240 /* 128 */
#endif

typedef struct {
  DPS_RESULT *Res;
  DPS_URLCRDLIST *L;
  const char *pattern;
  size_t l, r;
  int merge;
  int level;
} DPS_SORT_PARAM;


#ifdef WITH_REL_TRACK

#define SWAP_BY_SITE(i,j) if (i != j) {	\
      Crd = L->Coords[j]; \
      Dat = L->Data[j];   \
      L->Coords[j] = L->Coords[i]; \
      L->Data[j] = L->Data[i]; \
      L->Coords[i] = Crd; \
      L->Data[i] = Dat; \
      Trk = L->Track[j]; \
      L->Track[j] = L->Track[i]; \
      L->Track[i] = Trk; \
      if (merge) { \
	PerS = Res->PerSite[j]; \
	Res->PerSite[j] = Res->PerSite[i]; \
	Res->PerSite[i] = PerS; \
      } \
   }

#else
#define SWAP_BY_SITE(i,j) if (i != j) { \
      Crd = L->Coords[j]; \
      Dat = L->Data[j];   \
      L->Coords[j] = L->Coords[i]; \
      L->Data[j] = L->Data[i]; \
      L->Coords[i] = Crd; \
      L->Data[i] = Dat; \
      if (merge) { \
	PerS = Res->PerSite[j]; \
	Res->PerSite[j] = Res->PerSite[i]; \
	Res->PerSite[i] = PerS; \
      } \
   }


#endif


static size_t DpsPartitionSearchWordsBySite(DPS_RESULT *Res, DPS_URLCRDLIST *L, size_t p, size_t r, const char *pattern, int merge) {
  DPS_URL_CRD_DB Crd;
  DPS_URLDATA Dat;
#ifdef WITH_REL_TRACK
  DPS_URLTRACK Trk;
#endif
  size_t i = p, j = r, m, pl, pm, pn, d;
  size_t PerS;

  pm = (p + r) / 2;
  pl = p;
  pn = r;
  d = r - p;
  d /= 8;
  pl = med3c(DpsCmpSiteid, pl, pl + d, pl + 2 * d, L, pattern);
  pm = med3b(DpsCmpSiteid, pm - d, pm, pm + d, L, pattern);
  pn = med3a(DpsCmpSiteid, pn - 2 * d, pn - d, pn, L, pattern);
  
  m = med3b(DpsCmpSiteid, pl, pm, pn, L, pattern);

#if 0
  SWAP_BY_SITE(m,r);
  j = p;
  { 
    int u = 1;
    int rc;
    for (i = p; i < r; i++) {
      rc = DpsCmpSiteid(L, i, r, pattern);
      if ( rc < 0 || ((rc == 0) && u) ) {
	SWAP_BY_SITE(i,j);
	j++;
	u = ((j - p) < (r - j));
      }
    }
  }
  SWAP_BY_SITE(r,j);
#else

  SWAP_BY_SITE(m,r);
  for (i = p, j = r - 1; i <= j;) {
    while ( DpsCmpSiteid(L, i, r /*m*/, pattern) <= 0 ) {
      if (i == j) return i;
      i++;
    }
    while ( DpsCmpSiteid(L, j, r /*m*/, pattern) >= 0 ) {
      if (i == j) {
	if (i > p) return i - 1;
	return i;
      }
      j--;
    }
    SWAP_BY_SITE(i,j);
    i++;
    j--;
  }

#endif

  return j;
}



static void * DpsQsortSearchWordsBySite(void *arg) {
  DPS_SORT_PARAM PP;
  DPS_SORT_PARAM *P = (DPS_SORT_PARAM*)arg;
  DPS_URL_CRD_DB Crd;
  DPS_URLDATA Dat;
#ifdef WITH_REL_TRACK
  DPS_URLTRACK Trk;
#endif
  DPS_RESULT *Res;
  DPS_URLCRDLIST *L;
  size_t i, j;
  size_t l, r, q, d;
  size_t PerS;
  int rc, merge;
  size_t iMin;
#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)
  DPS_SORT_PARAM PAR;
  pthread_t tid = 0;
#endif

 DpsQsortBySiteLoop:
  l = P->l; r = P->r;
  if (l >= r) {
#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)
    if (tid) pthread_join(tid, NULL);
#endif
    return NULL;
  }
  d = r - l;
  if (d <= MIN_SLICE) {
    Res = P->Res;
    L = P->L;

#if 0
    for (j = l + 1; j <= r; j++) {
      for (i = j; (i > l) && (DpsCmpSiteid(L, i - 1, i, P->pattern) > 0); i--) {
	SWAP_BY_SITE(i,i-1);

      }
    }
#else
    merge = P->merge;

    for (i = l; i < r; i++) {
      iMin = i;
      for (j = i + 1; j <= r; j++) {
	if (DpsCmpSiteid(L, j, iMin, P->pattern) < 0) {
	  iMin = j;
	}
      }
      if (iMin != i) {
	SWAP_BY_SITE(i, iMin);
      }
    }
#endif

#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)
    if (tid) pthread_join(tid, NULL);
#endif
    return NULL;
  }
  q = DpsPartitionSearchWordsBySite(P->Res, P->L, l, r, P->pattern, P->merge);

  PP = *P;
  if ((q - l) > (r - q)) {
    PP.l = q + 1;
    P->r = r = q;
  } else {
    PP.r = q;
    P->l = l = q + 1;
  }

#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)

  if (d >= THREAD_SLICE && PP.level > 0) {
#if defined(HAVE_PTHREAD_TRYJOIN_NP_PROTO)
      rc = 0;
      if (tid) {
	  rc = pthread_tryjoin_np(tid, NULL);
	  if (rc == 0) tid = 0;
      }
      if (rc == 0) {
	  tid = 0;
	  PP.level--;
	  PAR = PP;
	  if (pthread_create(&tid, NULL, &DpsQsortSearchWordsBySite, &PAR) != 0) {
	      DpsQsortSearchWordsBySite(&PP);
	      tid = 0;
	  }
      } else {
	  DpsQsortSearchWordsBySite(&PP);
      }
#else
    if (tid) pthread_join(tid, NULL);
    tid = 0;
    PP.level--;
    PAR = PP;
    if (pthread_create(&tid, NULL, &DpsQsortSearchWordsBySite, &PAR) != 0) {
      DpsQsortSearchWordsBySite(&PP);
      tid = 0;
    }
#endif
  } else 
#endif
  {
    DpsQsortSearchWordsBySite(&PP);
  }
  goto DpsQsortBySiteLoop;

}


static void DpsHeapSortSearchWordsBySite(void *arg) {
  DPS_URLDATA Dat;
  DPS_SORT_PARAM *P = (DPS_SORT_PARAM*)arg;
  DPS_URLCRDLIST *L = P->L;
  DPS_URLDATA	*Data = L->Data;
  DPS_RESULT *Res = P->Res;
  const char *pattern = P->pattern;
  size_t n = P->r + 1, i = (n / 2), parent, child;
  size_t PerS;
  
#ifdef WITH_MULTIDBADDR
  DPS_URL_CRD_DB Crd;
  DPS_URL_CRD_DB  *Coords = L->Coords;
#else
  DPS_URL_CRD Crd;
  DPS_URL_CRD	*Coords = L->Coords;
#endif

#ifdef WITH_REL_TRACK
  DPS_URLTRACK Trk;
  DPS_URLTRACK    *Track = L->Track;
#endif

  if (n < 2) return;
  while (1) {
    if (i > 0) {
      i--;
      Crd = Coords[i];
      Dat = Data[i];
#ifdef WITH_REL_TRACK
      Trk = Track[i];
#endif
      if (P->merge) {
	PerS = Res->PerSite[i];
      }
    } else {
      n--;
      if (n == 0) {
	return;
      }
      Crd = Coords[n];
      Dat = Data[n];
#ifdef WITH_REL_TRACK
      Trk = Track[n];
#endif
      Coords[n] = Coords[0];
      Data[n] = Data[0];
#ifdef WITH_REL_TRACK
      Track[n] = Track[0];
#endif
      if (P->merge) {
	PerS = Res->PerSite[n];
	Res->PerSite[n] = Res->PerSite[0];
      }
    }

    parent = i;
    child = (i * 2) + 1;

    while (child < n) {
      if (child + 1 < n && DpsCmpSiteid(L, child + 1, child, pattern) > 0) {
	child++;
      }
      if (DpsCmpSiteid_T(L, child, &Crd, &Dat,
#ifdef WITH_REL_TRACK
			  &Trk,
#endif
			  pattern) > 0) {
	Coords[parent] = Coords[child];
	Data[parent] = Data[child];
#ifdef WITH_REL_TRACK
	Track[parent] = Track[child];
#endif
	if (P->merge) {
	  Res->PerSite[parent] = Res->PerSite[child];
	}
	parent = child;
	child = parent * 2 + 1;
      } else {
	break;
      }
    }
    Coords[parent] = Crd;
    Data[parent] = Dat;
#ifdef WITH_REL_TRACK
    Track[parent] = Trk;
#endif
    if (P->merge) {
      Res->PerSite[parent] = PerS;
    }
  }
  return;
}




void DpsSortSearchWordsBySite(DPS_RESULT *Res, DPS_URLCRDLIST *L, size_t num, const char *pattern) {
  
  if (num > 1) {
    DPS_SORT_PARAM P;
    P.Res = Res;
    P.L = L;
    P.l = 0;
    P.r = num - 1;
    P.pattern = pattern;
    P.merge = (Res->PerSite != NULL);
    P.level = 3;
    DpsCmpPattern = &DpsCmpPattern_full;
    DpsCmpPattern_T = &DpsCmpPattern_full_T;
    switch(*pattern) {
    case 'D': 
      if (pattern[1] == 'R' && pattern[2] == 'P' && pattern[3] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_DRP; 
	DpsCmpPattern_T = &DpsCmpPattern_DRP_T; 
      }
      break;
    case 'I': 
      if (pattern[1] == 'R' && pattern[2] == 'P' && pattern[3] == 'D' && pattern[4] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_IRPD;
	DpsCmpPattern_T = &DpsCmpPattern_IRPD_T;
      }
      break;
    case 'P': 
      if (pattern[1] == 'R' && pattern[2] == 'D' && pattern[3] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_PRD;
	DpsCmpPattern_T = &DpsCmpPattern_PRD_T;
      }
      break;
    case 'R': 
      if (pattern[1] == 'P' ) {
	if (pattern[2] == '\0') {
	  DpsCmpPattern = &DpsCmpPattern_RP; 
	  DpsCmpPattern_T = &DpsCmpPattern_RP_T;
	} else if (pattern[2] == 'D' && pattern[3] == '\0') {
	  DpsCmpPattern = &DpsCmpPattern_RPD;
	  DpsCmpPattern_T = &DpsCmpPattern_RPD_T;
	}
      }
      break;
    case 'U': 
      if (pattern[1] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_U;
	DpsCmpPattern_T = &DpsCmpPattern_U_T;
      }
      break;
    }
#if HEAP_SORTING
    DpsHeapSortSearchWordsBySite(&P);
#elif QUICK_SORTING
    DpsQsortSearchWordsBySite(&P);
#else
#error Not Results Sorting Method defined.
#endif
  }

}


#ifdef WITH_REL_TRACK

#define SWAP_BY_PATTERN(i,j) if (i != j) { \
      Crd = L->Coords[j]; \
      Dat = L->Data[j]; \
      L->Coords[j] = L->Coords[i]; \
      L->Data[j] = L->Data[i];     \
      L->Coords[i] = Crd;          \
      L->Data[i] = Dat;                    \
      Trk = L->Track[j];                   \
      L->Track[j] = L->Track[i];           \
      L->Track[i] = Trk;                   \
      if (Res->PerSite) {                  \
	Cnt = Res->PerSite[j];             \
	Res->PerSite[j] = Res->PerSite[i]; \
	Res->PerSite[i] = Cnt;             \
      }                                    \
  }

#else

#define SWAP_BY_PATTERN(i,j) if (i != j) { \
      Crd = L->Coords[j]; \
      Dat = L->Data[j]; \
      L->Coords[j] = L->Coords[i]; \
      L->Data[j] = L->Data[i];     \
      L->Coords[i] = Crd;          \
      L->Data[i] = Dat;                    \
      if (Res->PerSite) {                  \
	Cnt = Res->PerSite[j];             \
	Res->PerSite[j] = Res->PerSite[i]; \
	Res->PerSite[i] = Cnt;             \
      }                                    \
  }
#endif

static size_t DpsPartitionSearchWordsByPattern(DPS_RESULT *Res, DPS_URLCRDLIST *L, size_t p, size_t r, const char *pattern) {
  size_t i = p, j = r, m, pl, pm, pn, d;
  size_t Cnt = 1;
  DPS_URL_CRD_DB Crd;
  DPS_URLDATA Dat;
#ifdef WITH_REL_TRACK
  DPS_URLTRACK Trk;
#endif
  int u, rc;

  pm = (p + r) / 2;
  pl = p;
  pn = r;
  d = r - p;
  d /= 8;
  pl = med3c(DpsCmpPattern, pl, pl + d, pl + 2 * d, L, pattern);
  pm = med3b(DpsCmpPattern, pm - d, pm, pm + d, L, pattern);
  pn = med3a(DpsCmpPattern, pn - 2 * d, pn - d, pn, L, pattern);
  
  m = med3b(DpsCmpPattern, pl, pm, pn, L, pattern);

#if 0
  SWAP_BY_PATTERN(m,r);
  j = p;
  
  u = ((j - p) < (r - j));
  for (i = p; i < r; i++) {
      rc = DpsCmpPattern(L, i, r, pattern);
      if ( rc < 0 || ((rc == 0) && u) ) {
	  SWAP_BY_PATTERN(i,j);
	  j++;
	  u = ((j - p) < (r - j));
      }
  }
  
  SWAP_BY_PATTERN(r,j);
#else

  SWAP_BY_PATTERN(m,r);
  for (i = p, j = r - 1; i <= j;) {
    while ( DpsCmpPattern(L, i, r /*m*/, pattern) <= 0 ) {
      if (i == j) return i;
      i++;
    }
    while ( DpsCmpPattern(L, j, r /*m*/, pattern) >= 0 ) {
      if (i == j) {
	if (i > p) return i - 1;
	return i;
      }
      j--;
    }
    SWAP_BY_PATTERN(i,j);
    i++;
    j--;
  }

#endif

  return j;
}


static void * DpsQsortSearchWordsByPattern(void *arg) {
  DPS_SORT_PARAM PP;
  DPS_SORT_PARAM *P = (DPS_SORT_PARAM*)arg;
  size_t i, j;
  size_t l, r, c, d;
  size_t Cnt = 1;
  size_t iMin;
  int rc;
  DPS_URL_CRD_DB Crd;
  DPS_URLDATA Dat;
  DPS_RESULT *Res;
  DPS_URLCRDLIST *L;
#ifdef WITH_REL_TRACK
  DPS_URLTRACK Trk;
#endif
#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)
  DPS_SORT_PARAM PAR;
  pthread_t tid = 0;
#endif

 DpsQsortByPatternLoop:
  l = P->l; r = P->r;
  if (l >= r) {
#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)
    if (tid) pthread_join(tid, NULL); 
#endif
    return NULL;
  }
  d = r - l;
  if (d <= MIN_SLICE) {
    Res = P->Res;
    L = P->L;

#if 0
    for (j = l + 1; j <= r; j++) {
      for (i = j; (i > l) && (DpsCmpPattern(L, i - 1, i, P->pattern) > 0); i--) {
	SWAP_BY_PATERN(i, i-1);
      }
    }
#else

    for (i = l; i < r; i++) {
      iMin = i;
      for (j = i + 1; j <= r; j++) {
	if (DpsCmpPattern(L, j, iMin, P->pattern) <= 0) {
	  iMin = j;
	}
      }
      if (iMin != i) {
	SWAP_BY_PATTERN(i, iMin);
      }
    }
#endif

#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)
    if (tid) pthread_join(tid, NULL);
#endif
    return NULL;
  }
  c = DpsPartitionSearchWordsByPattern(P->Res, P->L, l, r, P->pattern);

  PP = *P;
  if ((c - l) > (r - c)) {
    PP.l = c + 1;
    P->r = r = c;
  } else {
    PP.r = c;
    P->l = l = c + 1;
  }

#if defined(HAVE_PTHREAD) && defined(MULTITHREADED_SORT)

  if (d >= THREAD_SLICE && PP.level > 0) {
#if defined(HAVE_PTHREAD_TRYJOIN_NP_PROTO)
      rc = 0;
      if (tid) {
	  rc = pthread_tryjoin_np(tid, NULL);
	  if (rc == 0) tid = 0;
      }
      if (rc == 0) {
	  tid = 0;
	  PP.level--;
	  PAR = PP;
	  if (pthread_create(&tid, NULL, &DpsQsortSearchWordsByPattern, &PAR) != 0) {
	      DpsQsortSearchWordsByPattern(&PP);
	      tid = 0;
	  }
      } else {
	  DpsQsortSearchWordsByPattern(&PP);
      }
#else
    if (tid) pthread_join(tid, NULL);
    tid = 0;
    PP.level--;
    PAR = PP;
    if (pthread_create(&tid, NULL, &DpsQsortSearchWordsByPattern, &PAR) != 0) {
      DpsQsortSearchWordsByPattern(&PP);
      tid = 0;
    }
#endif
  } else 
#endif
  {
    DpsQsortSearchWordsByPattern(&PP);
  }
  goto DpsQsortByPatternLoop;

}


static void DpsHeapSortSearchWordsByPattern(void *arg) {
  DPS_URLDATA Dat;
  DPS_SORT_PARAM *P = (DPS_SORT_PARAM*)arg;
  DPS_URLCRDLIST *L = P->L;
  DPS_URLDATA	*Data = L->Data;
  DPS_RESULT *Res = P->Res;
  const char *pattern = P->pattern;
  size_t n = P->r + 1, i = (n / 2), parent, child;
  size_t PerS;
  
#ifdef WITH_MULTIDBADDR
  DPS_URL_CRD_DB Crd;
  DPS_URL_CRD_DB  *Coords = L->Coords;
#else
  DPS_URL_CRD Crd;
  DPS_URL_CRD	*Coords = L->Coords;
#endif

#ifdef WITH_REL_TRACK
  DPS_URLTRACK Trk;
  DPS_URLTRACK    *Track = L->Track;
#endif

  if (n < 2) return;
  while (1) {
    if (i > 0) {
      i--;
      Crd = Coords[i];
      Dat = Data[i];
#ifdef WITH_REL_TRACK
      Trk = Track[i];
#endif
      if (P->merge) {
	PerS = Res->PerSite[i];
      }
    } else {
      n--;
      if (n == 0) {
	return;
      }
      Crd = Coords[n];
      Dat = Data[n];
#ifdef WITH_REL_TRACK
      Trk = Track[n];
#endif
      Coords[n] = Coords[0];
      Data[n] = Data[0];
#ifdef WITH_REL_TRACK
      Track[n] = Track[0];
#endif
      if (P->merge) {
	PerS = Res->PerSite[n];
	Res->PerSite[n] = Res->PerSite[0];
      }
    }

    parent = i;
    child = (i * 2) + 1;

    while (child < n) {
      if (child + 1 < n && DpsCmpPattern(L, child + 1, child, pattern) > 0) {
	child++;
      }
      if (DpsCmpPattern_T(L, child, &Crd, &Dat,
#ifdef WITH_REL_TRACK
			  &Trk,
#endif
			  pattern) > 0) {
	Coords[parent] = Coords[child];
	Data[parent] = Data[child];
#ifdef WITH_REL_TRACK
	Track[parent] = Track[child];
#endif
	if (P->merge) {
	  Res->PerSite[parent] = Res->PerSite[child];
	}
	parent = child;
	child = parent * 2 + 1;
      } else {
	break;
      }
    }
    Coords[parent] = Crd;
    Data[parent] = Dat;
#ifdef WITH_REL_TRACK
    Track[parent] = Trk;
#endif
    if (P->merge) {
      Res->PerSite[parent] = PerS;
    }
  }
  return;
}


void DpsSortSearchWordsByPattern(DPS_RESULT *Res, DPS_URLCRDLIST *L, size_t num, const char *pattern) {

  if (num > 1) {
    DPS_SORT_PARAM P;
    P.Res = Res;
    P.L = L;
    P.l = 0;
    P.r = num - 1;
    P.merge = 0;
    P.level = 3;
    P.pattern = pattern;
    DpsCmpPattern = &DpsCmpPattern_full;
    DpsCmpPattern_T = &DpsCmpPattern_full_T;
    switch(*pattern) {
    case 'D': 
      if (pattern[1] == 'R' && pattern[2] == 'P' && pattern[3] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_DRP;
 	DpsCmpPattern_T = &DpsCmpPattern_DRP_T;
     }
      break;
    case 'I': 
      if (pattern[1] == 'R' && pattern[2] == 'P' && pattern[3] == 'D' && pattern[4] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_IRPD;
	DpsCmpPattern_T = &DpsCmpPattern_IRPD_T;
      }
      break;
    case 'P': 
      if (pattern[1] == 'R' && pattern[2] == 'D' && pattern[3] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_PRD;
	DpsCmpPattern_T = &DpsCmpPattern_PRD_T;
      }
      break;
    case 'R': 
      if (pattern[1] == 'P' ) {
	if (pattern[2] == '\0') {
	  DpsCmpPattern = &DpsCmpPattern_RP; 
	  DpsCmpPattern_T = &DpsCmpPattern_RP_T; 
	} else if (pattern[2] == 'D' && pattern[3] == '\0') {
	  DpsCmpPattern = &DpsCmpPattern_RPD;
	  DpsCmpPattern_T = &DpsCmpPattern_RPD_T;
	}
      }
      break;
    case 'U': 
      if (pattern[1] == '\0') {
	DpsCmpPattern = &DpsCmpPattern_U;
 	DpsCmpPattern_T = &DpsCmpPattern_U_T;
     }
      break;
    }
#if HEAP_SORTING
    DpsHeapSortSearchWordsByPattern(&P);
#elif QUICK_SORTING
    DpsQsortSearchWordsByPattern(&P);
#else
#error Not Results Sorting Method defined.
#endif
  }

}


static int DpsExpandWord(DPS_AGENT *query, DPS_RESULT *Res, DPS_WIDEWORD *OWord, DPS_PREPARE_STATE *state) {
  DPS_PREPARE_STATE local = *state;
  DPS_WIDEWORDLIST * forms;
  dpsunicode_t *af_uwrd = NULL, *de_uwrd = NULL, *orig_uwrd = OWord->uword;
  char *clex;
  DPS_ACRONYM *first, *last;
  int Origin = OWord->origin;
  size_t ORDER_ADD = state->order;
  size_t frm;
  size_t z;
  int i;
#if 0 && defined HAVE_ASPELL
  AspellCanHaveError *ret;
  AspellSpeller *speller = NULL;
  AspellWordList *suggestions;
  AspellStringEnumeration *elements;
  char *lcsword;
  size_t af_len;

  if (state->sp && query->Flags.use_aspellext && state->have_bukva_forte) {
/**************************/
    DPS_GETLOCK(query, DPS_LOCK_ASPELL);
    ret = new_aspell_speller(query->aspell_config);
    if (aspell_error(ret) != 0) {
      DpsLog(query, DPS_LOG_ERROR, "aspell error: %s", aspell_error_message(ret));
      delete_aspell_can_have_error(ret);
    } else {
      speller = to_aspell_speller(ret);
      if (aspell_speller_check(speller, (const char *)OWord->uword, -1) == 0) {
	lcsword = (char*)DpsMalloc(12*query->WordParam.max_word_len);
	if (lcsword == NULL) {
	  delete_aspell_speller(speller);
	  return DPS_ERROR;
	}
	suggestions = aspell_speller_suggest(speller, (const char *)OWord->uword, -1);
	elements = aspell_word_list_elements(suggestions);
	for (i = 0; (i < 2) && ((af_uwrd = (dpsunicode_t*)aspell_string_enumeration_next(elements)) != NULL); ) { 

	  DpsUniStrToLower(af_uwrd);
	  af_len = DpsUniLen(af_uwrd);
	  if (af_len < OWord->ulen && OWord->ulen - af_len > 1) { af_uwrd = NULL; continue;}
	  if (DpsUniStrCmp(af_uwrd, OWord->uword) == 0) { af_uwrd = NULL; continue;}
	  DpsConv(&query->uni_lc, lcsword, 12*query->WordParam.max_word_len, 
		  (char*)(af_uwrd), sizeof(af_uwrd[0])*(DpsUniLen(af_uwrd)+1));
	  if ((strchr(lcsword, ' ') != NULL) || (strchr(lcsword, '-') != NULL)) { af_uwrd = NULL; continue;}
	  local.cmd = DPS_STACK_WORD;
	  local.add_cmd = DPS_STACK_OR;
	  local.order = ORDER_ADD;
	  local.origin = (Origin & ~1) | DPS_WORD_ORIGIN_ASPELL;
	  if (DpsAddStackItem(query, Res, &local, lcsword, af_uwrd) != DPS_OK) {
	    DPS_FREE(lcsword);
	    delete_aspell_string_enumeration(elements);
	    delete_aspell_speller(speller);
	    return DPS_ERROR;
	  }
	  af_uwrd = NULL;
	  i++;
	}
	DPS_FREE(lcsword);
	delete_aspell_string_enumeration(elements);
      }
      delete_aspell_speller(speller);
    }
    DPS_RELEASELOCK(query, DPS_LOCK_ASPELL);
  }
#endif

  if(state->sp && !(Origin & DPS_WORD_ORIGIN_SPELL) && (forms = DpsAllForms(query, OWord)) ) {

    for(frm = 0; frm < forms->nwords; frm++) {
      if (DpsUniStrCmp(OWord->uword, forms->Word[frm].uword) == 0) continue;
      DpsConv(&query->uni_lc, OWord->word, 12 * query->WordParam.max_word_len,
	      (char*)(forms->Word[frm].uword),
	      sizeof(forms->Word[frm].uword[0])*(DpsUniLen(forms->Word[frm].uword)+1));

      local.cmd = DPS_STACK_WORD;
      local.add_cmd = DPS_STACK_OR;
      local.order = ORDER_ADD;
      local.origin = ((Origin | forms->Word[frm].origin) & ~1);
      if (DpsAddStackItem(query, Res, &local, OWord->word, forms->Word[frm].uword) != DPS_OK) {
	return DPS_ERROR;
      }
      OWord->uword = forms->Word[frm].uword;
      OWord->ulen = DpsUniLen(forms->Word[frm].uword);
      if (!(state->nphrasecmd & 1) && (first = DpsAcronymListFind(&query->Conf->Acronyms, OWord, &last)) != NULL) {

	while(first <= last) {
	  local.cmd = DPS_STACK_OR;
	  local.add_cmd = DPS_STACK_OR;
	  if (DpsAddStackItem(query, Res, &local, NULL, NULL) != DPS_OK) {
	    return DPS_ERROR;
	  }
	  if (first->unroll.nwords > 1) {
	    local.cmd = DPS_STACK_PHRASE_LEFT;
	    local.add_cmd = DPS_STACK_OR;
	    if (DpsAddStackItem(query, Res, &local, NULL, NULL) != DPS_OK) {
	      return DPS_ERROR;
	    }
	  }
	  { 
	    for (z = 0; z < first->unroll.nwords; z++) {
	      if (z) {
		ORDER_ADD++;
		local.cmd = DPS_STACK_AND;
		local.add_cmd = DPS_STACK_OR;
		if (DpsAddStackItem(query, Res, &local, NULL, NULL) != DPS_OK) {
		  return DPS_ERROR;
		}
	      }
	      local.cmd = DPS_STACK_LEFT;
	      if (DpsAddStackItem(query, Res, &local, NULL, NULL) != DPS_OK) {
		return DPS_ERROR;
	      }
					
#if 1
	      local.cmd = DPS_STACK_WORD;
	      local.add_cmd = DPS_STACK_OR;
	      local.order = ORDER_ADD;
	      local.origin = DPS_WORD_ORIGIN_ACRONYM;
	      if (DpsAddStackItem(query, Res, &local, first->unroll.Word[z].word, first->unroll.Word[z].uword) != DPS_OK) {
		return DPS_ERROR;
	      }
	      OWord->uword = first->unroll.Word[z].uword;
	      OWord->ulen = DpsUniLen(first->unroll.Word[z].uword);
	      OWord->origin = DPS_WORD_ORIGIN_ACRONYM;
	      local.order = ORDER_ADD;
	      local.nphrasecmd = 1 /*(first->unroll.nwords > 1) ? 1 : 0*/;
	      local.have_bukva_forte = 0;
	      if (DPS_OK != DpsExpandWord(query, Res, OWord, &local)) {
		return DPS_ERROR;
	      }
#else
	      if(DpsStopListFind(&query->Conf->StopWords, first->unroll.Word[z].uword, (query->flags & DPS_FLAG_STOPWORDS_LOOSE) ? state->qlang : "") ||
		 (query->WordParam.min_word_len > first->unroll.Word[z].ulen) ||
		 (query->WordParam.max_word_len < first->unroll.Word[z].ulen)) {
		local.cmd = DPS_STACK_WORD;
		local.add_cmd = DPS_STACK_OR;
		local.order = (size_t)ORDER_ADD;
		local.origin = DPS_WORD_ORIGIN_STOP;
		if (DpsAddStackItem(query, Res, &local, first->unroll.Word[z].word, first->unroll.Word[z].uword) != DPS_OK) {
		  return DPS_ERROR;
		}
		Res->items[ORDER_ADD].order_origin |= DPS_WORD_ORIGIN_STOP;
	      } else {
		local.cmd = DPS_STACK_WORD;
		local.add_cmd = DPS_STACK_OR;
		local.order = (size_t)ORDER_ADD;
		local.origin = DPS_WORD_ORIGIN_ACRONYM;
		if (DpsAddStackItem(query, Res, &local, first->unroll.Word[z].word, first->unroll.Word[z].uword) != DPS_OK) {
		  return DPS_ERROR;
		}
		OWord->uword = first->unroll.Word[z].uword;
		OWord->ulen = DpsUniLen(first->unroll.Word[z].uword);
		OWord->origin = DPS_WORD_ORIGIN_ACRONYM;
		local.order = ORDER_ADD;
		local.nphrasecmd = 1/*(first->unroll.nwords > 1) ? 1 : 0*/;
		local.have_bukva_forte = 0;
		if (DPS_OK != DpsExpandWord(query, Res, OWord, &local)) {
		  return DPS_ERROR;
		}
	      }
#endif
	      local.cmd = DPS_STACK_RIGHT;
	      local.add_cmd = DPS_STACK_OR;
	      if (DpsAddStackItem(query, Res, &local, NULL, NULL) != DPS_OK) {
		return DPS_ERROR;
	      }
	    }
	  }
	  if (first->unroll.nwords > 1) {
	    local.cmd = DPS_STACK_PHRASE_RIGHT;
	    local.add_cmd = DPS_STACK_OR;
	    if (DpsAddStackItem(query, Res, &local, NULL, NULL) != DPS_OK) {
	      return DPS_ERROR;
	    }
	  }
	  first++;
	}
      }
    }
    DpsWideWordListFree(forms);
    DPS_FREE(forms);
    state->order = ORDER_ADD;
  }

  if (query->Flags.use_accentext) {
    af_uwrd = DpsUniAccentStrip(orig_uwrd);

    if (DpsUniStrCmp(af_uwrd, orig_uwrd) != 0) {
      DpsConv(&query->uni_lc, OWord->word, query->WordParam.max_word_len * 12, (char*)af_uwrd, 
	      sizeof(af_uwrd[0])*(DpsUniLen(af_uwrd) + 1));
      clex = DpsTrim(OWord->word, " \t\r\n");

      local.cmd = DPS_STACK_WORD;
      local.add_cmd = DPS_STACK_OR;
      local.order = state->order;
      local.origin = (Origin & ~1) | DPS_WORD_ORIGIN_ACCENT;
      if (DpsAddStackItem(query, Res, &local, clex, af_uwrd) != DPS_OK) {
	return DPS_ERROR;
      }
				
      OWord->len = dps_strlen(OWord->word);
      OWord->order = state->order;
      OWord->count = 0;
      OWord->crcword = DpsStrHash32(OWord->word);
/*      OWord->word = wrd;*/
      OWord->uword = af_uwrd;
      OWord->origin = DPS_WORD_ORIGIN_ACCENT;

      if(state->sp && (forms = DpsAllForms(query, OWord))) {

	for(frm = 0; frm < forms->nwords; frm++) {
	  DpsConv(&query->uni_lc, OWord->word, 12 * query->WordParam.max_word_len,
		  (char*)(forms->Word[frm].uword),
		  sizeof(forms->Word[frm].uword[0])*(DpsUniLen(forms->Word[frm].uword)+1));

	  local.cmd = DPS_STACK_WORD;
	  local.add_cmd = DPS_STACK_OR;
	  local.order = state->order;
	  local.origin = ((Origin | DPS_WORD_ORIGIN_ACCENT | forms->Word[frm].origin) & ~1);
	  if (DpsAddStackItem(query, Res, &local, OWord->word, forms->Word[frm].uword) != DPS_OK) {
	    return DPS_ERROR;
	  }
	}
	DpsWideWordListFree(forms);
	DPS_FREE(forms);
      }
    }
    DPS_FREE(af_uwrd);

    de_uwrd = DpsUniGermanReplace(orig_uwrd);
    if (DpsUniStrCmp(de_uwrd, orig_uwrd) != 0) {
      DpsConv(&query->uni_lc, OWord->word, query->WordParam.max_word_len * 12, (char*)de_uwrd, 
	      sizeof(de_uwrd[0])*(DpsUniLen(de_uwrd) + 1));
      clex = DpsTrim(OWord->word, " \t\r\n");

      local.cmd = DPS_STACK_WORD;
      local.add_cmd = DPS_STACK_OR;
      local.order = state->order;
      local.origin = (Origin & ~1) | DPS_WORD_ORIGIN_ACCENT;
      if (DpsAddStackItem(query, Res, &local, clex, de_uwrd) != DPS_OK) {
	return DPS_ERROR;
      }
				
      OWord->len = dps_strlen(OWord->word);
      OWord->order = state->order;
      OWord->count = 0;
      OWord->crcword = DpsStrHash32(OWord->word);
/*      OWord->word = wrd;*/
      OWord->uword = de_uwrd;
      OWord->origin = DPS_WORD_ORIGIN_ACCENT;

      if(state->sp && (forms = DpsAllForms(query, OWord))) {

	for(frm = 0; frm < forms->nwords; frm++) {
	  DpsConv(&query->uni_lc, OWord->word, 12 * query->WordParam.max_word_len,
		  (char*)(forms->Word[frm].uword),
		  sizeof(forms->Word[frm].uword[0])*(DpsUniLen(forms->Word[frm].uword)+1));

	  local.cmd = DPS_STACK_WORD;
	  local.add_cmd = DPS_STACK_OR;
	  local.order = state->order;
	  local.origin = ((Origin | DPS_WORD_ORIGIN_ACCENT | forms->Word[frm].origin) & ~1);
	  if (DpsAddStackItem(query, Res, &local, OWord->word, forms->Word[frm].uword) != DPS_OK) {
	    return DPS_ERROR;
	  }
	}
	DpsWideWordListFree(forms);
	DPS_FREE(forms);
      }
    }
    DPS_FREE(de_uwrd);
  }

  return DPS_OK;
}


int dps_need2segment(dpsunicode_t *uwrd) {
  dpsunicode_t u;
  for(; (u = *uwrd); uwrd++) {
    if (u >= 0xE01 && u <= 0xE5B) return 1; /* THAI */
    if (u == 0x321D || u == 0x321E || u == 0x327C || u == 0x327D || u == 0x327F) return 1; /* KOREAN */
    if (u >= 0x1100 && u <= 0x11F9) return 1; /* HANGUL */
    if (u == 0x302E || u == 0x302F) return 1;
    if ((u >= 0xAC00) && (u <= 0xD7A3)) return 1;
    if (u >= 0x3131 && u <= 0x318E) return 1;
    if ((u >= 0x3200) && (u <= 0x321C)) return 1;
    if (u >= 0x3260 && u <= 0x327F) return 1;
    if (u >= 0xFFA0 && u <= 0xFFDC) return 1;
    if (u >= 0x3031 && u <= 0x3035) return 1; /* KANA */
    if (u >= 0x3099 && u <= 0x309C) return 1;
    if (u >= 0x30A0 && u <= 0x30FF) return 1;
    if (u >= 0x31F0 && u <= 0x31FF) return 1;
    if (u >= 0x32D0 && u <= 0x32FF) return 1;
    if (u >= 0xFF65 && u <= 0xFF9F) return 1;
    if (u >= 0x2F00 && u <= 0x2F95) return 1; /* KANGXI */
    if (u >= 0x3041 && u <= 0x30A0) return 1; /* HIRAGANA */
    if (u == 0x30FC || u == 0xFF70) return 1;
    if ((u >= 0x2E80) && (u <= 0x2EF3)) return 1; /* CJK */
    if ((u >= 0x31C0) && (u <= 0x31CF)) return 1;
    if ((u >= 0xF900) && (u <= 0xFAD9)) return 1;
    if ((u >= 0x2F800) && (u <= 0x2FA1D)) return 1;
    if ((u >= 0x3400) && (u <= 0x4DB5)) return 1;
    if ((u >= 0x4E00) && (u <= 0x9FBB)) return 1;
    if ((u >= 0x20000) && (u <= 0x2A6D6)) return 1;
  }
  return 0;
}


typedef struct vql_cmd {
  int cmd;
  size_t len;
  dpsunicode_t vql_cmd[6];
  dpsunicode_t dps_cmd[10];
} DPS_VQLCMD;

static DPS_VQLCMD VQLcmd[] = {    /* should be in the same order as DPS_STACK_ commands */
  {DPS_STACK_BOT, 4, {'w','o','r','d'}, {' ',0}},
  {DPS_STACK_LEFT, 1, {'('}, {'('}},
  {DPS_STACK_RIGHT, 1, {')'}, {')'}},
  {DPS_STACK_PHRASE_LEFT, 6, {'p','h','r','a','s','e'}, {'"',0}},
  {DPS_STACK_PHRASE_RIGHT, 6, {'p','h','r','a','s','e'}, {'"',0}},
  {DPS_STACK_OR, 2, {'o','r'}, {' ','O','R',' ',0}},
  {DPS_STACK_AND, 3, {'a','n','d'}, {' ','A','N','D',' ',0}},
  {DPS_STACK_NEAR, 4, {'n','e','a','r'}, {' ','N','E','A','R',' ',0}},
  {DPS_STACK_ANYWORD, 3, {'a','n','y'}, {' ','A','N','Y','W','O','R','D',' ',0}},
  {DPS_STACK_NOT, 3, {'n','o','t'}, {' ','N','O','T',' ',0}},
/* aliases */
  {DPS_STACK_OR, 6, {'a','c','c','r','u','e'}, {'O','R',0}},
  {DPS_STACK_OR, 3, {'a','n','y'}, {'O','R',0}},
  {0, 0, {0}, {0}}
};


static dpsunicode_t * DpsVQLparse(dpsunicode_t *query) {
  int cmd_stack[1024];
  int p_cmd = -1, cur_cmd = 0, n;
  DPS_DSTR Q;
  dpsunicode_t *s = query;

  DpsDSTRInit(&Q, 4096);
  while(*s) {
    if (*s == '<') { /* a command */
      s++;
      for(n = 0; VQLcmd[n].len; n++) {
	if (DpsUniStrNCaseCmp(s, VQLcmd[n].vql_cmd, VQLcmd[n].len) == 0) {
	  if (n) cmd_stack[++p_cmd] = cur_cmd = VQLcmd[n].cmd; break;
	}
      }
      while(*s && (*s != '>')) s++; if (*s == '>') s++;
    } else
    if (*s == '(') {
      s++;
      DpsDSTRAppend(&Q, VQLcmd[DPS_STACK_LEFT].dps_cmd, sizeof(dpsunicode_t));
      if ((p_cmd >= 0) && cmd_stack[p_cmd] == DPS_STACK_PHRASE_LEFT) {
	DpsDSTRAppend(&Q, VQLcmd[DPS_STACK_PHRASE_LEFT].dps_cmd, sizeof(dpsunicode_t));
      }
      cmd_stack[++p_cmd] = DPS_STACK_LEFT;
    } else
    if (*s == ')') {
      s++;
      while((p_cmd >= 0) && (cmd_stack[p_cmd] != DPS_STACK_LEFT)) p_cmd--;
      if (p_cmd >= 0) p_cmd--;
      if ((p_cmd >= 0) && cmd_stack[p_cmd] == DPS_STACK_PHRASE_LEFT) {
	DpsDSTRAppend(&Q, VQLcmd[DPS_STACK_PHRASE_RIGHT].dps_cmd, sizeof(dpsunicode_t));
	p_cmd--;
      }
      DpsDSTRAppend(&Q, VQLcmd[DPS_STACK_RIGHT].dps_cmd, sizeof(dpsunicode_t));
    } else
    if (*s == ',' || *s == ' ') {
      s++;
      if ((p_cmd < 0) || (cmd_stack[p_cmd] == DPS_STACK_PHRASE_LEFT) 
	  || ((p_cmd > 0) && (cmd_stack[p_cmd] == DPS_STACK_LEFT) && (cmd_stack[p_cmd - 1] == DPS_STACK_PHRASE_LEFT))) {
	DpsDSTRAppendUniStr(&Q, VQLcmd[DPS_STACK_BOT].dps_cmd);
      } else {
	DpsDSTRAppendUniStr(&Q, VQLcmd[cur_cmd].dps_cmd);
      }
    } else {
      DpsDSTRAppend(&Q, s, sizeof(dpsunicode_t));
      s++;
    }
    for (n = p_cmd; (n >= 0) && (((cur_cmd = cmd_stack[n]) == DPS_STACK_LEFT) || (cur_cmd == DPS_STACK_PHRASE_LEFT)); n--);
    if (p_cmd == 1023) break;
  }

  DPS_FREE(query);
  return (dpsunicode_t*)Q.data;
}

#ifdef HAVE_ASPELL

#define DPS_PREPARE_RETURN(x)   	DPS_FREE(uwrd); DPS_FREE(wrd); DPS_FREE(seg_ustr); DPS_FREE(ustr); DPS_FREE(Res->items); \
			      DpsDSTRFree(&suggest); \
			      DPS_FREE(state.secno); \
			      TRACE_OUT(query); \
			      return (x); 
#else
#define DPS_PREPARE_RETURN(x)   	DPS_FREE(uwrd); DPS_FREE(wrd); DPS_FREE(seg_ustr); DPS_FREE(ustr); DPS_FREE(Res->items); \
			      DPS_FREE(state.secno); \
			      TRACE_OUT(query); \
			      return (x); 
#endif


int DpsPrepare(DPS_AGENT *query, DPS_RESULT *Res) {
        DPS_PREPARE_STATE state;
	DPS_CONV bc_uni;
	DPS_CHARSET * browser_cs, *sys_int;
	int  ctype, seg_ctype;
	dpsunicode_t *ustr, *nfc, *lt, *lex, *seg_ustr = NULL, *seg_lt, *seg_lex;
	int search_mode = DpsSearchMode(DpsVarListFindStr(&query->Vars, "m", "all"));
	int word_match   = DpsVarListFindInt(&query->Vars, "wm", DPS_MATCH_FULL);
	int add_cmd = DPS_STACK_AND;
	int addwrd, wrd_cmd, VQLflag = 0;
	int notfirstword = 0, have_bukva_forte = 0, seg_have_bukva_forte = 0;
	const char * txt = DpsVarListFindStr(&query->Vars, "q", "");
	const char * rlang, *lang;
	char *ltxt;
	size_t i, wlen, llen, seg_wlen;
	char *wrd, *clex;
	dpsunicode_t *uwrd;
	int ii;
#if defined HAVE_ASPELL
	int toadd = 0;
	AspellCanHaveError *ret;
	AspellSpeller *speller = NULL;
	const AspellWordList *suggestions;
	AspellStringEnumeration *elements;
	char *asug = NULL;
	DPS_DSTR suggest;
	size_t tlen;
	int have_suggest = 0, use_aspellext = query->Flags.use_aspellext;
#endif

	TRACE_IN(query, "DpsPrepare");

	if (Res->prepared) {TRACE_OUT(query); return 0; }

	bzero(&state, sizeof(state));
	state.order = state.order_inquery = (size_t)-1;
       	state.sp = DpsVarListFindInt(&query->Vars, "sp", 1);
	state.sy = DpsVarListFindInt(&query->Vars, "sy", 1);
	state.nphrasecmd = 0;
	state.qlang = DpsVarListFindStr(&query->Vars, "g", NULL);
	state.secno = DpsXmalloc((state.n_secno = 256) * sizeof(state.secno[0]));
	if (state.secno == NULL) {TRACE_OUT(query); return 0; }
	state.secno[0] = 0;

	if (state.qlang == NULL || *(state.qlang) == '\0') 
	  rlang = DpsLanguageCanonicalName(DpsVarListFindStr(&query->Vars, "g-lc", NULL));
	else rlang = DpsLanguageCanonicalName(state.qlang);
	if (rlang == NULL) rlang = "en";

	query->SpellLang = -1;
	for (i = 0; i < query->Conf->Spells.nLang; i++) {
	  if (strncasecmp(query->Conf->Spells.SpellTree[i].lang, rlang, 2) == 0) {
	    query->SpellLang = (int)i;
	    break;
	  }
	}
	DpsLog(query, DPS_LOG_EXTRA, ".spell lang: %s", rlang);

#ifdef HAVE_ASPELL
	if (use_aspellext) {
	  aspell_config_replace(query->aspell_config, "lang", rlang);

	  ret = new_aspell_speller(query->aspell_config);
	  if (aspell_error(ret) != 0) {
	    DpsLog(query, DPS_LOG_ERROR, "aspell error: %s", aspell_error_message(ret));
	    delete_aspell_can_have_error(ret);
	    use_aspellext = 0;
	  } else {
	    DpsDSTRInit(&suggest, 1024);
	    speller = to_aspell_speller(ret);
	  }
	}
#endif
	
	DpsWideWordListFree(&Res->WWList);
				  
	if ((wrd = (char*)DpsMalloc(query->WordParam.max_word_len * 12 + 1)) == NULL) {
#ifdef HAVE_ASPELL
	  DpsDSTRFree(&suggest); 
#endif
	  DPS_FREE(state.secno);
	  TRACE_OUT(query);
	  return 0; 
	}
	if ((uwrd = (dpsunicode_t*)DpsMalloc(sizeof(dpsunicode_t) * (query->WordParam.max_word_len + 1))) == NULL) { 
	  DPS_FREE(wrd); 
#ifdef HAVE_ASPELL
	  DpsDSTRFree(&suggest); 
#endif
	  DPS_FREE(state.secno);
	  TRACE_OUT(query);
	  return 0; 
	}


	if (!(browser_cs = query->Conf->bcs)) {
		browser_cs = DpsGetCharSet("iso-8859-1");
	}
	
	if (!(sys_int = DpsGetCharSet("sys-int"))) {
	        DPS_FREE(uwrd); DPS_FREE(wrd);
#ifdef HAVE_ASPELL
		DpsDSTRFree(&suggest); 
#endif
		DPS_FREE(state.secno);
		TRACE_OUT(query);
		return 0;
	}

	DpsConvInit(&bc_uni, browser_cs, sys_int, ""/*query->Conf->CharsToEscape*/, DPS_RECODE_HTML_FROM);

	if (*txt == '\0') { /* Empty query, trying to convert VQL */
	  txt =  DpsVarListFindStr(&query->Vars, "vq" /*"q_vql"*/, "");
	  VQLflag = 1;
	}

	
	llen = dps_strlen(txt) + 1;
	ustr = (dpsunicode_t*)(DpsMalloc(sizeof(dpsunicode_t) * 14 * llen));
	if (ustr == NULL) {
	        DPS_FREE(uwrd); DPS_FREE(wrd);
#ifdef HAVE_ASPELL
		DpsDSTRFree(&suggest); 
#endif
		DPS_FREE(state.secno);
		TRACE_OUT(query);
		return 0;
	}
	DpsConv(&bc_uni, (char*)ustr, sizeof(dpsunicode_t) * 14 * llen, txt, llen);

	if (VQLflag) {
	  ustr = DpsVQLparse(ustr);
	  search_mode = DPS_MODE_BOOL;
	}

	switch(search_mode) {
	case DPS_MODE_ANY:  add_cmd = DPS_STACK_OR;   break;
	case DPS_MODE_BOOL:
	case DPS_MODE_ALL:  add_cmd = DPS_STACK_AND;  break;
	case DPS_MODE_NEAR: add_cmd = DPS_STACK_NEAR; break;
	}

	/* Create copy of query, converted into LocalCharset (for DpsTrack) */
	ltxt = (char*)DpsMalloc(14 * llen);
	if (ltxt == NULL) {
	  DPS_FREE(uwrd); DPS_FREE(wrd); DPS_FREE(ustr);
#ifdef HAVE_ASPELL
	  DpsDSTRFree(&suggest); 
#endif
	  DPS_FREE(state.secno);
	  TRACE_OUT(query);
	  return 0;
	}
	DpsConv(&query->uni_lc, ltxt, 14 * llen, (char*)ustr, sizeof(dpsunicode_t) * llen /*bc_uni.obytes*/);
	ltxt[query->uni_lc.obytes] = '\0';
	DpsLog(query, DPS_LOG_DEBUG, "Prepare query: %s, ltxt:%s", txt, ltxt);
	DpsVarListReplaceStr(&query->Vars, "q", ltxt);  /* "q-lc" was here */
	
	DPS_FREE(ltxt);
	
	/* Parse query and build boolean search stack*/
	DpsUniStrToLower(ustr);
	nfc = DpsUniNormalizeNFC(NULL, ustr);
	DPS_FREE(ustr);
	switch(browser_cs->family) {
	case DPS_CHARSET_CHINESE_SIMPLIFIED:
	case DPS_CHARSET_CHINESE_TRADITIONAL: lang = "zh"; break;
	case DPS_CHARSET_JAPANESE: lang = "ja"; break;
	case DPS_CHARSET_THAI: lang = "th"; break;
	case DPS_CHARSET_KOREAN: lang = "ko"; break;
	case DPS_CHARSET_UNICODE: lang = (state.qlang == NULL) ? "" : state.qlang; break;
	default: lang = (state.qlang == NULL) ? "" : state.qlang;
	}

	DpsLog(query, DPS_LOG_EXTRA, "Segment lang: %s", lang);

	ustr = nfc;

	lex = (ustr != NULL) ? DpsUniGetSepToken(ustr, &lt , &ctype, &have_bukva_forte, 1, state.nphrasecmd & 1) : NULL;
	while(lex) {
	  wlen = lt - lex;
	  dps_memcpy(uwrd, lex, (dps_min(wlen, query->WordParam.max_word_len)) * sizeof(dpsunicode_t)); /* was: dps_memmove */
	  uwrd[dps_min(wlen, query->WordParam.max_word_len)] = 0;
	  DpsConv(&query->uni_lc, wrd, query->WordParam.max_word_len * 12,(char*)uwrd, sizeof(uwrd[0])*(wlen+1));
	  clex = DpsTrim(wrd, " \t\r\n");

	  DpsLog(query, DPS_LOG_DEBUG, "\t\t\twrd {len:%d,class:%d,forte:%d}: %s", wlen, DPS_UNI_CTYPECLASS(ctype), have_bukva_forte, clex);
			
	  if (DPS_UNI_CTYPECLASS(ctype) != DPS_UNI_BUKVA) {
	    size_t ULen = DpsUniLen(uwrd);
	    int cur_cmd;
	      for (i = 0; i < ULen; i++) {
		        switch(uwrd[i]) {
			case '&':
			case '+': cur_cmd = DPS_STACK_AND;  notfirstword = 0; break;
			case '|': cur_cmd = DPS_STACK_OR;  notfirstword = 0; break;
			case '~': cur_cmd = DPS_STACK_NOT; notfirstword = 0; break;
			case '(': cur_cmd = DPS_STACK_LEFT; notfirstword = 0; 
			  if (++state.p_secno >= state.n_secno) {
			    state.n_secno += 64;
			    state.secno = DpsRealloc(state.secno, state.n_secno * sizeof(state.secno[0]));
			    if (state.secno == NULL) {
			      DPS_PREPARE_RETURN(DPS_ERROR);
			    }
			  }
			  state.secno[state.p_secno] = state.secno[state.p_secno - 1];
			  break;
			case ')': cur_cmd = DPS_STACK_RIGHT; notfirstword = 0; 
			  if (state.p_secno > 0) {
			    state.p_secno--;
			  }
			  break;
			case '*': cur_cmd = DPS_STACK_ANYWORD; notfirstword = 0; break;
			case 171:  /* &#171; - left << */
			case 187:  /* &#187; - right >> */
			case '"':
			case '\'': 
			  if (state.nphrasecmd & 1) { 
			    cur_cmd = DPS_STACK_PHRASE_RIGHT;
			    notfirstword = 1;
			  } else {
			    cur_cmd = DPS_STACK_PHRASE_LEFT;
			    if (notfirstword) {
			      state.cmd = add_cmd;
			      state.add_cmd = add_cmd;
			      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
				DPS_PREPARE_RETURN(0);
			      }
			    }
			    notfirstword = 0;
			  }
			  Res->phrase = 1;
			  state.nphrasecmd++;
			  break;
			default: continue;
			}
			/*if (nphrasecmd & 1) notfirstword = 0;*/
			state.cmd = cur_cmd;
			state.add_cmd = add_cmd;
			if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			  DPS_PREPARE_RETURN(0);
			}
	      }
#ifdef HAVE_ASPELL
	      if (*clex != '\0' && use_aspellext) {
		DpsDSTRAppendStrWithSpace(&suggest, wrd);
	      }
#endif
	  } else {
	    if (lt && *lt && (strchr("-_.,/\\", (int)*lt) != NULL)) {
	      if (state.nphrasecmd & 1) {
	      } else if (state.autophrase == 0) {
		  state.nphrasecmd++;
		  state.autophrase = 1;
		  if (notfirstword) {
		    state.cmd = add_cmd;
		    state.add_cmd = add_cmd;
		    if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		      DPS_PREPARE_RETURN(0);
		    }
		  }
		  state.cmd = DPS_STACK_PHRASE_LEFT;
		  state.add_cmd = add_cmd;
		  if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		    DPS_PREPARE_RETURN(0);
		  }
		  notfirstword = 0;
		  Res->phrase = 1;
	      }
	    } else {
	      if (state.autophrase) {
		state.nphrasecmd++;
		state.autophrase = 0;
		state.cmd = DPS_STACK_PHRASE_RIGHT;
		state.add_cmd = add_cmd;
		notfirstword = 1;
		if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		  DPS_PREPARE_RETURN(0);
		}
	      }
	    }

	    if (!(state.nphrasecmd & 1) && (strncasecmp(clex, "allin", 5) == 0)) {
		DPS_VAR *var = DpsVarListFind(&query->Conf->Sections, DpsRTrim(&clex[5], ":"));
		state.secno[state.p_secno] = (var) ? var->section : 0;
		goto token_next;
	    } else if (!(state.nphrasecmd & 1) && (strcasecmp(clex, "AND") == 0)) {
	      notfirstword = 0;
	      state.cmd = DPS_STACK_AND;
	      state.add_cmd = add_cmd;
	      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		DPS_PREPARE_RETURN(0);
	      }
#ifdef HAVE_ASPELL
	      if (use_aspellext) {
		DpsDSTRAppendStrWithSpace(&suggest, wrd);
	      }
#endif
	    } else if ((search_mode == DPS_MODE_BOOL) && !(state.nphrasecmd & 1) && (strcasecmp(clex, "NEAR") == 0)) {
	      notfirstword = 0;
	      state.cmd = DPS_STACK_NEAR;
	      state.add_cmd = add_cmd;
	      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		DPS_PREPARE_RETURN(0);
	      }
#ifdef HAVE_ASPELL
	      if (use_aspellext) {
		DpsDSTRAppendStrWithSpace(&suggest, wrd);
	      }
#endif
	    } else if ((search_mode == DPS_MODE_BOOL) && !(state.nphrasecmd & 1) && (strcasecmp(clex, "ANYWORD") == 0)) {
	      notfirstword = 0;
	      state.cmd = DPS_STACK_ANYWORD;
	      state.add_cmd = add_cmd;
	      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		DPS_PREPARE_RETURN(0);
	      }
#ifdef HAVE_ASPELL
	      if (use_aspellext) {
		DpsDSTRAppendStrWithSpace(&suggest, wrd);
	      }
#endif
	    } else if ((search_mode == DPS_MODE_BOOL) && !(state.nphrasecmd & 1) && (strcasecmp(clex, "ANY") == 0)) {
	      notfirstword = 0;
	      state.cmd = DPS_STACK_ANYWORD;
	      state.add_cmd = add_cmd;
	      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		DPS_PREPARE_RETURN(0);
	      }
#ifdef HAVE_ASPELL
	      if (use_aspellext) {
		DpsDSTRAppendStrWithSpace(&suggest, wrd);
	      }
#endif
	    } else if ((search_mode == DPS_MODE_BOOL) && !(state.nphrasecmd & 1) && (strcasecmp(clex, "OR") == 0)) {
	      notfirstword = 0;
	      state.cmd = DPS_STACK_OR;
	      state.add_cmd = add_cmd;
	      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		DPS_PREPARE_RETURN(0);
	      }
#ifdef HAVE_ASPELL
	      if (use_aspellext) {
		DpsDSTRAppendStrWithSpace(&suggest, wrd);
	      }
#endif
	    } else if ((search_mode == DPS_MODE_BOOL) && !(state.nphrasecmd & 1) && ((strcasecmp(clex, "NOT") == 0) || (strcasecmp(clex, "~") == 0))) {
	      notfirstword = 0;
	      state.cmd = DPS_STACK_NOT;
	      state.add_cmd = add_cmd;
	      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		DPS_PREPARE_RETURN(0);
	      }
#ifdef HAVE_ASPELL
	      if (use_aspellext) {
		DpsDSTRAppendStrWithSpace(&suggest, wrd);
	      }
#endif
	    } else {

	      seg_ustr = (dps_need2segment(uwrd)) ? DpsUniSegment(query, uwrd, lang) : DpsUniDup(uwrd);

	      seg_lex = (seg_ustr != NULL) ? DpsUniGetSepToken(seg_ustr, &seg_lt , &seg_ctype, &seg_have_bukva_forte, 1, state.nphrasecmd & 1) : NULL;

	      while (seg_lex) {
		if (DPS_UNI_CTYPECLASS(seg_ctype) != DPS_UNI_BUKVA) goto seg_next;
		seg_wlen = seg_lt - seg_lex;
		dps_memcpy(uwrd, seg_lex, (dps_min(seg_wlen, query->WordParam.max_word_len)) * sizeof(dpsunicode_t)); /* was: dps_memmove */
		uwrd[dps_min(seg_wlen, query->WordParam.max_word_len)] = 0;
#ifdef HAVE_ASPELL
		if (use_aspellext && seg_have_bukva_forte && seg_wlen > 2 && query->naspell < DPS_DEFAULT_MAX_ASPELL
		    && (DpsUniStrChr(uwrd, (dpsunicode_t) '&') == NULL)  /* aspell trap workaround */
		    ) {
		  toadd = 1;
		  DpsConv(&query->uni_utf, wrd, query->WordParam.max_word_len * 12, (char*)uwrd, sizeof(uwrd[0])*(seg_wlen+1));
		  ii = aspell_speller_check(speller, (const char *)wrd, (int)(tlen = dps_strlen(wrd)) + 1);
		  if ( ii == 0) {
		    /* aspell trap workaround */
		    int rd[2];    
		    /* Create write and read pipe */
		    if (pipe(rd) == -1){
		      DpsLog(query, DPS_LOG_ERROR, "DpsPrepare: Cannot make a pipe");
		      DPS_PREPARE_RETURN(DPS_ERROR);
		    }    
		    /* Fork a clild */
		    if ((query->aspell_pid[query->naspell] = fork()) == -1) {
		      DpsLog(query, DPS_LOG_ERROR, "Cannot spawn a child");
		      DPS_PREPARE_RETURN(DPS_ERROR);
		    }
		    if (query->aspell_pid[query->naspell] > 0) {
		      /* Parent process */
		      ssize_t rc;
		      char *pwrd;
/*
#ifdef UNIONWAIT
		    union wait status;
#else
		    int status;
#endif
*/
		      close(rd[1]);
		      if ((rc = read(rd[0], &tlen, sizeof(tlen))) == sizeof(tlen)) {
		      
			pwrd = (char*)DpsMalloc(tlen + 2);
			if (read(rd[0], pwrd, tlen) == tlen) {
			  pwrd[tlen] = '\0';
			  DpsConv(&query->utf_lc, wrd, 12 * query->WordParam.max_word_len, pwrd, sizeof(pwrd[0]) * (tlen  + 1));
			  DpsDSTRAppendStrWithSpace(&suggest, wrd);
/*			  fprintf(stderr, " -- aspell suggest: %s\n", wrd);*/
			  have_suggest = 1;
			  toadd = 0;
			}
			DpsFree(pwrd);
		      }
		      close(rd[0]);
		      query->naspell++;
		    /* will wait later, after all output will been made */
/*		    while (waitpid(-1, &status, WNOHANG) > 0);*/
		    
		    } else {
		    /* Child process */
		      init_signals();
		      close(0); close(1); /* close STDIN and STDOUT */
		      close(rd[0]);

		      suggestions = aspell_speller_suggest(speller, (const char *)wrd, (int)tlen);
		      elements = aspell_word_list_elements(suggestions);
		      if ((asug = (char*)aspell_string_enumeration_next(elements)) != NULL) {
			tlen = dps_strlen(asug);
			(void)write(rd[1], &tlen, sizeof(tlen));
			(void)write(rd[1], asug, tlen);
		      }
		      delete_aspell_string_enumeration(elements);
		      close(2); /* close STDERR */
		      close(rd[1]);
		      /* wait till killing by parent */
		      /*while(1) DPSSLEEP(300);*/ exit(0);
		    }
		  }
		}
#endif
		DpsConv(&query->uni_lc, wrd, query->WordParam.max_word_len * 12, (char*)uwrd, sizeof(uwrd[0])*(seg_wlen+1));

#ifdef HAVE_ASPELL
		if (toadd) {
		  DpsDSTRAppendStrWithSpace(&suggest, wrd);
		}
#endif

		addwrd = 1;
		state.order++;
		state.order_inquery++;

		if ( notfirstword /*Res->nitems - Res->ncmds > 0*/ /*Res->items[Res->nitems - 1].cmd == DPS_STACK_WORD */) {
		  state.cmd = add_cmd;
		  state.add_cmd = add_cmd;
		  if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		    DPS_PREPARE_RETURN(0);
		  }
		}

		notfirstword = 1;
		wrd_cmd = DPS_WORD_ORIGIN_QUERY;

		state.cmd = DPS_STACK_LEFT;
		state.add_cmd = add_cmd;
		if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		  DPS_PREPARE_RETURN(0);
		}

		if(word_match==DPS_MATCH_FULL){
		  /* Check stopword only when full word         */
		  /* Substring searches should not exclude them */
		  if(DpsStopListFind(&query->Conf->StopWords, uwrd, (query->flags & DPS_FLAG_STOPWORDS_LOOSE) ? state.qlang : "") ||
		     (query->WordParam.min_word_len > seg_wlen) ||
		     (query->WordParam.max_word_len < seg_wlen)) {

		    wrd_cmd |= DPS_WORD_ORIGIN_STOP;
		  }
		}
		if(Res->WWList.nuniq >= DPS_MAXWORDPERQUERY-1){
		  addwrd = 0;
		}

		if(addwrd){
		  DPS_WIDEWORD OWord;
		  DPS_ACRONYM *first, *last;
		  dpsunicode_t *uwrddup = DpsUniDup(uwrd), *l_lt, *l_tok;
		  int l_forte, loose_split;
		  int notfirst_l_tok = 0;
		  l_tok = DpsUniGetToken(uwrddup, &l_lt, &l_forte, 1);
		  loose_split = (wlen != (size_t)(l_lt - l_tok));

		  state.cmd = DPS_STACK_WORD;
		  state.add_cmd = add_cmd;
/*		state.order = *(state.ORDER);*/
		  state.origin = wrd_cmd;
		  if (DpsAddStackItem(query, Res, &state, wrd, uwrd) != DPS_OK) {
		    DPS_FREE(uwrddup);
		    DPS_PREPARE_RETURN(0);
		  }
		  OWord.len = dps_strlen(wrd);
		  OWord.order = state.order;
		  OWord.order_inquery = state.order_inquery;
		  OWord.count = 0;
		  OWord.crcword = DpsStrHash32(wrd);
		  OWord.word = wrd;
		  OWord.uword = uwrd;
		  OWord.ulen = DpsUniLen(uwrd);
		  OWord.origin = DPS_WORD_ORIGIN_QUERY; /* 0 */
		  
/*		if (wrd_cmd == DPS_WORD_ORIGIN_STOP) Res->items[ORDER].order_origin = DPS_WORD_ORIGIN_STOP;*/
		  Res->items[state.order].order_origin |= wrd_cmd;

		  state.have_bukva_forte = seg_have_bukva_forte;
		  if (DPS_OK != DpsExpandWord(query, Res, &OWord, &state)) {
		    DPS_FREE(uwrddup);
		    DPS_PREPARE_RETURN(0);
		  }

#if 1
		  OWord.uword = uwrd;
		  if (state.sp) {
		    DPS_MATCH_PART	Parts[10];
		    DPS_MATCH	 *Alias;
		    size_t	 aliassize, nparts = 10;
		    char	 *alias = NULL;
		    dpsunicode_t *ualias = NULL;
		    int          cascade;
		    size_t z;
		    size_t nphrasecmd;


		    if (/*!(state.nphrasecmd & 1)&&-*/(first = DpsAcronymListFind(&query->Conf->Acronyms, &OWord, &last)) != NULL) {
		      while(first <= last) {
			if ((state.nphrasecmd & 1) && (first->unroll.nwords > 1)) { first++; continue; }
			if (!(state.nphrasecmd & 1)) state.order++;
			state.cmd = DPS_STACK_OR;
			state.add_cmd = add_cmd;
			if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			  DPS_FREE(uwrddup);
			  DPS_PREPARE_RETURN(0);
			}
			if (first->unroll.nwords > 1) {
			  state.cmd = DPS_STACK_PHRASE_LEFT;
			  state.add_cmd = add_cmd;
			  if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			    DPS_FREE(uwrddup);
			    DPS_PREPARE_RETURN(0);
			  }
			}
			{ 
			  for (z = 0; z < first->unroll.nwords; z++) {
			    if (z) {
			      state.order++;
			      state.cmd = DPS_STACK_AND;
			      state.add_cmd = add_cmd;
			      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
				DPS_FREE(uwrddup);
				DPS_PREPARE_RETURN(0);
			      }
			    }
			    state.cmd = DPS_STACK_LEFT;
			    state.add_cmd = add_cmd;
			    if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			      DPS_FREE(uwrddup);
			      DPS_PREPARE_RETURN(0);
			    }

			    state.cmd = DPS_STACK_WORD;
			    state.add_cmd = add_cmd;
			    state.origin = DPS_WORD_ORIGIN_ACRONYM;
			    if (DpsAddStackItem(query, Res, &state, first->unroll.Word[z].word, first->unroll.Word[z].uword) != DPS_OK) {
			      DPS_FREE(uwrddup);
			      DPS_PREPARE_RETURN(0);
			    }
			    OWord.uword = first->unroll.Word[z].uword;
			    OWord.ulen = DpsUniLen(first->unroll.Word[z].uword);
			    OWord.origin = DPS_WORD_ORIGIN_ACRONYM;
			    nphrasecmd = state.nphrasecmd;
			    state.nphrasecmd = 1/*(first->unroll.nwords > 1) ? 1 : 0*/;
			    state.have_bukva_forte = 0;
			    if (DPS_OK != DpsExpandWord(query, Res, &OWord, &state)) {
				DPS_FREE(uwrddup);
				DPS_PREPARE_RETURN(0);
			    }
			    state.nphrasecmd = nphrasecmd;
			    


			    state.cmd = DPS_STACK_RIGHT;
			    state.add_cmd = add_cmd;
			    if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			      DPS_FREE(uwrddup);
			      DPS_PREPARE_RETURN(0);
			    }
			  }
			}
			if (first->unroll.nwords > 1) {
			  state.cmd = DPS_STACK_PHRASE_RIGHT;
			  state.add_cmd = add_cmd;
			  if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			    DPS_FREE(uwrddup);
			    DPS_PREPARE_RETURN(0);
			  }
			}
			first++;
		      }
		    }
		    
		    for(cascade = 0; ((Alias = DpsMatchListFind(&query->Conf->QAliases, wrd, nparts, Parts))) && (cascade < 1024); cascade++) {
		      aliassize = dps_strlen(Alias->arg) + dps_strlen(Alias->pattern) + dps_strlen(wrd) + 128;
		      alias = (char*)DpsRealloc(alias, aliassize);
		      if (alias == NULL) {
			DpsLog(query, DPS_LOG_ERROR, "No memory (%d bytes). %s line %d", aliassize, __FILE__, __LINE__);
			goto ret;
		      }
		      DpsMatchApply(alias,aliassize,wrd,Alias->arg,Alias,nparts,Parts);
		      if(alias[0]){
			DpsLog(query, DPS_LOG_DEBUG, "QAlias%d: pattern:%s, arg:%s -> '%s'", cascade, Alias->pattern, Alias->arg, alias);
			ualias = (dpsunicode_t*)DpsRealloc(ualias, sizeof(dpsunicode_t) * aliassize);
			if (ualias == NULL) {
			  DpsLog(query, DPS_LOG_ERROR, "No memory (%d bytes). %s line %d", sizeof(dpsunicode_t) * aliassize, __FILE__, __LINE__);
			  goto ret;
			}
			DpsConv(&query->lc_uni, (char*)ualias, sizeof(dpsunicode_t) * aliassize, alias, aliassize);

			OWord.len = dps_strlen(alias);
			OWord.order = state.order;
			OWord.order_inquery = state.order_inquery;
			OWord.count = 0;
			OWord.crcword = DpsStrHash32(alias);
			OWord.word = alias;
			OWord.uword = ualias;
			OWord.ulen = DpsUniLen(ualias);
			OWord.origin = DPS_WORD_ORIGIN_ACRONYM;

			state.cmd = DPS_STACK_OR;
			state.add_cmd = add_cmd;
			if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			  DPS_FREE(uwrddup);
			  DPS_FREE(alias); DPS_FREE(ualias);
			  DPS_PREPARE_RETURN(0);
			}
			    state.cmd = DPS_STACK_WORD;
			    state.add_cmd = add_cmd;
			    state.origin = DPS_WORD_ORIGIN_ACRONYM;
			    if (DpsAddStackItem(query, Res, &state, alias, ualias) != DPS_OK) {
			      DPS_FREE(uwrddup);
			      DPS_FREE(alias); DPS_FREE(ualias);
			      DPS_PREPARE_RETURN(0);
			    }
			    OWord.uword = ualias;
			    OWord.ulen = DpsUniLen(ualias);
			    OWord.origin = DPS_WORD_ORIGIN_ACRONYM;
			    nphrasecmd = state.nphrasecmd;
			    state.nphrasecmd = 1;
			    state.have_bukva_forte = 0;
			    if (DPS_OK != DpsExpandWord(query, Res, &OWord, &state)) {
				DPS_FREE(uwrddup);
				DPS_FREE(alias); DPS_FREE(ualias);
				DPS_PREPARE_RETURN(0);
			    }
			    state.nphrasecmd = nphrasecmd;
			    

		      } else break;
		      if (Alias->last) break;
		    }
ret:	
		    DPS_FREE(alias); DPS_FREE(ualias);



		  }
#endif

		  if (loose_split && !(state.nphrasecmd & 1)) {
		    state.cmd = DPS_STACK_OR;
		    state.add_cmd = add_cmd;
		    if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		      DPS_FREE(uwrddup);
		      DPS_PREPARE_RETURN(0);
		    }
		    state.cmd = DPS_STACK_PHRASE_LEFT;
		    state.add_cmd = add_cmd;
		    if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		      DPS_FREE(uwrddup);
		      DPS_PREPARE_RETURN(0);
		    }
/***********************************************/
		    while(l_tok) {
		      wlen = l_lt - l_tok;
		      state.order++;
		      if (notfirst_l_tok /* l_tok != uwrddup*/) {
			state.cmd = DPS_STACK_AND;
			state.add_cmd = add_cmd;
			if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			  DPS_FREE(uwrddup);
			  DPS_PREPARE_RETURN(0);
			}
		      }
		      notfirst_l_tok = 1;
		      state.cmd = DPS_STACK_LEFT;
		      state.add_cmd = add_cmd;
		      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			DPS_FREE(uwrddup);
			DPS_PREPARE_RETURN(0);
		      }
		      dps_memcpy(uwrd, l_tok, (dps_min(wlen, query->WordParam.max_word_len)) * sizeof(dpsunicode_t)); /* was: dps_memmove */
		      uwrd[dps_min(wlen, query->WordParam.max_word_len)] = 0;
		      DpsConv(&query->uni_lc, wrd, query->WordParam.max_word_len * 12,(char*)uwrd, sizeof(uwrd[0])*(wlen+1));

		      state.cmd = DPS_STACK_WORD;
		      state.add_cmd = add_cmd;
		      state.origin = wrd_cmd;
		      if (DpsAddStackItem(query, Res, &state, wrd, uwrd) != DPS_OK) {
			DPS_FREE(uwrddup);
			DPS_PREPARE_RETURN(0);
		      }
		      OWord.len = dps_strlen(wrd);
		      OWord.order = state.order;
		      OWord.order_inquery = state.order_inquery;
		      OWord.count = 0;
		      OWord.crcword = DpsStrHash32(wrd);
		      OWord.word = wrd;
		      OWord.uword = uwrd;
		      OWord.ulen = DpsUniLen(uwrd);
		      OWord.origin = DPS_WORD_ORIGIN_QUERY;

		      Res->items[state.order].order_origin |= wrd_cmd;

		      state.have_bukva_forte = l_forte;
		      if (DPS_OK != DpsExpandWord(query, Res, &OWord, &state)) {
			DPS_FREE(uwrddup);
			DPS_PREPARE_RETURN(0);
		      }
		      state.cmd = DPS_STACK_RIGHT;
		      state.add_cmd = add_cmd;
		      if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
			DPS_FREE(uwrddup);
			DPS_PREPARE_RETURN(0);
		      }
		      l_tok = DpsUniGetToken(NULL, &l_lt, &l_forte, 1);
		    }
/***********************************************/
		    state.cmd = DPS_STACK_PHRASE_RIGHT;
		    state.add_cmd = add_cmd;
		    if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		      DPS_FREE(uwrddup);
		      DPS_PREPARE_RETURN(0);
		    }
		    

		  }
		  DPS_FREE(uwrddup);

		}
		state.cmd = DPS_STACK_RIGHT;
		state.add_cmd = add_cmd;
		if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
		  DPS_PREPARE_RETURN(0);
		}
	      seg_next:
		seg_lex = DpsUniGetSepToken(NULL, &seg_lt, &seg_ctype, &seg_have_bukva_forte, 1, state.nphrasecmd & 1);
	      }
	      DPS_FREE(seg_ustr);
	    }
	  }
	token_next:
	  lex = DpsUniGetSepToken(NULL, &lt, &ctype, &have_bukva_forte, 1, state.nphrasecmd & 1);
	}
	if (state.nphrasecmd & 1) {
	  if ((Res->nitems > 0) && (Res->items[Res->nitems-1].cmd == DPS_STACK_PHRASE_LEFT)) {
	    Res->nitems--;
	    Res->ncmds--;
	    while ( Res->nitems > 0 && ( (Res->items[Res->nitems-1].cmd == DPS_STACK_AND)
					 || (Res->items[Res->nitems-1].cmd == DPS_STACK_OR)
					 || (Res->items[Res->nitems-1].cmd == DPS_STACK_NOT) ) ) {
	      Res->nitems--;
	      Res->ncmds--;
	    }
	  } else {

	    state.cmd = DPS_STACK_PHRASE_RIGHT;
	    state.add_cmd = add_cmd;
	    if (DpsAddStackItem(query, Res, &state, NULL, NULL) != DPS_OK) {
	      DPS_PREPARE_RETURN(0);
	    }
	  }
	}
	Res->orig_nitems = Res->nitems;

/******************************************************/

	DPS_FREE(ustr); DPS_FREE(uwrd); DPS_FREE(wrd);
#ifdef HAVE_ASPELL
	if (use_aspellext) {
	  delete_aspell_speller(speller);
	  if (have_suggest) {
	    DPS_FREE(Res->Suggest);
	    if (suggest.data_size > 0) {
	      Res->Suggest = suggest.data;
	      Res->Suggest[suggest.data_size] = '\0';
	      Res->Suggest = DpsStrdup(suggest.data);
	    }
	  }
	  DpsDSTRFree(&suggest); 
	}
#endif
	Res->prepared = 1;
	DPS_FREE(state.secno);
	TRACE_OUT(query);
	return(0);
}



#define WF_ADD(sec)    (wf[(sec)] << (4 + 4 * (((sec) - 1) % 6)))

static unsigned DpsBitCntTable[256] = {
 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

static inline int DpsBitsCount(dps_uint4 x) {
  return DpsBitCntTable[ x & 0xFF] + DpsBitCntTable[(x >> 8) & 0xFF] + DpsBitCntTable[(x >> 16) & 0xFF] + DpsBitCntTable[(x >> 24) & 0xFF]; 
}


static inline dps_uint4 DpsCalcCosineWeightFull(dps_uint4 *R, double x, double xy, dps_uint4 *D, size_t phr_n
#ifdef WITH_REL_TRACK
						, double *D_y
#endif
						) {
  double y;
  double y_phrase, y_exact, y_origin;
#ifdef WITH_REL_WRDCOUNT
  double y_wrdcount, y_count;
#endif
#ifdef WITH_REL_POSITION
  double y_position, y_firstpos;
#endif
#ifdef WITH_REL_DISTANCE
  double y_distance;
#endif

  if (D[DPS_N_PHRASE] == 0) y_phrase = 10000; else y_phrase = 100 / D[DPS_N_PHRASE];
  if (D[DPS_N_EXACT] == 0) y_exact = 10000; else y_exact = 100 / D[DPS_N_EXACT];
  y_origin = xy * (D[DPS_N_ORIGIN] - R[DPS_N_ORIGIN]);


#ifdef WITH_REL_WRDCOUNT
  if (D[DPS_N_WRDCOUNT] > R[DPS_N_WRDCOUNT]) {
    y_wrdcount = DPS_WRD_CNT_FACTOR * (double)(D[DPS_N_WRDCOUNT] - R[DPS_N_WRDCOUNT]);
  } else {
    y_wrdcount = DPS_LESS_WRD_CNT_FACTOR * (double)(R[DPS_N_WRDCOUNT] - D[DPS_N_WRDCOUNT]);
  }
  y_count = ((D[DPS_N_COUNT] > DPS_UNICNT_BORDER) ? DPS_UNICNT_FACTOR : DPS_LESS_UNICNT_FACTOR) * D[DPS_N_COUNT];
/*  fprintf(stderr, "WRDCNT:: R: %d  D: %d -- %f\n", R[DPS_N_WRDCOUNT], D[DPS_N_WRDCOUNT], y_wrdcount );*/
/*  fprintf(stderr, "UNICNT:: R: %d  D: %f -- %f\n", R[DPS_N_COUNT], (double)D[DPS_N_COUNT], y_count );*/
#endif


#ifdef WITH_REL_POSITION
  if (D[DPS_N_POSITION] > R[DPS_N_POSITION]) {
    y_position = DPS_POSITION_FACTOR * (double)(D[DPS_N_POSITION] - R[DPS_N_POSITION]);
  } else {
    y_position = DPS_LESS_POSITION_FACTOR * (double)(R[DPS_N_POSITION] - D[DPS_N_POSITION]);
  }
  if (D[DPS_N_FIRSTPOS] > R[DPS_N_FIRSTPOS]) {
    y_firstpos = DPS_POSITION_FACTOR * (double)(D[DPS_N_FIRSTPOS] - R[DPS_N_FIRSTPOS]); 
  } else {
    y_firstpos = DPS_LESS_POSITION_FACTOR * (double)(R[DPS_N_FIRSTPOS] - D[DPS_N_FIRSTPOS]);
  }
/*    fprintf(stderr, "POS:: R: %d  D: %d -- %f\n", R[DPS_N_POSITION], D[DPS_N_POSITION], y_position );*/
/*    fprintf(stderr, "FIRSTPOS:: R: %d  D: %d -- %f\n", R[DPS_N_FIRSTPOS], D[DPS_N_FIRSTPOS], y_firstpos );*/
#endif

#ifdef WITH_REL_DISTANCE
  if (D[DPS_N_DISTANCE] > R[DPS_N_DISTANCE]) {
    y_distance = DPS_DISTANCE_FACTOR * (double)(D[DPS_N_DISTANCE] - R[DPS_N_DISTANCE]);
  } else {
    y_distance = DPS_LESS_DISTANCE_FACTOR * (double)(R[DPS_N_DISTANCE] - D[DPS_N_DISTANCE]);
  }
/*    fprintf(stderr, "DIST:: R: %d  D: %d -- %f\n", R[DPS_N_DISTANCE], D[DPS_N_DISTANCE], y_distance );*/
#endif

/*  fprintf(stderr, "2. x:%lf  xy:%lf  y:%lf {xy /(x + y):%lf}\n\n", 
	  x, xy, y, xy / (x + y) );*/
  xy *= (phr_n - 1);
  /*  y -= D[DPS_N_EXACT]; */
  xy /= phr_n;

  y = y_phrase + y_exact + y_origin
#ifdef WITH_REL_WRDCOUNT
    + y_wrdcount + y_count
#endif
#ifdef WITH_REL_POSITION
    + y_position + y_firstpos
#endif
#ifdef WITH_REL_DISTANCE
    + y_distance
#endif
    ;

#ifdef WITH_REL_TRACK
  *D_y = y;
#endif
  return (dps_uint4)((double)100000.0 * xy / (x + y) + (double)1.0);

/*  fprintf(stderr, "2. x:%.0lf  xy:%.0lf  y:%lf {0.5*(x + xy) / (x + y):%lf}\n\n", 
	  x, xy, y, 0.5 * (x + xy) / (x + y) );*/

/*  return 50000.0 * (x + xy) / (x + y) + 1;*/

}

static inline dps_uint4 DpsCalcCosineWeightFast(dps_uint4 *R, double x, dps_uint4 *D, size_t nw
#ifdef WITH_REL_TRACK
						, double *D_y, double *D_xy
#endif
						) {
  double y = 0.0;
  double xy = 0.0;
  size_t i;

  for (i = 0; i < nw; i++) {
    xy += DpsBitsCount(R[DPS_N_ADD + i] ^ D[DPS_N_ADD + i]);
  }

  if (xy >= x) {
#ifdef WITH_REL_TRACK
    *D_y = y; *D_xy = xy;
#endif
    return (dps_uint4)1;
  }

#ifdef WITH_REL_WRDCOUNT
  if (D[DPS_N_WRDCOUNT] > R[DPS_N_WRDCOUNT]) {
    y += DPS_WRD_CNT_FACTOR * (double) (D[DPS_N_WRDCOUNT] - R[DPS_N_WRDCOUNT]);
/*    fprintf(stderr, "WRDCNT:: R: %d  D: %d -- %f\n", R[DPS_N_WRDCOUNT], D[DPS_N_WRDCOUNT],
	    DPS_WRD_CNT_FACTOR * (double) (D[DPS_N_WRDCOUNT] - R[DPS_N_WRDCOUNT]) );*/
  } else {
    y += DPS_LESS_WRD_CNT_FACTOR * (double) (R[DPS_N_WRDCOUNT] - D[DPS_N_WRDCOUNT]);
/*    fprintf(stderr, "WRDCNT:: R: %d  D: %d -- %f\n", R[DPS_N_WRDCOUNT], D[DPS_N_WRDCOUNT], 
	    DPS_WRD_CNT_FACTOR * (double) (R[DPS_N_WRDCOUNT] - D[DPS_N_WRDCOUNT]));*/
  }
  y += DPS_UNICNT_FACTOR * ((double) D[DPS_N_COUNT]);
/*  fprintf(stderr, "UNICNT:: R: %d  D: %f -- %f\n", R[DPS_N_COUNT], (double)D[DPS_N_COUNT], 
	  DPS_UNICNT_FACTOR * ((double) D[DPS_N_COUNT]));*/
#endif


#ifdef WITH_REL_POSITION
  if (D[DPS_N_POSITION] > R[DPS_N_POSITION]) {
    y += DPS_POSITION_FACTOR * (double) ((D[DPS_N_POSITION] - R[DPS_N_POSITION]) );
/*    fprintf(stderr, "POS:: R: %d  D: %d -- %f\n", R[DPS_N_POSITION], D[DPS_N_POSITION], 
	    DPS_POSITION_FACTOR * (double) ((D[DPS_N_POSITION] - R[DPS_N_POSITION]) ));*/
  } else {
    y += DPS_LESS_POSITION_FACTOR * (double) ((R[DPS_N_POSITION] - D[DPS_N_POSITION]) );
/*    fprintf(stderr, "POS:: R: %d  D: %d -- %f\n", R[DPS_N_POSITION], D[DPS_N_POSITION], 
	    DPS_POSITION_FACTOR * (double) ((R[DPS_N_POSITION] - D[DPS_N_POSITION]) ));*/
  }
#endif

#ifdef WITH_REL_DISTANCE
  if (D[DPS_N_DISTANCE] > R[DPS_N_DISTANCE]) {
    y += DPS_DISTANCE_FACTOR * (double) (D[DPS_N_DISTANCE] - R[DPS_N_DISTANCE]);
/*    fprintf(stderr, "DIST:: R: %d  D: %d -- %f\n", R[DPS_N_DISTANCE], D[DPS_N_DISTANCE], 
	    DPS_DISTANCE_FACTOR * (double) (D[DPS_N_DISTANCE] - R[DPS_N_DISTANCE]));*/
  } else {
    y += DPS_LESS_DISTANCE_FACTOR * (double) (R[DPS_N_DISTANCE] - D[DPS_N_DISTANCE]);
/*    fprintf(stderr, "DIST:: R: %d  D: %d -- %f\n", R[DPS_N_DISTANCE], D[DPS_N_DISTANCE], 
	    DPS_DISTANCE_FACTOR * (double) (R[DPS_N_DISTANCE] - D[DPS_N_DISTANCE]));*/
  }
#endif

/*  fprintf(stderr, "2. x:%.0lf  xy:%.0lf  y:%lf {(x - xy) / (x + y):%lf}\n\n", 
	  x, xy, y, (x - xy) / (x + y) );*/

#ifdef WITH_REL_TRACK
  *D_y = y; *D_xy = xy;
#endif
  return (dps_uint4)(100000.0 * (x - xy) / (x + y) + 1);
  
}


static inline dps_uint4 DpsCalcCosineWeightUltra(dps_uint4 *R, double x, double xy, dps_uint4 *D, size_t ns, size_t phr_n
#ifdef WITH_REL_TRACK
						 , double *D_y
#endif
						 ) {
  double y = 1.0;

#ifdef WITH_REL_WRDCOUNT
#if 1
  y += DPS_WRD_CNT_FACTOR * (double)D[DPS_N_WRDCOUNT];
/*    fprintf(stderr, "WRDCNT:: R: %d  D: %d -- %f\n", R[DPS_N_WRDCOUNT], D[DPS_N_WRDCOUNT],
	    DPS_WRD_CNT_FACTOR * (double) D[DPS_N_WRDCOUNT] );*/
#else
  if (D[DPS_N_WRDCOUNT] > R[DPS_N_WRDCOUNT]) {
    y += DPS_WRD_CNT_FACTOR * (double) (D[DPS_N_WRDCOUNT] - R[DPS_N_WRDCOUNT]);
/*    fprintf(stderr, "WRDCNT:: R: %d  D: %d -- %f\n", R[DPS_N_WRDCOUNT], D[DPS_N_WRDCOUNT],
	    DPS_WRD_CNT_FACTOR * (double) (D[DPS_N_WRDCOUNT] - R[DPS_N_WRDCOUNT]) );*/
  } else {
    y += DPS_LESS_WRD_CNT_FACTOR * (double) (R[DPS_N_WRDCOUNT] - D[DPS_N_WRDCOUNT]);
/*    fprintf(stderr, "WRDCNT:: R: %d  D: %d -- %f\n", R[DPS_N_WRDCOUNT], D[DPS_N_WRDCOUNT], 
	    DPS_WRD_CNT_FACTOR * (double) (R[DPS_N_WRDCOUNT] - D[DPS_N_WRDCOUNT]));*/
  }
#endif
  y += DPS_UNICNT_FACTOR * ((double) D[DPS_N_COUNT]);
/*  fprintf(stderr, "UNICNT:: R: %d  D: %f -- %f\n", R[DPS_N_COUNT], (double)D[DPS_N_COUNT], 
	  DPS_UNICNT_FACTOR * ((double) D[DPS_N_COUNT]));*/
#endif


#ifdef WITH_REL_POSITION
  if (D[DPS_N_POSITION] > R[DPS_N_POSITION]) {
    y += DPS_POSITION_FACTOR * (double) ((D[DPS_N_POSITION] - R[DPS_N_POSITION]) );
/*    fprintf(stderr, "POS:: R: %d  D: %d -- %f\n", R[DPS_N_POSITION], D[DPS_N_POSITION], 
	    DPS_POSITION_FACTOR * (double) ((D[DPS_N_POSITION] - R[DPS_N_POSITION]) ));*/
  } else {
    y += DPS_LESS_POSITION_FACTOR * (double) ((R[DPS_N_POSITION] - D[DPS_N_POSITION]) );
/*    fprintf(stderr, "POS:: R: %d  D: %d -- %f\n", R[DPS_N_POSITION], D[DPS_N_POSITION], 
	    DPS_POSITION_FACTOR * (double) ((R[DPS_N_POSITION] - D[DPS_N_POSITION]) ));*/
  }
#endif

#ifdef WITH_REL_DISTANCE
  if (D[DPS_N_DISTANCE] > R[DPS_N_DISTANCE]) {
    y += DPS_DISTANCE_FACTOR * (double) (D[DPS_N_DISTANCE] - R[DPS_N_DISTANCE]);
/*    fprintf(stderr, "DIST:: R: %d  D: %d -- %f\n", R[DPS_N_DISTANCE], D[DPS_N_DISTANCE], 
	    DPS_DISTANCE_FACTOR * (double) (D[DPS_N_DISTANCE] - R[DPS_N_DISTANCE]));*/
  } else {
    y += DPS_LESS_DISTANCE_FACTOR * (double) (R[DPS_N_DISTANCE] - D[DPS_N_DISTANCE]);
/*    fprintf(stderr, "DIST:: R: %d  D: %d -- %f\n", R[DPS_N_DISTANCE], D[DPS_N_DISTANCE], 
	    DPS_DISTANCE_FACTOR * (double) (R[DPS_N_DISTANCE] - D[DPS_N_DISTANCE]));*/
  }
#endif

/*  fprintf(stderr, "2. x:%lf  xy:%lf  y:%lf {xy /(x + y):%lf}\n\n", 
	  x, xy, log(y), xy / (x + log(y)) );*/
/*  fprintf(stderr, "2. x:%lx  xy:%lx  y:%lf {xy /(x + y):%lf}\n\n", 
	  (long)x, (long)xy, y, xy / (x + y) );*/

#ifdef WITH_REL_TRACK
  *D_y = log(y);
#endif

  return (dps_uint4)(100000.0 * xy / (x + log(y)) + 1);
  
}

#define DPS_WORD_ORIGIN_MAX 7
static inline dps_uint4 DpsOriginIndex(int origin) {
  if (origin & DPS_WORD_ORIGIN_SYNONYM) return 6;
  if (origin & DPS_WORD_ORIGIN_ASPELL)  return 5;
  if (origin & DPS_WORD_ORIGIN_ACRONYM) return 4;
  if (origin & DPS_WORD_ORIGIN_SPELL)   return 3;
  if (origin & DPS_WORD_ORIGIN_ACCENT)  return 2;
  if (origin & DPS_WORD_ORIGIN_QUERY)   return 1;
  return DPS_WORD_ORIGIN_MAX;
}

static inline dps_uint4 DpsOriginWeightFull(int origin) {  /* Weight for origin can be from 1 to 15 */
  if (origin & DPS_WORD_ORIGIN_SYNONYM) return 0x01;
  if (origin & DPS_WORD_ORIGIN_ASPELL)  return 0x02;
  if (origin & DPS_WORD_ORIGIN_ACRONYM) return 0x04; /*0x14;*/
  if (origin & DPS_WORD_ORIGIN_SPELL)   return 0x08; /*0x0A;*/
  if (origin & DPS_WORD_ORIGIN_ACCENT)  return 0x10; /*0x21;*/
  if (origin & DPS_WORD_ORIGIN_QUERY)   return 0x30; /*0x24;*/
  if (origin & DPS_WORD_ORIGIN_COMMON)  return 0x3F;
  return 0;
}

static inline dps_uint4 DpsOriginWeightFast(int origin) {  /* Weight for origin can be from 1 to 15 */
  if (origin & DPS_WORD_ORIGIN_SYNONYM) return 0x1;
  if (origin & DPS_WORD_ORIGIN_ASPELL)  return 0x3;
  if (origin & DPS_WORD_ORIGIN_ACRONYM) return 0x5;
  if (origin & DPS_WORD_ORIGIN_SPELL)   return 0xA;
  if (origin & DPS_WORD_ORIGIN_ACCENT)  return 0xB;
  if (origin & DPS_WORD_ORIGIN_QUERY)   return 0xE;
  return 0;
}

static inline dps_uint4 DpsOriginWeightUltra(int origin) {  /* Weight for origin can be from 1 to 15 */
  if (origin & DPS_WORD_ORIGIN_ASPELL)  return 0x010;
  if (origin & DPS_WORD_ORIGIN_SYNONYM) return 0x050;
  if (origin & DPS_WORD_ORIGIN_ACRONYM) return 0x050;
  if (origin & DPS_WORD_ORIGIN_ACCENT)  return 0x330;
  if (origin & DPS_WORD_ORIGIN_SPELL)   return 0x170;
  if (origin & DPS_WORD_ORIGIN_QUERY)   return 0x7C0;
  if (origin & DPS_WORD_ORIGIN_COMMON)  return 0x7F0;
  return 0;
}


#define DPS_DISTANCE_INIT (150 + (10 * DPS_BEST_WRD_CNT))
#define DPS_POSITION_INIT (500 + (10 * DPS_BEST_WRD_CNT))
#define DPS_ORDER_PENALTY 16

#define DPS_WRDSEC_N(x,y) (DPS_WRDSEC((x)) % (y))


static void DpsGroupByURLFull(DPS_AGENT *query, DPS_RESULT *Res) {
  size_t	i, j = 0, D_size, R_size, phr_n;
  size_t  *count, *xy_o, count_size;
  size_t wordsec, wordpos, *prev_wordpos, wordnum, prev_wordnum, wordorder, prev_wordorder;
  size_t n_order_inquery = Res->max_order_inquery + 1;
  DPS_URL_CRD_DB *Crd;
#ifdef WITH_REL_TRACK
  DPS_URLTRACK *Track;
#endif
  size_t nsections = (size_t)DpsVarListFindInt(&query->Vars, "NumSections", 255) + 1;
  size_t cur_order, cur_exact, cur_sec;
  int wf[256];
  double Rbc;
  dps_uint4 *R, *D;
  int xy_wf;
  int nsec;
  size_t nwordpos;
  int w_origin;
  dps_uint4 a;
  size_t xy;
#ifdef WITH_REL_WRDCOUNT
  size_t tt, sum;
  size_t median;
#endif

  TRACE_IN(query, "DpsGroupByURLFull");

  DpsLog(query, DPS_LOG_DEBUG, "max_order: %d  max_order_inquery: %d", Res->max_order, Res->max_order_inquery);

#ifdef WITH_REL_TRACK
  Track = Res->CoordList.Track = (DPS_URLTRACK*)DpsRealloc(Res->CoordList.Track, (Res->CoordList.ncoords + 1) * sizeof(*Res->CoordList.Track));
  if (Track == NULL) {TRACE_OUT(query); return; }
#endif

  if (DPS_OK != DpsCalcBoolItems(query, Res)) {TRACE_OUT(query); return; }
  if(!Res->CoordList.ncoords || Res->nitems == 0) {TRACE_OUT(query); return; }

  /*  DpsLog(query, DPS_LOG_EXTRA, "\tBoolean.nrecords:%d", Res->CoordList.ncoords);*/
/*  DpsSortSearchWordsByURL(Res->CoordList.Coords, Res->CoordList.ncoords);*/


  Crd = Res->CoordList.Coords;
#ifdef DEBUG_MEM
  mprotect(Crd, sizeof(*Crd) *  Res->CoordList.ncoords, PROT_READ);
#endif

  count_size = (3 * Res->max_order_inquery + DPS_WORD_ORIGIN_MAX + 10) * sizeof(size_t);

  if ((count = (size_t*)DpsXmalloc(2 * count_size + 1)) == NULL) {TRACE_OUT(query); return; }
  xy_o = count + Res->max_order_inquery + DPS_WORD_ORIGIN_MAX + 1;
  prev_wordpos = xy_o + Res->max_order_inquery + 1;

  DpsWeightFactorsInit(DpsVarListFindStr(&query->Vars, "wf", ""), wf);

  D_size = (1 + nsections  + DPS_N_ADD) * sizeof(dps_uint4);
  R_size = (1 + DPS_N_ADD) * sizeof(dps_uint4);
  if ((R = (dps_uint4*)DpsXmalloc(R_size)) == NULL) {
    DPS_FREE(count);
    TRACE_OUT(query);
    return;
  }
  if ((D = (dps_uint4*)DpsXmalloc(2 * D_size)) == NULL) {
    DPS_FREE(count); DPS_FREE(R);
    TRACE_OUT(query);
    return;
  }


#ifdef WITH_REL_DISTANCE
  R[DPS_N_DISTANCE] = DPS_AVG_DISTANCE;
#endif
#ifdef WITH_REL_POSITION
  R[DPS_N_POSITION] = DPS_AVG_POSITION;
  R[DPS_N_FIRSTPOS] = DPS_BEST_POSITION;
#endif
#ifdef WITH_REL_WRDCOUNT
  R[DPS_N_WRDCOUNT] = DPS_BEST_WRD_CNT  * n_order_inquery;
#endif
  R[DPS_N_COUNT] = 0;
  R[DPS_N_ORIGIN] = 1;

  wordnum = DPS_WRDNUM(Crd[0].coord);
  wordsec = DPS_WRDSEC_N(Crd[0].coord, nsections);
  prev_wordorder = wordorder = Res->WWList.Word[wordnum].order_inquery;
  prev_wordpos[prev_wordorder] = wordpos = DPS_WRDPOS(Crd[0].coord);
  if (wordorder == 0) {
    cur_order = 0; cur_sec = wordsec;
    if (Res->WWList.Word[wordnum].origin == DPS_WORD_ORIGIN_QUERY) cur_exact = 1; else cur_exact = 0;
  } else { cur_order = (size_t)-1; cur_exact = 0; }

  for(xy_wf = 0, i = 1; i < nsections; i++) xy_wf += wf[i];
  Rbc = (double)1.0 * DpsOriginWeightFull(DPS_WORD_ORIGIN_COMMON) * n_order_inquery * xy_wf;

  xy_o[wordorder] = DpsOriginWeightFull(Res->WWList.Word[wordnum].origin);
  nsec = wf[wordsec]; D[DPS_N_ADD + wordsec] = 1;

/**********************************************/

#ifdef WITH_REL_DISTANCE
  D[DPS_N_DISTANCE] = DPS_DISTANCE_INIT;
#endif
#ifdef WITH_REL_POSITION
  D[DPS_N_POSITION] =  DPS_POSITION_INIT + (D[DPS_N_FIRSTPOS] = wordpos);
#ifdef WITH_REL_TRACK
  Track[0].D_firstpos = wordpos;
#endif
#endif
  count[wordorder]++;
  count[Res->max_order_inquery + DpsOriginIndex(Res->WWList.Word[wordnum].origin)]++;
  phr_n = 3;
  j = 0;

/*  fprintf(stderr, "wordnum:%d  wordsec:%d  wordpos:%d  WeightFul:%x  xy_o:%x\n", wordnum, wordsec, wordpos,
	  DpsOriginWeightFull(Res->WWList.Word[wordnum].origin), xy_o );*/

  for(i = 1; i < Res->CoordList.ncoords; i++) {
    /* Group by url_id */
    nwordpos = DPS_WRDPOS(Crd[i].coord);
    if (wordpos != nwordpos) {
	prev_wordpos[wordorder] = wordpos;
	prev_wordnum = wordnum;
	prev_wordorder = wordorder;
	wordpos = nwordpos;
    }
    
    wordnum = DPS_WRDNUM(Crd[i].coord);
    wordsec = DPS_WRDSEC_N(Crd[i].coord, nsections);
    wordorder = Res->WWList.Word[wordnum].order_inquery;

/*    fprintf(stderr, "wordnum:%d  wordsec:%d  wordpos:%d  wordorder:%d  cur_order:%d  WeightFul:%x  xy_o:%x\n", 
	    wordnum, wordsec, wordpos, wordorder, cur_order, DpsOriginWeightFull(Res->WWList.Word[wordnum].origin), xy_o );*/

    if(Crd[j].url_id == Crd[i].url_id) {
      /* Same document */
      w_origin = Res->WWList.Word[wordnum].origin;
      a = DpsOriginWeightFull(w_origin);
      xy_o[wordorder] |= a;
      if (D[DPS_N_ADD + wordsec] == 0) nsec += wf[wordsec];
      D[DPS_N_ADD + wordsec]++;

      phr_n++;
#ifdef WITH_REL_POSITION
      D[DPS_N_POSITION] += wordpos;
      if (count[wordorder] == 0) D[DPS_N_FIRSTPOS] += wordpos;
#endif
#ifdef WITH_REL_DISTANCE
      D[DPS_N_DISTANCE] += (wordpos - prev_wordpos[(wordorder) ? (wordorder - 1) : Res->max_order_inquery]);
#endif
      count[wordorder]++;
      count[Res->max_order_inquery + DpsOriginIndex(w_origin)]++;
      if ((wordorder == cur_order + 1) && ((cur_order == (size_t)-1) || ((wordpos == prev_wordpos[prev_wordorder] + 1) && (wordsec == cur_sec)))) {
	cur_order++;
	if (cur_order == 0) { cur_sec = wordsec; cur_exact = (w_origin == DPS_WORD_ORIGIN_QUERY); }
	else cur_exact *= (w_origin == DPS_WORD_ORIGIN_QUERY);
      	if (cur_order == Res->max_order_inquery) {
	  D[DPS_N_PHRASE] += 2; cur_order = (size_t)-1;
		if (cur_exact) D[DPS_N_EXACT] += 2;
		/*		cur_exact = (wordorder == 0) ? (w_origin == DPS_WORD_ORIGIN_QUERY) : 0;*/
	}
      } else if ((cur_order != wordorder) && ((wordorder != prev_wordorder) || (wordpos != prev_wordpos[prev_wordorder]))) {
	    if (wordorder == 0) {
	      cur_order = 0; cur_sec = wordsec; cur_exact = (w_origin == DPS_WORD_ORIGIN_QUERY);
	    } else cur_order = (size_t)-1; 
      }

    } else {
      /* Next document */

#ifdef WITH_REL_DISTANCE
      D[DPS_N_DISTANCE] /= phr_n;
#ifdef WITH_REL_TRACK
      Track[j].D_distance = D[DPS_N_DISTANCE];
#endif
#endif
#ifdef WITH_REL_POSITION
      D[DPS_N_POSITION] /= phr_n;
#ifdef WITH_REL_TRACK
      Track[j].D_position = D[DPS_N_POSITION];
#endif
#endif

      xy = 0;
#ifdef WITH_REL_WRDCOUNT
      sum = 0;
      median = phr_n / n_order_inquery;
#endif
      for (tt = 0; tt <= Res->max_order_inquery; tt++) {
	  xy += xy_o[tt];
#ifdef WITH_REL_WRDCOUNT
	  if (Res->items[tt].order_origin & DPS_WORD_ORIGIN_STOP) continue;
	  sum += ((count[tt] > median) ? (count[tt] - median) : (median - count[tt]));
#endif
      }
      D[DPS_N_ORIGIN] = tt;
      for (tt++; tt < Res->max_order_inquery + DPS_WORD_ORIGIN_MAX; tt++) {
	  if (count[D[DPS_N_ORIGIN]] < count[tt]) D[DPS_N_ORIGIN] = tt;
      }
      D[DPS_N_ORIGIN] -= Res->max_order_inquery;
/*	xy /= Res->max_order_inquery + 1;*/
#ifdef WITH_REL_WRDCOUNT
      D[DPS_N_WRDCOUNT] = phr_n;
      D[DPS_N_COUNT] = (dps_uint4)((((long)sum) * DPS_BEST_WRD_CNT) / n_order_inquery / ++median);
      
#ifdef WITH_REL_TRACK
      Track[j].D_wrdcount = phr_n;
      Track[j].D_n_count = D[DPS_N_COUNT];
      Track[j].D_n_origin = D[DPS_N_ORIGIN];
#endif
#endif
      Crd[j].coord = DpsCalcCosineWeightFull(
	  R, 
	  Rbc, 
	  (double)1.0 * xy * nsec , 
	  D, 
	  phr_n
#ifdef WITH_REL_TRACK
	  , &Track[j].y
#endif
	  );
#ifdef WITH_REL_TRACK
      Track[j].x = Rbc;
      Track[j].xy = xy * nsec * phr_n / (phr_n + 1);
#endif
      
      j++;

      Crd[j] = Crd[i];
      dps_memcpy(D, (char*)D + D_size, D_size);
      dps_memcpy(count, (char*)count + count_size, count_size);

      phr_n = 3;
#ifdef WITH_REL_DISTANCE
      D[DPS_N_DISTANCE] = DPS_DISTANCE_INIT;
#endif
#ifdef WITH_REL_POSITION
      D[DPS_N_POSITION] = DPS_POSITION_INIT + (D[DPS_N_FIRSTPOS] = wordpos);
#ifdef WITH_REL_TRACK
      Track[j].D_firstpos = wordpos;
#endif
#endif
      prev_wordpos[wordorder] = wordpos;
      w_origin = Res->WWList.Word[wordnum].origin;
      count[wordorder] = count[Res->max_order_inquery + DpsOriginIndex(w_origin)] = 1;
      if (wordorder == 0) {
	  cur_order = 0; cur_sec = wordsec; cur_exact = (w_origin == DPS_WORD_ORIGIN_QUERY);
      } else cur_order = (size_t)-1; 
      xy_o[wordorder] = DpsOriginWeightFull(w_origin);
      
      nsec = wf[wordsec]; D[DPS_N_ADD + wordsec] = 1;
    }
  }
  
  /* Check last word */
#ifdef WITH_REL_DISTANCE
  D[DPS_N_DISTANCE] /= phr_n;
#ifdef WITH_REL_TRACK
  Track[j].D_distance = D[DPS_N_DISTANCE];
#endif
#endif
#ifdef WITH_REL_POSITION
  D[DPS_N_POSITION] /= phr_n;
#ifdef WITH_REL_TRACK
  Track[j].D_position = D[DPS_N_POSITION];
#endif
#endif

   
  xy = 0;
#ifdef WITH_REL_WRDCOUNT
  sum = 0;
  median = phr_n / n_order_inquery;
#endif
  for (tt = 0; tt <= Res->max_order_inquery; tt++) {
      xy += xy_o[tt];
#ifdef WITH_REL_WRDCOUNT
      if (Res->items[tt].order_origin & DPS_WORD_ORIGIN_STOP) continue;
      sum += ((count[tt] > median) ? (count[tt] - median) : (median - count[tt]));
#endif
  }
  D[DPS_N_ORIGIN] = tt;
  for (tt++; tt < Res->max_order_inquery + DPS_WORD_ORIGIN_MAX; tt++) {
      if (count[D[DPS_N_ORIGIN]] < count[tt]) D[DPS_N_ORIGIN] = tt;
  }
  D[DPS_N_ORIGIN] -= Res->max_order_inquery;
  /*    xy /= Res->max_order_inquery + 1;*/
#ifdef WITH_REL_WRDCOUNT
  D[DPS_N_WRDCOUNT] = phr_n;
  D[DPS_N_COUNT] = (dps_uint4)((((long)sum) * DPS_BEST_WRD_CNT) / n_order_inquery / ++median);
  
#ifdef WITH_REL_TRACK
  Track[j].D_wrdcount = phr_n;
  Track[j].D_n_count = D[DPS_N_COUNT];
  Track[j].D_n_origin = D[DPS_N_ORIGIN];
#endif
#endif
	
  Res->CoordList.ncoords = j + 1;
	
  Crd[j].coord = DpsCalcCosineWeightFull(R, Rbc, (double)1.0 * xy * nsec, D, phr_n
#ifdef WITH_REL_TRACK
					 , &Track[j].y
#endif
      );
#ifdef WITH_REL_TRACK
  Track[j].x = Rbc;
  Track[j].xy = xy * nsec * (phr_n - 1) / phr_n;
  Res->CoordList.Track = (DPS_URLTRACK*)DpsRealloc(Res->CoordList.Track, Res->CoordList.ncoords * sizeof(*Res->CoordList.Track));
#endif
  
  DPS_FREE(D);
  DPS_FREE(R);
  DPS_FREE(count);
  TRACE_OUT(query);
  return;
}


static void DpsGroupByURLFast(DPS_AGENT *query, DPS_RESULT *Res) {
  size_t	i, j = 0, D_size, R_size, phr_n;
  size_t  *count, count_size;
  size_t wordsec, wordpos, prev_wordpos, wordnum, prev_wordnum, wordorder, prev_wordorder;
  size_t n_order_inquery = Res->max_order_inquery + 1;
  DPS_URL_CRD_DB *Crd;
#ifdef WITH_REL_TRACK
  DPS_URLTRACK *Track;
#endif
  size_t nsections = (size_t)DpsVarListFindInt(&query->Vars, "NumSections", 255) + 1;
  int wf[256];
  dps_uint4 *R, *D;
  double Rbc;
  size_t tt, sum;
  size_t median;

  TRACE_IN(query, "DpsGroupByURLFast");

  DpsLog(query, DPS_LOG_DEBUG, "max_order: %d  max_order_inquery: %d", Res->max_order, Res->max_order_inquery);

#ifdef WITH_REL_TRACK
  Track = Res->CoordList.Track = (DPS_URLTRACK*)DpsRealloc(Res->CoordList.Track, Res->CoordList.ncoords * sizeof(*Res->CoordList.Track));
  if (Track == NULL) {TRACE_OUT(query); return; }
#endif

  if (DPS_OK != DpsCalcBoolItems(query, Res)) {TRACE_OUT(query); return; }
  if(!Res->CoordList.ncoords || Res->nitems == 0) {TRACE_OUT(query); return; }
/*  DpsSortSearchWordsByURL(Res->CoordList.Coords, Res->CoordList.ncoords);*/

  Crd = Res->CoordList.Coords;
  count_size = (Res->max_order_inquery + 1) * sizeof(size_t);

  if ((count = (size_t*)DpsXmalloc(count_size + 1)) == NULL) {TRACE_OUT(query); return; }

  DpsWeightFactorsInit(DpsVarListFindStr(&query->Vars, "wf", ""), wf);

  D_size = (1 + Res->WWList.nwords + DPS_N_ADD) * sizeof(dps_uint4);
  R_size = (1 + Res->WWList.nwords + DPS_N_ADD) * sizeof(dps_uint4);
  if ((R = (dps_uint4*)DpsXmalloc(R_size)) == NULL) {
    DPS_FREE(count);
    TRACE_OUT(query);
    return;
  }
  if ((D = (dps_uint4*)DpsXmalloc(D_size)) == NULL) {
    DPS_FREE(count); DPS_FREE(R);
    TRACE_OUT(query);
    return;
  }

#ifdef WITH_REL_DISTANCE
  R[DPS_N_DISTANCE] = DPS_AVG_DISTANCE; /*Res->WWList.nuniq;*/ /*(Res->WWList.nuniq == 1) ? 1 : 2;*/ /* Res->WWList.nuniq - 1;*/
#endif
#ifdef WITH_REL_POSITION
  R[DPS_N_POSITION] = DPS_AVG_POSITION;
  R[DPS_N_FIRSTPOS] = DPS_BEST_POSITION;
#endif
#ifdef WITH_REL_WRDCOUNT
  R[DPS_N_WRDCOUNT] = DPS_BEST_WRD_CNT * (Res->max_order_inquery + 1);
#endif
  R[DPS_N_COUNT] = 0;

  wordnum = DPS_WRDNUM(Crd[0].coord);
  wordsec = DPS_WRDSEC_N(Crd[0].coord, nsections);
  prev_wordpos = wordpos = DPS_WRDPOS(Crd[0].coord);
  wordorder = Res->WWList.Word[wordnum].order_inquery;

  Rbc = (double)0.0;

  for(i = 0; i < Res->WWList.nwords; i++) {
    for (j = 1; j < nsections; j++) {
      R[DPS_N_ADD + i] |= WF_ADD(j);
    }
    R[DPS_N_ADD + i] |= 0xF; 
    Rbc += DpsBitsCount(R[DPS_N_ADD + i]);
/*    Rbc += 32.0;*/
  }
		
  D[DPS_N_ADD + wordnum] |= (WF_ADD(wordsec) | DpsOriginWeightFast(Res->WWList.Word[wordnum].origin));

/**********************************************/

#ifdef WITH_REL_DISTANCE
  D[DPS_N_DISTANCE] = DPS_DISTANCE_INIT;
#endif
#ifdef WITH_REL_POSITION
  D[DPS_N_POSITION] =  wordpos; /*1000*//*DPS_BEST_AVG_POSITION * Res->WWList.nwords*/ /* + */
#ifdef WITH_REL_TRACK
  Track[0].D_firstpos = wordpos;
#endif
#endif
  count[wordorder]++;
  phr_n = 1;
  j = 0;

  for(i = 1; i < Res->CoordList.ncoords; i++) {
    /* Group by url_id */
    prev_wordpos = wordpos;
    prev_wordnum = wordnum;
    prev_wordorder = wordorder;
    wordnum = DPS_WRDNUM(Crd[i].coord);
    wordsec = DPS_WRDSEC_N(Crd[i].coord, nsections);
    wordpos = DPS_WRDPOS(Crd[i].coord);
    wordorder = Res->WWList.Word[wordnum].order_inquery;

/*	  fprintf(stderr, "wordnum:%d  wordsec:%d  wordpos:%d\n", wordnum, wordsec, wordpos);*/

    if(Crd[j].url_id == Crd[i].url_id) {
      /* Same document */
      D[DPS_N_ADD + wordnum] |= (WF_ADD(wordsec) | DpsOriginWeightFast(Res->WWList.Word[wordnum].origin));

      phr_n++;
#ifdef WITH_REL_POSITION
/*	    D[DPS_N_POSITION] += wordpos;*/
#endif
#ifdef WITH_REL_DISTANCE
/*	    D[DPS_N_DISTANCE] += (wordnum == prev_wordnum + 1) ? (wordpos - prev_wordpos) : (1000 + wordpos - prev_wordpos);*/
      D[DPS_N_DISTANCE] += (wordpos - prev_wordpos);
      if ((prev_wordorder != (wordorder + 1) % n_order_inquery) && (wordorder != (prev_wordorder + 1) % n_order_inquery)) D[DPS_N_DISTANCE] += DPS_ORDER_PENALTY;
#endif
      count[wordorder]++;

    } else {
      /* Next document */

#ifdef WITH_REL_DISTANCE
      D[DPS_N_DISTANCE] /= phr_n;
#ifdef WITH_REL_TRACK
      Track[j].D_distance = D[DPS_N_DISTANCE];
#endif
#endif
#ifdef WITH_REL_POSITION
/*	    D[DPS_N_POSITION] /= phr_n;*/
#ifdef WITH_REL_TRACK
      Track[j].D_position = D[DPS_N_POSITION];
#endif
#endif
#ifdef WITH_REL_WRDCOUNT
      D[DPS_N_WRDCOUNT] = phr_n;

      sum = 0;
      median = (size_t)(((double)phr_n) / (Res->max_order_inquery + 1));
      for (tt = 0; tt <= Res->max_order_inquery; tt++) {
	  if (count[tt])
	      sum += ((count[tt] > median) ? (count[tt] - median) : (median - count[tt]));
	  else sum += 2000;
      }
      D[DPS_N_COUNT] = (dps_uint4)sum;
      
#ifdef WITH_REL_TRACK
      Track[j].D_wrdcount = phr_n;
      Track[j].D_n_count = D[DPS_N_COUNT];
#endif
#endif
/*	    fprintf(stderr, "** URL_ID: %d [phr_n:%d]\n", Crd[j].url_id, phr_n);*/
      Crd[j].coord = DpsCalcCosineWeightFast(R, Rbc, D, Res->WWList.nwords
#ifdef WITH_REL_TRACK
					     , &Track[j].y, &Track[j].xy
#endif
					     );
#ifdef WITH_REL_TRACK
      Track[j].x = Rbc;
#endif
      j++;

      Crd[j] = Crd[i];

      bzero((void*)D, D_size);
      bzero((void*)count, count_size);
      phr_n = 1;
#ifdef WITH_REL_DISTANCE
      D[DPS_N_DISTANCE] = DPS_DISTANCE_INIT;
#endif
#ifdef WITH_REL_POSITION
      D[DPS_N_POSITION] =  wordpos;
#ifdef WITH_REL_TRACK
      Track[j].D_firstpos = wordpos;
#endif
#endif
      count[wordorder] = 1;
/*	    count[Res->WWList.Word[wordnum].order]++;*/
      D[DPS_N_ADD + wordnum] = (WF_ADD(wordsec) | DpsOriginWeightFast(Res->WWList.Word[wordnum].origin));
    }
  }

	/* Check last word */
#ifdef WITH_REL_DISTANCE
  D[DPS_N_DISTANCE] /= phr_n;
#ifdef WITH_REL_TRACK
  Track[j].D_distance = D[DPS_N_DISTANCE];
#endif
#endif
#ifdef WITH_REL_POSITION
/*  D[DPS_N_POSITION] /= phr_n;*/
#ifdef WITH_REL_TRACK
  Track[j].D_position = D[DPS_N_POSITION];
#endif
#endif
#ifdef WITH_REL_WRDCOUNT
  D[DPS_N_WRDCOUNT] = phr_n;

  sum = 0;
  median = (size_t)(((double)phr_n) / (Res->max_order_inquery + 1));
  for (tt = 0; tt <= Res->max_order_inquery; tt++) {
      if (count[tt])
	  sum += ((count[tt] > median) ? (count[tt] - median) : (median - count[tt]));
      else sum += 2000;
  }
  D[DPS_N_COUNT] = (dps_uint4)sum;
  
#ifdef WITH_REL_TRACK
  Track[j].D_wrdcount = phr_n;
  Track[j].D_n_count = D[DPS_N_COUNT];
#endif
#endif
	
  Res->CoordList.ncoords = j + 1;
/*	fprintf(stderr, "** URL_ID: %d [phr_n:%d]\n", Crd[j].url_id, phr_n);*/
	
  Crd[j].coord = DpsCalcCosineWeightFast(R, Rbc, D, Res->WWList.nwords
#ifdef WITH_REL_TRACK
					 , &Track[j].y, &Track[j].xy
#endif
					 );
#ifdef WITH_REL_TRACK
  Track[j].x = Rbc;
  Res->CoordList.Track = (DPS_URLTRACK*)DpsRealloc(Res->CoordList.Track, Res->CoordList.ncoords * sizeof(*Res->CoordList.Track));
#endif

  DPS_FREE(D);
  DPS_FREE(R);
  DPS_FREE(count);
  TRACE_OUT(query);
  return;
}


static void DpsGroupByURLUltra(DPS_AGENT *query, DPS_RESULT *Res) {
  size_t	i, j = 0, D_size, R_size, phr_n;
  size_t  *count, count_size;
  size_t wordsec, wordpos, prev_wordpos, wordnum, prev_wordnum, wordorder, prev_wordorder;
  size_t n_order_inquery = Res->max_order_inquery + 1;
  DPS_URL_CRD_DB *Crd;
#ifdef WITH_REL_TRACK
  DPS_URLTRACK *Track;
#endif
  size_t nsections = (size_t)DpsVarListFindInt(&query->Vars, "NumSections", 255) + 1;
  int wf[256];
  int xy_w;
  dps_uint4 *R, *D;
  double Rbc;
  double xy, WFsections;
  size_t tt, cc, median, sum;

  TRACE_IN(query, "DpsGroupByURLUltra");
  
  DpsLog(query, DPS_LOG_DEBUG, "max_order: %d  max_order_inquery: %d", Res->max_order, Res->max_order_inquery);

#ifdef WITH_REL_TRACK
  Track = Res->CoordList.Track = (DPS_URLTRACK*)DpsRealloc(Res->CoordList.Track, (Res->CoordList.ncoords + 1) * sizeof(*Res->CoordList.Track));
  if (Track == NULL) {TRACE_OUT(query); return; }
#endif

  if (DPS_OK != DpsCalcBoolItems(query, Res)) {TRACE_OUT(query); return; }
  if(!Res->CoordList.ncoords || Res->nitems == 0) {TRACE_OUT(query); return; }
/*  DpsSortSearchWordsByURL(Res->CoordList.Coords, Res->CoordList.ncoords);*/

  Crd = Res->CoordList.Coords;
  count_size = (Res->max_order_inquery + 1) * sizeof(size_t);

  if ((count = (size_t*)DpsXmalloc(count_size + 1)) == NULL) {TRACE_OUT(query); return; }

  DpsWeightFactorsInit(DpsVarListFindStr(&query->Vars, "wf", ""), wf);

  D_size = (2 + nsections + Res->max_order_inquery + DPS_N_ADD) * sizeof(dps_uint4);
  R_size = (1 + DPS_N_ADD) * sizeof(dps_uint4);
  if ((R = (dps_uint4*)DpsXmalloc(R_size)) == NULL) {
    DPS_FREE(count);
    TRACE_OUT(query);
    return;
  }
  if ((D = (dps_uint4*)DpsXmalloc(D_size)) == NULL) {
    DPS_FREE(count); DPS_FREE(R);
    TRACE_OUT(query);
    return;
  }
  WFsections = 0.0;
  for (tt = 1; tt < nsections; tt++) WFsections += wf[tt];

#ifdef WITH_REL_DISTANCE
  R[DPS_N_DISTANCE] = DPS_AVG_DISTANCE;
#endif
#ifdef WITH_REL_POSITION
  R[DPS_N_POSITION] = DPS_AVG_POSITION;
  R[DPS_N_FIRSTPOS] = DPS_BEST_POSITION;
#endif
#ifdef WITH_REL_WRDCOUNT
  R[DPS_N_WRDCOUNT] = DPS_BEST_WRD_CNT * (Res->max_order_inquery + 1);
#endif
  R[DPS_N_COUNT] = 0;
  R[DPS_N_ORIGIN] = 1; /* DpsOriginIndex(DPS_WORD_ORIGIN_QUERY); */

  wordnum = DPS_WRDNUM(Crd[0].coord);
  wordsec = DPS_WRDSEC_N(Crd[0].coord, nsections);
  prev_wordpos = wordpos = DPS_WRDPOS(Crd[0].coord);
  wordorder = Res->WWList.Word[wordnum].order_inquery;

  Rbc =  1.0;

  D[DPS_N_ADD + wordsec]=1;
  xy = (double)(wf[wordsec]);
  xy_w = D[DPS_N_ADD + nsections + wordorder] = DpsOriginWeightUltra(Res->WWList.Word[wordnum].origin);

/**********************************************/

#ifdef WITH_REL_DISTANCE
  D[DPS_N_DISTANCE] = DPS_DISTANCE_INIT;
#endif
#ifdef WITH_REL_POSITION
  D[DPS_N_POSITION] =  wordpos; /*1000*//*DPS_BEST_AVG_POSITION * Res->WWList.nwords*/ /* + */
#ifdef WITH_REL_TRACK
  Track[0].D_firstpos = wordpos;
#endif
#endif
  D[DPS_N_ADD + wordsec] = 1;
  count[wordorder]++;
  phr_n = 1;
  j = 0;

  for(i = 1; i < Res->CoordList.ncoords; i++) {
    /* Group by url_id */
    prev_wordpos = wordpos;
    prev_wordnum = wordnum;
    prev_wordorder = wordorder;
    wordnum = DPS_WRDNUM(Crd[i].coord);
    wordsec = DPS_WRDSEC_N(Crd[i].coord, nsections);
    wordpos = DPS_WRDPOS(Crd[i].coord);
    wordorder = Res->WWList.Word[wordnum].order_inquery;
    
/*	  fprintf(stderr, "wordnum:%d  wordsec:%d  wordpos:%d\n", wordnum, wordsec, wordpos);*/

    if(Crd[j].url_id == Crd[i].url_id) {
      /* Same document */
      /*if (DPS_WORD_ORIGIN_QUERY & Res->WWList.Word[wordnum].origin)*/ D[DPS_N_ADD + wordsec] = 1;
      xy += (double)(wf[wordsec]);
      xy_w |= DpsOriginWeightUltra(Res->WWList.Word[wordnum].origin);
      D[DPS_N_ADD + nsections + wordorder] += DpsOriginWeightUltra(Res->WWList.Word[wordnum].origin);

      phr_n++;
#ifdef WITH_REL_POSITION
/*	    D[DPS_N_POSITION] += wordpos;*/
#endif
#ifdef WITH_REL_DISTANCE
/*	    D[DPS_N_DISTANCE] += (wordnum == prev_wordnum + 1) ? (wordpos - prev_wordpos) : (1000 + wordpos - prev_wordpos);*/
      D[DPS_N_DISTANCE] += (wordpos - prev_wordpos);
      if ((prev_wordorder != (wordorder + 1) % n_order_inquery) && (wordorder != (prev_wordorder + 1) % n_order_inquery)) D[DPS_N_DISTANCE] += DPS_ORDER_PENALTY;
#endif
      count[wordorder]++;

    } else {
      /* Next document */

#ifdef WITH_REL_DISTANCE
      D[DPS_N_DISTANCE] /= phr_n;
#ifdef WITH_REL_TRACK
      Track[j].D_distance = D[DPS_N_DISTANCE];
#endif
#endif
#ifdef WITH_REL_POSITION
/*	    D[DPS_N_POSITION] /= phr_n;*/
#ifdef WITH_REL_TRACK
      Track[j].D_position = D[DPS_N_POSITION];
#endif
#endif

      xy /= phr_n * 0xF;
      xy *= xy_w;
      xy /= DpsOriginWeightUltra(DPS_WORD_ORIGIN_COMMON);

#ifdef WITH_REL_WRDCOUNT
      D[DPS_N_WRDCOUNT] = 0/*phr_n*/;
#endif

      cc = 0;
#ifdef WITH_REL_WRDCOUNT
      median = (size_t)(((double)phr_n) / (Res->max_order_inquery + 1)), sum = 0;
#endif
      for (tt = 0; tt <= Res->max_order_inquery; tt++) {
	  if (count[tt]) cc += D[DPS_N_ADD + nsections + tt] / count[tt];
#ifdef WITH_REL_WRDCOUNT
	  if (count[tt])
	      sum += ((count[tt] > median) ? (count[tt] - median) : (median - count[tt]));
	  else sum += 2000;
	  D[DPS_N_WRDCOUNT] += ((count[tt] > DPS_BEST_WRD_CNT) ? (count[tt] - DPS_BEST_WRD_CNT) : (DPS_BEST_WRD_CNT - count[tt]));
#endif
      }
#ifdef WITH_REL_WRDCOUNT
      D[DPS_N_COUNT] = (dps_uint4)sum;
#ifdef WITH_REL_TRACK
      Track[j].D_wrdcount = phr_n;
      Track[j].D_n_count = D[DPS_N_COUNT];
#endif
#endif
      xy *= cc;
      xy /= (Res->max_order_inquery + 1) * DpsOriginWeightUltra(DPS_WORD_ORIGIN_QUERY);
      

/*	    fprintf(stderr, "** URL_ID: %d [phr_n:%d]\n", Crd[j].url_id, phr_n);*/

      cc = 1;
      for (tt = 1; tt < nsections; tt++) if (D[DPS_N_ADD + tt]) cc += wf[tt];
      xy *= cc;
      xy /= WFsections;
      

      Crd[j].coord = DpsCalcCosineWeightUltra(R, Rbc, xy, D, nsections, phr_n
#ifdef WITH_REL_TRACK
					     , &Track[j].y
#endif
					      );
#ifdef WITH_REL_TRACK
      Track[j].x = Rbc;
      Track[j].xy = xy;
#endif
      j++;

      Crd[j] = Crd[i];

      bzero((void*)D, D_size);
      bzero((void*)count, count_size);
      phr_n = 1;
#ifdef WITH_REL_DISTANCE
      D[DPS_N_DISTANCE] = DPS_DISTANCE_INIT;
#endif
#ifdef WITH_REL_POSITION
      D[DPS_N_POSITION] =  wordpos;
#ifdef WITH_REL_TRACK
      Track[j].D_firstpos = wordpos;
#endif
#endif
      count[wordorder] = 1;
/*	    count[Res->WWList.Word[wordnum].order]++;*/
      /*if (DPS_WORD_ORIGIN_QUERY & Res->WWList.Word[wordnum].origin)*/ D[DPS_N_ADD + wordsec] = 1;
      xy = (double)(wf[wordsec]);
      xy_w = D[DPS_N_ADD + nsections + wordorder] = DpsOriginWeightUltra(Res->WWList.Word[wordnum].origin);
    }
  }

	/* Check last word */
#ifdef WITH_REL_DISTANCE
  D[DPS_N_DISTANCE] /= phr_n;
#ifdef WITH_REL_TRACK
  Track[j].D_distance = D[DPS_N_DISTANCE];
#endif
#endif
#ifdef WITH_REL_POSITION
/*  D[DPS_N_POSITION] /= phr_n;*/
#ifdef WITH_REL_TRACK
  Track[j].D_position = D[DPS_N_POSITION];
#endif
#endif
#ifdef WITH_REL_WRDCOUNT
  D[DPS_N_WRDCOUNT] = phr_n;
#endif

  xy /= phr_n * 0xF;
  xy *= xy_w;
  xy /= DpsOriginWeightUltra(DPS_WORD_ORIGIN_COMMON);

  cc = 0;
#ifdef WITH_REL_WRDCOUNT
  median = (size_t)(((double)phr_n) / (Res->max_order_inquery + 1));
  sum = 0;
#endif
  for (tt = 0; tt <= Res->max_order_inquery; tt++) {
      if (count[tt]) cc += D[DPS_N_ADD + nsections + tt] / count[tt];
#ifdef WITH_REL_WRDCOUNT
      if (count[tt])
	  sum += ((count[tt] > median) ? (count[tt] - median) : (median - count[tt]));
      else sum += 2000;
#endif
  }
#ifdef WITH_REL_WRDCOUNT
  D[DPS_N_COUNT] = (dps_uint4)sum;
#ifdef WITH_REL_TRACK
  Track[j].D_wrdcount = phr_n;
  Track[j].D_n_count = D[DPS_N_COUNT];
#endif
#endif
  xy *= cc;
  xy /= ((Res->max_order_inquery + 1) * DpsOriginWeightUltra(DPS_WORD_ORIGIN_QUERY));
  
	
  Res->CoordList.ncoords = j + 1;
/*	fprintf(stderr, "** URL_ID: %d [phr_n:%d]\n", Crd[j].url_id, phr_n);*/
	
  cc = 1;
  for (tt = 1; tt < nsections; tt++) if (D[DPS_N_ADD + tt]) cc += wf[tt];
  xy *= cc;
  xy /= WFsections;
  

  Crd[j].coord = DpsCalcCosineWeightUltra(R, Rbc, xy, D, nsections, phr_n
#ifdef WITH_REL_TRACK
					 , &Track[j].y
#endif
					  );
#ifdef WITH_REL_TRACK
  Track[j].x = Rbc;
  Track[j].xy = xy;
  Res->CoordList.Track = (DPS_URLTRACK*)DpsRealloc(Res->CoordList.Track, Res->CoordList.ncoords * sizeof(*Res->CoordList.Track));
#endif

  DPS_FREE(D);
  DPS_FREE(R);
  DPS_FREE(count);
  TRACE_OUT(query);
  return;
}

void DpsGroupByURL(DPS_AGENT *query, DPS_RESULT *Res) {
  int rm = DpsVarListFindInt(&query->Vars, "rm", 
#ifdef FAST_RELEVANCE
			     1
#elif defined FULL_RELEVANCE
			     2
#else
			     3
#endif
			     );

  switch(rm) {
  case 1: DpsGroupByURLFast(query, Res); break;
  case 2: DpsGroupByURLFull(query, Res); break;
  case 3: DpsGroupByURLUltra(query, Res); break;
  default:
#ifdef FAST_RELEVANCE
    DpsGroupByURLFast(query, Res);
#elif defined FULL_RELEVANCE
    DpsGroupByURLFull(query, Res);
#else
    DpsGroupByURLUltra(query, Res);
#endif

  }
  return;
}

void DpsGroupBySite(DPS_AGENT *query, DPS_RESULT *Res){	
	size_t	i, j = 0, cnt = 1; 
	urlid_t Doc_site;
	DPS_URL_CRD_DB *Crd = Res->CoordList.Coords;
	DPS_URLDATA *Dat = Res->CoordList.Data;
#ifdef WITH_REL_TRACK
	DPS_URLTRACK *Trk = Res->CoordList.Track;
#endif
	int merge = 1;
#ifdef WITH_GOOGLEGRP
	int second = query->Flags.PagesInGroup;
#endif
	
	if(!Res->CoordList.ncoords) return;
	if (Res->PerSite == NULL) {
	  merge = 0;
	  if ((Res->PerSite = (size_t*)DpsMalloc(sizeof(*Res->PerSite) * Res->CoordList.ncoords + 1)) == NULL) return;
	}

	Doc_site = Dat[0].site_id;

	if (merge) {
	  cnt = Res->PerSite[0];
	  for(i = 1; i < Res->CoordList.ncoords; i++){
	    /* Group by site_id */
	    if(Doc_site == Dat[i].site_id){
	      /* Same site */
	      /* adjust document rating rating according nuber of documents from site, if we need this */
	      cnt += Res->PerSite[i];
#ifdef WITH_GOOGLEGRP
	      if (second > 0) {
		j++;
		Crd[j] = Crd[i];
		Dat[j] = Dat[i];
#ifdef WITH_REL_TRACK
		Trk[j] = Trk[i];
#endif
		second--;
	      }
#endif
	    }else{
	      /* Next site */
#ifdef WITH_GOOGLEGRP
	      for (; second < query->Flags.PagesInGroup; second++) {
		Res->PerSite[j + second - query->Flags.PagesInGroup] = cnt;
	      }
/*	      if (second == 0) {
		Res->PerSite[j - 1] = cnt;
		second = query->Flags.PagesInGroup;
	      }*/
#endif
	      Res->PerSite[j] = cnt;
	      cnt = Res->PerSite[i];
	      j++;
	      Doc_site = Dat[i].site_id;
	      Crd[j] = Crd[i];
	      Dat[j] = Dat[i];
#ifdef WITH_REL_TRACK
	      Trk[j] = Trk[i];
#endif
	    }
	  }
	} else {
	  for(i = 1; i < Res->CoordList.ncoords; i++){
		/* Group by site_id */
		if(Doc_site == Dat[i].site_id){
		/* Same site */
		  /* adjust document rating rating according nuber of documents from site, if we need this */
		  cnt++;
#ifdef WITH_GOOGLEGRP
		  if (second > 0) {
		    j++;
		    Crd[j] = Crd[i];
		    Dat[j] = Dat[i];
#ifdef WITH_REL_TRACK
		    Trk[j] = Trk[i];
#endif
		    second--;
		  }
#endif
		}else{
		  /* Next site */
#ifdef WITH_GOOGLEGRP
		  for (; second < query->Flags.PagesInGroup; second++) {
		    Res->PerSite[j + second - query->Flags.PagesInGroup] = cnt;
		  }
/*		  if (second == 0) {
		    Res->PerSite[j - 1] = cnt;
		    second = 1;
		  }*/
#endif
		  Res->PerSite[j] = cnt;
		  j++;
		  Doc_site = Dat[i].site_id;
		  Crd[j] = Crd[i];
		  Dat[j] = Dat[i];
#ifdef WITH_REL_TRACK
		  Trk[j] = Trk[i];
#endif
		  cnt = 1;
		}
	  }
	}
#ifdef WITH_GOOGLEGRP
	for (; second < query->Flags.PagesInGroup; second++) {
	  Res->PerSite[j + second - query->Flags.PagesInGroup] = cnt;
	}
/*	if (second == 0) {
	  Res->PerSite[j - 1] = cnt;
	}*/
#endif
	Res->PerSite[j] = cnt;
	Res->CoordList.ncoords = j + 1;

	return;
}



int DpsParseQueryString(DPS_AGENT * Agent,DPS_VARLIST * vars,char * query_string){
	char * tok, *lt;
	size_t len;
	char *str = (char *)DpsMalloc((len = dps_strlen(query_string)) + 7);
	char *qs = (char*)DpsStrdup(query_string);
	char qname[256];

	if ((str == NULL) || qs == NULL) {
	  DPS_FREE(str);
	  DPS_FREE(qs);
	  return 1;
	}

	Agent->nlimits = 0;
	DpsVarListDel(vars, "ul");

	DpsSGMLUnescape(qs);
	
	tok = dps_strtok_r(qs, "&", &lt, NULL);
	while(tok){
		char empty[]="";
		char * val;
		const char * lim;
		
		if((val=strchr(tok,'='))){
			*val='\0';
			val++;
		}else{
			val=empty;
		}
		DpsUnescapeCGIQuery(str,val);
		if (*val == '\0') {
		  DpsVarListDel(vars, tok);
		} else if (strcasecmp(tok, "DoExcerpt") == 0) {
		  int res = !strcasecmp(str, "yes");
		  Agent->Flags.do_excerpt = res;
		} else if (strcasecmp(tok, "EtcDir") == 0
			   || strcasecmp(tok, "VarDir") == 0) {
		  /* do nothing, don't allow to override those variables from a query */
		} else {
		  if (strncasecmp(tok, "ul", 2) == 0) DpsVarListAddStr(vars, tok, str);
		  else DpsVarListReplaceStr(vars, tok, str);
		  dps_snprintf(qname, 256, "query.%s", tok);
		  DpsVarListReplaceStr(vars, qname, str);

		  sprintf(str,"Limit-%s",tok);
		  if((lim = DpsVarListFindStr(vars, str, NULL))) {
			int ltype = 0;
			const char *fname = NULL;
			
			if(!strcasecmp(lim, "category")) {
			  ltype = DPS_LIMTYPE_NESTED; fname = DPS_LIMFNAME_CAT;
			} else if(!strcasecmp(lim, "tag")) {
			  ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_TAG;
			} else if(!strcasecmp(lim, "link")) {
			  ltype = DPS_LIMTYPE_LINEAR_INT; fname = DPS_LIMFNAME_LINK;
			} else if(!strcasecmp(lim, "since")) {
			  ltype = DPS_LIMTYPE_TIME; fname = str;
			} else if(!strcasecmp(lim, "time")) {
			  ltype = DPS_LIMTYPE_TIME; fname = DPS_LIMFNAME_TIME;
			} else if(!strcasecmp(lim, "hostname")) {
			  ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_HOST;
			} else if(!strcasecmp(lim, "language")) {
			  ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_LANG;
			} else if(!strcasecmp(lim, "content")) {
			  ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_CTYPE;
			} else if(!strcasecmp(lim, "siteid")) {
			  ltype = DPS_LIMTYPE_LINEAR_INT; fname = DPS_LIMFNAME_SITE;
			} else if (!strcasecmp(lim, "hex8str")) {
			  ltype = DPS_LIMTYPE_NESTED, fname = str;
			} else if (!strcasecmp(lim, "strcrc32")) {
			  ltype = DPS_LIMTYPE_LINEAR_CRC; fname = str;
			} else if (!strcasecmp(lim, "hour")) {
			  ltype = DPS_LIMTYPE_TIME; fname = str;
			} else if (!strcasecmp(lim, "hostname")) {
			  ltype = DPS_LIMTYPE_LINEAR_CRC; fname = str;
			} else if (!strcasecmp(lim, "char2")) {
			  ltype = DPS_LIMTYPE_LINEAR_CRC; fname = str;
			} else if (!strcasecmp(lim, "int")) {
			  ltype = DPS_LIMTYPE_LINEAR_INT; fname = str;
			}

			if((fname != NULL) && *val != '\0') {
			    DpsAddSearchLimit(Agent, &Agent->limits, &Agent->nlimits, ltype, fname, val);
			}
		  }
		}
		tok = dps_strtok_r(NULL, "&", &lt, NULL);
	}
	
	DPS_FREE(str);
	DPS_FREE(qs);
	return 0;
}


char * DpsHlConvert(DPS_WIDEWORDLIST *List, const char * src, DPS_CONV *lc_uni, DPS_CONV *uni_bc, int NOprefixHL) {
	dpsunicode_t	*tok, *lt, *uni;
	int             ctype, have_bukva_forte;
	char		*hpart, *htxt, *zend;
	size_t		len;
	
	if(!src)return NULL;
	
	if ((len = dps_strlen(src)) == 0) return NULL;
	hpart = (char*)DpsMalloc(len * 14 + 10);
	if (hpart == NULL) return NULL;
	zend = htxt = (char*)DpsMalloc(len * 14 + 10);
	if (htxt == NULL) {
	  DPS_FREE(hpart);
	  return NULL;
	}
	htxt[0]='\0';
	
	/* Convert to unicode */
	uni = (dpsunicode_t *)DpsMalloc((len + 10 + ((List != NULL) ? List->maxulen : 0)) * sizeof(dpsunicode_t));
	if (uni == NULL) {
	  DPS_FREE(hpart); DPS_FREE(htxt);
	  return NULL;
	}
	DpsConv(lc_uni, (char*)uni, sizeof(uni[0])*(len + 10 + ((List != NULL) ? List->maxulen : 0)), src, len+1);
/*	
	if (List != NULL) { 
	  size_t uw; 
	  for(uw = 0; uw < List->nwords; uw++) fprintf(stderr, "w%d:slen:%d|", uw, List->Word[uw].ulen); 
	} else {
	  fprintf(stderr, "List: NULL");
	}
	fprintf(stderr, "\n");
*/

	/* Parse unicode string */
	tok = DpsUniGetSepToken(uni, &lt, &ctype, &have_bukva_forte, 0, 0);
	while(tok){
		int found=0;
		size_t slen,flen;
		int euchar;
		size_t uw;

		flen=lt-tok;

		/* Convert token to BrowserCharset */
		euchar=tok[flen];
		tok[flen]=0;
		hpart[0]='\0';

		DpsConv(uni_bc, hpart, len * 14 + 10, (char*)tok, sizeof(*tok) * flen);
		
		/* Check that it is word to be marked */
		if (List != NULL) {
		  for (uw = 0; uw < List->nwords; uw++) {
		        if (List->Word[uw].origin & DPS_WORD_ORIGIN_STOP) continue;
			slen = List->Word[uw].ulen;
			if (slen > flen) continue;
			if (NOprefixHL && (DpsUniCType(tok[slen]) <= DPS_UNI_BUKVA) && (tok[slen] != 0) && (tok[slen] >= 0x30 )) continue;
			if (!DpsUniStrNCaseCmp(tok, List->Word[uw].uword, slen)) {
			  found = 1;
			  break;
			}
		  }
		}
		/*fprintf(stderr, "tok: %s| flen: %d, found:%d\n", hpart, flen, found);*/
		
		if (found) { *zend = '\2'; zend++; /*dps_strcat(htxt,"\2");*/ }
		/*dps_strcat(htxt,hpart);*/
		dps_strcpy(zend, hpart);
		zend += uni_bc->obytes;
		if (found) { *zend = '\3'; zend++; /*dps_strcat(htxt,"\3");*/ }
		tok[flen]=euchar;

		tok = DpsUniGetSepToken(NULL, &lt, &ctype, &have_bukva_forte, 0, 0);
	}
	*zend = '\0';
	DPS_FREE(hpart);
	DPS_FREE(uni);
	/*
	fprintf(stderr,"otxt='%s'\n",src);
	fprintf(stderr,"htxt='%s'\n",htxt);
	*/
	return(htxt);
}

int DpsConvert(DPS_ENV *Conf, DPS_VARLIST *Env_Vars, DPS_RESULT *Res, DPS_CHARSET *lcs, DPS_CHARSET *bcs){
	size_t		i, r, len;
	DPS_CONV	lc_bc, lc_bc_text, bc_bc;
	char            *newval, *newtxt;
	DPS_VAR         *Var;
	DPS_CHARSET	*sys_int;
	DPS_CONV	lc_uni, uni_bc;
	DPS_CONV	lc_uni_text, uni_bc_text;

	sys_int=DpsGetCharSet("sys-int");
	DpsConvInit(&lc_bc, lcs, bcs, Conf->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&lc_bc_text, lcs, bcs, Conf->CharsToEscape, DPS_RECODE_TEXT);
	DpsConvInit(&bc_bc, bcs, bcs, Conf->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&lc_uni, lcs, sys_int, Conf->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&uni_bc, sys_int, bcs, Conf->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&lc_uni_text, lcs, sys_int, Conf->CharsToEscape, DPS_RECODE_TEXT);
	DpsConvInit(&uni_bc_text, sys_int, bcs, Conf->CharsToEscape, DPS_RECODE_TEXT);

#ifdef HAVE_ASPELL
	if (Res->Suggest != NULL) {
/*	  DpsVarListAddStr(Env_Vars, "Suggest", Res->Suggest);*/

	  len = dps_strlen(Res->Suggest);
	  newval = (char*)DpsMalloc(len * 12 + 1);
	  if (newval == NULL) return DPS_ERROR;
	  DpsConv(&lc_bc, newval, len * 12 + 1, Res->Suggest, len + 1);
	  DPS_FREE(Res->Suggest);
	  Res->Suggest = newval;
	  
	}
#endif

	/* Convert word list */
	for(i=0;i<Res->WWList.nwords;i++) {
		DPS_WIDEWORD	*W=&Res->WWList.Word[i];

		len = dps_strlen(W->word);
		newval = (char*)DpsMalloc(len * 12 + 1);
		if (newval == NULL) return DPS_ERROR;
		DpsConv(&lc_bc, newval, len * 12 + 1, W->word, len + 1);
		DPS_FREE(W->word);
		W->word = newval;
	}
	
	/* Convert document sections */
	for(i=0;i<Res->num_rows;i++){
		DPS_DOCUMENT	*D=&Res->Doc[i];
		size_t		sec;
		int             NOprefixHL = 0;
		const char      *doclang = DpsVarListFindStr(&D->Sections, "Content-Language", "xx");
	
		if ((!Conf->Flags.make_prefixes) && strncasecmp(doclang, "zh", 2) && strncasecmp(doclang, "th", 2) 
		    && strncasecmp(doclang, "ja", 2) && strncasecmp(doclang, "ko", 2) ) NOprefixHL = 1;
		
		for (r = 0; r < 256; r++)
		for (sec = 0; sec < D->Sections.Root[r].nvars; sec++) {
			Var = &D->Sections.Root[r].Var[sec];

			newval = DpsHlConvert(&Res->WWList, Var->val, &lc_uni, &uni_bc, NOprefixHL);
			newtxt = DpsHlConvert(&Res->WWList, Var->txt_val, &lc_uni_text, &uni_bc_text, NOprefixHL);

			DPS_FREE(Var->val);
			DPS_FREE(Var->txt_val);
			Var->val = newval;
			Var->txt_val = newtxt;
		}
	}

	  /* Convert Env_Vars */
	for (r = 0; r < 256; r++)
	  for (i = 0; i < Env_Vars->Root[r].nvars; i++) {
	        Var = &Env_Vars->Root[r].Var[i];
	        len = dps_strlen(Var->val);
		newtxt = (char*)DpsMalloc(len * 12 + 1);
		newval = (char*)DpsMalloc(len * 12 + 1);
		if (newtxt == NULL || newval == NULL) { DPS_FREE(newtxt); DPS_FREE(newval); return DPS_ERROR; }

/*		if (db->DBDriver != DPS_DB_SEARCHD)*/  /* FIXME: need unification in charset from different DPS_DB */
		  DpsConv(&lc_bc, newval, len * 12 + 1, Var->val, len + 1);
/*		else 
		  DpsConv(&bc_bc, newval, len * 12 + 1, Var->val, len + 1);*/
		DpsConv(&lc_bc_text, newtxt, len * 12 + 1, Var->txt_val, len + 1);

		DPS_FREE(Var->val);
		DPS_FREE(Var->txt_val);
		Var->val = newval;
		Var->txt_val = newtxt;
	}

	return DPS_OK;
}

char* DpsRemoveHiLightDup(const char *s){
	size_t	len=dps_strlen(s)+1;
	char	*res = (char*)DpsMalloc(len);
	char	*d;

	if (res == NULL) return NULL;
	for(d=res;s[0];s++){
		switch(s[0]){
			case '\2':
			case '\3':
			case '\4':
				break;
			default:
				*d++=*s;
		}
	}
	*d='\0';
	return res;
}



int DpsCatToTextBuf(DPS_CATEGORY *C, char *textbuf, size_t len) {
	char	*end;
	size_t  i;
	
	textbuf[0]='\0';

/*	dps_snprintf(textbuf,len,"<CAT");
	end=textbuf+dps_strlen(textbuf);*/
	end = textbuf;
	
	for(i = 0; i < C->ncategories; i++) {
	  dps_snprintf(end, len - dps_strlen(textbuf), "<CAT\tid=\"%d\"\tpath=\"%s\"\tlink=\"%s\"\tname=\"%s\">\r\n",
		   C->Category[i].rec_id, C->Category[i].path, C->Category[i].link, C->Category[i].name );
	  end = end + dps_strlen(end);
	}
/*	dps_strcpy(end,">");*/
	return DPS_OK;
}

int DpsCatFromTextBuf(DPS_CATEGORY *C, char *textbuf) {
	const char	*htok, *last;
	DPS_HTMLTOK	tag;
	size_t		i, c;
	
	if (textbuf == NULL) return DPS_OK;
	DpsHTMLTOKInit(&tag);
	
	htok=DpsHTMLToken(textbuf,&last,&tag);
	
	if(!htok || tag.type != DPS_HTML_TAG)
	  return DPS_OK;

	C->Category = (DPS_CATITEM*)DpsRealloc(C->Category, sizeof(DPS_CATITEM) * ((c = C->ncategories) + 1));
	if (C->Category == NULL) {
	  C->ncategories = 0;
	  return DPS_ERROR;
	}
	bzero((void*)&C->Category[c], sizeof(DPS_CATITEM));
	
	for(i = 1; i < tag.ntoks; i++){
		size_t	nlen = tag.toks[i].nlen;
		size_t	vlen = tag.toks[i].vlen;
		char	*name = DpsStrndup(tag.toks[i].name, nlen);
		char	*data = DpsStrndup(tag.toks[i].val, vlen);

		if (!strcmp(name, "id")) {
		  C->Category[c].rec_id = atoi(data);
		} else if (!strcmp(name, "path")) {
		  dps_strncpy(C->Category[c].path, data, 128);
		} else if (!strcmp(name, "link")) {
		  dps_strncpy(C->Category[c].link, data, 128);
		} else if (!strcmp(name, "name")) {
		  dps_strncpy(C->Category[c].name, data, 128);
		}

		DPS_FREE(name);
		DPS_FREE(data);
	}

	C->ncategories++;
	return DPS_OK;
}


static size_t DpsUniSpaceCnt(dpsunicode_t *s) {
  size_t c = 0;
  while(*++s) {
    switch(*s) {
    case 0x0009:
    case 0x000A:
    case 0x000D:
    case 0x0020:
    case 0x00A0:
    case 0x1680:
    case 0x2000:
    case 0x2001:
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
    case 0x2007:
    case 0x2008:
    case 0x2009:
    case 0x200A:
    case 0x200B:
    case 0x202F:
    case 0x2420:
    case 0x3000:
    case 0x303F:
    case 0xFEFF: c++;
    default: break;
    }
  }
  return c;
}


dpsunicode_t *DpsUniSegment(DPS_AGENT *Indexer, dpsunicode_t *ustr, const char *lang) {
        DPS_DSTR        S;
	DPS_CHARSET     *tis_cs;
	DPS_CONV        uni_tis, tis_uni;
#if defined(CHASEN) || defined(MECAB)
	DPS_CHARSET     *eucjp_cs;
	DPS_CONV        uni_eucjp, eucjp_uni;
	char            *eucstr, *eucstr_seg;
	size_t          reslen;
#endif
	DPS_CHARSET	*sys_int;
	size_t          dstlen = DpsUniLen(ustr);
	size_t          bestparts = dstlen, curparts;
	dpsunicode_t    *ja_seg = NULL, *zh_seg = NULL, *ko_seg = NULL, *th_seg = NULL, *toseg;
	dpsunicode_t    *lt, *tok;
	int             have_bukva_forte;

	if (dstlen < 2) return DpsUniDup(ustr);

	sys_int = DpsGetCharSet("sys-int");

	tis_cs = DpsGetCharSet("tis-620");
	DpsConvInit(&tis_uni, tis_cs, sys_int, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&uni_tis, sys_int, tis_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
	
#if defined(CHASEN) || defined(MECAB)
	eucjp_cs = DpsGetCharSet("euc-jp");
	if (!eucjp_cs) eucjp_cs = DpsGetCharSet("sys-int");
	DpsConvInit(&uni_eucjp, sys_int, eucjp_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&eucjp_uni, eucjp_cs, sys_int, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
#endif

	DpsDSTRInit(&S, 4096);

	for(tok = DpsUniGetToken(ustr, &lt, &have_bukva_forte, 1); 
	    tok ; 
	    tok = DpsUniGetToken(NULL, &lt, &have_bukva_forte, 1) ) {

	  dstlen = lt - tok;
	  toseg = DpsUniNDup(tok, dstlen);

#if defined(CHASEN) || defined(MECAB)

	  if (lang == NULL || *lang == '\0' || !strncasecmp(lang, "ja", 2)) {
	    eucstr = (char*)DpsMalloc(14 * dstlen + 1);
	    if (eucstr == NULL) { DpsDSTRFree(&S); return NULL; }
	    DpsConv(&uni_eucjp, eucstr, 14 * dstlen + 1, (char*)toseg, (dstlen) * sizeof(dpsunicode_t));

#ifdef CHASEN
	    DPS_GETLOCK(Indexer, DPS_LOCK_SEGMENTER);
	    eucstr_seg = chasen_sparse_tostr(eucstr);
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_SEGMENTER);
#else
	    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
#if defined HAVE_PTHREAD && defined mecab_lock
	    mecab_lock(Indexer->Conf->mecab);
#endif
	    eucstr_seg = mecab_sparse_tostr(Indexer->Conf->mecab, eucstr);
#if defined HAVE_PTHREAD && defined mecab_unlock
	    mecab_unlock(Indexer->Conf->mecab);
#endif
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
#endif

	    reslen = dps_strlen(eucstr_seg) + 1;
	    ja_seg = (dpsunicode_t*)DpsMalloc(reslen * sizeof(dpsunicode_t));
	    if (ja_seg == NULL) {
	      DPS_FREE(eucstr);DpsDSTRFree(&S);
	      return NULL;
	    }
	    DpsConv(&eucjp_uni, (char*)ja_seg, reslen * sizeof(dpsunicode_t), eucstr_seg, reslen);
	    DPS_FREE(eucstr);
	  }
#endif
	  if (lang == NULL || *lang == '\0' || !strncasecmp(lang, "zh", 2)) {
	    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	    zh_seg = DpsSegmentByFreq(&Indexer->Conf->Chi, toseg);
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  }
	
	  if (lang == NULL || *lang == '\0' || !strncasecmp(lang, "th", 2)) {
	    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	    th_seg = DpsSegmentByFreq(&Indexer->Conf->Thai, toseg);
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  }

	  if (lang == NULL || *lang == '\0' || !strncasecmp(lang, "ko", 2)) {
	    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	    ko_seg = DpsSegmentByFreq(&Indexer->Conf->Korean, toseg);
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  }

	  if (ja_seg != NULL) {
	    curparts = DpsUniSpaceCnt(ja_seg);
	    if (curparts && (curparts < bestparts)) {
	      DPS_FREE(toseg); toseg = ja_seg; bestparts = curparts;
	    } else {
	      DPS_FREE(ja_seg);
	    }
	  }
	  if (zh_seg != NULL) {
	    curparts = DpsUniSpaceCnt(zh_seg);
	    if (curparts && (curparts < bestparts)) {
	      DPS_FREE(toseg); toseg = zh_seg; bestparts = curparts;
	    } else {
	      DPS_FREE(zh_seg);
	    }
	  }
	  if (ko_seg != NULL) {
	    curparts = DpsUniSpaceCnt(ko_seg);
	    if (curparts && (curparts < bestparts)) {
	      DPS_FREE(toseg); toseg = ko_seg; bestparts = curparts;
	    } else {
	      DPS_FREE(ko_seg);
	    }
	  }
	  if (th_seg != NULL) {
	    curparts = DpsUniSpaceCnt(th_seg);
	    if (curparts && (curparts < bestparts)) {
	      DPS_FREE(toseg); toseg = th_seg; bestparts = curparts;
	    } else {
	      DPS_FREE(th_seg);
	    }
	  }
	  DpsDSTRAppendUniWithSpace(&S, toseg);
	  DPS_FREE(toseg)
	}
	return (dpsunicode_t *)S.data;
}


char * DpsBuildPageURL(DPS_VARLIST * vars, char **dst) {
	size_t i, r, nargs = 0, dstlen = 1;
	char * end;
	for (r = 0; r < 256; r++)
	for(i = 0; i < vars->Root[r].nvars; i++) {
	  dstlen += 7 + dps_strlen(vars->Root[r].Var[i].name) + 3 * dps_strlen(vars->Root[r].Var[i].val);
	}
	*dst = (char*)DpsRealloc(*dst, dstlen);
	if (*dst == NULL) return NULL;
	end = *dst;

	for (r = 0; r < 256; r++)
	for (i = 0; i < vars->Root[r].nvars; i++) {
		dps_strcpy(end, nargs ? "&amp;" : "?");
		end += nargs ? 5 : 1;
		DpsEscapeURL(end, vars->Root[r].Var[i].name);
			
		end = end + dps_strlen(end);
		dps_strcpy(end, "="); end++;
		DpsEscapeURL(end, vars->Root[r].Var[i].val);
			
		end = end + dps_strlen(end);
		nargs++;
	}
	*end = '\0';
	return NULL;
}

void DpsParseQStringUnescaped(DPS_VARLIST *vars, const char *qstring){
	char *tok; 
	char *qs = (char*)DpsStrdup(qstring);
	char *name = NULL, *arg;

	if (qs != NULL) {
	  DpsVarListDel(vars, "ul");
	  DpsUnescapeCGIQuery(qs, qs);
	  name = qs;
	  tok = strchr(qs, '&');
	  while(tok){
	    if (tok[1] == '#') {
	      tok = strchr(tok + 1, '&');
	      continue;
	    }
	    arg = strchr(name, '=');
	    if(arg) *arg++ = '\0';
	    *tok = '\0';
	    if (strncasecmp(name, "ul", 2) == 0) DpsVarListAddStr(vars, name, arg ? arg : "");
	    else DpsVarListReplaceStr(vars, name, arg ? arg : "");
	    name = tok + 1;
	    tok = strchr(name, '&');
	  }
	  if (*name) {
	    arg = strchr(name, '=');
	    if(arg) *arg++ = '\0';
	    if (strncasecmp(name, "ul", 2) == 0) DpsVarListAddStr(vars, name, arg ? arg : "");
	    else DpsVarListReplaceStr(vars, name, arg ? arg : "");
	  }


	  DpsFree(qs);
	}
}

size_t DpsRemoveNullSections(DPS_URL_CRD *words, size_t n, int *wf) {
  size_t i, j = 0;
  for (i = 0; i < n; i++) {
    int s = DPS_WRDSEC(words[i].coord);
    if (s == 0 || wf[s] > 0) words[j++] = words[i]; /* zero section number means it's came from a limit */
  }
  return j;
}

size_t DpsRemoveNullSectionsDB(DPS_URL_CRD_DB *words, size_t n, int *wf, int secno) {
  size_t i, j = 0;
  int s;
  if (secno == 0) {
      for (i = 0; i < n; i++) {
	  if (wf[DPS_WRDSEC(words[i].coord)] > 0) words[j++] = words[i];
      }
  } else {
      for (i = 0; i < n; i++) {
	  s = DPS_WRDSEC(words[i].coord);
	  if ((s == secno) && (wf[s] > 0)) words[j++] = words[i];
      }
  }
  return j;
}
