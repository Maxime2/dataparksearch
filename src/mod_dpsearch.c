/* Copyright (C) 2004-2012 DataPark Ltd. All rights reserved.

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
#include "dpsearch.h"
#include "dps_xmalloc.h"
#include "dps_charsetutils.h"
#include <locale.h>
#include <strings.h>
#include "apr_strings.h"

#define DEBUG 0
/*#define DEBUG_MEM 1*/

#ifdef DEBUG_MEM
#include <sys/mman.h>
#endif

#define MOD_DPSEARCH_VERSION_INFO_STRING "mod_dpsearch/" VERSION
#define MAX_PS 100

#ifdef APACHE2
#define ap_kill_timeout(a)
#define ap_soft_timeout(a, r)
#endif


module 
#ifndef APACHE2
       MODULE_VAR_EXPORT 
#endif
                         dpsearch_module;

/* The storedoc content handler */
static int dpstoredoc_handler(request_rec *r) {
  DPS_AGENT *Agent = ap_get_module_config( r->server->module_config, &dpsearch_module );
  DPS_ENV *Env = Agent->Conf;
  DPS_RESULT	*Res;
  DPS_DOCUMENT	*Doc;
  DPS_VARLIST	query_vars;
  DPS_HTMLTOK	tag;
/*  DPS_CHARSET	*lcs = NULL, *bcs = NULL;*/
  DPS_CHARSET	*sys_int;
  DPS_CONV	lc_uni, uni_bc;
  DPS_CONV	lc_uni_text, uni_bc_text;
  const char *old_tmplt, *tmplt, *env;
  const char *bcharset, *lcharset, *htok;
  char          *last;
  char		*HDoc = NULL;
  char		*HEnd = NULL;
  char          ch;
  const char            *content_type;
#ifdef WITH_PARSER
  DPS_PARSER	*Parser;
#endif

  if (/*strcmp(r->handler, STATUS_MAGIC_TYPE) && */
      strcasecmp(DPS_NULL2EMPTY(r->handler), "dpstoredoc")) {
    return DECLINED;
  }

#ifdef APACHE2
  r->allowed |= (AP_METHOD_BIT << M_GET);
#else
  r->allowed |= (1 << M_GET);
#endif
  if (r->method_number == M_OPTIONS) {
    return DECLINED;
  }

  if (r->header_only) return OK;

  Agent->request = r;

  DpsVarListInit(&query_vars);
/*  DpsVarListInit(&Env_Vars);*/
  DpsVarListFree(&Agent->Vars);
  DpsVarListInit(&Agent->Vars);
  DpsVarListAddLst(&Agent->Vars, &Env->Vars, NULL, "*");

  DpsVarListDel(&Agent->Vars, "q");
  DpsVarListDel(&Agent->Vars, "np");
  DpsVarListDel(&Agent->Vars, "p");
  DpsVarListDel(&Agent->Vars, "E");
  DpsVarListDel(&Agent->Vars, "CS");
  DpsVarListDel(&Agent->Vars, "CP");
  *Env->errstr = '\0';

  ap_soft_timeout("mod_dpsearch", r);

  env = DPS_NULL2EMPTY(r->path_info);
  if (*env) {
    DpsVarListReplaceStr(&Agent->Vars, "st_tmplt", env + 1);
  }
  old_tmplt = (char*)DpsStrdup(DpsVarListFindStr(&Agent->Vars, "st_tmplt", " "/*"storedoc.htm"*/));
  DpsParseQueryString(Agent, &Agent->Vars, DPS_NULL2EMPTY(r->args));
#ifdef APACHE2
  env = apr_table_get(r->subprocess_env, "CHARSET_SAVED_QUERY_STRING");
#else
  env = ap_table_get(r->subprocess_env, "CHARSET_SAVED_QUERY_STRING");
#endif

  DpsParseQStringUnescaped(&query_vars, env ? env : DPS_NULL2EMPTY(r->args));

  tmplt = (char*)DpsStrdup(DpsVarListFindStr(&Agent->Vars, "st_tmplt", ""));

  if (strcasecmp(old_tmplt, tmplt)) {
    char template_name[PATH_MAX];
    const char *conf_dir;

    DpsTemplateFree(&Agent->st_tmpl);
    Agent->st_tmpl.Env_Vars = &Agent->Conf->Vars;
    
    if (!(conf_dir=getenv("DPS_ETC_DIR"))) conf_dir = DPS_CONF_DIR;
    dps_snprintf(template_name, sizeof(template_name), "%s/%s", conf_dir, tmplt);
    
    DpsURLNormalizePath(template_name);
	
    if ( strncmp(template_name, conf_dir, dps_strlen(conf_dir))  
	 || (DPS_OK != DpsTemplateLoad(Agent, Agent->Conf, &Agent->st_tmpl, template_name)) ) {

      DpsLog(Agent, DPS_LOG_ERROR, "Can't load stemplate '%s'", tmplt);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);
      return 
#ifdef APACHE1
		   SERVER_ERROR;
#else
		   HTTP_INTERNAL_SERVER_ERROR;
#endif
    }
  }
  
  /* Call again to load search Limits if need */
  DpsParseQueryString(Agent, &Agent->Vars, DPS_NULL2EMPTY(r->args));
  Agent->Flags = Agent->Conf->Flags;
  Agent->flags = Agent->Conf->flags |= DPS_FLAG_SPELL | DPS_FLAG_UNOCON;
  Agent->WordParam = Agent->Conf->WordParam;
/*  DpsVarListReplaceLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");*/
/*  DpsVarListReplaceLst(&Agent->Vars, &Env_Vars, NULL, "*");*/

  DpsOpenLog("mod_dpsearch", Agent->Conf, 1);
  DpsSetLogLevel(Agent, DpsVarListFindInt(&Agent->Vars, "LogLevel", 0)  );

#ifdef DEBUG

  DpsLog(Agent, DPS_LOG_DEBUG, "SetLogLevel: %d", DpsVarListFindInt(&Agent->Vars, "LogLevel", 0));

