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
#include "dps_db.h"
#include "dps_sqldbms.h"
#include "dps_agent.h"
#include "dps_env.h"
#include "dps_conf.h"
#include "dps_services.h"
#include "dps_sdp.h"
#include "dps_log.h"
#include "dps_xmalloc.h"
#include "dps_doc.h"
#include "dps_result.h"
#include "dps_searchtool.h"
#include "dps_vars.h"
#include "dps_match.h"
#include "dps_spell.h"
#include "dps_mutex.h"
#include "dps_signals.h"
#include "dps_socket.h"
#include "dps_store.h"
#include "dps_hash.h"
#include "dps_charsetutils.h"
#include "dps_searchcache.h"
#include "dps_url.h"
#include "dps_template.h"
#include "dps_proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <errno.h>
#include <locale.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <fcntl.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_SYS_MSG_H
#include <sys/msg.h>
#endif
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long)-1)
#endif

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif


#define DEBUG_SEARCH 1


static	fd_set mask;
static DPS_CHILD Children[DPS_CHILDREN_LIMIT];
/*static*/ int clt_sock, sockfd;
static pid_t parent_pid = 0;
static size_t MaxClients = 1;

/********************* SIG Handlers ****************/
static void sighandler(int sign);
static void TrackSighandler(int sign);
static void init_signals(void){
	/* Set up signals handler*/
	DpsSignal(SIGPIPE, sighandler);
	DpsSignal(SIGCHLD, sighandler);
	DpsSignal(SIGALRM, sighandler);
	DpsSignal(SIGUSR1, sighandler);
	DpsSignal(SIGUSR2, sighandler);
	DpsSignal(SIGHUP, sighandler);
	DpsSignal(SIGINT, sighandler);
	DpsSignal(SIGTERM, sighandler);
}

static void sighandler(int sign){
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
		        while ((chpid = waitpid(-1, &status, WNOHANG)) > 0) {
			  register size_t i;
			  for (i = 0; i < MaxClients; i++) {
			    if (chpid == Children[i].pid) Children[i].pid = 0;
			  }
			}
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

static void init_TrackSignals(void){
	/* Set up signals handler*/
	DpsSignal(SIGPIPE, TrackSighandler);
	DpsSignal(SIGCHLD, TrackSighandler);
	DpsSignal(SIGALRM, TrackSighandler);
	DpsSignal(SIGUSR1, TrackSighandler);
	DpsSignal(SIGUSR2, TrackSighandler);
	DpsSignal(SIGHUP, TrackSighandler);
	DpsSignal(SIGINT, TrackSighandler);
	DpsSignal(SIGTERM, TrackSighandler);
}

static void TrackSighandler(int sign){
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
			while (waitpid(-1, &status, WNOHANG) > 0);
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
	init_TrackSignals();
}

/*************************************************************/
#define REST_REQ_SIZE 4096
#define MAX_PS 1000

static int do_RESTful(DPS_AGENT *Agent, int client, const DPS_SEARCHD_PACKET_HEADER *hdr) {
  char		template_name[PATH_MAX+6]="";
  DPS_VARLIST	query_vars;
  DPS_ENV *Env = Agent->Conf;
  DPS_RESULT	*Res;
  const char *p = (const char*)hdr;
  const char *conf_dir;
  const char      *ResultContentType;
  char *query_string, *pp;
  char *self;
  char *template_filename = NULL;
  char		*nav = NULL;
  char		*url = NULL;
  char		*searchwords = NULL;
  char		*storedstr = NULL;
  char  *bcharset, *lcharset;
  size_t len;
  ssize_t nrecv;
  int res = DPS_OK;
  size_t          catcolumns = 0;
  ssize_t		page1,page2,npages,ppp=10;
  int		page_size, page_number, have_p = 1;
  size_t		i, swlen = 0, nav_len, storedlen;
#ifdef WITH_GOOGLEGRP
  int             site_id, prev_site_id = 0;
#endif

  while (*p != '\0' && *p != ' ') p++; /* skip command name, it's only GET implemented */
  if (*p == ' ') p++;
  if (*p == '\0') return DPS_ERROR;
  query_string = (char*)DpsMalloc(REST_REQ_SIZE);
  if (query_string == NULL) return DPS_ERROR;

  len = sizeof(*hdr) - (p - (const char*)hdr);
  dps_memcpy(query_string, p, len);

  nrecv = DpsRecvstr(client, query_string + len, REST_REQ_SIZE - len, 600);
  if (nrecv < 0) {
    DpsLog(Agent, DPS_ERROR, "RESTful command rceiving error nrecv=%d", (int)nrecv);
    DPS_FREE(query_string);
    return DPS_ERROR;
  }
  query_string[nrecv + len] = '\0';
  for(pp = query_string; *pp != '\0'; pp++) if (' ' == *pp) { *pp = '\0'; break; }

  DpsLog(Agent, DPS_LOG_EXTRA, "RESTful query: %s", query_string);

  conf_dir = DpsVarListFindStr(&Env->Vars, "EtcDir", DPS_CONF_DIR);
  DpsVarListInit(&query_vars);
  DpsParseQStringUnescaped(&query_vars, query_string);
  DpsParseQueryString(Agent, &Env->Vars, query_string);

  template_filename = (char*)DpsStrdup(DpsVarListFindStr(&Env->Vars, "tmplt", "search.htm"));
  dps_snprintf(template_name, sizeof(template_name), "%s/%s", conf_dir, 
	       (p = strrchr(template_filename, DPSSLASH)) ? (p+1) : template_filename);
  self = DpsVarListFindStr(&Env->Vars, "PATH_INFO", "");

  DpsVarListReplaceStr(&Agent->Conf->Vars, "tmplt", template_filename);
  DPS_FREE(template_filename);

  Agent->tmpl.Env_Vars = &Env->Vars;

  DpsURLNormalizePath(template_name);

#define RESTexit(rc) res = rc; goto farend;
#define RESTstatus(status) DpsSockPrintf(&client, "HTTP/1.0 %d %s\nServer: %s\nConnection: close\n", status, DpsHTTPStatusStr(status), PACKAGE)

  if ( (strncmp(template_name, conf_dir, dps_strlen(conf_dir)) || (res = DpsTemplateLoad(Agent, Env, &Agent->tmpl, template_name)))) {
    DpsLog(Agent, DPS_LOG_ERROR, "Can't load template: '%s' %s\n", template_name, Env->errstr);
    if (strcmp(template_name, "search.htm")) { /* trying load default template */
      DPS_FREE(template_filename);
      template_filename = (char*)DpsStrdup("search.htm");
      dps_snprintf(template_name, sizeof(template_name), "%s/%s", conf_dir, template_filename);

      if ((res = DpsTemplateLoad(Agent, Env, &Agent->tmpl, template_name))) {

	DpsLog(Agent, DPS_LOG_ERROR, "Can't load default template: '%s' %s\n", template_name, Env->errstr);
	DpsSockPrintf(&client, "%s\n", Env->errstr);
	RESTexit(DPS_ERROR);
      }
    } else {
      DpsSockPrintf(&client, "%s\n", Env->errstr);
      RESTexit(DPS_ERROR);
    }
  }

  /* set locale if specified */
  if ((url = DpsVarListFindStr(&Env->Vars, "Locale", NULL)) != NULL) {
    setlocale(LC_COLLATE, url);
    setlocale(LC_CTYPE, url);
    setlocale(LC_ALL, url);
    { char *p;
      if ((p = strchr(url, '.')) != NULL) {
	*p = '\0';
	DpsVarListReplaceStr(&Env->Vars, "g-lc", url);
	*p = '.';
      }
    }
    url = NULL;
  }
  /* set TZ if specified */
  if ((url = DpsVarListFindStr(&Env->Vars, "TZ", NULL)) != NULL) {
    setenv("TZ", url, 1);
    tzset();
    url = NULL;
  }

  /* Call again to load search Limits if need */
  DpsParseQueryString(Agent, &Env->Vars, query_string);

  DpsVarListAddLst(&Agent->Vars, &Env->Vars, NULL, "*");
  Agent->tmpl.Env_Vars = &Agent->Vars;
  /* This is for query tracking */
  DpsVarListAddStr(&Agent->Vars, "QUERY_STRING", query_string);
  DpsVarListAddStr(&Agent->Vars, "self", self);

  bcharset = DpsVarListFindStr(&Agent->Vars, "BrowserCharset", "iso-8859-1");
  Env->bcs = DpsGetCharSet(bcharset);
  lcharset = DpsVarListFindStr(&Agent->Vars, "LocalCharset", "iso-8859-1");
  Env->lcs = DpsGetCharSet(lcharset);
  if(!Env->bcs) {
      RESTstatus(DPS_HTTP_STATUS_INTERNAL_SERVER_ERROR);
      DpsSockPrintf(&client, "Unknown BrowserCharset '%s' in template '%s'\n", bcharset, template_name);
      RESTexit(DPS_ERROR);
  }
  if(!Env->lcs) {
      RESTstatus(DPS_HTTP_STATUS_INTERNAL_SERVER_ERROR);
      DpsSockPrintf(&client, "Unknown LocalCharset '%s' in template '%s'\n", lcharset, template_name);
      RESTexit(DPS_ERROR);
  }

  ppp = DpsVarListFindInt(&Agent->Vars, "PagesPerScreen", 10);
  ResultContentType = DpsVarListFindStr(&Agent->Vars, "ResultContentType", "text/html");

  RESTstatus(DPS_HTTP_STATUS_OK);
  /* Date: header */
  {
      char buf[64];
      struct tm tm = *gmtime(Agent->now);
      strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
      DpsSockPrintf(&client, "Date: %s\n", buf);
  }
  /* Add user defined headers */
  for(i = 0; i < Agent->tmpl.Env_Vars->Root[(size_t)'r'].nvars; i++) {
      DPS_VAR *Hdr = &Agent->tmpl.Env_Vars->Root[(size_t)'r'].Var[i];
      if (strncmp(DPS_NULL2EMPTY(Hdr->name), "Request.", 8)) continue;
      DpsSockPrintf(&client, "%s: %s\n", Hdr->name + 8, Hdr->val);
  }
  DpsSockPrintf(&client, "Content-Type: %s; charset=%s\n\n", ResultContentType, bcharset);

  res         = DpsVarListFindInt(&Agent->Vars, "ps", DPS_DEFAULT_PS);
  page_size   = dps_min(res, MAX_PS);
  page_number = DpsVarListFindInt(&Agent->Vars, "p", 0);
  if (page_number == 0) {
    page_number = DpsVarListFindInt(&Agent->Vars, "np", 0);
    DpsVarListReplaceInt(&Agent->Vars, "p", page_number + 1);
    have_p = 0;
  } else page_number--;
	
  res = DpsVarListFindInt(&Agent->Vars, "np", 0) * page_size;
  DpsVarListAddInt(&Agent->Vars, "pn", res);

  catcolumns = (size_t)atoi(DpsVarListFindStr(&Agent->Vars, "CatColumns", ""));

  if(NULL == (Res = DpsFind(Agent))) {
    DpsVarListAddStr(&Agent->Vars, "E", DpsEnvErrMsg(Agent->Conf));
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "top");
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "error");
    TRACE_LINE(Agent);
    goto end;
  }
	
  DpsVarListAddInt(&Agent->Vars, "first", (int)Res->first);
  DpsVarListAddInt(&Agent->Vars, "last", (int)Res->last);
  DpsVarListAddInt(&Agent->Vars, "total", (int)Res->total_found);
  DpsVarListAddInt(&Agent->Vars, "grand_total", (int)Res->grand_total);

