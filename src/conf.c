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
#include "dps_spell.h"
#include "dps_proto.h"
#include "dps_url.h"
#include "dps_parser.h"
#include "dps_conf.h"
#include "dps_log.h"
#include "dps_hrefs.h"
#include "dps_robots.h"
#include "dps_utils.h"
#include "dps_host.h"
#include "dps_server.h"
#include "dps_alias.h"
#include "dps_search_tl.h"
#include "dps_env.h"
#include "dps_match.h"
#include "dps_stopwords.h"
#include "dps_guesser.h"
#include "dps_unicode.h"
#include "dps_synonym.h"
#include "dps_acronym.h"
#include "dps_vars.h"
#include "dps_db.h"
#include "dps_agent.h"
#include "dps_db_int.h"
#include "dps_chinese.h"
#include "dps_mkind.h"
#include "dps_indexer.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#if defined(WITH_IDN) && !defined(APACHE1) && !defined(APACHE2)
#include <idna.h>
#elif defined(WITH_IDNKIT) && !defined(APACHE1) && !defined(APACHE2)
#include <idn/api.h>
#endif

/*
#define TRACE_CMDS 1
*/

static int EnvLoad(DPS_CFG *Cfg,const char *cname);

/****************************  Load Configuration **********************/

int DpsSearchMode(const char * mode){
	if(!mode)return(DPS_MODE_ALL);
	if(!strcmp(mode,"all"))return(DPS_MODE_ALL);
	if(!strcmp(mode,"near"))return(DPS_MODE_NEAR);
	if(!strcmp(mode,"any"))return(DPS_MODE_ANY);
	if(!strcmp(mode,"bool"))return(DPS_MODE_BOOL);
	if(!strcmp(mode,"phrase"))return(DPS_MODE_PHRASE);
	return(DPS_MODE_ALL);
}


int DpsMatchMode(const char * mode){
	if(!mode)return(DPS_MATCH_FULL);
	if(!strcmp(mode,"wrd"))return(DPS_MATCH_FULL);
	if(!strcmp(mode,"full"))return(DPS_MATCH_FULL);
	if(!strcmp(mode,"beg"))return(DPS_MATCH_BEGIN);
	if(!strcmp(mode,"end"))return(DPS_MATCH_END);
	if(!strcmp(mode,"sub"))return(DPS_MATCH_SUBSTR);
	return(DPS_MATCH_FULL);
}


int DpsGroupBySiteMode(const char *mode) {
  if(!mode)return(DPS_GROUP_NO);
  if (*mode == 'y' || *mode == 'Y') return DPS_GROUP_YES;
  if (*mode == 'f' || *mode == 'F') return DPS_GROUP_FULL;
  return DPS_GROUP_NO;
}


__C_LINK const char * __DPSCALL DpsFollowStr(int method) {
	switch(method){
		case DPS_FOLLOW_NO:		return "No";  /* was: "Page" */
		case DPS_FOLLOW_PATH:		return "Path";
		case DPS_FOLLOW_SITE:		return "Site";
		case DPS_FOLLOW_WORLD:		return "World";
	}
	return "<Unknown follow type>";
}


int DpsFollowType(const char * follow){
	if(!follow)return DPS_FOLLOW_UNKNOWN;
	if(!strcasecmp(follow,"no"))return(DPS_FOLLOW_NO);
	if(!strcasecmp(follow,"nofollow"))return(DPS_FOLLOW_NO);
/*	if(!strcasecmp(follow,"page"))return(DPS_FOLLOW_NO);*/
	if(!strcasecmp(follow,"yes"))return(DPS_FOLLOW_PATH);
	if(!strcasecmp(follow,"path"))return(DPS_FOLLOW_PATH);
	if(!strcasecmp(follow,"site"))return(DPS_FOLLOW_SITE);
	if(!strcasecmp(follow,"world"))return(DPS_FOLLOW_WORLD);
	return(DPS_FOLLOW_UNKNOWN);
}

const char *DpsMethodStr(int method){
	switch(method){
		case DPS_METHOD_DISALLOW:	return "Disallow";
		case DPS_METHOD_GET:		return "Allow";
		case DPS_METHOD_CHECKMP3ONLY:	return "CheckMP3Only";
		case DPS_METHOD_CHECKMP3:	return "CheckMP3";
		case DPS_METHOD_HEAD:		return "CheckOnly";
		case DPS_METHOD_HREFONLY:	return "HrefOnly";
		case DPS_METHOD_VISITLATER:	return "Skip";
	        case DPS_METHOD_INDEX:          return "IndexIf";
	        case DPS_METHOD_NOINDEX:        return "NoIndexIf";
	        case DPS_METHOD_TAG:            return "TagIf";
	        case DPS_METHOD_CATEGORY:       return "CategoryIf";
	        case DPS_METHOD_STORE:          return "Store";
	        case DPS_METHOD_NOSTORE:        return "NoStore";
	}
	return "<Unknown method>";
}

int DpsMethod(const char *s){
        if (s == NULL) return DPS_METHOD_UNKNOWN;
	else if(!strcasecmp(s,"Disallow"))	return DPS_METHOD_DISALLOW;
	else if(!strcasecmp(s,"Allow"))		return DPS_METHOD_GET;
	else if(!strcasecmp(s,"CheckMP3Only"))	return DPS_METHOD_CHECKMP3ONLY;
	else if(!strcasecmp(s,"CheckMP3"))	return DPS_METHOD_CHECKMP3;
	else if(!strcasecmp(s,"CheckOnly"))	return DPS_METHOD_HEAD;
	else if(!strcasecmp(s,"CheckOnlyIf"))	return DPS_METHOD_HEAD;
	else if(!strcasecmp(s,"HrefOnly"))	return DPS_METHOD_HREFONLY;
	else if(!strcasecmp(s,"Skip"))		return DPS_METHOD_VISITLATER;
	else if(!strcasecmp(s,"IndexIf"))       return DPS_METHOD_INDEX;
	else if(!strcasecmp(s,"NoIndexIf"))     return DPS_METHOD_NOINDEX;
	else if(!strcasecmp(s,"TagIf"))         return DPS_METHOD_TAG;
	else if(!strcasecmp(s,"CategoryIf"))    return DPS_METHOD_CATEGORY;
	else if(!strcasecmp(s,"Store"))         return DPS_METHOD_STORE;
	else if(!strcasecmp(s,"NoStore"))       return DPS_METHOD_NOSTORE;
	return DPS_METHOD_UNKNOWN;
}

enum dps_prmethod DpsPRMethod(const char *s) {
  if (s == NULL) return DPS_POPRANK_GOO;
  if (!strcasecmp(s, "Goo")) return DPS_POPRANK_GOO;
  else if (!strcasecmp(s, "Neo")) return DPS_POPRANK_NEO;
  return DPS_POPRANK_GOO;
}

dps_uint4 DpsHrefFrom(const char *str) {
  char	*lt;
  char	*tok;
  dps_uint4 result = 0;

  tok = DpsGetStrToken((char*)str, &lt);
	
  while (tok) {
    if (!strncasecmp(tok, "area", 4)) result |= DPS_HREF_FROM_AREA;
    else if (!strncasecmp(tok, "a", 1)) result |= DPS_HREF_FROM_A;
    else if (!strncasecmp(tok, "base", 4)) result |= DPS_HREF_FROM_BASE;
    else if (!strncasecmp(tok, "link", 4)) result |= DPS_HREF_FROM_LINK;
    else if (!strncasecmp(tok, "script", 6)) result |= DPS_HREF_FROM_SCRIPT;
    else if (!strncasecmp(tok, "input", 5)) result |= DPS_HREF_FROM_INPUT;
    else if (!strncasecmp(tok, "frame", 5)) result |= DPS_HREF_FROM_FRAME;
    else if (!strncasecmp(tok, "iframe", 6)) result |= DPS_HREF_FROM_IFRAME;
    else if (!strncasecmp(tok, "img", 3)) result |= DPS_HREF_FROM_IMG;
    tok = DpsGetStrToken(NULL, &lt);
  }
  return result;
}


int DpsWeightFactorsInit(const char *wf, int *res){
	size_t len;
	int flag = 0;
        register size_t sn;
	
	len = dps_strlen(wf);

	if (len == 0) {
	  for(sn = 0; sn < 256; sn++) {
		res[sn] = 1;
	  }
	} else {
	  register const char *p;
	  if (len > 255) len = 255;

	  p = wf + len - 1;
	  for (sn = 1; sn < 256; sn++) {
	    if ((res[sn] = DpsHex2Int((int)*p)) == 0) flag = 1;
	    if (p > wf) p--;
	  }
	}
	return flag;
}

size_t DpsRelEtcName(DPS_ENV *Env, char *res, size_t maxlen, const char *name) {
	size_t		n;
	const char	*dir = DpsVarListFindStr(&Env->Vars, "EtcDir", DPS_CONF_DIR);
	if(name[0]=='/')n = dps_snprintf(res, maxlen, name);
	else		n = dps_snprintf(res, maxlen, "%s%s%s", dir, DPSSLASHSTR, name);
	res[maxlen]='\0';
	return n;
}

size_t DpsRelVarName(DPS_ENV *Env,char *res,size_t maxlen,const char *name){
	size_t		n;
	const char	*dir = DpsVarListFindStr(&Env->Vars, "VarDir", DPS_VAR_DIR);
	if(name[0]=='/')n = dps_snprintf(res, maxlen, name);
	else		n = dps_snprintf(res, maxlen, "%s%s%s", dir, DPSSLASHSTR, name);
	res[maxlen]='\0';
	return n;
}

size_t DpsGetArgs(char *str, char **av, size_t max){
	size_t	ac=0;
	char	*lt;
	char	*tok;
	
	bzero((void*)av, max * sizeof(*av));
	tok=DpsGetStrToken(str,&lt);
	
	while (tok && (ac<max)){
		av[ac]=tok;
		ac++;
		tok=DpsGetStrToken(NULL,&lt);
	}
	return ac;
}


