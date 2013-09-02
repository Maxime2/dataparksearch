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

#ifndef DPS_UNICONV_H
#define DPS_UNICONV_H
/*
#define DEBUG_CONV 1
*/
#include <sys/types.h>

#define DPS_RECODE_TEXT_FROM            1
#define DPS_RECODE_TEXT_TO              2
#define DPS_RECODE_TEXT                 3 /* DPS_RECODE_TEXT_FROM | DPS_RECODE_TEXT_TO */
#define DPS_RECODE_HTML_FROM            4
#define DPS_RECODE_HTML_TO              8
#define DPS_RECODE_HTML                 12 /* DPS_RECODE_HTML_FROM | DPS_RECODE_HTML_TO */
#define DPS_RECODE_URL_FROM             16
#define DPS_RECODE_URL_TO               32
#define DPS_RECODE_URL                  48 /* DPS_RECODE_URL_FROM | DPS_RECODE_URL_TO */
#define DPS_RECODE_JSON_FROM            64
#define DPS_RECODE_JSON_TO              128
#define DPS_RECODE_JSON                 192 /* DPS_RECODE_JSON_FROM | DPS_RECODE_JSON_TO */


#define DPS_CHARSET_UNKNOWN             0
#define   DPS_CHARSET_ARABIC            1
#define   DPS_CHARSET_ARMENIAN          2
#define   DPS_CHARSET_BALTIC            3
#define   DPS_CHARSET_CELTIC            4
#define   DPS_CHARSET_CENTRAL           5
#define DPS_CHARSET_CHINESE_SIMPLIFIED  6
#define DPS_CHARSET_CHINESE_TRADITIONAL 7
#define   DPS_CHARSET_CYRILLIC          8
#define   DPS_CHARSET_GREEK             9
#define   DPS_CHARSET_HEBREW            10
#define   DPS_CHARSET_ICELANDIC         11
#define   DPS_CHARSET_JAPANESE          12
#define   DPS_CHARSET_KOREAN            13
#define   DPS_CHARSET_NORDIC            14
#define   DPS_CHARSET_SOUTHERN          15
#define   DPS_CHARSET_THAI              16
#define   DPS_CHARSET_TURKISH           17
#define   DPS_CHARSET_UNICODE           18
#define   DPS_CHARSET_VIETNAMESE        19
#define   DPS_CHARSET_WESTERN           20
#define DPS_CHARSET_INDIAN              21
#define DPS_CHARSET_GEORGIAN            22
#define DPS_CHARSET_LAO                 23
#define DPS_CHARSET_IRANIAN             24
#define DPS_CHARSET_TAJIK               25

typedef struct {
     int id;
     const char * name;
} DPS_CHARSETGROUP;

typedef struct {
	const	char *name;
	int   	id;
} DPS_CHARSET_ALIAS;

