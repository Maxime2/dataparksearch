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
#include "dps_utils.h"
#include "dps_uniconv.h"
#include "dps_vars.h"
#include "dps_textlist.h"
#include "dps_parsexml.h"
#include "dps_parsehtml.h"
#include "dps_hrefs.h"
#include "dps_sgml.h"
#include "dps_log.h"
#include "dps_charsetutils.h"
#include "dps_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

/*
#define DEBUG_XML 1
*/

static const char *DpsLex2str (int lex) {
  switch (lex) {
  case DPS_XML_EOF:      return "EOF";
  case DPS_XML_STRING:   return "STRING";
  case DPS_XML_IDENT:    return "IDENT";
  case DPS_XML_EQ:       return "'='";
  case DPS_XML_LT:       return "'<'";
  case DPS_XML_GT:       return "'>'";
  case DPS_XML_SLASH:    return "'/'";
  case DPS_XML_COMMENT:  return "COMMENT";
  case DPS_XML_TEXT:     return "TEXT";
  case DPS_XML_QUESTION: return "'?'";
  case DPS_XML_EXCLAM:   return "'!'";
  case DPS_XML_LSB:      return "'['";
  case DPS_XML_RSB:      return "']'";
  }
  return "UNKNOWN";
}

/************ /MY_XML **************/

int DpsXMLstartElement(DPS_XML_PARSER *parser, const char *name, size_t l) {
  XML_PARSER_DATA *D = parser->user_data;
  char *p;
  
  DPS_FREE(D->secpath);
  D->secpath = DpsStrndup(name, l);
  DPS_FREE(D->sec);
  p = strrchr(D->secpath, '.');
  D->sec = p ? DpsStrdup(p + 1) : DpsStrndup(name, l);
  return DPS_XML_OK;
}

int DpsXMLendElement(DPS_XML_PARSER *parser, const char *name, size_t l) {
  XML_PARSER_DATA *D = parser->user_data;
  size_t i = l;
  char *p;

  while (i && name[i] != '.') i--;

  DPS_FREE(D->secpath);
  D->secpath = DpsStrndup(name, i);
  DPS_FREE(D->sec);
  p = strrchr(D->secpath, '.');
  D->sec = p ? DpsStrdup(p + 1) : DpsStrndup(name, i);
  return(DPS_XML_OK);
}


