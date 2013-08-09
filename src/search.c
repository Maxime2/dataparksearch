/* Copyright (C) 2003-2012 Datapark corp. All rights reserved.
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
#include "dpsearch.h"
#include "dps_xmalloc.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define MAX_PS 1000


/*****************************************************************/

int main(int argc, char **argv, char **envp) {
  const char	*env, *bcharset, *lcharset, *conf_dir, *document_root;
	char		template_name[PATH_MAX+6]="";
	char            *template_filename = NULL;
	char		*query_string = NULL;
	char		self[1024]="";
	char		*nav = NULL;
	char		*url = NULL;
	char		*searchwords=NULL;
	char		*storedstr=NULL;
	const char      *ResultContentType;
	const char      *StoredocURL, *StoredocTmplt;
	int		res,httpd=0;
	size_t          catcolumns = 0;
	ssize_t		page1,page2,npages,ppp=10;
	int		page_size, page_number, have_p = 1;
	size_t		i, swlen = 0, nav_len, storedlen;
	DPS_ENV		*Env;
	DPS_AGENT	*Agent;
	DPS_RESULT	*Res;
	DPS_VARLIST	query_vars;
#ifdef WITH_GOOGLEGRP
	int             site_id, prev_site_id = 0;
#endif
	
	/* Output Content-type if under HTTPD	 */
	/* Some servers do not pass QUERY_STRING */
	/* if the query was empty, so check	 */
	/* REQUEST_METHOD too     to be safe     */
	
	httpd=(getenv("QUERY_STRING")||getenv("REQUEST_METHOD"));
	if (!(conf_dir=getenv("DPS_ETC_DIR")))
		conf_dir=DPS_CONF_DIR;

	document_root = DPS_NULL2EMPTY(getenv("DOCUMENT_ROOT"));
	
	
	DpsInit(argc, argv, envp);
	Env=DpsEnvInit(NULL);
	if (Env == NULL) {
	  if(httpd){
	    printf("Content-Type: text/plain\r\n\r\n");
	  }
	  printf("Can't alloc Env\n");
	  exit(0);
	}
	DpsVarListInit(&query_vars);
	Agent = DpsAgentInit(NULL, Env, 0);
	if (Agent == NULL) {
	  if(httpd){
	    printf("Content-Type: text/plain\r\n\r\n");
	  }
	  printf("Can't alloc Agent\n");
	  exit(0);
	}
	DpsVarListAddEnviron(&Env->Vars,"ENV");
	
	/* Detect self and template name */
	if((env = getenv("DPSEARCH_TEMPLATE")))
		dps_strncpy(template_name, env, sizeof(template_name) - 1);
	
	if((env=getenv("DPSEARCH_SELF")))
		dps_strncpy(self,env,sizeof(self)-1);
	
	if((env=getenv("QUERY_STRING"))){
	  if (httpd && ((strncasecmp(env, "http://", 7) == 0) || (strncasecmp(env, "http%3A%2F%2F", 13) == 0)) ) {
		  printf("Location: %s", env);
		  printf("Content-Type: text/html\r\n\r\n");
		  printf("<html><head><title>Redirecting</title><meta http-equiv=\"refresh\" content=\"0;url=%s\"></head><body></body>", 
			 env);
		  exit(0);
		}
	        query_string = (char*)DpsRealloc(query_string, dps_strlen(env) + 2);
		if (query_string == NULL) {
		  if(httpd){
		    printf("Content-Type: text/plain\r\n\r\n");
		  }
		  printf("Can't alloc query_string\n");
		  exit(0);
		}
		dps_strncpy(query_string, env, dps_strlen(env) + 1);

		/* Hack for Russian Apache from apache.lexa.ru  */
		/* QUERY_STRING is already converted to server  */
		/* character set. We must print original query  */
		/* string instead however. Under usual apache   */ 
		/* we'll use QUERY_STRING. Note that query_vars */
		/* list will contain not unescaped values, so   */
		/* we don't have to escape them when displaying */
		env = getenv("CHARSET_SAVED_QUERY_STRING");
		DpsParseQStringUnescaped(&query_vars,env?env:query_string);
	
		/* Unescape and save variables from QUERY_STRING */
		/* Env->Vars will have unescaped values however  */
		DpsParseQueryString(Agent,&Env->Vars,query_string);
	
		template_filename = (char*)DpsStrdup(DpsVarListFindStr(&Env->Vars, "tmplt", ""));
	
		if((env=getenv("REDIRECT_STATUS"))){
		        char *s,*e;

			/* Check Apache internal redirect  */
			/* via   "AddHandler" and "Action" */
			if(!self[0]){
				dps_strncpy(self,(env=getenv("REDIRECT_URL"))?env:"search.cgi",sizeof(self)-1);
			}
			if(!template_name[0]){
			        env = getenv("PATH_TRANSLATED");
				dps_strncpy(template_name, (env) ? env : "", sizeof(template_name)-1);
			}
			if(!template_name[0] && (env = getenv("PATH_INFO")) && env[0])
			        dps_strncpy(template_name, env + 1, sizeof(template_name) - 1);
			if(!template_name[0]) {

			  /*This is with OS specific SLASHES */
			  env=((env=getenv("SCRIPT_FILENAME"))?env:"search.cgi");

			  if(strcmp(conf_dir,".")){
			    /* Take from the config directory */
			    dps_snprintf(template_name, sizeof(template_name)-1, "%s/%s", 
					 conf_dir,(s=strrchr(env,DPSSLASH))?(s+1):(self));
			  }else{
			    /* Take from the current directory */
			    dps_strncpy(template_name,env,sizeof(template_name)-1);
			  }

			  /* Find right slash if it presents */
			  s=((s=strrchr(template_name,DPSSLASH))?s:template_name);

			  /* Find .cgi substring */
			  if ((e = strstr(s, ".cgi")) != NULL) {
			    /* Replace ".cgi" with ".htm" */
			    e[1]='h';e[2]='t';e[3]='m';
			  } else {
			    dps_strcat(s, ".htm");
			  }
			}
			e = strrchr(template_name, DPSSLASH);
			if (e == NULL) e = template_name;
			if (*template_filename == '\0') { 
			  DPS_FREE(template_filename);
			  template_filename = (char*)DpsStrdup(e + 1);
			} else {
			  dps_snprintf(e + 1, sizeof(template_name) - (e - template_name), "%s", template_filename);
			}
		}else{
			/* CGI executed without Apache internal redirect */

			/* Detect $Self variable with OS independant SLASHES */
			if(!self[0]){
				dps_strncpy(self,(env=getenv("SCRIPT_NAME"))?env:"search.cgi",sizeof(self)-1);
			}
			if(!template_name[0] && (env = getenv("PATH_INFO")) && env[0])
			        dps_strncpy(template_name, env + 1, sizeof(template_name) - 1);

			if(!template_name[0]){
				char *s,*e;
				
				/*This is with OS specific SLASHES */
				env=((env=getenv("SCRIPT_FILENAME"))?env:"search.cgi");

				if(strcmp(conf_dir,".")){
					/* Take from the config directory */
					dps_snprintf(template_name, sizeof(template_name)-1, "%s/%s", 
						     conf_dir,(s=strrchr(env,DPSSLASH))?(s+1):(self));
				}else{
					/* Take from the current directory */
					dps_strncpy(template_name,env,sizeof(template_name)-1);
				}

				/* Find right slash if it presents */
				s=((s=strrchr(template_name,DPSSLASH))?s:template_name);

				if (*template_filename == '\0') {

				  /* Find .cgi substring */
				  if ((e = strstr(s, ".cgi")) != NULL) {
					/* Replace ".cgi" with ".htm" */
					e[1]='h';e[2]='t';e[3]='m';
				  } else {
				        dps_strcat(s, ".htm");
				  }
				  e = strrchr(s, DPSSLASH);
				  if (e == NULL) e = s;
				  DPS_FREE(template_filename);
				  template_filename = (char*)DpsStrdup(e + 1);
				} else {
				  dps_strncpy(s + 1, template_filename, sizeof(template_name) - (s - template_name) - 2);
				}
			}
		}
	}else{
		/* Executed from command line     */
		/* or under server which does not */
		/* pass an empty QUERY_STRING var */
		if(argv[1]) {
		  size_t qslen;
		  query_string = (char*)DpsRealloc(query_string, (qslen = dps_strlen(argv[1]) + 10));
		  if (query_string == NULL) {
		    if(httpd){
		      printf("Content-Type: text/plain\r\n\r\n");
		    }
		    printf("Can't realloc query_string\n");
		    exit(0);
		  }
		  dps_snprintf(query_string, qslen, "q=%s", argv[1]);
		} else {
		  query_string = (char*)DpsRealloc(query_string, 1024);
		  if (query_string == NULL) {
		    if(httpd){
		      printf("Content-Type: text/plain\r\n\r\n");
		    }
		    printf("Can't realloc query_string\n");
		    exit(0);
		  }
		  sprintf(query_string, "q=");
		}

		/* Hack for Russian Apache from apache.lexa.ru  */
		/* QUERY_STRING is already converted to server  */
		/* character set. We must print original query  */
		/* string instead however. Under usual apache   */ 
		/* we'll use QUERY_STRING. Note that query_vars */
		/* list will contain not unescaped values, so   */
		/* we don't have to escape them when displaying */
		env = getenv("CHARSET_SAVED_QUERY_STRING");
		DpsParseQStringUnescaped(&query_vars,env?env:query_string);
	
		/* Unescape and save variables from QUERY_STRING */
		/* Env->Vars will have unescaped values however  */
		DpsParseQueryString(Agent,&Env->Vars,query_string);

		DPS_FREE(template_filename);
		template_filename = (char*)DpsStrdup(DpsVarListFindStr(&Env->Vars, "tmplt", "search.htm"));
	
		/*// Get template name from command line variable &tmplt */
		if(!template_name[0])
			dps_snprintf(template_name,sizeof(template_name),"%s/%s", conf_dir, template_filename);
	}
	
	DpsVarListReplaceStr(&Agent->Conf->Vars, "tmplt", template_filename);
	DPS_FREE(template_filename);

	Agent->tmpl.Env_Vars = &Env->Vars;

	DpsURLNormalizePath(template_name);

	if ( (strncmp(template_name, conf_dir, dps_strlen(conf_dir)) && strncmp(template_name, document_root, dps_strlen(document_root)))
	    || (res = DpsTemplateLoad(Agent, Env, &Agent->tmpl, template_name))) {
	  if (strcmp(template_name, "search.htm")) { /* trying load default template */
	    fprintf(stderr, "Can't load template: '%s' %s\n", template_name, Env->errstr);
	    DPS_FREE(template_filename);
	    template_filename = (char*)DpsStrdup("search.htm");
	    dps_snprintf(template_name, sizeof(template_name), "%s/%s", conf_dir, template_filename);

	    if ((res = DpsTemplateLoad(Agent, Env, &Agent->tmpl, template_name))) {
	      
	        fprintf(stderr, "Can't load default template: '%s' %s\n", template_name, Env->errstr);
		if(httpd)printf("Content-Type: text/plain\r\n\r\n");
		printf("%s\n",Env->errstr);
		DpsVarListFree(&query_vars);
		DpsEnvFree(Env);
		DPS_FREE(query_string);
		DpsAgentFree(Agent);
		return(0);
	    }
	  } else {
		if(httpd)printf("Content-Type: text/plain\r\n\r\n");
		printf("%s\n",Env->errstr);
		DpsVarListFree(&query_vars);
		DpsEnvFree(Env);
		DPS_FREE(query_string);
		DpsAgentFree(Agent);
		return(0);
	  }
	}

	/* set locale if specified */
	if ((url = DpsVarListFindStr(&Env->Vars, "Locale", NULL)) != NULL) {
	  setlocale(LC_COLLATE, url);
	  setlocale(LC_CTYPE, url);
	  setlocale(LC_ALL, url);
/*#ifdef HAVE_ASPELL*/
	  { char *p;
	    if ((p = strchr(url, '.')) != NULL) {
	      *p = '\0';
	      DpsVarListReplaceStr(&Env->Vars, "g-lc", url);
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
	
	/* Call again to load search Limits if need */
	DpsParseQueryString(Agent, &Env->Vars, query_string);
	Agent->Flags = Env->Flags;
	Agent->flags |= DPS_FLAG_UNOCON;
	Env->flags |= DPS_FLAG_UNOCON;
	DpsSetLogLevel(NULL, DpsVarListFindInt(&Env->Vars, "LogLevel", 0));
	DpsOpenLog("search.cgi", Env, !strcasecmp(DpsVarListFindStr(&Env->Vars, "Log2stderr", (!httpd) ? "yes" : "no"), "yes"));
	TRACE_IN(Agent, "search.cgi");
	DpsLog(Agent,DPS_LOG_ERROR,"search.cgi started with '%s'",template_name);
		DpsLog(Agent, DPS_LOG_DEBUG, "VarDir: '%s'", DpsVarListFindStr(&Agent->Conf->Vars, "VarDir", DPS_VAR_DIR));
		DpsLog(Agent, DPS_LOG_DEBUG, "Affixes: %d, Spells: %d, Synonyms: %d, Acronyms: %d, Stopwords: %d",
		       Env->Affixes.naffixes,Env->Spells.nspell,
		       Env->Synonyms.nsynonyms,
		       Env->Acronyms.nacronyms,
		       Env->StopWords.nstopwords);
		DpsLog(Agent, DPS_LOG_DEBUG, "Chinese dictionary with %d entries", Env->Chi.nwords);
		DpsLog(Agent, DPS_LOG_DEBUG, "Korean dictionary with %d entries", Env->Korean.nwords);
		DpsLog(Agent, DPS_LOG_DEBUG, "Thai dictionary with %d entries", Env->Thai.nwords);
	DpsVarListAddLst(&Agent->Vars, &Env->Vars, NULL, "*");
	Agent->tmpl.Env_Vars = &Agent->Vars;
/*	DpsVarListAddEnviron(&Agent->Vars, "ENV");*/
/****************************************************************************************************************************************/
	/* This is for query tracking */
	DpsVarListAddStr(&Agent->Vars, "QUERY_STRING", query_string);
	DpsVarListAddStr(&Agent->Vars, "self", self);
	env = getenv("HTTP_X_FORWARDER_FOR");
	if (env) {
	  DpsVarListAddStr(&Agent->Vars, "IP", env);
	} else {
	  env = getenv("REMOTE_ADDR");
	  DpsVarListAddStr(&Agent->Vars, "IP", env ? env : "localhost");
	}
	
	bcharset = DpsVarListFindStr(&Agent->Vars, "BrowserCharset", "iso-8859-1");
	Env->bcs=DpsGetCharSet(bcharset);
	lcharset = DpsVarListFindStr(&Agent->Vars, "LocalCharset", "iso-8859-1");
	Env->lcs=DpsGetCharSet(lcharset);
	ppp = DpsVarListFindInt(&Agent->Vars, "PagesPerScreen", 10);

	ResultContentType = DpsVarListFindStr(&Agent->Vars, "ResultContentType", "text/html");
	
	if(httpd) {
		if(!Env->bcs){
			printf("Content-Type: text/plain\r\n\r\n");
			printf("Unknown BrowserCharset '%s' in template '%s'\n",bcharset,template_name);
			exit(0);
		}else if(!Env->lcs){
			printf("Content-Type: text/plain\r\n\r\n");
			printf("Unknown LocalCharset '%s' in template '%s'\n",lcharset,template_name);
			exit(0);
		}else{
		  /* Add user defined headers */
/*		  register size_t r = (size_t)'r';
		  for(i = 0; i < Agent->tmpl.Env_Vars->Root[r].nvars; i++) {
		    DPS_VAR *Hdr = &Agent->tmpl.Env_Vars->Root[r].Var[i];
		    if (strncmp(DPS_NULL2EMPTY(Hdr->name), "Request.", 8)) continue;
		    printf("%s: %s\r\n", Hdr->name + 8, Hdr->val);
		  }*/
		  printf("Content-Type: %s; charset=%s\n\n", ResultContentType, bcharset);
		}
	}else{
		if(!Env->bcs){
			printf("Unknown BrowserCharset '%s' in template '%s'\n",bcharset,template_name);
			exit(0);
		}
		if(!Env->lcs){
			printf("Unknown LocalCharset '%s' in template '%s'\n",lcharset,template_name);
			exit(0);
		}
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
	DpsVarListAddInt(&Agent->Vars, "pn", res);
	
	catcolumns = (size_t)atoi(DpsVarListFindStr(&Agent->Vars, "CatColumns", ""));

#if 0	
	if(catcolumns){
		DPS_CATEGORY C;
		
		bzero((void*)&C, sizeof(C));
		dps_strcpy(C.addr, DpsVarListFindStr(&Agent->Vars, "c", "0"));
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
				sprintf(catlist+dps_strlen(catlist),"<td><a href=\"?c=%s\">%s</A></td><td width=60>&nbsp;</td>\n",
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
			DpsVarListAddStr(&Agent->Vars, "E", DpsEnvErrMsg(Agent->Conf));
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "top");
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "error");
			goto end;
		}

		DPS_FREE(C.Category);
		bzero((void*)&C, sizeof(C));
		dps_strcpy(C.addr, DpsVarListFindStr(&Agent->Vars, "c", "0"));
		if(DPS_OK == DpsCatAction(Agent, &C, DPS_CAT_ACTION_PATH)){
			char *catpath = NULL;
			size_t l = 0;
			
			for(i = 0; i < C.ncategories; i++) l += 32 + dps_strlen(C.Category[i].path) + dps_strlen(C.Category[i].name);
			if (l) catpath = (char*)DpsMalloc(l + 1);
			if (catpath != NULL) {
			  catpath[0] = '\0';
			  for(i = 0; i < C.ncategories; i++){
				sprintf(catpath+dps_strlen(catpath),"/<a href=\"?c=%s\">%s</A>",
					(C.Category[i].path) ? C.Category[i].path : "",
					(C.Category[i].name) ? C.Category[i].name : "");
			  }
			  DpsVarListAddStr(&Agent->Vars, "CP", catpath);
			  DPS_FREE(catpath);
			}
		}else{
			DpsVarListAddStr(&Agent->Vars, "E", DpsEnvErrMsg(Agent->Conf));
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "top");
			DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "error");
			goto end;
		}
		DPS_FREE(C.Category);
	}
#endif

	if(NULL==(Res=DpsFind(Agent))) {
		DpsVarListAddStr(&Agent->Vars, "E", DpsEnvErrMsg(Agent->Conf));
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "top");
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "error");
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
/*	    DpsUnescapeCGIQuery(q_save, q_save);*/
	    DpsVarListReplaceStr(&query_vars, "q", q_save);
