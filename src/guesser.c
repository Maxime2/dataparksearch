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
/*
#define DEBUG_GUESSER
*/
#include "dps_common.h"
#include "dps_hash.h"
#include "dps_guesser.h"
#include "dps_utils.h"
#include "dps_unicode.h"
#include "dps_log.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_charsetutils.h"
#include "dps_xmalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif


DPS_CHARSET_BY_LANG dps_charset_lang[] = { /* This array is sorted by language name */
  {DPS_CHARSET_UTF8,    "ar"},             /* see: http://dev.w3.org/html5/spec/Overview.html#determining-the-character-encoding */
  {DPS_CHARSET_8859_5,  "be"},
  {DPS_CHARSET_CP1251,  "bg"},
  {DPS_CHARSET_8859_2,  "cs"},
  {DPS_CHARSET_UTF8,    "cy"},
  {DPS_CHARSET_UTF8,    "fa"},
  {DPS_CHARSET_CP1255,  "he"},
  {DPS_CHARSET_UTF8,    "hr"},
  {DPS_CHARSET_8859_2,  "hu"},
  {DPS_CHARSET_SJIS,    "ja"},  /* 	Windows-31J, which is CP932, Microsoft's extended Shift-JIS */
  {DPS_CHARSET_UTF8,    "kk"},
  {DPS_CHARSET_EUC_KR,  "ko"},  /* 	windows-949, which is Microsoft's variant of EUC-KR */
  {DPS_CHARSET_CP1254,  "ku"},
  {DPS_CHARSET_CP1257,  "lt"},
  {DPS_CHARSET_8859_13, "lv"},
  {DPS_CHARSET_UTF8,    "mk"},
  {DPS_CHARSET_UTF8,    "or"},
  {DPS_CHARSET_8859_2,  "pl"},
  {DPS_CHARSET_UTF8,    "ro"},
  {DPS_CHARSET_CP1251,  "ru"},
  {DPS_CHARSET_CP1250,  "sk"},
  {DPS_CHARSET_8859_2,  "sl"},
  {DPS_CHARSET_UTF8,    "sr"},
  {DPS_CHARSET_CP874,   "th"},
  {DPS_CHARSET_CP1254,  "tr"},
  {DPS_CHARSET_CP1251,  "uk"},
  {DPS_CHARSET_UTF8,    "vi"},
  {DPS_CHARSET_GB18030, "zh-CN"},
  {DPS_CHARSET_BIG5,    "zh-TW"},
                                /* All other locales 	windows-1252 */
  { 0, NULL }
};


DPS_LANG_ALIAS dps_language[] = { /* This table is ordered by language ID */
  {DPS_LANG_AB, "ab" },
  {DPS_LANG_ADY, "ady" },
  {DPS_LANG_AF, "af" },
  {DPS_LANG_AM, "am" },
  {DPS_LANG_AN, "an" },
  {DPS_LANG_ANG, "ang" },
  {DPS_LANG_AR, "ar" },
  {DPS_LANG_AST, "ast" },
  {DPS_LANG_AV, "av" },
  {DPS_LANG_AZ, "az" },
  {DPS_LANG_BA, "ba" },
  {DPS_LANG_BE, "be" },
  {DPS_LANG_BG, "bg"},
  {DPS_LANG_BM, "bm"},
  {DPS_LANG_BN, "bn"},
  {DPS_LANG_BR, "br"},
  {DPS_LANG_BS, "bs"},
  {DPS_LANG_CA, "ca"},
  {DPS_LANG_CE, "ce"},
  {DPS_LANG_CHR, "chr"},
  {DPS_LANG_CO, "co"},
  {DPS_LANG_CS, "cs"},
  {DPS_LANG_CSB, "csb"},
  {DPS_LANG_CV, "cv"},
  {DPS_LANG_CY, "cy"},
  {DPS_LANG_DA, "da"},
  {DPS_LANG_DE_AT, "de-at"},
  {DPS_LANG_DE_CH, "de-ch"},
  {DPS_LANG_DE, "de"},
  {DPS_LANG_EL, "el"},
  {DPS_LANG_EN_AU, "en-au"},
  {DPS_LANG_EN_NZ, "en-nz"},
  {DPS_LANG_EN_US, "en-us"},
  {DPS_LANG_EN, "en"},
  {DPS_LANG_EO, "eo"},
  {DPS_LANG_ES, "es"},
  {DPS_LANG_ET, "et"},
  {DPS_LANG_EU, "eu"},
  {DPS_LANG_FA, "fa"},
  {DPS_LANG_FI, "fi"},
  {DPS_LANG_FJ, "fj"},
  {DPS_LANG_FO, "fo"},
  {DPS_LANG_FR, "fr"},
  {DPS_LANG_FUR, "fur"},
  {DPS_LANG_FY, "fy"},
  {DPS_LANG_GA, "ga"},
  {DPS_LANG_GD, "gd"},
  {DPS_LANG_GL, "gl"},
  {DPS_LANG_GU, "gu"},
  {DPS_LANG_HAW, "haw"},
  {DPS_LANG_HE, "he"},
  {DPS_LANG_HI, "hi"},
  {DPS_LANG_HR, "hr"},
  {DPS_LANG_HT, "ht"},
  {DPS_LANG_HU, "hu"},
  {DPS_LANG_HY, "hy"},
  {DPS_LANG_IA, "ia"},
  {DPS_LANG_ID, "id"},
  {DPS_LANG_IE, "ie"},
  {DPS_LANG_IO, "io"},
  {DPS_LANG_IS, "is"},
  {DPS_LANG_IT, "it"},
  {DPS_LANG_IU, "iu"},
  {DPS_LANG_JA, "ja"},
  {DPS_LANG_JV, "jv"},
  {DPS_LANG_KA, "ka"},
  {DPS_LANG_KK, "kk"},
  {DPS_LANG_KN, "kn"},
  {DPS_LANG_KO, "ko"},
  {DPS_LANG_KS, "ks"},
  {DPS_LANG_KU, "ku"},
  {DPS_LANG_KV, "kv"},
  {DPS_LANG_KW, "kw"},
  {DPS_LANG_KY, "ky"},
  {DPS_LANG_LA, "la"},
  {DPS_LANG_LB, "lb"},
  {DPS_LANG_LI, "li"},
  {DPS_LANG_LT, "lt"},
  {DPS_LANG_LV, "lv"},
  {DPS_LANG_MG, "mg"},
  {DPS_LANG_MI, "mi"},
  {DPS_LANG_MK, "mk"},
  {DPS_LANG_ML, "ml"},
  {DPS_LANG_MN, "mn"},
  {DPS_LANG_MO, "mo"},
  {DPS_LANG_MR, "mr"},
  {DPS_LANG_MS, "ms"},
  {DPS_LANG_MT, "mt"},
  {DPS_LANG_NAH, "nah"},
  {DPS_LANG_NAP, "nap"},
  {DPS_LANG_NB, "nb"},
  {DPS_LANG_NDS, "nds"},
  {DPS_LANG_NE, "ne"},
  {DPS_LANG_NL, "nl"},
  {DPS_LANG_NN, "nn"},
  {DPS_LANG_NO, "no"},
  {DPS_LANG_NV, "nv"},
  {DPS_LANG_OC, "oc"},
  {DPS_LANG_OS, "os"},
  {DPS_LANG_PI, "pi"},
  {DPS_LANG_PL, "pl"},
  {DPS_LANG_PT_BR, "pt-br"},
  {DPS_LANG_PT, "pt"},
  {DPS_LANG_RM, "rm"},
  {DPS_LANG_RMY, "rmy"},
  {DPS_LANG_RO, "ro"},
  {DPS_LANG_RU, "ru"},
  {DPS_LANG_SA, "sa"},
  {DPS_LANG_SCN, "scn"},
  {DPS_LANG_SCO, "sco"},
  {DPS_LANG_SE, "se"},
  {DPS_LANG_SH, "sh"},
  {DPS_LANG_SIMPLE, "simple"},
  {DPS_LANG_SK, "sk"},
  {DPS_LANG_SL, "sl"},
  {DPS_LANG_SO, "so"},
  {DPS_LANG_SQ, "sq"},
  {DPS_LANG_SR, "sr"},
  {DPS_LANG_SU, "su"},
  {DPS_LANG_SV, "sv"},
  {DPS_LANG_SW, "sw"},
  {DPS_LANG_TA, "ta"},
  {DPS_LANG_TL, "tl"},
  {DPS_LANG_TG, "tg"},
  {DPS_LANG_TH, "th"},
  {DPS_LANG_TK, "tk"},
  {DPS_LANG_TL, "tl"},
  {DPS_LANG_TPI, "tpi"},
  {DPS_LANG_TR, "tr"},
  {DPS_LANG_TT, "tt"},
  {DPS_LANG_TY, "ty"},
  {DPS_LANG_UDM, "udm"},
  {DPS_LANG_UG, "ug"},
  {DPS_LANG_UK, "uk"},
  {DPS_LANG_UR, "ur"},
  {DPS_LANG_UT, "ut"},
  {DPS_LANG_UZ, "uz"},
  {DPS_LANG_VI, "vi"},
  {DPS_LANG_VO, "vo"},
  {DPS_LANG_WA, "wa"},
  {DPS_LANG_XAL, "xal"},
  {DPS_LANG_YI, "yi"},
  {DPS_LANG_ZH_HANS, "zh-hans"},
  {DPS_LANG_ZH_HK, "zh-hk"},
  {DPS_LANG_ZH_MIN_NAN, "zh-min-nan"},
  {DPS_LANG_ZH_TW, "zh-tw"},
  {DPS_LANG_ZH, "zh"},
  {DPS_LANG_ZHS, "zhs"},
  {DPS_LANG_AA, "aa"},
  {DPS_LANG_AE, "ae"},
  {DPS_LANG_AK, "ak"},
  {DPS_LANG_AS, "as"},
  {DPS_LANG_AY, "ay"},
  {DPS_LANG_BH, "bh"},
  {DPS_LANG_BI, "bi"},
  {DPS_LANG_BO, "bo"},
  {DPS_LANG_CH, "ch"},
  {DPS_LANG_CR, "cr"},
  {DPS_LANG_CU, "cu"},
  {DPS_LANG_DV, "dv"},
  {DPS_LANG_DZ, "dz"},
  {DPS_LANG_EE, "ee"},
  {DPS_LANG_FF, "ff"},
  {DPS_LANG_GN, "gn"},
  {DPS_LANG_GV, "gv"},
  {DPS_LANG_HA, "ha"},
  {DPS_LANG_HO, "ho"},
  {DPS_LANG_HZ, "hz"},
  {DPS_LANG_IG, "ig"},
  {DPS_LANG_II, "ii"},
  {DPS_LANG_IK, "ik"},
  {DPS_LANG_KG, "kg"},
  {DPS_LANG_KI, "ki"},
  {DPS_LANG_KJ, "kj"},
  {DPS_LANG_KL, "kl"},
  {DPS_LANG_KM, "km"},
  {DPS_LANG_KR, "kr"},
  {DPS_LANG_LG, "lg"},
  {DPS_LANG_LN, "ln"},
  {DPS_LANG_LO, "lo"},
  {DPS_LANG_LU, "lu"},
  {DPS_LANG_MH, "mh"},
  {DPS_LANG_NA, "na"},
  {DPS_LANG_ND, "nd"},
  {DPS_LANG_NG, "ng"},
  {DPS_LANG_NR, "nr"},
  {DPS_LANG_NY, "ny"},
  {DPS_LANG_OJ, "oj"},
  {DPS_LANG_OM, "om"},
  {DPS_LANG_OR, "or"},
  {DPS_LANG_PA, "pa"},
  {DPS_LANG_PS, "ps"},
  {DPS_LANG_QU, "qu"},
  {DPS_LANG_RN, "rn"},
  {DPS_LANG_RW, "rw"},
  {DPS_LANG_SC, "sc"},
  {DPS_LANG_SD, "sd"},
  {DPS_LANG_SG, "sg"},
  {DPS_LANG_SI, "si"},
  {DPS_LANG_SM, "sm"},
  {DPS_LANG_SN, "sn"},
  {DPS_LANG_SS, "ss"},
  {DPS_LANG_ST, "st"},
  {DPS_LANG_TI, "ti"},
  {DPS_LANG_TN, "tn"},
  {DPS_LANG_TO, "to"},
  {DPS_LANG_TS, "ts"},
  {DPS_LANG_TW, "tw"},
  {DPS_LANG_VE, "ve"},
  {DPS_LANG_WO, "wo"},
  {DPS_LANG_XH, "xn"},
  {DPS_LANG_YO, "yo"},
  {DPS_LANG_ZA, "za"},
  {DPS_LANG_ZU, "zu"},
  {DPS_LANG_MY, "my"},
  {DPS_LANG_EN_IE, "en-ie"},
  {DPS_LANG_FR_FX, "fr-fx"},
  {DPS_LANG_RU_OLD,"ru-old"},
  
  { 0, NULL }
};

