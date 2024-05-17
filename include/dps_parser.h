/* Copyright (C) 2003 Datapark corp. All rights reserved.
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

#ifndef _DPS_PARSER_H
#define _DPS_PARSER_H

/* External parsers stuff */

extern __C_LINK int __DPSCALL DpsParserAdd (DPS_PARSERLIST *, DPS_PARSER *);
extern __C_LINK DPS_PARSER *__DPSCALL DpsParserFind (DPS_PARSERLIST *, const char *mime);
extern __C_LINK void __DPSCALL DpsParserListFree (DPS_PARSERLIST *);

extern char *DpsParserExec (DPS_AGENT *Agent, DPS_PARSER *P, DPS_DOCUMENT *Doc);

#endif
