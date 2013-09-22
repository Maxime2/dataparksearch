/* Copyright (C) 2003-2012 DataPark Ltd. All rights reserved.
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

This is C version of a LDC Chinese segmentor

*/
/*
###############################################################################
# This software is being provided to you, the LICENSEE, by the Linguistic     #
# Data Consortium (LDC) and the University of Pennsylvania (UPENN) under the  #
# following license.  By obtaining, using and/or copying this software, you   #
# agree that you have read, understood, and will comply with these terms and  #
# conditions:                                                                 #
#                                                                             #
# Permission to use, copy, modify and distribute, including the right to      #
# grant others the right to distribute at any tier, this software and its     #
# documentation for any purpose and without fee or royalty is hereby granted, #
# provided that you agree to comply with the following copyright notice and   #
# statements, including the disclaimer, and that the same appear on ALL       #
# copies of the software and documentation, including modifications that you  #
# make for internal use or for distribution:                                  #
#                                                                             #
# Copyright 1999 by the University of Pennsylvania.  All rights reserved.     #
#                                                                             #
# THIS SOFTWARE IS PROVIDED "AS IS"; LDC AND UPENN MAKE NO REPRESENTATIONS OR #
# WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not limitation,     #
# LDC AND UPENN MAKE NO REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR   #
# FITNESS FOR ANY PARTICULAR PURPOSE.                                         #
###############################################################################
# mansegment.perl Version 1.0
# Run as: mansegment.perl [dictfile] < infile > outfile
# Mandarin segmentation for both GB and BIG5 as long as the conresponding 
# word frequency dictionary is used.
#
# Written by Zhibiao Wu at LDC on April 12 1999
#
# Algorithm: Dynamic programming to find the path which has the highest 
# multiple of word probability, the next word is selected from the longest
# phrase.
#
# dictfile is a two column text file, first column is the frequency, 
# second column is the word. The program will change the file into a dbm 
# file in the first run. So be sure to remove the dbm file if you have a
# newer version of the text file.
##############################################################################
*/
#include "dps_common.h"
#include "dps_chinese.h"
#include "dps_utils.h"
#include "dps_charsetutils.h"
#include "dps_xmalloc.h"
#include "dps_log.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_vars.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>


static int cmpchinese(const void *s1,const void *s2){
     return(DpsUniStrCmp(((const DPS_CHINAWORD*)s1)->word, ((const DPS_CHINAWORD*)s2)->word));
}


static void DpsChineseListSort(DPS_CHINALIST *List){
     if (List->nwords > 1) DpsSort(List->ChiWord, List->nwords, sizeof(DPS_CHINAWORD), cmpchinese);
}


static void DpsChineseSortForLast(DPS_CHINAWORD *List, size_t n) {
  register size_t l = 0, c, r;
  DPS_CHINAWORD T = List[ r = (n - 1) ];
  while (l < r) {
    c = (l + r) / 2;
    if ( cmpchinese(&List[c], &T) < 0) l = c + 1;
    else r = c;
  }
  if (r < (n - 1) && cmpchinese(&List[r], &T) < 0) r++;
  if (r == (n - 1)) return;
  dps_memmove(&List[r + 1], &List[r], (n - r - 1) * sizeof(DPS_CHINAWORD));
  List[r] = T;
  return;
}

__C_LINK void __DPSCALL DpsChineseListFree(DPS_CHINALIST *List){
     size_t i;
     for(i = 0; i < List->nwords; i++){
          DPS_FREE(List->ChiWord[i].word);
     }
     DPS_FREE(List->ChiWord);
     DPS_FREE(List->hash);
     List->nwords = 0;
     List->mwords = 0;
}