#ifdef HAVE_ASPELL
  { const char *q_save;
    if (Res->Suggest != NULL) {
      q_save = DpsStrdup(DpsVarListFindStr(&query_vars, "q", ""));
      DpsVarListReplaceStr(&query_vars, "q", Res->Suggest);
      DpsBuildPageURL(&query_vars, &url);
      DpsVarListReplaceStr(&Agent->Vars, "Suggest_q", Res->Suggest);
      DpsVarListReplaceStr(&Agent->Vars, "Suggest_url", url);
      DpsVarListReplaceStr(&query_vars, "q", q_save);
      DPS_FREE(q_save);
    }
  }
#endif
  TRACE_LINE(Agent);

  { 
    const char *s_save = DpsStrdup(DpsVarListFindStr(&query_vars, "s", ""));
    int p_save = DpsVarListFindInt(&query_vars, "p", 0);
    int np_save = DpsVarListFindInt(&query_vars, "np", 0);
    if (p_save == 0) {
      DpsVarListReplaceInt(&query_vars, "np", 0);
    } else {
      DpsVarListReplaceInt(&query_vars, "p", 1);
      DpsVarListDel(&query_vars, "np");
    }
    DpsVarListDel(&query_vars, "s");
    DpsBuildPageURL(&query_vars, &url);
    DpsVarListReplaceStr(&Agent->Vars, "FirstPage", url);
    if (*s_save != '\0') DpsVarListReplaceStr(&query_vars, "s", s_save);
    if (np_save) DpsVarListReplaceInt(&query_vars, "np", np_save);
    if (p_save) DpsVarListReplaceInt(&query_vars, "p", p_save);
    DPS_FREE(s_save);
  }

  DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "top");

  if((Res->WWList.nwords == 0) && (Res->nitems - Res->ncmds == 0) && (Res->num_rows == 0)){
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "noquery");
    TRACE_LINE(Agent);
    goto freeres;
  }
	
  if(Res->num_rows == 0) {
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "notfound");
    TRACE_LINE(Agent);
    goto freeres;
  }

  for (i = 0; i < Res->WWList.nwords; i++) {
    swlen += (8 * Res->WWList.Word[i].len) + 2;
  }
  if ((searchwords = DpsXmalloc(swlen + 1)) != NULL) {
    int z=0;
    for (i = 0; i < Res->WWList.nwords; i++) {
      if (Res->WWList.Word[i].count > 0) {
	sprintf(DPS_STREND(searchwords), (z)?"+%s":"%s", Res->WWList.Word[i].word);
	z++;
      }
    }
  }
  storedstr = DpsRealloc(storedstr, storedlen = (1024 + 10 * swlen) );
  if (storedstr == NULL) {
    DpsSockPrintf(&client, "Can't realloc storedstr\n");
    res = DPS_ERROR;
    goto freeres;
  }

  npages = (Res->total_found/(page_size?page_size:20)) + ((Res->total_found % (page_size?page_size:20) != 0 ) ?  1 : 0);
  page1 = page_number-ppp/2;
  page2 = page_number+ppp/2;
  if(page1 < 0) {
    page2 -= page1;
    page1 = 0;
  } else if(page2 > npages) {
    page1 -= (page2 - npages);
    page2 = npages;
  }
  if(page1 < 0) page1 = page1 = 0;
  if(page2 > npages) page2 = npages;
  nav = (char *)DpsRealloc(nav, nav_len = (size_t)(page2 - page1 + 2) * (1024 + 1024)); 
	                                                    /* !!! 1024 - limit for navbar0/navbar1 template size */ 
  if (nav == NULL) {
    DpsSockPrintf(&client, "Can't realloc nav\n");
    res = DPS_ERROR;
    goto freeres;
  }
  nav[0] = '\0';
  TRACE_LINE(Agent);

  /* build NL NB NR */
  for(i = (size_t)page1; i < (size_t)page2; i++){
    DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", (int)i + have_p);
    DpsBuildPageURL(&query_vars, &url);
    DpsVarListReplaceStr(&Agent->Vars, "NH", url);
    DpsVarListReplaceInt(&Agent->Vars, "NP", (int)(i+1));
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, NULL, DPS_STREND(nav), nav_len - strlen(nav), 
		     &Agent->tmpl, (i == (size_t)page_number)?"navbar0":"navbar1");
  }
  DpsVarListAddStr(&Agent->Vars, "NB", nav);
	
  DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", page_number - 1 + have_p);
  DpsBuildPageURL(&query_vars, &url);
  DpsVarListReplaceStr(&Agent->Vars, "NH", url);

  if(Res->first == 1) {/* First page */
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, NULL, nav, nav_len, &Agent->tmpl, "navleft_nop");
    DpsVarListReplaceStr(&Agent->Vars, "NL", nav);
  }else{
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, NULL, nav, nav_len, &Agent->tmpl, "navleft");
    DpsVarListReplaceStr(&Agent->Vars, "NL", nav);
  }
	
  DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", page_number + 1 + have_p);
  DpsBuildPageURL(&query_vars, &url);
  DpsVarListReplaceStr(&Agent->Vars, "NH", url);

  if(Res->last >= Res->total_found) {/* Last page */
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, NULL, nav, nav_len, &Agent->tmpl, "navright_nop");
    DpsVarListReplaceStr(&Agent->Vars, "NR", nav);
  } else {
    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, NULL, nav, nav_len, &Agent->tmpl, "navright");
    DpsVarListReplaceStr(&Agent->Vars, "NR", nav);
  }
	
  DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "restop");

  for(i = 0; i < Res->num_rows; i++) {
    DPS_DOCUMENT	*Doc = &Res->Doc[i];
    DPS_CATEGORY	C;
    char		*clist;
    const char	*u, *dm;
    char		*eu, *edm;
    char		*ct, *ctu;
    size_t		cl, sc, r, clistsize;
    urlid_t		dc_url_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
    urlid_t		dc_origin_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "Origin-ID", 0);
		
    /* Skip clones */
    if(dc_origin_id) continue;
		
    clist = (char*)DpsMalloc(2048); 
    if (clist == NULL) {
      DpsSockPrintf(&client, "Can't alloc clist\n");
      res = DPS_ERROR;
      goto freeres;
    }
    clist[0] = '\0';
    clistsize = 0;
    for(cl = 0; cl < Res->num_rows; cl++) {
      DPS_DOCUMENT	*Clone = &Res->Doc[cl];
      urlid_t		cl_origin_id = (urlid_t)DpsVarListFindInt(&Clone->Sections, "Origin-ID", 0);
			
      if ((dc_url_id == cl_origin_id) && cl_origin_id){
	DPS_VARLIST	CloneVars;
				
	DpsVarListInit(&CloneVars);
	DpsVarListAddLst(&CloneVars, &Agent->Vars, NULL, "*");
	DpsVarListReplaceLst(&CloneVars, &Doc->Sections, NULL, "*");
	DpsVarListReplaceLst(&CloneVars, &Clone->Sections, NULL, "*");
	clist = (char*)DpsRealloc(clist, (clistsize = dps_strlen(clist)) + 2048);
	if (clist == NULL) {
	  DpsSockPrintf(&client, "Can't realloc clist\n");
	  res = DPS_ERROR;
	  goto freeres;
	}
	Agent->tmpl.Env_Vars = &CloneVars;
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, NULL, clist + clistsize, 2048, &Agent->tmpl, "clone");
	DpsVarListFree(&CloneVars);
      }
    }
    Agent->tmpl.Env_Vars = &Agent->Vars;
    clistsize += 2048;
		
    DpsVarListReplaceStr(&Agent->Vars, "CL", clist);
    DpsVarListReplaceInt(&Agent->Vars, "DP_ID", dc_url_id);
		
    DpsVarListReplace(&Agent->Vars, DpsVarListFind(&Doc->Sections, "Alias"));

    DpsVarListReplaceStr(&Agent->Vars, "title", "[no title]");

    /* Pass all found user-defined sections */
    for (r = 0; r < 256; r++)
      for (sc = 0; sc < Doc->Sections.Root[r].nvars; sc++) {
	DPS_VAR *S = &Doc->Sections.Root[r].Var[sc];
	DpsVarListReplace(&Agent->Vars, S);
      }
		
    bzero((void*)&C, sizeof(C));
    dps_strncpy(C.addr, DpsVarListFindStr(&Doc->Sections, "Category", "0"), sizeof(C.addr));
    if(catcolumns && !DpsCatAction(Agent, &C, DPS_CAT_ACTION_PATH)){
      char *catpath = NULL, *pp;
      size_t c, l = 2;

      for(c = (catcolumns > C.ncategories) ? (C.ncategories - catcolumns) : 0; c < C.ncategories; c++) 
	l += 32 + dps_strlen(C.Category[c].path) + dps_strlen(C.Category[c].name);
      pp = catpath = (char*)DpsMalloc(l);
      if (catpath != NULL) {
	*catpath = '\0';
	for(c = (catcolumns > C.ncategories) ? (C.ncategories - catcolumns) : 0; c < C.ncategories; c++) {
	  sprintf(pp, " &gt; <a href=\"?c=%s\">%s</a> ", C.Category[c].path, C.Category[c].name);
	  pp += dps_strlen(pp);
	}
	DpsVarListReplaceStr(&Agent->Vars, "DY", catpath);
	DPS_FREE(catpath);
      }
    }
    DPS_FREE(C.Category);

    u =  DpsVarListFindStrTxt(&Agent->Vars, "URL", "");
    eu = (char*)DpsMalloc(dps_strlen(u)*10 + 64);
    if (eu == NULL) {
      DpsSockPrintf(&client, "Can't alloc eu\n");
      res = DPS_ERROR;
      goto freeres;
    }
    DpsEscapeURL(eu, u);

    dm = DpsVarListFindStr(&Agent->Vars, "Last-Modified", "");
    edm = (char*)DpsMalloc(dps_strlen(dm)*10 + 10);
    if (edm == NULL) {
      DpsSockPrintf(&client, "Can't alloc edm\n");
      res = DPS_ERROR;
      goto freeres;
    }
    DpsEscapeURL(edm, dm);

    ct = DpsVarListFindStr(&Agent->Vars, "Content-Type", "");
    ctu = (char*)DpsMalloc(dps_strlen(ct) * 10 + 10);
    if (ctu == NULL) {
      DpsSockPrintf(&client, "Can't alloc ctu\n");
      res = DPS_ERROR;
      goto freeres;
    }
    DpsEscapeURL(ctu, ct);

    dps_snprintf(storedstr, storedlen, "%s?rec_id=%d&amp;label=%s&amp;DM=%s&amp;DS=%d&amp;L=%s&amp;CS=%s&amp;DU=%s&amp;CT=%s&amp;q=%s",
		 DpsVarListFindStr(&Agent->Vars, "StoredocURL", "/cgi-bin/storedoc.cgi"),
		 DpsURL_ID(Doc, NULL), 
		 DpsVarListFindStr(&Agent->Vars, "label", ""),
		 edm, /* Last-Modified escaped */
		 sc = DpsVarListFindInt(&Agent->Vars, "Content-Length", 0),
		 DpsVarListFindStr(&Agent->Vars, "Content-Language", ""),
		 DpsVarListFindStr(&Agent->Vars, "Charset", ""),
		 eu, /* URL escaped */
		 ctu, /* Content-Type escaped */
		 searchwords
		 );

    if (sc >= 10485760) {
      dps_snprintf(eu, 64, "%dM", sc / 1048576);
    } else if (sc >= 1048576) {
      dps_snprintf(eu, 64, "%.1fM", (double)sc / 1048576);
    } else if (sc >= 10240) {
      dps_snprintf(eu, 64, "%dK", sc / 1024);
    } else if (sc >= 1024) {
      dps_snprintf(eu, 64, "%.1fK", (double)sc / 1024);
    } else {
      dps_snprintf(eu, 64, "%d", sc);
    }

    DpsVarListReplaceStr(&Agent->Vars, "FancySize", eu);

    DpsFree(eu);
    DpsFree(edm);
    DpsFree(ctu);

    if ((DpsVarListFindStr(&Doc->Sections, "Z", NULL) == NULL)) {
      DpsVarListReplaceInt(&Agent->Vars, "ST", 1);
      DpsVarListReplaceStr(&Agent->Vars, "stored_href", storedstr);
    } else {
      DpsVarListReplaceInt(&Agent->Vars, "ST", 0);
      DpsVarListReplaceStr(&Agent->Vars, "stored_href", "");
    }
		
    if (Res->PerSite) {
      DpsVarListReplaceUnsigned(&Agent->Vars, "PerSite", Res->PerSite[i + Res->offset * (Res->first - 1)]);
    }

    /* put Xres sections if any */
    for (r = 2; r <= Res->num_rows; r++) {
      if ((i + 1) % r == 0) {
	dps_snprintf(template_name, sizeof(template_name), "%dres", r);
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, template_name);
      }
    }
    if ( (sc = DpsVarListFindInt(&Agent->Vars, "site", 0)) == 0) {
      DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", have_p);
      DpsVarListReplaceInt(&query_vars, "site", 
#ifdef WITH_GOOGLEGRP
			   site_id = 
#endif
			   DpsVarListFindInt(&Doc->Sections, "Site_id", 0));
      DpsBuildPageURL(&query_vars, &url);
      DpsVarListReplaceStr(&Agent->Vars, "sitelimit_href", url);
#ifdef WITH_GOOGLEGRP
      if (site_id == prev_site_id) {
	DpsVarListAddStr(&Agent->Vars, "grouped", "yes");
	DpsSockPrintf(&client, "%s", Agent->tmpl.GrBeg);
      }
#endif
    }

    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "res");