static int add_srv(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_AGENT *Indexer = C->Indexer;
	DPS_CHARSET *cs = NULL;
	size_t	i;
	int	has_alias=0;
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
	DPS_CHARSET *uni_cs;
	char   *ascii = NULL, *uni = NULL;
	DPS_CONV  url_uni;
	DPS_URL *SrvURL = NULL, *PunyURL = NULL;
	size_t len;
#endif
	
	if(!(C->flags & DPS_FLAG_ADD_SERV)) {
	  return DPS_OK;
	}
	
	C->Srv->command = 'S';
	C->Srv->ordre = ++C->ordre;
	bzero((void*)&C->Srv->Match, sizeof(C->Srv->Match));
	C->Srv->Match.nomatch=0;
	C->Srv->Match.case_sense = 1;
	DpsVarListReplaceStr(&C->Srv->Vars, "Method", "Allow");

	if(!strcasecmp(av[0],"Server")){
		C->Srv->Match.match_type=DPS_MATCH_BEGIN;
	}else
	if(!strcasecmp(av[0],"Subnet")){
		C->Srv->Match.match_type=DPS_MATCH_SUBNET;
/*		Conf->Servers.have_subnets=1;*/
	}else{
		C->Srv->Match.match_type=DPS_MATCH_WILD;
	}
	
	DpsVarListReplaceInt(&C->Srv->Vars,"Follow",DPS_FOLLOW_PATH);
	
	for(i=1; i<ac; i++){
		int	o;
		
		if(DPS_FOLLOW_UNKNOWN!=(o=DpsFollowType(av[i])))DpsVarListReplaceInt(&C->Srv->Vars,"Follow",o);
		else
		if(DPS_METHOD_UNKNOWN!=(o=DpsMethod(av[i])))DpsVarListReplaceStr(&C->Srv->Vars,"Method",av[i]);
		else
		if(!strcasecmp(av[i],"nocase"))C->Srv->Match.case_sense=0;
		else
		if(!strcasecmp(av[i],"case"))C->Srv->Match.case_sense=1;
		else
		if(!strcasecmp(av[i],"match"))C->Srv->Match.nomatch=0;
		else
		if(!strcasecmp(av[i],"nomatch"))C->Srv->Match.nomatch=1;
		else
		if(!strcasecmp(av[i],"string"))C->Srv->Match.match_type=DPS_MATCH_WILD;
		else
		if(!strcasecmp(av[i],"regex"))C->Srv->Match.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i], "page")) C->Srv->Match.match_type = DPS_MATCH_FULL;
		else{
		        if(!C->Srv->Match.pattern) {
				C->Srv->Match.pattern = (char*)DpsStrdup(av[i]);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if(((C->Srv->Match.match_type==DPS_MATCH_BEGIN) || (C->Srv->Match.match_type==DPS_MATCH_FULL) ) && (C->Srv->Match.pattern[0])) {
				  uni_cs = DpsGetCharSet("UTF8");
				  cs = DpsGetCharSet(DpsVarListFindStr(&C->Srv->Vars, "RemoteCharset", DpsVarListFindStr(&C->Srv->Vars, "URLCharset", "iso8859-1")));
				  DpsConvInit(&url_uni, cs, uni_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);
				  SrvURL = DpsURLInit(NULL);
				  PunyURL = DpsURLInit(NULL);
				  DpsURLParse(SrvURL, av[i]);
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
				    ascii = (char*)DpsMalloc(len);
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
				    RelLink(Indexer, SrvURL, PunyURL, &C->Srv->Match.idn_pattern, 0);
				  }
/*				  DPS_FREE(ascii);  will be freed later with DpsURLFree(PunyURL) */
				  DPS_FREE(uni); 
				  DpsURLFree(SrvURL); 
				  DpsURLFree(PunyURL);
				}
#endif
			} else
			if(!has_alias){
				has_alias=1;
				DpsVarListReplaceStr(&C->Srv->Vars,"Alias",av[i]);
			}else{
			  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "too many argiments: '%s'", av[i]);
			  return DPS_ERROR;
			}
		}
	}
	if (!C->Srv->Match.pattern) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr), "too few argiments in '%s' command", av[0]);
	  return DPS_ERROR;
	}
	if(DPS_OK != DpsServerAdd(Indexer, C->Srv)) {
		char * s_err;
		s_err = (char*)DpsStrdup(Conf->errstr);
		dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "%s [%s:%d]", s_err, __FILE__, __LINE__);
		DPS_FREE(s_err);
		DpsMatchFree(&C->Srv->Match);
		return DPS_ERROR;
	}
	if(((C->Srv->Match.match_type == DPS_MATCH_BEGIN) || (C->Srv->Match.match_type == DPS_MATCH_FULL) ) && 
	   (C->Srv->Match.pattern[0]) && (C->flags&DPS_FLAG_ADD_SERVURL)) {
		DPS_HREF Href;
		int charset_id;
		
		if (cs == NULL) cs = DpsGetCharSet(DpsVarListFindStr(&C->Srv->Vars, "RemoteCharset", DpsVarListFindStr(&C->Srv->Vars, "URLCharset", "iso8859-1")));

		bzero((void*)&Href, sizeof(Href));
		Href.url=C->Srv->Match.pattern;
		Href.method=DPS_METHOD_GET;
		Href.site_id = C->Srv->site_id;
		Href.server_id = C->Srv->site_id;
		if (cs != NULL) charset_id = cs->id;
		else {
		  if (Indexer->Conf->lcs != NULL) charset_id = Indexer->Conf->lcs->id;
		  else charset_id = 0;
		}
		Href.charset_id = charset_id;
		Href.checked = 1;
		DpsHrefListAdd(Indexer, &Indexer->Hrefs, &Href);
		if (Indexer->Hrefs.nhrefs > 1024) DpsStoreHrefs(Indexer);
	}
	DpsMatchFree(&C->Srv->Match);
	/*	DpsMatchListFree(&C->Srv->HTDBsec);*/
	DpsVarListDel(&C->Srv->Vars,"AuthBasic");
	DpsVarListDel(&C->Srv->Vars,"Alias");
	return DPS_OK;
}

static int add_alias(void *Cfg, size_t ac,char **av){
	DPS_CFG		*C=(DPS_CFG*)Cfg;
	DPS_ENV		*Conf = C->Indexer->Conf;
	DPS_MATCH	Alias;
	size_t		i;

	if(!(C->flags & DPS_FLAG_ADD_SERV))
		return DPS_OK;

	DpsMatchInit(&Alias);
	Alias.match_type = DPS_MATCH_BEGIN;
	
	for(i=1; i<ac; i++){
		if(!strcasecmp(av[i],"regex"))
			Alias.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"regexp"))
			Alias.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"case"))
			Alias.case_sense=1;
		else
		if(!strcasecmp(av[i],"nocase"))
			Alias.case_sense=0;
		else
		if(!strcasecmp(av[i],"last"))
			Alias.last = 1;
		else
		if(!Alias.pattern){
			Alias.pattern=av[i];
		}else{
			char		err[120]="";
			DPS_MATCHLIST	*L=NULL;
			
			Alias.arg=av[i];
			
			if(!strcasecmp(av[0],"Alias"))L=&Conf->Aliases;
			if(!strcasecmp(av[0],"ReverseAlias"))L=&Conf->ReverseAliases;
			
			if(DPS_OK != DpsMatchListAdd(NULL, L, &Alias, err, sizeof(err), 0)) {
				dps_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
				return DPS_ERROR;
			}
		}
	}
	if(!Alias.arg){
		dps_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"too few arguments");
		return DPS_ERROR;
	}
	return DPS_OK;
}


static int add_filter(void *Cfg, size_t ac, char **av) {
	DPS_CFG		*C=(DPS_CFG*)Cfg;
	DPS_ENV		*Conf=C->Indexer->Conf;
	DPS_MATCH	M;
	size_t		i;
	
	if(!(C->flags & DPS_FLAG_ADD_SERV)) return DPS_OK;
	
	DpsMatchInit(&M);
	M.match_type=DPS_MATCH_WILD;
	M.case_sense=1;
	
	C->ordre++;
	for(i=1; i<ac ; i++){
		if(!strcasecmp(av[i],"case"))M.case_sense=1;
		else
		if(!strcasecmp(av[i],"nocase"))M.case_sense=0;
		else
		if(!strcasecmp(av[i],"regex"))M.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"regexp"))M.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"string"))M.match_type=DPS_MATCH_WILD;
		else
		if(!strcasecmp(av[i],"nomatch"))M.nomatch=1;
		else
		if(!strcasecmp(av[i],"match"))M.nomatch=0;
		else{
			char		err[120]="";
			
			M.arg = av[0];
			M.pattern = av[i];
			
			if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->Filters, &M, err, sizeof(err), ++C->ordre)) {
				dps_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
				return DPS_ERROR;
			}
		}
	}
	return DPS_OK;
}


static int add_store_filter(void *Cfg, size_t ac, char **av) {
	DPS_CFG		*C = (DPS_CFG*)Cfg;
	DPS_ENV		*Conf = C->Indexer->Conf;
	DPS_MATCH	M;
	size_t		i;
	
	if(!(C->flags & DPS_FLAG_ADD_SERV)) return DPS_OK;
	
	DpsMatchInit(&M);
	M.match_type = DPS_MATCH_WILD;
	M.case_sense = 1;
	
	C->ordre++;
	for(i=1; i<ac ; i++){
		if(!strcasecmp(av[i],"case"))M.case_sense=1;
		else
		if(!strcasecmp(av[i],"nocase"))M.case_sense=0;
		else
		if(!strcasecmp(av[i],"regex"))M.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"regexp"))M.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"string"))M.match_type=DPS_MATCH_WILD;
		else
		if(!strcasecmp(av[i],"nomatch"))M.nomatch=1;
		else
		if(!strcasecmp(av[i],"match"))M.nomatch=0;
		else{
			char		err[120]="";
			
			M.arg = av[0];
			M.pattern = av[i];
			
			if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->StoreFilters, &M, err, sizeof(err), ++C->ordre)) {
				dps_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
				return DPS_ERROR;
			}
		}
	}
	return DPS_OK;
}



static int add_section_filter(void *Cfg, size_t ac,char **av) {
  DPS_CFG    *C=(DPS_CFG*)Cfg;
  DPS_ENV    *Conf=C->Indexer->Conf;
  DPS_MATCH  M;
  size_t     i;
  char       section_flag = 1;
  
  if(!(C->flags & DPS_FLAG_ADD_SERV)) return DPS_OK;
  
  DpsMatchInit(&M);
  M.match_type=DPS_MATCH_WILD;
  M.case_sense=1;
  
  C->ordre++;
  for( i = 1; i < ac; i++) {
    if(!strcasecmp(av[i],"case"))M.case_sense=1;
    else
    if(!strcasecmp(av[i],"nocase"))M.case_sense=0;
    else
    if(!strcasecmp(av[i],"regex"))M.match_type=DPS_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"regexp"))M.match_type=DPS_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"string"))M.match_type=DPS_MATCH_WILD;
    else
    if(!strcasecmp(av[i],"nomatch"))M.nomatch=1;
    else
    if(!strcasecmp(av[i],"match"))M.nomatch=0;
    else
    if (section_flag) {
      section_flag = 0;
      M.section = av[i];

    } else {

      char    err[128] = "";

      M.arg = av[0];
      M.pattern = av[i];

      if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->SectionFilters, &M, err, sizeof(err), ++C->ordre)) {
        dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "%s", err);
        return DPS_ERROR;
      }
    }
  }

  if (section_flag) {
    dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "No section given for %s", av[0]);
    return DPS_ERROR;
  }
  return DPS_OK;
}


