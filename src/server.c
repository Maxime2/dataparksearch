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
#include "dps_proto.h"
#include "dps_url.h"
#include "dps_hrefs.h"
#include "dps_server.h"
#include "dps_xmalloc.h"
#include "dps_host.h"
#include "dps_vars.h"
#include "dps_wild.h"
#include "dps_match.h"
#include "dps_db.h"
#include "dps_log.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#define ERRSTRSIZ 1024
#define M_SERVERS_ADD 64 /*(4096 / sizeof(DPS_SERVER))*/ /* FIXME: use page size instead of 4096 for your platform */

/*
#define TRACE_SRVS 1
*/

static size_t dps_max_server_ordre = 0;

/* return values: 0 on success, non-zero on error */

__C_LINK int __DPSCALL DpsServerAdd(DPS_AGENT *A, DPS_SERVER *srv){
	int		res = DPS_OK;
	int		add = 1;
	DPS_URL		from;
	char		*urlstr;
	DPS_SERVER	*new = NULL;
	DPS_SERVERLIST  *List;
	DPS_VARLIST     *V;
	size_t		i, len;
	DPS_ENV         *Conf = A->Conf;
	size_t          idx;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif

	if (srv == NULL) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(A->handle, paran);
#endif
	  return DPS_ERROR;
	}
	idx = (size_t)srv->Match.match_type;
	if (idx >= DPS_MATCH_max) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(A->handle, paran);
