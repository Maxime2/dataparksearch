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

/*#define DEBUG_MEM 1*/
/*#define MALLOC_STATS*/

#if defined MALLOC_STATS
extern void malloc_stats(void);
#endif

#include "dpsearch.h"
#include "dps_sqldbms.h"
#include "dps_uniconv.h"
#include "dps_cache.h"
#include "dps_indexertool.h"
#include "dps_xmalloc.h"
#include "dps_host.h"
#include "dps_charsetutils.h"
#include "dps_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <sys/stat.h>
#include <locale.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef CHASEN
#include <chasen.h>
#endif

#ifdef MECAB
#include <mecab.h>
#endif

#ifdef DEBUG_MEM
#include <sys/mman.h>
#endif


/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

       dps_uint8 flags    = 0; /* For indexer            */
volatile int total_threads  = 0; /* Total threads number         */
volatile int sleep_threads  = 0; /* Number of sleepping threads      */
       int max_index_time = -1;
       int max_index_size = -1;
       int cfg_url_number = 0x7FFFFFFF;
static char cname[PATH_MAX + 1024] = "";
static dps_uint8 add_servers = DPS_FLAG_ADD_SERV;
static dps_uint8 add_server_urls = DPS_FLAG_ADD_SERVURL;
static dps_uint8 load_langmaps = DPS_FLAG_LOAD_LANGMAP;
static dps_uint8 load_spells = DPS_FLAG_SPELL;
static int warnings = 1;
static int write_url_data = 0, flush_buffers = 0;
       int maxthreads = 1;
       int indexing = 0;
static size_t Total_ndocs = 0, Total_poprank_docs = 0;
static size_t Total_nbytes = 0, Total_poprank_pas = 0;


#ifdef DEBUG_MEM
DPS_AGENT GAP2[4096];
#endif

static DPS_AGENT Main;
static DPS_ENV Conf;

#ifdef DEBUG_MEM
DPS_AGENT GAP[4096];
#endif

static DPS_AGENT **ThreadIndexers;

#ifdef HAVE_PTHREAD

pthread_t *threads;

#endif


static int DpsDisplaySQLQuery(DPS_SQLMON_PARAM *prm, DPS_SQLRES *sqlres) {
     int       res = DPS_OK;
#ifdef HAVE_SQL     
     size_t         i,j;

     if (prm->flags & DPS_SQLMON_DISPLAY_FIELDS)
     {
          for (i=0;i<sqlres->nCols;i++){
               if(i>0)fprintf(prm->outfile,"\t");
               fprintf(prm->outfile,"%s",sqlres->Fields ? sqlres->Fields[i].sqlname : "<NONAME>");
               if(i+1==sqlres->nCols)fprintf(prm->outfile,"\n");
          }
     }
     
     for (i=0;i<sqlres->nRows;i++){
          for(j=0;j<sqlres->nCols;j++){
               const char *v=DpsSQLValue(sqlres,i,j);
               if(j>0)fprintf(prm->outfile,"\t");
               fprintf(prm->outfile,"%s",v?v:NULL);
               if(j+1==sqlres->nCols)fprintf(prm->outfile,"\n");
          }
     }
     
#endif
     return res;
}

static char* sqlmongets(DPS_SQLMON_PARAM *prm, char *str, size_t size)
{
#ifdef HAVE_READLINE
  if ((prm->infile == stdin) && isatty(0))
  {
     char prompt[]="SQL>";
     char *line= readline(prompt);
     if (!line)
       return 0;
     
     if (*line) add_history(line);
     dps_strncpy(str, line, size);
  }
  else
#endif
  {
    prm->prompt(prm, DPS_SQLMON_MSG_PROMPT, "SQL>");
    if(!fgets(str, (int)size, prm->infile))
      return 0;
  }
  return str;
}

static int sqlmonprompt(DPS_SQLMON_PARAM *prm, int msqtype, const char *msg)
{
  fprintf(prm->outfile,"%s",msg);
  return DPS_OK;
}

__C_LINK /*static*/ const char* __DPSCALL DpsIndCmdStr(enum dps_indcmd cmd)
{
  switch(cmd)
  {
    case DPS_IND_CREATE: return "create";
    case DPS_IND_DROP:   return "drop";
    default: return "";
  }
  return "unknown_cmd";
}


const char *dps_cache_dir[] = {"splitter",  "tree", "cache", "store",  "url", NULL};


static int CreateOrDrop(DPS_AGENT *A, enum dps_indcmd cmd) {
  size_t dn;
  char fname[PATH_MAX];
  const char *sdir = DpsVarListFindStr(&Conf.Vars, "ShareDir", DPS_SHARE_DIR);
  DPS_DBLIST *L= (A->flags & DPS_FLAG_UNOCON) ? &A->Conf->dbl : &A->dbl;
  DPS_SQLMON_PARAM prm;
  
  for (dn = 0; dn < L->nitems; dn++) {
    FILE *infile;
    DPS_DB *db= &L->db[dn];
    dps_snprintf(fname,sizeof(fname),"%s%s%s%s%s.%s.sql",
      sdir,DPSSLASHSTR, DpsDBTypeToStr(db->DBType),DPSSLASHSTR,
      DpsIndCmdStr(cmd),DpsDBModeToStr(db->DBMode));
    printf("'%s' dbtype=%d dbmode=%d\n",fname,db->DBType,db->DBMode);
    if(!(infile= fopen(fname,"r"))) {
      sprintf(A->Conf->errstr,"Can't open file '%s'",fname);
      return DPS_ERROR;
    }
    L->currdbnum = dn;
    bzero((void*)&prm,sizeof(prm));
    prm.infile= infile;
    prm.outfile= stdout;
    prm.flags= DPS_SQLMON_DISPLAY_FIELDS;
    prm.gets= sqlmongets;
    prm.display= DpsDisplaySQLQuery;
    prm.prompt= sqlmonprompt;
    DpsSQLMonitor(A, A->Conf,&prm);
    printf("%d queries sent, %d succeeded, %d failed\n", (int)prm.nqueries, (int)prm.ngood, (int)prm.nbad);
    fclose(infile);
    if (db->DBMode == DPS_DBMODE_CACHE) {
      const char *vardir = DpsVarListFindStr(&Conf.Vars, "VarDir", DPS_VAR_DIR);
      size_t i;

      if (mkdir(vardir, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH) != 0) {
	if (errno != EEXIST) {
	  dps_strerror(A, DPS_LOG_ERROR, "Can't create directory %s", vardir);
	  return DPS_ERROR;
	}
      }


      for(i = 0; dps_cache_dir[i] != NULL; i++) {

	dps_snprintf(fname, sizeof(fname), "%s/%s", vardir, dps_cache_dir[i]);

	if (mkdir(fname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH) != 0) {
	  if (errno != EEXIST) {
	    dps_strerror(A, DPS_LOG_ERROR, "Can't create directory %s", fname);
	    return DPS_ERROR;
	  }
	}
      }

    }
  }
  return DPS_OK;
}


/* CallBack Func for Referers*/
static void DpsRefProc(int code, const char *url, const char * ref){
     printf("%d %s %s\n",code,url,ref);
}

__C_LINK static int __DPSCALL ShowReferers(DPS_AGENT * Indexer){
int res;
     printf("\n          URLs and referers \n\n");
     res = DpsURLAction(Indexer, NULL, DPS_URL_ACTION_REFERERS);
     return(res);
}



static int cmpgrp(const void *v1, const void *v2){
     int res;
     const DPS_CHARSET *c1=v1;
     const DPS_CHARSET *c2=v2;
     if ((res = strcasecmp(DpsCsGroup(c1), DpsCsGroup(c2)))) return res;
     return strcasecmp(c1->name,c2->name);
}

static void display_charsets(void){
     DPS_CHARSET *cs=NULL;
     DPS_CHARSET c[100];
     size_t i=0;
     size_t n=0;
     int family=-1;
     
     for(cs=DpsGetCharSetByID(0) ; cs && cs->name ; cs++){
          /* Skip not compiled charsets */
          if(cs->family != DPS_CHARSET_UNKNOWN)
               c[n++]=*cs;
     }
     fprintf(stderr,"\n%d charsets available:\n", (int)n);

     DpsSort(c,n,sizeof(DPS_CHARSET),&cmpgrp);
     for(i=0;i<n;i++){
          if(family!=c[i].family){
               fprintf(stderr, "\n%19s : ", DpsCsGroup(&c[i]));
               family=c[i].family;
          }
          fprintf(stderr,"%s ",c[i].name);
     }
     fprintf(stderr,"\n");
}

