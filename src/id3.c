/* Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
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
#include "dps_id3.h"
#include "dps_vars.h"
#include "dps_textlist.h"
#include "dps_log.h"
#include "dps_uniconv.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

static int add_var(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, const char *name, char *val) {
        DPS_VAR         *Sec;

        if((Sec=DpsVarListFind(&Doc->Sections,name))){
                DPS_TEXTITEM    Item;
                bzero((void*)&Item, sizeof(Item));
                Item.section = Sec->section;
		Item.strict = Sec->strict;
                Item.str=val;
                Item.section_name = (char*)name;
		Item.len = 0;
                (void)DpsTextListAdd(&Doc->TextList,&Item);
		DpsLog(Indexer, DPS_LOG_DEBUG, "Added: %s:%s", name, val); 
        } else {
		DpsLog(Indexer, DPS_LOG_DEBUG, "Skipped: %s:%s", name, val); 
	}
        return DPS_OK;
}


static int id3_add_var(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc, const char *name, char *val, int charset, size_t len) {
	DPS_VAR		*Sec;
#if 1
	DPS_CHARSET     *from_cs, *utf8_cs;
	DPS_CONV        to_utf;
	char *value;
#endif

	if (len == 0) return DPS_OK;
#if 1	
	utf8_cs = from_cs = DpsGetCharSet("utf-8");
	switch(charset) {
	case 0: from_cs = DpsGetCharSet("iso-8859-1"); break;
	case 1: 
	  if ((val[0] == (char)0xFE) && (val[1] == (char)0xFF)) from_cs = DpsGetCharSet("utf-16be"); 
	  else if (val[0] == (char)0xFF && val[1] == (char)0xFE) from_cs = DpsGetCharSet("utf-16le"); 
	  else return DPS_OK; /* wrong sequence for UTF16 with BOM */
	  val += 2;
	  len -= 2;
	  break;
	case 2: from_cs = DpsGetCharSet("utf-16be"); break;
	case 3: /* utf-8 - no recoding required */ break;
	}
#endif
	if((Sec=DpsVarListFind(&Doc->Sections,name))){
		DPS_TEXTITEM	Item;
		bzero((void*)&Item, sizeof(Item));
#if 1
		if (charset != 3) {
		  DpsConvInit(&to_utf, from_cs, utf8_cs, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);
		  value = (char*)DpsMalloc(14 * len + 2);
		  if ( value == NULL) return DPS_OK;
		  DpsConv(&to_utf, value, 14 * len, val, len);
		  value[to_utf.obytes] = value[to_utf.obytes + 1] = 0;
		  len = to_utf.obytes;
		} else value = val;
#endif
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.str = /*(charset == 3) ? val :*/ value;
		Item.section_name = name;
		Item.len = len; /*(charset == 3) ? len : to_utf.obytes;*/
		(void)DpsTextListAdd(&Doc->TextList, &Item);
		DpsLog(Indexer, DPS_LOG_DEBUG, "Added: %s:%s", name, value); 
		if (charset != 3) DPS_FREE(value);
	} else {
		DpsLog(Indexer, DPS_LOG_DEBUG, "Skipped: %s:%s", name, val); 
	}
	
	return DPS_OK;
}

static int get_tag(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
	char	year[5]="";
	char	album[31];
	char	artist[31];
	char	songname[31];
	char	name[31];
	char    comment[31];
	char	*tag=Doc->Buf.buf+Doc->Buf.size-128;
	
	dps_memcpy(songname,tag + 3, 30);
	songname[30]='\0';
	
	dps_memcpy(artist, tag + 33, 30);
	artist[30]='\0';
	
	dps_memcpy(album, tag + 63, 30);
	album[30]='\0';
	
	dps_memcpy(year, tag + 93, 5);
	year[4]='\0';

	dps_memcpy(comment, tag + 98, 30);
	comment[30]='\0';
	
	DpsRTrim(songname, " ");
	DpsRTrim(artist, " ");
	DpsRTrim(album, " ");
	DpsRTrim(comment, " ");
	
	dps_strcpy(name,"MP3.Song");
	add_var(Indexer, Doc, name, songname);
	
	dps_strcpy(name,"MP3.Album");
	add_var(Indexer, Doc, name, album);
	
	dps_strcpy(name,"MP3.Artist");
	add_var(Indexer, Doc, name, artist);
	
	dps_strcpy(name,"MP3.Year");
	add_var(Indexer, Doc, name, year);
	
	dps_strcpy(name, "MP3.Comment");
	add_var(Indexer, Doc, name, comment);
	
	return DPS_OK;
}

/* 
	id3v2.2.0 version http://www.id3.org/
*/