#endif

  /* set locale if specified */
  if ((last = DpsVarListFindStr(&Agent->Vars, "Locale", NULL)) != NULL) {
    setlocale(LC_COLLATE, last);
    setlocale(LC_CTYPE, last);
    setlocale(LC_ALL, last);
#ifdef HAVE_ASPELL
    { char *p;
      if ((p = strchr(last, '.')) != NULL) {
	*p = '\0';
	DpsVarListReplaceStr(&Agent->Vars, "g-lc", last);
	*p = '.';
      }
    }
#endif
  }
  /* set TZ if specified */
  if ((last = DpsVarListFindStr(&Env->Vars, "TZ", NULL)) != NULL) {
    setenv("TZ", last, 1);
    tzset();
  }

  DpsVarListAddEnviron(&Agent->Vars, "ENV");
  /* This is for query tracking */
  DpsVarListReplaceStr(&Agent->Vars, "QUERY_STRING", DPS_NULL2EMPTY(r->args));
  DpsVarListReplaceStr(&Agent->Vars, "self", r->uri);
  DpsVarListReplaceStr(&Agent->Vars, "IP", r->connection->remote_ip);

  bcharset = DpsVarListFindStr(&Agent->Vars, "BrowserCharset", "iso-8859-1");
  Env->bcs = DpsGetCharSet(bcharset);
  lcharset = DpsVarListFindStr(&Agent->Vars, "LocalCharset", "iso-8859-1");
  Env->lcs = DpsGetCharSet(lcharset);
  
  Agent->st_tmpl.Env_Vars = &Agent->Vars;

  {
    char ConT[PATH_MAX];
    dps_snprintf(ConT, sizeof(ConT), "%s; charset=%s", DpsVarListFindStr(&Agent->Vars, "ResultContentType", "text/html"), bcharset);

    r->content_type = (char*)
#ifdef APACHE1
		      ap_pstrdup
#else
		      apr_pstrdup
#endif
		      (r->pool, ConT);
#ifdef APACHE1
    ap_send_http_header(r);
#endif
  }

  if (!Env->bcs) {

      DpsLog(Agent, DPS_LOG_ERROR, "Unknown BrowserCharset '%s' in template '%s'", bcharset, tmplt);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);

      return 
#ifdef APACHE1
		   SERVER_ERROR;
#else
		   HTTP_INTERNAL_SERVER_ERROR;
#endif
  }
  if (!Env->lcs) {

      DpsLog(Agent, DPS_LOG_ERROR, "Unknown LocalCharset '%s' in template '%s'", bcharset, tmplt);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);

      return 
#ifdef APACHE1
		   SERVER_ERROR;
#else
		   HTTP_INTERNAL_SERVER_ERROR;
#endif
  }
	
	
	/* Now start displaying template*/
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->st_tmpl, "top");
	
	/* UnStore Doc, Highlight and Display */
	
	Doc=DpsDocInit(NULL);
	Res=DpsResultInit(NULL);
	if (Res == NULL) {

	  DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc Result");

          DPS_FREE(old_tmplt);
	  DPS_FREE(tmplt);
	  return 
#ifdef APACHE1
		   SERVER_ERROR;
#else
		   HTTP_INTERNAL_SERVER_ERROR;
#endif

	}
	
	DpsPrepare(Agent, Res);
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
#endif /* HAVE_ASPELL */
	DpsWWLBoolItems(Res);
	DpsVarListReplaceStr(&Doc->Sections, "URL_ID", DpsVarListFindStr(&Agent->Vars, "rec_id", "0"));
	
	content_type = DpsVarListFindStr(&Env->Vars, "L", NULL);
	if (content_type != NULL) DpsVarListReplaceStr(&Env->Vars, "Content-Language", content_type);
	content_type = DpsVarListFindStr(&Env->Vars, "CS", NULL);
	if (content_type != NULL) DpsVarListReplaceStr(&Env->Vars, "Charset", content_type);
	content_type = DpsVarListFindStr(&Agent->Vars, "CT", "text/html");

	DpsAgentStoredConnect(Agent);
	
	DpsUnStoreDoc(Agent, Doc, NULL);
	Res->num_rows = 1;
	Res->Doc = Doc;

	if(!Doc->Buf.content) {
	  char *origurl = DpsVarListFindStr(&Env->Vars, "DU", NULL);
	  if (origurl != NULL) {
	    DpsVarListReplaceStr(&Doc->Sections, "URL", origurl);
	    Doc->Buf.max_size = (size_t)DpsVarListFindInt(&Agent->Vars, "MaxDocSize", DPS_MAXDOCSIZE);
	    DpsURLParse(&Doc->CurURL, origurl);
	    DpsDocAddDocExtraHeaders(Agent, Doc);
	    DpsDocLookupConn(Agent, Doc);
	    if (DpsGetURL(Agent, Doc, origurl) != DPS_OK) goto fin;
	    {
	      const char   *ce = DpsVarListFindStr(&Doc->Sections,"Content-Encoding","");
	      const int status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
#ifdef HAVE_ZLIB
	      if((!strcasecmp(ce, "gzip")) || (!strcasecmp(ce, "x-gzip"))) {
		if (status == 206) {
		  DpsLog(Agent, DPS_LOG_INFO, "Partial content, can't ungzip it.");
		  goto fin;
		}
		DpsUnGzip(Agent, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", 
				     Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size + DpsVarListFindInt(&Doc->Sections, "Content-Length", 0));
	      } else if(!strcasecmp(ce,"deflate")){
		if (status == 206) {
		  DpsLog(Agent, DPS_LOG_INFO, "Partial content, can't inflate it.");
		  goto fin;
		}
		DpsInflate(Agent, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", 
				     Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size + DpsVarListFindInt(&Doc->Sections, "Content-Length", 0));
	      } else if((!strcasecmp(ce, "compress")) || (!strcasecmp(ce, "x-compress"))) {
		if (status == 206) {
		  DpsLog(Agent, DPS_LOG_INFO, "Partial content, can't uncomress it.");
		  goto fin;
		}
		DpsUncompress(Agent, Doc);
		DpsVarListReplaceInt(&Doc->Sections, "Content-Length", 
				     Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size + DpsVarListFindInt(&Doc->Sections, "Content-Length", 0));
	      }else 
#endif	
	      if((!strcasecmp(ce,"identity")) || (!strcasecmp(ce,""))) {
		/* Nothing to do*/
	      }else{
		DpsLog(Agent, DPS_LOG_ERROR, "Unsupported Content-Encoding");
	      }

#ifdef WITH_PARSER
	      /* Let's try to start external parser for this Content-Type */

	      if((Parser = DpsParserFind(&Agent->Conf->Parsers, content_type))) {
		DpsLog(Agent, DPS_LOG_DEBUG, "Found external parser '%s' -> '%s'", Parser->from_mime ? Parser->from_mime : "NULL", Parser->to_mime ? Parser->to_mime : "NULL");
	      }
	      if(Parser) {
		if (status == DPS_HTTP_STATUS_OK) {
		  if (DpsParserExec(Agent, Parser, Doc)) {
		    char *to_charset;
		    content_type = Parser->to_mime ? Parser->to_mime : "unknown";
		    DpsLog(Agent, DPS_LOG_DEBUG, "Parser-Content-Type: %s", content_type);
		    if((to_charset = strstr(content_type, "charset="))){
		      const char *cs = DpsCharsetCanonicalName(DpsTrim(to_charset + 8, " \t;\"'"));
		      DpsVarListReplaceStr(&Doc->Sections, "Server-Charset", cs);
		      DpsLog(Agent, DPS_LOG_DEBUG, "to_charset='%s'", cs);
		    }
#ifdef DEBUG_PARSER
		    fprintf(stderr, "content='%s'\n", Doc->content);
#endif
		  }
		} else {
		  DpsLog(Agent, DPS_LOG_WARN, "Parser is not executed, document status: %d", status);
		  goto fin;
		}
	      }
#endif
	    }
	  }
	  else goto fin;
	}
	DpsConvert(Env, &Agent->Vars, Res, Env->lcs, Env->bcs);
	DpsVarListReplaceLst(&Env->Vars, &Doc->Sections, NULL, "*");
	
	if (*DpsVarListFindStr(&Env->Vars, "HlBeg", "") == '\0') {
	  DpsVarListAddStr(&Agent->Vars, "document", Doc->Buf.content);
	} else {

	  register char *tp = NULL;

	  HEnd = HDoc = (char*)DpsMalloc(DPS_MAXDOCSIZE + 32);
	  if (HDoc == NULL) goto fin;
	  *HEnd = '\0';
	
	  if (strncasecmp(content_type, "text/", 5) == 0 && strncasecmp(content_type, "text/html", 9) != 0 && strncasecmp(content_type, "text/xml", 8) != 0) {
	    sprintf(HEnd, "<pre>\n");
	    HEnd += dps_strlen(HEnd);
	  }

	  sys_int = DpsGetCharSet("sys-int");
	  DpsConvInit(&lc_uni, Env->lcs, sys_int, Env->CharsToEscape, DPS_RECODE_HTML);
	  DpsConvInit(&uni_bc, sys_int, Env->bcs, Env->CharsToEscape, DPS_RECODE_HTML);
	  DpsConvInit(&lc_uni_text, Env->lcs, sys_int, Env->CharsToEscape, DPS_RECODE_TEXT);
	  DpsConvInit(&uni_bc_text, sys_int, Env->bcs, Env->CharsToEscape, DPS_RECODE_TEXT);

	  DpsHTMLTOKInit(&tag);
  
	  for(htok = DpsHTMLToken(Doc->Buf.content, (const char **)&last, &tag) ; htok ;) {
		switch(tag.type) {
		case DPS_HTML_COM:
		case DPS_HTML_TAG:
		  /*			dps_memmove(HEnd, htok, (size_t)(last - htok));
			HEnd+=last-htok;
			HEnd[0]='\0';
			DpsHTMLParseTag(Agent, &tag, Doc);*/
			ch = *last; *last = '\0';
			sprintf(HEnd, "%s", tp = DpsHlConvert(NULL, htok, &lc_uni_text, &uni_bc_text, 0)); /* FIXME: add check for Content-Language */
			DPS_FREE(tp);
			HEnd=DPS_STREND(HEnd);
			*last = ch;
			break;
		case DPS_HTML_TXT:
		        ch = *last; *last = '\0';
			if (tag.title || tag.script) {
			  sprintf(HEnd, "%s", tp = DpsHlConvert(NULL, htok, &lc_uni_text, &uni_bc_text, 0)); /* FIXME: add check for Content-Language */
			} else {
			  sprintf(HEnd, "%s", tp = DpsHlConvert(&Res->WWList, htok, &lc_uni, &uni_bc, 0)); /* FIXME: add check for Content-Language */
			}
			DPS_FREE(tp);
			HEnd=DPS_STREND(HEnd);
			*last = ch;
			break;
		}
		htok = DpsHTMLToken(NULL, (const char **)&last, &tag);
	  }
	
	  if (strncasecmp(content_type, "text/", 5) == 0 && strncasecmp(content_type, "text/html", 9) != 0 && strncasecmp(content_type, "text/xml", 8) != 0) {
	    sprintf(HEnd, "</pre>\n");
	    HEnd += dps_strlen(HEnd);
	  }

	  DpsVarListReplaceStr(&Agent->Vars, "document", HDoc);
	}
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->st_tmpl, "result");
	
