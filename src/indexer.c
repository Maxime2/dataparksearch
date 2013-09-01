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
#include "dps_log.h"
#include "dps_conf.h"
#include "dps_indexer.h"
#include "dps_indexertool.h"
#include "dps_robots.h"
#include "dps_db.h"
#include "dps_sqldbms.h"
#include "dps_url.h"
#include "dps_parser.h"
#include "dps_proto.h"
#include "dps_hrefs.h"
#include "dps_mutex.h"
#include "dps_hash.h"
#include "dps_xmalloc.h"
#include "dps_http.h"
#include "dps_host.h"
#include "dps_server.h"
#include "dps_alias.h"
#include "dps_word.h"
#include "dps_crossword.h"
#include "dps_parsehtml.h"
#include "dps_parsexml.h"
#include "dps_spell.h"
#include "dps_execget.h"
#include "dps_agent.h"
#include "dps_match.h"
#include "dps_doc.h"
#include "dps_result.h"
#include "dps_parsedate.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_uniconv.h"
#include "dps_contentencoding.h"
#include "dps_vars.h"
#include "dps_guesser.h"
#include "dps_textlist.h"
#include "dps_id3.h"
#include "dps_stopwords.h"
#include "dps_wild.h"
#include "dps_image.h"
#include "dps_charsetutils.h"
#include "dps_template.h"
#include "dps_sqldbms.h"
#ifdef HAVE_ZLIB
#include "dps_store.h"
#endif
#include "dps_filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_BSD_LIBUTIL_H
#include <bsd/libutil.h>
#else
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/mman.h>

#ifdef HAVE_LIBEXTRACTOR
#include <extractor.h>
#endif

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define DPS_THREADINFO(A,s,m)	if(A->Conf->ThreadInfo)A->Conf->ThreadInfo(A,s,m)

/***************************************************************************/

#define MAXHSIZE	8192 /*4096*/	/* TUNE */

#define NS 10
int DpsFilterFind(int log_level, DPS_MATCHLIST *L, const char *newhref, char *reason, int default_method) {
	DPS_MATCH_PART	P[NS];
	DPS_MATCH	*M = NULL;
	int		res = default_method;
	
#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif
	if( (default_method != DPS_METHOD_DISALLOW) && (M = DpsMatchListFind(L, newhref, NS, P)) != NULL) {
	  res = DpsMethod(M->arg);
	  if (DpsNeedLog(log_level) || DpsNeedLog((res!=DPS_METHOD_DISALLOW)?DPS_LOG_DEBUG:DPS_LOG_EXTRA)) {
	    dps_snprintf(reason, PATH_MAX, "%s %s%s %s '%s'", DPS_NULL2EMPTY(M->arg),
			 M->nomatch ? "nomatch " : "",
			 DpsMatchTypeStr(M->match_type),
			 M->case_sense ? "Sensitive" : "InSensitive", M->pattern);
	  }
	  switch(default_method) {
	  case DPS_METHOD_HEAD: 
	  case DPS_METHOD_HREFONLY: if (res == DPS_METHOD_GET) res = default_method; break;
	  case DPS_METHOD_VISITLATER: if (res != DPS_METHOD_DISALLOW) res = default_method; break;
	  }
	}else{
	  if (DpsNeedLog(log_level) || DpsNeedLog((res!=DPS_METHOD_DISALLOW)?DPS_LOG_DEBUG:DPS_LOG_EXTRA)) {
	    sprintf(reason, "%s by default", DpsMethodStr(default_method));
	  }
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}

int DpsSectionFilterFind(int log_level, DPS_MATCHLIST *L, DPS_DOCUMENT *Doc, char *reason) {
	DPS_MATCH_PART	P[NS];
	DPS_MATCH	*M;
	int		res = DPS_METHOD_UNKNOWN;
	
#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif

	if((M = DpsSectionMatchListFind(L, Doc, NS, P))) {
	  if (DpsNeedLog(log_level))
	    dps_snprintf(reason, PATH_MAX, "%s %s %s '%s'", M->arg, DpsMatchTypeStr(M->match_type), 
			 M->case_sense ? "Sensitive" : "InSensitive", M->pattern);
	  res = DpsMethod(M->arg);
	}else{
	  if (DpsNeedLog(log_level))
	    dps_snprintf(reason, PATH_MAX, "%s method is used", DpsMethodStr(Doc->method));
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}

static int DpsStoreFilterFind(int log_level, DPS_MATCHLIST *L, DPS_DOCUMENT *Doc, char *reason) {
	DPS_MATCH_PART	P[NS];
	DPS_MATCH	*M;
	int		res = DPS_METHOD_STORE;
	
#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif

	if((M = DpsSectionMatchListFind(L, Doc, NS, P))) {
	  if (DpsNeedLog(log_level))
	    dps_snprintf(reason, PATH_MAX, "%s %s %s '%s'", M->arg, DpsMatchTypeStr(M->match_type), 
			 M->case_sense ? "Sensitive" : "InSensitive", M->pattern);
	  res = DpsMethod(M->arg);
	}else{
	  if (DpsNeedLog(log_level))
	    sprintf(reason, "Store by default");
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}


int DpsSubSectionMatchFind(DPS_AGENT *Indexer, int log_level, DPS_MATCHLIST *L, DPS_DOCUMENT *Doc, char *reason, char **subsection) {
	DPS_MATCH_PART	P[NS];
	DPS_MATCH	*M;
	int		res = DPS_METHOD_UNKNOWN;
	
#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif

	if((M = DpsSectionMatchListFind(L, Doc, NS, P))) {

	  if (DpsNeedLog(log_level))
	    dps_snprintf(reason, PATH_MAX, "%s %s %s '%s' %s", M->arg, DpsMatchTypeStr(M->match_type), 
			 M->case_sense ? "Sensitive" : "InSensitive", M->pattern, M->loose ? "loose" : "");
	  res = DpsMethod(M->arg);
	  if (M->loose) {
	    DPS_VAR *V = NULL;
	    switch(res) {
	    case DPS_METHOD_TAG:
	      V = DpsVarListFind(&Doc->Sections, "Tag");
	      if (V == NULL && Doc->Server != NULL) V = DpsVarListFind(&Doc->Server->Vars, "Tag");
	      break;
	    case DPS_METHOD_CATEGORY:
	      V = DpsVarListFind(&Doc->Sections, "Category");
	      if (V == NULL && Doc->Server != NULL) V = DpsVarListFind(&Doc->Server->Vars, "Category");
	      break;
	    }
	    if (V != NULL) return DPS_METHOD_UNKNOWN;
	  }
	  if (strchr(M->subsection, (int)'$') != NULL) {
	    char qbuf[16384];
	    DPS_TEMPLATE t;

	    bzero(&t, sizeof(t));
	    t.HlBeg = t.HlEnd = t.GrBeg = t.GrEnd = t.ExcerptMark = NULL;
	    t.Env_Vars = &Doc->Sections;
	    qbuf[0] = '\0';
	    DpsPrintTextTemplate(Indexer, NULL, NULL, qbuf, sizeof(qbuf), &t, M->subsection);
	    *subsection = DpsStrdup(qbuf);
	    DpsTemplateFree(&t);
	  } else {
	    *subsection = DpsStrdup(M->subsection);
	  }
/*	  DpsVarListReplaceInt(&Doc->Sections, "Server_id", M->server_id);*/
	}else{
	  if (DpsNeedLog(log_level))
	    dps_snprintf(reason, PATH_MAX, "No conditional subsection detected");
	  *subsection = NULL;
	}
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}


int DpsHrefCheck(DPS_AGENT *Indexer, DPS_HREF *Href, const char *newhref) {
	char		reason[PATH_MAX+1]="";
	DPS_URL		*newURL;
	DPS_SERVER	*Srv;
	size_t          depth;
	const char      *method, *s;

	if ((newURL = DpsURLInit(NULL)) == NULL) {
	  return DPS_ERROR;
	}
	DpsURLParse(newURL, newhref);
	
	Href->site_id = 0;
	Href->checked = 1;

	if (!strcasecmp(DPS_NULL2EMPTY(newURL->schema), "mailto") 
	    || !strcasecmp(DPS_NULL2EMPTY(newURL->schema), "javascript")
	    || !strcasecmp(DPS_NULL2EMPTY(newURL->schema), "feed")
	    ) {
		DpsLog(Indexer,DPS_LOG_DEBUG,"'%s' schema, skip it", newURL->schema, newhref);
		Href->method=DPS_METHOD_DISALLOW;
		goto check_ret;
	}
	
	/* Check Allow/Disallow/CheckOnly stuff */
	DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
	Href->method = DpsFilterFind(DPS_LOG_DEBUG, &Indexer->Conf->Filters, newhref, reason, DPS_METHOD_GET);
	DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);
	if(Href->method == DPS_METHOD_DISALLOW) {
		DpsLog(Indexer, DPS_LOG_DEBUG, " Filter: %s, skip it", reason);
		goto check_ret;
	}else{
		DpsLog(Indexer, DPS_LOG_DEBUG, " Filter: %s", reason);
	}

	if (!(Indexer->flags & DPS_FLAG_FAST_HREF_CHECK)) {
	  DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
	  Srv = DpsServerFind(Indexer, 0, newhref, newURL->charset_id, NULL);
	  DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);
	  if(Srv == NULL) {
		DpsLog(Indexer, DPS_LOG_DEBUG, " Server: no, skip it");
		Href->method = DPS_METHOD_DISALLOW;
		goto check_ret;
	  }
	
	  DpsLog(Indexer, DPS_LOG_DEBUG, " Server: site_id: %d pattern: %s", Srv->site_id, Srv->Match.pattern);
	  Href->server_id = Srv->site_id;

	  if (dps_strlen(newhref) > Srv->MaxURLength) {
	    DpsLog(Indexer, DPS_LOG_DEBUG, "too long URL (%d, max: %d), skip it", dps_strlen(newhref), Srv->MaxURLength);
	    Href->method = DPS_METHOD_DISALLOW;
	    goto check_ret;
	  }

	
	  method = DpsVarListFindStr(&Srv->Vars, "Method", "Allow");
	  if((Href->method = DpsMethod(method)) != DPS_METHOD_DISALLOW) {
	    /* Check Allow/Disallow/CheckOnly stuff */
	    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	    Href->method = DpsFilterFind(DPS_LOG_DEBUG, &Indexer->Conf->Filters, newhref, reason, Href->method);
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	    DpsLog(Indexer, DPS_LOG_DEBUG, "%s", reason);
	  }
	
	  if(Href->method == DPS_METHOD_DISALLOW) {
	    DpsLog(Indexer, DPS_LOG_DEBUG, "Disallowed by Server/Realm/Disallow command, skip it");
	    goto check_ret;
	  }

	  if(Href->method == DPS_METHOD_VISITLATER) {
	    DpsLog(Indexer, DPS_LOG_DEBUG, "Visit later by Server/Realm/Skip command, skip it");
	    goto check_ret;
	  }

/*
	  if (Href->hops > Srv->MaxHops) {
	    DpsLog(Indexer, DPS_LOG_DEBUG, "too many hops (%d, max: %d), skip it", Href->hops, Srv->MaxHops);
	    Href->method = DPS_METHOD_DISALLOW;
	    goto check_ret;
	  }
*/
	  depth = 0;
	  for(s = strchr(newURL->path, (int)'/'); s != NULL; s = strchr(++s, (int)'/')) depth++;
	  if (depth > Srv->MaxDepth) {
	    DpsLog(Indexer, DPS_LOG_DEBUG, "too deep depth (%d, max: %d), skip it", depth, Srv->MaxDepth);
	    Href->method = DPS_METHOD_DISALLOW;
	    goto check_ret;
	  }

	  if (Srv->MaxHrefsPerServer != (dps_uint4)-1) {
	    if (Srv->MaxHrefsPerServer > Srv->nhrefs) {
	      Href->method = DPS_METHOD_VISITLATER;
	      DpsLog(Indexer, DPS_LOG_DEBUG, " The maximum of %d hrefs per Server/Realm/Skip command reached, skip it", Srv->MaxHrefsPerServer);
	      goto check_ret;
	    } else {
	      DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
	      if (Srv) Srv->nhrefs++;
	      DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);
	    }
	  }

	  Href->delay = (int)(Srv->crawl_delay / 1000);
#if 1
	  if (Srv->use_robots) {
	    DPS_ROBOT_RULE	*rule;
	    rule = DpsRobotRuleFind(Indexer, Srv, NULL, newURL, 0, 0);
	    if (rule) {
	      DpsLog(Indexer, DPS_LOG_DEBUG, "Href.robots.txt: '%s %s'", (rule->cmd==DPS_METHOD_DISALLOW)?"Disallow":"Allow", rule->path);
	      if ((rule->cmd == DPS_METHOD_DISALLOW) || (rule->cmd == DPS_METHOD_VISITLATER) ) {
		Href->method = rule->cmd;
		goto check_ret;
	      }
	    }
	  }
#endif
	}
 check_ret:
	DpsURLFree(newURL);
	return DPS_OK;
}

__C_LINK int __DPSCALL DpsStoreHrefs(DPS_AGENT * Indexer) {
        char            dbuf[64];
	size_t		i;
	int             res;
	DPS_HREF        *H;
	DPS_DOCUMENT	Doc;
	time_t next_index_time = Indexer->now;

	DpsDocInit(&Doc);

	if (Indexer->Flags.collect_links) {
	  for (i = 0; i < Indexer->Hrefs.dhrefs; i++) {
	    H = &Indexer->Hrefs.Href[i];
	    if(H->stored == 0) {
	      if (!H->checked) DpsHrefCheck(Indexer, H, H->url);
	      if( H->method != DPS_METHOD_DISALLOW 
		  && H->method != DPS_METHOD_VISITLATER
		  /*dps_strlen(H->url) <= DPS_URLSIZE*/) {/* FIXME: replace this by config parameter chacking */
				DpsVarListReplaceInt(&Doc.Sections, "Referrer-ID", H->referrer);
				DpsVarListReplaceUnsigned(&Doc.Sections,"Hops", H->hops);
				DpsVarListReplaceStr(&Doc.Sections, "URL",H->url?H->url:"");
				DpsVarListReplaceInt(&Doc.Sections, "Site_id", H->site_id);
				DpsVarListReplaceInt(&Doc.Sections, "Server_id", H->server_id);
				DpsVarListReplaceDouble(&Doc.Sections, "weight", (double)H->weight);
				DpsVarListDel(&Doc.Sections, "E_URL");
				DpsVarListDel(&Doc.Sections, "URL_ID");
				Doc.charset_id = H->charset_id;
				if(DPS_OK != (res = DpsURLAction(Indexer, &Doc, DPS_URL_ACTION_ADD_LINK))){
				        DpsDocFree(&Doc);
					return(res);
				}
	      }
	      H->stored=1;
	    }
	  }
	}
	for (i = Indexer->Hrefs.dhrefs; i < Indexer->Hrefs.nhrefs; i++) {
	  H = &Indexer->Hrefs.Href[i];
	  if(H->stored == 0) {
	    if (!H->checked) DpsHrefCheck(Indexer, H, H->url);
	    DpsVarListReplaceInt(&Doc.Sections, "Referrer-ID", H->referrer);
	    DpsVarListReplaceUnsigned(&Doc.Sections,"Hops", H->hops);
	    DpsVarListReplaceStr(&Doc.Sections,"URL",H->url?H->url:"");
	    DpsVarListReplaceInt(&Doc.Sections,"Site_id", H->site_id);
	    DpsVarListReplaceInt(&Doc.Sections,"Server_id", H->server_id);
	    DpsVarListReplaceDouble(&Doc.Sections, "weight", (double)H->weight);
	    DpsVarListDel(&Doc.Sections, "E_URL");
	    DpsVarListDel(&Doc.Sections, "URL_ID");
	    Doc.charset_id = H->charset_id;
	    if (H->delay) {
	      dps_snprintf(dbuf, sizeof(dbuf), "%lu", next_index_time + H->delay);
	      DpsVarListReplaceStr(&Doc.Sections, "Next-Index-Time", dbuf);
	    }
	    if( H->method != DPS_METHOD_DISALLOW && H->method != DPS_METHOD_VISITLATER
		/*dps_strlen(H->url) <= DPS_URLSIZE*/) { /* FIXME: replace this by config parameter chacking */
				if(DPS_OK != (res = DpsURLAction(Indexer, &Doc, DPS_URL_ACTION_ADD))){
				        DpsDocFree(&Doc);
					return(res);
				}
	    } else {
				if(DPS_OK != (res = DpsURLAction(Indexer, &Doc, DPS_URL_ACTION_ADD_LINK))){
				        DpsDocFree(&Doc);
					return(res);
				}
	    }
	    H->stored=1;
	  }
	}
	DpsDocFree(&Doc);
	
	/* Remember last stored URL num */
	/* Note that it will became 0   */
	/* after next sort in AddUrl    */
	Indexer->Hrefs.dhrefs = Indexer->Hrefs.nhrefs - ((Indexer->Hrefs.nhrefs > 0) ? 1 : 0);
	
	/* We should not free URL list with onw database */
	/* to avoid double indexing of the same document */
	/* So, do it if compiled with SQL only           */
	
	/* FIXME: this is incorrect with both SQL and built-in compiled */
	if(Indexer->Hrefs.nhrefs > MAXHSIZE)
		DpsHrefListFree(&Indexer->Hrefs);

	return DPS_OK;
}


