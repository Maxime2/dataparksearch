/* Copyright (C) 2003-2010 Datapark corp. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "dps_uniconv.h"
#include "dps_sgml.h"
#include "dps_charsetutils.h"

#ifdef HAVE_CHARSET_tscii

static unsigned char len_tscii[] = {
  /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F          */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x20 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x30 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x50 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x70 */
  1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 1, 1, 1, /* 0x80 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, /* 0x90 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xA0 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xB0 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, /* 0xC0 */
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xD0 */
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xE0 */
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, /* 0xF0 */
  /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F          */
};

static unsigned short tab_tscii[] = {
  /*   0       1       2       3       4       5       6       7      */
  /*   8       9       A       B       C       D       E       F      */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x00 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x08 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x10 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x18 */
  0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, /* 0x20 */
  0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, /* 0x28 */
  0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, /* 0x30 */
  0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F, /* 0x38 */
  0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, /* 0x40 */
  0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, /* 0x48 */
  0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, /* 0x50 */
  0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F, /* 0x58 */
  0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, /* 0x60 */
  0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F, /* 0x68 */
  0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, /* 0x70 */
  0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, '?',    /* 0x78 */
  0x0BE6, 0x0BE7, '?', 0x0B9C, 0x0BB7, 0x0BB8, 0x0BB9, 0x0BE7,    /* 0x80 */
  0x0B9C, 0x0BB7, 0x0BB8, 0x0BB9, 0x0BE7, 0x0BE8, 0x0BE9, 0x0BEA, /* 0x88 */
  0x0BEB, 0x2018, 0x2019, 0x201C, 0x201D, 0x0BEC, 0x0BED, 0x0BEE, /* 0x90 */
  0x0BEF, 0x0B99, 0x0B9E, 0x0B99, 0x0B9E, 0x0BF0, 0x0BF1, 0x0BF2, /* 0x98 */
  0x00A0, 0x0BBE, 0x0BBF, 0x0BC0, 0x0BC1, 0x0BC2, 0x0BC6, 0x0BC7, /* 0xA0 */
  0x0BC8, 0x00A9, 0x0BCC, 0x0B85, 0x0B86, 0x0B87, 0x0B88, 0x0B89, /* 0xA8 */
  0x0B8A, 0x0B8E, 0x0B8F, 0x0B90, 0x0B92, 0x0B93, 0x0B94, 0x0B83, /* 0xB0 */
  0x0B95, 0x0B99, 0x0B9A, 0x0B9E, 0x0B9F, 0x0BA3, 0x0BA4, 0x0BA8, /* 0xB8 */
  0x0BAA, 0x0BAE, 0x0BAF, 0x0BB0, 0x0BB2, 0x0BB5, 0x0BB4, 0x0BB3, /* 0xC0 */
  0x0BB1, 0x0BA9, 0x0B9F, 0x0B9F, 0x0B95, 0x0B9A, 0x0B9F, 0x0BA3, /* 0xC8 */
  0x0BA4, 0x0BA8, 0x0BAA, 0x0BAE, 0x0BAF, 0x0BB0, 0x0BB2, 0x0BB5, /* 0xD0 */
  0x0BB4, 0x0BB3, 0x0BB1, 0x0BA9, 0x0B95, 0x0B9A, 0x0B9F, 0x0BA3, /* 0xD8 */
  0x0BA4, 0x0BA8, 0x0BAA, 0x0BAE, 0x0BAF, 0x0BB0, 0x0BB2, 0x0BB5, /* 0xE0 */
  0x0BB4, 0x0BB3, 0x0BB1, 0x0BA9, 0x0B95, 0x0B99, 0x0B9A, 0x0B9E, /* 0xE8 */
  0x0B9F, 0x0BA3, 0x0BA1, 0x0BA8, 0x0BAA, 0x0BAE, 0x0BAF, 0x0BB0, /* 0xF0 */
  0x0BB2, 0x0BB5, 0x0BB4, 0x0BB3, 0x0BB1, 0x0BA9, '?', '?',       /* 0xF8 */
  /*   0       1       2       3       4       5       6       7      */
  /*   8       9       A       B       C       D       E       F      */
};
static unsigned short tab2_tscii[] = {
  /*   0       1       2       3       4       5       6       7      */
  /*   8       9       A       B       C       D       E       F      */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x00 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x08 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x10 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x18 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x20 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x28 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x30 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x38 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x40 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x48 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x50 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x58 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x60 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x68 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x70 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x78 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0BB7, /* 0x80 */
  0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0BB7, 0x0000, 0x0000, 0x0000, /* 0x88 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0x90 */
  0x0000, 0x0BC1, 0x0BC1, 0x0BC2, 0x0BC2, 0x0000, 0x0000, 0x0000, /* 0x98 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0xA0 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0xA8 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0xB0 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0xB8 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* 0xC0 */
  0x0000, 0x0000, 0x0BBF, 0x0BC0, 0x0BC1, 0x0BC1, 0x0BC1, 0x0BC1, /* 0xC8 */
  0x0BC1, 0x0BC1, 0x0BC1, 0x0BC1, 0x0BC1, 0x0BC1, 0x0BC1, 0x0BC1, /* 0xD0 */
  0x0BC1, 0x0BC1, 0x0BC1, 0x0BC1, 0x0BC2, 0x0BC2, 0x0BC2, 0x0BC2, /* 0xD8 */
  0x0BC2, 0x0BC2, 0x0BC2, 0x0BC2, 0x0BC2, 0x0BC2, 0x0BC2, 0x0BC2, /* 0xE0 */
  0x0BC2, 0x0BC2, 0x0BC2, 0x0BC2, 0x0B82, 0x0B82, 0x0B82, 0x0B82, /* 0xE8 */
  0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0B82, /* 0xF0 */
  0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0B82, 0x0000, 0x0000, /* 0xF8 */
  /*   0       1       2       3       4       5       6       7      */
  /*   8       9       A       B       C       D       E       F      */
};