static int add_subsection_match(void *Cfg, size_t ac,char **av) {
  DPS_CFG    *C = (DPS_CFG*)Cfg;
  DPS_ENV    *Conf = C->Indexer->Conf;
  DPS_MATCH  M;
  size_t     i;
  char       section_flag = 1;
  
  if(!(C->flags & DPS_FLAG_ADD_SERV)) return DPS_OK;
  
  DpsMatchInit(&M);
  M.match_type=DPS_MATCH_WILD;
  M.case_sense=1;
  
  C->ordre++;
  for( i = 2; i < ac; i++) {
    if(!strcasecmp(av[i],"case"))M.case_sense=1;
    else
    if(!strcasecmp(av[i],"nocase"))M.case_sense=0;
    else
    if(!strcasecmp(av[i],"regex"))M.match_type=DPS_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"regexp"))M.match_type=DPS_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"string"))M.match_type=DPS_MATCH_WILD;
    else
    if(!strcasecmp(av[i],"nomatch"))M.nomatch=1;
    else
    if(!strcasecmp(av[i],"match"))M.nomatch=0;
    else
    if(!strcasecmp(av[i],"loose"))M.loose=1;
    else
    if (section_flag == 1) {
      section_flag = 0;
      M.section = av[i];

    } else {

      char    err[128] = "";
      char    buf[64];

      M.arg = av[0];
      if (strcasecmp(av[0], "CategoryIf")) {
	M.subsection = av[1];
      } else {
	unsigned int cid = DpsGetCategoryId(Conf, av[1]);
	dps_snprintf(buf, 64, "%u", cid);
	M.subsection = buf;
      }
      M.pattern = av[i];

      if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->SubSectionMatch, &M, err, sizeof(err), ++C->ordre)) {
        dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "%s", err);
        return DPS_ERROR;
      }
    }
  }

  if (section_flag) {
    dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "No value given for %s", av[0]);
    return DPS_ERROR;
  }
  return DPS_OK;
}





static int add_type(void *Cfg, size_t ac,char **av){
	DPS_CFG		*C=(DPS_CFG*)Cfg;
	DPS_ENV		*Conf=C->Indexer->Conf;
	DPS_MATCH	M;
	size_t		i;
	int		rc=DPS_OK;
	char err[128];
	
	if(!(C->flags & DPS_FLAG_ADD_SERV))
		return DPS_OK;
	
	DpsMatchInit(&M);
	M.match_type=DPS_MATCH_WILD;
	
	for (i=1; i<ac; i++){
		if(!strcasecmp(av[i],"regex"))M.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"regexp"))M.match_type=DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i],"string"))M.match_type=DPS_MATCH_WILD;
		else
		if(!strcasecmp(av[i],"case"))M.case_sense=1;
		else
		if(!strcasecmp(av[i],"nocase"))M.case_sense=0;
		else
		if(!M.arg)
			M.arg=av[i];
		else{
			M.pattern=av[i];
			if(DPS_OK != (rc = DpsMatchListAdd(NULL, &Conf->MimeTypes,&M,err,sizeof(err), 0))) {
				dps_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
				return rc;
			}
		}
	}
	return rc;
}

static int add_parser(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_PARSER P;
	P.from_mime=av[1];
	P.to_mime=av[2];
	P.cmd = DPS_NULL2EMPTY(av[3]);
	DpsParserAdd(&Conf->Parsers,&P);
	return DPS_OK;
}


static int add_section(void *Cfg, size_t ac,char **av){
	char err[128] = "";
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_VAR S;
	DPS_MATCH M;
	size_t shift = 0;

	bzero((void*)&S, sizeof(S));
	S.name = av[1];
	S.section = atoi(av[2]);
	S.maxlen = ((ac > 2) && av[3]) ? atoi(av[3]) : 0;

	DpsMatchInit(&M);
	M.match_type = DPS_MATCH_REGEX;
	M.case_sense = 0;

	if (ac >= 4) {
	  while(shift + 4 < ac) {
	    if (!strcasecmp(av[4 + shift], "strict")) S.strict = 1;
	    else if (!strcasecmp(av[4 + shift], "single")) S.single = 1;
	    else if (!strcasecmp(av[4 + shift], "regex")) M.match_type = DPS_MATCH_REGEX;
	    else if (!strcasecmp(av[4 + shift], "string")) M.match_type = DPS_MATCH_WILD;
	    else if (!strcasecmp(av[4 + shift], "substr")) M.match_type = DPS_MATCH_SUBSTR;
	    else if (!strcasecmp(av[4 + shift], "case")) M.case_sense = 1;
	    else if (!strcasecmp(av[4 + shift], "nocase")) M.case_sense = 0;
	    else if (shift + 4 < ac - 2) {
	      dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "unknown option %s in arguments of for Section command", av[shift + 4]);
	      return DPS_ERROR;
	    } else break;
	    shift++;
	  }
	}

	if (ac - shift == 6) {

/*	  if(!(C->flags & DPS_FLAG_ADD_SERV))
	    return DPS_OK;*/

	  M.section = av[1];
	  M.pattern = av[shift + 4];
	  M.arg = av[shift + 5];
	  if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->SectionMatch, &M, err, sizeof(err), ++C->ordre)) {
	    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "SectionMatch Add: %s", err);
	    return DPS_ERROR;
	  }
	} else if (ac > 4 + shift) {
	      dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "wrong number of arguments for Section command");
	      return DPS_ERROR;
	}

	DpsVarListReplace(&Conf->Sections, &S);
	return DPS_OK;
}


static int add_sectionsql(void *Cfg, size_t ac,char **av){
	char err[128] = "";
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_VAR S;
	DPS_MATCH M;
	size_t shift = 0;

	if (ac < 4 || ac > 8) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "wrong number (%d) of arguments for SectionSQL command", ac);
	  return DPS_ERROR;
	}

	bzero((void*)&S, sizeof(S));

	if (ac == 5) {
	  if (strcasecmp(av[4], "strict") == 0) {
	    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "fourth arguments is \"strict\", perhaps SQLtemplate is missed for SectionSQL command");
	    return DPS_ERROR;
	  }
	}

	S.name = av[1];
	S.section = atoi(av[2]);
	S.maxlen = ((ac > 2) && av[3]) ? atoi(av[3]) : 0;

	if (ac >= 4) {
	  while(shift + 4 < ac) {
	    if (!strcasecmp(av[4 + shift], "strict")) S.strict = 1;
	    else if (!strcasecmp(av[4 + shift], "single")) S.single = 1;
	    else if (shift + 4 < ac - 2) {
	      dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "unknown option %s in arguments of for SectionSQL command", av[shift + 4]);
	      return DPS_ERROR;
	    } else break;
	    shift++;
	  }
	}

	if (ac - shift == 6 || ac - shift == 5) {

	  if(!(C->flags & DPS_FLAG_ADD_SERV))
	    return DPS_OK;

	  DpsMatchInit(&M);
	  M.match_type = DPS_MATCH_BEGIN;
	  M.case_sense = 1;
	  M.section = av[1];
	  M.pattern = ".";
	  M.arg = av[shift + 4];
	  M.dbaddr = (ac == 5 + shift) ? NULL : av[5 + shift]; 
	  if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->SectionSQLMatch, &M, err, sizeof(err), ++C->ordre)) {
	    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "SectionSQLMatch Add: %s", err);
	    return DPS_ERROR;
	  }
	} else if (ac > 4 + shift) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "wrong number (%d,%d) of arguments for SectionSQL command", ac, shift);
	      return DPS_ERROR;
	}

	DpsVarListReplace(&Conf->Sections, &S);
	return DPS_OK;
}


static int add_body_pattern(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_MATCH M;
	char err[128] = "";

	DpsMatchInit(&M);
	M.match_type = DPS_MATCH_REGEX;
	M.case_sense = 1;
	M.section = "body";
	M.pattern = av[1];
	M.arg = av[2];
	if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->BodyPatterns, &M, err, sizeof(err), ++C->ordre)) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "%s", err);
	  return DPS_ERROR;
	}
	return DPS_OK;
}

static int add_body_brackets(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_MATCH M;
	char err[128] = "";

	DpsMatchInit(&M);
	M.match_type = DPS_MATCH_SUBSTR;
	M.case_sense = 1;
	M.section = "body";
	M.pattern = av[1];
	M.arg = av[2];
	if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->BodyPatterns, &M, err, sizeof(err), ++C->ordre)) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "%s", err);
	  return DPS_ERROR;
	}
	return DPS_OK;
}



static int add_hrefsection(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_VAR S;
	DPS_MATCH M;
	char err[128] = "";

	if (ac == 3) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "two arguments isn't supported for HrefSection command");
	  return DPS_ERROR;
	}

	bzero((void*)&S, sizeof(S));
	S.name = av[1];
	S.section = 0;
	S.maxlen = 0;

	if (ac == 4) {

	  if(!(C->flags & DPS_FLAG_ADD_SERV))
	    return DPS_OK;
	
	  DpsMatchInit(&M);
	  M.match_type = DPS_MATCH_REGEX;
	  M.case_sense = 1;
	  M.section = av[1];
	  M.pattern = av[2];
	  M.arg = av[3];
	  if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->HrefSectionMatch, &M, err, sizeof(err), ++C->ordre)) {
	    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "%s", err);
	    return DPS_ERROR;
	  }
	}
	DpsVarListReplace(&Conf->HrefSections, &S);
	return DPS_OK;
}

static int add_actionsql(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	DPS_MATCH M;
	size_t shift = 0;
	char err[128] = "";

	if (ac < 4 || ac > 6) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "wrong number of arguments for ActionSQL command");
	  return DPS_ERROR;
	}

	DpsMatchInit(&M);
	M.match_type = DPS_MATCH_REGEX;
	M.case_sense = 1;
	M.subsection = "i";
	if (strcasecmp(av[1], "delete") == 0) {
	  shift = 1;
	  M.subsection = "d";
	} else if (strcasecmp(av[1], "insert") == 0) {
	  shift = 1;
	  M.subsection = "i";
	} else if (strcasecmp(av[1], "add") == 0) {
	  shift = 1;
	  M.subsection = "i";
	} else if (strcasecmp(av[1], "update") == 0) {
	  shift = 1;
	  M.subsection = "u";
	}
	M.section = av[1 + shift];
	M.pattern = av[2 + shift];
	M.arg = av[3 + shift];
	M.dbaddr = (ac == 4 + shift) ? NULL : av[4 + shift];

	if(DPS_OK != DpsMatchListAdd(C->Indexer, &Conf->ActionSQLMatch, &M, err, sizeof(err), ++C->ordre)) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "%s", err);
	  return DPS_ERROR;
	}
	return DPS_OK;
}


static int do_include(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	if(C->level<5){
		int	rc;
		char	fname[PATH_MAX];
		DpsRelEtcName(C->Indexer->Conf, fname, sizeof(fname)-1, av[1]);
		C->level++;
		rc=EnvLoad(C,fname);
		C->level--;
		return rc;
	}else{
	  dps_snprintf(C->Indexer->Conf->errstr, sizeof(C->Indexer->Conf->errstr) - 1, "too big (%d) level in included files", C->level);
	  return DPS_ERROR;
	}
	return DPS_OK;
}

static int add_quffix(void *Cfg, size_t ac,char **av) {
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	
	if(C->flags & DPS_FLAG_SPELL) {
		char	fname[PATH_MAX];
		DpsRelEtcName(Conf,fname,sizeof(fname)-1,av[3]);
		if(DpsImportQuffixes(Conf,av[1],av[2],fname)){
		  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Can't load quffix file :%s", fname);
		  return DPS_ERROR;
		}
	}
	return DPS_OK;
}

