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

#include "dps_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "dps_guesser.h"
#include "dps_utils.h"
#include "dps_unicode.h"

#define DPS_OK			0
#define DPS_ERROR		1

/*
#define DEBUG_GUESSER
*/

/***************************************************************/

static void DpsPrintLangMap(DPS_LANGMAP * map){
     double ratio;
     size_t i, minv;
	
	printf("#\n");
	printf("#\n");
	printf("#\n");
	printf("\n");
	printf("Language: %s\n",map->lang);
	printf("Charset:  %s\n",map->charset);
	printf("\n");
	printf("\n");
	
	printf("Length: %d\n", DPS_LM_MAXGRAM1);
	DpsSort(map->memb3, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), &DpsLMcmpCount);
	minv = (map->memb3[DPS_LM_TOPCNT - 1].count > 8000) ? 8000 : map->memb3[DPS_LM_TOPCNT - 1].count;
	ratio = ((double) map->memb3[DPS_LM_TOPCNT - 1].count) / minv;
	if (minv > 0 && ratio > 0.0) {
	  for(i = 0; i < DPS_LM_TOPCNT; i++) {
	    map->memb3[i].count = (size_t) ((double)map->memb3[i].count / ratio);
	  }
	}
	for(i = 0; i < DPS_LM_TOPCNT; i++) {
		if(!map->memb3[i].count)break;
		printf("%03x\t%d\n", (unsigned int)map->memb3[i].index, (int)map->memb3[i].count);
	}

	printf("Length: %d\n", (int)DPS_LM_MAXGRAM2);
	DpsSort(map->memb6, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), &DpsLMcmpCount);
	minv = (map->memb6[DPS_LM_TOPCNT - 1].count > 8000) ? 8000 : map->memb6[DPS_LM_TOPCNT - 1].count;
	ratio = ((double) map->memb6[DPS_LM_TOPCNT - 1].count) / minv;
	if (minv > 0 && ratio > 0.0) {
	  for(i = 0; i < DPS_LM_TOPCNT; i++) {
	    map->memb6[i].count = (size_t) ((double)map->memb6[i].count / ratio);
	  }
	}
	for(i = 0; i < DPS_LM_TOPCNT; i++) {
		if(!map->memb6[i].count)break;
		printf("%03x\t%d\n", (unsigned int)map->memb6[i].index, (int)map->memb6[i].count);
	}
	printf("#\n");
}

static void usage(void){
	printf("dpguesser %s-%s\n\n", PACKAGE, VERSION);
	printf("To guess use:\n\n");
	printf("\tdpguesser [-n maxhits]< FILENAME\n\n");
	printf("To create new language map use:\n\n");
	printf("\tdpguesser -p -c charset -l language < FILENAME\n");
} 

int main(int argc, char ** argv){
	int ch;
	int verbose=0;
	int print=0;
	size_t n=1000, t, len;
	char buf[1024]="";
	DPS_LANGMAPLIST env;
	DPS_LANGMAP mchar;
	char * charset=NULL;
	char * lang=NULL;

	while((ch=getopt(argc,argv,"pv?c:l:n:"))!=-1){
		switch(ch){
			case 'n':
				n=atoi(optarg);
				break;
			case 'c':
				charset=optarg;
				break;
			case 'l':
				lang=optarg;
				break;
			case 'p':
				print++;
				break;
			case 'v':
				verbose++;
				break;
			case '?':
			default:
				usage();
				exit(1);
		}
	}
	argc-=optind;
	argv+=optind;

	/* Init structures */
	memset(&env,0,sizeof(env));
	memset(&mchar,0,sizeof(mchar));
	for (t = 0; t <= DPS_LM_HASHMASK; t++) mchar.memb3[t].index = mchar.memb6[t].index = t;

	if(!print){
		/* Load all available lang ngram maps */
		if(verbose){
			fprintf(stderr,"Loading language maps from '%s'\n", DPS_CONF_DIR DPSSLASHSTR "langmap");
		}
		if (DpsLoadLangMapList(&env, DPS_CONF_DIR DPSSLASHSTR "langmap") != DPS_OK) {
			return 1;
		}

		if(verbose){
			fprintf(stderr, "%d maps found\n", (int)env.nmaps);
		}
	}
	
	/* Add each STDIN line statistics */
	while( (len = fread(buf, 1, sizeof(buf), stdin)) > 0) {
	  DpsBuildLangMap(&mchar, buf, len, 0, 1);
	}


#ifdef DEBUG_GUESSER	
	{
	  float count0 = 0; int i;
	  for (i = 0; i < DPS_LM_HASHMASK; i++) {
	    if (mchar.memb3[i].count == 0) count0 += 1.0;
	    if (mchar.memb6[i].count == 0) count0 += 1.0;
	  }			
	  fprintf(stderr, "Count 0: %f, %.2f\n", count0, count0 * 50 / DPS_LM_HASHMASK);
	}
#endif

	if(print){
		/* Display built langmap */
		if(!charset){
			fprintf(stderr,"You must specify charset using -c\n");
		}else
		if(!lang){
			fprintf(stderr,"You must specify language using -l\n");
		}else{
			mchar.lang = (char*)DpsStrdup(lang);
			mchar.charset = (char*)DpsStrdup(charset);
			DpsPrintLangMap(&mchar);
		}
	}else{
		size_t i;
		DPS_MAPSTAT * mapstat;
		size_t InfMiss = DPS_LM_TOPCNT * DPS_LM_TOPCNT + 1;
		size_t InfHits = 2 * DPS_LM_TOPCNT;
		
		/* Prepare map to comparison */
		DpsPrepareLangMap(&mchar);

#ifdef DEBUG_GUESSER	
		DpsPrintLangMap(&mchar);
#endif

		/* Allocate memory for comparison statistics */
		mapstat = (DPS_MAPSTAT *)DpsMalloc((env.nmaps + 1) * sizeof(DPS_MAPSTAT));
		if (mapstat == NULL) {
		  fprintf(stderr, "Can't alloc %d bytes at %s:%d\n", (int)env.nmaps * sizeof(DPS_MAPSTAT), __FILE__, __LINE__);
		  exit(1);
		}

		/* Calculate each lang map        */
		/* correlation with text          */
		/* and store in mapstat structure */

		for(i = 0; i < env.nmaps; i++){
		  DpsCheckLangMap(&env.Map[i], &mchar, &mapstat[i], InfMiss, InfHits);
			mapstat[i].map = &env.Map[i];
		}

		/* Sort statistics in quality order */
		if (env.nmaps > 1) DpsSort(mapstat, env.nmaps, sizeof(DPS_MAPSTAT), &DpsLMstatcmp);


		/* Display results. Best language is shown first. */
		for(i = 0; (i < env.nmaps) && (i < n); i++){
			printf("%dh %dm\t%s\t%s\n", (int)mapstat[i].hits, (int)mapstat[i].miss, mapstat[i].map->lang, mapstat[i].map->charset);
		}

		/* Free variables */
		DpsFree(mapstat);
	}
	
	DpsLangMapListFree(&env);

	return 0;
}

