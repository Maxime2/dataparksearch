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

#include "dps_common.h"
#include "dps_contentencoding.h"
#include "dps_utils.h"
#include "dps_xmalloc.h"
#include "dps_charsetutils.h"
#include "dps_log.h"
#include "dps_vars.h"
#include "dps_proto.h"

#ifdef HAVE_ZLIB

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

__C_LINK int __DPSCALL DpsInflate(DPS_AGENT *query, DPS_DOCUMENT *Doc) {

  z_stream zstream;
  size_t csize, gap, allocated_size, xlen;
  Byte *pfree;
  int rc;
  
  if( (Doc->Buf.size) <= (size_t)(Doc->Buf.content - Doc->Buf.buf + 6) )
    return -1;

  gap = (Doc->Buf.content - Doc->Buf.buf)/* + 1*/;
  csize = Doc->Buf.size - gap;

  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;

  allocated_size = (size_t)Doc->Buf.allocated_size;
  if ((pfree = zstream.next_out = (Byte*)DpsMalloc(allocated_size + 1)) == NULL) {
    inflateEnd(&zstream);
    return -1;
  }
  if ((Doc->Buf.content[0] == (char)0x1f) && (Doc->Buf.content[1] == (char)0x8b)) {
    zstream.next_in = (Byte*)&Doc->Buf.content[2];
    /* 2 bytes - header, 4 bytes - trailing CRC */
    zstream.avail_in = csize - 6;   
  } else {
    zstream.next_in = (Byte*)Doc->Buf.content;
    zstream.avail_in = csize;
  }

  dps_memcpy(zstream.next_out, Doc->Buf.buf, gap); /* was: dps_memmove */

  zstream.next_out += gap;

  zstream.avail_out = (uLong)(allocated_size - gap);

  inflateInit2(&zstream, -MAX_WBITS);

  while(1) {
    rc = inflate(&zstream, Z_NO_FLUSH);
    if (rc == Z_OK) {
      if (allocated_size > Doc->Buf.max_size) {
	DpsLog(query, DPS_LOG_EXTRA, "Inflate: too large content");
	DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_PARTIAL_OK);
	break;
      }
      allocated_size += Doc->Buf.size;
      xlen = zstream.next_out - pfree;
      if ((pfree = (Byte*)DpsRealloc(pfree, allocated_size + 1)) == NULL) {
	inflateEnd(&zstream);
	return -1;
      }
      zstream.next_out = pfree + xlen;
      zstream.avail_out = (uLong)(allocated_size - xlen);
    } else break;
  }
  inflateEnd(&zstream);

  if(zstream.total_out == 0) {
    DPS_FREE(pfree);
    return -1;
  }
  
  DPS_FREE(Doc->Buf.buf);
  Doc->Buf.buf = (char*)pfree;
  Doc->Buf.size = gap + zstream.total_out;
  Doc->Buf.allocated_size = Doc->Buf.size + 1;

  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size)) == NULL) {
    Doc->Buf.allocated_size = 0;
    return -1;
  }

  Doc->Buf.content = Doc->Buf.buf + gap;
  Doc->Buf.content[zstream.total_out] = '\0';

  return 0;
}


struct gztrailer {
    dps_int4 crc32;
    dps_int4 zlen;
};


__C_LINK int __DPSCALL DpsUnGzip(DPS_AGENT *query, DPS_DOCUMENT *Doc) {

  const unsigned char gzheader[10] = { 0x1f, 0x8b, Z_DEFLATED, 0, 0, 0, 0, 0, 0, 3 };
  Byte *cpData;
  z_stream zstream;
  Byte *buf;
  size_t csize, xlen, gap, allocated_size;
  int rc;
  
  if( (Doc->Buf.size) <= (Doc->Buf.content - Doc->Buf.buf + sizeof(gzheader)) )
    return -1;
  /* check magic identificator */
  if ((unsigned char)Doc->Buf.content[0] != gzheader[0]) return -1;
  if ((unsigned char)Doc->Buf.content[1] != gzheader[1]) return -1;

  gap = (Doc->Buf.content - Doc->Buf.buf)/* + 1*/;
  csize = Doc->Buf.size - gap;

  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;

  allocated_size = 4 * (size_t)Doc->Buf.size;
  buf = zstream.next_out = (Byte*)DpsMalloc(allocated_size + 1);
  if (buf == NULL) {
    inflateEnd(&zstream);
    return -1;
  }
  cpData = (Byte*)Doc->Buf.content + sizeof(gzheader);
  csize -= sizeof(gzheader);

  if (Doc->Buf.content[3] & 4) { /* FLG.FEXTRA */
    xlen = (unsigned char) cpData[1];
    xlen <<= 8; 
    xlen += (unsigned char) *cpData;
    cpData += xlen + 2;
    csize -= xlen + 2;
  }
  if (Doc->Buf.content[3] & 8) { /* FLG.FNAME */
    while (*cpData != '\0') cpData++,csize--;
    cpData++; csize--;
  }
  if (Doc->Buf.content[3] & 16) { /* FLG.FCOMMENT */
    while (*cpData != '\0') cpData++,csize--;
    cpData++; csize--;
  }
  if (Doc->Buf.content[3] & 2) { /* FLG.FHCRC */
    cpData += 2;
    csize -= 2;
  }

/*  memcpy(zstream.next_in, cpData, csize);
  zstream.next_out = (Byte*)Doc->Buf.content;*/
  dps_memcpy(zstream.next_out, Doc->Buf.buf, gap); /* was: dps_memmove */
  zstream.next_out += gap;

  zstream.next_in = cpData;
  zstream.avail_in = csize - sizeof(struct gztrailer);
  zstream.avail_out = (uLong)(4 * Doc->Buf.size - gap);

  inflateInit2(&zstream, -MAX_WBITS);

  while(1) {
    rc = inflate(&zstream, Z_NO_FLUSH);
    if (rc == Z_OK) {
      if (allocated_size > Doc->Buf.max_size) {
	DpsLog(query, DPS_LOG_EXTRA, "Gzip: too large content");
	DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_PARTIAL_OK);
	break;
      }
      allocated_size += Doc->Buf.size;
      xlen = zstream.next_out - buf;
      if ((buf = (Byte*)DpsRealloc(buf, allocated_size + 1)) == NULL) {
	inflateEnd(&zstream);
	return -1;
      }
      zstream.next_out = buf + xlen;
      zstream.avail_out = (uLong)(allocated_size - xlen);
    } else break;
  }
  inflateEnd(&zstream);

  if(zstream.total_out == 0) {
    DPS_FREE(buf);
    return -1;
  }
  
  DPS_FREE(Doc->Buf.buf);
  Doc->Buf.buf = (char*)buf;
  Doc->Buf.size = gap + zstream.total_out;
  Doc->Buf.allocated_size = Doc->Buf.size + 1;

  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size + 1)) == NULL) {
    Doc->Buf.allocated_size = 0;
    return -1;
  }

  Doc->Buf.content = Doc->Buf.buf + gap;
  Doc->Buf.content[zstream.total_out] = '\0';
  return 0;
}


