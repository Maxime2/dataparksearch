/* Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
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
#include "dps_url.h"
#include "dps_utils.h"
#include "dps_charsetutils.h"
#include "dps_log.h"
#include "dps_match.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_alias.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

DPS_URL * __DPSCALL DpsURLInit(DPS_URL *url) {
  if (url == NULL) {
    url = (DPS_URL*)DpsMalloc(sizeof(DPS_URL));
    if (url == NULL) return NULL;
    bzero((void*)url, sizeof(DPS_URL));
    url->freeme = 1;
  } else {
    int fr = url->freeme;
    bzero((void*)url, sizeof(DPS_URL));
    url->freeme = fr;
  }
  return url;
}

void __DPSCALL DpsURLFree(DPS_URL *url) {
	DPS_FREE(url->schema);
	DPS_FREE(url->specific);
	DPS_FREE(url->hostinfo);
	DPS_FREE(url->auth);
	DPS_FREE(url->hostname);
	DPS_FREE(url->path);
	DPS_FREE(url->directory);
	DPS_FREE(url->filename);
	DPS_FREE(url->anchor);
	DPS_FREE(url->query_string);
	if(url->freeme){
		DPS_FREE(url);
	} else {
	  url->port = url->default_port = 0;
	}
}

#ifdef DEBUG_URL
int _DpsURLParse(DPS_URL *url, const char *str, const char *filename, int line) {
#else
int DpsURLParse(DPS_URL *url, const char *str) {
#endif
	char *schema,*anchor,*file,*query;
	char *s;
/*	size_t len = dps_strlen(str);*/
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif

#ifdef DEBUG_URL
	fprintf(stderr, " -- %s:%d Parser url: %s\n", filename, line, str);
#endif
	
	DPS_FREE(url->schema);
	DPS_FREE(url->specific);
	DPS_FREE(url->hostinfo);
	DPS_FREE(url->hostname);
	DPS_FREE(url->anchor);
	DPS_FREE(url->auth);
	url->port=0;
	url->default_port=0;
	DPS_FREE(url->path);
	DPS_FREE(url->directory);
	DPS_FREE(url->filename);
	DPS_FREE(url->query_string);

