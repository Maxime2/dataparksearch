/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
   Copyright (C) 2003 Lavtech.com corp. All rights reserved.

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
#include "dps_base.h"
#include "dps_vars.h"
#include "dps_mutex.h"
#include "dps_log.h"
#include "dps_xmalloc.h"
#include "dps_utils.h"
#include "dps_signals.h"
#include "dps_charsetutils.h"
#include "dps_hash.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef O_BINARY
#define DPS_BINARY O_BINARY
#else
#define DPS_BINARY 0
#endif

/*
#define DEBUG_SEARCH 1
*/


/*********************************************************************/


__C_LINK int __DPSCALL DpsBaseOpen(DPS_BASE_PARAM *P, int mode) {
  unsigned int hash;
  size_t filenamelen, z;
  ssize_t wr;
  DPS_BASEITEM  *hTable;
#ifdef DEBUG_SEARCH
  unsigned long total_ticks, stop_ticks, start_ticks = DpsStartTimer();
#endif

  TRACE_IN(P->A, "DpsBaseOpen");

  if (P->opened) DpsBaseClose(P);

  if (P->NFiles == 0) P->NFiles = DpsVarListFindUnsigned(&P->A->Vars, "BaseFiles", 0x100);
  P->FileNo =  DPS_FILENO(P->rec_id, P->NFiles);

  hash = DPS_HASH(P->rec_id);
  filenamelen = dps_strlen(P->vardir) + dps_strlen(P->subdir) + dps_strlen(P->indname) + dps_strlen(P->basename) +  48;
  if (
      ((P->Ifilename = (char *)DpsMalloc(filenamelen)) == NULL) ||
      ((P->Sfilename = (char *)DpsMalloc(filenamelen)) == NULL)            ) {
    DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
    DpsLog(P->A, DPS_LOG_ERROR, "Memory alloc error 2x%d bytes %s:%d", filenamelen, __FILE__, __LINE__);
    TRACE_OUT(P->A);
    return DPS_ERROR;
  }
  sprintf(P->Sfilename, "%s/%s/%s%04zx.s", P->vardir, P->subdir, P->basename, P->FileNo);
  sprintf(P->Ifilename, "%s/%s/%s%04zx.i", P->vardir, P->subdir, P->indname, P->FileNo);

  if ((P->Ifd = DpsOpen2(P->Ifilename, ((mode == DPS_READ_LOCK) ? O_RDONLY : O_RDWR) | DPS_BINARY)) < 0) {
    if ((mode == DPS_READ_LOCK) || ((P->Ifd = DpsOpen3(P->Ifilename, O_RDWR | O_CREAT | DPS_BINARY
/*#ifdef O_DIRECT
		     | O_DIRECT
#endif*/
						   ,
						   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
						   )) < 0)) {
      dps_strerror(P->A, (mode == DPS_READ_LOCK && errno == ENOENT) ? DPS_LOG_DEBUG : DPS_LOG_ERROR, "Can't open/create file %s for %s [%s:%d]", 
	     P->Ifilename, (mode == DPS_READ_LOCK) ? "read" : "write", __FILE__, __LINE__);
      DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
      TRACE_OUT(P->A);
      return DPS_ERROR;
    }
#if 1
    DPS_GETLOCK(P->A, DPS_LOCK_BASE_N(P->FileNo));
#endif
    DpsWriteLock(P->Ifd);
    if ((hTable = (DPS_BASEITEM *)DpsXmalloc(sizeof(DPS_BASEITEM) * DPS_HASH_PRIME)) == NULL) {
      DpsLog(P->A, DPS_LOG_ERROR, "Memory alloc error hTable: %d bytes", sizeof(DPS_BASEITEM) * DPS_HASH_PRIME);
      DpsUnLock(P->Ifd); 
#if 1
      DPS_RELEASELOCK(P->A, DPS_LOCK_BASE_N(P->FileNo));
#endif
      DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
      TRACE_OUT(P->A);
      return DPS_ERROR;
    }
    if ( (wr = write(P->Ifd, hTable, sizeof(DPS_BASEITEM) * DPS_HASH_PRIME)) != sizeof(DPS_BASEITEM) * DPS_HASH_PRIME) {
      dps_strerror(P->A, DPS_LOG_ERROR, "Can't set new index for file %s\nwritten %d bytes of %d\nIfd:%d hTable:%x", 
	     P->Ifilename, wr, sizeof(DPS_BASEITEM) * DPS_HASH_PRIME, P->Ifd, hTable);
      DPS_FREE(hTable);
      DpsUnLock(P->Ifd); 
#if 1
      DPS_RELEASELOCK(P->A, DPS_LOCK_BASE_N(P->FileNo));
#endif
      DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
      TRACE_OUT(P->A);
      return DPS_ERROR;
    }
    DpsUnLock(P->Ifd); 
#if 1
    DPS_RELEASELOCK(P->A, DPS_LOCK_BASE_N(P->FileNo));
#endif
    DPS_FREE(hTable);
    if (lseek(P->Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't seek for file %s", P->Ifilename);
      DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
      TRACE_OUT(P->A);
      return DPS_ERROR;
    }
  }
  if (!P->A->Flags.cold_var) {
#if 1
    DPS_GETLOCK(P->A, DPS_LOCK_BASE_N(P->FileNo));
#endif
    switch (mode) {
    case DPS_READ_LOCK:
      DpsReadLock(P->Ifd);
      break;
    case DPS_WRITE_LOCK:
      DpsWriteLock(P->Ifd);
      break;
    }
    P->locked = 1;
  }

  if ((P->Sfd = DpsOpen2(P->Sfilename, ((mode == DPS_READ_LOCK) ? O_RDONLY : O_RDWR) | DPS_BINARY
/*#ifdef O_DIRECT
		     | O_DIRECT
#endif*/
		     )) < 0) {
    if ((mode == DPS_READ_LOCK) || ((P->Sfd = DpsOpen3(P->Sfilename, O_RDWR | O_CREAT | DPS_BINARY
/*#ifdef O_DIRECT
		     | O_DIRECT
#endif*/
						   , 
						   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
						   )) < 0)) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't open/create file %s", P->Sfilename);
      DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
      TRACE_OUT(P->A);
      return DPS_ERROR;
    }
  }
  if (!P->A->Flags.cold_var) {
    switch(mode) {
    case DPS_READ_LOCK:
      DpsReadLock(P->Sfd);
      break;
    case DPS_WRITE_LOCK:
      DpsWriteLock(P->Sfd);
      break;
    }
  }

#ifdef DEBUG_SEARCH
    stop_ticks = DpsStartTimer();
    total_ticks = stop_ticks - start_ticks;
    DpsLog(P->A, DPS_LOG_EXTRA, "OpenBase1 %03X in %.5f sec.", P->FileNo, (float)total_ticks / 1000);
#endif

    for (z = 0; z < 3; z++) {

	/* search rec_id */
	if ( (P->CurrentItemPos = (dps_uint8)lseek(P->Ifd, (off_t)(hash * sizeof(DPS_BASEITEM)), SEEK_SET)) == (dps_uint8)-1) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't seeek for file %s", P->Ifilename);
	    DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
	    TRACE_OUT(P->A);
	    return DPS_ERROR;
	}
      if (read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
	DpsLog(P->A, DPS_LOG_ERROR, "{%s:%d} Can't read index for file %s seek:%ld hash: %u (%d)", 
	       __FILE__, __LINE__, P->Ifilename, P->CurrentItemPos, hash, hash);
	bzero(&P->Item, sizeof(P->Item));
/*	DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
	TRACE_OUT(P->A);
	return DPS_ERROR;
*/
      }

#ifdef DEBUG_SEARCH
      stop_ticks = DpsStartTimer();
      total_ticks = stop_ticks - start_ticks;
      DpsLog(P->A, DPS_LOG_EXTRA, "OpenBase2 %03X in %.5f sec.", P->FileNo, (float)total_ticks / 1000);
#endif

      if (P->Item.rec_id == P->rec_id || P->Item.rec_id == 0) P->mishash = 0;
      else P->mishash = 1;
      P->PreviousItemPos = P->CurrentItemPos;
      if (P->mishash)
	while((P->Item.next != 0) && (P->Item.rec_id != P->rec_id)) {
	  P->PreviousItemPos = P->CurrentItemPos;
	  P->CurrentItemPos = P->Item.next;
	  if (lseek(P->Ifd, (off_t)P->CurrentItemPos, SEEK_SET) == (off_t)-1) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't seek for file %s", P->Ifilename);
	    DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
	    TRACE_OUT(P->A);
	    return DPS_ERROR;
	  }
	  if ((wr = read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM))) != sizeof(DPS_BASEITEM)) {
	    if (wr == 0) {
	      DpsLog(P->A, DPS_LOG_ERROR, "Possible corrupted hash chain for file %s, trying to restore (%s:%d)", 
		     P->Ifilename, __FILE__, __LINE__);
	      if (lseek(P->Ifd, (off_t)P->PreviousItemPos, SEEK_SET) == (off_t)-1) {
		DpsLog(P->A, DPS_LOG_ERROR, "Can't seek for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
		DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
		TRACE_OUT(P->A);
		return DPS_ERROR;
	      }
	      if ((wr = read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM))) != sizeof(DPS_BASEITEM)) {
		DpsLog(P->A, DPS_LOG_ERROR, "Can't read previous pos for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
		DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
		TRACE_OUT(P->A);
		return DPS_ERROR;
	      }
	      P->Item.next = 0;
	      if (lseek(P->Ifd, (off_t)P->PreviousItemPos, SEEK_SET) == (off_t)-1) {
		DpsLog(P->A, DPS_LOG_ERROR, "Can't seeek for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
		DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
		TRACE_OUT(P->A);
		return DPS_ERROR;
	      }
	      if ((wr = write(P->Ifd, &P->Item, sizeof(DPS_BASEITEM))) != sizeof(DPS_BASEITEM)) {
		DpsLog(P->A, DPS_LOG_ERROR, "Can't write previous pos for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
		DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
		TRACE_OUT(P->A);
		return DPS_ERROR;
	      }
	      goto search_again;
	
	    } else {
	      DpsLog(P->A, DPS_LOG_ERROR, "Can't read hash chain for file %s %d of %d bytes (%s:%d)", 
		     P->Ifilename, wr, sizeof(DPS_BASEITEM), __FILE__, __LINE__);
	      DPS_FREE(P->Ifilename);    DPS_FREE(P->Sfilename);
	      TRACE_OUT(P->A);
	      return DPS_ERROR;
	    }
	  }
#ifdef DEBUG_SEARCH
	  stop_ticks = DpsStartTimer();
	  total_ticks = stop_ticks - start_ticks;
	  DpsLog(P->A, DPS_LOG_EXTRA, "OpenBase3 %03X in %.5f sec.", P->FileNo, (float)total_ticks / 1000);
#endif
	}
      break;
    search_again:;
    }
  P->opened = 1;
  P->mode = mode;