fin:
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->st_tmpl, "bottom");

	Res->Doc = NULL;
	Res->num_rows = 0;
	DpsResultFree(Res);
	DpsDocFree(Doc);
	
	DPS_FREE(HDoc);

	DpsVarListFree(&query_vars);
	DpsVarListFree(&Agent->Vars);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);

  ap_rflush(r);
  ap_kill_timeout(r);

  return OK;

}


/* The search content handler */
static int dpsearch_handler(request_rec *r) {
  DPS_AGENT *Agent = ap_get_module_config( r->server->module_config, &dpsearch_module );
  DPS_ENV *Env = Agent->Conf;
  DPS_RESULT	*Res;
  DPS_VARLIST	query_vars;
  const char *old_tmplt, *tmplt, *env;
  const char *bcharset, *lcharset;
  char		*nav = NULL;
  char		*url = NULL;
  char		*searchwords=NULL;
  char		*storedstr=NULL;
  const char      *ResultContentType;
  const char      *StoredocURL, *StoredocTmplt;
  size_t        catcolumns = 0;
  ssize_t	page1,page2,npages,ppp=10;
  int		res, page_size, page_number, have_p = 1;
  size_t	i, swlen = 0, nav_len, storedlen;
#ifdef WITH_GOOGLEGRP
  int           site_id, prev_site_id = 0;
#endif

#if defined(WITH_TRACE) && defined(DEBUG)

  fprintf(Agent->TR, "dpsearch_handler: handler: |%s|\n", DPS_NULL2EMPTY(r->handler));

  /*if (r->handler == NULL)*/ {
    request_rec *p;

    for(p = r->main; p != NULL; p = p->next) {
      fprintf(Agent->TR, "dpsearch_handler: p: %x handler: |%s|  unparsed uri: |%s|\n", p, DPS_NULL2EMPTY(p->handler), p->unparsed_uri );
    }
  }


  fflush(Agent->TR);
 

#endif

  if (strcmp(DPS_NULL2EMPTY(r->handler), "dpsearch")) {
    return DECLINED;
  }

#ifdef APACHE2
  r->allowed |= (AP_METHOD_BIT << M_GET);
#else
  r->allowed |= (1 << M_GET);
#endif
  if (r->method_number == M_OPTIONS) {
    return DECLINED;
  }

  if (r->header_only) return OK;

  Agent->request = r;

#if defined(WITH_TRACE) && defined(DEBUG)

  fprintf(Agent->TR, "dpsearch_handler: Agent %x  Conf %x\n", Agent, Env);
  fflush(Agent->TR);

#endif


  DpsVarListInit(&query_vars);
/*  DpsVarListInit(&Env_Vars);*/
  DpsVarListFree(&Agent->Vars);
  DpsVarListInit(&Agent->Vars);
  DpsVarListAddLst(&Agent->Vars, &Env->Vars, NULL, "*");
/*  DpsVarListDel(&Env->Vars, "np");
  DpsVarListDel(&Env->Vars, "E");
  DpsVarListDel(&Env->Vars, "CS");
  DpsVarListDel(&Env->Vars, "CP");*/

  DpsVarListDel(&Agent->Vars, "q");
  DpsVarListDel(&Agent->Vars, "np");
  DpsVarListDel(&Agent->Vars, "p");
  DpsVarListDel(&Agent->Vars, "E");
  DpsVarListDel(&Agent->Vars, "CS");
  DpsVarListDel(&Agent->Vars, "CP");
  *Env->errstr = '\0';

  ap_soft_timeout("mod_dpsearch", r);

  env = DPS_NULL2EMPTY(r->path_info);
  if (*env) {
    DpsVarListReplaceStr(&Agent->Vars, "tmplt", env + 1);
  }
  old_tmplt = (char*)DpsStrdup(DpsVarListFindStr(&Agent->Vars, "tmplt", " "/*"search.htm"*/));
  DpsParseQueryString(Agent, &Agent->Vars, DPS_NULL2EMPTY(r->args));
#ifdef APACHE2
  env = apr_table_get(r->subprocess_env, "CHARSET_SAVED_QUERY_STRING");
#else
  env = ap_table_get(r->subprocess_env, "CHARSET_SAVED_QUERY_STRING");
#endif
	       
  DpsParseQStringUnescaped(&query_vars, env ? env : DPS_NULL2EMPTY(r->args));

#ifdef DEBUG

  DpsLog(Agent, DPS_LOG_DEBUG, "dpsearch_handler: %s  ", env ? env : DPS_NULL2EMPTY(r->args) );

#endif

  tmplt = (char*)DpsStrdup(DpsVarListFindStr(&Agent->Vars, "tmplt", ""));


#ifdef DEBUG

  DpsLog(Agent, DPS_LOG_DEBUG, "dpsearch_handler: old_tmplt: %s  tmplt: %s", old_tmplt, tmplt );

#endif

  if (strcasecmp(old_tmplt, tmplt)) {
    char template_name[PATH_MAX];
    const char *conf_dir;

    DpsTemplateFree(&Agent->tmpl);
    Agent->tmpl.Env_Vars = &Agent->Conf->Vars;
    
    if (!(conf_dir=getenv("DPS_ETC_DIR"))) conf_dir = DPS_CONF_DIR;
    dps_snprintf(template_name, sizeof(template_name), "%s/%s", conf_dir, tmplt);
    
    DpsURLNormalizePath(template_name);
	
    if ( strncmp(template_name, conf_dir, dps_strlen(conf_dir))
	 || (DPS_OK != DpsTemplateLoad(Agent, Agent->Conf, &Agent->tmpl, template_name)) ) {

      DpsLog(Agent, DPS_LOG_ERROR, "Can't load stemplate '%s'", tmplt);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);
      return 
#ifdef APACHE1
		   SERVER_ERROR;
#else
		   HTTP_INTERNAL_SERVER_ERROR;
#endif
    }
  }
  
  /* Call again to load search Limits if need */
  DpsParseQueryString(Agent, &Agent->Vars, DPS_NULL2EMPTY(r->args));
  Agent->Flags = Agent->Conf->Flags;
  Agent->flags = Agent->Conf->flags |= DPS_FLAG_SPELL | DPS_FLAG_UNOCON;
  Agent->WordParam = Agent->Conf->WordParam;