#ifdef WITH_GOOGLEGRP
    if ((sc == 0) && (site_id == prev_site_id)) {
      DpsVarListDel(&Agent->Vars, "grouped");
      DpsSockPrintf(&client, "%s", Agent->tmpl.GrEnd);
    }
    prev_site_id = site_id;
#endif
    /* put resX sections if any */
    for (r = 2; r <= Res->num_rows; r++) {
      if ((i + 1) % r == 0) {
	dps_snprintf(template_name, sizeof(template_name), "res%d", r);
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, template_name);
      }
    }

    /* Revoke all found user-defined sections */
    for (r = 0; r < 256; r++)
      for(sc = 0; sc < Doc->Sections.Root[r].nvars; sc++){
	DPS_VAR *S = &Doc->Sections.Root[r].Var[sc];
	DpsVarListDel(&Agent->Vars, S->name);
      }
    DpsVarListDel(&Agent->Vars, "body"); /* remoke "body" if it's not in doc's info and made from Excerpt */
		
    DpsFree(clist);
  }
  TRACE_LINE(Agent);
  DpsVarListReplaceInt(&Agent->Vars, (have_p) ? "p" : "np", page_number + have_p);
  DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "resbot");
  DPS_FREE(searchwords);
  DPS_FREE(storedstr);
	

  res = DPS_OK;

freeres:
	
end:
  DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&DpsSockPrintf, &client, NULL, 0, &Agent->tmpl, "bottom");
  DpsResultFree(Res);
  dps_closesocket(client); /* Too early closure ? */

#ifdef HAVE_ASPELL
  if (Agent->Flags.use_aspellext && Agent->naspell > 0) {
    register size_t i;
#ifdef UNIONWAIT
    union wait status;
#else
    int status;
#endif
    for (i = 0; i < Agent->naspell; i++) {
      if (Agent->aspell_pid[i]) {
	kill(Agent->aspell_pid[i], SIGTERM);
	Agent->aspell_pid[i] = 0;
      }
    }
    while(waitpid(-1, &status, WNOHANG) > 0);
    Agent->naspell = 0;
  }
#endif /* HAVE_ASPELL*/	

farend:

  (void)DpsVarListDelLst(&Env->Vars, &query_vars, NULL, "*");

  DpsVarListFree(&query_vars);
  DPS_FREE(template_filename);
  DPS_FREE(query_string);
  DPS_FREE(url);
  DPS_FREE(nav);
  return res;
}

/*************************************************************/