static void DpsFeatures(DPS_VARLIST *V){

#ifdef WITH_SYSLOG
     DpsVarListReplaceStr(V,"WITH_SYSLOG","yes");
#else
     DpsVarListReplaceStr(V,"WITH_SYSLOG","no");
#endif
#ifdef WITH_TRACE
     DpsVarListReplaceStr(V, "WITH_TRACE", "yes");
#else
     DpsVarListReplaceStr(V, "WITH_TRACE", "no");
#endif
#ifdef WITH_GOOGLEGRP
     DpsVarListReplaceStr(V, "WITH_GOOGLEGRP", "yes");
#else
     DpsVarListReplaceStr(V, "WITH_GOOGLEGRP", "no");
#endif
#ifdef FULL_RELEVANCE
     DpsVarListReplaceStr(V, "RELEVANCE", "full");
#elif defined(FAST_RELEVANCE)
     DpsVarListReplaceStr(V, "RELEVANCE", "fast");
#else
     DpsVarListReplaceStr(V, "RELEVANCE", "ultra");
#endif
#ifdef WITH_POPHOPS
     DpsVarListReplaceStr(V,"WITH_POPHOPS","yes");
#else
     DpsVarListReplaceStr(V,"WITH_POPHOPS","no");
#endif
     DpsVarListReplaceDouble(V,"DPS_POPHOPS_FACTOR", DPS_POPHOPS_FACTOR);
#ifdef WITH_REL_DISTANCE
     DpsVarListReplaceStr(V,"WITH_REL_DISTANCE","yes");
#else
     DpsVarListReplaceStr(V,"WITH_REL_DISTANCE","no");
#endif
#ifdef WITH_REL_POSITION
     DpsVarListReplaceStr(V,"WITH_REL_POSITION","yes");
#else
     DpsVarListReplaceStr(V,"WITH_REL_POSITION","no");
#endif
#ifdef WITH_REL_WRDCOUNT
     DpsVarListReplaceStr(V,"WITH_REL_WRDCOUNT","yes");
#else
     DpsVarListReplaceStr(V,"WITH_REL_WRDCOUNT","no");
#endif
#ifdef WITH_REL_TRACK
     DpsVarListReplaceStr(V,"WITH_REL_TRACK","yes");
#else
     DpsVarListReplaceStr(V,"WITH_REL_TRACK","no");
#endif
     DpsVarListReplaceDouble(V,"DPS_BEST_POSITION", DPS_BEST_POSITION);


#ifdef HAVE_PTHREAD
     DpsVarListReplaceStr(V,"HAVE_PTHREAD","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_PTHREAD","no");
#endif
#ifdef WITH_HTTPS
     DpsVarListReplaceStr(V,"WITH_HTTPS","yes");
#else
     DpsVarListReplaceStr(V,"WITH_HTTPS","no");
#endif
#ifdef DMALLOC
     DpsVarListReplaceStr(V,"DMALLOC","yes");
#else
     DpsVarListReplaceStr(V,"DMALLOC","no");
#endif
#ifdef EFENCE
     DpsVarListReplaceStr(V,"EFENCE","yes");
#else
     DpsVarListReplaceStr(V,"EFENCE","no");
#endif
#ifdef CHASEN
     DpsVarListReplaceStr(V,"CHASEN","yes");
#else
     DpsVarListReplaceStr(V,"CHASEN","no");
#endif
#ifdef MECAB
     DpsVarListReplaceStr(V,"MECAB","yes");
#else
     DpsVarListReplaceStr(V,"MECAB","no");
#endif
#ifdef HAVE_ZLIB
     DpsVarListReplaceStr(V,"HAVE_ZLIB","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_ZLIB","no");
#endif
#ifdef WITH_PARSER
     DpsVarListReplaceStr(V,"WITH_PARSER","yes");
#else
     DpsVarListReplaceStr(V,"WITH_PARSER","no");
#endif
#ifdef WITH_MP3
     DpsVarListReplaceStr(V,"WITH_MP3","yes");
#else
     DpsVarListReplaceStr(V,"WITH_MP3","no");
#endif
#ifdef WITH_FILE
     DpsVarListReplaceStr(V,"WITH_FILE","yes");
#else
     DpsVarListReplaceStr(V,"WITH_FILE","no");
#endif
#ifdef WITH_HTTP
     DpsVarListReplaceStr(V,"WITH_HTTP","yes");
#else
     DpsVarListReplaceStr(V,"WITH_HTTP","no");
#endif
#ifdef WITH_FTP
     DpsVarListReplaceStr(V,"WITH_FTP","yes");
#else
     DpsVarListReplaceStr(V,"WITH_FTP","no");
#endif
#ifdef WITH_NEWS
     DpsVarListReplaceStr(V,"WITH_NEWS","yes");
#else
     DpsVarListReplaceStr(V,"WITH_NEWS","no");
#endif
#ifdef HAVE_DP_MYSQL
     DpsVarListReplaceStr(V,"HAVE_DP_MYSQL","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_DP_MYSQL","no");
#endif
#ifdef HAVE_DP_PGSQL
     DpsVarListReplaceStr(V,"HAVE_DP_PGSQL","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_DP_PGSQL","no");
#endif
#ifdef HAVE_DP_MSQL
     DpsVarListReplaceStr(V,"HAVE_DP_MSQL","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_DP_MSQL","no");
#endif
#ifdef HAVE_IODBC
     DpsVarListReplaceStr(V,"HAVE_IODBC","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_IODBC","no");
#endif
#ifdef HAVE_UNIXODBC
     DpsVarListReplaceStr(V,"HAVE_UNIXODBC","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_UNIXODBC","no");
#endif
#ifdef HAVE_DB2
     DpsVarListReplaceStr(V,"HAVE_DB2","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_DB2","no");
#endif
#ifdef HAVE_SOLID
     DpsVarListReplaceStr(V,"HAVE_SOLID","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_SOLID","no");
#endif
#ifdef HAVE_VIRT
     DpsVarListReplaceStr(V,"HAVE_VIRT","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_VIRT","no");
#endif
#ifdef HAVE_EASYSOFT
     DpsVarListReplaceStr(V,"HAVE_EASYSOFT","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_EASYSOFT","no");
#endif
#ifdef HAVE_SAPDB
     DpsVarListReplaceStr(V,"HAVE_SAPDB","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_SAPDB","no");
#endif
#ifdef HAVE_IBASE
     DpsVarListReplaceStr(V,"HAVE_IBASE","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_IBASE","no");
#endif
#ifdef HAVE_CTLIB
     DpsVarListReplaceStr(V,"HAVE_CTLIB","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_CTLIB","no");
#endif
#ifdef HAVE_ORACLE7
     DpsVarListReplaceStr(V,"HAVE_ORACLE7","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_ORACLE7","no");
#endif
#ifdef HAVE_ORACLE8
     DpsVarListReplaceStr(V,"HAVE_ORACLE8","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_ORACLE8","no");
#endif
#ifdef HAVE_CHARSET_chinese
     DpsVarListReplaceStr(V,"HAVE_CHARSET_chinese","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_CHARSET_chinese","no");
#endif
#ifdef HAVE_CHARSET_euc_kr
     DpsVarListReplaceStr(V,"HAVE_CHARSET_euc_kr","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_CHARSET_euc_kr","no");
#endif
#ifdef HAVE_CHARSET_japanese
     DpsVarListReplaceStr(V,"HAVE_CHARSET_japanese","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_CHARSET_japanese","no");
#endif
#ifdef HAVE_CHARSET_gujarati
     DpsVarListReplaceStr(V,"HAVE_CHARSET_gujarati","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_CHARSET_gujarati","no");
#endif
#ifdef HAVE_CHARSET_tscii
     DpsVarListReplaceStr(V,"HAVE_CHARSET_tscii","yes");
#else
     DpsVarListReplaceStr(V,"HAVE_CHARSET_tscii","no");
#endif

#ifdef HAVE_LIBEXTRACTOR
     DpsVarListReplaceStr(V, "HAVE_LIBEXTRACTOR", "yes");
#else
     DpsVarListReplaceStr(V, "HAVE_LIBEXTRACTOR", "no");
#endif


}


