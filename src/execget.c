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
#include "dps_execget.h"
#include "dps_log.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <string.h>
#include <errno.h>

/*
#define DEBUG_EXECGET
*/

int DpsExecGet(DPS_AGENT * Indexer,DPS_DOCUMENT * Doc){
	int res;
	char * args;
	char cmdline[1024];
	FILE * f;
	DPS_URL *Url=&Doc->CurURL;

	Doc->Buf.buf[Doc->Buf.size = 0] = '\0';

#ifdef DEBUG_EXECGET
	fprintf(stderr, "URL: filename:%s| path:%s| specific:%s| query_string:%s| filename: %s|\n", 
		Url->filename, Url->path, Url->specific, Url->query_string, Url->filename);
#endif

	if((args = strchr(DPS_NULL2EMPTY(Url->query_string), '?'))) {
		args++;
	}
	sprintf(cmdline, "%s%s", DPS_NULL2EMPTY(Url->path), DPS_NULL2EMPTY(Url->filename));
	
	if(!strcmp(DPS_NULL2EMPTY(Url->schema), "exec")) {
		if(args){
			sprintf(DPS_STREND(cmdline)," \"%s\"",args);
		}
	}else
	if(!strcmp(DPS_NULL2EMPTY(Url->schema), "cgi")) {
		/* Non-parsed-headers CGI return HTTP status itself */
		if(strncasecmp(DPS_NULL2EMPTY(Url->filename), "nph-", 4)) {
			sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");
			Doc->Buf.size = dps_strlen(Doc->Buf.buf);
		}
		DpsSetEnv("QUERY_STRING",args?args:"");
		DpsSetEnv("REQUEST_METHOD","GET");
	}

#ifdef DEBUG_EXECGET
	fprintf(stderr,"cmd='%s'\n",cmdline);
#endif

	DpsLog(Indexer,DPS_LOG_DEBUG,"Starting program '%s'",cmdline);

	f=popen(cmdline,"r");

	/* Remove set variables */
	if(!strcmp(DPS_NULL2EMPTY(Url->schema), "cgi")) {
		DpsUnsetEnv("REQUEST_METHOD");
		DpsUnsetEnv("QUERY_STRING");
	}

	if(f){
		int fd,bytes;
		
		fd=fileno(f);
		if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.max_size + 1)) == NULL) {
		  return Doc->Buf.size = Doc->Buf.allocated_size = 0;
		}
		while((bytes = read(fd, Doc->Buf.buf + Doc->Buf.size, Doc->Buf.max_size - Doc->Buf.size))){
			Doc->Buf.size += bytes;
			Doc->Buf.buf[Doc->Buf.size] = '\0';
		}
		if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.size + 1)) == NULL) {
		  return Doc->Buf.size = Doc->Buf.allocated_size = 0;
		}
		Doc->Buf.allocated_size = Doc->Buf.size + 1;
		res=pclose(f);
	}else{
		int status;
		printf("error=%s\n",strerror(errno));
		switch(errno){
			case ENOENT: status=404;break; /* Not found */
			case EACCES: status=403;break; /* Forbidden*/
			default: status=500;
		}
		sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n\r\n",status,strerror(errno));
		Doc->Buf.size = dps_strlen(Doc->Buf.buf);
	}
#ifdef DEBUG_EXECGET
	fprintf(stderr,"Doc.buf='%s'\n",Doc->Buf.buf);
#endif
	return(Doc->Buf.size);
}