static int do_client(DPS_AGENT *Agent, int client){
	char buf[1024]="";
	DPS_SEARCHD_PACKET_HEADER hdr;
	DPS_RESULT  *Res;
	struct	sockaddr_in server_addr;
	struct	sockaddr_in his_addr;
	struct	in_addr bind_address;
	char port_str[16];
	char *words = NULL;
	ssize_t nrecv,nsent;
	int verb = DPS_LOG_EXTRA;
	int done=0;
	int		page_number;
	int		page_size;
	int pre_server, server;
	const char *bcharset;
	size_t ExcerptSize, ExcerptPadding;
#if defined HAVE_PTHREAD
	pthread_t *threads;
	DPS_EXCERPT_CFG *Cfg;
	pthread_attr_t attr;
#else
	DPS_EXCERPT_CFG Cfg;
#endif
#ifdef __irix__
	int addrlen;
#else
	socklen_t addrlen;
#endif
#ifdef DEBUG_SEARCH
	unsigned long ticks = DpsStartTimer();
	unsigned long total_ticks;
#else
	unsigned long ticks = DpsStartTimer();
#endif

	Res=DpsResultInit(NULL);
	if (Res == NULL) return DPS_ERROR;

	server = client;

	while(!done){
	  size_t dlen = 0, ndocs, i, last_cmd;
		int * doc_id=NULL;
		char * dinfo=NULL;
		char * tok, * lt;
		
		DpsLog(Agent,verb,"Waiting for command header");
		nrecv = DpsRecvall(client, &hdr, sizeof(hdr), 5);
		if(nrecv != sizeof(hdr)){
			DpsLog(Agent,verb,"Received incomplete header nrecv=%d", (int)nrecv);
			if (!strncasecmp((char*)&hdr, "GET ", 4)) do_RESTful(Agent, client, &hdr);
			break;
		}else{
		  if (!strncasecmp((char*)&hdr, "GET ", 4)) {
			  do_RESTful(Agent, client, &hdr);
			  break;
			}
			DpsLog(Agent,verb,"Received header cmd=%d len=%d", hdr.cmd, hdr.len);
		}
		switch(last_cmd = hdr.cmd){
			case DPS_SEARCHD_CMD_DOCINFO:
				dinfo = (char*)DpsRealloc(dinfo, hdr.len + 1);
				if (dinfo == NULL) {
					DPS_FREE(doc_id);
					done=1;
					break;
				}
				nrecv = DpsRecvall(client, dinfo, hdr.len, 360);
				if((size_t)nrecv != hdr.len){
					DpsLog(Agent,verb,"Received incomplete data nbytes=%d nrecv=%d",hdr.len,(int)nrecv);
					DPS_FREE(doc_id);
					done=1;
					break;
				}
				dinfo[hdr.len]='\0';
				ndocs = 0;
				tok = dps_strtok_r(dinfo, "\r\n", &lt, NULL);
				
				while(tok){
					Res->Doc = (DPS_DOCUMENT*)DpsRealloc(Res->Doc, sizeof(DPS_DOCUMENT) * (ndocs + 1));
					if (Res->Doc == NULL) {
					  DPS_FREE(doc_id);
					  done=1;
					  break;
					}
					DpsDocInit(&Res->Doc[ndocs]);
					DpsVarListReplaceLst(&Res->Doc[ndocs].Sections, &Agent->Conf->Sections, NULL, "*");
					Res->Doc[ndocs].method = DPS_METHOD_GET;
					DpsDocFromTextBuf(&Res->Doc[ndocs], tok);
#ifdef WITH_MULTIDBADDR
					if ((Agent->flags & DPS_FLAG_UNOCON) ? (Agent->Conf->dbl.nitems > 1) : (Agent->dbl.nitems > 1)) {
					  char *dbstr = DpsVarListFindStr(&Res->Doc[ndocs].Sections, "dbnum", NULL);
					  if (dbstr != NULL) {
					    Res->Doc[ndocs].dbnum = DPS_ATOI(dbstr);
					  }
					}
#endif
					
					tok = dps_strtok_r(NULL, "\r\n", &lt, NULL);
					ndocs++;
				}
				
				Res->num_rows = ndocs;
				DpsLog(Agent,verb,"Received DOCINFO command len=%d ndocs=%d",hdr.len,ndocs);
#ifdef DEBUG_SEARCH
				total_ticks = DpsStartTimer();
#endif
				
				if(DPS_OK != DpsResAction(Agent, Res, DPS_RES_ACTION_DOCINFO)){
/*					DpsResultFree(Res);  must not be here */
					dps_snprintf(buf,sizeof(buf)-1,"%s",DpsEnvErrMsg(Agent->Conf));
					DpsLog(Agent, DPS_LOG_ERROR, "ResAction Error: %s", DpsEnvErrMsg(Agent->Conf));
					hdr.cmd=DPS_SEARCHD_CMD_ERROR;
					hdr.len=dps_strlen(buf);
					nsent = DpsSearchdSendPacket(server, &hdr, buf);
					done=1;
					break;
				}
				
#ifdef DEBUG_SEARCH
				total_ticks = DpsStartTimer() - total_ticks;
				DpsLog(Agent, DPS_LOG_EXTRA, "ResAction in %.2f sec.", (float)total_ticks / 1000);
#endif
				dlen=0;
				
#ifdef DEBUG_SEARCH
				total_ticks = DpsStartTimer();
#endif
				DpsAgentStoredConnect(Agent);
				ExcerptSize = (size_t)DpsVarListFindInt(&Agent->Vars, "ExcerptSize", 256);
				ExcerptPadding = (size_t)DpsVarListFindInt(&Agent->Vars, "ExcerptPadding", 40);

#if defined HAVE_PTHREAD
#ifdef HAVE_PTHREAD_SETCONCURRENCY_PROT
				if (pthread_setconcurrency(Res->num_rows + 1) != 0) {
				  DpsLog(A, DPS_LOG_ERROR, "Can't set %d concurrency threads", Res->num_rows + 1);
				}
#elif HAVE_THR_SETCONCURRENCY_PROT
				if (thr_setconcurrency(Res->num_rows + 1) != NULL) {
				  DpsLog(A, DPS_LOG_ERROR, "Can't set %d concurrency threads", Res->num_rows + 1);
				}
#endif
               
				threads = (pthread_t*)DpsMalloc((Res->num_rows + 1) * sizeof(pthread_t));
				Cfg = (DPS_EXCERPT_CFG*)DpsMalloc((Res->num_rows + 1) * sizeof(DPS_EXCERPT_CFG));
				if (threads != NULL && Cfg != NULL) {
				  pthread_attr_init(&attr);
				  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
				  for(i = 0; i < Res->num_rows; i++) {
				    Cfg[i].query = Agent;
				    Cfg[i].Res = Res;
				    Cfg[i].size = ExcerptSize;
				    Cfg[i].padding = ExcerptPadding;
				    Cfg[i].Doc = &Res->Doc[i];
				    if (pthread_create(&threads[i], &attr, &DpsExcerptDoc, Cfg + i) == 0) {
				    }
				  }
				  pthread_attr_destroy(&attr);
				}
				
#else /* HAVE_PTHREAD */
				Cfg.query = Agent;
				Cfg.Res = Res;
				Cfg.size = ExcerptSize;
				Cfg.padding = ExcerptPadding;
#endif /* HAVE_PTHREAD */
	
				for(i=0;i<Res->num_rows;i++){
					size_t		ulen;
					size_t		olen;
					char		*textbuf;
					size_t		nsec, r;
					DPS_DOCUMENT	*D=&Res->Doc[i];
#if defined(HAVE_PTHREAD)
					if (threads != NULL && Cfg != NULL) {
					    if (pthread_join(threads[i], NULL) != 0) {
					    }
					}
#else
					char		*al;
					
					al = DpsVarListFindStrTxt(&D->Sections, "URL", "");
					DpsLog(Agent, DPS_LOG_DEBUG, "Start excerpts for %s [dbnum:%d]", al, D->dbnum);

					if (Agent->Flags.do_excerpt) {
					  Cfg.Doc = D;
					  DpsExcerptDoc(&Cfg);
					}
#endif
					if (DpsVarListFindStr(&D->Sections, "Z", NULL) != NULL) {
					  DpsVarListReplaceStr(&D->Sections, "ST", "0");
					}
					textbuf = DpsDocToTextBuf(D, 1, 0);
					if (textbuf == NULL) break;
/*					
					fprintf(stderr, "%s\n\n", textbuf);
*/
					ulen=dps_strlen(textbuf)+2;
					olen=dlen;
					dlen=dlen+ulen;
					dinfo=(char*)DpsRealloc(dinfo,dlen+1);
					if (dinfo == NULL) {
					  DPS_FREE(doc_id);
					  done=1;
					  break;
					}
					dinfo[olen]='\0';
					sprintf(dinfo+olen,"%s\r\n",textbuf);
					DpsFree(textbuf);
				}
#if defined HAVE_PTHREAD
				DPS_FREE(Cfg);
				DPS_FREE(threads);
#endif				
				
#ifdef DEBUG_SEARCH
				total_ticks = DpsStartTimer() - total_ticks;
				DpsLog(Agent, DPS_LOG_EXTRA, "Excerpts in %.2f sec.", (float)total_ticks / 1000);
#endif
				if(!dinfo) dinfo = (char*)DpsStrdup("nodocinfo");
			
				hdr.cmd=DPS_SEARCHD_CMD_DOCINFO;
				hdr.len=dps_strlen(dinfo);
				nsent = DpsSearchdSendPacket(server, &hdr, dinfo);
				DpsLog(Agent, verb, "Sent doc_info packet %d bytes", (int)nsent);
				DPS_FREE(dinfo);
				
				break;

		        case DPS_SEARCHD_CMD_CLONES:
			  {
			    DPS_RESULT *Cl;
			    DPS_DOCUMENT *D;
			    int origin_id;
			    char cl_buf[128];

			    if (hdr.len > 128) {
			      DpsLog(Agent, verb, "Received too many data bytes=%d", hdr.len);
			      done = 1;
			      break;
			    }
			    nrecv = DpsRecvall(client, cl_buf, hdr.len, 360);
			    if((size_t)nrecv != hdr.len){
			      DpsLog(Agent, verb, "Received incomplete data nbytes=%d nrecv=%d", hdr.len, (int)nrecv);
			      done = 1;
			      break;
			    }
			    cl_buf[nrecv] = '\0';
			    sscanf(cl_buf, "%d", &origin_id);
			    D = DpsDocInit(NULL);
			    DpsVarListReplaceLst(&D->Sections, &Agent->Conf->Sections, NULL, "*");
			    D->method = DPS_METHOD_GET;
			    DpsVarListAddInt(&D->Sections, "DP_ID", origin_id);
			    Cl = DpsCloneList(Agent, &Agent->Vars, D);

			    DpsDocFree(D);
			    dlen=0;
				
			    if (Cl) for(i = 0; i < Cl->num_rows; i++) {
					size_t		ulen;
					size_t		olen;
					char		*textbuf;
					size_t		nsec, r;
					
					D = &Cl->Doc[i];
					
					for (r = 0; r < 256; r++)
					for (nsec = 0; nsec < D->Sections.Root[r].nvars; nsec++)
						D->Sections.Root[r].Var[nsec].section = 1;
					
					textbuf = DpsDocToTextBuf(D, 1, 0);
					if (textbuf == NULL) break;
					
					ulen=dps_strlen(textbuf)+2;
					olen=dlen;
					dlen=dlen+ulen;
					dinfo=(char*)DpsRealloc(dinfo,dlen+1);
					if (dinfo == NULL) {
					  DPS_FREE(doc_id);
					  done=1;
					  break;
					}
					dinfo[olen]='\0';
					sprintf(dinfo+olen,"%s\r\n",textbuf);
					DpsFree(textbuf);
			    }
			    
			    if (!dinfo) dinfo = (char*)DpsStrdup("nocloneinfo");
				
			    hdr.cmd = DPS_SEARCHD_CMD_DOCINFO;
			    hdr.len = dps_strlen(dinfo); 
			    nsent = DpsSearchdSendPacket(server, &hdr, dinfo);
			    DpsLog(Agent, verb, "Sent clone_info packet %d bytes", (int)nsent);
			    DPS_FREE(dinfo);
			    DpsResultFree(Cl);
			  }				
			  break;

 		        case DPS_SEARCHD_CMD_CATINFO:
			  {
			        DPS_CATEGORY Cat;
				int cmd;
				
				bzero((void*)&Cat, sizeof(DPS_CATEGORY));
				nrecv = DpsRecvall(client, &cmd, sizeof(int), 360);
				nrecv = DpsRecvall(client, Cat.addr, hdr.len - sizeof(int), 360);
				Cat.addr[hdr.len - sizeof(int)] = '\0';
				DpsLog(Agent,verb,"Received CATINFO command len=%d, cmd=%d, addr='%s'", hdr.len, cmd, Cat.addr);

				DpsCatAction(Agent, &Cat, cmd);
				
				dlen = Cat.ncategories * 1024;
				dinfo = (char*)DpsMalloc(dlen + 1);
				if (dinfo == NULL) {
					DPS_FREE(doc_id);
					done=1;
					break;
				}
				DpsCatToTextBuf(&Cat, dinfo, dlen);
				
				hdr.cmd = DPS_SEARCHD_CMD_CATINFO;
				hdr.len = dps_strlen(dinfo);
				nsent = DpsSearchdSendPacket(server, &hdr, dinfo);
				DpsLog(Agent,verb,"Sent cat_info packet %d bytes",(int)nsent);
				DPS_FREE(dinfo);

				DPS_FREE(Cat.Category);
				
			  }
			  break;
				
 		        case DPS_SEARCHD_CMD_URLACTION:
			  {
				int cmd;
				
				DpsLog(Agent,verb,"Received URLACTION command len=%d", hdr.len);
				nrecv = DpsRecvall(client, &cmd, sizeof(int), 360);

				DpsURLAction(Agent, NULL, cmd);
				DpsLog(Agent,verb,"Received URLACTION command len=%d", hdr.len);
				
				hdr.cmd = DPS_SEARCHD_CMD_DOCCOUNT;
				hdr.len = sizeof(Agent->doccount);
				nsent = DpsSearchdSendPacket(server, &hdr, (char*)&Agent->doccount);
				DpsLog(Agent,verb,"Sent doccount packet %d bytes",(int)nsent);

			  }
			  break;
				
			case DPS_SEARCHD_CMD_WORDS:
		        case DPS_SEARCHD_CMD_WORDS_ALL:
			        words = (char*)DpsRealloc(words, hdr.len + 1);
				if (words == NULL) {
					dps_snprintf(buf, sizeof(buf)-1, "Can't alloc memory for query");
					DpsLog(Agent, verb, "Can't alloc memory for query");
					hdr.cmd=DPS_SEARCHD_CMD_ERROR;
					hdr.len=dps_strlen(buf);
					DpsSearchdSendPacket(server, &hdr, buf);
					done=1;
					break;
				}
				nrecv=DpsRecvall(client, words, hdr.len, 360); /* FIXME: check */
				words[nrecv]='\0';
				DpsLog(Agent, verb, "Received words len=%d words='%s'", nrecv, words);
				
				DpsParseQueryString(Agent, &Agent->Vars, words);
				bcharset = DpsVarListFindStr(&Agent->Vars, "BrowserCharset", "iso-8859-1");
				if (!(Agent->Conf->bcs = DpsGetCharSet(bcharset))) {
					dps_snprintf(buf,sizeof(buf)-1,"Unknown BrowserCharset: %s", bcharset);
					DpsLog(Agent,verb,"Unknown BrowserCharset: %s", bcharset);
					hdr.cmd=DPS_SEARCHD_CMD_ERROR;
					hdr.len=dps_strlen(buf);
					DpsSearchdSendPacket(server, &hdr, buf);
					done=1;
					break;
				}
				
				DpsLog(Agent, DPS_LOG_INFO, "Query: [%s:%s:%s:%s] %s ", DpsVarListFindStr(&Agent->Vars, "IP", "localhost"),
				       DpsVarListFindStr(&Agent->Vars, "tmplt", "search.htm"), 
				       bcharset,
				       DpsVarListFindStr(&Agent->Vars, "label", ""), 
				       DpsVarListFindStr(&Agent->Vars, "q", ""));
				
				DpsPrepare(Agent, Res);		/* Prepare search query */
				if(DPS_OK != DpsFindWords(Agent, Res)) {
					dps_snprintf(buf,sizeof(buf)-1,"%s",DpsEnvErrMsg(Agent->Conf));
					DpsLog(Agent,verb,"%s",DpsEnvErrMsg(Agent->Conf));
					hdr.cmd=DPS_SEARCHD_CMD_ERROR;
					hdr.len = dps_strlen(buf) + 1;
					DpsSearchdSendPacket(server, &hdr, buf);
					done=1;
					break;
				}
				
				dps_snprintf(buf,sizeof(buf)-1,"Total_found=%d %d", Res->total_found, Res->grand_total);
				hdr.cmd=DPS_SEARCHD_CMD_MESSAGE;
				hdr.len = dps_strlen(buf) + 1;
				nsent=DpsSearchdSendPacket(server, &hdr, buf);
				DpsLog(Agent,verb,"Sent total_found packet %d bytes buf='%s'",(int)nsent,buf);

/*				if (Res->offset) {
				  hdr.cmd = DPS_SEARCHD_CMD_WITHOFFSET;
				  hdr.len = 0;
				  nsent = DpsSearchdSendPacket(server, &hdr, buf);
				  DpsLog(Agent,verb,"Sent withoffset packet %d bytes",(int)nsent);
				}*/

				{
				  char *wbuf, *p;
				  p = DpsVarListFindStr(&Agent->Vars, "q", "");
				  hdr.cmd = DPS_SEARCHD_CMD_QLC;
				  hdr.len = dps_strlen(p);
				  nsent = DpsSearchdSendPacket(server, &hdr, p);
				  if (nsent != sizeof(hdr) + hdr.len) {
				      DpsLog(Agent, DPS_LOG_ERROR, "Error sending cmd_qlc packet, %d of %d bytes sent", nsent, sizeof(hdr) + hdr.len);
				  }

				  hdr.cmd = DPS_SEARCHD_CMD_WWL;
				  hdr.len = sizeof(DPS_WIDEWORDLIST_EX);
				  for (i = 0; i < Res->WWList.nwords; i++) {
				    hdr.len += sizeof(DPS_WIDEWORD_EX) 
				      + sizeof(char) * (Res->WWList.Word[i].len + 1) 
				      + sizeof(dpsunicode_t) * (Res->WWList.Word[i].ulen + 1) 
				      + sizeof(int);
				  }
				  if (hdr.len & 1) hdr.len++;
				  p = wbuf = (char *)DpsXmalloc(hdr.len + 1); /* need DpsXmalloc */
				  if (p == NULL) {
					done=1;
					break;
				  }
				  dps_memcpy(p, &(Res->WWList), sizeof(DPS_WIDEWORDLIST_EX));
				  p += sizeof(DPS_WIDEWORDLIST_EX);
				  for (i = 0; i < Res->WWList.nwords; i++) {
				    dps_memcpy(p, &(Res->WWList.Word[i]), sizeof(DPS_WIDEWORD_EX));
				    p += sizeof(DPS_WIDEWORD_EX);
				    dps_memcpy(p, Res->WWList.Word[i].word, Res->WWList.Word[i].len + 1);
				    p += Res->WWList.Word[i].len + 1;
				    p += sizeof(dpsunicode_t) - ((SDPALIGN)p % sizeof(dpsunicode_t));
				    dps_memcpy(p, Res->WWList.Word[i].uword, sizeof(dpsunicode_t) * (Res->WWList.Word[i].ulen + 1));
				    p += sizeof(dpsunicode_t) * (Res->WWList.Word[i].ulen + 1);
				  }
				  nsent = DpsSearchdSendPacket(server, &hdr, wbuf);
				  DpsLog(Agent, verb, "Sent WWL packet %d bytes cmd=%d len=%d nwords=%d", 
					 (int)nsent, hdr.cmd, hdr.len, Res->WWList.nwords);

				  DPS_FREE(wbuf);
				}
#define MAX_PS 1000
				if (last_cmd == DPS_SEARCHD_CMD_WORDS_ALL) {
				  Res->num_rows = Res->total_found;
				  Res->first = 0;
				} else {
				  page_size   = DpsVarListFindInt(&Agent->Vars, "ps", DPS_DEFAULT_PS);
				  page_size   = dps_min(page_size, MAX_PS);
				  page_number = DpsVarListFindInt(&Agent->Vars, "p", 0);
				  if (page_number == 0) {
				    page_number = DpsVarListFindInt(&Agent->Vars, "np", 0);
				  } else page_number--;
				  Res->first = page_number * page_size;	
				  if(Res->first >= Res->total_found) Res->first = (Res->total_found)? (Res->total_found - 1) : 0;
				  if((Res->first + page_size) > Res->total_found){
				    Res->num_rows = Res->total_found - Res->first;
				  }else{
				    Res->num_rows = page_size;
				  }
				}
				Res->last = Res->first + Res->num_rows - 1;
				
				if (Res->PerSite) {
				  hdr.cmd = DPS_SEARCHD_CMD_PERSITE;
				  hdr.len = /*Res->CoordList.ncoords*/ Res->num_rows * sizeof(*Res->PerSite);
				  nsent = DpsSearchdSendPacket(server, &hdr, &Res->PerSite[Res->first]);
				  DpsLog(Agent, verb, "Sent PerSite packet %u bytes cmd=%d len=%d", nsent, hdr.cmd, hdr.len);
				}

				hdr.cmd = DPS_SEARCHD_CMD_DATA;
				hdr.len = /*Res->CoordList.ncoords*/ Res->num_rows * sizeof(*Res->CoordList.Data);
				nsent = DpsSearchdSendPacket(server, &hdr, &Res->CoordList.Data[Res->first]);
				DpsLog(Agent, verb, "Sent URLDATA packet %u bytes cmd=%d len=%d", nsent, hdr.cmd, hdr.len);

#ifdef WITH_REL_TRACK
				hdr.cmd = DPS_SEARCHD_CMD_TRACKDATA;
				hdr.len = /*Res->CoordList.ncoords*/ Res->num_rows * sizeof(*Res->CoordList.Track);
				nsent = DpsSearchdSendPacket(server, &hdr, &Res->CoordList.Track[Res->first]);
				DpsLog(Agent, verb, "Sent TRACKDATA packet %u bytes cmd=%d len=%d", nsent, hdr.cmd, hdr.len);
#endif


#if HAVE_ASPELL
				if (Res->Suggest != NULL) {
				  hdr.cmd = DPS_SEARCHD_CMD_SUGGEST;
				  hdr.len = dps_strlen(Res->Suggest) + 1;
				  nsent = DpsSearchdSendPacket(server, &hdr, Res->Suggest);
				  DpsLog(Agent, verb, "Sent Suggest packet %d bytes cmd=%d len=%d", (int)nsent, hdr.cmd, hdr.len);
				}
#endif

				hdr.cmd=DPS_SEARCHD_CMD_WORDS;
				hdr.len = /*Res->CoordList.ncoords*/ Res->num_rows * sizeof(DPS_URL_CRD_DB);
				nsent=DpsSearchdSendPacket(server, &hdr, &Res->CoordList.Coords[Res->first]);
				DpsLog(Agent,verb,"Sent words packet %d bytes cmd=%d len=%d nwords=%d",(int)nsent,hdr.cmd,hdr.len,Res->num_rows/*Res->CoordList.ncoords*/);

#ifdef HAVE_ASPELL
				if (Agent->Flags.use_aspellext && Agent->naspell > 0) {
				  register size_t z;
/*#ifdef UNIONWAIT
				  union wait status;
#else
				  int status;
#endif*/
				  for (z = 0; z < Agent->naspell; z++) {
				    if (Agent->aspell_pid[z]) {
				      kill(Agent->aspell_pid[z], SIGALRM);
				      Agent->aspell_pid[z] = 0;
				    }
				  }
/*				  while(waitpid(Agent->aspell_pid, &status, WNOHANG) > 0); we catch it by signal */
				  Agent->naspell = 0;
				}
#endif /* HAVE_ASPELL */
				/*
				for(i=0;i<Agent->total_found;i++){
					dps_snprintf(buf,sizeof(buf)-1,"u=%08X c=%08X",wrd[i].url_id,wrd[i].coord);
					DpsLog(Agent,verb,"%s",buf);
				}
				*/
				break;
			case DPS_SEARCHD_CMD_GOODBYE:
				Res->work_time = DpsStartTimer() - ticks;
				DpsTrackSearchd(Agent, Res);
/*				DpsTrack(Agent, &Agent->Conf->Vars, Res);*/
				DpsLog(Agent,verb,"Received goodbye command. Work time: %.3f sec.", (float)Res->work_time / 1000);
				done=1;
				break;
			default:
				DpsLog(Agent,verb,"Unknown command %d",hdr.cmd);
				done=1;
				break;
		}
	}
	close(server);
	close(client);
	client=0;
	DpsLog(Agent,verb,"Quit");
	DpsResultFree(Res);
	DPS_FREE(words);
	return DPS_OK;
}