#define DPS_CHARSET_8859_1    0
#define DPS_CHARSET_8859_10   1
#define DPS_CHARSET_8859_11   2
#define DPS_CHARSET_8859_13   3
#define DPS_CHARSET_8859_14   4
#define DPS_CHARSET_8859_15   5
#define DPS_CHARSET_8859_16   6
#define DPS_CHARSET_8859_2    7
#define DPS_CHARSET_8859_3    8
#define DPS_CHARSET_8859_4    9
#define DPS_CHARSET_8859_5    10
#define DPS_CHARSET_8859_6    11
#define DPS_CHARSET_8859_7    12
#define DPS_CHARSET_8859_8    13
#define DPS_CHARSET_8859_9    14
#define DPS_CHARSET_ARMSCII_8 15
#define DPS_CHARSET_CP1250    16
#define DPS_CHARSET_CP1251    17
#define DPS_CHARSET_CP1252    18
#define DPS_CHARSET_CP1253    19
#define DPS_CHARSET_CP1254    20
#define DPS_CHARSET_CP1255    21
#define DPS_CHARSET_CP1256    22
#define DPS_CHARSET_CP1257    23
#define DPS_CHARSET_CP1258    24
#define DPS_CHARSET_CP437     25
#define DPS_CHARSET_CP850     26
#define DPS_CHARSET_CP852     27
#define DPS_CHARSET_CP855     28
#define DPS_CHARSET_CP857     29
#define DPS_CHARSET_CP860     30
#define DPS_CHARSET_CP861     31
#define DPS_CHARSET_CP862     32
#define DPS_CHARSET_CP863     33
#define DPS_CHARSET_CP864     34
#define DPS_CHARSET_CP865     35
#define DPS_CHARSET_CP866       36
#define DPS_CHARSET_CP869       37
#define DPS_CHARSET_CP874       38
#define DPS_CHARSET_KOI8_R      39
#define DPS_CHARSET_KOI8_U      40
#define DPS_CHARSET_MACARABIC   41
#define DPS_CHARSET_MACCE       42
#define DPS_CHARSET_MACCROATIAN 43
#define DPS_CHARSET_MACCYRILLIC 44
#define DPS_CHARSET_MACGREEK    45
#define DPS_CHARSET_MACHEBREW   46
#define DPS_CHARSET_MACICELAND  47
#define DPS_CHARSET_MACROMAN    48
#define DPS_CHARSET_MACROMANIA  49
#define DPS_CHARSET_MACTHAI     50
#define DPS_CHARSET_MACTURKISH  51
#define DPS_CHARSET_US_ASCII    52
#define DPS_CHARSET_VISCII      53
#define DPS_CHARSET_UTF8        54
#define DPS_CHARSET_GB2312      55
#define DPS_CHARSET_BIG5        56
#define DPS_CHARSET_SJIS        57
#define DPS_CHARSET_EUC_KR      58
#define DPS_CHARSET_EUC_JP      60
#define DPS_CHARSET_GBK         61
#define DPS_CHARSET_GUJARATI    62
#define DPS_CHARSET_TSCII       63
#define DPS_CHARSET_ISO2022JP   64
#define DPS_CHARSET_GEOSTD8     65
#define DPS_CHARSET_CP950       66
#define DPS_CHARSET_BIG5HKSCS   67
#define DPS_CHARSET_CP037       68
#define DPS_CHARSET_CP1026      69
#define DPS_CHARSET_CP500       70
#define DPS_CHARSET_CP875       71
#define DPS_CHARSET_CP1133      72
#define DPS_CHARSET_ISIRI3342   73
#define DPS_CHARSET_CP866U      74
#define DPS_CHARSET_KOI_7       75
#define DPS_CHARSET_UTF7        76
#define DPS_CHARSET_UTF16LE     77
#define DPS_CHARSET_UTF16BE     78
#define DPS_CHARSET_GB18030     79
#define DPS_CHARSET_CP775       80
#define DPS_CHARSET_KOI8_T      81
#define DPS_CHARSET_GEO_ACADEMY 82
#define DPS_CHARSET_GEO_PS      83
#define DPS_CHARSET_KOI8_C      84
#define DPS_CHARSET_SYS_INT    255

typedef struct {
     dpsunicode_t from;
     dpsunicode_t to;
     unsigned char  *tab;
} DPS_UNI_IDX;

struct dps_conv_st;