static int add_affix(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	
	if(C->flags&DPS_FLAG_SPELL){
		char	fname[PATH_MAX];
		DpsRelEtcName(Conf,fname,sizeof(fname)-1,av[3]);
		if(DpsImportAffixes(Conf,av[1],av[2],fname)){
		  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Can't load affix :%s", fname);
		  return DPS_ERROR;
		}
	}
	return DPS_OK;
}

static int add_spell(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	
	if(C->flags&DPS_FLAG_SPELL){
		char	fname[PATH_MAX];
		DpsRelEtcName(Conf,fname,sizeof(fname)-1,av[3]);
		if(DpsImportDictionary(C->Indexer, av[1], av[2], fname, 0, "")) {
		  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Can't load dictionary :%s", fname);
		  return DPS_ERROR;
		}
	}
	return DPS_OK;
}

static int add_stoplist(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	int	res;
	char	fname[PATH_MAX];
	DpsRelEtcName(Conf,fname,sizeof(fname)-1,av[1]);
	res=DpsStopListLoad(Conf,fname);
	return res;
}

static int add_langmap(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	int	res=DPS_OK;
	if(C->flags&DPS_FLAG_LOAD_LANGMAP){
		char	fname[PATH_MAX];
		DpsRelEtcName(Conf,fname,sizeof(fname)-1,av[1]);
		res = DpsLoadLangMapFile(&Conf->LangMaps, fname);
	}
	return res;
}

static int add_synonym(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	int	res=DPS_OK;
	if(C->flags&DPS_FLAG_SPELL){
		char	fname[PATH_MAX];
		DpsRelEtcName(Conf, fname, sizeof(fname)-1, av[1]);
		res=DpsSynonymListLoad(Conf,fname);
	}
	return res;
}

static int add_acronym(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	int	res = DPS_OK;
	if(C->flags & DPS_FLAG_SPELL) {
		char	fname[PATH_MAX];
		DpsRelEtcName(Conf, fname, sizeof(fname)-1, av[1]);
		res = DpsAcronymListLoad(C->Indexer, fname);
	}
	return res;
}

static int add_chinese(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	char	fname[PATH_MAX];
	if(C->flags & DPS_FLAG_ADD_SERV){
	  DpsRelEtcName(Conf, fname, sizeof(fname)-1, (av[2] != NULL) ? av[2] : "mandarin.freq" );
	  return DpsChineseListLoad(C->Indexer, &Conf->Chi, (av[1] != NULL) ? av[1] : "GB2312", fname );
	}
	return DPS_OK;
}
static int add_thai(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	char	fname[PATH_MAX];
	if(C->flags & DPS_FLAG_ADD_SERV){
	  DpsRelEtcName(Conf, fname, sizeof(fname)-1, (av[2] != NULL) ? av[2] : "thai.freq" );
	  return DpsChineseListLoad(C->Indexer, &Conf->Thai, (av[1] != NULL) ? av[1] : "tis-620", fname );
	}
	return DPS_OK;
}
static int add_korean(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	char	fname[PATH_MAX];
	if(C->flags & DPS_FLAG_ADD_SERV){
	  DpsRelEtcName(Conf, fname, sizeof(fname)-1, (av[2] != NULL) ? av[2] : "korean.freq" );
	  return DpsChineseListLoad(C->Indexer, &Conf->Korean, (av[1] != NULL) ? av[1] : "euc-kr", fname);
	}
	return DPS_OK;
}
				

static int add_url(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_AGENT *Indexer = C->Indexer;
	
	if (C->flags & DPS_FLAG_ADD_SERVURL) {
		char		*al = NULL;
		DPS_SERVER	*Srv;
		DPS_HREF	Href;
		int charset_id;
		DPS_CHARSET *cs = DpsGetCharSet(DpsVarListFindStr(&C->Srv->Vars, "RemoteCharset", 
								  DpsVarListFindStr(&C->Srv->Vars, "URLCharset", "iso-8859-1")));

		if((Srv = DpsServerFind(Indexer, 0, av[1], cs->id, &al))) {
		        DPS_CHARSET *s_cs = DpsGetCharSet(DpsVarListFindStr(&Srv->Vars, "RemoteCharset", 
									    DpsVarListFindStr(&Srv->Vars, "URLCharset", "iso-8859-1")));
			if (s_cs == NULL) s_cs = cs;
			DpsHrefInit(&Href);
			Href.url=av[1];
			Href.method=DPS_METHOD_GET;
			if (s_cs != NULL) charset_id = s_cs->id;
			else {
			  if (Indexer->Conf->lcs != NULL) charset_id = Indexer->Conf->lcs->id;
			  else charset_id = 0;
			}
			Href.charset_id = charset_id;
			Href.checked = 1;
			DpsHrefListAdd(Indexer, &Indexer->Hrefs, &Href);
			if (Indexer->Hrefs.nhrefs > 1024) DpsStoreHrefs(Indexer);
		}
		DPS_FREE(al);
	}
	return DPS_OK;
}
		

static int add_srv_db(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	DPS_AGENT *Indexer = C->Indexer;
	DPS_DBLIST      dbl;
	DPS_DB		*db;
	int res, cmd;
	char *dbaddr = NULL;
	size_t i;
	
	if(!(C->flags & DPS_FLAG_ADD_SERV))
		return DPS_OK;
	
	if(!strcasecmp(av[0], "URLDB")) {
	    if(!(C->flags & DPS_FLAG_ADD_SERVURL)) return DPS_OK;
	    cmd = DPS_SRV_ACTION_URLDB;
	    dbaddr = av[1];
	} else {

	  C->Srv->command = 'S';
	  C->Srv->ordre = ++C->ordre;
	  C->Srv->Match.nomatch=0;
	  C->Srv->Match.case_sense = 1;
	  DpsVarListReplaceStr(&C->Srv->Vars, "Method", "Allow");
	  DpsVarListReplaceInt(&C->Srv->Vars, "Follow", DPS_FOLLOW_PATH);

	  if(!strcasecmp(av[0], "ServerDB")) {
	    cmd = DPS_SRV_ACTION_SERVERDB;
	    C->Srv->Match.match_type = DPS_MATCH_BEGIN;
	  }else
	  if(!strcasecmp(av[0], "SubnetDB")) {
	    cmd = DPS_SRV_ACTION_SUBNETDB;
	    C->Srv->Match.match_type = DPS_MATCH_SUBNET;
/*	    Conf->Servers.have_subnets = 1;*/
	  }else{
	    cmd = DPS_SRV_ACTION_REALMDB;
	    C->Srv->Match.match_type=DPS_MATCH_WILD;
	  }

	  for(i=1; i<ac; i++){
		int	o;
		
		if(DPS_FOLLOW_UNKNOWN!=(o=DpsFollowType(av[i]))) DpsVarListReplaceInt(&C->Srv->Vars, "Follow", o);
		else
		if(DPS_METHOD_UNKNOWN!=(o=DpsMethod(av[i]))) DpsVarListReplaceStr(&C->Srv->Vars, "Method", av[i]);
		else
		if(!strcasecmp(av[i],"nocase")) C->Srv->Match.case_sense = 0;
		else
		if(!strcasecmp(av[i],"case")) C->Srv->Match.case_sense = 1;
		else
		if(!strcasecmp(av[i],"match")) C->Srv->Match.nomatch = 0;
		else
		if(!strcasecmp(av[i],"nomatch")) C->Srv->Match.nomatch = 1;
		else
		if(!strcasecmp(av[i],"string")) C->Srv->Match.match_type = DPS_MATCH_WILD;
		else
		if(!strcasecmp(av[i],"regex")) C->Srv->Match.match_type = DPS_MATCH_REGEX;
		else
		if(!strcasecmp(av[i], "page")) C->Srv->Match.match_type = DPS_MATCH_FULL;
		else {
		  if(dbaddr == NULL)
		    dbaddr = av[i];
		  else {
		    dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "too many argiments: '%s'", av[i]);
		    return DPS_ERROR;
		  }
		}
	  }
	}

	DpsDBListInit(&dbl);
	DpsDBListAdd(&dbl, dbaddr, DPS_OPEN_MODE_READ);
	db = dbl.db[0];
		
#ifdef HAVE_SQL
	res = DpsSrvActionSQL(Indexer, C->Srv, cmd, db);
#endif
	if(res != DPS_OK){
	  dps_strncpy(Conf->errstr, db->errstr, sizeof(Conf->errstr));
	}

	DpsDBListFree(&dbl);
	DpsMatchFree(&C->Srv->Match);
	/*	DpsMatchListFree(&C->Srv->HTDBsec);*/
	DpsVarListDel(&C->Srv->Vars,"AuthBasic");
	DpsVarListDel(&C->Srv->Vars,"Alias");
	return DPS_OK;
}
		

static int add_srv_file(void *Cfg, size_t ac, char **av) {
        char fstr[16384];
        char	fname[PATH_MAX];
        struct stat     sb;
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	DPS_AGENT *A = C->Indexer;
	char **newav;
	size_t i, p;
	int url = 0;
	FILE *f;
	char *curch;

	if(!(C->flags & DPS_FLAG_ADD_SERV)) {
	  return DPS_OK;
	}

	newav = (char**)DpsMalloc((ac + 1) * sizeof(char*));
	if (newav == NULL) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Can't alloc %d bytes of memory", (ac + 1) * sizeof(char*));
	  return DPS_ERROR;
	}

	if (!strcasecmp(av[0], "ServerFile")) {
	  newav[0] = "Server";
	} else if (!strcasecmp(av[0], "RealmFile")) {
	  newav[0] = "Realm";
	} else if (!strcasecmp(av[0], "SubnetFile")) {
	  newav[0] = "Subnet";
	} else if (!strcasecmp(av[0], "URLFile")) {
	  newav[0] = "URL";
	  url = 1;
	} else {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Unknown command %s", av[0]);
	  DPS_FREE(newav);
	  return DPS_ERROR;
	}


	for(i = p = 1; i < ac; i++) {
	  if (DPS_FOLLOW_UNKNOWN != DpsFollowType(av[i])) newav[p++] = av[i];
	  else if (DPS_METHOD_UNKNOWN != DpsMethod(av[i])) newav[p++] = av[i];
	  else if (av[i] == NULL || !strcasecmp(av[i], "nocase") || !strcasecmp(av[i], "case") || !strcasecmp(av[i], "match") || !strcasecmp(av[i], "nomatch")
		   || !strcasecmp(av[i], "string") || !strcasecmp(av[i], "regex") || !strcasecmp(av[i], "page")) {
	    newav[p++] = av[i];
	  } else {

	    DpsRelEtcName(Conf, fname, sizeof(fname)-1, av[i]);
	    if (stat(fname, &sb)) {
	      dps_strerror(A, DPS_LOG_ERROR, "Unable to stat file '%s'", fname);
	      DPS_FREE(newav);
	      return DPS_ERROR;
	    }
	    if ((f = fopen(fname, "r")) == NULL) {
	      dps_strerror(A, DPS_LOG_ERROR, "Unable to open file '%s'", fname);
	      DPS_FREE(newav);
	      return DPS_ERROR;
	    }
	    while((fgets(fstr, sizeof(fstr), f)) != NULL) {
	      curch = strchr(fstr, NL_INT);
	      if (curch != NULL) *curch = '\0';
	      newav[p] = fstr;
	      if (url) {
		if (DPS_OK != add_url(Cfg, p + 1, newav)) {
		  DPS_FREE(newav);
		  return DPS_ERROR;
		}
	      } else {
		if (DPS_OK != add_srv(Cfg, p + 1, newav)) {
		  DPS_FREE(newav);
		  return DPS_ERROR;
		}
	      }
	    }
	    fclose(f);
	  }
	}
	DPS_FREE(newav);
	return DPS_OK;
}
		

