/* Copyright (C) 2003 Datapark corp. All rights reserved.

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

#ifndef _DPS_PARSE_DATE_H
#define _DPS_PARSE_DATE_H

/* GPL
 parse_date
 author: Heiko Stoermer, innominate AG, stoermer@innominate.de
 20000316

 DpsParseDate returns allocated pointer to string containing 
 the SQL-conformant datetime type.
 processes nntp datestrings of the following types:
 
 1: 17 Jun 1999 11:05:42 GMT
 2: Wed, 08 Mar 2000 13:52:43 +0100
 
*/

extern char * DpsDateParse(const char * datestring);

#endif