/*	if(len >= DPS_URLSIZE)return(DPS_URL_LONG);  FIXME: Chage this cheking for configured parameter, not DPS_URLSIZE */
	s = (char*)DpsStrdup(str);
	if (s == NULL) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}

	url->len = dps_strlen(str);
	
	/* Find possible schema end than   */	
	/* Check that it is really schema  */
	/* It must consist of alphas only  */
	/* We will take in account digits  */
	/* also for oracle8:// for example */
	/* We must check it because        */
	/* It might be anchor also         */
	/* For example:                    */
	/* "mod/index.html#a:1"            */

	if((schema=strchr(s,':'))){
		const char * ch;
		for(ch=s;ch<schema;ch++){
			if(!isalnum(*ch)){
				/* Bad character       */
				/* so it is not schema */
				schema=0;break;
			}
		}
	}

	if(schema){
		/* Have scheme - absolute path */
		*schema=0;
		url->schema = (char*)DpsStrdup(s);
		url->specific = (char*)DpsStrdup(schema + 1);
		*schema=':';
		if(!strcasecmp(url->schema,"http"))url->default_port=80;
		else
		if(!strcasecmp(url->schema,"https"))url->default_port=443;
		else
		if(!strcasecmp(url->schema,"nntp"))url->default_port=119;
		else
		if(!strcasecmp(url->schema,"news"))url->default_port=119;
		else
		if(!strcasecmp(url->schema,"ftp"))url->default_port=21;

		if(!strncmp(url->specific,"//",2)){
			char	*ss,*hostname;
			
			/* Have hostinfo */
			if((ss=strchr(url->specific+2,'/'))){
				/* Have hostname with path */
				*ss=0;
				url->hostinfo = (char*)DpsStrdup(url->specific + 2);
				*ss='/';
				url->path = (char*)DpsStrdup(ss);
			}else{
				/* Hostname without path */
			        if ((ss = strchr(url->specific + 2, '?'))) {
				  /* Have hostname with parameters */
				  *ss = 0;
				  url->hostinfo = (char*)DpsStrdup(url->specific + 2);
				  *ss='?';
				  url->path = (char*)DpsStrdup("/");
				}else {
				  url->hostinfo = (char*)DpsStrdup(url->specific + 2);
				  url->path = (char*)DpsStrdup("/");
				}
			}
			if((hostname=strrchr(url->hostinfo,'@'))){
				/* Username and password is given  */
				/* Store auth string user:password */
				*hostname=0;
				url->auth = (char*)DpsStrdup(url->hostinfo);
				*hostname='@';
				hostname++;
			}else{
				hostname = url->hostinfo;
			}
			/*
			FIXME:
			for(h=hostname;*h;h++){
				if( *h>='A' && *h<='Z')
				*h=(*h)-'A'+'a';
			}
			*/
	
			if((ss=strchr(hostname,':'))){
				*ss=0;
				url->hostname = (char*)DpsStrdup(hostname);
				*ss=':';
				url->port=atoi(ss+1);
			}else{
				url->hostname = (char*)DpsStrdup(hostname);
				url->port=0;
			}
		}else{
			/* Have not host but have schema                   */
			/* This is possible for:                           */
			/* file:  mailto:  htdb: news:                     */
			/* As far as we do not need mailto: just ignore it */
			
		        if(!strcasecmp(url->schema,"mailto") 
			   || !strcasecmp(url->schema,"javascript")
			   || !strcasecmp(url->schema,"feed")
			   ) {
			        DPS_FREE(s);
#ifdef WITH_PARANOIA
				DpsViolationExit(-1, paran);
#endif
				return(DPS_URL_BAD);
			} else
			if(!strcasecmp(url->schema,"file"))
				url->path = (char*)DpsStrdup(url->specific);
			else
			if(!strcasecmp(url->schema,"exec"))
				url->path = (char*)DpsStrdup(url->specific);
			else
			if(!strcasecmp(url->schema,"cgi"))
				url->path = (char*)DpsStrdup(url->specific);
			else
			if(!strcasecmp(url->schema,"htdb"))
				url->path = (char*)DpsStrdup(url->specific);
			else
			if(!strcasecmp(url->schema,"news")){
				/* Now we will use localhost as NNTP    */
				/* server as it is not indicated in URL */
				url->hostname = (char*)DpsStrdup("localhost");
				url->path = (char*)DpsMalloc(dps_strlen(url->specific) + 2);
				if (url->path == NULL) {
				  DPS_FREE(s);
#ifdef WITH_PARANOIA
				  DpsViolationExit(-1, paran);
#endif
				  return DPS_ERROR;
				}
				sprintf(url->path,"/%s",url->specific);
				url->default_port=119;
			}else{
				/* Unknown strange schema */
			        DPS_FREE(s);
#ifdef WITH_PARANOIA
				DpsViolationExit(-1, paran);
#endif
				return(DPS_URL_BAD);
			}
		}
	}else{
	  if (s[0] == '/' && s[1] == '/') { /* have hostinfo without scheme */
	    char	*ss,*hostname;
	    url->specific = (char*)DpsStrdup(s);
	    /* Have hostinfo */
	    if((ss = strchr(url->specific+2, '/'))) {
	      /* Have hostname with path */
	      *ss = 0;
	      url->hostinfo = (char*)DpsStrdup(url->specific + 2);
	      *ss = '/';
	      url->path = (char*)DpsStrdup(ss);
	    } else {
	      /* Hostname without path */
	      if ((ss = strchr(url->specific + 2, '?'))) {
		/* Have hostname with parameters */
		*ss = 0;
		url->hostinfo = (char*)DpsStrdup(url->specific + 2);
		*ss='?';
		url->path = (char*)DpsStrdup("/");
	      }else {
		url->hostinfo = (char*)DpsStrdup(url->specific + 2);
		url->path = (char*)DpsStrdup("/");
	      }
	    }
	    if((hostname=strrchr(url->hostinfo,'@'))){
	      /* Username and password is given  */
	      /* Store auth string user:password */
	      *hostname=0;
	      url->auth = (char*)DpsStrdup(url->hostinfo);
	      *hostname='@';
	      hostname++;
	    }else{
	      hostname = url->hostinfo;
	    }
	    /*
	      FIXME:
	      for(h=hostname;*h;h++){
	        if( *h>='A' && *h<='Z')
	          *h=(*h)-'A'+'a';
	      }
	    */
	
	    if((ss=strchr(hostname,':'))){
	      *ss=0;
	      url->hostname = (char*)DpsStrdup(hostname);
	      *ss=':';
	      url->port = atoi(ss+1);
	    }else{
	      url->hostname = (char*)DpsStrdup(hostname);
	      url->port=0;
	    }
	  } else {
		url->path = (char*)DpsStrdup(s);
	  }
	}

	/* Cat an anchor if exist */
	if((anchor=strstr(url->path,"#"))) {
	  url->anchor = (char*)DpsStrdup(anchor);
	  *anchor=0;
	}


	/* If path is not full just copy it to filename    */
	/* i.e. neither  /usr/local/ nor  c:/windows/temp/ */

	if((url->path != NULL) && (url->path[0] != '\0') && (url->path[0] != '/') && (url->path[0] != '?') && (url->path[1] != ':')) { 
		/* Relative path */
		if(!strncmp(url->path,"./",2))
			url->filename = (char*)DpsStrdup(url->path + 2);
		else
			url->filename = (char*)DpsStrdup(url->path);
		url->path[0] = 0;
	}

	/* truncate path to query_string */
	/* and store query_string        */

	if((query = strrchr(url->path, '?'))){
		url->query_string = (char*)DpsStrdup(query);
		*(query) = 0;
	} else if((query = strrchr(DPS_NULL2EMPTY(url->specific), '?'))) {
		url->query_string = (char*)DpsStrdup(query);
	}
	
	DpsURLNormalizePath(url->path);
	
	/* Now find right '/' sign and copy the rest to filename */

	if((file=strrchr(url->path,'/'))&&(strcmp(file,"/"))){
		url->filename = (char*)DpsStrdup(file + 1);
		*(file+1)=0;
	}

	/* Now find right '/' sign and copy the rest to directory */

	if ((file = strrchr(url->path,'/'))) {
	  char *p_save = file;
	  for(file--; (file > url->path) && (*file != '/'); file--);
	  file++;
	  if (*file) {
	    *p_save = '\0';
	    url->directory = (char*)DpsStrdup(file);
	    *p_save = '/';
	  }
	}

	DPS_FREE(s);
	if (url->hostname != NULL) {
	  DpsRTrim(url->hostname, ".");
	  url->domain_level = 1;
	  for (s = url->hostname; *s; s++) {
	    *s = (char)dps_tolower((int)*s);
	    if (*s == '.') url->domain_level++;
	    if (strchr(",'\";", (int)*s)) {
#ifdef WITH_PARANOIA
	      DpsViolationExit(-1, paran);
#endif
	      return DPS_URL_BAD;
	    }
	  }
	}
	if (url->hostinfo != NULL) {
	  DpsRTrim(url->hostinfo, ".");
	  s = strrchr(url->hostinfo, '@');
	  for (s = (s == NULL) ? url->hostinfo : s + 1; *s; s++) *s = (char)dps_tolower((int)*s);
	}
	if (url->schema != NULL) for (s = url->schema; *s; s++) *s = (char)dps_tolower((int)*s);

