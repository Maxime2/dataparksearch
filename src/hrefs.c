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
#include "dps_hrefs.h"
#include "dps_utils.h"
#include "dps_xmalloc.h"
#include "dps_sgml.h"
#include "dps_log.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>

/* Max URLs in cache: 4K URLs will use about 200K of RAM         */
/* This should be a configurable parameter but we'll use 4K now  */

#define HSIZE		256	/* Length of buffer increment  TUNE */
#define RESORT_HREFS	24	/* Max length of unsorted part TUNE */

DPS_HREF *DpsHrefInit(DPS_HREF * H){
	bzero((void*)H, sizeof(*H));
	return(H);
}


/* Function to sort URLs in alphabetic order */
static int cmphrefs(const void * v1, const void * v2){
	return(strcmp(((const DPS_HREF*)v1)->url,((const DPS_HREF*)v2)->url));
}


__C_LINK int __DPSCALL DpsHrefListAdd(DPS_AGENT *A, DPS_HREFLIST * HrefList,DPS_HREF * Href) {
	int l,r,c,res;
	size_t i,len;
	char *ehref, *s, *e, *ss;
	
	/* Don't add empty or too long link */
	len = dps_strlen(Href->url);
	if (len < 1) return 0;
	ehref = (char*)DpsMalloc(len + 1);
	if(ehref == NULL) {
	  DpsLog(A, DPS_LOG_ERROR, "Can't alloc %d bytes at "__FILE__":%d", len + 1, __LINE__);
	  return 0;
	}

	dps_strcpy(ehref,Href->url);
	DpsTrim(ehref," \t\r\n");
	DpsStrRemoveChars(ehref,"\t\r\n");
/*	DpsSGMLUnescape(ehref);  why we need this ? */

	/* Normalize hostname */
	if ((s = strstr(ehref, "://")) != NULL) {
	  if ((e = strchr(s + 3, '/')) != NULL) {
	    if ((ss = strchr(s + 3, '@')) == NULL) ss = s + 3;
	    for(;ss < e; ss++) if (*ss >= 'A' && *ss <= 'Z') *ss = (char)dps_tolower((int)*ss);
	  }
	}

	/* Find current URL in sorted part of list */
	l = 0; r = (int)HrefList->shrefs - 1;
	while(l<=r){
		c=(l+r)/2;
		if(!(res=strcmp(HrefList->Href[c].url,ehref))){
			HrefList->Href[c].stored = Href->stored;
			HrefList->Href[c].referrer = Href->referrer;
			HrefList->Href[c].hops = Href->hops;
			HrefList->Href[c].method = Href->method;
			HrefList->Href[c].stored = Href->stored;
			HrefList->Href[c].checked = Href->checked;
			HrefList->Href[c].site_id = Href->site_id;
			HrefList->Href[c].server_id = Href->server_id;
			HrefList->Href[c].charset_id = Href->charset_id;
			HrefList->Href[c].weight = Href->weight;
			HrefList->Href[c].delay = Href->delay;
			DPS_FREE(ehref);
			return(0);
		}
		if(res<0)
			l=c+1;
		else
			r=c-1;
	}
	/* Find in unsorted part */
	for(i=HrefList->shrefs;i<HrefList->nhrefs;i++){
		if(!strcmp(HrefList->Href[i].url,ehref)){
			HrefList->Href[i].stored = Href->stored;
			HrefList->Href[i].referrer = Href->referrer;
			HrefList->Href[i].hops = Href->hops;
			HrefList->Href[i].method = Href->method;
			HrefList->Href[i].stored = Href->stored;
			HrefList->Href[i].checked = Href->checked;
			HrefList->Href[i].site_id = Href->site_id;
			HrefList->Href[i].server_id = Href->server_id;
			HrefList->Href[i].charset_id = Href->charset_id;
			HrefList->Href[i].weight = Href->weight;
			HrefList->Href[i].delay = Href->delay;
			DPS_FREE(ehref);
			return(0);
		}
	}
	if(HrefList->nhrefs>=HrefList->mhrefs){
		HrefList->mhrefs+=HSIZE;
		HrefList->Href=(DPS_HREF *)DpsRealloc(HrefList->Href,HrefList->mhrefs*sizeof(DPS_HREF));
		if (HrefList->Href == NULL) {
		  DpsLog(A, DPS_LOG_ERROR, "Can't realloc %d bytes at "__FILE__":%d", HrefList->mhrefs * sizeof(DPS_HREF), __LINE__);
		  HrefList->mhrefs = HrefList->nhrefs = HrefList->dhrefs = HrefList->shrefs = 0;
		  return 0;
		}
	}
	HrefList->Href[HrefList->nhrefs].url = (char*)DpsStrdup(ehref);
	HrefList->Href[HrefList->nhrefs].referrer=Href->referrer;
	HrefList->Href[HrefList->nhrefs].hops=Href->hops;
	HrefList->Href[HrefList->nhrefs].method=Href->method;
	HrefList->Href[HrefList->nhrefs].stored = Href->stored;
	HrefList->Href[HrefList->nhrefs].checked = Href->checked;
	HrefList->Href[HrefList->nhrefs].site_id = Href->site_id;
	HrefList->Href[HrefList->nhrefs].server_id = Href->server_id;
	HrefList->Href[HrefList->nhrefs].charset_id = Href->charset_id;
	HrefList->Href[HrefList->nhrefs].weight = Href->weight;
	HrefList->Href[HrefList->nhrefs].delay = Href->delay;
	HrefList->nhrefs++;

	/* Sort unsorted part */
	if((HrefList->nhrefs-HrefList->shrefs)>RESORT_HREFS){
		DpsSort(HrefList->Href,HrefList->nhrefs,sizeof(DPS_HREF),cmphrefs);
		/* Remember count of sorted URLs  */
		HrefList->shrefs=HrefList->nhrefs;
		/* Count of stored URLs became 0  */
		HrefList->dhrefs=0;
	}
	DPS_FREE(ehref);
	return(1);
}

extern __C_LINK void __DPSCALL DpsHrefListFree(DPS_HREFLIST * HrefList){
	size_t i;

	for(i=0;i<HrefList->nhrefs;i++){
		DPS_FREE(HrefList->Href[i].url);
	}
	DPS_FREE(HrefList->Href);
	bzero((void*)HrefList, sizeof(*HrefList));
}

__C_LINK DPS_HREFLIST * __DPSCALL DpsHrefListInit(DPS_HREFLIST * Hrefs){
	bzero((void*)Hrefs, sizeof(*Hrefs));
	return(Hrefs);
}
