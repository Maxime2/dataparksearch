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
#include "dps_doc.h"
#include "dps_utils.h"
#include "dps_doc.h"
#include "dps_word.h"
#include "dps_crossword.h"
#include "dps_hrefs.h"
#include "dps_guesser.h"
#include "dps_vars.h"
#include "dps_textlist.h"
#include "dps_parsehtml.h"
#include "dps_xmalloc.h"
#include "dps_url.h"
#include "dps_log.h"
#include "dps_mutex.h"
#include "dps_proto.h"
#include "dps_host.h"
#include "dps_hash.h"
#include "dps_db.h"
#include "dps_wild.h"
#include "dps_match.h"
#include "dps_cookies.h"
#include "dps_charsetutils.h"
#include "dps_indexertool.h"
#include "dps_indexer.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#if defined(WITH_IDN) && !defined(APACHE1) && !defined(APACHE2)
#include <idna.h>
#elif defined(WITH_IDNKIT) && !defined(APACHE1) && !defined(APACHE2)
#include <idn/api.h>
#endif


DPS_DOCUMENT * __DPSCALL DpsDocInit(DPS_DOCUMENT * Doc){
	if(!Doc){
		Doc=(DPS_DOCUMENT*)DpsMalloc(sizeof(DPS_DOCUMENT));
		if (Doc == NULL) return NULL;
		bzero((void*)Doc, sizeof(DPS_DOCUMENT));
		Doc->freeme=1;
	}else{
		bzero((void*)Doc, sizeof(DPS_DOCUMENT));
	}
	Doc->Spider.read_timeout=DPS_READ_TIMEOUT;
	Doc->Spider.doc_timeout=DPS_DOC_TIMEOUT;
	Doc->Spider.net_error_delay_time=DPS_DEFAULT_NET_ERROR_DELAY_TIME;
	Doc->connp.connp = (DPS_CONN*)DpsXmalloc(sizeof(DPS_CONN)); /* need DpsXmalloc to avoid a trap */
	if (Doc->connp.connp == NULL) {
	  DpsDocFree(Doc);
	  return NULL;
	}
	DpsURLInit(&Doc->CurURL);
	return(Doc);
}


void __DPSCALL DpsDocFree(DPS_DOCUMENT *Result) {
	if(!Result)return;
	DPS_FREE(Result->Buf.buf);
	DPS_FREE(Result->Buf.pattern);
	DPS_FREE(Result->connp.hostname);
	DPS_FREE(Result->connp.user);
	DPS_FREE(Result->connp.pass);
#if 0
	if (Result->connp.connp) {
/*	  DPS_FREE(Result->connp.connp->hostname);*/
	  DPS_FREE(Result->connp.connp->user);
	  DPS_FREE(Result->connp.connp->pass);
	}
#endif
	DPS_FREE(Result->connp.connp);
	
	DpsHrefListFree(&Result->Hrefs);
	DpsWordListFree(&Result->Words);
	DpsCrossListFree(&Result->CrossWords);
	DpsVarListFree(&Result->RequestHeaders);
	DpsVarListFree(&Result->Sections);
	DpsTextListFree(&Result->TextList);
	DpsTextListFree(&Result->ExtractorList);
	
	DpsURLFree(&Result->CurURL);

	if(Result->freeme){
		DPS_FREE(Result);
	}else{
		bzero((void*)Result, sizeof(DPS_DOCUMENT));
	}
}

void DpsURLCRDListListFree(DPS_URLCRDLISTLIST *Lst){
	size_t i;
	
	for(i=0;i<Lst->nlists;i++){
		DPS_FREE(Lst->List[i].word);
		DPS_FREE(Lst->List[i].Coords);
	}
	DPS_FREE(Lst->List);
	if(Lst->freeme)DPS_FREE(Lst);
}


#define nonul(x)	((x)?(x):"")