#endif
	  return DPS_ERROR;
	}
	List = &Conf->Servers[idx];
	V = &srv->Vars;

	/* Copy URL to temp string    */
	/* to keep srv->url unchanged */
	len = dps_strlen(DPS_NULL2EMPTY(srv->Match.pattern)) + 4;
	if ((urlstr = (char*)DpsMalloc(len + 1)) == NULL) {
	  DpsLog(A, DPS_LOG_ERROR, "Can't alloc %d bytes at "__FILE__":%d", len, __LINE__);
#ifdef WITH_PARANOIA
	  DpsViolationExit(A->handle, paran);
#endif
	  return DPS_ERROR;
	}
	dps_strcpy(urlstr, DPS_NULL2EMPTY(srv->Match.pattern));
	
	from.freeme = 0;
	DpsURLInit(&from);

	if((idx == DPS_MATCH_BEGIN) && (urlstr[0])){
		int follow;  
		
		/* Check whether valid URL is passed */
		if((res=DpsURLParse(&from,urlstr))){
			switch(res){
				case DPS_URL_LONG:
				  DpsLog(A, DPS_LOG_ERROR, "URL too long: %s", urlstr);
				  break;
				case DPS_URL_BAD:
				  DpsLog(A, DPS_LOG_ERROR, "Badly formed URL: %s", urlstr);
				  break;
				default:
				  DpsLog(A, DPS_LOG_ERROR, "Error while parsing URL: %s", urlstr);
				  break;
			}
			DpsURLFree(&from);
			DPS_FREE(urlstr);
#ifdef WITH_PARANOIA
			DpsViolationExit(A->handle, paran);
#endif
			return(DPS_ERROR);
		}
		if((from.hostinfo) && (from.filename == NULL)) {
			/* Add trailing slash                    */
			/* http://localhost -> http://localhost/ */
			dps_snprintf(urlstr, len, "%s://%s%s", from.schema, from.hostinfo, DPS_NULL2EMPTY(from.path));
		}
		
		switch(follow=DpsVarListFindInt(&srv->Vars,"Follow",DPS_FOLLOW_PATH)){
			char * s, * anchor;
			case DPS_FOLLOW_PATH:
				/* Cut before '?' and after last '/' */
				if((anchor=strchr(urlstr,'?')))
					*anchor='\0';
				if((s=strrchr(urlstr,'/')))
					*(s+1)='\0';
				break;

			case DPS_FOLLOW_SITE:
				if (from.hostinfo != NULL) {
					/* Cut after hostinfo */
					dps_snprintf(urlstr, len, "%s://%s/", DPS_NULL2EMPTY(from.schema), from.hostinfo);
				}else{
					/* Cut after first '/' */
					if((s=strchr(urlstr,'/')))
						*(s+1)='\0';
				}
				break;
			
			case DPS_FOLLOW_NO: 
			case DPS_FOLLOW_WORLD:
			default:
				break;
		}
		if ( !strcmp(DPS_NULL2EMPTY(from.schema), "news") ) {
			char *c, *cc;
			/* Cat server name to remove group names */
			/* This is because group names do not    */
			/* present in message URL                */
			c=urlstr+7;
			cc=strchr(c,'/');
			if(cc)*(cc+1)='\0';
		}
	}else
	if( idx == DPS_MATCH_REGEX) {
		int err;
		char regerrstr[ERRSTRSIZ]="";
		
		if(DPS_OK!=(err=DpsMatchComp(&srv->Match,regerrstr,sizeof(regerrstr)-1))){
			dps_snprintf(Conf->errstr,sizeof(Conf->errstr),"Wrong regex in config file: %s: %s", urlstr,regerrstr);
			DPS_FREE(urlstr);
			DpsURLFree(&from);
#ifdef WITH_PARANOIA
			DpsViolationExit(A->handle, paran);
#endif
			return(DPS_ERROR);
		}
	}
	for (i = 0; i < List->nservers; i++) {
		if (strcmp(List->Server[i].Match.pattern, urlstr) == 0
		    && List->Server[i].Match.nomatch == srv->Match.nomatch
		    && List->Server[i].Match.match_type == srv->Match.match_type
		    && List->Server[i].Match.case_sense == srv->Match.case_sense
		    ) {
			add = 0;
			new = &List->Server[i];
			break;
		}
	}

	if (add) {
	  List->sorted = 0;
	  if(List->nservers >= List->mservers) {
	    List->mservers += M_SERVERS_ADD;
	    List->Server = (DPS_SERVER *)DpsRealloc(List->Server, List->mservers * sizeof(DPS_SERVER));
	    if (List->Server == NULL) {
	      DpsLog(A, DPS_LOG_ERROR, "Can't realloc %d bytes at "__FILE__":%d", List->mservers * sizeof(DPS_SERVER), __LINE__);
	      List->nservers = List->mservers = 0;
	      DPS_FREE(urlstr);
#ifdef WITH_PARANOIA
	      DpsViolationExit(A->handle, paran);
#endif
	      return DPS_ERROR;
	    }
	  }
	  new = &List->Server[List->nservers];
	  DpsServerInit(new);
/**/
	  DpsVarListReplaceLst(&new->Vars, &srv->Vars, NULL, "*");
	
	  new->Match.pattern = (char*)DpsStrdup(urlstr);
	  new->Match.pat_len = dps_strlen(new->Match.pattern);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
	  new->Match.idn_pattern = (char*)DpsStrdup(DPS_NULL2EMPTY(srv->Match.idn_pattern));
	  new->Match.idn_len = dps_strlen(new->Match.idn_pattern);
#endif
	  new->Match.nomatch = srv->Match.nomatch;
	  new->Match.case_sense = srv->Match.case_sense;
	  new->Match.match_type = srv->Match.match_type;
	  new->Match.reg = srv->Match.reg;
	  new->Match.arg = srv->Match.arg;
	  new->Match.compiled = srv->Match.compiled;
	  srv->Match.reg = NULL;
	  srv->Match.arg = NULL;
	  srv->Match.compiled = 0;
	  new->command = srv->command;
	  new->ordre = srv->ordre;
	  new->weight = srv->weight;
	  new->MinSiteWeight = srv->MinSiteWeight;
	  new->MinServerWeight = srv->MinServerWeight;
	  new->MaxHops = srv->MaxHops;
	  new->MaxDepth = srv->MaxDepth;
	  new->MaxURLength = srv->MaxURLength;
	  new->MaxDocsPerServer = srv->MaxDocsPerServer;
	  new->MaxDocsPerSite = srv->MaxDocsPerSite;
	  new->MaxHrefsPerServer = srv->MaxHrefsPerServer;
	  new->ExpireAt = srv->ExpireAt;
	  new->use_robots = srv->use_robots;
	  {
	    char str[64];
	    size_t i;
	    time_t default_period = (time_t)DpsVarListFindUnsigned(V, "Period", DPS_DEFAULT_REINDEX_TIME);
	    for (i = 0; i < DPS_DEFAULT_MAX_HOPS; i++) {
	      dps_snprintf(str, sizeof(str), "Period%u", i);
	      new->period[i] = (time_t)DpsVarListFindUnsigned(V, str, default_period);
	    }
	  }

	  if (List->nservers == 0) List->min_ordre = srv->ordre;
	  new->crawl_delay = srv->crawl_delay;
	  if (srv->last_crawled == NULL) {
	    new->last_crawled = (time_t*)DpsXmalloc(sizeof(time_t));
	    if (new->last_crawled == NULL) {
	      DpsLog(A, DPS_LOG_ERROR, "Can't alloc %d bytes at "__FILE__":%d", sizeof(time_t), __LINE__);
#ifdef WITH_PARANOIA
	      DpsViolationExit(A->handle, paran);
#endif
	      return DPS_ERROR;
	    }
	    new->need_free = 1;
	  } else {
	    new->last_crawled = srv->last_crawled;
	    new->need_free = 0;
	  }

	  { char err[512];
	    for (i = 0; i < srv->HTDBsec.nmatches; i++) {
	      DpsMatchListAdd(A, &new->HTDBsec, &srv->HTDBsec.Match[i], err, sizeof(err), 0);
	    }
	  }

#ifdef TRACE_SRVS
	  fprintf(stderr, " command:%c  match_type:%d  pattern: %s\n", srv->command, srv->Match.match_type, urlstr);
#endif
	
	  res = DpsSrvAction(A, new, DPS_SRV_ACTION_ADD);
	
	  List->nservers++;
	  if (new->ordre > dps_max_server_ordre) dps_max_server_ordre = new->ordre;
	
	} else {
/*	  DPS_FREE(new->Match.pattern);*/
	}

