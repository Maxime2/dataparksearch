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

#ifndef DPS_GUESSER_H
#define DPS_GUESSER_H

#include "dps_uniconv.h"

/* Structure to sort guesser results */
typedef struct {
     DPS_LANGMAP * map;
  size_t hits, miss, diff;
} DPS_MAPSTAT;


typedef struct {
  int        id;
  const char *name;
} DPS_LANG_ALIAS;


typedef struct {
  const int  charset_id;
  const char *lang;
} DPS_CHARSET_BY_LANG;


enum {
  DPS_LANG_AB  = 0,
  DPS_LANG_ADY = 1,
  DPS_LANG_AF  = 2,
  DPS_LANG_AM  = 3,
  DPS_LANG_AN  = 4,
  DPS_LANG_ANG = 5,
  DPS_LANG_AR  = 6,
  DPS_LANG_AST = 7,
  DPS_LANG_AV  = 8,
  DPS_LANG_AZ  = 9,
  DPS_LANG_BA  = 10,
  DPS_LANG_BE  = 11,
  DPS_LANG_BG  = 12,
  DPS_LANG_BM  = 13,
  DPS_LANG_BN  = 14,
  DPS_LANG_BR  = 15,
  DPS_LANG_BS  = 16,
  DPS_LANG_CA  = 17,
  DPS_LANG_CE  = 18,
  DPS_LANG_CHR = 19,
  DPS_LANG_CO  = 20,
  DPS_LANG_CS  = 21,
  DPS_LANG_CSB = 22,
  DPS_LANG_CV  = 23,
  DPS_LANG_CY  = 24,
  DPS_LANG_DA  = 25,
  DPS_LANG_DE_AT = 26,
  DPS_LANG_DE_CH = 27,
  DPS_LANG_DE  = 28,
  DPS_LANG_EL  = 29,
  DPS_LANG_EN_AU = 30,
  DPS_LANG_EN_NZ = 31,
  DPS_LANG_EN_US = 32,
  DPS_LANG_EN = 33,
  DPS_LANG_EO = 34,
  DPS_LANG_ES = 35,
  DPS_LANG_ET = 36,
  DPS_LANG_EU = 37,
  DPS_LANG_FA = 38,
  DPS_LANG_FI = 39,
  DPS_LANG_FJ = 40,
  DPS_LANG_FO = 41,
  DPS_LANG_FR = 42,
  DPS_LANG_FUR = 43,
  DPS_LANG_FY = 44,
  DPS_LANG_GA = 45,
  DPS_LANG_GD = 46,
  DPS_LANG_GL = 47,
  DPS_LANG_GU = 48,
  DPS_LANG_HAW = 49,
  DPS_LANG_HE = 50,
  DPS_LANG_HI = 51,
  DPS_LANG_HR = 52,
  DPS_LANG_HT = 53,
  DPS_LANG_HU = 54,
  DPS_LANG_HY = 55,
  DPS_LANG_IA = 56,
  DPS_LANG_ID = 57,
  DPS_LANG_IE = 58,
  DPS_LANG_IO = 59,
  DPS_LANG_IS = 60,
  DPS_LANG_IT = 61,
  DPS_LANG_IU = 62,
  DPS_LANG_JA = 63,
  DPS_LANG_JV = 64,
  DPS_LANG_KA = 65,
  DPS_LANG_KK = 66,
  DPS_LANG_KN = 67,
  DPS_LANG_KO = 68,
  DPS_LANG_KS = 69,
  DPS_LANG_KU = 70,
  DPS_LANG_KV = 71,
  DPS_LANG_KW = 72,
  DPS_LANG_KY = 73,
  DPS_LANG_LA = 74,
  DPS_LANG_LB = 75,
  DPS_LANG_LI = 76,
  DPS_LANG_LT = 77,
  DPS_LANG_LV = 78,
  DPS_LANG_MG = 79,
  DPS_LANG_MI = 80,
  DPS_LANG_MK = 81,
  DPS_LANG_ML = 82,
  DPS_LANG_MN = 83,
  DPS_LANG_MO = 84,
  DPS_LANG_MR = 85,
  DPS_LANG_MS = 86,
  DPS_LANG_MT = 87,
  DPS_LANG_NAH = 88,
  DPS_LANG_NAP = 89,
  DPS_LANG_NB = 90,
  DPS_LANG_NDS = 91,
  DPS_LANG_NE = 92,
  DPS_LANG_NL = 93,
  DPS_LANG_NN = 94,
  DPS_LANG_NO = 95,
  DPS_LANG_NV = 96,
  DPS_LANG_OC = 97,
  DPS_LANG_OS = 98,
  DPS_LANG_PI = 99,
  DPS_LANG_PL = 100,
  DPS_LANG_PT_BR = 101,
  DPS_LANG_PT = 102,
  DPS_LANG_RM = 103,
  DPS_LANG_RMY = 104,
  DPS_LANG_RO = 105,
  DPS_LANG_RU = 106,
  DPS_LANG_SA = 107,
  DPS_LANG_SCN = 108,
  DPS_LANG_SCO = 109,
  DPS_LANG_SE = 110,
  DPS_LANG_SH = 111,
  DPS_LANG_SIMPLE = 112,
  DPS_LANG_SK = 113,
  DPS_LANG_SL = 114,
  DPS_LANG_SO = 115,
  DPS_LANG_SQ = 116,
  DPS_LANG_SR = 117,
  DPS_LANG_SU = 118,
  DPS_LANG_SV = 119,
  DPS_LANG_SW = 120,
  DPS_LANG_TA = 121,
  DPS_LANG_TE = 122,
  DPS_LANG_TG = 123,
  DPS_LANG_TH = 124,
  DPS_LANG_TK = 125,
  DPS_LANG_TL = 126,
  DPS_LANG_TPI = 127,
  DPS_LANG_TR = 128,
  DPS_LANG_TT = 129,
  DPS_LANG_TY = 130,
  DPS_LANG_UDM = 131,
  DPS_LANG_UG = 132,
  DPS_LANG_UK = 133,
  DPS_LANG_UR = 134,
  DPS_LANG_UT = 135,
  DPS_LANG_UZ = 136,
  DPS_LANG_VI = 137,
  DPS_LANG_VO = 138,
  DPS_LANG_WA = 139,
  DPS_LANG_XAL = 140,
  DPS_LANG_YI = 141,
  DPS_LANG_ZH_HANS = 142,
  DPS_LANG_ZH_HK = 143,
  DPS_LANG_ZH_MIN_NAN = 144,
  DPS_LANG_ZH_TW = 145,
  DPS_LANG_ZH = 146,
  DPS_LANG_ZHS = 147,
  DPS_LANG_AA = 148,
  DPS_LANG_AE = 149,
  DPS_LANG_AK = 150,
  DPS_LANG_AS = 151,
  DPS_LANG_AY = 152,
  DPS_LANG_BH = 153,
  DPS_LANG_BI = 154,
  DPS_LANG_BO = 155,
  DPS_LANG_CH = 156,
  DPS_LANG_CR = 157,
  DPS_LANG_CU = 158,
  DPS_LANG_DV = 159,
  DPS_LANG_DZ = 160,
  DPS_LANG_EE = 161,
  DPS_LANG_FF = 162,
  DPS_LANG_GN = 163,
  DPS_LANG_GV = 164,
  DPS_LANG_HA = 165,
  DPS_LANG_HO = 166,
  DPS_LANG_HZ = 167,
  DPS_LANG_IG = 168,
  DPS_LANG_II = 169,
  DPS_LANG_IK = 170,
  DPS_LANG_KG = 171,
  DPS_LANG_KI = 172,
  DPS_LANG_KJ = 173,
  DPS_LANG_KL = 174,
  DPS_LANG_KM = 175,
  DPS_LANG_KR = 176,
  DPS_LANG_LG = 177,
  DPS_LANG_LN = 178,
  DPS_LANG_LO = 179,
  DPS_LANG_LU = 180,
  DPS_LANG_MH = 181,
  DPS_LANG_NA = 182,
  DPS_LANG_ND = 183,
  DPS_LANG_NG = 184,
  DPS_LANG_NR = 185,
  DPS_LANG_NY = 186,
  DPS_LANG_OJ = 187,
  DPS_LANG_OM = 188,
  DPS_LANG_OR = 189,
  DPS_LANG_PA = 190,
  DPS_LANG_PS = 191,
  DPS_LANG_QU = 192,
  DPS_LANG_RN = 193,
  DPS_LANG_RW = 194,
  DPS_LANG_SC = 195,
  DPS_LANG_SD = 196,
  DPS_LANG_SG = 197,
  DPS_LANG_SI = 198,
  DPS_LANG_SM = 199,
  DPS_LANG_SN = 200,
  DPS_LANG_SS = 201,
  DPS_LANG_ST = 202,
  DPS_LANG_TI = 203,
  DPS_LANG_TN = 204,
  DPS_LANG_TO = 205,
  DPS_LANG_TS = 206,
  DPS_LANG_TW = 207,
  DPS_LANG_VE = 208,
  DPS_LANG_WO = 209,
  DPS_LANG_XH = 210,
  DPS_LANG_YO = 211,
  DPS_LANG_ZA = 212,
  DPS_LANG_ZU = 213,
  DPS_LANG_MY = 214,
  DPS_LANG_EN_IE = 215,
  DPS_LANG_FR_FX = 216,
  DPS_LANG_RU_OLD = 217
};

