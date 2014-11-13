/* Copyright (C) 2013-2014 Maxim Zakharov. All rights reserved.
   Copyright (C) 2004-2012 DataPark Ltd. All rights reserved.
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

#ifndef _DPS_COMMON_H
#define _DPS_COMMON_H

#include "dps_config.h"

#include <stdio.h> /* for FILE etc. */
#include <strings.h> /* for Solaris? */
#include <time.h>

#include <sys/types.h>

#if defined(APACHE1) || defined(APACHE2)

#if !defined(_REGEX_H) \
  && !defined(_REGEX_H_) \
  && !defined(_PCREPOSIX_H) \
  && !defined(_RX_H) \
  && !defined(__REGEXP_LIBRARY_H__) \
  && !defined(_H_REGEX)                /* This one is for AIX */
#ifdef HAVE_TRE_REGEX_H
#include <tre/regex.h>
#else
#include <regex.h>
#endif
#endif

#else /*  defined(APACHE1) || defined(APACHE2) */

#ifndef _PCREPOSIX_H
#ifdef HAVE_TRE_REGEX_H
#include <tre/regex.h>
#else
#include <regex.h>
#endif
#endif

#endif /*  defined(APACHE1) || defined(APACHE2) */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef MECAB
#include <mecab.h>
#endif
#ifdef HAVE_ASPELL
#include "aspell.h"
#endif
#if defined(HAVE_LIBCARES)
#include "ares.h"
#include "ares_version.h"
#elif defined(HAVE_LIBARES)
#include "ares.h"
#endif

#ifdef DEBUG_CONF_LOCK
#include <assert.h>
#endif

#include "dps_filence.h"
#include "dps_unicode.h"
#include "dps_uniconv.h"

/* Some constants */
#define DPS_LANGPERDOC				16		/* FIXME */
#define DPS_USER_AGENT				"DataparkSearch/" VERSION " (+http://dataparksearch.org/bot)"
#define DPS_MAXWORDPERQUERY			256

/* Some sizes and others definitions */
#define DPS_MAXDOCSIZE				2*1024*1024	/**< 2 MB  */
#define DPS_DEFAULT_REINDEX_TIME		7*24*60*60	/**< 1week */
#define DPS_MAXWORDSIZE				32
#define DPS_MAXDISCWORDSIZE			64
#define DPS_DEFAULT_MAX_HOPS			256
#define DPS_DEFAULT_MAX_DEPTH			16
#define DPS_DEFAULT_MAX_URLENGTH                256
#define DPS_DEFAULT_PS                          10
#define DPS_DEFAULT_MAX_ASPELL                  16
#define DPS_READ_TIMEOUT			30
#define DPS_DOC_TIMEOUT				90
#define DPS_MAXNETERRORS			16
#define DPS_DEFAULT_NET_ERROR_DELAY_TIME	86400
#define DPS_DEFAULT_BAD_SINCE_TIME              15*24*60*60     /**< 15 days */
#define DPS_FINDURL_CACHE_SIZE                  1024
#define DPS_SERVERID_CACHE_SIZE                 256
#define DPS_ROBOTS_CACHE_SIZE                   64
#define	DPS_NET_BUF_SIZE		        65536
#define DPS_MAX_HOST_ADDR                       16
#define DPS_POPRANKSKIPSAMESITE                 "yes"
#define DPS_CROSSWORDSSKIPSAMESITE              "yes"
#define DPS_DETECTCLONES                        "no"

/***********************************/

#define DPS_LOGDIR	"raw"
#define DPS_TREEDIR     "tree"
#define DPS_SPLDIR      "splitter"
#define DPS_URLDIR      "url"


/** Forward declaration of DPS_AGENT */
struct dps_indexer_struct;
/** Forward declaration of DPS_STACK_ITEM */
struct dps_stack_item_struct;


/************************ Statistics **********************/
typedef struct stat_struct {
	int	        status;
        int	        expired;
	int	        total;
        dps_uint8           expired_size;
        dps_uint8           total_size;
} DPS_STAT;

typedef struct stat_list_struct{
	size_t		nstats;
	DPS_STAT	*Stat;
} DPS_STATLIST;

/* Unicode regex lite BEGIN */

typedef struct{
	int		type;
        int             rm_so, rm_eo;
	dpsunicode_t	*str;
} DPS_UNIREG_TOK;

typedef struct {
	size_t		ntokens;
	DPS_UNIREG_TOK	*Token;
} DPS_UNIREG_EXP;

/* Unicode regex lite END */

/************************ VARLISTs ************************/

typedef struct dps_var_st {
	char			*val;		/**< Field Value     */
        char                    *txt_val;       /**< Field Value in plain text */
	char			*name;		/**< Field Name      */
        int                     strict;         /**< strict word splitting */
        int                     single;         /**< single valued section, we will drop any second occurence to be processed.*/
	size_t			maxlen;		/**< Max length      */
	size_t			curlen;		/**< Cur length      */
	unsigned char		section;	/**< Number 0..255   */
} DPS_VAR;

typedef struct dps_varlist_st {
	int		freeme;
        struct {
	  size_t		nvars, mvars;
	  DPS_VAR		*Var;
	} Root[256];
} DPS_VARLIST;



typedef struct {
	char		*str;
	char		*href;
	const char	*section_name;
	int		section;
        int             strict;
        int             marked;
        size_t          len;
} DPS_TEXTITEM;

typedef struct {
	size_t		nitems, mitems;
	DPS_TEXTITEM	*Items;
} DPS_TEXTLIST;

/*****************************************************************/
typedef struct {
	int		match_type;
	int		nomatch;
        int             compiled;
        char            *section;
        char            *subsection;
	char		*pattern;
        size_t          pat_len;
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
        char            *idn_pattern;
        size_t          idn_len;
#endif
	char		*arg;
        char            *dbaddr;
	regex_t		*reg;
        DPS_UNIREG_EXP  UniReg;
	urlid_t         server_id;        /**< server.rec_id            */
	dps_uint2	case_sense;
        dps_uint2       last;
        dps_uint2       loose;
} DPS_MATCH;


typedef struct {
	int		match_type;
	int		nomatch;
        int             compiled;
        char            *section;
        char            *subsection;
	dpsunicode_t	*pattern;
#if (defined(WITH_IDN) || defined(WITH_IDNKIT)) && !defined(APACHE1) && !defined(APACHE2)
        dpsunicode_t    *idn_pattern;
#endif
        char	        *arg;
        char            *dbaddr;
        DPS_UNIREG_EXP  UniReg;
	urlid_t         server_id;        /**< server.rec_id            */
	dps_uint2	case_sense;
        dps_uint2       last;
} DPS_UNIMATCH;