static int add_srv_table(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	int		res = DPS_OK;
	DPS_DBLIST      dbl;
	DPS_DB		*db;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	
	if(!(C->flags & DPS_FLAG_ADD_SERV)) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_OK;
	}
	
	DpsDBListInit(&dbl);
	DpsDBListAdd(&dbl, av[1], DPS_OPEN_MODE_READ);
	db = dbl.db[0];

#ifdef HAVE_SQL
	res = DpsSrvActionSQL(C->Indexer, NULL, DPS_SRV_ACTION_TABLE, db);
#endif
	if(res != DPS_OK){
	  dps_strncpy(Conf->errstr, db->errstr, sizeof(Conf->errstr));
	}

	DpsDBListFree(&dbl);
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}

static int add_cat_table(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	int		res = DPS_OK;
	DPS_DBLIST      dbl;
	DPS_DB		*db;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	
	if(!(C->flags & DPS_FLAG_ADD_SERV)) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_OK;
	}
	
	DpsDBListInit(&dbl);
	DpsDBListAdd(&dbl, av[1], DPS_OPEN_MODE_READ);
	db = dbl.db[0];

#ifdef HAVE_SQL
	res = DpsSrvActionSQL(C->Indexer, NULL, DPS_SRV_ACTION_CATTABLE, db);
#endif
	if(res != DPS_OK){
	  dps_strncpy(Conf->errstr, db->errstr, sizeof(Conf->errstr));
	}

	DpsDBListFree(&dbl);
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}


static int add_limit(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	char * sc;
	char * nm;
	size_t nm_len;
	int rc = DPS_OK;
	
	if((sc = strchr(av[1],':'))){
		*sc++='\0';
		nm_len = dps_strlen(av[1]) + 24;
		nm=(char*)DpsMalloc(nm_len);
		if (nm == NULL) {
		  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Can't alloc %d bytes, Limit command: %s", nm_len, av[1]);
		  return DPS_ERROR;
		}
		dps_snprintf(nm, nm_len, "Limit-%s", av[1]);

		DpsVarListReplaceStr(&Conf->Vars, nm, sc);

		if(!strcasecmp(sc, "category")) {
		  Conf->Flags.limits |= DPS_LIMIT_CAT;
		} else if(!strcasecmp(sc, "tag")) {
		  Conf->Flags.limits |= DPS_LIMIT_TAG;
		} else if(!strcasecmp(sc, "time")) {
		  Conf->Flags.limits |= DPS_LIMIT_TIME;
		} else if(!strcasecmp(sc, "language")) {
		  Conf->Flags.limits |= DPS_LIMIT_LANG;
		} else if(!strcasecmp(sc, "content")) {
		  Conf->Flags.limits |= DPS_LIMIT_CTYPE;
		} else if(!strcasecmp(sc, "siteid")) {
		  Conf->Flags.limits |= DPS_LIMIT_SITE;
		} else if(!strcasecmp(sc, "link")) {
/*		  Conf->Flags.limits |= DPS_LIMIT_LINK;*/
		} else {
		  if (ac == 1) {
		    dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "SQL request isn't specified. [ac:%d]", ac);
		    return DPS_ERROR;
		  }
		  if (strcasecmp(sc, "hex8str") && strcasecmp(sc, "strcrc32") && strcasecmp(sc, "int") && strcasecmp(sc, "hour")
		      && strcasecmp(sc, "hostname")  && strcasecmp(sc, "str2crc32")) {
		    dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Unknown Limit type %s", sc);
		    return DPS_ERROR;
		  }
		  dps_snprintf(nm, nm_len, "Req-%s", av[1]);
		  DpsVarListReplaceStr(&Conf->Vars, nm, av[2]);
		  if (ac == 3) {
		    dps_snprintf(nm, nm_len, "DBAddr-%s", av[1]);
		    DpsVarListReplaceStr(&Conf->Vars, nm, av[3]);
		  }
		}
		DPS_FREE(nm);
	} else rc = DPS_ERROR;
	return rc;
}

static int preload_limit(void *Cfg, size_t ac,char **av) {
    DPS_CFG	*C=(DPS_CFG*)Cfg;
    DPS_AGENT *Indexer = C->Indexer;
    DPS_DBLIST *dbl = &Indexer->Conf->dbl;
    DPS_DB *db;
    size_t i;
    const char *fname = NULL;
    int res = DPS_ERROR;
    int ltype = 0;
    size_t lind;

    if(!strcasecmp(av[1], "category")) {
	ltype = DPS_LIMTYPE_NESTED; fname = DPS_LIMFNAME_CAT;
    } else
    if(!strcasecmp(av[1], "tag")) {
	ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_TAG;
    } else
    if(!strcasecmp(av[1], "time")) {
	ltype = DPS_LIMTYPE_TIME; fname = DPS_LIMFNAME_TIME;
    } else
    if(!strcasecmp(av[1], "hostname")) {
	ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_HOST;
    } else
    if(!strcasecmp(av[1], "language")) {
	ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_LANG;
    } else
    if(!strcasecmp(av[1], "content")) {
	ltype = DPS_LIMTYPE_LINEAR_CRC; fname = DPS_LIMFNAME_CTYPE;
    } else
    if(!strcasecmp(av[1], "siteid")) {
	ltype = DPS_LIMTYPE_LINEAR_INT; fname = DPS_LIMFNAME_SITE;
    }

    for (i = 0; i < dbl->nitems; i++) {
	db = dbl->db[i];

	if((fname != NULL) && dps_strlen(av[2])) {
	    res = DpsAddSearchLimit(Indexer, &db->limits, &db->nlimits, ltype, fname, av[2]);
	}

	if (res != DPS_OK) return res;

	lind = db->nlimits - 1;

	db->limits[lind].start = 0;
	db->limits[lind].origin = -1;
	db->limits[lind].need_free = 1;

	switch(ltype) {
	case DPS_LIMTYPE_NESTED:
	    db->limits[lind].data = LoadNestedLimit(Indexer, db,
						    db->limits[lind].file_name,
						    db->limits[lind].hi,
						    db->limits[lind].lo,
						    db->limits[lind].f_hi,
						    db->limits[lind].f_lo,
						    &db->limits[lind].size);
	    break;
	case DPS_LIMTYPE_TIME:
	    db->limits[lind].data = LoadTimeLimit(Indexer, db, 
						  db->limits[lind].file_name,
						  db->limits[lind].hi,
						  db->limits[lind].lo,
						  &db->limits[lind].size);
	    break;
	case DPS_LIMTYPE_LINEAR_INT:
	case DPS_LIMTYPE_LINEAR_CRC:
	    db->limits[lind].data = LoadLinearLimit(Indexer, db,
						    db->limits[lind].file_name,
						    db->limits[lind].hi,
						    &db->limits[lind].size);
	    break;
	}
    }
    return DPS_OK;
}


static int flush_srv_table(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	int	res = DPS_OK;
	if(C->flags & DPS_FLAG_ADD_SERVURL) {
		res = DpsSrvAction(C->Indexer, NULL, DPS_SRV_ACTION_FLUSH);
		C->flush_server = 1;
	}
	return res;
}


static int flush_cat_table(void *Cfg, size_t ac,char **av) {
	DPS_CFG	*C = (DPS_CFG*)Cfg;
	int	res = DPS_OK;
	if(C->flags & DPS_FLAG_ADD_SERVURL) {
		res = DpsSrvAction(C->Indexer, NULL, DPS_SRV_ACTION_CATFLUSH);
	}
	return res;
}
		
static int env_rpl_charset(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	DPS_CHARSET	*cs;
	if(!(cs=DpsGetCharSet(av[1]))){
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "charset '%s' is not supported", av[1]);
	  return DPS_ERROR;
	}
	if(!strcasecmp(av[0],"LocalCharset")){
		Conf->lcs=cs;
		DpsVarListReplaceStr(&Conf->Vars,av[0],av[1]);
	}else
	if(!strcasecmp(av[0],"BrowserCharset")){
		Conf->bcs=cs;
		DpsVarListReplaceStr(&Conf->Vars,av[0],av[1]);
	}
	return DPS_OK;
}


static int srv_rpl_charset(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf = C->Indexer->Conf;
	DPS_CHARSET	*cs;

	if (ac == 1) {
	  DpsVarListDel(&C->Srv->Vars, av[0]);
	} else {

	  if(!(cs=DpsGetCharSet(av[1]))){
	    dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "charset '%s' is not supported", av[1]);
	    return DPS_ERROR;
	  }
	  DpsVarListReplaceStr(&C->Srv->Vars, av[0], DpsCharsetCanonicalName(av[1]));
	}
	return DPS_OK;
}

static int srv_rpl_mirror(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	if (ac == 1) {
	  DpsVarListDel(&C->Srv->Vars, av[0]);
	} else 
	if(!strcasecmp(av[0],"MirrorRoot") || !strcasecmp(av[0],"MirrorHeadersRoot")){
		char	fname[PATH_MAX];
		DpsRelVarName(C->Indexer->Conf, fname, sizeof(fname)-1, av[1]);
		DpsVarListReplaceStr(&C->Srv->Vars,av[0],fname);
	}else
	if(!strcasecmp(av[0],"MirrorPeriod")){
		int	tm=Dps_dp2time_t(av[1]);
		DpsVarListReplaceInt(&C->Srv->Vars,"MirrorPeriod",tm);
	}
	return DPS_OK;
}
		

static int srv_rpl_auth(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	char	name[128];
	dps_snprintf(name,sizeof(name)-1,"%s",av[0]);
	name[sizeof(name)-1]='\0';
	if(av[1]){
		size_t	len=dps_strlen(av[1]);
		char	*auth=(char*)DpsMalloc(BASE64_LEN(dps_strlen(av[1])) + 1);
		if (auth == NULL) {
		  return DPS_ERROR;
		}
		dps_base64_encode(av[1],auth,len);
		DpsVarListReplaceStr(&C->Srv->Vars,name,auth);
		DPS_FREE(auth);
	}else{
		DpsVarListReplaceStr(&C->Srv->Vars,name,"");
	}
	return DPS_OK;
}

static char *str_store (char *dest, char *src) {
	size_t d_size = (dest ? dps_strlen(dest) : 0);
	size_t s_size = dps_strlen(src) + 1;
	char *d = DpsRealloc(dest, d_size + s_size);

	if (d) dps_memcpy(d + d_size, src, s_size); /* was: dps_memmove */
	else DPS_FREE(dest);
	return(d);
}