static int usage(int level){

     static char *usage_info = "\n(C)1998-2003, LavTech Corp.\
\n(C)2003-2011, DataPark Ltd.\n\
\n\
Usage: indexer [OPTIONS]  [configfile]\n\
\n\
Indexing options:"
#ifdef HAVE_SQL
"\n\
  -a              reindex all documents even if not expired (may be\n\
                  limited using -t, -u, -s, -c, -y, -z and -f options)\n\
  -m              reindex expired documents even if not modified (may\n\
                  be limited using -t, -u, -c, -s, -y, -z and -f options)\n\
  -e              index 'most expired' (oldest) documents first\n\
  -ee             index 'least expired' (newest) documents first\n\
  -o              index documents with less depth (hops value) first\n\
  -oo             index documents with higher depth (hops value) first\n\
  -d              index most popular documents first\n\
  -dd             index least popular documents first\n\
  -r              try to reduce remote servers load by randomising\n\
                  url fetch list before indexing (recommended for very \n\
                  big number of URLs)\n\
  -rr             other way of randomisation\n\
  -n n            index only n documents and exit\n\
  -c n            index only n seconds and exit\n\
  -q              quick startup (do not add Server URLs)\n\
  -B              reindex from stored database\n\
"
#endif
#ifdef HAVE_PTHREAD
"\
  -G n            index only n megabytes per thread and exit\n\
"
#else
"\
  -G n            index only n megabytes and exit\n\
"
#endif
"\n\
  -b              block starting more than one indexer instances\n\
  -i              insert new URLs (URLs to insert must be given using -u or -f)\n\
  -p n            sleep n milliseconds after each URL\n\
  -w              do not warn before clearing documents from database\n\
"
#ifdef HAVE_PTHREAD
"  -N n            run N threads\n\
  -U              use one connection to DB for all threads\n\
"
#endif

#ifdef HAVE_SQL
"\n\
Subsection control options (may be combined):\n\
  -s status       limit indexer to documents matching status (HTTP Status code)\n\
  -t tag          limit indexer to documents matching tag\n\
  -g category     limit indexer to documents matching category\n\
  -y content-type limit indexer to documents matching content-type\n\
  -L language     limit indexer to documents matching language\n\
  -u pattern      limit indexer to documents with URLs matching pattern\n\
                  (supports SQL LIKE wildcard '%%')\n\
  -z maxhop       limit indexer to documents with hops value less or equal to maxhop\n\
  -f filename     read URLs to be indexed/inserted/cleared from file (with -a\n\
                  or -C option, supports SQL LIKE wildcard '%%'; has no effect\n\
                  when combined with -m option)\n\
  -f -            Use STDIN instead of file as URL list\n\
"
#else
"\n\
URL options:\n\
  -u URL          insert URL at startup\n\
  -f filename     read URLs to be inserted from file\n\
"
#endif
"\n\
Logging options:\n\
"
#ifdef LOG_PERROR
"  -l              do not log to stdout/stderr\n\
"
#endif
"  -v n            verbose level, 0-5\n\
\n\
Misc. options:\n\
"
#ifdef HAVE_SQL

"  -C              clear database and exit\n\
  -S              print statistics and exit\n\
  -T              test config and exit\n\
  -I              print referers and exit\n\
  -R              calculate popularity rank\n\
  -H              send to cached command to flush all buffers\n\
                  (for cache mode only)\n\
  -W              send to cached command to write url data and to create limits\n\
                  (for cache mode only)\n\
  -Y              optimize stored database at exit\n\
  -YY             optimize and check-up stored database at exit\n\
  -Z              optimize cached database at exit\n\
  -ZZ             optimize and check-up cached database at exit\n\
  -ZZZ            optimize, check-up and urls verify for cached database at exit\n\
  -Ecreate        create SQL table structure and exit\n\
  -Edocinfo       recreate docinfo data in cached database from SQL tables\n\
  -Edrop          drop SQL table structure and exit\n\
  -Efilter        check URLs against Server/Realm/Subnet and Allow/Disallow directives only\n\
  -Esitemap       write sitemap file to stdout\n\
"
#endif
"  -h,-?           print help page and exit\n\
  -hh             print more help and exit\n\
\n\
\n";
     fprintf(stderr, "\nindexer from %s-%s-%s\n%s\n", PACKAGE, VERSION, DPS_DBTYPE, usage_info);
     
     if(level>1)display_charsets();
     return(0);
}




static enum dps_indcmd DpsIndCmd(const char *cmd) {
  if (!cmd)return DPS_IND_INDEX;
  if (!strncasecmp(cmd,"ind",3))return DPS_IND_INDEX;
  else if (!strncasecmp(cmd,"sta",3))return DPS_IND_STAT;
  else if (!strncasecmp(cmd,"cre",3))return DPS_IND_CREATE;
  else if (!strncasecmp(cmd,"dro",3))return DPS_IND_DROP;
  else if (!strncasecmp(cmd,"del",3))return DPS_IND_DELETE;
  else if (!strncasecmp(cmd,"ref",3))return DPS_IND_REFERERS;
  else if (!strncasecmp(cmd,"sql",3))return DPS_IND_SQLMON;
  else if (!strncasecmp(cmd,"che",3))return DPS_IND_CHECKCONF;
  else if (!strncasecmp(cmd,"docinfo",7))return DPS_IND_DOCINFO;
  else if (!strncasecmp(cmd,"resort",6)) return DPS_IND_RESORT;
  else if (!strncasecmp(cmd,"sitemap",7)) return DPS_IND_SITEMAP;
  else if (!strncasecmp(cmd,"rehashstore",11)) return DPS_IND_REHASHSTORED;
  else if (!strncasecmp(cmd,"filter",6)) return DPS_IND_FILTER;
   
  return DPS_IND_INDEX;
}

/*
  Parse command line
*/
static int DpsARGC;
static char **DpsARGV;
static enum dps_indcmd cmd = DPS_IND_INDEX;
static int insert = 0, expire = 0, pop_rank = 0, check_cached = 0, check_stored = 0, mkind = 0, block = 0, help = 0;
static char *url_filename=NULL;

/* Letters for flags
   ABCDEFGHIJKLMNOPQRSTUVWXYZ
   ---------**----*-----*-*--
   abcdefghijklmnopqrstuvwxyz
   ---------**------------*--
*/

static void DpsParseCmdLine(void) {
  int ch;

     while ((ch = getopt(DpsARGC, DpsARGV, "BCHIQUSRMTUWYZOabdheilmoqrw?A:D:E:F:G:L:N:t:c:f:g:n:s:p:u:v:y:z:")) != -1){
          switch (ch) {
          case 'F': {
               DPS_VARLIST V,W;
               size_t i, r;
               
               DpsVarListInit(&V);
               DpsVarListInit(&W);
               DpsFeatures(&V);
               DpsVarListAddLst(&W,&V,NULL,optarg);
	       for (r = 0; r < 256; r++) {
               for (i = 0; i < W.Root[r].nvars; i++)
                    printf("%s:%s\n", W.Root[r].Var[i].name, W.Root[r].Var[i].val);
	       }
               exit(0);
          }
	  case 'O': cmd = DPS_IND_CONVERT; add_servers = 0; load_langmaps = 0; load_spells = 0; add_server_urls = 0; break;
          case 'C': cmd = DPS_IND_DELETE;  add_servers=0;load_langmaps=0;load_spells=0; add_server_urls = 0; break;
          case 'S': cmd = DPS_IND_STAT;    add_servers=0;load_langmaps=0;load_spells=0; add_server_urls = 0; extended_stats++; break;
          case 'I': cmd = DPS_IND_REFERERS;add_servers=0;load_langmaps=0;load_spells=0; add_server_urls = 0; break;
          case 'Q': cmd = DPS_IND_SQLMON;  add_servers=0;load_langmaps=0;load_spells=0; add_server_urls = 0; break;
          case 'T': cmd = DPS_IND_CHECKCONF;  add_servers = 0; load_langmaps = 0; load_spells = 0; add_server_urls = 0; break;
          case 'E': cmd = DpsIndCmd(optarg); 
	    if (cmd == DPS_IND_FILTER) { load_langmaps = load_spells = add_server_urls = 0;
	    } else if (cmd != DPS_IND_INDEX) {add_servers = load_langmaps = load_spells = add_server_urls = 0; } 
	    break;
          case 'R': pop_rank++; break;
          case 'U': flags |= DPS_FLAG_UNOCON;break;
	  case 'B': flags |= DPS_FLAG_FROM_STORED; break;
	  case 'W': write_url_data++; break;
	  case 'H': flush_buffers++; break;
          case 'Y': check_stored++; break;
          case 'Z': check_cached++; break;
          case 'M': mkind=1;break;                /* unused */
          case 'q': add_server_urls = 0; break;
          case 'l': log2stderr=0;break;
          case 'a': expire=1;break;
          case 'b': block++;break;
	  case 'd': 
	    if (flags |= DPS_FLAG_SORT_POPRANK) flags |= DPS_FLAG_SORT_POPRANK_REV; 
	    else flags |= DPS_FLAG_SORT_POPRANK;
	    break;
          case 'e': 
	    if (flags | DPS_FLAG_SORT_EXPIRED) flags |= DPS_FLAG_SORT_EXPIRED_REV; 
	    else flags |= DPS_FLAG_SORT_EXPIRED;
	    break;
          case 'z': DpsVarListAddStr(&Conf.Vars, "maxhop", optarg);break;
	  case 'o': 
	    if (flags |= DPS_FLAG_SORT_HOPS) flags |= DPS_FLAG_SORT_HOPS_REV;
	    else flags |= DPS_FLAG_SORT_HOPS; 
	    break;
          case 'r': 
	    if (flags & DPS_FLAG_SORT_SEED) flags |= DPS_FLAG_SORT_SEED2; 
	    else flags |= DPS_FLAG_SORT_SEED; 
	    break;
          case 'm': flags |= DPS_FLAG_REINDEX; break;
          case 'n': cfg_url_number = Conf.url_number = atoi(optarg);break;
          case 'c': max_index_time = atoi(optarg);break;
          case 'G': max_index_size = Conf.url_size = atoi(optarg); Conf.url_size *= 1048576; break;
          case 'v': DpsSetLogLevel(NULL, atoi(optarg)); break;
          case 'p': milliseconds = atoi(optarg); break;
          case 't': DpsVarListAddStr(&Conf.Vars,"tag" , optarg);break;
          case 'g': DpsVarListAddStr(&Conf.Vars,"cat" , optarg);break;
          case 's': DpsVarListAddStr(&Conf.Vars, "status", optarg);break;
          case 'y': DpsVarListAddStr(&Conf.Vars,"type", optarg);break;
          case 'L': DpsVarListAddStr(&Conf.Vars,"lang", optarg);break;
          case 'u': if (strchr(optarg, (int)'%')) DpsVarListAddStr(&Conf.Vars, "u", optarg);
	            else DpsVarListAddStr(&Conf.Vars, "ue", optarg);
               if(insert){
                    DPS_HREF Href;
                    DpsHrefInit(&Href);
                    Href.url = optarg;
                    Href.method = DPS_METHOD_GET;
		    Href.checked = 1;
                    DpsHrefListAdd(&Main, &Main.Hrefs, &Href);
               }
               break;
          case 'N': maxthreads=atoi(optarg);break;
          case 'f': url_filename=optarg;break;
          case 'i': insert=1;break;
          case 'w': warnings=0;break;
          case '?':
          case 'h':
          default:
               help++;
          }
     }

}