typedef struct {
	size_t		nmatches;
	DPS_MATCH	*Match;
} DPS_MATCHLIST;


typedef struct {
	size_t		nmatches;
	DPS_UNIMATCH	*Match;
} DPS_UNIMATCHLIST;


typedef struct {
	int beg;
	int end;
} DPS_MATCH_PART;
/*****************************************************************/

/* word match type */
enum {
  DPS_MATCH_min    = 0,
  DPS_MATCH_FULL   = 0,
  DPS_MATCH_BEGIN  = 1,
  DPS_MATCH_SUBSTR = 2,
  DPS_MATCH_END    = 3,
  DPS_MATCH_REGEX  = 4,
  DPS_MATCH_WILD   = 5,
  DPS_MATCH_SUBNET = 6,
  DPS_MATCH_max    = 7
};

/*****************************************************/

/** StopList unit */
typedef struct dps_stopword_struct {
	char		*word;
	char		*lang;
	dpsunicode_t	*uword;
	size_t		len, ulen;
} DPS_STOPWORD;

typedef struct {
	size_t		 nstopwords;
	DPS_STOPWORD	 *StopWord;
        DPS_UNIMATCHLIST StopMatch;      /**< Stopword patterns list     */ 
} DPS_STOPLIST;

/*****************************************************/

/** Words parameters */
typedef struct {
	size_t		min_word_len;
	size_t		max_word_len;
	size_t		correct_factor;
	size_t		incorrect_factor;
} DPS_WORDPARAM;


#define DPS_N_COUNT    0
#define DPS_N_ORIGIN   1
#define DPS_N_PHRASE   2
#define DPS_N_EXACT    3
#ifdef WITH_REL_DISTANCE
#define DPS_N_DISTANCE (DPS_N_EXACT + 1)
#else
#define DPS_N_DISTANCE DPS_N_EXACT
#endif

#ifdef WITH_REL_POSITION
#define DPS_N_POSITION (DPS_N_DISTANCE + 1)
#define DPS_N_FIRSTPOS (DPS_N_DISTANCE + 2)
#else
#define DPS_N_POSITION DPS_N_DISTANCE
#define DPS_N_FIRSTPOS DPS_N_DISTANCE
#endif

#ifdef WITH_REL_WRDCOUNT
#define DPS_N_WRDCOUNT (DPS_N_FIRSTPOS + 1)
#else
#define DPS_N_WRDCOUNT DPS_N_FIRSTPOS
#endif

#define DPS_N_ADD (DPS_N_WRDCOUNT + 1)


/** Main search structure */
typedef struct{
	urlid_t		url_id;
	dps_uint4	coord;
} DPS_URL_CRD;


#ifdef WITH_MULTIDBADDR
typedef struct{
	urlid_t		url_id;
	dps_uint4	coord;
        dps_uint2       dbnum;
} DPS_URL_CRD_DB;
#else
#define DPS_URL_CRD_DB DPS_URL_CRD
#endif

typedef struct {
        urlid_t         url_id;
        urlid_t         site_id;
        dps_uint8       last_mod_time;
        double          pop_rank;
} DPS_URLDATA;


#ifdef WITH_REL_TRACK
typedef struct {
  double x, xy, y;
#ifdef WITH_REL_DISTANCE
  dps_uint4 D_distance;
#endif
#ifdef WITH_REL_POSITION
  dps_uint4 D_position;
  dps_uint4 D_firstpos;
#endif
#ifdef WITH_REL_WRDCOUNT
  dps_uint4 D_wrdcount;
#endif
  dps_uint4 D_n_count;
  dps_uint4 D_n_origin;
} DPS_URLTRACK;
#endif


typedef struct {
  size_t nrec;
/*  time_t mtime;*/
  DPS_URLDATA *URLData;
} DPS_URLDATA_FILE;

typedef struct {
	size_t		ncoords;
	size_t		order;
	char		*word;
#ifdef WITH_MULTIDBADDR
        DPS_URL_CRD_DB  *Coords;
#else
	DPS_URL_CRD	*Coords;
#endif
        DPS_URLDATA	*Data;
#ifdef WITH_REL_TRACK
        DPS_URLTRACK    *Track;
#endif
} DPS_URLCRDLIST;

typedef struct {
	int		freeme;
	size_t		nlists;
	DPS_URLCRDLIST	*List;
} DPS_URLCRDLISTLIST;

/** Word list unit */
typedef struct {
	dps_uint4	coord;
/*	dpshash32_t	crcword;*/
/*        char            *word;*/
	dpsunicode_t	*uword;
/*        size_t		len;*/
        size_t          ulen;
} DPS_WORD;

typedef struct {
	size_t		mwords;	/**< Number of memory allocated for words     */
	size_t		nwords;	/**< Real number of words in list             */
	size_t		swords;	/**< Number of words in sorted list           */
	size_t		wordpos;/**< For phrases, number of current word      */
	DPS_WORD	*Word;	/**< Word list  itself                        */
} DPS_WORDLIST;


#define DPS_WRDCOORD(p,w)	( (((dps_uint4)(p)) << 16) + (((dps_uint4)(w)) << 8) )
#define DPS_WRDCOORDL(p,w,l)	( (((dps_uint4)(p)) << 16) + (((dps_uint4)(w)) << 8) + (((dps_uint4)(l)) & 255) )
#define DPS_WRDSEC(c)		( ((c) >> 8) & 0xFF )
#define DPS_WRDPOS(c)		( (c) >> 16 )
#define DPS_WRDNUM(c)		( (c) & 0xFF )
#define DPS_WRDMASK(c)		( 1L << (((dps_uint4)(c)) & 0xFF) )


/***************************************************************/

/** Cross-word list unit */
typedef struct {
	size_t	pos;
	char	*url;
	urlid_t	referree_id;
	dpsunicode_t	*uword;
        size_t          ulen;
	short	weight;
} DPS_CROSSWORD;

typedef struct {
	size_t		ncrosswords;
	size_t		mcrosswords;
	size_t		wordpos;
	DPS_CROSSWORD	*CrossWord;
} DPS_CROSSLIST;

/*****************************************************************/

typedef union {
  struct {
    char min;  /* minute, 0-59 */
    char hour; /* hour, 0-23 */
    char day;  /* day of month, 1-31 */
    char month;/* month, 1-12 */
    char wday; /* day of week, 0-6, 0 - Sunday */
  } cron;
  dps_uint8 eight;
} DPS_EXPIRE;