/*  DpsVarListReplaceLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");*/
/*  DpsVarListReplaceLst(&Agent->Vars, &Env_Vars, NULL, "*");*/

  Agent->tmpl.Env_Vars = &Agent->Vars;

  DpsOpenLog("mod_dpsearch", Agent->Conf, 1);
  DpsSetLogLevel(Agent, DpsVarListFindInt(&Agent->Vars, "LogLevel", 0)  );

#ifdef DEBUG

  DpsLog(Agent, DPS_LOG_DEBUG, "SetLogLevel: %d", DpsVarListFindInt(&Agent->Vars, "LogLevel", 0));

#endif

  /* set locale if specified */
  if ((url = DpsVarListFindStr(&Agent->Vars, "Locale", NULL)) != NULL) {
    setlocale(LC_COLLATE, url);
    setlocale(LC_CTYPE, url);
    setlocale(LC_ALL, url);
/*#ifdef HAVE_ASPELL*/
    { char *p;
      if ((p = strchr(url, '.')) != NULL) {
	*p = '\0';
	DpsVarListReplaceStr(&Agent->Vars, "g-lc", url);
	*p = '.';
      }
    }
/*#endif*/
    url = NULL;
  }
  /* set TZ if specified */
  if ((url = DpsVarListFindStr(&Env->Vars, "TZ", NULL)) != NULL) {
    setenv("TZ", url, 1);
    tzset();
    url = NULL;
  }

  DpsVarListAddEnviron(&Agent->Vars, "ENV");
  /* This is for query tracking */
  DpsVarListReplaceStr(&Agent->Vars, "QUERY_STRING", DPS_NULL2EMPTY(r->args));
  DpsVarListReplaceStr(&Agent->Vars, "self", r->uri);
  DpsVarListReplaceStr(&Agent->Vars, "IP", r->connection->remote_ip);

  bcharset = DpsVarListFindStr(&Agent->Vars, "BrowserCharset", "iso-8859-1");
  Env->bcs = DpsGetCharSet(bcharset);
  lcharset = DpsVarListFindStr(&Agent->Vars, "LocalCharset", "iso-8859-1");
  Env->lcs = DpsGetCharSet(lcharset);
  DpsLog(Agent, DPS_LOG_DEBUG, "LocalCharset: '%s'  BrowserCharset '%s' in template '%s'", lcharset, bcharset, tmplt);
  
  ppp = DpsVarListFindInt(&Agent->Vars, "PagesPerScreen", 10);

  {
    char ConT[PATH_MAX];
    dps_snprintf(ConT, sizeof(ConT), "%s; charset=%s", DpsVarListFindStr(&Agent->Vars, "ResultContentType", "text/html"), bcharset);

    r->content_type =  (char*)
#ifdef APACHE1
                       ap_pstrdup
#else
		       apr_pstrdup
#endif
                       (r->pool, ConT);
#ifdef APACHE1
    ap_send_http_header(r);
#endif
  }

  if (!Env->bcs) {

      DpsLog(Agent, DPS_LOG_ERROR, "Unknown BrowserCharset '%s' in template '%s'", bcharset, tmplt);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);

      return 
#ifdef APACHE1
		   SERVER_ERROR;
#else
		   HTTP_INTERNAL_SERVER_ERROR;
#endif
  }
  if (!Env->lcs) {

      DpsLog(Agent, DPS_LOG_ERROR, "Unknown LocalCharset '%s' in template '%s'", lcharset, tmplt);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);

      return 
#ifdef APACHE1
		   SERVER_ERROR;
#else
		   HTTP_INTERNAL_SERVER_ERROR;
#endif
  }

  /* These parameters taken from "variable section of template"*/
  
  res         = DpsVarListFindInt(&Agent->Vars, "ps", DPS_DEFAULT_PS);
  page_size   = dps_min(res, MAX_PS);
  page_number = DpsVarListFindInt(&Agent->Vars, "p", 0);
  if (page_number == 0) {
    page_number = DpsVarListFindInt(&Agent->Vars, "np", 0);
    DpsVarListReplaceInt(&Agent->Vars, "p", page_number + 1);
    have_p = 0;
  } else page_number--;
  res = DpsVarListFindInt(&Agent->Vars, "np", 0) * page_size;
  DpsVarListReplaceInt(&Agent->Vars, "pn", res);
	
	catcolumns = (size_t)atoi(DpsVarListFindStr(&Agent->Vars, "CatColumns", ""));