static int Text (DPS_XML_PARSER *parser, const char *s, size_t len) {
  XML_PARSER_DATA *D = parser->user_data;
  DPS_AGENT *Indexer = D->Indexer;
  DPS_DOCUMENT *Doc = D->Doc;
  DPS_TEXTITEM  Item;
  DPS_VAR    *Sec;
  size_t slen = (D->secpath != NULL) ? dps_strlen(D->secpath) : 0;

/*  if (D->sec == NULL) return DPS_XML_OK;*/
  bzero((void*)&Item, sizeof(Item));
  Item.str = DpsStrndup(s, (size_t)len);
  Item.len = (size_t)len;
  if ((D->sec != NULL) && (!strcasecmp(D->sec, "icbm:latitude") || !strcasecmp(D->sec, "geo:lat"))
      && (Sec = DpsVarListFind(&Doc->Sections, "geo.lat"))) {
    Item.section = Sec->section;
    Item.strict = Sec->strict;
    Item.section_name = Sec->name;
    DpsVarListReplaceStr(&Doc->Sections, "geo.lat", Item.str);
    (void)DpsTextListAdd(&Doc->TextList, &Item);
  } else if ((D->sec != NULL) && (!strcasecmp(D->sec, "icbm:longitude") || !strcasecmp(D->sec, "geo:lon"))
      && (Sec = DpsVarListFind(&Doc->Sections, "geo.lon"))) {
    Item.section = Sec->section;
    Item.strict = Sec->strict;
    Item.section_name = Sec->name;
    DpsVarListReplaceStr(&Doc->Sections, "geo.lon", Item.str);
    (void)DpsTextListAdd(&Doc->TextList, &Item);
  } else if((D->sec != NULL) &&  (Sec = DpsVarListFind(&Indexer->Conf->HrefSections, D->secpath))) {
    Item.section = Sec->section;
    Item.strict = Sec->strict;
    Item.section_name = D->sec;
    (void)DpsTextListAdd(&Doc->TextList, &Item);
  } else if((D->sec != NULL) &&  (Sec = DpsVarListFind(&Doc->Sections, D->secpath))) {
    Item.section = Sec->section;
    Item.strict = Sec->strict;
    Item.section_name = D->sec;
    (void)DpsTextListAdd(&Doc->TextList, &Item);
  } else if((D->sec != NULL) &&  (Sec = DpsVarListFind(&Indexer->Conf->HrefSections, D->sec))) {
    Item.section = Sec->section;
    Item.strict = Sec->strict;
    Item.section_name = D->sec;
    (void)DpsTextListAdd(&Doc->TextList, &Item);
  } else if((D->sec != NULL) &&  (Sec = DpsVarListFind(&Doc->Sections, D->sec))) {
    DpsHTMLParseBuf(D->Indexer, D->Doc,  D->sec, Item.str);
  } else {
    DpsHTMLParseBuf(D->Indexer, D->Doc, "body", Item.str);
  }
#ifdef DEBUG_XML
  fprintf(stderr, " -- sec: %s\nsecpath: %s\nItem.sec:%s str: %s\n\n", 
	  DPS_NULL2EMPTY(D->sec), DPS_NULL2EMPTY(D->secpath), Item.section_name, Item.str);
#endif
  DpsFree(Item.str);

  if ((Doc->Spider.follow != DPS_FOLLOW_NO) && D->secpath &&
      slen >= 4 &&
      (!strncasecmp(&D->secpath[slen - 5], ".href", 5) ||
       !strncasecmp(&D->secpath[slen - 5], ".link", 5) ||
       !strncasecmp(&D->secpath[slen - 4], ".url", 4)
	  )
      ) {

    DPS_HREF	Href;

    DpsHrefInit(&Href);
    Href.url= DpsStrndup(s, (size_t)len);
/*    DpsSGMLUnescape(Href.url);  we do this later */
    Href.referrer = DpsVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
    Href.hops = 1 + DpsVarListFindInt(&Doc->Sections, "Hops", 0);
    Href.site_id = 0; /*DpsVarListFindInt(&Doc->Sections, "Site_id", 0);*/
    Href.method = DPS_METHOD_GET;
    DpsHrefListAdd(Indexer, &Doc->Hrefs, &Href);
    DpsFree(Href.url);
  }

  if (slen == 8 && !strcasecmp(DPS_NULL2EMPTY(D->sec), "encoding")) {
    char buf[64];
    if (len > 0 && len < sizeof(buf)) {
      const char *csname;
      dps_memcpy(buf, s, len);
      buf[len]= '\0';
      csname= DpsCharsetCanonicalName(buf);
      if (csname)
        DpsVarListInsStr(&Doc->Sections, "Meta-Charset", csname);
    }
  }
  if (len > 0 && slen == 8 && !strcasecmp(DPS_NULL2EMPTY(D->sec), "language")) {
    char *buf = DpsStrndup(s, len);
    DpsVarListInsStr(&Doc->Sections, "Meta-Language", buf);
    DpsFree(buf);
  }
  return DPS_XML_OK;
}

/************ MY_XML **************/

void DpsXMLParserCreate (DPS_XML_PARSER *p) {
  bzero((void*)p, sizeof(p[0]));
}

void DpsXMLParserFree (DPS_XML_PARSER *p) {
}

void DpsXMLSetValueHandler (DPS_XML_PARSER *p, int (*action)(DPS_XML_PARSER *p, const char *s, size_t l)) {
  p->value = action;
}

void DpsXMLSetEnterHandler (DPS_XML_PARSER *p, int (*action)(DPS_XML_PARSER *p, const char *s, size_t l)) {
  p->enter = action;
}

void DpsXMLSetLeaveHandler (DPS_XML_PARSER *p, int (*action)(DPS_XML_PARSER *p, const char *s, size_t l)) {
  p->leave_xml = action;
}

void DpsXMLSetUserData (DPS_XML_PARSER *p, void *user_data) {
  p->user_data = user_data;
}

const char *DpsXMLErrorString (DPS_XML_PARSER *p) {
  return(p->errstr);
}

size_t DpsXMLErrorPos (DPS_XML_PARSER *p) {
  const char *beg = p->beg;
  const char *s;
  for (s = p->beg; s < p->cur; s++) {
    if (s[0] == NL_CHAR)
      beg=s;
  }
  return(p->cur-beg);
}

size_t DpsXMLErrorLineno (DPS_XML_PARSER *p) {
  size_t res = 0;
  const char *s;
  for (s=p->beg; s<p->cur; s++) {
    if (s[0]==NL_CHAR)
      res++;
  }
  return(res);
}