/** Structure to store server parameters */
typedef struct {
	time_t	        period[DPS_DEFAULT_MAX_HOPS];		/**< Reindex period by hops          */
	DPS_MATCH	Match;
        DPS_MATCHLIST   HTDBsec;
	urlid_t         site_id;        /**< server.rec_id            */
	size_t          ordre;          /**< order in list to find    */
	urlid_t         parent;         /**< parent rec_id for grouping by site */
	float           weight;         /**< server weight for popularity rank calculation */
        float           MinServerWeight;/**< minimum weight for Server/Realm/Subnet to be indexed */
        float           MinSiteWeight;  /**< minimum weight for site to be indexed */
	DPS_VARLIST	Vars;		/**< Default lang, charset,etc*/
        DPS_EXPIRE      ExpireAt;
        dps_uint4       MaxHops;
        dps_uint4       ndocs;
        dps_uint4       nhrefs;
        dps_uint4       MaxDocsPerServer;/**< Maximum number of document from Server at one indexer run */
        dps_uint4       MaxDocsPerSite;/**< Maximum number of document from Server at one indexer run */
        dps_uint4       MaxHrefsPerServer;/**< Maximum number of href from Server at one indexer run */
        dps_uint4       MaxDepth;
        dps_uint4       MaxURLength;
        time_t          crawl_delay;     /**< Delay between consecutive fetches from this server, in seconds */
        time_t          *last_crawled;
        int             need_free;      /**< =1, if need to free last_crawled */
        int             use_robots;
        int             bad_urls_checked;
	char            command;        /**< 'S' - server,realm, 'F' - disallow,allow */
} DPS_SERVER;

typedef struct {
	size_t		nservers;
	size_t		mservers;
        size_t          min_ordre;
        int             sorted;
	DPS_SERVER	*Server;
} DPS_SERVERLIST;


typedef struct {
  DPS_EXPIRE    ExpireAt;
	int	max_net_errors;
	int	net_error_delay_time;
	size_t	read_timeout;
	size_t	doc_timeout;
	int	maxhops;	/**< Max way in mouse clicks  */
	int	index;		/**< Whether to index words   */
	int	follow;		/**< Whether to follow links  */
	int	use_robots;	/**< Whether to use robots.txt*/
	int	use_clones;	/**< Whether to detect clones */
	int	use_cookies;	/**< Whether to store cookies */
        DPS_SERVER    *Server;
} DPS_SPIDERPARAM;


/*******************************************************/
/* All links are stored in the cache of this structure */
/* before actual INSERT into database                  */

typedef struct {
	char    *url;
        urlid_t referrer;
	dps_uint4	hops;
	int	stored;	
        int     checked;
	int	method;
        int     charset_id;
        int     delay;
        urlid_t site_id;
        urlid_t server_id;
        float   weight;
} DPS_HREF;

typedef struct {
	size_t		mhrefs;
	size_t		nhrefs;
	size_t		shrefs;
	size_t		dhrefs;
	DPS_HREF	*Href;
} DPS_HREFLIST;

/*******************************************************/

/** Resolve stuff */
typedef struct dps_host_addr_struct {
	const char		*hostname;
	struct sockaddr_in	addr[DPS_MAX_HOST_ADDR];
        size_t          naddr;
	int		net_errors;
        int             charset_id;
	time_t		last_used;
} DPS_HOST_ADDR;

typedef struct {
	size_t		nhost_addr;
	size_t		mhost_addr;
	DPS_HOST_ADDR	*host_addr;
} DPS_HOSTLIST;


typedef struct {
  struct sockaddr_in addr;
  time_t last_used;
  int    neterrors;
} DPS_IP_ADDR;

typedef struct {
  size_t nip_addr;
  size_t mip_addr;
  DPS_IP_ADDR *ip_addr;
} DPS_IPLIST;


/** Used in FTP sessions */
typedef struct dps_conn_struct {
        int	status;
        int	connected;
        int	err;
        int	retry;
        int	conn_fd;
        int	port;
        size_t	timeout;
        char	*hostname;
        char    *user;
        char    *pass;
        struct	sockaddr_in sin;
        struct  sockaddr_in sinaddr[DPS_MAX_HOST_ADDR];
        size_t  n_sinaddr;
/*        size_t  naddr;*/
        size_t	buf_len_total;
        int	buf_len;
        int	len;
        int     charset_id;
        char	*buf;
        DPS_HOST_ADDR *Host;
        DPS_IP_ADDR   *IP;
        struct	dps_conn_struct *connp;
} DPS_CONN;

/** Parsed URL string */
typedef struct dps_url {
	char	*schema;
	char	*specific;
	char	*hostinfo;
	char	*auth;
	char	*hostname;
	char	*path;
        char    *directory;
	char	*filename;
	char	*anchor;
        char    *query_string;
	int	port;
	int	default_port;
        int     charset_id;
        int     freeme;
        int     domain_level;
        size_t  len;
} DPS_URL;


/***************************************************/

typedef struct {
	char	*buf;		/**< Buffer to download document to          */
	char	*content;	/**< Pointer to content, after headers       */
        char    *pattern;       /**< content with a pattern applied          */
	size_t	size;		/**< Number of bytes loaded                  */
        size_t  allocated_size; /**< Number of bytes allocated               */
	size_t	max_size;	/**< Maximum bytes to load into buf          */
} DPS_HTTPBUF;

typedef struct {
	int	freeme;		/**< Whether  memory was allocated for doc   */
	int	stored;		/**< If it is already stored, forAddHref()   */
	int	method;		/**< How to download document: GET, HEAD etc */
        int     fetched;        /**< Whether Doc's info was fetched or VaryLang is processing */
        int     charset_id;     /**< Document's charset ID                   */
        int     subdoc;         /**< Subdocument level                       */
        int     sd_cnt;         /**< Number of subdocuments                  */
    dpshash32_t id;             /**< Hash32(url) for seding and rec_id in special mode */
	
        DPS_SERVER              *Server;

	DPS_HTTPBUF		Buf;		/**< Buffer       */
	
	DPS_HREFLIST		Hrefs;		/**< Link list    */
	DPS_WORDLIST		Words;		/**< Word list    */
	DPS_CROSSLIST		CrossWords;	/**< Crosswords   */
	
	DPS_VARLIST		RequestHeaders;	/**< Extra headers*/
	DPS_VARLIST		Sections;	/**< User sections*/
	
	DPS_TEXTLIST		TextList;	/**< Text list    */
	DPS_TEXTLIST		ExtractorList;	/**< Text list    */

	DPS_URL			CurURL;		/**< Parsed URL   */
	DPS_CHARSET		*lcs;		/**< LocalCharser */
	DPS_SPIDERPARAM		Spider;		/**< Spider prms  */
	DPS_CONN		connp;		/**< For FTP      */
        DPS_LANGMAP             *lang_cs_map;   /**< Language map for detected language and charset, used in SEA */
	
        dps_uint2               dbnum;
} DPS_DOCUMENT;