/*	    fprintf(stderr, "q_save: %s\n", q_save);*/
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

	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "top");

	if((Res->WWList.nwords == 0) && (Res->nitems - Res->ncmds == 0) && (Res->num_rows == 0)){
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "noquery");
		TRACE_LINE(Agent);
		goto freeres;
	}
	
	if(Res->num_rows == 0) {
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "notfound");
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
	  if(httpd){
	    printf("Content-Type: text/plain\r\n\r\n");
	  }
	  printf("Can't realloc storedstr\n");
	  exit(0);
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
	  if(httpd){
	    printf("Content-Type: text/plain\r\n\r\n");
	  }
	  printf("Can't realloc nav\n");
	  exit(0);
	}
	nav[0] = '\0';
	TRACE_LINE(Agent);
	
	/* build NL NB NR */
	for(i = (size_t)page1; i < (size_t)page2; i++){
	        DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", (int)i + have_p);
		DpsBuildPageURL(&query_vars, &url);
		DpsVarListReplaceStr(&Agent->Vars, "NH", url);
		DpsVarListReplaceInt(&Agent->Vars, "NP", (int)(i+1));
		DpsTemplatePrint(Agent,(DPS_OUTPUTFUNCTION)&fprintf,  NULL, DPS_STREND(nav), nav_len - strlen(nav), 
				 &Agent->tmpl, (i == (size_t)page_number)?"navbar0":"navbar1");
	}
	DpsVarListAddStr(&Agent->Vars, "NB", nav);
	
	DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", page_number - 1 + have_p);
	DpsBuildPageURL(&query_vars, &url);
	DpsVarListReplaceStr(&Agent->Vars, "NH", url);
	
	if(Res->first==1){/* First page */
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, NULL, nav, nav_len, &Agent->tmpl, "navleft_nop");
		DpsVarListReplaceStr(&Agent->Vars, "NL", nav);
	}else{
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, NULL, nav, nav_len, &Agent->tmpl, "navleft");
		DpsVarListReplaceStr(&Agent->Vars, "NL", nav);
	}
	
	DpsVarListReplaceInt(&query_vars, (have_p) ? "p" : "np", page_number + 1 + have_p);
	DpsBuildPageURL(&query_vars, &url);
	DpsVarListReplaceStr(&Agent->Vars, "NH", url);

	if(Res->last>=Res->total_found){/* Last page */
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, NULL, nav, nav_len, &Agent->tmpl, "navright_nop");
		DpsVarListReplaceStr(&Agent->Vars, "NR", nav);
	}else{
		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, NULL, nav, nav_len, &Agent->tmpl, "navright");
		DpsVarListReplaceStr(&Agent->Vars, "NR", nav);
	}
	
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "restop");
	StoredocURL = DpsVarListFindStr(&Agent->Vars, "StoredocURL", "/cgi-bin/storedoc.cgi");
	StoredocTmplt = DpsVarListFindStr(&Agent->Vars, "StoredocTmplt", NULL);

	for(i=0;i<Res->num_rows;i++){
		DPS_DOCUMENT	*Doc=&Res->Doc[i];
		DPS_CATEGORY	C;
		char		*clist;
		const char	*u, *dm;
		char		*eu, *edm;
		char		*ct, *ctu;
		size_t		cl, sc, r, clistsize;
		urlid_t		dc_url_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "DP_ID", 0);
		urlid_t		dc_origin_id = (urlid_t)DpsVarListFindInt(&Doc->Sections, "Origin-ID", 0);
		
		/* Skip clones */
		if(dc_origin_id)continue;
		
		clist = (char*)DpsMalloc(2048); 
		if (clist == NULL) {
		  if(httpd){
		    printf("Content-Type: text/plain\r\n\r\n");
		  }
		  printf("Can't alloc clist\n");
		  exit(0);
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
				  if(httpd){
				    printf("Content-Type: text/plain\r\n\r\n");
				  }
				  printf("Can't realloc clist\n");
				  exit(0);
				}
				Agent->tmpl.Env_Vars = &CloneVars;
				DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, NULL, clist + clistsize, 2048, &Agent->tmpl, "clone");
				DpsVarListFree(&CloneVars);
			}
		}
		Agent->tmpl.Env_Vars = &Agent->Vars;
		clistsize += 2048;
		
		DpsVarListReplaceStr(&Agent->Vars, "CL", clist);
		DpsVarListReplaceInt(&Agent->Vars, "DP_ID", dc_url_id);
		