typedef struct dps_cset_st{
     int id;
     int (*mb_wc)(struct dps_conv_st *conv, struct dps_cset_st *cs, dpsunicode_t *wc, const unsigned char *s, const unsigned char *e);
     int (*wc_mb)(struct dps_conv_st *conv, struct dps_cset_st *cs, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
     const char * name;
     int family;
     dpsunicode_t *tab_to_uni;
     DPS_UNI_IDX    *tab_from_uni;
} DPS_CHARSET;

typedef struct dps_conv_st {
     DPS_CHARSET    *from;
     DPS_CHARSET    *to;
     char           *CharsToEscape;
     size_t         ibytes;
     size_t         obytes;
     size_t         icodes;
     size_t         ocodes;
     int       flags;
        int             istate;
        int             ostate;
} DPS_CONV;


/************** Language and charset guesser *************/


#define DPS_LM_MAXGRAM1		3
#define DPS_LM_MAXGRAM2		(3 * sizeof(dpsunicode_t))
#define DPS_LM_HASHMASK		0x07FF
#define DPS_LM_TOPCNT           150

typedef struct {
	size_t		count, index;
} DPS_LANGITEM;

typedef struct {
	DPS_LANGITEM	memb3[DPS_LM_HASHMASK+1];	/**< Items 3-list      */
	DPS_LANGITEM	memb6[DPS_LM_HASHMASK+1];	/**< Items 6-list      */
	float		expectation;			/**< Average value   */
        size_t          nbytes;                         /**< number of bytes processed */
        size_t          lang_len;
        int             needsave;
	char		*lang;				/**< Map Language    */
	char		*charset;			/**< Map charset     */
        char            *filename;                      /**< Filename to write updates, if need */
} DPS_LANGMAP;

typedef struct {
	size_t		nmaps;
	DPS_LANGMAP	*Map;
} DPS_LANGMAPLIST;

/*****************************************************/

/* Input string in xxx2uni                  */
/* convertion  has bad multi-byte sequence  */
#define DPS_CHARSET_ILSEQ     -1
#define DPS_CHARSET_ILSEQ2      -2
#define DPS_CHARSET_ILSEQ3      -3
#define DPS_CHARSET_ILSEQ4      -4
#define DPS_CHARSET_ILSEQ5      -5
#define DPS_CHARSET_ILSEQ6      -6

/* Input buffer in xxx2uni was terminated   */
/* in the middle of multi-byte sequence     */
#define DPS_CHARSET_TOOFEW(n) (-7-(n))

/* Can't convert unicode into given charset */
#define DPS_CHARSET_ILUNI     0

/* Output buffer in uni2xxx is too small    */
#define DPS_CHARSET_TOOSMALL  -1


extern DPS_CHARSET_ALIAS dps_cs_alias[];

extern __C_LINK const char * __DPSCALL DpsCsGroup(const DPS_CHARSET *cs);

extern int dps_mb_wc_utf8(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
extern int dps_wc_mb_utf8(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);

extern int dps_mb_wc_utf7(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
extern int dps_wc_mb_utf7(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);

extern int dps_mb_wc_utf16be(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
extern int dps_wc_mb_utf16be(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);

extern int dps_mb_wc_utf16le(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
extern int dps_wc_mb_utf16le(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);

extern int dps_mb_wc_8bit(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
extern int dps_wc_mb_8bit(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);

extern int dps_mb_wc_sys_int(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc,	const unsigned char *s, const unsigned char *e);
extern int dps_wc_mb_sys_int(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);


#ifdef HAVE_CHARSET_chinese
int dps_mb_wc_gb2312(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s,const unsigned char *e);
int dps_wc_mb_gb2312(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_japanese
int dps_mb_wc_sjis(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s,const unsigned char *e);
int dps_wc_mb_sjis(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char*s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_euc_kr
int dps_mb_wc_euc_kr(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_euc_kr(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_japanese
int dps_mb_wc_euc_jp(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_euc_jp(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_chinese
int dps_mb_wc_big5(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_big5(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
int dps_mb_wc_cp950(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_cp950(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc,  unsigned char *s, unsigned char *e);
int dps_mb_wc_big5hkscs(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_big5hkscs(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_chinese
int dps_mb_wc_gbk(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_gbk(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
int dps_mb_wc_gb18030(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_gb18030(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_gujarati
int dps_mb_wc_gujarati(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_gujarati(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_tscii
int dps_mb_wc_tscii(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_tscii(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_japanese
int dps_mb_wc_iso2022jp(DPS_CONV *conv, DPS_CHARSET *c, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e);
int dps_wc_mb_iso2022jp(DPS_CONV *conv, DPS_CHARSET *c, const dpsunicode_t *wc, unsigned char *s, unsigned char *e);
#endif


extern __C_LINK DPS_CHARSET * __DPSCALL DpsGetCharSet(const char * name);
extern __C_LINK DPS_CHARSET * __DPSCALL DpsGetCharSetByID(int id);
extern  const char * DpsCharsetCanonicalName(const char * alias);

extern __C_LINK void __DPSCALL DpsConvInit(DPS_CONV *c, DPS_CHARSET *from, DPS_CHARSET *to, const char *CharsToEscape, int fl);
#ifdef DEBUG_CONV
extern __C_LINK int  __DPSCALL _DpsConv(DPS_CONV *c, char *d, size_t dlen, const char *s, size_t slen, const char *file, int line);
#define DpsConv(c, d, dl, s, sl) _DpsConv(c, d, dl, s, sl, __FILE__, __LINE__)
#else
extern __C_LINK int  __DPSCALL DpsConv(DPS_CONV *c, char *d, size_t dlen, const char *s, size_t slen);
#endif
extern int  DpsNConv(DPS_CONV *c, size_t n, char *d, size_t dlen, const char *s, size_t slen);
extern size_t DpsUniConvLength(DPS_CONV *c, const char *s);
extern void DpsConvFree(DPS_CONV *c);

#endif