DPS_LANG_ALIAS dps_lang_alias[] = { /* This table is ordered by the language name */
  {DPS_LANG_AA,    "aa"},
  {DPS_LANG_AA,    "aar"},
  {DPS_LANG_AB,    "ab"},
  {DPS_LANG_AB,    "abk"},
  {DPS_LANG_ADY,   "ady"},
  {DPS_LANG_AE,    "ae"},
  {DPS_LANG_AF,    "af"},
  {DPS_LANG_AF,    "afr"},
  {DPS_LANG_AK,    "ak"},
  {DPS_LANG_AK,    "aka"},
  {DPS_LANG_SQ,    "alb"},
  {DPS_LANG_AM,    "am"},
  {DPS_LANG_AM,    "amh"},
  {DPS_LANG_AN,    "an"},
  {DPS_LANG_ANG,   "ang"},
  {DPS_LANG_AR,    "ar"},
  {DPS_LANG_AR,    "ara"},
  {DPS_LANG_AR,    "arabic"},
  {DPS_LANG_AN,    "arg"},
  {DPS_LANG_HY,    "arm"},
  {DPS_LANG_AS,    "as"},
  {DPS_LANG_AS,    "asm"},
  {DPS_LANG_AST,   "ast"},
  {DPS_LANG_AV,    "av"},
  {DPS_LANG_AV,    "ava"},
  {DPS_LANG_AE,    "ave"},
  {DPS_LANG_AY,    "ay"},
  {DPS_LANG_AY,    "aym"},
  {DPS_LANG_AZ,    "az"},
  {DPS_LANG_AZ,    "aze"},
  {DPS_LANG_BA,    "ba"},
  {DPS_LANG_BA,    "bak"},
  {DPS_LANG_BM,    "bam"},
  {DPS_LANG_EU,    "baq"},
  {DPS_LANG_BE,    "be"},
  {DPS_LANG_BE,    "bel"},
  {DPS_LANG_BN,    "ben"},
  {DPS_LANG_BG,    "bg"},
  {DPS_LANG_BH,    "bh"},
  {DPS_LANG_BI,    "bi"},
  {DPS_LANG_BH,    "bih"},
  {DPS_LANG_BI,    "bis"},
  {DPS_LANG_BM,    "bm"},
  {DPS_LANG_BN,    "bn"},
  {DPS_LANG_BO,    "bo"},
  {DPS_LANG_BO,    "bod"},
  {DPS_LANG_BS,    "bos"},
  {DPS_LANG_BR,    "br"},
  {DPS_LANG_BR,    "bre"},
  {DPS_LANG_BS,    "bs"},
  {DPS_LANG_BG,    "bul"},
  {DPS_LANG_MY,    "bur"},
  {DPS_LANG_CA,    "ca"},
  {DPS_LANG_CA,    "cat"},
  {DPS_LANG_CE,    "ce"},
  {DPS_LANG_CS,    "ces"},
  {DPS_LANG_CH,    "ch"},
  {DPS_LANG_CH,    "cha"},
  {DPS_LANG_CE,    "che"},
  {DPS_LANG_ZH,    "chi"},
  {DPS_LANG_ZH,    "chinese"},
  {DPS_LANG_CHR,   "chr"},
  {DPS_LANG_CU,    "chu"},
  {DPS_LANG_CV,    "chv"},
  {DPS_LANG_ZH,    "cn"},
  {DPS_LANG_CO,    "co"},
  {DPS_LANG_KW,    "cor"},
  {DPS_LANG_CO,    "cos"},
  {DPS_LANG_CR,    "cr"},
  {DPS_LANG_CR,    "cre"},
  {DPS_LANG_CS,    "cs"},
  {DPS_LANG_CSB,   "csb"},
  {DPS_LANG_CU,    "cu"},
  {DPS_LANG_CV,    "cv"},
  {DPS_LANG_CY,    "cy"},
  {DPS_LANG_CY,    "cym"},
  {DPS_LANG_CS,    "cze"},
  {DPS_LANG_DA,    "da"},
  {DPS_LANG_DA,    "dan"},
  {DPS_LANG_DE,    "de"},
  {DPS_LANG_DE_AT, "de-at"},
  {DPS_LANG_DE_CH, "de-ch"},
  {DPS_LANG_DE,    "de-de"},
  {DPS_LANG_DE_AT, "de_at"},
  {DPS_LANG_DE_CH, "de_ch"},
  {DPS_LANG_DE,    "de_de"},
  {DPS_LANG_DE,    "deu"},
  {DPS_LANG_DV,    "div"},
  {DPS_LANG_NL,    "dut"},
  {DPS_LANG_NL,    "dutch"},
  {DPS_LANG_DV,    "dv"},
  {DPS_LANG_DZ,    "dz"},
  {DPS_LANG_DZ,    "dzo"},
  {DPS_LANG_EE,    "ee"},
  {DPS_LANG_EL,    "el"},
  {DPS_LANG_EL,    "ell"},
  {DPS_LANG_EN,    "en"},
  {DPS_LANG_EN_AU, "en-au"},
  {DPS_LANG_EN_AU, "en-aus"},
  {DPS_LANG_EN,    "en-gb"},
  {DPS_LANG_EN_IE, "en-ie"},
  {DPS_LANG_EN_NZ, "en-nz"},
  {DPS_LANG_EN,    "en-uk"},
  {DPS_LANG_EN_US, "en-us"},
  {DPS_LANG_EN_AU, "en_au"},
  {DPS_LANG_EN,    "en_gb"},
  {DPS_LANG_EN_IE, "en_ie"},
  {DPS_LANG_EN_NZ, "en_nz"},
  {DPS_LANG_EN_US, "en_us"},
  {DPS_LANG_EN,    "eng"},
  {DPS_LANG_EN_AU, "eng-au"},
  {DPS_LANG_EN,    "eng-gb"},
  {DPS_LANG_EN_NZ, "eng-nz"},
  {DPS_LANG_EN_US, "eng-us"},
  {DPS_LANG_EN,    "english"},
  {DPS_LANG_EO,    "eo"},
  {DPS_LANG_EO,    "epo"},
  {DPS_LANG_ES,    "es"},
  {DPS_LANG_ET,    "est"},
  {DPS_LANG_ET,    "estonian"},
  {DPS_LANG_ET,    "et"},
  {DPS_LANG_EU,    "eu"},
  {DPS_LANG_EU,    "eus"},
  {DPS_LANG_EE,    "ewe"},
  {DPS_LANG_FA,    "fa"},
  {DPS_LANG_FO,    "fao"},
  {DPS_LANG_FA,    "fas"},
  {DPS_LANG_FF,    "ff"},
  {DPS_LANG_FI,    "fi"},
  {DPS_LANG_FJ,    "fij"},
  {DPS_LANG_FI,    "fin"},
  {DPS_LANG_FJ,    "fj"},
  {DPS_LANG_FO,    "fo"},
  {DPS_LANG_FR,    "fr"},
  {DPS_LANG_FR,    "fr-fr"},
  {DPS_LANG_FR_FX, "fr-fx"},
  {DPS_LANG_FR,    "fr_fr"},
  {DPS_LANG_FR_FX, "fr_fx"},
  {DPS_LANG_FR,    "fra"},
  {DPS_LANG_FR,    "fre"},
  {DPS_LANG_FR,    "fre-fr"},
  {DPS_LANG_FR,    "french"},
  {DPS_LANG_FY,    "fry"},
  {DPS_LANG_FF,    "ful"},
  {DPS_LANG_FUR,   "fur"},
  {DPS_LANG_FY,    "fy"},
  {DPS_LANG_GA,    "ga"},
  {DPS_LANG_GD,    "gd"},
  {DPS_LANG_KA,    "geo"},
  {DPS_LANG_DE,    "ger"},
  {DPS_LANG_DE,    "german"},
  {DPS_LANG_GL,    "gl"},
  {DPS_LANG_GD,    "gla"},
  {DPS_LANG_GA,    "gle"},
  {DPS_LANG_GL,    "glg"},
  {DPS_LANG_GV,    "glv"},
  {DPS_LANG_GN,    "gn"},
  {DPS_LANG_EL,    "gre"},
  {DPS_LANG_GN,    "grn"},
  {DPS_LANG_GU,    "gu"},
  {DPS_LANG_GU,    "guj"},
  {DPS_LANG_GV,    "gv"},
  {DPS_LANG_HA,    "ha"},
  {DPS_LANG_HT,    "hat"},
  {DPS_LANG_HA,    "hau"},
  {DPS_LANG_HAW,   "haw"},
  {DPS_LANG_SH,    "hbs"},
  {DPS_LANG_HE,    "he"},
  {DPS_LANG_HE,    "heb"},
  {DPS_LANG_HZ,    "her"},
  {DPS_LANG_HI,    "hi"},
  {DPS_LANG_HI,    "hin"},
  {DPS_LANG_HO,    "hmo"},
  {DPS_LANG_HO,    "ho"},
  {DPS_LANG_HR,    "hr"},
  {DPS_LANG_HR,    "hrv"},
  {DPS_LANG_HT,    "ht"},
  {DPS_LANG_HU,    "hu"},
  {DPS_LANG_HU,    "hun"},
  {DPS_LANG_HY,    "hy"},
  {DPS_LANG_HY,    "hye"},
  {DPS_LANG_HZ,    "hz"},
  {DPS_LANG_IA,    "ia"},
  {DPS_LANG_IG,    "ibo"},
  {DPS_LANG_IS,    "ice"},
  {DPS_LANG_ID,    "id"},
  {DPS_LANG_IO,    "ido"},
  {DPS_LANG_IE,    "ie"},
  {DPS_LANG_IG,    "ig"},
  {DPS_LANG_II,    "ii"},
  {DPS_LANG_II,    "iii"},
  {DPS_LANG_IK,    "ik"},
  {DPS_LANG_IU,    "iku"},
  {DPS_LANG_IE,    "ile"},
  {DPS_LANG_IA,    "ina"},
  {DPS_LANG_ID,    "ind"},
  {DPS_LANG_IO,    "io"},
  {DPS_LANG_IK,    "ipk"},
  {DPS_LANG_IS,    "is"},
  {DPS_LANG_IS,    "isl"},
  {DPS_LANG_IT,    "it"},
  {DPS_LANG_IT,    "ita"},
  {DPS_LANG_IT,    "italian"},
  {DPS_LANG_IU,    "iu"},
  {DPS_LANG_JA,    "ja"},
  {DPS_LANG_JA,    "ja-jp"},
  {DPS_LANG_JA,    "ja_jp"},
  {DPS_LANG_JA,    "japanese"},
  {DPS_LANG_JV,    "jav"},
  {DPS_LANG_JA,    "jpn"},
  {DPS_LANG_JV,    "jv"},
  {DPS_LANG_KA,    "ka"},
  {DPS_LANG_KL,    "kal"},
  {DPS_LANG_KN,    "kan"},
  {DPS_LANG_KS,    "kas"},
  {DPS_LANG_KA,    "kat"},
  {DPS_LANG_KR,    "kau"},
  {DPS_LANG_KK,    "kaz"},
  {DPS_LANG_KG,    "kg"},
  {DPS_LANG_KM,    "khm"},
  {DPS_LANG_KI,    "ki"},
  {DPS_LANG_KI,    "kik"},
  {DPS_LANG_RW,    "kin"},
  {DPS_LANG_KY,    "kir"},
  {DPS_LANG_KJ,    "kj"},
  {DPS_LANG_KK,    "kk"},
  {DPS_LANG_KL,    "kl"},
  {DPS_LANG_KM,    "km"},
  {DPS_LANG_KN,    "kn"},
  {DPS_LANG_KO,    "ko"},
  {DPS_LANG_KO,    "ko-kr"},
  {DPS_LANG_KO,    "ko_kr"},
  {DPS_LANG_KV,    "kom"},
  {DPS_LANG_KG,    "kon"},
  {DPS_LANG_KO,    "kor"},
  {DPS_LANG_KR,    "kr"},
  {DPS_LANG_KS,    "ks"},
  {DPS_LANG_KU,    "ku"},
  {DPS_LANG_KJ,    "kua"},
  {DPS_LANG_KU,    "kur"},
  {DPS_LANG_KV,    "kv"},
  {DPS_LANG_KW,    "kw"},
  {DPS_LANG_KY,    "ky"},
  {DPS_LANG_LA,    "la"},
  {DPS_LANG_LO,    "lao"},
  {DPS_LANG_LA,    "lat"},
  {DPS_LANG_LV,    "lav"},
  {DPS_LANG_LB,    "lb"},
  {DPS_LANG_LG,    "lg"},
  {DPS_LANG_LI,    "li"},
  {DPS_LANG_LI,    "lim"},
  {DPS_LANG_LN,    "lin"},
  {DPS_LANG_LT,    "lit"},
  {DPS_LANG_LT,    "lit-lt"},
  {DPS_LANG_LT,    "lithuanian"},
  {DPS_LANG_LN,    "ln"},
  {DPS_LANG_LO,    "lo"},
  {DPS_LANG_LT,    "lt"},
  {DPS_LANG_LT,    "lt-lt"},
  {DPS_LANG_LT,    "lt_lt"},
  {DPS_LANG_LB,    "ltz"},
  {DPS_LANG_LU,    "lu"},
  {DPS_LANG_LU,    "lub"},
  {DPS_LANG_LG,    "lug"},
  {DPS_LANG_LV,    "lv"},
  {DPS_LANG_MK,    "mac"},
  {DPS_LANG_MH,    "mah"},
  {DPS_LANG_ML,    "mal"},
  {DPS_LANG_MI,    "mao"},
  {DPS_LANG_MR,    "mar"},
  {DPS_LANG_MS,    "may"},
  {DPS_LANG_MG,    "mg"},
  {DPS_LANG_MH,    "mh"},
  {DPS_LANG_MI,    "mi"},
  {DPS_LANG_MI,    "mi-nz"},
  {DPS_LANG_MI,    "mi_nz"},
  {DPS_LANG_MK,    "mk"},
  {DPS_LANG_MK,    "mkd"},
  {DPS_LANG_ML,    "ml"},
  {DPS_LANG_MG,    "mlg"},
  {DPS_LANG_MT,    "mlt"},
  {DPS_LANG_MN,    "mn"},
  {DPS_LANG_MO,    "mo"},
  {DPS_LANG_MO,    "mol"},
  {DPS_LANG_MN,    "mon"},
  {DPS_LANG_MR,    "mr"},
  {DPS_LANG_MI,    "mri"},
  {DPS_LANG_MS,    "ms"},
  {DPS_LANG_MS,    "msa"},
  {DPS_LANG_MT,    "mt"},
  {DPS_LANG_MY,    "my"},
  {DPS_LANG_MY,    "mya"},
  {DPS_LANG_NA,    "na"},
  {DPS_LANG_NAH,   "nah"},
  {DPS_LANG_NAP,   "nap"},
  {DPS_LANG_NA,    "nau"},
  {DPS_LANG_NV,    "nav"},
  {DPS_LANG_NB,    "nb"},
  {DPS_LANG_NR,    "nbl"},
  {DPS_LANG_ND,    "nd"},
  {DPS_LANG_ND,    "nde"},
  {DPS_LANG_NG,    "ndo"},
  {DPS_LANG_NDS,   "nds"},
  {DPS_LANG_NE,    "ne"},
  {DPS_LANG_NE,    "nep"},
  {DPS_LANG_NG,    "ng"},
  {DPS_LANG_NL,    "nl"},
  {DPS_LANG_NL,    "nld"},
  {DPS_LANG_NN,    "nn"},
  {DPS_LANG_NN,    "nno"},
  {DPS_LANG_NO,    "no"},
  {DPS_LANG_NO,    "no-no"},
  {DPS_LANG_NO,    "no_no"},
  {DPS_LANG_NB,    "nob"},
  {DPS_LANG_NO,    "nor"},
  {DPS_LANG_NR,    "nr"},
  {DPS_LANG_NV,    "nv"},
  {DPS_LANG_NY,    "ny"},
  {DPS_LANG_NY,    "nya"},
  {DPS_LANG_OC,    "oc"},
  {DPS_LANG_OC,    "oci"},
  {DPS_LANG_OJ,    "oj"},
  {DPS_LANG_OJ,    "oji"},
  {DPS_LANG_OM,    "om"},
  {DPS_LANG_OR,    "or"},
  {DPS_LANG_OR,    "ori"},
  {DPS_LANG_OM,    "orm"},
  {DPS_LANG_OS,    "os"},
  {DPS_LANG_OS,    "oss"},
  {DPS_LANG_PA,    "pa"},
  {DPS_LANG_PA,    "pan"},
  {DPS_LANG_FA,    "per"},
  {DPS_LANG_PI,    "pi"},
  {DPS_LANG_PL,    "pl"},
  {DPS_LANG_PI,    "pli"},
  {DPS_LANG_PL,    "pol"},
  {DPS_LANG_PL,    "polish"},
  {DPS_LANG_PT,    "por"},
  {DPS_LANG_PS,    "ps"},
  {DPS_LANG_PT,    "pt"},
  {DPS_LANG_PT_BR, "pt-br"},
  {DPS_LANG_PT,    "pt-pt"},
  {DPS_LANG_PT_BR, "pt_br"},
  {DPS_LANG_PT,    "pt_pt"},
  {DPS_LANG_PS,    "pus"},
  {DPS_LANG_QU,    "qu"},
  {DPS_LANG_QU,    "que"},
  {DPS_LANG_RM,    "rm"},
  {DPS_LANG_RMY,   "rmy"},
  {DPS_LANG_RN,    "rn"},
  {DPS_LANG_RO,    "ro"},
  {DPS_LANG_RM,    "roh"},
  {DPS_LANG_RO,    "romanian"},
  {DPS_LANG_RO,    "ron"},
  {DPS_LANG_RU,    "ru"},
  {DPS_LANG_RU_OLD,"ru-old"},
  {DPS_LANG_RU,    "ru-ru"},
  {DPS_LANG_RU_OLD,"ru_old"},
  {DPS_LANG_RU,    "ru_ru"},
  {DPS_LANG_RO,    "rum"},
  {DPS_LANG_RN,    "run"},
  {DPS_LANG_RU,    "rus"},
  {DPS_LANG_RU,    "rus-ru"},
  {DPS_LANG_RU,    "russian"},
  {DPS_LANG_RW,    "rw"},
  {DPS_LANG_SA,    "sa"},
  {DPS_LANG_SG,    "sag"},
  {DPS_LANG_SA,    "san"},
  {DPS_LANG_SC,    "sc"},
  {DPS_LANG_SR,    "scc"},
  {DPS_LANG_SCN,   "scn"},
  {DPS_LANG_SCO,   "sco"},
  {DPS_LANG_HR,    "scr"},
  {DPS_LANG_SD,    "sd"},
  {DPS_LANG_SE,    "se"},
  {DPS_LANG_SG,    "sg"},
  {DPS_LANG_SH,    "sh"},
  {DPS_LANG_SI,    "si"},
  {DPS_LANG_SIMPLE,"simple"},
  {DPS_LANG_SI,    "sin"},
  {DPS_LANG_SK,    "sk"},
  {DPS_LANG_SL,    "sl"},
  {DPS_LANG_SK,    "slk"},
  {DPS_LANG_SK,    "slo"},
  {DPS_LANG_SL,    "slv"},
  {DPS_LANG_SM,    "sm"},
  {DPS_LANG_SE,    "sme"},
  {DPS_LANG_SM,    "smo"},
  {DPS_LANG_SN,    "sn"},
  {DPS_LANG_SN,    "sna"},
  {DPS_LANG_SD,    "snd"},
  {DPS_LANG_SO,    "so"},
  {DPS_LANG_SO,    "som"},
  {DPS_LANG_ST,    "sot"},
  {DPS_LANG_ES,    "spa"},
  {DPS_LANG_ES,    "spanish"},
  {DPS_LANG_SQ,    "sq"},
  {DPS_LANG_SQ,    "sqi"},
  {DPS_LANG_SR,    "sr"},
  {DPS_LANG_SC,    "srd"},
  {DPS_LANG_SR,    "srp"},
  {DPS_LANG_SS,    "ss"},
  {DPS_LANG_SS,    "ssw"},
  {DPS_LANG_ST,    "st"},
  {DPS_LANG_SU,    "su"},
  {DPS_LANG_SU,    "sun"},
  {DPS_LANG_SV,    "sv"},
  {DPS_LANG_SW,    "sw"},
  {DPS_LANG_SW,    "swa"},
  {DPS_LANG_SV,    "swe"},
  {DPS_LANG_TA,    "ta"},
  {DPS_LANG_TY,    "tah"},
  {DPS_LANG_TA,    "tam"},
  {DPS_LANG_TT,    "tat"},
  {DPS_LANG_TE,    "te"},
  {DPS_LANG_TE,    "tel"},
  {DPS_LANG_TG,    "tg"},
  {DPS_LANG_TG,    "tgk"},
  {DPS_LANG_TL,    "tgl"},
  {DPS_LANG_TH,    "th"},
  {DPS_LANG_TH,    "th-th"},
  {DPS_LANG_TH,    "th_th"},
  {DPS_LANG_TH,    "tha"},
  {DPS_LANG_TI,    "ti"},
  {DPS_LANG_BO,    "tib"},
  {DPS_LANG_TI,    "tir"},
  {DPS_LANG_TK,    "tk"},
  {DPS_LANG_TL,    "tl"},
  {DPS_LANG_TN,    "tn"},
  {DPS_LANG_TO,    "to"},
  {DPS_LANG_TO,    "ton"},
  {DPS_LANG_TPI,   "tpi"},
  {DPS_LANG_TR,    "tr"},
  {DPS_LANG_TS,    "ts"},
  {DPS_LANG_TN,    "tsn"},
  {DPS_LANG_TS,    "tso"},
  {DPS_LANG_TT,    "tt"},
  {DPS_LANG_TK,    "tuk"},
  {DPS_LANG_TR,    "tur"},
  {DPS_LANG_TW,    "tw"},
  {DPS_LANG_TW,    "twi"},
  {DPS_LANG_TY,    "ty"},
  {DPS_LANG_UDM,   "udm"},
  {DPS_LANG_UG,    "ug"},
  {DPS_LANG_UG,    "uig"},
  {DPS_LANG_UK,    "uk"},
  {DPS_LANG_UK,    "uk-ua"},
  {DPS_LANG_UK,    "uk_ua"},
  {DPS_LANG_UK,    "ukr"},
  {DPS_LANG_UR,    "ur"},
  {DPS_LANG_UR,    "urd"},
  {DPS_LANG_UT,    "ut"},
  {DPS_LANG_UZ,    "uz"},
  {DPS_LANG_UZ,    "uzb"},
  {DPS_LANG_VE,    "ve"},
  {DPS_LANG_VE,    "ven"},
  {DPS_LANG_VI,    "vi"},
  {DPS_LANG_VI,    "vie"},
  {DPS_LANG_VO,    "vo"},
  {DPS_LANG_VO,    "vol"},
  {DPS_LANG_WA,    "wa"},
  {DPS_LANG_CY,    "wel"},
  {DPS_LANG_WA,    "wln"},
  {DPS_LANG_WO,    "wo"},
  {DPS_LANG_WO,    "wol"},
  {DPS_LANG_XAL,   "xal"},
  {DPS_LANG_XH,    "xh"},
  {DPS_LANG_XH,    "xho"},
  {DPS_LANG_YI,    "yi"},
  {DPS_LANG_YI,    "yid"},
  {DPS_LANG_YO,    "yo"},
  {DPS_LANG_YO,    "yor"},
  {DPS_LANG_ZA,    "za"},
  {DPS_LANG_ZH,    "zh"},
  {DPS_LANG_ZH,    "zh-cn"},
  {DPS_LANG_ZH_HANS, "zh-hans"},
  {DPS_LANG_ZH_HK, "zh-hk"},
  {DPS_LANG_ZH_MIN_NAN, "zh-min-nan"},
  {DPS_LANG_ZH_TW, "zh-tw"},
  {DPS_LANG_ZH,    "zh_cn"},
  {DPS_LANG_ZH_HANS, "zh_hans"},
  {DPS_LANG_ZH_HK, "zh_hk"},
  {DPS_LANG_ZH_MIN_NAN, "zh_min_nan"},
  {DPS_LANG_ZH_TW, "zh_tw"},
  {DPS_LANG_ZA,  "zha"},
  {DPS_LANG_ZH,  "zho"},
  {DPS_LANG_ZHS, "zhs"},
  {DPS_LANG_ZU,  "zu"},
  {DPS_LANG_ZU,  "zul"},

  {0, NULL},
};


