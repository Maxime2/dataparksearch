/* Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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
#include "dps_wild.h"
#include "dps_match.h"
#include "dps_db.h"
#include "dps_vars.h"
#include "dps_charsetutils.h"
#include "dps_spell.h"
#include "dps_unicode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <ctype.h>
#ifdef HAVE_INET_NET_PTON_PROTO
# if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
# endif
# if HAVE_NETINET_IN_H
# include <netinet/in.h>
# endif
# if HAVE_ARPA_INET_H
#include <arpa/inet.h>
# endif
#endif

#define ERRSTRSIZ 1024
/*
#define DEBUG_MATCH 1
*/

int DpsMatchComp(DPS_MATCH *Match,char *errstr,size_t errstrsize){
	
	int flag=REG_EXTENDED;
	int err;
	
	errstr[0]='\0';
	
	switch(Match->match_type){
		case DPS_MATCH_REGEX:
		        if (Match->compiled) regfree((regex_t*)Match->reg);
			Match->reg = (regex_t*)DpsRealloc(Match->reg, sizeof(regex_t));
			if (Match->reg == NULL) {
			  dps_snprintf(errstr, errstrsize, "Can't alloc for regex at %s:%d\n", __FILE__, __LINE__);
				fprintf(stderr, " !!! - regexcomp: %s\n", errstr);
			  return DPS_ERROR;
			}
			bzero((void*)Match->reg, sizeof(regex_t));
			if(Match->case_sense)
				flag|=REG_ICASE;
			if((err=regcomp(Match->reg, Match->pattern, flag))){
				regerror(err, Match->reg, errstr, errstrsize);
				fprintf(stderr, "DpsMatchComp of %s !!! - regcomp[%d]: %s\n", DPS_NULL2EMPTY(Match->pattern), err, errstr);
				DPS_FREE(Match->reg);
				return DPS_ERROR;
			}
			Match->compiled = 1;
			
		case DPS_MATCH_WILD:
		case DPS_MATCH_BEGIN:
		case DPS_MATCH_END:
		case DPS_MATCH_SUBSTR:
		case DPS_MATCH_FULL:
			break;
		default:
			dps_snprintf(errstr,errstrsize,"Unknown match type '%d'",Match->match_type);
			return DPS_ERROR;
	}
	return DPS_OK;
}

DPS_MATCH *DpsMatchInit(DPS_MATCH *M){
	bzero((void*)M,sizeof(*M));
	return M;
}

void DpsMatchFree(DPS_MATCH * Match){
	DPS_FREE(Match->pattern);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
	DPS_FREE(Match->idn_pattern);
#endif
	DPS_FREE(Match->arg);
	DPS_FREE(Match->section);
	DPS_FREE(Match->subsection);
	DPS_FREE(Match->dbaddr);
	if(Match->reg){
		regfree((regex_t*)Match->reg);
		DPS_FREE(Match->reg);
	}
	Match->compiled = 0;
}

#define DPS_NSUBS 10