#if 0	
	if(catcolumns){
		DPS_CATEGORY C;
		
		bzero((void*)&C, sizeof(C));
		dps_strncpy(C.addr, DpsVarListFindStr(&Agent->Vars, "c", "0"), sizeof(C.addr));
		if(DPS_OK == DpsCatAction(Agent, &C, DPS_CAT_ACTION_LIST)){
			size_t n=1, l = 0;
			char *catlist = NULL;
			
			for(i = 0; i < C.ncategories; i++) l += 128 + dps_strlen(C.Category[i].path) + dps_strlen(C.Category[i].name);
			if (l > 0) catlist = (char*)DpsMalloc(l + 1);
			if (catlist != NULL) {
			  sprintf(catlist, "<table>\n");
			  for(i = 0; i < C.ncategories; i++){
				if(n==1){
					sprintf(catlist+dps_strlen(catlist),"<tr>\n");
				}
				sprintf(catlist+dps_strlen(catlist),"<td><a href=\"?cat=%s\">%s</A></td><td width=60>&nbsp;</td>\n",
						C.Category[i].path,
						C.Category[i].name);
				if(n==catcolumns){
					sprintf(catlist+dps_strlen(catlist),"</tr>\n");
					n=1;
				}else{
					n++;
				}
			  }
			  sprintf(catlist + dps_strlen(catlist), "</table>\n");
			  DpsVarListReplaceStr(&Agent->Vars, "CS", catlist);
			  DPS_FREE(catlist);
			}
		}else{
			DpsVarListReplaceStr(&Agent->Vars, "E", DpsEnvErrMsg(Agent->Conf));
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "top");
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "error");
			goto end;
		}

		DPS_FREE(C.Category);
		bzero((void*)&C, sizeof(C));
		dps_strcpy(C.addr,DpsVarListFindStr(&Agent->Vars, "c", "0"));
		if(DPS_OK == DpsCatAction(Agent, &C, DPS_CAT_ACTION_PATH)){
			char *catpath = NULL;
			size_t l = 0;
			
			for(i = 0; i < C.ncategories; i++) l += 32 + dps_strlen(C.Category[i].path) + dps_strlen(C.Category[i].name);
			catpath = (char*)DpsMalloc(l + 1);
			if (catpath != NULL) {
			  catpath[0] = '\0';
			  for(i = 0; i < C.ncategories; i++){
				sprintf(catpath+dps_strlen(catpath),"/<a href=\"?cat=%s\">%s</A>",
					(C.Category[i].path) ? C.Category[i].path : "",
					(C.Category[i].name) ? C.Category[i].name : "");
			  }
			  DpsVarListReplaceStr(&Agent->Vars, "CP", catpath);
			  DPS_FREE(catpath);
			}
		}else{
			DpsVarListReplaceStr(&Agent->Vars, "E", DpsEnvErrMsg(Agent->Conf));
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "top");
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "error");
			goto end;
		}
		DPS_FREE(C.Category);
	}
#endif

	if(NULL==(Res=DpsFind(Agent))) {
		DpsVarListReplaceStr(&Agent->Vars, "E", DpsEnvErrMsg(Agent->Conf));
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "top");
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "error");
		if (Res != NULL) goto freeres;
		goto end;
	}
	
	DpsVarListReplaceInt(&Agent->Vars,"first",(int)Res->first);
	DpsVarListReplaceInt(&Agent->Vars,"last",(int)Res->last);
	DpsVarListReplaceInt(&Agent->Vars,"total",(int)Res->total_found);
	DpsVarListReplaceInt(&Agent->Vars,"grand_total",(int)Res->grand_total);

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

	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "top");
	
	if((Res->WWList.nwords == 0) && (Res->nitems - Res->ncmds == 0) && (Res->num_rows == 0)) {
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "noquery");
		goto freeres;
	}
	
	if(Res->num_rows == 0){
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "notfound");
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
	  DPS_FREE(old_tmplt);
	  DPS_FREE(tmplt);

	  DpsLog(Agent, DPS_LOG_ERROR, "Can't realloc storedstr");

	  return 
#ifdef APACHE1
		       SERVER_ERROR;
#else
		       HTTP_INTERNAL_SERVER_ERROR;
#endif
	}
	
	npages=(Res->total_found/(page_size?page_size:20)) + ((Res->total_found % (page_size?page_size:20) != 0 ) ?  1 : 0);
	page1=page_number-ppp/2;
	page2=page_number+ppp/2;
	if(page1<0){
		page2-=page1;
		page1=0;
	}else
	if(page2>npages){
		page1-=(page2-npages);
		page2=npages;
	}
	if(page1<0)page1=page1=0;
	if(page2>npages)page2=npages;
	nav = (char *)DpsRealloc(nav, nav_len = (size_t)(page2 - page1 + 2) * (1024 + 1024)); 
	                                                    /* !!! 1024 - limit for navbar0/navbar1 template size */ 
	if (nav == NULL) {
	  DPS_FREE(old_tmplt);
	  DPS_FREE(tmplt);

	  DpsLog(Agent, DPS_LOG_ERROR, "Can't realloc nav");

	  return 
#ifdef APACHE1
		       SERVER_ERROR;
#else
		       HTTP_INTERNAL_SERVER_ERROR;
#endif
	}
	nav[0] = '\0';
	
	/* build NL NB NR */
	for(i = (size_t)page1; i < (size_t)page2; i++){
	  DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", (int)i + have_p);
		DpsBuildPageURL(&query_vars, &url);
		DpsVarListReplaceStr(&Agent->Vars, "NH", url);
		DpsVarListReplaceInt(&Agent->Vars, "NP", (int)(i+1));
		DpsTemplatePrint(Agent, NULL, NULL, DPS_STREND(nav), nav_len - (nav - DPS_STREND(nav)), &Agent->tmpl,
				 (i == (size_t)page_number)?"navbar0":"navbar1");
	}
	DpsVarListReplaceStr(&Agent->Vars,"NB",nav);
	
	DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", page_number - 1 + have_p);
	DpsBuildPageURL(&query_vars, &url);
	DpsVarListReplaceStr(&Agent->Vars,"NH",url);
	
	if(Res->first==1){/* First page */
		DpsTemplatePrint(Agent, NULL, NULL, nav, nav_len, &Agent->tmpl, "navleft_nop");
		DpsVarListReplaceStr(&Agent->Vars,"NL",nav);
	}else{
		DpsTemplatePrint(Agent, NULL, NULL, nav, nav_len, &Agent->tmpl, "navleft");
		DpsVarListReplaceStr(&Agent->Vars,"NL",nav);
	}
	
	DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", page_number + 1 + have_p);
	DpsBuildPageURL(&query_vars, &url);
	DpsVarListReplaceStr(&Agent->Vars,"NH",url);