static DPS_CHINAWORD * DpsChineseListFind(DPS_CHINALIST *List, const dpsunicode_t *word) {
     int low  = 0;
     int high = List->nwords - 1;

     if(!List->ChiWord) return(0);
     while (low <= high) {
          int middle = (low + high) / 2;
          int match = DpsUniStrCmp(List->ChiWord[middle].word, word);
          if (match < 0)  { low = middle + 1;
          } else if (match > 0) { high = middle - 1;
          } else return(&List->ChiWord[middle]);
     }
     return(NULL);
}


static void DpsChineseListAddBundle(DPS_CHINALIST *List, DPS_CHINAWORD * chinaword){
     unsigned int h;
     size_t len;

     if (List->nwords + 1 > List->mwords) {
       List->mwords += 1024;
       List->ChiWord = (DPS_CHINAWORD *)DpsRealloc(List->ChiWord, (List->mwords)*sizeof(DPS_CHINAWORD));
       if (List->ChiWord == NULL) {
	 List->mwords = List->nwords = 0;
	 return;
       }
     }
     if (List->hash == NULL) {
       List->hash = (size_t *)DpsXmalloc(65536 * sizeof(size_t));
       if (List->hash == NULL) {
	 List->mwords = List->nwords = 0;
	 return;
       }
     }
     List->ChiWord[List->nwords].word = chinaword->word;
     List->ChiWord[List->nwords].freq = chinaword->freq;
     List->total += chinaword->freq;
     h = (unsigned int)(List->ChiWord[List->nwords].word[0] & 0xffff);
     if (List->hash[h] < (len = DpsUniLen(List->ChiWord[List->nwords].word))) {
       List->hash[h] = len;
     }
     List->nwords++;
}


static void DpsChineseListAdd(DPS_CHINALIST *List, DPS_CHINAWORD * chinaword){
  dpsunicode_t *nfc = DpsUniNormalizeNFC(NULL, chinaword->word);
  DPS_CHINAWORD cw = *chinaword;
  cw.word = nfc;
  DpsChineseListAddBundle(List, &cw);
}