int DpsMatchExec(DPS_MATCH * Match, const char * string, const char *net_string, struct sockaddr_in *sin, 
		 size_t nparts, DPS_MATCH_PART * Parts) {
	size_t		i;
	int		res=0;
	regmatch_t	subs[DPS_NSUBS];
	char            regerrstr[ERRSTRSIZ]="";
	const char	*se;
	size_t		plen,slen;

#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif
	/*
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
	fprintf(stderr, " -- DpsMatchExec: '%s' -> '%s'(%d)['%s'(%d)] '%s'\n", string, DPS_NULL2EMPTY(Match->pattern), Match->pat_len,
		DPS_NULL2EMPTY(Match->idn_pattern), Match->idn_len, DpsMatchTypeStr(Match->match_type));
#else
	fprintf(stderr, " -- DpsMatchExec: '%s' -> '%s'(%d) '%s'\n", string, DPS_NULL2EMPTY(Match->pattern), Match->pat_len,
		DpsMatchTypeStr(Match->match_type));
#endif
	*/
	switch(Match->match_type){
		case DPS_MATCH_REGEX:
		        if (!Match->compiled) if (DPS_OK != (res = DpsMatchComp(Match, regerrstr, sizeof(regerrstr) - 1))) {
/*			  fprintf(stderr, "reg.errstr: %s\n", regerrstr);*/
#ifdef WITH_PARANOIA
			  DpsViolationExit(-1, paran);
#endif
			  return res;
			}
			if(nparts>DPS_NSUBS)nparts=DPS_NSUBS;
			res=regexec((regex_t*)Match->reg,string,nparts,subs,0);
/*			fprintf(stderr, "regex res: %d\n--**-- %s", res, string);*/
			if(res){
/*			  char errstr[ERRSTRSIZ];
				regerror(res, Match->reg, errstr, ERRSTRSIZ);
				fprintf(stderr, " !!! - regexcomp: %s\n", errstr);
*/
				for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			}else{
				for(i=0;i<nparts;i++){
					Parts[i].beg=subs[i].rm_so;
					Parts[i].end=subs[i].rm_eo;
				}
			}
			break;
		case DPS_MATCH_WILD:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			if(Match->case_sense){
				res=DpsWildCaseCmp(string,Match->pattern);
			}else{
				res=DpsWildCmp(string,Match->pattern);
			}
			if (res == -1) res = 1;
			break;
		case DPS_MATCH_SUBNET:
			for(i = 0; i < nparts; i++) Parts[i].beg = Parts[i].end = -1;
			{
			  dps_uint4 net, mask, addr;
			  struct sockaddr_in iNET;
			  int bits;
			  if ((sin != NULL) 
			      && ((bits = inet_net_pton(AF_INET, DPS_NULL2EMPTY(Match->pattern), &iNET.sin_addr.s_addr,sizeof(iNET.sin_addr.s_addr)))!= -1)) {
			    net = (dps_uint4)ntohl(iNET.sin_addr.s_addr);
			    mask = (dps_uint4)(0xffffffffU << (32 - bits));
			    addr = (dps_uint4)ntohl(sin->sin_addr.s_addr);
			    res = !((addr & mask) == net);
#ifdef DEBUG_MATCH
			    fprintf(stderr, "Subnet.pton: addr:%x @ net/mask:%x/%x [%s] => %d\n", addr, net, mask, DPS_NULL2EMPTY(Match->pattern), res);
#endif
			  } else {
/*			if(Match->case_sense){
				res = DpsWildCaseCmp(net_string, Match->pattern);
			}else{*/
				res = DpsWildCmp(net_string, Match->pattern);
/*			}*/
#ifdef DEBUG_MATCH
				fprintf(stderr, "Subnet.WildCmp: %s @ %s => %d\n", net_string, DPS_NULL2EMPTY(Match->pattern), res);
#endif
			  }
			}
			break;
		case DPS_MATCH_BEGIN:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			slen = Match->pat_len;
			if(Match->case_sense){
			  res=strncasecmp(DPS_NULL2EMPTY(Match->pattern),string,slen);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res) {
				  slen = Match->idn_len;
				  if (slen) res = strncasecmp(DPS_NULL2EMPTY(Match->idn_pattern), string, slen);
				}
#endif
			}else{
			  res=strncmp(DPS_NULL2EMPTY(Match->pattern),string,slen);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res) {
				  slen = Match->idn_len;
				  if (slen) res = strncmp(DPS_NULL2EMPTY(Match->idn_pattern), string, slen);
				}
#endif
			}
			break;
		case DPS_MATCH_FULL:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			if(Match->case_sense){
			  res = strcasecmp(DPS_NULL2EMPTY(Match->pattern), string);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res && Match->idn_pattern && Match->idn_pattern[0]) res = strcasecmp(DPS_NULL2EMPTY(Match->idn_pattern), string);
#endif
			}else{
			  res = strcmp(DPS_NULL2EMPTY(Match->pattern), string);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res && Match->idn_pattern && Match->idn_pattern[0]) res = strcmp(DPS_NULL2EMPTY(Match->idn_pattern), string);
#endif
			}
			break;
		case DPS_MATCH_END:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			plen = Match->pat_len;
			slen = dps_strlen(string);
			if(slen<plen){
				res=1;
				break;
			}
			se=string+slen-plen;
			if(Match->case_sense){
			  res = strcasecmp(DPS_NULL2EMPTY(Match->pattern), se);
			}else{
			  res = strcmp(DPS_NULL2EMPTY(Match->pattern), se);
			}
			break;

		case DPS_MATCH_SUBSTR:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			if(Match->case_sense){
			  res = (strcasestr(string, DPS_NULL2EMPTY(Match->pattern)) == NULL);
			}else{
			  res = (strstr(string, DPS_NULL2EMPTY(Match->pattern)) == NULL);
			}
			break;
		default:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			res = 0;
	}
	if (Match->nomatch) res = !res;
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}