char *DpsParseEnvVar (DPS_ENV *Conf, const char *str) {
	char *p1 = (char *)str;
	char *p2 = (char *)str;
	char *p3;
	char *s;
	char *o = NULL;

	if (! str) return(NULL);
	while ((p1 = strchr(p1, '$'))) {
		if (p1[1] != '(') {
			p1++;
			continue;
		}
		*p1 = 0;
		o = str_store(o, p2);
		*p1 = '$';
		if ((p3 = strchr(p1 + 2, ')'))) {
			*p3 = 0;
			s = (char *)DpsVarListFindStr(&Conf->Vars, p1 + 2, NULL);
			if (s) { 
			  o = str_store(o, s);
			  *p3 = ')';
			  p2 = p1 = p3 + 1;
			} else {
			  *p3 = ')';
			  p2 = p1;
			  p1 = p3 + 1;
			}
		} else {
			DPS_FREE(o);
			return(NULL);
		}
	}
	o = str_store(o, p2);
	return(o);
}

static int env_rpl_env_var (void *Cfg, size_t ac, char **av) {
	DPS_ENV *Conf = ((DPS_CFG *)Cfg)->Indexer->Conf;
	char *p = getenv(av[1]);
	if (! p) {
	  dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "ImportEnv '%s': no such variable.", av[1]);
	  return DPS_ERROR;
	}
	DpsVarListReplaceStr(&Conf->Vars, av[1], p);
	return DPS_OK;
}


static int env_rpl_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	int res = 0;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	if(!strcasecmp(av[0],"DBAddr")){
		if(DPS_OK != DpsDBListAdd(&Conf->dbl, av[1] ? av[1] : "", DPS_OPEN_MODE_WRITE)){
		  dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "Invalid DBAddr: '%s'",av[1]?av[1]:"");
#ifdef WITH_PARANOIA
		  DpsViolationExit(-1, paran);
#endif
		  return DPS_ERROR;
		}
	}else if(!strcasecmp(av[0],"Bind")){
	  Conf->Flags.bind_addr.sin_addr.s_addr = inet_addr(av[1]);
	  Conf->Flags.bind_addr.sin_port = htons((uint16_t)0 /*4096 + time(NULL) % 16384*/);
	  Conf->Flags.bind_addr.sin_family = AF_INET;
	}else if (!strcasecmp(av[0],"CharsToEscape")) {
	  DPS_FREE(Conf->CharsToEscape);
	  Conf->CharsToEscape = DpsStrdup(av[1]);
	} else if(!strcasecmp(av[0], "SkipUnreferred")) {
	  if (!strcasecmp(av[1], "yes")) res = DPS_METHOD_VISITLATER;
	  else if (!strncasecmp(av[1], "del", 3)) res = DPS_METHOD_DISALLOW;
	  Conf->Flags.skip_unreferred = res;
	} else if(!strcasecmp(av[0], "SkipHrefIn")) {
	  Conf->Flags.SkipHrefIn = DpsHrefFrom(av[1]);
	}else if (!strcasecmp(av[0], "PopRankMethod")) {
	  Conf->Flags.poprank_method = DpsPRMethod(av[1]);
	}
	DpsVarListReplaceStr(&Conf->Vars,av[0],av[1]);
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_OK;
}

static int srv_rpl_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	if (strcasecmp(av[0], "ExpireAt") == 0) {
	  C->Srv->ExpireAt.eight = 0;
	  if (ac > 1 && *av[1] != '*') C->Srv->ExpireAt.cron.min = 1 + (char)DPS_ATOI(av[1]);
	  if (ac > 2 && *av[2] != '*') C->Srv->ExpireAt.cron.hour = 1 + (char)DPS_ATOI(av[2]);
	  if (ac > 3 && *av[3] != '*') C->Srv->ExpireAt.cron.day = 1 + (char)DPS_ATOI(av[3]);
	  if (ac > 4 && *av[4] != '*') C->Srv->ExpireAt.cron.month = 1 + (char)DPS_ATOI(av[4]);
	  if (ac > 5 && *av[5] != '*') C->Srv->ExpireAt.cron.wday = 1 + (char)DPS_ATOI(av[5]);
	} else if (ac == 1) {
	  DpsVarListDel(&C->Srv->Vars, av[0]);
	} else if (ac == 2) {
	  if (strcasecmp(av[0], "VaryLang") == 0) {
	    int hascomas = 0;
	    char *varylang = DpsStrdup(av[1]), *s;
	    for(s = varylang; *s != '\0'; s++) {
	      if(isalpha((int)*s)) {
		hascomas = 0;
	      } else
	      if (*s == ' ' && hascomas == 0) {
		*s = ','; hascomas = 1;
	      } else
	      if (*s == ',') {
		if (hascomas == 1) *s = ' ';
		else hascomas = 1;
	      }
	    }
	    DpsVarListReplaceStr(&C->Srv->Vars, av[0], varylang);
	    DpsFree(varylang);
	  } else if (strcasecmp(av[0], "HTDBText") == 0) {
	    char name[PATH_MAX];
	    dps_snprintf(name, PATH_MAX, "HTDBText-%s", av[1]);
	    DpsVarListDel(&C->Srv->Vars, name);
	  } else 
	    DpsVarListReplaceStr(&C->Srv->Vars, av[0], av[1]);
	} else if (ac == 3 && (strcasecmp(av[0], "HTDBText") == 0)) {
	  char name[PATH_MAX];
	  dps_snprintf(name, PATH_MAX, "HTDBText-%s", av[1]);
	  DpsVarListReplaceStr(&C->Srv->Vars, name, av[2]);
	} else {
	  dps_snprintf(C->Indexer->Conf->errstr,  sizeof(C->Indexer->Conf->errstr) - 1, "Too many arguments for '%s' command", av[0]);
	  return DPS_ERROR;
	}
	return DPS_OK;
}


static int srv_htdb(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	char err[512];
	size_t i, j;
	if (ac == 1) {
	  for (i = j = 0; i < C->Srv->HTDBsec.nmatches; i++) {
	    if (strcasecmp(av[0], DPS_NULL2EMPTY(C->Srv->HTDBsec.Match[i].arg))) {
	      if (i != j) {
		DpsMatchFree(&C->Srv->HTDBsec.Match[j]);
		DpsMatchInit(&C->Srv->HTDBsec.Match[j]);
		C->Srv->HTDBsec.Match[j] = C->Srv->HTDBsec.Match[i];
		DpsMatchInit(&C->Srv->HTDBsec.Match[i]);
	      }
	      j++;
	    }
	  }
	  if (j != C->Srv->HTDBsec.nmatches) {
	    C->Srv->HTDBsec.nmatches = j;
	  }
	} else if (ac == 2 && !strcasecmp(av[0], "HTDBText")) {
	  for (i = j = 0; i < C->Srv->HTDBsec.nmatches; i++) {
	    if (strcasecmp(av[0], DPS_NULL2EMPTY(C->Srv->HTDBsec.Match[i].arg)) 
		|| strcasecmp(av[1], DPS_NULL2EMPTY(C->Srv->HTDBsec.Match[i].section))) {
	      if (i != j) {
		DpsMatchFree(&C->Srv->HTDBsec.Match[j]);
		DpsMatchInit(&C->Srv->HTDBsec.Match[j]);
		C->Srv->HTDBsec.Match[j] = C->Srv->HTDBsec.Match[i];
		DpsMatchInit(&C->Srv->HTDBsec.Match[i]);
	      }
	      j++;
	    }
	  }
	  if (j != C->Srv->HTDBsec.nmatches) {
	    C->Srv->HTDBsec.nmatches = j;
	  }
	} else {
	  DPS_MATCH M;
	  bzero(&M, sizeof(M));
	  M.nomatch = 0;
	  M.case_sense = 1;
	  M.match_type = DPS_MATCH_BEGIN;
	  M.arg = av[0];
	  if (!strcasecmp(av[0], "HTDBDoc")) {
	    M.subsection = av[1];
	    if (ac == 3) {
	      M.match_type = DPS_MATCH_REGEX;
	      M.pattern = av[2];
	    } else if (ac > 3) return DPS_ERROR;
	  } else if (!strcasecmp(av[0], "HTDBText")) {
	    M.section = av[1];
	    M.subsection = av[2];
	    if (ac == 4) {
	      M.match_type = DPS_MATCH_REGEX;
	      M.pattern = av[3];
	    } else if (ac > 4) return DPS_ERROR;
	  } else return DPS_ERROR; /* this should never happen */
	  DpsMatchListAdd(C->Indexer, &C->Srv->HTDBsec, &M, err, sizeof(err), 0);
	}
	return DPS_OK;
}


static int srv_rpl_hdr(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	char	*nam=NULL;
	char	*val=NULL;
	char	name[128];
	
	switch(ac){
		case 3:	nam=av[1];val=av[2];break;
		case 2:
			if((val=strchr(av[1],':'))){
				*val++='\0';
				val=DpsTrim(val," \t");
				nam=av[1];
			}
			break;
	}
	if(nam){
		dps_snprintf(name,sizeof(name),"Request.%s",nam);
		name[sizeof(name)-1]='\0';
		DpsVarListReplaceStr(&C->Srv->Vars,name,val);
	}
	return DPS_OK;
}

static int srv_rpl_bool_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	int res=!strcasecmp(av[1],"yes");
	if(!strcasecmp(av[0], "Robots")) C->Srv->use_robots = res;
	else DpsVarListReplaceInt(&C->Srv->Vars,av[0],res);
	if (!strcasecmp(av[0], "DetectClones")) DpsVarListReplaceStr(&C->Indexer->Conf->Vars, av[0], av[1]);
	return DPS_OK;
}