/*************************************************************/

static void usage(void){

	fprintf(stderr, "\nsearchd from %s-%s-%s\n(C)1998-2003, LavTech Corp.\n(C)2003-2011, DataPark Ltd.\n\n\
Usage: searchd [OPTIONS]\n\
\n\
Options are:\n\
  -l		do not log to stderr\n\
  -v n          verbose level, 0-5\n\
  -s n          sleep n seconds before starting\n\
  -w /path      choose alternative working /var directory\n\
  -f            run foreground, don't demonize\n\
"
#ifdef HAVE_PTHREAD
"  -U              use one connection to DB for all threads\n"
#endif
"\
  -h,-?         print this help page and exit\n\
\n\
\n", PACKAGE,VERSION,DPS_DBTYPE);

	return;
}

#define SEARCHD_EXIT    0
#define SEARCHD_RELOAD  1


static int client_main(DPS_ENV *Env, size_t handle) {
  struct timeval tval;
  int		ns;
  struct sockaddr_in client_addr;
  socklen_t	addrlen=sizeof(client_addr);
  char		addr[128]="";
  int		method;
  DPS_MATCH	*M;
  DPS_MATCH_PART	P[10];
  int mdone=0;
  int res=SEARCHD_EXIT;
  DPS_AGENT *Agent = DpsAgentInit(NULL, Env, 300 + (int)handle);

  MaxClients = DpsVarListFindInt(&Env->Vars, "MaxClients", 1);
  if (MaxClients > DPS_CHILDREN_LIMIT) MaxClients = DPS_CHILDREN_LIMIT;

  if (Agent == NULL) {
    DpsLog_noagent(Env, DPS_LOG_ERROR, "Can't alloc Agent at %s:%d", __FILE__, __LINE__);
    exit(1);
  }

  /* (re)setup signals here ?*/
  init_signals();

  while (!mdone) {
    fd_set msk;
    int sel;

    alarm(2400); /* 40 min. - maximum time of child execution */

    DpsVarListFree(&Agent->Vars);
    DpsVarListInit(&Agent->Vars);
    DpsVarListAddLst(&Agent->Vars, &Env->Vars, NULL, "*");
/*
    DpsVarListDel(&Agent->Vars, "q");
    DpsVarListDel(&Agent->Vars, "np");
    DpsVarListDel(&Agent->Vars, "p");
    DpsVarListDel(&Agent->Vars, "E");
    DpsVarListDel(&Agent->Vars, "CS");
    DpsVarListDel(&Agent->Vars, "CP");
    *Env->errstr = '\0';
*/
    tval.tv_sec = 2;
    tval.tv_usec = 0;

    DpsAcceptMutexLock(Agent);

    msk = mask;

    sel = select(FD_SETSIZE, &msk, 0, 0, &tval);

    if(have_sighup){
      DpsLog(Agent, DPS_LOG_WARN, "SIGHUP arrived");
      have_sighup = 0;
      res = SEARCHD_RELOAD;
      if (parent_pid > 0) kill(parent_pid, SIGHUP);
      mdone = 1;
    }
    if(have_sigint){
      DpsLog(Agent, DPS_LOG_ERROR, "SIGINT arrived");
      have_sigint = 0;
      mdone = 1;
    }
    if(have_sigterm){
      DpsLog(Agent, DPS_LOG_ERROR, "SIGTERM arrived");
      have_sigterm = 0;
      mdone = 1;
    }
    if(have_sigpipe){
      DpsLog(Agent, DPS_LOG_ERROR, "SIGPIPE arrived. Broken pipe!");
      have_sigpipe = 0;
      mdone = 1;
    }
    if (have_sigusr1) {
      DpsIncLogLevel(Agent);
      have_sigusr1 = 0;
    }
    if (have_sigusr2) {
      DpsDecLogLevel(Agent);
      have_sigusr2 = 0;
    }

    if(mdone) {
      DpsAcceptMutexUnlock(Agent);
      break;
    }

    if(sel == 0) {
      DpsAcceptMutexUnlock(Agent);
      continue;
    }
    if(sel == -1) {
      switch(errno){
      case EINTR:	/* Child */
	break;
      default:
	dps_strerror(Agent, DPS_LOG_WARN, "FIXME select error");
      }
      DpsAcceptMutexUnlock(Agent);
      continue;
    }

    if (FD_ISSET(sockfd, &msk)) {
      if ((ns = accept(sockfd, (struct sockaddr *) &client_addr, &addrlen)) == -1) {
	DpsAcceptMutexUnlock(Agent);
	dps_strerror(Agent, DPS_LOG_ERROR, "accept() error");
	DpsEnvFree(Agent->Conf);
	DpsAgentFree(Agent);
	exit(1);
      }
    } else if (FD_ISSET(clt_sock, &msk)) {
      if ((ns = accept(clt_sock, (struct sockaddr *) &client_addr, &addrlen)) == -1) {
	DpsAcceptMutexUnlock(Agent);
	dps_strerror(Agent, DPS_LOG_ERROR, "accept() error");
	DpsEnvFree(Agent->Conf);
	DpsAgentFree(Agent);
	exit(1);
      }
    } else {
      DpsAcceptMutexUnlock(Agent);
      continue;
    }
    DpsAcceptMutexUnlock(Agent);
    DpsLog(Agent, DPS_LOG_EXTRA, "Connect %s", inet_ntoa(client_addr.sin_addr));
    
    dps_snprintf(addr, sizeof(addr)-1, inet_ntoa(client_addr.sin_addr));
    M = DpsMatchListFind(&Agent->Conf->Filters,addr,10,P);
    method = M ? DpsMethod(M->arg) : DPS_METHOD_GET;
    DpsLog(Agent, DPS_LOG_EXTRA, "%s %s %s%s", M?M->arg:"",addr,M?M->pattern:"",M?"":"Allow by default");
			
    if(method == DPS_METHOD_DISALLOW) {
      DpsLog(Agent, DPS_LOG_ERROR, "Reject client");
      close(ns);
      continue;
    }

    do_client(Agent, ns);

    close(ns);

  }
  if (clt_sock > 0) dps_closesocket(clt_sock);
  if (sockfd > 0) dps_closesocket(sockfd);

  return res;
}