#ifdef DEBUG_SEARCH
  stop_ticks = DpsStartTimer();
  total_ticks = stop_ticks - start_ticks;
  DpsLog(P->A, DPS_LOG_EXTRA, "OpenBase4 %03X in %.5f sec.\n", P->FileNo, (float)total_ticks / 1000);
#endif
/*  fprintf(stderr, "Sfd:0x%x - %s\n", P->Sfd, P->Sfilename);
  fprintf(stderr, "Ifd:0x%x - %s\n", P->Ifd, P->Ifilename);*/
  TRACE_OUT(P->A);
  return DPS_OK;
}


__C_LINK int __DPSCALL DpsBaseClose(DPS_BASE_PARAM *P) {
  TRACE_IN(P->A, "DpsBaseClose");
/*  if (P->opened && (P->mode == DPS_WRITE_LOCK) ) {
      fsync(P->Sfd);
      fsync(P->Ifd);
  }*/
  if (!P->A->Flags.cold_var && P->locked) {
      DpsUnLock(P->Sfd);
      DpsUnLock(P->Ifd); 
#if 1
      DPS_RELEASELOCK(P->A, DPS_LOCK_BASE_N(P->FileNo));
#endif
      P->locked = 0;
  }
  if (P->opened){
    DpsClose(P->Sfd); 
    DpsClose(P->Ifd); 
    P->opened = 0;
  }
  DPS_FREE(P->Ifilename);
  DPS_FREE(P->Sfilename);
  TRACE_OUT(P->A);
  return DPS_OK;
}


__C_LINK int __DPSCALL DpsBaseFsync(DPS_BASE_PARAM *P) {
  if (P->opened) {
    if (P->mode == DPS_WRITE_LOCK) {
      fsync(P->Sfd);
      fsync(P->Ifd);
    }
  }
  return DPS_OK;
}