int DpsChineseListLoad(DPS_AGENT *Agent, DPS_CHINALIST *List, const char *charset, const char *fname) {
     struct stat     sb;
     char *str, *data = NULL, *cur_n = NULL;
     DPS_CHINAWORD chinaword;
     char word[PATH_MAX];
     dpsunicode_t uword[256];
     DPS_CHARSET *sys_int, *fcs;
     DPS_CONV to_uni;
     int             fd;
     char            savebyte;

     sys_int = DpsGetCharSet("sys-int");
     if (!(fcs = DpsGetCharSet(charset))) {
       if (Agent->Conf->is_log_open) DpsLog(Agent, DPS_LOG_ERROR, "Charset '%s' not found or not supported", charset);
       else fprintf(stderr, "Charset '%s' not found or not supported", charset);
       return DPS_ERROR;
     }
     DpsConvInit(&to_uni, fcs, sys_int, Agent->Conf->CharsToEscape, DPS_RECODE_HTML);

     if (*fname != '/') {
       dps_snprintf(word, sizeof(word), "%s/%s", DpsVarListFindStr(&Agent->Conf->Vars, "EtcDir", DPS_CONF_DIR), fname);
       fname = word;
     }

     if (stat(fname, &sb)) {
       dps_strerror((Agent->Conf->is_log_open) ? Agent : NULL, DPS_LOG_ERROR, "Unable to stat FreqDic file '%s'", fname);
       return DPS_ERROR;
     }
     if ((fd = open(fname, O_RDONLY)) <= 0) {
       dps_strerror((Agent->Conf->is_log_open) ? Agent : NULL, DPS_LOG_ERROR, "Unable to open FreqDic file '%s'", fname);
       return DPS_ERROR;
     }
     if ((data = (char*)DpsMalloc((size_t)sb.st_size + 1)) == NULL) {
       if (Agent->Conf->is_log_open) 
	 DpsLog(Agent, DPS_LOG_ERROR, "Unable to alloc %d bytes", sb.st_size);
       else fprintf(stderr, "Unable to alloc %ld bytes", (long)sb.st_size);
       close(fd);
       return DPS_ERROR;
     }
     if (read(fd, data, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
       dps_strerror((Agent->Conf->is_log_open) ? Agent : NULL, DPS_LOG_ERROR, "Unable to read FreqDic file '%s'", fname);
       DPS_FREE(data);
       close(fd);
       return DPS_ERROR;
     }
     data[sb.st_size] = '\0';
     str = data;
     cur_n = strchr(str, NL_INT);
     if (cur_n != NULL) {
       cur_n++;
       savebyte = *cur_n;
       *cur_n = '\0';
     }
     close(fd);

     bzero((void*)&chinaword, sizeof(chinaword));
     chinaword.word = uword;
     while(str != NULL) {
          if(!str[0]) goto loop_continue;
          if(str[0]=='#') goto loop_continue;
          if (2 != sscanf(str, "%d %63s ", &chinaword.freq, word )) goto loop_continue;
	  DpsConv(&to_uni, (char*)uword, sizeof(uword), word, sizeof(word));
          DpsChineseListAdd(List, &chinaword);
     loop_continue:
	  str = cur_n;
	  if (str != NULL) {
	    *str = savebyte;
	    cur_n = strchr(str, NL_INT);
	    if (cur_n != NULL) {
	      cur_n++;
	      savebyte = *cur_n;
	      *cur_n = '\0';
	    }
	  }
     }
     DPS_FREE(data);
     DpsChineseListSort(List);
     { register size_t i, j = 0;
       for (i = 1; i < List->nwords; i++) {
	 if (cmpchinese(&List->ChiWord[j], &List->ChiWord[i]) == 0) {
	   List->ChiWord[j].freq += List->ChiWord[i].freq;
	 } else { j++;
	 }
       }
       for (i = j + 1; i < List->nwords; i++) {
	 DPS_FREE(List->ChiWord[i].word);
       }
       List->nwords = j + 1;
     }
     return DPS_OK;
}


static dpsunicode_t *DpsSegmentProcess(DPS_CHINALIST *List, dpsunicode_t *line) {
    int top, nextid, *position, *next, len, maxid, i, current, father, needinsert, iindex;
    unsigned int h;
    double *value, p;
    dpsunicode_t **result;
    dpsunicode_t *otv, space[] = {32, 0};
    DPS_CHINAWORD *chinaword, chiw;

    if (/*(line[0] >= 0x80) &&*/ (List->hash != NULL)) {

	len = DpsUniLen(line);
	maxid = 2 * len + 1;
	position = (int*)DpsMalloc(maxid * sizeof(int));
	if (position == NULL) return NULL;
	next = (int*)DpsMalloc(maxid * sizeof(int));
	if (next == NULL) {
	    DPS_FREE(position);
	    return NULL;
	}
	value = (double*)DpsMalloc(maxid * sizeof(double));
	if (value == NULL) {
	    DPS_FREE(position); DPS_FREE(next);
	    return NULL;
	}
	result = (dpsunicode_t **)DpsMalloc(maxid * sizeof(dpsunicode_t *));
	if (result == NULL) {
	    DPS_FREE(position); DPS_FREE(next); DPS_FREE(value);
	    return NULL;
	}
    
	top = 0;
	value[0] = 1.0 * List->total * len; 
	position[0] = 0;
	next[0] = -1;
	result[0] = (dpsunicode_t*)DpsUniDup(&space[1]);
	nextid = 1;

/*    fprintf(stderr, "SegmentProcess start: len -- %d\n", len);*/

	while ((top != -1) && (!((position[top] >= len) && (next[top] == -1)))) {

/*      fprintf(stderr, "top: %d  position: %d (len: %d)  next:%d\n", top, position[top], len, next[top]);*/


/*   # find the first open path */
	    current = top;
	    father = top;
	    while ((current != -1) && (position[current] >= len)) {
		father = current;
		current = next[current];
	    }
/*   # remove this path */
	    if (current == top) {
		top = next[top];
	    } else {
		next[father] = next[current];
	    }

	    if (current == -1) {
/*       # no open path, finished, take the first path */
		next[top] = -1;
	    } else {
		otv = &line[position[current]];
		h = (unsigned int)(otv[0] & 0xffff);

/*       # if the first character doesn't have word phrase in the dict.*/
		if (List->hash[h] == 0) {
		    List->hash[h] = 1 /*2*/;
		}

		i = List->hash[h];
		if (i + position[current] > len) {
		    i = len - position[current];
		}
	/*i = i + 1*/ /*2*/;
		otv = NULL;
		for (; i > 0; i-- /*2*/) {
	  /*i = i - 1*/ /*2*/;
		    DPS_FREE(otv);
		    otv = DpsUniNDup(&line[position[current]], (size_t)i);
		    chinaword = DpsChineseListFind(List, otv);

		    if (i == 1 /*2*/ && chinaword == NULL) {
			DPS_FREE(otv);
			otv = DpsUniNDup(&line[position[current]], 1/*2*/);
			chiw.word = otv;
			chiw.freq = 1;
			DpsChineseListAdd(List, chinaword = &chiw);
/*	    DpsChineseListSort(List);*/
			/*i = 1*//*2*//*;*/
		    }
		    
		    if ((chinaword != NULL) && chinaword->freq) {
/*       # pronode()   */
/*	  value[nextid] = value[current] * chinaword->freq / List->total;*/
			p = (double)chinaword->freq / List->total;
			value[nextid] = value[current] / (-1.0 * log(p) / log(10.0));
			position[nextid] = position[current] + i;
			h = DpsUniLen(result[current]) + DpsUniLen(otv) + 2;
			result[nextid] = (dpsunicode_t*)DpsXmalloc((size_t)h * sizeof(dpsunicode_t));
			if (result[nextid] == NULL) {
			    DPS_FREE(position); DPS_FREE(next); DPS_FREE(value); DPS_FREE(result);
			    return NULL;
			}
			DpsUniStrCpy(result[nextid], result[current]);
			DpsUniStrCat(result[nextid], space);
			DpsUniStrCat(result[nextid], otv);
/*
    # check to see whether there is duplicated path
    # if there is a duplicate path, remove the small value path
*/
			needinsert = 1;
			iindex = top;
			father = top;
			while (iindex != -1) {
			    if (position[iindex] == position[nextid]) {
				if (0.85 * value[iindex] >= value[nextid]) {
				    needinsert = 0;
				} else {
				    if (top == iindex) {
					next[nextid] = next[iindex];
					top = nextid;
					needinsert = 0;
    /*          } else {
	          next[nextid] = next[father];*/ /*  next[father] = next[nextid];*/
				    }
				}
				iindex = -1;
			    } else {
				father = iindex;
				iindex = next[iindex];
			    }
			}
/*    # insert the new path into the list */
/*	    fprintf(stderr, "current:%d  position:%d  i:%d  value[current]:%.12lf  nextid:%d  value[nextid]:%.12lf\n", 
		    current, position[current], i, value[current], nextid, value[nextid]);*/
			if (needinsert == 1) {
			    while ((iindex != -1) && (value[iindex] > value[nextid])) {
				father = iindex;
				iindex = next[iindex];
			    }
			    if (top == iindex) {
				next[nextid] = top;
				top = nextid;
			    } else {
				next[father] = nextid;
				next[nextid] = iindex;
			    }
			}
			nextid++;
			if (nextid >= maxid) {
			    maxid +=128;
			    position = (int*)DpsRealloc(position, maxid * sizeof(int));
			    next = (int*)DpsRealloc(next, maxid * sizeof(int));
			    value = (double*)DpsRealloc(value, maxid * sizeof(double));
			    result = (dpsunicode_t **)DpsRealloc(result, maxid * sizeof(dpsunicode_t *));
			    if (position == NULL || next == NULL || value == NULL || result == NULL) {
				DPS_FREE(position); DPS_FREE(next); DPS_FREE(value);
				if (result != NULL) {
				    for (i = 0; i < nextid; i++) {
					if (i != top) DPS_FREE(result[i]);
				    }
				    DPS_FREE(result);
				}
				return NULL;
			    }
			}
		    }

		} /*while ((i >= 1) && ( chinaword == NULL));*/


		DPS_FREE(otv);
	    }
	}

	DPS_FREE(position); DPS_FREE(next);
	for (i = 0; i < nextid; i++) {
	    if (i != top) DPS_FREE(result[i]);
	}
	otv = (top != -1) ? result[top] : NULL;
	DPS_FREE(value); DPS_FREE(result);
	return otv;
	
    } else {
	return (dpsunicode_t*)DpsUniDup(line);
    }
}


dpsunicode_t *DpsSegmentByFreq(DPS_CHINALIST *List, dpsunicode_t *line) {
  dpsunicode_t *out, *mid, *last, *sentence, *segmented_sentence, part;
  size_t i, j, l, a;
  int /*reg = 1,*/ ctype, have_bukva_forte, fb_type;
  dpsunicode_t space[] = { 32, 0 };

  l = 2 * (DpsUniLen(line) + 1);
  if (l < 2) return NULL;
  out = (dpsunicode_t*)DpsMalloc(l * sizeof(dpsunicode_t));
  if (out == NULL) return NULL;
  *out = '\0';
  mid = (dpsunicode_t*)DpsXmalloc(l * sizeof(dpsunicode_t));
  if (mid == NULL) { DPS_FREE(out); return NULL; }
  *mid = '\0';
  
  for (i = j = 0; i < DpsUniLen(line); i++) {
/*    if (line[i] >= 0x80) {
      if (reg == 0) {
	mid[j++] = *space;
	reg = 1;
      }
    } else {
      if (reg == 1) {
	mid[j++] = *space;
	reg = 0;
      }
    }*/
    mid[j++] = line[i];
  }
/*  mid[j] = 0;*/

  for (sentence = DpsUniGetSepToken(/*line*/ mid, &last, &ctype, &have_bukva_forte, 0, 0);
       sentence;
       sentence = DpsUniGetSepToken(NULL, &last, &ctype, &have_bukva_forte, 0, 0)) {
    part = *last;
    *last = 0;
    fb_type = DpsUniCType(*sentence);

    if (fb_type > DPS_UNI_BUKVA || fb_type == 2 || fb_type == 1) {
      a = 2 * (DpsUniLen(sentence) + 1);
      j = DpsUniLen(out);
      if (j + a >= l) {
	l = j + a + 1;
	out = (dpsunicode_t*)DpsRealloc(out, l * sizeof(dpsunicode_t));
	if (out == NULL) {
	  DPS_FREE(mid); return NULL;
	}
      }
      if (*out) DpsUniStrCat(out, space);
      DpsUniStrCat(out, sentence);
    } else {
      if ((segmented_sentence = DpsSegmentProcess(List, sentence)) != NULL) {
	a = 2 * (DpsUniLen(segmented_sentence) + 1);
	j = DpsUniLen(out);
	if (j + a >= l) {
	  l = j + a + 1;
	  out = (dpsunicode_t*)DpsRealloc(out, l * sizeof(dpsunicode_t));
	  if (out == NULL) {
	    DPS_FREE(mid); return NULL;
	  }
	}
	if (*out) DpsUniStrCat(out, space);
	DpsUniStrCat(out, segmented_sentence);
	DPS_FREE(segmented_sentence);
      } else {
	  DPS_FREE(mid); DPS_FREE(out);
	  return NULL;
      }
    }
    *last = part;
  }

  DPS_FREE(mid);
  
  return out;
}