__C_LINK int __DPSCALL DpsUncompress(DPS_AGENT *query, DPS_DOCUMENT *Doc) {
  Byte *buf;
  uLong   Len;
  size_t csize, gap, allocated_size;
  int res;
  
  if( (Doc->Buf.size) <= (size_t)(Doc->Buf.content - Doc->Buf.buf) )
    return -1;
  gap = (Doc->Buf.content - Doc->Buf.buf)/* + 1*/;
  csize = Doc->Buf.size - gap;

  allocated_size = 6 * (size_t)Doc->Buf.allocated_size;
  buf = (Byte*)DpsMalloc(allocated_size + 1);
  if (buf == NULL) return -1;
/*  memcpy(buf, Doc->Buf.content, csize);*/
  dps_memcpy(buf, Doc->Buf.buf, gap); /* was: dps_memmove */
  
  while(1) {
    Len = (uLong)(allocated_size - gap);
    res = uncompress(buf + gap, &Len, (Byte*)Doc->Buf.content, csize);
    if (res == Z_BUF_ERROR) {
      if (allocated_size > Doc->Buf.max_size) {
	DpsLog(query, DPS_LOG_EXTRA, "Compress: too large content");
	DpsVarListReplaceInt(&Doc->Sections, "Status", DPS_HTTP_STATUS_PARTIAL_OK);
	break;
      }
      allocated_size += Doc->Buf.size;
      if ((buf = (Byte*)DpsRealloc(buf, allocated_size + 1)) == NULL) {
	return -1;
      }
    } else break;
  }
  
  DPS_FREE(Doc->Buf.buf);
  Doc->Buf.buf = (char*)buf;
  Doc->Buf.size = gap + Len;
  Doc->Buf.allocated_size = Doc->Buf.size + 1;
  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size)) == NULL) {
    Doc->Buf.allocated_size = 0;
    return -1;
  }

  if(res != Z_OK) return -1;
  
  Doc->Buf.content = Doc->Buf.buf + gap;
  Doc->Buf.content[Len] = '\0';
  return 0;
}

#endif



int DpsUnchunk(DPS_AGENT *query, DPS_DOCUMENT *Doc, const char *ce) {
  char *buf, *to_buf, *from_content, *dead_end;
  size_t csize, gap, allocated_size;
  int chunk_size, rc = DPS_OK;

  if( (Doc->Buf.size) <= (size_t)(Doc->Buf.content - Doc->Buf.buf) )
    return DPS_ERROR;
  gap = (Doc->Buf.content - Doc->Buf.buf);
  csize = Doc->Buf.size - gap;

  allocated_size = (size_t)Doc->Buf.allocated_size;
  buf = (char*)DpsMalloc(allocated_size + 1);
  if (buf == NULL) return DPS_ERROR;
  dps_memcpy(buf, Doc->Buf.buf, gap); /* was: dps_memmove */
  
  to_buf = buf + gap;
  from_content = Doc->Buf.content;
  dead_end = buf + allocated_size;

  chunk_size = DPS_HTOI(from_content);
  while(chunk_size) {
/*    while((from_content < dead_end) && (*from_content != '\r')) from_content++;
    if (from_content >= dead_end) { rc = DPS_ERROR; break; }*/
    while(from_content < dead_end && *from_content != '\n') from_content++;
    if (from_content >= dead_end) { rc = DPS_ERROR; break; }
    from_content++;

    if (from_content + chunk_size >= dead_end) { rc = DPS_ERROR; break; }
    dps_memcpy(to_buf, from_content, chunk_size); /* was: dps_memmove */

    to_buf += chunk_size;
    from_content += chunk_size;
    chunk_size = DPS_HTOI(from_content);
  }
  Doc->Buf.size = from_content - Doc->Buf.buf;
  DPS_FREE(Doc->Buf.buf);
  Doc->Buf.buf = (char*)buf;
  Doc->Buf.allocated_size = Doc->Buf.size + 1;
  if ((Doc->Buf.buf = (char*)DpsRealloc(Doc->Buf.buf, Doc->Buf.allocated_size)) == NULL) {
    Doc->Buf.allocated_size = 0;
    return DPS_ERROR;
  }
  Doc->Buf.content = Doc->Buf.buf + gap;
  Doc->Buf.buf[Doc->Buf.size] = '\0';

  return rc;
}