/*	
	DpsVarListReplaceInt(&query_vars, "np", 0);
	DpsVarListDel(&query_vars, "s");
	DpsBuildPageURL(&query_vars, &url);
	DpsVarListReplaceStr(&Agent->Vars, "FirstPage", url);
*/
	if(Res->last>=Res->total_found){/* Last page */
		DpsTemplatePrint(Agent, NULL, NULL, nav, nav_len, &Agent->tmpl, "navright_nop");
		DpsVarListReplaceStr(&Agent->Vars, "NR", nav);
	}else{
		DpsTemplatePrint(Agent, NULL, NULL, nav, nav_len, &Agent->tmpl, "navright");
		DpsVarListReplaceStr(&Agent->Vars,"NR",nav);
	}
	
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "restop");
	StoredocURL = DpsVarListFindStr(&Agent->Vars, "StoredocURL", "/cgi-bin/storedoc.cgi");
	StoredocTmplt = DpsVarListFindStr(&Agent->Vars, "StoredocTmplt", NULL);
	
	for(i=0;i<Res->num_rows;i++){
		DPS_DOCUMENT	*Doc=&Res->Doc[i];
		DPS_CATEGORY	C;
		char		*clist;
		const char	*u, *dm;
		char		*eu, *edm;
		char		*ct, *ctu;
		size_t		cl, sc, ir, clistsize;
		urlid_t		dc_url_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
		urlid_t		dc_origin_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "Origin-ID", 0);
		
		/* Skip clones */
		if(dc_origin_id)continue;
		
		clist = (char*)DpsMalloc(2048); 
		if (clist == NULL) {
		  DPS_FREE(old_tmplt);
		  DPS_FREE(tmplt);

		  DpsLog(Agent, DPS_LOG_ERROR, "Can't malloc clist");

		  return 
#ifdef APACHE1
			       SERVER_ERROR;
#else
			       HTTP_INTERNAL_SERVER_ERROR;
#endif
		}
		clist[0] = '\0';
		clistsize = 0;
		for(cl=0;cl<Res->num_rows;cl++){
			DPS_DOCUMENT	*Clone=&Res->Doc[cl];
			urlid_t		cl_origin_id = (urlid_t)DpsVarListFindInt(&Clone->Sections, "Origin-ID", 0);
			
			if ((dc_url_id == cl_origin_id) && cl_origin_id){
				DPS_VARLIST	CloneVars;
				
				DpsVarListInit(&CloneVars);
				DpsVarListAddLst(&CloneVars, &Agent->Vars, NULL, "*");
				DpsVarListReplaceLst(&CloneVars, &Doc->Sections, NULL, "*");
				DpsVarListReplaceLst(&CloneVars, &Clone->Sections, NULL, "*");
				clist = (char*)DpsRealloc(clist, (clistsize = dps_strlen(clist)) + 2048);
				if (clist == NULL) {
				  DPS_FREE(old_tmplt);
				  DPS_FREE(tmplt);

				  DpsLog(Agent, DPS_LOG_ERROR, "Can't realloc clist");

				  return 
#ifdef APACHE1
					       SERVER_ERROR;
#else
					       HTTP_INTERNAL_SERVER_ERROR;
#endif
				}
				Agent->tmpl.Env_Vars = &CloneVars;
				DpsTemplatePrint(Agent, NULL, NULL, clist + clistsize, 2048, &Agent->tmpl, "clone");
				DpsVarListFree(&CloneVars);
			}
		}
		Agent->tmpl.Env_Vars = &Agent->Vars;
		clistsize += 2048;
		
		DpsVarListReplaceStr(&Agent->Vars,"CL",clist);
		DpsVarListReplaceInt(&Agent->Vars, "DP_ID", dc_url_id);
		
		DpsVarListReplace(&Agent->Vars, DpsVarListFind(&Doc->Sections, "URL"));
		

		DpsVarListReplaceStr(&Agent->Vars, "title", "[no title]");

		/* Pass all found user-defined sections */
		for (ir = 0; ir < 256; ir++)
		for (sc = 0; sc < Doc->Sections.Root[ir].nvars; sc++) {
			DPS_VAR *S = &Doc->Sections.Root[ir].Var[sc];
			DpsVarListReplace(&Agent->Vars, S);
		}
		
		bzero((void*)&C, sizeof(C));
		dps_strcpy(C.addr,DpsVarListFindStr(&Doc->Sections,"Category","0"));
		if(catcolumns && !DpsCatAction(Agent, &C, DPS_CAT_ACTION_PATH)){
			char *catpath = NULL;
			size_t c, l = 2;

			for(c = (catcolumns > C.ncategories) ? (C.ncategories - catcolumns) : 0; c < C.ncategories; c++) 
			  l += 32 + dps_strlen(C.Category[c].path) + dps_strlen(C.Category[c].name);
			catpath = (char*)DpsMalloc(l + 1);
			if (catpath != NULL) {
			  *catpath = '\0';
			  for(c = (catcolumns > C.ncategories) ? (C.ncategories - catcolumns) : 0; c < C.ncategories; c++){
				sprintf(catpath+dps_strlen(catpath)," &gt; <A HREF=\"?c=%s\">%s</A> ",
					C.Category[c].path,
					C.Category[c].name);
			  }
			  DpsVarListReplaceStr(&Agent->Vars,"DY",catpath);
			  DPS_FREE(catpath);
			}
		}
		DPS_FREE(C.Category);

		u =  DpsVarListFindStrTxt(&Agent->Vars, "URL", "");
		eu = (char*)DpsMalloc(dps_strlen(u)*10 + 64);
		if (eu == NULL) {
		  DPS_FREE(old_tmplt);
		  DPS_FREE(tmplt);

		  DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc eu");

		  return 
#ifdef APACHE1
			       SERVER_ERROR;
#else
			       HTTP_INTERNAL_SERVER_ERROR;
#endif
		}
		DpsEscapeURL(eu, u);
		dm = DpsVarListFindStr(&Agent->Vars, "Last-Modified", "");
		edm = (char*)DpsMalloc(dps_strlen(dm)*10 + 10);
		if (edm == NULL) {
		  DPS_FREE(old_tmplt);
		  DPS_FREE(tmplt);
		  
		  DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc edm");

		  return 
#ifdef APACHE1
			       SERVER_ERROR;
#else
			       HTTP_INTERNAL_SERVER_ERROR;
#endif
		}
		DpsEscapeURL(edm, dm);

		ct = DpsVarListFindStr(&Agent->Vars, "Content-Type", "");
		ctu = (char*)DpsMalloc(dps_strlen(ct) * 10 + 10);
		if (ctu == NULL) {
		  DPS_FREE(old_tmplt);
		  DPS_FREE(tmplt);
		  
		  DpsLog(Agent, DPS_LOG_ERROR, "Can't alloc ctu");

		  return 
#ifdef APACHE1
			       SERVER_ERROR;
#else
			       HTTP_INTERNAL_SERVER_ERROR;
#endif
		}
		DpsEscapeURL(ctu, ct);

		dps_snprintf(storedstr, storedlen, "%s?%s%s%srec_id=%d&amp;label=%s&amp;DM=%s&amp;DS=%d&amp;L=%s&amp;CS=%s&amp;DU=%s&amp;CT=%s&amp;q=%s",
			     StoredocURL,
			     (StoredocTmplt) ? "tmplt=" : "",
			     (StoredocTmplt) ? StoredocTmplt : "",
			     (StoredocTmplt) ? "&amp;" : "",
			     DpsURL_ID(Doc, NULL),
			     DpsVarListFindStr(&Agent->Vars, "label", ""),
			     edm, /* Last-Modified escaped */
			     sc = DpsVarListFindInt(&Agent->Vars, "Content-Length", 0),
			     DpsVarListFindStr(&Agent->Vars,"Content-Language",""),
			     DpsVarListFindStr(&Agent->Vars,"Charset",""),
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

#ifdef DEBUG
		DpsLog(Agent, DPS_LOG_DEBUG, "body: %s", DpsVarListFindStr(&Agent->Vars,"body","(none)"));
#endif

#ifdef DEBUG
		DpsLog(Agent, DPS_LOG_DEBUG, "Z: %s", DpsVarListFindStr(&Doc->Sections, "Z", "<NULL>"));
#endif


		if (/*DpsVarListFindInt(&Doc->Sections, "ST", 0) &&*/ (DpsVarListFindStr(&Doc->Sections, "Z", NULL) == NULL)) {
			DpsVarListReplaceInt(&Agent->Vars,"ST",1);
			DpsVarListReplaceStr(&Agent->Vars, "stored_href", storedstr);
		} else {
			DpsVarListReplaceInt(&Agent->Vars,"ST",0);
			DpsVarListReplaceStr(&Agent->Vars, "stored_href", "");
		}
		
		if (Res->PerSite) {
		  DpsVarListReplaceUnsigned(&Agent->Vars, "PerSite", Res->PerSite[i + Res->offset  * (Res->first - 1)]);
		}

		/* put Xres sections if any */
		for (ir = 2; ir <= Res->num_rows; ir++) {
		  if ((i + 1) % ir == 0) {
		    char template_name[32];
		    dps_snprintf(template_name, sizeof(template_name), "%dres", r);
		    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, template_name);
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
		    ap_rprintf(r, Agent->tmpl.GrBeg);
		  }
#endif
		}
		
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "res");
#ifdef WITH_GOOGLEGRP
		if ((sc == 0) && (site_id == prev_site_id)) {
		  DpsVarListDel(&Agent->Vars, "grouped");
		  ap_rprintf(r, Agent->tmpl.GrEnd);
		}
		prev_site_id = site_id;