static int DpsDocBaseHref(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc){
	const char	*basehref=DpsVarListFindStr(&Doc->Sections,"base.href",NULL);
	/* <BASE HREF="xxx"> stuff            */
	/* Check that URL is properly formed  */
	/* baseURL is just temporary variable */
	/* If parsing  fails we'll use old    */
	/* base href, passed via CurURL       */
	/* Note that we will not check BASE     */
	/* if delete_no_server is unset         */
	/* This is  actually dirty hack. We     */
	/* must check that hostname is the same */
	
	if(basehref /*&& (Doc->Spider.follow==DPS_FOLLOW_WORLD)*/){
		DPS_URL		*baseURL = DpsURLInit(NULL);
		int		parse_res;

		if (baseURL == NULL) return DPS_ERROR;

		parse_res = DpsURLParse(baseURL, basehref);
		if (!parse_res && (baseURL->schema == NULL || baseURL->hostinfo == NULL)) parse_res = DPS_URL_BAD;

		if(!parse_res) {
			DpsURLParse(&Doc->CurURL,basehref);
			DpsLog(Indexer, DPS_LOG_DEBUG, "BASE HREF '%s'", basehref);
		}else{
			switch(parse_res){
			case DPS_URL_LONG:
				DpsLog(Indexer,DPS_LOG_ERROR,"BASE HREF too long: '%s'",basehref);
				break;
			case DPS_URL_BAD:
			default:
				DpsLog(Indexer,DPS_LOG_ERROR,"Error in BASE HREF URL: '%s'",basehref);
			}
		}
		DpsURLFree(baseURL);
	}
	return DPS_OK;
}


int DpsConvertHref(DPS_AGENT *Indexer, DPS_URL *CurURL, DPS_HREF *Href){
	int		parse_res;
	DPS_URL		*newURL;
	char		*newhref = NULL;

#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif
	if ((newURL = DpsURLInit(NULL)) == NULL) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return DPS_ERROR;
	}
	
	if((parse_res=DpsURLParse(newURL, Href->url))){
		switch(parse_res){
			case DPS_URL_LONG:
				DpsLog(Indexer,DPS_LOG_DEBUG,"URL too long: '%s'",Href->url);
				break;
			case DPS_URL_BAD:
			default:
				DpsLog(Indexer,DPS_LOG_DEBUG,"Error in URL: '%s'",Href->url);
		}
	}

	newURL->charset_id = Href->charset_id;
	RelLink(Indexer, CurURL, newURL, &newhref, 1);
	
	DpsLog(Indexer,DPS_LOG_DEBUG,"Link '%s' %s",Href->url,newhref);

	DpsHrefCheck(Indexer, Href, newhref);

	DPS_FREE(Href->url);
	Href->url = (char*)DpsStrdup(newhref);
/* ret:*/
	DPS_FREE(newhref);
	DpsURLFree(newURL);
#ifdef WITH_PARANOIA
	DpsViolationExit(Indexer->handle, paran);
#endif
	return DPS_OK;
}

static int DpsDocConvertHrefs(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc){
	size_t		i;
	dps_uint4	hops = DpsVarListFindUnsigned(&Doc->Sections, "Hops", 0);
	urlid_t		url_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
	dps_uint4       maxhops = DpsVarListFindUnsigned(&Doc->Sections, "MaxHops", 255);
	urlid_t         server_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "Server_id", 0);

	for(i=0;i<Doc->Hrefs.nhrefs;i++){
		DPS_HREF	*Href=&Doc->Hrefs.Href[i];
		Href->hops = hops + 1;
		Href->charset_id = Doc->charset_id;
		DpsConvertHref(Indexer,&Doc->CurURL,Href);
		Href->referrer=url_id;
		if ((server_id != Href->server_id) || (maxhops >= Href->hops)) {
		  Href->stored = 0;
		} else {
		  if (Href->method != DPS_METHOD_DISALLOW) 
		    DpsLog(Indexer, DPS_LOG_DEBUG, " link: too many hops (%d, max: %d)", Href->hops, maxhops);
		  Href->stored = 1;
		  Href->method = DPS_METHOD_DISALLOW;
		}
	}
	return DPS_OK;
}

int DpsDocStoreHrefs(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc){
	size_t i;
	float weight;

	if(Doc->method==DPS_METHOD_HEAD)
		return DPS_OK;
	
	DpsDocBaseHref(Indexer,Doc);
	DpsDocConvertHrefs(Indexer,Doc);
	weight = (float)((Doc->Hrefs.nhrefs) ?  (1.0 / Doc->Hrefs.nhrefs) : 0.1);
	for(i=0;i<Doc->Hrefs.nhrefs;i++){
		DPS_HREF	*Href=&Doc->Hrefs.Href[i];
		if((Href->method != DPS_METHOD_DISALLOW) && (Href->method != DPS_METHOD_VISITLATER)) {
		  Href->charset_id = Doc->charset_id;
		  Href->weight = weight;
		  DpsHrefListAdd(Indexer, &Indexer->Hrefs, Href);
		}
	}
	return DPS_OK;
}

/*********************** 'UrlFile' stuff (for -f option) *******************/

__C_LINK int __DPSCALL DpsURLFile(DPS_AGENT *Indexer, const char *fname,int action){
	FILE *url_file;
	char str[1024]="";
	char str1[1024]="";
	int result, res, cnt_flag = 0;
	DPS_URL *myurl;
	DPS_HREF Href;
	char *end;

#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif

	if ((myurl = DpsURLInit(NULL)) == NULL) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return DPS_ERROR;
	}
	
	/* Read lines and clear/insert/check URLs                     */
	/* We've already tested in main.c to make sure it can be read */
	/* FIXME !!! Checking should be done here surely              */

	if(!strcmp(fname,"-"))
		url_file=stdin;
	else
		url_file=fopen(fname,"r");
	
	if (action == DPS_URL_FILE_TARGET && Indexer->Conf->url_number ==  0x7FFFFFFF) {
	  cnt_flag = 1;
	  Indexer->Conf->url_number = 0;
	}

	while(1) {
	        if (fgets(str1, sizeof(str1), url_file) == NULL){
		  if (feof(url_file)) break;
		  if (ferror(url_file)) {
		    if (errno == EAGAIN) continue;
		    dps_strerror(Indexer, DPS_LOG_ERROR, "Error reading URL file %s", (strcmp(fname, "-")) ? "<STDIN>" : fname);
		    if(url_file != stdin) fclose(url_file);
		    DpsURLFree(myurl);
#ifdef WITH_PARANOIA
		    DpsViolationExit(Indexer->handle, paran);
#endif
		    return(DPS_ERROR);
		  }
		}
		if(!str1[0])continue;
		end=str1+dps_strlen(str1)-1;
		while((end>=str1)&&(*end==CR_CHAR||*end==NL_CHAR)){
			*end=0;if(end>str1)end--;
		}
		if(!str1[0])continue;
		if(str1[0]=='#')continue;

		if(*end=='\\'){
			*end=0; dps_strcat(str, str1);
			continue;
		}
		dps_strcat(str, str1);
		dps_strcpy(str1,"");

		switch(action){
		case DPS_URL_FILE_REINDEX:
		        DpsLog(Indexer, DPS_LOG_EXTRA, "Marking for reindexing: %s", str);
		        if (strchr(str, '%'))
			  DpsVarListReplaceStr(&Indexer->Vars, "ul", str);
			else
			  DpsVarListReplaceStr(&Indexer->Vars, "ue", str);
			result = DpsURLAction(Indexer, NULL, DPS_URL_ACTION_EXPIRE);
			if(result!=DPS_OK) { DpsURLFree(myurl); 
#ifdef WITH_PARANOIA
			  DpsViolationExit(Indexer->handle, paran);
#endif
			return(result); }
			DpsVarListDel(&Indexer->Vars, "ul");
			DpsVarListDel(&Indexer->Vars, "ue");
			break;
		case DPS_URL_FILE_TARGET:
		        DpsLog(Indexer, DPS_LOG_EXTRA, "Targeting for indexing: %s", str);
		        if (strchr(str, '%'))
			  DpsVarListReplaceStr(&Indexer->Vars, "ul", str);
			else
			  DpsVarListReplaceStr(&Indexer->Vars, "ue", str);
			/*result =*/ DpsAppendTarget(Indexer, str, "", 0, 0);
/*			if(result!=DPS_OK) { DpsURLFree(myurl); 
#ifdef WITH_PARANOIA
			  DpsViolationExit(Indexer->handle, paran);
#endif
			return(result); }*/
			if (cnt_flag) Indexer->Conf->url_number++;
			DpsVarListDel(&Indexer->Vars, "ul");
			DpsVarListDel(&Indexer->Vars, "ue");
			break;
		case DPS_URL_FILE_CLEAR:
		        DpsLog(Indexer, DPS_LOG_EXTRA, "Deleting: %s", str);
		        if (strchr(str, '%'))
			  DpsVarListReplaceStr(&Indexer->Vars, "ul", str);
			else
			  DpsVarListReplaceStr(&Indexer->Vars, "ue", str);
			result=DpsClearDatabase(Indexer);
			if(result!=DPS_OK) { DpsURLFree(myurl); 
#ifdef WITH_PARANOIA
			  DpsViolationExit(Indexer->handle, paran);
#endif
			return(DPS_ERROR); }
			DpsVarListDel(&Indexer->Vars, "ul");
			DpsVarListDel(&Indexer->Vars, "ue");
			break;
		case DPS_URL_FILE_INSERT:
		        DpsLog(Indexer, DPS_LOG_EXTRA, "Inserting: %s", str);
			DpsHrefInit(&Href);
			Href.url=str;
			Href.method=DPS_METHOD_GET;
			DpsHrefListAdd(Indexer, &Indexer->Hrefs, &Href);
			if (Indexer->Hrefs.nhrefs > MAXHSIZE) DpsStoreHrefs(Indexer);
			break;
		case DPS_URL_FILE_PARSE:
		        DpsLog(Indexer, DPS_LOG_EXTRA, "Parsing: %s", str);
			res=DpsURLParse(myurl, str);
			if((res != DPS_OK) && (myurl->schema == NULL))
				res=DPS_URL_BAD;
			if(res){
				switch(res){
				case DPS_URL_LONG:
					DpsLog(Indexer,DPS_LOG_ERROR,"URL too long: '%s'",str);
					break;
				case DPS_URL_BAD:
				default:
					DpsLog(Indexer,DPS_LOG_ERROR,"Error in URL: '%s'",str);
				}
				DpsURLFree(myurl);
#ifdef WITH_PARANOIA
				DpsViolationExit(Indexer->handle, paran);
#endif
				return(DPS_ERROR);
			}
			break;
		}
		str[0]=0;
	}
	if(url_file!=stdin)
		fclose(url_file);
	DpsURLFree(myurl);
#ifdef WITH_PARANOIA
	DpsViolationExit(Indexer->handle, paran);
#endif
	return(DPS_OK);
}




/*******************************************************************/


int DpsDocAlias(DPS_AGENT *Indexer,DPS_DOCUMENT *Doc){
	DPS_MATCH	*Alias;
	DPS_MATCH_PART	Parts[10];
	size_t		alstrlen, nparts=10;
	const char	*alias_prog=DpsVarListFindStr(&Indexer->Vars, "AliasProg", NULL);
	char		*alstr;
	int		result=DPS_OK;
	const char	*url=DpsVarListFindStr(&Doc->Sections,"URL","");

	alstrlen = 256 + 2 * dps_strlen(url);
	if ((alstr = (char*)DpsMalloc(alstrlen)) == NULL) return DPS_ERROR;
	alstr[0] = '\0';
	if(alias_prog){
		result = DpsAliasProg(Indexer, alias_prog, url, alstr, alstrlen - 1);
		DpsLog(Indexer,DPS_LOG_EXTRA,"AliasProg result: '%s'",alstr);
		if(result!=DPS_OK) { DPS_FREE(alstr); return result; }
	}
	
	/* Find alias when aliastr is empty, i.e.     */
	/* when there is no alias in "Server" command */
	/* and no AliasProg                           */
	if (!alstr[0]) {
	  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	  if((Alias = DpsMatchListFind(&Indexer->Conf->Aliases, url, nparts, Parts))) {
		DpsMatchApply(alstr, alstrlen - 1, url, Alias->arg, Alias, nparts, Parts);
	  }
	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	}
	if(alstr[0]){
		DpsVarListReplaceStr(&Doc->Sections,"Alias",alstr);
	}
	DPS_FREE(alstr);
	return DPS_OK;
}