/*		DpsVarListReplace(&Agent->Vars, DpsVarListFind(&Doc->Sections, "URL"));*/
		DpsVarListReplace(&Agent->Vars, DpsVarListFind(&Doc->Sections, "Alias"));
		

		DpsVarListReplaceStr(&Agent->Vars, "title", "[no title]");

		/* Pass all found user-defined sections */
		for (r = 0; r < 256; r++)
		for (sc = 0; sc < Doc->Sections.Root[r].nvars; sc++) {
			DPS_VAR *S = &Doc->Sections.Root[r].Var[sc];
			DpsVarListReplace(&Agent->Vars, S);
/*			fprintf(stderr, "sname: %s  sval: %s\n", S->name, S->val);*/
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
			  for(c = (catcolumns > C.ncategories) ? (C.ncategories - catcolumns) : 0; c < C.ncategories; c++){
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
		  if(httpd){
		    printf("Content-Type: text/plain\r\n\r\n");
		  }
		  printf("Can't alloc eu\n");
		  exit(0);
		}
		DpsEscapeURL(eu, u);

		dm = DpsVarListFindStr(&Agent->Vars, "Last-Modified", "");
		edm = (char*)DpsMalloc(dps_strlen(dm)*10 + 10);
		if (edm == NULL) {
		  if(httpd){
		    printf("Content-Type: text/plain\r\n\r\n");
		  }
		  printf("Can't alloc edm\n");
		  exit(0);
		}
		DpsEscapeURL(edm, dm);

		ct = DpsVarListFindStr(&Agent->Vars, "Content-Type", "");
		ctu = (char*)DpsMalloc(dps_strlen(ct) * 10 + 10);
		if (ctu == NULL) {
		  if(httpd){
		    printf("Content-Type: text/plain\r\n\r\n");
		  }
		  printf("Can't alloc ctu\n");
		  exit(0);
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

		if (/*DpsVarListFindInt(&Doc->Sections, "ST", 0) && */(DpsVarListFindStr(&Doc->Sections, "Z", NULL) == NULL)) {
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
		    fprintf(stdout, "%s", Agent->tmpl.GrBeg);
		  }
#endif
		}

		DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "res");
