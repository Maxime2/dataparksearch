/* Copyright (C) 2013-2014 Maxim Zakharov. All rights reserved.

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

#include "dps_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "dps_common.h"
#include "dps_agent.h"
#include "dps_doc.h"
#include "dps_env.h"
#include "dps_hash.h"
#include "dps_id3.h"
#include "dps_indexer.h"
#include "dps_log.h"
#include "dps_mutex.h"
#include "dps_parsehtml.h"
#include "dps_proto.h"
#include "dps_sea.h"
#include "dps_server.h"
#include "dps_signals.h"
#include "dps_uniconv.h"
#include "dps_unidata.h"
#include "dps_url.h"
#include "dps_utils.h"
#include "dps_vars.h"

#define DPCONV_BUF_SIZE 4096

static int name_width = 8;

static const char *csgroup(const DPS_CHARSET *cs) {
    switch(cs->family){
    case DPS_CHARSET_ARABIC		:	return "Arabic";
    case DPS_CHARSET_ARMENIAN	:	return "Armenian";
    case DPS_CHARSET_BALTIC		:	return "Baltic";
    case DPS_CHARSET_CELTIC		:	return "Celtic";
    case DPS_CHARSET_CENTRAL	:	return "Central Eur";
    case DPS_CHARSET_CHINESE_SIMPLIFIED:	return "Chinese Simplified";
    case DPS_CHARSET_CHINESE_TRADITIONAL:	return "Chinese Traditional";
    case DPS_CHARSET_CYRILLIC	:	return "Cyrillic";
    case DPS_CHARSET_GREEK		:	return "Greek";
    case DPS_CHARSET_HEBREW		:	return "Hebrew";
    case DPS_CHARSET_ICELANDIC	:	return "Icelandic";
    case DPS_CHARSET_JAPANESE	:	return "Japanese";
    case DPS_CHARSET_KOREAN		:	return "Korean";
    case DPS_CHARSET_NORDIC		:	return "Nordic";
    case DPS_CHARSET_SOUTHERN	:	return "South Eur";
    case DPS_CHARSET_THAI		:	return "Thai";
    case DPS_CHARSET_TURKISH	:	return "Turkish";
    case DPS_CHARSET_UNICODE	:	return "Unicode";
    case DPS_CHARSET_VIETNAMESE	:	return "Vietnamese";
    case DPS_CHARSET_WESTERN	:	return "Western";
    case DPS_CHARSET_GEORGIAN       :	return "Georgian";
    case DPS_CHARSET_INDIAN 	:	return "Indian";
    case DPS_CHARSET_LAO: return "Lao";
    case DPS_CHARSET_IRANIAN: return "Iranian";
    default				:	return "Unknown";
    }
}

static int cmpgrp(const void *v1, const void *v2){
    int res;
    const DPS_CHARSET *c1=v1;
    const DPS_CHARSET *c2=v2;
    if ((res=strcasecmp(csgroup(c1),csgroup(c2))))return res;
    return strcasecmp(c1->name,c2->name);
}

static void display_charsets(void){
    DPS_CHARSET *cs=NULL;
    DPS_CHARSET c[100];
    size_t i=0;
    size_t n=0;
    int family=-1;
	
    for(cs=DpsGetCharSetByID(0) ; cs && cs->name ; cs++){
	/* Skip not compiled charsets */
	if(cs->family != DPS_CHARSET_UNKNOWN)
	    c[n++]=*cs;
    }
    fprintf(stderr,"\n%d charsets available:\n", (int)n);
    
    DpsSort(c,n,sizeof(DPS_CHARSET),&cmpgrp);
    for(i=0;i<n;i++){
	if(family!=c[i].family){
	    fprintf(stderr,"\n%19s : ",csgroup(&c[i]));
	    family=c[i].family;
	}
	fprintf(stderr,"%s ",c[i].name);
    }
    fprintf(stderr,"\n");
}