int DpsDocCheck(DPS_AGENT *Indexer, DPS_SERVER *CurSrv, DPS_DOCUMENT *Doc) {
	char		reason[1024]="";
	int		nerrors=-1;
	int		hops=DpsVarListFindInt(&Doc->Sections,"Hops",0);
	const char	*method=DpsVarListFindStr(&CurSrv->Vars,"Method","Allow");
	const int       older = DpsVarListFindInt(&Doc->Sections, "DeleteOlder", 0);
	int             num_method = DpsMethod(method);
	float           site_weight;
	size_t          depth;
	const char      *s;
	
	TRACE_IN(Indexer, "DpsDocCheck");

	switch(CurSrv->Match.match_type){
		case DPS_MATCH_WILD:
		  DpsLog(Indexer,(num_method!=DPS_METHOD_DISALLOW)?DPS_LOG_DEBUG:DPS_LOG_EXTRA, "Realm %s wild '%s'", method, CurSrv->Match.pattern);
			break;
		case DPS_MATCH_REGEX:
			DpsLog(Indexer,(num_method!=DPS_METHOD_DISALLOW)?DPS_LOG_DEBUG:DPS_LOG_EXTRA, "Realm %s regex '%s'", method, CurSrv->Match.pattern);
			break;
		case DPS_MATCH_SUBNET:
			DpsLog(Indexer,(num_method!=DPS_METHOD_DISALLOW)?DPS_LOG_DEBUG:DPS_LOG_EXTRA, "Subnet %s '%s'", method, CurSrv->Match.pattern);
			break;
		case DPS_MATCH_BEGIN:
		default:
			DpsLog(Indexer,(num_method!=DPS_METHOD_DISALLOW)?DPS_LOG_DEBUG:DPS_LOG_EXTRA, "Server %s '%s'", method, CurSrv->Match.pattern);
			break;
	}
	
	if (dps_strlen(DpsVarListFindStr(&Doc->Sections,"URL","")) > CurSrv->MaxURLength) {
	  DpsLog(Indexer, DPS_LOG_EXTRA, "too long URL (max: %d)", CurSrv->MaxURLength);
	  Doc->method = DPS_METHOD_DISALLOW;
	  TRACE_OUT(Indexer);
	  return DPS_OK;
	}

	if((Doc->method = num_method) != DPS_METHOD_DISALLOW) {  /* was: == DPS_METHOD_GET */
	  /* Check Allow/Disallow/CheckOnly stuff */
	  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	  Doc->method = DpsFilterFind((Doc->method != DPS_METHOD_DISALLOW) ? DPS_LOG_DEBUG : DPS_LOG_EXTRA, &Indexer->Conf->Filters, DpsVarListFindStr(&Doc->Sections,"URL",""), reason,Doc->method);
	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  DpsLog(Indexer, (Doc->method != DPS_METHOD_DISALLOW) ? DPS_LOG_DEBUG : DPS_LOG_EXTRA, "%s", reason);
	}
	
	if(Doc->method==DPS_METHOD_DISALLOW) {
	  TRACE_OUT(Indexer);
	  return DPS_OK;
	}
	
	/* Check that hops is less than MaxHops */
	if(hops>Doc->Spider.maxhops){
	        DpsLog(Indexer, DPS_LOG_WARN, "Too many hops (%d, max: %d)", hops, Doc->Spider.maxhops);
		Doc->method = DPS_METHOD_DISALLOW;
		TRACE_OUT(Indexer);
		return DPS_OK;
	}

	/* Check that depth is less than MaxDepth */
	depth = 0;
	for(s = strchr(Doc->CurURL.path, (int)'/'); s != NULL; s = strchr(++s, (int)'/')) depth++;
	if (depth > CurSrv->MaxDepth) {
	  DpsLog(Indexer, DPS_LOG_DEBUG, "too deep depth (%d, max: %d), skip it", depth, CurSrv->MaxDepth);
	  Doc->method = DPS_METHOD_DISALLOW;
	  TRACE_OUT(Indexer);
	  return DPS_OK;
	}

	/* Check for older docs */
	if (older > 0) {
	  time_t now = Indexer->now, last_mod_time = DpsHttpDate2Time_t(DpsVarListFindStr(&Doc->Sections, "Last-Modified", ""));

	  if (last_mod_time > 0) {
	    if ((int)(now - last_mod_time) > older) {
	      DpsLog(Indexer, DPS_LOG_EXTRA, "Too old document (%d > %d)", now - last_mod_time, older);
	      Doc->method = DPS_METHOD_DISALLOW;
	      TRACE_OUT(Indexer);
	      return DPS_OK;
	    }
	  } else {
	    time_t since = (time_t)DpsVarListFindInt(&Doc->Sections, "Since", 0);
	    if ((int)(now - since) > older) {
	      DpsLog(Indexer, DPS_LOG_EXTRA, "Too old document (%d > %d)", now - since, older);
	      Doc->method = DPS_METHOD_DISALLOW;
	      TRACE_OUT(Indexer);
	      return DPS_OK;
	    }
	  }
	}

	/* Check for too many errors on this server */
	if (Indexer->Flags.cmd != DPS_IND_FILTER) DpsDocLookupConn(Indexer, Doc);
	nerrors = (Doc->connp.Host != NULL) ?  Doc->connp.Host->net_errors : 0;
	
	if((nerrors>=Doc->Spider.max_net_errors)&&(Doc->Spider.max_net_errors)){
		time_t	next_index_time = Indexer->now + Doc->Spider.net_error_delay_time;
		char	buf[64];
		
		DpsLog(Indexer,DPS_LOG_WARN,"Too many network errors (%d) for this server",nerrors);
		DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_SERVICE_UNAVAILABLE);
		dps_snprintf(buf, sizeof(buf), "%lu", (unsigned long int)((next_index_time & 0x80000000) ? 0x7fffffff : next_index_time));
		DpsVarListReplaceStr(&Doc->Sections,"Next-Index-Time",buf);
		Doc->method = DPS_METHOD_VISITLATER;
		if (nerrors == Doc->Spider.max_net_errors) {
		  DpsVarListReplaceInt(&Doc->Sections, "Site_id", DpsServerGetSiteId(Indexer, CurSrv, Doc));
		  DpsURLAction(Indexer, Doc, DPS_URL_ACTION_POSTPONE_ON_ERR);
		}
		TRACE_OUT(Indexer);
		return DPS_OK;
	}

	/* Check referring status, if need */
	if (Indexer->Flags.skip_unreferred && ((Indexer->flags & DPS_FLAG_REINDEX) == 0) && (DpsCheckReferrer(Indexer, Doc) != DPS_OK)) {
	  int prevstatus = DpsVarListFindInt(&Doc->Sections, "PrevStatus", 0);
	  if (prevstatus > 0) {
	    DpsLog(Indexer, DPS_LOG_EXTRA, "Unreferred, %s it",(Indexer->Flags.skip_unreferred == DPS_METHOD_DISALLOW) ? "delete" : "skip");
	    Doc->method = Indexer->Flags.skip_unreferred; /* DPS_METHOD_VISITLATER or DPS_METHO_DISALLOW */
	    if (prevstatus < 400) {
	      DpsVarListReplaceInt(&Doc->Sections,"Status", DPS_HTTP_STATUS_NOT_MODIFIED);
	    } else {
	      DpsVarListReplaceInt(&Doc->Sections, "Status", prevstatus);
	    }
	    TRACE_OUT(Indexer);
	    return DPS_OK;
	  }
	}

	DpsVarListReplaceInt(&Doc->Sections, "Site_id", DpsServerGetSiteId(Indexer, CurSrv, Doc));
	site_weight = (float)DpsVarListFindDouble(&Doc->Sections, "SiteWeight", 0.0);
	/* Check weights */
	if (site_weight < CurSrv->MinSiteWeight) {
	  DpsLog(Indexer, DPS_LOG_EXTRA, "Too low site weight (%f < %f)", site_weight, CurSrv->MinSiteWeight);
	      Doc->method = DPS_METHOD_VISITLATER;
	      TRACE_OUT(Indexer);
	      return DPS_OK;
	}
	if (CurSrv->weight < CurSrv->MinServerWeight) {
	  DpsLog(Indexer, DPS_LOG_EXTRA, "Too low server weight (%f < %f)", CurSrv->weight, CurSrv->MinServerWeight);
	      Doc->method = DPS_METHOD_VISITLATER;
	      TRACE_OUT(Indexer);
	      return DPS_OK;
	}
	

	{
	  const char *param = DpsVarListFindStr(&CurSrv->Vars, "IndexDocSizeLimit", NULL);

	  if (param != NULL) DpsVarListAddStr(&Doc->Sections, "IndexDocSizeLimit", param);
	}
	
	TRACE_OUT(Indexer);
	return DPS_OK;
}


static int DpsParseSections(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
  DPS_MATCH       *Alias;
  DPS_MATCH_PART  Parts[10];
  size_t nparts = 10;
  DPS_VAR *Sec;
  char *lt, *buf;
  const char *buf_content = (Doc->Buf.pattern == NULL) ? Doc->Buf.content : Doc->Buf.pattern;
  char savec;
  DPS_TEXTITEM Item;
  DPS_HREF     Href;
  size_t i, buf_len;

/*  fprintf(stderr, " -- SectionMatch.n:%d\n", Indexer->Conf->SectionMatch.nmatches);*/

  if (Indexer->Conf->SectionMatch.nmatches == 0 && Indexer->Conf->HrefSectionMatch.nmatches == 0) return DPS_OK;

  buf = (char*)DpsMalloc(buf_len = (Doc->Buf.size + 1024));
  if (buf == NULL) return DPS_OK;

  for (i = 0; i < Indexer->Conf->SectionMatch.nmatches; i++) {
    Alias = &Indexer->Conf->SectionMatch.Match[i];

    Sec = DpsVarListFind(&Doc->Sections, Alias->section);
/*    fprintf(stderr, " -- Alias:%s, Sec: %x\n", Alias->section, Sec);*/
    if (! Sec) continue;

    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
/*    fprintf(stderr, " -- Alias:%s, before match  pattern:%s\n", Alias->section, Alias->pattern);*/
    if (DpsMatchExec(Alias, buf_content, buf_content, NULL, nparts, Parts)) {
      DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
      continue;
    }

    DpsMatchApply(buf, buf_len - 1, buf_content, Alias->arg, Alias, nparts, Parts);
/*    fprintf(stderr, " -- Alias:%s, after match apply: %s\n", Alias->section, buf);*/
    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);

    bzero((void*)&Item, sizeof(Item));
    Item.href = NULL;
    Item.section = Sec->section;
    Item.strict = Sec->strict;
    Item.section_name = Sec->name;
    Item.str = dps_strtok_r(buf, "\r\n", &lt, &savec);
    while(Item.str) {
      Item.len = (size_t)((lt != NULL) ? (lt - Item.str) : 0 /*dps_strlen(Item.str)*/);
/*      fprintf(stderr, " -- Alias:%s/%s, [%d] %s\n", Sec->name, Alias->section, Item.len, Item.str);*/
      (void)DpsTextListAdd(&Doc->TextList, &Item);
      Item.str = dps_strtok_r(NULL, "\r\n", &lt, &savec);
    }
  }

  for (i = 0; i < Indexer->Conf->HrefSectionMatch.nmatches; i++) {
    Alias = &Indexer->Conf->HrefSectionMatch.Match[i];

    Sec = DpsVarListFind(&Indexer->Conf->HrefSections, Alias->section);
    if (! Sec) continue;

    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
    if (DpsMatchExec(Alias, buf_content, buf_content, NULL, nparts, Parts)) {
      DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
      continue;
    }

    DpsMatchApply(buf, buf_len - 1, buf_content, Alias->arg, Alias, nparts, Parts);
    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);

    DpsHrefInit(&Href);
    Href.referrer = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
    Href.hops = 1 + (dps_uint4)DpsVarListFindInt(&Doc->Sections,"Hops", 0);
    Href.site_id = 0; /*DpsVarListFindInt(&Doc->Sections, "Site_id", 0);*/
    Href.url = buf;
    Href.method = DPS_METHOD_GET;
    DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
  }

  DPS_FREE(buf);
  return DPS_OK;
}


static int DpsSQLSections(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
#ifdef HAVE_SQL
  DPS_MATCH       *Alias;
  DPS_SQLRES	SQLres;
  DPS_VAR *Sec;
  char *buf;
  DPS_TEXTITEM Item;
  size_t i, z, buf_len;

  if (Indexer->Conf->SectionSQLMatch.nmatches == 0) return DPS_OK;

  buf = (char*)DpsMalloc(buf_len = (Doc->Buf.size + 1024));
  if (buf == NULL) return DPS_OK;

  {
    DPS_TEMPLATE t;
    DPS_DBLIST      dbl;
    DPS_DB *db;
    bzero(&t, sizeof(t));
    t.HlBeg = t.HlEnd = t.GrBeg = t.GrEnd = t.ExcerptMark = NULL;
    t.Env_Vars = &Doc->Sections;

    DpsSQLResInit(&SQLres);
    bzero((void*)&Item, sizeof(Item));
    Item.href = NULL;

    for (z = 0; z < Indexer->Conf->SectionSQLMatch.nmatches; z++) {
      Alias = &Indexer->Conf->SectionSQLMatch.Match[z];

      Sec = DpsVarListFind(&Indexer->Conf->Sections, Alias->section);
      if (! Sec) continue;
      buf[0] = '\0';
      DpsPrintTextTemplate(Indexer, NULL, NULL, buf, buf_len, &t, Alias->arg);
      if (Alias->dbaddr != NULL) {
	DpsDBListInit(&dbl);
	DpsDBListAdd(&dbl, Alias->dbaddr, DPS_OPEN_MODE_READ);
	db = &dbl.db[0];
      } else {
	db = (Indexer->flags & DPS_FLAG_UNOCON) ? &Indexer->Conf->dbl.db[0] :  &Indexer->dbl.db[0];
	if (Indexer->flags & DPS_FLAG_UNOCON) DPS_GETLOCK(Indexer, DPS_LOCK_DB);
      }
      if (DPS_OK != DpsSQLQuery(db, &SQLres, buf)) DpsLog(Indexer, DPS_ERROR, "SectionSQL error");
      for (i = 0; i < DpsSQLNumRows(&SQLres); i++) {
	Item.str = DpsSQLValue(&SQLres, i, 0);
	Item.section = Sec->section;
	Item.strict = Sec->strict;
	Item.section_name = Sec->name;
	Item.len = 0;
	(void)DpsTextListAdd(&Doc->TextList, &Item);
      }
      if (Alias->dbaddr != NULL) DpsDBListFree(&dbl);
      else if (Indexer->flags & DPS_FLAG_UNOCON) DPS_RELEASELOCK(Indexer, DPS_LOCK_DB);
    }  
    DpsTemplateFree(&t);
  }

  DpsSQLFree(&SQLres);
  DPS_FREE(buf);
#endif
  return DPS_OK;
}



#ifdef HAVE_LIBEXTRACTOR