char *DpsDocToTextBuf(DPS_DOCUMENT * Doc, int numsection_flag, int e_url_flag) {
	size_t	i, r, l, len;
	char	*end, *textbuf;
	int u;
	
	len = 16;

	switch(Doc->method) {
	case DPS_METHOD_UNKNOWN:
	case DPS_METHOD_GET:
	case DPS_METHOD_CHECKMP3:
	case DPS_METHOD_CHECKMP3ONLY:
	case DPS_METHOD_INDEX:
	  u = 1;
	  break;
	default:
	  u = 0;
	}

	for (r = 0; r < 256; r++)
	for(i = 0; i < Doc->Sections.Root[r].nvars; i++) {
		DPS_VAR	*S = &Doc->Sections.Root[r].Var[i];

		if(!S->name || !S->val || (!S->val[0] && strcasecmp(S->name, "Z")) ) continue;
/*		fprintf(stderr, "-Sec.name: %s  .section:%d  .maxlen: %d\n", S->name, S->section, S->maxlen);*/
		if(!(((numsection_flag && S->section != 0) || S->maxlen) && u) && 
		   strcasecmp(S->name, "DP_ID") &&
		   strcasecmp(S->name, "URL_ID") &&
		   strcasecmp(S->name, "URL") &&
		   strcasecmp(S->name, "Title") &&
		   strcasecmp(S->name, "Status") &&
		   strcasecmp(S->name, "Charset") &&
		   strcasecmp(S->name, "Content-Type") &&
		   strcasecmp(S->name, "Content-Length") &&
		   strcasecmp(S->name, "Content-Language") &&
		   strcasecmp(S->name, "Tag") &&
/*		   strcasecmp(S->name, "Pop_Rank") &&*/
		   strcasecmp(S->name, "Z") &&
#ifdef WITH_MULTIDBADDR
		   (!(numsection_flag) || strcasecmp(S->name, "dbnum")) &&
#endif
		   strcasecmp(S->name, "Category"))
			continue;

/*		fprintf(stderr, "+Sec.name: %s  .section:%d  .maxlen: %d\n", S->name, S->section, S->maxlen);*/
		len += dps_strlen(S->name) + ((S->curlen) ? S->curlen : dps_strlen(S->val)) + 32;
	}

	textbuf = (char*)DpsMalloc(len + 1);
	if (textbuf == NULL) return NULL;

	textbuf[0]='\0';
	
	dps_snprintf(textbuf, len, "<DOC");
	dps_strcpy(textbuf, "<DOC");
	end = textbuf + 4 /*dps_strlen(textbuf)*/;
	
	for (r = 0; r < 256; r++)
	for(i = 0; i < Doc->Sections.Root[r].nvars; i++) {
		DPS_VAR	*S = &Doc->Sections.Root[r].Var[i];
		
		if(!S->name || !S->val || (!S->val[0] && strcasecmp(S->name, "Z")) ) continue;
/*		fprintf(stderr, "u:%d section: %d  name: %s  value: %s\n", u, S->section, S->name, S->val);*/
		if(!(((numsection_flag && S->section != 0) || S->maxlen) && u) && 
		   strcasecmp(S->name,"DP_ID") &&
		   strcasecmp(S->name,"URL_ID") &&
		   strcasecmp(S->name,"URL") &&
		   strcasecmp(S->name,"Title") &&
		   strcasecmp(S->name,"Status") &&
		   strcasecmp(S->name,"Charset") &&
		   strcasecmp(S->name,"Content-Type") &&
		   strcasecmp(S->name,"Content-Length") &&
		   strcasecmp(S->name,"Content-Language") &&
		   strcasecmp(S->name,"Tag") &&
/*		   strcasecmp(S->name, "Pop_Rank") &&*/
		   strcasecmp(S->name, "Z") &&
#ifdef WITH_MULTIDBADDR
		   (!(numsection_flag) || strcasecmp(S->name, "dbnum")) &&
#endif
		   strcasecmp(S->name,"Category"))
			continue;

		l = (end - textbuf);
		if (l + 2 < len) {
		  if (strcasecmp(S->name,"URL")) {
		    dps_snprintf(end, len-l, "\t%s=\"%s\"", S->name, S->val );
		  } else {
		    DPS_VAR *ES = (e_url_flag) ? DpsVarListFind(&Doc->Sections, "E_URL") : NULL;
		    if (ES != NULL) {
		      dps_snprintf(end, len-l, "\tURL=\"%s\"", ES->txt_val ? ES->txt_val : ES->val );
		    } else {
		      dps_snprintf(end, len-l, "\tURL=\"%s\"", S->txt_val ? S->txt_val : S->val );
		    }
		  }
		  end = end + dps_strlen(end);
		}
	}
	if (len - (end - textbuf) > 0) {
	  *end++ = '>';
	  *end = '\0';
	}
/*	dps_snprintf(end, len - (end - textbuf), ">\0");*/

/*	fprintf(stderr, " -- textbuf: %s\n", textbuf);*/
	return textbuf;
}

