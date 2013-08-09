/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 Datapark corp. All rights reserved.
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

#ifndef _DPS_PARSE_XML_H
#define _DPS_PARSE_XML_H


typedef struct {
  DPS_AGENT *Indexer;
  DPS_DOCUMENT *Doc;
  int body_sec;
  int body_strict;
  char *sec;
  char *secpath;
  size_t pathlen, curlen;
} XML_PARSER_DATA;


/************ MY_XML **************/
#define DPS_XML_EOF		'E'
#define DPS_XML_STRING		'S'
#define DPS_XML_IDENT		'I'
#define DPS_XML_EQ		'='
#define DPS_XML_LT		'<'
#define DPS_XML_GT		'>'
#define DPS_XML_SLASH		'/'
#define DPS_XML_COMMENT		'C'
#define DPS_XML_TEXT		'T'
#define DPS_XML_QUESTION	'?'
#define DPS_XML_EXCLAM		'!'
#define DPS_XML_LSB             '['
#define DPS_XML_RSB             ']'

#define DPS_XML_OK	0
#define DPS_XML_ERROR	1


typedef struct xml_attr_st {
  const char *beg;
  const char *end;
} DPS_XML_ATTR;

typedef struct xml_stack_st {
    char errstr[512];
    char attr[4096];
    int hops;
    char *attrend;
    const char *beg;
    const char *cur;
    const char *end;
    void *user_data;
    int (*enter)(struct xml_stack_st *st, const char *val, size_t len);
    int (*value)(struct xml_stack_st *st, const char *val, size_t len);
    int (*leave_xml)(struct xml_stack_st *st, const char *val, size_t len);
} DPS_XML_PARSER;


extern int  DpsXMLParse (DPS_AGENT*, DPS_DOCUMENT*);
extern int  DpsXMLParser (DPS_XML_PARSER *p, int level, const char *str, size_t len);
extern int  DpsXMLstartElement (DPS_XML_PARSER *parser, const char *name, size_t l);
extern int  DpsXMLendElement (DPS_XML_PARSER *parser, const char *name, size_t l);

extern void DpsXMLParserCreate (DPS_XML_PARSER *p);
extern void DpsXMLParserFree (DPS_XML_PARSER *p);
extern void DpsXMLSetValueHandler (DPS_XML_PARSER *p, int (*action)(DPS_XML_PARSER *p, const char *s, size_t l));
extern void DpsXMLSetEnterHandler (DPS_XML_PARSER *p, int (*action)(DPS_XML_PARSER *p, const char *s, size_t l));
extern void DpsXMLSetLeaveHandler (DPS_XML_PARSER *p, int (*action)(DPS_XML_PARSER *p, const char *s, size_t l));
extern void DpsXMLSetUserData (DPS_XML_PARSER *p, void *user_data);
extern const char *DpsXMLErrorString (DPS_XML_PARSER *p);
extern size_t DpsXMLErrorPos (DPS_XML_PARSER *p);
extern size_t DpsXMLErrorLineno (DPS_XML_PARSER *p);

#endif