static int env_rpl_bool_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	int res = !strcasecmp(av[1], "yes");
	int force = !strcasecmp(av[1], "force");
	if(!strcasecmp(av[0], "LogsOnly")) Conf->logs_only = res;
	else if(!strcasecmp(av[0], "DoStore")) Conf->Flags.do_store = res;
	else if(!strcasecmp(av[0], "DoExcerpt")) Conf->Flags.do_excerpt = res;
	else if(!strcasecmp(av[0], "CVSIgnore")) Conf->Flags.CVS_ignore = res;
	else if(!strcasecmp(av[0], "CollectLinks")) C->Indexer->Flags.collect_links = Conf->Flags.collect_links = res;
	else if(!strcasecmp(av[0], "UseCRC32URLId")) C->Indexer->Flags.use_crc32_url_id = Conf->Flags.use_crc32_url_id = res;
	else if(!strcasecmp(av[0], "CrossWords")) Conf->Flags.use_crosswords = res;
	else if(!strcasecmp(av[0], "NewsExtensions")) Conf->Flags.use_newsext = res;
	else if(!strcasecmp(av[0], "AccentExtensions")) Conf->Flags.use_accentext = res;
	else if(!strcasecmp(av[0], "AspellExtensions")) Conf->Flags.use_aspellext = res;
	else if(!strcasecmp(av[0], "GuesserUseMeta")) Conf->Flags.use_meta = res;
	else if(!strcasecmp(av[0], "ColdVar")) Conf->Flags.cold_var = res;
	else if(!strcasecmp(av[0], "LangMapUpdate")) Conf->Flags.update_lm = res;
	else if(!strcasecmp(av[0], "OptimizeAtUpdate")) Conf->Flags.OptimizeAtUpdate = res;
	else if(!strcasecmp(av[0], "PreloadURLData")) Conf->Flags.PreloadURLData = res;
	else if(!strcasecmp(av[0], "TrackHops")) Conf->Flags.track_hops = res;
	else if(!strcasecmp(av[0], "PopRankPostpone")) Conf->Flags.poprank_postpone = res;
	else if(!strcasecmp(av[0], "URLInfoSQL")) Conf->Flags.URLInfoSQL = res;
	else if(!strcasecmp(av[0], "SRVInfoSQL")) Conf->Flags.SRVInfoSQL = res;
	else if(!strcasecmp(av[0], "CheckInsertSQL")) Conf->Flags.CheckInsertSQL = res;
	else if(!strcasecmp(av[0], "MarkForIndex")) Conf->Flags.mark_for_index = res;
	else if(!strcasecmp(av[0], "UseDateHeader")) Conf->Flags.use_date_header = (force) ? 2 : ((res) ? 1 : 0);
	else if(!strcasecmp(av[0], "ProvideReferer")) Conf->Flags.provide_referer = res;
	else if(!strcasecmp(av[0], "MakePrefixes")) Conf->Flags.make_prefixes = res;
	else if(!strcasecmp(av[0], "MakeSuffixes")) Conf->Flags.make_suffixes = res;
	else if(!strcasecmp(av[0], "FillDictionary")) Conf->Flags.fill_dictionary = res;
	else if(!strcasecmp(av[0], "FastHrefCheck")) { if (res) Conf->flags |= DPS_FLAG_FAST_HREF_CHECK;
	                                               else Conf->flags &= ~((dps_uint8)DPS_FLAG_FAST_HREF_CHECK);
	                                             }
	else if(!strcasecmp(av[0], "StopWordsLoose")) { if (res) Conf->flags |= DPS_FLAG_STOPWORDS_LOOSE;
	                                                else Conf->flags &= ~((dps_uint8)DPS_FLAG_STOPWORDS_LOOSE);
						      }
	else if(!strcasecmp(av[0], "DisableRelNoFollow")) Conf->Flags.rel_nofollow = !res;
	else if(!strcasecmp(av[0], "ResegmentChinese"))  if (res) { Conf->Flags.Resegment |= DPS_RESEGMENT_CHINESE;
	                                                 } else Conf->Flags.Resegment &= ~DPS_RESEGMENT_CHINESE;
	else if(!strcasecmp(av[0], "ResegmentJapanese")) if (res) { Conf->Flags.Resegment |= DPS_RESEGMENT_JAPANESE;
	                                                 } else Conf->Flags.Resegment &= ~DPS_RESEGMENT_JAPANESE;
	else if(!strcasecmp(av[0], "ResegmentKorean"))   if (res) { Conf->Flags.Resegment |= DPS_RESEGMENT_KOREAN;
	                                                 } else Conf->Flags.Resegment &= ~DPS_RESEGMENT_KOREAN;
	else if(!strcasecmp(av[0], "ResegmentThai"))     if (res) { Conf->Flags.Resegment |= DPS_RESEGMENT_THAI;
	                                                 } else Conf->Flags.Resegment &= ~DPS_RESEGMENT_THAI;
	else DpsVarListReplaceInt(&Conf->Vars, av[0], res);
	return DPS_OK;
}

static int env_rpl_num_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	int	res = DPS_ATOI(av[1]);
	if(!strcasecmp(av[0],"IspellCorrectFactor"))Conf->WordParam.correct_factor=res;
	else if(!strcasecmp(av[0],"IspellIncorrectFactor"))Conf->WordParam.incorrect_factor=res;
	else if(!strcasecmp(av[0],"MinWordLength"))Conf->WordParam.min_word_len=res;
	else if(!strcasecmp(av[0],"MaxWordLength"))Conf->WordParam.max_word_len=res;
	else if(!strcasecmp(av[0],"PopRankNeoIterations"))Conf->Flags.PopRankNeoIterations = res;
	else if(!strcasecmp(av[0],"GuesserBytes"))Conf->Flags.GuesserBytes = res;
	else if(!strcasecmp(av[0],"MaxSiteLevel"))Conf->Flags.MaxSiteLevel = res;
	else if(!strcasecmp(av[0],"SEASentences"))Conf->Flags.SEASentences = res;
	else if(!strcasecmp(av[0],"SEASentenceMinLength"))Conf->Flags.SEASentenceMinLength = res;
	else if(!strcasecmp(av[0],"PagesInGroup"))Conf->Flags.PagesInGroup = res;
	else if(!strcasecmp(av[0],"LongestTextItems"))Conf->Flags.LongestTextItems = res;
	else if(!strcasecmp(av[0],"SubDocLevel"))Conf->Flags.SubDocLevel = res;
	else if(!strcasecmp(av[0],"SubDocCnt"))Conf->Flags.SubDocCnt = res;
	else if(!strcasecmp(av[0],"MaxCrawlDelay"))Conf->Flags.MaxCrawlDelay = res;
	return DPS_OK;
}		

static int env_rpl_time_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	time_t	res = Dps_dp2time_t(av[1]);
	if(res==-1){
	  dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "bad time interval: %s", av[1]);
	  return DPS_ERROR;
	}
	if(!strcasecmp(av[0], "HoldCache")) Conf->Flags.hold_cache = res;
	else if(!strcasecmp(av[0], "RobotsPeriod")) Conf->Flags.robots_period = res;
	return DPS_OK;
}

static int env_rpl_named_var(void *Cfg, size_t ac, char **av) {
  DPS_CFG  *C = (DPS_CFG*)Cfg;
  DPS_ENV  *Conf = C->Indexer->Conf;
  DPS_CONV dc_lc;
  char *str = NULL;
  size_t len = dps_strlen(av[2]);

  DpsConvInit(&dc_lc, (Conf->bcs) ? Conf->bcs : DpsGetCharSet("iso-8859-1"),
	      (Conf->lcs) ? Conf->lcs : DpsGetCharSet("iso-8859-1"),
	      Conf->CharsToEscape, DPS_RECODE_HTML);
  str = (char*)DpsMalloc(20 * len);
  if (str == NULL) return DPS_ERROR;
  DpsConv(&dc_lc, str, 20 * len, av[2], len + 1);
  DpsVarListReplaceStr(&Conf->Vars, av[1], str);
  DpsFree(str);
  return DPS_OK;
}


static int env_rpl_named_var_lcs(void *Cfg, size_t ac, char **av) {
  DPS_CFG  *C = (DPS_CFG*)Cfg;
  DPS_ENV  *Conf = C->Indexer->Conf;
  DpsVarListReplaceStr(&Conf->Vars, av[1], av[2]);
  return DPS_OK;
}


static int srv_rpl_num_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	int	res = DPS_ATOI(av[1]);
	float   val = (float)DPS_ATOF(av[1]);
	DpsVarListReplaceInt(&C->Srv->Vars,av[0],res);
	if (strcasecmp(av[0], "MaxHops") == 0) C->Srv->MaxHops = (dps_uint4) res;
	else if (strcasecmp(av[0], "MaxDocsPerServer") == 0) C->Srv->MaxDocsPerServer = (dps_uint4) res;
	else if (strcasecmp(av[0], "MaxDocsPerSite") == 0) C->Srv->MaxDocsPerSite = (dps_uint4) res;
	else if (strcasecmp(av[0], "MaxHrefsPerServer") == 0) C->Srv->MaxHrefsPerServer = (dps_uint4) res;
	else if (strcasecmp(av[0], "MaxDepth") == 0) C->Srv->MaxDepth = (dps_uint4) res;
	else if (strcasecmp(av[0], "MaxURLength") == 0) C->Srv->MaxURLength = (dps_uint4) res;
	else if (strcasecmp(av[0], "MinServerWeight") == 0) C->Srv->MinServerWeight = val;
	else if (strcasecmp(av[0], "MinSiteWeight") == 0) C->Srv->MinSiteWeight = val;
	else if (strcasecmp(av[0], "ServerWeight") == 0) C->Srv->weight = val;
	return DPS_OK;
}

static int srv_rpl_time_var(void *Cfg, size_t ac,char **av){
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;
	time_t	res;

	if (!strcasecmp(av[0], "CrawlDelay")) {
	  C->Srv->crawl_delay = (time_t)(1000 * DPS_ATOF(av[1]));
	} else if (strcasecmp(av[0], "PeriodByHops")) {
	  if (0 == ac) {
	    DpsVarListDel(&C->Srv->Vars, av[0]);
	  } else {
	    res = Dps_dp2time_t(av[1]);
	    if(res==-1){
		dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "bad time interval: %s", av[1]);
		return DPS_ERROR;
	    }
	    DpsVarListReplaceUnsigned(&C->Srv->Vars, av[0], (unsigned)res);
	  }
	  
	} else { /* PeriodByHops command */
	  char str[64];
	  int hops = 0;
	  sscanf(av[1], "%d", &hops);
	  if (hops >= DPS_DEFAULT_MAX_HOPS) {
		dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "hops %s is too big", av[1]);
		return DPS_ERROR;
	  }
	  dps_snprintf(str, sizeof(str), "Period%s", av[1]);
	  switch(ac) {
	  case 3: 
	    res = Dps_dp2time_t(av[2]);
	    if(res==-1){
		dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "bad time interval: %s", av[2]);
		return DPS_ERROR;
	    }
	    DpsVarListReplaceUnsigned(&C->Srv->Vars, str, (unsigned)res);
	    break;
	  case 2:
	    DpsVarListDel(&C->Srv->Vars, str);
	    break;
	  default:
	    dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "bad format for %s command", av[0]);
	    return DPS_ERROR;
	  }
	}

	return DPS_OK;
}

static int srv_rpl_category(void *Cfg, size_t ac, char **av) {
	DPS_CFG	*C=(DPS_CFG*)Cfg;
	DPS_ENV	*Conf=C->Indexer->Conf;

	if(!(C->flags & DPS_FLAG_ADD_SERV))
		return DPS_OK;
	
	if (ac == 1) {
	  DpsVarListDel(&C->Srv->Vars, av[0]);
	} else {
	  unsigned int cid = DpsGetCategoryId(Conf, av[1]);
	  char buf[64];
	  dps_snprintf(buf, 64, "%u", cid);
	  DpsVarListReplaceStr(&C->Srv->Vars, av[0], buf);
	}
	return DPS_OK;
}

typedef struct conf_cmd_st {
	const char	*name;
	size_t		argmin;
	size_t		argmax;
	int		(*action)(void *a,size_t n,char **av);
} DPS_CONFCMD;


static int dps_commands_cmp(DPS_CONFCMD *c1, DPS_CONFCMD *c2) {
  return strcasecmp(c1->name, c2->name);
}

static DPS_CONFCMD commands[] = 
{
#include "commands.inc"
	
/* END Marker */
/*	{NULL, 0, 0, 0}*/
};