int __DPSCALL DpsDocFromTextBuf(DPS_DOCUMENT * Doc,const char *textbuf){
	const char	*htok, *last;
	DPS_HTMLTOK	tag;
	DPS_VAR	        Sec;
	size_t		i;
	
	if (textbuf == NULL) return DPS_OK;

	DpsHTMLTOKInit(&tag);
	bzero(&Sec, sizeof(Sec));
	
	htok=DpsHTMLToken(textbuf,&last,&tag);
	
	if(!htok || tag.type!=DPS_HTML_TAG)
		return DPS_OK;
	
	for(i=1;i<tag.ntoks;i++){
		char	*name = DpsStrndup(tag.toks[i].name, tag.toks[i].nlen);
		char	*data = DpsStrndup(DPS_NULL2EMPTY(tag.toks[i].val), tag.toks[i].vlen);
/*		char	*name = tag.toks[i].name;
		char	*data = DPS_NULL2EMPTY(tag.toks[i].val);*/
/*		name[tag.toks[i].nlen] = '\0';
		if (*data) data[tag.toks[i].vlen] = '\0';*/
		bzero((void*)&Sec, sizeof(Sec));
		Sec.name = (strcasecmp(name, "ID")) ? name : "DP_ID";
		Sec.val = data;
		Sec.txt_val = data;
		DpsVarListReplace(&Doc->Sections, &Sec);
/*		if (!strcasecmp(name, "dbnum")) D->dbnum = DPS_ATOI(data);*/
/*		name[tag.toks[i].nlen] = '=';
		if (*data) data[tag.toks[i].vlen] = '"';*/
		DPS_FREE(name);
		DPS_FREE(data);
	}
	return 0;
}


int DpsDocLookupConn(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
	const char *proxy;
	int u;
	
	TRACE_IN(Indexer, "DpsDocLookupConn");

	if((proxy=DpsVarListFindStr(&Doc->RequestHeaders,"Proxy",NULL))){
		char *port;

		DpsLog(Indexer, DPS_LOG_DEBUG, "Using Proxy: %s", proxy);
		Doc->connp.hostname = (char*)DpsStrdup(proxy);
		if((port=strchr(Doc->connp.hostname,':'))){
			*port++='\0';
			Doc->connp.port=atoi(port);
		}else{
			Doc->connp.port=3128;
		}
	}else{
		if (Doc->CurURL.hostname){
			Doc->connp.hostname = (char*)DpsStrdup(Doc->CurURL.hostname);
			Doc->connp.port=Doc->CurURL.port?Doc->CurURL.port:Doc->CurURL.default_port;
		}
	}

	Doc->connp.charset_id = Doc->charset_id;

	u = DpsHostLookup(Indexer, &Doc->connp);

	if(Doc->CurURL.hostname != NULL && *Doc->CurURL.hostname != '\0' && (u != 0)) {
	  DpsLog(Indexer, DPS_LOG_DEBUG, "Can't resolve host '%s' [u:%d]", Doc->connp.hostname, u);
		Doc->method = DPS_METHOD_VISITLATER;
		DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_SERVICE_UNAVAILABLE);
	}
	TRACE_OUT(Indexer);
	return DPS_OK;
}


