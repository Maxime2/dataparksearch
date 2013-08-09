/* Copyright (C) 2004-2011 DataPark Ltd. All rights reserved.

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
/*
* parsedate.c
* author: Heiko Stoermer, innominate AG, stoermer@innominate.de
* 20000316
*
* 23 Aug 2000, modified by Kir <kir@sever.net>
*/

#include "dps_common.h"
#include "dps_parsedate.h"
#include "dps_xmalloc.h"
#include "dps_utils.h"
#include "dps_sdp.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define BUFF 20

static int get_month(char * month)
{
	if(strcmp(month,"Jan") == 0)
		return 1;
	else if(strcmp(month,"Feb") == 0)
		return 2;
	else if(strcmp(month,"Mar") == 0)
		return 3;
	else if(strcmp(month,"Apr") == 0)
		return 4;
	else if(strcmp(month,"May") == 0)
		return 5;
	else if(strcmp(month,"Jun") == 0)
		return 6;
	else if(strcmp(month,"Jul") == 0)
		return 7;
	else if(strcmp(month,"Aug") == 0)
		return 8;
	else if(strcmp(month,"Sep") == 0)
		return 9;
	else if(strcmp(month,"Oct") == 0)
		return 10;
	else if(strcmp(month,"Nov") == 0)
		return 11;
	else if(strcmp(month,"Dec") == 0)
		return 12;
	else
		return 0;
}

/* Returns allocated pointer to string 
 * containing the sql-conformant datetime type.
 *
 * Processes nntp datestrings of the following types:
 * 1: 17 Jun 1999 11:05:42 GMT
 * 2: Wed, 08 Mar 2000 13:52:43 +0100
 * 3: Tuesday, 28-Nov-95 9:15:26 GMT
 *
 * 23 Aug 2000, modified by Kir <kir@sever.net>
 * to support the (3) date format
 * Bug report was submitted by Nagy Erno <ned@elte.hu>
 */

char * DpsDateParse(const char * datestring) {
	
	/* buffers */
	char year  [BUFF] = "";
	char month [BUFF] = "";
	char date  [BUFF] = "";
	char timebuf[BUFF] = "";
	char * tokens[4];
	/* result */
	char * res = NULL;
	/* some ints for substring indices */
	SDPALIGN pos  = 0;
	size_t allocsize = 0;

	/* pointers for parsing */
	char * next = NULL;
	char * prev = NULL;
	size_t len = 0;
	/* working copy and copy-copy */
	char * copy = NULL;
	char * copyref = NULL;
	/* we need this for the different types */
	int offset = 0;
	char *tok, savec;
	/* initialize everythign nicely */
	tokens[0] = date;
	tokens[1] = month;
	tokens[2] = year;
	tokens[3] = timebuf;

	if(datestring != NULL && *datestring != '\0') {
		/* select string type */
		if((pos = (SDPALIGN)strchr(datestring,',')))
		{
			/* simply skip the first characters containing the weekday string */
			offset = (int) ( (SDPALIGN)pos - (SDPALIGN)datestring);	
			offset += 2; /* skip comma and whitespace */
		}
		copy = DpsMalloc(dps_strlen(datestring+offset)+1);
		if (copy == NULL) return "";
		dps_strcpy(copy, datestring + offset);
		copyref = copy; /* save the pointer for deallocation later */
	
		/* now we can start.*/
		/* split up the string*/
		pos = 0;
		prev = copy;
		next = dps_strtok_r(copy, " -", &tok, &savec);
		while(pos < 4)
		{
		        if((next = dps_strtok_r(NULL, " -", &tok, &savec)) == NULL) /* last element */
				len = dps_strlen(prev);
			else
				len = (size_t) (next-prev);
			if(len > BUFF) /* unexpected token length*/
				return NULL;
			dps_strncpy(tokens[pos], prev, len);
			/* preparations for the next run*/
			prev = next;
			++pos;
			 /*next = dps_strtok_r(NULL, " ", &tok, &savec);*/
		}


		/* o.k., this type of parsing is no fexible enough... :(*/
		/*
		dps_strncpy(date,datestring+offset,2);
		date[2] = 0;
		dps_strncpy(month,datestring+offset+3,3);
		month[3] = 0;
		dps_strncpy(year,datestring+offset+7,4); 
		year[4] = 0;
		dps_strncpy(timebuf,datestring+offset+12,8);	
		timebuf[8] = 0;
		*/
		
		if (dps_strlen(year)==2){
			/* We have two-digit year, need to convert to four-digit */
			year[2]=year[0];
			year[3]=year[1];

			/* Here we assume that 70 and up are 19xx,
			 * and 00-69 are 20xx years. --kir.
			 */
			if (year[0]>'6'){
				year[0]='1';
				year[1]='9';    
			}
			else{
				year[0]='2';
				year[1]='0';    
			}
		}

		/* we allocate 4 extra characters for separators and \0  */
		allocsize = dps_strlen(date)+dps_strlen(month)+dps_strlen(year)+dps_strlen(timebuf) + 5;
		res = DpsMalloc(allocsize);
		if (res == NULL) {
		  DPS_FREE(copyref);
		  return "";
		}
		dps_snprintf(res,allocsize,"%s-%02i-%02i %s",year,get_month(month),atoi(date),timebuf);
		res[allocsize-1] = '\0';
		/* clean up nicely. we dont want memory leaks within 100 lines...*/
		DPS_FREE(copyref);
	} else {
		if ((res = DpsMalloc(20)))
			sprintf(res, "1970-01-01 00:01");
		else return "";
	}
	return res;
}