#if EXTRACTOR_VERSION < 0x00060000
static const char * DpsLibextractorMsgName(int type) {
  switch(type) {
/*  case EXTRACTOR_UNKNOWN: return "body";*/
  case EXTRACTOR_FILENAME: return "Filename";
  case EXTRACTOR_MIMETYPE: return "Mimetype";
  case EXTRACTOR_TITLE: return "Title";
  case EXTRACTOR_AUTHOR: return "Author";
  case EXTRACTOR_ARTIST: return "Artist";
  case EXTRACTOR_DESCRIPTION: return "Description";
  case EXTRACTOR_COMMENT: return "Comment";
  case EXTRACTOR_DATE: return "Date";
  case EXTRACTOR_PUBLISHER: return "Publisher";
  case EXTRACTOR_LANGUAGE: return "Content-Language";
  case EXTRACTOR_ALBUM: return "Album";
  case EXTRACTOR_GENRE: return "Genre";
  case EXTRACTOR_LOCATION: return "Location";
  case EXTRACTOR_VERSIONNUMBER: return "VersionNumber";
  case EXTRACTOR_ORGANIZATION: return "Organization";
  case EXTRACTOR_COPYRIGHT: return "Copyright";
  case EXTRACTOR_SUBJECT: return "Subject";
  case EXTRACTOR_KEYWORDS: return "Meta.Keywords";
  case EXTRACTOR_CONTRIBUTOR: return "Contributor";
  case EXTRACTOR_RESOURCE_TYPE: return "Resource-Type";
  case EXTRACTOR_FORMAT: return "Format";
  case EXTRACTOR_RESOURCE_IDENTIFIER: return "Resource-Idendifier";
  case EXTRACTOR_SOURCE: return "Source";
  case EXTRACTOR_RELATION: return "Relation";
  case EXTRACTOR_COVERAGE: return "Coverage";
  case EXTRACTOR_SOFTWARE: return "Software";
  case EXTRACTOR_DISCLAIMER: return "Disclaimer";
  case EXTRACTOR_WARNING: return "Warning";
  case EXTRACTOR_TRANSLATED: return "Translated";
  case EXTRACTOR_CREATION_DATE: return "Creation-Date";
  case EXTRACTOR_MODIFICATION_DATE: return "Modification-Date";
  case EXTRACTOR_CREATOR: return "Creator";
  case EXTRACTOR_PRODUCER: return "Producer";
  case EXTRACTOR_PAGE_COUNT: return "Page-Count";
  case EXTRACTOR_PAGE_ORIENTATION: return "Page-Orientation";
  case EXTRACTOR_PAPER_SIZE: return "Paper-Size";
  case EXTRACTOR_USED_FONTS: return "Used-Fonts";
  case EXTRACTOR_PAGE_ORDER: return "Page-Order";
  case EXTRACTOR_CREATED_FOR: return "Created-For";
  case EXTRACTOR_MAGNIFICATION: return "Magnification";
  case EXTRACTOR_RELEASE: return "Release";
  case EXTRACTOR_GROUP: return "Group";
  case EXTRACTOR_SIZE: return "Size";
  case EXTRACTOR_SUMMARY: return "Summary";
  case EXTRACTOR_PACKAGER: return "Packager";
  case EXTRACTOR_VENDOR: return "Vendor";
  case EXTRACTOR_LICENSE: return "License";
  case EXTRACTOR_DISTRIBUTION: return "Distribution";
  case EXTRACTOR_BUILDHOST: return "BuildHost";
  case EXTRACTOR_OS: return "OS";
  case EXTRACTOR_DEPENDENCY: return "Dependency";
  case EXTRACTOR_HASH_MD4: return "Hash-MD4";
  case EXTRACTOR_HASH_MD5: return "Hash-MD5";
  case EXTRACTOR_HASH_SHA0: return "Hash-SHA0";
  case EXTRACTOR_HASH_SHA1: return "Hash-SHA1";
  case EXTRACTOR_HASH_RMD160: return "Hash-RMD160";
  case EXTRACTOR_RESOLUTION: return "Resolution";
  case EXTRACTOR_CATEGORY: return "Ext.Category";
  case EXTRACTOR_BOOKTITLE: return "BookTitle";
  case EXTRACTOR_PRIORITY: return "Priority";
  case EXTRACTOR_CONFLICTS: return "Conflicts";
  case EXTRACTOR_REPLACES: return "Replaces";
  case EXTRACTOR_PROVIDES: return "Provides";
  case EXTRACTOR_CONDUCTOR: return "Conductor";
  case EXTRACTOR_INTERPRET: return "Interpret";
  case EXTRACTOR_OWNER: return "Owner";
  case EXTRACTOR_LYRICS: return "Lyrics";
  case EXTRACTOR_MEDIA_TYPE: return "Media-Type";
  case EXTRACTOR_CONTACT: return "Contact";
  case EXTRACTOR_THUMBNAIL_DATA: return "Thumbnail-Data";
  case EXTRACTOR_PUBLICATION_DATE: return "Publication-Date";
  case EXTRACTOR_CAMERA_MAKE: return "Camera-Make";
  case EXTRACTOR_CAMERA_MODEL: return "Camera-Model";
  case EXTRACTOR_EXPOSURE: return "Exposure";
  case EXTRACTOR_APERTURE: return "Aperture";
  case EXTRACTOR_EXPOSURE_BIAS: return "Exposure-Bias";
  case EXTRACTOR_FLASH: return "Flash";
  case EXTRACTOR_FLASH_BIAS: return "Flash-Bias";
  case EXTRACTOR_FOCAL_LENGTH: return "Focal-Length";
  case EXTRACTOR_FOCAL_LENGTH_35MM: return "Focal-Length-35MM";
  case EXTRACTOR_ISO_SPEED: return "ISO-Speed";
  case EXTRACTOR_EXPOSURE_MODE: return "Exposure-Mode";
  case EXTRACTOR_METERING_MODE: return "Metering-Mode";
  case EXTRACTOR_MACRO_MODE: return "Macro-Mode";
  case EXTRACTOR_IMAGE_QUALITY: return "Image-Quality";
  case EXTRACTOR_WHITE_BALANCE: return "White-Balance";
  case EXTRACTOR_ORIENTATION: return "Orientation";
  case EXTRACTOR_TEMPLATE: return "Template";
  case EXTRACTOR_SPLIT: return "Split";
  case EXTRACTOR_PRODUCTVERSION: return "ProductVersion";
  case EXTRACTOR_LAST_SAVED_BY: return "Last-Saved-By";
  case EXTRACTOR_LAST_PRINTED: return "Last-Printed";
  case EXTRACTOR_WORD_COUNT: return "Word-Count";
  case EXTRACTOR_CHARACTER_COUNT: return "Character-Count";
  case EXTRACTOR_TOTAL_EDITING_TIME: return "Total-Editing-Time";
  case EXTRACTOR_THUMBNAILS: return "Thumbnails";
  case EXTRACTOR_SECURITY: return "Security";
  case EXTRACTOR_CREATED_BY_SOFTWARE: return "Created-By-Software";
  case EXTRACTOR_MODIFIED_BY_SOFTWARE: return "Modified-By-Software";
  case EXTRACTOR_REVISION_HISTORY: return "Revision-History";
  case EXTRACTOR_LOWERCASE: return "Lowercase";
  case EXTRACTOR_COMPANY: return "Company";
  case EXTRACTOR_GENERATOR: return "Generator";
  case EXTRACTOR_CHARACTER_SET: return "Meta-Charset";
  case EXTRACTOR_LINE_COUNT: return "Line-Count";
  case EXTRACTOR_PARAGRAPH_COUNT: return "Paragraph-Count";
  case EXTRACTOR_EDITING_CYCLES: return "Editing-Cycles";
  case EXTRACTOR_SCALE: return "Scale";
  case EXTRACTOR_MANAGER: return "Manager";
  case EXTRACTOR_MOVIE_DIRECTOR: return "Movie-Director";
  case EXTRACTOR_DURATION: return "Duration";
  case EXTRACTOR_INFORMATION: return "Information";
  case EXTRACTOR_FULL_NAME: return "Full-Name";
  case EXTRACTOR_CHAPTER: return "Chapter";
  case EXTRACTOR_YEAR: return "Year";
  case EXTRACTOR_LINK: return "Link";
  case EXTRACTOR_MUSIC_CD_IDENTIFIER: return "Music-CD-Identifier";
  case EXTRACTOR_PLAY_COUNTER: return "Play-Counter";
  case EXTRACTOR_POPULARITY_METER: return "Popularity-Meter";
  case EXTRACTOR_CONTENT_TYPE: return "Ext.Content-Type";
  case EXTRACTOR_ENCODED_BY: return "Encoded-By";
  case EXTRACTOR_TIME: return "Time";
  case EXTRACTOR_MUSICIAN_CREDITS_LIST: return "Musician-Credits-List";
  case EXTRACTOR_MOOD: return "Mood";
  case EXTRACTOR_FORMAT_VERSION: return "Format-Version";
  case EXTRACTOR_TELEVISION_SYSTEM: return "Television-System";
  case EXTRACTOR_SONG_COUNT: return "Song-Count";
  case EXTRACTOR_STARTING_SONG: return "Strting-Song";
  case EXTRACTOR_HARDWARE_DEPENDENCY: return "Hardware-Dependency";
  case EXTRACTOR_RIPPER: return "Ripper";
  case EXTRACTOR_FILE_SIZE: return "File-Size";
  case EXTRACTOR_TRACK_NUMBER: return "Track-Number";
  case EXTRACTOR_ISRC: return "ISRC";
  case EXTRACTOR_DISC_NUMBER: return "Disc-Number";
  }
  return "body";
}
#else

typedef struct dps_cls {
  DPS_AGENT *Indexer;
  DPS_DOCUMENT *Doc;
} DPS_CLS;

int Dps_MetaDataProcessor(void *cls, const char *plugin_name, enum EXTRACTOR_MetaType type, enum EXTRACTOR_MetaFormat format, const char *data_mime_type,
			  const char *data, size_t data_len) {
  DPS_CLS *C = (DPS_CLS*)cls;
  DPS_AGENT *Indexer = C->Indexer;
  DPS_DOCUMENT  *Doc = C->Doc;
  DPS_TEXTITEM  Item;
  const char *secname = EXTRACTOR_metatype_to_string(type);
  DPS_VAR       *Sec = DpsVarListFind(&Doc->Sections, secname);
  DPS_VAR       *BSec = DpsVarListFind(&Doc->Sections, "body");

  bzero((void*)&Item, sizeof(Item));
  Item.href = NULL;

  DpsLog(Indexer, DPS_LOG_DEBUG, "Libextracted %s: %s", secname, data);
  if (Sec != NULL || BSec != NULL) {
    Item.section = (Sec != NULL) ? Sec->section : BSec->section;
    Item.strict = (Sec != NULL) ? Sec->strict : BSec->strict;
    Item.str = data;
    Item.section_name = (Sec) ? Sec->name : BSec->name;
    Item.len = data_len;
    (void)DpsTextListAdd(&Doc->ExtractorList, &Item);
  }

  return DPS_OK;
}

#endif

#endif /* HAVE_LIBEXTRACTOR */

int DpsDocParseContent(DPS_AGENT * Indexer, DPS_DOCUMENT * Doc) {
	
#ifdef WITH_PARSER
	DPS_PARSER	*Parser;
#endif
	const int       status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
	const char	*real_content_type=NULL;
	const char	*url=DpsVarListFindStr(&Doc->Sections,"URL","");
	const char	*ct=DpsVarListFindStr(&Doc->Sections,"Content-Type","");
	const char	*ce=DpsVarListFindStr(&Doc->Sections,"Content-Encoding","");
	const char	*te=DpsVarListFindStr(&Doc->Sections,"Transfer-Encoding","");
	int             nosections = 1;
	int		result = DPS_OK;
	size_t          i, r;

	if(!strcmp(DPS_NULL2EMPTY(Doc->CurURL.filename), "robots.txt")) return DPS_OK;

	if(Doc->method != DPS_METHOD_HEAD) {
	
	  if(!strcasecmp(te, "chunked")) {
		DPS_THREADINFO(Indexer, "Unchunk", url);
		if (status == 206) {
		  DpsLog(Indexer, DPS_LOG_INFO, "Partial content, can't unchunk it.");
		  return result;
		}
		DpsUnchunk(Indexer, Doc, ce);
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", 
				     Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size + DpsVarListFindInt(&Doc->Sections, "Content-Length", 0));
	  }
#ifdef HAVE_ZLIB
	  if((!strcasecmp(ce, "gzip")) || (!strcasecmp(ce, "x-gzip"))) {
		DPS_THREADINFO(Indexer,"UnGzip",url);
		if (status == 206) {
		  DpsLog(Indexer, DPS_LOG_INFO, "Partial content, can't ungzip it.");
		  return result;
		}
		DpsUnGzip(Indexer, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", 
				     Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size + DpsVarListFindInt(&Doc->Sections, "Content-Length", 0));
	  }else
	  if(!strcasecmp(ce,"deflate")){
		DPS_THREADINFO(Indexer,"Inflate",url);
		if (status == 206) {
		  DpsLog(Indexer, DPS_LOG_INFO, "Partial content, can't inflate it.");
		  return result;
		}
		DpsInflate(Indexer, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", 
				     Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size + DpsVarListFindInt(&Doc->Sections, "Content-Length", 0));
	  }else
	    if((!strcasecmp(ce, "compress")) || (!strcasecmp(ce, "x-compress"))) {
		DPS_THREADINFO(Indexer,"Uncompress",url);
		if (status == 206) {
		  DpsLog(Indexer, DPS_LOG_INFO, "Partial content, can't uncomress it.");
		  return result;
		}
		DpsUncompress(Indexer, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", 
				     Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size + DpsVarListFindInt(&Doc->Sections, "Content-Length", 0));
	  }else
#endif	
	      if((!strcasecmp(ce,"identity")) || (!strcasecmp(ce,""))) {
		/* Nothing to do*/
	  }else{
		DpsLog(Indexer,DPS_LOG_ERROR,"Unsupported Content-Encoding");
/*		DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);*/
	  }

	  DpsMirrorPUT(Indexer, Doc, &Doc->CurURL, "before");
	
#ifdef HAVE_LIBEXTRACTOR
	  if (strncasecmp(ct, "text/", 5) != 0) {

#if EXTRACTOR_VERSION < 0x00060000
	    DPS_TEXTITEM  Item;
	    DPS_VAR       *Sec;
	    DPS_VAR       *BSec = DpsVarListFind(&Doc->Sections, "body");
	    EXTRACTOR_ExtractorList *plugins;
	    EXTRACTOR_KeywordList   *md_list, *pmd;

	    DpsLog(Indexer, DPS_LOG_EXTRA, "Executing Libextractor parser");

	    DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
	     plugins = EXTRACTOR_loadDefaultLibraries();
/*	     md_list = EXTRACTOR_getKeywords2(plugins, Doc->Buf.content, Doc->Buf.size - (Doc->Buf.content - Doc->Buf.buf));*/
	     md_list = EXTRACTOR_getKeywords2(plugins, Doc->Buf.buf, Doc->Buf.size);

	     bzero((void*)&Item, sizeof(Item));
	     Item.href = NULL;
	     for (pmd = md_list; pmd != NULL; pmd = pmd->next) {
	      const char *secname = DpsLibextractorMsgName(pmd->keywordType);
	      nosections = 0;
	      DpsLog(Indexer, DPS_LOG_DEBUG, "Libextracted %s: %s", secname, pmd->keyword);
	      Sec = DpsVarListFind(&Doc->Sections, secname);
	      if (Sec != NULL || BSec != NULL) {
		Item.section = (Sec != NULL) ? Sec->section : BSec->section;
		Item.strict = (Sec != NULL) ? Sec->strict : BSec->strict;
		Item.str = pmd->keyword;
		Item.section_name = (Sec) ? Sec->name : BSec->name;
		Item.len = dps_strlen(Item.str);
		(void)DpsTextListAdd(&Doc->ExtractorList, &Item);
	      }
	     }

	     EXTRACTOR_freeKeywords(md_list);
	     EXTRACTOR_removeAll(plugins);
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
#else /* For versions 0.6.x */
	    struct EXTRACTOR_PluginList *plugins = EXTRACTOR_plugin_add_defaults(EXTRACTOR_OPTION_DEFAULT_POLICY);
	    DPS_CLS CLS;

	    DpsLog(Indexer, DPS_LOG_EXTRA, "Executing Libextractor parser");

	    CLS.Indexer = Indexer;
	    CLS.Doc = Doc;
	    EXTRACTOR_extract(plugins, NULL, Doc->Buf.buf, Doc->Buf.size, &Dps_MetaDataProcessor, &CLS);
	    EXTRACTOR_plugin_remove_all(plugins);
#endif
	  }
#endif

	  if(!real_content_type) real_content_type = ct;

#ifdef WITH_PARSER
	  i = 0;
	  do {
	  /* Let's try to start external parser for this Content-Type */

	    if((Parser = DpsParserFind(&Indexer->Conf->Parsers, ct))) {
		DpsLog(Indexer,DPS_LOG_DEBUG,"Found external parser '%s' -> '%s'",
			Parser->from_mime?Parser->from_mime:"NULL",
			Parser->to_mime?Parser->to_mime:"NULL");
	    }
	    if(Parser) {
	      if (status == DPS_HTTP_STATUS_OK) {
		if (DpsParserExec(Indexer, Parser, Doc)) {
		  char *to_charset;
		  ct = real_content_type = Parser->to_mime?Parser->to_mime:"unknown";
		  DpsLog(Indexer,DPS_LOG_DEBUG,"Parser-Content-Type: %s",real_content_type);
		  if((to_charset=strstr(real_content_type,"charset="))){
		        const char *cs = DpsCharsetCanonicalName(DpsTrim(to_charset + 8, " \t;\"'"));
			DpsVarListReplaceStr(&Doc->Sections, "Server-Charset", cs);
			DpsLog(Indexer,DPS_LOG_DEBUG, "to_charset='%s'", cs);
		  }
#ifdef DEBUG_PARSER
		  fprintf(stderr,"content='%s'\n",Doc->content);
#endif
		}
	      } else {
		DpsLog(Indexer, DPS_LOG_WARN, "Parser is not executed, document status: %d", status);
		break;
		/*return result;*/
	      }
	    }
	  } while ((Parser != NULL) && (++i < 1000));
#endif
	
	  DpsVarListAddStr(&Doc->Sections,"Parser-Content-Type",real_content_type);

	  /* CRC32 without headers */
	  if (Doc->Buf.content != NULL) {
	      size_t crclen = Doc->Buf.size - (size_t)(Doc->Buf.content-Doc->Buf.buf);
	    DpsVarListReplaceInt(&Doc->Sections, "crc32", (int)DpsHash32(Doc->Buf.content, crclen));
	  } else {
	    DpsVarListDel(&Doc->Sections, "crc32");
	  }

	  if (Indexer->Conf->BodyPatterns.nmatches != 0) {
	    char            *buf;
	    DPS_MATCH       *Alias;
	    DPS_MATCH_PART  Parts[10];
	    size_t nparts = 10;
	    size_t buf_len, pattern_len = 0, new_buf_len;;

	    buf = (char*)DpsMalloc(buf_len = (Doc->Buf.size + 1024));
	    if (buf == NULL) return DPS_OK;
	    DPS_FREE(Doc->Buf.pattern);

	    for (i = 0; i < Indexer->Conf->BodyPatterns.nmatches; i++) {
	      Alias = &Indexer->Conf->BodyPatterns.Match[i];

	      if (Alias->match_type == DPS_MATCH_REGEX) {
	      
		DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
		if (DpsMatchExec(Alias, Doc->Buf.content, Doc->Buf.content, NULL, nparts, Parts)) {
		  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		  continue;
		}
		DpsMatchApply(buf, buf_len - 1, Doc->Buf.content, Alias->arg, Alias, nparts, Parts);
		DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		new_buf_len = dps_strlen(buf);
		if ((Doc->Buf.pattern = DpsRealloc(Doc->Buf.pattern, pattern_len + new_buf_len + 1 )) == NULL) {
		  DPS_FREE(buf);
		  return DPS_OK;
		}
		dps_strncpy(Doc->Buf.pattern, buf, new_buf_len + 1);
		pattern_len += new_buf_len;
		DpsLog(Indexer, DPS_LOG_DEBUG, "%dth, BodyPattern applied", i);
		/*break;*/ /* We do not break anymore to apply all possible patterns and body brackets */
	      } else if (Alias->match_type == DPS_MATCH_SUBSTR) {
		char *second, *first = strstr(Doc->Buf.content, Alias->pattern);
		size_t flen;
		while (first != NULL) {
		  second = strstr(first, Alias->arg);
		  if (second != NULL) {
		    flen = dps_strlen(Alias->pattern);
		    Doc->Buf.pattern = DpsRealloc(Doc->Buf.pattern, pattern_len + (size_t)(second - first) - flen + 1);
		    if (Doc->Buf.pattern == NULL) break;
		    dps_strncpy(Doc->Buf.pattern + pattern_len, first + flen, (size_t)(second - first - flen));
		    pattern_len += (size_t)(second - first - flen);
		    /*Doc->Buf.pattern = DpsStrndup(first + flen, (size_t)(second - first - flen));*/
		    DpsLog(Indexer, DPS_LOG_DEBUG, "%dth, BodyBrackets applied", i);
		  }
		  first = strstr(second, Alias->pattern);
		}
		if (Doc->Buf.pattern) Doc->Buf.pattern[pattern_len] = '\0';
		/*break;*/ /* We do not break anymore to apply all passible pattern and body brackets */
	      }
	    }
	    DPS_FREE(buf);
	  }

	  for (r = 0; r < 256; r++) {
	    for(i = 0; i < Indexer->Conf->Vars.Root[r].nvars; i++) {
		DPS_VAR *Sec = &Indexer->Conf->Vars.Root[r].Var[i];
		if (Sec->section) {
		  DPS_VAR *Dsec = DpsVarListFind(&Doc->Sections, Sec->name);
		  if (Dsec && Dsec->val) {
		        DPS_TEXTITEM	Item;
			bzero((void*)&Item, sizeof(Item));
			Item.section = Sec->section;
			Item.strict = Sec->strict;
			Item.str = Dsec->val;
			Item.section_name = Sec->name;
			Item.len = 0;
			(void)DpsTextListAdd(&Doc->TextList, &Item);
		  }
		}
	    }
	  }

#ifdef WITH_MP3
	  if(Doc->method == DPS_METHOD_CHECKMP3ONLY ||
	     !strncasecmp(real_content_type, "audio/mpeg",10) || !strncasecmp(real_content_type, "audio/x-mpeg",12)) {
			DpsMP3Parse(Indexer,Doc);
			nosections = 0;
	  } else
#endif
	  if(!strncasecmp(real_content_type, "text/plain", 10)
	     || !strncasecmp(real_content_type, "text/tab-separated-values", 25)
	     || !strncasecmp(real_content_type, "text/css", 8)
	     ){
			DpsParseText(Indexer, Doc);
			DpsParseSections(Indexer, Doc);
			nosections = 0;
	  }else	
	  if(!strncasecmp(real_content_type,"text/html",9)
	     || !strncasecmp(real_content_type, "application/xhtml+xml", 21)
	     || !strncasecmp(real_content_type, "text/vnd.wap.wml", 16)
	     ){
			DpsMirrorPUT(Indexer, Doc, &Doc->CurURL, "bf_html");
			DpsHTMLParse(Indexer,Doc);
			DpsParseSections(Indexer, Doc);
			nosections = 0;
	  }else
	  if(!strncasecmp(real_content_type, "text/xml", 8) 
	     || !strncasecmp(real_content_type, "application/xml", 15)
	     || !strncasecmp(real_content_type, "application/x.xml", 17)
	     || !strncasecmp(real_content_type, "application/atom+xml", 20)
	     || !strncasecmp(real_content_type, "application/x.atom+xml", 22)
	     || !strncasecmp(real_content_type, "application/x.rss+xml", 21)
	     || !strncasecmp(real_content_type, "application/rss+xml", 19) ) {
			DpsXMLParse(Indexer, Doc);
			DpsParseSections(Indexer, Doc);
			nosections = 0;
	  }else
	  if(!strncasecmp(real_content_type, "image/gif", 9)) {
			DpsGIFParse(Indexer, Doc);
			nosections = 0;
	  }
	  if (nosections && status != DPS_HTTP_STATUS_MOVED_PARMANENTLY && status != DPS_HTTP_STATUS_MOVED_TEMPORARILY) {

			/* Unknown Content-Type  */
			DpsLog(Indexer,DPS_LOG_ERROR,"Unsupported Content-Type '%s'",real_content_type);
			DpsVarListReplaceInt(&Doc->Sections,"Status",DPS_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);
	  }
	}

	DpsMirrorPUT(Indexer, Doc, &Doc->CurURL, "after");

	/* Guesser stuff */	
	DpsGuessCharSet(Indexer, Doc, &Indexer->Conf->LangMaps);
				
	DpsLog(Indexer, DPS_LOG_EXTRA, "Guesser bytes: %d, Lang: %s, Charset: %s", Indexer->Flags.GuesserBytes,
	       DpsVarListFindStr(&Doc->Sections,"Content-Language",""),
	       DpsVarListFindStr(&Doc->Sections,"Charset",""));
	/* /Guesser stuff */	