static void DpsXMLNormText (DPS_XML_ATTR *a) {
  for (; (a->beg < a->end) && strchr(" \t\r\n", a->beg[0]); a->beg++);
  for (; (a->beg < a->end) && strchr(" \t\r\n", a->end[-1]); a->end--);
}

static int DpsXMLScan (DPS_XML_PARSER *p, DPS_XML_ATTR *a) {
  int lex;

  for(; (p->cur < p->end) && strchr(" \t\r\n", p->cur[0]); p->cur++);

  if (p->cur >= p->end) {
    a->beg = p->end;
    a->end = p->end;
          lex = DPS_XML_EOF;
    goto ret;
  }

  a->beg = p->cur;
  a->end = p->cur;

  if (! strncmp(p->cur, "<!--", 4)) {
    for(; (p->cur < p->end) && strncmp(p->cur, "-->", 3); p->cur++);
    if(! strncmp(p->cur, "-->", 3))
      p->cur += 3;
    a->end = p->cur;
    lex = DPS_XML_COMMENT;
  } else if (strchr("?=/<>![]", p->cur[0])) {
    p->cur++;
    a->end = p->cur;
    lex = a->beg[0];
  } else if ((p->cur[0] == '"') || (p->cur[0] == '\'')) {
    p->cur++;
    for(; ( p->cur < p->end ) && (p->cur[0] != a->beg[0]); p->cur++) {}
    a->end = p->cur;
    if (a->beg[0] == p->cur[0]) p->cur++;
    a->beg++;
    DpsXMLNormText(a);
    lex = DPS_XML_STRING;
  } else {
    for(; (p->cur < p->end) && ! strchr("?'\"=/<>[] \t\r\n", p->cur[0]); p->cur++) {}
    a->end = p->cur;
    DpsXMLNormText(a);
    lex = DPS_XML_IDENT;
  }

#if DEBUG_XML
  { char *tt = DpsStrndup(a->beg, a->end-a->beg);
    fprintf(stderr, " -- LEX=%s[%d] : %s : at line %d pos %d\n", DpsLex2str(lex), a->end-a->beg, tt,
	    DpsXMLErrorLineno(p),
	    DpsXMLErrorPos(p)
	    );
    DPS_FREE(tt);
  }
#endif

ret:
  return lex;
}

static int DpsXMLValue (DPS_XML_PARSER *st, const char *str, size_t len) {
  return((st->value) ? (st->value)(st, str, len) : DPS_XML_OK);
}

static int DpsXMLEnter(DPS_XML_PARSER *st, const char *str, size_t len) {

  if ((st->attrend - st->attr + len + 1) > sizeof(st->attr)) {
    sprintf(st->errstr, "Too deep XML");
    return DPS_XML_ERROR;
  }
  if (st->attrend > st->attr) {
    st->attrend[0] = '.';
    st->attrend++;
  }
  dps_memcpy(st->attrend, str, len);
  st->attrend += len;
  st->attrend[0] = '\0';
  return(st->enter ? st->enter(st, st->attr, st->attrend - st->attr) : DPS_XML_OK);
}

static int DpsXMLLeave(DPS_XML_PARSER *p, const char *str, size_t slen) {
  char *e;
  size_t glen;
  char s[256];
  char g[256];
  int rc;

  /* Find previous '.' or beginning */
  for(e = p->attrend; (e > p->attr) && (e[0] != '.'); e--);
  glen = (e[0] == '.') ? (p->attrend - e - 1) : p->attrend - e;
/*
  fprintf(stderr, "= attr: %s\n= e: %s\n", p->attr, e);
*/
  if (str && (slen != glen)) {
    dps_mstr(s, str, sizeof(s) - 1, slen);
    dps_mstr(g, e + 1, sizeof(g) - 1, glen),
    sprintf(p->errstr, "'</%s>' unexpected ('</%s>' wanted)", s, g);
    return(DPS_XML_ERROR);
/*    fprintf(stderr, "= s: %s\n", s);*/
  }

  rc = p->leave_xml ? p->leave_xml(p, p->attr, p->attrend - p->attr) : DPS_XML_OK;

  *e = '\0';
  p->attrend = e;
  return(rc);
}