int DpsMatchApply(char *res, size_t size, const char *string, const char *rpl,
			DPS_MATCH *Match, size_t nparts, DPS_MATCH_PART *Parts) {
	char		*dst;
	int		len=0;
	const char	*repl=rpl;
	char            digit[2];
	int             sub;
	size_t          len1, avail1;

	if(!size)return 0;

	switch(Match->match_type){
		case DPS_MATCH_REGEX:
			dst=res;

			while((*repl)&&((size_t)(dst-res)<(size-1))){
				if(*repl=='$' && repl[1] > '0' && repl[1] <= '9') {
							
					digit[0]=repl[1];
					digit[1]='\0';
					sub=atoi(digit);

					if((Parts[sub].beg>-1)&&(Parts[sub].end>Parts[sub].beg)){
						len1 = Parts[sub].end - Parts[sub].beg;
						avail1 = size - (dst - res) - 1;
						if (len1 > avail1) len1 = avail1;
						dps_strncpy(dst, string + Parts[sub].beg, len1);
						dst+=len1;
						*dst='\0';
					}
					repl+=2;
				}else{
					*dst=*repl;
					dst++;
					*dst='\0';
					repl++;
				}
			}
			*dst='\0';
			len=dst-res;
			break;
			
		case DPS_MATCH_BEGIN:
		  len = dps_snprintf(res, size - 1, "%s%s", rpl, string + Match->pat_len);
			break;
		case DPS_MATCH_SUBSTR:
			len = dps_snprintf(res, size - 1, "%s", rpl);
			break;
	        case DPS_MATCH_FULL:
		        len = dps_snprintf(res, size-1, "%s", rpl);
			break;
		default:
			*res='\0';
			len=0;
			break;
	}
	return len;
}



int DpsMatchListAdd(DPS_AGENT *A, DPS_MATCHLIST *L, DPS_MATCH *M, char *err, size_t errsize, int ordre) {
	DPS_MATCH	*N;
	DPS_SERVER n;
	int rc;
	size_t i;

	for (i = 0; i < L->nmatches; i++) {
	  if ((strcmp(L->Match[i].pattern, DPS_NULL2EMPTY(M->pattern)) == 0) && 
	      (strcmp(DPS_NULL2EMPTY(L->Match[i].subsection), DPS_NULL2EMPTY(M->subsection)) == 0) && 
	      (strcmp(DPS_NULL2EMPTY(L->Match[i].arg), DPS_NULL2EMPTY(M->arg)) == 0) &&
	      (L->Match[i].match_type == M->match_type) &&
	      (L->Match[i].case_sense == M->case_sense) &&
	      (L->Match[i].nomatch == M->nomatch)) {
	    return DPS_OK;
	  }
	}
	
	L->Match=(DPS_MATCH *)DpsRealloc(L->Match,(L->nmatches+1)*sizeof(DPS_MATCH));
	if (L->Match == NULL) {
	  L->nmatches = 0;
	  dps_snprintf(err, errsize, "Can't realloc at %s:%d\n", __FILE__, __LINE__);
	  return DPS_ERROR;
	}
	N=&L->Match[L->nmatches++];
	DpsMatchInit(N);
	N->pattern = (char*)DpsStrdup(DPS_NULL2EMPTY(M->pattern));
	N->pat_len = dps_strlen(N->pattern);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
	N->idn_pattern = (char*)DpsStrdup(DPS_NULL2EMPTY(M->idn_pattern));
	N->idn_len = dps_strlen(N->idn_pattern);
#endif
	N->match_type=M->match_type;
	N->case_sense=M->case_sense;
	N->nomatch=M->nomatch;
	N->arg = M->arg ? (char*)DpsStrdup(DPS_NULL2EMPTY(M->arg)) : NULL;
	N->section = M->section ? (char *)DpsStrdup(M->section) : NULL;
	N->subsection = M->subsection ? (char *)DpsStrdup(M->subsection) : NULL;
	N->dbaddr = M->dbaddr ? (char *)DpsStrdup(M->dbaddr) : NULL;
	N->last = M->last;
	N->loose = M->loose;

	if (A != NULL) {

	  bzero((void*)&n, sizeof(n));
	  n.command = 'F';
	  n.Match.pattern = M->pattern;
	  n.Match.match_type = M->match_type;
	  n.Match.case_sense = M->case_sense;
	  n.Match.nomatch = M->nomatch;
	  n.Match.arg = N->arg;
	  n.Match.section = N->section;
	  n.Match.subsection = N->subsection;
	  n.Match.last = N->last;
	  n.Match.loose = N->loose;
	  n.ordre = ordre;

	  if(A->flags & DPS_FLAG_ADD_SERVURL) {
	    rc = DpsSrvAction(A, &n, DPS_SRV_ACTION_ADD);
	    N->server_id = n.site_id;
	  } else {
	    rc = DPS_OK;
	    N->server_id = 0;
	  }
	  DpsVarListFree(&n.Vars);

	  if (rc != DPS_OK) return rc;
	}

	return DpsMatchComp(N, err, errsize);
}