static int get_id3v2(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
	char	*ch;
	char 	*buf_in = Doc->Buf.content;
	size_t	hdr_len=Doc->Buf.content-Doc->Buf.buf;
	size_t	cont_len=Doc->Buf.size-hdr_len;
	size_t  tag_size, frame_size;
	
	if (hdr_len > Doc->Buf.size) return DPS_OK;
	
	ch = buf_in;

	if ( ch[5] & 0x40) return DPS_OK; /* compression is not defined for this version */

	DpsVarListReplaceStr(&Doc->Sections, "Server-Charset", "utf-8");

	tag_size = ( ( (ch[6] & 0x7F) << 21 ) |
		     ( (ch[7] & 0x7F) << 14 ) |
		     ( (ch[8] & 0x7F) << 07 ) |
		     ( ch[9] & 0x7F ) );
	
	ch += 10;

	while( (ch + 6 < buf_in + tag_size) && (ch + 6 < buf_in + cont_len) ){
		
		frame_size = (ch[3] << 16) + (ch[4] << 8) + ch[5];

		if ( (ch + 6 + frame_size > buf_in + tag_size) ||
		     (frame_size > tag_size) ||
		     (frame_size == 0) )
		  break;

		if (!strncmp(ch, "TT1", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TT1", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TT2", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TT2", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TT3", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TT3", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TP1", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TP1", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TP2", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TP2", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TP3", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TP3", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TP4", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TP4", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TCM", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TCM", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TXT", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TXT", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TLA", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TLA", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TAL", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TAL", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Album", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TYE", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TYE", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Year", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TCR", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TCR", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TPB", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TPB", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TOT", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TOT", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TOA", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TOA", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TOL", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TOL", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 7, (int)ch[6], frame_size - 1);
		} else if(!strncmp(ch , "TOR", 3)) {
		  id3_add_var(Indexer, Doc, "ID3.TOR", ch + 7, (int)ch[6], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Year", ch + 7, (int)ch[6], frame_size - 1);
		}

		ch += 6 + frame_size;
	}
	
	return DPS_OK;
}
/* 
	id3v2.3.0 version http://www.id3.org/
*/

static int get_id3v23(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
	char	*ch;
	char	*buf_in=Doc->Buf.content;
	size_t	hdr_len=Doc->Buf.content-Doc->Buf.buf;
	size_t	cont_len=Doc->Buf.size-hdr_len;
	size_t  tag_size, frame_size, padding, flags;
	
	if (hdr_len > Doc->Buf.size) return DPS_OK;
	
	ch = buf_in;

	if ( ch[5] & 0x20) return DPS_OK; /* experimental is not supported */
	
	tag_size = ( ( (ch[6] & 0x7F) << 21 ) |
		     ( (ch[7] & 0x7F) << 14 ) |
		     ( (ch[8] & 0x7F) << 7 ) |
		     ( ch[9] & 0x7F ) );

	if (ch[6] & 0x40) { /* extended header present */
	  frame_size = ( ( (ch[10]) << 24 ) |
			 ( (ch[11]) << 16 ) |
			 ( (ch[12]) << 8 ) |
			 ( ch[12]) );

	  flags = (ch[13] << 8) + ch[14];

	  padding  = ( ( (ch[15]) << 24 ) |
		       ( (ch[16]) << 16 ) |
		       ( (ch[17]) << 8 ) |
		       ( ch[18]) );

	  ch += ((flags & 0x8000) ? 14 : 10) + frame_size;
	  if (padding < tag_size) tag_size -= padding;
	  else return DPS_OK;
	}else{
		ch += 10;
	}
	DpsVarListReplaceStr(&Doc->Sections, "Server-Charset", "utf-8");
	
	while(((size_t)(ch - buf_in) + 10 < tag_size) && ((size_t)(ch - buf_in) + 10 < cont_len)) {
		
		frame_size = (ch[4] << 24) + (ch[5] << 16) + (ch[6] << 8) + ch[7];
		
		if ( ((ch - buf_in) + 10 + frame_size > tag_size) ||
		     (frame_size > tag_size) ||
		     (frame_size == 0) )
		  break;

		flags = (ch[8] << 8) + ch[9];
		if ( ( (flags & 0x80) > 0) /* compressed, not yet supported */ ||
		     ( (flags & 0x40) > 0) /* encrypted, not supported */ ) {
		  ch += 10 + frame_size;
		  continue;
		}

		if (flags & 0x20) { /* skip groupping byte if present */
		  ch++;
		  frame_size--;
		}

		if (!strncmp(ch, "TALB", 4)){
		  id3_add_var(Indexer, Doc, "ID3.TALB", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Album", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TCOM", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TCOM", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TCOP", 10)) {
		  id3_add_var(Indexer, Doc, "ID3.TCOP", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TENC", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TENC", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TEXT", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TEXT", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TIT1", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TIT1", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TIT2", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TIT2", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TIT3", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TIT3", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TLAN", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TLAN", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TOAL", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TOAL", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Album", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TOLY", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TOLY", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TOPE", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TOPE", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TORY", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TORY", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Year", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE1", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE1", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE2", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE2", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE3", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE3", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE4", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE4", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPOS", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPOS", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPUB", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPUB", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TRCK", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TRCK", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TRSN", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TRSN", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TYER", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TYER", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Year", ch + 11, (int)ch[10], frame_size - 1);
		}

		ch += 10 + frame_size;
	}
	
	return DPS_OK;
}

/* 
	id3v2.4.0 version http://www.id3.org/
*/

static int get_id3v24(DPS_AGENT *Indexer, DPS_DOCUMENT *Doc) {
	char	*ch;
	char	*buf_in=Doc->Buf.content;
	size_t	hdr_len=Doc->Buf.content-Doc->Buf.buf;
	size_t	cont_len=Doc->Buf.size-hdr_len;
	size_t  tag_size, frame_size, flags;
	
	if (hdr_len > Doc->Buf.size) return DPS_OK;
	
	ch = buf_in;

	if ( ch[5] & 0x20) return DPS_OK; /* experimental is not supported */
	
	tag_size = ( ( (ch[6] & 0x7F) << 21 ) |
		     ( (ch[7] & 0x7F) << 14 ) |
		     ( (ch[8] & 0x7F) << 7 ) |
		     ( ch[9] & 0x7F ) );

	if (ch[6] & 0x40) { /* extended header present */
	  frame_size = ( ( (ch[10] & 0x7F) << 21 ) |
			 ( (ch[11] & 0x7F) << 14 ) |
			 ( (ch[12] & 0x7F) << 7 ) |
			 ( ch[12] & 0x7F) );

	  ch += frame_size;
	}else{
		ch += 10;
	}
	
	DpsVarListReplaceStr(&Doc->Sections, "Server-Charset", "utf-8");

	while(((size_t)(ch - buf_in) + 10 < tag_size) && ((size_t)(ch - buf_in) + 10 < cont_len)) {
		
	  frame_size = ((ch[4] & 0x7F) << 21) + ((ch[5] & 0x7F) << 14) + ((ch[6] & 0x7F) << 7) + (ch[7] & 0x7F);
		
		if ( ((ch - buf_in) + 10 + frame_size > tag_size) ||
		     (frame_size > tag_size) ||
		     (frame_size == 0) )
		  break;

		flags = (ch[8] << 8) + ch[9];
		if ( ( (flags & 0x80) > 0) /* compressed, not yet supported */ ||
		     ( (flags & 0x40) > 0) /* encrypted, not supported */ ) {
		  ch += 10 + frame_size;
		  continue;
		}

		 if(!strncmp(ch , "TIT1", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TIT1", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TIT2", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TIT2", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TIT3", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TIT3", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Song", ch + 11, (int)ch[10], frame_size - 1);
		} else if (!strncmp(ch, "TALB", 4)){
		  id3_add_var(Indexer, Doc, "ID3.TALB", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Album", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TOAL", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TOAL", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Album", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TRCK", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TRCK", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPOS", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPOS", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TSST", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TSST", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Title", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE1", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE1", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE2", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE2", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE3", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE3", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPE4", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPE4", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TOPE", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TOPE", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Artist", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TEXT", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TEXT", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TOLY", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TOLY", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TCOM", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TCOM", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TMCL", 10)) {
		  id3_add_var(Indexer, Doc, "ID3.TMCL", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TIPL", 10)) {
		  id3_add_var(Indexer, Doc, "ID3.TIPL", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TENC", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TENC", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TLAN", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TLAN", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TCOP", 10)) {
		  id3_add_var(Indexer, Doc, "ID3.TCOP", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TPUB", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TPUB", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TRSN", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TRSN", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Comment", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TDRC", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TDRC", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Year", ch + 11, (int)ch[10], frame_size - 1);
		} else if(!strncmp(ch , "TDOR", 4)) {
		  id3_add_var(Indexer, Doc, "ID3.TDOR", ch + 11, (int)ch[10], frame_size - 1);
		  id3_add_var(Indexer, Doc, "MP3.Year", ch + 11, (int)ch[10], frame_size - 1);
		}

		ch += 10 + frame_size;
	}
	
	return DPS_OK;
}

#if 0

/* 
	id3v2.3 header http://www.id3.org/
	ID3	3
	version	1+1
    	flag	1
	size	4
*/

static int get_id3(DPS_DOCUMENT *Doc){
	char	*ch;
	char	*buf_in=Doc->Buf.content;
	size_t	hdr_len=Doc->Buf.content-Doc->Buf.buf;
	size_t	cont_len=Doc->Buf.size-hdr_len;
	char	*artist=NULL;
	char	*album=NULL;
	char	*songname=NULL;
	char	name[64];
	int	tagcount=0;
	
	if (hdr_len > Doc->Buf.size) return DPS_OK;
	
	ch = buf_in;

	if ( ch[5] & 0x40) return DPS_OK; /* compression is not defined for this version */

	
	if ( *(ch+6) == 'b'){
		/*
			extened header:
			size	4
			flag	2
			size of pagging 4
		*/
		ch +=20;
	}else{
		ch +=10;
	}
	
	ch += 10;

	while(1){
		/*
			frame header:
			frame id 4
			size	 4
			flags	 2 
		*/
		
		unsigned char	frame_size = (unsigned char)*(ch+7);
		size_t		len = frame_size>cont_len?cont_len:frame_size;
		
		if (!strncmp(ch , "TPE1", 4)){
			ch +=10;
			artist = DpsMalloc(len+1);
			if (artist == NULL) break;
			dps_snprintf(artist, len, "%s", ch+1);
			artist[len]='\0';
			DpsRTrim(artist," ");
			if (++tagcount == 3)
					break;
		}else if(!strncmp(ch , "TALB", 4)){
			ch +=10;
			album = DpsMalloc(len+1);
			if (album == NULL) break;
			dps_snprintf(album, len, "%s", ch+1);
			album[len]='\0';
			DpsRTrim(album," ");
			if (++tagcount == 3)
				break;
		}else if(!strncmp(ch , "TIT2", 4) ){
			ch +=10;
			songname = DpsMalloc(len+1);
			if (songname == NULL) break;
			dps_snprintf(songname, len, "%s", ch+1);
			songname[len]='\0';
			DpsRTrim(songname," ");
			if (++tagcount == 3)
				break;
		}else if (((size_t)(ch - buf_in)+frame_size) <cont_len){
			ch +=10;
		}else{
			break;
		}
		ch +=frame_size;
	}
	
	if (!artist) artist = (char*)DpsStrdup("");
	if (!album) album = (char*)DpsStrdup("");
	if (!songname) songname = (char*)DpsStrdup("");
	
	dps_strcpy(name,"MP3.Song");
	add_var(Doc,name,songname);
	
	dps_strcpy(name,"MP3.Album");
	add_var(Doc,name,album);
	
	dps_strcpy(name,"MP3.Artist");
	add_var(Doc,name,artist);
	
	DPS_FREE(artist);
	DPS_FREE(album);
	DPS_FREE(songname);
	
	return DPS_OK;
}

#endif

int DpsMP3Type(DPS_DOCUMENT *Doc){
	int	hd=((unsigned char)(Doc->Buf.content[0])+256*(unsigned char)(Doc->Buf.content[1])) & 0xf0ff;
	
	if(hd == 0xf0ff)
		return DPS_MP3_TAG;
        
        if (!strncmp(Doc->Buf.content,"RIFF",4))
		return DPS_MP3_RIFF;
	
        if (!strncmp(Doc->Buf.content, "ID3", 3))
		return DPS_MP3_ID3;
	
	return DPS_MP3_UNKNOWN;
}

int DpsMP3Parse(DPS_AGENT *A,DPS_DOCUMENT *Doc){

  DpsLog(A, DPS_LOG_DEBUG, "Executing MP3 parser");

  if (!strncmp(Doc->Buf.content, "ID3", 3)) {
    if (Doc->Buf.content[3] == 2 && Doc->Buf.content[4] == 0) {
      DpsLog(A, DPS_LOG_EXTRA, "ID3v20 tag detected");
      get_id3v2(A, Doc);
    } else if (Doc->Buf.content[3] == 3 && Doc->Buf.content[4] == 0) {
      DpsLog(A, DPS_LOG_EXTRA, "ID3v23 tag detected");
      get_id3v23(A, Doc);
    } else if (Doc->Buf.content[3] == 4 && Doc->Buf.content[4] == 0) {
      DpsLog(A, DPS_LOG_EXTRA, "ID3v24 tag detected");
      get_id3v24(A, Doc);
    }
  } else if ((Doc->Buf.size>=128) && (!strncmp(Doc->Buf.buf+Doc->Buf.size-128, "TAG", 3))) {
    DpsLog(A, DPS_LOG_EXTRA, "MP3 TAG tag detected");
    get_tag(A, Doc);
  } else 
      DpsLog(A, DPS_LOG_EXTRA, "No tag detected");
	
  return DPS_OK;
}
