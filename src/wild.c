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
#include "dps_wild.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>


#if 0

int DpsWildCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; wexp[y]; ++y, ++x) {
	if ((!str[x]) && (wexp[y] != '*'))return -1;
	if (wexp[y] == '*') {
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCmp(&str[x++], &wexp[y])) != 1)return ret;
	    }
	    return -1;
	}else
	if ((wexp[y] != '?') && (str[x] != wexp[y]))return 1;
    }
    return (str[x] != '\0');
}

#else
int DpsWildCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; str[x] && wexp[y]; ++y, ++x) {
      switch(wexp[y]) {
      case '*':
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCmp(&str[x++], &wexp[y])) != 1) return ret;
	    }
	    return -1;
      case '?':
	break;
      case '\\':
	y++; /* no break here by the design */
      default:
	if (str[x] != wexp[y]) return 1;
	break;
      }
    }
    if (str[x] != '\0') return 1;
    while(wexp[y] == '*' /* || wexp[y] == '?'*/) y++;
    if (wexp[y] == '\0' || wexp[y] == '$') return 0;
    return -1;
}

#endif



#if 0

int DpsWildCaseCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; wexp[y]; ++y, ++x) {
	if ((!str[x]) && (wexp[y] != '*'))return -1;
	if (wexp[y] == '*') {
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCaseCmp(&str[x++], &wexp[y])) != 1)return ret;
	    }
	    return -1;
	}else
	if ((wexp[y] != '?') && (dps_tolower(str[x]) != dps_tolower(wexp[y]))) return 1;
    }
    return (str[x] != '\0');
}

#else

int DpsWildCaseCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; str[x] && wexp[y]; ++y, ++x) {
      switch(wexp[y]) {
      case '*':
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCaseCmp(&str[x++], &wexp[y])) != 1) return ret;
	    }
	    return -1;
      case '?':
	break;
      case '\\':
	y++; /* no break here by the design */
      default:
	if (dps_tolower(str[x]) != dps_tolower(wexp[y])) return 1;
	break;
      }
    }
    if (str[x] != '\0') return 1;
    while(wexp[y] == '*' /* || wexp[y] == '?'*/) y++;
    if (wexp[y] == '\0' || wexp[y] == '$') return 0;
    return -1;
}

#endif


/* dpsunicode_t */


int DpsUniWildCmp(const dpsunicode_t *str, const dpsunicode_t *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; str[x] && wexp[y]; ++y, ++x) {
      if (wexp[y] == (dpsunicode_t)'*') {
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsUniWildCmp(&str[x++], &wexp[y])) != 1) return ret;
	    }
	    return -1;
      } else if (wexp[y] != (dpsunicode_t)'?') {
	    if (str[x] != wexp[y]) return 1;
      }
    }
    if (str[x] != (dpsunicode_t)'\0') return 1;
    while(wexp[y] == (dpsunicode_t)'*' /* || wexp[y] == (dpsunicode_t)'?' */) y++;
    if (wexp[y] == (dpsunicode_t)'\0' || wexp[y] == (dpsunicode_t)'$') return 0;
    return -1;
}


int DpsUniWildCaseCmp(const dpsunicode_t *str, const dpsunicode_t *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; str[x] && wexp[y]; ++y, ++x) {

      if (wexp[y] == (dpsunicode_t)'*') {
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsUniWildCaseCmp(&str[x++], &wexp[y])) != 1) return ret;
	    }
	    return -1;
      } else if (wexp[y] != (dpsunicode_t)'?') {
	    if (dps_tolower(str[x]) != dps_tolower(wexp[y])) return 1;
      }
    }


    if (str[x] != (dpsunicode_t)'\0') return 1;
    while(wexp[y] == (dpsunicode_t)'*' /* || wexp[y] == (dpsunicode_t)'?' */) y++;
    if (wexp[y] == (dpsunicode_t)'\0' || wexp[y] == (dpsunicode_t)'$') return 0;
    return -1;
}