/********************************************************/

/** External Parsers */
typedef struct dps_parser_struct{
        const char	*from_mime;
	const char	*to_mime;
	const char	*cmd;
} DPS_PARSER;

typedef struct {
	size_t		nparsers;
	DPS_PARSER	*Parser;
} DPS_PARSERLIST;


/* Ispell BEGIN */


typedef struct spell_struct {
	dpsunicode_t	*word;
	char		flag[11];
	char		lang[6];
} DPS_SPELL;


typedef struct aff_struct {
	DPS_UNIREG_EXP	reg;
	dpsunicode_t	mask[41];
        dpsunicode_t	find[16];
	dpsunicode_t	repl[16];
	size_t		replen;
        size_t		findlen;
        char		flag[3]; /**< 2 bytes for japanese extension */
	char		type;
	char		lang[6];
        char		compile;
} DPS_AFFIX;


typedef struct Tree_struct {
	int		Left[256];
	int		Right[256];
        char		lang[3];
} Tree_struct;

typedef struct {
	Tree_struct	PrefixTree[DPS_LANGPERDOC];
	Tree_struct	SuffixTree[DPS_LANGPERDOC];
	DPS_AFFIX	*Affix;
	size_t		naffixes;
	size_t		maffixes;
        int             sorted;
} DPS_AFFIXLIST;

typedef struct {
	Tree_struct	SpellTree[DPS_LANGPERDOC];
	DPS_SPELL	*Spell;
	size_t		nspell;
	size_t		mspell;
        size_t          nLang;
        int             sorted;
} DPS_SPELLLIST;

/* Ispell END */

typedef struct qreg_struct {
  DPS_UNIREG_EXP reg;
  dpsunicode_t	 mask[41];
  dpsunicode_t	 find[16];
  dpsunicode_t	 repl[16];
  size_t	 replen;
  size_t	 findlen;
  char		 flag[3]; /**< 2 bytes for japanese extension */
  char		 lang[6];
  char           compile;
} DPS_QUFFIX;

typedef struct {
	Tree_struct	PrefixTree[DPS_LANGPERDOC];
	Tree_struct	SuffixTree[DPS_LANGPERDOC];
	DPS_QUFFIX	*Quffix;
	size_t		nrecs;
	size_t		mrecs;
        int             sorted;
} DPS_QUFFIXLIST;



typedef struct{
	int		cmd; /**< 'allow' or 'disallow' */
        char	        *path;
        size_t          len;
} DPS_ROBOT_RULE;

typedef struct {
    time_t time;
    size_t ref_cnt;
} DPS_ROBOT_CRAWL;

typedef struct{
	const char	*hostinfo;
	size_t		nrules;
        time_t          crawl_delay;
        DPS_ROBOT_CRAWL *last_crawled;
	DPS_ROBOT_RULE	*Rule;
} DPS_ROBOT;

typedef struct{
	size_t		nrobots;
	DPS_ROBOT	*Robot;
} DPS_ROBOTS;


typedef struct {
        char            *domain;
        char            *name;
        char            *value;
        char            *path;
        char            secure;
        char            from_config;
} DPS_COOKIE;

typedef struct {
        size_t          ncookies;
        DPS_COOKIE      *Cookie;
} DPS_COOKIES;


typedef struct dps_search_limit {
	char		 file_name[PATH_MAX];
        char             cgi_var[32];
	int		 type;
        int              origin;
        int              need_free;
        int              second_hand;
        size_t           size;
        size_t           total_size;
        size_t           start;
        size_t           next;
	dps_uint4	 hi;
	dps_uint4	 lo;
        dps_uint4        f_hi;
        dps_uint4        f_lo;
        urlid_t          *data;
#ifdef HAVE_PTHREAD
        pthread_t        tid;
#endif
} DPS_SEARCH_LIMIT;


typedef struct {
        dps_uint4	order, order_inquery;
	dps_uint4	count;
	dps_uint4	len, ulen;
        dps_int4    	origin;
	dpshash32_t	crcword;
	char		*word;
	dpsunicode_t	*uword;
} DPS_WIDEWORD;

typedef struct {
        dps_uint4	order, order_inquery;
	dps_uint4	count;
	dps_uint4	len, ulen;
        dps_int4    	origin;
	dpshash32_t	crcword;
} DPS_WIDEWORD_EX;


typedef struct {
	dps_uint4	nuniq;
	dps_uint4	nwords;
        dps_uint4       maxulen;
	DPS_WIDEWORD	*Word;
} DPS_WIDEWORDLIST;

typedef struct {
	dps_uint4	nuniq;
	dps_uint4	nwords;
        dps_uint4       maxulen;
} DPS_WIDEWORDLIST_EX;


typedef struct {
	DPS_WIDEWORD	p;
	DPS_WIDEWORD	s;
} DPS_SYNONYM;

typedef struct {
	DPS_SYNONYM	*Synonym;
	DPS_SYNONYM	**Back;
	size_t		nsynonyms;
	size_t		msynonyms;
        int             sorted;
} DPS_SYNONYMLIST;



typedef struct {
  DPS_WIDEWORD     a;
  DPS_WIDEWORDLIST unroll;
} DPS_ACRONYM;

typedef struct {
	DPS_ACRONYM	*Acronym;
	size_t		nacronyms;
	size_t		macronyms;
        int             sorted;
} DPS_ACRONYMLIST;



typedef struct dps_chinaword_struct {
  dpsunicode_t *word;
  int          freq;
} DPS_CHINAWORD;

typedef struct {
  size_t        nwords, mwords;
  size_t        total;
  DPS_CHINAWORD *ChiWord;
  size_t        *hash;
} DPS_CHINALIST;


typedef struct dps_category_struct {
	int		rec_id;
	char		path[128];
	char		link[128];
	char		name[128];
} DPS_CATITEM;

typedef struct {
	char		addr[128];
	size_t		ncategories;
	DPS_CATITEM	*Category;
} DPS_CATEGORY;