int DpsDocAddDocExtraHeaders(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
  int rc = DPS_OK;
  /* Host Name for virtual hosts */

  if((Doc->CurURL.hostname != NULL) && (Doc->CurURL.hostname[0] != '\0') ) {
    char   arg[128] = "";
    char   *ascii = NULL;
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
    DPS_CHARSET *url_cs, *uni_cs;
    DPS_CONV  url_uni;
    char    *uni = NULL;
    size_t len;

    if (Doc->charset_id != DPS_CHARSET_US_ASCII) {
      uni_cs = DpsGetCharSet("UTF8");
      url_cs = DpsGetCharSetByID(Doc->charset_id);
      DpsConvInit(&url_uni, url_cs, uni_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_URL);

      uni = (char*)DpsMalloc(len = (48 * dps_strlen(Doc->CurURL.hostname + 1)));
      if (uni == NULL) {
	return DPS_ERROR;
      }
      DpsConv(&url_uni, (char*)uni, len, Doc->CurURL.hostname, len);
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
      DPS_FREE(uni); 
      DpsLog(Indexer, DPS_LOG_DEBUG, "IDN Robots Host: %s [%s] -> %s", Doc->CurURL.hostname, url_cs->name, ascii);
    } else
#endif
      {
	ascii = DpsStrdup(Doc->CurURL.hostname);
      }

    if(Doc->CurURL.port){
      dps_snprintf(arg, 128, "%s:%d", ascii, Doc->CurURL.port);
      DpsVarListReplaceStr(&Doc->RequestHeaders,"Host", arg);
    }else{
      DpsVarListReplaceStr(&Doc->RequestHeaders,"Host", ascii);
    }

/* Add Cookies if any */
    if (Doc->Spider.use_cookies) DpsCookiesFind(Indexer, Doc, ascii);
    if (Indexer->Flags.provide_referer && strncasecmp(Doc->CurURL.schema, "http", 4) == 0 ) 
      rc = DpsURLAction(Indexer, Doc, DPS_URL_ACTION_REFERER);
    
    DPS_FREE(ascii);
  }
  return rc;
}


int DpsDocAddConfExtraHeaders(DPS_ENV *Conf,DPS_DOCUMENT *Doc) {
	char		arg[128]="";
	const char	*lc;
	size_t		i, r;
	
	/* If LocalCharset specified, add Accept-Charset header */
	if((lc=DpsVarListFindStr(&Conf->Vars,"LocalCharset",NULL))){
	        dps_snprintf(arg, sizeof(arg)-1, "%s;q=1.0,UTF-8;q=0.5,*;q=0.1", DpsCharsetCanonicalName(lc));
		arg[sizeof(arg)-1]='\0';
		DpsVarListAddStr(&Doc->RequestHeaders,"Accept-Charset",arg);
	}
	
	r = (size_t) 'r';
	for (i = 0; i < Conf->Vars.Root[r].nvars; i++) {
		DPS_VAR *v = &Conf->Vars.Root[r].Var[i];
		if(!strncmp(v->name, "Request.", 8))
			DpsVarListInsStr(&Doc->RequestHeaders,v->name+8,v->val);
	}
	
	DpsVarListInsStr(&Doc->RequestHeaders, "Connection","close");
#ifdef HAVE_ZLIB
	DpsVarListInsStr(&Doc->RequestHeaders,"Accept-Encoding","gzip,deflate,compress");
	DpsVarListInsStr(&Doc->RequestHeaders,"TE","gzip,deflate,compress,identity;q=0.5,chuncked;q=0.1");
#else
	DpsVarListInsStr(&Doc->RequestHeaders,"TE","identity,chuncked;q=0.1");
#endif
	return DPS_OK;
}