unsigned char tab_0A8F[] = { 0xAC, 0xAD, 0xB2 };
unsigned char tab_0A93[] = {
  0xB0, 0xB1, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
  0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0,
  0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6
};
unsigned char tab_0AC7[] = { 0xE1, 0xE2, 0xE7 };

int
dps_mb_wc_tscii (DPS_CONV *conv, DPS_CHARSET *cs, dpsunicode_t *pwc, const unsigned char *s, const unsigned char *end)
{

  int hi;
  const unsigned char *p;
  unsigned char *e, z;
  unsigned int sw;
  int n;

  hi = s[0];
  conv->icodes = conv->ocodes = 1;

  if (hi < 0x80)
    {
      if ((*s == '&' && ((conv->flags & DPS_RECODE_HTML_FROM) || (conv->flags & DPS_RECODE_URL_FROM))) ||
          (*s == '!' && (conv->flags & DPS_RECODE_URL_FROM)))
        {
          /*if ((p = strchr(s, ';')) != NULL)*/ {
            if (s[1] == '#')
              {
                p = s + 2;
                if (s[2] == 'x' || s[2] == 'X')
                  sscanf (s + 3, "%x", &sw);
                else
                  sscanf (s + 2, "%d", &sw);
                *pwc = (dpsunicode_t) sw;
              }
            else
              {
                p = s + 1;
                if (!(conv->flags & DPS_RECODE_TEXT_FROM))
                  {
                    for (e = s + 1; (e - s < DPS_MAX_SGML_LEN) && (((*e <= 'z') && (*e >= 'a')) || ((*e <= 'Z') && (*e >= 'A'))); e++)
                      ;
                    if (/*!(conv->flags & DPS_RECODE_URL_FROM) ||*/ (*e == ';'))
                      {
                        z = *e;
                        *e = '\0';
                        n = DpsSgmlToUni (s + 1, pwc);
                        if (n == 0)
                          *pwc = 0;
                        else
                          conv->ocodes = n;
                        *e = z;
                      }
                    else
                      *pwc = 0;
                  }
                else
                  *pwc = 0;
              }
            if (*pwc)
              {
                for (; isalpha (*p) || isdigit (*p); p++)
                  ;
                if (*p == ';')
                  p++;
                return conv->icodes = (p - s /*+ 1*/);
              }
          }
        }
      pwc[0] = hi;
      return 1;
    }
  switch (len_tscii[hi])
    {
    case 3:
      pwc[2] = 0x0B82;
      conv->ocodes++;
    case 2:
      pwc[1] = tab2_tscii[hi];
      conv->ocodes++;
    case 1:
      pwc[0] = tab_tscii[hi];
    }
  return 1;
}