int DpsXMLParser (DPS_XML_PARSER *p, int level, const char *str, size_t len) {
  p->attrend = p->attr;
  p->beg = str;
  p->cur = str;
  p->end = str + len;

#ifdef DEBUG_XML
  fprintf(stderr, " -- DpsXMLParser: level %d  len %d\n", level, len);
#endif

  if (level > 2) {
    sprintf(p->errstr, "0: too deep recursion on '[]'");
#ifdef DEBUG_XML
    fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
    return DPS_XML_ERROR;
  }

  while (p->cur < p->end) {
    DPS_XML_ATTR a;
    if (p->cur[0] == '<' || p->cur[0] == ']') {
      int lex;
      int question = 0;
      int exclam = 0;
      int square = 0;

      lex = DpsXMLScan(p, &a);

      if (DPS_XML_COMMENT == lex) continue;

      if (level && lex == DPS_XML_RSB) {
#ifdef DEBUG_XML
	fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
	return DPS_XML_OK;
      }

      lex = DpsXMLScan(p, &a);

      if (DPS_XML_SLASH == lex) {
        if (DPS_XML_IDENT != (lex = DpsXMLScan(p, &a))) {
          sprintf(p->errstr, "1: %s unexpected (ident wanted)", DpsLex2str(lex));
          return(DPS_XML_ERROR);
        }
        if (DPS_XML_OK != DpsXMLLeave(p, a.beg, a.end-a.beg)) {
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
          return(DPS_XML_ERROR);
	}
	lex = DpsXMLScan(p, &a);
        goto gt;
      }

      if (DPS_XML_EXCLAM == lex) {
        lex = DpsXMLScan(p, &a);
        exclam = 1;
      } else if (DPS_XML_QUESTION == lex) {
        lex = DpsXMLScan(p, &a);
        question = 1;
      } 

      if (DPS_XML_LSB == lex) {
	DPS_XML_PARSER parser;
	XML_PARSER_DATA Data, *ud = p->user_data;
	int rc;

/*  fprintf(stderr, "p->cur: %s\n\n", p->cur);*/
/*	lex = DpsXMLScan(p, &a);*/
        DpsXMLParserCreate(&parser);
	bzero(&Data, sizeof(Data));
	Data.Indexer = ud->Indexer;
	Data.Doc = ud->Doc;
	Data.body_sec = ud->body_sec;
	Data.body_strict = ud->body_strict;
	Data.sec = DpsStrdup(ud->sec);
	Data.secpath = DpsStrdup(ud->secpath);

	DpsXMLSetUserData(&parser, &Data);
	DpsXMLSetEnterHandler(&parser, DpsXMLstartElement);
	DpsXMLSetLeaveHandler(&parser, DpsXMLendElement);
	DpsXMLSetValueHandler(&parser, Text);
	rc = DpsXMLParser(&parser, level + 1, p->cur, (p->end - p->cur));
	p->cur = parser.cur;
	DpsXMLParserFree(&parser);
	DPS_FREE(Data.sec);
	DPS_FREE(Data.secpath);

        if (DPS_XML_OK != rc) {
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
	  dps_strcpy(p->errstr, parser.errstr);
	  return DPS_XML_ERROR;
	}
#if DEBUG_XML
	fprintf(stderr, " --  LEX=%s\n", DpsLex2str(lex));
#endif
/*
	lex = DpsXMLScan(p, &a);
	if (level && lex == DPS_XML_RSB) {
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser[%d]: leave level %d\n", __LINE__, level);
#endif	
	  return DPS_XML_OK;
	}
*/
/*	goto gt;*/
/*	square = 1;*/
      }

      if (DPS_XML_IDENT == lex) {
        if (DPS_XML_OK != DpsXMLEnter(p, a.beg, a.end-a.beg)) {
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
          return(DPS_XML_ERROR);
	}
      } else if (DPS_XML_LSB) {
        if (DPS_XML_OK != DpsXMLEnter(p, "|", 1)) {
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
          return(DPS_XML_ERROR);
	}
      } else {
        sprintf(p->errstr, "3: %s unexpected (ident or '/' wanted)", DpsLex2str(lex));
#ifdef DEBUG_XML
	fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
        return(DPS_XML_ERROR);
      }

      while ((DPS_XML_IDENT == (lex = DpsXMLScan(p, &a))) ||
	     (DPS_XML_STRING == lex) || (DPS_XML_LSB == lex)) {
        DPS_XML_ATTR b;
	int rc;
	if (DPS_XML_LSB == lex) {
	  DPS_XML_PARSER parser;
	  XML_PARSER_DATA Data, *ud = p->user_data;

	  lex = DpsXMLScan(p, &a);
	  DpsXMLParserCreate(&parser);
	  bzero(&Data, sizeof(Data));
	  Data.Indexer = ud->Indexer;
	  Data.Doc = ud->Doc;
	  Data.body_sec = ud->body_sec;
	  Data.sec = DpsStrdup(ud->sec);
	  Data.secpath = DpsStrdup(ud->secpath);

	  DpsXMLSetUserData(&parser, &Data);
	  DpsXMLSetEnterHandler(&parser, DpsXMLstartElement);
	  DpsXMLSetLeaveHandler(&parser, DpsXMLendElement);
	  DpsXMLSetValueHandler(&parser, Text);
	  rc = DpsXMLParser(&parser, level + 1, p->cur, (p->end - p->cur));
	  p->cur = parser.cur;
	  DpsXMLParserFree(&parser);
	  DPS_FREE(Data.sec);
	  DPS_FREE(Data.secpath);

          if (DPS_XML_OK != rc) {
#ifdef DEBUG_XML
	    fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
	    dps_strcpy(p->errstr, parser.errstr);
	    return DPS_XML_ERROR;
	  }
	}
        if (DPS_XML_EQ == (lex = DpsXMLScan(p, &b))) {
          lex = DpsXMLScan(p, &b);
          if ((lex == DPS_XML_IDENT) || (lex == DPS_XML_STRING)) {
            if ((DPS_XML_OK != DpsXMLEnter(p, a.beg, a.end-a.beg)) ||
                (DPS_XML_OK != DpsXMLValue(p, b.beg, b.end - b.beg)) ||
                (DPS_XML_OK != DpsXMLLeave(p, a.beg, a.end - a.beg))) {
#ifdef DEBUG_XML
	      fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
              return(DPS_XML_ERROR);
	    }
          } else {
            sprintf(p->errstr, "4: %s unexpected (ident or string wanted)", DpsLex2str(lex));
            return(DPS_XML_ERROR);
          }
        } else if ((DPS_XML_STRING == lex) || (DPS_XML_IDENT == lex)) {
          if (DPS_XML_IDENT == lex) {
            if ((DPS_XML_OK != DpsXMLEnter(p, a.beg, a.end - a.beg)) ||
                (DPS_XML_OK != DpsXMLLeave(p, a.beg, a.end - a.beg))) {
#ifdef DEBUG_XML
	      fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
              return(DPS_XML_ERROR);
	    }
          } else {
#if DEBUG_XML
	    fprintf(stderr, " -- .LEX=%s\n", DpsLex2str(lex));
#endif
	    /* Do nothing */
          }
        }
        else {
	  break;
	}
      }

#if DEBUG_XML
      fprintf(stderr, " -- :LEX=%s level:%d\n", DpsLex2str(lex), level);
#endif
      if (lex == DPS_XML_SLASH) {
        if (DPS_XML_OK != DpsXMLLeave(p, NULL, 0)) {
#ifdef DEBUG_XML
	  fprintf(stderr, "DpsXMLParser: leave level %d\n", level);
#endif	
          return(DPS_XML_ERROR);
	}
        lex = DpsXMLScan(p, &a);
      }
gt:
      if (level && lex == DPS_XML_RSB) {
#ifdef DEBUG_XML
	fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
	return DPS_XML_OK;
      }

      if (square) {
#ifdef DEBUG_XML
	fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
	if (lex != DPS_XML_RSB) {
          sprintf(p->errstr, "7: %s unexpected (']' wanted)", DpsLex2str(lex));
          return(DPS_XML_ERROR);
	}
	return DPS_XML_OK;
/*
        if (DPS_XML_OK != DpsXMLLeave(p, NULL, 0)) {
          return(DPS_XML_ERROR);
	}
        lex = DpsXMLScan(p, &a);
*/
      }

      if (question) {
        if (lex != DPS_XML_QUESTION) {
          sprintf(p->errstr, "6: %s unexpected ('?' wanted)", DpsLex2str(lex));
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
          return(DPS_XML_ERROR);
        }
        if (DPS_XML_OK != DpsXMLLeave(p, NULL, 0)) {
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
          return(DPS_XML_ERROR);
	}
        lex = DpsXMLScan(p, &a);
      }

      if (exclam) {
        if (DPS_XML_OK != DpsXMLLeave(p, NULL, 0)) {
#ifdef DEBUG_XML
	  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
          return(DPS_XML_ERROR);
	}
      }

      if (lex != DPS_XML_GT) {
        sprintf(p->errstr, "5: %s unexpected ('>' wanted)", DpsLex2str(lex));
#ifdef DEBUG_XML
	fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
        return(DPS_XML_ERROR);
      }
    } else {
      a.beg = p->cur;
      if (!strncasecmp(p->cur, "CDATA[", 6)) {
	char *t = strstr(a.beg = p->cur + 6, "]]>");
	if (t == NULL) return DPS_XML_ERROR;
	a.end = t;
	p->cur = t + 1;
      } else {
	if (level)
	  for (; (p->cur < p->end) && (p->cur[0] != '<') && (p->cur[0] != ']') /*&& (p->cur[0] != '[')*/; p->cur++);
	else 
	  for (; (p->cur < p->end) && (p->cur[0] != '<') /*&& (p->cur[0] != ']')*/ /*&& (p->cur[0] != '[')*/; p->cur++);
	a.end = p->cur;
      }

      DpsXMLNormText(&a);
      if (a.beg!=a.end) {
/*	XML_PARSER_DATA *ud = p->user_data;
	char *html_content = DpsStrndup(a.beg, a.end - a.beg);
	DpsSGMLUnescape(html_content);
	DpsHTMLParseBuf(ud->Indexer, ud->Doc,  ud->sec, html_content);
	DPS_FREE(html_content);*/
        int rc = DpsXMLValue(p, a.beg, a.end - a.beg);
      }
    }
  }
#ifdef DEBUG_XML
  fprintf(stderr, " -- DpsXMLParser: leave level %d\n", level);
#endif	
  return(DPS_XML_OK);
}