static void * thread_main(void *arg){
     DPS_AGENT * Indexer = (DPS_AGENT *)arg;
     int res=DPS_OK;
     int done=0;
     int i_sleep=0;
     int notfound = 0;
     int notarget = 0;
     int URLSize;
     time_t now;
#if defined MALLOC_STATS
     int tick = 0;
#endif

     TRACE_IN(Indexer, "thread_main");

#ifdef HAVE_PTHREAD
     DpsSigHandlersInit(Indexer);
#endif

#if defined(HAVE_PTHREAD) && defined(__FreeBSD__) && !defined(HAVE_LIBBIND) && !(defined(HAVE_LIBARES) || defined(HAVE_LIBCARES))
     DpsResolverStart(Indexer);
#endif

/*     if (!(Indexer->flags & DPS_FLAG_UNOCON)) {*/
    
          
     if(DPS_OK != DpsOpenCache(Indexer, 0/*1*/, (Indexer->flags & DPS_FLAG_UNOCON)) ) {
          fprintf(stderr,"Cache mode initializing error: '%s'\n",DpsEnvErrMsg(&Conf));
	  done = 1;
     }
/*     }*/
     while(!done){
     
          if(max_index_time >= 0) {
               time(&now);
               if((now - Indexer->start_time) > max_index_time) {
		 res = DPS_TERMINATED;
#ifdef HAVE_PTHREAD
		 if(!i_sleep){
                         DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
                         sleep_threads++;
                         DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
                         i_sleep=1;
		 }
		 
		 DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
		 done = (sleep_threads >= total_threads);
		 DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
#else
		 done = 1;
#endif
		 DpsLog(Indexer, DPS_LOG_EXTRA, "Maximum time limit exceeded (%d > %d)", now - Indexer->start_time, max_index_time);
	       }
	  }

	  if (max_index_size > 0) {
	    if (Indexer->Conf->url_size <= 0 /*Indexer->nbytes >= max_index_size * 1048576 */) {
	      res = DPS_TERMINATED;
#ifdef HAVE_PTHREAD
	      if(!i_sleep){
		DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
		sleep_threads++;
		DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
		i_sleep=1;
	      }
		 
	      DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
	      done = (sleep_threads >= total_threads);
	      DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
#else
	      done = 1;
#endif
	      DpsLog(Indexer, DPS_LOG_EXTRA, "Maximum size limit of %d exceeded", max_index_size * 1048576);
	    }
	  }

	  DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
          if(have_sighup){
               DPS_ENV   NewConf;
               int  rc, z;
               
               DpsLog(Indexer,DPS_LOG_ERROR,"Reloading config '%s'",cname);

               if (DpsEnvInit(&NewConf) != NULL) {
		 DpsSetLockProc(&NewConf,DpsLockProc);
		 DpsSetRefProc(&NewConf,DpsRefProc);

		 DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
		 Indexer->Conf = &NewConf;
		 rc = DpsIndexerEnvLoad(Indexer, cname, flags);
		 Indexer->Conf = &Conf;
               
		 if(rc!=DPS_OK){
		    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
                    DpsLog(Indexer,DPS_LOG_ERROR,"Can't load config: %s",DpsEnvErrMsg(&NewConf));
                    DpsLog(Indexer,DPS_LOG_ERROR,"Continuing with old config");
                    DpsEnvFree(&NewConf);
		 }else{
                    DpsEnvFree(&Conf);
                    Conf=NewConf;
		    DpsParseCmdLine();
		    Conf.flags = flags;
                    DpsOpenLog("indexer", &Conf, log2stderr);
		    for (z = 0 ; z < maxthreads; z++) {
		      if (ThreadIndexers[z]) {
			DpsAgentSetAction(ThreadIndexers[z], DPS_RELOADCONFIG);
		      }
		    }
		    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
		 }
	       }

               have_sighup=0;
          }
	  if (have_sigint || have_sigterm) {
	    int z;
            DpsLog(Indexer, DPS_LOG_ERROR, "%s received. Terminating. Please wait...", (have_sigint) ? "SIGINT" : "SIGTERM");
#ifdef DEBUG_MEM
	    mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ | PROT_WRITE);
#endif
	    for (z = 0 ; z < maxthreads; z++) {
	      if (ThreadIndexers[z]) {
		DpsAgentSetAction(ThreadIndexers[z], DPS_TERMINATED);
	      }
	    }
#ifdef DEBUG_MEM
	  mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ);
#endif
	    DpsAgentSetAction(&Main, DPS_TERMINATED);
	    have_sigint = have_sigterm = 0;
	  }

	  if (have_sigusr1) {
	    DpsIncLogLevel(Indexer);
	    have_sigusr1 = 0;
	  }
	  if (have_sigusr2) {
	    DpsDecLogLevel(Indexer);
	    have_sigusr2 = 0;
	  }
	  DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);

          if(done)break;

	  if (Indexer->action == DPS_RELOADCONFIG) {
	    DPS_CHARSET *unics, *loccs;
#ifdef HAVE_ASPELL
	    DPS_CHARSET *utfcs;
#endif
	    DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
	    Indexer->Flags = Indexer->Conf->Flags;
	    Indexer->flags &= ~((dps_uint8)DPS_FLAG_ADD_SERVURL);
	    Indexer->WordParam = Indexer->Conf->WordParam;
	    Indexer->Conf->url_number = cfg_url_number;
	    Indexer->Conf->url_size = max_index_size * 1048576;

	    loccs = Indexer->Conf->lcs;
	    if (!loccs) loccs = DpsGetCharSet(DpsVarListFindStr(&Indexer->Conf->Vars, "LocalCharset", "iso-8859-1"));
	    unics = DpsGetCharSet("sys-int");
	    DpsConvInit(&Indexer->uni_lc, unics, loccs, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
	    DpsConvInit(&Indexer->lc_uni, loccs, unics, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
	    DpsConvInit(&Indexer->lc_uni_text, loccs, unics, Indexer->Conf->CharsToEscape, DPS_RECODE_TEXT);
#ifdef HAVE_ASPELL
	    utfcs = DpsGetCharSet("UTF-8");
	    DpsConvInit(&Indexer->utf_lc, utfcs, loccs, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
#endif
	    DpsVarListFree(&Indexer->Vars);
	    DpsVarListAddLst(&Indexer->Vars, &Indexer->Conf->Vars, NULL, "*");
	    res = Indexer->action = DPS_OK;
	    DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	  }
          
          if (i_sleep) {
                  DpsLog(Indexer, DPS_LOG_EXTRA, "%s, sleeping %d seconds", 
                      (res == DPS_NOTARGET) ? "No targets" : ((res == DPS_TERMINATED) ? "Terminating" : "An error occured"), 60);
		  Indexer->nsleepsecs += (60 - DPSSLEEP(60)) * 1000;
          }

          if(res == DPS_OK || res == DPS_NOTARGET) {  /* Possible after bad startup */
	       if (max_index_size > 0) URLSize = Indexer->nbytes;

               res = DpsIndexNextURL(Indexer);

	       if (max_index_size > 0) {
		 URLSize = Indexer->nbytes - URLSize;
		 DPS_GETLOCK(Indexer, DPS_LOCK_CONF);
		 Indexer->Conf->url_size -= URLSize;
		 DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
	       }
	  }
	  DpsAgentSetAction(Indexer, res);

          switch(res){
	       case DPS_RELOADCONFIG:
               case DPS_OK:
                    if(i_sleep){
                         DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
                         sleep_threads--;
                         DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
                         i_sleep=0;
                    }
                    break;

               
               case DPS_NOTARGET:
		 DpsURLAction(Indexer, NULL, DPS_URL_ACTION_FLUSH); /* flush DocCache */

		 DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
		 if(pop_rank && notarget == 0 && (Indexer->Conf->Flags.poprank_method == DPS_POPRANK_NEO)) {
		   size_t z;
		   if (Indexer->Flags.cmd != DPS_IND_POPRANK) {
		     Indexer->Flags.cmd = Indexer->Conf->Flags.cmd = DPS_IND_POPRANK;
		     for (z = 0 ; z < maxthreads; z++) {
		       if (ThreadIndexers[z]) {
			 DpsAgentSetAction(ThreadIndexers[z], DPS_RELOADCONFIG);
		       }
		     }
		   }
		   notarget++;
		   DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
		   break;
		 }
		 DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);

#ifdef HAVE_PTHREAD
               /* in multi-threaded environment we          */
               /* should wait for a moment when every thread     */
               /* has nothing to do                    */

                    if(!i_sleep){
                         DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
                         sleep_threads++;
                         DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
                         i_sleep=1;
                    }

                    DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
                    done=(sleep_threads>=total_threads);
                    DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);

                    break;
#else
                    done=1;
                    break;
#endif
               case DPS_ERROR:
		    DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
		    { size_t z;
		      for (z = 0 ; z < maxthreads; z++) {
			if (ThreadIndexers[z]) {
			  DpsAgentSetAction(ThreadIndexers[z], DPS_TERMINATED);
			}
		      }
		    }
		    DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
		                                                       /* no break here! */
	       case DPS_TERMINATED:

               default:
#ifdef HAVE_PTHREAD
               /* in multi-threaded environment we          */
               /* should wait for a moment when every thread     */
               /* has nothing to do                    */

                    if(!i_sleep){
		         if (res == DPS_ERROR) DpsLog(Indexer,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Indexer->Conf));
                         DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
                         sleep_threads++;
                         DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);
                         i_sleep=1;
                    }

                    DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
                    done=(sleep_threads>=total_threads);
                    DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);

                    break;