#define MAXMSG (4096 + sizeof(long))

static void SearchdTrack(DPS_AGENT *Agent) {
  DPS_SQLRES      sqlRes;
  DPS_DBLIST      dbl;
  DPS_DB          *db, *tr_db;
  DPS_DBLIST      *DBL, *TrL = NULL;
  DIR * dir;
  struct dirent * item;
  int fd, rec_id, res = DPS_OK;
  ssize_t n;
  char query[MAXMSG];
  char *lt;
  char qbuf[4096 + MAXMSG];
  char fullname[PATH_MAX]="";
  char *IP, *qwords, *qtime, *total_found, *wtime, *var, *val;
  size_t i, dbfrom = 0, dbto;
  const char      *TrackDBAddr = DpsVarListFindStr(&Agent->Vars, "TrackDBAddr", NULL);
  size_t to_sleep = 0, to_delete, trdone;

  init_TrackSignals();

  DpsSQLResInit(&sqlRes);
  if (TrackDBAddr) {
    TrL = &dbl;
    bzero((void*)TrL, sizeof(dbl));
    if(DPS_OK != DpsDBListAdd(TrL, TrackDBAddr, DPS_OPEN_MODE_WRITE)) {
      DpsLog(Agent,  DPS_LOG_ERROR, "Invalid TrackDBAddr: '%s'", TrackDBAddr);
      return;
    }
  }
  DBL = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl : &Agent->dbl;
  dbto = DBL->nitems;

  /* To see the URL being indexed in "ps" output on xBSD */
  dps_setproctitle("Query Tracker");

  for (i = dbfrom; i < dbto; i++) {
    db = DBL->db[i];
    db->connected = 0;
  }

  while(1) {
    if(have_sigint){
      DpsLog(Agent, DPS_LOG_ERROR, "Query Tracker: SIGINT arrived");
      have_sigint = 0;
      break;
    }
    if(have_sigterm){
      DpsLog(Agent, DPS_LOG_ERROR, "Query Tracker: SIGTERM arrived");
      have_sigterm = 0;
      break;
    }
    if(have_sigpipe){
      DpsLog(Agent, DPS_LOG_ERROR, "Query Tracker: SIGPIPE arrived. Broken pipe !");
      have_sigpipe = 0;
      break;
    }
    if (have_sigusr1) {
      DpsIncLogLevel(Agent);
      have_sigusr1 = 0;
    }
    if (have_sigusr2) {
      DpsDecLogLevel(Agent);
      have_sigusr2 = 0;
    }
    DpsLog(Agent, DPS_LOG_DEBUG, "Query Track: starting directory reading");
    trdone = 0;

    for (i = dbfrom; (i < dbto); i++) {
      db = DBL->db[i];
      tr_db = (TrackDBAddr!=NULL) ? TrL->db[0] : db;
      
/*      fprintf(stderr, " -- %d TrackDBAddr:%s  TrackQuery:%d\n", i, DPS_NULL2EMPTY(TrackDBAddr), db->TrackQuery);*/

#ifdef HAVE_SQL
      if(TrackDBAddr || db->TrackQuery) {
	const char      *qu = (tr_db->DBType == DPS_DB_PGSQL) ? "'" : "";
	const char      *vardir = (db->vardir != NULL) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);

	dir = opendir(vardir);
/*	fprintf(stderr, " -- opendir: %s\n", dir);*/
	if (dir) {
	  while((item = readdir(dir))) {
	    if (strncmp(item->d_name, "track.", 6) && strncmp(item->d_name, "query.", 6)) continue;
	    to_delete = 0;
	    dps_snprintf(fullname, sizeof(fullname), "%s%s%s", vardir, DPSSLASHSTR, item->d_name);
	    if ((fd = DpsOpen2(fullname, O_RDONLY | DPS_BINARY)) > 0) {
	      n = read(fd, query, sizeof(query) - 1);
	      DpsClose(fd);

	      if (n > 0) {
		query[n] = '\0';

		to_delete = 1;

		if (strncmp(item->d_name, "track.", 6)) {
		  DpsLog(Agent, DPS_LOG_EXTRA, "Query Track: track[%d]: %s", dps_strlen(query + sizeof(long)), query + sizeof(long) );

		  res = DpsSQLAsyncQuery(tr_db, NULL, query);
		  if (res != DPS_OK) {to_delete = 0; continue; }
		  trdone = 1;
		} else {
		  char *cmd_escaped;
		  char *text_escaped;
		  DpsLog(Agent, DPS_LOG_EXTRA, "Query Track: query[%d]: %s", dps_strlen(query), query);

		  trdone = 1;
		  IP = dps_strtok_r(query + sizeof(long), "\2", &lt, NULL);
/*	  DpsLog(Agent, DPS_LOG_EXTRA, "Query Track: IP: %s", IP);*/
		  qwords = dps_strtok_r(NULL, "\2", &lt, NULL);
/*	  DpsLog(Agent, DPS_LOG_EXTRA, "Query Track: qwords: %s", qwords);*/
		  qtime = dps_strtok_r(NULL, "\2", &lt, NULL);
/*	  DpsLog(Agent, DPS_LOG_EXTRA, "Query Track: qtime: %s", qtime);*/
		  total_found = dps_strtok_r(NULL, "\2", &lt, NULL); 
/*	  DpsLog(Agent, DPS_LOG_EXTRA, "Query Track: total: %s", total_found);*/
		  wtime = dps_strtok_r(NULL, "\2", &lt, NULL); 
/*	  DpsLog(Agent, DPS_LOG_EXTRA, "Query Track: wtime: %s", wtime);*/
      
		  dps_snprintf(qbuf, sizeof(qbuf), "INSERT INTO qtrack (ip,qwords,qtime,found,wtime) VALUES ('%s','%s',%s,%s,%s)",
			       IP, qwords, qtime, total_found, wtime );
 
		  DpsLog(Agent, DPS_LOG_EXTRA, "%s", qbuf);

		  res = DpsSQLAsyncQuery(tr_db, NULL, qbuf);
		  if (res != DPS_OK) {to_delete = tr_db->connected; goto tr_loop_continue; }

		  dps_snprintf(qbuf, sizeof(qbuf), "SELECT rec_id FROM qtrack WHERE ip='%s' AND qtime=%s", IP, qtime);
		  DpsLog(Agent, DPS_LOG_EXTRA, "%s", qbuf);
		  res = DpsSQLQuery(tr_db, &sqlRes, qbuf);
		  if (res != DPS_OK) { to_delete = 0; DpsSQLFree(&sqlRes); goto tr_loop_continue; }
		  if (DpsSQLNumRows(&sqlRes) == 0) { DpsSQLFree(&sqlRes); res = DPS_ERROR; goto tr_loop_continue; }
		  rec_id = DPS_ATOI(DpsSQLValue(&sqlRes, 0, 0));
		  DpsSQLFree(&sqlRes);

		  do {
		    var = dps_strtok_r(NULL, "\2", &lt, NULL);
		    if (var != NULL) {
		      val = dps_strtok_r(NULL, "\2", &lt, NULL); 
		      cmd_escaped = DpsDBEscStr(db, NULL, DPS_NULL2EMPTY(var), dps_strlen(DPS_NULL2EMPTY(var))); /* Escape parameter name */
		      text_escaped = DpsDBEscStr(db, NULL, DPS_NULL2EMPTY(val), dps_strlen(DPS_NULL2EMPTY(val))); /* Escape parameter value */
		      dps_snprintf(qbuf, sizeof(qbuf), "INSERT INTO qinfo (q_id,name,value) VALUES (%s%i%s,'%s','%s')", 
				   qu, rec_id, qu, cmd_escaped, text_escaped);
		      DpsLog(Agent, DPS_LOG_EXTRA, "%s", qbuf);
		      res = DpsSQLAsyncQuery(tr_db, NULL, qbuf);
		      DPS_FREE(text_escaped);
		      DPS_FREE(cmd_escaped);
		      if (res != DPS_OK) continue;
		    }
		  } while (var != NULL);
		}

	      }
	    tr_loop_continue:
	      if (to_delete) unlink(fullname);

	    }
	  }
	  closedir(dir);

	  if (trdone) {
	    to_sleep = 0;
	  }
	} else {
	  dps_strerror(Agent, DPS_LOG_ERROR, "Can't open directory: %s", vardir);
	}


      }
#endif
    }

    if (to_sleep < 3600) {
      to_sleep += 10;
    }
    DpsLog(Agent, DPS_LOG_DEBUG, "Query Track: sleeping %d", to_sleep);
    DPSSLEEP(to_sleep);

  }
}



