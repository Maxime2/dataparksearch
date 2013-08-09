/* Copyright (C) 2005-2007 Datapark corp. All rights reserved.

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
#include "php.h"
#include "php_globals.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "php_ini.h"
#ifdef ZTS
#include "TSRM.h"
#endif

#include "dpsearch.h"
#include "dps_indexertool.h"
/*#include "dps_utils.h"
#include "dps_mutex.h"
#include "dps_agent.h"
#include "dps_env.h"*/

/* True globals, no need for thread safety */
static int le_agent, le_res;

extern zend_module_entry dpsearch_module_entry;

#define dpsearch_module_ptr &dpsearch_module_entry

PHP_MINIT_FUNCTION(dpsearch);
PHP_MSHUTDOWN_FUNCTION(dpsearch);
PHP_RINIT_FUNCTION(dpsearch);
PHP_RSHUTDOWN_FUNCTION(dpsearch);
PHP_MINFO_FUNCTION(dpsearch);

PHP_FUNCTION(dps_version);
PHP_FUNCTION(dps_agent_init);
/*PHP_FUNCTION(dpsearch_setoption);
PHP_FUNCTION(dpsearch_getoption);
PHP_FUNCTION(dpsearch_connect);
PHP_FUNCTION(dpsearch_pconnect);
PHP_FUNCTION(dpsearch_query);
PHP_FUNCTION(dpsearch_unbuffered_query);
PHP_FUNCTION(dpsearch_result_data);
PHP_FUNCTION(dpsearch_fetch_urlset);
PHP_FUNCTION(dpsearch_fetch_url);
PHP_FUNCTION(dpsearch_url_data);
PHP_FUNCTION(dpsearch_fetch_cloneurls);
PHP_FUNCTION(dpsearch_fetch_cached_page);
PHP_FUNCTION(dpsearch_cached_page_data);
PHP_FUNCTION(dpsearch_passthru_cached_page);
PHP_FUNCTION(dpsearch_close);
PHP_FUNCTION(dpsearch_error);
PHP_FUNCTION(dpsearch_errno);
*/

ZEND_BEGIN_MODULE_GLOBALS(dpsearch)
	long default_link;
	long num_links,num_persistent;
	long max_links,max_persistent;
	long allow_persistent;
	long default_port;
	char *default_host;
	char *connect_error;
	long connect_errno;
ZEND_END_MODULE_GLOBALS(dpsearch)

#ifdef ZTS
# define DPSearchG(v) TSRMG(dpsearch_globals_id, zend_dpsearch_globals *, v)
#else
# define DPSearchG(v) (dpsearch_globals.v)
#endif

/***************************************************************************/
/* 
 */
function_entry dpsearch_functions[] = {
	PHP_FE(dps_version,		NULL)
/*	PHP_FE(dpsearch_setoption,		NULL)
	PHP_FE(dpsearch_getoption,		NULL)
	PHP_FE(dpsearch_connect,		NULL)
	PHP_FE(dpsearch_pconnect,		NULL)
	PHP_FE(dpsearch_query,			NULL)
	PHP_FE(dpsearch_unbuffered_query,	NULL)
	PHP_FE(dpsearch_result_data,		NULL)
	PHP_FE(dpsearch_fetch_urlset,		NULL)
	PHP_FE(dpsearch_fetch_url,		NULL)
	PHP_FE(dpsearch_url_data,		NULL)
	PHP_FE(dpsearch_fetch_cloneurls,	NULL)
	PHP_FE(dpsearch_fetch_cached_page,	NULL)
	PHP_FE(dpsearch_cached_page_data,	NULL)
	PHP_FE(dpsearch_passthru_cached_page,	NULL)
	PHP_FE(dpsearch_close,			NULL)
	PHP_FE(dpsearch_error,			NULL)
	PHP_FE(dpsearch_errno,			NULL)*/
	{NULL, NULL, NULL}
};
/* */

/* 
 */
zend_module_entry dpsearch_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"dpsearch",                      /* extension name */
	dpsearch_functions,              /* extension function list */
	ZEND_MODULE_STARTUP_N(dpsearch), /* extension-wide startup function */
	PHP_MSHUTDOWN(dpsearch),         /* extension-wide shutdown function */
	PHP_RINIT(dpsearch),             /* per-request startup function */
	PHP_RSHUTDOWN(dpsearch),         /* per-request shutdown function */
	PHP_MINFO(dpsearch),             /* information function */
#if ZEND_MODULE_API_NO >= 20010901
	NO_VERSION_YET,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* */