/**/
	srv->site_id = new->site_id;
	
	DPS_FREE(urlstr);
	DpsURLFree(&from);
#ifdef WITH_PARANOIA
	DpsViolationExit(A->handle, paran);
#endif
	return(res);
}

void DpsServerFree(DPS_SERVER *Server){
	DpsMatchFree(&Server->Match);
	DpsMatchListFree(&Server->HTDBsec);
	DpsVarListFree(&Server->Vars);
	if (Server->need_free) {
	  DPS_FREE(Server->last_crawled);
	  Server->need_free = 0;
	}
}

void DpsServerListFree(DPS_SERVERLIST *List){
	size_t i;
	
	for(i=0;i<List->nservers;i++)
		DpsServerFree(&List->Server[i]);
	
	List->nservers=List->mservers=0;
	DPS_FREE(List->Server);
}

int cmpsrvpnt(const void *p1,const void *p2) {
  const DPS_SERVER **s1 = (const DPS_SERVER**)p1, **s2 = (const DPS_SERVER**)p2;
  if ((*s1)->site_id < (*s2)->site_id) return -1;
  if ((*s1)->site_id > (*s2)->site_id) return 1;
  return 0;
}

/* This fuction finds Server entry for given URL         */
/* and return Alias in "aliastr" if it is not NULL       */
/* "aliastr" must be big enough to store result          */
/* not more than DPS_URLSTR bytes are written to aliastr */

