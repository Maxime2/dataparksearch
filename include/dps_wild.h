/* Copyright (C) 2003-2009 Datapark corp. All rights reserved.
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

#ifndef _DPS_WILD_H
#define _DPS_WILD_H

extern int DpsWildCmp(const char *str, const char *expr);
extern __C_LINK int __DPSCALL DpsWildCaseCmp(const char *str, const char *expr);

extern int DpsUniWildCmp(const dpsunicode_t *str, const dpsunicode_t *expr);
extern int DpsUniWildCaseCmp(const dpsunicode_t *str, const dpsunicode_t *expr);

#endif
