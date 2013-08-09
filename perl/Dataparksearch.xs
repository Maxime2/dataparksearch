/* Copyright (C) 2004-2009 Datapark corp. All rights reserved.
 
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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/
#include "dps_config.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <stdlib.h>


#define P_PACKAGE PACKAGE
#define P_VERSION VERSION
#define P_REENTRANT _REENTRANT
#undef PACKAGE
#undef VERSION
#undef _REENTRANT

#include "dpsearch.h"
#include "dps_indexertool.h"
#include "dps_log.h"

#undef PACKAGE
#undef VERSION
#undef _REENTRANT
#define PACKAGE P_PACKAGE
#define VERSION P_VERSION
#define _REENTRANT P_REENTRANT

#define DEFAULT_TRACK	DPS_TRACK_DISABLED
#define DEFAULT_MODE	DPS_MODE_ALL
#define DEFAULT_ORD	DPS_ORD_RATE
#define DEFAULT_PS	20
#define DEFAULT_PN	0

#define MAX_QUERY_SIZE  200
#define MAX_PS          100

#define MAX_QUERY_SIZE  200
#define MAX_PS          100
#define MAX_SPELL       128
#define MAX_AFFIX       128

/*
typedef struct tmp_affix_struct {
        char lang[3];
	char *path;
} AFFIX;

typedef struct tmp_spell_struct {
	char lang[3];
	char *path;
} SPELL;

AFFIX Affix[MAX_AFFIX];
SPELL Spell[MAX_SPELL];

int affix_num;
int spell_num;
*/

AV *AVstats;
AV *AVreferers;

/*
 * store results in hash
 */