extern int DpsLMstatcmp(const void * i1, const void * i2);
extern int DpsLMcmpCount(const void * i1,const void * i2);
/*extern int DpsLMcmpIndex(const void * i1,const void * i2);*/
extern int DpsLMcmpIndex(const void *v1, const void *v2);

extern void  DpsBuildLangMap(DPS_LANGMAP * map, const char * text, size_t text_len, size_t max_nbytes, int StrFlag);
extern void  DpsPrepareLangMap(DPS_LANGMAP * map);
extern void  DpsCheckLangMap(DPS_LANGMAP * map, DPS_LANGMAP * text, DPS_MAPSTAT *mstat, size_t InfMiss, size_t InfHits);

extern void  DpsBuildLangMap6(DPS_LANGMAP * map, const char * text, size_t text_len, size_t max_nbytes, int StrFlag);
extern void  DpsPrepareLangMap6(DPS_LANGMAP * map);
extern void  DpsCheckLangMap6(DPS_LANGMAP * map, DPS_LANGMAP * text, DPS_MAPSTAT *mstat, size_t InfMiss, size_t InfHits);

extern void  DpsLangMapListFree(DPS_LANGMAPLIST *);
extern void DpsLangMapListSave(DPS_LANGMAPLIST *List);
#ifdef _DPS_COMMON_H
extern  int  DpsGuessCharSet(DPS_AGENT *Indexer, DPS_DOCUMENT * D, DPS_LANGMAPLIST *L);
#endif
extern const char *DpsLanguageCanonicalName(const char *name);

extern __C_LINK int __DPSCALL DpsLoadLangMapList(DPS_LANGMAPLIST *L, const char * mapdir);
extern __C_LINK int __DPSCALL DpsLoadLangMapFile(DPS_LANGMAPLIST *L, const char * mapname);

#endif