#ifdef WITH_GOOGLEGRP
		if ((sc == 0) && (site_id == prev_site_id)) {
		  DpsVarListDel(&Agent->Vars, "grouped");
		  fprintf(stdout, "%s", Agent->tmpl.GrEnd);
		}
		prev_site_id = site_id;
#endif
		/* put resX sections if any */
		for (r = 2; r <= Res->num_rows; r++) {
		  if ((i + 1) % r == 0) {
		    dps_snprintf(template_name, sizeof(template_name), "res%d", r);
		    DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, template_name);
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
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "resbot");
	DPS_FREE(searchwords);
	DPS_FREE(storedstr);
	
freeres:
	
end:
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "bottom");
	if (httpd) fflush(NULL); else fclose(stdout);
	DpsResultFree(Res);

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

	TRACE_OUT(Agent);
	DpsVarListFree(&query_vars);
	DpsAgentFree(Agent);
	DpsEnvFree(Env);
	DPS_FREE(query_string);
	DPS_FREE(url);
	DPS_FREE(nav);

	DpsDeInit();
	
#ifdef EFENCE
	fprintf(stderr, "Memory leaks checking\n");
	DpsEfenceCheckLeaks();
#endif
#ifdef FILENCE
	fprintf(stderr, "FD leaks checking\n");
	DpsFilenceCheckLeaks(NULL);
#endif

	return DPS_OK;
}