DPS_MATCHLIST *DpsMatchListInit(DPS_MATCHLIST *L){
	bzero((void*)L,sizeof(*L));
	return L;
}

void DpsMatchListFree(DPS_MATCHLIST *L){
	size_t i;
	for(i=0;i<L->nmatches;i++){
		DpsMatchFree(&L->Match[i]);
	}
	L->nmatches=0;
	DPS_FREE(L->Match);
}

DPS_MATCH * __DPSCALL DpsMatchListFind(DPS_MATCHLIST *L, const char *str, size_t nparts, DPS_MATCH_PART *Parts) {
	size_t i;
	for(i=0;i<L->nmatches;i++){
		DPS_MATCH *M=&L->Match[i];
		if(!DpsMatchExec(M, str, str, NULL, nparts, Parts))
			return M;
	}
	return NULL;
}


DPS_MATCH * __DPSCALL DpsSectionMatchListFind(DPS_MATCHLIST *L, DPS_DOCUMENT *Doc, size_t nparts, DPS_MATCH_PART *Parts){
  size_t i, j, r;
  DPS_TEXTLIST	*tlist = &Doc->TextList;

	for(i = 0; i < L->nmatches; i++) {
		DPS_MATCH *M = &L->Match[i];
/*		const char *str = DpsVarListFindStr(&Doc->Sections, M->section, "");*/
		
		if (M->section) {
		  r = (size_t)dps_tolower(M->section[0]);
		  for(j = 0; j < Doc->Sections.Root[r].nvars; j++) {
		    DPS_VAR *Sec = &Doc->Sections.Root[r].Var[j];
		    if(/*Sec->section &&*/ Sec->val != NULL && !strcasecmp(M->section, Sec->name)) {
		      if(!DpsMatchExec(M, Sec->val, Sec->val, NULL, nparts, Parts)) return M;
		    }
		  }
		}

		for(j = 0; j < tlist->nitems; j++) {
		  DPS_TEXTITEM	*Item = &tlist->Items[j];
		  if (Item->section && !strcasecmp(DPS_NULL2EMPTY(M->section), DPS_NULL2EMPTY(Item->section_name))) {
		    if(!DpsMatchExec(M, Item->str, Item->str, NULL, nparts, Parts)) return M;
		  }
		}
	}
	return NULL;
}


const char *DpsMatchTypeStr(int m){
	switch(m){
		case DPS_MATCH_REGEX:	return	"Regex";
		case DPS_MATCH_WILD:	return	"Wild";
		case DPS_MATCH_BEGIN:	return	"Begin";
		case DPS_MATCH_END:	return	"End";
		case DPS_MATCH_SUBSTR:	return	"SubStr";
		case DPS_MATCH_FULL:	return	"Full";
		case DPS_MATCH_SUBNET:	return	"Subnet";
	}
	return "<Unknown Match Type>";
}



/* DPS_UNIMATCH */

