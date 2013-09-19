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
#include "dps_charsetutils.h"
#include "dps_stopwords.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_match.h"
#include "dps_conf.h"
#include "dps_match.h"
#include "dps_spell.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

DPS_STOPWORD dps_reg_match = {"<Match>", "", NULL, 7, 0};

DPS_STOPWORD * DpsStopListFind(DPS_STOPLIST *List, const dpsunicode_t *word, const char *lang) {
  size_t low, high, middle;
  int match;

	if(List->nstopwords > 0) {
	  low = 0;
	  high = List->nstopwords - 1;
	  while (low <= high) {
		middle = (low + high) / 2;
		match = DpsUniStrCmp(List->StopWord[middle].uword, word);
		if (match == 0 && lang != NULL && *lang != '\0') {
		  match = strncasecmp(List->StopWord[middle].lang, lang, dps_strlen(List->StopWord[middle].lang));
		}
		if (match == 0) return &List->StopWord[middle];
		if (match < 0 || low == high) { 
		  low = middle + 1;
		} else if (match > 0) { 
		  if (middle == 0) break; 
		  high = middle - 1;
		}
	  }
	}
	for(low = 0; low < List->StopMatch.nmatches; low++) {
	  match = DpsUniRegExec(&List->StopMatch.Match[low].UniReg, word);
	  if (match) return &dps_reg_match;
	}
	return NULL;
}

static int cmpstop(const void *s1, const void *s2){
  register int res = DpsUniStrCmp(((const DPS_STOPWORD*)s1)->uword, ((const DPS_STOPWORD*)s2)->uword);
  if (res == 0) res = strcasecmp(((const DPS_STOPWORD*)s1)->lang, ((const DPS_STOPWORD*)s2)->lang);
  return res;
}

void DpsStopListSort(DPS_STOPLIST *List){
	/* Sort stoplist to run binary search later */
	if (List->nstopwords > 1) DpsSort(List->StopWord, List->nstopwords, sizeof(DPS_STOPWORD), cmpstop);
}

static void DpsStopListSortForLast(DPS_STOPWORD *List, size_t n) {
  register size_t l = 0, c, r;
  DPS_STOPWORD T = List[ r = (n - 1) ];
  while (l < r) {
    c = (l + r) / 2;
    if ( cmpstop(&List[c], &T) < 0) l = c + 1;
    else r = c;
  }
  if (r < (n - 1) && cmpstop(&List[r], &T) < 0) r++;
  if (r == (n - 1)) return;
  dps_memmove(&List[r + 1], &List[r], (n - r - 1) * sizeof(DPS_STOPWORD));
  List[r] = T;
  return;
}

int DpsStopListAdd(DPS_STOPLIST *List, DPS_STOPWORD * stopword) {

	/* If the word is already in list     */
	/* We will not add it again           */
	/* But mark it as "international word"*/
	/* i.e. the word without language     */
	/* It will allow to avoid troubles    */
	/* with language guesser              */

	DPS_STOPWORD *F = DpsStopListFind(List, stopword->uword, stopword->lang);

	if (F != NULL) return 0;

	List->StopWord=(DPS_STOPWORD *)DpsRealloc(List->StopWord,(List->nstopwords+1)*sizeof(DPS_STOPWORD));
	if (List->StopWord == NULL) {
	  List->nstopwords = 0;
	  return 0;
	}
	List->StopWord[List->nstopwords].word = NULL;
	List->StopWord[List->nstopwords].uword = DpsUniDup(stopword->uword);
	List->StopWord[List->nstopwords].lang = (char*)DpsStrdup(stopword->lang?stopword->lang:"");
	List->StopWord[List->nstopwords].len = 0;
	List->StopWord[List->nstopwords].ulen = DpsUniLen(stopword->uword);
	List->nstopwords++;
	if (List->nstopwords > 1) DpsStopListSortForLast(List->StopWord, List->nstopwords);

	return(1);
}