static int usage(int level){

    fprintf(stderr, "\ndpurl2text from %s-%s-%s\n\
(C)2013, Maxim Zakharov.\n\
\n\
Usage: dpurl2text [OPTIONS] url [config file]\n\
\n\
Converting options:\n\
  -v n          verbose level, 0-5\n\
  -e            use HTML escape entities for input\n\
  -E            use HTML escape entities for output\n\
  -f charset    set RemoteCharset to charset\n\
  -t charset    set LocalCharset to charset\n\
"
"  -h,-?         print help page and exit\n\
  -hh,-??       print more help and exit\n\
\n\
\n\
\n",
	    PACKAGE,VERSION,DPS_DBTYPE);
	
    if (level > 1) display_charsets();
    return(0);
}




static int DpsPrintSections(DPS_AGENT * Indexer, DPS_DOCUMENT * Doc) {
  size_t		i;
  const char	*doccset;
  DPS_CHARSET	*doccs;
  DPS_CHARSET	*loccs;
  DPS_CHARSET	*sys_int;
  DPS_CONV	dc_uni;
  DPS_CONV      utf8_uni;
  DPS_TEXTLIST	*extractor_tlist = &Doc->ExtractorList;
  DPS_TEXTLIST	*tlist = &Doc->TextList;
  DPS_VAR	*Sec;
  int		res = DPS_OK;
  dpshash32_t	crc32 = 0;
  int		crossec, seasec, makesea;
  dpsunicode_t    *uword = NULL;    /* Word in UNICODE      */
  char            *lcsword = NULL;  /* Word in LocalCharset */
  size_t          max_word_len, min_word_len, uwordlen = DPS_MAXWORDSIZE;
  size_t          indexed_size = 0, indexed_limit = (size_t)DpsVarListFindInt(&Doc->Sections, "IndexDocSizeLimit", 0);
  const char      *content_lang = DpsVarListFindStr(&Doc->Sections, "Content-Language", "");
  const char      *SEASections = DpsVarListFindStr(&Indexer->Vars, "SEASections", "body");
  DPS_DSTR        exrpt;
#ifdef HAVE_ASPELL
  AspellCanHaveError *ret;
  AspellSpeller *speller;
  int have_speller = 0;
  DPS_DSTR suggest;
#endif
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif

  TRACE_IN(Indexer, "DpsPrintSections");
  DpsLog(Indexer, DPS_LOG_DEBUG, "Printing sections");

  if (DpsDSTRInit(&exrpt, dps_max( 4096, Doc->Buf.size >> 2 )) == NULL) {
    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
    DpsViolationExit(Indexer->handle, paran);
#endif
    return DPS_ERROR;
  }

  if ((uword = (dpsunicode_t*)DpsMalloc((uwordlen + 1) * sizeof(dpsunicode_t))) == NULL) {
    DpsDSTRFree(&exrpt);
    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
    DpsViolationExit(Indexer->handle, paran);
#endif
    return DPS_ERROR;
  }
  if ((lcsword = (char*)DpsMalloc(12 * uwordlen + 1)) == NULL) { DPS_FREE(uword); 
    DpsDSTRFree(&exrpt);
    TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
    DpsViolationExit(Indexer->handle, paran);
#endif
    return DPS_ERROR; 
  }

  Sec = DpsVarListFind(&Doc->Sections,"crosswords");
  crossec = Sec ? Sec->section : 0;
  Sec = DpsVarListFind(&Doc->Sections, "sea");
  seasec = Sec ? Sec->section : 0;
  makesea = Sec ? 1 : 0;

  doccset=DpsVarListFindStr(&Doc->Sections,"Charset",NULL);
  if(!doccset||!*doccset)doccset=DpsVarListFindStr(&Doc->Sections,"RemoteCharset","iso-8859-1");
  doccs=DpsGetCharSet(doccset);
  if(!doccs)doccs=DpsGetCharSet("iso-8859-1");
  loccs = Doc->lcs;
  if (!loccs) loccs = Indexer->Conf->lcs;
  if (!loccs) loccs = DpsGetCharSet("iso-8859-1");
  sys_int=DpsGetCharSet("sys-int");

  DpsConvInit(&dc_uni, loccs /*doccs*/, sys_int, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
  DpsConvInit(&utf8_uni, DpsGetCharSet("utf-8"), sys_int, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
  max_word_len = Indexer->WordParam.max_word_len;
  min_word_len = Indexer->WordParam.min_word_len;
	
#ifdef HAVE_ASPELL
  if (Indexer->Flags.use_aspellext) {
    AspellConfig * spell_config2;
    DPS_GETLOCK(Indexer, DPS_LOCK_ASPELL);
    spell_config2 = aspell_config_clone(Indexer->aspell_config);
    aspell_config_replace(spell_config2, "lang", (*content_lang != '\0') ? content_lang : "en");
    ret = new_aspell_speller(spell_config2);
    if (aspell_error(ret) != 0) {
      DpsLog(Indexer, DPS_LOG_ERROR, " aspell error: %s", aspell_error_message(ret));
      delete_aspell_can_have_error(ret);
    } else {
      speller = to_aspell_speller(ret);
      DpsDSTRInit(&suggest, 1024);
      have_speller = 1;
    }
    delete_aspell_config(spell_config2);
    DPS_RELEASELOCK(Indexer, DPS_LOCK_ASPELL);
  }
#endif				

  if (Indexer->Flags.LongestTextItems > 0) {
    DPS_TEXTITEM **items = (DPS_TEXTITEM**)DpsMalloc((tlist->nitems + 1) * sizeof(DPS_TEXTITEM*));
    if (items != NULL) {
      for(i = 0; i < tlist->nitems; i++) items[i] = &tlist->Items[i];
      DpsSort(items, tlist->nitems, sizeof(DPS_TEXTITEM*), (qsort_cmp) dps_itemptr_cmp);
      for(i = 0; (i < tlist->nitems) && (i < (size_t)Indexer->Flags.LongestTextItems); i++) items[i]->marked = 1;
    }
    DPS_FREE(items);
  }

/* Now convert everything to UNICODE format and calculate CRC32 */

  for(i = 0; i < tlist->nitems; i++) {
    size_t		srclen;
    size_t		dstlen;
    size_t		reslen;
    char		*src,*dst;
    dpsunicode_t	*ustr = NULL, *UStr = NULL;
    DPS_TEXTITEM	*Item = &tlist->Items[i];
		
		
    srclen = ((Item->len) ? Item->len : (dps_strlen(Item->str)) + 1);	/* with '\0' */
    dstlen = (16 * (srclen + 1)) * sizeof(dpsunicode_t);	/* with '\0' */
		
    if ((ustr = (dpsunicode_t*)DpsMalloc(dstlen + 1)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't alloc %u bytes", __FILE__, __LINE__, dstlen);
      DPS_FREE(uword); DPS_FREE(lcsword); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
		
    src = Item->str;
    dst = (char*)ustr;

    DpsConv(&dc_uni, dst, dstlen, src, srclen);
/*		DpsSGMLUniUnescape(ustr);*/
    DpsUniRemoveDoubleSpaces(ustr);
    if ((UStr = DpsUniDup(ustr)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't DpsUniDup", __FILE__, __LINE__);
      DPS_FREE(uword); DPS_FREE(lcsword); DPS_FREE(ustr); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
    reslen = DpsUniLen(ustr);
		
    /*
      TODO for clones detection:
      Replace any separators into space to ignore 
      various pseudo-graphics, commas, semicolons
      and so on to improve clone detection quality
    */
    if (strncasecmp(DPS_NULL2EMPTY(Item->section_name), "url", 3) != 0) /* do not calculate crc32  on url* sections */
      crc32 = DpsHash32Update(crc32, (char*)ustr, reslen * sizeof(dpsunicode_t));

    if (makesea && strstr(SEASections, Item->section_name)) {
      DpsDSTRAppendUniWithSpace(&exrpt, UStr);
    }

    if (reslen > 0) {
	DpsConv(&Indexer->uni_lc, dst, dstlen, (char*)UStr, reslen * sizeof(dpsunicode_t));
	DpsLog(Indexer, DPS_LOG_INFO, "%*s: %s", name_width, Item->section_name, dst);
    }
		
    DPS_FREE(ustr);
    DPS_FREE(UStr);
    if (res != DPS_OK) break;
  }

 /* do the same for libextrated meta-data, which is always encoded in UTF-8 */
  for(i = 0; i < extractor_tlist->nitems; i++) {
    size_t		srclen;
    size_t		dstlen;
    size_t		reslen;
    char		*src,*dst;
    dpsunicode_t	*ustr = NULL, *UStr = NULL;
    DPS_TEXTITEM	*Item = &extractor_tlist->Items[i];

    srclen = ((Item->len) ? Item->len : (dps_strlen(Item->str)) + 1);	/* with '\0' */
    dstlen = (16 * (srclen + 1)) * sizeof(dpsunicode_t);	/* with '\0' */
		
    if ((ustr = (dpsunicode_t*)DpsMalloc(dstlen + 1)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't alloc %u bytes", __FILE__, __LINE__, dstlen);
      DPS_FREE(uword); DPS_FREE(lcsword); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
		
    src = Item->str;
    dst = (char*)ustr;

    DpsConv(&utf8_uni, dst, dstlen, src, srclen);
    DpsUniRemoveDoubleSpaces(ustr);
    if ((UStr = DpsUniDup(ustr)) == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "%s:%d Can't DpsUniDup", __FILE__, __LINE__);
      DPS_FREE(uword); DPS_FREE(lcsword); DPS_FREE(ustr); 
      DpsDSTRFree(&exrpt);
      TRACE_OUT(Indexer);
#ifdef HAVE_ASPELL
      if (have_speller) DpsDSTRFree(&suggest); 
#endif
#ifdef WITH_PARANOIA
      DpsViolationExit(Indexer->handle, paran);
#endif
      return DPS_ERROR;
    }
    reslen = DpsUniLen(ustr);

    /*
      TODO for clones detection:
      Replace any separators into space to ignore 
      various pseudo-graphics, commas, semicolons
      and so on to improve clone detection quality
    */
    if (strncasecmp(DPS_NULL2EMPTY(Item->section_name), "url", 3) != 0) /* do not calculate crc32  on url* sections */
      crc32 = DpsHash32Update(crc32, (char*)ustr, reslen * sizeof(dpsunicode_t));

    if (makesea && strstr(SEASections, Item->section_name)) {
      DpsDSTRAppendUniWithSpace(&exrpt, UStr);
    }

    if (reslen > 0) {
	DpsConv(&Indexer->uni_lc, dst, dstlen, (char*)UStr, reslen * sizeof(dpsunicode_t));
	DpsLog(Indexer, DPS_LOG_INFO, "%*s: %s", name_width, Item->section_name, dst);
    }
		
    DPS_FREE(ustr);
    DPS_FREE(UStr);
    if (res != DPS_OK) break;
  }



  DpsVarListReplaceInt(&Doc->Sections,"crc32", (int)crc32);

  if (makesea) {
    DpsDSTRAppendUni(&exrpt, 0);
    DpsSEAMake(Indexer, Doc, &exrpt, content_lang, &indexed_size, &indexed_limit, max_word_len, min_word_len, crossec, seasec
#ifdef HAVE_ASPELL
	       , have_speller, speller
#endif
	       );
  }

#ifdef HAVE_ASPELL
  if (have_speller && Indexer->Flags.use_aspellext) {
    DPS_GETLOCK(Indexer, DPS_LOCK_ASPELL);
    delete_aspell_speller(speller);
    DPS_RELEASELOCK(Indexer, DPS_LOCK_ASPELL);
    if (suggest.data_size > 0) {
      char *spelling = DpsMalloc(24 * (suggest.data_size + 2));
      DpsDSTRAppendUni(&suggest, 0);
      if (spelling != NULL) {
	DpsConv(&Indexer->uni_lc, spelling, 24 * (suggest.data_size + 1), (char*)suggest.data, suggest.data_size);
	DpsVarListReplaceStr(&Doc->Sections, "spelling", spelling);
	DPS_FREE(spelling);
      }
    }
  }
  if (have_speller) DpsDSTRFree(&suggest); 
#endif

  DpsLog(Indexer, DPS_LOG_DEBUG, "Done Printing sections");
	
  DPS_FREE(uword); DPS_FREE(lcsword); 
  DpsDSTRFree(&exrpt);
  TRACE_OUT(Indexer);
#ifdef WITH_PARANOIA
  DpsViolationExit(Indexer->handle, paran);
#endif
  return res;
}





int main(int argc, char **argv, char **envp) {
    char cname[PATH_MAX + 1024] = "";
    DPS_AGENT Main, *Indexer = &Main;
    DPS_ENV Conf;
    char *charset_from = NULL, *charset_to = NULL;
    int html_from = 0, html_to = 0;
    char *url = NULL, *env;
    int ch, help = 0;
    dps_uint8 flags    = DPS_FLAG_ADD_SERV | DPS_FLAG_LOAD_LANGMAP; /* we load langmaps always */
    DPS_DOCUMENT	*Doc;
    DPS_SERVER Srv;
    int status = 0;

    while ((ch = getopt(argc, argv, "EUeh?t:f:v:")) != -1){
	switch (ch) {
	case 'E': html_to = DPS_RECODE_HTML_TO; break;
	case 'e': html_from = DPS_RECODE_HTML_FROM; break;
	case 'v': DpsSetLogLevel(NULL, atoi(optarg)); break;
	case 't': charset_to =  optarg; break;
	case 'f': charset_from = optarg; break;
	case 'U': flags |= DPS_FLAG_UNOCON;break;
	case '?':
	case 'h':
	default:
	    help++;
	}
    }

    argc -= optind; argv += optind;

    if ((argc > 2) || (argc < 1) || (help)) {
	usage(help);
	return 1;
    }

    if (charset_from != NULL) {
	if (!DpsGetCharSet(charset_from)) {
	    fprintf(stderr, "Charset: %s not found or not supported", charset_from);
	    display_charsets();
	    return 1;
	}
    }

    if (charset_to != NULL) {
	if (!DpsGetCharSet(charset_to)) {
	    fprintf(stderr, "Charset: %s not found or not supported", charset_to);
	    display_charsets();
	    return 1;
	}
    }

    bzero(&Conf, sizeof(Conf));
    bzero(&Main, sizeof(Main));

    DpsInit(argc, argv, envp); /* Initialize library */
    DpsGetSemLimit();
     
    if (DpsEnvInit(&Conf) == NULL) exit(1);
    DpsAgentInit(&Main, &Conf, 0);

    Main.flags = Conf.flags = flags;
    Main.flags |= DPS_FLAG_UNOCON;
#ifndef HAVE_PTHREAD
    Conf.flags |= DPS_FLAG_UNOCON;
#endif

    env = getenv("DPS_CONF_DIR");
    DpsVarListReplaceStr(&Conf.Vars, "EtcDir", env ? env : DPS_CONF_DIR);
     
    env = getenv("DPS_SHARE_DIR");
    DpsVarListReplaceStr(&Conf.Vars, "ShareDir", env ? env : DPS_SHARE_DIR);
     
    if (argc == 2) {
	dps_strncpy(cname, argv[1], sizeof(cname));
	cname[sizeof(cname) - 1] = '\0';
    } else {
	const char *cd = DpsVarListFindStr(&Conf.Vars,"DPS_CONF_DIR",DPS_CONF_DIR);
	dps_snprintf(cname, sizeof(cname), "%s%s%s", cd, DPSSLASHSTR, "indexer.conf");
	cname[sizeof(cname) - 1] = '\0';
    }

    DpsOpenLog("dpurl2text", &Conf, log2stderr);
    if (DPS_OK != DpsIndexerEnvLoad(&Main, cname, flags)) {
	fprintf(stderr,"%s\n",DpsEnvErrMsg(&Conf));
	DpsAgentFree(&Main);
	DpsEnvFree(&Conf);
	exit(1);
    }

    DpsInitMutexes();
    Main.Flags = Conf.Flags;
    Main.WordParam = Conf.WordParam;
    {
	DPS_CHARSET *unics, *loccs;
#ifdef HAVE_ASPELL
	DPS_CHARSET *utfcs;
#endif
	if (charset_to != NULL) {
	    loccs = Conf.lcs = DpsGetCharSet(charset_to);
	    DpsVarListReplaceStr(&Conf.Vars, "LocalCharset", charset_to);
	} else loccs = Conf.lcs;
	if (!loccs) loccs = DpsGetCharSet(DpsVarListFindStr(&Conf.Vars, "LocalCharset", "iso-8859-1"));
	unics = DpsGetCharSet("sys-int");
	DpsConvInit(&Main.uni_lc, unics, loccs, Conf.CharsToEscape, DPS_RECODE_HTML_FROM | html_to);
	DpsConvInit(&Main.lc_uni, loccs, unics, Conf.CharsToEscape, DPS_RECODE_HTML_TO | html_from);
	DpsConvInit(&Main.lc_uni_text, loccs, unics, Conf.CharsToEscape, DPS_RECODE_TEXT);
#ifdef HAVE_ASPELL
	utfcs = DpsGetCharSet("UTF-8");
	DpsConvInit(&Main.utf_lc, utfcs, loccs, Conf.CharsToEscape, DPS_RECODE_HTML);
#endif
    }
    /* set locale if specified */
    { const char *locname;
	if ((locname = DpsVarListFindStr(&Conf.Vars, "Locale", NULL)) != NULL) {
	    setlocale(LC_ALL, locname);
	}
    }
    DpsSigHandlersInit(&Main);
    DpsSetLockProc(&Conf, DpsLockProc);


    url = argv[0];
    Doc = DpsDocInit(NULL);
    DpsServerInit(&Srv);

    name_width = 0;
    { size_t i, r, nlen;
	for (r = 0; r < 256; r++)
	    for (i = 0; i < Conf.Sections.Root[r].nvars; i++) {
		nlen = dps_strlen(Conf.Sections.Root[r].Var[i].name);
		if (nlen > (size_t)name_width) name_width = (int)nlen;
	    }
    }

    /* Check that URL has valid syntax */
    if (DpsURLParse(&Doc->CurURL, url)) {
	DpsLog(&Main, DPS_LOG_WARN, "Invalid URL: %s", url);
	Doc->method = DPS_METHOD_DISALLOW;
    } else {
	int	start, state, result;
	int	mp3type = DPS_MP3_UNKNOWN;

	Doc->Buf.max_size = (size_t)DpsVarListFindInt(&Indexer->Vars, "MaxDocSize", DPS_MAXDOCSIZE);
	DpsVarList2Doc(Doc, &Srv);
	DpsVarListReplaceLst(&Doc->Sections, &Conf.Sections, NULL, "*");
	DpsVarListReplaceStr(&Doc->Sections, "URL", url);
	DpsDocAddConfExtraHeaders(Main.Conf, Doc);
	DpsDocAddDocExtraHeaders(&Main, &Srv, Doc);
	if (charset_from != NULL) {
	    DpsVarListReplaceStr(&Doc->Sections, "RemoteCharset", charset_from);
	}

	/* Check filters */
	result = DpsDocCheck(&Main, &Srv, Doc);

	DpsLog(&Main, DPS_LOG_EXTRA, "Getting %s", url);
	start = (Doc->method==DPS_METHOD_CHECKMP3 || Doc->method==DPS_METHOD_CHECKMP3ONLY) ? 1 : 0;
		
	for (state = start; state >= 0; state--) {
	    const char *hdr = NULL;
			
	    if (state == 1) hdr = "bytes=0-2048";
	    if (mp3type == DPS_MP3_TAG) hdr = "bytes=-128";
			
	    DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_UNKNOWN);
			
	    DpsVarListLog(&Main, &Doc->RequestHeaders, DPS_LOG_DEBUG, "Request");
				
	    if(hdr) {
		DpsVarListAddStr(&Doc->RequestHeaders, "Range", hdr);
		DpsLog(&Main, DPS_LOG_INFO, "Range: [%s]", hdr);
	    }
				
	    result = DpsGetURL(&Main, Doc, NULL);

	    if(hdr) {
		DpsVarListDel(&Doc->RequestHeaders, "Range");
	    }
				
	    if (result != DPS_OK) break;
			
	    DpsDocProcessResponseHeaders(&Main, Doc);
			
	    status = DpsVarListFindInt(&Doc->Sections, "Status", 0);
	    DpsLog(&Main, DPS_LOG_EXTRA, "Status: %d %s", status, DpsHTTPErrMsg(status));

	    if(status != DPS_HTTP_STATUS_PARTIAL_OK && status != DPS_HTTP_STATUS_OK)
		break;
			
	    if (state == 1) {	/* Needs guessing */
		if(DPS_MP3_UNKNOWN != (mp3type = DpsMP3Type(Doc))) {
		    DpsVarListReplaceStr(&Doc->Sections, "Content-Type", "audio/mpeg");
		    if(Doc->method == DPS_METHOD_CHECKMP3ONLY && mp3type != DPS_MP3_TAG) break;
		}
		if(Doc->method == DPS_METHOD_CHECKMP3ONLY) break;
	    }
	}

	if((!Doc->Buf.content) && (status < 500) && (!strncasecmp(DPS_NULL2EMPTY(Doc->CurURL.schema), "htdb:", 5))) {
	    DpsLog(&Main, DPS_LOG_WARN, "No data received");
	    status = DPS_HTTP_STATUS_SERVICE_UNAVAILABLE;
	    DpsVarListReplaceInt(&Doc->Sections, "Status", status);
	}

	if (Doc->Buf.content != NULL ) {
	    size_t		min_size;
	    size_t	hdr_len = (size_t)(Doc->Buf.content - Doc->Buf.buf);
	    size_t	cont_len = Doc->Buf.size - hdr_len;
	    int skip_too_small;
		   	
	    min_size = (size_t)DpsVarListFindUnsigned(&Main.Vars, "MinDocSize", 0);
	    skip_too_small = (cont_len < min_size);

	    if (skip_too_small) {
		Doc->method = DPS_METHOD_HEAD;
		DpsLog(&Main, DPS_LOG_EXTRA, "Too small content size (%d < %d), CheckOnly.", cont_len, min_size); 
	    }

	    DpsLog(&Main, DPS_LOG_EXTRA, "Parsing", url);
			
	    result = DpsDocParseContent(&Main, Doc);
	}
	if(result == DPS_OK){
	
	    /* Guesser was here */			
			
	    DpsParseURLText(&Main, Doc);
	    {
		char reason[PATH_MAX+1];
		int m, pas;
		DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
		m = DpsSectionFilterFind(DPS_LOG_DEBUG, &Main.Conf->SectionFilters,Doc,reason);
		DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		for (pas = 2; pas > 0; pas--) {
		    if (m != DPS_METHOD_NOINDEX && m != DPS_METHOD_DISALLOW) {
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

			if (pas == 2 && Doc->method != DPS_METHOD_HREFONLY) {
			    DpsPrintSections(Indexer, Doc);
			    m = DpsSectionFilterFind(DPS_LOG_DEBUG,&Indexer->Conf->SectionFilters,Doc,reason);
			} else pas--;
		    } else {
			Doc->method = m;
			pas--;
		    }
		}
		DpsLog(Indexer, DPS_LOG_DEBUG, "%s", reason);
	    }
	}
	DpsVarListLog(Indexer, &Doc->Sections, DPS_LOG_DEBUG, "Response");
    }


    fflush(NULL);
    DpsServerFree(&Srv);
    DpsDocFree(Doc);
    DpsAgentFree(&Main);
    DpsEnvFree(&Conf);
    DpsDestroyMutexes();
    fclose(stdout);

    DpsDeInit();

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
