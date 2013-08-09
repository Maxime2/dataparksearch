/* Copyright (C) 2003-2010 Datapark corp. All rights reserved.
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
#include "dps_word.h"
#include "dps_doc.h"
#include "dps_utils.h"
#include "dps_result.h"
#include "dps_parsehtml.h"
#include "dps_boolean.h"
#include "dps_xmalloc.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


DPS_RESULT *DpsResultInit(DPS_RESULT *Res){
	if(!Res){
		Res=(DPS_RESULT*)DpsMalloc(sizeof(DPS_RESULT));
		if (Res == NULL) return NULL;
		bzero((void*)Res, sizeof(DPS_RESULT));
		Res->freeme=1;
	}else{
		bzero((void*)Res, sizeof(DPS_RESULT));
	}
	Res->items = (DPS_STACK_ITEM*)DpsXmalloc(DPS_MAXSTACK * sizeof(DPS_STACK_ITEM));
	if (Res->items == NULL) {
	  DpsResultFree(Res);
	  return NULL;
	}
	Res->mitems = DPS_MAXSTACK;
	return Res;
}

void __DPSCALL DpsResultFree(DPS_RESULT *Res) {
	size_t i;
	
	if(!Res)return;
	for (i = 0; i < /*DPS_MAXSTACK*/ Res->mitems; i++) {
	  DPS_FREE(Res->items[i].word);
	  DPS_FREE(Res->items[i].uword);
	  DPS_FREE(Res->items[i].db_pbegin);
	  DPS_FREE(Res->items[i].pbegin);
	}
	DPS_FREE(Res->items);
	DPS_FREE(Res->PerSite);
	DPS_FREE(Res->CoordList.Coords);
	DPS_FREE(Res->CoordList.Data);
#ifdef WITH_REL_TRACK
	DPS_FREE(Res->CoordList.Track);
#endif
	DPS_FREE(Res->Suggest);
	DpsWideWordListFree(&Res->WWList);
	if(Res->Doc){
		for(i=0;i<Res->num_rows;i++){
			DpsDocFree(&Res->Doc[i]);
		}
		DPS_FREE(Res->Doc);
	}
	if(Res->freeme){
		DPS_FREE(Res);
	}else{
		bzero((void*)Res, sizeof(*Res));
	}
	return;
}

/*
int DpsResultFromTextBuf(DPS_RESULT *R,char *buf){
	size_t	num_rows=0;
	char	*tok,*lt;
	
	for(tok = dps_strtok_r(buf,"\r\n",&lt); tok; tok = dps_strtok_r(NULL,"\r\n",&lt)) {
		if(!memcmp(tok,"<DOC",4)){
			DPS_DOCUMENT	D;
			DpsDocInit(&D);
			DpsDocFromTextBuf(&D,tok);
			R->Doc=(DPS_DOCUMENT*)DpsRealloc(R->Doc,sizeof(DPS_DOCUMENT)*(R->num_rows+1));
			R->Doc[R->num_rows]=D;
			R->num_rows++;
		}else
		if(!memcmp(tok,"<WRD",4)){
			size_t		i;
			DPS_HTMLTOK	tag;
			const char	*htok,*last;
			DPS_WIDEWORD	*W;
			
			R->WWList.Word=(DPS_WIDEWORD*)DpsRealloc(R->WWList.Word,sizeof(R->WWList.Word[0])*(R->WWList.nwords+1));
			W=&R->WWList.Word[R->WWList.nwords];
			bzero((void*)W, sizeof(*W));
			
			DpsHTMLTOKInit(&tag);
			htok=DpsHTMLToken(tok,&last,&tag);
			
			for(i=0;i<tag.ntoks;i++){
				size_t  nlen=tag.toks[i].nlen;
				size_t  vlen=tag.toks[i].vlen;
				char	*name = DpsStrndup(tag.toks[i].name,nlen);
				char	*data = DpsStrndup(tag.toks[i].val,vlen);
				if(!strcmp(name,"word")){
					W->word = (char*)DpsStrdup(data);
				}else
				if(!strcmp(name,"order")){
					W->order=atoi(data);
				}else
				if(!strcmp(name,"count")){
					W->count=atoi(data);
				}else
				if(!strcmp(name,"origin")){
					W->origin=atoi(data);
				}
				DPS_FREE(name);
				DPS_FREE(data);
			}
			R->WWList.nwords++;
		}else{
			size_t		i;
			DPS_HTMLTOK	tag;
			const char	*htok,*last;
			
			DpsHTMLTOKInit(&tag);
			htok=DpsHTMLToken(tok,&last,&tag);
			
			for(i=0;i<tag.ntoks;i++){
				size_t  nlen=tag.toks[i].nlen;
				size_t  vlen=tag.toks[i].vlen;
				char	*name = DpsStrndup(tag.toks[i].name,nlen);
				char	*data = DpsStrndup(tag.toks[i].val,vlen);
				if(!strcmp(name,"first")){
					R->first=atoi(data);
				}else
				if(!strcmp(name,"last")){
					R->last=atoi(data);
				}else
				if(!strcmp(name,"count")){
					R->total_found=atoi(data);
				}else
				if(!strcmp(name,"rows")){
					num_rows=atoi(data);
				}
				DPS_FREE(name);
				DPS_FREE(data);
			}
		}
	}
	return DPS_OK;
}


int DpsResultToTextBuf(DPS_RESULT *R,char *buf,size_t len){
	char	*end=buf;
	size_t	i;
	
	end+=sprintf(end,"<RES\ttotal=\"%d\"\trows=\"%d\"\tfirst=\"%d\"\tlast=\"%d\">\n", R->total_found, R->num_rows, R->first, R->last);
	
	for (i = 0; i< R->WWList.nwords; i++) {
		DPS_WIDEWORD	*W=&R->WWList.Word[i];
		end+=sprintf(end,"<WRD\tword=\"%s\"\torder=\"%d\"\tcount=\"%d\"\torigin=\"%d\">\n",
			W->word,W->order,W->count,W->origin);
	}
	
	for(i=0;i<R->num_rows;i++){
		DPS_DOCUMENT	*D=&R->Doc[i];
		size_t		nsec, r;
		
		for (r = 0; r < 256; r++)
		for(nsec=0; nsec < D->Sections.Root[r].nvars; nsec++)
			D->Sections.Root[r].Var[nsec].section = 1;
		
		DpsDocToTextBuf(D,end,len-1);
		end+=dps_strlen(end);
		*end=NL_CHAR;
		end++;
	}
	return DPS_OK;
}
*/
