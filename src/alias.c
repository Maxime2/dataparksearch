/* Copyright (C) 2003-2006 Datapark corp. All rights reserved.
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
#include "dps_alias.h"
#include "dps_match.h"
#include "dps_log.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


int DpsAliasProg(DPS_AGENT *Indexer,const char *alias_prog,const char *argument,char *res, size_t rsize){
	FILE		*aprog;
	char		*cmd;
	char		*arg = NULL;
	char		*ares=NULL;
	char		*args[1];
	const char	*a;
	char		*c;
	size_t          arg_len, cmd_len;
 	
	arg = (char*)DpsMalloc(arg_len = (2 * dps_strlen(argument) + 1));
	if (arg == NULL) return DPS_ERROR;
	cmd = (char*)DpsMalloc(cmd_len = (arg_len + 2 * dps_strlen(alias_prog) + 1));
	if (cmd == NULL) { DPS_FREE(arg); return DPS_ERROR; }


	/* Escape command argument */
	for(a=argument,c=arg;*a;a++,c++){
		switch(*a){
			case '\\':
			case '\'':
			case '\"':
				(*c)='\\';
				c++;
				break;
			default:
				break;
		}
		*c=*a;
	}
	*c='\0';
	args[0]=arg;
	
	DpsBuildParamStr(cmd, cmd_len, alias_prog, args, 1);
	aprog=popen(cmd,"r");
	
	DpsLog(Indexer, DPS_LOG_EXTRA, "Starting AliasProg: '%s'", cmd);
	
	if(aprog){
		ares=fgets(res,(int)rsize,aprog);
		res[rsize-1]='\0';
		pclose(aprog);
		if(!ares){
			DpsLog(Indexer,DPS_LOG_ERROR,"AliasProg didn't return result: '%s'",cmd);
			DPS_FREE(cmd); DPS_FREE(arg);
			return(DPS_ERROR);
		}
	}else{
		DpsLog(Indexer,DPS_LOG_ERROR,"Can't start AliasProg: '%s'",cmd);
		DPS_FREE(cmd); DPS_FREE(arg);
		return(DPS_ERROR);
	}
	if(ares[0]){
		ares += dps_strlen(ares);
		ares--;
		while((ares>=res)&&strchr(" \r\n\t",*ares)){
			*ares='\0';
			ares--;
		}
	}
	DPS_FREE(cmd); DPS_FREE(arg);
	return DPS_OK;
}
