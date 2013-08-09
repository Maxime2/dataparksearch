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

#include "dps_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "dps_uniconv.h"
#include "dps_unidata.h"

#define DPCONV_BUF_SIZE 4096

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

	fprintf(stderr, "\ndpconv from %s-%s-%s\n\
(C)1998-2003, LavTech Corp.\n\
(C)2003-2009, Datapark Corp.\n\
\n\
Usage: dpconv [OPTIONS] -f charset_from -t charset_to  < infile > outfile\n\
\n\
Converting options:\n\
  -v            verbose output\n\
  -e            use HTML escape entities for input\n\
  -E            use HTML escape entities for output\n\
  -C            use Unicode Normalization NFC\n\
  -D            use Unicode Normalization NFD\n\
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

int main(int argc, char **argv) {
	char *charset_from = NULL, *charset_to = NULL;
	DPS_CHARSET *CH_F, *CH_T, *sys_int;
	DPS_CONV fc_uni, uni_tc;
	int html_from = 0, html_to = 0;
	char *from_buf, *uni_buf, *to_buf;
	int ch, help = 0, verbose = 0, NFC = 0, NFD = 0;
        const char *CharsToEscape = "\"&<>";

	while ((ch = getopt(argc, argv, "CDEehv?t:f:")) != -1){
		switch (ch) {
		case 'E': html_to = 1; break;
		case 'e': html_from = 1; break;
		case 'v': verbose = 1; break;
		case 't': charset_to =  optarg; break;
		case 'f': charset_from = optarg; break;
                case 'C': NFC = 1; NFD = 0; break;
                case 'D': if (NFC == 0) NFD = 1; break;
		case '?':
		case 'h':
		default:
			help++;
		}
	}

	argc -= optind;argv += optind;

	if((argc>1) || (help) || (!charset_from) || (!charset_to)){
		usage(help);
		return(1);
	}

	if (!(CH_F = DpsGetCharSet(charset_from))) {
	  if (verbose) {
	    fprintf(stderr, "Charset: %s not found or not supported", charset_from);
	    display_charsets();
	  }
	  exit(1);
	}
	if (!(CH_T = DpsGetCharSet(charset_to))) {
	  if (verbose) {
	    fprintf(stderr, "Charset: %s not found or not supported", charset_to);
	    display_charsets();
	  }
	  exit(1);
	}
	sys_int=DpsGetCharSet("sys-int");
 
	DpsConvInit(&fc_uni, CH_F, sys_int, CharsToEscape, (html_from) ? DPS_RECODE_HTML : 0);
	DpsConvInit(&uni_tc, sys_int, CH_T, CharsToEscape, (html_to) ? DPS_RECODE_HTML : 0);
 
	if ((from_buf = (char*)DpsMalloc(DPCONV_BUF_SIZE)) == NULL) {
	  if (verbose) fprintf(stderr, "Can't alloc memory, %d bytes", DPCONV_BUF_SIZE);
	  exit(1);
	}
	if ((to_buf = (char*)DpsMalloc(16 * DPCONV_BUF_SIZE)) == NULL) {
	  if (verbose) fprintf(stderr, "Can't alloc memory, %d bytes", 16 * DPCONV_BUF_SIZE);
	  exit(1);
	}
	if ((uni_buf = (char*)DpsMalloc(4 * DPCONV_BUF_SIZE * sizeof(int))) == NULL) {
	    if (verbose) fprintf(stderr, "Can't alloc memory, %d bytes", (int)(4 * DPCONV_BUF_SIZE * sizeof(int)));
	    exit(1);
	}
 
	while((fgets(from_buf, DPCONV_BUF_SIZE, stdin)) != NULL) {
	  DpsConv(&fc_uni, uni_buf, 4 * DPCONV_BUF_SIZE * sizeof(int), from_buf, DPCONV_BUF_SIZE);
          if (NFC) uni_buf = (char*)DpsUniNormalizeNFC((dpsunicode_t*)uni_buf, (dpsunicode_t*)uni_buf);
          if (NFD) uni_buf = (char*)DpsUniNormalizeNFD((dpsunicode_t*)uni_buf, (dpsunicode_t*)uni_buf);
	  DpsConv(&uni_tc, to_buf, 16 * DPCONV_BUF_SIZE, uni_buf, 4 * DPCONV_BUF_SIZE * sizeof(int));
          if (fwrite(to_buf, uni_tc.obytes, 1, stdout) != 1) {
/*	  if (fputs(to_buf, stdout) != 0) {*/
	    if (verbose) fprintf(stderr, "An output error ocurred");
	    exit(1);
	  }
	}

	if (ferror(stdin)) {
	  if (verbose) fprintf(stderr, "An input error ocurred");
	  exit(1);
	}

	fflush(NULL);
        free(to_buf);
        free(from_buf);
        free(uni_buf);
	
	return 0;
}