#include "dps_db_int.h"
#include "dps_base.h"

typedef struct {
  DPS_BASE_PARAM BASEP;
  struct dps_stack_item_struct **pmerg;
  DPS_LOGDEL *del_buf;
  int *wf;
  size_t del_count;
  int flag_null_wf;
} DPS_WRD_CFG;


typedef struct dps_stack_item_struct {
        int		cmd, secno;
        int             origin, order_origin;
  /*	unsigned long	arg;          .order now */
#ifndef S_SPLINT_S
  DPS_URL_CRD_DB *pbegin, *pcur, *plast, *pchecked;
#endif
  /*  char gap[8192];*/
  DPS_URL_CRD           *db_pbegin;
  /*  char gap2[8192];*/
  DPS_URL_CRD *db_pcur, *db_plast, *db_pchecked;
        size_t          order, order_inquery;
        size_t          wordnum;
        size_t          count;
        size_t          len, ulen;
        size_t          order_from, order_to;
        dpshash32_t     crcword;
        char            *word;
        dpsunicode_t    *uword;
#ifdef HAVE_PTHREAD
        pthread_t       thread;
        DPS_WRD_CFG WrdCfg;
#endif
} DPS_STACK_ITEM;

typedef struct {
	size_t		ncstack, mcstack;
	int		*cstack;
	size_t		nastack, mastack;
	DPS_STACK_ITEM	*astack;
        int             freeme;
} DPS_BOOLSTACK;

typedef struct {
        size_t		        total_found, grand_total;
	size_t			work_time;
	size_t			first;
	size_t			last;
        size_t                  fetched;
	size_t			num_rows;
	size_t			cur_row;
	size_t			offset;
	size_t			memused;
        size_t                  *PerSite;
	int			freeme;
	DPS_DOCUMENT		*Doc;
        char                    *Suggest;
	
	DPS_WIDEWORDLIST	WWList;
	DPS_URLCRDLIST		CoordList;
	
	/* Bool stuff */
  size_t			nitems, mitems, ncmds, orig_nitems, max_order, max_order_inquery;
        int                     phrase, prepared;
	DPS_STACK_ITEM		*items;
	
} DPS_RESULT;



typedef struct {
	size_t		nitems;
        size_t          currdbnum;
        size_t          cnt_db;
        size_t          dbfrom;
        size_t          dbto;
	DPS_DB		**db;
} DPS_DBLIST;


enum dps_indcmd {
  DPS_IND_INDEX,
  DPS_IND_STAT,
  DPS_IND_CREATE,
  DPS_IND_DROP,
  DPS_IND_DELETE,
  DPS_IND_REFERERS,
  DPS_IND_SQLMON,
  DPS_IND_CHECKCONF,
  DPS_IND_CONVERT,
  DPS_IND_DOCINFO,
  DPS_IND_POPRANK,
  DPS_IND_RESORT,
  DPS_IND_REHASHSTORED,
  DPS_IND_SITEMAP,
  DPS_IND_FILTER
};

enum dps_prmethod {
  DPS_POPRANK_GOO,
  DPS_POPRANK_NEO
};

typedef struct {
  struct sockaddr_in    bind_addr;        /**< address for outbound connections      */
        time_t          hold_cache;       /**< How time in secs hold search cache    */
        time_t          robots_period;    /**< How time in secs hold robots.txt data */
        size_t          GuesserBytes;     /**< Number of bytes used for language and charset guessing */
        size_t          SEASentences;     /**< Maximal number of sentenses using by SEA */
        size_t          SEASentenceMinLength; /**< minimal length of the sentence to use in SEA */
        size_t          MaxCrawlDelay;    /**< Maximum Crawl-delay seconds to wait */
        int             do_store;         /**< Compressed copies storage flag        */
        int             do_excerpt;       /**< Document Excerpts making flag         */
	int		CVS_ignore;	  /**< Skip CVS directgories - for tests     */
        int             collect_links;    /**< Collect links flag                    */
        int             use_crc32_url_id; /**< UseCRC32URLId                         */
        int             use_crosswords;
        int             use_newsext;
        int             use_accentext;
        int             use_aspellext;
        int             use_meta;
        int             update_lm;
        int             provide_referer;
        int             make_prefixes;    /**< Make word prefixes for cache mode */
        int             make_suffixes;    /**< Make word suffixes for cache mode */
        int             fill_dictionary;  /**< Fill "dict" table in dbmode cache */
        int             OptimizeAtUpdate;
        int             PreloadURLData;
        int             cold_var;         /**< Do not use file locking for read-only operations */
        int             PopRankNeoIterations;
        int             skip_unreferred;
        int             rel_nofollow;     /**< Flag to obi rel="nofollow" attribute */
        int             track_hops;
        int             poprank_postpone; /**< Skip the Neo PopRank calculation at indexing */
        int             limits;           /**< mask of defined cache mode limits */
        int             nmaps;
        int             URLInfoSQL;       /**< Store URLInfo into SQL-base for cache mode */
        int             SRVInfoSQL;       /**< Store SRVInfo into SQL-base */
        int             CheckInsertSQL;   /**< Check before INSERT new record */
        int             mark_for_index;
        int             use_date_header;  /**< Use Date: HTTP header if Last-Modified: is not specified */
        int             MaxSiteLevel;     /**< Maximum level of hostname for site_id */
        int             Resegment;        /**< Resegmenting flags for East-Asian languages */
        int             PagesInGroup;     /**< Number of additional pages from same site when google-like groupping is enabled */
        int             LongestTextItems; /**< Number of longest text items to index */
        int             SubDocLevel;      /**< Maximum nested level for sub-documents */
        int             SubDocCnt;        /**< Maximum number of subdocuments to be indexed */
        dps_uint4       SkipHrefIn;       /**< Flag to skip some HTML tags from new href lookup */
        dps_uint4       expire;           /**< Flag to process to expired documents */
   enum dps_prmethod    poprank_method;
   enum dps_indcmd      cmd;
} DPS_FLAGS;