int
dps_wc_mb_tscii (DPS_CONV *conv, DPS_CHARSET *cs, const dpsunicode_t *wc, unsigned char *s, unsigned char *e)
{

  conv->icodes = 1;
  conv->ocodes = 1;

  if (*wc < 0x7F)
    {
      s[0] = *wc;
      if ((conv->flags & DPS_RECODE_HTML_TO) && (strchr (DPS_NULL2EMPTY (conv->CharsToEscape), (int) s[0]) != NULL))
        return DPS_CHARSET_ILUNI;
      if ((conv->flags & DPS_RECODE_URL_TO) && (s[0] == '!'))
        return DPS_CHARSET_ILUNI;
      return conv->ocodes;
    }
  switch (*wc)
    {
    case 0x00a0:
      s[0] = 0xA0;
      break;
    case 0x00a9:
      s[0] = 0xA9;
      break;
    case 0x0b83:
      s[0] = 0xB7;
      break;
    case 0x0b85:
      s[0] = 0xAB;
      break;
    case 0x0b86:
      s[0] = 0xAC;
      break;
    case 0x0b87:
      s[0] = 0xad;
      break;
    case 0x0b88:
      s[0] = 0xae;
      break;
    case 0x0b89:
      s[0] = 0xaf;
      break;
    case 0x0b8a:
      s[0] = 0xb0;
      break;
    case 0x0b8e:
      s[0] = 0xb1;
      break;
    case 0x0b8f:
      s[0] = 0xb2;
      break;
    case 0x0b90:
      s[0] = 0xb3;
      break;
    case 0x0b92:
      s[0] = 0xb4;
      break;
    case 0x0b93:
      s[0] = 0xb5;
      break;
    case 0x0b94:
      s[0] = 0xb6;
      break;
    case 0xb95:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xec;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xdc;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xcc;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xb8;
        }
      break;
    case 0xb99:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xed;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0x9b;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0x99;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xb9;
        }
      break;
    case 0xb9a:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xee;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xcd;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xdd;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xba;
        }
      break;
    case 0xb9c:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0x88;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0x83;
        }
      break;
    case 0xb9e:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xef;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0x9a;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0x9c;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xbb;
        }
      break;
    case 0xb9f:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf0;
          conv->icodes = 2;
          break;
        case 0x0bbf:
          s[0] = 0xca;
          conv->icodes = 2;
          break;
        case 0x0bc0:
          s[0] = 0xcb;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xce;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xde;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xbc;
        }
      break;
    case 0xba1:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf2;
          conv->icodes = 2;
          break;
        default:
          return DPS_CHARSET_ILUNI; /*s[0] = '?';*/
        }
      break;
    case 0xba3:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf1;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xcf;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xdf;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xbd;
        }
      break;
    case 0xba4:
      switch (wc[1])
        {
        case 0x0bc1:
          s[0] = 0xd0;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe0;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xbe;
        }
      break;
    case 0xba8:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf3;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd1;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe1;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xbf;
        }
      break;
    case 0xba9:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xfd;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xdb;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xeb;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc9;
        }
      break;
    case 0xbaa:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf4;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd2;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe2;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc0;
        }
      break;
    case 0xbae:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf5;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd3;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe3;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc1;
        }
      break;
    case 0xbaf:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf6;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd4;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe4;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc2;
        }
      break;
    case 0xbb0:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf7;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd5;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe5;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc3;
        }
      break;
    case 0xbb1:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xfc;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xda;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xea;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc8;
        }
      break;
    case 0xbb2:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf8;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd6;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe6;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc4;
        }
      break;
    case 0xbb3:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xfb;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd9;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe9;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc7;
        }
      break;
    case 0xbb4:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xfa;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd8;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe8;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc6;
        }
      break;
    case 0xbb5:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0xf9;
          conv->icodes = 2;
          break;
        case 0x0bc1:
          s[0] = 0xd7;
          conv->icodes = 2;
          break;
        case 0x0bc2:
          s[0] = 0xe7;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0xc5;
        }
      break;
    case 0xbb7:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0x89;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0x84;
        }
      break;
    case 0xbb8:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0x8a;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0x85;
        }
      break;
    case 0xbb9:
      switch (wc[1])
        {
        case 0x0b82:
          s[0] = 0x8b;
          conv->icodes = 2;
          break;
        default:
          s[0] = 0x86;
        }
      break;
    case 0x0bbe:
      s[0] = 0xa1;
      break;
    case 0x0bbf:
      s[0] = 0xa2;
      break;
    case 0x0bc0:
      s[0] = 0xa3;
      break;
    case 0x0bc1:
      s[0] = 0xa4;
      break;
    case 0x0bc2:
      s[0] = 0xa5;
      break;
    case 0x0bc6:
      s[0] = 0xa6;
      break;
    case 0x0bc7:
      s[0] = 0xa7;
      break;
    case 0x0bc8:
      s[0] = 0xa8;
      break;
    case 0x0bcc:
      s[0] = 0xaa;
      break;
    case 0x0be6:
      s[0] = 0x80;
      break;
    case 0xbe7:
      switch (wc[1])
        {
        case 0x0bb7:
          if (wc[2] == 0x0b82)
            {
              conv->icodes = 3;
              s[0] = 0x8c;
            }
          else
            {
              conv->icodes = 2;
              s[0] = 0x87;
            }
          break;
        default:
          s[0] = 0x81;
        }
      break;
    case 0x0be8:
      s[0] = 0x8d;
      break;
    case 0x0be9:
      s[0] = 0x8e;
      break;
    case 0x0bea:
      s[0] = 0x8f;
      break;
    case 0x0beb:
      s[0] = 0x90;
      break;
    case 0x0bec:
      s[0] = 0x95;
      break;
    case 0x0bed:
      s[0] = 0x96;
      break;
    case 0x0bee:
      s[0] = 0x97;
      break;
    case 0x0bef:
      s[0] = 0x98;
      break;
    case 0x0bf0:
      s[0] = 0x9d;
      break;
    case 0x0bf1:
      s[0] = 0x9e;
      break;
    case 0x0bf2:
      s[0] = 0x9f;
      break;
    case 0x2018:
      s[0] = 0x91;
      break;
    case 0x2019:
      s[0] = 0x92;
      break;
    case 0x201c:
      s[0] = 0x93;
      break;
    case 0x201d:
      s[0] = 0x94;
      break;
    default:
      return DPS_CHARSET_ILUNI; /*s[0] = '?';*/
    }

  return conv->ocodes;
}

#endif

#if 0
typedef struct back {
  unsigned char c;
  int u;
} B;

static int cmpB(const void *s1,const void *s2){
  B *b1 = (B*)s1;
  B *b2 = (B*)s2;
  return b1->u - b2->u;
}


int main() {
  B table[256];
  int i;

  for (i = 0; i < 256; i++) {
    table[i].c = i;
    table[i].u = tab_tscii[i];
  }
  DpsSort((void*)table, 256, sizeof(B), cmpB);

  for (i = 0; i < 256; i++) {
    if (table[i].u)
      printf("%02x %04x %04x %01d\n", table[i].c, table[i].u, tab2_tscii[table[i].c], len_tscii[table[i].c]);
  }

}

#endif
