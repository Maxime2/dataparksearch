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
#include "dps_utils.h"
#include "dps_vars.h"
#include "dps_hash.h"
#include "dps_doc.h"
#include "dps_result.h"
#include "dps_word.h"
#include "dps_searchtool.h"
#include "dps_searchcache.h"
#include "dps_parsehtml.h"
#include "dps_log.h"
#include "dps_db_int.h"
#include "dps_boolean.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <time.h>

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif

/***************** Results cache functions **************/

/* Compose search cache file name   */
/* taking in account all parameters */
/* and query words themself         */

/*
#define DEBUG_CACHE
*/

static void cache_file_name(char *dst,size_t len, DPS_VARLIST *Conf_Vars, DPS_RESULT *Res) {
	char param[4*1024];
	const char *vardir = DpsVarListFindStr(Conf_Vars, "VarDir", DPS_VAR_DIR);
	const char *appname = DpsVarListFindStr(Conf_Vars, "appname", NULL);
	int bytes;
	
	bytes = dps_snprintf(param, sizeof(param)-1, "%s.%s.%d.%s.%s.%s.%s.%s.%s.%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s-%s-%s-%s-%d",
			     DpsVarListFindStr(Conf_Vars, "m", ""),
			     DpsVarListFindStr(Conf_Vars, "wm", ""),
			     DpsVarListFindInt(Conf_Vars, "o", 0),
			     DpsVarListFindStr(Conf_Vars, "t", ""),
			     DpsVarListFindStr(Conf_Vars, "cat", DpsVarListFindStr(Conf_Vars, "c", "")),
			     DpsVarListFindStr(Conf_Vars, "ul", ""),
			     DpsVarListFindStr(Conf_Vars, "wf", ""),
			     DpsVarListFindStr(Conf_Vars, "g", ""),
			     DpsVarListFindStr(Conf_Vars, "GroupBySite", "no"),
			     DpsVarListFindStr(Conf_Vars, "site", "0"),
			     DpsVarListFindStr(Conf_Vars, "type", ""),
			     DpsVarListFindStr(Conf_Vars, "sp", ""),
			     DpsVarListFindStr(Conf_Vars, "sy", ""),
			     DpsVarListFindStr(Conf_Vars, "dt", ""),
			     DpsVarListFindStr(Conf_Vars, "dp", ""),
			     DpsVarListFindStr(Conf_Vars, "dx", ""),
			     DpsVarListFindStr(Conf_Vars, "dm", ""),
			     DpsVarListFindStr(Conf_Vars, "dy", ""),
			     DpsVarListFindStr(Conf_Vars, "db", ""),
			     DpsVarListFindStr(Conf_Vars, "de", ""),
			     DpsVarListFindStr(Conf_Vars, "s", ""),
			     DpsVarListFindStr(Conf_Vars, "rm", 
#ifdef FAST_RELEVANCE
					       "1"
#elif defined FULL_RELEVANCE
					       "2"
#else
					       "3"
#endif
					       ),
			     DpsVarListFindStr(Conf_Vars, "link", ""),
			     Res->orig_nitems
			     );

#ifdef DEBUG_CACHE
	fprintf(stderr, "param: |%s|\n", param);
#endif
	
	dps_snprintf(dst, len, "%s%scache%s%s%s%d.%s%08X.%08X",
		     vardir, DPSSLASHSTR, DPSSLASHSTR, 
		     appname?appname:"", appname?"-":"",
		     DpsVarListFindInt(Conf_Vars, "Listen", 0),
		     DpsVarListFindStr(Conf_Vars, "label", ""),
		     DpsStrHash32(param),
		     DpsStrHash32(DpsVarListFindStr(Conf_Vars, "q", ""))
		     );
}