#else
                    if (res == DPS_ERROR) DpsLog(Indexer,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Indexer->Conf));
                    done=1;
#endif
                    break;
          }
          if((milliseconds)&&(!done)){
               DpsLog(Indexer,DPS_LOG_DEBUG,"Sleeping %d millisecond(s)", milliseconds);
               Indexer->nsleepsecs += milliseconds * (DPS_MSLEEP(milliseconds) == 0);
          }
#if defined MALLOC_STATS
	       if (Indexer->handle == 0 && ++tick == 10) {
		 malloc_stats();
		 tick = 0;
	       }
#endif
     }

     if(res != DPS_ERROR){
          time_t sec;
          float M = 0.0, K = 0.0;

          DpsURLAction(Indexer, NULL, DPS_URL_ACTION_FLUSH); /* flush DocCache */
	  DpsStoreHrefs(Indexer);    /**< store hrefs if any */

          time(&now);
          sec = now - Indexer->start_time - Indexer->nsleepsecs / 1000;
          if (sec > 0) {
               M = Indexer->nbytes / 1048576.0 / sec;
               if (M < 1.0) K = Indexer->nbytes / 1024.0 / sec;
          }
          DpsLog(Indexer, DPS_LOG_INFO, "Done (%d seconds, %u documents, %u bytes, %5.2f %cbytes/sec.)",
		 sec, Indexer->ndocs, Indexer->nbytes, (M < 1.0) ? K : M, (M < 1.0) ? 'K' : 'M' );
#if defined(HAVE_PTHREAD)
	  {
	    register int z;

#ifdef DEBUG_MEM
	    mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ | PROT_WRITE);
#endif
	    for (z = 1 ; z < total_threads; z++)
	      if (ThreadIndexers[z])  pthread_kill(threads[z], SIGALRM); /* wake-up sleeping threads */
#ifdef DEBUG_MEM
	    mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ);
#endif
	  }
#endif
     }

/*     if (!(Indexer->flags & DPS_FLAG_UNOCON)) {*/
     DpsCloseCache(Indexer, 0/*1*/, (Indexer->flags & DPS_FLAG_UNOCON) );
/*     }*/

     DPS_GETLOCK(Indexer, DPS_LOCK_THREAD);
     Total_nbytes += Indexer->nbytes;
     Total_ndocs += Indexer->ndocs;
     Total_poprank_pas += Indexer->poprank_pas;
     Total_poprank_docs += Indexer->poprank_docs;
#ifdef DEBUG_MEM
	  mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ | PROT_WRITE);
#endif

     ThreadIndexers[Indexer->handle] = NULL;

#ifdef DEBUG_MEM
	  mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ);
#endif
     total_threads--;
     DPS_RELEASELOCK(Indexer, DPS_LOCK_THREAD);

#if defined(HAVE_PTHREAD) && defined(__FreeBSD__) && !defined(HAVE_LIBBIND) && !(defined(HAVE_LIBARES) || defined(HAVE_LIBCARES))
     DpsResolverFinish(Indexer);
#endif

     TRACE_OUT(Indexer);

#ifdef HAVE_PTHREAD
     if (Indexer->handle) DpsAgentFree(Indexer);
#endif
 
     return NULL;
}


static char pidname[1024];
static char time_pid[100];

static void exitproc(void){
     unlink(pidname);
}


static char * time_pid_info(void){
     time_t t = time(NULL);
#ifdef HAVE_PTHREAD
     struct tm l_tim;
     struct tm *tim = localtime_r(&t, &l_tim);
#else
     struct tm *tim = localtime(&t);
#endif
     strftime(time_pid, sizeof(time_pid), "%a %d %H:%M:%S", tim);
     sprintf(time_pid+dps_strlen(time_pid)," [%d]",(int)getpid());
     return(time_pid);
}



static int DpsConfirm(const char *msg)
{
        char str[8];
        printf("%s",msg);
        return (fgets(str,sizeof(str),stdin) && !strncmp(str,"YES",3));
}

static int DpsClear(DPS_AGENT *A, const char *url_fname)
{
     int clear_confirmed=1;
     if(warnings) {
          size_t i;
          printf("You are going to delete content from database(s):\n");
	  if (A->flags & DPS_FLAG_UNOCON) {
	    for (i = 0; i < A->Conf->dbl.nitems; i++) printf("%s\n", A->Conf->dbl.db[i].DBADDR);
	  } else {
	    for (i = 0; i < A->dbl.nitems; i++) printf("%s\n", A->dbl.db[i].DBADDR);
	  }
          clear_confirmed=DpsConfirm("Are you sure?(YES/no)");
     }
     
     if(clear_confirmed) {
          if(url_fname) {
               if(DPS_OK!=DpsURLFile(A,url_fname,DPS_URL_FILE_CLEAR)){
                    DpsLog(A,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(A->Conf));
               }
          }
          else {
               printf("Deleting...");
               if(DPS_OK!=DpsClearDatabase(A)){
                    return DPS_ERROR;
               }
               printf("Done\n");
          }
     }else{
          printf("Canceled\n");
     }
     return DPS_OK;
}