#ifdef HAVE_ZLIB
	if ((Doc->method != DPS_METHOD_HEAD) && (DpsVarListFindInt(&Doc->Sections, "Content-Length", 0) != 0) && (strncmp(real_content_type, "text/", 5) == 0)) {
	  char reason[PATH_MAX+1];
	  int m;
	  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	  m =  DpsStoreFilterFind(DPS_LOG_DEBUG, &Indexer->Conf->StoreFilters, Doc, reason);
	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  DpsLog(Indexer, DPS_LOG_DEBUG, "%s", reason);
	  if (m == DPS_METHOD_STORE && !(Indexer->flags & DPS_FLAG_FROM_STORED)) DpsStoreDoc(Indexer, Doc, DpsVarListFindStr(&Doc->Sections, "ORIG_URL", NULL));
	}
	
#endif
	{
	const char      *vary = DpsVarListFindStr(&Doc->Sections, "Vary", NULL);
	const int       parent = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
	char savec;

	if ((vary != NULL) && (Doc->fetched == 0)) {
	  if (strcasestr(vary, "accept-language") != NULL) {
	    DPS_HREF Href;
	    DPS_URL *newURL = DpsURLInit(NULL);
	    char *curl; const char *ourl;
	    const char *VaryLang = DpsVarListFindStr(&Doc->Sections, "VaryLang", "en"), *CL;
	    const char *ContentLanguage = DpsVarListFindStr(&Doc->Sections, "Content-Language", "en");
	    const size_t hops = (size_t)DpsVarListFindInt(&Doc->Sections, "Hops", 0);
	    char *tok, *lt;
	    size_t urlen;
	    size_t cl_len = dps_strlen(ContentLanguage);

	    if (newURL == NULL) return DPS_ERROR;
	    DpsHrefInit(&Href);
	    Href.referrer = parent;
	    Href.hops = hops;
	    Href.site_id = 0;
	    Href.method = DPS_METHOD_GET;
	    Href.charset_id = Doc->charset_id;
	    Href.weight = 0.5;
	    DpsURLParse(newURL, ourl = DpsVarListFindStr(&Doc->Sections, "URL", ""));
	    if ((status < 400) && (strcmp(DPS_NULL2EMPTY(newURL->filename), "robots.txt") != 0)) {
	      CL = DpsVarListFindStr(&Doc->Sections, "Content-Location", DPS_NULL2EMPTY(newURL->filename));
	      urlen = 128 + dps_strlen(DPS_NULL2EMPTY(newURL->hostinfo)) + dps_strlen(DPS_NULL2EMPTY(newURL->path)) + dps_strlen(CL);
	      if ((curl = (char*)DpsMalloc(urlen)) != NULL) {
		dps_snprintf(curl, urlen, "%s://%s%s%s", DPS_NULL2EMPTY(newURL->schema), DPS_NULL2EMPTY(newURL->hostinfo), 
			     DPS_NULL2EMPTY(newURL->path), CL );
		Href.url = curl;
		DpsHrefListAdd(Indexer, &Indexer->Hrefs, &Href);
		if (Doc->subdoc < Indexer->Flags.SubDocLevel) {
		  tok = dps_strtok_r((char*)VaryLang, " ,\t", &lt, &savec);
		  while (tok != NULL) {
		    size_t min_len = dps_min(cl_len, dps_strlen(tok));
		    if (0 != strncasecmp(ContentLanguage, tok, min_len))
		      DpsIndexSubDoc(Indexer, Doc, NULL, tok, ourl);
		    tok = dps_strtok_r(NULL, " ,\t", &lt, &savec);
		  }
		}
		DPS_FREE(curl);
	      }
	    }
	    DpsURLFree(newURL);
	  }
	}
	}
	/* converting stuff */
	{
	  const char	*doccset;
	  char          *loc_str;
	  DPS_CHARSET	*doccs, *loccs;
	  DPS_CONV	dc_lc;
	  DPS_TEXTLIST	*tlist = &Doc->TextList;
	  DPS_TEXTITEM  *Item;
	  size_t z, dst_len;
	  doccset=DpsVarListFindStr(&Doc->Sections,"Charset",NULL);
	  if(!doccset||!*doccset)doccset=DpsVarListFindStr(&Doc->Sections,"RemoteCharset","iso-8859-1");
	  doccs=DpsGetCharSet(doccset);
	  if(!doccs)doccs=DpsGetCharSet("iso-8859-1");
	  loccs = Doc->lcs;
	  if (!loccs) loccs = Indexer->Conf->lcs;
	  if (!loccs) loccs = DpsGetCharSet("iso-8859-1");
	  if (loccs != doccs) {
	    DpsConvInit(&dc_lc, doccs, loccs, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
	    for(z = 0; z < tlist->nitems; z++) {
	      Item = &tlist->Items[z];
	      if ((loc_str = DpsMalloc((dst_len = 24 * Item->len + 1) * sizeof(char))) == NULL) {
		DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", dst_len, __FILE__, __LINE__);
		result = DPS_ERROR;
		break;
	      }
	      DpsConv(&dc_lc, loc_str, dst_len, Item->str, Item->len);
	      DPS_FREE(Item->str);
	      if ((Item->str = DpsRealloc(loc_str, (Item->len = dc_lc.obytes) + 1)) == NULL) {
		DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc %d bytes at %s:%d", dst_len, __FILE__, __LINE__);
		result = DPS_ERROR;
		break;
	      }
	    }
	  }
	}
	if (result == DPS_OK) result = DpsSQLSections(Indexer, Doc);

	return result;
}



static int DpsNextTarget(DPS_AGENT * Indexer, DPS_DOCUMENT *Result) {
	int	result=DPS_NOTARGET;
	int     u;

	DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
	Indexer->now = time(NULL);
	u = (Indexer->Conf->url_number <= 0) || (Indexer->Conf->url_size < 0);

	if (u) {
	        DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
		return DPS_NOTARGET;
	}
	
	/* Load targets into memory cache */
	u = (Indexer->Conf->Targets.cur_row >= Indexer->Conf->Targets.num_rows);
	if (u) {
	        result = (Indexer->action == DPS_OK) ? DpsTargets(Indexer) : Indexer->action;
		if(result!=DPS_OK) {
		  DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
		  return result;
		}
	}
	
	/* Choose next target */
	if(Indexer->Conf->Targets.num_rows && (Indexer->Conf->Targets.cur_row < Indexer->Conf->Targets.num_rows ) ) {
		DPS_DOCUMENT *Doc=&Indexer->Conf->Targets.Doc[Indexer->Conf->Targets.cur_row];

		if (Doc != NULL) {
		  DpsVarListReplaceLst(&Result->Sections, &Indexer->Conf->Sections, NULL, "*");
		  DpsVarListReplaceLst(&Result->Sections, &Doc->Sections, NULL, "*");
		  DpsVarListReplaceLst(&Result->RequestHeaders, &Doc->RequestHeaders, NULL, "*");
		  Result->charset_id = Doc->charset_id;
		  
		  Indexer->Conf->Targets.cur_row++;
		  Indexer->Conf->url_number--;
		  DpsDocFree(Doc);
		  DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
/*		  DpsVarListLog(Indexer, &Result->Sections, DPS_LOG_DEBUG, "Target");*/
		  return DPS_OK;
		}
	}
	
	Indexer->Conf->url_number = -1;
	DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
	return DPS_NOTARGET;
}

int DpsVarList2Doc(DPS_DOCUMENT *Doc, DPS_SERVER *Server) {
  DPS_SPIDERPARAM *S = &Doc->Spider;
  DPS_VARLIST *V = &Server->Vars;
  const char *value;
/*  char str[64];
  size_t i;
  time_t default_period = (time_t)DpsVarListFindUnsigned(V, "Period", DPS_DEFAULT_REINDEX_TIME);*/

	S->maxhops		= DpsVarListFindInt(V, "MaxHops",	DPS_DEFAULT_MAX_HOPS);
	S->follow		= DpsVarListFindInt(V, "Follow",		DPS_FOLLOW_PATH);
	S->max_net_errors	= DpsVarListFindInt(V, "MaxNetErrors",	DPS_MAXNETERRORS);
	S->net_error_delay_time	= DpsVarListFindInt(V, "NetErrorDelayTime",DPS_DEFAULT_NET_ERROR_DELAY_TIME);
	Doc->connp.timeout = S->read_timeout = DpsVarListFindUnsigned(V, "ReadTimeOut",	DPS_READ_TIMEOUT);
	S->doc_timeout		= DpsVarListFindUnsigned(V, "DocTimeOut",	DPS_DOC_TIMEOUT);
	S->index		= DpsVarListFindInt(V, "Index",		1);
	S->use_robots		= Server->use_robots;
	S->use_clones		= DpsVarListFindInt(V, "DetectClones",	1);
	S->use_cookies		= DpsVarListFindInt(V, "Cookies",	0);
	S->Server               = Server;

	value =  DpsVarListFindStr(V, "HoldBadHrefs", NULL);
	if (value != NULL) DpsVarListReplaceStr(&Doc->Sections, "HoldBadHrefs", value);

	DpsVarListReplaceInt(&Doc->Sections, "Follow", S->follow);
	DpsVarListReplaceInt(&Doc->Sections, "Index", S->index);

	value = DpsVarListFindStr(V, "Category", NULL);
	if (value != NULL) DpsVarListReplaceStr(&Doc->Sections, "Category", value);

	value = DpsVarListFindStr(V, "Tag", NULL);
	if (value != NULL) DpsVarListReplaceStr(&Doc->Sections, "Tag", value);

/*
	for (i = 0; i < DPS_DEFAULT_MAX_HOPS; i++) {
	  dps_snprintf(str, sizeof(str), "Period%u", i);
	  S->period[i] = (time_t)DpsVarListFindUnsigned(V, str, default_period);
	}
*/
	return DPS_OK;
}