static void dosearch(HV *result,char *query_words,DPS_AGENT *Agent) {

  DPS_RESULT *Res=NULL;
  AV *records = newAV();
  size_t first = 0;
  size_t last = 0;
  size_t num_pages = 0, page_size, page_number;
  int is_next=0;
  size_t i, j, r;

/*	DpsSetLogLevel(Agent, 6);*/
	DpsOpenLog("perl", Agent->Conf, 1);
/*	fprintf(stderr, "QUERY: %s\n", query_words);*/
  DpsParseQueryString(Agent, &Agent->Conf->Vars, query_words);
  /* Call again to load search Limits if need */
  DpsParseQueryString(Agent, &Agent->Conf->Vars, query_words);
  Agent->Flags = Agent->Conf->Flags;
  /* res */

  DpsVarListReplaceStr(&Agent->Conf->Vars, "DetectClones", "no"); /* FIXME: need some coding */
  DpsVarListReplaceLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");
  DpsVarListReplaceStr(&Agent->Vars, "QUERY_STRING", query_words);

  DpsAgentStoredConnect(Agent);
	
  if((Res = DpsFind(Agent)) != NULL){

    if(Res->num_rows){

      first = Res->first;
      last = Res->last;
      page_size   = DpsVarListFindInt(&Agent->Conf->Vars, "ps", DEFAULT_PS);
      page_number = DpsVarListFindInt(&Agent->Conf->Vars, "np", 0);

      num_pages = (Res->total_found / (page_size ? page_size : DEFAULT_PS)) + 
	((Res->total_found % (page_size ? page_size : DEFAULT_PS) != 0 ) ?  1 : 0);

      is_next = last < Res->total_found;
      
      for(i = 0; i < Res->num_rows; i++) {
      
	HV* found = newHV();
	DPS_DOCUMENT *Doc = &Res->Doc[i];

	for (r = 0; r < 256; r++)
	for (j = 0; j < Doc->Sections.Root[r].nvars; j++) {
/*	fprintf(stderr, "DOC[%d]: %s -> %s\n", i, Doc->Sections.Root[r].Var[j].name, Doc->Sections.Root[r].Var[j].val);*/
	  if (Doc->Sections.Root[r].Var[j].val) {
		  hv_store(found, Doc->Sections.Root[r].Var[j].name, strlen(Doc->Sections.Root[r].Var[j].name),
			   newSVpv(Doc->Sections.Root[r].Var[j].val, 0), 0);
	  }
	}
	
        /* int values */
 /*       hv_store(found, "url_id", strlen("url_id"), newSViv(DpsVarListFindInt(&Doc->Sections, "DP_ID", 0)), 0);
        hv_store(found,"status",strlen("status"),newSViv(Res->Doc[i].status),0);
        hv_store(found,"size",strlen("size"),newSViv(Res->Doc[i].size),0);
        hv_store(found,"rating",strlen("rating"),newSViv(Res->Doc[i].rating),0);
        hv_store(found,"order",strlen("order"),newSViv(Res->Doc[i].order),0);
        hv_store(found,"referrer",strlen("referrer"),newSViv(Res->Doc[i].referrer),0);
        hv_store(found,"tag",strlen("tag"),newSViv(Res->Doc[i].tag),0);
        hv_store(found,"hops",strlen("hops"),newSViv(Res->Doc[i].hops),0);
        hv_store(found,"indexed",strlen("indexed"),newSViv(Res->Doc[i].indexed),0);*/
        
	/* char values */
 /*       hv_store(found,"url",strlen("url"),newSVpv(Res->Doc[i].url,0),0);
        hv_store(found,"content_type",strlen("content_type"),newSVpv(Res->Doc[i].content_type,0),0);
        hv_store(found,"title",strlen("title"),newSVpv(Res->Doc[i].title,0),0);
        hv_store(found,"keywords",strlen("keywords"),newSVpv(Res->Doc[i].keywords,0),0);
        hv_store(found,"description",strlen("description"),newSVpv(Res->Doc[i].description,0),0);
        hv_store(found,"text",strlen("text"),newSVpv(Res->Doc[i].text,0),0);
        hv_store(found,"category",strlen("category"),newSVpv(Res->Doc[i].category,0),0);*/
  
        /* time_t values */
 /*       hv_store(found,"last_mod_time",strlen("last_mod_time"),newSViv(Res->Doc[i].last_mod_time),0);
        hv_store(found,"last_index_time",strlen("last_index_time"),newSViv(Res->Doc[i].last_index_time),0);
        hv_store(found,"next_index_time",strlen("next_index_time"),newSViv(Res->Doc[i].next_index_time),0);*/
    
        /* push record's ref in AV records */
        av_push(records, newRV_inc((SV*) found));
      
      }

    }
    
    hv_store(result, "work_time", strlen("work_time"), newSVpv(DpsVarListFindStr(&Agent->Conf->Vars, "SearchTime", "<>"), 0), 0);

    DpsResultFree(Res);

  } else
      fprintf(stderr, "Error: Dataparksearch : %s", DpsEnvErrMsg(Agent->Conf));

  /* insert records's ref in result "records"*/
  hv_store(result,"records",strlen("records"),newRV_inc((SV*) records),0);

  /* insert others SVs */
  hv_store(result, "total_found", strlen("total_found"), newSViv((int)Res->total_found), 0);
  hv_store(result,"page_number",strlen("page_number"),newSViv(page_number),0);
  hv_store(result,"word_info",strlen("word_info"),newSVpv( DpsVarListFindStr(&Agent->Conf->Vars, "W", "<>"),0),0);
  hv_store(result,"word_info_ext",strlen("word_info_ext"),newSVpv( DpsVarListFindStr(&Agent->Conf->Vars, "WE", "<>"),0),0);
/*  hv_store(result,"W",strlen("W"),newSVpv( DpsVarListFindStr(&Agent->Conf->Vars, "W", "<>"),0),0);
  hv_store(result,"WE",strlen("WE"),newSVpv( DpsVarListFindStr(&Agent->Conf->Vars, "WE", "<>"),0),0);*/

  hv_store(result,"num_pages",strlen("num_pages"),newSViv(num_pages),0);
  hv_store(result,"is_next",strlen("is_next"),newSViv(is_next),0);
  hv_store(result,"first",strlen("first"),newSViv(first),0);
  hv_store(result,"last",strlen("last"),newSViv(last),0);

  for (r = 0; r < 256; r++)
  for (j = 0; j < Agent->Conf->Vars.Root[r].nvars; j++) {
    hv_store(result, Agent->Conf->Vars.Root[r].Var[j].name, strlen(Agent->Conf->Vars.Root[r].Var[j].name),
	     newSVpv(Agent->Conf->Vars.Root[r].Var[j].val, 0), 0);
 /*	fprintf(stderr, "ENV: %s -> %s\n", Agent->Conf->Vars.Root[r].Var[j].name, Agent->Conf->Vars.Root[r].Var[j].val);*/
  }


}

/* 
 * CallBack Func for Referers
 */

static void DpsRefProc(int code, 
		       const char *Purl, 
		       const char *Pref) {
  AV *records = newAV();
  SV *SVcode;
  SV *SVurl;
  SV *SVref;

  SVcode = newSViv(code);
  SVurl  = newSVpv((char*)Purl,0);
  SVref  = newSVpv((char*)Pref,0);

  av_push(records, SVcode);
  av_push(records, SVurl);
  av_push(records, SVref);

  av_push(AVreferers, newRV_inc((SV*) records));

}

static int GetReferers(DPS_AGENT * Indexer){
  int res;
  AVreferers = newAV();
  printf("\n          URLs and referers \n\n");
  res = DpsURLAction(Indexer, NULL, DPS_URL_ACTION_REFERERS);
  return(res);
}


