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

#include "dps_config.h"
#include "dps_charsetutils.h"
#include <string.h>
#if 0
#include <stdio.h>
#endif

#include "dps_uniconv.h"
#include "uconv-8bit.h"

static DPS_CHARSET built_charsets[]={
{
  DPS_CHARSET_8859_1,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-1",
  DPS_CHARSET_WESTERN,
  tab_8859_1_uni,
  idx_uni_8859_1
},
{
  DPS_CHARSET_8859_10,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-10",
  DPS_CHARSET_NORDIC,
  tab_8859_10_uni,
  idx_uni_8859_10
},
{
  DPS_CHARSET_8859_11,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-11",
  DPS_CHARSET_THAI,
  tab_8859_11_uni,
  idx_uni_8859_11
},
{
  DPS_CHARSET_8859_13,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-13",
  DPS_CHARSET_BALTIC,
  tab_8859_13_uni,
  idx_uni_8859_13
},
{
  DPS_CHARSET_8859_14,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-14",
  DPS_CHARSET_CELTIC,
  tab_8859_14_uni,
  idx_uni_8859_14
},
{
  DPS_CHARSET_8859_15,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-15",
  DPS_CHARSET_WESTERN,
  tab_8859_15_uni,
  idx_uni_8859_15
},
{
  DPS_CHARSET_8859_16,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-16",
  DPS_CHARSET_CENTRAL,
  tab_8859_16_uni,
  idx_uni_8859_16
},
{
  DPS_CHARSET_8859_2,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-2",
  DPS_CHARSET_CENTRAL,
  tab_8859_2_uni,
  idx_uni_8859_2
},
{
  DPS_CHARSET_8859_3,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-3",
  DPS_CHARSET_SOUTHERN,
  tab_8859_3_uni,
  idx_uni_8859_3
},
{
  DPS_CHARSET_8859_4,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-4",
  DPS_CHARSET_BALTIC,
  tab_8859_4_uni,
  idx_uni_8859_4
},
{
  DPS_CHARSET_8859_5,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-5",
  DPS_CHARSET_CYRILLIC,
  tab_8859_5_uni,
  idx_uni_8859_5
},
{
  DPS_CHARSET_8859_6,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-6",
  DPS_CHARSET_ARABIC,
  tab_8859_6_uni,
  idx_uni_8859_6
},
{
  DPS_CHARSET_8859_7,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-7",
  DPS_CHARSET_GREEK,
  tab_8859_7_uni,
  idx_uni_8859_7
},
{
  DPS_CHARSET_8859_8,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-8",
  DPS_CHARSET_HEBREW,
  tab_8859_8_uni,
  idx_uni_8859_8
},
{
  DPS_CHARSET_8859_9,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISO-8859-9",
  DPS_CHARSET_TURKISH,
  tab_8859_9_uni,
  idx_uni_8859_9
},
{
  DPS_CHARSET_ARMSCII_8,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "armscii-8",
  DPS_CHARSET_ARMENIAN,
  tab_armscii_8_uni,
  idx_uni_armscii_8
},
{
  DPS_CHARSET_CP037,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "IBM037",
  DPS_CHARSET_WESTERN,
  tab_cp037_uni,
  idx_uni_cp037
},
{
  DPS_CHARSET_CP1026,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp1026",
  DPS_CHARSET_TURKISH,
  tab_cp1026_uni,
  idx_uni_cp1026
},
{
  DPS_CHARSET_CP1133,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp1133",
  DPS_CHARSET_LAO,
  tab_cp1133_uni,
  idx_uni_cp1133
},
{
  DPS_CHARSET_CP1250,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1250",
  DPS_CHARSET_CENTRAL,
  tab_cp1250_uni,
  idx_uni_cp1250
},
{
  DPS_CHARSET_CP1251,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1251",
  DPS_CHARSET_CYRILLIC,
  tab_cp1251_uni,
  idx_uni_cp1251
},
{
  DPS_CHARSET_CP1252,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1252",
  DPS_CHARSET_WESTERN,
  tab_cp1252_uni,
  idx_uni_cp1252
},
{
  DPS_CHARSET_CP1253,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1253",
  DPS_CHARSET_GREEK,
  tab_cp1253_uni,
  idx_uni_cp1253
},
{
  DPS_CHARSET_CP1254,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1254",
  DPS_CHARSET_TURKISH,
  tab_cp1254_uni,
  idx_uni_cp1254
},
{
  DPS_CHARSET_CP1255,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1255",
  DPS_CHARSET_HEBREW,
  tab_cp1255_uni,
  idx_uni_cp1255
},
{
  DPS_CHARSET_CP1256,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1256",
  DPS_CHARSET_ARABIC,
  tab_cp1256_uni,
  idx_uni_cp1256
},
{
  DPS_CHARSET_CP1257,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1257",
  DPS_CHARSET_BALTIC,
  tab_cp1257_uni,
  idx_uni_cp1257
},
{
  DPS_CHARSET_CP1258,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "windows-1258",
  DPS_CHARSET_VIETNAMESE,
  tab_cp1258_uni,
  idx_uni_cp1258
},
{
  DPS_CHARSET_CP437,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp437",
  DPS_CHARSET_WESTERN,
  tab_cp437_uni,
  idx_uni_cp437
},
{
  DPS_CHARSET_CP500,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp500",
  DPS_CHARSET_WESTERN,
  tab_cp500_uni,
  idx_uni_cp500
},
{
  DPS_CHARSET_CP775,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp775",
  DPS_CHARSET_BALTIC,
  tab_cp775_uni,
  idx_uni_cp775
},
{
  DPS_CHARSET_CP850,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp850",
  DPS_CHARSET_WESTERN,
  tab_cp850_uni,
  idx_uni_cp850
},
{
  DPS_CHARSET_CP852,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp852",
  DPS_CHARSET_CENTRAL,
  tab_cp852_uni,
  idx_uni_cp852
},
{
  DPS_CHARSET_CP855,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp855",
  DPS_CHARSET_CYRILLIC,
  tab_cp855_uni,
  idx_uni_cp855
},
{
  DPS_CHARSET_CP857,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp857",
  DPS_CHARSET_TURKISH,
  tab_cp857_uni,
  idx_uni_cp857
},
{
  DPS_CHARSET_CP860,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp860",
  DPS_CHARSET_WESTERN,
  tab_cp860_uni,
  idx_uni_cp860
},
{
  DPS_CHARSET_CP861,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp861",
  DPS_CHARSET_ICELANDIC,
  tab_cp861_uni,
  idx_uni_cp861
},
{
  DPS_CHARSET_CP862,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp862",
  DPS_CHARSET_HEBREW,
  tab_cp862_uni,
  idx_uni_cp862
},
{
  DPS_CHARSET_CP863,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp863",
  DPS_CHARSET_WESTERN,
  tab_cp863_uni,
  idx_uni_cp863
},
{
  DPS_CHARSET_CP864,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp864",
  DPS_CHARSET_ARABIC,
  tab_cp864_uni,
  idx_uni_cp864
},
{
  DPS_CHARSET_CP865,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp865",
  DPS_CHARSET_NORDIC,
  tab_cp865_uni,
  idx_uni_cp865
},
{
  DPS_CHARSET_CP866,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp866",
  DPS_CHARSET_CYRILLIC,
  tab_cp866_uni,
  idx_uni_cp866
},
{
  DPS_CHARSET_CP866U,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp866u",
  DPS_CHARSET_CYRILLIC,
  tab_cp866u_uni,
  idx_uni_cp866u
},
{
  DPS_CHARSET_CP869,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp869",
  DPS_CHARSET_GREEK,
  tab_cp869_uni,
  idx_uni_cp869
},
{
  DPS_CHARSET_CP874,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp874",
  DPS_CHARSET_THAI,
  tab_cp874_uni,
  idx_uni_cp874
},
{
  DPS_CHARSET_CP875,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "cp875",
  DPS_CHARSET_GREEK,
  tab_cp875_uni,
  idx_uni_cp875
},
{
  DPS_CHARSET_ISIRI3342,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "ISIRI3342",
  DPS_CHARSET_IRANIAN,
  tab_isiri3342_uni,
  idx_uni_isiri3342
},
{
  DPS_CHARSET_KOI_7,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "KOI-7",
  DPS_CHARSET_CYRILLIC,
  tab_koi7_uni,
  idx_uni_koi7
},
{
  DPS_CHARSET_KOI8_C,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "KOI8-C",
  DPS_CHARSET_CYRILLIC,
  tab_koi8_c_uni,
  idx_uni_koi8_c
},
{
  DPS_CHARSET_KOI8_R,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "KOI8-R",
  DPS_CHARSET_CYRILLIC,
  tab_koi8_r_uni,
  idx_uni_koi8_r
},
{
  DPS_CHARSET_KOI8_T,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "KOI8-T",
  DPS_CHARSET_TAJIK,
  tab_koi8_t_uni,
  idx_uni_koi8_t
},
{
  DPS_CHARSET_KOI8_U,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "KOI8-U",
  DPS_CHARSET_CYRILLIC,
  tab_koi8_u_uni,
  idx_uni_koi8_u
},
{
  DPS_CHARSET_MACARABIC,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacArabic",
  DPS_CHARSET_ARABIC,
  tab_macarabic_uni,
  idx_uni_macarabic
},
{
  DPS_CHARSET_MACCE,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacCE",
  DPS_CHARSET_CENTRAL,
  tab_macce_uni,
  idx_uni_macce
},
{
  DPS_CHARSET_MACCROATIAN,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacCroatian",
  DPS_CHARSET_CENTRAL,
  tab_maccroatian_uni,
  idx_uni_maccroatian
},
{
  DPS_CHARSET_MACCYRILLIC,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacCyrillic",
  DPS_CHARSET_CYRILLIC,
  tab_maccyrillic_uni,
  idx_uni_maccyrillic
},
{
  DPS_CHARSET_MACGREEK,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacGreek",
  DPS_CHARSET_GREEK,
  tab_macgreek_uni,
  idx_uni_macgreek
},
{
  DPS_CHARSET_MACHEBREW,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacHebrew",
  DPS_CHARSET_HEBREW,
  tab_machebrew_uni,
  idx_uni_machebrew
},
{
  DPS_CHARSET_MACICELAND,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacIceland",
  DPS_CHARSET_ICELANDIC,
  tab_maciceland_uni,
  idx_uni_maciceland
},
{
  DPS_CHARSET_MACROMAN,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacRoman",
  DPS_CHARSET_WESTERN,
  tab_macroman_uni,
  idx_uni_macroman
},
{
  DPS_CHARSET_MACROMANIA,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacRomania",
  DPS_CHARSET_CENTRAL,
  tab_macromanian_uni,
  idx_uni_macromanian
},
{
  DPS_CHARSET_MACTHAI,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacThai",
  DPS_CHARSET_THAI,
  tab_macthai_uni,
  idx_uni_macthai
},
{
  DPS_CHARSET_MACTURKISH,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "MacTurkish",
  DPS_CHARSET_TURKISH,
  tab_macturkish_uni,
  idx_uni_macturkish
},
{
  DPS_CHARSET_US_ASCII,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "US-ASCII",
  DPS_CHARSET_WESTERN,
  tab_us_ascii_uni,
  idx_uni_us_ascii
},
{
  DPS_CHARSET_VISCII,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "VISCII",
  DPS_CHARSET_VIETNAMESE,
  tab_viscii_uni,
  idx_uni_viscii
},
{
  DPS_CHARSET_GEOSTD8,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "geostd8",
  DPS_CHARSET_GEORGIAN,
  tab_geostd8_uni,
  idx_uni_geostd8
},
{
  DPS_CHARSET_GEO_ACADEMY,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "georgian-academy",
  DPS_CHARSET_GEORGIAN,
  tab_georgian_academy_uni,
  idx_uni_georgian_academy
},
{
  DPS_CHARSET_GEO_PS,
  dps_mb_wc_8bit,
  dps_wc_mb_8bit,
  "georgian-ps",
  DPS_CHARSET_GEORGIAN,
  tab_georgian_ps_uni,
  idx_uni_georgian_ps
},
/*{
  DPS_CHARSET_UTF7,
  dps_mb_wc_utf7,
  dps_wc_mb_utf7,
  "UTF-7",
  DPS_CHARSET_UNICODE,
  NULL,
  NULL
},*/
{
  DPS_CHARSET_UTF8,
  dps_mb_wc_utf8,
  dps_wc_mb_utf8,
  "UTF-8",
  DPS_CHARSET_UNICODE,
  NULL,
  NULL
},
{
  DPS_CHARSET_UTF16BE,
  dps_mb_wc_utf16be,
  dps_wc_mb_utf16be,
  "UTF-16BE",
  DPS_CHARSET_UNICODE,
  NULL,
  NULL
},
{
  DPS_CHARSET_UTF16LE,
  dps_mb_wc_utf16le,
  dps_wc_mb_utf16le,
  "UTF-16LE",
  DPS_CHARSET_UNICODE,
  NULL,
  NULL
},
{
  DPS_CHARSET_SYS_INT,
  dps_mb_wc_sys_int,
  dps_wc_mb_sys_int,
  "sys-int",
  DPS_CHARSET_UNICODE,
  NULL,
  NULL
},

#ifdef HAVE_CHARSET_chinese
{
  DPS_CHARSET_GB2312,
  dps_mb_wc_gb2312,
  dps_wc_mb_gb2312,
  "GB2312",
  DPS_CHARSET_CHINESE_SIMPLIFIED,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_chinese
{
  DPS_CHARSET_BIG5,
  dps_mb_wc_big5,
  dps_wc_mb_big5,
  "Big5",
  DPS_CHARSET_CHINESE_TRADITIONAL,
  NULL,
  NULL
},
{
  DPS_CHARSET_BIG5HKSCS,
  dps_mb_wc_big5hkscs,
  dps_wc_mb_big5hkscs,
  "Big5-HKSCS",
  DPS_CHARSET_CHINESE_TRADITIONAL,
  NULL,
  NULL
},
{
  DPS_CHARSET_CP950,
  dps_mb_wc_cp950,
  dps_wc_mb_cp950,
  "cp950",
  DPS_CHARSET_CHINESE_TRADITIONAL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_japanese
{
  DPS_CHARSET_SJIS,
  dps_mb_wc_sjis,
  dps_wc_mb_sjis,
  "Shift_JIS",
  DPS_CHARSET_JAPANESE,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_euc_kr
{
  DPS_CHARSET_EUC_KR,
  dps_mb_wc_euc_kr,
  dps_wc_mb_euc_kr,
  "EUC-KR",
  DPS_CHARSET_KOREAN,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_japanese
{
  DPS_CHARSET_EUC_JP,
  dps_mb_wc_euc_jp,
  dps_wc_mb_euc_jp,
  "EUC-JP",
  DPS_CHARSET_JAPANESE,
  NULL,
  NULL
},
#endif


#ifdef HAVE_CHARSET_chinese
{
  DPS_CHARSET_GBK,
  dps_mb_wc_gbk,
  dps_wc_mb_gbk,
  "GBK",
  DPS_CHARSET_CHINESE_SIMPLIFIED,
  NULL,
  NULL
},
{
  DPS_CHARSET_GB18030,
  dps_mb_wc_gb18030,
  dps_wc_mb_gb18030,
  "GB-18030",
  DPS_CHARSET_CHINESE_TRADITIONAL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_gujarati
{
  DPS_CHARSET_GUJARATI,
  dps_mb_wc_gujarati,
  dps_wc_mb_gujarati,
  "MacGujarati",
  DPS_CHARSET_INDIAN,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_tscii
{
  DPS_CHARSET_TSCII,
  dps_mb_wc_tscii,
  dps_wc_mb_tscii,
  "tscii",
  DPS_CHARSET_INDIAN,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_japanese
{
  DPS_CHARSET_ISO2022JP,
  dps_mb_wc_iso2022jp,
  dps_wc_mb_iso2022jp,
  "ISO-2022-JP",
  DPS_CHARSET_JAPANESE,
  NULL,
  NULL
},
#endif

{
  0,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL
}

};


DPS_CHARSET_ALIAS dps_cs_alias[]={
	{"037",			DPS_CHARSET_CP037},
	{"1026",		DPS_CHARSET_CP1026},
	{"1133",		DPS_CHARSET_CP1133},
	{"437",			DPS_CHARSET_CP437},
	{"500",			DPS_CHARSET_CP500},
	{"775",                 DPS_CHARSET_CP775},
	{"850",			DPS_CHARSET_CP850},
	{"852",			DPS_CHARSET_CP852},
	{"855",			DPS_CHARSET_CP855},
	{"857",			DPS_CHARSET_CP857},
	{"860",			DPS_CHARSET_CP860},
	{"861",			DPS_CHARSET_CP861},
	{"862",			DPS_CHARSET_CP862},
	{"863",			DPS_CHARSET_CP863},
	{"864",			DPS_CHARSET_CP864},
	{"865",			DPS_CHARSET_CP865},
	{"866",			DPS_CHARSET_CP866},
	{"866u",		DPS_CHARSET_CP866U},
	{"869",			DPS_CHARSET_CP869},
	{"874",			DPS_CHARSET_CP874},
	{"875",			DPS_CHARSET_CP875},
	{"950",			DPS_CHARSET_CP950},
	{"ansi_x3.4-1968",	DPS_CHARSET_US_ASCII},
	{"arabic",		DPS_CHARSET_8859_6},
	{"armscii-8",		DPS_CHARSET_ARMSCII_8},
	{"armscii8",		DPS_CHARSET_ARMSCII_8},
	{"ascii",		DPS_CHARSET_US_ASCII},
	{"asmo-708",		DPS_CHARSET_8859_6},
	{"big-5",		DPS_CHARSET_BIG5},
	{"big-five",		DPS_CHARSET_BIG5},
	{"big5",		DPS_CHARSET_BIG5},
	{"big5-hkscs",		DPS_CHARSET_BIG5HKSCS},
	{"big5_hkscs",		DPS_CHARSET_BIG5HKSCS},
	{"big5hk",		DPS_CHARSET_BIG5HKSCS},
	{"bigfive",		DPS_CHARSET_BIG5},
	{"chinese",		DPS_CHARSET_GB2312},
	{"cmac",		DPS_CHARSET_MACCE},
	{"cn-big5",		DPS_CHARSET_BIG5},
	{"cn-gb",               DPS_CHARSET_GB2312},
	{"cp-1026",		DPS_CHARSET_CP1026},
	{"cp-1133",		DPS_CHARSET_CP1133},
	{"cp-1250",		DPS_CHARSET_CP1250},
	{"cp-1251",		DPS_CHARSET_CP1251},
	{"cp-1252",		DPS_CHARSET_CP1252},
	{"cp-1253",		DPS_CHARSET_CP1253},
	{"cp-1254",		DPS_CHARSET_CP1254},
	{"cp-1255",		DPS_CHARSET_CP1255},
	{"cp-1256",		DPS_CHARSET_CP1256},
	{"cp-1257",		DPS_CHARSET_CP1257},
	{"cp-1258",		DPS_CHARSET_CP1258},
	{"cp037",		DPS_CHARSET_CP037},
	{"cp1026",		DPS_CHARSET_CP1026},
	{"cp1133",		DPS_CHARSET_CP1133},
	{"cp1250",		DPS_CHARSET_CP1250},
	{"cp1251",		DPS_CHARSET_CP1251},
	{"cp1252",		DPS_CHARSET_CP1252},
	{"cp1253",		DPS_CHARSET_CP1253},
	{"cp1254",		DPS_CHARSET_CP1254},
	{"cp1255",		DPS_CHARSET_CP1255},
	{"cp1256",		DPS_CHARSET_CP1256},
	{"cp1257",		DPS_CHARSET_CP1257},
	{"cp1258",		DPS_CHARSET_CP1258},
	{"cp367",		DPS_CHARSET_US_ASCII},
	{"cp437",		DPS_CHARSET_CP437},
	{"cp500",		DPS_CHARSET_CP500},
	{"cp775",               DPS_CHARSET_CP775},
	{"cp819",		DPS_CHARSET_8859_1},
	{"cp850",		DPS_CHARSET_CP850},
	{"cp852",		DPS_CHARSET_CP852},
	{"cp855",		DPS_CHARSET_CP855},
	{"cp857",		DPS_CHARSET_CP857},
	{"cp860",		DPS_CHARSET_CP860},
	{"cp861",		DPS_CHARSET_CP861},
	{"cp862",		DPS_CHARSET_CP862},
	{"cp863",		DPS_CHARSET_CP863},
	{"cp864",		DPS_CHARSET_CP864},
	{"cp865",		DPS_CHARSET_CP865},
	{"cp866",		DPS_CHARSET_CP866},
	{"cp866u",		DPS_CHARSET_CP866U},
	{"cp869",		DPS_CHARSET_CP869},
	{"cp874",		DPS_CHARSET_CP874},
	{"cp875",		DPS_CHARSET_CP875},
	{"cp936",		DPS_CHARSET_GBK},
	{"cp950",		DPS_CHARSET_CP950},
	{"cs874",		DPS_CHARSET_CP874},
	{"csascii",		DPS_CHARSET_US_ASCII},
	{"csbig5",		DPS_CHARSET_BIG5},
	{"cseucjp",		DPS_CHARSET_EUC_JP},
	{"cseuckr",		DPS_CHARSET_EUC_KR},
	{"csgb2312",		DPS_CHARSET_GB2312},
	{"csibm037",		DPS_CHARSET_CP037},
	{"csibm866",		DPS_CHARSET_CP866},
	{"csibm869",		DPS_CHARSET_CP869},
	{"csiso2022jp",         DPS_CHARSET_ISO2022JP},
	{"csiso58gb231280",	DPS_CHARSET_GB2312},
	{"csisolatin1",		DPS_CHARSET_8859_1},
	{"csisolatin2",		DPS_CHARSET_8859_2},
	{"csisolatin3",		DPS_CHARSET_8859_3},
	{"csisolatin4",		DPS_CHARSET_8859_4},
	{"csisolatin5",		DPS_CHARSET_8859_9},
	{"csisolatin6",		DPS_CHARSET_8859_10},
	{"csisolatinarabic",	DPS_CHARSET_8859_6},
	{"csisolatincyrillic",	DPS_CHARSET_8859_5},
	{"csisolatingreek",	DPS_CHARSET_8859_7},
	{"csisolatinhebrew",	DPS_CHARSET_8859_8},
	{"cskoi8c",		DPS_CHARSET_KOI8_C},
	{"cskoi8r",		DPS_CHARSET_KOI8_R},
	{"cskoi8t",		DPS_CHARSET_KOI8_T},
	{"cskoi8u",		DPS_CHARSET_KOI8_U},
	{"csmacintosh",		DPS_CHARSET_MACROMAN},
	{"cspc850multilingual",	DPS_CHARSET_CP850},
	{"csshiftjis",		DPS_CHARSET_SJIS},
	{"csviscii",		DPS_CHARSET_VISCII},
	{"cyrillic",		DPS_CHARSET_8859_5},
	{"ecma-114",		DPS_CHARSET_8859_6},
	{"ecma-118",		DPS_CHARSET_8859_7},
	{"elot_928",		DPS_CHARSET_8859_7},
	{"euc-cn",              DPS_CHARSET_GB2312},
	{"euc-jp",		DPS_CHARSET_EUC_JP},
	{"euc-kr",		DPS_CHARSET_EUC_KR},
	{"euc_cn",              DPS_CHARSET_GB2312},
	{"euc_jp",		DPS_CHARSET_EUC_JP},
	{"euc_kr",		DPS_CHARSET_EUC_KR},
	{"euccn",               DPS_CHARSET_GB2312},
	{"eucjp",		DPS_CHARSET_EUC_JP},
	{"euckr",		DPS_CHARSET_EUC_KR},
	{"gb-18030",		DPS_CHARSET_GB18030},
	{"gb18030",		DPS_CHARSET_GB18030},
	{"gb2312",		DPS_CHARSET_GB2312},
	{"gb_2312-80",		DPS_CHARSET_GB2312},
	{"gbk",			DPS_CHARSET_GBK},
	{"geo8-gov",            DPS_CHARSET_GEOSTD8},
	{"georgian-academy",    DPS_CHARSET_GEO_ACADEMY},
	{"georgian-ps",         DPS_CHARSET_GEO_PS},
	{"geostd8",             DPS_CHARSET_GEOSTD8},
	{"greek",		DPS_CHARSET_8859_7},
	{"greek8",		DPS_CHARSET_8859_7},
	{"hebrew",		DPS_CHARSET_8859_8},
	{"hkscs",		DPS_CHARSET_BIG5HKSCS},
	{"ibm037",		DPS_CHARSET_CP037},
	{"ibm1026",		DPS_CHARSET_CP1026},
	{"ibm1133",		DPS_CHARSET_CP1133},
	{"ibm367",		DPS_CHARSET_US_ASCII},
	{"ibm437",		DPS_CHARSET_CP437},
	{"ibm500",		DPS_CHARSET_CP500},
	{"ibm775",              DPS_CHARSET_CP775},
	{"ibm819",		DPS_CHARSET_8859_1},
	{"ibm850",		DPS_CHARSET_CP850},
	{"ibm852",		DPS_CHARSET_CP852},
	{"ibm855",		DPS_CHARSET_CP855},
	{"ibm857",		DPS_CHARSET_CP857},
	{"ibm860",		DPS_CHARSET_CP860},
	{"ibm861",		DPS_CHARSET_CP861},
	{"ibm862",		DPS_CHARSET_CP862},
	{"ibm863",		DPS_CHARSET_CP863},
	{"ibm864",		DPS_CHARSET_CP864},
	{"ibm865",		DPS_CHARSET_CP865},
	{"ibm866",		DPS_CHARSET_CP866},
	{"ibm869",		DPS_CHARSET_CP869},
	{"ibm874",		DPS_CHARSET_CP874},
	{"ibm875",		DPS_CHARSET_CP875},
	{"isiri-3342",          DPS_CHARSET_ISIRI3342},
	{"isiri3342",           DPS_CHARSET_ISIRI3342},
	{"iso 2022-jp",         DPS_CHARSET_ISO2022JP},
	{"iso 8859-1",		DPS_CHARSET_8859_1},
	{"iso 8859-10",		DPS_CHARSET_8859_10},
	{"iso 8859-11",		DPS_CHARSET_8859_11},
	{"iso 8859-13",		DPS_CHARSET_8859_13},
	{"iso 8859-14",		DPS_CHARSET_8859_14},
	{"iso 8859-15",		DPS_CHARSET_8859_15},
	{"iso 8859-16",		DPS_CHARSET_8859_16},
	{"iso 8859-2",		DPS_CHARSET_8859_2},
	{"iso 8859-3",		DPS_CHARSET_8859_3},
	{"iso 8859-4",		DPS_CHARSET_8859_4},
	{"iso 8859-5",		DPS_CHARSET_8859_5},
	{"iso 8859-6",		DPS_CHARSET_8859_6},
	{"iso 8859-7",		DPS_CHARSET_8859_7},
	{"iso 8859-8",		DPS_CHARSET_8859_8},
	{"iso 8859-9",		DPS_CHARSET_8859_9},
	{"iso-2022-jp",         DPS_CHARSET_ISO2022JP},
	{"iso-8859-1",		DPS_CHARSET_8859_1},
	{"iso-8859-10",		DPS_CHARSET_8859_10},
	{"iso-8859-11",		DPS_CHARSET_8859_11},
	{"iso-8859-13",		DPS_CHARSET_8859_13},
	{"iso-8859-14",		DPS_CHARSET_8859_14},
	{"iso-8859-15",		DPS_CHARSET_8859_15},
	{"iso-8859-16",		DPS_CHARSET_8859_16},
	{"iso-8859-2",		DPS_CHARSET_8859_2},
	{"iso-8859-3",		DPS_CHARSET_8859_3},
	{"iso-8859-4",		DPS_CHARSET_8859_4},
	{"iso-8859-5",		DPS_CHARSET_8859_5},
	{"iso-8859-6",		DPS_CHARSET_8859_6},
	{"iso-8859-7",		DPS_CHARSET_8859_7},
	{"iso-8859-8",		DPS_CHARSET_8859_8},
	{"iso-8859-9",		DPS_CHARSET_8859_9},
	{"iso-ir-100",		DPS_CHARSET_8859_1},
	{"iso-ir-101",		DPS_CHARSET_8859_2},
	{"iso-ir-109",		DPS_CHARSET_8859_3},
	{"iso-ir-110",		DPS_CHARSET_8859_4},
	{"iso-ir-126",		DPS_CHARSET_8859_7},
	{"iso-ir-127",		DPS_CHARSET_8859_6},
	{"iso-ir-138",		DPS_CHARSET_8859_8},
	{"iso-ir-144",		DPS_CHARSET_8859_5},
	{"iso-ir-148",		DPS_CHARSET_8859_9},
	{"iso-ir-157",		DPS_CHARSET_8859_10},
	{"iso-ir-179",		DPS_CHARSET_8859_13},
	{"iso-ir-199",		DPS_CHARSET_8859_14},
	{"iso-ir-203",		DPS_CHARSET_8859_15},
	{"iso-ir-226",		DPS_CHARSET_8859_16},
	{"iso-ir-37",		DPS_CHARSET_KOI_7},
	{"iso-ir-58",		DPS_CHARSET_GB2312},
	{"iso-ir-6",		DPS_CHARSET_US_ASCII},
	{"iso646-us",		DPS_CHARSET_US_ASCII},
	{"iso8859-1",		DPS_CHARSET_8859_1},
	{"iso8859-10",		DPS_CHARSET_8859_10},
	{"iso8859-11",		DPS_CHARSET_8859_11},
	{"iso8859-13",		DPS_CHARSET_8859_13},
	{"iso8859-14",		DPS_CHARSET_8859_14},
	{"iso8859-15",		DPS_CHARSET_8859_15},
	{"iso8859-16",		DPS_CHARSET_8859_16},
	{"iso8859-2",		DPS_CHARSET_8859_2},
	{"iso8859-3",		DPS_CHARSET_8859_3},
	{"iso8859-4",		DPS_CHARSET_8859_4},
	{"iso8859-5",		DPS_CHARSET_8859_5},
	{"iso8859-6",		DPS_CHARSET_8859_6},
	{"iso8859-7",		DPS_CHARSET_8859_7},
	{"iso8859-8",		DPS_CHARSET_8859_8},
	{"iso8859-9",		DPS_CHARSET_8859_9},
	{"iso_646.irv:1991",	DPS_CHARSET_US_ASCII},
	{"iso_8859-1",		DPS_CHARSET_8859_1},
	{"iso_8859-10",		DPS_CHARSET_8859_10},
	{"iso_8859-10:1992",	DPS_CHARSET_8859_10},
	{"iso_8859-11",		DPS_CHARSET_8859_11},
	{"iso_8859-11:1992",	DPS_CHARSET_8859_11},
	{"iso_8859-13",		DPS_CHARSET_8859_13},
	{"iso_8859-14",		DPS_CHARSET_8859_14},
	{"iso_8859-14:1998",	DPS_CHARSET_8859_14},
	{"iso_8859-15",		DPS_CHARSET_8859_15},
	{"iso_8859-15:1998",	DPS_CHARSET_8859_15},
	{"iso_8859-16",		DPS_CHARSET_8859_16},
	{"iso_8859-16:2000",	DPS_CHARSET_8859_16},
	{"iso_8859-1:1987",	DPS_CHARSET_8859_1},
	{"iso_8859-2",		DPS_CHARSET_8859_2},
	{"iso_8859-2:1987",	DPS_CHARSET_8859_2},
	{"iso_8859-3",		DPS_CHARSET_8859_3},
	{"iso_8859-3:1988",	DPS_CHARSET_8859_3},
	{"iso_8859-4",		DPS_CHARSET_8859_4},
	{"iso_8859-4:1988",	DPS_CHARSET_8859_4},
	{"iso_8859-5",		DPS_CHARSET_8859_5},
	{"iso_8859-5:1988",	DPS_CHARSET_8859_5},
	{"iso_8859-6",		DPS_CHARSET_8859_6},
	{"iso_8859-6:1987",	DPS_CHARSET_8859_6},
	{"iso_8859-7",		DPS_CHARSET_8859_7},
	{"iso_8859-7:1987",	DPS_CHARSET_8859_7},
	{"iso_8859-8",		DPS_CHARSET_8859_8},
	{"iso_8859-8:1988",	DPS_CHARSET_8859_8},
	{"iso_8859-9",		DPS_CHARSET_8859_9},
	{"iso_8859-9:1989",	DPS_CHARSET_8859_9},
	{"koi-7",		DPS_CHARSET_KOI_7},
	{"koi7",		DPS_CHARSET_KOI_7},
	{"koi8-c",		DPS_CHARSET_KOI8_C},
	{"koi8-r",		DPS_CHARSET_KOI8_R},
	{"koi8-t",		DPS_CHARSET_KOI8_T},
	{"koi8-u",		DPS_CHARSET_KOI8_U},
	{"koi8c",		DPS_CHARSET_KOI8_C},
	{"koi8r",		DPS_CHARSET_KOI8_R},
	{"koi8t",		DPS_CHARSET_KOI8_T},
	{"koi8u",		DPS_CHARSET_KOI8_U},
	{"l1",			DPS_CHARSET_8859_1},
	{"l2",			DPS_CHARSET_8859_2},
	{"l3",			DPS_CHARSET_8859_3},
	{"l4",			DPS_CHARSET_8859_4},
	{"l5",			DPS_CHARSET_8859_9},
	{"l6",			DPS_CHARSET_8859_10},
	{"l7",			DPS_CHARSET_8859_13},
	{"l8",			DPS_CHARSET_8859_14},
	{"l9",			DPS_CHARSET_8859_15},
	{"latin-0",		DPS_CHARSET_8859_15},
	{"latin-1",		DPS_CHARSET_8859_1},
	{"latin-2",		DPS_CHARSET_8859_2},
	{"latin-3",		DPS_CHARSET_8859_3},
	{"latin-4",		DPS_CHARSET_8859_4},
	{"latin-5",		DPS_CHARSET_8859_9},
	{"latin-6",		DPS_CHARSET_8859_10},
	{"latin-7",		DPS_CHARSET_8859_13},
	{"latin-8",		DPS_CHARSET_8859_14},
	{"latin-9",		DPS_CHARSET_8859_15},
	{"latin0",		DPS_CHARSET_8859_15},
	{"latin1",		DPS_CHARSET_8859_1},
	{"latin2",		DPS_CHARSET_8859_2},
	{"latin3",		DPS_CHARSET_8859_3},
	{"latin4",		DPS_CHARSET_8859_4},
	{"latin5",		DPS_CHARSET_8859_9},
	{"latin6",		DPS_CHARSET_8859_10},
	{"latin7",		DPS_CHARSET_8859_13},
	{"latin8",		DPS_CHARSET_8859_14},
	{"latin9",		DPS_CHARSET_8859_15},
	{"mac",			DPS_CHARSET_MACROMAN},
	{"macarabic",		DPS_CHARSET_MACARABIC},
	{"macce",		DPS_CHARSET_MACCE},
	{"maccentraleurope",	DPS_CHARSET_MACCE},
	{"maccroation",		DPS_CHARSET_MACCROATIAN},
	{"maccyrillic",		DPS_CHARSET_MACCYRILLIC},
	{"macgreek",		DPS_CHARSET_MACGREEK},
	{"macgujarati",         DPS_CHARSET_GUJARATI},
	{"machebrew",		DPS_CHARSET_MACHEBREW},
	{"macintosh",		DPS_CHARSET_MACROMAN},
	{"macisland",		DPS_CHARSET_MACICELAND},
	{"macroman",		DPS_CHARSET_MACROMAN},
	{"macromania",		DPS_CHARSET_MACROMANIA},
	{"macthai",		DPS_CHARSET_MACTHAI},
	{"macturkish",		DPS_CHARSET_MACTURKISH},
	{"ms-ansi",		DPS_CHARSET_CP1252},
	{"ms-arab",		DPS_CHARSET_CP1256},
	{"ms-cyr",		DPS_CHARSET_CP1251},
	{"ms-cyrl",		DPS_CHARSET_CP1251},
	{"ms-ee",		DPS_CHARSET_CP1250},
	{"ms-greek",		DPS_CHARSET_CP1253},
	{"ms-hebr",		DPS_CHARSET_CP1255},
	{"ms-turk",		DPS_CHARSET_CP1254},
	{"ms_kanji",		DPS_CHARSET_SJIS},
	{"s-jis",		DPS_CHARSET_SJIS},
	{"shift-jis",		DPS_CHARSET_SJIS},
	{"shift_jis",		DPS_CHARSET_SJIS},
	{"sjis",		DPS_CHARSET_SJIS},
	{"sys-int",		DPS_CHARSET_SYS_INT},
	{"tactis",		DPS_CHARSET_8859_11},
	{"thai",                DPS_CHARSET_8859_11},
	{"tis-620",		DPS_CHARSET_8859_11},
	{"tis620",		DPS_CHARSET_8859_11},
	{"tscii",               DPS_CHARSET_TSCII},
	{"ujis",		DPS_CHARSET_EUC_JP},
	{"us",			DPS_CHARSET_US_ASCII},
	{"us-ascii",		DPS_CHARSET_US_ASCII},
	{"utf-16",		DPS_CHARSET_UTF16BE},
	{"utf-16be",		DPS_CHARSET_UTF16BE},
	{"utf-16le",		DPS_CHARSET_UTF16LE},
/*	{"utf-7",		DPS_CHARSET_UTF7},*/
	{"utf-8",		DPS_CHARSET_UTF8},
	{"utf16",		DPS_CHARSET_UTF16BE},
	{"utf16be",		DPS_CHARSET_UTF16BE},
	{"utf16le",		DPS_CHARSET_UTF16LE},
/*	{"utf7",		DPS_CHARSET_UTF7},*/
	{"utf8",		DPS_CHARSET_UTF8},
	{"viscii",		DPS_CHARSET_VISCII},
	{"viscii1.1-1",		DPS_CHARSET_VISCII},
	{"win-1251",	        DPS_CHARSET_CP1251},
	{"win1251",	        DPS_CHARSET_CP1251},
	{"winbaltrim",		DPS_CHARSET_CP1257},
	{"windows-1250",	DPS_CHARSET_CP1250},
	{"windows-1251",	DPS_CHARSET_CP1251},
	{"windows-1252",	DPS_CHARSET_CP1252},
	{"windows-1253",	DPS_CHARSET_CP1253},
	{"windows-1254",	DPS_CHARSET_CP1254},
	{"windows-1255",	DPS_CHARSET_CP1255},
	{"windows-1256",	DPS_CHARSET_CP1256},
	{"windows-1257",	DPS_CHARSET_CP1257},
	{"windows-1258",	DPS_CHARSET_CP1258},
	{"windows-874",		DPS_CHARSET_CP874},
	{"windows-875",		DPS_CHARSET_CP875},
	{"windows-936",		DPS_CHARSET_GBK},
	{"windows-950",		DPS_CHARSET_CP950},
	{"x-euc-jp",		DPS_CHARSET_EUC_JP},
	{"x-mac-ce",		DPS_CHARSET_MACCE},
	{"x-mac-cyrillic",	DPS_CHARSET_MACCYRILLIC},
	{"x-sjis",		DPS_CHARSET_SJIS},
{NULL,0}};



__C_LINK DPS_CHARSET * __DPSCALL DpsGetCharSetByID(int id) {
  DPS_CHARSET *c;

  for (c=built_charsets; c->name; c++)
    if (c->id==id)
      return c;
  return NULL;
}

#if 0
#include <stdio.h>
static void DpsCheckCharsetList(void) {
  int i;
  fprintf(stderr, "DpsCheckCharsetList\n");
  for (i = 1; i < sizeof(dps_cs_alias)/sizeof(DPS_CHARSET_ALIAS) - 1; i++) {
    if (strcasecmp(dps_cs_alias[i-1].name, dps_cs_alias[i].name) > 0) {
      fprintf(stderr, "%s > %s !\n", dps_cs_alias[i-1].name, dps_cs_alias[i].name);
    }
  }
}
#endif

const char * DpsCharsetCanonicalName(const char * s){
  DPS_CHARSET *c;

#if 0
  DpsCheckCharsetList();
#endif
  c=DpsGetCharSet(s);
  if(!c)return NULL;
  return c->name;
}

DPS_CHARSET * __DPSCALL DpsGetCharSet(const char * name){
  int l,m,r,s;

  l=0;
  s=r=sizeof(dps_cs_alias)/sizeof(DPS_CHARSET_ALIAS)-1;

  while(l<r){
    m=(l+r)/2;
    if(strcasecmp(dps_cs_alias[m].name,name)<0)l=m+1;
    else  r=m;
  }
  if(s==r)
    return(NULL);
  else{
    if(!strcasecmp(dps_cs_alias[r].name, name))
       return DpsGetCharSetByID(dps_cs_alias[r].id);
      /*return(&built_charsets[alias[r].id]);*/
    else  
      return(NULL);
  }
  return NULL;
}