int DpsUniMatchListAdd(DPS_AGENT *A, DPS_UNIMATCHLIST *L, DPS_UNIMATCH *M, char *err, size_t errsize, int ordre) {
	DPS_UNIMATCH	*N;
	size_t i;

	for (i = 0; i < L->nmatches; i++) {
	  if ((DpsUniStrCmp(L->Match[i].pattern, DPS_UNINULL2EMPTY(M->pattern)) == 0) &&
	      (L->Match[i].match_type == M->match_type) &&
	      (L->Match[i].case_sense == M->case_sense) &&
	      (L->Match[i].nomatch == M->nomatch)) {
	    return DPS_OK;
	  }
	}
	
	L->Match=(DPS_UNIMATCH *)DpsRealloc(L->Match,(L->nmatches+1)*sizeof(DPS_UNIMATCH));
	if (L->Match == NULL) {
	  L->nmatches = 0;
	  dps_snprintf(err, errsize, "Can't realloc at %s:%d\n", __FILE__, __LINE__);
	  return DPS_ERROR;
	}
	N=&L->Match[L->nmatches++];
	DpsUniMatchInit(N);
	N->pattern = DpsUniDup(DPS_UNINULL2EMPTY(M->pattern));
	N->match_type=M->match_type;
	N->case_sense=M->case_sense;
	N->nomatch=M->nomatch;
	N->arg = M->arg ? (char*)DpsStrdup(DPS_NULL2EMPTY(M->arg)) : NULL;
	N->section = M->section ? (char *)DpsStrdup(M->section) : NULL;
	N->subsection = M->subsection ? (char *)DpsStrdup(M->subsection) : NULL;
	N->dbaddr = M->dbaddr ? (char *)DpsStrdup(M->dbaddr) : NULL;
	N->last = M->last;

	return DpsUniMatchComp(N, err, errsize);
}

DPS_UNIMATCH *DpsUniMatchInit(DPS_UNIMATCH *M){
	bzero((void*)M, sizeof(*M));
	return M;
}

void DpsUniMatchListFree(DPS_UNIMATCHLIST *L){
	size_t i;
	for(i=0;i<L->nmatches;i++){
		DpsUniMatchFree(&L->Match[i]);
	}
	L->nmatches = 0;
	DPS_FREE(L->Match);
}

void DpsUniMatchFree(DPS_UNIMATCH * Match) {
	DPS_FREE(Match->pattern);
	DPS_FREE(Match->arg);
	DPS_FREE(Match->section);
	DPS_FREE(Match->subsection);
	DPS_FREE(Match->dbaddr);
	DpsUniRegFree(&Match->UniReg);
	Match->compiled = 0;
}

DPS_UNIMATCH * DpsUniMatchListFind(DPS_UNIMATCHLIST *L, const dpsunicode_t *str, size_t nparts, DPS_MATCH_PART *Parts) {
	size_t i;
	for(i = 0; i < L->nmatches; i++) {
		DPS_UNIMATCH *M = &L->Match[i];
		if(!DpsUniMatchExec(M, str, str, NULL, nparts, Parts))
			return M;
	}
	return NULL;
}

int DpsUniMatchComp(DPS_UNIMATCH *Match, char *errstr, size_t errstrsize) {
	
	errstr[0]='\0';
	
	switch(Match->match_type){
		case DPS_MATCH_REGEX:
		        if (Match->compiled) DpsUniRegFree(&Match->UniReg);
			bzero((void*)&Match->UniReg, sizeof(Match->UniReg));
			if (DPS_OK != DpsUniRegComp(&Match->UniReg, Match->pattern)) {
				DpsUniPrint("DpsUniMatchComp error for ", Match->pattern);
				return DPS_ERROR;
			}
			Match->compiled = 1;
			
		case DPS_MATCH_WILD:
		case DPS_MATCH_BEGIN:
		case DPS_MATCH_END:
		case DPS_MATCH_SUBSTR:
		case DPS_MATCH_FULL:
			break;
		default:
			dps_snprintf(errstr, errstrsize, "Unknown match type '%d'", Match->match_type);
			return DPS_ERROR;
	}
	return DPS_OK;
}