const char *DpsLanguageCanonicalName(const char *name) {
  int l,m,r,s;

  if (name != NULL) {
    l = 0;
    s = r = sizeof(dps_lang_alias)/sizeof(DPS_LANG_ALIAS) - 1;
    while(l < r) {
      m = (l + r) / 2;
      if(strcasecmp(dps_lang_alias[m].name, name) < 0) l = m + 1;
      else  r = m;
    }
    if (s == r) return name;
    if (!strcasecmp(dps_lang_alias[r].name, name))
      return dps_language[dps_lang_alias[r].id].name;
  }
  return name;
}


static int LangMapCmp(const DPS_LANGMAP *m1, const DPS_LANGMAP *m2) {
  register int r = strcasecmp(m1->lang, m2->lang);
  if (r != 0) return r;
  return strcasecmp(m1->charset, m2->charset);
}

static DPS_LANGMAP *FindLangMap(DPS_LANGMAPLIST *L, const char *lang, const char *charset, const char *filename, const int addnew) {
  DPS_LANGMAP *o = NULL;
  ssize_t i, l, r;
  register int c;
  char savec;
  char *lt;
  const char *language = DpsLanguageCanonicalName(dps_strtok_r((char*)lang, ", ", &lt, &savec));

  if (L->nmaps) {
    while(language) {

      l = 0; r = (ssize_t)L->nmaps - 1;
      if (addnew) {
	while(l <= r) {
	  i = (l + r) / 2;
	  c = strcasecmp(L->Map[i].lang, language);
	  if (c == 0) {
	    c = strcasecmp(L->Map[i].charset, charset);
	  }
	  if (c == 0) return &L->Map[i];
	  if (c < 0) l = i + 1;
	  else r = i - 1;
	}

      }else {

	while(l <= r) {
	  i = (l + r) / 2;
	  c = strncasecmp(L->Map[i].lang, language, L->Map[i].lang_len);
	  if (c == 0) {
	    c = strncasecmp(L->Map[i].charset, charset, dps_strlen(L->Map[i].charset));
	  }
	  if (c == 0) return &L->Map[i];
	  if (c < 0) l = i + 1;
	  else r = i - 1;
	}
      }
      language = DpsLanguageCanonicalName(dps_strtok_r(NULL, ", ", &lt, &savec));
    }
  }

  if (!addnew) return NULL;

  if(L->nmaps == 0){
    o = L->Map = (DPS_LANGMAP*)DpsMalloc(sizeof(DPS_LANGMAP));
    if (o == NULL) return NULL;
  }else{
    L->Map = (DPS_LANGMAP*)DpsRealloc(L->Map, (L->nmaps + 1) * sizeof(DPS_LANGMAP));
    if (L->Map == NULL) {
      L->nmaps = 0;
      return NULL;
    }
    o = &L->Map[L->nmaps];
  }
  if (o == NULL || L->Map == NULL) {
    fprintf(stderr, "Can't alloc/realloc for language map (%s, %s), nmaps: %d (%lu)", lang, charset, 
		 (int)L->nmaps + 1, (long unsigned)(L->nmaps + 1) * sizeof(DPS_LANGMAP) );
    return NULL;
  }
  bzero((void*)o, sizeof(DPS_LANGMAP));
  for (i = 0; i <= DPS_LM_HASHMASK; i++) o->memb3[i].index = o->memb6[i].index = i;
  o->charset = (char*)DpsStrdup(charset);
  o->lang = (char*)DpsStrdup((lang = DPS_NULL2EMPTY(DpsLanguageCanonicalName(lang))));
  o->lang_len = dps_strlen(lang);
  o->filename = (filename == NULL) ? NULL : (char*)DpsStrdup(filename);
  L->nmaps++;

/*  DpsPreSort(L->Map, L->nmaps, sizeof(DPS_LANGMAP), (qsort_cmp)LangMapCmp);*/
  DpsPreSort(L->Map, L->nmaps, sizeof(DPS_LANGMAP), (qsort_cmp)LangMapCmp);

  l = 0; r = (ssize_t)L->nmaps - 1;
  while(l <= r) {
    i = (l + r) / 2;
    c = strcasecmp(L->Map[i].lang, lang);
    if (c == 0) {
      c = strcasecmp(L->Map[i].charset, charset);
    }
    if (c == 0) return &L->Map[i];
    if (c < 0) l = i + 1;
    else r = i - 1;
  }
  return L->Map; /* this should never happen */
}