/*	fprintf(stderr, "url: .path: %s port:%d\n", url->path, url->port);*/

#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_OK;
}



char * DpsURLNormalizePath(char * str){
	char * s=str;
	char * q;
	char * d;

	/* Hide query string */

	if((q=strchr(s,'?'))){
		*q++='\0';
		if(!*q)q=NULL;
	}

	/* Remove all "/../" entries */

	while((d=strstr(str,"/../"))){
		char * p;
		
		if(d>str){
			/* Skip non-slashes */
			for(p=d-1;(*p!='/')&&(p>str);p--);
			
			/* Skip slashes */
			while((p>(str+1))&&(*(p-1)=='/'))p--;
		}else{
			/* We are at the top level and have ../  */
			/* Remove it too to avoid loops          */
			p=str;
		}
		dps_memmove(p,d+3,dps_strlen(d)-2);
	}

	/* Remove remove trailig "/.." */

	d=str+dps_strlen(str);
	if((d-str>2)&&(!strcmp(d-3,"/.."))){
		d-=4;
		while((d>str)&&(*d!='/'))d--;
		if(*d=='/')d[1]='\0';
		else	dps_strcpy(str,"/");
	}

	/* Remove all "/./" entries */
	
	while((d=strstr(str,"/./"))){
		dps_memmove(d,d+2,dps_strlen(d)-1);
	}

	/* Remove the trailing "/."  */
	
	if((d=str+dps_strlen(str))>(str+2))
		if(!strcmp(d-2,"/."))
			*(d-1)='\0';

	/* Remove all "//" entries */
	while((d=strstr(str,"//"))){
		dps_memmove(d,d+1,dps_strlen(d));
	}

	
	/* Replace "%7E" with "~"         */
	/* Actually it is to be done      */
	/* for all valid characters       */
	/* which do not require escaping  */
	/* However I'm lazy, do it for 7E */
	/* as the most often "abused"     */

	while((d=strstr(str,"%7E"))){
		*d='~';
		dps_memmove(d+1,d+3,dps_strlen(d+3)+1);
	}

	/* Restore query string */

	if(q){
		char * e=str+dps_strlen(str);
		*e='?';
		dps_memmove(e+1,q,dps_strlen(q)+1);
	}

	return str;
}


 void RelLink(DPS_AGENT *Indexer, DPS_URL *curURL, DPS_URL *newURL, char **str, int ReverseAliasFlag) {
	const char	*schema = newURL->schema ? newURL->schema : curURL->schema;
	const char	*hostname = newURL->hostname ? newURL->hostname : curURL->hostname;
	const char	*auth = NULL;
	const char	*path = (newURL->path && newURL->path[0]) ? newURL->path : curURL->path;
	const char	*fname = ((newURL->filename && newURL->filename[0]) || (newURL->path && newURL->path[0])) ? 
	  newURL->filename : curURL->filename;
	const char      *query_string = newURL->query_string;
	char            *anchor = newURL->anchor;
	char		*pathfile = (char*)DpsMalloc(dps_strlen(DPS_NULL2EMPTY(path)) + dps_strlen(DPS_NULL2EMPTY(fname)) +
						     dps_strlen(DPS_NULL2EMPTY(query_string)) + +dps_strlen(DPS_NULL2EMPTY(anchor)) + 5);
	int             cascade;
	DPS_MATCH	*Alias;
	char		*alias = NULL;
	size_t		aliassize, nparts = 10;
	DPS_MATCH_PART	Parts[10];

	if (pathfile == NULL) return;

	if (newURL->hostinfo == NULL) newURL->charset_id = curURL->charset_id;

	if (newURL->hostname == NULL) {
	  auth = newURL->auth ? newURL->auth : curURL->auth;
	} else auth = newURL->auth;

/*	sprintf(pathfile, "/%s%s%s",  DPS_NULL2EMPTY(path), DPS_NULL2EMPTY(fname), DPS_NULL2EMPTY(query_string));*/
	pathfile[0] = '/'; 
	dps_strcpy(pathfile + 1, DPS_NULL2EMPTY(path)); dps_strcat(pathfile, DPS_NULL2EMPTY(fname)); dps_strcat(pathfile, DPS_NULL2EMPTY(query_string));
	if (anchor != NULL) {
	  dps_strcat(pathfile, anchor);
	}
		
	DpsURLNormalizePath(pathfile);

	if (!strcasecmp(DPS_NULL2EMPTY(schema), "mailto") 
	    || !strcasecmp(DPS_NULL2EMPTY(schema), "javascript")
	    || !strcasecmp(DPS_NULL2EMPTY(schema), "feed")
	    ) {
	        *str = (char*)DpsMalloc(dps_strlen(DPS_NULL2EMPTY(schema)) + dps_strlen(DPS_NULL2EMPTY(newURL->specific)) + 4);
		if (*str == NULL) return;
/*		sprintf(*str, "%s:%s", DPS_NULL2EMPTY(schema), DPS_NULL2EMPTY(newURL->specific));*/
		dps_strcpy(*str, DPS_NULL2EMPTY(schema)); dps_strcat(*str, ":"); dps_strcat(*str, DPS_NULL2EMPTY(newURL->specific));
	} else if(/*!strcasecmp(DPS_NULL2EMPTY(schema), "file") ||*/ !strcasecmp(DPS_NULL2EMPTY(schema), "htdb")) {
	        *str = (char*)DpsMalloc(dps_strlen(DPS_NULL2EMPTY(schema)) + dps_strlen(pathfile) + 4);
		if (*str == NULL) return;
/*		sprintf(*str, "%s:%s", DPS_NULL2EMPTY(schema), pathfile);*/
		dps_strcpy(*str, DPS_NULL2EMPTY(schema)); dps_strcat(*str, ":"); dps_strcat(*str, pathfile);
	}else{
	  *str = (char*)DpsMalloc(dps_strlen(DPS_NULL2EMPTY(schema)) + dps_strlen(pathfile) + dps_strlen(DPS_NULL2EMPTY(hostname)) + dps_strlen(DPS_NULL2EMPTY(auth)) + 8);
	  if (*str == NULL) return;
/*		sprintf(*str, "%s://%s%s", DPS_NULL2EMPTY(schema), DPS_NULL2EMPTY(hostinfo), pathfile);*/
	  dps_strcpy(*str, DPS_NULL2EMPTY(schema)); dps_strcat(*str, "://"); 
	  if (auth) {
	    dps_strcat(*str, auth); dps_strcat(*str,"@");
	  }
	  dps_strcat(*str, DPS_NULL2EMPTY(hostname)); dps_strcat(*str, pathfile);
	}
	
	if(!strncmp(*str, "ftp://", 6) && (strstr(*str, ";type=")))
		*(strstr(*str, ";type")) = '\0';
	DPS_FREE(pathfile);

	if (ReverseAliasFlag) {
	  const char *alias_prog = DpsVarListFindStr(&Indexer->Vars, "ReverseAliasProg", NULL);
	  
	  if (alias_prog) {
	    int		result;
	    aliassize = 256 + 2 * dps_strlen(*str);
	    alias = (char*)DpsRealloc(alias, aliassize);
	    if (alias == NULL) {
	      DpsLog(Indexer, DPS_LOG_ERROR, "No memory (%d bytes). %s line %d", aliassize, __FILE__, __LINE__);
	      goto ret;
	    }
	    alias[0] = '\0';
	    result = DpsAliasProg(Indexer, alias_prog, *str, alias, aliassize - 1);
	    DpsLog(Indexer, DPS_LOG_EXTRA, "ReverseAliasProg result: '%s'", alias);
	    if(result != DPS_OK) goto ret;
	    DPS_FREE(*str);
	    *str = (char*)DpsStrdup(alias);
	  }

	  DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	  for(cascade = 0; ((Alias=DpsMatchListFind(&Indexer->Conf->ReverseAliases,*str,nparts,Parts))) && (cascade < 1024); cascade++) {
	        aliassize = dps_strlen(Alias->arg) + dps_strlen(Alias->pattern) + dps_strlen(*str) + 128;
		alias = (char*)DpsRealloc(alias, aliassize);
		if (alias == NULL) {
		  DpsLog(Indexer, DPS_LOG_ERROR, "No memory (%d bytes). %s line %d", aliassize, __FILE__, __LINE__);
		  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		  goto ret;
		}
		DpsMatchApply(alias,aliassize,*str,Alias->arg,Alias,nparts,Parts);
		if(alias[0]){
		  DpsLog(Indexer,DPS_LOG_DEBUG,"ReverseAlias%d: pattern:%s, arg:%s -> '%s'", cascade, Alias->pattern, Alias->arg, alias);
		  DPS_FREE(*str);
		  *str = (char*)DpsStrdup(alias);
		} else break;
		if (Alias->last) break;
	  }
	  DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	}

	/* Cat an anchor if exist */
	if((anchor=strstr(*str, "#"))) *anchor = 0;

ret:	
	DPS_FREE(alias);

}