ZEND_DECLARE_MODULE_GLOBALS(dpsearch)

ZEND_GET_MODULE(dpsearch)  /* this is only for loadable module version */

/**/

static void _dps_agent_free(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
	DPS_AGENT * Agent = (DPS_AGENT *)rsrc->ptr;
	DpsEnvFree(Agent->Conf);
	DpsAgentFree(Agent);
}


static void _dps_res_free(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
	DPS_RESULT * Res = (DPS_RESULT *)rsrc->ptr;

	DpsResultFree(Res);
}

PHP_MINIT_FUNCTION(dpsearch) {
  DpsInit(0, NULL, NULL);

#ifdef WITH_HTTPS
  SSL_library_init();
  SSL_load_error_strings(); 
#endif
	
  DpsInitMutexes();

  le_agent = zend_register_list_destructors_ex(_dps_agent_free, NULL, "DataparkSearch agent", module_number);
  le_res = zend_register_list_destructors_ex(_dps_res_free, NULL, "DataparkSearch result", module_number);

  REGISTER_LONG_CONSTANT("DPS_FLAG_REINDEX", DPS_FLAG_REINDEX, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_SORT_EXPIRED", DPS_FLAG_SORT_EXPIRED, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_SORT_HOPS", DPS_FLAG_SORT_HOPS, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_ADD_SERV", DPS_FLAG_ADD_SERV, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_SPELL", DPS_FLAG_SPELL, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_LOAD_LANGAMP", DPS_FLAG_LOAD_LANGMAP, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_SORT_SEED", DPS_FLAG_SORT_SEED, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_ADD_SERVURL", DPS_FLAG_ADD_SERVURL, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("DPS_FLAG_UNOCON", DPS_FLAG_UNOCON, CONST_CS | CONST_PERSISTENT);

  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(dpsearch) {
  DpsDestroyMutexes();
  return SUCCESS;
}


PHP_RINIT_FUNCTION(dpsearch) {
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(dpsearch) {
	return SUCCESS;
}

DLEXPORT PHP_MINFO_FUNCTION(dpsearch) {
	
	php_info_print_table_start();
	php_info_print_table_row(2, "DataparkSearch Support", "enabled" );
	
	php_info_print_table_row(2, "DataparkSearch version", DpsVersion() );
	php_info_print_table_end();
}

/**/

PHP_FUNCTION(dps_version) {
  RETURN_STRING((char*) DpsVersion(), 1);
}

/* {{ proto int dps_agent_init(long flags[, string config_file])
    alloc DataparkSearch agent */
PHP_FUNCTION(dps_agent_init) {
  DPS_ENV   * Env = DpsEnvInit(NULL);
  DPS_AGENT * Agent;
  pval **yy;
  char *filename = NULL;
  long lflags = 0;
  int rc = DPS_OK;

  if (Env == NULL) {
    RETURN_FALSE;
  }

  Agent = DpsAgentInit(NULL, Env, 0);
  if (Agent == NULL) {
    RETURN_FALSE;
  }

  switch(ZEND_NUM_ARGS()) {
  case 2: 
    if(zend_get_parameters_ex(2, &yy) == FAILURE) {
      RETURN_FALSE;
    }
    convert_to_string_ex(yy);
    filename = Z_STRVAL_PP(yy);
    /* no break by design */
  case 1:
    if(zend_get_parameters_ex(1, &yy) == FAILURE) {
      RETURN_FALSE;
    }
    convert_to_long_ex(yy);
    lflags = Z_LVAL_PP(yy);

    Agent->flags = Env->flags = lflags;

    if (filename != NULL) {

      if (DPS_OK == (rc = DpsEnvLoad(Agent, filename, lflags))){
	rc = (Agent->flags & DPS_FLAG_UNOCON) ? (Agent->Conf->dbl.nitems == 0) : (Agent->dbl.nitems == 0);
          if (rc) {
               php_error(E_WARNING, "Error: '%s': No required DBAddr commands were specified", filename);
               rc= DPS_ERROR;
          } else rc = DPS_OK;
     }

     if(DPS_OK != rc) {
       DpsAgentFree(Agent);
       DpsEnvFree(Env);
       RETURN_FALSE;
     }
    }

    DpsOpenLog("php_module", Env, log2stderr);
/*    DpsSigHandlersInit(Agent);  for dps_indexing_start */
  
    break;
  default:
    WRONG_PARAM_COUNT;
    break;
  }
  ZEND_REGISTER_RESOURCE(return_value, Agent, le_agent);
}
/* }}} */