__C_LINK int __DPSCALL DpsLoadLangMapFile(DPS_LANGMAPLIST *L, const char * filename) {
     struct stat     sb;
     char *str, *cur_n = NULL, *data = NULL;
     char *Ccharset = NULL, *Clanguage = NULL;
     int Clen = DPS_LM_MAXGRAM1;
     DPS_LANGMAP *Cmap = NULL;
     DPS_CHARSET * cs;
     int             fd;
     char savebyte;

     if (stat(filename, &sb)) {
       dps_strerror(NULL, 0, "Unable to stat LangMap file '%s'", filename);
       return DPS_ERROR;
     }
     if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
       dps_strerror(NULL, 0, "Unable to open LangMap file '%s'", filename);
       return DPS_ERROR;
     }
     if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
       fprintf(stderr, "Unable to alloc %d bytes", (int)sb.st_size);
       DpsClose(fd);
       return DPS_ERROR;
     }
     if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
       dps_strerror(NULL, 0, "Unable to read LangMap file '%s'", filename);
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

     while(str != NULL) {
          if(str[0]=='#'||str[0]==' '||str[0]==HT_CHAR) goto loop_continue;

          if(!strncmp(str,"Charset:",8)){
               
               char * charset, * lasttok;
               DPS_FREE(Ccharset);
               if((charset = dps_strtok_r(str + 8, " \t\n\r", &lasttok, NULL))){
		   const char *canon = DpsCharsetCanonicalName(charset);
                    if (canon) {
			Ccharset = (char*)DpsStrdup(canon);
                    } else {
			fprintf(stderr, "Charset: %s in %s not supported\n", charset, filename);
			DPS_FREE(data);
			DpsClose(fd);
			return DPS_ERROR;
                    }
               }
          }else
          if(!strncmp(str,"Language:",9)){
               char * lang, *lasttok;
               DPS_FREE(Clanguage);
               if((lang = dps_strtok_r(str + 9, " \t\n\r", &lasttok, NULL))){
		 Clanguage = (char*)DpsStrdup(DpsLanguageCanonicalName(lang));
               }
          }else
          if(!strncmp(str,"Length:",7)){
               char * lang, *lasttok;
               if((lang = dps_strtok_r(str + 9, " \t\n\r", &lasttok, NULL))){
		 Clen = DPS_ATOI(lang);
               }
          }else{
               char *s;
               int count;
               
               if(!(s = strchr(str, HT_INT))) goto loop_continue;
               if(Clanguage == NULL) {
                 fprintf(stderr, "No language definition in LangMapFile '%s'\n", filename);
		 DPS_FREE(data);
		 DpsClose(fd);
                 return DPS_ERROR;
               }

               if(Ccharset == NULL) {
                 fprintf(stderr, "No charset definition in LangMapFile '%s'\n", filename);
		 DPS_FREE(data);
		 DpsClose(fd);
                 return DPS_ERROR;
               }
               if(!(cs = DpsGetCharSet(Ccharset))) {
                 fprintf(stderr, "Unknown charset '%s' in LangMapFile '%s'\n", Ccharset, filename);
		 DPS_FREE(data);
		 DpsClose(fd);
                 return DPS_ERROR;
               }
               if (Cmap == NULL) {
		 Cmap = FindLangMap(L, Clanguage, Ccharset, filename, 1);
		 DpsPreSort(Cmap->memb3, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
		 DpsPreSort(Cmap->memb6, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
	       }
               if (Cmap == NULL) {
		   DPS_FREE(data);
		   DpsClose(fd);
		   return DPS_ERROR;
	       }
               *s='\0';

               if(((count = DPS_ATOI(s+1))==0) || (*str=='\0')/*||(dps_strlen(str)>DPS_LM_MAXGRAM)*/)
                    goto loop_continue;

               if(*str){
                    int hindex;
		    sscanf(str, "%x", &hindex);
		    hindex &= DPS_LM_HASHMASK;
		    if (Clen == DPS_LM_MAXGRAM1) {
		      Cmap->memb3[hindex].count += count;
		    } else {
		      Cmap->memb6[hindex].count += count;
		    }
               }
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
     DpsClose(fd);
     DPS_FREE(data);
     DPS_FREE(Clanguage);
     DPS_FREE(Ccharset);
     
     if (Cmap != NULL) DpsPrepareLangMap(Cmap);
     return DPS_OK;
}

__C_LINK int __DPSCALL DpsLoadLangMapList(DPS_LANGMAPLIST *L, const char * mapdir) {

	DIR * dir;
	struct dirent * item;
	char fullname[PATH_MAX]="";
	char name[PATH_MAX]="";
	int res = DPS_OK;

	dir=opendir(mapdir);
	if(!dir)return 0;

	while((item=readdir(dir))){
		char * tail;
		dps_strcpy(name,item->d_name);
		if((tail=strstr(name,".lm"))){
		        *tail='\0';
			dps_snprintf(fullname, sizeof(fullname), "%s/%s", mapdir, item->d_name);
			res = DpsLoadLangMapFile(L, fullname);
/*			if (res != DPS_OK) return res;*/
		}
	}
	closedir(dir);

	return DPS_OK;
}


void DpsLangMapListFree(DPS_LANGMAPLIST *List){
     size_t i;
     
     for(i=0;i<List->nmaps;i++){
          DPS_FREE(List->Map[i].charset);
          DPS_FREE(List->Map[i].lang);
          DPS_FREE(List->Map[i].filename);
     }
     DPS_FREE(List->Map);
     List->nmaps = 0;
}


void DpsLangMapListSave(DPS_LANGMAPLIST *List) {
     char time_str[128];
     size_t i, j, minv;
     FILE *out;
     DPS_LANGMAP *Cmap;
     char name[128];
     time_t t=time(NULL);
     double ratio;
#ifdef HAVE_PTHREAD
     struct tm l_tim;
     struct tm *tim = localtime_r(&t, &l_tim);
#else
     struct tm *tim = localtime(&t);
#endif
     
     for(i = 0; i < List->nmaps; i++) {
       Cmap = &List->Map[i];
       if (Cmap->needsave) {
         if (Cmap->filename == NULL) {
           dps_snprintf(name, 128, "%s.%s.lm", Cmap->lang, Cmap->charset);
           if ((out = fopen(name, "w")) == NULL) continue;
         } else {
           if ((out = fopen(Cmap->filename, "w")) == NULL) continue;
         }
	 strftime(time_str, sizeof(time_str), "%c %Z (UTC%z)", tim);
         fprintf(out, "#\n");
         fprintf(out, "# Autoupdated: %s, %s-%s\n", time_str, PACKAGE, VERSION);
         fprintf(out, "#\n\n");
         fprintf(out, "Language: %s\n", Cmap->lang);
         fprintf(out, "Charset:  %s\n", Cmap->charset);
         fprintf(out, "\n\n");

	 fprintf(out, "Length:   %d\n", (int)DPS_LM_MAXGRAM1);
         DpsPreSort(Cmap->memb3, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), &DpsLMcmpCount);
	 minv = (Cmap->memb3[DPS_LM_TOPCNT - 1].count > 8000) ? 8000 : Cmap->memb3[DPS_LM_TOPCNT - 1].count;
	 ratio = ((double) Cmap->memb3[DPS_LM_TOPCNT - 1].count) / minv;
	 if (ratio > 0.0) {
	   for(j = 0; j < DPS_LM_TOPCNT; j++) {
	     Cmap->memb3[j].count = (size_t) ((double)Cmap->memb3[j].count / ratio);
	   }
	 }
         for(j = 0; j < DPS_LM_TOPCNT; j++) {
          if(!Cmap->memb3[j].count) break;
          fprintf(out, "%03x\t%u\n", (unsigned int)Cmap->memb3[j].index, (unsigned int)Cmap->memb3[j].count);
         }

	 fprintf(out, "Length:   %d\n", (int)DPS_LM_MAXGRAM2);
         DpsPreSort(Cmap->memb6, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), &DpsLMcmpCount);
	 minv = (Cmap->memb6[DPS_LM_TOPCNT - 1].count > 8000) ? 8000 : Cmap->memb6[DPS_LM_TOPCNT - 1].count;
	 ratio = ((double) Cmap->memb6[DPS_LM_TOPCNT - 1].count) / minv;
	 if (ratio > 0.0) {
	   for(j = 0; j < DPS_LM_TOPCNT; j++) {
	     Cmap->memb6[j].count = (size_t) ((double)Cmap->memb6[j].count / ratio);
	   }
	 }
         for(j = 0; j < DPS_LM_TOPCNT; j++) {
          if(!Cmap->memb6[j].count) break;
          fprintf(out, "%03x\t%u\n", (unsigned int)Cmap->memb6[j].index, (unsigned int)Cmap->memb6[j].count);
         }

         fprintf(out, "#\n");
         fclose(out);
       }
     }
}


static const char *dps_next_char2map(register const char *p, register const char *end) {
  for(; p < end; p++) {
    if (*p >= 0 && *p <= 0x40) continue;
    if (*p == '<') {
      register int i = 0;
      for (p++; i < 64 && p < end; i++, p++) {
	if (*p == '>') break;
      }
    } else if (*p == '&') {
    }
    return p;
  }
  return p;
}


void DpsBuildLangMap(DPS_LANGMAP * map, const char * text, size_t textlen, size_t max_nbytes, int StrFlag) {
  const char * end2 = text + textlen;
  register const char *p;
  size_t ngl = 0;
  char s1[2 * DPS_LM_MAXGRAM1];
  char s2[2 * DPS_LM_MAXGRAM2];
  size_t p1 = 0, p2 = 0, n1 = 0, n2 = 0;

  for (p = dps_next_char2map(text, end2); p < end2; p = dps_next_char2map(++p, end2)) {
    ngl++;
    s1[n1] = s1[n1 + DPS_LM_MAXGRAM1] = *p; n1++;
    s2[n2] = s2[n2 + DPS_LM_MAXGRAM2] = *p; n2++;
    if (n1 == DPS_LM_MAXGRAM1 - 1) {
      p++;
      break;
    }
  }
  for (; p < end2; p = dps_next_char2map(++p, end2)) {
    register unsigned int hindex;
    if (*p >= 0 && *p <= 0x40) continue;
    ngl++;
    s1[n1] = s1[n1 + DPS_LM_MAXGRAM1] = *p; 
    n1 = (n1 + 1) % DPS_LM_MAXGRAM1;
    hindex = DpsHash32(s1 + p1, DPS_LM_MAXGRAM1) & DPS_LM_HASHMASK;
    p1 = (p1 + 1) % DPS_LM_MAXGRAM1;
    map->memb3[hindex].count++;

    s2[n2] = s2[n2 + DPS_LM_MAXGRAM2] = *p; n2++;
    if (n2 == DPS_LM_MAXGRAM2 - 1) {
      p++;
      break;
    }
  }

  for(; p < end2; p = dps_next_char2map(++p, end2)) {
    register unsigned int hindex;
    if (*p >= 0 && *p <= 0x40) continue;
    ngl++;

    s1[n1] = s1[n1 + DPS_LM_MAXGRAM1] = *p; 
    n1 = (n1 + 1) % DPS_LM_MAXGRAM1;
    hindex = DpsHash32(s1 + p1, DPS_LM_MAXGRAM1) & DPS_LM_HASHMASK;
    p1 = (p1 + 1) % DPS_LM_MAXGRAM1;
    map->memb3[hindex].count++;

    s2[n2] = s2[n2 + DPS_LM_MAXGRAM1] = *p; 
    n2 = (n2 + 1) % DPS_LM_MAXGRAM2;
    hindex = DpsHash32(s2 + p2, DPS_LM_MAXGRAM2) & DPS_LM_HASHMASK;
    p2 = (p2 + 1) % DPS_LM_MAXGRAM2;
    map->memb6[hindex].count++;

    if ((max_nbytes > 0) && (map->nbytes + ngl > max_nbytes)) break;

  }
  map->nbytes += ngl;
}

void DpsBuildLangMap6(DPS_LANGMAP * map, const char * text, size_t textlen, size_t max_nbytes, int StrFlag) {
  const char * end2 = text + textlen;
  register const char *p;
  size_t ngl = 0;
  char s2[2 * DPS_LM_MAXGRAM2];
  size_t p2 = 0, n2 = 0;

  for (p = dps_next_char2map(text, end2); p < end2; p = dps_next_char2map(++p, end2)) {
    if (*p >= 0 && *p <= 0x40) continue;
    ngl++;
    s2[n2] = s2[n2 + DPS_LM_MAXGRAM2] = *p; n2++;
    if (n2 == DPS_LM_MAXGRAM2 - 1) {
      p++;
      break;
    }
  }

  for(; p < end2; p = dps_next_char2map(++p, end2)) {
    register unsigned int hindex;
    if (*p <= 0x40) continue;
    ngl++;

    s2[n2] = s2[n2 + DPS_LM_MAXGRAM1] = *p; 
    n2 = (n2 + 1) % DPS_LM_MAXGRAM2;
    hindex = DpsHash32(s2 + p2, DPS_LM_MAXGRAM2) & DPS_LM_HASHMASK;
    p2 = (p2 + 1) % DPS_LM_MAXGRAM2;
    map->memb6[hindex].count++;

    if ((max_nbytes > 0) && (map->nbytes + ngl > max_nbytes)) break;

  }
  map->nbytes += ngl;
}



int DpsLMstatcmp(const void * i1, const void * i2) {
  register const DPS_MAPSTAT *s1 = (const DPS_MAPSTAT *)i1;
  register const DPS_MAPSTAT *s2 = (const DPS_MAPSTAT *)i2;

/*  if (DPS_LM_TOPCNT * s1->miss + s1->hits < DPS_LM_TOPCNT * s2->miss + s2->hits) return -1;
  if (DPS_LM_TOPCNT * s1->miss + s1->hits > DPS_LM_TOPCNT * s2->miss + s2->hits) return 1;*/

  if (s1->hits == 0 && s2->hits != 0) return 1;
  if (s1->hits != 0 && s2->hits == 0) return -1;

  if (s1->diff < s2->diff) return -1;
  if (s1->diff > s2->diff) return 1;

  if (s1->miss < s2->miss) return -1;
  if (s1->miss > s2->miss) return 1;

  if (s1->hits < s2->hits) return 1;
  if (s1->hits > s2->hits) return -1;

  return 0;

}

int DpsLMcmpCount(const void * i1, const void * i2) {
     register const DPS_LANGITEM *m1 = i1;
     register const DPS_LANGITEM *m2 = i2;
     if (m2->count < m1->count) return -1;
     if (m2->count > m1->count) return 1;
     if (m2->index < m1->index) return 1;
     if (m2->index > m1->index) return -1;
     return 0;
}

/*
int DpsLMcmpIndex(const void * i1, const void * i2) {
     register const DPS_LANGITEM * m1 = i1;
     register const DPS_LANGITEM * m2 = i2;
     if (m2->index > m1->index) return -1;
     if (m2->index < m1->index) return 1;
     return 0;
}
*/
int DpsLMcmpIndex(const void *v1, const void *v2) {
    const DPS_LANGITEM *m1 = v1, *m2 = v2;
    if (m2->index < m1->index) return 1;
    if (m2->index > m1->index) return -1;
    return 0;
}


void DpsPrepareLangMap(DPS_LANGMAP * map){
  register int i;
  for (i = 0; i < DPS_LM_HASHMASK + 1; i++) map->memb3[i].index = map->memb6[i].index = i;
  DpsPreSort(map->memb3, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpCount);
  DpsPreSort(map->memb3, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
  DpsPreSort(map->memb6, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpCount);
  DpsPreSort(map->memb6, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
}

void DpsPrepareLangMap6(DPS_LANGMAP * map){
  register int i;
  for (i = 0; i < DPS_LM_HASHMASK + 1; i++) map->memb6[i].index = i;
  DpsPreSort(map->memb6, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpCount);
  DpsPreSort(map->memb6, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
}


void DpsCheckLangMap(DPS_LANGMAP * map0, DPS_LANGMAP * map1, DPS_MAPSTAT *Stat, size_t InfMiss, size_t InfHits) {
     register int i;
     DPS_LANGITEM *HIT;

     Stat->hits = Stat->miss = Stat->diff = 0;
     for (i = 0; i < DPS_LM_TOPCNT; i++) {
#if 1
       if ( (HIT = dps_bsearch(&map1->memb3[i], map0->memb3, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex)) == NULL) {
	 Stat->miss += (DPS_LM_TOPCNT - i);
	 Stat->diff += DPS_LM_TOPCNT;
       } else {
	 register int p = (HIT - map0->memb3);
	 Stat->diff += (i >= p) ? (i - p) : (p - i);
	 Stat->hits++;
       }
       if ( (HIT = dps_bsearch(&map1->memb6[i], map0->memb6, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex)) == NULL) {
	 Stat->miss += (DPS_LM_TOPCNT - i);
	 Stat->diff += DPS_LM_TOPCNT;
       } else {
	 register int p = (HIT - map0->memb6);
	 Stat->diff += ((i >= p) ? (i - p) : (p - i));
	 Stat->hits += 1;
       }
#endif
#if 0
       if ( (HIT = dps_bsearch(&map0->memb3[i], map1->memb3, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex)) == NULL) {
	 Stat->miss += (DPS_LM_TOPCNT - i);
	 Stat->diff += DPS_LM_TOPCNT;
       } else {
	 register int p = (HIT - map1->memb3);
	 Stat->diff += (i >= p) ? (i - p) : (p - i);
	 Stat->hits++;
       }
       if ( (HIT = dps_bsearch(&map0->memb6[i], map1->memb6, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex)) == NULL) {
	 Stat->miss += (DPS_LM_TOPCNT - i);
	 Stat->diff += DPS_LM_TOPCNT;
       } else {
	 register int p = (HIT - map1->memb6);
	 Stat->diff += (i >= p) ? (i - p) : (p - i);
	 Stat->hits += 1;
       }
#endif
       if (Stat->diff > InfMiss && Stat->hits > InfHits) break;
     }
}


void DpsCheckLangMap6(DPS_LANGMAP * map0, DPS_LANGMAP * map1, DPS_MAPSTAT *Stat, size_t InfMiss, size_t InfHits) {
     register int i;
     DPS_LANGITEM *HIT;

     Stat->hits = Stat->miss = Stat->diff = 0;
     for (i = 0; i < DPS_LM_TOPCNT; i++) {
       if ( (HIT = dps_bsearch(&map0->memb6[i], map1->memb6, DPS_LM_TOPCNT, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex)) == NULL) {
	 Stat->miss += (DPS_LM_TOPCNT - i);
       } else {
	 register int p = (HIT - map1->memb6);
	 Stat->diff += (i >= p) ? (i - p) : (p - i);
	 Stat->hits++;
       }
/*       if (Stat->diff > InfMiss && Stat->hits > InfHits) break;*/
     }
}


int  DpsGuessCharSet(DPS_AGENT *Indexer, DPS_DOCUMENT * Doc, DPS_LANGMAPLIST *List) {
     DPS_CHARSET * cs;
     DPS_MAPSTAT * mapstat = NULL;
     DPS_LANGMAP *Cmap;
     const char *server_lang = DpsStrdup(DpsLanguageCanonicalName(DpsVarListFindStr(&Doc->Sections, "Content-Language", "")));
     const char *lang = "";
     const char *meta_lang = DpsStrdup(DpsLanguageCanonicalName(DpsVarListFindStr(&Doc->Sections, "Meta-Language", "")));
     const char *server_charset =  DPS_NULL2EMPTY(DpsCharsetCanonicalName(DpsVarListFindStr(&Doc->Sections, "Server-Charset", DpsVarListFindStr(&Doc->Sections, "RemoteCharset", ""))));
     const char *meta_charset =  DPS_NULL2EMPTY(DpsCharsetCanonicalName(DpsVarListFindStr(&Doc->Sections, "Meta-Charset", DpsVarListFindStr(&Doc->Sections, "RemoteCharset", ""))));
     const char *charset =  "";
     const char *lang0 = NULL, *charset0 = NULL;
     size_t InfMiss = 2 * DPS_LM_TOPCNT * DPS_LM_TOPCNT;
     size_t InfHits = 2 * DPS_LM_TOPCNT;
     register size_t i;
     int have_server_lang = (*server_lang != '\0');
     int forte_lang = 0, forte_charset = 0;
     int use_meta, u;
#ifdef DEBUG_GUESSER
     int u1, u2, u3, u4;
#endif
     
     use_meta = Indexer->Flags.use_meta;
			
     if (use_meta) {
       if (*meta_charset != '\0' && *server_charset != '\0' && !strcasecmp(meta_charset, server_charset)) {
	 charset = server_charset; forte_charset = 1;
       }
       if (*meta_lang != '\0' && *server_lang != '\0' && !strcasecmp(meta_lang, server_lang)) {
	 lang = server_lang; forte_lang = 1;
       }
     }
     DpsVarListReplaceStr(&Doc->Sections, "Charset", DPS_NULL2EMPTY(charset));
     DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang);

     /* 
          TODO for Guesser
          
          There are three sources to detect charset:
          1. HTTP header:  Content-Type: ... charset=XXX
          2. <META HTTP-EQUIV="Content-Type" Contnet="... charset=YYY">
          3. ZZZ[i] - array of guessed charsets in mapstat[]
          good(ZZZ[n]) means that guesser returned good results for n.

          Now we may have various combinations:
          Simple situations, non-guessed and guessed charsets
          seem to be the same. At least one of non-guessed
          charset is the same with the best guessed charset
          and guessed charset gave good results:

          1. XXX==YYY==ZZZ[0],      good(ZZZ[0]). Take XXX value.
          2. No YYY, XXX=ZZZ[0] and good(ZZZ[0]). Take XXX value.
          3. No XXX, YYY=ZZZ[0] and good(ZZZ[0]). Take YYY value.
          4. XXX<>YYY, XXX==ZZZ[0], good(ZZZ[0]). Take XXX value.
          5. XXX<>YYY, YYY==ZZZ[0], good(ZZZ[0]). Take XXX value.
               4 and 5 seem to be webmaster mistake.

          There are the same fith situations when ZZZ[x] is still good 
          enough, but it is not the best one, i.e. x>0 
          Choose charset in the same way.
     */

     u = Indexer->Flags.update_lm;
     if (u || forte_lang == 0 || forte_charset == 0) {
	 if(Indexer->Flags.nmaps && Doc->method != DPS_METHOD_DISALLOW) {
	     bzero((void*)Indexer->LangMap, sizeof(DPS_LANGMAP));
	     for (i = 0; i <= DPS_LM_HASHMASK; i++) Indexer->LangMap->memb3[i].index = Indexer->LangMap->memb6[i].index = i;
	     for(i = 0; i < Doc->TextList.nitems; i++) {
		 DPS_TEXTITEM *Item = &Doc->TextList.Items[i];
		 DpsBuildLangMap(Indexer->LangMap, Item->str, dps_strlen(Item->str), Indexer->Flags.GuesserBytes, Indexer->Flags.update_lm);
		 if (Indexer->Flags.GuesserBytes > 0 && Indexer->LangMap->nbytes >= Indexer->Flags.GuesserBytes) break;
	     }
	 }
	 /* Prepare document langmap */
	 DpsPrepareLangMap(Indexer->LangMap);
     }

     DPS_GETLOCK(Indexer, DPS_LOCK_CONF);

     if (forte_lang == 0 || forte_charset == 0 /*1*/ /**charset == '\0' || *lang == '\0'*/) {

	 /* Allocate memory for comparison statistics */
	 if ((mapstat=(DPS_MAPSTAT *)DpsXmalloc((List->nmaps + 1) * sizeof(DPS_MAPSTAT))) == NULL) {
	     DPS_RELEASELOCK(Indexer,DPS_LOCK_CONF);
	     DpsLog(Indexer, DPS_LOG_ERROR, "Can't alloc momory for DpsGuessCharSet (%d bytes)", List->nmaps*sizeof(DPS_MAPSTAT));
	     DPS_FREE(server_lang); DPS_FREE(meta_lang);
	     DpsVarListDel(&Doc->Sections, "URL_ID"); /* since we've changed Content-Language */
	     return DPS_ERROR;
	 }
     
#ifdef DEBUG_GUESSER
	 if (DpsNeedLog(DPS_LOG_EXTRA))
	     fprintf(stderr, "Guesser start: lang: %s, charset: %s\n", DPS_NULL2EMPTY(lang), DPS_NULL2EMPTY(charset));
#endif

	 for(i=0;i<List->nmaps;i++){
	     mapstat[i].map = &List->Map[i];
	     if (forte_charset && strcasecmp(charset, mapstat[i].map->charset)) {
		 mapstat[i].diff = InfMiss + 100; continue;
	     }
	     if (forte_lang && strncasecmp(lang, mapstat[i].map->lang, mapstat[i].map->lang_len)) {
		 mapstat[i].diff = InfMiss + 100; continue;
	     }
	     if ((*charset == '\0') && (*lang == '\0')) {
		 DpsCheckLangMap(&List->Map[i], Indexer->LangMap, &mapstat[i], InfMiss, InfHits);
	     } else if ((*charset != '\0') && 
			(!strcasecmp(mapstat[i].map->charset, charset) || !strcasecmp(mapstat[i].map->charset, DPS_NULL2EMPTY(meta_charset))) ) {
		 DpsCheckLangMap(&List->Map[i], Indexer->LangMap, &mapstat[i], InfMiss, InfHits);
	     } else if ((*lang != '\0') && !strncasecmp(mapstat[i].map->lang, lang, mapstat[i].map->lang_len )) {
		 DpsCheckLangMap(&List->Map[i], Indexer->LangMap, &mapstat[i], InfMiss, InfHits);
	     } else {
		 mapstat[i].hits = 2 * DPS_LM_TOPCNT;
		 mapstat[i].miss = 20 * DPS_LM_TOPCNT + 4;
		 mapstat[i].diff = DPS_LM_TOPCNT * DPS_LM_TOPCNT;
	     }
	     if (mapstat[i].diff < InfMiss) { InfMiss = mapstat[i].diff; InfHits = mapstat[i].hits; }
	 }

	 /* Sort statistics in quality order */
	 if (List->nmaps > 1) DpsPreSort(mapstat, List->nmaps, sizeof(DPS_MAPSTAT), &DpsLMstatcmp);

	 lang0 = DpsStrdup(List->nmaps ? mapstat[0].map->lang : "");
	 charset0 = DpsStrdup(List->nmaps ? mapstat[0].map->charset : "");

#ifdef DEBUG_GUESSER
	 /* Display results, best is shown first */

	 for (i = 0; i < 15; i++) {
	     if (i >= List->nmaps) break;
	     if (DpsNeedLog(DPS_LOG_EXTRA))
		 fprintf(stderr, "Guesser: %dh:%dm:%dd %s-%s\n", mapstat[i].hits, mapstat[i].miss, mapstat[i].diff, mapstat[i].map->lang, mapstat[i].map->charset);
	 }
#endif

	 if (*server_charset != '\0' || *meta_charset != '\0')
	     for(i=0;i<List->nmaps;i++){

#ifdef DEBUG_GUESSER
		 if (DpsNeedLog(DPS_LOG_EXTRA))
		     fprintf(stderr, "Guesser @ charset: %dh:%dm %s-%s\n", 
			     mapstat[i].hits, mapstat[i].miss, mapstat[i].map->lang, mapstat[i].map->charset);
#endif

		 if (mapstat[i].map->lang && (*lang != '\0') && (!strncasecmp(mapstat[i].map->lang, lang, mapstat[i].map->lang_len ))) {
		     if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, DPS_NULL2EMPTY(server_charset))) {
			 DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
			 DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
		     } else if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, DPS_NULL2EMPTY(meta_charset))) {
			 DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
			 DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
		     }
		 } else {
		     if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, DPS_NULL2EMPTY(server_charset))) {
			 DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
			 DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
		     } else if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, DPS_NULL2EMPTY(meta_charset))) {
			 DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
			 DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
		     }
		 }
		 if (*charset != '\0') break;
		 if ((i > 50) && (mapstat[i].miss > mapstat[0].miss + mapstat[0].miss/10)) break;
	     }
          
	 for(i=0;i<List->nmaps;i++){

	     if ((*lang != '\0') && (*charset != '\0')) break;
#ifdef DEBUG_GUESSER
	     if (DpsNeedLog(DPS_LOG_EXTRA))
		 fprintf(stderr, "Guesser @ first: %dh:%dm %s.%s\n", 
			 mapstat[i].hits, mapstat[i].miss, mapstat[i].map->lang, mapstat[i].map->charset);
#endif

	     if(mapstat[i].map->lang && *lang == '\0'/* && (*charset == '\0' || (!strcasecmp(mapstat[i].map->charset, charset)))*/ ){
		 DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
	     }
	     if (mapstat[i].map->charset && *charset == '\0' && (!strcmp(lang, mapstat[i].map->lang)) ) {
		 DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
	     }
          
/*          fprintf(stderr, "Guesser: %dh:%dm %s-%s\n",mapstat[i].hits, mapstat[i].miss,mapstat[i].map->lang,mapstat[i].map->charset);*/
	     
	 }
	 if (List->nmaps > 0 && mapstat[0].map->charset && (*charset == '\0') ) {
	     DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[0].map->charset);
	 }
	 if (List->nmaps > 0 && mapstat[0].map->lang && (*lang == '\0') ) {
             DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[0].map->lang);
	 }
	 DPS_FREE(mapstat);
     }

     if (*DPS_NULL2EMPTY(charset) == '\0') {
       if(use_meta && (*meta_charset != '\0')) DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = meta_charset);
       else if (*server_charset != '\0') DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = server_charset);
       else DpsVarListReplaceStr(&Doc->Sections, "Charset", charset = "iso8859-1");
       
     }
     if (*DPS_NULL2EMPTY(lang) == '\0') {
       if(use_meta && (*meta_lang != '\0')) DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = meta_lang);
       else if (*server_lang != '\0') DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = server_lang);
       else DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = "en");
     }
     