DPS_SERVER * DpsServerFind(DPS_AGENT *Agent, urlid_t server_id, const char *url, int charset_id, char **aliastr) {
#define NS 10
  DPS_MATCH_PART P[NS];
  DPS_SERVERLIST *List;	
  size_t	 i, cur_idx = dps_max_server_ordre, tix;
  DPS_SERVER	 *Res = NULL;
  DPS_CONN       conn;
  char           net[32];

  TRACE_IN(Agent, "DpsServerFind");

  net[0] = '\0';
  if (/*!(Agent->flags & DPS_FLAG_ADD_SERVURL) &&*/ (server_id != 0)) {
    DPS_SERVER key, *pkey = &key, **res;
    key.site_id = server_id;
    res = dps_bsearch(&pkey, Agent->Conf->SrvPnt, Agent->Conf->total_srv_cnt, sizeof(DPS_SERVER*), cmpsrvpnt); 
    if (res != NULL) {
      DPS_SERVER      *pRes = *res;
      int             follow = DpsVarListFindInt(&pRes->Vars, "Follow", DPS_FOLLOW_PATH);

      if(follow == DPS_FOLLOW_WORLD || !DpsMatchExec(&pRes->Match, url, net, &conn.sin, NS, P) ) {
	const char      *alias = DpsVarListFindStr(&pRes->Vars,"Alias",NULL);

	if((aliastr != NULL) && (alias != NULL)) {
	  size_t          aliastrlen = 128 + dps_strlen(url) + dps_strlen(alias) + dps_strlen(pRes->Match.pattern);
	  *aliastr = (char*)DpsMalloc(aliastrlen + 1);
	  if (*aliastr != NULL)
	    DpsMatchApply(*aliastr, aliastrlen, url, alias, &pRes->Match, 10, P);
	}
      }
      
      TRACE_OUT(Agent);
      return pRes;
    }
  }
	
  net[0] = '\0';
  /*
  fprintf(stderr, " -- FindServer for URL: %s [max_server_ordre:%d]\n", url, dps_max_server_ordre);
  */
  for (tix = DPS_MATCH_min; tix < DPS_MATCH_max; tix++) {
    List = &Agent->Conf->Servers[tix];
    if (List->nservers == 0) continue;
    if (List->min_ordre > cur_idx) continue;

    if (tix == DPS_MATCH_SUBNET) {
      DPS_URL  *URL = DpsURLInit(NULL);
	
      if (URL == NULL) continue;
			
      if(DpsURLParse(URL, url)) {
	DpsURLFree(URL);
	continue;
      }
      bzero(&conn, sizeof(conn));
      conn.hostname = URL->hostname;
      conn.port=80;
      conn.charset_id = charset_id;
      if (DpsHostLookup(Agent, &conn) != -1) {
	dps_memcpy(&conn.sin, &conn.sinaddr[0], sizeof(conn.sin));
	inet_ntop(AF_INET, &conn.sin.sin_addr, net, sizeof(net));
/*	unsigned char * h;
	h = (unsigned char*)(&conn.sin.sin_addr);
	dps_snprintf(net, sizeof(net) - 1, "%d.%d.%d.%d", h[0], h[1], h[2], h[3]);*/
      }
      DpsURLFree(URL);
    }

    for(i = 0; (i < List->nservers) && (List->Server[i].ordre <= cur_idx); i++) {
      DPS_SERVER      *srv = &List->Server[i];
      int             follow = DpsVarListFindInt(&srv->Vars, "Follow", DPS_FOLLOW_PATH);
      
      if(follow == DPS_FOLLOW_WORLD || !DpsMatchExec(&srv->Match, url, net, &conn.sin, NS, P) ) {
	const char      *alias = DpsVarListFindStr(&srv->Vars,"Alias",NULL);
	cur_idx = srv->ordre;
	Res = srv;
	if((aliastr != NULL) && (alias != NULL)) {
	  size_t          aliastrlen = 128 + dps_strlen(url) + dps_strlen(alias) + dps_strlen(srv->Match.pattern);
	  *aliastr = (char*)DpsMalloc(aliastrlen + 1);
	  if (*aliastr != NULL)
	    DpsMatchApply(*aliastr, aliastrlen, url, alias, &srv->Match, 10, P);
	}
	break;
      }
    }
    /*
    fprintf(stderr, " -- tix: %s -- cur_idx: %d, i:%d nserver:%d  lastORDRE:%d\n", DpsMatchTypeStr(tix), cur_idx, i, List->nservers, 
	    (List->nservers > 0) ? List->Server[List->nservers-1].ordre : 0);
    if (i < List->nservers) fprintf(stderr, "\t\tServer[i].ordre:%d\n", List->Server[i].ordre);
    */
  }
  TRACE_OUT(Agent);
  return(Res);
}