__C_LINK int __DPSCALL DpsIndexSubDoc(DPS_AGENT *Indexer, DPS_DOCUMENT *Parent, const char *base, const char *lang, const char *url) {
	DPS_DOCUMENT	*Doc;
	DPS_URL		*newURL, *baseURL = NULL;
	const char	*alias = NULL;
	char		*newhref = NULL;
	char		*origurl = NULL, *aliasurl = NULL;
	DPS_SERVER	*Server = NULL;
	int		result = DPS_OK, status = 0, parse_res;
	int             hops = DpsVarListFindInt(&Parent->Sections, "Hops", 0);
	size_t          i;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif

	TRACE_IN(Indexer, "DpsIndexSubDoc");
	DPS_THREADINFO(Indexer,"Starting subdoc","");
	
	if ((Doc = DpsDocInit(NULL)) == NULL) {
	  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return DPS_ERROR;
	}
	Doc->subdoc = Parent->subdoc + 1;
	Doc->Spider = Parent->Spider;
	
	if ((newURL = DpsURLInit(NULL)) == NULL) {
	  DpsDocFree(Doc);
	  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
	  DpsViolationExit(Indexer->handle, paran);
#endif
	  return DPS_ERROR;
	}

	if((parse_res=DpsURLParse(newURL, url))) {
		Doc->method = DPS_METHOD_DISALLOW;
		switch(parse_res){
			case DPS_URL_LONG:
				DpsLog(Indexer, DPS_LOG_DEBUG, "URL too long: '%s'", url);
				break;
			case DPS_URL_BAD:
			default:
				DpsLog(Indexer, DPS_LOG_DEBUG, "Error in URL: '%s'", url);
		}
	}
	if (base != NULL) {
	  if ((baseURL = DpsURLInit(NULL)) == NULL) {
	    DpsURLFree(newURL);
	    DpsDocFree(Doc);
	    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
	    DpsViolationExit(Indexer->handle, paran);
#endif
	    return DPS_ERROR;
	  }
	  if((parse_res = DpsURLParse(baseURL, base))) {
		Doc->method = DPS_METHOD_DISALLOW;
		switch(parse_res){
			case DPS_URL_LONG:
				DpsLog(Indexer, DPS_LOG_DEBUG, "Base URL too long: '%s'", base);
				break;
			case DPS_URL_BAD:
			default:
				DpsLog(Indexer, DPS_LOG_DEBUG, "Error in base URL: '%s'", base);
		}
	  }
	}

	newURL->charset_id = Parent->charset_id;
	RelLink(Indexer, (base) ? baseURL : &Parent->CurURL, newURL, &newhref, 1);
	DpsVarListReplaceLst(&Doc->Sections, &Parent->Sections, NULL, "*");
	DpsVarListDel(&Doc->Sections, "E_URL");
	DpsVarListDel(&Doc->Sections, "URL_ID");
	DpsVarListDel(&Doc->Sections, "DP_ID");
	DpsVarListDel(&Doc->Sections, "Content-Language");
	DpsVarListDel(&Doc->Sections, "Meta-Language");
	DpsVarListDel(&Doc->Sections, "Charset");
	DpsVarListDel(&Doc->Sections, "Meta-Charset");
	DpsVarListDel(&Doc->Sections, "Server-Charset");

	DpsVarListReplaceStr(&Doc->Sections, "URL", newhref);
	if (base) DpsVarListReplaceStr(&Doc->Sections, "base.href", base);
	else DpsVarListDel(&Doc->Sections, "base.href");
	DpsVarListReplaceInt(&Doc->Sections, "crc32old", DpsVarListFindInt(&Doc->Sections, "crc32", 0));
	if (lang != NULL) {
	  Doc->fetched = 1;
	  DpsVarListReplaceStr(&Doc->RequestHeaders, "Accept-Language", lang);
	  DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang);
	}
	DpsLog(Indexer, (Indexer->Flags.cmd != DPS_IND_POPRANK) ? DPS_LOG_INFO : DPS_LOG_DEBUG, "[%s] Subdoc level:%d, URL: %s", 
	       DPS_NULL2EMPTY(lang), Doc->subdoc, newhref);
	
	/* To see the URL being indexed in "ps" output on */
	if (DpsNeedLog(DPS_LOG_EXTRA) && Indexer->Flags.cmd != DPS_IND_POPRANK) dps_setproctitle("[%d] Subdoc:%s", Indexer->handle, newhref);
	
	/* Collect information from Conf */
	
	if(Indexer->Flags.cmd != DPS_IND_POPRANK && !Doc->Buf.buf) {
		/* Alloc buffer for document */
		Doc->Buf.max_size = (size_t)DpsVarListFindInt(&Indexer->Vars, "MaxDocSize", DPS_MAXDOCSIZE);
		Doc->Buf.allocated_size = DPS_NET_BUF_SIZE;
		if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		  DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory (%d bytes) %s:%d", Doc->Buf.allocated_size + 1, __FILE__, __LINE__);
		  DpsDocFree(Doc);
		  if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
		  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		  DpsViolationExit(Indexer->handle, paran);
#endif
		  return DPS_ERROR;
		}
		Doc->Buf.buf[0]='\0';
	}
	
	/* Check that URL has valid syntax */
	if(DpsURLParse(&Doc->CurURL, newhref)) {
		DpsLog(Indexer,DPS_LOG_WARN,"Invalid URL: %s", newhref);
		Doc->method = DPS_METHOD_DISALLOW;
	}else
	if ((Doc->CurURL.filename != NULL) && (!strcmp(Doc->CurURL.filename, "robots.txt"))) {
		Doc->method = DPS_METHOD_DISALLOW;
	}else if (Indexer->Flags.cmd != DPS_IND_POPRANK) {
		char		*alstr = NULL;

		Doc->CurURL.charset_id = Doc->charset_id;
		/* Find correspondent Server */
		DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
		Server = DpsServerFind(Indexer, (urlid_t)DpsVarListFindInt(&Doc->Sections, "Server_id", 0), newhref, Doc->charset_id, &alstr);
		DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);

		if ( !Server ) Server = Parent->Server;
		if ( !Server ) {
			DpsLog(Indexer, DPS_LOG_INFO, "No 'Server' command for url");
			Doc->method = DPS_METHOD_DISALLOW;
		} else {
		        DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
			Doc->lcs = Indexer->Conf->lcs;
			DpsVarList2Doc(Doc, Server);
			Doc->Spider.ExpireAt = Server->ExpireAt;
			Doc->Server = Server;
			
			DpsDocAddConfExtraHeaders(Indexer->Conf, Doc);
			DpsDocAddServExtraHeaders(Server, Doc);
			DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);

			DpsVarListReplaceLst(&Doc->Sections, &Server->Vars,NULL,"*");
/*			DpsVarListReplaceInt(&Doc->Sections, "Site_id", DpsServerGetSiteId(Indexer, Server, Doc));*/
			DpsVarListReplaceInt(&Doc->Sections, "Server_id", Server->site_id);
			DpsVarListReplaceInt(&Doc->Sections, "MaxHops", Doc->Spider.maxhops);
			
			if(alstr != NULL) {
				/* Server Primary alias found */
				DpsVarListReplaceStr(&Doc->Sections, "Alias", alstr);
			}else{
				/* Apply non-primary alias */
				result = DpsDocAlias(Indexer, Doc);
			}

			if((alias = DpsVarListFindStr(&Doc->Sections, "Alias", NULL))) {
			  const char *u = DpsVarListFindStr(&Doc->Sections, "URL", NULL);
			  origurl = (char*)DpsStrdup(u);
			  aliasurl = (char*)DpsStrdup(alias);
			  DpsLog(Indexer,DPS_LOG_EXTRA,"Alias: '%s'", alias);
			  DpsVarListReplaceStr(&Doc->Sections, "URL", alias);
			  DpsVarListReplaceStr(&Doc->Sections, "ORIG_URL", origurl);
			  DpsVarListDel(&Doc->Sections, "E_URL");
			  DpsVarListDel(&Doc->Sections, "URL_ID");
			  DpsURLParse(&Doc->CurURL, alias);
			}

			/* Check hops, network errors, filters */
			result = DpsDocCheck(Indexer, Server, Doc);
			switch(Doc->method) {
			case DPS_METHOD_GET:
			case DPS_METHOD_HEAD:
			case DPS_METHOD_HREFONLY:
			case DPS_METHOD_CHECKMP3:
			case DPS_METHOD_CHECKMP3ONLY:
			  if( (Server->MaxDocsPerServer != (dps_uint4)-1) && (Server->ndocs > Server->MaxDocsPerServer) ) {
			    DpsLog(Indexer, DPS_LOG_WARN, "Maximum of %u documents per server reached, skip it.", Server->MaxDocsPerServer);
			    Doc->method = DPS_METHOD_VISITLATER;
			  }
			default:
			  break;
			}
			
		}
		DPS_FREE(alstr);
	}
/*	DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);*/
	
	
	if(result!=DPS_OK){
	        DpsDocFree(Doc); DPS_FREE(origurl); DPS_FREE(aliasurl);
		if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
		TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		DpsViolationExit(Indexer->handle, paran);
#endif
		return result;
	}
	
	if(Doc->method != DPS_METHOD_DISALLOW && Doc->method != DPS_METHOD_VISITLATER && Indexer->Flags.cmd != DPS_IND_POPRANK) {
	  DpsDocAddDocExtraHeaders(Indexer, Doc);
	  if(!strncmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "http", 4)) {
	    if(!Doc->Spider.use_robots){
	      DpsLog(Indexer,DPS_LOG_DEBUG, "robots.txt support is disallowed for '%s'", DPS_NULL2EMPTY(Doc->CurURL.hostinfo));
/*	      DPS_GETLOCK(Indexer,DPS_LOCK_CONF);*/
	      result = DpsRobotParse(Indexer, NULL, NULL, DPS_NULL2EMPTY(Doc->CurURL.hostinfo), hops + 1);
/*	      DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);*/
	    }else{
	      DPS_ROBOT_RULE	*rule;

	      /* Check whether URL is disallowed by robots.txt */
	      rule = DpsRobotRuleFind(Indexer, Server, Doc, &Doc->CurURL, 1, (alias) ? 1 : 0);
	      if(rule) {
		char *w;
		switch(rule->cmd) {
		case DPS_METHOD_DISALLOW:
		case DPS_METHOD_VISITLATER:
		  w = "Disallow"; break;
		case DPS_METHOD_CRAWLDELAY:
		  w = "Postpone"; break;
		default:
		  w = "Allow";
		}
		DpsLog(Indexer,(rule->cmd==DPS_METHOD_DISALLOW||rule->cmd==DPS_METHOD_VISITLATER) ? DPS_LOG_INFO : DPS_LOG_EXTRA, "SubDoc.robots.txt: '%s %s'", w, rule->path);
		if((rule->cmd == DPS_METHOD_DISALLOW) || (rule->cmd == DPS_METHOD_VISITLATER) || (rule->cmd == DPS_METHOD_CRAWLDELAY) )
		  Doc->method = rule->cmd;
	      }
	    }
	  }
	  if(origurl != NULL){
	    DpsVarListReplaceStr(&Doc->Sections,"URL",origurl);
	    DpsVarListDel(&Doc->Sections, "E_URL");
	    DpsVarListDel(&Doc->Sections, "URL_ID");
	    DpsURLParse(&Doc->CurURL,origurl);
	  }
	}
	
	if(result!=DPS_OK){
	        DPS_FREE(origurl); DPS_FREE(aliasurl);
		DpsDocFree(Doc);
		if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
		TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		DpsViolationExit(Indexer->handle, paran);
#endif
		return result;
	}

	if (Indexer->Flags.cmd == DPS_IND_POPRANK) {
	  if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_PASNEO))) {
	    DpsDocFree(Doc); DPS_FREE(origurl); DPS_FREE(aliasurl);
	    if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
	    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
	    DpsViolationExit(Indexer->handle, paran);
#endif
	    return result;
	  }
	}else if(Doc->method != DPS_METHOD_DISALLOW && Doc->method != DPS_METHOD_VISITLATER && Doc->method != DPS_METHOD_CRAWLDELAY) {
		int	start,state;
		int	mp3type=DPS_MP3_UNKNOWN;
		
		if(!(Indexer->flags&DPS_FLAG_REINDEX)){
			const char *l = DpsVarListFindStr(&Doc->Sections, "Last-Modified", NULL);
			int prevstatus = DpsVarListFindInt(&Doc->Sections, "PrevStatus", 0);
			if ((prevstatus < 400) && (l != NULL)) DpsVarListReplaceStr(&Doc->RequestHeaders, "If-Modified-Since", l);
		}
		
		DPS_THREADINFO(Indexer, "Getting", newhref);
		
		start = (Doc->method == DPS_METHOD_CHECKMP3 || Doc->method == DPS_METHOD_CHECKMP3ONLY) ? 1 : 0;
		
		for(state=start;state>=0;state--){
			const char	*hdr=NULL;
			
			if(state==1) hdr = "bytes=0-2048";
			if(mp3type==DPS_MP3_TAG)hdr="bytes=-128";
			
			DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_UNKNOWN);
			
			if(aliasurl != NULL) {
			  DpsVarListReplaceStr(&Doc->Sections,"URL",alias);
			  DpsVarListDel(&Doc->Sections, "E_URL");
			  DpsVarListDel(&Doc->Sections, "URL_ID");
			  DpsURLParse(&Doc->CurURL, alias);
			}
				
			DpsVarListLog(Indexer,&Doc->RequestHeaders, DPS_LOG_DEBUG, "SubDoc.Request");
				
			if(hdr) {
			        DpsVarListAddStr(&Doc->RequestHeaders, "Range", hdr);
				DpsLog(Indexer, DPS_LOG_INFO, "Range: [%s]", hdr);
			}

			Indexer->flags |= DPS_FLAG_FROM_STORED | DPS_FLAG_REINDEX;
			result = DpsGetURL(Indexer, Doc, origurl);
			Indexer->flags = Indexer->Conf->flags;

			if(hdr) {
			        DpsVarListDel(&Doc->RequestHeaders, "Range");
			}
				
			if(origurl != NULL) {
			  DpsVarListReplaceStr(&Doc->Sections, "URL", origurl);
			  DpsVarListDel(&Doc->Sections, "E_URL");
			  DpsVarListDel(&Doc->Sections, "URL_ID");
			  DpsURLParse(&Doc->CurURL,origurl);
			}
			
			if(result!=DPS_OK){
			        DPS_FREE(origurl); DPS_FREE(aliasurl);
				DpsDocFree(Doc);
				if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
				TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
				DpsViolationExit(Indexer->handle, paran);
#endif
				return result;
			}
			
			DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
			if (Server) Server->ndocs++;
			DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);

/*			DpsParseHTTPResponse(Indexer, Doc);*/
			DpsDocProcessResponseHeaders(Indexer, Doc);