/* */

static int GetStatistics(DPS_AGENT * Indexer){
  int res;
  DPS_STATLIST   Stats;
  size_t         snum;
  DPS_STAT  Total;
  AV *TotalRecords = newAV();
  SV *SVcode;
  SV *SVexpired;
  SV *SVexpired_size;
  SV *SVtotal;
  SV *SVtotal_size;
  SV *SVstr;

  AVstats = newAV();
  bzero((void*)&Total, sizeof(Total));

  res = DpsStatAction(Indexer, &Stats);

  for(snum=0;snum<Stats.nstats;snum++){
    DPS_STAT  *S = &Stats.Stat[snum];
    AV *records = newAV();

    SVcode = newSViv(S->status);
    SVexpired = newSViv(S->expired);
    SVtotal = newSViv(S->total);
    SVexpired_size = newSViv(S->expired_size);
    SVtotal_size = newSViv(S->total_size);
    SVstr = newSVpv((char*)DpsHTTPErrMsg(S->status), 0);

    av_push(records, SVcode);
    av_push(records, SVexpired);
    av_push(records, SVexpired_size);
    av_push(records, SVtotal);
    av_push(records, SVtotal_size);
    av_push(records, SVstr);

    av_push(AVstats, newRV_inc((SV*) records));

    Total.expired += S->expired;
    Total.total += S->total;
    Total.total_size += S->total_size;
    Total.expired_size += S->expired_size;
  }

  SVcode = newSVpv((char*)"Total", 0);
  SVexpired = newSViv(Total.expired);
  SVexpired_size = newSViv(Total.expired_size);
  SVtotal = newSViv(Total.total);
  SVtotal_size = newSViv(Total.total_size);
  SVstr = newSVpv((char*)"Total", 0);

  av_push(TotalRecords, SVcode);
  av_push(TotalRecords, SVexpired);
  av_push(TotalRecords, SVexpired_size);
  av_push(TotalRecords, SVtotal);
  av_push(TotalRecords, SVtotal_size);
  av_push(TotalRecords, SVstr);

  av_push(AVstats, newRV_inc((SV*) TotalRecords));

  DPS_FREE(Stats.Stat);
  return res;
}


/*
 * Assign Hashs values into DPS_AGENT
 */
/*
static void assign_hash(DPS_AGENT *Agent, char *name, SV *value ) {

  SV* itervalue;
  HV* hash = (HV*)SvRV((SV*)value);
  HE* hashentry;
  STRLEN n_a;
  char *location;
  char *lang;
  char str[PATH_MAX]="";

  if (!strncasecmp(name,"Affix", 5)){
    hv_iterinit(hash);
    while ((hashentry=hv_iternext(hash)) != NULL){

      itervalue = hv_iterval(hash,hashentry);
      if ( SvTYPE(itervalue) !=SVt_PV ) croak("Usage: Dataparksearch::_DpsQuery('Affix'=>{lang => location,...},...)");
      lang = (char *)SvPV(hv_iterkeysv(hashentry), n_a);
      location = (char *)SvPV(itervalue, n_a);

      if (affix_num < MAX_AFFIX){

        if (*location=='/') strcpy(str,location);
        else sprintf(str,"%s/%s",DPS_CONF_DIR,location);

        strncpy(Affix[(int)affix_num].lang,lang,2);
        Affix[(int)affix_num].lang[2]=0;
        Affix[(int)affix_num].path=strdup(str);
        affix_num++;
      } else {
        fprintf(stderr,"Warning: Dataparksearch MAX_AFFIX reached, skipping %s - %s\n",lang,location);
      }
    }
    return;
  }

  if (!strncasecmp(name,"Spell", 5)) {
    hv_iterinit(hash);
    while ((hashentry=hv_iternext(hash)) != NULL){

      itervalue = hv_iterval(hash,hashentry);
      if ( SvTYPE(itervalue) !=SVt_PV ) croak("Usage: Dataparksearch::_DpsQuery('Spell'=>{lang => location,...},...)");
      lang = (char *)SvPV(hv_iterkeysv(hashentry), n_a);
      location = (char *)SvPV(itervalue, n_a);

      if (spell_num < MAX_SPELL){

        if (*location=='/') strcpy(str,location);
        else sprintf(str,"%s/%s",DPS_CONF_DIR,location);

        strncpy(Spell[(int)spell_num].lang,lang,2);
        Spell[(int)spell_num].lang[2]=0;
        Spell[(int)spell_num].path=strdup(str);
        spell_num++;
      } else {
        fprintf(stderr,"Warning: Dataparksearch MAX_SPELL reached, skipping %s - %s\n",lang,location);
      }
    }
    return;
  }

  fprintf(stderr,"Warning: Dataparksearch hash argument %s not supported\n",name);

}
*/
/*
 *
 *
 *
 */