__C_LINK int __DPSCALL DpsEnvAddLine(DPS_CFG *C,char *str){
	DPS_ENV		*Conf=C->Indexer->Conf;
	DPS_CONFCMD	*Cmd, key;
	char		*av[256];
	size_t		ac;
	int             res = DPS_OK;
	int             argc;
	size_t          i;
	char            *p;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	
	ac = DpsGetArgs(str, av, 255);

	if (ac == 0) {
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_OK;
	}

	key.name = DPS_NULL2EMPTY(av[0]);
	Cmd = dps_bsearch(&key, commands, sizeof(commands) / sizeof(commands[0]), sizeof(commands[0]), (qsort_cmp)dps_commands_cmp);
	if (Cmd != NULL) {
	  argc = ac;
			argc--;
			if(ac<Cmd->argmin+1){
			  dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "too few (%d) arguments for command '%s'",argc,Cmd->name);
#ifdef WITH_PARANOIA
			  DpsViolationExit(-1, paran);
#endif
			  return DPS_ERROR;
			}
			
			if(ac>Cmd->argmax+1){
			  dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "too many (%d) arguments for command '%s'",argc,Cmd->name);
#ifdef WITH_PARANOIA
			  DpsViolationExit(-1, paran);
#endif
			  return DPS_ERROR;
			}
			
			for (i = 1; i < ac; i++) {
				if (! av[i]) continue;
				p = DpsParseEnvVar(Conf, av[i]);
				if (! p) {
				  dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "An error occured while parsing '%s'", av[i]);
#ifdef WITH_PARANOIA
				  DpsViolationExit(-1, paran);
#endif
				  return DPS_ERROR;
				}
				av[i] = p;
			}

			if(Cmd->action){
				res = Cmd->action((void*)C,ac,av);
			}
			
			for (i = 1; i < ac; i++) DPS_FREE(av[i]);

			if(Cmd->action){
#ifdef WITH_PARANOIA
			  DpsViolationExit(-1, paran);
#endif
			  return res;
			}
	}
	dps_snprintf(Conf->errstr,  sizeof(Conf->errstr) - 1, "Unknown command: %s", DPS_NULL2EMPTY(av[0]));
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return DPS_ERROR;
}


static int EnvLoad(DPS_CFG *Cfg,const char *cname){
	struct stat     sb;
	char	*str0 = NULL;	/* Unsafe copy - will be used in strtok	*/
	char    *data, *cur_n;
	char	*str1;	/* To concatenate lines			*/
	int	rc=DPS_OK;
	int             fd;
	size_t	line = 0, str0len = 0, str1len, str0size = 4096;
	char            savebyte;
	DPS_ENV   *Env = Cfg->Indexer->Conf;
	DPS_AGENT *A = Cfg->Indexer;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	
	if ((str0 = (char*)DpsMalloc(str0size)) == NULL) {
		sprintf(Cfg->Indexer->Conf->errstr, "Can't alloc %zu bytes at '%s': %d", str0size, __FILE__, __LINE__);
#ifdef WITH_PARANOIA
		DpsViolationExit(-1, paran);
#endif
		return DPS_ERROR;
	}
	str0[0]=0;
	
	/* Open config file */
	if (stat(cname, &sb)) {
	  dps_strerror(A, DPS_LOG_ERROR, "Unable to stat config file '%s'", cname);
	  DPS_FREE(str0);
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	if ((fd = open(cname, O_RDONLY)) <= 0) {
	  dps_strerror(A, DPS_LOG_ERROR, "Unable to open config file '%s'", cname);
	  DPS_FREE(str0);
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
	  dps_snprintf(Env->errstr, sizeof(Env->errstr)-1, "Unable to alloc %d bytes", sb.st_size);
	  DPS_FREE(str0);
	  close(fd);
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
	  dps_strerror(A, DPS_LOG_ERROR, "Unable to read config file '%s'", cname);
	  DPS_FREE(data);
	  DPS_FREE(str0);
	  close(fd);
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	data[sb.st_size] = '\0';
	str1 = data;
	cur_n = strchr(str1, NL_INT);
	if (cur_n != NULL) {
	  cur_n++;
	  savebyte = *cur_n;
	  *cur_n = '\0';
	}

	/*  Read lines and parse */
	while(str1 != NULL) {
		char	*end;
		
		line++;
		
		if(str1[0]=='#') goto loop_continue;
		for (end = str1 + (str1len = dps_strlen(str1)) - 1 ; (end>=str1) && (*end==CR_CHAR||*end==NL_CHAR||*end==' ') ; *end--='\0');
		if(!str1[0]) goto loop_continue;
		
		if(*end=='\\'){
			*end=0;
			if (str0len + str1len >= str0size) {
			  str0size += 4096 + str1len;
			  if ((str0 = (char*)DpsRealloc(str0, str0size)) == NULL) {
			    sprintf(Cfg->Indexer->Conf->errstr, "Can't realloc %zu bytes at '%s': %d", str0size, __FILE__, __LINE__);
			    DPS_FREE(data);
#ifdef WITH_PARANOIA
			    DpsViolationExit(-1, paran);
#endif
			    return DPS_ERROR;
			  }
			}
			dps_strcat(str0, str1);
			str0len += str1len;
			goto loop_continue;
		}
		dps_strcat(str0, str1);
		str0len += str1len;

#if TRACE_CMDS
		fprintf(stderr, ">Cmd.%d: %s\n", line, str0);
#endif
		
		if(DPS_OK != (rc=DpsEnvAddLine(Cfg,str0))){
			char	err[2048];
			dps_strncpy(err, Cfg->Indexer->Conf->errstr, 2048);
			dps_snprintf(Cfg->Indexer->Conf->errstr, sizeof(Cfg->Indexer->Conf->errstr), "%s:%zu: %s", cname, line, err);
			break;
		}
		
		str0[0]=0;
		str0len = 0;
	loop_continue:
		str1 = cur_n;
		if (str1 != NULL) {
		  *str1 = savebyte;
		  cur_n = strchr(str1, NL_INT);
		  if (cur_n != NULL) {
		    cur_n++;
		    savebyte = *cur_n;
		    *cur_n = '\0';
		  }
		}
		
	}
	DPS_FREE(data);
	DPS_FREE(str0);
	close(fd);
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return rc;
}


__C_LINK  int __DPSCALL DpsEnvLoad(DPS_AGENT *Indexer, const char *cname, dps_uint8 lflags) {
	DPS_CFG		Cfg;
	int		rc=DPS_OK;
	const char	*dbaddr=NULL;
	size_t i, len = 0;
	char *accept_str;
#ifdef WITH_PARANOIA
	void * paran = DpsViolationEnter(paran);
#endif
	
	bzero((void*)&Cfg, sizeof(Cfg));
	Cfg.Indexer = Indexer;
	if (Indexer->Conf->Cfg_Srv != NULL) {
	  DpsServerFree(Indexer->Conf->Cfg_Srv);
	  DPS_FREE(Indexer->Conf->Cfg_Srv);
	}
	if ((Indexer->Conf->Cfg_Srv = Cfg.Srv = (DPS_SERVER*)DpsMalloc(sizeof(DPS_SERVER))) == NULL) {
	  dps_snprintf(Indexer->Conf->errstr,  sizeof(Indexer->Conf->errstr) - 1, "Unable to alloc DPS_SERVER");
#ifdef WITH_PARANOIA
	  DpsViolationExit(-1, paran);
#endif
	  return DPS_ERROR;
	}
	DpsServerInit(Cfg.Srv);
	Cfg.flags=lflags;
	Cfg.level=0;
	
	/* Set DBAddr if for example passed from environment */
	if((dbaddr=DpsVarListFindStr(&Indexer->Conf->Vars,"DBAddr",NULL))){
		if(DPS_OK != DpsDBListAdd(&Indexer->Conf->dbl, dbaddr, DPS_OPEN_MODE_WRITE)){
		  dps_snprintf(Indexer->Conf->errstr,  sizeof(Indexer->Conf->errstr) - 1, "Invalid DBAddr: '%s'", dbaddr);
		  rc = DPS_ERROR;
		  goto freeex;
		}
	}
	
	if(DPS_OK == (rc=EnvLoad(&Cfg,cname))){
		/* Sort ispell dictionay if it has been loaded */
		if(Indexer->Conf->Spells.nspell) {
			DpsSortDictionary(&Indexer->Conf->Spells);
			DpsSortAffixes(&Indexer->Conf->Affixes, &Indexer->Conf->Spells);
			DpsSortQuffixes(&Indexer->Conf->Quffixes, &Indexer->Conf->Spells);
		}
		/* Sort synonyms */
		DpsSynonymListSort(&Indexer->Conf->Synonyms);
		/* Sort acronyms */
		DpsAcronymListSort(&Indexer->Conf->Acronyms);

		if (Cfg.flush_server) {
		  DpsSrvAction(Indexer, NULL, DPS_SRV_ACTION_CLEAN);
		}
			
		DpsVarListInsStr(&Indexer->Conf->Vars, "Request.User-Agent", DPS_USER_AGENT);

		for (i = 0; i < Indexer->Conf->Parsers.nparsers; i++) {
		  len += dps_strlen(Indexer->Conf->Parsers.Parser[i].from_mime) + 8;
		}
		if ((accept_str = (char*)DpsMalloc(2048 + len)) == NULL) {
		  sprintf(Indexer->Conf->errstr, "No memory for Accept [%s:%d]", __FILE__, __LINE__);
		  rc = DPS_ERROR;
		  goto freeex;
		}
		sprintf(accept_str, "text/html;q=1.0,application/xhtml+xml;q=1.0,application/xml;q=1.0,text/plain;q=0.9,text/xml;q=1.0,text/tab-separated-values;q=0.8,text/css;q=0.5,image/gif;q=0.5"
#ifdef WITH_MP3
			",audio/mpeg;q=0.5"
#endif
			);
		for (i = 0; i < Indexer->Conf->Parsers.nparsers; i++) {
		  sprintf(DPS_STREND(accept_str), ",%s", Indexer->Conf->Parsers.Parser[i].from_mime);
		  DpsRTrim(accept_str, "*");
		  sprintf(DPS_STREND(accept_str), ";q=0.6");
		}
		sprintf(DPS_STREND(accept_str), ",*;q=0.1");

		DpsVarListInsStr(&Indexer->Conf->Vars, "Request.Accept", accept_str);
		DPS_FREE(accept_str);
		/* Flush stored hrefs */
		Indexer->Flags.collect_links = Indexer->Conf->Flags.collect_links;
		DpsStoreHrefs(Indexer);
	}
    DpsVarListAddStr(&Indexer->Conf->Vars, "IndexDocSizeLimit", DpsVarListFindStr(&Indexer->Conf->Cfg_Srv->Vars, "IndexDocSizeLimit", "0"));
    Indexer->Conf->Flags.nmaps = Indexer->Flags.nmaps = (Indexer->Conf->LangMaps.nmaps > 0);
#ifdef HAVE_ASPELL
    aspell_config_replace(Indexer->aspell_config, "home-dir", DpsVarListFindStr(&Indexer->Conf->Vars, "EtcDir", DPS_CONF_DIR));
    aspell_config_replace(Indexer->aspell_config, "use-other-dicts", "true");
#endif

freeex:
    /*	DpsServerFree(&Srv);*/
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return rc;
}