/** Config file */
typedef struct dps_config_struct {
	int		freeme;
	char		errstr[2048];
	DPS_CHARSET	*bcs;
	DPS_CHARSET	*lcs;
	
        int	        url_number;	/**< For indexer -nXXX          */
        int             url_size;       /**< For indexer -GXXX          */
	
	DPS_SERVERLIST	Servers[DPS_MATCH_max];	/**< List of servers and realms */
        DPS_SERVER      *Cfg_Srv;
        DPS_SERVER      **SrvPnt;
        int             total_srv_cnt;  /**< total number of servers    */
	DPS_ROBOTS	Robots;		/**< robots.txt information     */
	
	DPS_MATCHLIST	Aliases;	/**< Straight aliases           */
	DPS_MATCHLIST	ReverseAliases;	/**< Reverse aliases            */
	DPS_MATCHLIST	MimeTypes;	/**< For AddType commands       */
	DPS_MATCHLIST	Filters;	/**< Allow, Disallow,etc        */
	DPS_MATCHLIST	SectionFilters;	/**< IndexIf, NoIndexIf, etc    */
	DPS_MATCHLIST	StoreFilters;	/**< Store, NoStore, etc        */
        DPS_MATCHLIST   SectionMatch;   /**< Section's patterns         */
        DPS_MATCHLIST   HrefSectionMatch;   /**< HrefSection's patterns */
	DPS_MATCHLIST	SubSectionMatch;/**< TagIf, CategoryIf          */
        DPS_MATCHLIST   BodyPatterns;   /**< Body extraction patterns   */
        DPS_MATCHLIST   ActionSQLMatch; /**< ActionSQL patterns         */
	DPS_MATCHLIST	QAliases;	/**< query word aliases         */
        DPS_MATCHLIST   SectionSQLMatch;/**< SectionSQL queries         */
	
	DPS_RESULT	Targets;	/**< Targets cache              */
	
	DPS_VARLIST	Sections;	/**< document sections to parse */
	DPS_VARLIST	HrefSections;	/**< document href sections     */
	DPS_VARLIST	Vars;		/**< Config parameters          */
	
	DPS_LANGMAPLIST	LangMaps;	/**< For lang+charset quesser   */
	DPS_SYNONYMLIST	Synonyms;	/**< Synonyms list              */
        DPS_ACRONYMLIST	Acronyms;	/**< Acronyms list              */
	DPS_STOPLIST	StopWords;	/**< Stopwords list             */
	DPS_PARSERLIST	Parsers;	/**< External  parsers          */
	DPS_DBLIST	dbl;		/**< DB addresses	        */
	DPS_SPELLLIST	Spells;		/**< For ispell dictionaries    */
	DPS_AFFIXLIST	Affixes;	/**< For ispell affixes         */
        DPS_QUFFIXLIST	Quffixes;	        /**< For query regular expressions */
	DPS_WORDPARAM	WordParam;	/**< Word limits                */
        DPS_CHINALIST   Chi;            /**< Chinese words list         */
        DPS_CHINALIST   Thai;           /**< Thai words list            */
        DPS_CHINALIST   Korean;         /**< Korean words list          */
        DPS_FLAGS       Flags;
        dps_uint8           flags;
        DPS_URLDATA_FILE **URLDataFile;  /**< url data preloaded         */
        char            *CharsToEscape; /**< characters to escape in output */
	
	/* Various file descriptors */
        int             logs_only;      /**< Cache mode writes mode            */
	int		is_log_open;	/**< if DpsOpenLog is already called   */
	FILE		*logFD;		/**< FILE structure, syslog descriptor */

	void (*ThreadInfo)(struct dps_indexer_struct *,const char * state,const char * str);
	void (*LockProc)(struct dps_indexer_struct *, int command, size_t type, const char *fname, int lineno);
	void (*RefInfo)(int code,const char *url, const char *ref);

#ifdef MECAB
        mecab_t         *mecab;
#endif

} DPS_ENV;


typedef struct {
        int             stored_sd;      /* stored connection socket descriptors */
        int             stored_rv;
        int             cached_sd;      /* cached connection socket descriptors */
        int             cached_rv; 
} DPS_DEMONCONN;

typedef struct {
  size_t nitems;
  DPS_DEMONCONN *Demon;
} DPS_DEMONCONNLIST;

typedef struct {
  DPS_VARLIST  vars;
  DPS_VARLIST  *Env_Vars;
  const char   *HlBeg, *HlEnd; /**< template highlighting      */
  const char   *GrBeg, *GrEnd; /**< template same site quoting for grouping a-la google */
  const char   *SpBeg, *SpEnd; /**< template spelling error highlighting */
  const char   *ExcerptMark;   /**< delimiting mark for excerpts */
} DPS_TEMPLATE;

typedef struct {
        float   Weight;
        char    *Match_Pattern;
        urlid_t Id;
        size_t  Ndocs;
        char    Command;
        char    OnErrored;
} DPS_SERVERCACHE;

/** Indexer */
typedef struct dps_indexer_struct{
	int		freeme;		/**< whenever it was allocated    */
	int		handle;		/**< Handler for threaded version */
	time_t		start_time;	/**< Time of allocation, for stat */
        time_t          now;            /**< Time of current document processing */
	size_t		ndocs;		/**< Number of documents indexed  */
        size_t          poprank_docs;   /**< Number of documents popranked*/
	size_t  	nbytes;		/**< Number of bytes downloaded   */
        size_t          poprank_pas;    /**< Number of rounds of PopRank  */
        dps_uint8       nsleepsecs;     /**> Number of sleep seconds      */
	dps_uint8	flags;		/**< Running flags                */
        int             action;
	int		doccount;	/**< for DpsGetDocCount()         */
	DPS_ENV		*Conf;		/**< Configuration                */
	DPS_LANGMAP	*LangMap;	/**< LangMap for current document */
        DPS_RESULT      *Res;           /**< Search result pointer      */
	DPS_RESULT	Indexed;	/**< Indexed cache              */
	DPS_HREFLIST	Hrefs;		/**< Links cache                */
  DPS_DEMONCONNLIST     Demons;         /**< Daemons connections        */
	DPS_HOSTLIST	Hosts;		/**< Resolve cache              */
	DPS_DBLIST	dbl;		/**< DB addresses	      */
        DPS_TEMPLATE    tmpl;           /**< parsed template */
        DPS_TEMPLATE    st_tmpl;        /**< storedoc parsed template */
	DPS_VARLIST	Vars;		/**< Config parameters          */
	DPS_ROBOTS	Robots;		/**< robots.txt information     */
        DPS_COOKIES     Cookies;        /**< HTTP cookies information   */

        DPS_FLAGS       Flags;
	DPS_WORDPARAM	WordParam;	/**< Word limits                */
	
        DPS_SEARCH_LIMIT *limits;
        size_t          nlimits;

        int     SpellLang;

        char    *DpsFindURLCache[DPS_FINDURL_CACHE_SIZE];
        urlid_t DpsFindURLCacheId[DPS_FINDURL_CACHE_SIZE];
        urlid_t DpsFindURLCacheSiteId[DPS_FINDURL_CACHE_SIZE];
        int     DpsFindURLCacheHops[DPS_FINDURL_CACHE_SIZE];
        size_t  pURLCache;

        DPS_SERVERCACHE ServerIdCache[DPS_SERVERID_CACHE_SIZE];
        size_t  pServerIdCache;

        int     *Locked;  /**< is locked, how many times */
        void    *request; /**< Apache request */

        pid_t   resolver_pid; /**< 0 in resolver process, pid of resolver in parent process */ 
        int     rcv_pipe[2];  /**< pipe to receive resolving requests */
        int     snd_pipe[2];  /**< pipe to send resolving results */

        DPS_CONV uni_lc, lc_uni, lc_uni_text;

#ifdef HAVE_ASPELL
        DPS_CONV uni_utf, utf_uni, utf_lc;
        AspellConfig *aspell_config;
        pid_t   aspell_pid[DPS_DEFAULT_MAX_ASPELL];
        size_t  naspell;
#endif

#if defined(HAVE_LIBARES) || defined(HAVE_LIBCARES)
        ares_channel    channel;
#endif

#ifdef WITH_TRACE
        FILE *TR;
        int level;
/*        char timebuf[32];*/
#endif

#ifdef HAVE_PTHREAD
        unsigned int seed;
#endif
	
} DPS_AGENT;