#if 0
static int cmpserver(const void *s1,const void *s2){
	int res;
	
	if(!(res=dps_strlen(((const DPS_SERVER*)s2)->url)-dps_strlen(((const DPS_SERVER*)s1)->url)))
		res=(((const DPS_SERVER*)s2)->rec_id)-(((const DPS_SERVER*)s1)->rec_id);
	return(res);
}
void DpsServerListSort(DPS_SERVERLIST *List){
	/*  Long name should be found first    */
	/*  to allow different options         */
	/*  for server and it's subdirectories */
	if (List->nservers) DpsSort(List->Server, List->nservers, sizeof(DPS_SERVER), cmpserver);
}
#endif

 int DpsSpiderParamInit(DPS_SPIDERPARAM *Spider){
	Spider->max_net_errors = DPS_MAXNETERRORS;
	Spider->read_timeout = DPS_READ_TIMEOUT;
	Spider->doc_timeout = DPS_DOC_TIMEOUT;
	Spider->maxhops = DPS_DEFAULT_MAX_HOPS;
	Spider->index = 1;
	Spider->follow = DPS_FOLLOW_PATH;
	Spider->use_robots = 1;
	Spider->use_clones = 1;
	Spider->net_error_delay_time=DPS_DEFAULT_NET_ERROR_DELAY_TIME;
	Spider->ExpireAt.eight = 0;
	return DPS_OK;
}

__C_LINK int __DPSCALL DpsServerInit(DPS_SERVER * srv){
  size_t i;
	bzero((void*)srv, sizeof(*srv));
	for (i = 0; i < DPS_DEFAULT_MAX_HOPS; i++) {
	  srv->period[i] = DPS_DEFAULT_REINDEX_TIME;
	}
	srv->Match.match_type=DPS_MATCH_BEGIN;
	srv->weight = 1;                       /* default ServerWeight */
	srv->MinSiteWeight = 0.0;              /* default Minimum Site weight to be indexed */
	srv->MinServerWeight = 0.0;            /* default Minimum Server weight to be indexed */
	srv->MaxHops = DPS_DEFAULT_MAX_HOPS;   /* default MaxHops value */
	srv->MaxDepth = DPS_DEFAULT_MAX_DEPTH; /* default MaxDepth value */
	srv->MaxURLength = DPS_DEFAULT_MAX_URLENGTH; /* default MaxURLength value */
	srv->MaxDocsPerServer = (dps_uint4)-1; /* default MaxDocsPerServer value */
	srv->MaxDocsPerSite = 0;               /* default MaxDocsPerSite value */
	srv->MaxHrefsPerServer = (dps_uint4)-1;/* default MaxHrefsPerServer value */
	srv->ndocs = 0;                        /* no docs indexed */
	srv->nhrefs = 0;                       /* no hrefs added */
	srv->use_robots = 1;
	return(0);
}