int main(int argc, char **argv, char **envp) {
        struct sockaddr_in old_server_addr;
	dps_uint8 flags = DPS_FLAG_SPELL | DPS_FLAG_ADD_SERV;
	int done = 0, demonize = 1;
	int ch=0, pid_fd;
	int nport=DPS_SEARCHD_PORT;
	const char * config_name = DPS_CONF_DIR "/searchd.conf";
	int debug_level = DPS_LOG_INFO;
	char	pidbuf[64];
	const char	*var_dir = NULL, *pvar_dir = DPS_VAR_DIR;
	pid_t track_pid = 0;
	size_t i;

	log2stderr = 1;
	DpsInit(argc, argv, envp); /* Initialize library */

	DpsInitMutexes();
	parent_pid = getpid();

	while ((ch = getopt(argc, argv, "Ufhl?v:w:s:")) != -1){
		switch (ch) {
			case 'l':
				log2stderr=0;
				break;
		        case 'v': debug_level = atoi(optarg); break;
			case 'w':
				pvar_dir = optarg;
				break;
		        case 's':
			        sleep((unsigned int)atoi(optarg));
				break;
		        case 'f':
			        demonize = 0;
				break;
		        case 'U':
			        flags |= DPS_FLAG_UNOCON;
				break;
			case 'h':
			case '?':
			default:
				usage();
				return 1;
				break;
		}
	}
	argc -= optind;argv += optind;
	if(argc==1)config_name=argv[0];

	dps_snprintf(dps_pid_name, PATH_MAX, "%s%s%s", pvar_dir, DPSSLASHSTR, "searchd.pid");
	pid_fd = DpsOpen3(dps_pid_name, O_CREAT|O_EXCL|O_WRONLY, 0644);
	if(pid_fd < 0) {

	  dps_strerror(NULL, 0, "Can't create '%s'", dps_pid_name);
	  if(errno == EEXIST){
	      int pid = 0, kill_rc;
	    pid_fd = DpsOpen3(dps_pid_name, O_RDWR, 0644);
	    if (pid_fd < 0) {
	      dps_strerror(NULL, 0, "Can't open '%s'", dps_pid_name);
	      return DPS_ERROR;
	    }
	    (void)read(pid_fd, pidbuf, sizeof(pidbuf));
	    if (1 > sscanf(pidbuf, "%d", &pid)) {
	      dps_strerror(NULL, 0, "Can't read pid from '%s'", dps_pid_name);
	      close(pid_fd);
	      return DPS_ERROR;
	    }
	    kill_rc = kill((pid_t)pid, 0);
	    if (kill_rc == 0) {
	      fprintf(stderr, "It seems that another indexer is already running!\n");
	      fprintf(stderr, "Remove '%s' if it is not true.\n", dps_pid_name);
	      close(pid_fd);
	      return DPS_ERROR;
	    }
	    if (errno == EPERM) {
	      fprintf(stderr, "Can't check if another indexer is already running!\n");
	      fprintf(stderr, "Remove '%s' if it is not true.\n", dps_pid_name);
	      close(pid_fd);
	      return DPS_ERROR;
	    }
	    dps_strerror(NULL, 0, "Process %d seems to be dead. Flushing '%s'", pid, dps_pid_name);
	    lseek(pid_fd, 0L, SEEK_SET);
	    ftruncate(pid_fd, 0L);
	  }
	}

	if (demonize) {
	  if (dps_demonize() != 0) {
	    fprintf(stderr, "Can't demonize\n");
	    DpsClose(pid_fd);
	    exit(1);
	  }
	}
	sprintf(pidbuf, "%d\n", (int)getpid());
	(void)write(pid_fd, &pidbuf, dps_strlen(pidbuf));
	DpsClose(pid_fd);

	bzero(Children, DPS_CHILDREN_LIMIT * sizeof(DPS_CHILD));
	bzero(&old_server_addr, sizeof(old_server_addr));

	while(!done){
		struct sockaddr_in server_addr;
		struct sockaddr_un unix_addr;
		int on = 1;
		DPS_AGENT * Agent;
		DPS_ENV * Conf=NULL;
		int verb = DPS_LOG_EXTRA;
		int res = 0, internaldone = 0;
		const char *lstn, *unix_socket;

		if (track_pid != 0) {
		  kill(track_pid, SIGTERM);
		}

		Conf = DpsEnvInit(NULL);
		if (Conf == NULL) {
		  fprintf(stderr, "Can't alloc Env at %s:%d", __FILE__, __LINE__);
		  return DPS_ERROR;
		}
		Agent=DpsAgentInit(NULL,Conf,0);
		
		if (Agent == NULL) {
		  fprintf(stderr, "Can't alloc Agent at %s:%d", __FILE__, __LINE__);
		  return DPS_ERROR;
		}

		Agent->flags = Conf->flags = flags; /*DPS_FLAG_UNOCON | DPS_FLAG_ADD_SERV;*/

		res = DpsEnvLoad(Agent, config_name, flags);
		if ((NULL == DpsAgentDBLSet(Agent, Conf))) {
		  fprintf(stderr, "Can't set DBList at %s:%d", __FILE__, __LINE__);
		  DpsAgentFree(Agent);
		  DpsEnvFree(Conf);
		  unlink(dps_pid_name);
		  exit(2);
		}
		
		if (pvar_dir == NULL) var_dir = DpsVarListFindStr(&Conf->Vars, "VarDir", DPS_VAR_DIR);
		else var_dir = pvar_dir;

		DpsAcceptMutexInit(var_dir, "searchd");
		DpsUniRegCompileAll(Conf);

		DpsOpenLog("searchd", Conf, log2stderr);
		
		DpsSetLogLevel(Agent, debug_level);
		Agent->flags = Conf->flags = flags;
		Agent->Flags = Conf->Flags;

		if(res!=DPS_OK){
			DpsLog(Agent,verb,"%s",DpsEnvErrMsg(Conf));
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		}

		if (Conf->Flags.PreloadURLData) {
		  DpsLog(Agent, verb, "Preloading url data");
		  DpsURLDataPreload(Agent);
		}
		MaxClients = DpsVarListFindInt(&Agent->Conf->Vars, "MaxClients", 1);
		if (MaxClients > DPS_CHILDREN_LIMIT) MaxClients = DPS_CHILDREN_LIMIT;
		unix_socket = DpsVarListFindStr(&Agent->Conf->Vars, "Socket", NULL);
		DpsLog(Agent, verb, "searchd started with '%s'", config_name);
		DpsLog(Agent, verb, "VarDir: '%s'", DpsVarListFindStr(&Agent->Conf->Vars, "VarDir", DPS_VAR_DIR));
		DpsLog(Agent, verb, "MaxClients: %d", MaxClients);
		if (unix_socket != NULL) DpsLog(Agent, verb, "Unix socket: '%s'", unix_socket);
		DpsLog(Agent, verb, "Affixes: %d, Spells: %d, Synonyms: %d, Acronyms: %d, Stopwords: %d",
		       Conf->Affixes.naffixes,Conf->Spells.nspell,
		       Conf->Synonyms.nsynonyms,
		       Conf->Acronyms.nacronyms,
		       Conf->StopWords.nstopwords);
		DpsLog(Agent,verb,"Chinese dictionary with %d entries", Conf->Chi.nwords);
		DpsLog(Agent,verb,"Korean dictionary with %d entries", Conf->Korean.nwords);
		DpsLog(Agent,verb,"Thai dictionary with %d entries", Conf->Thai.nwords);

		for (i = 0; i < DPS_CHILDREN_LIMIT; i++) {
		  if (Children[i].pid) kill(Children[i].pid, SIGTERM);
/*		  Children[i].pid = 0;*/
		}

		if ((track_pid = fork() ) == -1) {
		  dps_strerror(Agent, DPS_LOG_ERROR, "fork() error");
		  DpsAgentFree(Agent);
		  DpsEnvFree(Conf);
		  unlink(dps_pid_name);
		  exit(1);
		}
		if (track_pid == 0) { /* child process */
		  DpsVarListAddLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");
		  SearchdTrack(Agent);
		  DpsAgentFree(Agent);
		  DpsEnvFree(Conf);
		  exit(0);
		}
		DpsLog(Agent,verb,"Query tracker child started.");

#if defined(HAVE_PTHREAD) && defined(HAVE_THR_SETCONCURRENCY_PROT)
		if (thr_setconcurrency(16) != NULL) { /* FIXME: Does 16 is enough ? */
		  DpsLog(A, DPS_LOG_ERROR, "Can't set %d concurrency threads", maxthreads);
		  return DPS_ERROR;
		}
#endif
		
		bzero((void*)&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		if((lstn=DpsVarListFindStr(&Agent->Conf->Vars,"Listen",NULL))){
			char * cport;
			
			if((cport=strchr(lstn, ':'))) {
				*cport = '\0';
				server_addr.sin_addr.s_addr = inet_addr(lstn);
				nport=atoi(cport+1);
			}else if (strchr(lstn, '.')) {
			        server_addr.sin_addr.s_addr = inet_addr(lstn);
			}else{
				nport = atoi(lstn);
				server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
			}
			DpsLog(Agent, verb, "Listening port %d", nport);
		}else{
			DpsLog(Agent,verb,"Listening port %d",nport);
			server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		server_addr.sin_port = htons((u_short)nport);

		for (i = 0; i < DPS_CHILDREN_LIMIT; i++) {
		  if (Children[i].pid) kill(Children[i].pid, SIGKILL);
/*		  Children[i].pid = 0;*/
		}

		FD_ZERO(&mask);
		
		if (memcmp(&old_server_addr, &server_addr, sizeof(server_addr)) != 0) {

		  if (clt_sock > 0) dps_closesocket(clt_sock);

		  if ((clt_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			DpsLog(Agent, DPS_LOG_ERROR, "socket() error %d", errno);
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		  }
		  
		  if (setsockopt(clt_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0){
			DpsLog(Agent, DPS_LOG_ERROR, "setsockopt() error %d", errno);
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		  }
#ifdef SO_REUSEPORT
		  if (setsockopt(clt_sock, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on)) != 0){
			dps_strerror(Agent, DPS_LOG_ERROR, "setsockopt() error");
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		  }
#endif
		  DpsSockOpt(Agent, clt_sock);

		  for (i = 0; ;i++) {

		    if (bind(clt_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		      dps_strerror(Agent, DPS_LOG_ERROR, "Can't bind: error");
		      if (i < 15) {
			DPSSLEEP(1 + i * 3);
			continue;
		      }
		      DpsAgentFree(Agent);
		      DpsEnvFree(Conf);
		      unlink(dps_pid_name);
		      exit(1);
		    }
		    break;
		  }

		  /* Backlog 64 is enough? */
		  if (listen(clt_sock, 64) == -1) {
			dps_strerror(Agent, DPS_LOG_ERROR, "listen() error");
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		  }
		  dps_memcpy(&old_server_addr, &server_addr, sizeof(server_addr));
		}
		FD_SET(clt_sock, &mask);

		if (unix_socket != NULL) {
		  char unix_path[128];
		  int saddrlen;
		  int old_umask;
		  if (DpsRelVarName(Conf, unix_path, sizeof(unix_path), unix_socket) < 105) {
		  } else {
		    DpsLog(Agent, DPS_LOG_ERROR, "Unix socket name '%s' is too large", unix_path);
		    DpsAgentFree(Agent);
		    DpsEnvFree(Conf);
		    unlink(dps_pid_name);
		    exit(1);
		  }
		  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
			DpsLog(Agent, DPS_LOG_ERROR, "unix socket() error %d", errno);
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		  }
		  DpsSockOpt(Agent, sockfd);

		  unlink(unix_path);
		  bzero((void*)&unix_addr, sizeof(unix_addr));
		  unix_addr.sun_family = AF_UNIX;
		  dps_strncpy(unix_addr.sun_path, unix_path, sizeof(unix_addr.sun_path));
		  saddrlen = sizeof(unix_addr.sun_family) + dps_strlen(unix_addr.sun_path);

		  old_umask = umask( ~((S_IRWXU | S_IRWXG | S_IRWXO) & 0777));

		  if (bind(sockfd, (struct sockaddr *)&unix_addr, saddrlen) == -1) {
			dps_strerror(Agent, DPS_LOG_ERROR, "Can't bind unix socket: error");
			umask(old_umask);
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		  }
		  umask(old_umask);
		  /* Backlog 64 is enough? */
		  if (listen(sockfd, 64) == -1) {
			dps_strerror(Agent, DPS_LOG_ERROR, "listen() unix socket error");
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			exit(1);
		  }
		  chmod(unix_path, S_IRWXU | S_IRWXG | S_IRWXO);
		  FD_SET(sockfd, &mask);
		  
		}
		
		
		DpsLog(Agent, DPS_LOG_WARN, "Ready");
		
		init_signals();

		if(DpsSearchCacheClean(Agent) != DPS_OK) {
			DpsLog(Agent, DPS_LOG_ERROR, "Error in search cache cleaning");
			DpsAgentFree(Agent);
			DpsEnvFree(Conf);
			unlink(dps_pid_name);
			DpsDestroyMutexes();
			exit(1);
		}

		for (i = 0; i < MaxClients; i++) {
		  pid_t pid;
		  if (Children[i].pid) continue;
/*		  if (Children[i].pid) kill(Children[i].pid, SIGTERM);*/
		  pid = fork();
		  if(pid == 0){
		    client_main(Agent->Conf, i);
		    DpsEnvFree(Agent->Conf);
		    DpsAgentFree(Agent);
		    exit(0);
		  } else {
		    if (pid > 0) {
		      Children[i].pid = pid;
		    } else {
		      Children[i].pid = 0;
		      DpsLog(Agent, DPS_LOG_ERROR, "fork() error %d", pid);
		    }
		  }
		}

		while(!internaldone) {
		  DPSSLEEP(1);
		  if(have_sighup){
			DpsLog(Agent,verb,"SIGHUP arrived");
			have_sighup=0;
			res = SEARCHD_RELOAD;
			internaldone = 1;
		  }
		  if(have_sigint){
			DpsLog(Agent,verb,"SIGINT arrived");
			have_sigint=0;
			res = SEARCHD_EXIT;
			internaldone = 1;
		  }
		  if(have_sigterm){
			DpsLog(Agent,verb,"SIGTERM arrived");
			have_sigterm=0;
			res = SEARCHD_EXIT;
			internaldone = 1;
		  }
		  if(have_sigpipe){ /* FIXME: why this may happens and what to do with it */
			DpsLog(Agent, verb, "SIGPIPE arrived. Broken pipe!");
			have_sigpipe = 0;
/*			mdone = 1;*/
		  }
		  if (have_sigusr1) {
		    DpsIncLogLevel(Agent);
		    have_sigusr1 = 0;
		  }
		  if (have_sigusr2) {
		    DpsDecLogLevel(Agent);
		    have_sigusr2 = 0;
		  }
		  for (i = 0; i < MaxClients; i++) {
		    if (Children[i].pid == 0) {
		      pid_t pid = fork();
		      if(pid == 0){
			client_main(Agent->Conf, i);
			DpsEnvFree(Agent->Conf);
			DpsAgentFree(Agent);
			exit(0);
		      } else {
			if (pid > 0) {
			  Children[i].pid = pid;
			} else {
			  Children[i].pid = 0;
			  DpsLog(Agent, DPS_LOG_ERROR, "fork() error %d", pid);
			}
		      }
		    }
		  }

		}

		if(res != SEARCHD_RELOAD){
			done = 1;
			DpsLog(Agent,verb,"Shutdown");
		}else{
			DpsLog(Agent,verb,"Reloading conf");
		}

		if (Conf->Flags.PreloadURLData) {
		  DpsURLDataDePreload(Agent);
		}
		DpsAgentFree(Agent);
		DpsEnvFree(Conf);
		DpsAcceptMutexCleanup();
	}
		
	for (i = 0; i < MaxClients; i++) {
	  if (Children[i].pid) kill(Children[i].pid, SIGTERM);
	}
	if (track_pid != 0) {
	  kill(track_pid, SIGTERM);
	}

	unlink(dps_pid_name);
	DpsDestroyMutexes();

#ifdef EFENCE
	fprintf(stderr, "Memory leaks checking\n");
	DpsEfenceCheckLeaks();
#endif
#ifdef FILENCE
	fprintf(stderr, "FD leaks checking\n");
	DpsFilenceCheckLeaks(NULL);
#endif

#ifdef BOEHMGC
	CHECK_LEAKS();
#endif
     
	return DPS_OK;
}