typedef struct {
  DPS_AGENT Agent;
  pid_t     pid;
  int       status;
  int       generation;
} DPS_CHILD;

#define DPS_CHILDREN_LIMIT 16


typedef struct {
	char	*url;
	int	status;
} DPS_URLSTATE;

typedef int (*qsort_cmp)(const void*, const void*);

typedef struct {
	dps_uint4	hi,lo;
	dps_uint8	pos;
        size_t	len;
/*        size_t  orig_len;*/
} DPS_UINT8_POS_LEN;

typedef struct {
	dps_uint4	val;
	dps_uint8	pos;
        size_t	len;
/*        size_t  orig_len;*/
} DPS_UINT4_POS_LEN;

typedef struct {
	dps_uint4	val;
	urlid_t	url_id;
} DPS_UINT4URLID;

typedef struct {
        char            shm_name[PATH_MAX];
	size_t		nitems;
        int             mapped;
	DPS_UINT4URLID	*Item;
} DPS_UINT4URLIDLIST;

typedef struct {
	dps_uint4	hi,lo;
	urlid_t	url_id;
} DPS_UINT8URLID;

typedef struct {
        char            shm_name[PATH_MAX];
        size_t		nitems, mitems;
        int             mapped;
	DPS_UINT8URLID	*Item;
} DPS_UINT8URLIDLIST;


typedef struct {
	dps_uint4       cmd;
        dps_uint4       len;
} DPS_SEARCHD_PACKET_HEADER;


#define DPS_MAXTAGVAL	64

typedef struct {
	int	type;
	int	script;
	int	style;
	int	title;
	int	body;
	int	follow;
	int	index;
	int	comment;
	int     comment_inside;
	int	noindex;
        int     select;
        int     frameset;
        int     br;
	char	*lasthref;
        void    (*next_b)(void *t);
        void    (*next_e)(void *t);
        const char *e;
        const char *b;
        const char **lt;
        const char *s;
        int socket_sd, socket_rv;
        int chunks;
        char *Content;
        int finished;
        size_t  level;
	size_t	ntoks;
	struct {
		const char *name;
		const char *val;
		size_t     nlen;
		size_t     vlen;
	} toks[DPS_MAXTAGVAL+1];
        unsigned char visible[1024];
        char trail[4096];
        char *trailend;
        unsigned char section[1024], strict[1024];
        char *section_name[1024];
} DPS_HTMLTOK;

typedef struct dps_cfg_st {
        DPS_AGENT       *Indexer;
	DPS_SERVER	*Srv;
	dps_uint8	flags;
	int		level;
	int		ordre;
        int             flush_server;
} DPS_CFG;

typedef int (*DPS_OUTPUTFUNCTION)(void*, const char *fmt, ...);


typedef struct {
  int cmd;
  int add_cmd;
  int origin;
  int sp;
  int sy;
  int have_bukva_forte;
  int *secno, p_secno, n_secno;
  int nphrasecmd;
  int autophrase;
  ssize_t order, order_inquery;
  const char *qlang;
} DPS_PREPARE_STATE;



/* Indexer return codes */
enum {
  DPS_OK           = 0,
  DPS_ERROR        = 1,
  DPS_NOTARGET     = 2,
  DPS_TERMINATED   = 3,
  DPS_RELOADCONFIG = 4
};

/* storage types */
enum {
  
  DPS_DBMODE_SINGLE     = 0,
  DPS_DBMODE_MULTI      = 1,
  DPS_DBMODE_SINGLE_CRC = 2,
  DPS_DBMODE_MULTI_CRC  = 3,
  DPS_DBMODE_CACHE      = 4
};

/* database open modes */
enum {
  DPS_OPEN_MODE_READ  = 0,
  DPS_OPEN_MODE_WRITE = 1
};

/* search modes */
enum {
  DPS_MODE_ALL	  = 0,
  DPS_MODE_ANY	  = 1,
  DPS_MODE_BOOL	  = 2,
  DPS_MODE_PHRASE = 3,
  DPS_MODE_NEAR	  = 4
};

/* group by site modes */
enum {
  DPS_GROUP_NO = 0,
  DPS_GROUP_YES = 1,
  DPS_GROUP_FULL = 2
};


/* Flags for indexing */
enum {
  DPS_FLAG_SORT_EXPIRED	    = 1,
  DPS_FLAG_SORT_EXPIRED_REV = 2,
  DPS_FLAG_SORT_HOPS	    = 4,
  DPS_FLAG_SORT_HOPS_REV    = 8,
  DPS_FLAG_SORT_SEED        = 16,
  DPS_FLAG_SORT_SEED2       = 32,
  DPS_FLAG_SORT_POPRANK     = 64,
  DPS_FLAG_SORT_POPRANK_REV = 128,

  DPS_FLAG_REINDEX	= 1024,
  DPS_FLAG_ADD_SERV	= 2048,
  DPS_FLAG_SPELL	= 4096,
  DPS_FLAG_LOAD_LANGMAP	= 8192,
  DPS_FLAG_ADD_SERVURL	= 16384,
  DPS_FLAG_UNOCON       = 32768,
  DPS_FLAG_FROM_STORED     = 65536,
  DPS_FLAG_FAST_HREF_CHECK = 131072,
  DPS_FLAG_STOPWORDS_LOOSE = 262144
};

