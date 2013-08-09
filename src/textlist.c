/* Copyright (C) 2003-2012 Datapark corp. All rights reserved.
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

/*#define DEBUG_MEM 1*/

#include "dps_common.h"
#include "dps_utils.h"
#include "dps_textlist.h"
#include "dps_unicode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#ifdef DEBUG_MEM
#include <sys/mman.h>
#endif

#define DPS_TEXTLIST_PAS 128

__C_LINK DPS_TEXTITEM * __DPSCALL DpsTextListAdd(DPS_TEXTLIST * tlist, const DPS_TEXTITEM *item) {
#ifdef WITH_PARANOIA
     void * paran = DpsViolationEnter(paran);
#endif
     if(!item->str) {
#ifdef WITH_PARANOIA
       DpsViolationExit(-1, paran);
#endif
       return NULL;
     }

#ifdef DEBUG_MEM
     if (tlist->mitems) {
       mprotect(tlist->Items, tlist->nitems * sizeof(DPS_TEXTITEM), PROT_READ | PROT_WRITE);
/*       fprintf(stderr, "addr: %x, len: %d till %x -- PROT_BOTH\n", tlist->Items, tlist->nitems * sizeof(DPS_TEXTITEM),
	       tlist->Items + tlist->nitems );*/
     }
#endif
     
     if (tlist->nitems + 1 > tlist->mitems) {
       tlist->mitems += DPS_TEXTLIST_PAS;
       tlist->Items = (DPS_TEXTITEM*)DpsRealloc(tlist->Items, (tlist->mitems) * sizeof(DPS_TEXTITEM) 
#ifdef DEBUG_MEM
						+ 4096
#endif
						);
       if (tlist->Items == NULL) {
	 tlist->nitems = tlist->mitems = 0;
#ifdef WITH_PARANOIA
	 DpsViolationExit(-1, paran);
#endif
	 return NULL;
       }
     }
     
     tlist->Items[tlist->nitems].str = (char*)DpsStrdup(item->str);
     tlist->Items[tlist->nitems].href = (item->href != NULL) ? (char*)DpsStrdup(item->href) : NULL;
     tlist->Items[tlist->nitems].section_name = (item->section_name != NULL) ? (char*)DpsStrdup(item->section_name) : NULL;
     tlist->Items[tlist->nitems].section = item->section;
     tlist->Items[tlist->nitems].strict = item->strict;
     tlist->Items[tlist->nitems].len = (item->len) ? item->len : dps_strlen(item->str);
     tlist->Items[tlist->nitems].marked = 0;
     tlist->nitems++;

#ifdef DEBUG_MEM
     if (tlist->mitems) {
       mprotect(tlist->Items, tlist->nitems * sizeof(DPS_TEXTITEM), PROT_READ);
/*       fprintf(stderr, "addr: %x, len: %d till %x -- PROT_READ\n", tlist->Items, tlist->nitems * sizeof(DPS_TEXTITEM),
	       tlist->Items + tlist->nitems  );*/
     }
#endif

#ifdef WITH_PARANOIA
     DpsViolationExit(-1, paran);
#endif
     return tlist->Items + (tlist->nitems - 1);
}

__C_LINK void __DPSCALL DpsTextListFree(DPS_TEXTLIST *tlist){
     size_t i;
#ifdef WITH_PARANOIA
     void * paran = DpsViolationEnter(paran);
#endif

#ifdef DEBUG_MEM
     if (tlist->mitems) {
       mprotect(tlist->Items, tlist->mitems * sizeof(DPS_TEXTITEM), PROT_READ | PROT_WRITE);
     }
#endif

     for(i = 0; i < tlist->nitems; i++) {
          DPS_FREE(tlist->Items[i].str);
          DPS_FREE(tlist->Items[i].href);
          DPS_FREE(tlist->Items[i].section_name);
     }
     DPS_FREE(tlist->Items);
     tlist->nitems = 0;
     tlist->mitems = 0;
#ifdef WITH_PARANOIA
     DpsViolationExit(-1, paran);
#endif
     return;
}
