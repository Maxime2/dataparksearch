/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2004-2012 DataPark Ltd. All rights reserved.
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

#ifndef _DPS_BASE_H
#define _DPS_BASE_H


#define DPS_BASE_BITS 16      /* bits of rec_id for file no. */
#define DPS_BASE_MASK ((1UL << DPS_BASE_BITS) - 1)

#define DPS_HASH_PRIME 4093UL
/*#define DPS_HASH(x)    (((size_t)x) % DPS_HASH_PRIME)*/
#define DPS_HASH(x)    ((((unsigned long)(x)) & DPS_BASE_MASK) % DPS_HASH_PRIME)
#define DPS_FILENO(x, NFILES)  (((((unsigned long)(x)) >> DPS_BASE_BITS) & DPS_BASE_MASK) % ((unsigned long)(NFILES)))

#define DPS_READ_LOCK  0
#define DPS_WRITE_LOCK 1

#ifdef HAVE_ZLIB
#define DPS_BASE_WRD_WINDOWBITS 11
#define DPS_BASE_WRD_STRATEGY Z_DEFAULT_STRATEGY
#define DPS_BASE_INFO_WINDOWBITS 11
#define DPS_BASE_INFO_STRATEGY Z_DEFAULT_STRATEGY
#endif

typedef struct BaseItem {
  urlid_t rec_id;
  dps_uint8 offset;
  dps_uint8 next;
  size_t size;
  size_t orig_size;
} DPS_BASEITEM;

typedef struct BaseItem_4_23 {
  urlid_t rec_id;
  dps_uint8 offset;
  dps_uint8 next;
  size_t size;
} DPS_BASEITEM_4_23;

typedef struct SortBaseItem {
/*  dps_uint8 offset;*/
  DPS_BASEITEM Item;
} DPS_SORTBASEITEM;

typedef struct {
  DPS_BASEITEM  Item;
  struct dps_indexer_struct *A;
  dps_uint8 CurrentItemPos, PreviousItemPos;
  const char *subdir;
  const char *basename;
  const char *indname;
  const char *vardir;
  char *Ifilename;
  char *Sfilename;
  urlid_t rec_id;
  size_t NFiles, FileNo;
  int Ifd, Sfd;
  int  mode, mishash, opened, locked;
  int zlib_level, zlib_method, zlib_windowBits, zlib_memLevel, zlib_strategy;
} DPS_BASE_PARAM;

extern __C_LINK int __DPSCALL DpsBaseOpen(DPS_BASE_PARAM *P, int mode);
extern __C_LINK int __DPSCALL DpsBaseSeek(DPS_BASE_PARAM *P, int mode);
extern __C_LINK int __DPSCALL DpsBaseClose(DPS_BASE_PARAM *P);

extern __C_LINK int __DPSCALL DpsBaseWrite(DPS_BASE_PARAM *P, void *data, size_t len);
extern __C_LINK int __DPSCALL DpsBaseRead(DPS_BASE_PARAM *P, void *buf, size_t len);
extern __C_LINK void * __DPSCALL DpsBaseARead(DPS_BASE_PARAM *P, size_t *len);
extern __C_LINK int __DPSCALL DpsBaseDelete(DPS_BASE_PARAM *P);
extern __C_LINK int __DPSCALL DpsBaseCheckup(DPS_BASE_PARAM *P, int (*checkrec) (struct dps_indexer_struct *A, const urlid_t rec_id));
extern __C_LINK int __DPSCALL DpsBaseOptimize(DPS_BASE_PARAM *P, int base);
extern __C_LINK int __DPSCALL DpsBaseFsync(DPS_BASE_PARAM *P);
extern __C_LINK int __DPSCALL DpsBaseRelocate(struct dps_indexer_struct *Agent, int base_type);


#endif