__C_LINK int __DPSCALL DpsBaseSeek(DPS_BASE_PARAM *P, int mode) {
  unsigned int hash;
  unsigned int FileNo = DPS_FILENO(P->rec_id, P->NFiles);
  ssize_t wr;

  TRACE_IN(P->A, "DpsBaseSeek");
  if (FileNo != P->FileNo || ((P->mode != mode) && (P->mode == DPS_READ_LOCK)) || P->opened == 0) {
    if (P->opened) DpsBaseClose(P);
    TRACE_OUT(P->A);
    return DpsBaseOpen(P, mode);
  }
/*  if (P->rec_id == P->Item.rec_id) return DPS_OK;*/
  hash = DPS_HASH(P->rec_id);
  
  /* search rec_id */

  if ( (P->CurrentItemPos = (dps_uint8)lseek(P->Ifd, (off_t)(hash * sizeof(DPS_BASEITEM)), SEEK_SET)) == (dps_uint8)-1) {
    DpsLog(P->A, DPS_LOG_ERROR, "Can't seeek for file %s", P->Ifilename);
    TRACE_OUT(P->A);
    return DPS_ERROR;
  }
  if (read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
    DpsLog(P->A, DPS_LOG_ERROR, "{%s:%d} Can't read index for file %s seek:%ld hash: %u (%d)", __FILE__, __LINE__, P->Ifilename, P->CurrentItemPos, hash, hash);
    TRACE_OUT(P->A);
    return DPS_ERROR;
  }

  if (P->Item.rec_id == P->rec_id || P->Item.rec_id == 0) P->mishash = 0;
  else P->mishash = 1;
  P->PreviousItemPos = P->CurrentItemPos;
  if (P->mishash)
    while((P->Item.next != 0) && (P->Item.rec_id != P->rec_id)) {
      P->PreviousItemPos = P->CurrentItemPos;
      P->CurrentItemPos = P->Item.next;
      if (lseek(P->Ifd, (off_t)P->CurrentItemPos, SEEK_SET) == (off_t)-1) {
	DpsLog(P->A, DPS_LOG_ERROR, "Can't seek for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
	TRACE_OUT(P->A);
	return DPS_ERROR;
      }
      if (( wr = read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM))) != sizeof(DPS_BASEITEM)) {
	if (wr == 0) {
	  DpsLog(P->A, DPS_LOG_ERROR, "Possible corrupted hash chain for file %s, trying to restore (%s:%d)",
		 P->Ifilename, __FILE__, __LINE__);
	  if (lseek(P->Ifd, (off_t)P->PreviousItemPos, SEEK_SET) == (off_t)-1) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't seek for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
	    TRACE_OUT(P->A);
	    return DPS_ERROR;
	  }
	  if ((wr = read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM))) != sizeof(DPS_BASEITEM)) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't read previous pos for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
	    TRACE_OUT(P->A);
	    return DPS_ERROR;
	  }
	  P->Item.next = 0;
	  if (lseek(P->Ifd, (off_t)P->PreviousItemPos, SEEK_SET) == (off_t)-1) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't seek for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
	    TRACE_OUT(P->A);
	    return DPS_ERROR;
	  }
	  if ((wr = write(P->Ifd, &P->Item, sizeof(DPS_BASEITEM))) != sizeof(DPS_BASEITEM)) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't write previous pos for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
	    TRACE_OUT(P->A);
	    return DPS_ERROR;
	  }
	  
	} else {

	  DpsLog(P->A, DPS_LOG_ERROR, "Can't read hash chain for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
	  TRACE_OUT(P->A);
	  return DPS_ERROR;
	}
      }
    }
  TRACE_OUT(P->A);
  return DPS_OK;
}


__C_LINK int __DPSCALL DpsBaseDelete(DPS_BASE_PARAM *P) {
  int res = DPS_OK;

  if ((res = DpsBaseSeek(P, DPS_WRITE_LOCK)) != DPS_OK) return res;

  if (P->Item.rec_id == P->rec_id) {

    P->Item.rec_id = 0;
    if (lseek(P->Ifd, (off_t)P->CurrentItemPos, SEEK_SET) == (off_t)-1) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't seek file %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
      return DPS_ERROR;
    }
    if (write(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't write hash chain for file %s (%s:%d)", P->Ifilename, __FILE__, __LINE__);
      return DPS_ERROR;
    }
#ifdef DEBUG_SEARCH
    DpsLog(P->A, DPS_LOG_DEBUG, "[%s/%s] Deleted rec_id: %x", P->subdir, P->basename, P->rec_id);
#endif
  } else {
    DpsLog(P->A, DPS_LOG_DEBUG, "[%s/%s] rec_id: %x not found for delete", P->subdir, P->basename, P->rec_id);
  }
  return DPS_OK;
}


