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
#include "dps_utils.h"
#include "dps_env.h"
#include "dps_parser.h"
#include "dps_robots.h"
#include "dps_hrefs.h"
#include "dps_server.h"
#include "dps_url.h"
#include "dps_proto.h"
#include "dps_alias.h"
#include "dps_log.h"
#include "dps_match.h"
#include "dps_stopwords.h"
#include "dps_guesser.h"
#include "dps_vars.h"
#include "dps_synonym.h"
#include "dps_acronym.h"
#include "dps_doc.h"
#include "dps_result.h"
#include "dps_spell.h"
#include "dps_db.h"
#include "dps_db_int.h"
#include "dps_chinese.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

/**************************** DBAddr ***********************************/

DPS_ENV *DpsEnvInit(DPS_ENV *Conf){
#ifdef MECAB
  const char *mecab_argv[8] = {"mecab", "-F", "%m ", "-B", " ", "-E", " ", NULL};
#endif
	if(!Conf){
		Conf=(DPS_ENV *)DpsMalloc(sizeof(DPS_ENV));
		if (Conf == NULL) return NULL;
		bzero((void*)Conf, sizeof(*Conf));
		Conf->freeme=1;
	}else{
		bzero((void*)Conf, sizeof(*Conf));
	}
	
	Conf->Flags.OptimizeAtUpdate = 1;
	Conf->Flags.do_excerpt = 1;
	Conf->Flags.PopRankNeoIterations = 3;
	Conf->Flags.GuesserBytes = 512;
	Conf->Flags.robots_period = 604800;   /* one week */
	Conf->Flags.URLInfoSQL = 1;
	Conf->Flags.SRVInfoSQL = 1;
	Conf->Flags.CheckInsertSQL = 1;
	Conf->Flags.mark_for_index = 1;
	Conf->Flags.MaxSiteLevel = 2;
	Conf->Flags.SEASentences = 32;
	Conf->Flags.SEASentenceMinLength = 64;
	Conf->Flags.PagesInGroup = 1;
	Conf->Flags.SubDocCnt = 5;
	Conf->Flags.MaxCrawlDelay = 300;
	Conf->Flags.rel_nofollow = 1;
	Conf->Flags.bind_addr.sin_family = AF_INET;
	Conf->WordParam.min_word_len = 1;
	Conf->WordParam.max_word_len = 32;
	Conf->WordParam.correct_factor = 1;
	Conf->WordParam.incorrect_factor = 1;
	Conf->url_number = 0x7FFFFFFF;
	Conf->lcs=DpsGetCharSet("latin1");
	Conf->bcs=DpsGetCharSet("latin1");
	Conf->CharsToEscape = DpsStrdup("\"&<>");
#ifdef MECAB
/*	Conf->mecab = mecab_new2 ("mecab -F \"%m \" -B \" \" -E \" \"");*/
	Conf->mecab = mecab_new(7, (char**)mecab_argv);
#endif
	
	return(Conf);
}

void DpsEnvFree(DPS_ENV * Env){
  size_t filenum;
/*  size_t NFiles = (size_t)DpsVarListFindInt(&Env->Vars, "URLDataFiles", 0x300);*/
#ifdef MECAB
        mecab_destroy (Env->mecab);
#endif
	if (/*Env->Flags.PreloadURLData &&*/ Env->URLDataFile != NULL) {
/*	  for (filenum = 0 ; filenum < Env->n_files; filenum++) {
	    DPS_FREE(Env->URLDataFile[filenum].URLData);
	  }*/
	  DPS_FREE(Env->URLDataFile);
	}
	if (Env->Cfg_Srv != NULL) {
	  DpsServerFree(Env->Cfg_Srv);
	  DPS_FREE(Env->Cfg_Srv);
	}

	DpsDBListFree(&Env->dbl);
	DpsResultFree(&Env->Targets);
	DpsParserListFree(&Env->Parsers);
	DpsStopListFree(&Env->StopWords);
	DpsRobotListFree(&Env->Robots);
	
	DpsMatchListFree(&Env->MimeTypes);
	DpsMatchListFree(&Env->Aliases);
	DpsMatchListFree(&Env->ReverseAliases);
	DpsMatchListFree(&Env->Filters);
	DpsMatchListFree(&Env->SectionFilters);
	DpsMatchListFree(&Env->StoreFilters);
	DpsMatchListFree(&Env->SectionMatch);
	DpsMatchListFree(&Env->HrefSectionMatch);
	DpsMatchListFree(&Env->SubSectionMatch);
	DpsMatchListFree(&Env->ActionSQLMatch);
	DpsMatchListFree(&Env->SectionSQLMatch);
	DpsMatchListFree(&Env->BodyPatterns);
	DpsMatchListFree(&Env->QAliases);
	
	DpsSynonymListFree(&Env->Synonyms);
	DpsAcronymListFree(&Env->Acronyms);
	DpsVarListFree(&Env->Sections);
	DpsVarListFree(&Env->HrefSections);
	DpsLangMapListSave(&Env->LangMaps);
	DpsLangMapListFree(&Env->LangMaps);
	for (filenum = DPS_MATCH_min; filenum < DPS_MATCH_max; filenum++) DpsServerListFree(&Env->Servers[filenum]);
	DpsSpellListFree(&Env->Spells);
	DpsAffixListFree(&Env->Affixes);
	DpsQuffixListFree(&Env->Quffixes);
	DpsVarListFree(&Env->Vars);
	DpsChineseListFree(&Env->Chi);
	DpsChineseListFree(&Env->Thai);
	DpsChineseListFree(&Env->Korean);

	DPS_FREE(Env->CharsToEscape);
	DPS_FREE(Env->SrvPnt);

	if(Env->freeme)DPS_FREE(Env);
}

char * DpsEnvErrMsg(DPS_ENV * Conf) {
  size_t	i;
  DPS_DB *db;

  for(i = 0; i<Conf->dbl.nitems; i++){
    db = &Conf->dbl.db[i];
    if (db->errcode) {
      char *oe = (char*)DpsStrdup(Conf->errstr);
      dps_snprintf(Conf->errstr, 2048, "DB err: %s - %s", db->errstr, oe);
      DPS_FREE(oe);
    }
  }
  return(Conf->errstr);
}