int DpsDocAddServExtraHeaders(DPS_SERVER *Server,DPS_DOCUMENT *Doc) {
	char	arg[128]="";
	char    bu[] = "aprv\0";
	size_t	i, j, r;
	
	for (j = 0; bu[j] != 0; j++) {
	  r = (size_t) bu[j];
	  for(i = 0; i < Server->Vars.Root[r].nvars; i++) {
		DPS_VAR *Hdr = &Server->Vars.Root[r].Var[i];

		if(!strcasecmp(Hdr->name,"AuthBasic")){
			/* HTTP and FTP specific stuff */
			if((!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "http")) ||
				(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "https")) ||
				(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "ftp")) ||
				(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "https"))) {
				
				dps_snprintf(arg, sizeof(arg)-1, "Basic %s", Hdr->val);
				arg[sizeof(arg)-1]='\0';
				DpsVarListReplaceStr(&Doc->RequestHeaders,"Authorization",arg);
			}
			
			if(!strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "nntp") || 
			   !strcasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "news")) {
				/* Auth if required                      */
				/* NNTPGet will parse this header        */
				/* We'll pass authinfo still in base64   */
				/* form to avoid plain user name in core */
				/* file on crashes if any                */
				
				if(Hdr->val && Hdr->val[0]){
					DpsVarListReplaceStr(&Doc->RequestHeaders,"Authorization",Hdr->val);
				}
			}
		}else
		if(!strcasecmp(Hdr->name,"ProxyAuthBasic")){
			if(Hdr->val && Hdr->val[0]){
				dps_snprintf(arg, sizeof(arg)-1, "Basic %s", Hdr->val);
				arg[sizeof(arg)-1]='\0';
				DpsVarListReplaceStr(&Doc->RequestHeaders,"Proxy-Authorization",arg);
			}
		}else
		if(!strcasecmp(Hdr->name, "Proxy")){
			if(Hdr->val && Hdr->val[0]){
				DpsVarListReplaceStr(&Doc->RequestHeaders, Hdr->name, Hdr->val);
			}
		}else
		  if(!strcasecmp(Hdr->name, "VaryLang") && DpsVarListFind(&Doc->RequestHeaders, "Accept-Language") == NULL) {
			if(Hdr->val && Hdr->val[0]){
				DpsVarListReplaceStr(&Doc->RequestHeaders, "Accept-Language", Hdr->val);
			}
		}else{
			if(!strncmp(Hdr->name,"Request.",8))
				DpsVarListReplaceStr(&Doc->RequestHeaders,Hdr->name+8,Hdr->val);
		}
	  }
	}
	return DPS_OK;
}


