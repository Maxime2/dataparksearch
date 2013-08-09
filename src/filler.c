/* Copyright (C) 2006-2007 Datapark corp. All rights reserved.

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
#include <sys/types.h>
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

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define MAX_PS 1000


/*****************************************************************/

int main(int argc, char ** argv, char **envp) {
	const char	*env, *bcharset, *lcharset, *conf_dir;
	char		template_name[PATH_MAX+6]="";
	char            *template_filename = NULL;
	char		*query_string = NULL;
	char		self[1024]="";
	char		*url = NULL;
	const char      *ResultContentType;
	int		res,httpd=0;
	size_t          catcolumns = 0;
	int		page_size,page_number;
	DPS_ENV		*Env;
	DPS_AGENT	*Agent;
	DPS_VARLIST	query_vars;
	
	/* Output Content-type if under HTTPD	 */
	/* Some servers do not pass QUERY_STRING */
	/* if the query was empty, so check	 */
	/* REQUEST_METHOD too     to be safe     */
	
	httpd=(getenv("QUERY_STRING")||getenv("REQUEST_METHOD"));
	if (!(conf_dir=getenv("DPS_ETC_DIR")))
		conf_dir=DPS_CONF_DIR;
	
	
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
	else if((env = getenv("PATH_INFO")) && env[0])
		dps_strncpy(template_name, env + 1, sizeof(template_name) - 1);
	
	if((env=getenv("DPSEARCH_SELF")))
		dps_strncpy(self,env,sizeof(self)-1);
	
	if((env=getenv("QUERY_STRING"))){
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

			/* Check Apache internal redirect  */
			/* via   "AddHandler" and "Action" */
			if(!self[0]){
				dps_strncpy(self,(env=getenv("REDIRECT_URL"))?env:"filler.cgi",sizeof(self)-1);
			}
			if(!template_name[0]){
				dps_strncpy(template_name,(env=getenv("PATH_TRANSLATED"))?env:"",sizeof(template_name)-1);
			}
			if (*template_filename == '\0') { 
			  DPS_FREE(template_filename); 
			  template_filename = (char*)DpsStrdup("filler.htm"); 
			}
		}else{
			/* CGI executed without Apache internal redirect */

			/* Detect $Self variable with OS independant SLASHES */
			if(!self[0]){
				dps_strncpy(self,(env=getenv("SCRIPT_NAME"))?env:"filler.cgi",sizeof(self)-1);
			}

			if(!template_name[0]){
				char *s,*e;
				
				/*This is with OS specific SLASHES */
				env=((env=getenv("SCRIPT_FILENAME"))?env:"filler.cgi");

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
				  e = strrchr(s, '/');
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
		  query_string = (char*)DpsRealloc(query_string, dps_strlen(argv[1]) + 10);
		  if (query_string == NULL) {
		    if(httpd){
		      printf("Content-Type: text/plain\r\n\r\n");
		    }
		    printf("Can't realloc query_string\n");
		    exit(0);
		  }
		  sprintf(query_string, "q=%s", argv[1]);
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
		template_filename = (char*)DpsStrdup(DpsVarListFindStr(&Env->Vars, "tmplt", ""));
		if (*template_filename == '\0') { 
		  DPS_FREE(template_filename); template_filename = (char*)DpsStrdup("filler.htm"); 
		}
	
		/*// Get template name from command line variable &tmplt */
		if(!template_name[0])
			dps_snprintf(template_name,sizeof(template_name),"%s/%s", conf_dir, template_filename);
	}
	
	DpsVarListReplaceStr(&Agent->Conf->Vars, "tmplt", template_filename);
	DPS_FREE(template_filename);

	Agent->tmpl.Env_Vars = &Env->Vars;
	
	DpsURLNormalizePath(template_name);
	
	if (strncmp(template_name, conf_dir, dps_strlen(conf_dir)) 
	    || (res = DpsTemplateLoad(Agent, Env, &Agent->tmpl, template_name))) {
	  if (strcmp(template_name, "filler.htm")) { /* trying load default template */
	    fprintf(stderr, "Can't load template: '%s' %s\n", template_name, Env->errstr);
	    DPS_FREE(template_filename);
	    template_filename = (char*)DpsStrdup("filler.htm");
	    dps_snprintf(template_name, sizeof(template_name), "%s/%s", conf_dir, template_filename);

	    if ((res = DpsTemplateLoad(Agent, Env, &Agent->tmpl, template_name))) {

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
	
	/* Call again to load search Limits if need */
	DpsParseQueryString(Agent, &Env->Vars, query_string);
	Agent->Flags = Env->Flags;
	Agent->flags |= DPS_FLAG_UNOCON;
	Env->flags |= DPS_FLAG_UNOCON;
	DpsSetLogLevel(NULL, DpsVarListFindInt(&Env->Vars, "LogLevel", 0));
	DpsOpenLog("filler.cgi", Env, !strcasecmp(DpsVarListFindStr(&Env->Vars, "Log2stderr", (!httpd) ? "yes" : "no"), "yes"));
	DpsLog(Agent,DPS_LOG_ERROR,"filler.cgi started with '%s'",template_name);
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

	ResultContentType = DpsVarListFindStr(&Agent->Vars, "ResultContentType", "text/html");
	
	if(httpd){
		if(!Env->bcs){
			printf("Content-Type: text/plain\r\n\r\n");
			printf("Unknown BrowserCharset '%s' in template '%s'\n",bcharset,template_name);
			exit(0);
		}else if(!Env->lcs){
			printf("Content-Type: text/plain\r\n\r\n");
			printf("Unknown LocalCharset '%s' in template '%s'\n",lcharset,template_name);
			exit(0);
		}else{
		  printf("Content-type: %s; charset=%s\r\n\r\n", ResultContentType, bcharset);
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
	} else page_number--;
	
	res = DpsVarListFindInt(&Agent->Vars, "np", 0) * page_size;
	DpsVarListAddInt(&Agent->Vars, "pn", res);
	
	catcolumns = (size_t)atoi(DpsVarListFindStr(&Agent->Vars, "CatColumns", ""));


	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "top");

	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "restop");
	
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "res");

	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "resbot");
	
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->tmpl, "bottom");
	
	DpsVarListFree(&query_vars);
	DpsAgentFree(Agent);
	DpsEnvFree(Env);
	DPS_FREE(query_string);
	DPS_FREE(url);
	if (httpd) fflush(NULL); else fclose(stdout);
	
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