/* URLFile actions */
enum {
  DPS_URL_FILE_REINDEX	= 1,
  DPS_URL_FILE_CLEAR	= 2,
  DPS_URL_FILE_INSERT	= 3,
  DPS_URL_FILE_PARSE	= 4,
  DPS_URL_FILE_TARGET   = 5
};

/* Action type: HTTP methods */
enum {
  DPS_METHOD_UNKNOWN	= 0,
  DPS_METHOD_GET	= 1,
  DPS_METHOD_DISALLOW	= 2,
  DPS_METHOD_HEAD	= 3,
  DPS_METHOD_HREFONLY	= 4,
  DPS_METHOD_CHECKMP3	= 5,
  DPS_METHOD_CHECKMP3ONLY = 6,
  DPS_METHOD_VISITLATER	 = 7,
  DPS_METHOD_INDEX       = 8,
  DPS_METHOD_NOINDEX     = 9,
  DPS_METHOD_TAG         = 10,
  DPS_METHOD_CATEGORY    = 11,
  DPS_METHOD_CRAWLDELAY  = 12,
  DPS_METHOD_STORE       = 13,
  DPS_METHOD_NOSTORE     = 14,
  DPS_METHOD_HOST        = 15,
  DPS_METHOD_POST 	 = 16
};

/* Words origins */
enum {
  DPS_WORD_ORIGIN_QUERY   = 1,
  DPS_WORD_ORIGIN_SPELL   = 2,
  DPS_WORD_ORIGIN_SYNONYM = 4,
  DPS_WORD_ORIGIN_ACRONYM = 8,
  DPS_WORD_ORIGIN_STOP    = 16,
  DPS_WORD_ORIGIN_ACCENT  = 32,
  DPS_WORD_ORIGIN_ASPELL  = 64,
  DPS_WORD_ORIGIN_COMMON  = 128
};

/* Locking mutex numbers */
enum {
  DPS_LOCK_CONF		= 0,
  DPS_LOCK_THREAD       = 1,
  DPS_LOCK_SEGMENTER    = 2,
  DPS_LOCK_DB           = 3,
  DPS_LOCK_RESOLV       = 4,
  DPS_LOCK_ROBOTS       = 5,
  DPS_LOCK_ASPELL       = 6,
  DPS_LOCK_BASE         = 7,
  DPS_LOCK_CACHED       = 8, /* should be the last */
  DPS_LOCK_MAX          = 9
};


enum {
  DPS_DT_BACK    = 1,
  DPS_DT_ER      = 2,
  DPS_DT_RANGE   = 3,
  DPS_DT_UNKNOWN = 4
};

enum {
  DPS_UNIREG_SUB = 1,
  DPS_UNIREG_BEG = 2,
  DPS_UNIREG_END = 3,
  DPS_UNIREG_INC = 4,
  DPS_UNIREG_EXC = 5
};

enum {
  DPS_SEARCHD_CMD_ERROR	= 1,
  DPS_SEARCHD_CMD_MESSAGE = 2,
  DPS_SEARCHD_CMD_WORDS	= 3,
  DPS_SEARCHD_CMD_GOODBYE = 4,
  DPS_SEARCHD_CMD_DOCINFO = 5,
  DPS_SEARCHD_CMD_WITHOFFSET = 7,
  DPS_SEARCHD_CMD_WWL =       8,
  DPS_SEARCHD_CMD_CATINFO    = 9,
  DPS_SEARCHD_CMD_URLACTION  = 10,
  DPS_SEARCHD_CMD_DOCCOUNT   = 11,
  DPS_SEARCHD_CMD_PERSITE    = 12,
  DPS_SEARCHD_CMD_DATA       = 13,
  DPS_SEARCHD_CMD_CLONES     = 14,
  DPS_SEARCHD_CMD_QLC        = 15,
  DPS_SEARCHD_CMD_SUGGEST    = 16,
  DPS_SEARCHD_CMD_TRACKDATA  = 17,
  DPS_SEARCHD_CMD_WORDS_ALL  = 18
};

enum {
  DPS_LOGD_CMD_WORD    = 0,
  DPS_LOGD_CMD_DATA    = 1,
  DPS_LOGD_CMD_CHECK   = 2,
  DPS_LOGD_CMD_URLINFO = 3,
  DPS_LOGD_CMD_FLUSH   = 4,
  DPS_LOGD_CMD_BYE     = 5,
  DPS_LOGD_CMD_DELETE  = 6,
  DPS_LOGD_CMD_NEWORD  = 7
};

enum {
  DPS_LOGDEL_FLAG_DELETE = 1
};

enum {
  DPS_RESEGMENT_CHINESE  = 1,
  DPS_RESEGMENT_JAPANESE = 2,
  DPS_RESEGMENT_KOREAN   = 4,
  DPS_RESEGMENT_THAI     = 8
};

enum dps_href_from {
  DPS_HREF_FROM_UNKNOWN = 0,
  DPS_HREF_FROM_A       = 1,
  DPS_HREF_FROM_AREA    = 2,
  DPS_HREF_FROM_BASE    = 4,
  DPS_HREF_FROM_FRAME   = 8,
  DPS_HREF_FROM_IFRAME  = 16,
  DPS_HREF_FROM_INPUT   = 32,
  DPS_HREF_FROM_IMG     = 64,
  DPS_HREF_FROM_LINK    = 128,
  DPS_HREF_FROM_SCRIPT  = 256
};


extern char dps_pid_name[];
extern unsigned int milliseconds; /* To sleep between documents    */
extern int log2stderr;


#define DPS_DBL_DB(A, i) ((A)->flags & DPS_FLAG_UNOCON) ? (A)->Conf->dbl.db[i] : (A)->dbl.db[i]
#define DPS_DBL_TO(A) ((A)->flags & DPS_FLAG_UNOCON) ? (A)->Conf->dbl.nitems : (A)->dbl.nitems

#define DPS_STATUS_UPPER(I) (((I)->Flags.SubDocLevel > 0) ? 400 : 300)
#define DPS_STATUS_IN_INDEX(u,s)  ((s >= 200 && s < u) || s == 304)
#define DPS_STATUS_NOT_INDEX(u,s) ((s < 200 || s >= u) && s != 304)

#endif /* _DPS_COMMON_H */