int DpsSearchCacheClean(DPS_AGENT *query) {
	char param[PATH_MAX];
	char filen[PATH_MAX];
	const char *vardir = DpsVarListFindStr(&query->Conf->Vars, "VarDir", DPS_VAR_DIR);
	const char *appname = DpsVarListFindStr(&query->Conf->Vars, "appname", NULL);
	DIR * dir;
	struct dirent * item;
	size_t prefix_len;

	dps_snprintf(param, sizeof(param), "%s%scache%s", vardir, DPSSLASHSTR, DPSSLASHSTR);
	dps_snprintf(filen, sizeof(filen), "%s%s%d.",
		     appname?appname:"", appname?"-":"",
		     DpsVarListFindInt(&query->Conf->Vars, "Listen", 0));
	prefix_len = dps_strlen(filen);
	dir = opendir(param);
	if (!dir) return DPS_ERROR;
	while((item = readdir(dir))){
#if defined(_DIRENT_HAVE_D_TYPE)
	  if (item->d_type != DT_REG) continue;
#endif
	  if (strncasecmp(item->d_name, filen, prefix_len) != 0) continue;
	  dps_snprintf(filen, sizeof(filen), "%s%s", param, item->d_name);
	  unlink(filen);
	}
	closedir(dir);

	return DPS_OK;

}


int __DPSCALL DpsSearchCacheStore(DPS_AGENT * query, DPS_RESULT *Res){
	int	fd;
	char	fname[PATH_MAX];
	size_t	i;
	
	
	cache_file_name(fname,sizeof(fname), &query->Vars, Res);
		
#ifdef DEBUG_CACHE
	fprintf(stderr,"write to %s\n",fname);
#endif
	if((fd = DpsOpen3(fname, O_WRONLY | O_CREAT | O_TRUNC | DPS_BINARY, DPS_IWRITE)) >= 0) {
#ifdef DEBUG_CACHE
	  fprintf(stderr, "found:%d, grand_total:%d ncoords:%d\n", Res->total_found, Res->grand_total, Res->CoordList.ncoords );	
#endif
	  (void)write(fd, &Res->total_found, sizeof(Res->total_found));
	  (void)write(fd, &Res->grand_total, sizeof(Res->grand_total));

			
	  (void)write(fd, &(Res->WWList),sizeof(DPS_WIDEWORDLIST)); 
	  for (i = 0; i< Res->WWList.nwords; i++) {
	    (void)write(fd, &(Res->WWList.Word[i]), sizeof(DPS_WIDEWORD));
/*	    (void)write(fd, Res->WWList.Word[i].word, Res->WWList.Word[i].len);
	    (void)write(fd, Res->WWList.Word[i].uword, sizeof(dpsunicode_t) * Res->WWList.Word[i].ulen);*/
	  }
			
	  (void)write(fd, Res->CoordList.Coords, Res->CoordList.ncoords * sizeof(DPS_URL_CRD_DB));
	  (void)write(fd, Res->CoordList.Data, Res->CoordList.ncoords * sizeof(DPS_URLDATA));
#ifdef WITH_REL_TRACK
	  (void)write(fd, Res->CoordList.Track, Res->CoordList.ncoords * sizeof(DPS_URLTRACK));
#endif
	  if (Res->PerSite) {
	    (void)write(fd, &Res->total_found, sizeof(Res->total_found));
	    (void)write(fd, Res->PerSite, Res->CoordList.ncoords * sizeof(size_t));
	  } else {
	    size_t topcount = 0;
	    (void)write(fd, &topcount, sizeof(topcount));
	  }
			
	  DpsClose(fd);
	}else{
#ifdef DEBUG_CACHE
	  dps_strerror(NULL, 0, "debug");
#endif
	}
	return DPS_OK;
}