urlid_t DpsServerGetSiteId(DPS_AGENT *Indexer, DPS_SERVER *srv, DPS_DOCUMENT *Doc) {
  char *urlstr, *pp = NULL;
  DPS_SERVER S;
  int rc;
  char *url, *psite;

  url = DpsVarListFindStr(&Doc->Sections, "ORIG_URL", NULL);
  if (url == NULL) {
    url = DpsVarListFindStr(&Doc->Sections, "E_URL", NULL);
  }
  if (url == NULL) {
    url = DpsVarListFindStr(&Doc->Sections, "URL", NULL);
  }

  if (url == NULL) {
    if((urlstr = (char*)DpsMalloc(dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.schema)) + 
				  dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.hostname)) + 
				  dps_strlen(DPS_NULL2EMPTY(Doc->CurURL.path)) +
				  10)) == NULL) {
      return 0;
    }
    sprintf(urlstr, "%s://%s/%s", DPS_NULL2EMPTY(Doc->CurURL.schema), DPS_NULL2EMPTY(Doc->CurURL.hostname),
	    (Indexer->Flags.MaxSiteLevel < 0) ? DPS_NULL2EMPTY(Doc->CurURL.path) : "");
  } else {
    register char *p, *pp;
    if((urlstr = (char*)DpsMalloc(dps_strlen(url) + 2)) == NULL) {
      return 0;
    }
    dps_strcpy(urlstr, url);
    if ((p = strstr(urlstr, ":/")) == NULL) {DPS_FREE(urlstr); return 0; }
    if (Indexer->Flags.MaxSiteLevel < 0) {
      if ((pp = strrchr(urlstr, '/')) == NULL) { DPS_FREE(urlstr); return 0; }
    } else {
      if ((pp = strchr(p + 3, '/')) == NULL) {DPS_FREE(urlstr); return 0; }
    }
    pp[1] = '\0';
    if ((pp = strchr(p + 3, '@')) != NULL) {
      dps_strcpy(p + 3, pp + 1);
    }
  }

  psite = urlstr;

  if (Indexer->Flags.MaxSiteLevel < 0) {
    int level = Indexer->Flags.MaxSiteLevel;
    register char *p;
    if ((p = strstr(urlstr, ":/")) == NULL) {DPS_FREE(urlstr); return 0; }
    if ((pp = strchr(p + 3, '/')) == NULL) {DPS_FREE(urlstr); return 0; }
    for (p += 3; p < pp; p++) { *p = (char)dps_tolower((int)*p); }
    for ( ; level < 0; level++) {
      if ((p = strchr(pp + 1, '/')) == NULL) break;
      pp = p;
    }
    pp[1] = '\0';
  } else
  {
    char *e = urlstr + dps_strlen(urlstr) - 2, *pd = e;
    int level = 0, have_three = 0;
    for(; e > urlstr; --e) {
      if (*e == '.') {
	if (level == 1 && have_three == 0) {
	  if (pd - e < 5) have_three++;
	  else level++;
	} else level++;
	pd = e;
	if (level == Indexer->Flags.MaxSiteLevel) {
	  if (strncasecmp(e, ".www.", 5) == 0) {
	    dps_memcpy(e - 2, "http://", 7);
	    psite = e - 2;
	  } else {
	    dps_memcpy(e - 6, "http://", 7);
	    psite = e - 6;
	  }
	  break;
	}
      } else if (*e == '/') {
	if (strncasecmp(e, "/www.", 5) == 0) {
	  dps_memcpy(e - 2, "http://", 7);
	  psite = e - 2;
	}
	break;
      }
    }
  
    {
      register size_t ii;
      for (ii = 0; ii < dps_strlen(psite); ii++) psite[ii] = (char)dps_tolower((int)psite[ii]);
    }
  }

  bzero((void*)&S, sizeof(S));
  S.Match.pattern     = psite;
  S.Match.match_type  = DPS_MATCH_BEGIN;
  S.Match.nomatch     = 0;
  S.command = 'S';
  S.ordre = srv->ordre;
  S.parent = srv->site_id;
  rc = DpsSrvAction(Indexer, &S, DPS_SRV_ACTION_ID);

  DpsVarListReplaceDouble(&Doc->Sections, "SiteWeight", (double)S.weight);
  DpsVarListReplaceInt(&Doc->Sections, "SiteNdocs", (int)S.ndocs++);

  DPS_FREE(urlstr);
  return (rc == DPS_OK) ? S.site_id : 0;
}
