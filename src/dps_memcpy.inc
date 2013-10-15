/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2010-2012 DataPark Ltd. All rights reserved.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/
/*
  memmove
  memcpy
 */

#ifndef DPS_MEMMOVE_ADD
#define DPS_MEMMOVE_ADD

typedef long dps_mword;  /* up to 32 bytes long */
#define SIZEOF_DPS_MWORD SIZEOF_LONG
#define wsize sizeof(dps_mword)
#define wmask (wsize - 1)

#endif


#if (defined DPS_MEMMOVE && (defined DPS_USE_MEMMOVE_UNALIGNED || defined DPS_CONFIGURE)) || (!defined DPS_MEMMOVE && (defined DPS_USE_MEMCPY_UNALIGNED || defined DPS_CONFIGURE))

#ifdef DPS_MEMMOVE
#ifdef DPS_CONFIGURE
void *dps_memmove1(void *dst0, const void *src0, size_t length) 
#else
void *dps_memmove(void *dst0, const void *src0, size_t length) 
#endif
#else
void *dps_memcpy(void *dst0, const void *src0, size_t length) 
#endif
{
	size_t t;

  if (length == 0 || dst0 == src0)		/* nothing to do */
    return dst0;
  if ((DPS_PNTYPE)dst0 < (DPS_PNTYPE)src0) { /* copy forward */
    register char *dst = (char*)dst0;
    register const char *src = (const char*)src0;
    t = (size_t)((unsigned long)src & wmask);

    if (t) {
    	if (length < wsize) {
		t = length;
    	} else {
		t = wsize - t;
    	}
	if (t) { dst[0] = src[0];
	if (t > 1) { dst[1] = src[1];
	if (t > 2) { dst[2] = src[2];
	if (t > 3) { dst[3] = src[3];
#if SIZEOF_DPS_MWORD > 4
	if (t > 4) { dst[4] = src[4];
	if (t > 5) { dst[5] = src[5];
	if (t > 6) { dst[6] = src[6];
	if (t > 7) { dst[7] = src[7];
#if SIZEOF_DPS_MWORD > 8
	if (t > 8) { dst[8] = src[8];
	if (t > 9) { dst[9] = src[9];
	if (t > 10) { dst[10] = src[10];
	if (t > 11) { dst[11] = src[11];
	if (t > 12) { dst[12] = src[12];
	if (t > 13) { dst[13] = src[13];
	if (t > 14) { dst[14] = src[14];
	if (t > 15) { dst[15] = src[15];
#if SIZEOF_DPS_MWORD > 16
	if (t > 16) { dst[16] = src[16];
	if (t > 17) { dst[17] = src[17];
	if (t > 18) { dst[18] = src[18];
	if (t > 19) { dst[19] = src[19];
	if (t > 20) { dst[20] = src[20];
	if (t > 21) { dst[21] = src[21];
	if (t > 22) { dst[22] = src[22];
	if (t > 23) { dst[23] = src[23];
	if (t > 24) { dst[24] = src[24];
	if (t > 25) { dst[25] = src[25];
	if (t > 26) { dst[26] = src[26];
	if (t > 27) { dst[27] = src[27];
	if (t > 28) { dst[28] = src[28];
	if (t > 29) { dst[29] = src[29];
	if (t > 30) { dst[30] = src[30];
	}}}}}}}}}}}}}}}
#endif
	}}}}}}}}
#endif 
	}}}}
#endif
	}}}}
	length -= t;
	src += t; dst += t;
     }
     t = length / wsize;
     if (t) {
	register size_t n = t / 8;
    	register size_t r = (t % 8);
	register dps_mword *wdst = (dps_mword*)dst;
	register const dps_mword *wsrc = (const dps_mword*)src;
    	if (r == 0) r = 8; else n++;
    	wdst[0] = wsrc[0];
    	if (r > 1) { wdst[1] = wsrc[1];
    	if (r > 2) { wdst[2] = wsrc[2];
    	if (r > 3) { wdst[3] = wsrc[3];
    	if (r > 4) { wdst[4] = wsrc[4];
    	if (r > 5) { wdst[5] = wsrc[5];
    	if (r > 6) { wdst[6] = wsrc[6];
    	if (r > 7) { wdst[7] = wsrc[7];
	}}}}}}}
    	wsrc += r; wdst += r;
    	while (--n > 0) {
    		wdst[0] = wsrc[0];
    		wdst[1] = wsrc[1];
    		wdst[2] = wsrc[2];
    		wdst[3] = wsrc[3];
    		wdst[4] = wsrc[4];
    		wdst[5] = wsrc[5];
    		wdst[6] = wsrc[6];
    		wdst[7] = wsrc[7];
    		wsrc += 8; wdst += 8;
    	}
 	dst = (char*)wdst; src = (const char *)wsrc;
     }
     if ( (t = (length & wmask)) ) {
	if (t) { dst[0] = src[0];
	if (t > 1) { dst[1] = src[1];
	if (t > 2) { dst[2] = src[2];
	if (t > 3) { dst[3] = src[3];
#if SIZEOF_DPS_MWORD > 4
	if (t > 4) { dst[4] = src[4];
	if (t > 5) { dst[5] = src[5];
	if (t > 6) { dst[6] = src[6];
	if (t > 7) { dst[7] = src[7];
#if SIZEOF_DPS_MWORD > 8
	if (t > 8) { dst[8] = src[8];
	if (t > 9) { dst[9] = src[9];
	if (t > 10) { dst[10] = src[10];
	if (t > 11) { dst[11] = src[11];
	if (t > 12) { dst[12] = src[12];
	if (t > 13) { dst[13] = src[13];
	if (t > 14) { dst[14] = src[14];
	if (t > 15) { dst[15] = src[15];
#if SIZEOF_DPS_MWORD > 16
	if (t > 16) { dst[16] = src[16];
	if (t > 17) { dst[17] = src[17];
	if (t > 18) { dst[18] = src[18];
	if (t > 19) { dst[19] = src[19];
	if (t > 20) { dst[20] = src[20];
	if (t > 21) { dst[21] = src[21];
	if (t > 22) { dst[22] = src[22];
	if (t > 23) { dst[23] = src[23];
	if (t > 24) { dst[24] = src[24];
	if (t > 25) { dst[25] = src[25];
	if (t > 26) { dst[26] = src[26];
	if (t > 27) { dst[27] = src[27];
	if (t > 28) { dst[28] = src[28];
	if (t > 29) { dst[29] = src[29];
	if (t > 30) { dst[30] = src[30];
	}}}}}}}}}}}}}}}
#endif
	}}}}}}}}
#endif 
	}}}}
#endif
	}}}}
     }

  } else { /* copy backward */
    register char *dst = (char*)dst0 + length;
    register const char *src = (const char *)src0 + length;
    t = (size_t)((unsigned long)src & wmask);

//    fprintf(stderr, "** dst: %p t: %zu\n", dst  

    if (t) {
	if (length < wsize) {
		t = length;
	}
	dst -= t; src -= t;
	length -= t;
	switch(t) {
#if SIZEOF_DPS_MWORD > 16
	case 31: dst[30] = src[30];
	case 30: dst[29] = src[29];
	case 29: dst[28] = src[28];
	case 28: dst[27] = src[27];
	case 27: dst[26] = src[26];
	case 26: dst[25] = src[25];
	case 25: dst[24] = src[24];
	case 24: dst[23] = src[23];
	case 23: dst[22] = src[22];
	case 22: dst[21] = src[21];
	case 21: dst[20] = src[20];
	case 20: dst[19] = src[19];
	case 19: dst[18] = src[18];
	case 18: dst[17] = src[17];
	case 17: dst[16] = src[16];
#endif
#if SIZEOF_DPS_MWORD > 8
	case 16: dst[15] = src[15];
	case 15: dst[14] = src[14];
	case 14: dst[13] = src[13];
	case 13: dst[12] = src[12];
	case 12: dst[11] = src[11];
	case 11: dst[10] = src[10];
	case 10: dst[9] = src[9];
	case 9: dst[8] = src[8];
#endif
#if SIZEOF_DPS_MWORD > 4
	case 8: dst[7] = src[7];
	case 7: dst[6] = src[6];
	case 6: dst[5] = src[5];
	case 5: dst[4] = src[4];
#endif
	case 4: dst[3] = src[3];
	case 3: dst[2] = src[2];
	case 2: dst[1] = src[1];
	case 1: dst[0] = src[0];
	case 0: break;
	}
    }
    t = length / wsize;
    if (t) {
    	register size_t n = t / 8;
    	register size_t r = (t % 8);
	register dps_mword *wdst = (dps_mword*)dst;
	register const dps_mword *wsrc = (const dps_mword*)src;
    	if (r == 0) r = 8; else n++;
	wsrc -= r; wdst -= r;
	switch(r) {
	case 8:wdst[7] = wsrc[7];
	case 7:wdst[6] = wsrc[6];
	case 6:wdst[5] = wsrc[5];
	case 5:wdst[4] = wsrc[4];
	case 4:wdst[3] = wsrc[3];
	case 3:wdst[2] = wsrc[2];
	case 2:wdst[1] = wsrc[1];
	case 1:wdst[0] = wsrc[0];
	}
     	while (--n > 0) {
		wsrc -= 8; wdst -= 8;
		wdst[7] = wsrc[7];
		wdst[6] = wsrc[6];
		wdst[5] = wsrc[5];
		wdst[4] = wsrc[4];
		wdst[3] = wsrc[3];
		wdst[2] = wsrc[2];
		wdst[1] = wsrc[1];
		wdst[0] = wsrc[0];
	}
	dst = (char*)wdst; src = (const char*)wsrc;
    }
    t = length & wmask;
    dst -= t;
    src -= t;
    switch(t) {
#if SIZEOF_DPS_MWORD > 16
    case 31: dst[30] = src[30];
    case 30: dst[29] = src[29];
    case 29: dst[28] = src[28];
    case 28: dst[27] = src[27];
    case 27: dst[26] = src[26];
    case 26: dst[25] = src[25];
    case 25: dst[24] = src[24];
    case 24: dst[23] = src[23];
    case 23: dst[22] = src[22];
    case 22: dst[21] = src[21];
    case 21: dst[20] = src[20];
    case 20: dst[19] = src[19];
    case 19: dst[18] = src[18];
    case 18: dst[17] = src[17];
    case 17: dst[16] = src[16];
#endif
#if SIZEOF_DPS_MWORD > 8
    case 16: dst[15] = src[15];
    case 15: dst[14] = src[14];
    case 14: dst[13] = src[13];
    case 13: dst[12] = src[12];
    case 12: dst[11] = src[11];
    case 11: dst[10] = src[10];
    case 10: dst[9] = src[9];
    case 9: dst[8] = src[8];
#endif
#if SIZEOF_DPS_MWORD > 4
    case 8: dst[7] = src[7];
    case 7: dst[6] = src[6];
    case 6: dst[5] = src[5];
    case 5: dst[4] = src[4];
#endif
    case 4: dst[3] = src[3];
    case 3: dst[2] = src[2];
    case 2: dst[1] = src[1];
    case 1: dst[0] = src[0];
    case 0: break;
    }
  }
  return dst0;
}

#endif