int __DPSCALL DpsSearchCacheFind(DPS_AGENT * Agent, DPS_RESULT *Res) {
	char fname[PATH_MAX];
	struct stat sb;
	int fd;
	DPS_URL_CRD_DB *wrd = NULL;
	DPS_URLDATA *dat = NULL;
#ifdef WITH_REL_TRACK
	DPS_URLTRACK *trk = NULL;
#endif
	ssize_t bytes;
	DPS_WIDEWORDLIST wwl;
	DPS_WIDEWORD ww;
	size_t i;
	size_t topcount;
	
#ifdef DEBUG_CACHE
	fprintf(stderr, "DpsSearchCacheFind: Start\n");
#endif
	
	Res->offset = 1;
	DpsPrepare(Agent, Res);	/* Prepare query    */


	cache_file_name(fname, sizeof(fname), &Agent->Vars, Res);
	if((fd = DpsOpen2(fname,O_RDONLY|DPS_BINARY)) < 0) {
#ifdef DEBUG_CACHE
	  dps_strerror(NULL, 0, " %s open error", fname);
#endif
	  return DPS_ERROR;
	}

	if (fstat(fd, &sb)) {
#ifdef DEBUG_CACHE
	  dps_strerror(NULL, 0, " %s [fd:%d] fstat", fname, fd);
#endif
	  DpsClose(fd);
	  return DPS_ERROR;
	}
	if (sb.st_size < (off_t)(sizeof(Res->total_found) + sizeof(DPS_WIDEWORDLIST))) {
	  DpsClose(fd);
	  unlink(fname);
	  return DPS_ERROR;
	}
	if ((Agent->Flags.hold_cache > 0) && (sb.st_mtime + Agent->Flags.hold_cache < time(NULL))) {
	  DpsClose(fd);
	  unlink(fname);
	  return DPS_ERROR;
	}


	for (i = 0; i < Res->nitems; i++) {
	  if (Res->items[i].cmd != DPS_STACK_WORD) continue;
	  ww.order = Res->items[i].order;
	  ww.order_inquery = Res->items[i].order_inquery;
	  ww.count = Res->items[i].count;
	  ww.crcword = Res->items[i].crcword;
	  ww.word = Res->items[i].word;
	  ww.uword = Res->items[i].uword;
	  ww.origin = Res->items[i].origin;
	  DpsWideWordListAdd(&Res->WWList, &ww, DPS_WWL_LOOSE);
	}

	
	if( (-1 == read(fd,&Res->total_found, sizeof(Res->total_found))) ){
		DpsClose(fd);
		return DPS_ERROR;
	}
	
#ifdef DEBUG_CACHE
	fprintf(stderr, " found: %d\n", Res->total_found);
#endif

	if( (-1 == read(fd,&Res->grand_total, sizeof(Res->grand_total))) ){
		DpsClose(fd);
		return DPS_ERROR;
	}
	
#ifdef DEBUG_CACHE
	fprintf(stderr, " grand_total: %d\n", Res->grand_total);
#endif

	if (-1==read(fd, &wwl, sizeof(DPS_WIDEWORDLIST))) {
		DpsClose(fd);
		return DPS_ERROR;
	}
#ifdef DEBUG_CACHE
	fprintf(stderr, " nwords: %d\n", wwl.nwords);
#endif

	for(i = 0; i < wwl.nwords; i++) {
	  if (-1 == read(fd, &ww, sizeof(DPS_WIDEWORD))) {
	    DpsClose(fd);
	    return DPS_ERROR;
	  }
	  if (i < Res->WWList.nwords) {
	    Res->WWList.Word[i].count = ww.count;
	  }
	}

/*
	DpsWideWordListFree(&Res->WWList);
	Res->WWList = wwl;
	Res->WWList.Word = (DPS_WIDEWORD*)DpsMalloc(wwl.nwords * sizeof(DPS_WIDEWORD));
	if (Res->WWList.Word == NULL) {
		DpsClose(fd);
		return DPS_ERROR;
	}

	for(i = 0; i < wwl.nwords; i++) {
	  if (-1==read(fd, &Res->WWList.Word[i], sizeof(DPS_WIDEWORD))) {
	    DpsClose(fd);
	    return DPS_ERROR;
	  }
	  Res->WWList.Word[i].word = (char*)DpsMalloc(Res->WWList.Word[i].len + 1);
	  if (Res->WWList.Word[i].word == NULL) {
	    DpsClose(fd);
	    return DPS_ERROR;
	  }
	  bzero((void*)Res->WWList.Word[i].word, Res->WWList.Word[i].len + 1);
	  Res->WWList.Word[i].uword = (dpsunicode_t *)DpsMalloc(sizeof(dpsunicode_t) * Res->WWList.Word[i].ulen + 1);
	  if (Res->WWList.Word[i].uword == NULL) {
	    DpsClose(fd);
	    return DPS_ERROR;
	  }
	  bzero((void*)Res->WWList.Word[i].uword, sizeof(dpsunicode_t) * Res->WWList.Word[i].ulen + 1);
	  if (-1==read(fd, Res->WWList.Word[i].word, Res->WWList.Word[i].len)) {
	    DpsClose(fd);
	    return DPS_ERROR;
	  }
	  if (-1==read(fd, Res->WWList.Word[i].uword, sizeof(dpsunicode_t) * Res->WWList.Word[i].ulen)) {
	    DpsClose(fd);
	    return DPS_ERROR;
	  }
	}
*/

	
	wrd = (DPS_URL_CRD_DB*)DpsMalloc((Res->total_found + 1) * sizeof(*wrd));
	dat = (DPS_URLDATA*)DpsMalloc((Res->total_found + 1) * sizeof(*dat));
#ifdef WITH_REL_TRACK
	trk = (DPS_URLTRACK*)DpsMalloc((Res->total_found + 1) * sizeof(*trk));
	if (trk == NULL) {
	  DPS_FREE(wrd); DPS_FREE(dat);
	  DpsClose(fd);
	  return DPS_ERROR;
	}
#endif
	if (wrd == NULL || dat == NULL) {
	  DPS_FREE(wrd);
	  DpsClose(fd);
	  return DPS_ERROR;
	}
#if 0	
	if(-1==lseek(fd,(off_t)0/*(page_number*page_size*sizeof(*wrd))*/,SEEK_CUR)){
		DpsClose(fd);
		return DPS_ERROR;
	}
#endif
	if(-1==(bytes=read(fd, wrd, Res->total_found/* page_size*/ * sizeof(*wrd) ))){
		DpsClose(fd);
		return DPS_ERROR;
	}
	Res->CoordList.ncoords = (size_t)bytes / sizeof(*wrd);
	if(-1==(bytes=read(fd, dat, Res->total_found/* page_size*/ * sizeof(*dat) ))){
		DpsClose(fd);
		return DPS_ERROR;
	}
#ifdef WITH_REL_TRACK
	if(-1==(bytes=read(fd, trk, Res->total_found/* page_size*/ * sizeof(*trk) ))){
		DpsClose(fd);
		return DPS_ERROR;
	}
#endif

	if( (-1 == read(fd, &topcount, sizeof(topcount))) ){
		DpsClose(fd);
		return DPS_ERROR;
	}
	if (topcount) {
	  Res->PerSite = (size_t*)DpsMalloc((Res->total_found + 1) * sizeof(size_t));
	  if (Res->PerSite == NULL) {
	    DPS_FREE(wrd); DPS_FREE(dat);
	    DpsClose(fd);
	    return DPS_ERROR;
	  }
	  if(-1 == (bytes=read(fd, Res->PerSite, topcount * sizeof(size_t) ))){
		DpsClose(fd);
		return DPS_ERROR;
	  }
	}

	DpsClose(fd);
	DPS_FREE(Res->CoordList.Coords);
	Res->CoordList.Coords = wrd;
	Res->CoordList.Data = dat;
#ifdef WITH_REL_TRACK
	Res->CoordList.Track = trk;
#endif
	
	Res->num_rows = Res->total_found = Res->CoordList.ncoords; /* ??? was commented, why ? */

/*	Res->prepared = 1;*/

#ifdef DEBUG_CACHE
	fprintf(stderr, "DpsSearchCacheFind: Done\n");
#endif

	return DPS_OK;
}

