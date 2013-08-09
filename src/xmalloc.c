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


#include "dps_config.h"
#include "dps_xmalloc.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>

#ifdef EFENCE

void *_DpsXmalloc(size_t size, const char *filename, size_t fileline) {
	void *value;

	value = _DpsMalloc (size, filename, fileline);
	if (value == NULL) return NULL;
	bzero(value, size);
	return value;
}

void *_DpsXrealloc(void *ptr, size_t newsize, const char *filename, size_t fileline) {
	void *value;
	
	if(!ptr)
    	    value = _DpsXmalloc(newsize, filename, fileline);
	else
    	    value = _DpsRealloc(ptr, newsize, filename, fileline);
	return value;
}

#else

void *DpsXmalloc(size_t size) {
	void *value;

	value = DpsMalloc (size);
	if (value == NULL) return NULL;
	bzero(value, size);
	return value;
}

void *DpsXrealloc(void *ptr, size_t newsize) {
	void *value;
	
	if(!ptr)
    	    value = DpsXmalloc(newsize);
	else
    	    value = DpsRealloc(ptr, newsize);
	return value;
}

#ifdef BOEHMGC
/*
char * _DpsStrdup(const char *str) {
        size_t len;
        char *copy;

        len = dps_strlen(str) + 1;
        if ((copy = DpsMalloc(len)) == NULL)
                return (NULL);
        dps_memcpy(copy, str, len);
        return (copy);
}
*/

#else

void *DpsRealloc(void *p, size_t size) {
  void *t = p;
  p = realloc(p, size);
  if (p == NULL) if (t != NULL) DpsFree(t);
  return p;
}

#endif /* BOEHMGC */

#endif /* EFENCE */