/*			DpsVarListLog(Indexer, &Doc->Sections, DPS_LOG_DEBUG, "Response");*/
			
			status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
			
			DpsLog(Indexer, DPS_LOG_EXTRA, "Status: %d %s", status, DpsHTTPErrMsg(status));

			if(status != DPS_HTTP_STATUS_PARTIAL_OK && status != DPS_HTTP_STATUS_OK)
				break;
			
			if(state==1){	/* Needs guessing */
				if(DPS_MP3_UNKNOWN != (mp3type = DpsMP3Type(Doc))) {
					DpsVarListReplaceStr(&Doc->Sections, "Content-Type", "audio/mpeg");
					if(Doc->method == DPS_METHOD_CHECKMP3ONLY && mp3type != DPS_MP3_TAG) break;
				}
				if(Doc->method == DPS_METHOD_CHECKMP3ONLY) break;
			}
		}
		
		/* Add URL from Location: header */
		/* This is to give a chance for  */
		/* a concurent thread to take it */
		result = DpsDocStoreHrefs(Indexer, Doc);
		if(result!=DPS_OK){
		        DPS_FREE(origurl); DPS_FREE(aliasurl);
			DpsDocFree(Doc);
			if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
			TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
			DpsViolationExit(Indexer->handle, paran);
#endif
			return result;
		}
		
		if((!Doc->Buf.content) && (status < 500)) {
			DpsLog(Indexer, DPS_LOG_WARN, "No data received");
			status=DPS_HTTP_STATUS_SERVICE_UNAVAILABLE;
			DpsVarListReplaceInt(&Doc->Sections, "Status", status);
		}
		
		/* Collect only hrefs from erroneous documents */
		if(status != DPS_HTTP_STATUS_OK && status != DPS_HTTP_STATUS_PARTIAL_OK && status != DPS_HTTP_STATUS_MOVED_TEMPORARILY
		   && !(status == DPS_HTTP_STATUS_MOVED_PARMANENTLY && Doc->subdoc > 1) ) {
		  Doc->method = DPS_METHOD_HREFONLY;
		}
		if (Doc->Buf.content != NULL ) {
		   	size_t		wordnum, min_size;
			size_t	hdr_len = (size_t)(Doc->Buf.content - Doc->Buf.buf);
			size_t	cont_len = Doc->Buf.size - hdr_len;
			const char *cont_lang = NULL;
			int skip_too_small;
			char reason[PATH_MAX+1];
		   	
			min_size = (size_t)DpsVarListFindUnsigned(&Indexer->Vars, "MinDocSize", 0);
			skip_too_small = (cont_len < min_size);

			if (skip_too_small && Doc->method != DPS_METHOD_CHECKMP3ONLY) {
			  Doc->method = DPS_METHOD_HEAD;
			  DpsLog(Indexer, DPS_LOG_EXTRA, "Too small content size (%d < %d), CheckOnly.", cont_len, min_size); 
			}

			DPS_THREADINFO(Indexer,"Parsing", newhref);
			
			result = DpsDocParseContent(Indexer, Doc);
			if(result!=DPS_OK){
			        DPS_FREE(origurl); DPS_FREE(aliasurl);
				DpsDocFree(Doc);
				if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
				TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
				DpsViolationExit(Indexer->handle, paran);
#endif
				return result;
			}
			/* Guesser was here */			
			cont_lang = DpsVarListFindStr(&Doc->Sections,"Content-Language","");
			
			DpsParseURLText(Indexer, Doc);
/*			DpsParseHeaders(Indexer, Doc);*/
			{
			  int m;
			  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
			  m = DpsSectionFilterFind(DPS_LOG_DEBUG,&Indexer->Conf->SectionFilters,Doc,reason);
			  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
			  if (m != DPS_METHOD_NOINDEX) {
			    char *subsection = NULL;
			    DpsLog(Indexer, DPS_LOG_DEBUG, "%s", reason);
			    if (m == DPS_METHOD_INDEX) Doc->method = DPS_METHOD_GET;

			    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
			    switch(DpsSubSectionMatchFind(Indexer, DPS_LOG_DEBUG, &Indexer->Conf->SubSectionMatch, Doc, reason, &subsection)) {
			    case DPS_METHOD_TAG:
			      DpsVarListReplaceStr(&Doc->Sections, "Tag", subsection); break;
			    case DPS_METHOD_CATEGORY:
			      DpsVarListReplaceStr(&Doc->Sections, "Category", subsection); break;
			    }
			    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
			    DPS_FREE(subsection);

			    if (Doc->method != DPS_METHOD_HREFONLY) { DpsPrepareWords(Indexer, Doc); }
			  } else Doc->method = m;
			  DpsLog(Indexer, DPS_LOG_DEBUG, "%s", reason);
			}
			
			/* Remove StopWords */
			/*			DPS_GETLOCK(Indexer,DPS_LOCK_CONF);*/
			/*DpsWordListSort(&Doc->Words);*/
			if (Doc->Words.nwords > 0) {
			  DPS_WORD *p_word = Doc->Words.Word;
			  size_t wlen = p_word->ulen;
			  int rc = (wlen > Indexer->WordParam.max_word_len ||
				    wlen < Indexer->WordParam.min_word_len ||
				    DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? cont_lang : "" ) != NULL);
			  if (rc) p_word->coord = 0;
			  for(wordnum = 1; wordnum < Doc->Words.nwords; wordnum++) {
			    if (DpsWordCmp(p_word, Doc->Words.Word + wordnum) == 0) {
			      if (rc) Doc->Words.Word[wordnum].coord = 0;
			    } else {
			      p_word = Doc->Words.Word + wordnum;
			      wlen = p_word->ulen;
			      rc = (wlen > Indexer->WordParam.max_word_len ||
				    wlen < Indexer->WordParam.min_word_len ||
				    DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? cont_lang : "" ) != NULL);
			      if (rc) p_word->coord = 0;
			    }
			  }
			}

			/*DpsCrossListSort(&Doc->CrossWords);*/
			if (Doc->CrossWords.ncrosswords > 0) {
			  DPS_CROSSWORD *p_word = Doc->CrossWords.CrossWord;
			  size_t wlen = p_word->ulen;
			  int rc = (wlen > Indexer->WordParam.max_word_len ||
				    wlen < Indexer->WordParam.min_word_len ||
				    DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? cont_lang : "" ) != NULL);
			  if (rc) p_word->weight = 0;
			  for(wordnum = 1; wordnum < Doc->CrossWords.ncrosswords; wordnum++) {
			    if (DpsCrossCmp(p_word, Doc->CrossWords.CrossWord + wordnum) == 0) {
			      if (rc) Doc->CrossWords.CrossWord[wordnum].weight = 0;
			    } else {
			      p_word = Doc->CrossWords.CrossWord + wordnum;
			      wlen = p_word->ulen;
			      rc = (wlen > Indexer->WordParam.max_word_len ||
				    wlen < Indexer->WordParam.min_word_len ||
				    DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? cont_lang : "" ) != NULL);
			      if (rc) p_word->weight = 0;
			    }
			  }

			}
			/*			DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);*/
			if (Indexer->Flags.collect_links && (status == 200 || status == 206 || status == 302) )
			  if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_LINKS_MARKTODEL))) {
			    DpsDocFree(Doc); DPS_FREE(origurl); DPS_FREE(aliasurl);
			    if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
			    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
			    DpsViolationExit(Indexer->handle, paran);
#endif
			    return result;
			  }
		}
		DpsVarListLog(Indexer, &Doc->Sections, DPS_LOG_DEBUG, "SubDoc.Response");
	

		if (DPS_OK == (result = DpsDocStoreHrefs(Indexer, Doc))) {
		  if ((DPS_OK == (result = DpsStoreHrefs(Indexer))) && Indexer->Flags.collect_links  
		      && (status == 200 || status == 206 || status == 302) )
		    result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_LINKS_DELETE);
		}

		if(result!=DPS_OK){
		  DPS_FREE(origurl); DPS_FREE(aliasurl);
		  DpsDocFree(Doc);
		  if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
		  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		  DpsViolationExit(Indexer->handle, paran);
#endif
		  return result;
		}

		if(status == DPS_HTTP_STATUS_OK || status == DPS_HTTP_STATUS_PARTIAL_OK) {
		  DPS_CHARSET *parent_cs = DpsGetCharSet("utf-8"), *doc_cs = Indexer->Conf->lcs;
		  DPS_CONV     dc_parent;
		  DPS_TEXTLIST *tlist = &Doc->TextList;
		  DPS_TEXTLIST *extractor_tlist = &Doc->ExtractorList;
		  char *src, *dst = NULL;
		  size_t srclen = (size_t)DpsVarListFindInt(&Doc->Sections, "Content-Length", 0);
		  size_t dstlen = (size_t)DpsVarListFindInt(&Parent->Sections, "Content-Length", 0);

		  if (!doc_cs) doc_cs = DpsGetCharSet("iso-8859-1");

		  DpsVarListReplaceInt(&Parent->Sections, "Content-Length", (int)(srclen + dstlen));
		  DpsConvInit(&dc_parent, doc_cs, parent_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
		  for(i = 0; i < tlist->nitems; i++) {
		    DPS_TEXTITEM *Item = &tlist->Items[i];
		    srclen = ((Item->len) ? Item->len : (dps_strlen(Item->str)) + 1);	/* with '\0' */
		    dstlen = (16 * (srclen + 1)) * sizeof(char);	/* with '\0' */
		    if ((dst = (char*)DpsRealloc(dst, dstlen + 1)) == NULL) {
		      DPS_FREE(origurl); DPS_FREE(aliasurl);
		      DpsDocFree(Doc);
		      if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);
		      TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		      DpsViolationExit(Indexer->handle, paran);
#endif
		      return DPS_ERROR;
		    }
		    src = Item->str;
		    DpsConv(&dc_parent, dst, dstlen, src, srclen);
		    Item->str = dst;
		    Item->len = dc_parent.obytes;
/*	    fprintf(stderr, " -- Section: %s [%d] = %s\n", Item->section_name, Item->section, Item->str);*/
		    (void)DpsTextListAdd(&Parent->ExtractorList, Item);
		    Item->str = src;
		    Item->len = srclen;
		  }
		  DPS_FREE(dst);
		  for(i = 0; i < extractor_tlist->nitems; i++) {
		    DPS_TEXTITEM *Item = &extractor_tlist->Items[i];
		    (void)DpsTextListAdd(&Parent->ExtractorList, Item);
/*	    fprintf(stderr, " -- ExtrSection: %s [%d] = %s\n", Item->section_name, Item->section, Item->str);*/
		  }
		} 
	}
	Parent->sd_cnt += 1 + Doc->sd_cnt;
/*
	for (i = 0; i < Doc->Words.nwords; i++) {
	  DpsWordListAdd(Parent, &Doc->Words.Word[i], DPS_WRDSEC(Doc->Words.Word[i].coord));
	}
	for (i = 0; i < Doc->CrossWords.ncrosswords; i++) {
	  DpsCrossListAdd(Parent, &Doc->CrossWords.CrossWord[i]);
	}
*/	
	DpsDocFree(Doc);
	if (base) DpsURLFree(baseURL); DpsURLFree(newURL); DPS_FREE(newhref);

	DPS_FREE(origurl); DPS_FREE(aliasurl);
	TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
	DpsViolationExit(Indexer->handle, paran);
#endif
	return result;
}



__C_LINK int __DPSCALL DpsIndexNextURL(DPS_AGENT *Indexer){
	int		result=DPS_OK, status = 0;
	DPS_DOCUMENT	*Doc;
	const char	*url, *alias = NULL;
	char		*origurl = NULL, *aliasurl = NULL;
	DPS_SERVER	*Server = NULL;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif

	TRACE_IN(Indexer, "DpsIndexNextURL");
	
	Doc = DpsDocInit(NULL);
	
	DPS_THREADINFO(Indexer,"Selecting","");

	if(DPS_OK==(result=DpsStoreHrefs(Indexer))) {
	        if (Indexer->action != DPS_OK && Indexer->action != DPS_NOTARGET) {
		  DpsDocFree(Doc);
		  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		  DpsViolationExit(Indexer->handle, paran);
#endif
		  return Indexer->action;
		}
		result = DpsNextTarget(Indexer, Doc);
	}
	
	if(result==DPS_NOTARGET){
		DpsDocFree(Doc);	/* To free Doc.connp->connp */
		TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		DpsViolationExit(Indexer->handle, paran);
#endif
		return result;
	}
	
	if(result!=DPS_OK){
		DpsDocFree(Doc);
		TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		DpsViolationExit(Indexer->handle, paran);
#endif
		return result;
	}
	
	url = DpsVarListFindStr(&Doc->Sections, "URL", "");
	DpsVarListReplaceInt(&Doc->Sections, "crc32old", DpsVarListFindInt(&Doc->Sections, "crc32", 0));
	DpsLog(Indexer, (Indexer->Flags.cmd != DPS_IND_POPRANK) ? DPS_LOG_INFO : DPS_LOG_DEBUG, "URL: %s", url);
	
	/* To see the URL being indexed in "ps" output on xBSD */
	if (DpsNeedLog(DPS_LOG_EXTRA) && Indexer->Flags.cmd != DPS_IND_POPRANK) dps_setproctitle("[%d] URL:%s", Indexer->handle, url);
	
	/* Collect information from Conf */
	
	if(Indexer->Flags.cmd == DPS_IND_INDEX && !Doc->Buf.buf) {
		/* Alloc buffer for document */
		Doc->Buf.max_size = (size_t)DpsVarListFindInt(&Indexer->Vars, "MaxDocSize", DPS_MAXDOCSIZE);
		Doc->Buf.allocated_size = DPS_NET_BUF_SIZE;
		if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
		  DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory (%d bytes) %s:%d", Doc->Buf.allocated_size + 1, __FILE__, __LINE__);
		  DpsDocFree(Doc);
		  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		  DpsViolationExit(Indexer->handle, paran);
#endif
		  return DPS_ERROR;
		}
		Doc->Buf.buf[0]='\0';
	}
	
	/* Check that URL has valid syntax */
	if(DpsURLParse(&Doc->CurURL, url)) {
		DpsLog(Indexer,DPS_LOG_WARN,"Invalid URL: %s",url);
		Doc->method = DPS_METHOD_DISALLOW;
	}else
	if ((Doc->CurURL.filename != NULL) && (!strcmp(Doc->CurURL.filename, "robots.txt"))) {
		Doc->method = DPS_METHOD_DISALLOW;
	}else if (Indexer->Flags.cmd != DPS_IND_POPRANK) {
		char		*alstr = NULL;

		Doc->CurURL.charset_id = Doc->charset_id;
		/* Find correspondent Server */
		DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
		Server = DpsServerFind(Indexer, (urlid_t)DpsVarListFindInt(&Doc->Sections, "Server_id", 0), url, Doc->charset_id, &alstr);
		DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);

		if ( !Server ) {
			DpsLog(Indexer,DPS_LOG_WARN,"No 'Server' command for url");
			Doc->method = DPS_METHOD_DISALLOW;
		}else{
		        DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
			Doc->lcs = Indexer->Conf->lcs;
			DpsVarList2Doc(Doc, Server);
			Doc->Spider.ExpireAt = Server->ExpireAt;
			Doc->Server = Server;
			
			DpsDocAddConfExtraHeaders(Indexer->Conf, Doc);
			DpsDocAddServExtraHeaders(Server, Doc);
			DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);

			DpsVarListReplaceLst(&Doc->Sections, &Server->Vars,NULL,"*");
			DpsVarListReplaceInt(&Doc->Sections, "Server_id", Server->site_id);
			DpsVarListReplaceInt(&Doc->Sections, "MaxHops", Doc->Spider.maxhops);
			
			if(alstr != NULL) {
				/* Server Primary alias found */
				DpsVarListReplaceStr(&Doc->Sections, "Alias", alstr);
			}else{
				/* Apply non-primary alias */
				result = DpsDocAlias(Indexer, Doc);
			}

			if((alias = DpsVarListFindStr(&Doc->Sections, "Alias", NULL))) {
			  const char *u = DpsVarListFindStr(&Doc->Sections, "URL", NULL);
			  origurl = (char*)DpsStrdup(u);
			  aliasurl = (char*)DpsStrdup(alias);
			  DpsLog(Indexer,DPS_LOG_EXTRA,"Alias: '%s'", alias);
			  DpsVarListReplaceStr(&Doc->Sections, "URL", alias);
			  DpsVarListReplaceStr(&Doc->Sections, "ORIG_URL", origurl);
			  DpsVarListDel(&Doc->Sections, "E_URL");
			  DpsVarListDel(&Doc->Sections, "URL_ID");
			  DpsURLParse(&Doc->CurURL, alias);
			}

			/* Check hops, network errors, filters */
			result = DpsDocCheck(Indexer, Server, Doc);
			switch(Doc->method) {
			case DPS_METHOD_GET:
			case DPS_METHOD_HEAD:
			case DPS_METHOD_HREFONLY:
			case DPS_METHOD_CHECKMP3:
			case DPS_METHOD_CHECKMP3ONLY:
			  if( (Server->MaxDocsPerServer != (dps_uint4)-1) && (Server->ndocs > Server->MaxDocsPerServer) ) {
			    DpsLog(Indexer, DPS_LOG_WARN, "Maximum of %u documents per server reached, skip it.", Server->MaxDocsPerServer);
			    Doc->method = DPS_METHOD_VISITLATER;
			  }
			  DpsVarListReplaceInt(&Doc->Sections, "Site_id", DpsServerGetSiteId(Indexer, Server, Doc));
			  if( (Server->MaxDocsPerSite > 0) && (DpsVarListFindInt(&Doc->Sections, "SiteNDocs", 1) > Server->MaxDocsPerSite) ) {
			    DpsLog(Indexer, DPS_LOG_WARN, "Maximum of %u documents per site reached, skip it.", Server->MaxDocsPerSite);
			    Doc->method = DPS_METHOD_VISITLATER;
			  }
			default:
			  break;
			}
			
		}
		DPS_FREE(alstr);
	}
	DpsVarListReplaceInt(&Doc->Sections, "DomainLevel", Doc->CurURL.domain_level);