void DpsStopListFree(DPS_STOPLIST *List){
	size_t i;
	DpsUniMatchListFree(&List->StopMatch);
	for(i=0;i<List->nstopwords;i++){
		DPS_FREE(List->StopWord[i].uword);
		DPS_FREE(List->StopWord[i].word);
		DPS_FREE(List->StopWord[i].lang);
	}
	DPS_FREE(List->StopWord);
	List->nstopwords=0;
}


__C_LINK int __DPSCALL DpsStopListLoad(DPS_ENV * Conf, const char *filename) {
        struct stat     sb;
	char *str, *cur_n = NULL, *data = NULL, *sharp;
	char * lasttok;
	DPS_STOPWORD stopword;
	DPS_CHARSET *cs = NULL, *uni_cs = DpsGetCharSet("sys-int");
	DPS_CONV cnv;
	DPS_UNIMATCH M;
	char * charset=NULL;
	dpsunicode_t *uwrd, *nfc;
	int             fd;
	char savebyte;
	char		*av[256];
	size_t		ac, i;

	if (stat(filename, &sb)) {
	  dps_strerror(NULL, 0, "Unable to stat stopword file '%s'", filename);
	  return DPS_ERROR;
	}
	if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
	  dps_strerror(NULL, 0, "Unable to open stopword file '%s'", filename);
	  return DPS_ERROR;
	}
	if ((data = (char*)DpsMalloc((size_t)(sb.st_size + 1))) == NULL) {
	  dps_snprintf(Conf->errstr,sizeof(Conf->errstr)-1, "Unable to alloc %d bytes", sb.st_size);
	  DpsClose(fd);
	  return DPS_ERROR;
	}
	if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
	  dps_strerror(NULL, 0, "Unable to read stopword file '%s'", filename);
	  DPS_FREE(data);
	  DpsClose(fd);
	  return DPS_ERROR;
	}
	data[sb.st_size] = '\0';
	str = data;
	cur_n = strchr(str, NL_INT);
	if (cur_n != NULL) {
	  cur_n++;
	  savebyte = *cur_n;
	  *cur_n = '\0';
	}
	DpsClose(fd);

	if ((uwrd = (dpsunicode_t*)DpsMalloc(sizeof(dpsunicode_t) * (Conf->WordParam.max_word_len + 128))) == NULL) {
	  DPS_FREE(data);
	  return DPS_ERROR;
	}

	bzero((void*)&stopword, sizeof(stopword));

	while(str != NULL) {
		if(!str[0]) goto loop_continue;
		if(str[0]=='#') goto loop_continue;
		sharp = strchr(str, (int)'#');
		while (sharp != NULL) 
		  if (*(sharp - 1) != '\\') {
		    *sharp = '\0';
		    break;
		  } else {
		    sharp = strchr(sharp + 1, (int)'#');
		  }
		
		if(!strncmp(str,"Charset:",8)){
			DPS_FREE(charset);
			charset = dps_strtok_r(str + 8, " \t\n\r", &lasttok, NULL);
			if(charset){
				charset = (char*)DpsStrdup(charset);
			}
		}else
		if(!strncmp(str,"Language:",9)){
			DPS_FREE(stopword.lang);
			stopword.lang = dps_strtok_r(str + 9, " \t\n\r", &lasttok, NULL);
			if(stopword.lang)stopword.lang = (char*)DpsStrdup(stopword.lang);
		}else
		if(!strncmp(str, "Match:", 6)) {
			if(!cs){
				if(!charset){
					sprintf(Conf->errstr,"No charset definition in stopwords file '%s'", filename);
					DPS_FREE(stopword.lang);
					DPS_FREE(uwrd);
					DPS_FREE(data);
					return DPS_ERROR;
				}else{
					cs=DpsGetCharSet(charset);
					if(!cs){
						sprintf(Conf->errstr,"Unknown charset '%s' in stopwords file '%s'", charset,filename);
						DPS_FREE(stopword.lang);
						DPS_FREE(charset);
						DPS_FREE(uwrd);
						DPS_FREE(data);
						return DPS_ERROR;
					}
					DpsConvInit(&cnv, cs, uni_cs, Conf->CharsToEscape, DPS_RECODE_HTML);
				}
			}
			ac = DpsGetArgs(str + 6, av, 255);
			DpsUniMatchInit(&M);
			M.match_type = DPS_MATCH_WILD;
			M.case_sense = 1;
			for(i = 0; i < ac ; i++) {
			  if(!strcasecmp(av[i], "case")) M.case_sense = 1;
			  else
			  if(!strcasecmp(av[i], "nocase")) M.case_sense = 0;
			  else
			  if(!strcasecmp(av[i], "regex")) M.match_type = DPS_MATCH_REGEX;
			  else
			  if(!strcasecmp(av[i], "regexp")) M.match_type = DPS_MATCH_REGEX;
			  else
			  if(!strcasecmp(av[i], "string")) M.match_type = DPS_MATCH_WILD;
			  else
			  if(!strcasecmp(av[i], "nomatch")) M.nomatch = 1;
			  else
			  if(!strcasecmp(av[i], "match")) M.nomatch = 0;
			  else{
			    char		err[120] = "";
			
			    M.arg = "stopword";

			    DpsConv(&cnv, (char*)uwrd, sizeof(dpsunicode_t) * (Conf->WordParam.max_word_len + 127), 
				    av[i], dps_strlen(av[i]) + 1);
			    uwrd[Conf->WordParam.max_word_len] = '\0';
			    nfc = DpsUniNormalizeNFC(NULL, uwrd);

			    M.pattern = nfc;
			
			    if(DPS_OK != DpsUniMatchListAdd(NULL, &Conf->StopWords.StopMatch, &M, err, sizeof(err), 0)) {
				dps_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "%s", err);
				DPS_FREE(charset);
				DPS_FREE(stopword.lang);
				DPS_FREE(uwrd);
				DPS_FREE(data);
				return DPS_ERROR;
			    }
			    DPS_FREE(nfc);
			  }
			}

		}else
		  if((stopword.word = dps_strtok_r(str, "\t\n\r", &lasttok, NULL))) {
			
			if(!cs){
				if(!charset){
					sprintf(Conf->errstr,"No charset definition in stopwords file '%s'", filename);
					DPS_FREE(stopword.lang);
					DPS_FREE(uwrd);
					DPS_FREE(data);
					return DPS_ERROR;
				}else{
					cs=DpsGetCharSet(charset);
					if(!cs){
						sprintf(Conf->errstr,"Unknown charset '%s' in stopwords file '%s'", charset,filename);
						DPS_FREE(stopword.lang);
						DPS_FREE(charset);
						DPS_FREE(uwrd);
						DPS_FREE(data);
						return DPS_ERROR;
					}
					DpsConvInit(&cnv, cs, uni_cs, Conf->CharsToEscape, DPS_RECODE_HTML);
				}
			}
			
			DpsConv(&cnv, (char*)uwrd, sizeof(dpsunicode_t) * Conf->WordParam.max_word_len, 
				stopword.word, dps_strlen(stopword.word) + 1);
			uwrd[Conf->WordParam.max_word_len] = '\0';
			nfc = DpsUniNormalizeNFC(NULL, uwrd);
			stopword.uword = nfc;
			DpsStopListAdd(&Conf->StopWords, &stopword);
			DPS_FREE(nfc);

		}
	loop_continue:
		str = cur_n;
		if (str != NULL) {
		  *str = savebyte;
		  cur_n = strchr(str, NL_INT);
		  if (cur_n != NULL) {
		    cur_n++;
		    savebyte = *cur_n;
		    *cur_n = '\0';
		  }
		}
	}
	DPS_FREE(data);
	DpsStopListSort(&Conf->StopWords);
	DPS_FREE(stopword.lang);
	DPS_FREE(charset);
	DPS_FREE(uwrd);
	return DPS_OK;
}