#ifdef DEBUG_GUESSER
     if (DpsNeedLog(DPS_LOG_EXTRA))
       fprintf(stderr, "Guesser start0: meta_lang: %s, meta_charset: %s\n", DPS_NULL2EMPTY(meta_lang), DPS_NULL2EMPTY(meta_charset));
     if (DpsNeedLog(DPS_LOG_EXTRA))
       fprintf(stderr, "Guesser start0: server_lang: %s, server_charset: %s\n", DPS_NULL2EMPTY(server_lang), DPS_NULL2EMPTY(server_charset));
     if (DpsNeedLog(DPS_LOG_EXTRA))
       fprintf(stderr, "Guesser start0: lang0: %s, charset0: %s\n", DPS_NULL2EMPTY(lang0), DPS_NULL2EMPTY(charset0));
     if (DpsNeedLog(DPS_LOG_EXTRA))
       fprintf(stderr, "Guesser start0: lang: %s, charset: %s\n", DPS_NULL2EMPTY(lang), DPS_NULL2EMPTY(charset));
#endif

     Doc->lang_cs_map = FindLangMap(&Indexer->Conf->LangMaps, lang, charset, NULL, 0);

     u = (u && (forte_lang || ((*server_lang != '\0' || *meta_lang != '\0') && (0 == strcasecmp( DpsLanguageCanonicalName(lang), DPS_NULL2EMPTY(lang0)) ))) );
     u = (u && (forte_charset || ((*server_charset != '\0' || *meta_charset != '\0') && (0 == strcasecmp( DpsCharsetCanonicalName(charset), DPS_NULL2EMPTY(charset0)) ))) );

     if (u) {

       Cmap = FindLangMap(&Indexer->Conf->LangMaps, lang, charset, NULL, 1);
       if (Cmap != NULL) {
/*	 DpsVarListReplaceStr(&Doc->Sections, "Content-Language", lang = Cmap->lang);*/

	 DpsPreSort(Cmap->memb3, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
	 DpsPreSort(Indexer->LangMap->memb3, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
	 DpsPreSort(Cmap->memb6, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
	 DpsPreSort(Indexer->LangMap->memb6, DPS_LM_HASHMASK + 1, sizeof(DPS_LANGITEM), (qsort_cmp)DpsLMcmpIndex);
	 for (i = 0; i < DPS_LM_HASHMASK+1; i++) {
	   Cmap->memb3[i].count += Indexer->LangMap->memb3[i].count;
	   Cmap->memb6[i].count += Indexer->LangMap->memb6[i].count;
	 }
	 DpsPrepareLangMap(Cmap);
	 Cmap->needsave = 1;
	 DpsLog(Indexer, 
#ifdef DEBUG_GUESSER
		DPS_LOG_INFO,
#else
		DPS_LOG_EXTRA, 
#endif
		"Lang map: %s.%s updated", Cmap->lang, Cmap->charset);
       }
     }
     DPS_RELEASELOCK(Indexer, DPS_LOCK_CONF);
     cs = DpsGetCharSet(DpsVarListFindStr(&Doc->Sections, "Charset", "iso8859-1"));
     if (cs) Doc->charset_id = cs->id;
     DPS_FREE(server_lang); DPS_FREE(meta_lang); DPS_FREE(charset0); DPS_FREE(lang0);
     DpsVarListDel(&Doc->Sections, "URL_ID"); /* since we've changed Content-Language */
     return DPS_OK;
}