#endif
		/* put resX sections if any */
		for (ir = 2; ir <= Res->num_rows; ir++) {
		  if ((i + 1) % ir == 0) {
		    char template_name[32];
		    dps_snprintf(template_name, sizeof(template_name), "res%d", r);
		    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, template_name);
		  }
		}
		
		/* Revoke all found user-defined sections */
		for (ir = 0; ir < 256; ir++)
		for(sc = 0; sc < Doc->Sections.Root[ir].nvars; sc++){
			DPS_VAR *S = &Doc->Sections.Root[ir].Var[sc];
			DpsVarListDel(&Agent->Vars, S->name);
/*			DpsLog(Agent, DPS_LOG_DEBUG, "Var: '%s' revoked", S->name);*/
		}
		DpsVarListDel(&Agent->Vars, "body"); /* remoke "body" if it's not in doc's info and made from Excerpt */
		
		DpsFree(clist);
	}
        DpsVarListReplaceInt(&Agent->Vars, (have_p) ? "p" : "np", page_number + have_p);
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "resbot");
	DPS_FREE(searchwords);
	DPS_FREE(storedstr);
	
freeres:
	
end:
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&ap_rprintf, r, NULL, 0, &Agent->tmpl, "bottom");
	
	DpsResultFree(Res);
	DpsVarListDelLst(&Agent->Vars, &query_vars, NULL, "*");
	DpsVarListFree(&query_vars);
/*	DpsVarListFree(&Env_Vars);*/
	DPS_FREE(url);
	DPS_FREE(nav);

      DPS_FREE(old_tmplt);
      DPS_FREE(tmplt);

  ap_rflush(r);
  ap_kill_timeout(r);

  return OK;
}

#ifdef APACHE1
static void dpsearch_mod_init( server_rec *server, pool *p ) {
#else
static int dpsearch_mod_init( apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s ) {
#endif
	ap_add_version_component(
#ifdef APACHE2
				 p,
#endif
				 MOD_DPSEARCH_VERSION_INFO_STRING);
	DpsInit(0, NULL, NULL);        /* Initialize dpsearch library */

	DpsInitMutexes();
#ifdef APACHE2
	return OK;
#endif
}
 
 
static 
#ifdef APACHE1
void 
#else
apr_status_t
#endif
ShutdownDpsearch(void *p) {
  DPS_AGENT *Agent = (DPS_AGENT*)p;
  DPS_ENV *Conf = Agent->Conf;

  if (Conf->Flags.PreloadURLData) {
    DpsURLDataDePreload(Agent);
  }
  DpsAgentFree(Agent);
  DpsEnvFree(Conf);
  DpsDestroyMutexes();

#ifdef EFENCE
  fprintf(stderr, "Memory leaks checking\n");
  DpsEfenceCheckLeaks();
#endif
#ifdef FILENCE
  fprintf(stderr, "FD leaks checking\n");
  DpsFilenceCheckLeaks(NULL);
#endif

#ifdef APACHE2
  return APR_SUCCESS;
#endif

}

/*
#ifdef APACHE2
static apr_status_t ShutdownEmpty(void *p) {
  return APR_SUCCESS;
}
#endif
*/

static void *dpsearch_server_init(
#ifdef APACHE1 
				  pool *p,
#else
				  apr_pool_t *p,
#endif
				  server_rec *s) {
  DPS_ENV *Conf;
  DPS_AGENT *Agent;

  Conf = DpsEnvInit(NULL);
  Agent = DpsAgentInit(NULL, Conf, 0);
  DpsVarListAddStr(&Conf->Vars, "mod_dpsearch", "yes");
#ifdef APACHE2
  apr_pool_cleanup_register(p, Agent, ShutdownDpsearch, apr_pool_cleanup_null /*ShutdownEmpty*/);
#endif
  return Agent;
}


#ifdef APACHE1
static void dpsearch_child_exit( server_rec *s, pool *p ) {
  DPS_AGENT *Agent = ap_get_module_config( s->module_config, &dpsearch_module );
  ShutdownDpsearch(Agent);
}
#endif


static const char *cmd_template(cmd_parms *cmd, void *mconfig, const char *template_filename) {
  DPS_AGENT *Agent = ap_get_module_config( cmd->server->module_config, &dpsearch_module );
  char *old_tmplt = (char*)DpsStrdup(DpsVarListFindStr(&Agent->Vars, "tmplt", " "/*"search.htm"*/));

  
  if (strcasecmp(old_tmplt, template_filename)) {
    DpsVarListReplaceStr(&Agent->Conf->Vars, "tmplt", template_filename);
    Agent->tmpl.Env_Vars = &Agent->Conf->Vars;
    if ( DPS_OK != DpsTemplateLoad(Agent, Agent->Conf, &Agent->tmpl, template_filename) ) {
      ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 
#ifdef APACHE2
		   0,
#endif
		   cmd->server, "Can't load template '%s' : %s", template_filename, Agent->Conf->errstr);
    }
    Agent->Flags = Agent->Conf->Flags;
    Agent->WordParam = Agent->Conf->WordParam;
    DpsVarListReplaceLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");
    Agent->tmpl.Env_Vars = &Agent->Vars;
  }
  DPS_FREE(old_tmplt);
  return NULL;
}


