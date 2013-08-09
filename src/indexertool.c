/* Copyright (C) 2004-2010 Datapark corp. All rights reserved.

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
#include "dps_indexertool.h"
#include "dps_proto.h"
#include "dps_db.h"
#include "dps_hash.h"
#include "dps_mutex.h"
#include "dps_vars.h"
#include "dps_doc.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>


int extended_stats = -1;

const char *dps_version = PACKAGE "-" VERSION "-" DPS_DBTYPE;

const char *DpsVersion(void) {
  return dps_version;
}

int DpsShowStatistics(DPS_AGENT *Indexer) {
     int       res;
     DPS_STATLIST   Stats;
     size_t         snum;
     DPS_STAT  Total;
     
     bzero((void*)&Total, sizeof(Total));
     res=DpsStatAction(Indexer,&Stats);
     printf("\n          Database statistics\n\n");
     if (extended_stats) printf("%8s %13s %27s\n","Status", "Expired", "Total");
     else printf("%6s %10s %10s\n","Status","Expired","Total");
     if (extended_stats) printf("%6s %17s %28s\n", " ", "count | size", " count | size");
     if (extended_stats) printf("   -----------------------------------------------------------------------------------\n");
     else printf("   -----------------------------\n");
     for(snum=0;snum<Stats.nstats;snum++){
          DPS_STAT  *S=&Stats.Stat[snum];
	  if (extended_stats) 
	    printf("%6d %10d | %14llu  %10d | %14llu %s\n", 
		   S->status, S->expired, (unsigned long long)S->expired_size, S->total, 
		   (unsigned long long)S->total_size, DpsHTTPErrMsg(S->status));
          else printf("%6d %10d %10d %s\n", S->status, S->expired, S->total, DpsHTTPErrMsg(S->status));
          Total.expired += S->expired;
          Total.total += S->total;
          Total.total_size += S->total_size;
          Total.expired_size += S->expired_size;
     }
     if (extended_stats) printf("   -----------------------------------------------------------------------------------\n");
     else printf("   -----------------------------\n");
     if (extended_stats) 
       printf("%6s %10d | %14llu  %10d | %14llu\n", "Total", Total.expired, (unsigned long long)Total.expired_size, 
	      Total.total, (unsigned long long)Total.total_size);
     else printf("%6s %10d %10d\n", "Total", Total.expired, Total.total);
     printf("\n");
     DPS_FREE(Stats.Stat);
     return(res);
}



void DpsAppendTarget(DPS_AGENT *Indexer, const char *url, const char *lang, const int hops, int parent) {
  DPS_DOCUMENT *Doc, *Save;
  size_t i;

  TRACE_IN(Indexer, "AppendTarget");

  DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
  if (Indexer->Conf->Targets.num_rows > 0) {
    for (i = Indexer->Conf->Targets.num_rows - 1; i > 0; i--) {
      Doc = &Indexer->Conf->Targets.Doc[i];
      if ((strcasecmp(DpsVarListFindStr(&Doc->Sections, "URL", ""), url) == 0) 
	  && (strcmp(DpsVarListFindStr(&Doc->RequestHeaders, "Accept-Language", ""), lang) == 0)) {
	DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
	TRACE_OUT(Indexer);
	return;
      }
    }
  }
  if ((Indexer->Conf->Targets.Doc = 
       DpsRealloc(Save = Indexer->Conf->Targets.Doc, (Indexer->Conf->Targets.num_rows + 1) * sizeof(DPS_DOCUMENT))) == NULL) {
    Indexer->Conf->Targets.Doc = Save;
    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
    DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
    TRACE_OUT(Indexer);
    return;
  }
  Doc = &Indexer->Conf->Targets.Doc[Indexer->Conf->Targets.num_rows];
  DpsDocInit(Doc);
  DpsVarListAddStr(&Doc->Sections, "URL", url);
  DpsVarListAddInt(&Doc->Sections, "Hops", hops);
  DpsVarListDel(&Doc->Sections, "URL_ID");
  DpsVarListReplaceInt(&Doc->Sections, "Referrer-ID", parent);
  if (*lang != '\0') DpsVarListAddStr(&Doc->RequestHeaders, "Accept-Language", lang);
  if (DPS_OK == DpsURLAction(Indexer, Doc, DPS_URL_ACTION_FINDBYURL)) {
    urlid_t url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
    if (url_id != 0) Indexer->Conf->Targets.num_rows++;
    else DpsDocFree(Doc);
  }
/*  fprintf(stderr, "-- AppandTarget: url:%s  URL_ID:%d\n", url, DpsStrHash32(url));*/
  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
  DpsURLAction(Indexer, Doc, DPS_URL_ACTION_ADD);
  DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
  TRACE_OUT(Indexer);
  return;
}