__C_LINK int __DPSCALL DpsBaseWrite(DPS_BASE_PARAM *P, void *buffer, size_t len) {
  dps_uint8 NewItemPos;
  int res = DPS_OK;
  size_t size = len;
  size_t orig_size = 0;
  void *data = buffer;

#ifdef HAVE_ZLIB
  z_stream zstream;
  Byte *CData = NULL;

  bzero(&zstream, sizeof(zstream));

  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;
  zstream.next_in = buffer;

  if ( (P->zlib_method == Z_DEFLATED) 
       && (deflateInit2(&zstream, P->zlib_level, Z_DEFLATED, P->zlib_windowBits, P->zlib_memLevel, P->zlib_strategy) == Z_OK) ) {
    
      zstream.avail_in = (uInt)len;
      zstream.avail_out = (uInt)(/*sizeof(gz_header) +*/ 4096 + 2 * len);
    CData = zstream.next_out = (Byte *) DpsMalloc(zstream.avail_out);
    if (zstream.next_out == NULL) {
      return DPS_ERROR;
    }
    deflate(&zstream, Z_FINISH);
    deflateEnd(&zstream);
    orig_size = len;
    size = zstream.total_out;
    data = CData;
    
  }

#endif


  if ((res = DpsBaseSeek(P, DPS_WRITE_LOCK)) != DPS_OK) {
    goto DpsBaseWrite_exit;
  }

  if (P->Item.rec_id == P->rec_id) {
    if (P->Item.size < size) {
      if ((P->Item.offset = (dps_uint8)lseek(P->Sfd, (off_t)0, SEEK_END)) == (dps_uint8)-1) {
	DpsLog(P->A, DPS_LOG_ERROR, "Can't seek file %s {%s:%d}", P->Sfilename, __FILE__, __LINE__);
	res = DPS_ERROR;
	goto DpsBaseWrite_exit;
      }
    } else {
      if (lseek(P->Sfd, (off_t)P->Item.offset, SEEK_SET) == (off_t)-1) {
	DpsLog(P->A, DPS_LOG_ERROR, "Can't seek file %s offset %ld {%s:%d}", P->Sfilename, (long)P->Item.offset, __FILE__, __LINE__);
	res = DPS_ERROR;
	goto DpsBaseWrite_exit;
      }
    }
  } else { /* new rec_id added */
    if (P->mishash && P->Item.rec_id != 0) {
      if ((P->Item.next = NewItemPos = (dps_uint8)lseek(P->Ifd, (off_t)0, SEEK_END)) == (dps_uint8)-1) {
	DpsLog(P->A, DPS_LOG_ERROR, "Can't seek file %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
	res = DPS_ERROR;
	goto DpsBaseWrite_exit;
      }
      if (lseek(P->Ifd, (off_t)P->CurrentItemPos, SEEK_SET) == (off_t)-1) {
	res = DPS_ERROR;
	goto DpsBaseWrite_exit;
      }
      if (write(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
	res = DPS_ERROR;
	goto DpsBaseWrite_exit;
      }
      P->CurrentItemPos = NewItemPos;
      P->Item.next = (off_t)0;
    }
    P->Item.rec_id = P->rec_id;
    if ((P->Item.offset = (dps_uint8)lseek(P->Sfd, (off_t)0, SEEK_END)) == (dps_uint8)-1) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't seek file %s {%s:%d}", P->Sfilename, __FILE__, __LINE__);
      res = DPS_ERROR;
      goto DpsBaseWrite_exit;
    }
  }
  if (write(P->Sfd, data, size) != (ssize_t)size) {
    dps_strerror(P->A, DPS_LOG_ERROR, "Can't write %ld bytes at %ld of file %s {%s:%d}",
		 (long)size, (long)P->Item.offset, P->Sfilename, __FILE__, __LINE__);
    res = DPS_ERROR;
    goto DpsBaseWrite_exit;
  }
  if (lseek(P->Ifd, (off_t)P->CurrentItemPos, SEEK_SET) == (off_t)-1) {
    DpsLog(P->A, DPS_LOG_ERROR, "Can't seek file %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
    res = DPS_ERROR;
    goto DpsBaseWrite_exit;
  }

  P->Item.size = size;
  P->Item.orig_size = orig_size;
  if (write(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
    DpsLog(P->A, DPS_LOG_ERROR, "Can't write index for file %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
  }
/*  DpsBaseFsync(P->A, P);*/
#ifdef DEBUG_SEARCH
  DpsLog(P->A, DPS_LOG_DEBUG, "[%s/%s] Stored rec_id: %x Size: %d", P->subdir, P->basename, P->rec_id, len);
#endif

 DpsBaseWrite_exit:

#ifdef HAVE_ZLIB
  DPS_FREE(CData);
#endif
  return res;
}


__C_LINK int __DPSCALL DpsBaseRead(DPS_BASE_PARAM *P, void *buf, size_t len) {
  int res = DPS_OK;
#ifdef HAVE_ZLIB
  z_stream zstream;
  Byte *CDoc = NULL;
#endif

  if ((res = DpsBaseSeek(P, DPS_READ_LOCK)) != DPS_OK) return res;

  if (P->Item.rec_id == P->rec_id) {
    if (lseek(P->Sfd, (off_t)P->Item.offset, SEEK_SET) == (off_t)-1) {
      DpsLog(P->A, DPS_LOG_ERROR, "[%s/%s.%d] %ld lseek error, rec_id: %x",  
	     P->subdir, P->basename, P->FileNo, P->Item.offset, P->rec_id);
      return DPS_ERROR;
    }
    if ((P->Item.orig_size ? P->Item.orig_size : P->Item.size) > len) {
      DpsLog(P->A, DPS_LOG_ERROR, "[%s/%s] size %d->%d error, rec_id: %x",  
	     P->subdir, P->basename, (P->Item.orig_size ? P->Item.orig_size : P->Item.size), len, P->rec_id);
      return DPS_ERROR;
    }
#ifdef HAVE_ZLIB
    bzero(&zstream, sizeof(zstream));

    if ((P->zlib_method == Z_DEFLATED) && (P->Item.orig_size != 0)) {
	zstream.avail_in = (uInt)P->Item.size;
	zstream.avail_out = (uInt)len;
      zstream.next_out = (Byte *) buf;
      CDoc = zstream.next_in = (Byte *) DpsMalloc(P->Item.size + 1);
      if (CDoc == NULL) {
	return DPS_ERROR;
      }
      zstream.zalloc = Z_NULL;
      zstream.zfree = Z_NULL;
      zstream.opaque = Z_NULL;
      if (read(P->Sfd, CDoc, P->Item.size) != (ssize_t)P->Item.size) {
	DpsLog(P->A, DPS_LOG_ERROR, "[%s/%s] %d read error, rec_id: %x -- %d",  P->subdir, P->basename, P->Item.size, P->rec_id, __LINE__);
	DPS_FREE(CDoc);
	return DPS_ERROR;
      }
      inflateInit2(&zstream, P->zlib_windowBits);
      inflate(&zstream, Z_FINISH);
      inflateEnd(&zstream);
      DPS_FREE(CDoc);
    } else 
  
#endif
    if (read(P->Sfd, buf, P->Item.size) != (ssize_t)P->Item.size) {
      DpsLog(P->A, DPS_LOG_ERROR, "[%s/%s] %d read error, rec_id: %x -- %d",  P->subdir, P->basename, P->Item.size, P->rec_id, __LINE__);
      return DPS_ERROR;
    }

  } else {
    DpsLog(P->A, DPS_LOG_DEBUG, "%s:[%s/%s] Not found rec_id: %x",  P->vardir, P->subdir, P->basename, P->rec_id);
    return DPS_ERROR;
  }
#ifdef DEBUG_SEARCH
  DpsLog(P->A, DPS_LOG_DEBUG, "[%s/%s] Retrieved rec_id: %x Size: %d", P->subdir, P->basename, P->rec_id, P->Item.size);
#endif
  return DPS_OK;
}


__C_LINK void * __DPSCALL DpsBaseARead(DPS_BASE_PARAM *P, size_t *len) {
  int res = DPS_OK;
  char *buf = NULL;
#ifdef HAVE_ZLIB
  Byte *CDoc = NULL;
  z_stream zstream;
#endif

  if ((res = DpsBaseSeek(P, DPS_READ_LOCK)) != DPS_OK) {
    *len = 0;
    return NULL;
  }

  if (P->Item.rec_id == P->rec_id) {
    if (lseek(P->Sfd, (off_t)P->Item.offset, SEEK_SET) == (off_t)-1) {
      *len = 0;
      return NULL;
    }
#ifdef HAVE_ZLIB

    bzero(&zstream, sizeof(zstream));

    if ((P->zlib_method == Z_DEFLATED) && (P->Item.orig_size != 0)) {
	zstream.avail_in = (uInt)P->Item.size;
	*len = zstream.avail_out = (uInt)(2 * P->Item.size + P->Item.orig_size);
      CDoc = zstream.next_in = (Byte *) DpsMalloc(P->Item.size + 1);
      if (CDoc == NULL) {
	*len = 0;
	return NULL;
      }
      if ((buf = (char*)DpsMalloc(*len + 1)) == NULL) {
	DPS_FREE(CDoc);
	*len = 0;
	return NULL;
      }
/*      fprintf(stderr, "BaseARead: Item.size: %d  .orig_size: %d  len: %d\n", P->Item.size, P->Item.orig_size, *len);*/
      zstream.next_out = (Byte *) buf;
      zstream.zalloc = Z_NULL;
      zstream.zfree = Z_NULL;
      zstream.opaque = Z_NULL;
      if (read(P->Sfd, CDoc, P->Item.size) != (ssize_t)P->Item.size) {
	DpsLog(P->A, DPS_LOG_ERROR, "[%s/%s] %d read error, rec_id: %x, deleting... -- %d",  P->subdir, P->basename, P->Item.size, P->rec_id, __LINE__);
	DpsBaseDelete(P);
	DPS_FREE(buf);
	DPS_FREE(CDoc);
	return NULL;
      }
      inflateInit2(&zstream, P->zlib_windowBits);
      res = inflate(&zstream, Z_FINISH);
/*      fprintf(stderr, "inflate exit: %d  avail_out: %d  total_out: %d  avail_in: %d\n", 
	      res, zstream.avail_out, zstream.total_out, zstream.avail_in);*/
	;
      *len = zstream.total_out;
      inflateEnd(&zstream);
      DPS_FREE(CDoc);

    } else 
#endif
      {
	if ((buf = (char*)DpsMalloc((*len = P->Item.size) + 1)) == NULL) {
	  *len = 0;
	  return NULL;
	}
	if (read(P->Sfd, buf, P->Item.size) != (ssize_t)P->Item.size) {
	  DpsFree(buf);
	  *len = 0;
	  return NULL;
	}
      }
  } else {
    DpsLog(P->A, DPS_LOG_DEBUG, "%s:[%s/%s] Not found rec_id: %x", P->vardir, P->subdir, P->basename, P->rec_id);
    *len = 0;
    return NULL;
  }
  buf[*len] = '0';
#ifdef DEBUG_SEARCH
  DpsLog(P->A, DPS_LOG_DEBUG, "[%s/%s] ARetrieved rec_id: %x Size: %d->%d", P->subdir, P->basename, P->rec_id, P->Item.size, P->Item.orig_size);
#endif
  return buf;
}


__C_LINK int __DPSCALL DpsBaseCheckup(DPS_BASE_PARAM *P, int (*checkrec) (DPS_AGENT *A, const urlid_t rec_id)) {
  int found;
  urlid_t i;
  size_t z;
  urlid_t *todel = (int*)DpsMalloc(128 * sizeof(urlid_t));
  size_t ndel = 0, mdel = 128, totaldel = 0;

  if (todel == NULL) return DPS_ERROR;

  for (i = 0; i < (urlid_t)P->NFiles; i++) {

    if (have_sigterm || have_sigint || have_sigalrm) {
      DpsLog(P->A, DPS_LOG_EXTRA, "%s signal received. Exiting chackup", (have_sigterm) ? "SIGTERM" :
	     (have_sigint) ? "SIGINT" : "SIGALRM");
      DpsBaseClose(P);
      DPS_FREE(todel);
      return DPS_OK;
    }
    P->rec_id = i << DPS_BASE_BITS;
    if (DpsBaseOpen(P, DPS_READ_LOCK) != DPS_OK) {
      DpsBaseClose(P);
      continue;
    }
    if (lseek(P->Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't seeek for file %s", P->Ifilename);
      DpsBaseClose(P);
      DPS_FREE(todel);
      return DPS_ERROR;
    }
    while (read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) == sizeof(DPS_BASEITEM)) {
      if (P->Item.rec_id != 0) {
	found = checkrec(P->A, P->Item.rec_id);
	if (found == 0) {
	  if (ndel >= mdel) {
	    mdel += 128;
	    todel = (urlid_t*)DpsRealloc(todel, mdel * sizeof(urlid_t));
	    if (todel == NULL) {
	      DpsBaseClose(P);
	      DpsLog(P->A, DPS_LOG_ERROR, "Can't realloc %d bytes %s:%d", mdel * sizeof(urlid_t),  __FILE__, __LINE__);
	      DPS_FREE(todel);
	      return DPS_ERROR;
	    }
	  }
	  todel[ndel++] = P->Item.rec_id;
	}
      }
    }
    DpsBaseClose(P);
    for (z = 0; z < ndel; z++) {
      DpsLog(P->A, DPS_LOG_DEBUG, "Base %s/%s store %03X: deleting url_id: %X", P->subdir, P->basename, i, todel[z]);
      P->rec_id = todel[z];
      DpsBaseDelete(P);
    }
    DpsBaseClose(P);
    DpsLog(P->A, DPS_LOG_INFO, "Base %s/%s store %03X, %d lost records deleted", P->subdir, P->basename, i, ndel);
    totaldel += ndel;
    ndel = 0;
  }
  DPS_FREE(todel);
  DpsLog(P->A, DPS_LOG_EXTRA, "Total lost record(s) deleted: %d\n", totaldel);
  return DPS_OK;
}

static int cmpsi(const void *s1, const void *s2) {
  const DPS_SORTBASEITEM *d1 = (const DPS_SORTBASEITEM*)s1;
  const DPS_SORTBASEITEM *d2 = (const DPS_SORTBASEITEM*)s2;
  if ((d1->Item.offset) < (d2->Item.offset)) return -1;
  if ((d1->Item.offset) > (d2->Item.offset)) return 1;
  return 0;
}


extern __C_LINK int __DPSCALL DpsBaseOptimize(DPS_BASE_PARAM *P, int sbase) {
  struct	stat sb;
  urlid_t base, base_from, base_to;
  long unsigned ActualSize, OriginalSize, i, nitems;
  off_t pos, posold, NewItemPos, SSize;
  dps_uint8 diff, gain;
  double dr = 0.0, cr = 0.0;
  ssize_t nread; size_t rsize;
  ssize_t wr;
  int OptimizeRatio, res, error_cnt;
  char buffer[BUFSIZ];
  DPS_BASEITEM *hTable;
  DPS_SORTBASEITEM *si = NULL;

  OptimizeRatio = DpsVarListFindInt(&P->A->Vars, "OptimizeRatio", 15);

  P->mode = DPS_WRITE_LOCK;
  if (sbase == -1) {
    base_from = 0; base_to = (urlid_t)P->NFiles;
  } else {
    base_from = sbase; base_to = sbase + 1;
  }

  for (base = base_from; base < base_to; base++) {

    error_cnt = 0;
    gain = (dps_uint8)0;
    P->rec_id = ((base & DPS_BASE_MASK) << DPS_BASE_BITS);
    if (DpsBaseOpen(P, DPS_WRITE_LOCK) != DPS_OK) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't open base %s/%s {%s:%d}", P->subdir, P->basename, __FILE__, __LINE__);
      DpsBaseClose(P);
      return DPS_ERROR;
    }
    if (lseek(P->Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
      DpsLog(P->A, DPS_LOG_ERROR, "Can't seek %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
      DpsBaseClose(P);
      return DPS_ERROR;
    }

    if (fstat(P->Sfd, &sb) == 0) {
      SSize = sb.st_size;
    } else {
      if ((SSize = (off_t)lseek(P->Sfd, (off_t)0, SEEK_END)) == (off_t)-1) {
	DpsLog(P->A, DPS_LOG_ERROR, "Can't seek %s {%s:%d}", P->Sfilename, __FILE__, __LINE__);
	DpsBaseClose(P);
	return DPS_ERROR;
      }
    }

    nitems = 0;
    ActualSize = 0;
    OriginalSize = 0;
    while(read(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) == sizeof(DPS_BASEITEM)) {
      nitems++;
      if ((P->Item.rec_id != 0) && ((dps_uint8)P->Item.offset < (dps_uint8)SSize) && (P->Item.size > 0)) {
	ActualSize += (long unsigned)P->Item.size;
	OriginalSize += (long unsigned)(P->Item.orig_size ? P->Item.orig_size : P->Item.size);
      }
    }
    if (ftruncate(P->Ifd, (off_t)(nitems * sizeof(DPS_BASEITEM))) != 0) {
	dps_strerror(P->A, DPS_LOG_EXTRA, "ftruncate error (pos:%ld) [%s:%d]", (off_t)(nitems * sizeof(DPS_BASEITEM)), __FILE__, __LINE__);
    }

    dr = (nitems) ? fabs(100.0 * ((long unsigned)SSize - ActualSize) / ((double)SSize + 1.0)) : 0.0;
    cr = (nitems) ? fabs(100.0 * ActualSize / (OriginalSize + 1)) : 0.0;

    DpsLog(P->A, DPS_LOG_EXTRA, "Optimize: %s/%s base 0x%X, %ld recs defrag: %.2f%% Ratio: %.2f%% Data: %ld File: %ld", 
	   P->subdir, P->basename, P->FileNo, nitems, dr, cr,  ActualSize, (long)SSize);

    if ((dr >= (double)OptimizeRatio) || (ActualSize == 0 && SSize != 0)) {

      si = (DPS_SORTBASEITEM*)DpsMalloc((nitems + 1) * sizeof(DPS_SORTBASEITEM));

      if (si == NULL) {
	DpsLog(P->A, DPS_LOG_ERROR, "Can't alloc si (%d bytes) at {%s:%d}", (nitems + 1) * sizeof(DPS_SORTBASEITEM), __FILE__, __LINE__);
	DpsBaseClose(P);
	return DPS_ERROR;
      }
      if (lseek(P->Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
	DpsLog(P->A, DPS_LOG_ERROR, "Can't seek %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
	DpsBaseClose(P);
	DPS_FREE(si);
	return DPS_ERROR;
      }

      for (i = 0; (i < nitems) && (read(P->Ifd, &si[i].Item, sizeof(DPS_BASEITEM)) == sizeof(DPS_BASEITEM)); ) {
	if(si[i].Item.rec_id != 0 && ((dps_uint8)si[i].Item.offset < (dps_uint8)SSize) && (si[i].Item.size > 0) && (si[i].Item.size < ActualSize) ) {
	  i++;
	}
      }

      if (i < nitems) nitems = i;
      if (nitems > 1) DpsSort((void*)si, (size_t)nitems, sizeof(DPS_SORTBASEITEM), cmpsi);

      gain = (dps_uint8)0;
      pos = (off_t)0;
      posold = (off_t)0;
      if (nitems > 0) {
	if ((long unsigned)si[0].Item.offset < (long unsigned)SSize) {
	  posold = (off_t)si[0].Item.offset;
	} else {
	  si[0].Item.offset = (off_t)0;
	  si[0].Item.size = 0;
	}
      }
      if (nitems > 1) {
	if (si[0].Item.size > (rsize = (size_t)(si[1].Item.offset - si[0].Item.offset))) {
	  DpsLog(P->A, DPS_LOG_ERROR, "si[0] size adjusted by offset: %ld -> %ld", (long)si[0].Item.size, (long)rsize);
	  si[0].Item.size = rsize;
	  error_cnt++;
	}
      }
      if ((diff = (dps_uint8)posold) > 0) {
	for(
	    lseek(P->Sfd, posold, SEEK_SET), rsize = 0;
	    (rsize < si[0].Item.size) && ((nread = read(P->Sfd, buffer, 
							(rsize + BUFSIZ < si[0].Item.size) ? BUFSIZ : (si[0].Item.size - rsize) )) > 0);
	    lseek(P->Sfd, posold, SEEK_SET)
	    ) {
	  lseek(P->Sfd, pos, SEEK_SET);
	  (void)write(P->Sfd, buffer, (size_t)nread);
	  rsize += (size_t)nread;
	  posold += (off_t)nread;
	  pos += (off_t)nread;
	}
	si[0].Item.offset = 0;
	if (rsize != si[0].Item.size) {
	  DpsLog(P->A, DPS_LOG_ERROR, "si[0] size adjusted by size: %ld -> %ld", (long)si[0].Item.size, (long)rsize);
	  si[0].Item.size = rsize;
	  error_cnt++;
	}
	gain += diff;
      }
      
      if (nitems > 0)
      for (i = 0; i < nitems - 1; i++) {
	if ((long unsigned)si[i + 1].Item.offset > (long unsigned)SSize) {
	  DpsLog(P->A, DPS_LOG_ERROR, "si[%ld] too long offset: %ld > %ld, removing", i , (long)si[i + 1].Item.offset, (long)SSize);
	  si[i + 1].Item.size = 0;
	  si[i + 1].Item.offset = si[i].Item.offset + si[i].Item.size;
	  error_cnt++;
	} else {
	  pos = (off_t)(si[i].Item.offset + si[i].Item.size);
	  posold = (off_t)si[i + 1].Item.offset;
	  if (i < nitems - 2) {
	    if (si[i + 1].Item.size > (rsize = (size_t)(si[i + 2].Item.offset - si[i + 1].Item.offset))) {
	      DpsLog(P->A, DPS_LOG_ERROR, "si[%ld] size adjusted by offset: %ld -> %ld", i + 1, (long)si[i + 1].Item.size, (long)rsize );
	      si[i + 1].Item.size = rsize;
	      error_cnt++;
	    }
	  }
	  if ((diff = (dps_uint8)posold - (dps_uint8)pos) > 0) {
	    for(
		lseek(P->Sfd, posold, SEEK_SET), rsize = 0;
		(rsize < si[i + 1].Item.size) && ((nread = read(P->Sfd, buffer,
					      (rsize + BUFSIZ < si[i + 1].Item.size) ? BUFSIZ : (si[i + 1].Item.size - rsize) )) > 0);
		lseek(P->Sfd, posold, SEEK_SET)
		) {
	      lseek(P->Sfd, pos, SEEK_SET);
	      (void)write(P->Sfd, buffer, (size_t)nread);
	      rsize += (size_t)nread;
	      posold += (off_t)nread;
	      pos += (off_t)nread;
	    }
	    if (rsize != si[i + 1].Item.size) {
	      DpsLog(P->A, DPS_LOG_ERROR, "si[%ld] size adjusted by size: %ld -> %ld", i + 1, (long)si[i + 1].Item.size, (long)rsize);
	      si[i + 1].Item.size = rsize;
	      error_cnt++;
	    }
	    si[i + 1].Item.offset = si[i].Item.offset + si[i].Item.size;
	    gain += diff;
	  }
	}
      }
      posold = SSize;
      pos = (nitems) ? (off_t)(si[nitems - 1].Item.offset + si[nitems - 1].Item.size) : (off_t)0;
      if (ftruncate(P->Sfd, (off_t)(pos)) != 0) {
	dps_strerror(P->A, DPS_LOG_ERROR, "ftruncate error (pos:%ld) [%s:%d]", pos, __FILE__, __LINE__);
      }
      SSize = pos;

      if (posold > pos) {
	gain += ((dps_uint8)posold - (dps_uint8)pos);
      }

      /*if (gain != 0 || OptimizeRatio == 0 || error_cnt > 0)*/ {

	posold = lseek(P->Ifd, (off_t)0, SEEK_END);
	(void)ftruncate(P->Ifd, (off_t)0);
	lseek(P->Ifd, (off_t)0, SEEK_SET);

	if ((hTable = (DPS_BASEITEM *)DpsXmalloc(sizeof(DPS_BASEITEM) * DPS_HASH_PRIME)) == NULL) {
	  DpsLog(P->A, DPS_LOG_ERROR, "Memory alloc error hTable: %d bytes", sizeof(DPS_BASEITEM) * DPS_HASH_PRIME);
	  DpsBaseClose(P);
	  DPS_FREE(si);
	  return DPS_ERROR;
	}
	if ( (wr = write(P->Ifd, hTable, sizeof(DPS_BASEITEM) * DPS_HASH_PRIME)) != sizeof(DPS_BASEITEM) * DPS_HASH_PRIME) {
	  dps_strerror(P->A, DPS_LOG_ERROR, "[%s:%d] Can't set new index for file %s\nwritten %d bytes of %d",
		 __FILE__, __LINE__, P->Ifilename, wr, sizeof(DPS_BASEITEM) * DPS_HASH_PRIME);
	  DPS_FREE(hTable);
	  DpsBaseClose(P);
	  DPS_FREE(si);
	  return DPS_ERROR;
	}
	DPS_FREE(hTable);

	for (i = 0; i < nitems; i++) {
	  if (si[i].Item.rec_id == 0 || si[i].Item.size == 0) continue;
	  if ((long)si[i].Item.offset > (long)SSize) {
	    DpsLog(P->A, DPS_LOG_ERROR, "si[%ld] too long offset: %ld > %ld, removing", i , (long)si[i].Item.offset, (long)SSize);
	    error_cnt++;
	    continue;
	  }
	  P->rec_id = si[i].Item.rec_id;
	  if ((res = DpsBaseSeek(P, DPS_WRITE_LOCK)) != DPS_OK) {
	    DpsBaseClose(P);
	    DPS_FREE(si);
	    return res;
	  }
	  if (P->Item.rec_id != P->rec_id) {
	    if (P->mishash && P->Item.rec_id != 0) {
	      if ((P->Item.next = (dps_uint8)(NewItemPos = lseek(P->Ifd, (off_t)0, SEEK_END))) == (dps_uint8)-1) {
		DpsBaseClose(P);
		DPS_FREE(si);
		return DPS_ERROR;
	      }
	      if (lseek(P->Ifd, (off_t)P->CurrentItemPos, SEEK_SET) == (off_t)-1) {
		DpsBaseClose(P);
		DPS_FREE(si);
		return DPS_ERROR;
	      }
	      if (write(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
		DpsBaseClose(P);
		DPS_FREE(si);
		return DPS_ERROR;
	      }
	      P->CurrentItemPos = (dps_uint8)NewItemPos;
	    }
	  }
	  P->Item = si[i].Item;
	  P->Item.next = (off_t)0;
	  if (lseek(P->Ifd, (off_t)P->CurrentItemPos, SEEK_SET) == (off_t)-1) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't seek %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
	    DpsBaseClose(P);
	    DPS_FREE(si);
	    return DPS_ERROR;
	  }
	  if (write(P->Ifd, &P->Item, sizeof(DPS_BASEITEM)) != sizeof(DPS_BASEITEM)) {
	    DpsLog(P->A, DPS_LOG_ERROR, "Can't write index for file %s {%s:%d}", P->Ifilename, __FILE__, __LINE__);
	    DpsBaseClose(P);
	    DPS_FREE(si);
	    return DPS_ERROR;
	  }
	}
	pos = lseek(P->Ifd, (off_t)0, SEEK_END);
	gain += ((dps_uint8)posold - (dps_uint8)pos);

	DpsLog(P->A, DPS_LOG_DEBUG, "Optimize: %s/%s base 0x%X cleaned, %ld bytes freed", P->subdir, P->basename, base, gain);
      }

      DPS_FREE(si);
    }

    if (error_cnt) base--;
    DpsBaseClose(P);
  }
  return DPS_OK;
}



extern __C_LINK int __DPSCALL DpsBaseRelocate(DPS_AGENT *Agent, int base_type) {
  DPS_BASE_PARAM O, N;
  DPS_BASE_PARAM *Old = &O, *New = &N;
  size_t base, i, ndel, mdel = 128, data_len;
  urlid_t *todel = (int*)DpsMalloc(128 * sizeof(urlid_t));
  void *data;

  bzero(Old, sizeof(O));
  bzero(New, sizeof(N));

  switch(base_type) {
  case 0: /* stored */
    Old->subdir = "store";
    Old->basename = "doc";
    Old->indname = "doc";
    Old->mode = DPS_WRITE_LOCK;
    Old->NFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "OldStoredFiles", 0x100);
    Old->vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    Old->A = Agent;
    New->subdir = "store";
    New->basename = "doc";
    New->indname = "doc";
    New->mode = DPS_WRITE_LOCK;
    New->NFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
    New->vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    New->A = Agent;
    DpsLog(Agent, DPS_LOG_INFO, "Relocating stored database");
    break;
  case 1: /* URL data */
    Old->subdir = DPS_URLDIR;
    Old->basename = "info";
    Old->indname = "info";
    Old->mode = DPS_WRITE_LOCK;
    Old->NFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "OldURLDataFiles", 0x300);
    Old->vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    Old->A = Agent;
#ifdef HAVE_ZLIB
    O.zlib_method = Z_DEFLATED;
    O.zlib_level = 9;
    O.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
    O.zlib_memLevel = 9;
    O.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif
    New->subdir = DPS_URLDIR;
    New->basename = "info";
    New->indname = "info";
    New->mode = DPS_WRITE_LOCK;
    New->NFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "URLDataFiles", 0x300);
    New->vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    New->A = Agent;
#ifdef HAVE_ZLIB
    N.zlib_method = Z_DEFLATED;
    N.zlib_level = 9;
    N.zlib_windowBits = DPS_BASE_INFO_WINDOWBITS;
    N.zlib_memLevel = 9;
    N.zlib_strategy = DPS_BASE_INFO_STRATEGY;
#endif
    DpsLog(Agent, DPS_LOG_INFO, "Relocating URLData database");
    break;

  case 2: /* tree wrd */
    Old->subdir = DPS_TREEDIR;
    Old->basename = "wrd";
    Old->indname = "wrd";
    Old->mode = DPS_WRITE_LOCK;
    Old->NFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "OldWrdFiles", 0x300);
    Old->vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    Old->A = Agent;
#ifdef HAVE_ZLIB
    O.zlib_method = Z_DEFLATED;
    O.zlib_level = 9;
    O.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
    O.zlib_memLevel = 9;
    O.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif
    New->subdir = DPS_TREEDIR;
    New->basename = "wrd";
    New->indname = "wrd";
    New->mode = DPS_WRITE_LOCK;
    New->NFiles = (size_t)DpsVarListFindInt(&Agent->Vars, "WrdFiles", 0x300);
    New->vardir = DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    New->A = Agent;
#ifdef HAVE_ZLIB
    N.zlib_method = Z_DEFLATED;
    N.zlib_level = 9;
    N.zlib_windowBits = DPS_BASE_WRD_WINDOWBITS;
    N.zlib_memLevel = 9;
    N.zlib_strategy = DPS_BASE_WRD_STRATEGY;
#endif
    DpsLog(Agent, DPS_LOG_INFO, "Relocating Wrd database");
    break;
    
  default:
      DPS_FREE(todel);
      return DPS_OK;
  }

  for (base = 0; base < O.NFiles; base++) {
    ndel = 0;
    if (have_sigterm || have_sigint || have_sigalrm) {
      DpsLog(Agent, DPS_LOG_EXTRA, "%s signal received. Exiting chackup", (have_sigterm) ? "SIGTERM" :
	     (have_sigint) ? "SIGINT" : "SIGALRM");
      DpsBaseClose(Old);
      DpsBaseClose(New);
      DPS_FREE(todel);
      return DPS_OK;
    }

    Old->rec_id = (urlid_t)(base << DPS_BASE_BITS);
    if (DpsBaseOpen(Old, DPS_READ_LOCK) != DPS_OK) {
      DpsBaseClose(Old);
      DpsBaseClose(New);
      continue;
    }
    if (lseek(O.Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
      DpsLog(Agent, DPS_LOG_ERROR, "Can't seeek for file %s", Old->Ifilename);
      DpsBaseClose(Old);
      DpsBaseClose(New);
      DPS_FREE(todel);
      return DPS_ERROR;
    }
    while (read(Old->Ifd, &Old->Item, sizeof(DPS_BASEITEM)) == sizeof(DPS_BASEITEM)) {
      if (Old->Item.rec_id != 0) {
	if (ndel >= mdel) {
	  mdel += 128;
	  todel = (urlid_t*)DpsRealloc(todel, mdel * sizeof(urlid_t));
	  if (todel == NULL) {
	    DpsBaseClose(Old);
	    DpsBaseClose(New);
	    DpsLog(Agent, DPS_LOG_ERROR, "Can't realloc %d bytes %s:%d", mdel * sizeof(urlid_t),  __FILE__, __LINE__);
	    DPS_FREE(todel);
	    return DPS_ERROR;
	  }
	}
	todel[ndel++] = Old->Item.rec_id;
      }
    }
    DpsBaseClose(Old);
    for (i = 0; i < ndel; i++) {
      Old->rec_id = todel[i];
      data = DpsBaseARead(Old, &data_len);
      if (data == NULL) continue;
      DpsBaseDelete(Old);
      DpsBaseClose(Old);
      New->rec_id = todel[i];
      DpsBaseWrite(New, data, data_len);
      DpsBaseClose(New);
      DPS_FREE(data);
    }
    DpsLog(Agent, DPS_LOG_EXTRA, "\tbase: %d [0x%x], %d records relocated", base, base, ndel);
  }
  DPS_FREE(todel);
  for (base = N.NFiles; base < O.NFiles; base++) {
      Old->rec_id = (urlid_t)(base << DPS_BASE_BITS);
    if (DpsBaseOpen(Old, DPS_READ_LOCK) != DPS_OK) {
      DpsBaseClose(Old);
      continue;
    }
    unlink(O.Ifilename);
    unlink(O.Sfilename);
    DpsBaseClose(Old);
  }
  return DPS_OK;
}