static const char *cmd_storedoc_template(cmd_parms *cmd, void *mconfig, const char *template_filename) {
  DPS_AGENT *Agent = ap_get_module_config( cmd->server->module_config, &dpsearch_module );
  char *old_tmplt = (char*)DpsStrdup(DpsVarListFindStr(&Agent->Vars, "st_tmplt", " "/*"search.htm"*/));

  if (strcasecmp(old_tmplt, template_filename)) {

    DpsVarListReplaceStr(&Agent->Conf->Vars, "st_tmplt", template_filename);
    Agent->st_tmpl.Env_Vars = &Agent->Conf->Vars;
    if ( DPS_OK != DpsTemplateLoad(Agent, Agent->Conf, &Agent->st_tmpl, template_filename) ) {
      ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 
#ifdef APACHE2
		   0,
#endif
		   cmd->server, "Can't load storedoc template '%s' : %s", template_filename, Agent->Conf->errstr);
    }
    Agent->Flags = Agent->Conf->Flags;
    Agent->WordParam = Agent->Conf->WordParam;
    DpsVarListReplaceLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");
    Agent->tmpl.Env_Vars = &Agent->Vars;
  }

  return NULL;
}

static const char *cmd_searchd_conf(cmd_parms *cmd, void *mconfig, const char *word1){
  DPS_AGENT *Agent = ap_get_module_config( cmd->server->module_config, &dpsearch_module );
  dps_uint8 flags = DPS_FLAG_SPELL | DPS_FLAG_UNOCON;
  int verb = -1;
  
  if (DPS_OK != DpsEnvLoad(Agent, word1, flags)) {
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 
#ifdef APACHE2
		 0,
#endif
		 cmd->server, "Can't load config file '%s': %s", word1, Agent->Conf->errstr);
  }
/*  DpsUniRegCompileAll(Agent->Conf);*/
  DpsOpenLog("mod_dpsearch", Agent->Conf, 1);
  DpsSetLogLevel(Agent, DpsVarListFindInt(&Agent->Vars, "LogLevel", DPS_LOG_INFO) );
  Agent->flags = Agent->Conf->flags = flags;
  if (Agent->Conf->Flags.PreloadURLData) {
    DpsLog(Agent, verb, "Preloading url data");
    DpsURLDataPreload(Agent);
  }
  Agent->Flags = Agent->Conf->Flags;
  Agent->WordParam = Agent->Conf->WordParam;
  DpsVarListFree(&Agent->Vars);
  DpsVarListInit(&Agent->Vars);
  DpsVarListAddLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");
  DpsLog(Agent, verb, "mod_dpsearch started with '%s'", word1);
  DpsLog(Agent, verb, "LogLevel: %d", DpsVarListFindInt(&Agent->Vars, "LogLevel", DPS_LOG_INFO));
  DpsLog(Agent, verb, "VarDir: '%s'", DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR));
  DpsLog(Agent, verb, "Affixes: %d, Spells: %d, Synonyms: %d", 
	 Agent->Conf->Affixes.naffixes, Agent->Conf->Spells.nspell, Agent->Conf->Synonyms.nsynonyms);

  return NULL;
}


#ifdef APACHE2
static const command_rec dpsearch_cmds[] = {
  AP_INIT_TAKE1("DataparkSearchTemplate", cmd_template, NULL, RSRC_CONF | ACCESS_CONF | OR_ALL,
		"DataparkSearch template file name"),
  AP_INIT_TAKE1("DataparkStoredocTemplate", cmd_storedoc_template, NULL, RSRC_CONF | ACCESS_CONF | OR_ALL,
		"DataparkSearch template file name"),
  AP_INIT_TAKE1("DataparkSearchdConf", cmd_searchd_conf, NULL, RSRC_CONF | OR_NONE,
		"DataparkSearch searchd config file name"),
  {NULL}	
};
#else

static const command_rec dpsearch_cmds[] = {
  { "DataparkSearchTemplate", 
    cmd_template, 
    NULL,
    RSRC_CONF | ACCESS_CONF | OR_ALL, 
    TAKE1, 
    "DataparkSearch template file name" 
  },
  { "DataparkStoredocTemplate", 
    cmd_storedoc_template, 
    NULL,
    RSRC_CONF | ACCESS_CONF | OR_ALL, 
    TAKE1, 
    "DataparkSearch storedoc template file name" 
  },
  { "DataparkSearchdConf", 
    cmd_searchd_conf, 
    NULL, 
    RSRC_CONF | OR_NONE, 
    TAKE1, 
    "DataparkSearch searchd config file name" 
  },
  {NULL}
};

#endif


#ifdef APACHE2

static void dpsearch_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(dpsearch_handler, NULL, NULL, APR_HOOK_FIRST);
    ap_hook_handler(dpstoredoc_handler, NULL, NULL, APR_HOOK_FIRST);
    ap_hook_post_config (dpsearch_mod_init, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA dpsearch_module = {
    STANDARD20_MODULE_STUFF, 
    NULL,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    dpsearch_server_init,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    dpsearch_cmds,                  /* table of config file commands       */
    dpsearch_register_hooks  /* register hooks                      */
};


#else


/* Dispatch list of content handlers */
static const handler_rec dpsearch_handlers[] = { 
    { "dpsearch", dpsearch_handler }, 
    { "dpstoredoc", dpstoredoc_handler }, 
    { NULL, NULL }
};


/* Dispatch list for API hooks */
module MODULE_VAR_EXPORT dpsearch_module = {
    STANDARD_MODULE_STUFF, 
    dpsearch_mod_init,                  /* module initializer                  */
    NULL,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    dpsearch_server_init,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    dpsearch_cmds,                  /* table of config file commands       */
    dpsearch_handlers,       /* [#8] MIME-typed-dispatched handlers */
    NULL,                  /* [#1] URI to filename translation    */
    NULL,                  /* [#4] validate user id from request  */
    NULL,                  /* [#5] check if the user is ok _here_ */
    NULL,                  /* [#3] check access by host address   */
    NULL,                  /* [#6] determine MIME type            */
    NULL,                  /* [#7] pre-run fixups                 */
    NULL,                  /* [#9] log a transaction              */
    NULL,                  /* [#2] header parser                  */
    NULL,                  /* child_init                          */
    dpsearch_child_exit,                  /* child_exit                          */
    NULL                   /* [#0] post read-request              */
#ifdef EAPI
   ,NULL,                  /* EAPI: add_module                    */
    NULL,                  /* EAPI: remove_module                 */
    NULL,                  /* EAPI: rewrite_command               */
    NULL                   /* EAPI: new_connection                */
#endif
};


#endif