static int DpsIndex(DPS_AGENT *A) {
/*  DPS_DOCUMENT *Doc;*/

	ThreadIndexers = (DPS_AGENT**)DpsXmalloc((maxthreads + 2 /*+ 4096*/) * sizeof(DPS_AGENT*));

	if (ThreadIndexers == NULL) {
	    DpsLog(A, DPS_LOG_ERROR, "Can't alloc %d bytes %s:%d", (maxthreads + 2 /*+ 4096*/) * sizeof(DPS_AGENT*), __FILE__, __LINE__);
	    return DPS_ERROR;
	}


#ifdef HAVE_PTHREAD
     {
#ifdef DEBUG_MEM
       int gagap[4096];
#endif
          int i;
#if defined MALLOC_STATS
	  int tick = 0;
#endif

#ifdef HAVE_PTHREAD_SETCONCURRENCY_PROT
	  if (pthread_setconcurrency(maxthreads + 1) != 0) {
	    DpsLog(A, DPS_LOG_ERROR, "Can't set %d concurrency threads", maxthreads + 1);
	    return DPS_ERROR;
	  }
#elif HAVE_THR_SETCONCURRENCY_PROT
	  if (thr_setconcurrency(maxthreads + 1) != NULL) {
	    DpsLog(A, DPS_LOG_ERROR, "Can't set %d concurrency threads", maxthreads + 1);
	    return DPS_ERROR;
	  }
#endif
               
          threads = (pthread_t*)DpsMalloc((maxthreads + 2 /*+ 4096*/) * sizeof(pthread_t));

	  if (threads == NULL) {
	    DpsLog(A, DPS_LOG_ERROR, "Can't alloc for threads %s:%d", __FILE__, __LINE__);
	    return DPS_ERROR;
	  }

#ifdef DEBUG_MEM
	  mprotect(threads, (maxthreads + 1) * sizeof(pthread_t), PROT_READ);
#endif
#ifdef DEBUG_MEM
	  mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ);
#endif
#ifdef DEBUG_MEM
	mprotect(&A, sizeof(DPS_AGENT *), PROT_READ);
#endif

/*
	Doc = DpsDocInit(NULL);
	if (Doc != NULL) {
	  int rc;
	  DpsVarListAddStr(&Doc->Sections, "DP_ID", "0");
	  rc = DpsDeleteBadHrefs(Indexer, Doc, db)
	    DpsDocFree(Doc);
	  if(DPS_OK!= rc) return rc;
	}

*/

	for(i = 0; i < maxthreads - 1; i++) {
	    DPS_AGENT *Indexer;

	    DPS_GETLOCK(A, DPS_LOCK_THREAD);
	    DPS_GETLOCK(A, DPS_LOCK_CONF);
	    Indexer = DpsAgentInit(NULL, A->Conf, i + 1);
#ifdef DEBUG_MEM
	  mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ | PROT_WRITE);
#endif
	    ThreadIndexers[i + 1] = Indexer;
#ifdef DEBUG_MEM
	  mprotect(ThreadIndexers, (maxthreads + 2) * sizeof(DPS_AGENT*), PROT_READ);
#endif
	    DPS_RELEASELOCK(A, DPS_LOCK_CONF);
	    DPS_RELEASELOCK(A, DPS_LOCK_THREAD);

	    if (Indexer == NULL) {
	      DpsLog(A, DPS_LOG_ERROR, "DpsAgentInit error %s:%d", __FILE__, __LINE__);
#ifdef DEBUG_MEM
	mprotect(&A, sizeof(DPS_AGENT *), PROT_READ | PROT_WRITE);
#endif
	      return DPS_ERROR;
	    }
	    Indexer->flags = flags;
	    Indexer->start_time = A->start_time;

	    {
	      pthread_attr_t attr;
	      size_t stksize, oldstksize;
                    
	      pthread_attr_init(&attr);
	      pthread_attr_getstacksize(&attr, &oldstksize);
	      stksize = oldstksize + 
#ifdef HAVE_ASPELL
		4 /*16*/
#else
		2
#endif
		* 1024 * 1024

#if defined(MECAB) || defined(CHASEN)
		+ 2 * DpsVarListFindInt(&A->Vars, "IndexDocSizeLimit", 
					DpsVarListFindInt(&A->Vars, "MaxDocSize", DPS_MAXDOCSIZE)) * sizeof(dpsunicode_t)
#endif
		;
	      pthread_attr_setstacksize(&attr, stksize);
#ifdef WITH_TRACE
	      fprintf(A->TR, "thread stack: system: %d  our: %d\n", oldstksize, stksize);
	      fflush(A->TR);
#endif

	      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
/*		    pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
#ifdef SCHED_FIFO
		    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
#endif*/
#ifdef DEBUG_MEM
	      mprotect(threads, (maxthreads + 1) * sizeof(pthread_t), PROT_READ | PROT_WRITE);
#endif
	      if (pthread_create(&threads[i + 1], &attr, &thread_main, Indexer) == 0) {
		DPS_GETLOCK(A, DPS_LOCK_THREAD);
		total_threads++; /* = i + 1;*/
		DPS_RELEASELOCK(A,DPS_LOCK_THREAD);
	      }
#ifdef DEBUG_MEM
	      mprotect(threads, (maxthreads + 1) * sizeof(pthread_t), PROT_READ);
#endif
	      pthread_attr_destroy(&attr);
	    }
	    if (milliseconds) DPS_MSLEEP(500 + milliseconds);
	    else DPS_MSLEEP(620);
          }
	Main.flags = flags;
	DPS_GETLOCK(A, DPS_LOCK_THREAD);
	total_threads++;
	ThreadIndexers[0] = &Main;
	DPS_RELEASELOCK(A,DPS_LOCK_THREAD);
	thread_main(&Main);
          while(1) {
               int num;
               DPS_GETLOCK(A,DPS_LOCK_THREAD);
               num=total_threads;
               DPS_RELEASELOCK(A,DPS_LOCK_THREAD);
               DPSSLEEP(1);
               if(!num)break;
#if defined MALLOC_STATS
	       if (++tick == 10) {
		 malloc_stats();
		 tick = 0;
	       }
#endif
          }
          DPS_FREE(threads);
     }
#else
     Main.handle = 1;
     thread_main(&Main);
#endif

     DPS_FREE(ThreadIndexers);

#ifdef DEBUG_MEM
	mprotect(&A, sizeof(DPS_AGENT *), PROT_READ | PROT_WRITE);
#endif

     return DPS_OK;
}

#ifdef __FreeBSD__
#if __FreeBSD__ < 5
extern char *malloc_options = "axH>>>R";
#endif
#endif