/************ /MY_XML **************/


#if 0
static void Decl(void *userData, const XML_Char *version, const XML_Char *encoding, int standalone) {
  XML_PARSER_DATA *D = userData;
  DPS_DOCUMENT *Doc = D->Doc;

  if (encoding != NULL)
    DpsVarListReplaceStr(&Doc->Sections,
                         "Meta-Charset", DpsCharsetCanonicalName(encoding));

}

static int EncHandler(void *encodingHandlerData, const XML_Char *name, XML_Encoding *info) {
  DPS_AGENT *indexer = encodingHandlerData;
  DPS_CHARSET  *cs;
  size_t i;

  if (!(cs = DpsGetCharSet(name))) {
    return 0;
  }
  
  /* FIXME: rewrite this for multibytes encodings */
  if (cs->tab_to_uni == NULL) return 0; 

  info->convert = NULL;
  info->release = NULL;
  info->data = NULL;
  for(i = 0; i < 256; i++)
    info->map[i] = cs->tab_to_uni[i];

  return 1;
}
#endif

int DpsXMLParse(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
  XML_PARSER_DATA Data;
  DPS_XML_PARSER parser;
  DPS_VAR *BSec = DpsVarListFind(&Doc->Sections, "body");
  const char *buf_content = (Doc->Buf.pattern == NULL) ? Doc->Buf.content : Doc->Buf.pattern;
  int body_sec  = BSec ? BSec->section : 0;
  int body_strict = BSec ? BSec->strict : 0;
  int res = DPS_OK;

  DpsLog(Indexer, DPS_LOG_DEBUG, "Executing XML parser");

  DpsXMLParserCreate(&parser);
  bzero(&Data, sizeof(Data));
  Data.Indexer = Indexer;
  Data.Doc = Doc;
  Data.body_sec = body_sec;
  Data.body_strict = body_strict;

  DpsXMLSetUserData(&parser, &Data);
  DpsXMLSetEnterHandler(&parser, DpsXMLstartElement);
  DpsXMLSetLeaveHandler(&parser, DpsXMLendElement);
  DpsXMLSetValueHandler(&parser, Text);

  if (DpsXMLParser(&parser, 0, buf_content, (int)dps_strlen(buf_content)) == DPS_XML_ERROR) {
    char err[256];    
    dps_snprintf(err, sizeof(err), 
                 "XML parsing error: %s at line %d pos %d\n",
                  DpsXMLErrorString(&parser),
                  DpsXMLErrorLineno(&parser),
                  DpsXMLErrorPos(&parser));
    DpsVarListReplaceStr(&Doc->Sections, "X-Reason", err);
    DpsLog(Indexer, DPS_LOG_ERROR, err);
    res = DPS_ERROR;
  }

  DpsXMLParserFree(&parser);
  DPS_FREE(Data.sec);
  DPS_FREE(Data.secpath);
  return res;
}