/*	DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);*/
	
	
	if(result!=DPS_OK){
	        DPS_FREE(aliasurl); DPS_FREE(origurl);
		DpsDocFree(Doc);
		TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		DpsViolationExit(Indexer->handle, paran);
#endif
		return result;
	}
	
	if(Doc->method != DPS_METHOD_DISALLOW && Doc->method != DPS_METHOD_VISITLATER && Indexer->Flags.cmd != DPS_IND_POPRANK) {
	  DpsDocAddDocExtraHeaders(Indexer, Doc);
	  if(!strncmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "http", 4)) {
	    if(!Doc->Spider.use_robots){
	      DpsLog(Indexer,DPS_LOG_DEBUG, "robots.txt support is disallowed for '%s'", DPS_NULL2EMPTY(Doc->CurURL.hostinfo));
/*	      DPS_GETLOCK(Indexer,DPS_LOCK_CONF);*/
	      result = DpsRobotParse(Indexer, NULL, NULL, DPS_NULL2EMPTY(Doc->CurURL.hostinfo), DpsVarListFindInt(&Doc->Sections, "Hops", 0) + 1);
/*	      DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);*/
	    }else{
	      DPS_ROBOT_RULE	*rule;

	      /* Check whether URL is disallowed by robots.txt */
	      rule = DpsRobotRuleFind(Indexer, Server, Doc, &Doc->CurURL, 1, (alias) ? 1 : 0);
	      if(rule) {
		char *w;
		switch(rule->cmd) {
		case DPS_METHOD_DISALLOW:
		case DPS_METHOD_VISITLATER:
		  w = "Disallow"; break;
		case DPS_METHOD_CRAWLDELAY:
		  w = "Postpone"; break;
		default:
		  w = "Allow";
		}
		DpsLog(Indexer, (rule->cmd==DPS_METHOD_DISALLOW||rule->cmd==DPS_METHOD_VISITLATER) ? DPS_LOG_INFO : DPS_LOG_EXTRA, "Doc.robots.txt: '%s %s'", w, rule->path);
		if((rule->cmd == DPS_METHOD_DISALLOW) || (rule->cmd == DPS_METHOD_VISITLATER) || (rule->cmd == DPS_METHOD_CRAWLDELAY) )
		  Doc->method = rule->cmd;
	      }
	    }
	  }
	  if(origurl != NULL){
	    DpsVarListReplaceStr(&Doc->Sections,"URL",origurl);
	    DpsVarListDel(&Doc->Sections, "E_URL");
	    DpsVarListDel(&Doc->Sections, "URL_ID");
	    DpsURLParse(&Doc->CurURL,origurl);
	  }
	}
	
	if(result!=DPS_OK){
	        DPS_FREE(origurl); DPS_FREE(aliasurl);
		DpsDocFree(Doc);
		TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		DpsViolationExit(Indexer->handle, paran);
#endif
		return result;
	}

	if (Indexer->Flags.cmd == DPS_IND_POPRANK) {
	  if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_PASNEO))) {
	    DPS_FREE(origurl); DPS_FREE(aliasurl);
	    DpsDocFree(Doc);
	    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
	    DpsViolationExit(Indexer->handle, paran);
#endif
	    return result;
	  }
	}else if (Indexer->Flags.cmd == DPS_IND_FILTER) {
	    if (Doc->method == DPS_METHOD_DISALLOW) {
		result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_DELETE);
		if (DPS_OK != result) {
		    DPS_FREE(origurl); DPS_FREE(aliasurl);
		    DpsDocFree(Doc);
		    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		    DpsViolationExit(Indexer->handle, paran);
#endif
		    return result;
		}
	    }
	}else if(Doc->method != DPS_METHOD_DISALLOW && Doc->method != DPS_METHOD_VISITLATER && Doc->method != DPS_METHOD_CRAWLDELAY) {
		int	start,state;
		int	mp3type=DPS_MP3_UNKNOWN;
		
		if(!(Indexer->flags&DPS_FLAG_REINDEX)){
			const char *l = DpsVarListFindStr(&Doc->Sections, "Last-Modified", NULL);
			int prevstatus = DpsVarListFindInt(&Doc->Sections, "PrevStatus", 0);
			if ((prevstatus < 400) && (l != NULL)) DpsVarListReplaceStr(&Doc->RequestHeaders, "If-Modified-Since", l);
		}
		
		DPS_THREADINFO(Indexer,"Getting",url);
		
		start = (Doc->method == DPS_METHOD_CHECKMP3 || Doc->method == DPS_METHOD_CHECKMP3ONLY) ? 1 : 0;
		
		for(state=start;state>=0;state--){
			const char	*hdr=NULL;
			
			if(state==1) hdr = "bytes=0-2048";
			if(mp3type==DPS_MP3_TAG)hdr="bytes=-128";
			
			DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_UNKNOWN);
			
			if(aliasurl != NULL) {
			  DpsVarListReplaceStr(&Doc->Sections,"URL",alias);
			  DpsVarListDel(&Doc->Sections, "E_URL");
			  DpsVarListDel(&Doc->Sections, "URL_ID");
			  DpsURLParse(&Doc->CurURL, alias);
			}
				
			DpsVarListLog(Indexer,&Doc->RequestHeaders, DPS_LOG_DEBUG, "Request");
				
			if(hdr) {
			        DpsVarListAddStr(&Doc->RequestHeaders, "Range", hdr);
				DpsLog(Indexer, DPS_LOG_INFO, "Range: [%s]", hdr);
			}
				
			result = DpsGetURL(Indexer, Doc, origurl);

			if(hdr) {
			        DpsVarListDel(&Doc->RequestHeaders, "Range");
			}
				
			if(origurl != NULL) {
			  DpsVarListReplaceStr(&Doc->Sections, "URL", origurl);
			  DpsVarListDel(&Doc->Sections, "E_URL");
			  DpsVarListDel(&Doc->Sections, "URL_ID");
			  DpsURLParse(&Doc->CurURL,origurl);
			}
			
			if(result!=DPS_OK){
			        DPS_FREE(origurl); DPS_FREE(aliasurl);
				DpsDocFree(Doc);
				TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
				DpsViolationExit(Indexer->handle, paran);
#endif
				return result;
			}
			
			DPS_GETLOCK(Indexer,DPS_LOCK_CONF);
			if (Server) Server->ndocs++;
			DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);

/*			DpsParseHTTPResponse(Indexer, Doc);*/
			DpsDocProcessResponseHeaders(Indexer, Doc);
/*			DpsVarListLog(Indexer, &Doc->Sections, DPS_LOG_DEBUG, "Response");*/
			
			status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
			
			DpsLog(Indexer, DPS_LOG_EXTRA, "Status: %d %s", status, DpsHTTPErrMsg(status));

			if(status != DPS_HTTP_STATUS_PARTIAL_OK && status != DPS_HTTP_STATUS_OK)
				break;
			
			if(state==1){	/* Needs guessing */
				if(DPS_MP3_UNKNOWN != (mp3type = DpsMP3Type(Doc))) {
					DpsVarListReplaceStr(&Doc->Sections, "Content-Type", "audio/mpeg");
					if(Doc->method == DPS_METHOD_CHECKMP3ONLY && mp3type != DPS_MP3_TAG) break;
				}
				if(Doc->method == DPS_METHOD_CHECKMP3ONLY) break;
			}
		}
		
		/* Add URL from Location: header */
		/* This is to give a chance for  */
		/* a concurent thread to take it */
		result = DpsDocStoreHrefs(Indexer, Doc);
		if(result!=DPS_OK){
		        DPS_FREE(origurl); DPS_FREE(aliasurl);
			DpsDocFree(Doc);
			TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
			DpsViolationExit(Indexer->handle, paran);
#endif
			return result;
		}
		
		if((!Doc->Buf.content) && (status < 500) && (!strncasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "htdb:", 5))) {
			DpsLog(Indexer, DPS_LOG_WARN, "No data received");
			status=DPS_HTTP_STATUS_SERVICE_UNAVAILABLE;
			DpsVarListReplaceInt(&Doc->Sections, "Status", status);
		}
		
		/* Collect only hrefs from erroneous documents */
		if(status != DPS_HTTP_STATUS_OK && status != DPS_HTTP_STATUS_PARTIAL_OK && status != DPS_HTTP_STATUS_MOVED_TEMPORARILY) {
		  Doc->method = DPS_METHOD_HREFONLY;
		}
		if (Doc->Buf.content != NULL ) {
		   	size_t		min_size;
			size_t	hdr_len = (size_t)(Doc->Buf.content - Doc->Buf.buf);
			size_t	cont_len = Doc->Buf.size - hdr_len;
			int skip_too_small;
		   	
			min_size = (size_t)DpsVarListFindUnsigned(&Indexer->Vars, "MinDocSize", 0);
			skip_too_small = (cont_len < min_size);

			if (skip_too_small) {
			  Doc->method = DPS_METHOD_HEAD;
			  DpsLog(Indexer, DPS_LOG_EXTRA, "Too small content size (%d < %d), CheckOnly.", cont_len, min_size); 
			}

			DPS_THREADINFO(Indexer,"Parsing",url);
			
			result = DpsDocParseContent(Indexer, Doc);

			if(result!=DPS_OK){
			        DPS_FREE(origurl); DPS_FREE(aliasurl);
				DpsDocFree(Doc);
				TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
				DpsViolationExit(Indexer->handle, paran);
#endif
				return result;
			}
		}
	

		/* Guesser was here */			
			
		DpsParseURLText(Indexer, Doc);
		{
		  char reason[PATH_MAX+1];
		  int m;
		  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
		  m = DpsSectionFilterFind(DPS_LOG_DEBUG,&Indexer->Conf->SectionFilters,Doc,reason);
		  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		  if (m != DPS_METHOD_NOINDEX) {
		    char *subsection = NULL;
		    DpsLog(Indexer, DPS_LOG_DEBUG, "%s", reason);
		    if (m == DPS_METHOD_INDEX) Doc->method = DPS_METHOD_GET;

		    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
		    switch(DpsSubSectionMatchFind(Indexer, DPS_LOG_DEBUG, &Indexer->Conf->SubSectionMatch, Doc, reason, &subsection)) {
		    case DPS_METHOD_TAG:
		      DpsVarListReplaceStr(&Doc->Sections, "Tag", subsection); break;
		    case DPS_METHOD_CATEGORY:
		      DpsVarListReplaceStr(&Doc->Sections, "Category", subsection); break;
		    }
		    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		    DPS_FREE(subsection);

		    if (Doc->method != DPS_METHOD_HREFONLY) DpsPrepareWords(Indexer, Doc);

		  } else Doc->method = m;
		  DpsLog(Indexer, DPS_LOG_DEBUG, "%s", reason);
		}

		if (Server != NULL && Doc->method != DPS_METHOD_DISALLOW) {
		    /* replace Last-Modified header if the replacement section is defined */
		    char *LMDSection = DpsVarListFindStr(&Server->Vars, "LMDSection", NULL);
		    if (LMDSection != NULL) {
			char *value = DpsVarListFindStr(&Doc->Sections, LMDSection, NULL);
			if (value != NULL) {
			    DpsVarListReplaceStr(&Doc->Sections, "Last-Modified", value);
			}
		    }
		}
	
		{
		  size_t wordnum;
		  const char *lang = DpsVarListFindStr(&Doc->Sections,"Content-Language","");
		  /* Remove StopWords */
		  /*		  DPS_GETLOCK(Indexer,DPS_LOCK_CONF);*/
		  /*DpsWordListSort(&Doc->Words);*/
		  if (Doc->Words.nwords > 0) {
		    DPS_WORD *p_word = Doc->Words.Word;
		    size_t wlen = p_word->ulen;
		    int rc = (wlen > Indexer->WordParam.max_word_len ||
			      wlen < Indexer->WordParam.min_word_len ||
			      DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? lang : "" ) != NULL);
		    if (rc) p_word->coord = 0;
		    for(wordnum = 1; wordnum < Doc->Words.nwords; wordnum++) {
		      if (DpsWordCmp(p_word, Doc->Words.Word + wordnum) == 0) {
			if (rc) Doc->Words.Word[wordnum].coord = 0;
		      } else {
			p_word = Doc->Words.Word + wordnum;
			wlen = p_word->ulen;
			rc = (wlen > Indexer->WordParam.max_word_len ||
			      wlen < Indexer->WordParam.min_word_len ||
			      DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? lang : "" ) != NULL);
			if (rc) p_word->coord = 0;
		      }
		    }
		  }

		  /*DpsCrossListSort(&Doc->CrossWords);*/
		  if (Doc->CrossWords.ncrosswords > 0) {
		    DPS_CROSSWORD *p_word = Doc->CrossWords.CrossWord;
		    size_t wlen = p_word->ulen;
		    int rc = (wlen > Indexer->WordParam.max_word_len ||
			      wlen < Indexer->WordParam.min_word_len ||
			      DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? lang : "" ) != NULL);
		    if (rc) p_word->weight = 0;
		    for(wordnum = 1; wordnum < Doc->CrossWords.ncrosswords; wordnum++) {
		      if (DpsCrossCmp(p_word, Doc->CrossWords.CrossWord + wordnum) == 0) {
			if (rc) Doc->CrossWords.CrossWord[wordnum].weight = 0;
		      } else {
			p_word = Doc->CrossWords.CrossWord + wordnum;
			wlen = p_word->ulen;
			rc = (wlen > Indexer->WordParam.max_word_len ||
			      wlen < Indexer->WordParam.min_word_len ||
			      DpsStopListFind(&Indexer->Conf->StopWords, p_word->uword, (Indexer->flags & DPS_FLAG_STOPWORDS_LOOSE) ? lang : "" ) != NULL);
			if (rc) p_word->weight = 0;
		      }
		    }
		  }
		  /*		  DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);*/
		  if (Indexer->Flags.collect_links && (status == 200 || status == 206 || status == 302) )
		    if(DPS_OK != (result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_LINKS_MARKTODEL))) {
		      DpsDocFree(Doc);
		      DPS_FREE(origurl); DPS_FREE(aliasurl);
		      TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		      DpsViolationExit(Indexer->handle, paran);
#endif
		      return result;
		    }
		}

		DpsVarListLog(Indexer, &Doc->Sections, DPS_LOG_DEBUG, "Response");


		if (DPS_OK == (result = DpsDocStoreHrefs(Indexer, Doc))) {
		  if ((DPS_OK == (result = DpsStoreHrefs(Indexer))) && Indexer->Flags.collect_links  
		      && (status == 200 || status == 206 || status == 302) )
		    result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_LINKS_DELETE);
		}
	
		if(result!=DPS_OK){
		  DPS_FREE(origurl); DPS_FREE(aliasurl);
		  DpsDocFree(Doc);
		  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
		  DpsViolationExit(Indexer->handle, paran);
#endif
		  return result;
		}
	}
	
	if (Indexer->Flags.cmd == DPS_IND_POPRANK || Indexer->Flags.cmd == DPS_IND_FILTER) DpsDocFree(Doc);
	else {
	  /* Free unnecessary information */
	  DpsHrefListFree(&Doc->Hrefs);
	  DpsVarListFree(&Doc->RequestHeaders);
	  DpsTextListFree(&Doc->TextList);
	  DPS_FREE(Doc->Buf.buf);
	  Doc->Buf.max_size = Doc->Buf.allocated_size = 0;

	  result = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_FLUSH);
	}
	
	DPS_FREE(origurl); DPS_FREE(aliasurl);
	DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
	if (result == DPS_OK) result = Indexer->action;
	DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
	TRACE_OUT(Indexer);
#ifdef FILENCE
	DpsFilenceCheckLeaks(
#ifdef WITH_TRACE
			     Indexer->TR
#else
			     NULL
#endif
			     );
#endif
#ifdef WITH_PARANOIA
	DpsViolationExit(Indexer->handle, paran);
#endif
	return result;
}


/*
  Load indexer.conf and check if any DBAddr were given
*/
int DpsIndexerEnvLoad(DPS_AGENT *Indexer, const char *fname, dps_uint8 lflags) {
     int rc;
     if (DPS_OK == (rc = DpsEnvLoad(Indexer, fname, lflags))){
          if ((NULL == DpsAgentDBLSet(Indexer, Indexer->Conf))) {
	    sprintf(Indexer->Conf->errstr, "Can't set DBList at %s:%d", __FILE__, __LINE__);
	    return DPS_ERROR;
	  }
          rc = (Indexer->flags & DPS_FLAG_UNOCON) ? (Indexer->Conf->dbl.nitems == 0) : (Indexer->dbl.nitems == 0);
          if (rc) {
               sprintf(Indexer->Conf->errstr, "Error: '%s': No DBAddr command was specified", fname);
               rc= DPS_ERROR;
          } else {
	    size_t i, tix, cpnt;
	    DPS_SERVERLIST *List;	
	    rc = DPS_OK;
	    if (Indexer->Conf->total_srv_cnt) DPS_FREE(Indexer->Conf->SrvPnt)  else  Indexer->Conf->SrvPnt = NULL;
	    Indexer->Conf->total_srv_cnt = 0; cpnt = 0;
	    for (tix = DPS_MATCH_min; tix < DPS_MATCH_max; tix++) {
	      List = &Indexer->Conf->Servers[tix];
	      Indexer->Conf->total_srv_cnt += List->nservers;
	      Indexer->Conf->SrvPnt =(DPS_SERVER**)DpsRealloc(Indexer->Conf->SrvPnt, (Indexer->Conf->total_srv_cnt + 1) * sizeof(DPS_SERVER*));
	      for (i = 0; i < List->nservers; i++) {
		Indexer->Conf->SrvPnt[cpnt++] = &List->Server[i];
	      }
	    }
	    if (Indexer->Conf->total_srv_cnt > 1) DpsSort(Indexer->Conf->SrvPnt, cpnt, sizeof(DPS_SERVER*), cmpsrvpnt);
	  }
     }
     return rc;
}