int main(int argc, char **argv, char **envp) {
     char      *language=NULL,*affix=NULL,*dictionary=NULL, *env;
     int       pid_fd, cfg_res, cache_opened = 0;
     char      pidbuf[1024];
#ifdef __FreeBSD__
#if __FreeBSD__ >= 7
 #if __FreeBSD_version < 701000
     _malloc_options = "axNNNH";
 #else
     _malloc_options = "ax3N";
 #endif
#elif __FreeBSD__ >= 5
     _malloc_options = "axH>>R";
#else
     malloc_options = "axH>>>R";
#endif
#endif

     bzero(&Conf, sizeof(Conf));
     bzero(&Main, sizeof(Main));

     DpsInit(argc, argv, envp); /* Initialize library */
     DpsGetSemLimit();
     
     if (DpsEnvInit(&Conf) == NULL) exit(1);
     DpsSetRefProc(&Conf,DpsRefProc);
#ifdef THINFO_TEST
     DpsSetThreadProc(&Conf,DpsShowInfo);
#endif
     DpsAgentInit(&Main,&Conf,0);
     
     DpsARGC = argc;

     DpsARGV = (char**)DpsXmalloc((argc + 1) * sizeof(char*));
     if (DpsARGV == NULL) {
       fprintf(stderr, "Can't allocate DpsARGV\n");
       exit(-1);
     }
     {
       size_t i;
       for (i = 0; i < argc; i++) {
	 if ((DpsARGV[i] = DpsStrdup(argv[i])) == NULL) {
	   fprintf(stderr, "Can't duplicate DpsARGV[%d]\n", (int)i);
	   exit(-1);
	 }
       }
       DpsARGV[DpsARGC = argc] = NULL;
     }

     DpsParseCmdLine();
     /*     
     if (cmd != DPS_IND_INDEX) {
       add_servers=0;
       load_langmaps=0;
       load_spells=0;
       add_server_urls = 0;
     }
     */
     flags |= add_servers | add_server_urls | load_langmaps | load_spells;
#ifndef HAVE_PTHREAD
     Conf.flags |= DPS_FLAG_UNOCON;
#endif
     Main.flags = Conf.flags = flags;

     argc -= optind;argv += optind;

     if((argc>1) || (help)){
          usage(help);
	  DpsAgentFree(&Main);
          DpsEnvFree(&Conf);
          return(1);
     }
     
     env = getenv("DPS_CONF_DIR");
     DpsVarListReplaceStr(&Conf.Vars, "EtcDir", env ? env : DPS_CONF_DIR);
     
     env=getenv("DPS_SHARE_DIR");
     DpsVarListReplaceStr(&Conf.Vars,"ShareDir",env?env:DPS_SHARE_DIR);
     
     if(argc==1){
          dps_strncpy(cname, argv[0], sizeof(cname));
          cname[sizeof(cname)-1]='\0';
     }else{
          const char *cd=DpsVarListFindStr(&Conf.Vars,"DPS_CONF_DIR",DPS_CONF_DIR);
          dps_snprintf(cname,sizeof(cname),"%s%s%s",cd,DPSSLASHSTR,"indexer.conf");
          cname[sizeof(cname)-1]='\0';
     }

     if (block) {
       /* Check that another instance isn't running */
       /* and create PID file.                      */
                    
       sprintf(pidname, "%s/%s", DPS_VAR_DIR, "indexer.pid");
       pid_fd = DpsOpen3(pidname, O_CREAT|O_EXCL|O_WRONLY, 0644);
       if(pid_fd < 0){
	 dps_strerror(NULL, 0, "%s Can't create '%s'", time_pid_info(), pidname);
	 if(errno == EEXIST){
	   int pid = 0;
	   pid_fd = DpsOpen3(pidname, O_RDWR, 0644);
	   if (pid_fd < 0) {
	     dps_strerror(NULL, 0, "%s Can't open '%s'", time_pid_info(), pidname);
	     goto ex;
	   }
	   (void)read(pid_fd, pidbuf, sizeof(pidbuf));
	   if (1 > sscanf(pidbuf, "%d", &pid)) {
	     dps_strerror(NULL, 0, "%s Can't read pid from '%s'", time_pid_info(), pidname);
	     close(pid_fd);
	     goto ex;
	   }
	   pid = kill((pid_t)pid, 0);
	   if (pid == 0) {
	     fprintf(stderr, "It seems that another indexer is already running!\n");
	     fprintf(stderr, "Remove '%s' if it is not true.\n", pidname);
	     close(pid_fd);
	     goto ex;
	   }
	   if (errno == EPERM) {
	     fprintf(stderr, "Can't check if another indexer is already running!\n");
	     fprintf(stderr, "Remove '%s' if it is not true.\n", pidname);
	     close(pid_fd);
	     goto ex;
	   }
	 }
       }
       DpsClose(pid_fd);
       unlink(pidname);
     }
     
     DpsOpenLog("indexer.cfg", &Conf, log2stderr);
     if(DPS_OK != (cfg_res = DpsIndexerEnvLoad(&Main, cname, flags))) {
          fprintf(stderr,"%s\n",DpsEnvErrMsg(&Conf));
	  DpsAgentFree(&Main);
          DpsEnvFree(&Conf);
          exit(1);
     }

     DpsInitMutexes();
/*     Main.Flags.PopRankNeoIterations = Conf.Flags.PopRankNeoIterations;*/
     Main.Flags = Conf.Flags;
     Main.WordParam = Conf.WordParam;
     {
       DPS_CHARSET *unics, *loccs;
#ifdef HAVE_ASPELL
       DPS_CHARSET *utfcs;
#endif
       loccs = Conf.lcs;
       if (!loccs) loccs = DpsGetCharSet(DpsVarListFindStr(&Conf.Vars, "LocalCharset", "iso-8859-1"));
       unics = DpsGetCharSet("sys-int");
       DpsConvInit(&Main.uni_lc, unics, loccs, Conf.CharsToEscape, DPS_RECODE_HTML);
       DpsConvInit(&Main.lc_uni, loccs, unics, Conf.CharsToEscape, DPS_RECODE_HTML);
       DpsConvInit(&Main.lc_uni_text, loccs, unics, Conf.CharsToEscape, DPS_RECODE_TEXT);
#ifdef HAVE_ASPELL
       utfcs = DpsGetCharSet("UTF-8");
       DpsConvInit(&Main.utf_lc, utfcs, loccs, Conf.CharsToEscape, DPS_RECODE_HTML);
#endif
     }
     DpsVarListAddLst(&Main.Vars, &Conf.Vars, NULL, "*");
     /* set locale if specified */
     { const char *url;
       if ((url = DpsVarListFindStr(&Conf.Vars, "Locale", NULL)) != NULL) {
	 setlocale(LC_ALL, url);
       }
     }
/*     
     if (cmd==DPS_IND_CHECKCONF){
          exit(0);
     }
  */   
     DpsOpenLog("indexer", &Conf, log2stderr);
     DpsSigHandlersInit(&Main);
     DpsSetLockProc(&Conf, DpsLockProc);
     
     /* DpsOpenCache was here */
/*     if (cmd != DPS_IND_INDEX && cmd != DPS_IND_POPRANK) {*/
     if(DPS_OK != DpsOpenCache(&Main, 0, (Main.flags & DPS_FLAG_UNOCON))) {
	 fprintf(stderr,"Cache mode initializing error: '%s'\n",DpsEnvErrMsg(&Conf));
	 DpsEnvFree(&Conf);
	 exit(1);
     }
     cache_opened = 1;
/*     }*/
    
     if (cmd==DPS_IND_SQLMON){
          DPS_SQLMON_PARAM prm;
          prm.infile= stdin;
          prm.outfile= stdout;
          prm.flags= DPS_SQLMON_DISPLAY_FIELDS;
          prm.gets= sqlmongets;
          prm.display= DpsDisplaySQLQuery;
          prm.prompt= sqlmonprompt;
          DpsSQLMonitor(&Main, &Conf, &prm);
	  goto ex;
     }
     
     if(url_filename && strcmp(url_filename,"-")) {
          /* Make sure URL file is readable if not STDIN */
          FILE *url_file;
          if(!(url_file=fopen(url_filename,"r"))){
               dps_strerror(&Main, DPS_LOG_ERROR, "Error: can't open url file '%s'", url_filename);
               goto ex;
          }
          fclose(url_file);
     }
     
     if(insert && url_filename) {
          
          if(strcmp(url_filename,"-")){
               /* Make sure all URLs to be inserted are OK */
               if(DPS_OK!=DpsURLFile(&Main, url_filename,DPS_URL_FILE_PARSE)){
                    DpsLog(&Main,DPS_LOG_ERROR,"Error: Invalid URL in '%s'",url_filename);
                    goto ex;
               }
          }
          
          if(DPS_OK!=DpsURLFile(&Main,url_filename,DPS_URL_FILE_INSERT)){
               DpsLog(&Main,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Main.Conf));
               goto ex;
          }
	  DpsStoreHrefs(&Main);    /**< store hrefs from config and command line */
     }
     
     if(expire){
          int  res = DPS_OK;
          
          if(url_filename){
               res=DpsURLFile(&Main,url_filename,DPS_URL_FILE_REINDEX);
          }else{
	    if (cfg_url_number != 0x7FFFFFFF || max_index_time > 0 || max_index_size > 0 ) {
	      Conf.Flags.expire = 1;
	    } else {
	      res = DpsURLAction(&Main, NULL, DPS_URL_ACTION_EXPIRE);
	    }
          }
          if(res!=DPS_OK){
               DpsLog(&Main,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Main.Conf));
               goto ex;
          }
     }
     
     if(affix||dictionary){
          if(!language){
               DpsLog(&Main,DPS_LOG_ERROR,"Error: Language is not specified for import!");
          }else
          if(dps_strlen(language)!=2){
               DpsLog(&Main,DPS_LOG_ERROR,"Error: Language should be 2 letters!");
          }
          goto ex;
     }

     switch(cmd){
          case DPS_IND_CONVERT:
	    {
	      int clear_confirmed = 1;
	      {
		size_t i;
		printf("You are going to convern content from database(s):\n");
		if (Main.flags & DPS_FLAG_UNOCON) {
		  for (i = 0; i < Main.Conf->dbl.nitems; i++) printf("%s\n", Main.Conf->dbl.db[i].DBADDR);
		} else {
		  for (i = 0; i < Main.dbl.nitems; i++) printf("%s\n", Main.dbl.db[i].DBADDR);
		}
		clear_confirmed = DpsConfirm("Are you sure?(YES/no)");
	      }
	      if (clear_confirmed && (DPS_OK != DpsCacheConvert(&Main))) {
                    DpsLog(&Main, DPS_LOG_ERROR, "Error: '%s'", DpsEnvErrMsg(Main.Conf));
	      }
	    }
	    break;
          case DPS_IND_DELETE:
	       DpsAgentStoredConnect(&Main);
               if(DPS_OK!=DpsClear(&Main,url_filename)){
                    DpsLog(&Main,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Main.Conf));
               }
               break;
          case DPS_IND_STAT:
               if(DPS_OK!=DpsShowStatistics(&Main)){
                    DpsLog(&Main,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Main.Conf));
               }
               break;
          case DPS_IND_REFERERS:
               if(DPS_OK!=ShowReferers(&Main)){
                    DpsLog(&Main,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Main.Conf));
               }
               break;
          case DPS_IND_CREATE:
          case DPS_IND_DROP:
               if(DPS_OK!=CreateOrDrop(&Main,cmd)){
                    DpsLog(&Main,DPS_LOG_ERROR,"Error: '%s'",DpsEnvErrMsg(Main.Conf));
               }
               break;
          case DPS_IND_DOCINFO:
	    if (DPS_OK != DpsURLAction(&Main, NULL, DPS_URL_ACTION_REFRESHDOCINFO)) {
                    DpsLog(&Main, DPS_LOG_ERROR, "Error: '%s'", DpsEnvErrMsg(Main.Conf));
	    }
	    break;
          case DPS_IND_SITEMAP:
	    if (DPS_OK != DpsURLAction(&Main, NULL, DPS_URL_ACTION_SITEMAP)) {
                    DpsLog(&Main, DPS_LOG_ERROR, "Error: '%s'", DpsEnvErrMsg(Main.Conf));
	    }
	    break;
          case DPS_IND_RESORT:
	    if (DPS_OK != DpsURLAction(&Main, NULL, DPS_URL_ACTION_RESORT)) {
                    DpsLog(&Main, DPS_LOG_ERROR, "Error: '%s'", DpsEnvErrMsg(Main.Conf));
	    }
	    break;
          case DPS_IND_REHASHSTORED:
	    if (DPS_OK != DpsURLAction(&Main, NULL, DPS_URL_ACTION_REHASHSTORED)) {
                    DpsLog(&Main, DPS_LOG_ERROR, "Error: '%s'", DpsEnvErrMsg(Main.Conf));
	    }
	    break;
          case DPS_IND_CHECKCONF:
               DpsLog(&Main,DPS_LOG_INFO, "indexer from %s-%s-%s, config test %s with '%s'", 
		      PACKAGE, VERSION, DPS_DBTYPE, (cfg_res == DPS_OK) ? "OK" : "FAILED", cname);
	       if (cfg_res == DPS_OK) {
		 if (DpsVarListFindInt(&Main.Vars, "OldStoredFiles", 0) != 0) DpsBaseRelocate(&Main, 0);
		 if (DpsVarListFindInt(&Main.Vars, "OldURLDataFiles", 0) != 0) DpsBaseRelocate(&Main, 1);
		 if (DpsVarListFindInt(&Main.Vars, "OldWrdFiles", 0) != 0) DpsBaseRelocate(&Main, 2);
	       }
               if (!pop_rank || (Main.Flags.poprank_method != DPS_POPRANK_NEO)) break;
	       cmd = DPS_IND_POPRANK;
          default:
          {
	    time_t sec, now;
	    float M = 0.0, K = 0.0;

               if (block) {
                    /* Check that another instance isn't running */
                    /* and create PID file.                      */
                    
                    sprintf(pidname,"%s/%s", DpsVarListFindStr(&Conf.Vars, "VarDir", DPS_VAR_DIR), "indexer.pid");
                    pid_fd = DpsOpen3(pidname, O_CREAT | O_EXCL | O_WRONLY, 0644);
                    if(pid_fd < 0){
		      dps_strerror(NULL, 0, "%s Can't create '%s'", time_pid_info(), pidname);
		      if(errno == EEXIST){
			int pid = 0;
			pid_fd = DpsOpen3(pidname, O_RDWR, 0644);
			if (pid_fd < 0) {
			  dps_strerror(NULL, 0, "%s Can't open '%s'", time_pid_info(), pidname);
			  goto ex;
			}
			(void)read(pid_fd, pidbuf, sizeof(pidbuf));
			if (1 > sscanf(pidbuf, "%d", &pid)) {
			  dps_strerror(NULL, 0, "%s Can't read pid from '%s'", time_pid_info(), pidname);
			  close(pid_fd);
			  goto ex;
			}
			pid = kill((pid_t)pid, 0);
			if (pid == 0) {
			  fprintf(stderr, "It seems that another indexer is already running!\n");
			  fprintf(stderr, "Remove '%s' if it is not true.\n", pidname);
			  close(pid_fd);
			  goto ex;
			}
			if (errno == EPERM) {
			  fprintf(stderr, "Can't check if another indexer is already running!\n");
			  fprintf(stderr, "Remove '%s' if it is not true.\n", pidname);
			  close(pid_fd);
			  goto ex;
			}
			dps_strerror(NULL, 0, "%s Process %s seems to be dead. Flushing '%s'", time_pid_info(), pidbuf, pidname);
			lseek(pid_fd, 0L, SEEK_SET);
			ftruncate(pid_fd, 0L);
		      }
                    }
                    sprintf(pidbuf,"%d\n",(int)getpid());
                    (void)write(pid_fd, pidbuf, dps_strlen(pidbuf));
		    DpsClose(pid_fd);
                    atexit(&exitproc);
               }
	       Conf.Flags.cmd = Main.Flags.cmd = cmd;
	       DpsSetLockProc(&Conf, DpsLockProc);
               DpsLog(&Main, DPS_LOG_ERROR, "indexer from %s-%s-%s started with '%s'", PACKAGE, VERSION, DPS_DBTYPE, cname);
	       DpsLog(&Main, DPS_LOG_EXTRA, "Chinese dictionary with %d entries", Main.Conf->Chi.nwords);
	       DpsLog(&Main, DPS_LOG_EXTRA, "Korean dictionary with %d entries", Main.Conf->Korean.nwords);
	       DpsLog(&Main, DPS_LOG_EXTRA, "Thai dictionary with %d entries", Main.Conf->Thai.nwords);
	       DpsLog(&Main, DPS_LOG_EXTRA, "LogsOnly: %s", Main.Conf->logs_only ? "yes" : "no");
               DpsLog(&Main, DPS_LOG_DEBUG, "mutexes used: %d", DpsNsems);
	       DpsLog(&Main, DPS_LOG_DEBUG, "The following sections are defined");
	       if (DpsNeedLog(DPS_LOG_DEBUG)) DpsVarListLog(&Main, &Conf.Sections, DPS_LOG_DEBUG, "Sections");
               DpsStoreHrefs(&Main);    /**< store hrefs from config and command line */
	       DpsRobotClean(&Main);    /**< clean expired robots.txt data */
	       DpsCookiesClean(&Main);  /**< clean expired cookies data */
	       if (url_filename) {
		 if(DPS_OK != DpsURLFile(&Main, url_filename, DPS_URL_FILE_TARGET)) {
		   DpsLog(&Main, DPS_LOG_ERROR, "Error: '%s'", url_filename);
		   goto ex;
		 }
	       }
	       DpsAgentStoredConnect(&Main);
               DpsIndex(&Main);

	       time(&now);
	       sec = now - Main.start_time;
	       if (sec > 0) {
		 M = Total_nbytes / 1048576.0 / sec;
		 if (M < 1.0) K = Total_nbytes / 1024.0 / sec;
	       } else sec = 1;
	       DpsLog(&Main, DPS_LOG_ERROR, 
		      "Total %d seconds, %u documents, %u bytes, %5.2f %cbytes/sec, %5.2f %s, %u bytes/doc.",
		      sec, Total_ndocs, Total_nbytes, (M < 1.0) ? K : M, (M < 1.0) ? 'K' : 'M',
		      (sec > Total_ndocs) ? ((Total_ndocs) ? (float)sec/Total_ndocs : 0.0)
		      : (float)Total_ndocs/sec , 
		      (sec > Total_ndocs) ? "sec/doc" : "doc/sec",
		      (Total_ndocs)? Total_nbytes/Total_ndocs : 0);

	       M = Total_poprank_pas / 1048576.0 / sec;
	       if (M < 1.0) K = Total_poprank_pas / 1024.0 / sec;
	       DpsLog(&Main, DPS_LOG_ERROR, 
		      "Neo PopRank: %u documents, %lu pas, %5.2f %cpas/sec, %5.2f %s, %5.2f pas/doc.",
		      Total_poprank_docs, Total_poprank_pas, (M < 1.0) ? K : M, (M < 1.0) ? 'K' : 'M',
		      (sec > Total_poprank_docs) ? ((Total_poprank_docs) ? (float)sec/Total_poprank_docs : 0.0)
		      : (float)Total_poprank_docs/sec , 
		      (sec > Total_poprank_docs) ? "sec/doc" : "doc/sec",
		      (Total_poprank_docs)? (float)Total_poprank_pas/Total_poprank_docs : 0);

	       
          }
     }

     if (write_url_data) {
       DpsLog(&Main, DPS_LOG_EXTRA, "Writing url data");
       DpsURLAction(&Main, NULL, DPS_URL_ACTION_WRITEDATA);
     }

     DpsVarListAddInt(&Main.Vars, "FlushBuffers", flush_buffers);
     if (flush_buffers) {
       DpsLog(&Main, DPS_LOG_EXTRA, "Flushing cached buffers");
     }
     DpsURLAction(&Main, NULL, DPS_URL_ACTION_FLUSHCACHED);
     
     
