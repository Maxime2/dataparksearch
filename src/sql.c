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
#include "dps_spell.h"
#include "dps_robots.h"
#include "dps_mutex.h"
#include "dps_db.h"
#include "dps_unicode.h"
#include "dps_url.h"
#include "dps_log.h"
#include "dps_proto.h"
#include "dps_conf.h"
#include "dps_hash.h"
#include "dps_xmalloc.h"
#include "dps_cache.h"
#include "dps_boolean.h"
#include "dps_searchtool.h"
#include "dps_searchcache.h"
#include "dps_server.h"
#include "dps_stopwords.h"
#include "dps_doc.h"
#include "dps_result.h"
#include "dps_vars.h"
#include "dps_agent.h"
#include "dps_store.h"
#include "dps_hrefs.h"
#include "dps_word.h"
#include "dps_db_int.h"
#include "dps_sqldbms.h"
#include "dps_match.h"
#include "dps_indexer.h"
#include "dps_textlist.h"
#include "dps_charsetutils.h"
#include "dps_template.h"

#ifdef HAVE_SQL

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#if defined(WITH_IDN) && !defined(APACHE1) && !defined(APACHE2)
#include <idna.h>
#elif defined(WITH_IDNKIT) && !defined(APACHE1) && !defined(APACHE2)
#include <idn/api.h>
#endif

/*
#define NEO_USE_CLONES 1
*/
/*
#define DEBUG
*/


static int DpsPopRankPasNeo(DPS_AGENT *A, DPS_DB *db, const char *rec_id, const char *hops_str, int skip_same_site, size_t url_num, int need_count,
			    int detect_clones);


#if HAVE_ORACLE8
static void param_add(DPS_DB *db, int pos_out_1, int pos_out_2, int pos_out_3, int pos_out_4){
        db->par->out_pos_val[0][db->par->out_rec] = pos_out_1;
        db->par->out_pos_val[1][db->par->out_rec] = pos_out_2;
        db->par->out_pos_val[2][db->par->out_rec] = pos_out_3;
        db->par->out_pos_val[3][db->par->out_rec] = pos_out_4;
        db->par->out_rec++;
}

static void param_init(DPS_DB *db, int pos1, int pos2, int pos3, int pos4){
        db->par->out_pos[0]=pos1;
        db->par->out_pos[1]=pos2;
        db->par->out_pos[2]=pos3;
        db->par->out_pos[3]=pos4;
}

#endif



static const char *BuildWhere(DPS_AGENT *Agent, DPS_DB *db) {
	size_t	i;
	char	*urlstr;
	char	*tagstr;
	char	*statusstr;
	char	*catstr;
	char	*langstr;
	char    *typestr;
	char    *fromstr;
	char    *serverstr;
	char    *sitestr;
	char    *timestr;
	char    *hopstr;
	int     fromserver = 1, fromurlinfo_lang = 1, fromurlinfo_type = 1, fromurlinfo_tag = 1, fromurlinfo_cat = 1;
	int     dt = DPS_DT_UNKNOWN, dx = 1, dm = 0, dy = 1970, dd = 1;
	time_t  dp = 0, DB = 0, DE = time(NULL);
	struct tm tm;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	unsigned int r;
	
	if(db->where)return(db->where);
	
	bzero((void*)&tm, sizeof(struct tm));
	urlstr = (char*)DpsStrdup("");
	tagstr = (char*)DpsStrdup("");
	statusstr = (char*)DpsStrdup("");
	catstr = (char*)DpsStrdup("");
	langstr = (char*)DpsStrdup("");
	typestr = (char*)DpsStrdup("");
	fromstr = (char*)DpsStrdup("");
	serverstr = (char*)DpsStrdup("");
	sitestr = (char*)DpsStrdup("");
	timestr = (char*)DpsStrdup("");
	hopstr = (char*)DpsStrdup("");
	
	r = (unsigned int) 'c';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		
		if(!val[0])continue;

		if(!strcmp(var,"cat") || !strcmp(var,"c")){
			catstr=(char*)DpsRealloc(catstr,dps_strlen(catstr)+dps_strlen(val)+50);
			if (catstr == NULL) continue;
			if(catstr[0])dps_strcpy(DPS_STREND(catstr)-1," OR ");
			else	dps_strcat(catstr,"(");
			sprintf(DPS_STREND(catstr),"c.path LIKE '%s%%')",val);

			if (Agent->Flags.URLInfoSQL) {
			  if (fromurlinfo_cat) {
			    fromurlinfo_cat = 0;
			    fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 32);
			    if (fromstr == NULL) continue;
			    sprintf(DPS_STREND(fromstr), ", urlinfo ic, categories c");
			    serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 256);
			    if (serverstr == NULL) continue;
			    sprintf(DPS_STREND(serverstr), "%surl.rec_id=ic.url_id AND ic.sname='category' AND c.rec_id=CAST(ic.sval AS %s)",
				    (serverstr[0]) ? " AND " : "", (db->DBType == DPS_DB_MYSQL) ? "SIGNED" : "INTEGER");
			  }
			} else {
			  if (fromserver) {
			    fromserver = 0;
			    fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 32);
			    if (fromstr == NULL) continue;
			    sprintf(DPS_STREND(fromstr), ", server s, categories c");
			    serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 64);
			    if (serverstr == NULL) continue;
			    sprintf(DPS_STREND(serverstr), "%ss.rec_id=url.server_id AND s.category=c.rec_id", (serverstr[0])?" AND ":"");
			  }
			}
		}
	}

	r = (unsigned int) 'd';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		int intval = atoi(val);
		
		if(!val[0])continue;

		if (!strcmp(var, "dt")) {
		  if(!strcasecmp(val, "back")) dt = DPS_DT_BACK;
		  else if (!strcasecmp(val, "er")) dt = DPS_DT_ER;
		  else if (!strcasecmp(val, "range")) dt = DPS_DT_RANGE;
		}else
		if (!strcmp(var, "dx")) {
		  if (intval == 1 || intval == -1) dx = intval;
		  else dx = 1;
		}else
		if (!strcmp(var, "dm")) {
		  dm = (intval) ? intval : 1;
		}else
		if (!strcmp(var, "dy")) {
		  dy = (intval) ? intval : 1970;
		}else
		if (!strcmp(var, "dd")) {
		  dd = (intval) ? intval : 1;
		}else
		if (!strcmp(var, "dp")) {
		  dp = Dps_dp2time_t(val);
		}else
		if (!strcmp(var, "db")) {
		  sscanf(val, "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year);
		  tm.tm_year -= 1900; tm.tm_mon--;
		  DB = mktime(&tm);
		}else
		if (!strcmp(var, "de")) {
		  sscanf(val, "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year);
		  tm.tm_year -= 1900; tm.tm_mon--;
		  DE = mktime(&tm);
		}
	}

	r = (unsigned int) 'g';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		
		if(!val[0])continue;

		if(!strcmp(var,"g")) { /* the same as for 'lang' */
			langstr=(char*)DpsRealloc(langstr,dps_strlen(langstr)+dps_strlen(val)+50);
			if (langstr == NULL) continue;
			if(langstr[0])dps_strcpy(DPS_STREND(langstr)-1," OR ");
			else	dps_strcat(langstr,"(");
			if (strchr(val, '%') || strchr(val, '_'))
			  sprintf(DPS_STREND(langstr),"il.sval LIKE '%s')", val);
			else sprintf(DPS_STREND(langstr),"il.sval='%s')", val);
			if (fromurlinfo_lang) {
			  fromurlinfo_lang = 0;
			  fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 16);
			  if (fromstr == NULL) continue;
			  sprintf(DPS_STREND(fromstr), ", urlinfo il");
			  serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 64);
			  if (serverstr == NULL) continue;
			  sprintf(DPS_STREND(serverstr), "%sil.url_id=url.rec_id AND il.sname='content-language'",(serverstr[0]) ? " AND " : "");
			}
		}
		
	}

	r = (unsigned int) 'l';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		
		if(!val[0])continue;

		if(!strcmp(var, "lang")) { /* the same as for 'g' */
			langstr=(char*)DpsRealloc(langstr,dps_strlen(langstr)+dps_strlen(val)+50);
			if (langstr == NULL) continue;
			if(langstr[0])dps_strcpy(DPS_STREND(langstr)-1," OR ");
			else	dps_strcat(langstr,"(");
			if (strchr(val, '%') || strchr(val, '_'))
			  sprintf(DPS_STREND(langstr),"il.sval LIKE '%s')",val);
			else sprintf(DPS_STREND(langstr),"il.sval='%s')", val);
			if (fromurlinfo_lang) {
			  fromurlinfo_lang = 0;
			  fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 16);
			  if (fromstr == NULL) continue;
			  sprintf(DPS_STREND(fromstr), ", urlinfo il");
			  serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 64);
			  if (serverstr == NULL) continue;
			  sprintf(DPS_STREND(serverstr), "%sil.url_id=url.rec_id AND il.sname='content-language'",(serverstr[0])?" AND ":"");
			}
		}
		
	}

	r = (unsigned int) 's';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		int intval = atoi(val);
		
		if(!val[0])continue;

		if(!strcmp(var,"status")){
			statusstr=(char*)DpsRealloc(statusstr,dps_strlen(statusstr)+dps_strlen(val)+50);
			if (statusstr == NULL) continue;
			
			if(db->DBSQL_IN){
				if(statusstr[0])sprintf(DPS_STREND(statusstr)-1,",%d)",intval);
				else	sprintf(statusstr," url.status IN (%d)",intval);
			}else{
				if(statusstr[0])dps_strcpy(DPS_STREND(statusstr)-1," OR ");
				else	dps_strcat(statusstr,"(");
				sprintf(DPS_STREND(statusstr),"url.status=%d)",intval);
			}
		}else
		if(!strcmp(var,"site") && intval != 0) {
			sitestr=(char*)DpsRealloc(sitestr, dps_strlen(sitestr) + dps_strlen(val) + 50);
			if (sitestr == NULL) continue;
			
			if(db->DBSQL_IN){
				if (sitestr[0]) sprintf(DPS_STREND(sitestr) - 1, ",%s%i%s)", qu, intval, qu);
				else	sprintf(sitestr, " url.site_id IN (%s%i%s)", qu, intval, qu);
			}else{
				if (sitestr[0]) dps_strcpy(DPS_STREND(sitestr) - 1, " OR ");
				else	dps_strcat(sitestr, "(");
				sprintf(DPS_STREND(sitestr), "url.site_id=%s%d%s)", qu, intval, qu);
			}
		}
	}

	r = (unsigned int) 't';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		
		if(!val[0])continue;

		if(!strcmp(var,"tag") || !strcmp(var,"t")){
			tagstr=(char*)DpsRealloc(tagstr,dps_strlen(tagstr)+dps_strlen(val)+50);
			if (tagstr == NULL) continue;
			if(tagstr[0])dps_strcpy(DPS_STREND(tagstr)-1," OR ");
			else	dps_strcat(tagstr,"(");

			if (Agent->Flags.URLInfoSQL) {
			  if (strchr(val, '%') || strchr(val, '_')) 
			    sprintf(DPS_STREND(tagstr),"ia.sval LIKE '%s')",val);
			  else sprintf(DPS_STREND(tagstr),"ia.sval='%s')",val);

			  if (fromurlinfo_tag) {
			    fromurlinfo_tag = 0;
			    fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 32);
			    if (fromstr == NULL) continue;
			    sprintf(DPS_STREND(fromstr), ", urlinfo ia");
			    serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 64);
			    if (serverstr == NULL) continue;
			    sprintf(DPS_STREND(serverstr), "%surl.rec_id=ia.url_id AND ia.sname='tag'",(serverstr[0])?" AND ":"");
			  }
			} else {
			  if (strchr(val, '%') || strchr(val, '_')) 
			    sprintf(DPS_STREND(tagstr),"s.tag LIKE '%s')",val);
			  else sprintf(DPS_STREND(tagstr),"s.tag='%s')",val);

			  if (fromserver) {
			    fromserver = 0;
			    fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 12);
			    if (fromstr == NULL) continue;
			    sprintf(DPS_STREND(fromstr), ", server s");
			    serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 64);
			    if (serverstr == NULL) continue;
			    sprintf(DPS_STREND(serverstr), "%ss.rec_id=url.server_id",(serverstr[0])?" AND ":"");
			  }
			}
		} else
		if(!strcmp(var,"type")){
			typestr = (char*)DpsRealloc(typestr, dps_strlen(typestr) + dps_strlen(val) + 50);
			if (typestr == NULL) continue;
			if(typestr[0]) dps_strcpy(DPS_STREND(typestr) - 1, " OR ");
			else	dps_strcat(typestr,"(");

			if (strchr(val, '%') || strchr(val, '_')) 
				sprintf(DPS_STREND(typestr),"it.sval LIKE '%s')",val);
			else sprintf(DPS_STREND(typestr),"it.sval='%s')",val);

			if (fromurlinfo_type) {
			  fromurlinfo_type = 0;
			  fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 16);
			  if (fromstr == NULL) continue;
			  sprintf(DPS_STREND(fromstr), ", urlinfo it");
			  serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 64);
			  if (serverstr == NULL) continue;
			  sprintf(DPS_STREND(serverstr), "%sit.url_id=url.rec_id AND it.sname='content-type'",(serverstr[0])?" AND ":"");
			}
		}

	}

	r = (unsigned int) 'u';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		
		if(!val[0])continue;
		
		if(!strcmp(var,"ul")){
			DPS_URL	URL;
			const char *first = "%";

			URL.freeme = 0;
			DpsURLInit(&URL);
			DpsURLParse(&URL,val);
			urlstr=(char*)DpsRealloc(urlstr,dps_strlen(urlstr)+dps_strlen(val)+50);
			if (urlstr == NULL) continue;

			if((URL.schema != NULL) && (URL.hostinfo != NULL)) {
			  first = "";
			}
			if(urlstr[0])dps_strcpy(DPS_STREND(urlstr)-1," OR ");
			else	dps_strcat(urlstr,"(");
/*			if(db->DBType==DPS_DB_PGSQL)
				sprintf(DPS_STREND(urlstr),"(url.url || '') LIKE '%s%s%%')", first, val);
			else*/
				sprintf(DPS_STREND(urlstr),"url.url LIKE '%s%s%%')", first, val);
			DpsURLFree(&URL);
		}else
		if(!strcmp(var,"u")){
			urlstr=(char*)DpsRealloc(urlstr,dps_strlen(urlstr)+dps_strlen(val)+50);
			if (urlstr == NULL) continue;
			if(urlstr[0])dps_strcpy(DPS_STREND(urlstr)-1," OR ");
			else	dps_strcat(urlstr,"(");
/*			if(db->DBType==DPS_DB_PGSQL)
				sprintf(DPS_STREND(urlstr),"(url.url || '') LIKE '%s')", val);
			else*/
				sprintf(DPS_STREND(urlstr),"url.url LIKE '%s')", val);
		}else
		if(!strcmp(var,"ue")){
			urlstr=(char*)DpsRealloc(urlstr,dps_strlen(urlstr)+dps_strlen(val)+50);
			if (urlstr == NULL) continue;
			if(urlstr[0])dps_strcpy(DPS_STREND(urlstr)-1," OR ");
			else	dps_strcat(urlstr,"(");
/*			if(db->DBType==DPS_DB_PGSQL)
				sprintf(DPS_STREND(urlstr),"(url.url || '') LIKE '%s')", val);
			else*/
				sprintf(DPS_STREND(urlstr),"url.url='%s')", val);
		}
		
	}
	
	r = (unsigned int) 'm';

	for(i = 0; i < Agent->Vars.Root[r].nvars; i++) {
		const char *var = Agent->Vars.Root[r].Var[i].name ? Agent->Vars.Root[r].Var[i].name : "";
		const char *val = Agent->Vars.Root[r].Var[i].val ? Agent->Vars.Root[r].Var[i].val : "";
		
		if(!val[0])continue;
		if(!strcmp(var,"maxhop")){
			hopstr=(char*)DpsRealloc(hopstr, dps_strlen(hopstr) + dps_strlen(val) + 50);
			if (hopstr == NULL) continue;
			if(hopstr[0]) dps_strcpy(DPS_STREND(hopstr)-1, " OR ");
			else	dps_strcat(hopstr,"(");
			sprintf(DPS_STREND(hopstr), "url.hops <= %d)", DPS_ATOI(val));	
		}
	}

	switch(dt) {
	case DPS_DT_BACK:
	  timestr = (char*)DpsRealloc(timestr, 128);
	  if (timestr == NULL) {
		db->where = (char*)DpsStrdup("");
		DPS_FREE(db->from);
		db->from = (char*)DpsStrdup("");
		goto ret;
	  }
	  if (dp) sprintf(timestr, "url.last_mod_time >= %li", (long int)(time(NULL) - dp));
	  break;
	case DPS_DT_ER:
	  timestr = (char*)DpsRealloc(timestr, 128);
	  if (timestr == NULL) {
		db->where = (char*)DpsStrdup("");
		DPS_FREE(db->from);
		db->from = (char*)DpsStrdup("");
		goto ret;
	  }
	  tm.tm_mday = dd; tm.tm_mon = dm, tm.tm_year = dy - 1900;
	  sprintf(timestr, "url.last_mod_time %s %li", (dx == 1) ? ">=" : "<=", (long int)mktime(&tm));
	  break;
	case DPS_DT_RANGE:
	  timestr = (char*)DpsRealloc(timestr, 128);
	  if (timestr == NULL) {
		db->where = (char*)DpsStrdup("");
		DPS_FREE(db->from);
		db->from = (char*)DpsStrdup("");
		goto ret;
	  }
	  sprintf(timestr, "url.last_mod_time >= %li AND url.last_mod_time <= %li", (long int)DB, (long int)DE);
	  break;
	case DPS_DT_UNKNOWN:
	default:
	  break;
	}
#if 1
	if (fromserver  && (/* (Agent->Flags.cmd == DPS_IND_POPRANK) || */ (Agent->flags & DPS_FLAG_SORT_POPRANK)) ) {
	  fromserver = 0;
	  fromstr = (char*)DpsRealloc(fromstr, dps_strlen(fromstr) + 12);
	  if (fromstr == NULL) {
		db->where = (char*)DpsStrdup("");
		DPS_FREE(db->from);
		db->from = (char*)DpsStrdup("");
		goto ret;
	  }
	  sprintf(DPS_STREND(fromstr), ", server s");
	  serverstr = (char*)DpsRealloc(serverstr, dps_strlen(serverstr) + 64);
	  if (serverstr == NULL) {
		db->where = (char*)DpsStrdup("");
		DPS_FREE(db->from);
		db->from = (char*)DpsStrdup("");
		goto ret;
	  }
	  sprintf(DPS_STREND(serverstr), "%ss.rec_id=url.server_id", (serverstr[0]) ? " AND " : "");
	}
#endif

	if( !urlstr[0] && !tagstr[0] && !statusstr[0] && !catstr[0] && !langstr[0] 
	    && !typestr[0] && !serverstr[0] && !fromstr[0] && !sitestr[0] && !timestr[0] && !hopstr[0] ){
		db->where = (char*)DpsStrdup("");
		DPS_FREE(db->from);
		db->from = (char*)DpsStrdup("");
		goto ret;
	}
	i = dps_strlen(urlstr) + dps_strlen(tagstr) + dps_strlen(statusstr) + dps_strlen(catstr) + dps_strlen(langstr) 
	  + dps_strlen(typestr) + dps_strlen(serverstr) + dps_strlen(sitestr) + dps_strlen(timestr) + dps_strlen(hopstr);
	db->where = (char*)DpsMalloc(i + 100);
	if (db->where == NULL) goto ret;
	db->where[0] = '\0';
	DPS_FREE(db->from);
	db->from = (char*)DpsStrdup(fromstr);
	
	if(urlstr[0]){
		dps_strcat(db->where,urlstr);
	}
	if(tagstr[0]){
		if(db->where[0])dps_strcat(db->where," AND ");
		dps_strcat(db->where,tagstr);
	}
	if(statusstr[0]){
		if(db->where[0])dps_strcat(db->where," AND ");
		dps_strcat(db->where,statusstr);
	}
	if(catstr[0]){
		if(db->where[0])dps_strcat(db->where," AND ");
		dps_strcat(db->where,catstr);
	}
	if(langstr[0]){
		if(db->where[0])dps_strcat(db->where," AND ");
		dps_strcat(db->where,langstr);
	}
	if(typestr[0]){
		if(db->where[0]) dps_strcat(db->where, " AND ");
		dps_strcat(db->where, typestr);
	}
	if(sitestr[0]){
		if(db->where[0]) dps_strcat(db->where, " AND ");
		dps_strcat(db->where, sitestr);
	}
	if(serverstr[0]){
		if(db->where[0]) dps_strcat(db->where, " AND ");
/*		if(!db->where[0]) dps_strcat(db->where, " 1 ");*/
		dps_strcat(db->where, serverstr);
	}
	if(timestr[0]){
		if(db->where[0]) dps_strcat(db->where, " AND ");
		dps_strcat(db->where, timestr);
	}
	if(hopstr[0]){
		if(db->where[0]) dps_strcat(db->where, " AND ");
		dps_strcat(db->where, hopstr);
	}
ret:
	DPS_FREE(urlstr);
	DPS_FREE(tagstr);
	DPS_FREE(statusstr);
	DPS_FREE(catstr);
	DPS_FREE(langstr);
	DPS_FREE(typestr);
	DPS_FREE(fromstr);
	DPS_FREE(serverstr);
	DPS_FREE(sitestr);
	DPS_FREE(timestr);
	DPS_FREE(hopstr);
	return db->where;
}


/********************************************************************/
/* --------------------------------------------------------------- */
/* Almost Unified code for all supported SQL backends              */
/* --------------------------------------------------------------- */
/*******************************************************************/




/************* Servers ******************************************/

static char * strdupnull(const char * src){
	if(src){
		if(*src){
			return((char*)DpsStrdup(src));
		}else{
			return (char*)DpsStrdup("");
		}
	}else{
		return (char*)DpsStrdup("");
	}
}



static int DpsURLDB(DPS_AGENT *Indexer, DPS_SERVER *S, DPS_DB *db) {
  size_t	rows, i;
  DPS_SQLRES	SQLRes;
  char    *url;
  DPS_HREF	Href;
  int           res;
  DPS_CHARSET *cs = DpsGetCharSet(DpsVarListFindStr(&Indexer->Conf->Cfg_Srv->Vars, "RemoteCharset", 
						    DpsVarListFindStr(&Indexer->Conf->Cfg_Srv->Vars, "URLCharset", "iso-8859-1")));
  const char *tablename = ((db->addrURL.filename != NULL) && (db->addrURL.filename[0] != '\0')) ? db->addrURL.filename : "links";
  const char *fieldname = DpsVarListFindStr(&db->Vars, "field", "url");
  char		qbuf[1024];

  DpsHrefInit(&Href);
  Href.charset_id = (cs != NULL) ? cs->id : ((Indexer->Conf->lcs != NULL) ? Indexer->Conf->lcs->id : 0);
  DpsSQLResInit(&SQLRes);

  dps_snprintf(qbuf, sizeof(qbuf) - 1, "SELECT %s FROM %s", fieldname, tablename);

  if(DPS_OK != (res = DpsSQLQuery(db, &SQLRes, qbuf))) {
    DpsLog(Indexer, DPS_LOG_INFO, "URLDB: db.err: %s", db->errstr);
    DpsSQLFree(&SQLRes);
    return res;
  }

  rows = DpsSQLNumRows(&SQLRes);
  DpsLog(Indexer, DPS_LOG_DEBUG, "URLDB: %d records fetched", rows);
  for(i = 0; i < rows; i++) {

    url = (char*)DpsSQLValue(&SQLRes, i, 0);
    Href.url = url;
    Href.method = DPS_METHOD_GET;
    DpsLog(Indexer, DPS_LOG_DEBUG, "URLDB: %s", url);

    DpsHrefCheck(Indexer, &Href, Href.url);
    if( Href.method != DPS_METHOD_DISALLOW && Href.method != DPS_METHOD_VISITLATER) {
      DpsHrefListAdd(Indexer, &Indexer->Hrefs, &Href);
      if (Indexer->Hrefs.nhrefs > 1024) DpsStoreHrefs(Indexer);
    }
  }
  DpsSQLFree(&SQLRes);
  return(DPS_OK);
}


static int DpsServerDB(DPS_AGENT *Indexer, DPS_SERVER *Srv, DPS_DB *db) {
  size_t	rows, i;
  DPS_SQLRES	SQLRes;
  DPS_HREF	Href;
  int           res, charset_id;
  DPS_CHARSET *cs = DpsGetCharSet(DpsVarListFindStr(&Indexer->Conf->Cfg_Srv->Vars, "RemoteCharset", 
						    DpsVarListFindStr(&Indexer->Conf->Cfg_Srv->Vars, "URLCharset", "iso-8859-1")));
  const char *tablename = ((db->addrURL.filename != NULL) && (db->addrURL.filename[0] != '\0')) ? db->addrURL.filename : "links";
  const char *fieldname = DpsVarListFindStr(&db->Vars, "field", "url");
  char		qbuf[1024];
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
  DPS_CHARSET *uni_cs;
  char   *ascii = NULL, *uni = NULL;
  DPS_CONV  url_uni;
  DPS_URL *SrvURL = NULL, *PunyURL = NULL;
  size_t len;
#endif

  DpsSQLResInit(&SQLRes);

  dps_snprintf(qbuf, sizeof(qbuf) - 1, "SELECT %s FROM %s WHERE %s IS NOT NULL", fieldname, tablename, fieldname);
  if(DPS_OK != (res = DpsSQLQuery(db, &SQLRes, qbuf))) return res;

  rows = DpsSQLNumRows(&SQLRes);
  for(i = 0; i < rows; i++) {
    
    DpsMatchFree(&Srv->Match);
    Srv->Match.pattern	= strdupnull(DpsSQLValue(&SQLRes,i,0));
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
    if(((Srv->Match.match_type==DPS_MATCH_BEGIN) || (Srv->Match.match_type==DPS_MATCH_FULL) ) && (Srv->Match.pattern[0]) ) {

      uni_cs = DpsGetCharSet("UTF8");
      DpsConvInit(&url_uni, cs, uni_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);
      SrvURL = DpsURLInit(NULL);
      PunyURL = DpsURLInit(NULL);
      DpsURLParse(SrvURL, Srv->Match.pattern);
      if (SrvURL->hostname != NULL) {
	  uni = (char*)DpsMalloc(48 * (len = dps_strlen(SrvURL->hostname) + 1));
	  if (uni == NULL) {
	      return DPS_ERROR;
	  }
	  DpsConv(&url_uni, (char*)uni, 48 * len, SrvURL->hostname, len);
#ifdef WITH_IDN
	  if (idna_to_ascii_8z((const char *)uni, &ascii, 0) != IDNA_SUCCESS) {
	      DPS_FREE(uni); 
	      return DPS_ERROR;
	  }
#else
	  ascii = (char*)DpsMalloc(48 * len);
	  if (ascii == NULL) {
	      DPS_FREE(uni); 
	      return DPS_ERROR;
	  }
	  if (idn_encodename(IDN_IDNCONV, (const char *)uni, ascii, len) != idn_success) {
	      DPS_FREE(ascii);
	      DPS_FREE(uni); 
	      return DPS_ERROR;
	  }
#endif
	  PunyURL->hostname = ascii;
	  RelLink(Indexer, SrvURL, PunyURL, &Srv->Match.idn_pattern, 0);
      }
/*				  DPS_FREE(ascii);  will be freed later with DpsURLFree(PunyURL) */
      DPS_FREE(uni); 
      DpsURLFree(SrvURL); DpsURLFree(PunyURL);
    }
#endif
    if(DPS_OK != DpsServerAdd(Indexer, Srv)) {
      char * s_err;
      s_err = (char*)DpsStrdup(Indexer->Conf->errstr);
      dps_snprintf(Indexer->Conf->errstr, sizeof(Indexer->Conf->errstr) - 1, "%s", s_err);
      DPS_FREE(s_err);
      DpsMatchFree(&Srv->Match);
      DpsSQLFree(&SQLRes);
      return DPS_ERROR;
    }
    if((Indexer->flags & DPS_FLAG_ADD_SERVURL) 
       && (Srv->Match.match_type == DPS_MATCH_BEGIN) && (Srv->Match.pattern[0])) {

      bzero((void*)&Href, sizeof(Href));
      Href.url = Srv->Match.pattern;
      Href.method = DPS_METHOD_GET;
      Href.site_id = Srv->site_id;
      Href.server_id = Srv->site_id;
      if (cs != NULL) charset_id = cs->id;
      else {
	if (Indexer->Conf->lcs != NULL) charset_id = Indexer->Conf->lcs->id;
	else charset_id = 0;
      }
      Href.charset_id = charset_id;
      DpsHrefListAdd(Indexer, &Indexer->Hrefs, &Href);
      if (Indexer->Hrefs.nhrefs > 1024) DpsStoreHrefs(Indexer);
    }
  }
  DpsMatchFree(&Srv->Match);
  DpsSQLFree(&SQLRes);
  return DPS_OK;
}


static int DpsLoadServerTable(DPS_AGENT * Indexer, DPS_DB *db){
	size_t		rows, i, j, jrows;
	DPS_SQLRES	SQLRes, SRes;
	DPS_HREF	Href;
	char		qbuf[1024];
	const char	*name = ((db->addrURL.filename != NULL) && (db->addrURL.filename[0] != '\0')) ? db->addrURL.filename : "server";
	const char      *infoname = DpsVarListFindStr(&db->Vars, "srvinfo", "srvinfo");
	int		res;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
	DPS_CHARSET *uni_cs, *cs;
	char   *ascii = NULL, *uni = NULL;
	DPS_CONV  url_uni;
	DPS_URL *SrvURL = NULL, *PunyURL = NULL;
	size_t len;
#endif
	
	DpsSQLResInit(&SQLRes);
	DpsSQLResInit(&SRes);

	dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT rec_id,url,tag,category,command,weight,ordre \
FROM %s WHERE enabled=1 AND parent=%s0%s ORDER BY ordre", name, qu, qu);
	
	if(DPS_OK!=(res=DpsSQLQuery(db,&SQLRes,qbuf)))
		return res;
	
	bzero((void*)&Href, sizeof(Href));
	
	rows=DpsSQLNumRows(&SQLRes);
	for(i=0;i<rows;i++){
		const char	*val;
		DPS_SERVER	*Server = Indexer->Conf->Cfg_Srv;
		
		Server->site_id		= DPS_ATOI(DpsSQLValue(&SQLRes, i, 0));
		DpsMatchFree(&Server->Match);
		Server->Match.pattern	= strdupnull(DpsSQLValue(&SQLRes,i,1));
		Server->ordre		= DPS_ATOU(DpsSQLValue(&SQLRes, i, 6));
		Server->command		= *DpsSQLValue(&SQLRes, i, 4);
		Server->weight		= (float)DPS_ATOF(DpsSQLValue(&SQLRes, i, 5));
		
		if((val=DpsSQLValue(&SQLRes,i,2)) && val[0])
			DpsVarListReplaceStr(&Server->Vars, "Tag", val);
		
		if((val=DpsSQLValue(&SQLRes,i,3)) && val[0])
			DpsVarListReplaceStr(&Server->Vars, "Category", val);
		

		sprintf(qbuf,"SELECT sname,sval FROM %s WHERE srv_id=%s%i%s", infoname, qu, Server->site_id, qu);
		if(DPS_OK != (res = DpsSQLQuery(db, &SRes, qbuf)))
		  return res;
		jrows = DpsSQLNumRows(&SRes);
		for(j = 0; j < jrows; j++) {
		  const char *sname = DpsSQLValue(&SRes, j, 0);
		  const char *sval = DpsSQLValue(&SRes, j, 1);
		  DpsVarListReplaceStr(&Server->Vars, sname, sval);
		}

		Server->Match.match_type	= DpsVarListFindInt(&Server->Vars, "match_type", DPS_MATCH_BEGIN);
		Server->Match.case_sense	= (dps_uint2)DpsVarListFindInt(&Server->Vars, "case_sense", 1);
		Server->Match.nomatch	= DpsVarListFindInt(&Server->Vars, "nomatch", 0);
		Server->MaxHops = DpsVarListFindUnsigned(&Server->Vars, "MaxHops", DPS_DEFAULT_MAX_HOPS);
		Server->MaxDepth = DpsVarListFindUnsigned(&Server->Vars, "MaxDepth", DPS_DEFAULT_MAX_DEPTH);
		Server->MaxURLength = DpsVarListFindUnsigned(&Server->Vars, "MaxURLength", DPS_DEFAULT_MAX_URLENGTH);
		Server->MinSiteWeight = (float)DpsVarListFindDouble(&Server->Vars, "MinSiteWeight", 0.0);
		Server->MinServerWeight = (float)DpsVarListFindDouble(&Server->Vars, "MinServerWeight", 0.0);
		DPS_FREE(Server->Match.arg);
		
		if (Server->command == 'S') {
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
		        if(((Server->Match.match_type==DPS_MATCH_BEGIN) || (Server->Match.match_type==DPS_MATCH_FULL) ) && (Server->Match.pattern[0]) ) {

			  uni_cs = DpsGetCharSet("UTF8");
			  cs = DpsGetCharSet(DpsVarListFindStr(&Server->Vars, "RemoteCharset", DpsVarListFindStr(&Server->Vars, "URLCharset", "iso8859-1")));
			  DpsConvInit(&url_uni, cs, uni_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);
			  SrvURL = DpsURLInit(NULL);
			  PunyURL = DpsURLInit(NULL);
			  DpsURLParse(SrvURL, Server->Match.pattern);
			  if (SrvURL->hostname != NULL) {
			      uni = (char*)DpsMalloc(48 * (len = dps_strlen(SrvURL->hostname) + 1));
			      if (uni == NULL) {
				  return DPS_ERROR;
			      }
			      DpsConv(&url_uni, (char*)uni, 48 * len, SrvURL->hostname, len);
#ifdef WITH_IDN
			      if (idna_to_ascii_8z((const char *)uni, &ascii, 0) != IDNA_SUCCESS) {
				  DPS_FREE(uni); 
				  return DPS_ERROR;
			      }
#else
			      ascii = (char*)DpsMalloc(48 * len);
			      if (ascii == NULL) {
				  DPS_FREE(uni); 
				  return DPS_ERROR;
			      }
			      if (idn_encodename(IDN_IDNCONV, (const char *)uni, ascii, len) != idn_success) {
				  DPS_FREE(ascii);
				  DPS_FREE(uni); 
				  return DPS_ERROR;
			      }
#endif
			      PunyURL->hostname = ascii;
			      RelLink(Indexer, SrvURL, PunyURL, &Server->Match.idn_pattern, 0);
			  }
/*				  DPS_FREE(ascii);  will be freed later with DpsURLFree(PunyURL) */
			  DPS_FREE(uni); 
			  DpsURLFree(SrvURL); DpsURLFree(PunyURL);
			}
#endif
			DpsServerAdd(Indexer, Server);
			if(((Server->Match.match_type==DPS_MATCH_BEGIN) ||(Server->Match.match_type==DPS_MATCH_FULL) ) && 
			   (Indexer->flags & DPS_FLAG_ADD_SERVURL)) {
				Href.url = Server->Match.pattern;
				Href.method=DPS_METHOD_GET;
				Href.site_id = Server->site_id;
				Href.server_id = Server->site_id;
				DpsHrefListAdd(Indexer, &Indexer->Hrefs, &Href);
			  }
		} else {
			char errstr[128];
			switch(DpsMethod(DpsVarListFindStr(&Server->Vars, "Method", "UNKNOWN"))) {
			case DPS_METHOD_TAG:
			  DPS_FREE(Server->Match.subsection);
			  Server->Match.subsection = DpsStrdup(DpsVarListFindStr(&Server->Vars, "Tag", ""));
			  DpsMatchListAdd(Indexer, &Indexer->Conf->SubSectionMatch, &Server->Match, errstr, sizeof(errstr), Server->ordre);
			  break;
			case DPS_METHOD_CATEGORY:
			  DPS_FREE(Server->Match.subsection);
			  Server->Match.subsection = DpsStrdup(DpsVarListFindStr(&Server->Vars, "Category", "0"));
			  DpsMatchListAdd(Indexer, &Indexer->Conf->SubSectionMatch, &Server->Match, errstr, sizeof(errstr), Server->ordre);
			  break;
			case DPS_METHOD_INDEX:
			case DPS_METHOD_NOINDEX:
			  DpsMatchListAdd(Indexer, &Indexer->Conf->SectionFilters, &Server->Match, errstr, sizeof(errstr), Server->ordre);
			  break;
			case DPS_METHOD_STORE:
			case DPS_METHOD_NOSTORE:
			  DpsMatchListAdd(Indexer, &Indexer->Conf->StoreFilters, &Server->Match, errstr, sizeof(errstr), Server->ordre);
			  break;
			case DPS_METHOD_UNKNOWN:
			  Server->Match.arg	= DpsStrdup(DpsVarListFindStr(&Server->Vars, "Method", "Disallow"));
			  DpsMatchListAdd(Indexer, &Indexer->Conf->SectionMatch, &Server->Match, errstr, sizeof(errstr), Server->ordre);
			  break;
			default:
			  DpsMatchListAdd(Indexer, &Indexer->Conf->Filters, &Server->Match, errstr, sizeof(errstr), Server->ordre);
			  break;
			}
		}
		for(j = 0; j < jrows; j++) {
		  const char *sname = DpsSQLValue(&SRes, j, 0);
		  DpsVarListDel(&Server->Vars, sname);
		}
		DpsSQLFree(&SRes);
/*		DpsVarListDel(&Server->Vars, "AuthBasic");
		DpsVarListDel(&Server->Vars, "Alias");*/
	}
	DpsSQLFree(&SQLRes);
	return(DPS_OK);
}


static int DpsLoadCategoryTable(DPS_AGENT *Indexer, DPS_DB *catdb) {
	char		qbuf[1024];
	DPS_SQLRES	SQLRes, SRes;
	size_t		rows, i, d, dbfrom = 0, dbto;
	const char	*name = ((catdb->addrURL.filename != NULL) && (catdb->addrURL.filename[0] != '\0')) ? catdb->addrURL.filename : "categories";
	int             res;
	DPS_DB		*db;

	DpsSQLResInit(&SQLRes);
	DpsSQLResInit(&SRes);

	if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	dbto =  DPS_DBL_TO(Indexer);
	if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);

	for (d = dbfrom; d < dbto; d++) {
	    db = DPS_DBL_DB(Indexer, d);
	    if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB); 


	    dps_snprintf(qbuf, sizeof(qbuf), "SELECT rec_id, path, link, name FROM %s", name);
	    if(DPS_OK != (res = DpsSQLQuery(catdb, &SQLRes, qbuf))) {
	      if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	      return res;
	    }
	    rows = DpsSQLNumRows(&SQLRes);
	    for(i = 0; i < rows; i++) {
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM categories WHERE rec_id=%s", DpsSQLValue(&SQLRes, i, 0));
	      if(DPS_OK != (res = DpsSQLQuery(catdb, &SRes, qbuf))) {
		if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
		DpsSQLFree(&SQLRes);
		return res;
	      }
	      switch(DPS_ATOI(DpsSQLValue(&SRes, 0, 0))) {
	      case 0: dps_snprintf(qbuf, sizeof(qbuf), "INSERT INTO categories(rec_id,path,link,name)VALUES(%s,'%s','%s','%s')",
				  DpsSQLValue(&SQLRes, i, 0), DpsSQLValue(&SQLRes, i, 1),  DpsSQLValue(&SQLRes, i, 2), DpsSQLValue(&SQLRes, i, 3));
	      break;
	      default: dps_snprintf(qbuf, sizeof(qbuf), "UPDATE categories SET path='%s',link='%s',name='%s' WHERE rec_id=%s",
				  DpsSQLValue(&SQLRes, i, 1), DpsSQLValue(&SQLRes, i, 2),  DpsSQLValue(&SQLRes, i, 3), DpsSQLValue(&SQLRes, i, 0));
	      }
	      if(DPS_OK != (res = DpsSQLAsyncQuery(catdb, NULL, qbuf))) {
		if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
		DpsSQLFree(&SQLRes);
		return res;
	      }
	    }
	    if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	}
	DpsSQLFree(&SRes);
	DpsSQLFree(&SQLRes);
	return DPS_OK;
}


static int DpsServerTableClean(DPS_DB *db){
	char str[128];
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	int rc;
	dps_snprintf(str, sizeof(str),  "DELETE FROM server WHERE enabled=%s0%s", qu, qu);
	rc = DpsSQLAsyncQuery(db, NULL, str);
	return rc;
}

static int DpsServerTableFlush(DPS_DB *db){
  DPS_SQLRES	SQLRes, SRes;
	size_t		i, rows;
	int rc;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	char str[128];
	int per_site, per_server;
	/*	
	rc = DpsSQLAsyncQuery(db, NULL, "UPDATE server SET enabled=0");
	if (rc != DPS_OK) return rc;
	*/
	DpsSQLResInit(&SQLRes);
	DpsSQLResInit(&SRes);
	dps_snprintf(str, sizeof(str), "SELECT rec_id FROM server WHERE command='S'");
	if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, str))) goto flush_exit;
	rows = DpsSQLNumRows(&SQLRes);
/*
	if (db->DBSQL_LIMIT) {
	  for(i = 0; i < rows; i++) {
	    dps_snprintf(str, sizeof(str), "SELECT 1 FROM url WHERE site_id=%s%s%s LIMIT 1", qu, DpsSQLValue(&SQLRes, i, 0), qu);
	    if(DPS_OK != (rc = DpsSQLQuery(db, &SRes, str))) goto flush_exit;
	    if (DpsSQLNumRows(&SRes) > 0) {
	      dps_snprintf(str, sizeof(str),  "UPDATE server SET enabled=1 WHERE rec_id=%s%s%s", qu, DpsSQLValue(&SQLRes, i, 0), qu);
	      rc = DpsSQLAsyncQuery(db, NULL, str);
	      if (rc != DPS_OK) goto flush_exit;
	    }
	  }
	} else {
*/
	  for(i = 0; i < rows; i++) {
	    dps_snprintf(str, sizeof(str), "SELECT COUNT(*) FROM url WHERE site_id=%s%s%s", qu, DpsSQLValue(&SQLRes, i, 0), qu);
	    if(DPS_OK != (rc = DpsSQLQuery(db, &SRes, str))) goto flush_exit;
	    per_site = DPS_ATOI(DpsSQLValue(&SRes, 0, 0));

	    dps_snprintf(str, sizeof(str), "SELECT COUNT(*) FROM url WHERE server_id=%s%s%s", qu, DpsSQLValue(&SQLRes, i, 0), qu);
	    if(DPS_OK != (rc = DpsSQLQuery(db, &SRes, str))) goto flush_exit;
	    per_server = DPS_ATOI(DpsSQLValue(&SRes, 0, 0));

	    if (per_site > 0 || per_server > 0) {
	      dps_snprintf(str, sizeof(str),  "UPDATE server SET enabled=1,pop_weight=%s%d%s WHERE rec_id=%s%s%s", 
			   qu, dps_max(per_site,per_server), qu,
			   qu, DpsSQLValue(&SQLRes, i, 0), qu);
	      rc = DpsSQLAsyncQuery(db, NULL, str);
	      if (rc != DPS_OK) goto flush_exit;
	    } else {
	      dps_snprintf(str, sizeof(str),  "UPDATE server SET enabled=0 WHERE rec_id=%s%s%s",
			   qu, DpsSQLValue(&SQLRes, i, 0), qu);
	      rc = DpsSQLAsyncQuery(db, NULL, str);
	      if (rc != DPS_OK) goto flush_exit;
	    }
	  }
/*	}*/
 flush_exit:
	DpsSQLFree(&SRes);
	DpsSQLFree(&SQLRes);

	return rc;
}


static int DpsCatTableFlush(DPS_DB *db) {
	int rc;
	
	rc = DpsSQLAsyncQuery(db, NULL, "DELETE FROM categories");

	return rc;
}


static int DpsServerTableAdd(DPS_AGENT *A, DPS_SERVER *Server, DPS_DB *db) {
	DPS_SQLRES	SQLRes;
	char            server_weight[32];
	int		res = DPS_OK, done = 1, nr = 0;
	const char	*alias = DpsVarListFindStr(&Server->Vars, "Alias", NULL);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	size_t		i, r, len;
	char		*buf, *arg, *arg_insert;
	dpshash32_t     rec_id = DpsStrHash32(DPS_NULL2EMPTY(Server->Match.pattern));
	DPS_CHARSET     *doccs = DpsGetCharSet(DpsVarListFindStr(&Server->Vars, "RemoteCharset", 
								 DpsVarListFindStr(&Server->Vars, "URLCharset", "iso-8859-1")));
	DPS_CHARSET	*loccs = A->Conf->lcs;
	DPS_CONV        dc_lc;
	
	DpsSQLResInit(&SQLRes);
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");
	DpsConvInit(&dc_lc, doccs, loccs, A->Conf->CharsToEscape, DPS_RECODE_URL);

	len = ((Server->Match.pattern) ? dps_strlen(Server->Match.pattern) : 0) + ((alias) ? dps_strlen(alias) : 0) + 2048;
	buf = (char*)DpsMalloc(len + 1);
	arg = (char*)DpsMalloc(len + 1);
	arg_insert = (char*)DpsMalloc(2*len + 1);
	if (buf == NULL) {
		dps_strcpy(db->errstr, "Out of memory");
		db->errcode = 1;
		return DPS_ERROR;
	}
	if (arg == NULL) {
	        DPS_FREE(buf);
		dps_strcpy(db->errstr, "Out of memory");
		db->errcode = 1;
		return DPS_ERROR;
	}
	if (arg_insert == NULL) {
	  DPS_FREE(buf); DPS_FREE(arg);
		dps_strcpy(db->errstr, "Out of memory");
		db->errcode = 1;
		return DPS_ERROR;
	}

	/* Convert URL to LocalCharset */
	DpsConv(&dc_lc, arg, len, DPS_NULL2EMPTY(Server->Match.pattern), dps_strlen(DPS_NULL2EMPTY(Server->Match.pattern)) + 1);
	/* Escape URL string */
	(void)DpsDBEscStr(db, arg_insert, arg, dps_strlen(arg));
	
	dps_snprintf(server_weight, sizeof(server_weight), "%f", Server->weight);
	DpsDBEscDoubleStr(server_weight);

	while(done) {
	  dps_snprintf(buf, len, "SELECT rec_id, url, tag, category, command, parent, ordre, weight, enabled,pop_weight FROM server WHERE rec_id=%s%d%s", 
		       qu, rec_id, qu);
	  if (DPS_OK != (res = DpsSQLQuery(db, &SQLRes, buf)))
	    goto ex;

	  nr = DpsSQLNumRows(&SQLRes);
	  if ((nr > 0) && (strcmp(arg, DpsSQLValue(&SQLRes, 0, 1)) != 0)) {
	    rec_id++;
	  } else {
	    done = 0;
	  }
/*	  DpsSQLFree(&SQLRes);*/
	}

	DpsVarListReplaceInt(&Server->Vars, "match_type",  Server->Match.match_type);
	DpsVarListReplaceInt(&Server->Vars, "case_sense",  Server->Match.case_sense);
	DpsVarListReplaceInt(&Server->Vars, "nomatch",  Server->Match.nomatch);
	DpsVarListDel(&Server->Vars, "Section");
	if (Server->command == 'F' && Server->Match.section != NULL) {
	  DpsVarListReplaceStr(&Server->Vars, "Section", Server->Match.section);
	}
	if (Server->command == 'F' && Server->Match.arg != NULL) {
	  DpsVarListReplaceStr(&Server->Vars, "Method", Server->Match.arg);
	  switch(DpsMethod(Server->Match.arg)) {
	  case DPS_METHOD_TAG: 
	    DpsVarListReplaceStr(&Server->Vars, "Tag", Server->Match.subsection); break;
	  case DPS_METHOD_CATEGORY: 
	    DpsVarListReplaceUnsigned(&Server->Vars, "Category", DpsGetCategoryId(A->Conf, Server->Match.subsection)); break;
	  case DPS_METHOD_UNKNOWN:
	    DpsVarListReplaceStr(&Server->Vars, "Arg", Server->Match.arg);
	    DpsVarListDel(&Server->Vars, "Method");
	    /* no break here */
	  default: break;
	  }
	} 

	if (nr == 0) {
	  dps_snprintf(buf, len, "\
INSERT INTO server (rec_id, enabled, tag, category, \
command, parent, ordre, weight, url, pop_weight \
) VALUES (%s%d%s, 1, '%s', %s, '%c', %s%d%s, %d, %s, '%s', 0\
)",
		       qu, rec_id, qu,
		       DpsVarListFindStr(&Server->Vars, "Tag", ""),
		       DpsVarListFindStr(&Server->Vars, "Category", "0"),
		       Server->command,
		       qu, Server->parent, qu,
		       Server->ordre,
		       server_weight,
		       arg_insert
		       );
	
	  if (DPS_OK != (res = DpsSQLAsyncQuery(db, NULL, buf))) goto ex;
	} else {
	
	  /* Update Server.ndocs according to pop_weight value */
	  if(A->Conf->flags & DPS_FLAG_ADD_SERVURL) {
	    Server->ndocs = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 9));
	  }

	  if ((Server->command != *DpsSQLValue(&SQLRes, 0, 4)) ||
	      (Server->parent != DPS_ATOI(DpsSQLValue(&SQLRes, 0, 5))) ||
	      (Server->ordre != (size_t)DPS_ATOI(DpsSQLValue(&SQLRes, 0, 6))) ||
	      (((Server->weight != 1) && (Server->weight != DPS_ATOF(DpsSQLValue(&SQLRes, 0, 7))))) ||
	      strcmp(DpsVarListFindStr(&Server->Vars, "Tag", ""), DpsSQLValue(&SQLRes, 0, 2)) ||
	      strcmp(DpsVarListFindStr(&Server->Vars, "Category", "0"), DpsSQLValue(&SQLRes, 0, 3)) ||
	      DPS_ATOI(DpsSQLValue(&SQLRes, 0, 8)) != 1 ) {

/*
	    fprintf(stderr, "\nrec_id: %d\n", rec_id);
	    fprintf(stderr, "Command: %c - %c|\n", Server->command, *DpsSQLValue(&SQLRes, 0, 4));
	    fprintf(stderr, " Parent: %d - %d|\n", Server->parent, DPS_ATOI(DpsSQLValue(&SQLRes, 0, 5)));
	    fprintf(stderr, "  Ordre: %d - %d|\n", Server->ordre, DPS_ATOI(DpsSQLValue(&SQLRes, 0, 6)));
	    fprintf(stderr, " Weight: %f - %f|\n", Server->ordre, DPS_ATOF(DpsSQLValue(&SQLRes, 0, 7)));
	    fprintf(stderr, "    Tag: %s - %s|\n", DpsVarListFindStr(&Server->Vars, "Tag", ""), DpsSQLValue(&SQLRes, 0, 2));
	    fprintf(stderr, "    Cat: %s - %s|\n", DpsVarListFindStr(&Server->Vars, "Category", ""), DpsSQLValue(&SQLRes, 0, 3));
*/
	    if (Server->weight != 1.0) 
	      dps_snprintf(buf, len, "UPDATE server SET enabled=1, tag='%s', category=%s, command='%c', parent=%s%i%s, ordre=%d, weight=%s WHERE rec_id=%s%d%s",
			   DpsVarListFindStr(&Server->Vars, "Tag", ""),
			   DpsVarListFindStr(&Server->Vars, "Category", "0"),
			   Server->command,
			   qu, Server->parent, qu,
			   Server->ordre,
			   server_weight,
			   qu, rec_id, qu
			   );
	    else 
	      dps_snprintf(buf, len, "UPDATE server SET enabled=1, tag='%s', category=%s, command='%c', parent=%s%i%s, ordre=%d WHERE rec_id=%s%d%s",
			 DpsVarListFindStr(&Server->Vars, "Tag", ""),
			 DpsVarListFindStr(&Server->Vars, "Category", "0"),
			 Server->command,
			 qu, Server->parent, qu,
			 Server->ordre,
			 qu, rec_id, qu
			 );
/*	    fprintf(stderr, "qbuf: %s\n", buf);*/
	    if(DPS_OK != (res = DpsSQLAsyncQuery(db, NULL, buf))) goto ex;
	  }
	}
	
	DpsSQLFree(&SQLRes);
	Server->site_id = rec_id;

	if (A->flags & DPS_FLAG_ADD_SERVURL) {
	  sprintf(buf, "DELETE FROM srvinfo WHERE srv_id=%s%i%s", qu, Server->site_id, qu);
	  if(DPS_OK != (res = DpsSQLAsyncQuery(db, NULL, buf))) goto ex;
	  
	  if ((Server->parent == 0) && A->Conf->Flags.SRVInfoSQL) {
	    for (r = 0; r < 256; r++) {
	      for(i = 0; i < Server->Vars.Root[r].nvars; i++) {
		DPS_VAR *Sec = &Server->Vars.Root[r].Var[i];
		if(Sec->val && Sec->name && 
		   (
		    strcasecmp(Sec->name, "Category") &&
		    strcasecmp(Sec->name, "Tag")
		    )
		   ){
		      (void)DpsDBEscStr(db, arg_insert, Sec->val, dps_strlen(Sec->val));
		      dps_snprintf(buf, len, "INSERT INTO srvinfo (srv_id,sname,sval) VALUES (%s%i%s,'%s','%s')",
				   qu, Server->site_id, qu, dps_strtolower(Sec->name), arg_insert);
		      if(DPS_OK != (res = DpsSQLAsyncQuery(db, NULL, buf)))break;
		}
	      }
	    }
	  }
	}

ex:
	DpsSQLFree(&SQLRes);
	DPS_FREE(buf);
	DPS_FREE(arg);
	DPS_FREE(arg_insert);
	return res;
}

static int DpsServerTableGetId(DPS_AGENT *Indexer, DPS_SERVER *Server, DPS_DB *db) {
	DPS_SQLRES	SQLRes;
	size_t len = ((Server->Match.pattern)?dps_strlen(Server->Match.pattern):0) + 1024;
	char *buf = (char*)DpsMalloc(len + 1);
	char *arg = (char*)DpsMalloc(2*len + 1);
	int res, id = 0, i;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	DpsSQLResInit(&SQLRes);

	if (buf == NULL) {
	  DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
	  return DPS_ERROR;
	}
	if (arg == NULL) {
	  DPS_FREE(buf);
	  DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
	  return DPS_ERROR;
	}

	for (i = 0; i < DPS_SERVERID_CACHE_SIZE; i++) {
	  if (Indexer->ServerIdCache[i].Command == Server->command)
	    if (!strcmp(DPS_NULL2EMPTY(Indexer->ServerIdCache[i].Match_Pattern), DPS_NULL2EMPTY(Server->Match.pattern))) {
	      DPS_SERVERCACHE tp = Indexer->ServerIdCache[i];
	      Server->site_id = id = Indexer->ServerIdCache[i].Id;
	      Server->weight = Indexer->ServerIdCache[i].Weight;
	      Server->ndocs = Indexer->ServerIdCache[i].Ndocs;

	      Indexer->ServerIdCache[i] = Indexer->ServerIdCache[Indexer->pServerIdCache];
	      Indexer->ServerIdCache[Indexer->pServerIdCache] = tp;
	      Indexer->pServerIdCache = (Indexer->pServerIdCache + 1) % DPS_SERVERID_CACHE_SIZE;
	      break;
	    }
	}

	if (id == 0) {
	  dpshash32_t     rec_id;
	  int done = 1, have_data;
	  float weight = 1.0;
	
	  dps_snprintf(buf, len, "SELECT rec_id, weight, pop_weight FROM server WHERE command='%c' AND url='%s'",
		       Server->command,
		       DPS_NULL2EMPTY(Server->Match.pattern)
		 );
	  res = DpsSQLQuery(db, &SQLRes, buf);
	  if ((res == DPS_OK) && DpsSQLNumRows(&SQLRes)) {
	    id = Server->site_id = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
	    Server->weight = (float)DPS_ATOF(DpsSQLValue(&SQLRes, 0, 1));
	    Server->ndocs = (size_t)DPS_ATOU(DpsSQLValue(&SQLRes, 0, 2));
	    DPS_FREE(Indexer->ServerIdCache[Indexer->pServerIdCache].Match_Pattern);
	    Indexer->ServerIdCache[Indexer->pServerIdCache].Match_Pattern = (char*)DpsStrdup(DPS_NULL2EMPTY(Server->Match.pattern));
	    Indexer->ServerIdCache[Indexer->pServerIdCache].Command = Server->command;
	    Indexer->ServerIdCache[Indexer->pServerIdCache].Id = id;
	    Indexer->ServerIdCache[Indexer->pServerIdCache].Weight = Server->weight;
	    Indexer->ServerIdCache[Indexer->pServerIdCache].Ndocs = Server->ndocs;
	    Indexer->ServerIdCache[Indexer->pServerIdCache].OnErrored = 0;
	    Indexer->pServerIdCache = (Indexer->pServerIdCache + 1) % DPS_SERVERID_CACHE_SIZE;
	    DPS_FREE(buf); DPS_FREE(arg);
	    DpsSQLFree(&SQLRes);
	    return DPS_OK;
	  }
	  DpsSQLFree(&SQLRes);
	  rec_id = DpsStrHash32(DPS_NULL2EMPTY(Server->Match.pattern));
	  while(done) {
	    dps_snprintf(buf, len, "SELECT rec_id, url FROM server WHERE rec_id=%s%i%s", qu, rec_id, qu);
	    if (DPS_OK != (res = DpsSQLQuery(db, &SQLRes, buf))) {
		DPS_FREE(buf); DPS_FREE(arg);
		return res;
	    }
	    
	    if (DpsSQLNumRows(&SQLRes) && (strcmp(DPS_NULL2EMPTY(Server->Match.pattern),DpsSQLValue(&SQLRes, 0, 1)) != 0)) {
	      rec_id++;
	    } else done = 0;
	    DpsSQLFree(&SQLRes);
	  }
	  dps_snprintf(buf, len, "SELECT enabled,tag,category,ordre,weight FROM server WHERE rec_id=%s%i%s", qu, Server->parent, qu);
	  res = DpsSQLQuery(db, &SQLRes, buf);
	  if (res != DPS_OK) {
	    DPS_FREE(buf); DPS_FREE(arg);
	    DpsSQLFree(&SQLRes);
	    return res;
	  }

	  if ((have_data = DpsSQLNumRows(&SQLRes)))
	    weight = (float)DPS_ATOF(DpsSQLValue(&SQLRes, 0, 4));

	  dps_snprintf(buf, len, 
"INSERT INTO server (rec_id, enabled, tag, category, command, parent, ordre, weight, url, pop_weight) VALUES (%s%d%s, %d, '%s', %s, '%c', %s%d%s, %d, %s, '%s', 1)",
		       qu, rec_id, qu,
		       (have_data) ? DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0)) : 0,
		       (have_data) ? DpsSQLValue(&SQLRes, 0, 1) : "",
		       (have_data) ? DpsSQLValue(&SQLRes, 0, 2) : "0",
		       Server->command,
		       qu, (have_data) ? Server->parent : 0, qu,
		       (have_data) ? DPS_ATOI(DpsSQLValue(&SQLRes, 0, 3)) : 0,
		       (have_data) ? DpsSQLValue(&SQLRes, 0, 4) : "1.0",
	       DpsDBEscStr(db, arg, DPS_NULL2EMPTY(Server->Match.pattern), dps_strlen(DPS_NULL2EMPTY(Server->Match.pattern)) )
		 );
	  res = DpsSQLAsyncQuery(db, NULL, buf);
	  DpsSQLFree(&SQLRes);

	  Server->site_id = id = rec_id;
	  DPS_FREE(Indexer->ServerIdCache[Indexer->pServerIdCache].Match_Pattern);
	  Indexer->ServerIdCache[Indexer->pServerIdCache].Match_Pattern = (char*)DpsStrdup(DPS_NULL2EMPTY(Server->Match.pattern));
	  Indexer->ServerIdCache[Indexer->pServerIdCache].Command = Server->command;
	  Indexer->ServerIdCache[Indexer->pServerIdCache].Id = id;
	  Indexer->ServerIdCache[Indexer->pServerIdCache].Weight = Server->weight = weight;
	  Indexer->ServerIdCache[Indexer->pServerIdCache].Ndocs = Server->ndocs = 1;
	  Indexer->ServerIdCache[Indexer->pServerIdCache].OnErrored = 0;
	  Indexer->pServerIdCache = (Indexer->pServerIdCache + 1) % DPS_SERVERID_CACHE_SIZE;
	}
	DPS_FREE(buf); DPS_FREE(arg);
	return DPS_OK;
}

/************************* find url ********************************/

static int DpsFindURL(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db){
	DPS_SQLRES	SQLRes;
	const char	*url=DpsVarListFindStr(&Doc->Sections,"URL","");
	const char      *o;
	char            *qbuf = NULL;
	dpshash32_t	id = 0, site_id = 0;
	int             hops = DpsVarListFindInt(&Doc->Sections, "Hops", 0);
	int		rc = DPS_OK;
	char            *e_url = DpsVarListFindStr(&Doc->Sections, "E_URL", NULL), *lc_url = NULL;
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        dc_lc;
	int             need_free_e_url = 0;
	size_t          i, l, len;
	
	len = dps_strlen(url);
	l = ((e_url == NULL) ? (24 * len) : dps_strlen(e_url)) + 1;
	if (e_url == NULL) {
		
	  doccs = DpsGetCharSetByID(Doc->charset_id);
	  if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
	  loccs = Indexer->Conf->lcs;
	  if(!loccs) loccs = DpsGetCharSet("iso-8859-1");
	  DpsConvInit(&dc_lc, doccs, loccs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);

	  /* Escape URL string */
	  if ((e_url = (char*)DpsMalloc( l )) == NULL) {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
	    return DPS_ERROR;
	  }
	  need_free_e_url = 1;
	  if ((lc_url = (char*)DpsMalloc( l )) == NULL) {
	    DPS_FREE(e_url);
	    DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
	    return DPS_ERROR;
	  }

	  /* Convert URL to LocalCharset */
	  DpsConv(&dc_lc, lc_url, l, url, len + 1);
	  /* Escape URL string */
	  (void)DpsDBEscStr(db, e_url, lc_url, dps_strlen(lc_url));
	  DpsVarListAddStr(&Doc->Sections, "E_URL", e_url);
	}

	DpsSQLResInit(&SQLRes);

		
	if ((qbuf = (char*)DpsMalloc( l + 100 )) == NULL){
	    DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
	    if(need_free_e_url) {
		DPS_FREE(lc_url);
		DPS_FREE(e_url);
	    }
	    return DPS_ERROR;
	}
		
	for(i = 0; i < DPS_FINDURL_CACHE_SIZE; i++) {
	    if (Indexer->DpsFindURLCache[i])
		if (!strcmp(e_url, Indexer->DpsFindURLCache[i])) {
		    char *tp = Indexer->DpsFindURLCache[i];
		    hops = Indexer->DpsFindURLCacheHops[i];
		    id = Indexer->DpsFindURLCacheId[i];
		    site_id = Indexer->DpsFindURLCacheSiteId[i];
		    Indexer->DpsFindURLCache[i] = Indexer->DpsFindURLCache[Indexer->pURLCache]; 
		    Indexer->DpsFindURLCacheId[i] = Indexer->DpsFindURLCacheId[Indexer->pURLCache];
		    Indexer->DpsFindURLCacheSiteId[i] = Indexer->DpsFindURLCacheSiteId[Indexer->pURLCache];
		    Indexer->DpsFindURLCacheHops[i] = Indexer->DpsFindURLCacheHops[Indexer->pURLCache];
		    Indexer->DpsFindURLCache[Indexer->pURLCache] = tp;
		    Indexer->DpsFindURLCacheId[Indexer->pURLCache] = id;
		    Indexer->DpsFindURLCacheSiteId[Indexer->pURLCache] = site_id;
		    Indexer->DpsFindURLCacheHops[Indexer->pURLCache] = hops;
		    Indexer->pURLCache = (Indexer->pURLCache + 1) % DPS_FINDURL_CACHE_SIZE;
		    break;
		}
	}

	if (i == DPS_FINDURL_CACHE_SIZE) {
	    dps_snprintf(qbuf, l + 100, "SELECT rec_id,hops,site_id FROM url WHERE url='%s'",e_url);
	    if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLRes,qbuf))){
		if (need_free_e_url) { 
		    DPS_FREE(e_url);
		    DPS_FREE(lc_url);
		}
		DPS_FREE(qbuf);
		return rc;
	    }
	    for(i=0;i<DpsSQLNumRows(&SQLRes);i++){
		if((o = DpsSQLValue(&SQLRes, i, 0))) {
		    id = DPS_ATOI(o);
		}
		if((o = DpsSQLValue(&SQLRes, i, 1))) {
		    hops = DPS_ATOI(o);
		}
		if((o = DpsSQLValue(&SQLRes, i, 2))) {
		    site_id = DPS_ATOI(o);
		    break;
		}
	    }
	    DpsSQLFree(&SQLRes);
	    DPS_FREE(Indexer->DpsFindURLCache[Indexer->pURLCache]);
	    Indexer->DpsFindURLCache[Indexer->pURLCache] = (char*)DpsStrdup(e_url);
	    Indexer->DpsFindURLCacheId[Indexer->pURLCache] = id;
	    Indexer->DpsFindURLCacheSiteId[Indexer->pURLCache] = site_id;
	    Indexer->DpsFindURLCacheHops[Indexer->pURLCache] = hops;
	    Indexer->pURLCache = (Indexer->pURLCache + 1) % DPS_FINDURL_CACHE_SIZE;
	}
	DPS_FREE(qbuf);

	if(need_free_e_url) {
	  DPS_FREE(lc_url);
	  DPS_FREE(e_url);
	}
	DpsVarListReplaceInt(&Doc->Sections, "DP_ID", id);
	DpsVarListReplaceInt(&Doc->Sections, "Site_id", site_id);
	DpsVarListReplaceInt(&Doc->Sections, "hops", hops);
	return	rc;
}


static int DpsFindMessage(DPS_AGENT *Indexer, DPS_DOCUMENT * Doc, DPS_DB *db){
	size_t 		i, len;
	char 		*qbuf;
	char 		*eid;
	DPS_SQLRES	SQLRes;
	const char	*message_id=DpsVarListFindStr(&Doc->Sections,"Header.Message-ID",NULL);
	int		rc;
	
	if(!message_id)
		return DPS_OK;
	
	DpsSQLResInit(&SQLRes);

	len = dps_strlen(message_id);
	eid = (char*)DpsMalloc(4 * len + 1);
	if (eid == NULL) return DPS_ERROR;
	qbuf = (char*)DpsMalloc(4 * len + 128);
	if (qbuf == NULL) { DPS_FREE(eid); return DPS_ERROR; }

	/* Escape URL string */
	(void)DpsDBEscStr(db, eid, message_id, len);
	
	dps_snprintf(qbuf, 4 * len + 128, 
		 "SELECT rec_id FROM url u, urlinfo i WHERE u.rec_id=i.url_id AND i.sname='message-id' AND i.sval='%s'", eid);
	rc = DpsSQLQuery(db,&SQLRes,qbuf);
	DPS_FREE(qbuf);
	DPS_FREE(eid);
	if (DPS_OK != rc)
		return rc;
	
	for(i=0;i<DpsSQLNumRows(&SQLRes);i++){
		const char * o;
		if((o=DpsSQLValue(&SQLRes,i,0))){
			DpsVarListReplaceInt(&Doc->Sections,"DP_ID", DPS_ATOI(o));
			break;
		}
	}
	DpsSQLFree(&SQLRes);
	return(DPS_OK);
}


/********************** Words ***********************************/

static int DpsDeleteWordFromURL(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc,DPS_DB *db){
	char	qbuf[512];
	int	i = 0, rc = DPS_OK;
	size_t  last = 0;
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
#if defined(WITH_TRACE) && defined(DEBUG)
	fprintf(Indexer->TR, "[%d] DpsDeleteWordFromURL: %d\n", Indexer->handle, url_id);
	fflush(Indexer->TR);
#endif
	switch(db->DBMode){
	
	case DPS_DBMODE_MULTI:
		for(i=MINDICT;i<MAXDICT;i++){
			if(last!=DICTNUM(i)){
				dps_snprintf(qbuf,sizeof(qbuf),"DELETE FROM dict%d WHERE url_id=%s%i%s", DICTNUM(i), qu, url_id, qu);
				if(DPS_OK!=(rc=DpsSQLAsyncQuery(db, NULL, qbuf)))
					return rc;
				last=DICTNUM(i);
			}
		}
		break;
	case DPS_DBMODE_MULTI_CRC:
		for(i=MINDICT;i<MAXDICT;i++){
			if(last!=DICTNUM(i)){
				int done=0;
#ifdef HAVE_ORACLE8
				if(db->DBDriver==DPS_DB_ORACLE8){
					dps_snprintf(qbuf,sizeof(qbuf)-1,"DELETE FROM ndict%d WHERE url_id=:1", DICTNUM(i),url_id);
					param_init(db, 1, 0, 0, 0);
					param_add(db, url_id, 0, 0, 0);
					done=1;
				}
#endif				
				if(!done)
					dps_snprintf(qbuf, sizeof(qbuf) - 1, "DELETE FROM ndict%d WHERE url_id=%s%d%s",
						     DICTNUM(i), qu, url_id, qu);

				if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf)))
					return rc;
				
				last=DICTNUM(i);
			}
		}
		break;
	case DPS_DBMODE_SINGLE_CRC:
		dps_snprintf(qbuf,sizeof(qbuf)-1,"DELETE FROM ndict WHERE url_id=%s%d%s", qu, url_id, qu);
		if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
			return rc;
		break;
	case DPS_DBMODE_CACHE:
		/* Let's lock it */
/*		i = DpsDeleteURLFromCache(Indexer, url_id, db);
		return i;*/
/*	  return DPS_OK;  It's already removed early ? */
	default:  /* DPS_DBMODE_SINGLE */
		dps_snprintf(qbuf, sizeof(qbuf)-1, "DELETE FROM dict WHERE url_id=%s%d%s", qu, url_id, qu);
		if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
			return rc;
		break;
	}
	return(DPS_OK);
}

static int StoreWordsMulti(DPS_AGENT * Indexer,DPS_DOCUMENT * Doc,DPS_DB *db){
	char	qbuf[512];
	char	tablename[64]="dict";
	char	tbl_nm[64];
	char    *word_escaped, *lcsword;
	int	n, rc = DPS_OK;
	size_t  prev_dictlen = 0, lcslen;
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	if ((word_escaped = (char*)DpsMalloc((lcslen = 18 * Indexer->WordParam.max_word_len) + 1)) == NULL) return DPS_ERROR;
	if ((lcsword = (char*)DpsMalloc(lcslen + 1)) == NULL) { DPS_FREE(word_escaped); return DPS_ERROR; }
	lcsword[lcslen] = '\0';

	if(db->DBMode==DPS_DBMODE_MULTI){
		dps_strcpy(tablename,"dict");
	}else{
		dps_strcpy(tablename,"ndict");
	}
	
	for(n=0;n<NDICTS;n++){
		if(prev_dictlen==dictlen[n])continue;
		prev_dictlen=dictlen[n];

		sprintf(tbl_nm, "%s%lu", tablename, (long unsigned)dictlen[n]);
		if(1){
			switch(db->DBType){
				case DPS_DB_PGSQL:
					rc=DpsSQLAsyncQuery(db,NULL,"BEGIN");
					break;
				case DPS_DB_ORACLE7:
				case DPS_DB_ORACLE8:
				case DPS_DB_SAPDB:
					rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
					db->commit_fl = 1;
					break;
				default:
					db->commit_fl = 1;
					break;
			}
			if(rc!=DPS_OK) {
			  DpsFree(word_escaped); DpsFree(lcsword);
			  return rc;
			}
		}
		
		/* Delete old words */
		dps_snprintf(qbuf,sizeof(qbuf),"DELETE FROM %s WHERE url_id=%s%i%s", tbl_nm, qu, url_id, qu);
		if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
			goto unlock_StoreWordsMulti;
		
		/* Insert new word */
		if(db->DBSQL_MULTINSERT) { /* MySQL & PgSQL */
			int have_words=0;
			char * qb,*qe;
			size_t step=4096,mlen=4096,len,i;

			qb=(char*)DpsMalloc(mlen);
			if (qb == NULL) goto unlock_StoreWordsMulti;
			if(db->DBMode==DPS_DBMODE_MULTI){
				sprintf(qb,"INSERT INTO %s(url_id,word,intag)VALUES",tbl_nm);
			}else{
				sprintf(qb,"INSERT INTO %s(url_id,word_id,intag)VALUES",tbl_nm);
			}
			qe=qb+dps_strlen(qb);

			for(i=0;i<Doc->Words.nwords;i++){
				if(!Doc->Words.Word[i].coord)continue;
				
				DpsConv(&Indexer->uni_lc, lcsword, lcslen, 
					(char*)Doc->Words.Word[i].uword, sizeof(dpsunicode_t) * (Doc->Words.Word[i].ulen + 1));

				if (DICTNUM(dps_strlen(lcsword)) == dictlen[n]) {
					len = qe - qb;
					/* DPS_MAXWORDSIZE+100 should be enough */
					if((len + 18 * Indexer->WordParam.max_word_len + 100) >= mlen){
						mlen+=step;
						qb=(char*)DpsRealloc(qb,mlen);
						if (qb == NULL) goto unlock_StoreWordsMulti;
						qe=qb+len;
					}
					if(have_words)dps_strcpy(qe++,",");
					have_words++;
					if(db->DBMode==DPS_DBMODE_MULTI){
					  (void)DpsDBEscStr(db, word_escaped, lcsword, dps_strlen(lcsword));
					  sprintf(qe,"(%s%i%s,'%s',%d)", qu, url_id, qu, word_escaped, Doc->Words.Word[i].coord);
					}else{
					  sprintf(qe,"(%s%i%s,%d,%d)", qu, url_id, qu, DpsStrHash32(lcsword), Doc->Words.Word[i].coord);
					}
					qe = qe + dps_strlen(qe);
					
					if((qe - qb) + 128 >= DPS_MAX_MULTI_INSERT_QSIZE){
						if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qb))) {
							DPS_FREE(qb);
							goto unlock_StoreWordsMulti;
						}
						/* Init query again */
						if(db->DBMode==DPS_DBMODE_MULTI){
							sprintf(qb,"INSERT INTO %s (url_id,word,intag) VALUES ",tbl_nm);
						}else{
							sprintf(qb,"INSERT INTO %s (url_id,word_id,intag) VALUES ",tbl_nm);
						}
						qe=qb+dps_strlen(qb);
						have_words = 0;
					}
				}
			}
			if(have_words)rc=DpsSQLAsyncQuery(db,NULL,qb);
			DPS_FREE(qb);
			if(rc!=DPS_OK) goto unlock_StoreWordsMulti;
		}else{
			size_t i;
			for(i=0;i<Doc->Words.nwords;i++){
				size_t len;
				if(!Doc->Words.Word[i].coord)continue;

				DpsConv(&Indexer->uni_lc, lcsword, lcslen, 
					(char*)Doc->Words.Word[i].uword, sizeof(dpsunicode_t) * (Doc->Words.Word[i].ulen + 1));
				
				len = dps_strlen(lcsword);
				if (DICTNUM(len) == dictlen[n]){
					if(db->DBMode==DPS_DBMODE_MULTI){
					  (void)DpsDBEscStr(db, word_escaped, lcsword, len);
					  dps_snprintf(qbuf, sizeof(qbuf) - 1, 
						       "INSERT INTO %s (url_id,word,intag) VALUES(%s%i%s,'%s',%d)",
						       tbl_nm, qu, url_id, qu, word_escaped, Doc->Words.Word[i].coord);
					}else{
					  dps_snprintf(qbuf, sizeof(qbuf) - 1,
						       "INSERT INTO %s (url_id,word_id,intag) VALUES(%s%i%s,%d,%d)",
						       tbl_nm, qu, url_id, qu, DpsStrHash32(lcsword), Doc->Words.Word[i].coord);
					}
					if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
						goto unlock_StoreWordsMulti;
				}
			}
		}

unlock_StoreWordsMulti:		
		if(1){
			switch(db->DBType){
				case DPS_DB_PGSQL:
				case DPS_DB_ORACLE7:
				case DPS_DB_ORACLE8:
				case DPS_DB_SAPDB:
					rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
					db->commit_fl = 0;
					break;
					
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_DB2)
			        case DPS_DB_SOLID:
			        case DPS_DB_VIRT:
			        case DPS_DB_DB2:
					rc=DpsSQLAsyncQuery(db,NULL,"DPS_COMMIT");
#endif
				default:
					db->commit_fl = 0;
					break;
			}
			if(rc!=DPS_OK) {
			  DpsFree(word_escaped); DpsFree(lcsword);
			  return rc;
			}
		}
	}
	DpsFree(word_escaped); DpsFree(lcsword);
	return(DPS_OK);
}


static int StoreWordsSingle(DPS_AGENT * Indexer,DPS_DOCUMENT * Doc,DPS_DB *db){
  size_t	i, lcslen;
	char	qbuf[512]="";
	char    *word_escaped, *lcsword;
	int	rc=DPS_OK;
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	if ((word_escaped = (char*)DpsMalloc((lcslen = 18 * Indexer->WordParam.max_word_len) + 1)) == NULL) return DPS_ERROR;
	if ((lcsword = (char*)DpsMalloc(lcslen + 1)) == NULL) { DPS_FREE(word_escaped); return DPS_ERROR; }
	lcsword[lcslen] = '\0';
	
	/* Start transaction if supported */
	/* This is to make stuff faster   */

	if(1){
		switch(db->DBType){
			case DPS_DB_PGSQL:
				rc=DpsSQLAsyncQuery(db,NULL,"BEGIN");
				break;
			case DPS_DB_SQLITE:
			case DPS_DB_SQLITE3:
			case DPS_DB_MSSQL:
				rc=DpsSQLAsyncQuery(db,NULL,"BEGIN TRANSACTION");
				break;
			case DPS_DB_ORACLE7:
			case DPS_DB_ORACLE8:
			case DPS_DB_SAPDB:
				rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
				db->commit_fl = 1;
				break;
			case DPS_DB_IBASE:
				rc=DpsSQLAsyncQuery(db,NULL,"BEGIN");
				db->commit_fl = 1;
				break;
			default:
				rc=DPS_OK;
				db->commit_fl = 1;
				break;
		}
		if(rc!=DPS_OK) {
		  DpsFree(word_escaped); DpsFree(lcsword);
		  return rc;
		}
	}
	
	/* Delete old words */
	if(db->DBMode==DPS_DBMODE_SINGLE || db->DBMode==DPS_DBMODE_CACHE){
		sprintf(qbuf,"DELETE FROM dict WHERE url_id=%s%i%s", qu, url_id, qu);
	}else
	if(db->DBMode==DPS_DBMODE_SINGLE_CRC){
		sprintf(qbuf,"DELETE FROM ndict WHERE url_id=%s%i%s", qu, url_id, qu);
	}
	
	if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
		goto unlock_StoreWordsSingle;
	
	/* Insert new words */
	if(db->DBSQL_MULTINSERT) { /* MySQL & PgSQL */
		if(Doc->Words.nwords){
			size_t nstored=0;
			char * qb,*qe;
			size_t step=1024;
			size_t mlen = dps_min(DPS_MAX_MULTI_INSERT_QSIZE, 
					      Doc->Words.nwords * Indexer->WordParam.max_word_len * 18 + 256);
			qb=(char*)DpsMalloc(mlen);
			if (qb == NULL) goto unlock_StoreWordsSingle;
			
			while(nstored<Doc->Words.nwords){
			  size_t rstored = 0;

				if(db->DBMode==DPS_DBMODE_SINGLE || db->DBMode==DPS_DBMODE_CACHE){
					dps_strcpy(qb,"INSERT INTO dict (word,url_id,intag) VALUES ");
				}else
				if(db->DBMode==DPS_DBMODE_SINGLE_CRC){
					dps_strcpy(qb,"INSERT INTO ndict (url_id,word_id,intag) VALUES ");
				}
				qe=qb+dps_strlen(qb);

				for(i=nstored;i<Doc->Words.nwords;i++){
					size_t len = qe - qb;
					if(!Doc->Words.Word[i].coord) { nstored++; continue;}
					rstored++;
				
					DpsConv(&Indexer->uni_lc, lcsword, lcslen, 
						(char*)Doc->Words.Word[i].uword, sizeof(dpsunicode_t) * (Doc->Words.Word[i].ulen + 1));

					/* DPS_MAXWORDSIZE+100 should be enough */
					if((len + 18 * Indexer->WordParam.max_word_len + 100) >= mlen){
					        mlen += dps_max(step, 18 * Indexer->WordParam.max_word_len + 100);
						qb=(char*)DpsRealloc(qb, mlen);
						if (qb == NULL) {
						  rc = DPS_ERROR;
						  goto unlock_StoreWordsSingle;
						}
						qe=qb+len;
					}
					
					if(i>nstored)*qe++=',';

					if(db->DBMode==DPS_DBMODE_SINGLE || db->DBMode==DPS_DBMODE_CACHE){
					  (void)DpsDBEscStr(db, word_escaped, lcsword, dps_strlen(lcsword));
					  dps_snprintf(qe, mlen - (size_t)(qe - qb), "('%s',%d,%d)", word_escaped, url_id, Doc->Words.Word[i].coord);
					  qe = DPS_STREND(qe);
					}else
					if(db->DBMode==DPS_DBMODE_SINGLE_CRC){
					  dps_snprintf(qe, mlen - (size_t)(qe-qb), "(%i,%d,%d)", url_id, DpsStrHash32(lcsword), Doc->Words.Word[i].coord);
					  qe = DPS_STREND(qe);
					}
					if(qe>qb+DPS_MAX_MULTI_INSERT_QSIZE)
						break;
				}
				nstored=i;
				rc = (rstored > 0) ? DpsSQLAsyncQuery(db, NULL, qb) : DPS_OK;
				if(rc!=DPS_OK) {
				  DPS_FREE(qb);
				  goto unlock_StoreWordsSingle;
				}
			}
			DPS_FREE(qb);
		}
	}else{
		for(i=0;i<Doc->Words.nwords;i++){
			if(!Doc->Words.Word[i].coord)continue;
				
			DpsConv(&Indexer->uni_lc, lcsword, lcslen, 
				(char*)Doc->Words.Word[i].uword, sizeof(dpsunicode_t) * (Doc->Words.Word[i].ulen + 1));

			if(db->DBMode == DPS_DBMODE_SINGLE || db->DBMode == DPS_DBMODE_CACHE) {
			  (void)DpsDBEscStr(db, word_escaped, lcsword, dps_strlen(lcsword));
			  dps_snprintf(qbuf, sizeof(qbuf), "INSERT INTO dict (url_id,word,intag)VALUES(%s%i%s,'%s',%d)", qu, url_id, qu, 
				  word_escaped, Doc->Words.Word[i].coord);
			}else{
			  dps_snprintf(qbuf, sizeof(qbuf), "INSERT INTO ndict (url_id,word_id,intag)VALUES(%s%i%s,%d,%d)", qu, url_id, qu,
					DpsStrHash32(lcsword), Doc->Words.Word[i].coord);
			}
			if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
				goto unlock_StoreWordsSingle;
		}
	}

unlock_StoreWordsSingle:
	/* Commit */
	if(1){
		switch(db->DBType){
			case DPS_DB_PGSQL:
			case DPS_DB_SQLITE:
			case DPS_DB_SQLITE3:
			case DPS_DB_MSSQL:
				rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
				break;
			case DPS_DB_ORACLE7:
			case DPS_DB_ORACLE8:
			case DPS_DB_SAPDB:
				db->commit_fl = 0;
				rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
				break;
			case DPS_DB_IBASE:
				rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
				db->commit_fl = 1;
				break;
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_DB2)
		        case DPS_DB_SOLID:
		        case DPS_DB_VIRT:
			case DPS_DB_DB2:
				rc=DpsSQLAsyncQuery(db,NULL,"DPS_COMMIT");
#endif
			default:
				db->commit_fl = 0;
				break;
		}
		if(rc!=DPS_OK) {
		  DpsFree(word_escaped); DpsFree(lcsword);
		  return rc;
		}
	}
	DpsFree(word_escaped); DpsFree(lcsword);
	return(DPS_OK);
}




static int DpsStoreWords(DPS_AGENT * Indexer,DPS_DOCUMENT *Doc,DPS_DB *db){
	int	res;

	switch(db->DBMode){
		case DPS_DBMODE_CACHE:
			res=StoreWordsSingle(Indexer,Doc,db); /* For FillDictionary option enabled */
/*			res=DpsStoreWordsCache(Indexer,Doc,db);*/
			break;
		case DPS_DBMODE_MULTI:
		case DPS_DBMODE_MULTI_CRC:
			res=StoreWordsMulti(Indexer,Doc,db);
			break;
		case DPS_DBMODE_SINGLE:
		case DPS_DBMODE_SINGLE_CRC:
		default:
			res=StoreWordsSingle(Indexer,Doc,db);
			break;
	}
	return(res);
}


static int DpsDeleteAllFromDict(DPS_AGENT *Indexer,DPS_DB *db){
	char	qbuf[512];
	size_t	i,last=0;
	int	rc=DPS_OK;
	
	switch(db->DBMode){
	case DPS_DBMODE_MULTI:
		for(i=MINDICT;i<MAXDICT;i++){
			if(last!=DICTNUM(i)){
				if (db->DBSQL_TRUNCATE)
				  sprintf(qbuf,"TRUNCATE TABLE dict%lu", (long unsigned)DICTNUM(i));
				else
				  sprintf(qbuf,"DELETE FROM dict%lu", (long unsigned)DICTNUM(i));

				if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
					return rc;
				last=DICTNUM(i);
			}
		}
		break;
	case DPS_DBMODE_MULTI_CRC:
		for(i=MINDICT;i<MAXDICT;i++){
			if(last!=DICTNUM(i)){
				if (db->DBSQL_TRUNCATE)
				  sprintf(qbuf,"TRUNCATE TABLE ndict%lu", (long unsigned)DICTNUM(i));
				else
				  sprintf(qbuf,"DELETE FROM ndict%lu", (long unsigned)DICTNUM(i));
	
				if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
					return(DPS_ERROR);
				last=DICTNUM(i);
			}
		}
		break;
	case DPS_DBMODE_SINGLE_CRC:
		if (db->DBSQL_TRUNCATE)
			rc=DpsSQLAsyncQuery(db,NULL,"TRUNCATE TABLE ndict");
		else
			rc=DpsSQLAsyncQuery(db,NULL,"DELETE FROM ndict");
		break;
	default:
		if (db->DBSQL_TRUNCATE)
			rc=DpsSQLAsyncQuery(db,NULL,"TRUNCATE TABLE dict");
		else
			rc=DpsSQLAsyncQuery(db,NULL,"DELETE FROM dict");
		break;
	}
	return rc;
}


/***************** CrossWords *******************************/

static int DpsDeleteAllFromCrossDict(DPS_AGENT * Indexer,DPS_DB *db){
	char	qbuf[1024];
	char	table[64]="ncrossdict";
	
	if((db->DBMode==DPS_DBMODE_SINGLE)||(db->DBMode==DPS_DBMODE_MULTI)/*||(db->DBMode==DPS_DBMODE_CACHE)*/) {
		dps_strcpy(table,"crossdict");
	}
	sprintf(qbuf,"DELETE FROM %s",table);
	return DpsSQLAsyncQuery(db,NULL,qbuf);
}



static int DpsDeleteCrossWordsToURL(DPS_AGENT * Indexer,DPS_DOCUMENT *Doc,DPS_DB *db){
	char	qbuf[128];
	char	table[16]="ncrossdict";
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	int	rc=DPS_OK;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	if((db->DBMode==DPS_DBMODE_SINGLE)||(db->DBMode==DPS_DBMODE_MULTI)/*||(db->DBMode==DPS_DBMODE_CACHE)*/) {
		dps_strcpy(table,"crossdict");
	}
	if(url_id){
		sprintf(qbuf,"DELETE FROM %s WHERE url_id=%s%i%s", table, qu, url_id, qu);
		if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf))) {
		  DpsSQLEnd(db);
			return rc;
		}
	}
	return rc;
}


static int DpsDeleteCrossWordsFromURL(DPS_AGENT * Indexer,DPS_DOCUMENT *Doc,DPS_DB *db){
	char	qbuf[128];
	char	table[16]="ncrossdict";
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	int	rc=DPS_OK;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	if((db->DBMode==DPS_DBMODE_SINGLE)||(db->DBMode==DPS_DBMODE_MULTI)/*||(db->DBMode==DPS_DBMODE_CACHE)*/) {
		dps_strcpy(table,"crossdict");
	}
	if(url_id){
		sprintf(qbuf,"DELETE FROM %s WHERE ref_id=%s%i%s", table, qu, url_id, qu);
		if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf))) {
			return rc;
		}
	}
	return rc;
}


static int DpsStoreCrossWords(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db) {
	DPS_DOCUMENT	U;
	size_t		i, lcslen;
	char		qbuf[1024];
	char		table[64]="ncrossdict";
	char            *word_escaped, *lcsword;
	int		crcmode=1;
	int skip_same_site = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "CrossWordsSkipSameSite", DPS_CROSSWORDSSKIPSAMESITE), "yes");
	const char	*lasturl="scrap";
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	urlid_t		referrer = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	const char      *site_id = DpsVarListFindStr(&Doc->Sections, "Site_id", "0");
	urlid_t		childid = 0;
	int		rc=DPS_OK, disallow = 0;
	DPS_HREF        Href;
	DPS_URL         docURL;

	if ((Indexer->Flags.use_crosswords == 0)/* || (db->DBMode == DPS_DBMODE_CACHE)*/) return DPS_OK;

	if ((word_escaped = (char*)DpsMalloc((lcslen = 18 * Indexer->WordParam.max_word_len) + 1)) == NULL) return DPS_ERROR;
	if ((lcsword = (char*)DpsMalloc(lcslen + 1)) == NULL) { DPS_FREE(word_escaped); return DPS_ERROR; }
	lcsword[lcslen] = '\0';
	
	DpsDocInit(&U);
	bzero((void*)&Href, sizeof(Href));
	if(DPS_OK != (rc = DpsDeleteCrossWordsFromURL(Indexer, Doc, db))) {
		DpsDocFree(&U);
		DpsFree(word_escaped); DpsFree(lcsword);
		return rc;
	}
	
	if(Doc->CrossWords.ncrosswords==0) {
		DpsDocFree(&U);
		DpsFree(word_escaped); DpsFree(lcsword);
		return rc;
	}
	
	if((db->DBMode==DPS_DBMODE_SINGLE)||(db->DBMode==DPS_DBMODE_MULTI)/*||(db->DBMode==DPS_DBMODE_CACHE)*/) {
		dps_strcpy(table,"crossdict");
		crcmode=0;
	}
	
	docURL.freeme = 0;
	U.charset_id = Doc->charset_id;
	DpsURLInit(&docURL);
	DpsURLParse(&docURL, DpsVarListFindStr(&Doc->Sections, "URL", ""));
	for(i=0;i<Doc->CrossWords.ncrosswords;i++){
		if(!Doc->CrossWords.CrossWord[i].weight)continue;
		if(strcmp(lasturl,Doc->CrossWords.CrossWord[i].url)){
		        Href.url = (char*)DpsStrdup(Doc->CrossWords.CrossWord[i].url);
			DpsConvertHref(Indexer, &docURL, &Href);
			DpsVarListReplaceStr(&U.Sections, "URL", Href.url);
			DpsVarListDel(&U.Sections, "E_URL");
			DpsVarListDel(&U.Sections, "URL_ID");
			if(DPS_OK!=(rc=DpsFindURL(Indexer,&U,db))){
				DpsDocFree(&U);
				DpsURLFree(&docURL);
				DpsFree(word_escaped); DpsFree(lcsword);
				return rc;
			}
			childid = DpsVarListFindInt(&U.Sections,"DP_ID",0);
			lasturl = Doc->CrossWords.CrossWord[i].url;
			if (skip_same_site) disallow = !strcasecmp(site_id, DpsVarListFindStr(&U.Sections, "Site_id", "0"));
			DPS_FREE(Href.url);
		}
		Doc->CrossWords.CrossWord[i].referree_id = childid;
		if (disallow) Doc->CrossWords.CrossWord[i].weight = 0;
	}
	
	/* Begin transacttion/lock */
	if(0){
		switch(db->DBType){
			case DPS_DB_MYSQL:
				sprintf(qbuf,"LOCK TABLES %s WRITE",table);
				rc=DpsSQLAsyncQuery(db,NULL,qbuf);
				break;
			case DPS_DB_PGSQL:
				rc=DpsSQLAsyncQuery(db,NULL,"BEGIN");
				break;
			case DPS_DB_ORACLE7:
			case DPS_DB_ORACLE8:
			case DPS_DB_SAPDB:
				db->commit_fl = 1;
				rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
				break;
			default:
				db->commit_fl = 1;
				break;
		}
		if(rc!=DPS_OK){
			DpsDocFree(&U);
			DpsURLFree(&docURL);
			DpsFree(word_escaped); DpsFree(lcsword);
			return rc;
		}
	}
	/* Insert new words */
	if(db->DBSQL_MULTINSERT) { /* MySQL & PgSQL */
	  int have_words = 0;
	  char *qb, *qe;
	  size_t step = 4096, mlen = 4096, len;

	  qb = (char*)DpsMalloc(mlen);
	  if (qb == NULL) goto unlock_DpsStoreCrossWords;
	  if(crcmode){
	    sprintf(qb,"INSERT INTO %s(ref_id,url_id,word_id,intag)VALUES", table);
	  }else{
	    sprintf(qb,"INSERT INTO %s(ref_id,url_id,word,intag)VALUES", table);
	  }
	  qe = qb + dps_strlen(qb);

	  for(i=0;i<Doc->CrossWords.ncrosswords;i++){
	    if((Doc->CrossWords.CrossWord[i].weight == 0) || (Doc->CrossWords.CrossWord[i].referree_id == 0)) continue;
	    {
	      int weight = DPS_WRDCOORDL(Doc->CrossWords.CrossWord[i].pos, Doc->CrossWords.CrossWord[i].weight, Doc->CrossWords.CrossWord[i].ulen);

	      DpsConv(&Indexer->uni_lc, lcsword, lcslen, 
		      (char*)Doc->CrossWords.CrossWord[i].uword, sizeof(dpsunicode_t) * (Doc->CrossWords.CrossWord[i].ulen + 1));

	      len = (qe - qb);
	      if((len + 128 + ((crcmode) ? 64 : (18 * Indexer->WordParam.max_word_len))) >= mlen) {
		mlen += step;
		qb = (char*)DpsRealloc(qb, mlen);
		if (qb == NULL) goto unlock_DpsStoreCrossWords;
		qe = qb + len;
	      }
	      if(have_words)dps_strcpy(qe++,",");
	      have_words++;

	      if(crcmode){
		sprintf(qe,"(%s%i%s,%s%i%s,%d,%d)", qu, referrer, qu, qu, Doc->CrossWords.CrossWord[i].referree_id, qu,
			DpsStrHash32(lcsword), weight);
	      }else{
		/* Escape text to track it  */
		(void)DpsDBEscStr(db, word_escaped, lcsword, dps_strlen(lcsword));

		sprintf(qe,"(%s%i%s,%s%i%s,'%s',%d)", qu, referrer, qu, qu, Doc->CrossWords.CrossWord[i].referree_id, qu,
			word_escaped, weight);
	      }
	      qe = qe + dps_strlen(qe);

	      if((qe - qb) + 128 >= DPS_MAX_MULTI_INSERT_QSIZE) {
		if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qb))) {
				DpsDocFree(&U);
				DPS_FREE(qb);
				goto unlock_DpsStoreCrossWords;
		}
		/* Init query again */
		if(crcmode){
		  sprintf(qb,"INSERT INTO %s(ref_id,url_id,word_id,intag)VALUES", table);
		}else{
		  sprintf(qb,"INSERT INTO %s(ref_id,url_id,word,intag)VALUES", table);
		}
		qe = qb + dps_strlen(qb);
		have_words = 0;
	      }
	    }
	  }
	  if (have_words) rc = DpsSQLAsyncQuery(db, NULL, qb);
	  DPS_FREE(qb);
	  if(DPS_OK != rc) goto unlock_DpsStoreCrossWords;

	} else
	for(i=0;i<Doc->CrossWords.ncrosswords;i++){
		if(Doc->CrossWords.CrossWord[i].weight && Doc->CrossWords.CrossWord[i].referree_id){
		  int weight = DPS_WRDCOORDL(Doc->CrossWords.CrossWord[i].pos, Doc->CrossWords.CrossWord[i].weight, Doc->CrossWords.CrossWord[i].ulen);

			DpsConv(&Indexer->uni_lc, lcsword, lcslen, 
				(char*)Doc->CrossWords.CrossWord[i].uword, sizeof(dpsunicode_t) * (Doc->CrossWords.CrossWord[i].ulen + 1));

			if(crcmode){
			  sprintf(qbuf,"INSERT INTO %s (ref_id,url_id,word_id,intag) VALUES(%s%i%s,%s%i%s,%d,%d)",
				  table, qu, referrer, qu, qu, Doc->CrossWords.CrossWord[i].referree_id, qu,
				  DpsStrHash32(lcsword), weight);
			}else{
			  /* Escape text to track it  */
			  (void)DpsDBEscStr(db, word_escaped, lcsword, dps_strlen(lcsword));

			  sprintf(qbuf,"INSERT INTO %s (ref_id,url_id,word,intag) VALUES(%s%i%s,%s%i%s,'%s',%d)",
				  table, qu, referrer, qu, qu, Doc->CrossWords.CrossWord[i].referree_id, qu,
				  word_escaped, weight);
			}
			if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf))){
				DpsDocFree(&U);
				goto unlock_DpsStoreCrossWords;
			}
		}
	}

unlock_DpsStoreCrossWords:
	/* COMMIT/UNLOCK */
	if(0){
		switch(db->DBType){
			case DPS_DB_MYSQL:
				rc=DpsSQLAsyncQuery(db,NULL,"UNLOCK TABLES");
				break;
			case DPS_DB_PGSQL:
			case DPS_DB_ORACLE7:
			case DPS_DB_ORACLE8:
			case DPS_DB_SAPDB:
				db->commit_fl = 0;
				rc=DpsSQLAsyncQuery(db,NULL,"COMMIT");
				break;
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_DB2)
		        case DPS_DB_SOLID:
			case DPS_DB_VIRT:
			case DPS_DB_DB2:
				rc=DpsSQLAsyncQuery(db,NULL,"DPS_COMMIT");
#endif
			default:
				db->commit_fl = 0;
				break;
		}
	}
	DpsDocFree(&U);
	DpsURLFree(&docURL);
	DpsFree(word_escaped); DpsFree(lcsword);
	return rc;
}




/************************ URLs ***********************************/


static int DpsAddURL(DPS_AGENT *Indexer, DPS_DOCUMENT * Doc, DPS_DB *db) {
	char		*e_url, *qbuf;
	urlid_t		next_url_id = 0, rec_id = 0, crc32_rec_id;
	DPS_SQLRES	SQLRes;
	const char	*url;
	int		url_seed, updated = 1, hops, old_hops;
	int		rc = DPS_OK;
	size_t          len;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";

	DpsSQLResInit(&SQLRes);
	url = DpsVarListFindStr(&Doc->Sections,"URL","");
	len = dps_strlen(url);
	e_url = DpsVarListFindStr(&Doc->Sections, "E_URL", NULL);
	qbuf = (char*)DpsMalloc(24 * len + 512);
	if (qbuf == NULL) return DPS_ERROR;
	
	if (e_url == NULL) {

	  DpsFindURL(Indexer, Doc, db);
	  e_url = DpsVarListFindStr(&Doc->Sections, "E_URL", NULL);

	}
	rec_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	old_hops = (urlid_t)DpsVarListFindInt(&Doc->Sections, "hops", 0);
	url_seed = (crc32_rec_id = (urlid_t)DpsStrHash32(e_url)) & 0xFFF /*& 0xFF*/;
	if (!(Indexer->flags & DPS_FLAG_FAST_HREF_CHECK)) {
	  DpsVarListReplaceInt(&Doc->Sections, "Site_id", DpsServerGetSiteId(Indexer, Doc->Server ? Doc->Server : Indexer->Conf->Cfg_Srv, Doc));
	}

	if (rec_id == 0 || Indexer->Flags.use_crc32_url_id) {

	  if(Indexer->Flags.use_crc32_url_id) {
	    /* Auto generation of rec_id */
	    /* using CRC32 algorithm     */
	    rec_id = crc32_rec_id;
		
	    dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (rec_id,url,referrer,hops,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank,since,charset_id) VALUES (%s%i%s,'%s',%s%i%s,%d,0,%d,0,%d,%d,%s%i%s,%s%i%s,%s%i%s,%li,0,%s,%d,%d)",
			 qu, rec_id, qu,
			 e_url,
			 qu, DpsVarListFindInt(&Doc->Sections,"Referrer-ID",0), qu,
			 DpsVarListFindInt(&Doc->Sections,"Hops",0),
			 DpsVarListFindInt(&Doc->Sections, "Next-Index-Time", (int)Indexer->now),
			 url_seed, (int)Indexer->now,
			 qu, DpsVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
			 qu, DpsVarListFindInt(&Doc->Sections, "Server_id", 0), qu,
			 qu, DpsVarListFindInt(&Doc->Sections, "Content-Length", 0), qu,
			 DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, (Indexer->Flags.use_date_header == 2) ? "Date" : "Last-Modified", 
					      (Indexer->Flags.use_date_header) ? DpsVarListFindStr(&Doc->Sections, "Date", "") : "")),
			 DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "0.25")),
			 (int)Indexer->now, Doc->charset_id
			 );

	    /* Exec INSERT now */
	    if(DPS_OK!=(rc=DpsSQLAsyncQuery(db, NULL, qbuf))) {
	      DPS_FREE(qbuf); return rc;
	    }
	  } else {
	    /* Use dabatase generated rec_id */
	    /* It depends on used DBType     */
	    updated = 0;

	    switch(db->DBType){
	    case DPS_DB_MSQL:
		/* miniSQL has _seq as autoincrement value */
		if (DPS_OK != (rc = DpsSQLQuery(db,&SQLRes,"SELECT _seq FROM url"))) {
		    DPS_FREE(qbuf); return rc;
		}
	      next_url_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLRes,0,0));
	      DpsSQLFree(&SQLRes);
	      dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,rec_id,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank,since,charset_id) VALUES ('%s',%i,%d,%i,0,%d,0,%d,%d,%i,%i,%i,%li,0,%s,%d,%d)",
			   e_url,
			   DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0),
			   DpsVarListFindInt(&Doc->Sections,"Hops",0),
			   next_url_id,
			   DpsVarListFindInt(&Doc->Sections, "Next-Index-Time", (int)Indexer->now),
			   url_seed, (int)Indexer->now,
			   DpsVarListFindInt(&Doc->Sections, "Site_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Server_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Content-Length", 0),
			   DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, (Indexer->Flags.use_date_header == 2) ? "Date" : "Last-Modified", 
						(Indexer->Flags.use_date_header) ? DpsVarListFindStr(&Doc->Sections, "Date", "") : "")),
			   DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "0.25")),
			   (int)Indexer->now, Doc->charset_id
			   );
	      break;

	    case DPS_DB_SOLID:
	    case DPS_DB_ORACLE7:
	    case DPS_DB_ORACLE8:
	    case DPS_DB_SAPDB:
	      /* FIXME: Dirty hack for stupid too smart databases 
		 Change this for config parameter checking */
/*			if (dps_strlen(e_url)>DPS_URLSIZE)e_url[DPS_URLSIZE]=0;*/
	      /* Use sequence next_url_id.nextval */
	      dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,rec_id,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank,since,charset_id) VALUES ('%s',%i,%d,next_url_id.nextval,0,%d,0,%d,%d,%i,%i,%i,%li,0,%s,%d,%d)",
			   e_url,
			   DpsVarListFindInt(&Doc->Sections,"Referrer-ID",0),
			   DpsVarListFindInt(&Doc->Sections,"Hops",0),
			   DpsVarListFindInt(&Doc->Sections, "Next-Index-Time", (int)Indexer->now),
			   url_seed, (int)Indexer->now,
			   DpsVarListFindInt(&Doc->Sections, "Site_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Server_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Content-Length", 0),
			   DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, (Indexer->Flags.use_date_header == 2) ? "Date" : "Last-Modified", 
						(Indexer->Flags.use_date_header) ? DpsVarListFindStr(&Doc->Sections, "Date", "") : "")),
			   DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "0.25")),
			   (int)Indexer->now, Doc->charset_id
			   );
	      break;
	    case DPS_DB_MIMER:
	      dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,rec_id,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank,since,charset_id) VALUES ('%s',%i,%d,NEXT_VALUE OF rec_id_GEN,0,%d,0,%d,%d,%i,%i,%i,%li,0,%s,%d,%d)",
			   e_url,
			   DpsVarListFindInt(&Doc->Sections,"Referrer-ID",0),
			   DpsVarListFindInt(&Doc->Sections,"Hops",0),
			   DpsVarListFindInt(&Doc->Sections, "Next-Index-Time", (int)Indexer->now),
			   url_seed, (int)Indexer->now,
			   DpsVarListFindInt(&Doc->Sections, "Site_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Server_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Content-Length", 0),
			   DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, (Indexer->Flags.use_date_header == 2) ? "Date" : "Last-Modified", 
						(Indexer->Flags.use_date_header) ? DpsVarListFindStr(&Doc->Sections, "Date", "") : "")),
			   DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "0.25")),
			   (int)Indexer->now, Doc->charset_id
			   );
	      break;		
	    case DPS_DB_IBASE:
	      dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,rec_id,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank,since,charset_id) VALUES ('%s',%i,%d,GEN_ID(rec_id_GEN,1),0,%d,0,%d,%d,%i,%i,%i,%li,0,%s,%d,%d)",
			   e_url,
			   DpsVarListFindInt(&Doc->Sections,"Referrer-ID",0),
			   DpsVarListFindInt(&Doc->Sections,"Hops",0),
			   DpsVarListFindInt(&Doc->Sections, "Next-Index-Time", (int)Indexer->now),
			   url_seed, (int)Indexer->now,
			   DpsVarListFindInt(&Doc->Sections, "Site_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Server_id", 0),
			   DpsVarListFindInt(&Doc->Sections, "Content-Length", 0),
			   DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, (Indexer->Flags.use_date_header == 2) ? "Date" : "Last-Modified", 
						(Indexer->Flags.use_date_header) ? DpsVarListFindStr(&Doc->Sections, "Date", "") : "")),
			   DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "0.25")),
			   (int)Indexer->now, Doc->charset_id
			   );
	      break;
	    case DPS_DB_MYSQL:
	      /* MySQL generates itself */
	    default:	
	      dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank,since,charset_id) VALUES ('%s',%s%i%s,%d,0,%d,0,%d,%d,%s%i%s,%s%i%s,%s%i%s,%li,0,%s,%d,%d)",
			   e_url,
			   qu, DpsVarListFindInt(&Doc->Sections,"Referrer-ID",0), qu,
			   DpsVarListFindInt(&Doc->Sections,"Hops",0),
			   DpsVarListFindInt(&Doc->Sections, "Next-Index-Time", (int)Indexer->now),
			   url_seed, (int)Indexer->now,
			   qu, DpsVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
			   qu, DpsVarListFindInt(&Doc->Sections, "Server_id", 0), qu,
			   qu, DpsVarListFindInt(&Doc->Sections, "Content-Length", 0), qu,
			   DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, (Indexer->Flags.use_date_header == 2) ? "Date" : "Last-Modified", 
						(Indexer->Flags.use_date_header) ? DpsVarListFindStr(&Doc->Sections, "Date", "") : "")),
			   DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "0.25")),
			   (int)Indexer->now, Doc->charset_id
			   );
	    }

	    /* Exec INSERT now */
	    if(DPS_OK!=(rc=DpsSQLAsyncQuery(db, NULL, qbuf))) {
	      DPS_FREE(qbuf); return rc;
	    }
	    if ((!Indexer->Flags.use_crc32_url_id) && Indexer->Flags.collect_links) {
	      dps_snprintf(qbuf, 4 * len + 512, "SELECT rec_id FROM url WHERE url='%s'", e_url);
	      if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		DPS_FREE(qbuf); return rc;
	      }
	      rc = DpsSQLNumRows(&SQLRes);
	      if (rc > 0) rec_id = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
	      else rec_id = 0;
	      DpsSQLFree(&SQLRes);
	    }
	  }
	}

	hops = DpsVarListFindInt(&Doc->Sections, "Hops", 0);
	if (updated && (rec_id != 0) && ((hops == 0 && hops != old_hops) || (Indexer->Flags.track_hops && old_hops > hops))) {
	  dps_snprintf(qbuf, 4 * len + 512, "UPDATE url SET hops=%s%s%s, referrer=%s%s%s WHERE rec_id=%s%d%s AND (referrer=%s%s%s OR referrer=%s-1%s)",
		       qu, DpsVarListFindStr(&Doc->Sections, "Hops", "0"), qu,
		       qu, DpsVarListFindStr(&Doc->Sections,"Referrer-ID", "0"), qu,
		       qu, rec_id, qu,
		       qu, DpsVarListFindStr(&Doc->Sections,"Referrer-ID", "0"), qu,
		       qu, qu
		       );
	  if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) {
	    DPS_FREE(qbuf); return rc;
	  }
	}

	if (Indexer->Flags.collect_links) {
	  urlid_t ot = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);

	  if (rec_id != 0 && ot != 0) {
	    const char *weight = DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "1.0"));
	    int	      skip_same_site = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "PopRankSkipSameSite", DPS_POPRANKSKIPSAMESITE), "yes");
	    int is_not_same_site = (rec_id == ot);
/*	    urlid_t k = rec_id;*/

	    DpsVarListReplaceInt(&Doc->Sections, "DP_ID", rec_id);
	    if (skip_same_site && !is_not_same_site) {
	      urlid_t site_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "Site_id", 0);
	      urlid_t ot_site_id = 0;
	      if (site_id == 0) {
		site_id = (urlid_t)DpsServerGetSiteId(Indexer, Doc->Server ? Doc->Server : Indexer->Conf->Cfg_Srv, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Site_id", (int)site_id);
	      }
	      if (site_id != 0) {
		dps_snprintf(qbuf, 4 * len + 512, "SELECT site_id FROM url WHERE rec_id=%s%i%s", qu, ot, qu);

		if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		  DPS_FREE(qbuf); return rc;
		}
		if (DpsSQLNumRows(&SQLRes)) ot_site_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
		DpsSQLFree(&SQLRes);
		is_not_same_site = (site_id != ot_site_id);
	      } else is_not_same_site = 1; /* treat any new doc as one from different site */
	    } else is_not_same_site = 1;
#if 0
	      if (1 /*ot != rec_id*/ /*k*/) {
		dps_snprintf(qbuf, 4 * len + 512, "SELECT count(*) FROM links WHERE ot=%s%i%s AND k=%s%i%s",  
			     qu, rec_id, qu, qu, rec_id, qu);

		if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		  DPS_FREE(qbuf); return rc;
		}
		rc = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
		DpsSQLFree(&SQLRes);

		if (rc == 0) {
		  dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO links (ot,k,weight,valid) VALUES (%s%i%s,%s%i%s,%s%s%s,'t')",
			       qu, rec_id, qu,  qu, rec_id, qu,  qu, weight, qu);

		  if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) {
		    DPS_FREE(qbuf); return rc;
		  }
		}
	      }
#endif
	    if (is_not_same_site) {

		dps_snprintf(qbuf, 4 * len + 512, "SELECT count(*) FROM links WHERE ot=%s%i%s AND k=%s%i%s",  
			     qu, ot, qu, qu, rec_id, qu);

		if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		  DPS_FREE(qbuf); return rc;
		}
		rc = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
		DpsSQLFree(&SQLRes);

		if (rc == 0) {
		  dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO links (ot,k,weight,valid) VALUES (%s%i%s,%s%i%s,%s%s%s,'t')",
			       qu, ot, qu,  qu, rec_id, qu,  qu, weight, qu);
		} else {
		  dps_snprintf(qbuf, 4 * len + 512, "UPDATE links SET valid='t' WHERE ot=%s%i%s AND k=%s%i%s",
			       qu, ot, qu,  qu, rec_id, qu);
		}
		if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) {
		  DPS_FREE(qbuf); return rc;
		}
	    } else {
/*	      DpsLog(Indexer, DPS_LOG_ERROR, "AddURL: URL not found: %s", e_url);*/
	    }
	  }
	}
	DPS_FREE(qbuf);
	return DPS_OK;
}


static int DpsAddLink(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db) {
	char		*e_url, *lc_url = NULL, *qbuf;
	DPS_SQLRES	SQLRes;
	const char	*url;
	int		rc = DPS_OK, need_free_e_url = 0;
	size_t          len;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        dc_lc;
	urlid_t k;

	doccs = DpsGetCharSetByID(Doc->charset_id);
	if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
	if (doccs == NULL) {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Document charset is undefined (or unknown)");
	    return DPS_ERROR;
	}
	loccs = Doc->lcs;
	if(!loccs) loccs = Indexer->Conf->lcs;
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");
	if (loccs == NULL) {
	    DpsLog(Indexer, DPS_LOG_ERROR, "Local charset is undefined (or unknown)");
	    return DPS_ERROR;
	}

	DpsSQLResInit(&SQLRes);

	url = DpsVarListFindStr(&Doc->Sections,"URL","");
	len = dps_strlen(url);
	e_url = DpsVarListFindStr(&Doc->Sections, "E_URL", NULL);
	qbuf = (char*)DpsMalloc(24 * len + 512);
	if (qbuf == NULL) return DPS_ERROR;
	if (!(Indexer->flags & DPS_FLAG_FAST_HREF_CHECK)) {
	  DpsVarListReplaceInt(&Doc->Sections, "Site_id", DpsServerGetSiteId(Indexer, Doc->Server ? Doc->Server : Indexer->Conf->Cfg_Srv, Doc));
	}

	if (e_url == NULL) {

	  e_url = (char*)DpsMalloc(24 * len + 1);
	  if (e_url == NULL) { DPS_FREE(qbuf); return DPS_ERROR; }
	  lc_url = (char*)DpsMalloc(24 * len + 1);
	  if (lc_url == NULL) { DPS_FREE(qbuf); DPS_FREE(e_url); return DPS_ERROR; }
	  need_free_e_url = 1;
	  DpsConvInit(&dc_lc, doccs, loccs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);
	  /* Convert URL to LocalCharset */
	  DpsConv(&dc_lc, lc_url, 24 * len,  url, len + 1);
	  /* Escape URL string */
	  (void)DpsDBEscStr(db, e_url, lc_url, dps_strlen(lc_url));
	  DpsVarListAddStr(&Doc->Sections, "E_URL", e_url);
	}
	
	dps_snprintf(qbuf, 4 * len + 512, "SELECT rec_id FROM url WHERE url='%s'", e_url);
	if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
	  DpsLog(Indexer, DPS_LOG_ERROR, "LocalCharset: %s, DocCharset: %s", loccs->name, doccs->name);
	  DPS_FREE(qbuf); if (need_free_e_url) DPS_FREE(e_url); DPS_FREE(lc_url); return rc;
	}
	rc = DpsSQLNumRows(&SQLRes);
	if (rc > 0) k = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));

	DpsSQLFree(&SQLRes);

	if (rc != 0) {
	    urlid_t ot = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
	    const char *weight = DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "weight", "1.0"));
	    int	      skip_same_site = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "PopRankSkipSameSite", DPS_POPRANKSKIPSAMESITE), "yes");
	    int is_not_same_site = (ot == k);

	    DpsVarListReplaceInt(&Doc->Sections, "DP_ID", k);
	    if (skip_same_site && !is_not_same_site) {
	      urlid_t site_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "Site_id", 0);
	      urlid_t ot_site_id = 0;
	      if (site_id == 0) {
		site_id = (urlid_t)DpsServerGetSiteId(Indexer, Doc->Server ? Doc->Server : Indexer->Conf->Cfg_Srv, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Site_id", (int)site_id);
	      }
	      if (site_id != 0) {
		dps_snprintf(qbuf, 4 * len + 512, "SELECT site_id FROM url WHERE rec_id=%s%i%s", qu, ot, qu);

		if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		  DPS_FREE(qbuf); if (need_free_e_url) DPS_FREE(e_url); DPS_FREE(lc_url); return rc;
		}
		if (DpsSQLNumRows(&SQLRes)) ot_site_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
		DpsSQLFree(&SQLRes);
		is_not_same_site = (site_id != ot_site_id);
	      } else is_not_same_site = 1; /* treat any new doc as one from different site */
	    } else is_not_same_site = 1;
#if 1
	    if (ot != k) {
		dps_snprintf(qbuf, 4 * len + 512, "SELECT count(*) FROM links WHERE ot=%s%i%s AND k=%s%i%s",  qu, k, qu, qu, k, qu);

		if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		  DPS_FREE(qbuf); if (need_free_e_url) DPS_FREE(e_url); DPS_FREE(lc_url); return rc;
		}
		rc = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
		DpsSQLFree(&SQLRes);

		if (rc == 0) {
		  dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO links (ot,k,weight) VALUES (%s%i%s,%s%i%s,%s%s%s)",
			       qu, k, qu,  qu, k, qu,  qu, weight, qu);
		  if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) {
		    DPS_FREE(qbuf); if (need_free_e_url) DPS_FREE(e_url); DPS_FREE(lc_url); return rc;
		  }
		}
	    }
#endif
	    if (is_not_same_site) {

		dps_snprintf(qbuf, 4 * len + 512, "SELECT count(*) FROM links WHERE ot=%s%i%s AND k=%s%i%s", qu, ot, qu, qu, k, qu);

		if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		  DPS_FREE(qbuf); if (need_free_e_url) DPS_FREE(e_url); DPS_FREE(lc_url); return rc;
		}
		rc = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
		DpsSQLFree(&SQLRes);

		if (rc == 0) {
		  dps_snprintf(qbuf, 4 * len + 512, "INSERT INTO links (ot,k,weight) VALUES (%s%i%s,%s%i%s,%s%s%s)",
			       qu, ot, qu, qu, k, qu,  qu, weight, qu);
		} else {
		  dps_snprintf(qbuf, 4 * len + 512, "UPDATE links SET valid='t' WHERE ot=%s%i%s AND k=%s%i%s",
			       qu, ot, qu,  qu, k, qu);
		}
		if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) {
		  DPS_FREE(qbuf); if (need_free_e_url) DPS_FREE(e_url); DPS_FREE(lc_url); return rc;
		}
	    } else {
/*	      DpsLog(Indexer, DPS_LOG_DEBUG, "AddLink: URL not found: %s", e_url);*/
	    }
	}
	DPS_FREE(qbuf); if (need_free_e_url) DPS_FREE(e_url); DPS_FREE(lc_url);
	return DPS_OK;
}



static int DpsDeleteURL(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc,DPS_DB *db);

static int DpsDeleteBadHrefs(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db) {
	DPS_DOCUMENT	rDoc;
	DPS_SQLRES	SQLRes;
	char		q[256];
	size_t		i;
	size_t		nrows;
	int		rc=DPS_OK;
	int		hold_period=DpsVarListFindInt(&Doc->Sections,"HoldBadHrefs",0);
	urlid_t		url_id = DpsVarListFindInt(&Doc->Sections,"DP_ID",0);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	const char      *url;
	char            *dc_url;
	size_t          len;
	int             prev_id = -1;
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        lc_dc;
	
#if defined(WITH_TRACE) && defined(DEBUG)
	fprintf(Indexer->TR, "[%d] DpsDeleteBadHrefs\n", Indexer->handle);
	fflush(Indexer->TR);
#endif
	if (hold_period == 0) return rc;

	DpsSQLResInit(&SQLRes);

	loccs = Indexer->Conf->lcs;
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");

	dps_snprintf(q, sizeof(q), 
		  "SELECT o.rec_id,o.url,o.charset_id FROM url o, links l WHERE o.status > 399 AND o.status < 2000 AND l.k=%s%i%s AND l.ot=o.rec_id AND o.bad_since_time<%s%d%s",
		     qu, url_id, qu, qu, (int)Indexer->now - hold_period, qu);
	if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLRes,q)))return rc;
	
	nrows = DpsSQLNumRows(&SQLRes);
	
	DpsDocInit(&rDoc);
	for(i = 0; i < nrows ; i++) {
	  urlid_t	rec_id = DPS_ATOI(DpsSQLValue(&SQLRes, i, 0));

	        rDoc.charset_id = DPS_ATOI(DpsSQLValue(&SQLRes, i, 2));

		if (rDoc.charset_id != prev_id) {
		  doccs = DpsGetCharSetByID(prev_id = rDoc.charset_id);
		  if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
		  DpsConvInit(&lc_dc, loccs, doccs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);
		}	  
		len = dps_strlen(url = DpsSQLValue(&SQLRes,i,1));
		dc_url = (char*)DpsMalloc((size_t)(24 * len + 1));
		if (dc_url == NULL) continue;
		/* Convert URL from LocalCharset */
		DpsConv(&lc_dc, dc_url, (size_t)24 * len,  url, (size_t)(len + 1));
		DpsVarListReplaceStr(&rDoc.Sections, "URL", dc_url);

		DpsVarListDel(&rDoc.Sections, "URL_ID");
		DPS_FREE(dc_url);

		DpsVarListReplaceStr(&rDoc.Sections, "DP_ID", DpsSQLValue(&SQLRes,i,0));
#if defined(WITH_TRACE) && defined(DEBUG)
		fprintf(Indexer->TR, "[%d]   DpsDeleteBadHref: %d\n", Indexer->handle, rec_id);
		fflush(Indexer->TR);
#endif
		if(db->DBMode == DPS_DBMODE_CACHE)
		  if (DpsDeleteURLFromCache(Indexer, rec_id, db) != DPS_OK) {
			break;
		  }
		if(DPS_OK!=(rc=DpsDeleteURL(Indexer, &rDoc, db)))
			break;
	}
	DpsDocFree(&rDoc);
	DpsSQLFree(&SQLRes);
	return rc;
}

static int DpsDeleteURL(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db) {
	char	qbuf[128];
	int	rc;
	urlid_t	url_id  =DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	int	use_crosswords, collect_links;
	const char *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	use_crosswords = ((Indexer->Flags.use_crosswords > 0) && (db->DBMode != DPS_DBMODE_CACHE));
	collect_links  = Indexer->Flags.collect_links;

	if (DPS_OK != (rc = DpsExecActions(Indexer, Doc, 'd'))) return rc;

	if(use_crosswords /*&& db->DBMode!=DPS_DBMODE_CACHE*/) {
		if(DPS_OK!=(rc=DpsDeleteCrossWordsFromURL(Indexer,Doc,db)))return(rc);
		if(DPS_OK!=(rc=DpsDeleteCrossWordsToURL(Indexer,Doc,db)))return(rc);
	}
	
	if (db->DBMode != DPS_DBMODE_CACHE) /* for DBMode cache it's already deleted early */
	  if (DPS_OK != (rc = DpsDeleteWordFromURL(Indexer, Doc, db))) return rc;
	
	if (collect_links) {
	
	  sprintf(qbuf, "DELETE FROM links WHERE ot=%s%i%s", qu, url_id, qu);
	  if (DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) return rc;
	
	  sprintf(qbuf, "DELETE FROM links WHERE k=%s%i%s", qu, url_id, qu);
	  if (DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) return rc;
	}

/*	if (Indexer->Flags.URLInfoSQL) {*/
	  sprintf(qbuf,"DELETE FROM urlinfo WHERE url_id=%s%i%s", qu, url_id, qu);
	  if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))return rc;
/*	}*/

	sprintf(qbuf,"DELETE FROM url WHERE rec_id=%s%i%s", qu, url_id, qu);
	if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))return rc;
	
	DpsStoreDeleteDoc(Indexer, Doc);

	/* remove all old broken hrefs from this document to avoid broken link collecting */
	/* if (DPS_OK != (rc=DpsDeleteBadHrefs(Indexer,Doc, db))) return rc;*/

	sprintf(qbuf,"UPDATE url SET referrer=%s-1%s WHERE referrer=%s%i%s", qu, qu, qu, url_id, qu);
	return DpsSQLAsyncQuery(db,NULL,qbuf);
}

static int DpsDeleteLinks(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db){
	char	qbuf[256];
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	const char *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	int rc;

	dps_snprintf(qbuf, sizeof(qbuf), "DELETE FROM links WHERE ot=%s%i%s AND valid='f'", qu, url_id, qu);
	rc = DpsSQLAsyncQuery(db, NULL, qbuf);
/*	if (db->DBSQL_SUBSELECT) {
	  dps_snprintf(qbuf, sizeof(qbuf), "DELETE FROM links WHERE (k,ot) IN (SELECT k,ot FROM links l, url u1, url u2 WHERE l.k=%s%i%s AND l.ot=u1.rec_id AND u2.rec_id=%s%i%s AND u1.site_id=u2.site_id AND l.k!=l.ot)", qu, url_id, qu, qu, url_id, qu);
	  rc = DpsSQLAsyncQuery(db, NULL, qbuf);
	}*/
	return rc;
}

static int DpsLinksMarkToDelete(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db){
	char	qbuf[128];
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	const char *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";

	sprintf(qbuf,"UPDATE links SET valid='f' WHERE ot=%s%i%s AND ot != k", qu, url_id, qu);
	return DpsSQLAsyncQuery(db, NULL, qbuf);
}

static int DpsMarkForReindex(DPS_AGENT *Indexer,DPS_DB *db){
	char		qbuf[1024];
	char            *ubuf;
	const char	*where;
	DPS_SQLRES 	SQLRes;
	size_t dump_num = (size_t)DpsVarListFindUnsigned(&Indexer->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	size_t          i, j, nrec, ulen = 1024 + 512 * 32;
	unsigned long offset;
	int             rc, u;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	urlid_t         rec_id;

	DpsSQLResInit(&SQLRes);

	where = BuildWhere(Indexer, db);
	if (where == NULL) return DPS_ERROR;
	
	if (db->DBSQL_SUBSELECT) {
	  dps_snprintf(qbuf,sizeof(qbuf),"UPDATE url SET next_index_time=%d WHERE rec_id IN (SELECT url.rec_id FROM url%s %s %s)",
		   (int)Indexer->now, db->from, (where[0]) ? "WHERE" : "", where);
	  return DpsSQLAsyncQuery(db,NULL,qbuf);
	}

	if ((ubuf = (char*)DpsMalloc(ulen)) == NULL) {
	  DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc %d bytes at "__FILE__":%d", ulen, __LINE__);
	  return DPS_ERROR;
	}

	if (!db->DBSQL_LIMIT) {
	  dps_snprintf(qbuf, sizeof(qbuf), "SELECT MIN(rec_id) FROM url");
	  rc = DpsSQLQuery(db, &SQLRes, qbuf);
	  if(DPS_OK != rc) {
	    DPS_FREE(ubuf);
	    return rc;
	  }
	  if (DpsSQLNumRows(&SQLRes) < 1) {
	    DpsSQLFree(&SQLRes);
	    DPS_FREE(ubuf);
	    return DPS_OK;
	  }
	  rec_id = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0)) - 1;
	}

	u = 1;
	offset = 0;
	while (u) {

	  if (db->DBSQL_LIMIT) {

	    dps_snprintf(qbuf, sizeof(qbuf), "SELECT url.rec_id FROM url%s %s %s ORDER BY url.rec_id LIMIT %d OFFSET %ld", 
			 db->from, (where[0]) ? "WHERE" : "", where, dump_num, offset);
	    if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
	      DPS_FREE(ubuf);
	      return rc;
	    }
	    rec_id = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
	  } else { /* DBSQL_LIMIT */

	    dps_snprintf(qbuf, sizeof(qbuf), "SELECT MIN(rec_id),COUNT(*) FROM url WHERE rec_id>%d", rec_id);
	    rc = DpsSQLQuery(db, &SQLRes, qbuf);
	    if(DPS_OK != rc) {
	      DPS_FREE(ubuf);
	      return rc;
	    }
	    if ( ( u = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 1))) > 0) {
	      rec_id = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT url.rec_id FROM url%s %s %s %s url.rec_id>=%d AND url.rec_id<=(%d+%d) ORDER BY url.rec_id", 
			   db->from, (where[0]) ? "WHERE" : "", where, (where[0]) ? "AND" : "WHERE", rec_id, rec_id, dump_num);
	      if(DPS_OK != (rc = DpsSQLQuery(db, &SQLRes, qbuf))) {
		DPS_FREE(ubuf);
		return rc;
	      }
	    }
	    
	  }

	  nrec = (u) ? DpsSQLNumRows(&SQLRes) : 0;

	  if (db->DBSQL_IN) {
	    for (i = 0; i < nrec; i += 512) {
	      sprintf(ubuf, "UPDATE url SET next_index_time=%d WHERE rec_id IN (", (int)Indexer->now);
	      for (j = 0; (j < 512) && (i + j < nrec); j++) {
		sprintf(DPS_STREND(ubuf), "%s%s%s%s", (j) ? "," : "", qu, DpsSQLValue(&SQLRes, i + j, 0), qu);
	      }
	      sprintf(DPS_STREND(ubuf), ")");
	      if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, ubuf))) {
		DpsSQLFree(&SQLRes);
		DPS_FREE(ubuf);
		return rc;
	      }
	    }
	  } else {
	    for (i = 0; i < nrec; i++) {
	      sprintf(ubuf, "UPDATE url SET next_index_time=%d WHERE rec_id=%s", (int)Indexer->now,  DpsSQLValue(&SQLRes, i, 0));
	      if(DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, ubuf))) {
		DpsSQLFree(&SQLRes);
		DPS_FREE(ubuf);
		return rc;
	      }
	    }
	  }
	  if (db->DBSQL_LIMIT) {
	    u = (nrec == dump_num);
	  } else {
	    if (u) {
	      u = (rec_id != DPS_ATOI(DpsSQLValue(&SQLRes, nrec - 1, 0)));
	      rec_id = DPS_ATOI(DpsSQLValue(&SQLRes, nrec - 1, 0));
	    }
	  }
	  DpsSQLFree(&SQLRes);
	  offset += nrec;
	}
	DPS_FREE(ubuf);
	return DPS_OK;
}

static int DpsUpdateUrl(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc,DPS_DB *db){
	char	qbuf[256]="";
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	int	status=DpsVarListFindInt(&Doc->Sections,"Status",0);
	int	prevStatus = DpsVarListFindInt(&Doc->Sections, "PrevStatus", 0);
	time_t	next_index_time = (time_t)DPS_ATOU(DpsVarListFindStr(&Doc->Sections, "Next-Index-Time", ""));
	int	res;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
#ifdef HAVE_ORACLE8_OFF
	if(db->DBDriver==DPS_DB_ORACLE8){
	  if ( (prevStatus != status) && (status > 300) && (status != 304) && (status < 2000) ) {
		sprintf(qbuf,"UPDATE url SET status=:1,next_index_time=:2,bad_since_time=:4, site_id=%i, server_id=%i WHERE rec_id=:3",
			DpsVarListFindInt(&Doc->Sections, "Site_id",0), DpsVarListFindInt(&Doc->Sections, "Server_id",0));
		param_init(db, 1, 2, 3, 4);
		param_add(db, status ? status : prevStatus, next_index_time, url_id, Indexer->now);
	  } else {
		sprintf(qbuf,"UPDATE url SET status=:1,next_index_time=:2, site_id=%i, server_id=%i WHERE rec_id=:3",
			DpsVarListFindInt(&Doc->Sections, "Site_id",0), DpsVarListFindInt(&Doc->Sections, "Server_id",0));
		param_init(db, 1, 2, 3, 0);
		param_add(db, status ? status : prevStatus, next_index_time, url_id, 0);
	  }
	}
#endif
	if(!qbuf[0]) {
	  if ( (prevStatus != status) && (status > 300) && (status != 304) && (status < 2000) )
		sprintf(qbuf, "UPDATE url SET status=%d,next_index_time=%u,bad_since_time=%d,site_id=%s%i%s,server_id=%s%i%s,pop_rank=%s%s%s WHERE rec_id=%s%i%s",
			status ? status : prevStatus, (unsigned int)next_index_time, (int)Indexer->now, 
			qu, DpsVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
			qu, DpsVarListFindInt(&Doc->Sections, "Server_id",0), qu, 
			qu, DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "Pop_Rank","0.25")), qu, 
			qu, url_id, qu);
	  else
		sprintf(qbuf,"UPDATE url SET status=%d,next_index_time=%u,site_id=%s%i%s,server_id=%s%i%s,pop_rank=%s%s%s WHERE rec_id=%s%i%s",
			status ? status: prevStatus, (unsigned int)next_index_time, 
			qu, DpsVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
			qu, DpsVarListFindInt(&Doc->Sections, "Server_id",0), qu, 
			qu, DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "Pop_Rank","0.25")), qu, 
			qu, url_id, qu);
	}

	if(DPS_OK!=(res=DpsSQLAsyncQuery(db,NULL,qbuf)))return res;
	
	if ((status >= 200 && status < 400) || (status >= 2200 && status <= 2304) || (status == 0)) {
	  const char *method = DpsVarListFindStr(&Indexer->Vars, "PopRankMethod", "Goo");
	  if ((Indexer->Flags.poprank_postpone == 0) && (Indexer->Flags.collect_links) && (strcasecmp(method, "Neo") == 0)) {
	    int	      skip_same_site = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "PopRankSkipSameSite", DPS_POPRANKSKIPSAMESITE), "yes");
	    int	      detect_clones = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "DetectClones", DPS_DETECTCLONES), "yes");
	    size_t    url_num = (size_t)DpsVarListFindUnsigned(&Indexer->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	    res = DpsPopRankPasNeo(Indexer, db, DpsVarListFindStr(&Doc->Sections, "DP_ID", "0"), 
#ifdef WITH_POPHOPS
				   DpsVarListFindStr(&Doc->Sections, "Hops", "0"), 
#else
				   NULL,
#endif
				   skip_same_site, url_num, 0, detect_clones);
	  }
	}

	if (res != DPS_OK) return res;

	/* remove all old broken hrefs from this document to avoid broken link collecting */
	return DpsDeleteBadHrefs(Indexer,Doc,db);
}


static int DpsRegisterChild(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc,DPS_DB *db) {
	char	qbuf[1024];
	urlid_t	url_id = DpsVarListFindInt(&Doc->Sections,"DP_ID",0);
	urlid_t	parent_id = DpsVarListFindInt(&Doc->Sections,"Parent-ID",0);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	dps_snprintf(qbuf,sizeof(qbuf),"INSERT INTO links (ot,k,weight) VALUES (%s%i%s,%s%i%s,1.0)", qu, parent_id, qu, qu, url_id, qu);
	return DpsSQLAsyncQuery(db,NULL,qbuf);
}



static int  DpsUpdateClone(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db) {
	char	*qbuf;
	int	rc;
	const char	*charset;
	DPS_VAR		*var;
	int		status, prevStatus;
	const char      *url_id;
	size_t		i, len = 0;
	char		qsmall[64];
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
	prevStatus = DpsVarListFindInt(&Doc->Sections, "PrevStatus", 0);
	url_id = DpsVarListFindStr(&Doc->Sections, "DP_ID", "0");
	
#if 0
	{
		int a;
		char *l=DpsVarListFindStr(&Doc->Sections,"Last-Modified","");
		a=DpsHttpDate2Time_t(l);
		fprintf(stderr,"DDD='%s'\n%d\n",DpsVarListFindStr(&Doc->Sections,"Last-Modified","<NULL>"),a);
	}
#endif
	
	if((var=DpsVarListFind(&Doc->Sections,"Content-Language"))){
	        if (var->val == NULL) {
			var->val = (char*)DpsStrdup(DpsVarListFindStr(&Doc->Sections, "DefaultLang", "en"));
		}
		len = dps_strlen(var->val);
		for(i = 0; i < len; i++) {
		  var->val[i] = (char)dps_tolower((int)var->val[i]);
		}
	}
	
	charset = DpsVarListFindStr(&Doc->Sections, "Charset", 
				    DpsVarListFindStr(&Doc->Sections, "RemoteCharset", 
						      DpsVarListFindStr(&Doc->Sections, "URLCharset", "iso-8859-1")));
	charset = DpsCharsetCanonicalName(charset);
	DpsVarListReplaceStr(&Doc->Sections, "Charset", charset);
	
	if ( (prevStatus != status) && (status > 300) && (status != 304) && (status < 2000) )
	  dps_snprintf(qsmall, 64, ", bad_since_time=%d", (int)Indexer->now);
	else qsmall[0] = '\0';
	
	qbuf=(char*)DpsMalloc(1024);
	if (qbuf == NULL) return DPS_ERROR;

	dps_snprintf(qbuf, 1023, "\
UPDATE url SET \
status=%d,last_mod_time=%li,\
next_index_time=%s,\
docsize=%d,pop_rank=%s%s%s,\
crc32=%d%s, site_id=%s%i%s, server_id=%s%i%s \
WHERE rec_id=%s%s%s",
        status ? status : prevStatus,
	DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, (Indexer->Flags.use_date_header == 2) ? "Date" : "Last-Modified", 
					     (Indexer->Flags.use_date_header) ? DpsVarListFindStr(&Doc->Sections, "Date", "") : "")),
	DpsVarListFindStr(&Doc->Sections, "Next-Index-Time","0"),
	DpsVarListFindInt(&Doc->Sections,"Content-Length",0),
	qu, DpsDBEscDoubleStr(DpsVarListFindStr(&Doc->Sections, "Pop_Rank", "0.25")), qu,
	DpsVarListFindInt(&Doc->Sections,"crc32",0),
	qsmall,
	qu, DpsVarListFindInt(&Doc->Sections, "Site_id",0), qu,
	qu, DpsVarListFindInt(&Doc->Sections, "Server_id",0), qu,
	qu, url_id, qu);
	
	rc=DpsSQLAsyncQuery(db,NULL,qbuf);
	DPS_FREE(qbuf);

	if (rc != DPS_OK) return rc;

	/* remove all old broken hrefs from this document to avoid broken link collecting */
	if(DPS_OK != (rc = DpsDeleteBadHrefs(Indexer, Doc, db))) {
	  return rc;
	}
	
	if ((status >= 200 && status < 400) || (status >= 2200 && status <= 2304) || (status == 0)) {
	  const char *method = DpsVarListFindStr(&Indexer->Vars, "PopRankMethod", "Goo");
	  if ((Indexer->Flags.poprank_postpone == 0) && (Indexer->Flags.collect_links) && (strcasecmp(method, "Neo") == 0)) {
	    int	      skip_same_site = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "PopRankSkipSameSite", DPS_POPRANKSKIPSAMESITE), "yes");
	    int	      detect_clones = !strcasecmp(DpsVarListFindStr(&Indexer->Vars, "DetectClones", DPS_DETECTCLONES), "yes");
	    size_t    url_num = (size_t)DpsVarListFindUnsigned(&Indexer->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	    rc = DpsPopRankPasNeo(Indexer, db, url_id, 
#ifdef WITH_POPHOPS
				  DpsVarListFindStr(&Doc->Sections, "Hops", "0"), 
#else
				  NULL,
#endif
				  skip_same_site, url_num, 0, detect_clones);
	  }
	}
	return rc;
}


static int DpsLongUpdateURL(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, DPS_DB *db) {
  int		rc=DPS_OK, u;
	size_t		i, r;
	char		*qbuf;
	char		*arg;
	char		qsmall[128];
	size_t		len=0;
	urlid_t		url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	
	if(DPS_OK != (rc = DpsUpdateClone(Indexer, Doc, db)))
		return rc;

	if (Indexer->Flags.URLInfoSQL) DpsSQLBegin(db);
	
/*	if (Indexer->Flags.URLInfoSQL) {*/
	  sprintf(qsmall,"DELETE FROM urlinfo WHERE url_id=%s%i%s", qu, url_id, qu);
	  if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qsmall))) {
	    DpsSQLEnd(db);
	    return rc;
	  }
/*	}*/

/* No need to delete from links here, it has been done before */
	
	  len=0; 
	  {
	    size_t l;
	    for (r = 0; r < 256; r++)
	      for (i = 0; i < Doc->Sections.Root[r].nvars; i++) {
		l = Doc->Sections.Root[r].Var[i].curlen;
		if ((l == 0) && (Doc->Sections.Root[r].Var[i].val != NULL)) l += dps_strlen(Doc->Sections.Root[r].Var[i].val);
		l += (Doc->Sections.Root[r].Var[i].name) ? dps_strlen(Doc->Sections.Root[r].Var[i].name) : 0;
		if(len < l)
		  len = l;
	      }
	  }
	if(!len) {
	  if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
	  return DPS_OK;
	}
	
	qbuf=(char*)DpsMalloc(4 * len + 128);
	if (qbuf == NULL) {
	  if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
	  return DPS_ERROR;
	}
	arg=(char*)DpsMalloc(4 * len + 128);
	if (arg == NULL) {
	  DPS_FREE(qbuf);
	  if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
	  return DPS_ERROR;
	}
	
	switch(Doc->method) {
	case DPS_METHOD_UNKNOWN:
	case DPS_METHOD_GET:
	case DPS_METHOD_CHECKMP3:
	case DPS_METHOD_CHECKMP3ONLY:
	case DPS_METHOD_INDEX:
	  u = 0;
	  break;
	default:
	  u = 1;
	}

	if(db->DBSQL_MULTINSERT) { /* MySQL & PgSQL */
	  int have_inserts = 0;
	  char *qe;
	  size_t step = dps_max(4096, 4 * len + 128), mlen = 4 * len + 128, buflen;

	  sprintf(qbuf, "INSERT INTO urlinfo(url_id,sname,sval)VALUES");
	  qe = qbuf + dps_strlen(qbuf);

	  if (Indexer->Flags.URLInfoSQL) {
	    for (r = 0; r < 256; r++)
	      for (i = 0; i < Doc->Sections.Root[r].nvars; i++) {
		DPS_VAR *Sec = &Doc->Sections.Root[r].Var[i];

		if(!Sec->name || !Sec->val || (!Sec->val[0] && strcasecmp(Sec->name, "Z"))  ) continue;
		if (
		   !strcasecmp(Sec->name, "URL") ||
		   !strcasecmp(Sec->name, "DP_ID") ||
		   !strcasecmp(Sec->name, "Status") ||
		   !strcasecmp(Sec->name, "Z") ||
		   !strcasecmp(Sec->name, "Pop_Rank") ||
		   !strcasecmp(Sec->name, "Content-Length")
		   ) continue;
		if( ((/*Sec->section == 0 &&*/ Sec->maxlen == 0) || u) && 
		   strcasecmp(Sec->name, "Title") &&
		   strcasecmp(Sec->name, "Charset") &&
		   strcasecmp(Sec->name, "Content-Type") &&
		   strcasecmp(Sec->name, "Content-Language")
		    ) continue;

		buflen = qe - qbuf;
		if((buflen + len + 32) >= mlen){
		  mlen += step;
		  qbuf = (char*)DpsRealloc(qbuf, mlen);
		  if (qbuf == NULL) {
		    if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
		    DPS_FREE(arg);
		    return DPS_ERROR;
		  }
		  qe = qbuf + buflen;
		}
		if(have_inserts) dps_strcpy(qe++, ",");
		have_inserts++;

		arg = DpsDBEscStr(db, arg, Sec->val, dps_strlen(Sec->val));
		sprintf(qe, "(%s%i%s,'%s','%s')", qu, url_id, qu, dps_strtolower(Sec->name), arg);
		qe = qe + dps_strlen(qe);
		if((qe - qbuf) + 128 >= DPS_MAX_MULTI_INSERT_QSIZE) {
		  if (DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) {
		    if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
		    DPS_FREE(qbuf);
		    DPS_FREE(arg);
		    return rc;
		  }
		  /* Init query again */
		  sprintf(qbuf, "INSERT INTO urlinfo(url_id,sname,sval)VALUES");
		  qe = qbuf + dps_strlen(qbuf);
		  have_inserts = 0;
		}
	      }
	  } else if (u == 0) {
	    for (r = 0; r < 256; r++)
	      for (i = 0; i < Doc->Sections.Root[r].nvars; i++) {
		DPS_VAR *Sec = &Doc->Sections.Root[r].Var[i];

		if(!Sec->name || !Sec->val || (!Sec->val[0] && strcasecmp(Sec->name, "Z"))  ) continue;
		if( (Sec->maxlen == 0) || (
		   strcasecmp(Sec->name, "Charset") &&
		   strcasecmp(Sec->name, "Content-Type") &&
		   strcasecmp(Sec->name, "Tag") &&
		   strcasecmp(Sec->name, "Category") &&
		   strcasecmp(Sec->name, "Content-Language")
		   )) continue;

		buflen = qe - qbuf;
		if((buflen + len + 32) >= mlen){
		  mlen += step;
		  qbuf = (char*)DpsRealloc(qbuf, mlen);
		  if (qbuf == NULL) {
		    if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
		    DPS_FREE(arg);
		    return DPS_ERROR;
		  }
		  qe = qbuf + buflen;
		}
		if(have_inserts) dps_strcpy(qe++, ",");
		have_inserts++;

		arg = DpsDBEscStr(db, arg, Sec->val, dps_strlen(Sec->val));
		sprintf(qe, "(%s%i%s,'%s','%s')", qu, url_id, qu, dps_strtolower(Sec->name), arg);
		qe = qe + dps_strlen(qe);
		if((qe - qbuf) + 128 >= DPS_MAX_MULTI_INSERT_QSIZE) {
		  if (DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) {
		    if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
		    DPS_FREE(qbuf);
		    DPS_FREE(arg);
		    return rc;
		  }
		  /* Init query again */
		  sprintf(qbuf, "INSERT INTO urlinfo(url_id,sname,sval)VALUES");
		  qe = qbuf + dps_strlen(qbuf);
		  have_inserts = 0;
		}
	      }
	  }

	  if(have_inserts) rc = DpsSQLAsyncQuery(db, NULL, qbuf);

	} else {

	  if (Indexer->Flags.URLInfoSQL) {
	    for (r = 0; r < 256; r++)
	      for (i = 0; i < Doc->Sections.Root[r].nvars; i++) {
		DPS_VAR *Sec = &Doc->Sections.Root[r].Var[i];

		if(!Sec->name || !Sec->val || (!Sec->val[0] && strcasecmp(Sec->name, "Z"))  ) continue;
		if (
		   !strcasecmp(Sec->name, "URL") ||
		   !strcasecmp(Sec->name, "DP_ID") ||
		   !strcasecmp(Sec->name, "Status") ||
		   !strcasecmp(Sec->name, "Z") ||
		   !strcasecmp(Sec->name, "Pop_Rank") ||
		   !strcasecmp(Sec->name, "Content-Length")
		   ) continue;
		if( ((/*Sec->section == 0 &&*/ Sec->maxlen == 0) || u) && 
		   strcasecmp(Sec->name, "Title") &&
		   strcasecmp(Sec->name, "Charset") &&
		   strcasecmp(Sec->name, "Content-Type") &&
		   strcasecmp(Sec->name, "Content-Language")
		    ) continue;

		arg = DpsDBEscStr(db, arg, Sec->val, dps_strlen(Sec->val));
		sprintf(qbuf, "INSERT INTO urlinfo(url_id,sname,sval)VALUES(%s%i%s,'%s','%s')", qu, url_id, qu, dps_strtolower(Sec->name), arg);
		if (DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) break;
	      }
	  } else if (u == 0) {
	    for (r = 0; r < 256; r++)
	      for (i = 0; i < Doc->Sections.Root[r].nvars; i++) {
		DPS_VAR *Sec = &Doc->Sections.Root[r].Var[i];

		if(!Sec->name || !Sec->val || (!Sec->val[0] && strcasecmp(Sec->name, "Z"))  ) continue;
		if( (Sec->maxlen == 0) || (
		   strcasecmp(Sec->name, "Charset") &&
		   strcasecmp(Sec->name, "Content-Type") &&
		   strcasecmp(Sec->name, "Tag") &&
		   strcasecmp(Sec->name, "Category") &&
		   strcasecmp(Sec->name, "Content-Language")
		   )) continue;

		arg = DpsDBEscStr(db, arg, Sec->val, dps_strlen(Sec->val));
		sprintf(qbuf, "INSERT INTO urlinfo(url_id,sname,sval)VALUES(%s%i%s,'%s','%s')", qu, url_id, qu, dps_strtolower(Sec->name), arg);
		if (DPS_OK != (rc = DpsSQLAsyncQuery(db, NULL, qbuf))) break;
	      }
	  }
	}

	if (Indexer->Flags.URLInfoSQL) DpsSQLEnd(db);
	DPS_FREE(qbuf);
	DPS_FREE(arg);
	return rc;
}


static int DpsDeleteAllFromUrl(DPS_AGENT *Indexer,DPS_DB *db){
	int	rc;
	
	if(db->DBSQL_TRUNCATE)
		rc=DpsSQLAsyncQuery(db,NULL,"TRUNCATE TABLE url");
	else
		rc=DpsSQLAsyncQuery(db,NULL,"DELETE FROM url");
	
	if(rc!=DPS_OK)return rc;
	
	if(db->DBSQL_TRUNCATE)
		rc = DpsSQLAsyncQuery(db, NULL, "TRUNCATE TABLE links");
	else
		rc = DpsSQLAsyncQuery(db, NULL, "DELETE FROM links");
	
	if(rc != DPS_OK) return rc;
	
	if(db->DBSQL_TRUNCATE)
		rc=DpsSQLAsyncQuery(db,NULL,"TRUNCATE TABLE urlinfo");
		
	else
		rc=DpsSQLAsyncQuery(db,NULL,"DELETE FROM urlinfo");
	
	return rc;
}



/************************ Clones stuff ***************************/
static int DpsFindOrigin(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc,DPS_DB *db){
	char		qbuf[256]="";
	DPS_SQLRES	SQLRes;
	urlid_t		origin_id = 0;
	int		crc32 = DpsVarListFindInt(&Doc->Sections, "crc32", 0);
	int             docsize  = DpsVarListFindInt(&Doc->Sections, "Content-Length", 0);
	int		rc;
	static const char *limit = "LIMIT 1";
	
	if (crc32==0)return DPS_OK;
	
	DpsSQLResInit(&SQLRes);

	if (docsize) {
	  if (db->DBSQL_IN)
		sprintf(qbuf,"SELECT rec_id FROM url WHERE crc32=%d AND docsize>%d AND docsize<%d AND status IN (200,206,304) %s", 
			crc32, docsize - docsize/10, docsize + docsize/10, (db->DBSQL_LIMIT) ? limit : "");
	  else
		sprintf(qbuf,"SELECT rec_id FROM url WHERE crc32=%d AND docsize>%d AND docsize<%d AND (status=200 OR status=304 OR status=206) %s", 
			crc32, docsize - docsize/10, docsize + docsize/10, (db->DBSQL_LIMIT) ? limit : "");
	} else {
	  if (db->DBSQL_IN)
		sprintf(qbuf,"SELECT rec_id FROM url WHERE crc32=%d AND status IN (200,206,304) %s", crc32, (db->DBSQL_LIMIT) ? limit : "");
	  else
		sprintf(qbuf,"SELECT rec_id FROM url WHERE crc32=%d AND (status=200 OR status=304 OR status=206) %s", crc32, (db->DBSQL_LIMIT) ? limit : "");
	}
	if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLRes,qbuf)))
		return rc;

	if (DpsSQLNumRows(&SQLRes)) {
		const char *o;
		if((o = DpsSQLValue(&SQLRes, 0, 0)))
		    origin_id = (urlid_t)DPS_ATOI(o);
	}
	DpsSQLFree(&SQLRes);
	DpsVarListReplaceInt(&Doc->Sections, "Origin-ID", origin_id);
	return(DPS_OK);
}



/************** Get Target to be indexed ***********************/

int DpsTargetsSQL(DPS_AGENT *Indexer, DPS_DB *db){
	char		sortstr[128] = "";
	char		lmtstr[128]="";
	char		updstr[64]="";
	char            nitstr[64]="";
	size_t		i = 0, j, nrows, qbuflen, start_target;
	size_t		url_num;
	urlid_t         rec_id;
	urlid_t         nit;
	DPS_SQLRES 	SQLRes, sr;
	char		smallbuf[128];
	int		rc=DPS_OK;
	const char	*where;
	char		*qbuf=NULL;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	const char	*select_seed = (db->DBType == DPS_DB_MIMER) ? ",url.seed" : "";
	const char      *select_referer = (Indexer->Flags.provide_referer) ? ",referrer" : "";
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        lc_dc;
	DPS_VAR         *V;
	int             prev_id = -1, ntry = 0, first;

	TRACE_IN(Indexer, "DpsTargetsSQL");

	DpsSQLResInit(&SQLRes);
	if (Indexer->flags & DPS_FLAG_FROM_STORED) DpsSQLResInit(&sr);

	start_target = Indexer->Conf->Targets.num_rows;
	url_num = dps_min(Indexer->Conf->url_number, 
			  DpsVarListFindInt(&Indexer->Vars, "URLSelectCacheSize", DPS_URL_SELECT_CACHE_SIZE));
	if (url_num <= start_target) {
	  TRACE_OUT(Indexer);
	  return DPS_OK;
	}
	url_num -= start_target;

	where = BuildWhere(Indexer, db);
	if (where == NULL) {
	  TRACE_OUT(Indexer);
	  return DPS_ERROR;
	}
	loccs = Indexer->Conf->lcs;
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");

	if (0) {
	  switch(db->DBType) {
	  case DPS_DB_MYSQL:
	    rc = DpsSQLAsyncQuery(db, NULL, "LOCK TABLES url WRITE, server AS s WRITE, categories AS c WRITE");
	    break;
	  case DPS_DB_PGSQL:
	    rc = DpsSQLAsyncQuery(db, NULL, "BEGIN");
/*	  sprintf(updstr, " FOR UPDATE OF url ");*/
/*				rc=DpsSQLAsyncQuery(db,NULL,"LOCK url");*/
	    break;
	  case DPS_DB_ORACLE7:
	  case DPS_DB_ORACLE8:
	    sprintf(updstr, " FOR UPDATE ");
#if HAVE_ORACLE8
	    if(db->DBDriver==DPS_DB_ORACLE8){
	      sprintf(lmtstr, " AND ROWNUM<=%lu", (unsigned long)url_num); 
	    }
#endif
	    if(!lmtstr[0])
	      sprintf(lmtstr, " AND ROWNUM<=%lu", (long unsigned)url_num); 
	    break;
	  case DPS_DB_SAPDB:
	    sprintf(updstr, " WITH LOCK ");
	    dps_strcpy(lmtstr, "");
	    break;
	  default:
	    break;
	  }
	  if(rc != DPS_OK) goto unlock;
	}
	
	qbuflen = 1024 + 4 * dps_strlen(where);
		
	if ( (qbuf = (char*)DpsMalloc(qbuflen + 2)) == NULL) {
		  DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
		  rc = DPS_ERROR;
		  goto unlock;
	}

	if ((V = DpsVarListFind(&Indexer->Conf->Vars, "PopRank_nit")) == NULL) {
	    if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
	    rc = DpsSQLQuery(db, &SQLRes, "SELECT MIN(rec_id) FROM url");
	    if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
	    if (DpsSQLNumRows(&SQLRes)) nit = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
	    DpsSQLFree(&SQLRes);
	    first = 1;
	} else { 
	    nit = (urlid_t)DPS_ATOU(V->val);
	    first = 0;
	}

 one_try:
	qbuf[0]='\0';
	sortstr[0] = '\0';
	smallbuf[0] = '\0';
	if (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) {
	  sprintf(sortstr, " ORDER BY url.rec_id");
	} else if ( (Indexer->flags & (DPS_FLAG_SORT_HOPS | DPS_FLAG_SORT_EXPIRED | DPS_FLAG_SORT_POPRANK)) 
	     || (Indexer->flags & DPS_FLAG_SORT_SEED)  ) {
	  int notfirst = 0;

	  if (Indexer->flags & DPS_FLAG_SORT_POPRANK_REV) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",pop_rank ASC" : "ORDER BY pop_rank ASC");
	    notfirst = 1;
	  } else if (Indexer->flags & DPS_FLAG_SORT_POPRANK) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",pop_rank DESC" : "ORDER BY pop_rank DESC");
	    notfirst = 1;
	  }

	  if (Indexer->flags & DPS_FLAG_SORT_HOPS_REV) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",hops DESC" : "ORDER BY hops DESC");
	    notfirst = 1;
	  } else if (Indexer->flags & DPS_FLAG_SORT_HOPS) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",hops" : "ORDER BY hops");
	    notfirst = 1;
	  }

	  if (Indexer->flags & DPS_FLAG_SORT_EXPIRED_REV) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",next_index_time DESC" : "ORDER BY next_index_time DESC");
	    notfirst = 1;
	  } else if (Indexer->flags & DPS_FLAG_SORT_EXPIRED) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",next_index_time" : "ORDER BY next_index_time");
	    notfirst = 1;
	  }

	  if (Indexer->flags & DPS_FLAG_SORT_SEED) {
	    if(db->DBSQL_LIMIT){
	      dps_snprintf(qbuf, qbuflen, "SELECT url.seed FROM url%s WHERE %s%lu %s %s LIMIT 10", db->from, 
			   (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) ? "next_index_time>" : "next_index_time<=",
			   (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) ? (unsigned long)nit : (unsigned long)Indexer->now, 
		       where[0] ? "AND" : "", where);
	    } else {
	      dps_snprintf(qbuf, qbuflen, "SELECT url.seed FROM url%s WHERE %s%lu %s %s", db->from, 
			   (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) ? "next_index_time>" : "next_index_time<=",
			   (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) ? (unsigned long)nit : (unsigned long)Indexer->now, 
		       where[0] ? "AND" : "", where);
	    }

	    if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLRes, qbuf))) goto unlock;
	    if ((nrows = DpsSQLNumRows(&SQLRes)) > 0) {
	      if (Indexer->flags & DPS_FLAG_SORT_SEED2) {
#ifdef HAVE_PTHREAD
		int dir = rand_r(&Indexer->seed) % 2;
#else
		int dir = rand() % 2;
#endif
		sprintf(DPS_STREND(sortstr), "%s %s", (notfirst) ? ",seed" : "ORDER BY seed", (dir) ? "" : "DESC");
		notfirst = 1;
	      } else {
		dps_snprintf(smallbuf, sizeof(smallbuf), "AND seed=%s", DpsSQLValue(&SQLRes, 
#ifdef HAVE_PTHREAD
										    rand_r(&Indexer->seed) % nrows,
#else
										    rand() % nrows, 
#endif
										    0));
	      }
	    }
	    DpsSQLFree(&SQLRes);
	  }
	  if (Indexer->flags & DPS_FLAG_SORT_POPRANK_REV) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",s.pop_weight DESC" : "ORDER BY s.pop_weight DESC");
	    notfirst = 1;
	  } else if (Indexer->flags & DPS_FLAG_SORT_POPRANK) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",s.pop_weight" : "ORDER BY s.pop_weight");
	    notfirst = 1;
	  }
	  if (DpsVarListFind(&Indexer->Vars, "status") == NULL) {
	    sprintf(DPS_STREND(sortstr), "%s", (notfirst) ? ",url.status" : "ORDER BY url.status");
	    notfirst = 1;
	  }

	} else { /* No sorting flag specified, order by status */
	    sprintf(sortstr, "%s", "ORDER BY url.status");
	}
	

	if (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) {
	    dps_snprintf(nitstr, sizeof(nitstr), "url.rec_id>%s%ld", (first)?"=":"",(long)nit);
	} else if (Indexer->Flags.expire) {
	  nitstr[0] = '\0';
	} else {
	  dps_snprintf(nitstr, sizeof(nitstr), "next_index_time<=%lu", (unsigned long)Indexer->now);
	}
			
	if(db->DBSQL_LIMIT){
	  dps_snprintf(qbuf, qbuflen, "SELECT url.url,url.rec_id,docsize,status,hops,crc32,last_mod_time,since,pop_rank,charset_id,site_id,server_id%s%s%s FROM url%s WHERE %s %s %s %s %s LIMIT %d%s",
		       select_referer, select_seed, (Indexer->Flags.cmd & DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) ? ",next_index_time" : "", db->from,
		       (where[0] || nitstr[0]) ? nitstr : "TRUE",
		       (where[0] && nitstr[0]) ? "AND" : "", where, smallbuf, sortstr, url_num, updstr);
	}else{
	  db->res_limit=url_num;
	  if(!qbuf[0])
	    dps_snprintf(qbuf, qbuflen, "SELECT url.url,url.rec_id,docsize,status,hops,crc32,last_mod_time,since,pop_rank,charset_id,site_id,server_id%s%s%s FROM url%s WHERE %s %s %s %s %s %s %s",
			 select_referer, select_seed, (Indexer->Flags.cmd & DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) ? ",next_index_time" : "", db->from,
			 (where[0] || nitstr[0]) ? nitstr : "TRUE",
			 (where[0] && nitstr[0]) ? "AND" : "", where, smallbuf, lmtstr, sortstr, updstr);
	}
	if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLRes, qbuf))) goto unlock;

	if(!(nrows = DpsSQLNumRows(&SQLRes))) {
		DpsSQLFree(&SQLRes);
		if (ntry++ < 3) {
		    DPSSLEEP(1);
		    goto one_try;
		}
		goto unlock;
	}
	if (!db->DBSQL_LIMIT && nrows > url_num) nrows = url_num;

/*	DpsResultFree(&Indexer->Conf->Targets);*/
	Indexer->Conf->Targets.num_rows += nrows;
	
	Indexer->Conf->Targets.Doc = 
	  (DPS_DOCUMENT*)DpsRealloc(Indexer->Conf->Targets.Doc, sizeof(DPS_DOCUMENT)*(Indexer->Conf->Targets.num_rows + 1));
	if (Indexer->Conf->Targets.Doc == NULL) {
	  DpsSQLFree(&SQLRes);
	  DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory at realloc %s[%d], numrows: %d", __FILE__, __LINE__, 
		 Indexer->Conf->Targets.num_rows );
	  rc = DPS_ERROR;
	  goto unlock;
	}
	
	for(i = 0; i < nrows; i++){
		char		buf[96]="";
		time_t		last_mod_time, len;
		DPS_DOCUMENT	*Doc = &Indexer->Conf->Targets.Doc[start_target + i];
		const char *url;
		char *dc_url;

		DpsDocInit(Doc);
		DpsVarListReplaceLst(&Doc->Sections, &Indexer->Conf->Sections, NULL, "*");
		Doc->charset_id = DPS_ATOI(DpsSQLValue(&SQLRes, i, 9));
		if (Doc->charset_id != prev_id) {
		  doccs = DpsGetCharSetByID(prev_id = Doc->charset_id);
		  if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
		  DpsConvInit(&lc_dc, loccs, doccs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL_FROM);
		}	  
		len = dps_strlen(url = DpsSQLValue(&SQLRes,i,0));
		dc_url = (char*)DpsMalloc((size_t)(24 * len + 1));
		if (dc_url == NULL) continue;
		/* Convert URL from LocalCharset */
		DpsConv(&lc_dc, dc_url, (size_t)24 * len,  url, (size_t)(len + 1));
		DpsVarListReplaceStr(&Doc->Sections, "URL", dc_url);
		DpsVarListReplaceStr(&Doc->Sections, "E_URL", url);

		DpsVarListDel(&Doc->Sections, "URL_ID");
		DPS_FREE(dc_url);

		DpsVarListReplaceInt(&Doc->Sections, "DP_ID", rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLRes,i,1)));
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", DPS_ATOI(DpsSQLValue(&SQLRes,i,2)));
		DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_ATOI(DpsSQLValue(&SQLRes,i,3)));
		DpsVarListReplaceInt(&Doc->Sections, "PrevStatus", DPS_ATOI(DpsSQLValue(&SQLRes,i,3)));
		DpsVarListReplaceInt(&Doc->Sections, "Hops", DPS_ATOI(DpsSQLValue(&SQLRes,i,4)));
		DpsVarListReplaceInt(&Doc->Sections, "crc32", DPS_ATOI(DpsSQLValue(&SQLRes,i,5)));
		DpsVarListReplaceInt(&Doc->Sections, "Site_id", DPS_ATOI(DpsSQLValue(&SQLRes, i, 10)));
		DpsVarListReplaceInt(&Doc->Sections, "Server_id", 0 /*DPS_ATOI(DpsSQLValue(&SQLRes, i, 11))*/);
		last_mod_time = (time_t) atol(DpsSQLValue(&SQLRes,i,6));
		DpsTime_t2HttpStr(last_mod_time, buf);
		if (last_mod_time != 0 && *buf != '\0') {
		  DpsVarListReplaceStr(&Doc->Sections, "Last-Modified",buf);
		}
		DpsVarListReplaceStr(&Doc->Sections, "Since", DpsSQLValue(&SQLRes, i, 7));
		DpsVarListReplaceStr(&Doc->Sections, "Pop_Rank", DpsSQLValue(&SQLRes, i, 8));
		if (Indexer->Flags.provide_referer) DpsVarListReplaceStr(&Doc->Sections, "Referrer-ID", DpsSQLValue(&SQLRes, i, 12));
		if (Indexer->flags & DPS_FLAG_FROM_STORED) {
		  dps_snprintf(buf, sizeof(buf), "SELECT sval FROM urlinfo WHERE url_id=%d AND sname='content-language'", rec_id);
		  if(DPS_OK != (rc = DpsSQLQuery(db, &sr, buf))) {
		    DpsSQLFree(&SQLRes);
		    DpsLog(Indexer, DPS_LOG_ERROR, "Error SQL execution at %s:%d", __FILE__, __LINE__);
		    goto unlock;
		  }
		  if (DpsSQLNumRows(&sr)) DpsVarListReplaceStr(&Doc->Sections, "Content-Language", DpsSQLValue(&sr, 0, 0));
		}
	}
	if ((Indexer->Flags.cmd & DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) && (nrows != 0)) {
	    nit = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLRes, nrows - 1, 1));
	}
	DpsSQLFree(&SQLRes);
	if (Indexer->flags & DPS_FLAG_FROM_STORED) DpsSQLFree(&sr);
	
	if (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) {
	  DpsVarListReplaceUnsigned(&Indexer->Conf->Vars, "PopRank_nit", (unsigned)nit);
	  /* To see what rec_id has been indexed in "ps" output on xBSD */
	  if (DpsNeedLog(DPS_LOG_INFO)) dps_setproctitle("[%d] %s rec_id: %u", Indexer->handle, 
							 (Indexer->Flags.cmd == DPS_IND_POPRANK) ? "PopRank" : "Filter", (unsigned)rec_id);
	  DpsLog(Indexer, DPS_LOG_INFO, "[%d] %s rec_id: %u", 
		 Indexer->handle, (Indexer->Flags.cmd == DPS_IND_POPRANK) ? "PopRank" : "Filter", (unsigned)rec_id);
	}else if (Indexer->Flags.mark_for_index) {
	  if (db->DBSQL_IN) {
		char	*urlin=NULL;
		
		if ( (qbuf = (char*)DpsRealloc(qbuf, qbuflen = qbuflen + 35 * url_num)) == NULL) {
			DPS_FREE(qbuf);
			DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
			rc= DPS_ERROR;
			goto unlock;
		}
		
		if ( (urlin = (char*)DpsMalloc(35 * url_num + 1)) == NULL) {
			DPS_FREE(qbuf);
			DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory");
			rc = DPS_ERROR;
			goto unlock;
		}
		urlin[0]=0;
		
		for(i = 0; i < nrows; i+= url_num) {

		  urlin[0] = 0;

		  for (j = 0; (j < url_num) && (i + j < nrows) ; j++) {

			DPS_DOCUMENT	*Doc = &Indexer->Conf->Targets.Doc[start_target + i + j];
			urlid_t		url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
			
			if(urlin[0])dps_strcat(urlin,",");
			sprintf(urlin+dps_strlen(urlin), "%s%i%s", qu, url_id, qu);
		  }
		  dps_snprintf(qbuf, qbuflen, "UPDATE url SET next_index_time=%u WHERE rec_id in (%s)",
			       (Indexer->now + URL_LOCK_TIME), urlin);
		  rc = DpsSQLAsyncQuery(db,NULL,qbuf);
		  if (rc != DPS_OK) {
		    DPS_FREE(urlin);
		    goto unlock;
		  }
		}
		DPS_FREE(urlin);
	  }else{
		for(i = 0; i < nrows; i++){
			DPS_DOCUMENT	*Doc = &Indexer->Conf->Targets.Doc[start_target + i];
			urlid_t		url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
			
			dps_snprintf(smallbuf, 128, "UPDATE url SET next_index_time=%u WHERE rec_id=%i",
				     (Indexer->now + URL_LOCK_TIME), url_id);
			if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,smallbuf))) {
			  goto unlock;
			}
		}
	  }
	}
	
unlock:
	if (rc != DPS_OK) {
	  DpsLog(Indexer, DPS_LOG_ERROR, "DpsTargetsSQL: DB error: %s", db->errstr);
	}


	if (0) {
	  switch(db->DBType) {
	  case DPS_DB_MYSQL:
	    rc = DpsSQLAsyncQuery(db, NULL, "UNLOCK TABLES");
	    break;
	  case DPS_DB_PGSQL:
	    rc = DpsSQLAsyncQuery(db, NULL, "COMMIT");
	    break;
	  default:
	    break;
	  }
	}
	DPS_FREE(qbuf);
	TRACE_OUT(Indexer);
	return rc;
}


/************************************************************/
/* Misc functions                                           */
/************************************************************/

static int DpsGetDocCount(DPS_AGENT * Indexer,DPS_DB *db){
	char		qbuf[200]="";
	DPS_SQLRES	SQLres;
	int		rc;
	
	DpsSQLResInit(&SQLres);

	sprintf(qbuf,NDOCS_QUERY);
	if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf)))return rc;
	
	if(DpsSQLNumRows(&SQLres)){
		const char * s;
		s=DpsSQLValue(&SQLres,0,0);
		if(s)Indexer->doccount += atoi(s);
	}
	DpsSQLFree(&SQLres);
	return(DPS_OK);
}


int DpsStatActionSQL(DPS_AGENT *Indexer,DPS_STATLIST *Stats,DPS_DB *db){
	size_t		i,j,n;
	char		qbuf[2048];
	DPS_SQLRES	SQLres;
	int		have_group=db->DBSQL_GROUP;
	const char	*where;
	int		rc=DPS_OK;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	time_t          curtime = Indexer->now;
	
	bzero(&SQLres, sizeof(SQLres));

	if(db->DBType==DPS_DB_IBASE)
		have_group=0;

	where = BuildWhere(Indexer, db);
	if (where == NULL) return DPS_ERROR;

	DpsSQLBegin(db);

	if(have_group){
		
		switch(db->DBType){
		
			case DPS_DB_MYSQL:
			  dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT status,sum(next_index_time<=%u),count(*),sum(docsize), 0 FROM url%s %s %s GROUP BY status ORDER BY status",
				       curtime, db->from, where[0] ? "WHERE" : "", where);
				break;

			case DPS_DB_MSSQL:
			  dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT status,sum(case when next_index_time<=%u then 1 else 0 end),count(*),sum(cast(docsize as bigint)),sum(case when next_index_time<=%u then cast(docsize as bigint) else 0 end) FROM url%s %s %s GROUP BY status ORDER BY status",
				       curtime, curtime, db->from, where[0] ? "WHERE" : "", where);
				break;
		
			case DPS_DB_PGSQL:
			case DPS_DB_DB2:
			case DPS_DB_SQLITE:
			case DPS_DB_SQLITE3:
			default:
			  dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT status,sum(case when next_index_time<=%u then 1 else 0 end),count(*),sum(docsize),sum(case when next_index_time<=%u then docsize else 0 end) FROM url%s %s %s GROUP BY status ORDER BY status",
				       curtime, curtime, db->from, where[0] ? "WHERE" : "", where);
				break;

			case DPS_DB_ACCESS:
				dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT status,sum(IIF(next_index_time<=%u, 1, 0)),count(*),sum(docsize),0 FROM url%s WHERE url.rec_id<>%s0%s %s %s GROUP BY status ORDER BY status",
					curtime, db->from, qu, qu, where[0] ? "AND" :"", where);
				break;

			case DPS_DB_ORACLE7:
			case DPS_DB_ORACLE8:
			case DPS_DB_SAPDB:
				dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT status, SUM(DECODE(SIGN(%u-next_index_time),-1,0,1,1)), count(*),sum(docsize),0 FROM url%s WHERE url.rec_id<>0 %s %s GROUP BY status ORDER BY status",
					 curtime, db->from, where[0] ? "AND" : "", where);
				break;
		}

		if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		  DpsSQLEnd(db);
		  return rc;
		}
		
		if((n = DpsSQLNumRows(&SQLres))) {
		  for(i = 0; i < n; i++){
			for(j=0;j<Stats->nstats;j++){
				if(Stats->Stat[j].status==atoi(DpsSQLValue(&SQLres,i,0))){
				        Stats->Stat[j].expired += atoi(DpsSQLValue(&SQLres,i,1));
				        Stats->Stat[j].total += atoi(DpsSQLValue(&SQLres,i,2));
				        Stats->Stat[j].total_size += (dps_uint8)strtoull(DpsSQLValue(&SQLres, i, 3), (char**)NULL, 10);
				        Stats->Stat[j].expired_size += (dps_uint8)strtoull(DpsSQLValue(&SQLres, i, 4), (char**)NULL, 10);
					break;
				}
			}
			if(j==Stats->nstats){
				  DPS_STAT	*S;
				
				  Stats->Stat=(DPS_STAT*)DpsRealloc(Stats->Stat,(Stats->nstats+1)*sizeof(Stats->Stat[0]));
				  if (Stats->Stat == NULL) {
				    DpsSQLEnd(db);
				    return DPS_ERROR;
				  }
				  S = &Stats->Stat[Stats->nstats];
				  S->status = atoi(DpsSQLValue(&SQLres,i,0));
				  S->expired = atoi(DpsSQLValue(&SQLres,i,1));
				  S->total = atoi(DpsSQLValue(&SQLres,i,2));
				  S->total_size = (dps_uint8)strtoull(DpsSQLValue(&SQLres, i, 3), (char**)NULL, 10);
				  S->expired_size = (dps_uint8)strtoull(DpsSQLValue(&SQLres, i, 4), (char**)NULL, 10);
				  Stats->nstats++;
			}
		  }
		}
		DpsSQLFree(&SQLres);
		
	}else{
/*	
	FIXME: learn how to get it from SOLID and IBASE
	(HAVE_IBASE||HAVE_MSQL || HAVE_SOLID || HAVE_VIRT )
*/
		n = Indexer->now;
		
		dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT status,next_index_time,docsize FROM url%s WHERE url.rec_id>0 %s %s ORDER BY status",
			db->from, where[0] ? "AND" : "", where);

		if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		  DpsSQLEnd(db);
		  return rc;
		}
		
		for(i=0;i<DpsSQLNumRows(&SQLres);i++){
			for(j=0;j<Stats->nstats;j++){
				if(Stats->Stat[j].status==atoi(DpsSQLValue(&SQLres,i,0))){
				  if((size_t)DPS_ATOU(DpsSQLValue(&SQLres,i,1)) <= n) {
				    Stats->Stat[j].expired++;
				    Stats->Stat[j].expired_size += (dps_uint8)strtoll(DpsSQLValue(&SQLres, i, 2), (char**)NULL, 10);
				  }
				  Stats->Stat[j].total++;
				  Stats->Stat[j].total_size += (dps_uint8)strtoll(DpsSQLValue(&SQLres, i, 2), (char**)NULL, 10);
				  break;
				}
			}
			if(j==Stats->nstats){
				Stats->Stat=(DPS_STAT *)DpsRealloc(Stats->Stat,sizeof(DPS_STAT)*(Stats->nstats+1));
				if (Stats->Stat == NULL) {
				  DpsSQLEnd(db);
				  return DPS_ERROR;
				}
				Stats->Stat[j].status = DPS_ATOI(DpsSQLValue(&SQLres,i,0));
				Stats->Stat[j].expired=0;
				if((size_t)DPS_ATOU(DpsSQLValue(&SQLres,i,1)) <= n) {
				  Stats->Stat[j].expired++;
				  Stats->Stat[j].expired_size = (dps_uint8)strtoll(DpsSQLValue(&SQLres, i, 2), (char**)NULL, 10);
				}
				Stats->Stat[j].total=1;
				Stats->Stat[j].total_size = (dps_uint8)strtoll(DpsSQLValue(&SQLres, i, 2), (char**)NULL, 10);
				Stats->nstats++;
			}
		}
		DpsSQLFree(&SQLres);
	}
	DpsSQLEnd(db);
	return rc;
}


static int DpsGetReferers(DPS_AGENT *Indexer,DPS_DB *db){
	size_t		i,j;
	char		qbuf[2048];
	DPS_SQLRES	SQLres;
	const char	*where;
	int		rc;

	DpsSQLResInit(&SQLres);

	where = BuildWhere(Indexer, db);
	if (where == NULL) return DPS_ERROR;
		
	dps_snprintf(qbuf,sizeof(qbuf),"SELECT url.status,url2.url,url.url FROM url,url url2%s WHERE url.referrer=url2.rec_id %s %s",
		db->from, where[0] ? "AND" : "", where);
	
	if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf)))return rc;
	
	j=DpsSQLNumRows(&SQLres);
	DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	for(i=0;i<j;i++){
		if(Indexer->Conf->RefInfo)Indexer->Conf->RefInfo(
			atoi(DpsSQLValue(&SQLres,i,0)),
			DpsSQLValue(&SQLres,i,2),
			DpsSQLValue(&SQLres,i,1)
		);
	}
	DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	DpsSQLFree(&SQLres);
	return rc;
}


int DpsClearDBSQL(DPS_AGENT *Indexer, DPS_DB *db) {
	size_t		i,j;
	int		rc;
	char		qbuf[1024*4];
	const char*	where;
	int		use_crosswords;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	const char      *url;
	char            *dc_url;
	size_t          len;
	int             prev_id = -1;
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        lc_dc;

	where = BuildWhere(Indexer, db);
	if (where == NULL) return DPS_ERROR;

	use_crosswords = ((Indexer->Flags.use_crosswords > 0) /*&& (db->DBMode != DPS_DBMODE_CACHE)*/);

	loccs = Indexer->Conf->lcs;
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");
	
	if(!where[0]){

		if(db->DBMode==DPS_DBMODE_CACHE){
		        DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
			DpsClearCacheTree(Indexer->Conf);
			DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		}

		if(use_crosswords /*&& db->DBMode!=DPS_DBMODE_CACHE*/) {
			if((DPS_OK!=(rc=DpsDeleteAllFromCrossDict(Indexer,db))))return(rc);
		}
		if((DPS_OK!=(rc=DpsDeleteAllFromDict(Indexer,db))))return(rc);
		if((DPS_OK!=(rc=DpsDeleteAllFromUrl(Indexer,db))))return(rc);
	}else{
		j=0;
		while(1){
			size_t 		last = 0;
			char 		urlin[1024*3]="";
			char		limit[100]="";
			DPS_SQLRES	SQLres;
			size_t		url_num, rows_num;
			
			DpsSQLResInit(&SQLres);

			url_num = DpsVarListFindInt(&Indexer->Vars, "URLDumpCacheSize", DPS_URL_DELETE_CACHE_SIZE);

			if(db->DBSQL_LIMIT){
				sprintf(limit," LIMIT %zu", url_num);
			}
			sprintf(qbuf,"SELECT url.rec_id,url.url,url.charset_id FROM url%s WHERE url.rec_id<>%s0%s %s %s %s", 
				db->from, qu, qu, where[0] ? "AND" : "",  where, limit);
			
			if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf)))
				return rc;
			
			if((rows_num = (size_t)DpsSQLNumRows(&SQLres))) {
				DPS_DOCUMENT Doc;
				
				DpsDocInit(&Doc);
				if(db->DBMode==DPS_DBMODE_CACHE){
					for(i=0;i<DpsSQLNumRows(&SQLres);i++){

					  Doc.charset_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 2));

					  if (Doc.charset_id != prev_id) {
					    doccs = DpsGetCharSetByID(prev_id = Doc.charset_id);
					    if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
					    DpsConvInit(&lc_dc, loccs, doccs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);
					  }
					  len = dps_strlen(url = DpsSQLValue(&SQLres, i, 1));
					  dc_url = (char*)DpsMalloc((size_t)(24 * len + 1));
					  if (dc_url == NULL) continue;
					  /* Convert URL from LocalCharset */
					  DpsConv(&lc_dc, dc_url, (size_t)24 * len,  url, (size_t)(len + 1));
					  DpsVarListReplaceStr(&Doc.Sections, "URL", dc_url);

					  DpsVarListDel(&Doc.Sections, "URL_ID");
					  DPS_FREE(dc_url);

					  DpsVarListReplaceInt(&Doc.Sections, "DP_ID", DPS_ATOI(DpsSQLValue(&SQLres,i,0)));
					  if (DpsURLActionCache(Indexer, &Doc, DPS_URL_ACTION_DELETE, db) != DPS_OK) {
					    DpsSQLFree(&SQLres);
					    return(DPS_ERROR);
					  }
					  if (db->DBMode == DPS_DBMODE_CACHE)
					    if (DPS_OK != (rc = DpsDeleteWordFromURL(Indexer, &Doc, db))) return rc;
					  if(DPS_OK!=DpsDeleteURL(Indexer,&Doc,db)) {
					    DpsSQLFree(&SQLres);
					    return(DPS_ERROR);
					  }
					}
					if (rows_num < url_num) break;
					continue;
				}
				if(db->DBSQL_IN){
					urlin[0]=0;
					for(i=0;i<DpsSQLNumRows(&SQLres);i++){
						if(i)dps_strcat(urlin,",");
						dps_strcat(urlin, qu);
						dps_strcat(urlin,DpsSQLValue(&SQLres,i,0));
						dps_strcat(urlin, qu);
					}
					j+=i;
					switch(db->DBMode){
					case DPS_DBMODE_MULTI:
						for(i=MINDICT;i<MAXDICT;i++){
							if(last!=DICTNUM(i)){
								sprintf(qbuf,"DELETE FROM dict%zu WHERE url_id in (%s)",DICTNUM(i),urlin);
								if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf))) {
								        DpsSQLFree(&SQLres);
									return rc;
								}
								last=DICTNUM(i);
							}
						}
						break;
					case DPS_DBMODE_MULTI_CRC:
						for(i=MINDICT;i<MAXDICT;i++){
							if(last!=DICTNUM(i)){
								sprintf(qbuf,"DELETE FROM ndict%zu WHERE url_id in (%s)",DICTNUM(i),urlin);
								if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf))) {
								        DpsSQLFree(&SQLres);
									return rc;
								}
								last=DICTNUM(i);
							}
						}
						break;
					case DPS_DBMODE_CACHE:
						break;
					default:
						sprintf(qbuf,"DELETE FROM dict WHERE url_id in (%s)",urlin);
						if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf))) {
						        DpsSQLFree(&SQLres);
							return rc;
						}
						break;
					}
					sprintf(qbuf,"DELETE FROM url WHERE rec_id in (%s)",urlin);
					if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
						return rc;
					
					if (Indexer->Flags.URLInfoSQL) {
					  sprintf(qbuf,"DELETE FROM urlinfo WHERE url_id in (%s)",urlin);
					  if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
					        return rc;
					}

					sprintf(qbuf,"DELETE FROM links WHERE ot in (%s)",urlin);
					if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
						return rc;

					sprintf(qbuf,"DELETE FROM links WHERE k in (%s)",urlin);
					if(DPS_OK!=(rc=DpsSQLAsyncQuery(db,NULL,qbuf)))
						return rc;

					DpsSQLFree(&SQLres);
				}else{
				        for(i=0;i<DpsSQLNumRows(&SQLres);i++) {
						DpsVarListReplaceInt(&Doc.Sections, "DP_ID", DPS_ATOI(DpsSQLValue(&SQLres,i,0)));
					        if(DPS_OK != DpsDeleteURL(Indexer, &Doc, db)) {
						  DpsSQLFree(&SQLres);
							return(DPS_ERROR);
						}
					}
					j+=i;
					DpsSQLFree(&SQLres);
				}
				if (rows_num < url_num) break;
			}else{
				DpsSQLFree(&SQLres);
				break;
			}
		}
	}
	return(DPS_OK);
}

/********************* Categories ************************************/
static int DpsCatList(DPS_AGENT * Indexer,DPS_CATEGORY *Cat,DPS_DB *db){
	size_t		i, rows;
	char		qbuf[1024];
	DPS_SQLRES	SQLres, Res;
	int		rc;
	const char      *addr;

	if (*Cat->addr == '\0') {
	  Cat->ncategories = 0;
	  return DPS_OK;
	}

	DpsSQLResInit(&SQLres);
	DpsSQLResInit(&Res);

	dps_snprintf(qbuf, sizeof(qbuf) - 1, "SELECT path FROM categories WHERE rec_id=%s", Cat->addr);

	if(DPS_OK != (rc = DpsSQLQuery(db, &Res, qbuf))) return rc;
	
	if (DpsSQLNumRows(&Res) < 1) {
	  Cat->ncategories = 0;
	  DpsSQLFree(&Res);
	  return DPS_OK;
	}

	addr = DpsSQLValue(&Res, 0, 0);

	if(db->DBType==DPS_DB_SAPDB){
		dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,lnk,name FROM categories WHERE path LIKE '%s__'", addr);
	}else{
		dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,link,name FROM categories WHERE path LIKE '%s__'", addr);
	}
	
	if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
	        DpsSQLFree(&Res);
		return rc;
	}
	
	if( (rows = DpsSQLNumRows(&SQLres)) ){
		size_t nbytes;
		
		nbytes = sizeof(DPS_CATITEM) * (rows + Cat->ncategories);
		Cat->Category=(DPS_CATITEM*)DpsRealloc(Cat->Category,nbytes + 1);
		if (Cat->Category == NULL) {
		  Cat->ncategories = 0;
		  DpsSQLFree(&Res);
		  DpsSQLFree(&SQLres);
		  return DPS_ERROR;
		}
		for(i=0;i<rows;i++){
			DPS_CATITEM *r = &Cat->Category[Cat->ncategories];
			r[i].rec_id=atoi(DpsSQLValue(&SQLres,i,0));
			dps_strcpy(r[i].path,DpsSQLValue(&SQLres,i,1));
			dps_strcpy(r[i].link,DpsSQLValue(&SQLres,i,2));
			dps_strcpy(r[i].name,DpsSQLValue(&SQLres,i,3));
		}
		Cat->ncategories+=rows;
	}
	DpsSQLFree(&Res);
	DpsSQLFree(&SQLres);
	return DPS_OK;
}

static int DpsCatPath(DPS_AGENT *Indexer,DPS_CATEGORY *Cat,DPS_DB *db){
	size_t		i,l;
	char		qbuf[1024];
	int		rc;
	char            *head = NULL;
	DPS_SQLRES	Res;
	const char      *addr;
	
	if (*Cat->addr == '\0') {
	  Cat->ncategories = 0;
	  return DPS_OK;
	}

	DpsSQLResInit(&Res);

	dps_snprintf(qbuf, sizeof(qbuf) - 1, "SELECT path FROM categories WHERE rec_id=%s", Cat->addr);
		
	if(DPS_OK != (rc = DpsSQLQuery(db, &Res, qbuf))) return rc;
	
	if (DpsSQLNumRows(&Res) < 1) {
	  Cat->ncategories = 0;
	  DpsSQLFree(&Res);
	  return DPS_OK;
	}

	addr = DpsSQLValue(&Res, 0, 0);


	l = (dps_strlen(addr)/2) + 1;
	Cat->Category=(DPS_CATITEM*)DpsRealloc(Cat->Category,sizeof(DPS_CATITEM)*(l+Cat->ncategories));
	if (Cat->Category == NULL) {
	  Cat->ncategories = 0;
	  DpsSQLFree(&Res);
	  return DPS_ERROR;
	}
	head = (char *)DpsMalloc(2 * l + 1);

	if (head != NULL) {
	  DPS_CATITEM	*r = &Cat->Category[Cat->ncategories];
	  
	  for(i = 0; i < l; i++){
		DPS_SQLRES	SQLres;

		DpsSQLResInit(&SQLres);

		dps_strncpy(head, addr, i * 2); head[i * 2] = 0;

		if(db->DBType==DPS_DB_SAPDB){
			dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,lnk,name FROM categories WHERE path='%s'", head);
		}else{
			dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,link,name FROM categories WHERE path='%s'", head);
		}
		
		if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		        DpsSQLFree(&Res);
			return rc;
		}
		
		if(DpsSQLNumRows(&SQLres)){
			r[i].rec_id=atoi(DpsSQLValue(&SQLres,0,0));
			dps_strcpy(r[i].path,DpsSQLValue(&SQLres,0,1));
			dps_strcpy(r[i].link,DpsSQLValue(&SQLres,0,2));
			dps_strcpy(r[i].name,DpsSQLValue(&SQLres,0,3));
			Cat->ncategories++;
		}
		DpsSQLFree(&SQLres);
	  }
	  DPS_FREE(head);
	}
	DpsSQLFree(&Res);
	return DPS_OK;
}


/******************* Search stuff ************************************/

int DpsCloneListSQL(DPS_AGENT * Indexer, DPS_VARLIST *Env_Vars, DPS_DOCUMENT *Doc, DPS_RESULT *Res, DPS_DB *db) {
	char		qbuf[256];
	char		buf[128];
	time_t		last_mod_time;
	size_t		i, nr, nadd;
#ifdef HAVE_PTHREAD
	struct tm       l_tim;
#endif
	DPS_SQLRES	SQLres;
	urlid_t		origin_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	int		rc;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	const char	*format = DpsVarListFindStrTxt(Env_Vars, "DateFormat", "%a, %d %b %Y, %X %Z");
	const char      *url;
	char            *dc_url;
	size_t          len;
	int             prev_id = -1;
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        lc_dc;

	if (Res->num_rows > 4) return DPS_OK;

	DpsSQLResInit(&SQLres);

	loccs = Indexer->Conf->lcs;
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");
	
	sprintf(qbuf,"SELECT u.rec_id,u.url,u.last_mod_time,u.docsize,u.charset_id FROM url u, url uo WHERE u.crc32!=0 AND uo.crc32!=0 AND u.crc32=uo.crc32 AND (u.status=200 OR u.status=304 OR u.status=206) AND u.rec_id<>uo.rec_id AND uo.rec_id=%s%i%s", qu, origin_id, qu);
	if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf)))
		return DPS_OK;
	
	nr = DpsSQLNumRows(&SQLres);
	if( nr == 0) {
	  DpsSQLFree(&SQLres);
	  return DPS_OK;
	}
	nadd = 5 - Res->num_rows;
	if(nr < nadd) nadd = nr;
	
	Res->Doc = (DPS_DOCUMENT*)DpsRealloc(Res->Doc, (Res->num_rows + nadd + 1) * sizeof(DPS_DOCUMENT));
	if (Res->Doc == NULL) {
	  DpsSQLFree(&SQLres);
	  return DPS_ERROR;
	}
	
	for(i = 0; i < nadd; i++) {
		DPS_DOCUMENT	*D = &Res->Doc[Res->num_rows + i];
		
		DpsDocInit(D);

		D->charset_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 4));

		if (D->charset_id != prev_id) {
		  doccs = DpsGetCharSetByID(prev_id = D->charset_id);
		  if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
		  DpsConvInit(&lc_dc, loccs, doccs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);
		}
		len = dps_strlen(url = DpsSQLValue(&SQLres, i, 1));
		dc_url = (char*)DpsMalloc((size_t)(24 * len + 1));
		if (dc_url == NULL) continue;
		/* Convert URL from LocalCharset */
		DpsConv(&lc_dc, dc_url, (size_t)24 * len,  url, (size_t)(len + 1));
		DpsVarListReplaceStr(&D->Sections, "URL", dc_url);

		DpsVarListDel(&D->Sections, "URL_ID");
		DPS_FREE(dc_url);

		DpsVarListAddInt(&D->Sections, "DP_ID", DPS_ATOI(DpsSQLValue(&SQLres,i,0)));
		last_mod_time=atol(DpsSQLValue(&SQLres,i,2));
		if (last_mod_time > 0) {
		  if (strftime(buf, 128, format, 
#ifdef HAVE_PTHREAD
			       localtime_r(&last_mod_time, &l_tim)
#else
			       localtime(&last_mod_time)
#endif
			       ) == 0) {
		    DpsTime_t2HttpStr(last_mod_time, buf);
		  }
		  DpsVarListReplaceStr(&D->Sections, "Last-Modified", buf);
		}
		DpsVarListAddInt(&D->Sections, "Content-Length",atoi(DpsSQLValue(&SQLres,i,3)));
		DpsVarListAddInt(&D->Sections, "Origin-ID", origin_id);
	}
	Res->num_rows += nadd;
	DpsSQLFree(&SQLres);
	return DPS_OK;
}

/********************************************************/

int DpsURLDataLoadSQL(DPS_AGENT *A, DPS_RESULT *R, DPS_DB *db) {
	DPS_SQLRES	SQLres;
	size_t i,j;
	int rc;
	char qbuf[4*1024];
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";

	if (R->CoordList.ncoords == 0) return DPS_OK;
	DpsSQLResInit(&SQLres);

	R->CoordList.Data = (DPS_URLDATA*)DpsRealloc(R->CoordList.Data, R->CoordList.ncoords * sizeof(DPS_URLDATA) + 1);
	if (R->CoordList.Data == NULL) {
	  return DPS_ERROR;
	}
	if (db->DBSQL_IN) {
	  for (j = 0; j < R->CoordList.ncoords; j += 256) {
	    int notfirst = 0;
	    sprintf(qbuf, "SELECT rec_id,site_id,pop_rank,last_mod_time,since FROM url WHERE rec_id IN (");
	    for (i = 0; (i < 256) && (j + i < R->CoordList.ncoords); i++) {
	      sprintf(DPS_STREND(qbuf), "%s%s%i%s", (notfirst) ? "," : "", qu, R->CoordList.Coords[j + i].url_id, qu);
	      notfirst = 1;
	    }
	    sprintf(DPS_STREND(qbuf), ") ORDER BY rec_id");
	    if (DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf)))
	      return rc;
	    for(i = 0; i < DpsSQLNumRows(&SQLres); i++){
	      R->CoordList.Data[i + j].url_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, i, 0));
	      
	      if (R->CoordList.Data[j + i].url_id != R->CoordList.Coords[j + i].url_id)
		DpsLog(A, DPS_LOG_ERROR, "SQL: Crd url_id (%d) != Dat url_id (%d)", 
		       R->CoordList.Coords[j + i].url_id, R->CoordList.Data[j + i].url_id);
	      R->CoordList.Data[i + j].site_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 1));
	      R->CoordList.Data[i + j].pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, i, 2));
	      if ((R->CoordList.Data[i + j].last_mod_time = DPS_ATOI(DpsSQLValue(&SQLres, i, 3))) == 0) {
		R->CoordList.Data[i + j].last_mod_time = DPS_ATOI(DpsSQLValue(&SQLres, i, 4));
	      }
	    }
	    DpsSQLFree(&SQLres);
	  }

	} else {
	  for (i = 0; i < R->CoordList.ncoords; i++) {
	    dps_snprintf(qbuf, sizeof(qbuf), "SELECT site_id,pop_rank,last_mod_time,since FROM url WHERE rec_id=%i", R->CoordList.Coords[i].url_id);
	    if (DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf)))
	      return rc;
	    if(DpsSQLNumRows(&SQLres)){
	      R->CoordList.Data[i].url_id = R->CoordList.Coords[i].url_id;
	      R->CoordList.Data[i].site_id = DPS_ATOI(DpsSQLValue(&SQLres, 0, 0));
	      R->CoordList.Data[i].pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, 0, 1));
	      if ((R->CoordList.Data[i].last_mod_time = DPS_ATOI(DpsSQLValue(&SQLres, 0, 2))) == 0) {
		R->CoordList.Data[i].last_mod_time = DPS_ATOI(DpsSQLValue(&SQLres, 0, 3));
	      }
	    }
	    DpsSQLFree(&SQLres);
	  }
	}
	return DPS_OK;
}

int DpsURLDataPreloadSQL(DPS_AGENT *Agent, DPS_DB *db) {
  DPS_URLDATA_FILE *DF;
  DPS_URLDATA *D;
  DPS_SQLRES	SQLres;
  size_t url_num = (size_t)DpsVarListFindUnsigned(&Agent->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
  size_t NFiles = (db->URLDataFiles) ? db->URLDataFiles : (size_t)DpsVarListFindUnsigned(&Agent->Conf->Vars, "URLDataFiles", 0x300);
  int filenum;
  size_t i, nrec = 0, mem_used = 0;
  unsigned long offset;
  int rc = DPS_OK, u;
  urlid_t url_id;
  char qbuf[256];

  if (Agent->Conf->URLDataFile == NULL) {
    size_t nitems = (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
    if ((Agent->Conf->URLDataFile = (DPS_URLDATA_FILE**)DpsXmalloc(nitems * sizeof(DPS_URLDATA_FILE) + 1)) == NULL)
      return DPS_ERROR;
  }
  if (Agent->Conf->URLDataFile[db->dbnum] == NULL) {
    if ((Agent->Conf->URLDataFile[db->dbnum] = (DPS_URLDATA_FILE*)DpsXmalloc(NFiles * sizeof(DPS_URLDATA_FILE))) == NULL) {
      TRACE_OUT(Agent);
      return DPS_ERROR;
    }
    mem_used += NFiles * sizeof(DPS_URLDATA_FILE);
  }
  DF = Agent->Conf->URLDataFile[db->dbnum];

  DpsSQLResInit(&SQLres);

  u = 1;
  offset = 0;
  while (u) {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT rec_id, site_id, pop_rank, last_mod_time FROM url ORDER BY rec_id LIMIT %d OFFSET %ld", 
	     url_num, offset);
    if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) {
      return rc;
    }

    nrec = DpsSQLNumRows(&SQLres);

    for(i = 0; i < nrec; i++) {
      url_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, i, 0));
      filenum = DPS_FILENO(url_id, NFiles);

      DF[filenum].URLData = (DPS_URLDATA*)DpsRealloc(DF[filenum].URLData, (DF[filenum].nrec + 1) * sizeof(DPS_URLDATA));
      if (DF[filenum].URLData == NULL) {
	DpsSQLFree(&SQLres);
	return DPS_ERROR;
      }
      D = &DF[filenum].URLData[DF[filenum].nrec];
      D->url_id = url_id;
      D->site_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 1));
      D->pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, i, 2));
      D->last_mod_time = DPS_ATOI(DpsSQLValue(&SQLres, i, 3));
      DF[filenum].nrec++;
    }
    DpsSQLFree(&SQLres);
    u = (nrec == url_num);
    offset += nrec;
    mem_used += nrec *  sizeof(DPS_URLDATA);
    DpsLog(Agent, DPS_LOG_EXTRA, "%d records processed", offset);
    if (u) DPSSLEEP(0);
  }
  DpsLog(Agent, DPS_LOG_INFO, "URL data preloaded. %u bytes of memory used", mem_used);
  return DPS_OK;
}


static int DpsSortAndGroupByURL(DPS_AGENT *A, DPS_RESULT *R, DPS_DB *db){
    unsigned long	ticks;
	int group_by_site = DpsGroupBySiteMode(DpsVarListFindStr(&A->Vars, "GroupBySite", NULL));
	int use_site_id = ( group_by_site && (DpsVarListFindInt(&A->Vars, "site", 0) == 0));
/*****/
	DpsLog(A,DPS_LOG_DEBUG,"Start group by url_id %d docs",R->CoordList.ncoords);
	ticks=DpsStartTimer();
	DpsGroupByURL(A, R);
	ticks=DpsStartTimer()-ticks;
	DpsLog(A,DPS_LOG_DEBUG,"Stop group by url_id:\t%.2f",(float)ticks/1000);


	DpsLog(A,DPS_LOG_DEBUG,"Start load url data %d docs",R->CoordList.ncoords);
	ticks=DpsStartTimer();
	DpsURLDataLoad(A, R, db);
	ticks=DpsStartTimer()-ticks;
	DpsLog(A,DPS_LOG_DEBUG,"Stop load url data:\t%.2f",(float)ticks/1000);

	R->grand_total = R->CoordList.ncoords;

	if (use_site_id && (group_by_site == DPS_GROUP_FULL)) {
	  DpsLog(A,DPS_LOG_DEBUG,"Start sort by site_id %d words",R->CoordList.ncoords);
	  if (R->CoordList.ncoords > 1) 
	    DpsSortSearchWordsBySite(R, &R->CoordList, R->CoordList.ncoords, DpsVarListFindStr(&A->Vars, "s", "RP"));
	  ticks=DpsStartTimer()-ticks;
	  DpsLog(A,DPS_LOG_DEBUG,"Stop sort by site_id:\t%.2f",(float)ticks/1000);
       
	  DpsLog(A,DPS_LOG_DEBUG,"Start group by site_id %d docs", R->CoordList.ncoords);
	  ticks=DpsStartTimer();
	  DpsGroupBySite(A, R);
	  ticks=DpsStartTimer() - ticks;
	  DpsLog(A,DPS_LOG_DEBUG,"Stop group by site_id:\t%.2f", (float)ticks/1000);
	}

	DpsLog(A,DPS_LOG_DEBUG,"Start SORT by PATTERN %d words", R->CoordList.ncoords);
	ticks=DpsStartTimer();
	DpsSortSearchWordsByPattern(R, &R->CoordList, R->CoordList.ncoords, DpsVarListFindStr(&A->Vars, "s", "RP"));
	ticks=DpsStartTimer()-ticks;
	DpsLog(A,DPS_LOG_DEBUG,"Stop SORT by PATTERN:\t%.2f",(float)ticks/1000);

	if (use_site_id && (group_by_site == DPS_GROUP_YES)) {
	  DpsLog(A,DPS_LOG_DEBUG,"Start group by site_id %d docs", R->CoordList.ncoords);
	  ticks=DpsStartTimer();
	  DpsGroupBySite(A, R);
	  ticks=DpsStartTimer() - ticks;
	  DpsLog(A,DPS_LOG_DEBUG,"Stop group by site_id:\t%.2f", (float)ticks/1000);
	} 

	R->total_found = R->CoordList.ncoords;
	return DPS_OK;
}

/********************************************************/


int DpsFindWordsSQL(DPS_AGENT * query, DPS_RESULT *Res, DPS_DB *db) {
	char		qbuf[1024*4];
	size_t		wordnum, nwords, i, nskipped;
	urlid_t         cur_url_id;
	DPS_SQLRES	SQLres;
	DPS_STACK_ITEM *pmerg = NULL;
	register DPS_URL_CRD_DB *Crd;
	int		word_match;
	int		wf[256];
	char	        *escwrd = NULL;
	const char	*where;
	int		use_crosswords;
	int		rc;
	int flag_null_wf;
	size_t z, idx;
	unsigned long 	ticks;

	TRACE_IN(query, "DpsFindWordsSQL");

	DpsSQLResInit(&SQLres);

	DpsPrepare(query, Res);	/* Prepare query    */
	DpsWWLBoolItems(Res);
	
	word_match = DpsMatchMode(DpsVarListFindStr(&query->Vars, "wm", "wrd"));
	where = BuildWhere(query, db);
	flag_null_wf = DpsWeightFactorsInit(DpsVarListFindStr(&query->Vars, "wf", ""), wf);
	if (where == NULL) {TRACE_OUT(query); return DPS_ERROR;}

	use_crosswords = ((query->Flags.use_crosswords > 0) /*&& (db->DBMode != DPS_DBMODE_CACHE)*/);
	nwords = Res->nitems - Res->ncmds;
	
	if ((pmerg = (DPS_STACK_ITEM*)DpsXmalloc((MAXMULTI + 2) * sizeof(DPS_STACK_ITEM))) == NULL) {
	  DpsLog(query, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", (nwords + 1) * sizeof(DPS_STACK_ITEM *), __FILE__, __LINE__);
	  TRACE_OUT(query);
	  return DPS_ERROR;
	}

	for(z = wordnum = 0; wordnum < Res->nitems; wordnum++) {
	  if (Res->items[wordnum].cmd != DPS_STACK_WORD) continue;
	  if (z < Res->items[wordnum].len) z = Res->items[wordnum].len;
	}
	if ((escwrd = (char*)DpsMalloc(2 * z + 1)) == NULL) {
	  DpsLog(query, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", 2 * z + 1, __FILE__, __LINE__);
	  DPS_FREE(pmerg);
	  TRACE_OUT(query);
	  return DPS_ERROR;
	}

	/* Now find each word */
	for(wordnum = 0; wordnum < Res->nitems; wordnum++) {
		size_t	numrows, tnum, tmin, tmax, tlst = 0;
		char	tablename[32]="dict";
		
		if (Res->items[wordnum].cmd != DPS_STACK_WORD) continue;
		if (Res->items[wordnum].origin & DPS_WORD_ORIGIN_STOP) continue;
		
		for (z = 0 ; z < wordnum; z++) {
		  if (Res->items[z].cmd != DPS_STACK_WORD) continue;
		  if (Res->items[z].origin & DPS_WORD_ORIGIN_STOP) continue;
		  if (Res->items[z].crcword == Res->items[wordnum].crcword && (Res->items[z].secno == 0 || Res->items[z].secno == Res->items[wordnum].secno)) break;
		}
		if (z < wordnum) continue;
	  
		(void)DpsDBEscStr(db, escwrd, Res->items[wordnum].word, Res->items[wordnum].len);

		if((db->DBMode==DPS_DBMODE_MULTI)&&(word_match!=DPS_MATCH_FULL)){
			/* This is for substring search!  */
			/* In Multi mode: we have to scan */
			/* almost all tables except those */
			/* with to short words            */
			
		        tmin = Res->items[wordnum].len;
			tmax = dps_max(tmin, 32);
		}else{
			tmin = tmax = DICTNUM(Res->items[wordnum].len);
		}

		for(tnum = tmin; tnum <= tmax; tnum++) {

		  if(tlst != DICTNUM(tnum)) {
		    tlst = DICTNUM(tnum);

		    ticks=DpsStartTimer();
		    DpsLog(query,DPS_LOG_DEBUG,"Start search for '%s'(%d)", Res->items[wordnum].word, Res->items[wordnum].secno);
				
		    switch(db->DBMode){
		    case DPS_DBMODE_MULTI:
		      sprintf(tablename, "dict%zu", tlst);
		      break;
		    case DPS_DBMODE_MULTI_CRC:
		      sprintf(tablename, "ndict%zu", tlst);
		      break;
		    case DPS_DBMODE_SINGLE_CRC:
		      dps_strcpy(tablename, "ndict");
		      break;
		    default:
		      break;
		    }
		    if((db->DBMode==DPS_DBMODE_SINGLE_CRC)||(db->DBMode==DPS_DBMODE_MULTI_CRC)){
		      dpshash32_t crc = Res->items[wordnum].crcword;
		      if(where[0]){
			dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT %s.url_id,%s.intag \
FROM %s, url%s \
WHERE %s.word_id=%d \
AND url.rec_id=%s.url_id AND %s ORDER BY SIGN(%s.url_id)DESC,ABS(%s.url_id),intag",
				     tablename,tablename,
				     tablename, db->from, tablename,
				     crc, tablename, where, tablename, tablename);
		      }else{
			dps_snprintf(qbuf,sizeof(qbuf)-1,"SELECT url_id,intag FROM %s WHERE %s.word_id=%d ORDER BY SIGN(url_id)DESC,ABS(url_id),intag",
				     tablename,tablename,crc);
		      }
		    }else{
		      char cmparg[256];
		      switch(word_match){	
		      case DPS_MATCH_BEGIN:
			sprintf(cmparg," LIKE '%s%%'",escwrd);
			break;
		      case DPS_MATCH_END:
			sprintf(cmparg," LIKE '%%%s'",escwrd);
			break;
		      case DPS_MATCH_SUBSTR:
			sprintf(cmparg," LIKE '%%%s%%'",escwrd);
			break;
		      case DPS_MATCH_FULL:
		      default:
			sprintf(cmparg," = '%s'",escwrd);
			break;
		      }
		      if(where[0]){
			dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT %s.url_id,%s.intag \
FROM %s, url%s \
WHERE %s.word%s \
AND url.rec_id=%s.url_id AND %s ORDER BY SIGN(%s.url_id)DESC,ABS(%s.url_id),intag",
				     tablename,tablename,
				     tablename, db->from, tablename,
				     cmparg, tablename, where, tablename, tablename);
		      }else{
			dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT url_id,intag FROM %s,url WHERE %s.word%s AND url.rec_id=%s.url_id ORDER BY SIGN(url_id)DESC,ABS(url_id),intag",
				     tablename, tablename, cmparg, tablename);
		      }
		    }
		    if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		      DPS_FREE(pmerg);
		      DPS_FREE(escwrd);
		      TRACE_OUT(query);
		      return rc;
		    }
					
		    numrows = DpsSQLNumRows(&SQLres);
		    ticks = DpsStartTimer()-ticks;
		    DpsLog(query, DPS_LOG_DEBUG, "Stop search for '%s'\t%.2f  %d found", 
			   Res->items[wordnum].word, (float)ticks/1000,numrows);
				
		    /* Add new found word to the list */
/*				pmerg[npmerge] = &Res->items[wordnum];*/
		    pmerg[tlst].pcur = pmerg[tlst].pbegin = pmerg[tlst].pchecked = 
		      (DPS_URL_CRD_DB*)DpsMalloc(numrows * sizeof(DPS_URL_CRD_DB) + 1);

		    if (pmerg[tlst].pbegin == NULL) {
		      DpsSQLFree(&SQLres);
		      for (i = tmin; i <= MAXMULTI + 1; i++) { DPS_FREE(pmerg[i].pbegin); DPS_FREE(pmerg[i].db_pbegin); }
		      DPS_FREE(escwrd);
		      DPS_FREE(pmerg);
		      TRACE_OUT(query);
		      return DPS_ERROR;
		    }
		    for (i = 0; i < numrows; i++) {
		      pmerg[tlst].pcur[i].url_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 0));
		      pmerg[tlst].pcur[i].coord = DPS_ATOI(DpsSQLValue(&SQLres, i, 1));
		    }
		    pmerg[tlst].count = numrows;
		    if (flag_null_wf || Res->items[wordnum].secno) {
		      pmerg[tlst].count = DpsRemoveNullSectionsDB(pmerg[tlst].pcur, numrows, wf, Res->items[wordnum].secno);
		    }
		    pmerg[tlst].plast = &pmerg[tlst].pcur[pmerg[tlst].count];
		    Res->CoordList.ncoords += pmerg[tlst].count;
/*				npmerge++;*/
				
		    DpsSQLFree(&SQLres);

		  }
		}
		/* Now find each word in crosstable */
		if (use_crosswords) {
		  ticks = DpsStartTimer();
		  DpsLog(query,DPS_LOG_DEBUG,"Start search crosswords for '%s'(%d)", Res->items[wordnum].word, Res->items[wordnum].secno);
		
		  switch(db->DBMode) {
		  case DPS_DBMODE_SINGLE_CRC:
		  case DPS_DBMODE_MULTI_CRC:
		  case DPS_DBMODE_CACHE:
		    dps_strcpy(tablename, "ncrossdict");
		    break;
		  case DPS_DBMODE_MULTI:
		  default:
		    dps_strcpy(tablename,"crossdict");
		    break;
		  }
		  if((db->DBMode==DPS_DBMODE_SINGLE_CRC)||
		     (db->DBMode==DPS_DBMODE_MULTI_CRC) || (db->DBMode == DPS_DBMODE_CACHE)) {
		    int crc;
		    crc = Res->items[wordnum].crcword;
		    if(where[0]){
		      dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT %s.url_id,%s.intag \
FROM %s, url%s \
WHERE %s.word_id=%d \
AND url.rec_id=%s.url_id AND %s ORDER BY SIGN(%s.url_id)DESC,ABS(%s.url_id),intag",
				   tablename,tablename,
				   tablename, db->from, tablename,
				   crc, tablename, where, tablename, tablename);
		    }else{
		      dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT url_id,intag FROM %s,url WHERE %s.word_id=%d AND url.rec_id=%s.url_id ORDER BY SIGN(url_id)DESC,ABS(url_id),intag", 
				   tablename, tablename, crc, tablename);
		    }
		  }else{
		    char cmparg[256];
		    switch(word_match){	
		    case DPS_MATCH_BEGIN:
		      sprintf(cmparg," LIKE '%s%%'",escwrd);
		      break;
		    case DPS_MATCH_END:
		      sprintf(cmparg," LIKE '%%%s'",escwrd);
		      break;
		    case DPS_MATCH_SUBSTR:
		      sprintf(cmparg," LIKE '%%%s%%'",escwrd);
		      break;
		    case DPS_MATCH_FULL:
		    default:
		      sprintf(cmparg," = '%s'",escwrd);
		      break;
		    }
		    if(where[0]){

		      dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT %s.url_id,%s.intag \
FROM %s, url%s \
WHERE %s.word%s \
AND url.rec_id=%s.url_id AND %s ORDER BY SIGN(%s.url_id)DESC,ABS(%s.url_id),intag",
				   tablename,tablename,
				   tablename, db->from, tablename,
				   cmparg, tablename, where, tablename, tablename);
		    } else {
		      dps_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT url_id,intag FROM %s,url WHERE %s.word%s AND url.rec_id=%s.url_id ORDER BY SIGN(url_id)DESC,ABS(url_id),intag",
				   tablename, tablename, cmparg, tablename);
		    }
		  }
		  if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		    DPS_FREE(pmerg);
		    DPS_FREE(escwrd);
		    TRACE_OUT(query);
		    return rc;
		  }
		  
		  numrows = DpsSQLNumRows(&SQLres);
		  ticks = DpsStartTimer()-ticks;
		  DpsLog(query,DPS_LOG_DEBUG,"Stop search crosswords for '%s'\t%.2f %d found",
			 Res->items[wordnum].word, (float)ticks / 1000, numrows);
		
		  /* Add new found word to the list */
		  idx = MAXMULTI + 1;
		  pmerg[idx].pcur = pmerg[idx].pbegin = pmerg[idx].pchecked = (DPS_URL_CRD_DB*)DpsMalloc(numrows * sizeof(DPS_URL_CRD_DB) + 1);
		
		  if (pmerg[idx].pbegin == NULL) {
		    DpsSQLFree(&SQLres);
		    for (i = tmin; i <= MAXMULTI + 1; i++) { DPS_FREE(pmerg[i].pbegin); DPS_FREE(pmerg[i].db_pbegin); }
		    DPS_FREE(escwrd);
		    DPS_FREE(pmerg);
		    TRACE_OUT(query);
		    return DPS_ERROR;
		  }
		  for (i = 0; i < numrows; i++) {
		    pmerg[idx].pcur[i].url_id = DPS_ATOI(DpsSQLValue(&SQLres, i, 0));
		    pmerg[idx].pcur[i].coord = DPS_ATOI(DpsSQLValue(&SQLres, i, 1));
		  }
		  pmerg[idx].count = numrows;
		  if (flag_null_wf || Res->items[wordnum].secno) {
		    pmerg[idx].count = DpsRemoveNullSectionsDB(pmerg[idx].pcur, numrows, wf, Res->items[wordnum].secno);
		  }
		  pmerg[idx].plast = &pmerg[idx].pcur[pmerg[idx].count];
		  Res->CoordList.ncoords += pmerg[idx].count;

		  DpsSQLFree(&SQLres);
		}

/*******************************/

		Res->items[wordnum].count = 0;
		for(i = tmin; i <= tmax; i++) {
		  Res->items[wordnum].count += pmerg[i].count;
		}
		Res->items[wordnum].count += pmerg[MAXMULTI + 1].count;

		Res->items[wordnum].pcur = Res->items[wordnum].pbegin = Res->items[wordnum].pchecked = 
				  (DPS_URL_CRD_DB*)DpsMalloc(Res->items[wordnum].count * sizeof(DPS_URL_CRD_DB) + 1);
		Crd = Res->items[wordnum].pcur;

		while (1) {
		  nskipped = 0;
		  for (i = 0; i <= MAXMULTI + 1; i++) {
		    if ((pmerg[i].pcur != NULL) && (pmerg[i].pcur < pmerg[i].plast)) {
		      cur_url_id = pmerg[i].pcur->url_id;
		      i++;
		      break;
		    }
		    nskipped++;
		  }

		  if (nskipped >= MAXMULTI + 2) break;
		  for( ; i <= MAXMULTI + 1; i++) {
		    if ((pmerg[i].pcur != NULL) && (pmerg[i].pcur < pmerg[i].plast)) {
		      if (cur_url_id > pmerg[i].pcur->url_id) cur_url_id = pmerg[i].pcur->url_id;
		    }
		  }
	  
		  for(i = 0; i <= MAXMULTI + 1; i++) {
		    if (pmerg[i].pcur != NULL) {
		      while ((pmerg[i].pcur < pmerg[i].plast) && (pmerg[i].pcur->url_id == cur_url_id) ) {
			Crd->coord = pmerg[i].pcur->coord;
			if (pmerg[i].secno == 0 || DPS_WRDSEC(Crd->coord) == pmerg[i].secno) {
/*			  register size_t mlen = (Crd->coord & 0xFF);            We don't need to check ulen here for SQL-based modes!!!
			  if (mlen == 0 || mlen == pmerg[i].ulen) {*/
			    Crd->url_id = pmerg[i].pcur->url_id;
			    Crd->coord &= 0xFFFFFF00;
			    Crd->coord += (Res->items[wordnum].wordnum /*order*/ & 0xFF);
#ifdef WITH_MULTIDBADDR
			    Crd->dbnum = db->dbnum;
#endif		  
			    pmerg[i].pchecked++;
			    Crd++;
/*			  }*/
			}
			pmerg[i].pcur++;
		      }
		    }
		  }

		}

		for (i = tmin; i <= MAXMULTI + 1; i++) {
		  DPS_FREE(pmerg[i].pbegin); DPS_FREE(pmerg[i].db_pbegin);
		  bzero(&pmerg[i], sizeof(DPS_STACK_ITEM));
		}

	}
/*	
	for(i = 0; i < (size_t)npmerge; i++) {
	  Res->CoordList.ncoords += pmerg[i]->count;
	}
*/
	DPS_FREE(escwrd);
	DPS_FREE(pmerg);
	DpsSortAndGroupByURL(query, Res, db);
	
	DpsWWLBoolItems(Res);
	TRACE_OUT(query);
	return DPS_OK;
}


int DpsTrackSQL(DPS_AGENT *query, DPS_RESULT *Res, DPS_DB *db) {
        DPS_SQLRES      sqlRes;
	char		*qbuf;
	char		*text_escaped;
	const char	*words = DpsVarListFindStr(&query->Vars, "q", ""); /* "q-lc" was here */
	const char      *IP = DpsVarListFindStr(&query->Vars, "IP", "localhost");
	size_t          i, r, escaped_len, qbuf_len, cmd_len;
	int             res, qtime, rec_id;
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	char            *cmd_escaped = NULL;
	
	if (*words == '\0') return DPS_OK; /* do not store empty queries */

	DpsSQLResInit(&sqlRes);

	escaped_len = dps_max(4 * dps_strlen(words), 256) + 1;
	qbuf_len = escaped_len + 4096;

	if ((qbuf = (char*)DpsMalloc(qbuf_len)) == NULL) return DPS_ERROR;
	if ((text_escaped = (char*)DpsMalloc(escaped_len)) == NULL) { DPS_FREE(qbuf); return DPS_ERROR; }
	
	/* Escape text to track it  */
	(void)DpsDBEscStr(db, text_escaped, words, dps_strlen(words));
	
	dps_snprintf(qbuf, qbuf_len - 1, "INSERT INTO qtrack (ip,qwords,qtime,found,wtime) VALUES ('%s','%s',%d,%d,%d)",
		     IP, text_escaped, qtime = (int)time(NULL), Res->total_found, Res->work_time
		 );

	res = DpsSQLAsyncQuery(db, NULL, qbuf);
	if (res != DPS_OK) goto DpsTrack_exit;
       
	dps_snprintf(qbuf, qbuf_len - 1, "SELECT rec_id FROM qtrack WHERE ip='%s' AND qtime=%d", IP, qtime);
	res = DpsSQLQuery(db, &sqlRes, qbuf);
	if (res != DPS_OK) goto DpsTrack_exit;
	if (DpsSQLNumRows(&sqlRes) == 0) { DpsSQLFree(&sqlRes); res = DPS_ERROR; goto DpsTrack_exit; }
	rec_id = DPS_ATOI(DpsSQLValue(&sqlRes, 0, 0));
	DpsSQLFree(&sqlRes);

	r = (size_t)'q';
	for (i = 0; i < query->Vars.Root[r].nvars; i++) {
	  DPS_VAR *Var = &query->Vars.Root[r].Var[i];
	  if (strncasecmp(Var->name, "query.",6)==0 && strcasecmp(Var->name, "query.q") && strcasecmp(Var->name, "query.BrowserCharset")
	      && strcasecmp(Var->name, "query.g-lc") && strncasecmp(Var->name, "query.Excerpt", 13) 
	      && strcasecmp(Var->name, "query.IP")  && strcasecmp(Var->name, "query.DateFormat") && Var->val != NULL && *Var->val != '\0') {

	    cmd_escaped = DpsDBEscStr(db, NULL, &Var->name[6], dps_strlen(&Var->name[6])); /* Escape parameter name */
	    (void)DpsDBEscStr(db, text_escaped, Var->val, Var->curlen); /* Escape parameter value */

	    dps_snprintf(qbuf, qbuf_len, "INSERT INTO qinfo (q_id,name,value) VALUES (%s%i%s,'%s','%s')", 
			 qu, rec_id, qu, cmd_escaped, text_escaped);
	    res = DpsSQLAsyncQuery(db, NULL, qbuf);
	    DPS_FREE(cmd_escaped);
	    if (res != DPS_OK) goto DpsTrack_exit;
	  }
	}
DpsTrack_exit:
	DPS_FREE(text_escaped);
	DPS_FREE(qbuf);
	return res;
}

static void SQLResToDoc(DPS_ENV *Conf, DPS_DOCUMENT *D, DPS_SQLRES *sqlres, size_t i) {
	char		dbuf[128];
	time_t		last_mod_time;
#ifdef HAVE_PTHREAD
	struct tm       l_tim;
#endif
	const char	*format = DpsVarListFindStrTxt(&Conf->Vars, "DateFormat", "%a, %d %b %Y, %X %Z");
	double          pr;
	const char      *url;
	char            *dc_url;
	size_t          len;
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        lc_dc;

	loccs = Conf->lcs;
	if(!loccs) loccs = DpsGetCharSet("iso-8859-1");

	D->charset_id = DPS_ATOI(DpsSQLValue(sqlres, i, 9));

	doccs = DpsGetCharSetByID(D->charset_id);
	if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
	DpsConvInit(&lc_dc, loccs, doccs, Conf->CharsToEscape, DPS_RECODE_URL);
	
	len = dps_strlen(url = DpsSQLValue(sqlres, i, 1));
	dc_url = (char*)DpsMalloc((size_t)(24 * len + 1));
	if (dc_url == NULL) return;
	/* Convert URL from LocalCharset */
	DpsConv(&lc_dc, dc_url, (size_t)24 * len,  url, (size_t)(len + 1));
	DpsVarListReplaceStr(&D->Sections, "URL", dc_url);

	DpsVarListDel(&D->Sections, "URL_ID");
	DPS_FREE(dc_url);

	if ((last_mod_time = atol(DpsSQLValue(sqlres,i,2))) > 0) {
	  if (strftime(dbuf, 128, format, 
#ifdef HAVE_PTHREAD
		       localtime_r(&last_mod_time, &l_tim)
#else
		       localtime(&last_mod_time)
#endif
		       ) == 0) {
	    DpsTime_t2HttpStr(last_mod_time, dbuf);
	  }
	  DpsVarListReplaceStr(&D->Sections,"Last-Modified",dbuf);
	}
	DpsVarListReplaceStr(&D->Sections,"Content-Length",DpsSQLValue(sqlres,i,3));
	DpsVarListReplaceStr(&D->Sections,"Next-Index-Time", DpsSQLValue(sqlres, i, 4));
	DpsVarListReplaceInt(&D->Sections, "Referrer-ID", DPS_ATOI(DpsSQLValue(sqlres,i,5)));
	DpsVarListReplaceInt(&D->Sections,"crc32",atoi(DpsSQLValue(sqlres,i,6)));
	DpsVarListReplaceStr(&D->Sections, "Site_id", DpsSQLValue(sqlres, i, 7));
	pr = dps_atof(DpsSQLValue(sqlres, i, 8));
	dps_snprintf(dbuf, 128, "%.5f", pr);
	DpsVarListReplaceStr(&D->Sections, "Pop_Rank", dbuf);
}


static int DpsSitemap(DPS_AGENT *A, DPS_DB *db) {
  char timestr[64];
  char priostr[32];
  DPS_SQLRES	SQLres;
  DPS_CHARSET	*loccs, *utf8cs;
  DPS_CONV      lc_utf8;
  long offset = 0L;
  double PRmin, PRmax;
  time_t last_mod_time, diff;
#ifdef HAVE_GMTIME_R
  struct tm l_tim;
#endif
  int u = 1, rc = DPS_OK;
  size_t len, url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLSelectCacheSize", DPS_URL_SELECT_CACHE_SIZE);
  urlid_t rec_id = 0;
  size_t i, nrows, qbuflen;
  char *qbuf, *url, *dc_url, *pp;
  const char *freq, *where;

  loccs = A->Conf->lcs;
  if(!loccs) loccs = DpsGetCharSet("iso-8859-1");
  utf8cs = DpsGetCharSet("UTF-8");
  DpsConvInit(&lc_utf8, loccs, utf8cs, A->Conf->CharsToEscape, DPS_RECODE_URL | DPS_RECODE_HTML_TO);

  where = BuildWhere(A, db);
  if (where == NULL) return DPS_ERROR;

  if ((qbuf = (char*)DpsMalloc(qbuflen = 1024)) == NULL) {
    return DPS_ERROR;
  }

  DpsSQLResInit(&SQLres);

  dps_snprintf(qbuf, qbuflen, "SELECT MIN(rec_id),MIN(pop_rank),MAX(pop_rank) FROM url");
  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
  rc = DpsSQLQuery(db, &SQLres, qbuf);
  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
  if(DPS_OK != rc) {
    DPS_FREE(qbuf);
    return rc;
  }
  rec_id = DPS_ATOI(DpsSQLValue(&SQLres, 0, 0)) - 1;
  PRmin = DPS_ATOF(DpsSQLValue(&SQLres, 0, 1));
  PRmax = DPS_ATOF(DpsSQLValue(&SQLres, 0, 2));
  DpsSQLFree(&SQLres);


  printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  printf("<urlset xmlns=\"http://www.google.com/schemas/sitemap/0.84\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.google.com/schemas/sitemap/0.84 http://www.google.com/schemas/sitemap/0.84/sitemap.xsd\">\n");

  while (u) {
    dps_snprintf(qbuf, qbuflen, 
		 "SELECT url,last_mod_time,rec_id,pop_rank FROM url WHERE %s%srec_id > %d AND (status=0 OR (status>=200 AND status< 400) OR (status>2200 AND status<2400)) ORDER BY rec_id LIMIT %d", 
		 where[0] ? where : "", where[0] ? " AND ": "", rec_id, url_num);
    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
    rc = DpsSQLQuery(db, &SQLres, qbuf);
    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
    if(DPS_OK != rc) {
      DPS_FREE(qbuf);
      return rc;
    }
    nrows = DpsSQLNumRows(&SQLres);

    for(i = 0; i < nrows; i++) {

      last_mod_time = atol(DpsSQLValue(&SQLres, i, 1));
      strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S+00:00", 
#ifdef HAVE_GMTIME_R
	       gmtime_r(&last_mod_time, &l_tim)
#else
	       gmtime(&last_mod_time)
#endif
	       );

      diff = A->now - last_mod_time;
      if (diff < 3600) freq = "hourly";
      else if (diff < 24 * 3600) freq = "daily";
      else if (diff < 7 * 24 * 3600) freq = "weekly";
      else if (diff < 31 * 24 * 3600) freq = "monthly";
      else if (diff < 366 * 24 * 3600) freq = "yearly";
      else freq = "never";

      len = dps_strlen(url = DpsSQLValue(&SQLres,i,0));
      dc_url = (char*)DpsMalloc((size_t)(24 * len + 1));
      if (dc_url == NULL) continue;
      /* Convert URL from LocalCharset */
      DpsConv(&lc_utf8, dc_url, (size_t)24 * len,  url, (size_t)(len + 1));

      dps_snprintf(priostr, sizeof(priostr), "%f", (DPS_ATOF(DpsSQLValue(&SQLres, i, 3)) - PRmin) / (PRmax - PRmin + 0.00001));
      if ((pp = strchr(priostr, (int)',')) != NULL) {
	*pp = '.';
      }
      for (pp = priostr + dps_strlen(priostr) - 1;
	   pp > priostr && (*pp == '0' || *pp == '.'); pp--) *pp = '\0';

      printf("<url><loc>%s</loc><lastmod>%s</lastmod><changefreq>%s</changefreq><priority>%s</priority></url>\n", dc_url, timestr, freq, priostr);

      DPS_FREE(dc_url);

    }

    if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, nrows - 1, 2));
    u = (nrows == url_num);
    offset += nrows;
    DpsLog(A, DPS_LOG_EXTRA, "%ld records processed at %d", offset, rec_id);
    DpsSQLFree(&SQLres);
    if (u) DPSSLEEP(0);
  }
  printf("</urlset>\n");


  DPS_FREE(qbuf);
  return rc;
}


static int DpsDocInfoRefresh(DPS_AGENT *A, DPS_DB *db) {
  DPS_RESULT *Res;
  DPS_SQLRES	SQLres;
  long offset = 0L;
  int u = 1, rc = DPS_OK;
  size_t i, nrows, qbuflen, ncached;
  size_t url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLSelectCacheSize", DPS_URL_SELECT_CACHE_SIZE);
  char *qbuf;
  urlid_t rec_id = 0;
    
  if ((qbuf = (char*)DpsMalloc(qbuflen = 1024)) == NULL) {
    return DPS_ERROR;
  }
	
  DpsSQLResInit(&SQLres);
  while (u) {
    dps_snprintf(qbuf, qbuflen, 
		 "SELECT rec_id FROM url WHERE rec_id > %d AND (status=200 OR status=206 OR status=302 OR status=304 OR status=303 OR status=307) ORDER BY rec_id LIMIT %d", 
		 rec_id, url_num);
    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
    rc = DpsSQLQuery(db, &SQLres, qbuf);
    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
    if(DPS_OK != rc) {
      DPS_FREE(qbuf);
      return rc;
    }
    nrows = DpsSQLNumRows(&SQLres);

    Res = DpsResultInit(NULL);
    if (Res == NULL) {
      DPS_FREE(qbuf);
      DpsSQLFree(&SQLres);
      return DPS_ERROR;
    }
    Res->Doc = (DPS_DOCUMENT*)DpsMalloc(sizeof(DPS_DOCUMENT) * nrows + 1);
    if (Res->Doc == NULL) {
      DPS_FREE(qbuf);
      DpsSQLFree(&SQLres);
      DpsResultFree(Res);
      return DPS_ERROR;
    }

    for(i = 0; i < nrows; i++) {
      DpsDocInit(&Res->Doc[i]);
      DpsVarListReplaceStr(&Res->Doc[i].Sections, "DP_ID", DpsSQLValue(&SQLres, i, 0));
    }

    Res->num_rows = nrows;

    if (db->DBMode == DPS_DBMODE_CACHE) {
      rc = DpsResAddDocInfoCache(A, db, Res, db->dbnum);
    }

    ncached = Res->fetched;

    if (A->Flags.URLInfoSQL) {

#ifdef HAVE_SQL
      rc = DpsResAddDocInfoSQL(A, db, Res, db->dbnum);
#endif
    }else {
      char sbuf[512];
      for(i = 0; i < nrows; i++) {
	if (Res->Doc[i].fetched) continue;
	dps_strcpy(sbuf, "UPDATE url SET next_index_time=0,last_mod_time=0,crc32=0,status=0 WHERE rec_id=");
	dps_strcat(sbuf, DpsVarListFindStr(&Res->Doc[i].Sections, "DP_ID", "0") );
	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	rc = DpsSQLAsyncQuery(db, NULL, sbuf);
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	if(DPS_OK != rc) {
	  DPS_FREE(qbuf);
	  return rc;
	}
      }
    }

    DpsResultFree(Res);

    if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, nrows - 1, 0));
    u = (nrows == url_num);
    offset += nrows;
    DpsLog(A, DPS_LOG_EXTRA, "%ld records processed. %d cached of last %d (%.2f%%) at %d", 
	   offset, ncached, nrows, 100.0 * ncached / nrows, rec_id);
    DpsSQLFree(&SQLres);
    if (u) DPSSLEEP(0);
  }

  DPS_FREE(qbuf);
  return rc;
}


static int DpsStoredRehash(DPS_AGENT *A, DPS_DB *db) {
  DPS_RESULT *Res;
  DPS_SQLRES	SQLres;
  DPS_BASE_PARAM P;
  long offset = 0L;
  int u = 1, rc = DPS_OK;
  size_t i, nrows, qbuflen, ncached;
  size_t url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLSelectCacheSize", DPS_URL_SELECT_CACHE_SIZE);
  char *qbuf;
  urlid_t rec_id = 0;
  unsigned int NFiles = DpsVarListFindInt(&A->Vars, "StoredFiles", 0x100);
    
  if ((qbuf = (char*)DpsMalloc(qbuflen = 1024)) == NULL) {
    return DPS_ERROR;
  }
	
  bzero(&P, sizeof(P));
  P.subdir = "store";
  P.basename = "doc";
  P.indname = "doc";
  P.mode = DPS_WRITE_LOCK;
  P.NFiles = (db->StoredFiles) ? db->StoredFiles : NFiles;
  P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&A->Vars, "VarDir", DPS_VAR_DIR);
  P.A = A;

  DpsSQLResInit(&SQLres);
  while (u) {
    dps_snprintf(qbuf, qbuflen, 
		 "SELECT rec_id FROM url WHERE rec_id > %d AND (status=200 OR status=206 OR status=302 OR status=304 OR status=303 OR status=307) ORDER BY rec_id LIMIT %d", 
		 rec_id, url_num);
    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
    rc = DpsSQLQuery(db, &SQLres, qbuf);
    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
    if(DPS_OK != rc) {
      DPS_FREE(qbuf);
      return rc;
    }
    nrows = DpsSQLNumRows(&SQLres);

    Res = DpsResultInit(NULL);
    if (Res == NULL) {
      DPS_FREE(qbuf);
      DpsSQLFree(&SQLres);
      return DPS_ERROR;
    }
    Res->Doc = (DPS_DOCUMENT*)DpsMalloc(sizeof(DPS_DOCUMENT) * nrows + 1);
    if (Res->Doc == NULL) {
      DPS_FREE(qbuf);
      DpsSQLFree(&SQLres);
      DpsResultFree(Res);
      return DPS_ERROR;
    }

    for(i = 0; i < nrows; i++) {
      DpsDocInit(&Res->Doc[i]);
      DpsVarListReplaceStr(&Res->Doc[i].Sections, "DP_ID", DpsSQLValue(&SQLres, i, 0));
    }

    Res->num_rows = nrows;

    if (db->DBMode == DPS_DBMODE_CACHE) {
      rc = DpsResAddDocInfoCache(A, db, Res, 0);
    }

    ncached = Res->fetched;

    if (A->Flags.URLInfoSQL) {

#ifdef HAVE_SQL
      rc = DpsResAddDocInfoSQL(A, db, Res, i);
#endif
    }

    for(i = 0; i < nrows; i++) {
      P.rec_id = rec_id = DpsURL_ID(&Res->Doc[i], NULL);
      if (DpsBaseSeek(&P, DPS_READ_LOCK) != DPS_OK) continue;
      if (P.Item.rec_id == P.rec_id) continue;
      P.rec_id = DpsStrHash32(DpsVarListFindStr(&Res->Doc[i].Sections, "URL", ""));
      if (DpsBaseSeek(&P, DPS_WRITE_LOCK) != DPS_OK) continue;
      if (P.Item.rec_id != P.rec_id) continue;
      if (ncached > 0) ncached--;
      DpsVarListReplaceInt(&Res->Doc[i].Sections, "URL_ID", P.rec_id);
      DpsUnStoreDoc(A, &Res->Doc[i], NULL);
      DpsBaseDelete(&P);
      DpsVarListReplaceInt(&Res->Doc[i].Sections, "URL_ID", rec_id);
      DpsStoreDoc(A, &Res->Doc[i], NULL);

    }

    DpsResultFree(Res);

    if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, nrows - 1, 0));
    u = ((nrows == url_num) || (rec_id == 0));
    offset += nrows;
    DpsLog(A, DPS_LOG_EXTRA, "%ld records processed. %d cached of last %d (%.2f%%) at %d", 
	   offset, ncached, nrows, 100.0 * ncached / nrows, rec_id);
    DpsSQLFree(&SQLres);
    if (u) DPSSLEEP(0);
  }

  DPS_FREE(qbuf);
  return rc;
}


static int DpsDocPostponeSite(DPS_AGENT *A, DPS_DOCUMENT *Doc, DPS_DB *db) {
  char qbuf[512];
  int i, site_id = DpsVarListFindInt(&Doc->Sections, "site_id", 0);
  char *already;

  if (site_id == 0) return DPS_OK;

  for (i = 0; i < DPS_SERVERID_CACHE_SIZE; i++) {
    if (A->ServerIdCache[i].Id == site_id) {
      if (A->ServerIdCache[i].OnErrored) return DPS_OK;
      A->ServerIdCache[i].OnErrored = 1;
      break;
    }
  }
  dps_snprintf(qbuf, sizeof(qbuf), "%dpp", site_id);
  DPS_GETLOCK(A, DPS_LOCK_CONF);
  already = DpsVarListFindStr(&A->Conf->Vars, qbuf, NULL);
  if (already == NULL) {
    DpsVarListReplaceStr(&A->Conf->Vars, qbuf, "");
    DPS_RELEASELOCK(A, DPS_LOCK_CONF);
    dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET next_index_time=%lu WHERE site_id=%d", A->now + Doc->Spider.net_error_delay_time, site_id);
    return DpsSQLAsyncQuery(db, NULL, qbuf);
  }
  DPS_RELEASELOCK(A, DPS_LOCK_CONF);
  return DPS_OK;
}


static int DpsRefererGet(DPS_AGENT *A, DPS_DOCUMENT *Doc, DPS_DB *db) {
  char	qbuf[128];
  DPS_SQLRES Res;
  urlid_t url_id = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
  int rc = DPS_OK;

  if (url_id != 0 && url_id != -1) {
    DpsSQLResInit(&Res);
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT url FROM url WHERE rec_id=%d", url_id);
    if(DPS_OK != (rc = DpsSQLQuery(db, &Res, qbuf))) {
      return rc;
    }
    if (DpsSQLNumRows(&Res)) {
      DpsVarListReplaceStr(&Doc->RequestHeaders, "Referer", DpsSQLValue(&Res, 0, 0));
    }
    DpsSQLFree(&Res);
  }
  return rc;
}


int DpsResAddDocInfoSQL(DPS_AGENT *query, DPS_DB *db, DPS_RESULT *Res, size_t dbnum) {
  size_t		i, qbuflen;
	char		*instr;
	char		*qbuf;
	DPS_SQLRES	SQLres;
	int		rc;
	int		use_showcnt = !strcasecmp(DpsVarListFindStr(&query->Vars, "PopRankUseShowCnt", "no"), "yes");
	double          pr, ratio = 0.0;
	DPS_VAR		*Sec, DPS_SEC;
	
	TRACE_IN(query, "DpsResAddDocInfoSQL");

	DpsSQLResInit(&SQLres);

	if(!Res->num_rows) {
	  TRACE_OUT(query);
	  return DPS_OK;
	}
	if (Res->fetched >= Res->num_rows) {
	  TRACE_OUT(query);
	  return DPS_OK;
	}

	if ((qbuf = (char*)DpsMalloc(qbuflen = 1024 + (Res->num_rows - Res->fetched) * 24)) == NULL) {
	  return DPS_ERROR;
	}
	if ((instr = (char*)DpsMalloc(qbuflen)) == NULL) {
	  DPS_FREE(qbuf);
	  return DPS_ERROR;
	}
	instr[0] = qbuf[0] = '\0';


/*	DpsLog(query, DPS_LOG_ERROR, "num_rows: %d   fetched: %d", Res->num_rows, Res->fetched);*/
	if (use_showcnt) ratio = DpsVarListFindDouble(&query->Vars, "PopRankShowCntRatio", 25.0);
	DpsLog(query, DPS_LOG_DEBUG, "use_showcnt: %d  ratio: %f", use_showcnt, ratio);
	
	if(db->DBSQL_IN){
		size_t	j, n, notfirst = 0;
		
		/* Compose IN string and set to zero url_id field */
		for(i = 0; i < Res->num_rows; i++) {
/*		  fprintf(stderr, " -- docinfoSQL: %d fetched:%d  dbnum:%d for %d [%d]\n", i, Res->Doc[i].fetched, Res->Doc[i].dbnum, db->dbnum, dbnum);*/
#ifdef WITH_MULTIDBADDR
		  if (Res->Doc[i].dbnum != dbnum) continue;
#endif
		  if (Res->Doc[i].fetched) continue;
			if(db->DBType==DPS_DB_PGSQL)
			  sprintf(DPS_STREND(instr), "%s'%i'", notfirst ? "," : "",
				  DpsVarListFindInt(&Res->Doc[i].Sections, "DP_ID", 0));
			else
			  sprintf(DPS_STREND(instr),"%s%i", notfirst ? "," : "",
				  DpsVarListFindInt(&Res->Doc[i].Sections, "DP_ID", 0));
			notfirst = 1;
		}

		if (notfirst == 0) goto lite_exit;
		
		dps_snprintf(qbuf, qbuflen, "SELECT rec_id,url,last_mod_time,docsize,next_index_time,referrer,crc32,site_id,pop_rank,charset_id FROM url WHERE rec_id IN (%s)", instr);
		if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		        DPS_FREE(qbuf);
			DPS_FREE(instr);
		        TRACE_OUT(query);
			return rc;
		}

		n = DpsSQLNumRows(&SQLres);
		
		for(j=0;j<Res->num_rows;j++) {
		        DPS_DOCUMENT	*D = &Res->Doc[j];
			urlid_t		url_id;

			if (D->fetched) continue;
#ifdef WITH_MULTIDBADDR
			if (D->dbnum != dbnum) continue;
#endif
			url_id = DpsVarListFindInt(&D->Sections, "DP_ID", 0);	
			for(i = 0; i < n; i++) {
				if(url_id == DPS_ATOI(DpsSQLValue(&SQLres,i,0))) {
					SQLResToDoc(query->Conf, D, &SQLres, i);
					if (use_showcnt) {
					  pr = atof(DpsVarListFindStr(&D->Sections, "Score", "0.0"));
					  if (pr >= ratio) {
					    dps_snprintf(qbuf, qbuflen, "UPDATE url SET shows = shows + 1 WHERE rec_id = %s%i%s",
							 (db->DBType == DPS_DB_PGSQL) ? "'" : "",
							 url_id, (db->DBType == DPS_DB_PGSQL) ? "'" : "");
					    DpsSQLAsyncQuery(db, NULL, qbuf);
					  }
					}
					break;
				}
			}
		}
		DpsSQLFree(&SQLres);

		dps_snprintf(qbuf, qbuflen, "SELECT u.rec_id,c.path FROM url u,server s,categories c WHERE u.rec_id IN (%s) AND u.server_id=s.rec_id AND s.category=c.rec_id", instr); 
		if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		        DPS_FREE(qbuf);
			DPS_FREE(instr);
		        TRACE_OUT(query);
			return rc;
		}

		n = DpsSQLNumRows(&SQLres);
		
		for(j=0;j<Res->num_rows;j++) {
		        DPS_DOCUMENT	*D = &Res->Doc[j];
			urlid_t		url_id;
			
			if (D->fetched) continue;
			url_id = DpsVarListFindInt(&D->Sections, "DP_ID", 0);	
			for(i = 0; i < n; i++) {
				if(url_id == DPS_ATOI(DpsSQLValue(&SQLres,i,0))) {
				        DpsVarListReplaceStr(&D->Sections, "Category", DpsSQLValue(&SQLres, i, 1));
					break;
				}
			}
		}
		DpsSQLFree(&SQLres);

	}else{
		for(i=0;i<Res->num_rows;i++){
			DPS_DOCUMENT	*D=&Res->Doc[i];
			urlid_t		url_id;

			if (D->fetched) continue;
			url_id = DpsVarListFindInt(&D->Sections, "DP_ID", 0);
			sprintf(qbuf,"SELECT rec_id,url,last_mod_time,docsize,next_index_time,referrer,crc32,site_id,pop_rank,charset_id FROM url WHERE rec_id=%i", url_id);
			if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
			        DPS_FREE(qbuf);
				DPS_FREE(instr);
			        TRACE_OUT(query);
				return rc;
			}
			
			if(DpsSQLNumRows(&SQLres)){
				SQLResToDoc(query->Conf, D, &SQLres, 0);
				if (use_showcnt) {
				  pr = atof(DpsVarListFindStr(&D->Sections, "Score", "0.0"));
				  if (pr >= ratio) {
				    dps_snprintf(qbuf, qbuflen, "UPDATE url SET shows = shows + 1 WHERE rec_id = %i", url_id);
				    DpsSQLAsyncQuery(db, NULL, qbuf);
				  }
				}
			}
			DpsSQLFree(&SQLres);
			sprintf(qbuf,"SELECT u.rec_id,c.path FROM url u,server s,categories c WHERE rec_id=%i AND u.server_id=s.rec_id AND s.category=c.rec_id", url_id);
			if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
			        DPS_FREE(qbuf);
				DPS_FREE(instr);
			        TRACE_OUT(query);
				return rc;
			}
			
			if(DpsSQLNumRows(&SQLres)){
			        DpsVarListReplaceStr(&D->Sections, "Category", DpsSQLValue(&SQLres, i, 1));
			}
			DpsSQLFree(&SQLres);
		}
	}
	
	if (!query->Flags.URLInfoSQL) goto lite_exit;
	if(db->DBSQL_IN){
		size_t j, n, notfirst = 0;
		
		instr[0]='\0';
		/* Compose IN string and set to zero url_id field */
		for(i=0;i<Res->num_rows;i++){
		  if(Res->Doc[i].fetched) continue;
			if(db->DBType==DPS_DB_PGSQL)
			  sprintf(DPS_STREND(instr),"%s'%i'", notfirst ? "," : "",
				  DpsVarListFindInt(&Res->Doc[i].Sections, "DP_ID", 0));
			  else
			    sprintf(DPS_STREND(instr),"%s%i", notfirst ? "," : "",
				    DpsVarListFindInt(&Res->Doc[i].Sections, "DP_ID", 0));
			notfirst = 1;
		}
		
		if (notfirst == 0) goto lite_exit;

		dps_snprintf(qbuf, qbuflen, "SELECT url_id,sname,sval FROM urlinfo WHERE url_id IN (%s)", instr);
		if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
		        DPS_FREE(qbuf);
			DPS_FREE(instr);
		        TRACE_OUT(query);
			return rc;
		}
		
		n = DpsSQLNumRows(&SQLres);
		
		for(j=0;j<Res->num_rows;j++) {
			DPS_DOCUMENT	*D=&Res->Doc[j];
			urlid_t		url_id;

			if (D->fetched) continue;
			url_id = DpsVarListFindInt(&D->Sections, "DP_ID", 0);
			for(i = 0; i < n; i++) {
				if(url_id == DPS_ATOI(DpsSQLValue(&SQLres,i,0))){
					char *sname = DpsSQLValue(&SQLres, i, 1);
					char *sval = DpsSQLValue(&SQLres, i, 2);
					Sec = DpsVarListFind(&query->Conf->Sections, sname);
					if (Sec != NULL) {
					  DPS_SEC = *Sec;
					  DPS_SEC.val = DPS_SEC.txt_val = sval;
					  DPS_SEC.curlen = dps_strlen(sval);
					  DpsVarListReplace(&D->Sections, &DPS_SEC);
					} else DpsVarListReplaceStr(&D->Sections,sname,sval);
				}
			}
			D->fetched = 1;
			Res->fetched++;
			if (db->DBMode == DPS_DBMODE_CACHE) {
			  rc = DpsAddURLCache(query, D, db);
			}
		}
		DpsSQLFree(&SQLres);
	}else{
		for(i=0;i<Res->num_rows;i++){
			DPS_DOCUMENT	*D=&Res->Doc[i];
			size_t		row;
			urlid_t		url_id;

			if (D->fetched) continue;
			url_id = DpsVarListFindInt(&D->Sections, "DP_ID", 0);
			sprintf(qbuf,"SELECT url_id,sname,sval FROM urlinfo WHERE url_id=%i", url_id);
			if(DPS_OK!=(rc=DpsSQLQuery(db,&SQLres,qbuf))) {
			  DPS_FREE(qbuf);
			  DPS_FREE(instr);
			  TRACE_OUT(query);
			  return rc;
			}
			
			for(row=0;row<DpsSQLNumRows(&SQLres);row++) {
				char *sname = DpsSQLValue(&SQLres, row, 1);
				char *sval = DpsSQLValue(&SQLres, row, 2);
				Sec = DpsVarListFind(&query->Conf->Sections, sname);
				if (Sec != NULL) {
				  DPS_SEC = *Sec;
				  DPS_SEC.val = DPS_SEC.txt_val = sval;
				  DPS_SEC.curlen = dps_strlen(sval);
				  DpsVarListReplace(&D->Sections, &DPS_SEC);
				} else DpsVarListReplaceStr(&D->Sections,sname,sval);
			}
			DpsSQLFree(&SQLres);
			D->fetched = 1;
			Res->fetched++;
			if (db->DBMode == DPS_DBMODE_CACHE) {
			  rc = DpsAddURLCache(query, D, db);
			}
		}
	}
 lite_exit:
	DPS_FREE(qbuf);
	DPS_FREE(instr);
	TRACE_OUT(query);
	return(DPS_OK);
}



/***********************************************************/
/*  HTDB stuff:  Indexing of database content              */
/***********************************************************/


static char * get_path_part(char *path,char *dst,int part){
	const char *s;
	char *e;
	int i=0;

	s=path;
	while(*s){
		if(i==part){
		  if((e=strchr(s,'/'))) { 
		    dps_strncpy(dst, s, (unsigned)(e-s));
		    dst[e - s] = '\0';
		  } else dps_strcpy(dst,s);
			return(dst);
		}
		if(*s=='/')i++;
		s++;
	}
	*dst=0;
	return(dst);
}

static char * include_params(const char *src,char *path,char *dst, size_t start, size_t limit) {
	const char *s;
	char *e;
	int i;
	
	s=src;e=dst;*e=0;
	while(*s){
		if((*s=='\\')){
			*e=*(s+1);e++;*e=0;s+=2;
			continue;
		}
		if(*s!='$'){
			*e=*s;
			s++;e++;*e=0;
			continue;
		}
		s++;i=atoi(s);
		while((*s>='0')&&(*s<='9'))s++;
		get_path_part(path,e,i);
		while(*e)e++;
	}
	if (limit) sprintf(e, " LIMIT %zu OFFSET %zu", limit, start);
	else *e = '\0';
	return(dst);
}

#define MAXHSIZE	8192 /*4096*/	/* TUNE */

int DpsHTDBGet(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, int flag_short) {
	char		*qbuf = NULL;
	char		*end;
	DPS_SQLRES	SQLres;
	DPS_URL		realURL;
	DPS_DB          db;
	const char	*url=DpsVarListFindStr(&Doc->Sections,"URL","");
	const char	*htdblist=DpsVarListFindStr(&Doc->Sections,"HTDBList","");
	const char	*htdbdoc = NULL;
	const char	*htdbaddr = DpsVarListFindStr(&Doc->Sections, "HTDBAddr", "");
	DPS_SERVER      *Srv = Doc->Server;
	int		rc = DPS_OK, have_htdbtext = 0;
	size_t          i, j, k, r, len = 0, qbuf_len;
	char            real_path[PATH_MAX]="";
	DPS_MATCH_PART  Parts[10];
	size_t          nparts = 10, content_length;
	int             have_words = 0;
	DPS_VAR	        *Sec;
	DPS_TEXTITEM	Item;
	DPS_TEMPLATE    t;
	char            *buf = NULL;
	
	if (flag_short) {
	  if (Srv == NULL) return DPS_OK;
	  if (Srv->HTDBsec.nmatches == 0) return DPS_OK;
	}

	for (i = 0; i < Srv->HTDBsec.nmatches; i++) {
	  r = dps_strlen(Srv->HTDBsec.Match[i].subsection);
	  if (len < r) len = r;
	  if (!strcasecmp(Srv->HTDBsec.Match[i].arg, "HTDBText")) have_htdbtext = 1;
	  else htdbdoc = Srv->HTDBsec.Match[i].subsection;
	}
	if (flag_short && have_htdbtext == 0) return DPS_OK;

	DpsSQLResInit(&SQLres);

	if (Doc->Buf.allocated_size <= DPS_NET_BUF_SIZE) {
	  Doc->Buf.allocated_size = DPS_NET_BUF_SIZE;
	  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
	    Doc->Buf.allocated_size = 0;
	    rc = DPS_NET_ALLOC_ERROR;
	    return rc;
	  }
	}

	Doc->Buf.buf[0] = '\0';
	realURL.freeme = 0;
	DpsURLInit(&realURL);
	DpsURLParse(&realURL,url);

	if ((qbuf = (char*)DpsMalloc(qbuf_len = 4 * 1024 + dps_strlen(url) + len + dps_strlen(htdblist) )) == NULL) 
	  return DPS_ERROR;
	*qbuf = '\0';
	
	if (NULL == DpsDBInit(&db)) {
	  DPS_FREE(qbuf);
	  return DPS_ERROR;
	}
	DpsDBSetAddr(&db, htdbaddr, DPS_OPEN_MODE_READ);


	if ((buf = (char*)DpsMalloc(qbuf_len)) == NULL) {
	  rc = DPS_ERROR;
	  goto HTDBexit;
	}
	*buf = '\0';

	bzero(&t, sizeof(t));
	t.HlBeg = t.HlEnd = t.GrBeg = t.GrEnd = t.ExcerptMark = NULL;
	t.Env_Vars = &Doc->Sections;

	dps_snprintf(real_path, sizeof(real_path)-1, "%s%s", realURL.path, realURL.filename);
	real_path[sizeof(real_path)-1]='\0';

	bzero((void*)&Item, sizeof(Item));
	if (have_htdbtext) {
	  Item.href = NULL;

	  for (i = 0; i < Srv->HTDBsec.nmatches; i++) {
	    if (strcasecmp(Srv->HTDBsec.Match[i].arg, "HTDBText")) continue;
	    Sec = DpsVarListFind(&Doc->Sections, Srv->HTDBsec.Match[i].section);
	    if (Sec  == NULL) continue;
	    if (DPS_MATCH_REGEX == Srv->HTDBsec.Match[i].match_type) {
	      if (DpsMatchExec(&Srv->HTDBsec.Match[i], real_path, real_path, NULL, nparts, Parts)) continue;
	      DpsMatchApply(qbuf, qbuf_len-1, real_path, Srv->HTDBsec.Match[i].subsection, &Srv->HTDBsec.Match[i], nparts, Parts);
	    } else {
	      include_params(Srv->HTDBsec.Match[i].subsection, real_path, qbuf, 0, 0);
	    }
	    DpsPrintTextTemplate(Indexer,NULL,NULL, buf, qbuf_len-1, &t, qbuf);
	    if(DPS_OK != (rc = DpsSQLQuery(&db, &SQLres, buf))) {
	      DPS_FREE(buf);
	      goto HTDBexit;
	    }
	    for (j = 0; j < DpsSQLNumRows(&SQLres); j++){
	      if (!strcasecmp(Sec->name, "Pop_Rank") || !strcasecmp(Sec->name, "Meta-Language") || !strcasecmp(Sec->name, "Meta-Charset")) {
		DpsVarListReplaceStr(&Doc->Sections, Sec->name, DpsSQLValue(&SQLres, j, 0));
	      } else {
		for (k = 0; k < SQLres.nCols; k++) {
		  Item.section = Sec->section;
		  Item.strict = Sec->strict;
		  Item.str = DpsSQLValue(&SQLres, j, k);
		  Item.section_name = Sec->name;
		  Item.len = dps_strlen(Item.str);
		  Indexer->nbytes += Item.len;
		  (void)DpsTextListAdd(&Doc->TextList, &Item);
		  have_words = 1;
		}
	      }
	    }
	    DpsSQLFree(&SQLres);
	  }
	}

	DpsTemplateFree(&t);
	DPS_FREE(buf);
	if (flag_short) goto HTDBexit;

	if(realURL.filename != NULL) {

		
		if (htdbdoc != NULL) {
		  for (i = 0; i < Srv->HTDBsec.nmatches; i++) {
		    if (strcasecmp(Srv->HTDBsec.Match[i].arg, "HTDBDoc")) continue;
		    if (DPS_MATCH_REGEX == Srv->HTDBsec.Match[i].match_type) {
		      if (DpsMatchExec(&Srv->HTDBsec.Match[i], real_path, real_path, NULL, nparts, Parts)) continue;
		      DpsMatchApply(qbuf, qbuf_len-1, real_path, Srv->HTDBsec.Match[i].subsection, &Srv->HTDBsec.Match[i], nparts, Parts);
		    } else {
		      include_params(htdbdoc, real_path, qbuf, 0, 0);
		    }
		    if(DPS_OK != (rc = DpsSQLQuery(&db, &SQLres, qbuf))) {
		      if (have_words)
		        sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n\r\n");
		      else
		        sprintf(Doc->Buf.buf,"HTTP/1.0 404 Not Found\r\n\r\n");
		      goto HTDBexit;
		    }
		    for (r = 0; r < DpsSQLNumRows(&SQLres); r ++) {
		        content_length = dps_strlen(DpsSQLValue(&SQLres, i, 0));
			if (Doc->Buf.allocated_size <= Doc->Buf.size + content_length + 1) {
			  Doc->Buf.allocated_size += content_length + 1;
			  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
			    Doc->Buf.allocated_size = 0;
			    rc = DPS_NET_ALLOC_ERROR;
/*
			    if (have_words)
			      sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n\r\n");
			    else
			      sprintf(Doc->Buf.buf,"HTTP/1.0 404 Not Found\r\n\r\n");
*/
  			    goto HTDBexit;
			  }
			}
			dps_strcat(Doc->Buf.buf, DpsSQLValue(&SQLres, i, 0));
		    }
		    DpsSQLFree(&SQLres);
		  }
		} else {
		    if (have_words)
		        sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n\r\n");
		    else
		        sprintf(Doc->Buf.buf,"HTTP/1.0 404 Not found\r\n\r\n");
		}
	}else{
		size_t	start = 0;
		urlid_t	url_id = DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
		const size_t  htdblimit = DpsVarListFindUnsigned(&Doc->Sections, "HTDBLimit", 0);
		int	done = 0, hops=DpsVarListFindInt(&Doc->Sections,"Hops",0);


		dps_snprintf(Doc->Buf.buf, Doc->Buf.allocated_size, 
			     "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n<HTML><BODY>\n</BODY></HTML>\n");

		while (!done) {

		  include_params(htdblist, realURL.path, qbuf, start, htdblimit);
		  if(DPS_OK != (rc = DpsSQLQuery(&db, &SQLres, qbuf))) {
			goto HTDBexit;
		  }

		  done = (htdblimit != DpsSQLNumRows(&SQLres));
		  start += DpsSQLNumRows(&SQLres);

		  for(i = 0; i < DpsSQLNumRows(&SQLres); i++) {
			DPS_HREF Href;
			DpsHrefInit(&Href);
			Href.referrer=url_id;
			Href.hops=hops+1;
			Href.url = (char*)DpsStrdup(DpsSQLValue(&SQLres, i, 0));
			Href.method=DPS_METHOD_GET;
			DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
			DPS_FREE(Href.url);
		  }
		  DpsSQLFree(&SQLres);
		  if(Doc->Hrefs.nhrefs > MAXHSIZE) {
		    if (DPS_OK == DpsDocStoreHrefs(Indexer, Doc)) {
		      DpsHrefListFree(&Doc->Hrefs);
		    }
		  }
		  if(Indexer->Hrefs.nhrefs > MAXHSIZE) {
		    DpsStoreHrefs(Indexer);
		  }
		}
	}
	end = DPS_STREND(Doc->Buf.buf);
	Doc->Buf.size = end - Doc->Buf.buf;
HTDBexit:
	DpsDBFree(&db);
	DpsURLFree(&realURL);
	DPS_FREE(qbuf);
	return rc;
}

/************************** make index stuff *******************************/

static char *BuildLimitQuery(DPS_DB *db, const char * field) {
  char qbuf[2048];
  char smallbuf[128];

  dps_snprintf(smallbuf, 128, ":%s:", field);
  if (strstr(":status:docsize:next_index_time:crc32:referrer:hops:seed:bad_since_time:site_id:pop_rank:url:", 
	     smallbuf) != NULL) {
    dps_snprintf(qbuf, 2048, "SELECT %s,rec_id as id,status FROM url u WHERE u.status>0 AND rec_id", field);
  } else if(strstr(":last_mod_time:", smallbuf) != NULL) {
    switch(db->DBType) {
    case DPS_DB_PGSQL:
    case DPS_DB_MSSQL:
    case DPS_DB_DB2:
    case DPS_DB_SQLITE:
    case DPS_DB_SQLITE3:
    default:
      dps_snprintf(qbuf, 2048, "SELECT (CASE WHEN %s=0 THEN since ELSE %s END),rec_id as id,status FROM url u WHERE u.status>0 AND rec_id",field, field);
      break;
    case DPS_DB_MYSQL:
    case DPS_DB_ACCESS:
    case DPS_DB_ORACLE7:
    case DPS_DB_ORACLE8:
    case DPS_DB_SAPDB:
      dps_snprintf(qbuf, 2048, "SELECT IF(%s>0,%s,since),rec_id as id,status FROM url u WHERE u.status>0 AND rec_id", field, field);
      break;
    }

/*  } else if(strstr(":tag:", smallbuf) != NULL) {
    dps_snprintf(qbuf, 2048, "SELECT s.%s,u.rec_id,u.status FROM server s, url u WHERE s.rec_id=u.server_id AND u.status>0 AND", field);*/
  } else if(strstr(":link:", smallbuf) != NULL) {
    dps_snprintf(qbuf, 2048, "SELECT l.ot,l.k as id,200 FROM links l WHERE TRUE AND l.k", field);
  } else {
    dps_snprintf(qbuf, 2048, "SELECT i.sval,i.url_id as id,200 FROM urlinfo i WHERE i.sname=LOWER('%s') AND i.url_id", field);
  }
  return (char*)DpsStrdup(qbuf);
}


int DpsLimit8SQL(DPS_AGENT *A, DPS_UINT8URLIDLIST *L,const char *field, int type, DPS_DB *db){
	char		*qbuf, *limit_query = BuildLimitQuery(db, field);
	size_t		i, p, nrows, offset, qbuflen;
	size_t          url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	DPS_SQLRES	SQLres;
	int		rc = DPS_OK, u;
	urlid_t rec_id = 0;

	if ((qbuf = (char*)DpsMalloc((qbuflen = 128 + dps_strlen(limit_query)))) == NULL) {
	        DPS_FREE(limit_query);
		return DPS_ERROR;
	}
	
	DpsSQLResInit(&SQLres);

	u = 1;
	offset = 0;
/*	DpsSQLBegin(db);*/
	while (u) {
	  dps_snprintf(qbuf, qbuflen, "%s>%d ORDER BY id LIMIT %d", limit_query, rec_id, url_num);
	
	  for (i = 0; i < 3; i ++) {
	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    rc = DpsSQLQuery(db, &SQLres, qbuf);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    if(DPS_OK != rc) {
	      if (i < 2) { DPSSLEEP(120); continue; }
/*	        DpsSQLEnd(db);*/
	        DPS_FREE(limit_query);
	        DPS_FREE(qbuf);
		return rc;
	    } else break;
	  }

	  nrows = DpsSQLNumRows(&SQLres);
	
	  L->Item = (DPS_UINT8URLID*)DpsRealloc(L->Item, (L->nitems + nrows + 1) * sizeof(DPS_UINT8URLID));
	  if(L->Item == NULL) {
/*	        DpsSQLEnd(db);*/
	    dps_strerror(A, DPS_LOG_ERROR, "Error:");
	    db->errcode=1;
	    DpsSQLFree(&SQLres);
	    DPS_FREE(limit_query);
	    DPS_FREE(qbuf);
	    return DPS_ERROR;
	  }
	  for(i = p = 0; i < nrows; i++) {
		const char *val0 = DpsSQLValue(&SQLres, i, 0);
		const char *val1 = DpsSQLValue(&SQLres, i, 1);
		int status = DPS_ATOI(DpsSQLValue(&SQLres, i, 2));

		if ((status > 199 && status < 400) /*|| status == 304*/) {

		  switch(type){

			case DPS_IFIELD_TYPE_HEX8STR: 
			  DpsDecodeHex8Str(val0,&L->Item[L->nitems + p].hi, &L->Item[L->nitems + p].lo, NULL, NULL); break;

			case DPS_IFIELD_TYPE_INT: 
			  L->Item[L->nitems + p].hi = atoi(val0); L->Item[L->nitems + p].lo = 0; break;
		  }
		  L->Item[L->nitems + p].url_id = DPS_ATOI(val1);
		  p++;
		}
	  }
	  offset += nrows;
	  DpsLog(A, DPS_LOG_EXTRA, "%d records processed at %d", offset, rec_id);
	  if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, nrows - 1, 1));
	  DpsSQLFree(&SQLres);
	  L->nitems += p;
	  u = (nrows == url_num);
	  if (u) DPSSLEEP(0);
	}
/*	DpsSQLEnd(db);*/
	DPS_FREE(limit_query);
	DPS_FREE(qbuf);
	return rc;
}


int DpsLimitTagSQL(DPS_AGENT *A, DPS_UINT4URLIDLIST *L, DPS_DB *db) {
  char qbuf[512];
  const char *info_query = "SELECT i.sval,u.rec_id FROM url u,urlinfo i WHERE u.rec_id=i.url_id AND i.sname='tag' AND";
  const char *srv_query  = "SELECT s.tag,u.rec_id FROM url u,server s WHERE s.rec_id=u.server_id AND";
  DPS_SQLRES Res;
  size_t  url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
  size_t i, pL, nL, nrows, offset;
  int rc= DPS_OK, u;
  urlid_t rec_id, start_id = 0;
  
  DpsSQLResInit(&Res);
  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
  rc = DpsSQLQuery(db, &Res, "SELECT MIN(rec_id) FROM url");
  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
  if (DpsSQLNumRows(&Res)) start_id = DPS_ATOI(DpsSQLValue(&Res, 0, 0));
  DpsSQLFree(&Res);

  u = 1;
  offset = 0;
  rec_id = start_id - 1;
  while (u) {
    dps_snprintf(qbuf, sizeof(qbuf), "%s u.rec_id>%d ORDER BY u.rec_id LIMIT %d", info_query, rec_id, url_num);

    for (i = 0; i < 3; i++) {
      if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
      rc = DpsSQLQuery(db, &Res, qbuf);
      if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
      if(DPS_OK != rc) {
	if (i < 2) { DPSSLEEP(120); continue; }
	return rc;
      } else break;
    }
    nrows = DpsSQLNumRows(&Res);
    L->Item = (DPS_UINT4URLID*)DpsRealloc(L->Item, (L->nitems + nrows + 1) * sizeof(DPS_UINT4URLID));
    if(L->Item == NULL) {
      dps_strerror(A, DPS_LOG_ERROR, "Error:");
      db->errcode = 1;
      DpsSQLFree(&Res);
      return DPS_ERROR;
    }
    for(i = 0; i < nrows; i++) {
      L->Item[L->nitems].url_id = DPS_ATOI(DpsSQLValue(&Res, i, 1));
      L->Item[L->nitems].val = DpsStrHash32(DpsSQLValue(&Res, i, 0));
      L->nitems++;
    }
    offset += nrows;
    DpsLog(A, DPS_LOG_EXTRA, "%d records processed at %d", offset, rec_id);
    if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&Res, nrows - 1, 1));
    DpsSQLFree(&Res);
    u = (nrows == url_num);
    if (u) DPSSLEEP(0);
  }


  pL = 0;
  nL = L->nitems;

  u = 1;
  offset = 0;
  rec_id = start_id - 1;
  while (u) {
    dps_snprintf(qbuf, sizeof(qbuf), "%s u.rec_id>%d ORDER BY u.rec_id LIMIT %d", srv_query, rec_id, url_num);

    for (i = 0; i < 3; i++) {
      if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
      rc = DpsSQLQuery(db, &Res, qbuf);
      if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
      if(DPS_OK != rc) {
	if (i < 2) { DPSSLEEP(120); continue; }
	return rc;
      } else break;
    }
    nrows = DpsSQLNumRows(&Res);
    L->Item = (DPS_UINT4URLID*)DpsRealloc(L->Item, (L->nitems + nrows + 1) * sizeof(DPS_UINT4URLID));
    if(L->Item == NULL) {
      dps_strerror(A, DPS_LOG_ERROR, "Error:");
      db->errcode = 1;
      DpsSQLFree(&Res);
      return DPS_ERROR;
    }
    for(i = 0; i < nrows; i++) {
      L->Item[L->nitems].url_id = DPS_ATOI(DpsSQLValue(&Res, i, 1));
      while(pL < nL && L->Item[pL].url_id < L->Item[L->nitems].url_id) pL++;
      if (pL < nL && L->Item[pL].url_id == L->Item[L->nitems].url_id) continue;
      L->Item[L->nitems].val = DpsStrHash32(DpsSQLValue(&Res, i, 0));
      L->nitems++;
    }
    offset += nrows;
    DpsLog(A, DPS_LOG_EXTRA, "%d records processed at %d", offset, rec_id);
    if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&Res, nrows - 1, 1));
    DpsSQLFree(&Res);
    u = (nrows == url_num);
    if (u) DPSSLEEP(0);
  }

  return rc;
}

int DpsLimitCategorySQL(DPS_AGENT *A, DPS_UINT8URLIDLIST *L, const char *field, int type, DPS_DB *db) {
	char		*qbuf;
	size_t		c, i, pL, nL, nrows, offset, qbuflen, ncats;
	size_t          url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	DPS_VARLIST     cat;
	DPS_SQLRES	SQLres, CatRes;
	int		rc = DPS_OK, u;
 	const char *cat_query = "SELECT c.rec_id, c.path, c.link, l.rec_id FROM categories c, categories l WHERE c.link=l.path ORDER BY c.rec_id";
	const char *info_query = "SELECT u.rec_id,c.path FROM url u,urlinfo i,categories c WHERE u.rec_id=i.url_id AND i.sname='category' AND u.status>0 AND";
	const char *srv_query = "SELECT u.rec_id,c.path FROM url u,server s,categories c WHERE u.status>0 AND u.server_id=s.rec_id AND s.category=c.rec_id AND";
	urlid_t         rec_id = 0, start_id = 0;
	const char      *val0, *val1, *oldlist, *valink;
	char            *p;

	if ((qbuf = (char*)DpsMalloc(qbuflen = 8192)) == NULL) {
		return DPS_ERROR;
	}

	DpsVarListInit(&cat);
	DpsSQLResInit(&SQLres);
	DpsSQLResInit(&CatRes);

	if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	rc = DpsSQLQuery(db, &SQLres, "SELECT MIN(rec_id) FROM url");
	if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	if (DpsSQLNumRows(&SQLres)) start_id = DPS_ATOI(DpsSQLValue(&SQLres, 0, 0));
	DpsSQLFree(&SQLres);

	for (i = 0; i < 3; i++) {
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	  rc = DpsSQLQuery(db, &CatRes, cat_query);
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	  if(DPS_OK != rc) {
	    if (i < 2) { DPSSLEEP(120); continue; }
	        DPS_FREE(qbuf);
		return rc;
	  } else break;
	}
	ncats = DpsSQLNumRows(&CatRes);
	for (c = 0; c < ncats; c++) {
	  val0 = DpsSQLValue(&CatRes, c, 1);
	  if (strchr(val0, '@') == NULL) {
	    oldlist = DpsVarListFindStr(&cat, val0, NULL);
	    if (oldlist == NULL) {
	      DpsVarListAddStr(&cat, val0, val0);
	    } else {
	      char *newlist = (char*)DpsMalloc(i = (dps_strlen(oldlist) + dps_strlen(val0) + 4));
	      if (newlist == NULL) {
		DpsVarListFree(&cat); DPS_FREE(qbuf); return DPS_ERROR;
	      }
	      dps_snprintf(newlist, i, "%s:%s", oldlist, val0);
	      DpsVarListReplaceStr(&cat, val0, newlist);
	      DPS_FREE(newlist);
	    }
	  } else {
	    valink = DpsSQLValue(&CatRes, c, 2);
	    oldlist = DpsVarListFindStr(&cat, valink, NULL);
	    if (oldlist == NULL) {
	      DpsVarListAddStr(&cat, valink, valink);
	    } else {
	      char *newlist = (char*)DpsMalloc(i = (dps_strlen(oldlist) + dps_strlen(val0) + 4));
	      if (newlist == NULL) {
		DpsVarListFree(&cat); DPS_FREE(qbuf); return DPS_ERROR;
	      }
	      dps_snprintf(newlist, i, "%s:%s", oldlist, val0);
	      DpsVarListReplaceStr(&cat, valink, newlist);
	      DPS_FREE(newlist);
	    }
	  }
	}
	DpsSQLFree(&CatRes);

	u = 1;
	offset = 0;
	rec_id = start_id - 1;
	while (u) {
	  dps_snprintf(qbuf, qbuflen, "%s c.rec_id=CAST(i.sval AS %s) AND u.rec_id>%d ORDER BY u.rec_id LIMIT %d", info_query, 
		       (db->DBType == DPS_DB_MYSQL) ? "SIGNED" : "INTEGER", rec_id, url_num);
	
	  for (i = 0; i < 3; i++) {
	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    rc = DpsSQLQuery(db, &SQLres, qbuf);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    if(DPS_OK != rc) {
	      if (i < 2) { DPSSLEEP(120); continue; }
	      DpsVarListFree(&cat); DPS_FREE(qbuf);
	      return rc;
	    }
	  }

	  nrows = DpsSQLNumRows(&SQLres);

	  L->Item = (DPS_UINT8URLID*)DpsRealloc(L->Item, (L->mitems = (L->nitems + nrows + 1)) * sizeof(DPS_UINT8URLID));
	  if(L->Item == NULL) {
	    dps_strerror(A, DPS_LOG_ERROR,"Error:");
	    db->errcode = 1;
	    DpsSQLFree(&SQLres);
	    DpsVarListFree(&cat);
	    DPS_FREE(qbuf);
	    return DPS_ERROR;
	  }
	  for(i = 0; i < nrows; i++) {
	    val0 = DpsSQLValue(&SQLres, i, 0);
	    val1 = DpsSQLValue(&SQLres, i, 1);
	    oldlist = DpsVarListFindStr(&cat, val1, NULL);

	    if (oldlist != NULL) {

	      while(1) {
		p = strchr(oldlist, ':');
		if (p != NULL) *p = '\0';

		switch(type){
		case DPS_IFIELD_TYPE_HEX8STR: 
		  DpsDecodeHex8Str(oldlist, &L->Item[L->nitems].hi, &L->Item[L->nitems].lo, NULL, NULL); break;
		case DPS_IFIELD_TYPE_INT: 
		  L->Item[L->nitems].hi = (dps_uint4)DPS_ATOI(oldlist); L->Item[L->nitems].lo = 0; break;
		}
		L->Item[L->nitems].url_id = DPS_ATOI(val0);

		L->nitems++;
		if (L->nitems >= L->mitems) {
		  L->Item = (DPS_UINT8URLID*)DpsRealloc(L->Item, (L->mitems = (L->nitems + 4096)) * sizeof(DPS_UINT8URLID));
		  if(L->Item == NULL) {
		    dps_strerror(A, DPS_LOG_ERROR, "Error:");
		    db->errcode = 1;
		    DpsSQLFree(&SQLres);
		    DpsVarListFree(&cat);
		    DPS_FREE(qbuf);
		    return DPS_ERROR;
		  }
		}
		if (p != NULL) {
		  *p = ':';
		  oldlist = p + 1;
		} else break;
	      }
	    }
	  }
	  offset += nrows;
	  DpsLog(A, DPS_LOG_EXTRA, "Category Limit by urlinfo: %d records processed at %d (total:%d)", nrows, rec_id, offset);
	  if (nrows) rec_id = DPS_ATOI(DpsSQLValue(&SQLres, nrows - 1, 0));
	  DpsSQLFree(&SQLres);
	  u = (nrows == url_num);
	  if (u) DPSSLEEP(0);
	}


	pL = 0;
	nL = L->nitems;

	u = 1;
	rec_id = start_id - 1;
	while (u) {
	  dps_snprintf(qbuf, qbuflen, "%s u.rec_id>%d ORDER BY u.rec_id LIMIT %d", srv_query, rec_id, url_num);
	
	  for (i = 0; i < 3; i++) {
	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    rc = DpsSQLQuery(db, &SQLres, qbuf);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    if(DPS_OK != rc) {
	      if (i < 2) { DPSSLEEP(120); continue; }
	      DpsVarListFree(&cat); DPS_FREE(qbuf);
	      return rc;
	    }
	  }

	  nrows = DpsSQLNumRows(&SQLres);

	  L->Item = (DPS_UINT8URLID*)DpsRealloc(L->Item, (L->mitems = (L->nitems + nrows + 1)) * sizeof(DPS_UINT8URLID));
	  if(L->Item == NULL) {
	    dps_strerror(A, DPS_LOG_ERROR, "Error:");
	    db->errcode = 1;
	    DpsSQLFree(&SQLres);
	    DpsVarListFree(&cat);
	    DPS_FREE(qbuf);
	    return DPS_ERROR;
	  }
	  for(i = 0; i < nrows; i++) {
	    val0 = DpsSQLValue(&SQLres, i, 0);
	    L->Item[L->nitems].url_id = DPS_ATOI(val0);
	    while(pL < nL && L->Item[pL].url_id < L->Item[L->nitems].url_id) pL++;
	    if (pL < nL && L->Item[pL].url_id == L->Item[L->nitems].url_id) continue;
	    val1 = DpsSQLValue(&SQLres, i, 1);
	    oldlist = DpsVarListFindStr(&cat, val1, NULL);

	    if (oldlist != NULL) {

	      while(1) {
		p = strchr(oldlist, ':');
		if (p != NULL) *p = '\0';

		switch(type){
		case DPS_IFIELD_TYPE_HEX8STR: 
		  DpsDecodeHex8Str(oldlist, &L->Item[L->nitems].hi, &L->Item[L->nitems].lo, NULL, NULL); break;
		case DPS_IFIELD_TYPE_INT: 
		  L->Item[L->nitems].hi = (dps_uint4)DPS_ATOI(oldlist); L->Item[L->nitems].lo = 0; break;
		}
		L->Item[L->nitems].url_id = DPS_ATOI(val0);

		L->nitems++;
		if (L->nitems >= L->mitems) {
		  L->Item = (DPS_UINT8URLID*)DpsRealloc(L->Item, (L->mitems = (L->nitems + 4096)) * sizeof(DPS_UINT8URLID));
		  if(L->Item == NULL) {
		    dps_strerror(A, DPS_LOG_ERROR, "Error:");
		    db->errcode = 1;
		    DpsSQLFree(&SQLres);
		    DpsVarListFree(&cat);
		    DPS_FREE(qbuf);
		    return DPS_ERROR;
		  }
		}
		if (p != NULL) {
		  *p = ':';
		  oldlist = p + 1;
		} else break;
	      }
	    }
	  }
	  offset += nrows;
	  DpsLog(A, DPS_LOG_EXTRA, "Category Limit by server: %d records processed at %d (total:%d)", nrows, rec_id, offset);
	  if (nrows) rec_id = DPS_ATOI(DpsSQLValue(&SQLres, nrows - 1, 0));
	  DpsSQLFree(&SQLres);
	  u = (nrows == url_num);
	  if (u) DPSSLEEP(0);
	}


	DpsVarListFree(&cat);
	DPS_FREE(qbuf);
	return rc;
}


int DpsLimitLinkSQL(DPS_AGENT *A, DPS_UINT4URLIDLIST *L, const char *field, int type, DPS_DB *db) {
	char		*qbuf;
	size_t		i, nrows, qbuflen, ncats;
	DPS_SQLRES	SQLres;
	int		rc = DPS_OK, fd;
	const char      *val0, *val1;

	if ((qbuf = (char*)DpsMalloc(qbuflen = 8192)) == NULL) {
		return DPS_ERROR;
	}

	DpsSQLResInit(&SQLres);

	dps_snprintf(qbuf, qbuflen, "SELECT k, ot FROM links");

	for (i = 0; i < 3; i++) {
	  if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	  rc = DpsSQLQuery(db, &SQLres, qbuf);
	  if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	  if(DPS_OK != rc) {
	    if (i < 2) { DPSSLEEP(120); continue; }
	    DPS_FREE(qbuf);
	    return rc;
	  } else break;
	}

	nrows = DpsSQLNumRows(&SQLres);
	
	L->Item = (DPS_UINT4URLID*)DpsRealloc(L->Item, (nrows + 1) * sizeof(DPS_UINT4URLID));
	if(L->Item == NULL) {
	  dps_strerror(A, DPS_LOG_ERROR, "Error alloc %d bytes", (nrows + 1) * sizeof(DPS_UINT4URLID) );
	  db->errcode = 1;
	  DpsSQLFree(&SQLres);
	  DPS_FREE(qbuf);
	  return DPS_ERROR;
	}
	for(i = 0; i < nrows; i++) {
	  val0 = DpsSQLValue(&SQLres, i, 0);
	  val1 = DpsSQLValue(&SQLres, i, 1);

	  L->Item[i].val = DPS_ATOI(val0);
	  L->Item[i].url_id = DPS_ATOI(val1);
	}
	DpsLog(A, DPS_LOG_EXTRA, "Link Limit: %d records processed", nrows);
	L->nitems = nrows;
	DpsSQLFree(&SQLres);

	DPS_FREE(qbuf);
	return rc;
}




int DpsLimit4SQL(DPS_AGENT *A, DPS_UINT4URLIDLIST *L,const char *field, int type, DPS_DB *db){
	char		*qbuf, *limit_query = BuildLimitQuery(db, field);
	size_t		i, p, nrows, offset, qbuflen;
	size_t          url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	DPS_SQLRES	SQLres;
	int		rc = DPS_OK, u;
	urlid_t rec_id = 0;
	
	if ((qbuf = (char*)DpsMalloc((qbuflen = 128 + dps_strlen(limit_query)))) == NULL) {
	        DPS_FREE(limit_query);
		return DPS_ERROR;
	}
	
	DpsSQLResInit(&SQLres);

	u = 1;
	offset = 0;
/*	DpsSQLBegin(db);*/
	while (u) {
	  dps_snprintf(qbuf, qbuflen, "%s>%d ORDER BY id LIMIT %d", limit_query, rec_id, url_num);
	
	  for (i = 0; i < 3; i++) {
	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    rc = DpsSQLQuery(db, &SQLres, qbuf);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    if(DPS_OK != rc) {
	      if (i < 2) { DPSSLEEP(120); continue; }
/*	        DpsSQLEnd(db);*/
	        DPS_FREE(limit_query);
	        DPS_FREE(qbuf);
		return rc;
	    } else break;
	  }
	
	  nrows = DpsSQLNumRows(&SQLres);
	
	  L->Item = (DPS_UINT4URLID*)DpsRealloc(L->Item, (L->nitems + nrows + 1) * sizeof(DPS_UINT4URLID));
	  if(L->Item == NULL) {
/*	        DpsSQLEnd(db);*/
	    dps_strerror(NULL, 0, "Error:");
	    db->errcode=0;
	    DpsSQLFree(&SQLres);
	    DPS_FREE(limit_query);
	    DPS_FREE(qbuf);
	    return DPS_ERROR;
	  }
	  for(i = p = 0; i < nrows; i++) {
		const char *val0 = DpsSQLValue(&SQLres, i, 0);
		const char *val1 = DpsSQLValue(&SQLres, i, 1);
		int status = DPS_ATOI(DpsSQLValue(&SQLres, i, 2));


		if ((status > 199 && status < 400) /*|| status == 304*/) {

		  switch(type){
			case DPS_IFIELD_TYPE_HOUR: L->Item[L->nitems + p].val = atoi(val0) / 3600; break;
			case DPS_IFIELD_TYPE_MIN: L->Item[L->nitems + p].val = atoi(val0) / 60; break;
			case DPS_IFIELD_TYPE_HOSTNAME: {
				DPS_URL *url = DpsURLInit(NULL);
				if (url != NULL) {
				  if(!DpsURLParse(url,val0)){
					if(url->hostname) L->Item[L->nitems + p].val = DpsStrHash32(url->hostname);
					else L->Item[L->nitems + p].val=0;
				  }else
					L->Item[L->nitems + p].val=0;
				  DpsURLFree(url);
				}
			}
				break;
		        case DPS_IFIELD_TYPE_STR2CRC32: 
			  L->Item[L->nitems + p].val = DpsHash32(val0, (dps_strlen(val0)) > 2 ? 2 : dps_strlen(val0)); break;
			case DPS_IFIELD_TYPE_STRCRC32: L->Item[L->nitems + p].val = DpsStrHash32(val0); break;
			case DPS_IFIELD_TYPE_INT: L->Item[L->nitems + p].val = atoi(val0); break;
			
		  }
		  L->Item[L->nitems + p].url_id = DPS_ATOI(val1);
		  p++;
		}
	  }
	  offset += nrows;
	  DpsLog(A, DPS_LOG_EXTRA, "%d records processed at %d", offset, rec_id);
	  if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, nrows - 1, 1));
	  DpsSQLFree(&SQLres);
	  L->nitems += p;
	  u = (nrows == url_num);
	  if (u) DPSSLEEP(0);
	}
/*	DpsSQLEnd(db);*/
	DPS_FREE(limit_query);
	DPS_FREE(qbuf);
	return rc;
}


__C_LINK int __DPSCALL DpsSQLLimit8(DPS_AGENT *A, DPS_UINT8URLIDLIST *L, const char *req, int type, DPS_DB *db) {
	DPS_SQLRES SQLres;
	dps_uint8  offset;
	char       *qbuf;
	size_t     url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	size_t	   i, p, nrows, qbuflen;
	int	   rc = DPS_OK, u;

	if ((qbuf = (char*)DpsMalloc((qbuflen = 128 + dps_strlen(req)))) == NULL) {
		return DPS_ERROR;
	}
	
	DpsSQLResInit(&SQLres);
	u = 1;
	offset = (dps_uint8)0;
	while (u) {
	  dps_snprintf(qbuf, qbuflen, "%s LIMIT %d OFFSET %ld", req, url_num, (long)offset);
	  for (i = 0; i < 3; i++) {
	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    rc = DpsSQLQuery(db, &SQLres, qbuf);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    if(DPS_OK != rc) {
	      if (i < 2) { DPSSLEEP(120); continue; }
	        DPS_FREE(qbuf);
		return rc;
	    } else break;
	  }
	
	  nrows = DpsSQLNumRows(&SQLres);
	  L->Item = (DPS_UINT8URLID*)DpsRealloc(L->Item, (L->nitems + nrows + 1) * sizeof(DPS_UINT8URLID));
	  if(L->Item == NULL) {
	    dps_strerror(A, DPS_LOG_ERROR, "Error:");
	    db->errcode=0;
	    DpsSQLFree(&SQLres);
	    DPS_FREE(qbuf);
	    return DPS_ERROR;
	  }
	  for(i = p = 0; i < nrows; i++) {
		const char *val0 = DpsSQLValue(&SQLres, i, 0);
		const char *val1 = DpsSQLValue(&SQLres, i, 1);

		switch(type){

			case DPS_IFIELD_TYPE_HEX8STR: 
			  DpsDecodeHex8Str(val0,&L->Item[L->nitems + p].hi, &L->Item[L->nitems + p].lo, NULL, NULL); break;

			case DPS_IFIELD_TYPE_INT: 
			  L->Item[L->nitems + p].hi = atoi(val0); L->Item[L->nitems + p].lo = 0; break;
		}
		L->Item[L->nitems + p].url_id = DPS_ATOI(val1);
		p++;
	  }
	  DpsSQLFree(&SQLres);
	  offset += (dps_uint8)nrows;
	  DpsLog(A, DPS_LOG_EXTRA, "%ld records processed.", (long)offset);
	  L->nitems += p;
	  u = (nrows == url_num);
	}
	DPS_FREE(qbuf);
	return rc;
}

__C_LINK int __DPSCALL DpsSQLLimit4(DPS_AGENT *A, DPS_UINT4URLIDLIST *L, const char *req, int type, DPS_DB *db) {
	DPS_SQLRES SQLres;
	dps_uint8  offset;
	char       *qbuf;
	size_t     url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	size_t	   i, p, nrows, qbuflen;
	int	   rc = DPS_OK, u;

	if ((qbuf = (char*)DpsMalloc((qbuflen = 128 + dps_strlen(req)))) == NULL) {
		return DPS_ERROR;
	}
	
	DpsSQLResInit(&SQLres);
	u = 1;
	offset = (dps_uint8)0;
	while (u) {
	  dps_snprintf(qbuf, qbuflen, "%s LIMIT %d OFFSET %ld", req, url_num, (long)offset);
	  for (i = 0; i < 3; i++) {
	    if (A->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(A, DPS_LOCK_DB);
	    rc = DpsSQLQuery(db, &SQLres, qbuf);
	    if (A->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(A, DPS_LOCK_DB);
	    if(DPS_OK != rc) {
	      if (i < 2) { DPSSLEEP(120); continue; }
	        DPS_FREE(qbuf);
		return rc;
	    } else break;
	  }
	
	  nrows = DpsSQLNumRows(&SQLres);
	  L->Item = (DPS_UINT4URLID*)DpsRealloc(L->Item, (L->nitems + nrows + 1) * sizeof(DPS_UINT4URLID));
	  if(L->Item == NULL) {
	    dps_strerror(A, DPS_LOG_ERROR, "Error:");
	    db->errcode=0;
	    DpsSQLFree(&SQLres);
	    DPS_FREE(qbuf);
	    return DPS_ERROR;
	  }
	  for(i = p = 0; i < nrows; i++) {
		const char *val0 = DpsSQLValue(&SQLres, i, 0);
		const char *val1 = DpsSQLValue(&SQLres, i, 1);

		switch(type){

			case DPS_IFIELD_TYPE_HOUR: L->Item[L->nitems + p].val = atoi(val0) / 3600; break;
			case DPS_IFIELD_TYPE_MIN: L->Item[L->nitems + p].val = atoi(val0) / 60; break;
			case DPS_IFIELD_TYPE_HOSTNAME: {
				DPS_URL *url = DpsURLInit(NULL);
				if (url != NULL) {
				  if(!DpsURLParse(url,val0)){
					if(url->hostname) L->Item[L->nitems + p].val = DpsStrHash32(url->hostname);
					else L->Item[L->nitems + p].val=0;
				  }else
					L->Item[L->nitems + p].val=0;
				  DpsURLFree(url);
				}
			}
				break;
		        case DPS_IFIELD_TYPE_STR2CRC32: 
			  L->Item[L->nitems + p].val = DpsHash32(val0, (dps_strlen(val0)) > 2 ? 2 : dps_strlen(val0)); break;
			case DPS_IFIELD_TYPE_STRCRC32: L->Item[L->nitems + p].val = DpsStrHash32(val0); break;
			case DPS_IFIELD_TYPE_INT: L->Item[L->nitems + p].val = atoi(val0); break;
		}
		L->Item[L->nitems + p].url_id = DPS_ATOI(val1);
		p++;
	  }
	  DpsSQLFree(&SQLres);
	  offset += (dps_uint8)nrows;
	  DpsLog(A, DPS_LOG_EXTRA, "%ld records processed.", (long)offset);
	  L->nitems += p;
	  u = (nrows == url_num);
	}
	DPS_FREE(qbuf);
	return rc;
}


/***************************************************************************/
#ifdef WITH_POPHOPS
#define f(x) (1.0 / (1.0 + exp(-1.0 * hops * (x))))
#define f2(x) (1.0 / (1.0 + exp(-1.0 * hops * (x))))
#else
#define f(x) (1.0 / (1.0 + exp(-1.0 * (x))))
#define f2(x) (1.0 / (1.0 + exp(-1.0 * (x))))
#endif
#define EPS 0.0001
#define LOW_BORDER_EPS  0.000001
#define LOW_BORDER_EPS2 0.000001
#define HI_BORDER_EPS  (1.0 - 0.000001)
#define HI_BORDER_EPS2 (1.0 - 0.000001)

#define LINK_WEIGHT_HI 1.0
#define LINK_WEIGHT_LO LOW_BORDER_EPS
#define PAS_HI -0.01
#define PAS_LO -9999999.99

#define XSTR(s) STR(s)
#define STR(s) #s

typedef struct {
  double weight, pop_rank;
  urlid_t rec_id;
} DPS_LNK;


static int DpsPopRankPasNeoSQL(DPS_AGENT *A, DPS_DB *db, const char *rec_id, const char *hops_str, int skip_same_site, size_t url_num, int need_count,
			       int detect_clones) {
  char double_str[64];
  DPS_SQLRES	SQLres;
  const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
  char		qbuf[512];
  double di = LOW_BORDER_EPS2, Oi = 0.25, delta, pas, pdiv, cur_div, dw;
  size_t j, jrows, li_offset;
  int  rc = DPS_ERROR, v, it, u_it, to_update = 1;
  urlid_t li_rec_id = 0;
#ifdef WITH_POPHOPS
  double hops = DPS_POPHOPS_FACTOR / (DPS_ATOI(hops_str) + 1);
#endif

/*  skip_same_site = 0;*/ /* disable here for speed, removed on insertion early */

  DpsSQLResInit(&SQLres);
/*  DpsSQLBegin(db);*/

/* links K */	
  if (skip_same_site) {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uo.pop_rank * l.weight), COUNT(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.k=l.ot) AND l.k=%s%s%s", qu, rec_id, qu);
  } else {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uo.pop_rank * l.weight), COUNT(*) FROM links l, url uo WHERE l.k=%s%s%s AND uo.rec_id=l.ot", 
		 qu, rec_id, qu);
  }
  if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) {
/*    DpsSQLEnd(db);*/
    return rc;
  }
  if ( (DPS_ATOI(DpsSQLValue(&SQLres, 0, 1))) > 0) {
    di = f( (double)DPS_ATOF(DpsSQLValue(&SQLres, 0, 0)) );
/*    di =  (double)DPS_ATOF(DpsSQLValue(&SQLres, 0, 0));*/
    if (di < LOW_BORDER_EPS2) di = LOW_BORDER_EPS2;
    else if (di > HI_BORDER_EPS2) di = HI_BORDER_EPS2;
/*  } else {
    di = EPS;*/
  }
  DpsSQLFree(&SQLres);

/* links OT */
#if 1
  if (skip_same_site) {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uk.pop_rank * l.weight), COUNT(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.ot=l.k) AND l.ot=%s%s%s", qu, rec_id, qu);
  } else {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uk.pop_rank * l.weight), COUNT(*) FROM links l, url uo, url uk WHERE l.ot=%s%s%s AND uo.rec_id=l.ot AND uk.rec_id=l.k", qu, rec_id, qu);
  }
  if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) { 
/*    DpsSQLEnd(db);*/
    return rc; 
  }

  if ( DPS_ATOI(DpsSQLValue(&SQLres, 0, 1)) > 0) {
    Oi = (double)DPS_ATOF(DpsSQLValue(&SQLres, 0, 0));
  }
  Oi = f2( Oi );
  if (Oi < LOW_BORDER_EPS) Oi = LOW_BORDER_EPS;
  else if (Oi > HI_BORDER_EPS) Oi = HI_BORDER_EPS;

  DpsSQLFree(&SQLres);
#endif


  if (need_count) A->Conf->url_number--;

  pas = -0.7;

  pdiv = cur_div = fabs(di - Oi);
  /* u_it = ((di != 0.0) && (di != 1.0));*/
  u_it = ( cur_div > EPS );

  for (it = 0; u_it && (it < A->Flags.PopRankNeoIterations); it++) {

/*    delta = pas * (di - Oi) * Oi * (1.0 - Oi);*/
    delta = pas * (Oi - di) * di * (1.0 - di);

/*   DpsLog(A, DPS_LOG_EXTRA, "%s:%02d|%12.9f->%12.9f|di:%11.9f|Oi:%11.9f|delta:%12.9f|pas:%11.9f", 
	   rec_id, it, pdiv, cur_div,  di, Oi, delta, pas);*/

    if (fabs(delta) > 0.0) {

      A->poprank_pas++;

      v = 1;
      li_offset = 0;
      li_rec_id = (urlid_t)0;

      while (v) {
	dps_snprintf(qbuf, sizeof(qbuf), 
	    "SELECT u.rec_id, u.pop_rank from url u, links l WHERE l.ot=%s%s%s AND u.rec_id=l.k AND u.rec_id>%d ORDER BY u.rec_id LIMIT %d",
		     qu, rec_id, qu, li_rec_id, url_num);
	if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) {   
/*	  DpsSQLEnd(db);*/
	  return rc; 
	}
	jrows = DpsSQLNumRows(&SQLres);
	for (j = 0; j < jrows; j++) {

	  dw = delta * DPS_ATOF(DpsSQLValue(&SQLres, j, 1));
	    
	  if (fabs(dw) > 0.000000000001) {
	    dps_snprintf(double_str, sizeof(double_str), "%.12f", dw);
	    dps_snprintf(qbuf, sizeof(qbuf), 
			 "UPDATE links SET weight = MAX(" XSTR(LINK_WEIGHT_LO) ", MIN(" XSTR(LINK_WEIGHT_HI) ", weight + (%s))) WHERE k=%s%s%s AND ot=%s%s%s", 
			 DpsDBEscDoubleStr(double_str), qu, DpsSQLValue(&SQLres, j, 0), qu, qu, rec_id, qu);
	    DpsSQLAsyncQuery(db, NULL, qbuf);
	  }
	}
	if (jrows > 0) {
	  li_rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&SQLres, jrows - 1, 0));
	}
	DpsSQLFree(&SQLres);
	v = (jrows == url_num);
	li_offset += jrows;
      }

    } else {
      dps_snprintf(double_str, sizeof(double_str), "%.12f", (di + Oi)/2);
      dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s WHERE rec_id=%s%s%s", DpsDBEscDoubleStr(double_str), qu, rec_id, qu );
      DpsSQLAsyncQuery(db, NULL, qbuf);
      to_update = 0;
      break;
    }

#if 0
/* links K */	
    if (skip_same_site) {
      dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uo.pop_rank * l.weight), COUNT(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.ot=l.k) AND l.k=%s%s%s", qu, rec_id, qu);
    } else {
      dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uo.pop_rank * l.weight), COUNT(*) FROM links l, url uo WHERE l.k=%s%s%s AND uo.rec_id=l.ot", 
		   qu, rec_id, qu);
    }
    if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) { 
/*      DpsSQLEnd(db);*/
      return rc; 
    }
    if (DPS_ATOI(DpsSQLValue(&SQLres, 0, 1)) > 0) {
      di = f( DPS_ATOF(DpsSQLValue(&SQLres, 0, 0)));
/*      di = (double)DPS_ATOF(DpsSQLValue(&SQLres, 0, 0));*/
      if (di < LOW_BORDER_EPS2) di = LOW_BORDER_EPS2;
      else if (di > HI_BORDER_EPS2) di = HI_BORDER_EPS2;
    } else di = LOW_BORDER_EPS2;
    DpsSQLFree(&SQLres);
#endif

/* links OT */
#if 1
    if (skip_same_site) {
      dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uk.pop_rank * l.weight), COUNT(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.ot=l.k) AND l.ot=%s%s%s", qu, rec_id, qu);
    } else {
      dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(uk.pop_rank * l.weight), COUNT(*) FROM links l, url uo, url uk WHERE l.ot=%s%s%s AND uo.rec_id=l.ot AND uk.rec_id=l.k", qu, rec_id, qu);
    }
    if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) { 
/*      DpsSQLEnd(db);*/
      return rc; 
    }

    if (DPS_ATOI(DpsSQLValue(&SQLres, 0, 1)) > 0) {
      Oi = (double)DPS_ATOF(DpsSQLValue(&SQLres, 0, 0));
    } else Oi = 0.25;
    Oi = f2( Oi );
    if (Oi < LOW_BORDER_EPS) Oi = LOW_BORDER_EPS;
    else if (Oi > HI_BORDER_EPS) Oi = HI_BORDER_EPS;

    DpsSQLFree(&SQLres);
#endif

    cur_div = fabs(di - Oi);
    
    if ((cur_div > pdiv) && ((cur_div - pdiv) > EPS)) {
      pas *= 0.43;
    } else if (fabs(delta) < 1.1) {
      pas *= 2.11;
    } else if (fabs(delta) > 1.0) pas *= 0.95;
    if (pas > PAS_HI) pas = PAS_HI;
    else if (PAS_LO > pas) pas = PAS_LO;

    DpsLog(A, DPS_LOG_DEBUG, "%s:%02d|%12.9f->%12.9f|di:%11.9f|Oi:%11.9f|delta:%12.9f|pas:%11.9f", 
	   rec_id, it, pdiv, cur_div,  di, Oi, delta, pas);

    u_it = ( (pdiv = cur_div) > EPS );

    dps_snprintf(double_str, sizeof(double_str), "%.12f", (di + Oi)/2);
    dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s WHERE rec_id=%s%s%s", DpsDBEscDoubleStr(double_str), qu, rec_id, qu );
    DpsSQLAsyncQuery(db, NULL, qbuf);
/*    to_update = 0;*/
	      
  }
  if (to_update) {
    double pr, nPR = (di + Oi) / 2;

#ifdef NEO_USE_CLONES
    if (detect_clones) {
      dps_snprintf(qbuf, sizeof(qbuf), 
"SELECT COUNT(*),MAX(u.pop_rank) FROM url u,url o WHERE o.rec_id=%s%s%s AND u.status>2000 AND u.crc32=o.crc32 AND u.site_id=o.site_id", 
		   qu, rec_id, qu);
      rc = DpsSQLQuery(db, &SQLres, qbuf);
      if (rc == DPS_OK) {
	if ( (DpsSQLNumRows(&SQLres) > 0) && (DPS_ATOI(DpsSQLValue(&SQLres, 0, 0)) > 0) ) {
	  pr = DPS_ATOF(DpsSQLValue(&SQLres, 0, 1));
	  if (pr > nPR) nPR = pr;
	}
      }
      DpsSQLFree(&SQLres);
    }
#endif
    
    dps_snprintf(double_str, sizeof(double_str), "%.12f", nPR);
    dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s WHERE rec_id=%s%s%s", DpsDBEscDoubleStr(double_str), qu, rec_id, qu );
    DpsSQLAsyncQuery(db, NULL, qbuf);
    DpsLog(A, DPS_LOG_EXTRA, "Neo PopRank: %s", double_str);
  }
  
/*  DpsSQLEnd(db);*/
  return DPS_OK;
}




static int DpsPopRankPasNeo(DPS_AGENT *A, DPS_DB *db, const char *rec_id, const char *hops_str, int skip_same_site, size_t url_num,
			    int need_count, int detect_clones) {
  char		qbuf[512];
  char          double_str[64];
  DPS_SQLRES	SQLres;
  DPS_LNK /**IN = NULL,*/ *OUT = NULL;
  double pr, nPR;
  const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
  double di = 0.0, Oi = 0.0, delta, pas, pdiv, cur_div, dw, PopRank = 0.5;
  size_t j, jrows;
  size_t n_di, n_Oi;
  int  rc = DPS_ERROR, it, u_it, to_update = 0;
  urlid_t rec_id_num = DPS_ATOI(rec_id);
#ifdef WITH_POPHOPS
  double hops = DPS_POPHOPS_FACTOR / (DPS_ATOI(hops_str) + 1);
#endif

  skip_same_site = 0; /* disable here for speed, removed on insertion early */

  DpsSQLResInit(&SQLres);
  A->poprank_docs++;

/* links K */	

  if (skip_same_site) {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.k=l.ot) AND l.k=%s%s%s", qu, rec_id, qu);
  } else {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM links l WHERE l.k=%s%s%s", qu, rec_id, qu);
  }
  if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) {
    return rc;
  }
  n_di = DPS_ATOI(DpsSQLValue(&SQLres, 0, 0));
  DpsSQLFree(&SQLres);
  if (n_di > url_num) return DpsPopRankPasNeoSQL(A, db, rec_id, hops_str, skip_same_site, url_num, need_count, detect_clones);

/* links OT */
  if (skip_same_site) {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.k=l.ot) AND l.ot=%s%s%s", qu, rec_id, qu);
  } else {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM links l WHERE l.ot=%s%s%s", qu, rec_id, qu);
  }
  if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) {
    DPS_FREE(OUT); return rc;
  }
  n_Oi = DPS_ATOI(DpsSQLValue(&SQLres, 0, 0));
  DpsSQLFree(&SQLres);
  if (n_Oi > url_num) return DpsPopRankPasNeoSQL(A, db, rec_id, hops_str, skip_same_site, url_num, need_count, detect_clones);


/* links K */	

/*
  IN = (DPS_LNK*)DpsMalloc((n_di + 1) * sizeof(DPS_LNK));
  if (IN == NULL) return DpsPopRankPasNeoSQL(A, db, rec_id, hops_str, skip_same_site, url_num, need_count);
*/
  if (skip_same_site) {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT uo.rec_id, uo.pop_rank, l.weight FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.k=l.ot) AND l.k=%s%s%s", qu, rec_id, qu);
  } else {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT uo.rec_id, uo.pop_rank, l.weight FROM links l, url uo WHERE l.k=%s%s%s AND uo.rec_id=l.ot", qu, rec_id, qu);
  }
  if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) {
    /*DPS_FREE(IN);*/ return DpsPopRankPasNeoSQL(A, db, rec_id, hops_str, skip_same_site, url_num, need_count, detect_clones);
  }
  jrows = DpsSQLNumRows(&SQLres);
  for (j = 0; j < jrows /*&& j < n_di*/; j++) {
/*    IN[j].rec_id = DPS_ATOI(DpsSQLValue(&SQLres, j, 0));
    IN[j].pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, j, 1));
    IN[j].weight = DPS_ATOF(DpsSQLValue(&SQLres, j, 2));
    di += IN[j].pop_rank * IN[j].weight;*/
    di += DPS_ATOF(DpsSQLValue(&SQLres, j, 1)) * DPS_ATOF(DpsSQLValue(&SQLres, j, 2));
    if (!strcmp(DpsSQLValue(&SQLres, j, 0), rec_id)) PopRank = DPS_ATOF(DpsSQLValue(&SQLres, j, 1));
  }
  DpsSQLFree(&SQLres);
/*  if (jrows < n_di) n_di = jrows;*/
  if (di != 0.0) {
    di = f( di );
    if (di < LOW_BORDER_EPS2) di = LOW_BORDER_EPS2;
    else if (di > HI_BORDER_EPS2) di = HI_BORDER_EPS2;
  } else di = LOW_BORDER_EPS2;

/* links OT */
  OUT = (DPS_LNK*)DpsMalloc((n_Oi + 1) * sizeof(DPS_LNK));
  if (OUT == NULL) { DPS_FREE(OUT); return DpsPopRankPasNeoSQL(A, db, rec_id, hops_str, skip_same_site, url_num, need_count, detect_clones); }
  if (skip_same_site) {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT uk.rec_id, uk.pop_rank, l.weight FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.k=l.ot) AND l.ot=%s%s%s", qu, rec_id, qu);
  } else {
    dps_snprintf(qbuf, sizeof(qbuf), "SELECT uo.rec_id, uo.pop_rank, l.weight FROM links l, url uo WHERE l.ot=%s%s%s AND uo.rec_id=l.k", qu, rec_id, qu);
  }
  if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) {
    DPS_FREE(OUT); return DpsPopRankPasNeoSQL(A, db, rec_id, hops_str, skip_same_site, url_num, need_count, detect_clones);
  }
  jrows = DpsSQLNumRows(&SQLres);
  for (j = 0; j < jrows && j < n_Oi; j++) {
    OUT[j].rec_id = DPS_ATOI(DpsSQLValue(&SQLres, j, 0));
    OUT[j].pop_rank = DPS_ATOF(DpsSQLValue(&SQLres, j, 1));
    OUT[j].weight = DPS_ATOF(DpsSQLValue(&SQLres, j, 2));
    if (OUT[j].weight > LINK_WEIGHT_HI) OUT[j].weight = LINK_WEIGHT_HI;
    else if (OUT[j].weight < LINK_WEIGHT_LO) OUT[j].weight = LINK_WEIGHT_LO;
    Oi += OUT[j].pop_rank * OUT[j].weight;
/*    Oi += DPS_ATOF(DpsSQLValue(&SQLres, j, 1)) * DPS_ATOF(DpsSQLValue(&SQLres, j, 2));*/
  }
  DpsSQLFree(&SQLres);
  if (jrows < n_Oi) n_Oi = jrows;
  Oi = (n_Oi) ? f2( Oi ) : 0.25;
  if (Oi < LOW_BORDER_EPS) Oi = LOW_BORDER_EPS;
  else if (Oi > HI_BORDER_EPS) Oi = HI_BORDER_EPS;

  if (need_count) A->Conf->url_number--;

  pas = -0.7;

  cur_div = fabs(di - Oi);
  if ((pdiv = fabs(PopRank - Oi)) > EPS) to_update++;
  u_it = (cur_div > EPS || pdiv > EPS);
  /*
  DpsLog(A, DPS_LOG_DEBUG, " -- di:%f  Oi:%f  cur_div:%f  pdiv:%f  nOi:%d", di, Oi, cur_div, pdiv, n_Oi);
  */
  for (it = 0; u_it && (it < A->Flags.PopRankNeoIterations); it++) {

/*    delta = pas * (di - Oi) * Oi * (1.0 - Oi);*/
    delta = pas * (Oi - di) * di * (1.0 - di);
    if (fabs(delta) > 0.0) {

      A->poprank_pas++; to_update++;
      Oi = 0.0;
      for (j = 0; j < n_Oi; j++) {
	  dw = delta * OUT[j].pop_rank;
	  if (OUT[j].rec_id != rec_id_num) {
	    OUT[j].weight += dw;
	    if (OUT[j].weight > LINK_WEIGHT_HI) OUT[j].weight = LINK_WEIGHT_HI;
	    else if (OUT[j].weight < LINK_WEIGHT_LO) OUT[j].weight = LINK_WEIGHT_LO;
	  }
	  Oi += OUT[j].pop_rank * OUT[j].weight;
      }
      Oi = f2( Oi );
      if (Oi < LOW_BORDER_EPS) Oi = LOW_BORDER_EPS;
      else if (Oi > HI_BORDER_EPS) Oi = HI_BORDER_EPS;

    }
    cur_div = fabs(di - Oi);
    
    if ((cur_div > pdiv) && ((cur_div - pdiv) > EPS)) {
      pas *= 0.43;
    } else if (fabs(delta) < 1.1) {
      pas *= 2.11;
    } else if (fabs(delta) > 1.0) pas *= 0.95;
    if (pas > PAS_HI) pas = PAS_HI;
    else if (PAS_LO > pas) pas = PAS_LO;

    DpsLog(A, DPS_LOG_DEBUG, "%s:%02d|%12.9f->%12.9f|di:%11.9f|Oi:%11.9f|delta:%12.9f|pas:%11.9f", 
	   rec_id, it, pdiv, cur_div,  di, Oi, delta, pas);

    u_it = ( (pdiv = cur_div) > EPS );

  }

  if (to_update) {
      for (j = 0; j < n_Oi; j++) {
	dps_snprintf(double_str, sizeof(double_str), "%.12f", OUT[j].weight);
	dps_snprintf(qbuf, sizeof(qbuf), "UPDATE links SET weight=%s WHERE k=%s%d%s AND ot=%s%s%s", 
		     DpsDBEscDoubleStr(double_str), qu, OUT[j].rec_id, qu, qu, rec_id, qu );
	DpsSQLAsyncQuery(db, NULL, qbuf);
      }    
  
      nPR = ((n_Oi == 1) && (OUT[0].rec_id == rec_id_num) ) ? di : (di + Oi)/2;
#ifdef NEO_USE_CLONES
      if (detect_clones) {
	dps_snprintf(qbuf, sizeof(qbuf), 
"SELECT COUNT(*),MAX(u.pop_rank) FROM url u,url o WHERE o.rec_id=%s%s%s AND u.status>2000 AND u.crc32=o.crc32 AND o.site_id=u.site_id", 
		     qu, rec_id, qu);
	rc = DpsSQLQuery(db, &SQLres, qbuf);
	if (rc == DPS_OK) {
	  if ( (DpsSQLNumRows(&SQLres) > 0) && (DPS_ATOI(DpsSQLValue(&SQLres, 0, 0)) > 0) ) {
	    pr = DPS_ATOF(DpsSQLValue(&SQLres, 0, 1));
	    if (pr > nPR) nPR = pr;
	  }
	}
	DpsSQLFree(&SQLres);
      }
#endif

      dps_snprintf(double_str, sizeof(double_str), "%.12f", nPR);
      dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s WHERE rec_id=%s%s%s", DpsDBEscDoubleStr(double_str), qu, rec_id, qu );
      DpsSQLAsyncQuery(db, NULL, qbuf);
      DpsLog(A, DPS_LOG_EXTRA, "Neo PopRank: %s", double_str);
  }

  /*DPS_FREE(IN);*/ DPS_FREE(OUT);
  return DPS_OK;
}



static int DpsPopRankCalculateNeo(DPS_AGENT *A, DPS_DB *db) {
	DPS_SQLRES	Res;
	size_t		i, irows, offset = 0;
/*	urlid_t         rec_id = 0;*/
	dps_uint4       nit = 0;
	int             rc = DPS_ERROR, u = 1;
	int		skip_same_site = !strcasecmp(DpsVarListFindStr(&A->Vars, "PopRankSkipSameSite",DPS_POPRANKSKIPSAMESITE),"yes");
	int		detect_clones = !strcasecmp(DpsVarListFindStr(&A->Vars, "DetectClones", DPS_DETECTCLONES), "yes");
	size_t          url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	const char	*where;
	char		qbuf[512];

	where = BuildWhere(A, db);
	if (where == NULL) {
	  rc = DPS_ERROR;
	  goto Calc_unlockNeo;
	}

	DpsSQLResInit(&Res);

	if (skip_same_site)  DpsLog(A, DPS_LOG_EXTRA, "Will skip links from same site");

	while (u) {

#ifdef WITH_POPHOPS
	  dps_snprintf(qbuf, sizeof(qbuf),"SELECT url.rec_id,url.next_index_time,url.hops FROM url%s WHERE url.next_index_time>%d %s %s ORDER BY url.next_index_time LIMIT %d", 
		       db->from, nit, (where[0]) ? "AND" : "", where, url_num);
#else
	  dps_snprintf(qbuf, sizeof(qbuf),"SELECT url.rec_id,url.next_index_time, FROM url%s WHERE url.next_index_time>%d %s %s ORDER BY url.next_index_time LIMIT %d", 
		       db->from, nit, (where[0]) ? "AND" : "", where, url_num);
#endif

	  if(DPS_OK != (rc = DpsSQLQuery(db, &Res, qbuf))) goto Calc_unlockNeo;
	  irows = DpsSQLNumRows(&Res);
	  for (i = 0; i < irows; i++) {

#ifdef WITH_POPHOPS
	    rc = DpsPopRankPasNeo(A, db, DpsSQLValue(&Res, i, 0), DpsSQLValue(&Res, i, 2), skip_same_site, url_num, 1, detect_clones);
#else
	    rc = DpsPopRankPasNeo(A, db, DpsSQLValue(&Res, i, 0), NULL, skip_same_site, url_num, 1, detect_clones);
#endif
	    if (rc != DPS_OK) goto Calc_unlockNeo;
	    if (milliseconds > 0) DPS_MSLEEP(milliseconds);
	    if (A->Conf->url_number <= 0) break;

	  }
	  if (irows > 0) nit = (urlid_t)DPS_ATOI(DpsSQLValue(&Res, irows - 1, 1));
	  DpsSQLFree(&Res);
	  u = ((irows == url_num) && (A->Conf->url_number > 0)) ;
	  offset += (A->Conf->url_number > 0) ? irows : (i + 1);
	  /* To see the URL being indexed in "ps" output on xBSD */
	  if (DpsNeedLog(DPS_LOG_EXTRA)) dps_setproctitle("[%d] Neo:%d URLs done", A->handle, offset);
	  DpsLog(A, DPS_LOG_EXTRA, "Neo:%d URLs processed", offset);
	}

	rc = DPS_OK;

Calc_unlockNeo:
	/* To see the URL being indexed in "ps" output on xBSD */
	if (DpsNeedLog(DPS_LOG_INFO)) dps_setproctitle("[%d] Neo done", A->handle);
	DpsLog(A, DPS_LOG_INFO, "Neo PopRank done: %d URLs processed, total pas: %ld", offset, A->poprank_pas);
	return rc;
}

static int DpsPopRankCalculateGoo(DPS_AGENT *A, DPS_DB *db) {
	char		qbuf[256];
	char            ratio_str[64];
	DPS_SQLRES	SQLres, Res, POPres;
	int             rc = DPS_ERROR, u = 0;
	size_t		i, nrows, offset = 0;
	urlid_t         rec_id = 0;
	int		skip_same_site = !strcasecmp(DpsVarListFindStr(&A->Vars, "PopRankSkipSameSite",DPS_POPRANKSKIPSAMESITE),"yes");
	int		feed_back = !strcasecmp(DpsVarListFindStr(&A->Vars, "PopRankFeedBack", "no"), "yes");
	int		use_tracking = !strcasecmp(DpsVarListFindStr(&A->Vars, "PopRankUseTracking", "no"), "yes");
	int		use_showcnt = !strcasecmp(DpsVarListFindStr(&A->Vars, "PopRankUseShowCnt", "no"), "yes");
	size_t          url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
	double          ratio = DpsVarListFindDouble(&A->Vars, "PopRankShowCntWeight", 0.01);
	const char      *qu = (db->DBType == DPS_DB_PGSQL) ? "'" : "";
	const char	*where;

	DpsSQLResInit(&SQLres);
	DpsSQLResInit(&Res);
	DpsSQLResInit(&POPres);

	dps_snprintf(ratio_str, sizeof(ratio_str), "%f", ratio);
	DpsDBEscDoubleStr(ratio_str);

	where = BuildWhere(A, db);
	if (where == NULL) {
	  rc = DPS_ERROR;
	  goto Calc_unlock;
	}

	if (feed_back || use_tracking) {
	  if (use_tracking) DpsLog(A, DPS_LOG_EXTRA, "Will calculate servers weights using tracking");
	  if (feed_back) DpsLog(A, DPS_LOG_EXTRA, "Will calculate feed back servers weights");

	  if(DPS_OK != (rc = DpsSQLQuery(db, &Res, "SELECT rec_id FROM server WHERE command='S'")))
	    goto Calc_unlock;

	  nrows = DpsSQLNumRows(&Res);
	  for (i = 0; i < nrows; i++) {
	    if (use_tracking) {
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM qinfo WHERE name='site' AND value='%s'", DpsSQLValue(&Res, i, 0) );
	      if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
	      u = (DPS_ATOI(DpsSQLValue(&SQLres, 0, 0)) == 0);
	    }
	    if (feed_back && (u || !use_tracking)) {
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(pop_rank) FROM url WHERE site_id=%s%s%s", qu, DpsSQLValue(&Res, i, 0), qu);
	      if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
	    }
	    if (*DpsSQLValue(&SQLres, 0, 0)) {
	      dps_snprintf(qbuf, sizeof(qbuf), "UPDATE server SET weight=%s WHERE rec_id=%s%s%s", DpsSQLValue(&SQLres, 0, 0), 
			   qu, DpsSQLValue(&Res, i, 0), qu);
	      DpsSQLAsyncQuery(db, NULL, qbuf);
	    }
	    DpsSQLFree(&SQLres);
	  }
	  DpsSQLFree(&Res);
	  DpsSQLAsyncQuery(db, NULL, "UPDATE server SET weight=1 WHERE weight=0 AND command='S'");
	}


	if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, "SELECT rec_id, url, weight FROM server WHERE command='S'")))
		goto Calc_unlock;
	
	nrows = DpsSQLNumRows(&SQLres);

	for (i = 0; i < nrows; i++) {

	  dps_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM url WHERE site_id=%s%s%s", qu, DpsSQLValue(&SQLres, i, 0), qu);
	  if(DPS_OK != (rc = DpsSQLQuery(db, &Res, qbuf)))
		goto Calc_unlock;
	  DpsLog(A, DPS_LOG_EXTRA, "Site: %s Weight: %s URL count: %s",
		 DpsSQLValue(&SQLres, i, 1), DpsSQLValue(&SQLres, i, 2), DpsSQLValue(&Res, 0, 0)); 
	  if (atoi(DpsSQLValue(&Res, 0, 0)) > 0) {
	    dps_snprintf(qbuf, sizeof(qbuf), "UPDATE server SET pop_weight=(%s/%s) WHERE rec_id=%s%s%s",
		  DpsSQLValue(&SQLres, i, 2), DpsSQLValue(&Res, 0, 0), qu, DpsSQLValue(&SQLres, i, 0), qu);
	    DpsSQLAsyncQuery(db, NULL, qbuf);
	    if (!A->Flags.collect_links) {
	      dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank = (%s/%s) WHERE site_id=%s%s%s",
			   DpsSQLValue(&SQLres, i, 2), DpsSQLValue(&Res, 0, 0), qu, DpsSQLValue(&SQLres, i, 0), qu);
	      DpsSQLAsyncQuery(db, NULL, qbuf);
	    }
	  }
	  DpsSQLFree(&Res);

	}
	DpsSQLFree(&SQLres);


	DpsLog(A, DPS_LOG_EXTRA, "update links and pages weights");
	if (skip_same_site)  DpsLog(A, DPS_LOG_EXTRA, "Will skip links from same site");
	if (use_showcnt)  DpsLog(A, DPS_LOG_EXTRA, "Will add show count");

	u = 1;
	offset = 0;
	rec_id = (urlid_t)0;
	while (u) {

	  dps_snprintf(qbuf, sizeof(qbuf), 
		       "SELECT url.rec_id, site_id  FROM url%s WHERE url.rec_id>%d %s %s ORDER BY url.rec_id LIMIT %d", 
		       db->from, rec_id, (where[0]) ? "AND" : "", where, url_num);

	  if(DPS_OK != (rc = DpsSQLQuery(db, &Res, qbuf))) goto Calc_unlock;
	  nrows = DpsSQLNumRows(&Res);
	  for (i = 0; i < nrows; i++) {


/* links OT */

	    if (skip_same_site) {
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT count(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.ot=l.k) AND l.ot=%s%s%s", qu, DpsSQLValue(&Res, i, 0), qu);
	    } else {
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT count(*) FROM links WHERE ot=%s%s%s", qu, DpsSQLValue(&Res, i, 0), qu);
	    }
	    if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
	    if (*DpsSQLValue(&SQLres, 0, 0)) {
	      if (DPS_ATOI(DpsSQLValue(&SQLres, 0, 0))) {

		dps_snprintf(qbuf, sizeof(qbuf), "SELECT pop_weight FROM server WHERE rec_id=%s%s%s", qu, DpsSQLValue(&Res, i, 1), qu);
		if(DPS_OK != (rc = DpsSQLQuery(db, &POPres, qbuf))) goto Calc_unlock;
		if (DpsSQLNumRows(&POPres) != 1) { DpsSQLFree(&POPres); DpsSQLFree(&SQLres); continue; }

		dps_snprintf(qbuf, sizeof(qbuf), "UPDATE links SET weight = (%s/%s.0) WHERE ot=%s%s%s",
			    DpsSQLValue(&POPres, 0, 0), DpsSQLValue(&SQLres, 0, 0), qu, DpsSQLValue(&Res, i, 0), qu);

		DpsSQLFree(&POPres);
		DpsSQLAsyncQuery(db, NULL, qbuf);
	      }
	    }
	    DpsSQLFree(&SQLres);


/* links K */	
	    if (skip_same_site) {
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(weight) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND (uo.site_id<>uk.site_id OR l.ot=l.k) AND l.k=%s%s%s", qu, DpsSQLValue(&Res, i, 0), qu);
	    } else {
	      dps_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(weight) FROM links WHERE k=%s%s%s", qu, DpsSQLValue(&Res, i, 0), qu);
	    }
	    if(DPS_OK != (rc = DpsSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
	    if (*DpsSQLValue(&SQLres, 0, 0)) {
	      if (use_showcnt) {
		dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s + (shows * %s) WHERE rec_id=%s%s%s", 
			     DpsSQLValue(&SQLres, 0, 0), ratio_str, qu, DpsSQLValue(&Res, i, 0), qu );
	      } else {
		dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s WHERE rec_id=%s%s%s", 
			     DpsSQLValue(&SQLres, 0, 0), qu, DpsSQLValue(&Res, i, 0), qu );
	      }
	      DpsSQLAsyncQuery(db, NULL, qbuf);
	    } else {
	      if (use_showcnt) {
		dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=(shows * %s) WHERE rec_id=%s%s%s", 
			     ratio_str, qu, DpsSQLValue(&Res, i, 0), qu );
	      } else {
		dps_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=0 WHERE rec_id=%s%s%s", 
			     qu, DpsSQLValue(&Res, i, 0), qu );
	      }
	      DpsSQLAsyncQuery(db, NULL, qbuf);
	    }
	    DpsSQLFree(&SQLres);

	    if (milliseconds > 0) DPS_MSLEEP(milliseconds);

	  }
	  if (nrows > 0) rec_id = (urlid_t)DPS_ATOI(DpsSQLValue(&Res, nrows - 1, 0));
	  DpsSQLFree(&Res);
	  u = (nrows == url_num);
	  offset += nrows;
	  /* To see the URL being indexed in "ps" output on xBSD */
	  if (DpsNeedLog(DPS_LOG_EXTRA)) dps_setproctitle("[%d] Goo:%d URLs done", A->handle, offset);
	  DpsLog(A, DPS_LOG_EXTRA, "Goo:%d URLs processed", offset);
	}

	rc = DPS_OK;

Calc_unlock:
	/* To see the URL being indexed in "ps" output on xBSD */
	if (DpsNeedLog(DPS_LOG_INFO)) dps_setproctitle("[%d] Goo done", A->handle);
	DpsLog(A, DPS_LOG_INFO, "Goo PopRank done.");
	return rc;
}


static int DpsPopRankCalculate(DPS_AGENT *A, DPS_DB *db) {
  const char *method = DpsVarListFindStr(&A->Vars, "PopRankMethod", "Goo");
  if (strcasecmp(method, "Goo") == 0) {
    return DpsPopRankCalculateGoo(A, db);
  } else if (strcasecmp(method, "Neo") == 0) {
    return DpsPopRankCalculateNeo(A, db);
  }
  DpsLog(A, DPS_LOG_ERROR, "Unknown PopRankMethod %s", method);
  return DPS_OK;
}



int DpsURLActionSQL(DPS_AGENT * A, DPS_DOCUMENT * D, int cmd,DPS_DB *db){
	int res;

	switch(cmd){
		case DPS_URL_ACTION_DELETE:
			res=DpsDeleteURL(A,D,db);
			break;
			
		case DPS_URL_ACTION_ADD:
			res=DpsAddURL(A,D,db);
			break;
			
		case DPS_URL_ACTION_ADD_LINK:
			res=DpsAddLink(A,D,db);
			break;
			
		case DPS_URL_ACTION_SUPDATE:
			res=DpsUpdateUrl(A,D,db);
			break;
			
		case DPS_URL_ACTION_LUPDATE:
			res=DpsLongUpdateURL(A,D,db);
			break;
			
		case DPS_URL_ACTION_INSWORDS:
			res=DpsStoreWords(A,D,db);
			break;
			
		case DPS_URL_ACTION_INSCWORDS:
			res=DpsStoreCrossWords(A,D,db);
			break;
			
		case DPS_URL_ACTION_DELWORDS:
			res=DpsDeleteWordFromURL(A,D,db);
			break;
			
		case DPS_URL_ACTION_DELCWORDS:
			res=DpsDeleteCrossWordsFromURL(A,D,db);
			break;
			
		case DPS_URL_ACTION_UPDCLONE:
			res=DpsUpdateClone(A,D,db);
			break;
			
		case DPS_URL_ACTION_REGCHILD:
			res=DpsRegisterChild(A,D,db);
			break;
			
		case DPS_URL_ACTION_FINDBYURL:
			res=DpsFindURL(A,D,db);
			break;
			
		case DPS_URL_ACTION_FINDBYMSG:
			res=DpsFindMessage(A,D,db);
			break;
			
		case DPS_URL_ACTION_FINDORIG:
			res=DpsFindOrigin(A,D,db);
			break;
			
		case DPS_URL_ACTION_EXPIRE:
			res=DpsMarkForReindex(A,db);
			break;
			
		case DPS_URL_ACTION_REFERERS:
			res=DpsGetReferers(A,db);
			break;
		
		case DPS_URL_ACTION_DOCCOUNT:
			res=DpsGetDocCount(A,db);
			break;
		
	        case DPS_URL_ACTION_WRITEDATA:
			if (db->DBMode == DPS_DBMODE_CACHE) {
			  return DpsURLDataWrite(A, db);
			}
		        return DPS_OK;

	        case DPS_URL_ACTION_FLUSHCACHED:
			if (db->DBMode == DPS_DBMODE_CACHE) {
			  return DpsCachedFlush(A, db);
			}
		        return DPS_OK;

		case DPS_URL_ACTION_LINKS_DELETE:
			res = DpsDeleteLinks(A, D, db);
			break;

		case DPS_URL_ACTION_LINKS_MARKTODEL:
		        res = DpsLinksMarkToDelete(A, D, db);
			break;
	        case DPS_URL_ACTION_REFRESHDOCINFO:
		        res = DpsDocInfoRefresh(A, db);
			break;
	        case DPS_URL_ACTION_SITEMAP:
		        res = DpsSitemap(A, db);
			break;
	        case DPS_URL_ACTION_REFERER:
		        res= DpsRefererGet(A, D, db);
			break;
  	        case DPS_URL_ACTION_PASNEO: 
		  {
		    int	   skip_same_site = !strcasecmp(DpsVarListFindStr(&A->Vars, "PopRankSkipSameSite", DPS_POPRANKSKIPSAMESITE), "yes");
		    int	   detect_clones = !strcasecmp(DpsVarListFindStr(&A->Vars, "DetectClones", DPS_DETECTCLONES), "yes");
		    size_t url_num = (size_t)DpsVarListFindUnsigned(&A->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);
#ifdef WITH_POPHOPS
		    res = DpsPopRankPasNeo(A, db, DpsVarListFindStr(&D->Sections, "DP_ID", "0"), 
					   DpsVarListFindStr(&D->Sections, "Hops", "0"), skip_same_site, url_num, 0, detect_clones);
#else
		    res = DpsPopRankPasNeo(A, db, DpsVarListFindStr(&D->Sections, "DP_ID", "0"), 
					   NULL, skip_same_site, url_num, 0, detect_clones);
#endif
		  }
			break;
	        case DPS_URL_ACTION_RESORT:
		        res = DPS_OK;
			break;
	        case DPS_URL_ACTION_REHASHSTORED:
		  res = DpsStoredRehash(A, db);
			break;
	        case DPS_URL_ACTION_POSTPONE_ON_ERR:
		  res = DpsDocPostponeSite(A, D, db);
		        break;
		default:
  		        DpsLog(A, DPS_LOG_ERROR, "Unsupported URL Action SQL");
			res=DPS_ERROR;
	}
	return res;
}

int DpsResActionSQL(DPS_AGENT *Agent, DPS_RESULT *Res, int cmd, DPS_DB *db, size_t dbnum){
	switch(cmd){
		case DPS_RES_ACTION_DOCINFO:
			return DpsResAddDocInfoSQL(Agent, db, Res, dbnum);
		default:
  		        DpsLog(Agent, DPS_LOG_ERROR, "Unsupported Res Action SQL");
			return DPS_ERROR;
	}
}

int DpsCatActionSQL(DPS_AGENT *Agent, DPS_CATEGORY *Cat, int cmd,DPS_DB *db){
	switch(cmd){
		case DPS_CAT_ACTION_LIST:
			return DpsCatList(Agent,Cat,db);
		case DPS_CAT_ACTION_PATH:
			return DpsCatPath(Agent,Cat,db);
		default:
  		        DpsLog(Agent, DPS_LOG_ERROR, "Unsupported Cat Action SQL");
			return DPS_ERROR;
	}
}

int DpsSrvActionSQL(DPS_AGENT *A, DPS_SERVER *S, int cmd, DPS_DB *db) {
	switch(cmd){

	case DPS_SRV_ACTION_TABLE:
	  return DpsLoadServerTable(A, db);

	case DPS_SRV_ACTION_CATTABLE:
	  return DpsLoadCategoryTable(A, db);

	case DPS_SRV_ACTION_URLDB:
	  return DpsURLDB(A, S, db);

	case DPS_SRV_ACTION_SERVERDB:
	case DPS_SRV_ACTION_REALMDB:
	case DPS_SRV_ACTION_SUBNETDB:
	  return DpsServerDB(A, S, db);

	case DPS_SRV_ACTION_FLUSH:
	  return DpsServerTableFlush(db);

	case DPS_SRV_ACTION_CATFLUSH:
	  return DpsCatTableFlush(db);

	case DPS_SRV_ACTION_CLEAN:
	  return DpsServerTableClean(db);
	  
	case DPS_SRV_ACTION_ADD:
	  return DpsServerTableAdd(A, S, db);
	  
	case DPS_SRV_ACTION_ID:
	  return DpsServerTableGetId(A, S, db);
   	  
	case DPS_SRV_ACTION_POPRANK:
	  return DpsPopRankCalculate(A, db);
	
	default:
	  DpsLog(A, DPS_LOG_ERROR, "Unsupported Srv Action SQL");
	  return DPS_ERROR;
	}
}

unsigned int   DpsGetCategoryIdSQL(DPS_ENV *Conf, char *category, DPS_DB *db) {
  DPS_SQLRES Res;
  char qbuf[128];
  unsigned int rc = 0;

  DpsSQLResInit(&Res);
  dps_snprintf(qbuf, 128, "SELECT rec_id FROM categories WHERE path='%s'", category);
  if(DPS_OK != (rc = DpsSQLQuery(db, &Res, qbuf))) return rc;
  if ( DpsSQLNumRows(&Res) > 0) {
    sscanf(DpsSQLValue(&Res, 0, 0), "%u", &rc);
  }
  DpsSQLFree(&Res);
  return rc;
}


int DpsCheckUrlidSQL(DPS_AGENT *Agent, DPS_DB *db, urlid_t id) {
  DPS_SQLRES SQLRes;
  char qbuf[128];
  unsigned int rc = 0;

  DpsSQLResInit(&SQLRes);
  dps_snprintf(qbuf, sizeof(qbuf), "SELECT rec_id FROM url WHERE rec_id=%d", id);
  rc = DpsSQLQuery(db, &SQLRes, qbuf);
  if(DPS_OK != rc) {
    rc = 1;
  } else {
    if (DpsSQLNumRows(&SQLRes) != 0) rc = 1;
    else rc = 0;
  }
  DpsSQLFree(&SQLRes);
  return rc;
}


int DpsCheckReferrerSQL(DPS_AGENT *Agent, DPS_DB *db, urlid_t id) {
  DPS_SQLRES SQLRes;
  char qbuf[128];
  unsigned int rc = DPS_ERROR;

  DpsSQLResInit(&SQLRes);

  if (Agent->Flags.collect_links) {

    if (db->DBSQL_LIMIT) {
      dps_snprintf(qbuf, sizeof(qbuf), "SELECT ot FROM links WHERE k=%d AND ot!=k LIMIT 1", id);
      rc = DpsSQLQuery(db, &SQLRes, qbuf);
      if(DPS_OK == rc) {
	if (DpsSQLNumRows(&SQLRes) == 0) goto check_referrer;
      }
    } else {
      dps_snprintf(qbuf, sizeof(qbuf), "SELECT count(*) FROM links WHERE k=%d AND ot!=k", id);
      rc = DpsSQLQuery(db, &SQLRes, qbuf);
      if(DPS_OK == rc) {
	if (DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0)) == 0) goto check_referrer;
      }
    }
  } else {
  check_referrer:
      dps_snprintf(qbuf, sizeof(qbuf), "SELECT referrer FROM url WHERE rec_id=%d", id);
      rc = DpsSQLQuery(db, &SQLRes, qbuf);
      if(DPS_OK == rc) {
	if (DpsSQLNumRows(&SQLRes) > 0) {
	  int referrer = DPS_ATOI(DpsSQLValue(&SQLRes, 0, 0));
	  if (referrer == -1) rc = DPS_ERROR;
	} else rc = DPS_ERROR;
      }
  }
  DpsSQLFree(&SQLRes);
  return rc;
}

#endif /* HAVE_SQL */