static void InitAgent(SV *self, DPS_AGENT *Agent) {
  HE* hashentry;
  char * value;
  STRLEN n_a;
  HV* hash = (HV*)SvRV((SV*)self);


  DpsAgentInit(Agent, Agent->Conf, Agent->handle);
/*  affix_num=0;
  spell_num=0;
*/

  /* inits with defaults */
/*  Agent->search_mode=DEFAULT_MODE;
  Agent->sort_order=DEFAULT_ORD;
  Agent->page_number=DEFAULT_PN;
  Agent->page_size=DEFAULT_PS;
  Agent->track_mode=DEFAULT_TRACK;*/

  /* Check for TrackQuery */
/*  if ( hv_exists(hash,"TrackQuery",strlen("TrackQuery"))){
    hashentry = hv_fetch_ent(hash,newSVpv("TrackQuery",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    switch(*value){
      case '0' :
        Agent->track_mode=DPS_TRACK_DISABLED;
      case '1' :
        Agent->track_mode=DPS_TRACK_QUERIES;
      default :
        Agent->track_mode=DPS_TRACK_DISABLED;
    }
  }*/
  
  /* Check for mode */
   if ( hv_exists(hash,"mode",strlen("mode"))){
    hashentry = hv_fetch_ent(hash,newSVpv("mode",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "m", value);
   }
 
  /* Check for sort */
  if ( hv_exists(hash,"sort",strlen("sort"))){
    hashentry = hv_fetch_ent(hash,newSVpv("sort",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "s", value);
  }

  /* Check for pn */
  if ( hv_exists(hash,"pn",strlen("pn"))){
    hashentry = hv_fetch_ent(hash,newSVpv("pn",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "pn", value);
  }
  
  /* Check for ps */
  if ( hv_exists(hash,"ps",strlen("ps"))){
    hashentry = hv_fetch_ent(hash,newSVpv("ps",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "ps", value);
  }
  
  /* Check for LocalCharset */
  if ( hv_exists(hash,"LocalCharset",strlen("LocalCharset"))){
    hashentry = hv_fetch_ent(hash,newSVpv("LocalCharset",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "LocalCharset", value);
  }
  
  /* Check for BrowserCharset */
  if ( hv_exists(hash,"BrowserCharset",strlen("BrowserCharset"))){
    hashentry = hv_fetch_ent(hash,newSVpv("BrowserCharset",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "BrowserCharset", value);
  }
  
  /* Check for ul */
  if ( hv_exists(hash,"ul",strlen("ul"))){
    hashentry = hv_fetch_ent(hash,newSVpv("ul",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "ul", value);
  }
  
  /* Check for Weight Factor wf */
  if ( hv_exists(hash,"wf",strlen("wf"))){
    hashentry = hv_fetch_ent(hash,newSVpv("wf",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "wf", value);
  }
  
  /* Check for Word Match wm */
  if ( hv_exists(hash,"wm",strlen("wm"))){
    hashentry = hv_fetch_ent(hash,newSVpv("wm",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    DpsVarListReplaceStr(&Agent->Conf->Vars, "wm", value);
  }
  
  /* Check for IspellMode */
/*  if ( hv_exists(hash,"IspellMode",strlen("IspellMode"))){
    hashentry = hv_fetch_ent(hash,newSVpv("IspellMode",0), 0, 0);
    value = (char *)SvPV(HeVAL(hashentry), n_a);
    if (!strcasecmp(value, "db"))
      Agent->Conf->ispell_mode|=DPS_ISPELL_MODE_DB;
    else
      Agent->Conf->ispell_mode &= ~DPS_ISPELL_MODE_DB;
  }*/

  /* check Affix & Spell only if not IspellMode db */
/*  if(!(Agent->Conf->ispell_mode&DPS_ISPELL_MODE_DB)){*/
    
    /* Check for Affix */
/*    if ( hv_exists(hash,"Affix",strlen("Affix"))){
      hashentry = hv_fetch_ent(hash,newSVpv("Affix",0), 0, 0);
      assign_hash(Agent,"Affix",(SV*)HeVAL(hashentry));
    }
  */  
    /* Check for Spell*/
/*    if ( hv_exists(hash,"Spell",strlen("Spell"))){
      hashentry = hv_fetch_ent(hash,newSVpv("Spell",0), 0, 0);
      assign_hash(Agent,"Spell",(SV*)HeVAL(hashentry));
    }

  }
*/
  return;

}


/*
 * Return an DPS_ENV, if defined, or initialize new otherwise with 
 * $self->{'DBAddr'} & $self->{'DBMode'}
 *
 */

static DPS_ENV * LoadConf(SV *self){

  DPS_ENV *Conf=NULL;
  HV* hash = (HV*)SvRV((SV*)self);
  HE* hashentry;
  STRLEN n_a;
  char * DBAddr;


  if (! hv_exists(hash, "Conf", strlen("Conf"))) {

        DpsInit(0, NULL, NULL);
  	Conf = DpsEnvInit(NULL);

  	/* define callback funcs */
  	DpsSetRefProc(Conf,DpsRefProc);
	Conf->flags = DPS_FLAG_UNOCON;
  
/*  	if (! hv_exists(hash,"DBAddr",strlen("DBAddr")))
		croak("Error: Dataparksearch need a DBAddr");
  */

#if 0
  	hashentry = hv_fetch_ent(hash,newSVpv("DBAddr",0), 0, 0);
  	DBAddr = (char *)SvPV(HeVAL(hashentry), n_a);

  	if(DPS_OK != DpsDBListAdd(&Conf->dbl, DBAddr, DPS_OPEN_MODE_WRITE)) {
    		croak("Error: invalid DBAddr ", DBAddr);
/*                sprintf(Conf->errstr, "Invalid DBAddr: '%s'",av[1]?av[1]:"");*/
  	}
  	DpsVarListReplaceStr(&Conf->Vars, "DBAddr", DBAddr);
#endif

	hv_store(hash, "Conf", strlen("Conf"), newSViv((int)Conf), 0);

  } else {
	hashentry = hv_fetch_ent(hash, newSVpv("Conf", 0), 0, 0);
	Conf = (DPS_ENV*)SvIV(HeVAL(hashentry));

  }

  return(Conf);

}

static int DpsPerlLoadConfig(SV *self, DPS_AGENT *Agent) {
     HV* hash = (HV*)SvRV((SV*)self);
     HE* hashentry;
     STRLEN n_a;
     int rc;
     char *fname;

     hashentry = hv_fetch_ent(hash,newSVpv("config",0), 0, 0);
     fname = (char *)SvPV(HeVAL(hashentry), n_a);
     Agent->flags = Agent->Conf->flags |= DPS_FLAG_UNOCON | DPS_FLAG_SPELL;

     if (DPS_OK == (rc = DpsEnvLoad(Agent, fname, DPS_FLAG_UNOCON | DPS_FLAG_SPELL /*0L*/ /*lflags*/ ))){
          rc = (Agent->flags & DPS_FLAG_UNOCON) ? (Agent->Conf->dbl.nitems == 0) : (Agent->dbl.nitems == 0);
          if (rc) {
               sprintf(Agent->Conf->errstr, "Error: '%s': No required DBAddr commands were specified", fname);
               rc= DPS_ERROR;
          } else rc = DPS_OK;
     }
     Agent->Flags = Agent->Conf->Flags;
     Agent->flags |= DPS_FLAG_UNOCON;
     Agent->Conf->flags |= DPS_FLAG_UNOCON;
     DpsSetLogLevel(NULL, DpsVarListFindInt(&Agent->Conf->Vars, "LogLevel", 5));
     DpsOpenLog("dp-perl", Agent->Conf, !strcasecmp(DpsVarListFindStr(&Agent->Conf->Vars, "Log2stderr", "yes"), "yes"));
     DpsVarListAddLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");
     DpsLog(Agent, DPS_LOG_DEBUG, "VarDir: '%s'", DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR));
     DpsLog(Agent, DPS_LOG_DEBUG, "URLDataFiles: '%d'", DpsVarListFindInt(&Agent->Vars, "URLDataFiles", 0x300));
     DpsLog(Agent, DPS_LOG_DEBUG, "Affixes: %d, Spells: %d, Synonyms: %d, Acronyms: %d, Stopwords: %d",
		       Agent->Conf->Affixes.naffixes, Agent->Conf->Spells.nspell,
		       Agent->Conf->Synonyms.nsynonyms,
		       Agent->Conf->Acronyms.nacronyms,
		       Agent->Conf->StopWords.nstopwords);
     DpsLog(Agent,DPS_LOG_DEBUG,"Chinese dictionary with %d entries", Agent->Conf->Chi.nwords);
     DpsLog(Agent,DPS_LOG_DEBUG,"Korean dictionary with %d entries", Agent->Conf->Korean.nwords);
     DpsLog(Agent,DPS_LOG_DEBUG,"Thai dictionary with %d entries", Agent->Conf->Thai.nwords);
     return rc;
}

static int DpsPerlLoadTemplate(SV *self, DPS_AGENT *Agent) {
     HV* hash = (HV*)SvRV((SV*)self);
     HE* hashentry;
     STRLEN n_a;
     int rc;
     char *fname;

     hashentry = hv_fetch_ent(hash,newSVpv("template",0), 0, 0);
     fname = (char *)SvPV(HeVAL(hashentry), n_a);


     DpsVarListReplaceStr(&Agent->Conf->Vars, "tmplt", fname);
     Agent->tmpl.Env_Vars = &Agent->Conf->Vars;
     if ((rc = DpsTemplateLoad(Agent, Agent->Conf, &Agent->tmpl, fname))) {
       fprintf(stderr, "Error loading template '%s'\n", fname);
       return rc;
     }

     Agent->Flags = Agent->Conf->Flags;
     Agent->flags |= DPS_FLAG_UNOCON;
     Agent->Conf->flags |= DPS_FLAG_UNOCON;
     DpsVarListReplaceLst(&Agent->Vars, &Agent->Conf->Vars, NULL, "*");

     return rc;
}


/*
  Return an DPS_AGENT, if defined, or initialize new otherwise
*/
static DPS_AGENT *LoadAgent(SV *self, DPS_ENV *Conf) {
	DPS_AGENT *Agent = NULL;
  	HV* hash = (HV*)SvRV((SV*)self);
  	HE* hashentry;
  	STRLEN n_a;
  
	if (! hv_exists(hash, "Agent", strlen("Agent"))) {
		Agent = DpsAgentInit(NULL, Conf, 0);
		InitAgent(self, Agent);
		hv_store(hash, "Agent", strlen("Agent"), newSViv((int)Agent), 0);

		if ( hv_exists(hash, "template", strlen("template"))) {
		  DpsPerlLoadTemplate(self, Agent);
		} else {
		  DpsPerlLoadConfig(self, Agent);
		}

	} else {
  		hashentry = hv_fetch_ent(hash, newSVpv("Agent", 0), 0, 0);
		Agent = (DPS_AGENT*)SvIV(HeVAL(hashentry));
	}
	return Agent;
}



/*
 * Perl interface
 *
 */

MODULE = Dataparksearch		PACKAGE = Dataparksearch

SV*
DpsVersion()
	PPCODE:
	 EXTEND(SP, 1);
	 PUSHs(newSVpv((char*)DpsVersion(), 0));

void
GetStatistics( ... )
        PREINIT:
         DPS_AGENT *Agent=NULL;
         DPS_ENV *Conf=NULL;
	 SV *self=NULL;
        PPCODE:
	 self = ST(0);
	 Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
	 if(GetStatistics(Agent) != DPS_OK){
	   DpsLog(Agent,DPS_LOG_ERROR,"Error: '%s'", DpsEnvErrMsg(Agent->Conf));
	 }
	 EXTEND(SP,1);
	 PUSHs(newRV_inc((SV*) AVstats));

void
GetReferers( ... )
        PREINIT:
         DPS_AGENT *Agent=NULL;
         DPS_ENV *Conf=NULL;
         SV *self=NULL;
        PPCODE:
         self = ST(0);
	 Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
         if(GetReferers(Agent) != DPS_OK){
	   DpsLog(Agent, DPS_LOG_ERROR, "Error: '%s'", DpsEnvErrMsg(Agent->Conf));
         }
         EXTEND(SP,1);
         PUSHs(newRV_inc((SV*) AVreferers));

int
DpsGetDocCount(...)
        PREINIT:
         DPS_AGENT *Agent=NULL;
         DPS_ENV *Conf=NULL;
         SV *self=NULL;
	 int count=0;
        PPCODE:
         self = ST(0);
	 Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
         DpsInit(0, NULL, NULL);
	 if (Agent->doccount == 0) DpsURLAction(Agent, NULL, DPS_URL_ACTION_DOCCOUNT);
	 count = Agent->doccount;
         EXTEND(SP,1);
         PUSHs(newSViv(count));

int
_DpsQuery( ... )
	PREINIT:
	 DPS_AGENT *Agent=NULL;
	 DPS_ENV *Conf=NULL;
	 HV *result=newHV();
	 SV *self=NULL;
	 int i;
	 char *query;
  	 char words[MAX_QUERY_SIZE + 1];
	 STRLEN n_a;
	PPCODE:
	 words[0] = '\0';

	 if (items != 2) 
	   croak("Usage: Dataparksearch $self->_DpsQuery(\"words\")"); 

	 self = ST(0);
 
	 Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);

	 /* read query words */
	 query = (char *)SvPV(ST(1),n_a);

 /*         strncpy(words,query,MAX_QUERY_SIZE);*/
         if (!strcmp(query, "")) 
	   croak("Error: Dataparksearch Need a query argument");

	 dps_snprintf(words, MAX_QUERY_SIZE, "q=%s", query);
	 words[MAX_QUERY_SIZE] = '\0';

	 dosearch(result,words,Agent);
         
 /*	 for(i=0;i<affix_num;i++) free(Affix[i].path);
	 for(i=0;i<spell_num;i++) free(Spell[i].path);
 */	 
	      
	 EXTEND(SP,1);
	 PUSHs(newRV_inc((SV*) result));


void
Free( ... )
        PREINIT:
         DPS_AGENT *Agent=NULL;
         DPS_ENV *Conf=NULL;
         SV *self=NULL;
        PPCODE:
         self = ST(0);
         Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
         DpsAgentFree(Agent);
         DpsEnvFree(Conf);
         EXTEND(SP,1);

int
LoadConfig( ... )
        PREINIT:
         DPS_AGENT *Agent=NULL;
         DPS_ENV *Conf=NULL;
         SV *self=NULL;
	 int rc;
        PPCODE:
         self = ST(0);
         Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
         rc = DpsPerlLoadConfig(self, Agent);
         EXTEND(SP,1);
         PUSHs(newSViv(rc));


int
LoadTemplate( ... )
        PREINIT:
         DPS_AGENT *Agent=NULL;
         DPS_ENV *Conf=NULL;
         SV *self=NULL;
	 int rc;
        PPCODE:
         self = ST(0);
         Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
         rc = DpsPerlLoadTemplate(self, Agent);
         EXTEND(SP,1);
         PUSHs(newSViv(rc));

SV*
_GetSEA( ... )
	PREINIT:
         DPS_AGENT *Agent = NULL;
         DPS_ENV *Conf = NULL;
	 DPS_RESULT *Res = NULL;
         DPS_DB *db;
         SV *self = NULL;
	 char *SEA = "";
	 char *url = NULL;
	 size_t i, dbfrom = 0, dbto;
	 STRLEN n_a;
	 int rc;
        PPCODE:
         self = ST(0);
	 url = (char*)SvPV(ST(1), n_a);
         Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
	 dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
	 Res = DpsResultInit(NULL);
	 if (Res != NULL) {
	 	Res->num_rows = 1;
		Res->total_found = 1;
		Res->Doc = (DPS_DOCUMENT*)DpsMalloc(sizeof(DPS_DOCUMENT) * (Res->num_rows));
		if (Res->Doc != NULL) {
			DpsDocInit(Res->Doc);
			DpsVarListAddStr(&Res->Doc->Sections, "URL", url);
			if (DPS_OK == (rc = DpsURLAction(Agent, Res->Doc, DPS_URL_ACTION_FINDBYURL))) {
				for (i = dbfrom; i < dbto; i++) {
				  db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] : &Agent->dbl.db[i];
				  switch(db->DBDriver){
					case DPS_DB_SEARCHD:
					  rc = DpsResAddDocInfoSearchd(Agent, db, Res, i);
					  break;
					default:
					  if (db->DBMode == DPS_DBMODE_CACHE) {
					    rc = DpsResAddDocInfoCache(Agent, db, Res, i);
					  }
#ifdef HAVE_SQL
					  rc = DpsResAddDocInfoSQL(Agent, db, Res, i);
						break;
#endif
				  }
				}
			}
			if (rc == DPS_OK) {
				SEA = DpsStrdup(DpsVarListFindStr(&Res->Doc->Sections, "SEA", ""));
			}
			DpsDocFree(Res->Doc);
		}
		DpsResultFree(Res);
	 }
	 EXTEND(SP,1);
	 PUSHs(newSVpv(SEA, strlen(SEA)));
	 DPS_FREE(SEA);


SV*
_GetSEAbyId( ... )
	PREINIT:
         DPS_AGENT *Agent = NULL;
         DPS_ENV *Conf = NULL;
	 DPS_RESULT *Res = NULL;
         DPS_DB *db;
         SV *self = NULL;
	 char *SEA = "";
	 size_t i, dbfrom = 0, dbto;
	 STRLEN n_a;
	 char *DP_ID;
	 int rc;
        PPCODE:
         self = ST(0);
	 DP_ID = (char*)SvPV(ST(1), n_a);
         Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
	 dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
	 Res = DpsResultInit(NULL);
	 if (Res != NULL) {
	 	Res->num_rows = 1;
		Res->total_found = 1;
		Res->Doc = (DPS_DOCUMENT*)DpsMalloc(sizeof(DPS_DOCUMENT) * (Res->num_rows));
		if (Res->Doc != NULL) {
			DpsDocInit(Res->Doc);
			DpsVarListAddStr(&Res->Doc->Sections, "DP_ID", DP_ID);
				for (i = dbfrom; i < dbto; i++) {
				  db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] : &Agent->dbl.db[i];
				  switch(db->DBDriver){
					case DPS_DB_SEARCHD:
					  rc = DpsResAddDocInfoSearchd(Agent, db, Res, i);
					  break;
					default:
					  if (db->DBMode == DPS_DBMODE_CACHE) {
					    rc = DpsResAddDocInfoCache(Agent, db, Res, i);
					  }
#ifdef HAVE_SQL
					  rc = DpsResAddDocInfoSQL(Agent, db, Res, i);
						break;
#endif
				  }
				}
			if (rc == DPS_OK) {
				SEA = DpsStrdup(DpsVarListFindStr(&Res->Doc->Sections, "SEA", ""));
			}
			DpsDocFree(Res->Doc);
		}
		DpsResultFree(Res);
	 }
	 EXTEND(SP,1);
	 PUSHs(newSVpv(SEA, strlen(SEA)));
	 DPS_FREE(SEA);

SV*
_WordNormalize( ... )
	PREINIT:
         DPS_AGENT *Agent = NULL;
         DPS_ENV *Conf = NULL;
	 DPS_WIDEWORD OWord;
	 DPS_SPELL **cur = NULL, **norm = NULL;
	 DPS_PSPELL FZ;
	 DPS_SPELL s_p, *p_sp = &s_p;
         SV *self = NULL;
 	 DPS_CONV bc_uni, uni_bc;
	 DPS_CHARSET *browser_cs, *sys_int;
	 size_t WRDlen, NORMlen;
	 STRLEN n_a;
	 char *WRD, *NORM = NULL;
	 dpsunicode_t *uwrd = NULL;
        PPCODE:
         self = ST(0);
	 WRD = (char*)SvPV(ST(1), n_a);
         Conf = LoadConf(self);
	 Agent = LoadAgent(self, Conf);
	
	 if (!(browser_cs = Agent->Conf->bcs)) {
		if (!(browser_cs = Agent->Conf->lcs)) {
			browser_cs = DpsGetCharSet("iso-8859-1");
		}
	 }
	 sys_int = DpsGetCharSet("sys-int");
	 DpsConvInit(&bc_uni, browser_cs, sys_int, ""/*Conf->CharsToEscape*/, DPS_RECODE_HTML_FROM);
	 DpsConvInit(&uni_bc, sys_int, browser_cs, Conf->CharsToEscape, DPS_RECODE_HTML_TO);

	 WRDlen = dps_strlen(WRD);
	 if ((uwrd = (dpsunicode_t*)DpsMalloc(sizeof(dpsunicode_t) * (28 * WRDlen + 64))) == NULL) {
		goto Norm_exit;
         }
	 DpsConv(&bc_uni, (char*)uwrd, sizeof(dpsunicode_t) * (WRDlen + 1), WRD, WRDlen + 1);

  	 FZ.nspell = 0;
  	 FZ.cur = &p_sp;
  	 s_p.word = NULL;

	 OWord.len = WRDlen;
	 OWord.order = 0;
	 OWord.count = 0;
	 OWord.crcword = DpsStrHash32(WRD);
	 OWord.word = WRD;
	 OWord.uword = uwrd;
	 OWord.ulen = DpsUniLen(uwrd);
	 OWord.origin = 0;
	 cur = norm = DpsNormalizeWord(Agent, &OWord, &FZ);
	 if (cur != NULL) {
	   NORMlen = DpsUniLen((*cur)->word);
	   if ((NORM = (char*)DpsMalloc(sizeof(char*) * (14 * NORMlen + 1))) == NULL) {
		goto Norm_exit;
           }
     	   DpsUniStrRCpy(uwrd, (*cur)->word); 
	   DpsConv(&uni_bc, NORM, 14 * NORMlen + 1, (char*)uwrd, sizeof(dpsunicode_t) * (NORMlen + 1));
	 } else if (FZ.nspell > Agent->WordParam.min_word_len) {
	   NORMlen = DpsUniLen(s_p.word);
	   if ((NORM = (char*)DpsMalloc(sizeof(char*) * (14 * NORMlen + 1))) == NULL) {
		goto Norm_exit;
           }
     	   DpsUniStrRCpy(uwrd, s_p.word); 
	   DpsConv(&uni_bc, NORM, 14 * NORMlen + 1, (char*)uwrd, sizeof(dpsunicode_t) * (NORMlen + 1));
	 }
 Norm_exit:
	 DPS_FREE(uwrd);
         DPS_FREE(norm);
	 DPS_FREE(s_p.word);
	 EXTEND(SP,1);
	 if (NORM == NULL) {
		 PUSHs(newSVpv(WRD, dps_strlen(WRD)));
	 } else {
		 PUSHs(newSVpv(NORM, dps_strlen(NORM)));
	         DPS_FREE(NORM);
	 }