#ifdef HAVE_SQL
     if (pop_rank && (Main.Flags.poprank_method != DPS_POPRANK_NEO)) {
       DpsLog(&Main, DPS_LOG_EXTRA, "PopRank caclulation");
       DpsSrvAction(&Main, NULL, DPS_SRV_ACTION_POPRANK);
     }
     if (check_stored) {
       DpsLog(&Main, DPS_LOG_EXTRA, "Stored check up");
       DpsAgentStoredConnect(&Main);
       DpsStoreCheckUp(&Main, check_stored);
     }
     if (check_cached) {
       DpsLog(&Main, DPS_LOG_EXTRA, "Cached check up");
       DpsCachedCheck(&Main, check_cached);
     }
#endif
     
ex:
     if (cache_opened /*cmd != DPS_IND_INDEX && cmd != DPS_IND_POPRANK*/) {
	 DpsCloseCache(&Main, 0, (Main.flags & DPS_FLAG_UNOCON));
     }
     total_threads=0;
     DpsAgentFree(&Main);
     DpsEnvFree(&Conf);
     DpsDestroyMutexes();
     fclose(stdout);

     {
       size_t i;
       for (i = 0; i < DpsARGC; i++) {
	 DPS_FREE(DpsARGV[i]);
       }
       DPS_FREE(DpsARGV);
     }
     
     DpsDeInit();

#ifdef EFENCE
     fprintf(stderr, "Memory leaks checking\n");
     DpsEfenceCheckLeaks();
#endif
#ifdef FILENCE
     fprintf(stderr, "FD leaks checking\n");
     DpsFilenceCheckLeaks(NULL);
#endif

#ifdef BOEHMGC
     CHECK_LEAKS();
#endif
     
     return DPS_OK;
}