int DpsUniMatchExec(DPS_UNIMATCH *Match, const dpsunicode_t *string, const dpsunicode_t *net_string, struct sockaddr_in *sin, 
		 size_t nparts, DPS_MATCH_PART * Parts) {
	size_t		i;
	int		res=0;
	char            regerrstr[ERRSTRSIZ]="";
	const dpsunicode_t *se;
	size_t		plen,slen;

#ifdef WITH_PARANOIA
	void *paran = DpsViolationEnter(paran);
#endif
	
	switch(Match->match_type){
		case DPS_MATCH_REGEX:
		        if (!Match->compiled) if (DPS_OK != (res = DpsUniMatchComp(Match, regerrstr, sizeof(regerrstr) - 1))) {
#ifdef WITH_PARANOIA
			  DpsViolationExit(-1, paran);
#endif
			  return res;
			}
			nparts = Match->UniReg.ntokens;
			res = DpsUniRegExec(&Match->UniReg, string);

			if(res){
				for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			}else{
				for(i=0;i<nparts;i++){
					Parts[i].beg = Match->UniReg.Token[i].rm_so;
					Parts[i].end = Match->UniReg.Token[i].rm_eo;
				}
			}
			break;
		case DPS_MATCH_WILD:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			if(Match->case_sense) {
			  res = DpsUniWildCaseCmp(string, DPS_UNINULL2EMPTY(Match->pattern));
			}else{
			  res = DpsUniWildCmp(string, DPS_UNINULL2EMPTY(Match->pattern));
			}
			break;
		case DPS_MATCH_SUBNET:
#if 0  /* not implemented yet */
			for(i = 0; i < nparts; i++) Parts[i].beg = Parts[i].end = -1;
			{
			  dps_uint4 net, mask, addr;
			  struct sockaddr_in iNET;
			  int bits;
			  if ((sin != NULL) 
			      && ((bits = inet_net_pton(AF_INET, DPS_NULL2EMPTY(Match->pattern), &iNET.sin_addr.s_addr,sizeof(iNET.sin_addr.s_addr)))!= -1)) {
			    net = (dps_uint4)ntohl(iNET.sin_addr.s_addr);
			    mask = (dps_uint4)(0xffffffffU << (32 - bits));
			    addr = (dps_uint4)ntohl(sin->sin_addr.s_addr);
			    res = !((addr & mask) == net);
#ifdef DEBUG_MATCH
			    fprintf(stderr, "Subnet.pton: addr:%x @ net/mask:%x/%x [%s] => %d\n", addr, net, mask, DPS_NULL2EMPTY(Match->pattern), res);
#endif
			  } else {
/*			if(Match->case_sense){
				res = DpsWildCaseCmp(net_string, Match->pattern);
			}else{*/
			    res = DpsUniWildCmp(net_string, DPS_NULL2EMPTY(Match->pattern));
/*			}*/
#ifdef DEBUG_MATCH
			    fprintf(stderr, "Subnet.WildCmp: %s @ %s => %d\n", net_string, DPS_NULL2EMPTY(Match->pattern), res);
#endif
			  }
			}
#endif /* 0 */
			break;
		case DPS_MATCH_BEGIN:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			slen = DpsUniLen(DPS_UNINULL2EMPTY(Match->pattern));
			if(Match->case_sense){
			  res = DpsUniStrNCaseCmp(DPS_UNINULL2EMPTY(Match->pattern), string, slen);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res) {
				  slen = DpsUniLen(DPS_UNINULL2EMPTY(Match->idn_pattern));
				  if (slen) res = DpsUniStrNCaseCmp(DPS_UNINULL2EMPTY(Match->idn_pattern), string, slen);
				}
#endif
			}else{
			  res = DpsUniStrNCmp(DPS_UNINULL2EMPTY(Match->pattern), string, slen);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res) {
				  slen = DpsUniLen(DPS_UNINULL2EMPTY(Match->idn_pattern));
				  if (slen) res = DpsUniStrNCmp(DPS_UNINULL2EMPTY(Match->idn_pattern), string, slen);
				}
#endif
			}
			break;
		case DPS_MATCH_FULL:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			if(Match->case_sense){
				res = DpsUniStrCaseCmp(Match->pattern, string);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res && Match->idn_pattern && Match->idn_pattern[0]) res = DpsUniStrCaseCmp(DPS_UNINULL2EMPTY(Match->idn_pattern), string);
#endif
			}else{
			  res = DpsUniStrCmp(DPS_UNINULL2EMPTY(Match->pattern), string);
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
				if (res && Match->idn_pattern && Match->idn_pattern[0]) res = DpsUniStrCmp(DPS_UNINULL2EMPTY(Match->idn_pattern), string);
#endif
			}
			break;
		case DPS_MATCH_END:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			plen = DpsUniLen(Match->pattern);
			slen = DpsUniLen(string);
			if(slen < plen){
				res = 1;
				break;
			}
			se = string + slen - plen;
			if(Match->case_sense){
				res = DpsUniStrCaseCmp(Match->pattern, se);
			}else{
				res = DpsUniStrCmp(Match->pattern, se);
			}
			break;

		case DPS_MATCH_SUBSTR:
		default:
			for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
			res=0;
	}
	if (Match->nomatch) res = !res;
#ifdef WITH_PARANOIA
	DpsViolationExit(-1, paran);
#endif
	return res;
}
