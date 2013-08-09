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

#ifndef _DPS_SGML_H
#define _DPS_SGML_H

#define DPS_MAX_SGML_LEN 32

extern __C_LINK char * __DPSCALL DpsSGMLUnescape(char * str);
extern void DpsSGMLUniUnescape(dpsunicode_t * ustr);
extern int DpsSgmlToUni(const char *sgml, dpsunicode_t *wc);
extern int DpsJSONToUni(const char *sgml, dpsunicode_t *wc, size_t *icodes);

#endif