int DpsDocProcessResponseHeaders(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {  /* This function must have exclusive acces to Conf */
	DPS_VAR		*var;
	const char      *content_type = DpsVarListFindStr(&Doc->Sections, "Content-Type", NULL);
	const int       content_length = DpsVarListFindInt(&Doc->Sections, "Content-Length", 0);
	const int       status = DpsVarListFindInt(&Doc->Sections, "Status", 0);


	if ((size_t)content_length > Doc->Buf.max_size) {
	  DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_PARTIAL_OK);
	}

	if (content_type != NULL) {
	  char *p;
	  if ((p = strstr(content_type, "charset=")) != NULL) {
	    const char *cs = DpsTrim(p + 8, " \t;\"'");
	    *p = '\0';
	    DpsRTrim((char*)content_type, "; \t");
	    if ((p = strchr(cs, ' '))) {
	      *p = '\0';
	      DpsRTrim((char*)cs, ";\t\"");
	    }
	    if ((p = strchr(cs, HT_INT))) {
	      *p = '\0';
	      DpsRTrim((char*)cs, ";\"");
	    }
	    p = (char*)DpsCharsetCanonicalName(cs);
	    DpsVarListReplaceStr(&Doc->Sections, "Server-Charset", p ? p : cs);
	  }
	  if ((p = strchr(content_type, ' '))) {
	    *p = '\0';
	    DpsRTrim((char*)content_type, ";\t");
	  }
	  if ((p = strchr(content_type, HT_INT))) {
	    *p = '\0';
	    DpsRTrim((char*)content_type, "; ");
	  }
	}
	
	if ((var=DpsVarListFind(&Doc->Sections,"Server"))){
		if(!strcasecmp("yes",DpsVarListFindStr(&Indexer->Vars,"ForceIISCharset1251","no"))){
			if (!DpsWildCmp(var->val,"*Microsoft*")||!DpsWildCmp(var->val,"*IIS*")){
				const char *cs;
				if((cs=DpsCharsetCanonicalName("windows-1251")))
					DpsVarListReplaceStr(&Doc->Sections, "Server-Charset", cs);
			}
		}
	}
	if((strcasecmp(DpsVarListFindStr(&Indexer->Vars,"UseRemoteContentType","yes"),"yes") != 0) || (content_type == NULL) 
	   || (strcasecmp(content_type, "application/octet-stream") == 0)
	   || (strcasecmp(content_type, "unknown") == 0)
	   ) {
	   	DPS_MATCH	*M;
	   	const char	*fn = (Doc->CurURL.filename && Doc->CurURL.filename[0]) ? Doc->CurURL.filename : "index.html";
		
		DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
		if((M = DpsMatchListFind(&Indexer->Conf->MimeTypes, fn, 0, NULL))) {
			DpsVarListReplaceStr(&Doc->Sections,"Content-Type",M->arg);
		} else {
		  if (NULL != (fn = DpsVarListFindStr(&Doc->Sections, "URL", NULL))) {
		    if((M = DpsMatchListFind(&Indexer->Conf->MimeTypes, fn, 0, NULL)))
		        DpsVarListReplaceStr(&Doc->Sections, "Content-Type", M->arg);
		  }
		}
		DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	}
	if(!DpsVarListFind(&Doc->Sections,"Content-Type")) {
		DpsVarListAddStr(&Doc->Sections,"Content-Type","application/octet-stream");
	}

	if ((var = DpsVarListFind(&Doc->Sections, "Location"))) {
	  int short_url = 0;
	  if ((Doc->CurURL.len < 32) && (strcmp(Doc->CurURL.path, "/") == 0) && (Doc->CurURL.hostname != NULL) && (Doc->CurURL.filename != NULL)) short_url = 1;
	  if ((Doc->subdoc < Indexer->Flags.SubDocLevel) && (Doc->sd_cnt < Indexer->Flags.SubDocCnt) && 
	      ((status == DPS_HTTP_STATUS_MOVED_TEMPORARILY) 
	       || (status == DPS_HTTP_STATUS_MOVED_PARMANENTLY && (Doc->subdoc > 1 || short_url))
	       || (status == DPS_HTTP_STATUS_SEE_OTHER) 
	       || (status == DPS_HTTP_STATUS_TEMPORARY_REDIRECT) )) {
	    DpsIndexSubDoc(Indexer, Doc, NULL, NULL, var->val);
	  } else {
	        int parse_rc;
		DPS_URL *newURL = DpsURLInit(NULL);
		if (newURL == NULL) return DPS_ERROR;
		parse_rc = DpsURLParse(newURL, var->val);
		switch(parse_rc) {
				case DPS_URL_OK:
					if (DPS_NULL2EMPTY(newURL->schema) != NULL) {
						DPS_HREF Href;
						DpsHrefInit(&Href);
						Href.url=var->val;
						Href.hops = DpsVarListFindInt(&Doc->Sections,"Hops", 0) + 1;
						Href.referrer=DpsVarListFindInt(&Doc->Sections,"Referrer-ID",0);
						Href.method=DPS_METHOD_GET;
						Href.site_id = 0; /*DpsVarListFindInt(&Doc->Sections, "Site_id", 0);*/
						Href.server_id = DpsVarListFindInt(&Doc->Sections,"Server_id", 0);
						DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
					}
					break;
				case DPS_URL_LONG:
					DpsLog(Indexer,DPS_LOG_ERROR,"Redirect URL too long: '%s'",var->val);
					break;
				case DPS_URL_BAD:
				default:
					DpsLog(Indexer,DPS_LOG_ERROR,"Error in redirect URL: '%s'",var->val);
		}
		DpsURLFree(newURL);
	  }
	}
	return DPS_OK;
}
