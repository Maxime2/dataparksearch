/* Copyright (C) 2003-2007 Datapark corp. All rights reserved.
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
#include <string.h>
#include "dps_uniconv.h"
#include "dps_charsetutils.h"


int dps_wc_mb_sys_int (DPS_CONV *conv, DPS_CHARSET *cs, const dpsunicode_t *wc, unsigned char *s, unsigned char *e) {
  register dpsunicode_t *wb = (dpsunicode_t*)s;
  
  conv->icodes = conv->ocodes = 1;

  if ( s+sizeof(dpsunicode_t) > e)
    return DPS_CHARSET_TOOSMALL;
  
  *wb = *wc;

/*  dps_memcpy(s, wc, sizeof(dpsunicode_t));*/
  
  return sizeof(dpsunicode_t);
}


int dps_mb_wc_sys_int (DPS_CONV *conv, DPS_CHARSET *cs, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *e) {
  register dpsunicode_t *wb = (dpsunicode_t*)s;
  
  conv->icodes = conv->ocodes = 1;

  if (s+sizeof(dpsunicode_t) > e)
    return DPS_CHARSET_TOOFEW(0);
  
  *pwc = *wb;

/*  dps_memcpy(pwc, s, sizeof(dpsunicode_t)); */
  
  return sizeof(dpsunicode_t);
}

