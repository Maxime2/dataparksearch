/* Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.

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
#include "dps_log.h"
#include "dps_image.h"
#include "dps_utils.h"
#include "dps_vars.h"
#include "dps_textlist.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

static int add_var(DPS_DOCUMENT *Doc, char *name,char *val, size_t len) {
	DPS_VAR		*Sec;
	
	if((Sec = DpsVarListFind(&Doc->Sections, name))) {
		DPS_TEXTITEM	Item;
		bzero((void*)&Item, sizeof(Item));
		Item.section = Sec->section;
		Item.strict = Sec->strict;
		Item.str = val;
		Item.section_name = name;
		Item.len = len;
		(void)DpsTextListAdd(&Doc->TextList, &Item);
	}
	return DPS_OK;
}

int DpsGIFParse(DPS_AGENT *A, DPS_DOCUMENT *Doc) {
  size_t	hdr_len = Doc->Buf.content - Doc->Buf.buf;
  size_t	cont_len = Doc->Buf.size - hdr_len;
  const unsigned char	*buf_in = (const unsigned char*)Doc->Buf.content, *p;
  int global_palette, colors;
  char *str;

  DpsLog(A, DPS_LOG_DEBUG, "Executing GIF parser");
  if (strncmp(buf_in, "GIF", 3)) {
    DpsLog(A, DPS_LOG_EXTRA, "This is not GIF image, skiping.");
    return DPS_OK;
  }
  if (buf_in[cont_len - 1] != 0x3b) {
  }
  global_palette = (buf_in[10] & 0x80);
  colors = 1 << ( (buf_in[10] & 7) + 1  );

  p = buf_in + 13;

  if (global_palette) {
    p += 3 * colors;
  }

  while (*p != 0x3b && ((size_t)(p - buf_in) < cont_len)) {

    if (*p == 0x21) { /* extension block */
      if (p[1] == 0xFE) { /* comment extension */
	DpsLog(A, DPS_LOG_DEBUG, "GIF comment extension found.");
	p += 2;
	while (*p != 0) {
	  str = DpsStrndup(p + 1, (size_t)*p);
	  add_var(Doc, "IMG.comment", str, (size_t)*p);
	  DPS_FREE(str);
	  p += *p;
	  p++;
	}
	p++;
      } else if (p[1] == 0x01) { /* plain text extension */
	DpsLog(A, DPS_LOG_DEBUG, "GIF plain text extension found.");
	p += 14;
	while (*p != 0) {
	  str = DpsStrndup(p + 1, (size_t)*p);
	  add_var(Doc, "body", str, (size_t)*p);
	  DPS_FREE(str);
	  p += *p;
	  p++;
	}
	p++;
      } else {
	p += 2;
	while (*p != 0) {
	  p += *p;
	  p++;
	}
	p++;
      }

    } else if (*p == 0x2c) { /* image block */
      int local_palette = (p[9] & 0x80);
      p += 10;
      if (local_palette) p += 3 * colors;
      p++;
      while (*p != '\0') {
	p += *p;
	p++;
      }
      p++;
    } else {
      DpsLog(A, DPS_LOG_EXTRA, "Possible Broken GIF image.");
      return DPS_OK;
    }
  }

  return DPS_OK;
}
