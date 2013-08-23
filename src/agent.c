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
#include "dps_utils.h"
#include "dps_agent.h"
#include "dps_hrefs.h"
#include "dps_result.h"
#include "dps_xmalloc.h"
#include "dps_log.h"
#include "dps_host.h"
#include "dps_db.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_socket.h"
#include "dps_robots.h"
#include "dps_template.h"
#include "dps_uniconv.h"
#include "dps_cookies.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif


__C_LINK int DpsAgentStoredConnect(DPS_AGENT *Indexer) {
  size_t i;
  DPS_ENV *Env = Indexer->Conf;
  char port_str[16];
  struct sockaddr_in dps_addr;
  unsigned char *p = (unsigned char*)&dps_addr.sin_port;
  unsigned int ip[2];

  if (Indexer->Demons.Demon == NULL) {
    Indexer->Demons.nitems = Indexer->Conf->dbl.nitems;
    Indexer->Demons.Demon = (DPS_DEMONCONN*)DpsXmalloc(Indexer->Demons.nitems * sizeof(DPS_DEMONCONN) + 1);
    if (Indexer->Demons.Demon == NULL) {
      DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc at %s:%d", __FILE__, __LINE__);
      return DPS_ERROR;
    }
  }

  for (i = 0; i < Env->dbl.nitems; i++) {
    if (Env->dbl.db[i].stored_addr.sin_port != 0 && Indexer->Demons.Demon[i].stored_sd == 0) {
      if((Indexer->Demons.Demon[i].stored_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	dps_strerror(Indexer, DPS_LOG_ERROR, "StoreD ERR socket_sd");
	return DPS_ERROR;
      }
      if((Indexer->Demons.Demon[i].stored_rv = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	dps_strerror(Indexer, DPS_LOG_ERROR, "StoreD ERR socket_rv");
	return DPS_ERROR;
      }

      DpsSockOpt(Indexer, Indexer->Demons.Demon[i].stored_sd);
      DpsSockOpt(Indexer, Indexer->Demons.Demon[i].stored_rv);

      if(connect(Indexer->Demons.Demon[i].stored_sd, (struct sockaddr *)&Env->dbl.db[i].stored_addr, 
		 sizeof(Env->dbl.db[i].stored_addr)) == -1) {
	dps_strerror(Indexer, DPS_LOG_ERROR, "StoreD ERR connect");
	DpsLog(Indexer, DPS_LOG_ERROR, "StoreD ERR connect to %s", inet_ntoa(Env->dbl.db[i].stored_addr.sin_addr));
	return DPS_ERROR;
      }

      /* revert connection */

      if (sizeof(port_str) != DpsRecvall(Indexer->Demons.Demon[i].stored_sd, port_str, sizeof(port_str), 360)) {
	dps_strerror(Indexer, DPS_LOG_ERROR, "StoreD ERR receiving port data");
	return DPS_ERROR;
      }
      dps_addr = Env->dbl.db[i].stored_addr;
      dps_addr.sin_port = 0;
      sscanf(port_str, "%d,%d", ip, ip + 1);
      p[0] = (unsigned char)(ip[0] & 255);
      p[1] = (unsigned char)(ip[1] & 255);

      DpsLog(Indexer, DPS_LOG_EXTRA, "Stored @ [%s] PORT: %s, decimal:%d", 
	     inet_ntoa(Env->dbl.db[i].stored_addr.sin_addr), port_str, ntohs(dps_addr.sin_port));

      if(connect(Indexer->Demons.Demon[i].stored_rv, (struct sockaddr *)&dps_addr, sizeof(dps_addr)) == -1) {
	dps_strerror(Indexer, DPS_LOG_ERROR, "StoreD ERR revert connect");
	DpsLog(Indexer, DPS_LOG_ERROR, "StoreD ERR revert connect to %s:%d", inet_ntoa(dps_addr.sin_addr), ntohs(dps_addr.sin_port));
	return DPS_ERROR;
      }

/**********************************/

    }
  }
  return DPS_OK;
}

DPS_DBLIST * DpsAgentDBLSet(DPS_AGENT *result, DPS_ENV *Env) {
  DPS_DBLIST *DBL = NULL;
  size_t i;
  if (Env->flags & DPS_FLAG_UNOCON) {
    DBL = &Env->dbl;
  } else {
    DBL = &result->dbl;
    for (i = 0 ; i < Env->dbl.nitems; i++) {
      if (DPS_OK != DpsDBListAdd(DBL, Env->dbl.db[i].DBADDR, Env->dbl.db[i].open_mode)) {
	DBL = NULL;
	break;
      }
    }
  }
  return DBL;
}


__C_LINK DPS_AGENT * __DPSCALL DpsAgentInit(DPS_AGENT *result, DPS_ENV * Env, int handle){
  size_t i, z;
  DPS_DBLIST *DBL;
  char port_str[16];
  struct sockaddr_in dps_addr;
  unsigned char *p = (unsigned char*)&dps_addr.sin_port;
  unsigned int ip[2];
  DPS_CHARSET *unics, *loccs;
#ifdef HAVE_ASPELL
  DPS_CHARSET *utfcs;
#endif
#ifdef WITH_TRACE
  char filename[PATH_MAX];
#endif
	if(!result){
		result=(DPS_AGENT*)DpsMalloc(sizeof(DPS_AGENT));
		if (result == NULL) return NULL;
		bzero((void*)result, sizeof(*result));
		result->freeme=1;
	}else{
		bzero((void*)result, sizeof(*result));
	}

	if ((result->Locked = (int*)DpsXmalloc(DpsNsems * sizeof(int) + 1)) == NULL) {
	      DpsAgentFree(result);
	      return NULL;
	}
	result->now = result->start_time = time(NULL);
	result->Conf = Env;
	result->handle = handle; /* Handle is used in multi-threaded version */
	result->Flags = Env->Flags;
	result->flags = Env->flags;
	result->WordParam = Env->WordParam;
	result->action = DPS_OK;
	result->SpellLang = -1;
#ifdef HAVE_PTHREAD
	result->seed = (unsigned int)(result->now ^ (result->now << 2));
#endif
	result->LangMap = (DPS_LANGMAP*)DpsMalloc(sizeof(DPS_LANGMAP));
	if (result->LangMap == NULL) {
	  DpsAgentFree(result);
	  return NULL;
	}
	bzero((void*)result->LangMap, sizeof(DPS_LANGMAP));

	DpsVarListAddLst(&result->Vars, &Env->Vars, NULL, "*");

	DBL = DpsAgentDBLSet(result, Env);
	if (NULL == DBL) {
	  DpsAgentFree(result);
	  return NULL;
	}

/* Init demons connections */
	if ((result->Demons.nitems = DBL->nitems) > 0 ) {
	
	  result->Demons.Demon = (DPS_DEMONCONN*)DpsXmalloc(result->Demons.nitems * sizeof(DPS_DEMONCONN) + 1);
	  if (result->Demons.Demon == NULL) {
	    DpsAgentFree(result);
	    return NULL;
	  }
	  for (i = 0; i < DBL->nitems; i++) {

	    if (DBL->db[i].stored_addr.sin_port != 0) {
	      if((result->Demons.Demon[i].stored_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dps_strerror(NULL, DPS_LOG_ERROR, "StoreD ERR socket");
		DpsAgentFree(result);
		return NULL;
	      }
	      if((result->Demons.Demon[i].stored_rv = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dps_strerror(NULL, DPS_LOG_ERROR, "StoreD ERR socket_rv");
		DpsAgentFree(result);
		return NULL;
	      }
  
	      DpsSockOpt(NULL, result->Demons.Demon[i].stored_sd);
	      DpsSockOpt(NULL, result->Demons.Demon[i].stored_rv);

	      if(connect(result->Demons.Demon[i].stored_sd, (struct sockaddr *)&DBL->db[i].stored_addr, 
			 sizeof(DBL->db[i].stored_addr)) == -1) {
		dps_strerror(NULL, DPS_LOG_ERROR, "StoreD ERR connect");
		fprintf(stderr, "StoreD ERR connect to %s\n", inet_ntoa(DBL->db[i].stored_addr.sin_addr));
		DpsAgentFree(result);
		return NULL;
	      }
	      /* revert connection */

	      if (sizeof(port_str) != DpsRecvall(result->Demons.Demon[i].stored_sd, port_str, sizeof(port_str), 360)) {
		dps_strerror(NULL, DPS_LOG_ERROR, "StoreD ERR receiving port data");
		DpsAgentFree(result);
		return NULL;
	      }
	      dps_addr = Env->dbl.db[i].stored_addr;
	      dps_addr.sin_port = 0;
	      sscanf(port_str, "%d,%d", ip, ip + 1);
	      p[0] = (unsigned char)(ip[0] & 255);
	      p[1] = (unsigned char)(ip[1] & 255);

/*	      fprintf(stderr, "[%s] PORT: %s, decimal:%d\n", 
		      inet_ntoa(Env->dbl.db[i].stored_addr.sin_addr), port_str, ntohs(dps_addr.sin_port));*/

	      if(connect(result->Demons.Demon[i].stored_rv, (struct sockaddr *)&dps_addr, sizeof(dps_addr)) == -1) {
		dps_strerror(NULL, DPS_LOG_ERROR, "StoreD ERR revert connect to %s:%d", inet_ntoa(dps_addr.sin_addr), ntohs(dps_addr.sin_port));
		DpsAgentFree(result);
		return NULL;
	      }
/**********************/


	    }

	    if (DBL->db[i].cached_addr.sin_port != 0) {
	      
	      if((result->Demons.Demon[i].cached_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dps_strerror(NULL, DPS_LOG_ERROR, "CacheD ERR socket_sd");
		DpsAgentFree(result);
		return NULL;
	      }
  
	      if((result->Demons.Demon[i].cached_rv = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dps_strerror(NULL, DPS_LOG_ERROR, "CacheD ERR socket_rv");
		DpsAgentFree(result);
		return NULL;
	      }
  
	      DpsSockOpt(NULL, result->Demons.Demon[i].cached_sd);
	      DpsSockOpt(NULL, result->Demons.Demon[i].cached_rv);

	      for (z = 0; z < 5; z++) {
		if(connect(result->Demons.Demon[i].cached_sd, (struct sockaddr *)&DBL->db[i].cached_addr, 
			   sizeof(DBL->db[i].cached_addr)) == 0) {
		  break;
		}
	      }
	      if (z == 5) {
		dps_strerror(NULL, DPS_LOG_ERROR, "CacheD ERR connect to %s", inet_ntoa(DBL->db[i].cached_addr.sin_addr));
		DpsAgentFree(result);
		return NULL;
	      }


	      /* revert connection */


	      if (sizeof(port_str) != DpsRecvall(result->Demons.Demon[i].cached_sd, port_str, sizeof(port_str), 360)) {
		dps_strerror(NULL, DPS_LOG_ERROR, "CacheD ERR receiving port data");
		DpsAgentFree(result);
		return NULL;
	      }
	      dps_addr = Env->dbl.db[i].cached_addr;
	      dps_addr.sin_port = 0;
	      sscanf(port_str, "%d,%d", ip, ip + 1);
	      p[0] = (unsigned char)(ip[0] & 255);
	      p[1] = (unsigned char)(ip[1] & 255);

/*	      fprintf(stderr, "[%s] PORT: %s, decimal:%d\n", 
		      inet_ntoa(Env->dbl.db[i].cached_addr.sin_addr), port_str, ntohs(dps_addr.sin_port));*/

	      if(connect(result->Demons.Demon[i].cached_rv, (struct sockaddr *)&dps_addr, sizeof(dps_addr)) == -1) {
		dps_strerror(NULL, DPS_LOG_ERROR, "CacheD ERR revert connect to %s:%d", inet_ntoa(dps_addr.sin_addr), ntohs(dps_addr.sin_port));
		DpsAgentFree(result);
		return NULL;
	      }

/*********************/

/*********************/

	    }
	  }
	}

	loccs = result->Conf->lcs;
	if (!loccs) loccs = DpsGetCharSet(DpsVarListFindStr(&Env->Vars, "LocalCharset", "iso-8859-1"));
	unics = DpsGetCharSet("sys-int");

	DpsConvInit(&result->uni_lc, unics, loccs, Env->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&result->lc_uni, loccs, unics, Env->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&result->lc_uni_text, loccs, unics, Env->CharsToEscape, DPS_RECODE_TEXT);
	
#ifdef HAVE_ASPELL
	utfcs = DpsGetCharSet("UTF-8");
	DpsConvInit(&result->utf_uni, utfcs, unics, Env->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&result->utf_lc, utfcs, loccs, Env->CharsToEscape, DPS_RECODE_HTML);
	DpsConvInit(&result->uni_utf, unics, utfcs, Env->CharsToEscape, DPS_RECODE_HTML);

	result->aspell_config = new_aspell_config();
	aspell_config_replace(result->aspell_config, "encoding", "utf-8");
	aspell_config_replace(result->aspell_config, "sug-mode", "slow");
	aspell_config_replace(result->aspell_config, "ignore-case", "true");
	aspell_config_replace(result->aspell_config, "normalize", "false");
	aspell_config_replace(result->aspell_config, "size", "10");
	aspell_config_replace(result->aspell_config, "home-dir", DpsVarListFindStr(&result->Conf->Vars,"EtcDir",DPS_CONF_DIR));
	aspell_config_replace(result->aspell_config, "use-other-dicts", "true");
#endif
#if defined(HAVE_LIBARES) || defined (HAVE_LIBCARES)
	{
	  int status =  ares_init(&result->channel);
	  if (status != ARES_SUCCESS) {

	    fprintf(stderr, "ares_init: %s", ares_strerror(status
#if defined(HAVE_LIBARES)
							   , NULL
#endif
							   ));
	    DpsAgentFree(result);
	    return NULL;
	  }
	}
#endif
#ifdef WITH_TRACE
	dps_snprintf(filename, sizeof(filename), "/tmp/dps_agent.%d.trace", handle);
	result->TR = fopen(filename, "w");
#endif
	return(result);
}


__C_LINK void __DPSCALL DpsAgentFree(DPS_AGENT *Indexer){
  size_t i;

	if(!Indexer)return;
#ifdef HAVE_ASPELL
	delete_aspell_config(Indexer->aspell_config);
#endif
#ifdef WITH_TRACE
	if (Indexer->TR) fclose(Indexer->TR);
#endif
#if defined(HAVE_LIBARES) || defined(HAVE_LIBCARES)
	if (Indexer->channel != 0) ares_destroy(Indexer->channel);
#endif
	DpsDBListFree(&Indexer->dbl);
	DpsResultFree(&Indexer->Indexed);
	DpsHrefListFree(&Indexer->Hrefs);
	DpsHostListFree(&Indexer->Hosts);
	DpsTemplateFree(&Indexer->tmpl);
	DpsTemplateFree(&Indexer->st_tmpl);
	DpsVarListFree(&Indexer->Vars);
	DpsRobotListFree(Indexer, &Indexer->Robots);
	DpsCookiesFree(&Indexer->Cookies);
	DPS_FREE(Indexer->Locked);
	DPS_FREE(Indexer->LangMap);
	for (i = 0; i < Indexer->loaded_limits; i++)
	  DPS_FREE(Indexer->limits[i].data);
	DPS_FREE(Indexer->limits);
/*	fprintf(stderr, "<%d> Indexer->Demons.nitems: %d   Indexer->Demons.Demon: %x\n", Indexer->handle,
		Indexer->Demons.nitems, Indexer->Demons.Demon);*/
	if (Indexer->Demons.Demon != NULL)
	  for(i = 0; i < Indexer->Demons.nitems; i++) {
/*	    fprintf(stderr, "Indexer->Demons.Demon[i].cached_sd: %d  Indexer->Demons.Demon[i].stored_sd: %d\n",
		    Indexer->Demons.Demon[i].cached_sd, Indexer->Demons.Demon[i].stored_sd);*/
	    if (Indexer->Demons.Demon[i].cached_sd) {
	      DPS_LOGD_CMD cmd;
	      cmd.stamp = Indexer->now;
	      cmd.url_id = 0;
	      cmd.cmd = DPS_LOGD_CMD_BYE;
	      cmd.nwords = 0;

	      DpsSend(Indexer->Demons.Demon[i].cached_sd, &cmd, sizeof(cmd), 0);
	      shutdown(Indexer->Demons.Demon[i].cached_sd, SHUT_RDWR);
	      dps_closesocket(Indexer->Demons.Demon[i].cached_sd);
	    }
	    if (Indexer->Demons.Demon[i].stored_sd) { 
	      const char *hello = "B\0";
	      DpsSend(Indexer->Demons.Demon[i].stored_sd, hello, 1, 0);
	      shutdown(Indexer->Demons.Demon[i].stored_sd, SHUT_RDWR);
	      dps_closesocket(Indexer->Demons.Demon[i].stored_sd);
	    }
	  }
	DPS_FREE(Indexer->Demons.Demon);
	Indexer->Demons.nitems = 0;
	for(i = 0; i < DPS_FINDURL_CACHE_SIZE; i++) DPS_FREE(Indexer->DpsFindURLCache[i]);
	for(i = 0; i < DPS_SERVERID_CACHE_SIZE; i++) DPS_FREE(Indexer->ServerIdCache[i].Match_Pattern);
	if(Indexer->freeme) DPS_FREE(Indexer);
}


__C_LINK void __DPSCALL DpsAgentSetAction(DPS_AGENT *Indexer,int action){
	Indexer->action = action;
}

