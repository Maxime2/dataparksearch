/* Copyright (C) 2004 Datapark corp. All rights reserved.

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
/**************************************************************************
 *
 * carry_coder.c -- functions for carryover12 encoding/decoding
 *                  part of "carry"
 * 
 * carry is the demonstration of inverted list coding scheme
 *   "carryover12", described in the paper "Inverted Index Compression
 *   using Word-aligned Binary Codes" by Anh and Moffat, which has been
 *   submitted for publication to the "Information Retrieval" journal
 *   
 * Copyright (C) 2003  Authors: Vo Ngoc Anh & Alistair Moffat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/
#include "dps_common.h"
#include "dps_carry.h"
#include "dps_log.h"
#include "dps_utils.h"

#include <stdio.h>
#include <stdlib.h>


/* bits[i]= bits needed to code gaps[i]
   return max(bits[i])
*/
int CalcMinBits(urlid_t *gaps, unsigned char *bits, size_t n, unsigned *global_max, unsigned *global_sum, unsigned *global_n) {
  register size_t i;
  register int max=0;
  register urlid_t gmax = *global_max;
  register urlid_t gsum = *global_sum;
  CLOG2TAB_VAR;
  
  for (i = 0; i < n; i++) { 
    
    QCEILLOG_2(gaps[i], bits[i]);
    if (max<bits[i]) max= bits[i];
    if (gmax<gaps[i]) gmax= gaps[i];
    gsum += gaps[i];
  }
  if (max>28)
  {
    fprintf(stderr, "Error: At least one gap exceeds 2^28. It cannot be coded by this method. Terminated.\n");
    exit(1);
  }
  *global_max= gmax;
  *global_sum= gsum;
  *global_n += n;
  return max;
}

/* given codeleng of "len" bits, and "avail" bits available for coding,
 * bits[] - sequence of sizes   
 * Return number_of_elems_coded (possible) if "avail" bits can be used to
 * code the number of elems  with the remaining < "len"
 * Returns 0 (impossible) otherwise
 */
static int elems_coded(int avail, int len, unsigned char *bits, int start, int end) {
  register int i, real_end, max;
  if (len)
  {
    max= avail/len;
    real_end= start + max - 1 <= end ? start + max: end+1; 
    for (i=start; i<real_end && bits[i]<=len; i++);
    if (i<real_end) return 0;
    return real_end-start;
  }
  else
  {
    for (i=start; i<start+MAX_ELEM_PER_WORD && i<=end && bits[i]<=len; i++);
    if (i-start<2) return 0;
    return i-start;
  }  
}
  





int
CarryDecodeFile(unsigned *a,  char *ifile,
    char *ofile, int text_file, int sequence_type)
{
  FILE *f;
  FILE *outf=NULL;
  register unsigned i;
  unsigned n;
  unsigned curr= 0;
  
  /* open input file */
  f = OpenFile(ifile,"r") ;
  if (!(outf = OpenFile(ofile, "w"))) return 0;
  
  
  CARRY_DECODE_START(f);

  while(1)
  {
    CARRY_BLOCK_DECODE_START(n);
    for (i=0; i<n; i++)
    {
      CARRY_DECODE(a[i]);
    }
  
    if (!WriteDocGaps(outf, a, n, ofile, text_file, sequence_type,&curr)) return 0;
    if (n<ELEMS_PER_BLOCK) break;
  }
  CARRY_DECODE_END;
  if (*ifile) fclose(f);
  if (*ofile) fclose(outf);
  return 1;
}

/**************************************************************************
 *
 * uncompress_io.c -- function for io uncompressed files
 *                    part of "carry"
 * 
 * "carry" is the demonstration of inverted list coding scheme
 *   "carryover12", described in the paper "Inverted Index Compression
 *   using Word-aligned Binary Codes" by Anh and Moffat, which has been
 *   submitted for publication to the "Information Retrieval" journal
 *   
 * Copyright (C) 2003  Authors: Vo Ngoc Anh & Alistair Moffat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/



FILE *
OpenFile(char *filename, char *mode)
{
  FILE *f= NULL;
  if (*filename)
    {
    if (!(f = fopen(filename, mode)))
      fprintf(stderr, "Cannot open file %s\n", filename);
    }
  else 
    f= *mode=='r'? stdin : stdout ;

  return f;
}  
    



/*-------------------------
   Read at most "*n" intergers from "filename" to "a"
   flag = 0 : input numbers intepreted as document gaps
   flag = 1 : input numbers intepreted as document number
   text_file = 1 if reading from a text file
   return 1 if Ok, 0 otherwise; 
          *n = number of items read;
-------------------------*/
   
int
ReadDocGaps(unsigned int *a, unsigned int *n, FILE *f, 
    int text_file, int flag, unsigned *global_curr)
{
  int i;
  unsigned curr=*global_curr;
  int tmp;

  for (i=0; !feof(f) && i<ELEMS_PER_BLOCK; i++)
  {
    if (text_file)
      if ( fscanf(f, " %d ", &tmp) != 1)
      {
         fprintf(stderr, "Errors when reading file\n");
         exit(1);
      }
    if (!text_file)
      if ( fread( (char*) &tmp, sizeof(int), 1, f) != 1)
      {
	 if (feof(f)) { break;};
         fprintf(stderr, "Errors when reading file \n");
         exit(1);
      }
    
    if (flag==DOCNUM)
    {
      if (tmp <= curr)
      {
	fprintf(stderr, "Error: sequence not in increasing order"
	                " at item number %d\n",i+1);
	fprintf(stderr, "Suggestion: when using -d option for compression "
	    "be sure that the input file is a sequence of positive"
	    " numbers in strictly increasing order\n");
	    
	exit(1);
      }
      a[i] = tmp - curr;
      curr = tmp;
    }
    else
    {
      if (tmp <= 0)
      {
	fprintf(stderr, "Error: invalid d-gap"
	                " at item number %d\n",i+1);
	exit(1);
      }
      a[i]= tmp;
    }
  }
  *n = i;
  *global_curr= curr;
  return i;
}


/*-------------------------
 Write "n" item from "a" to file "filename"
-------------------------*/   

int
WriteDocGaps(FILE *f, unsigned int *a, unsigned int n, char *filename, int text_file, int flag, unsigned *global_curr)
{
  unsigned i;
 
  if (flag==DOCNUM)
  { 
    a[0] = *global_curr + a[0];
    for (i=1; i<n; i++) a[i] += a[i-1];
    *global_curr= a[n-1];
  }
      
  if (text_file)
    for (i=0; i<n; i++)
    {
      if ( fprintf(f,"%u\n", a[i]) <1 )
      {
        fprintf(stderr, "Errors when writing file %s\n", filename);
        return 0;
      }
    }
  else
    if ( fwrite( (char*) a, sizeof(unsigned int), n, f) != n)
    {
      fprintf(stderr, "Errors when writing file %s\n", filename);
      return 0;
    }
  
  return 1;
}
  


/*-------------------------
-------------------------*/   
int 
CreateDocGaps(unsigned *a, unsigned *nn, double Pr)
{
  double pr;
  char *A;
  int x,i,j;
  unsigned N, n= *nn;
  unsigned newn;
  
  if (n<1) return 0;
  
  if (n>ELEMS_PER_BLOCK)
  {
    n= ELEMS_PER_BLOCK;
  }
  N= n*Pr + 0.5;
  if (N<n) N=n;
  *nn= *nn - n;
  newn= n;
    
  if (N>LIMIT)
  {
    fprintf (stderr, "Value -N and/or -p not appropriate\n");
    exit(1);
  }
  
  if (!(A = malloc(N+1)))
  {
    fprintf(stderr, "No memory\n");
    exit(1);
  }
  for (i=0; i<N; i++) A[i] = 0;
 
  while (n)
  {
    x = random()%N;
    if (!A[x]) { A[x] = 1; n--; }
  }  
  for (x=0,i=0,j=0; i<N; i++) 
    if (A[i]) {a[j++]= i+1-x; x=i+1;}
  
  free(A);
  return newn;
}

/******************************************************/

int DpsCarryLimitWrite(DPS_AGENT *Indexer, FILE *f, urlid_t *data, size_t num) {
  unsigned char *bits;
  urlid_t curr = 0;
  urlid_t *a;
  size_t i, n, z;
  int j;
  size_t avail, elems;
  unsigned size, max_bits;
  unsigned char *table, *base;
  unsigned global_max, global_sum, global_n;

  /* allocating mem for bits[i] - minimal bits needed to code a[i] */
  if (! (bits = (unsigned char*)malloc(ELEMS_PER_BLOCK * sizeof(unsigned char)))) {
    DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory [%s:%d]", __FILE__, __LINE__);
    return DPS_ERROR;
  }
  if (! (a = (urlid_t*) malloc(ELEMS_PER_BLOCK * sizeof(urlid_t)))) {
    DpsLog(Indexer, DPS_LOG_ERROR, "Out of memory [%s:%d]", __FILE__, __LINE__);
    DPS_FREE(bits);
    return DPS_ERROR;
  }

  CARRY_ENCODE_START(f);
  size = TRANS_TABLE_STARTER;
  global_max = global_sum = global_n = 0;

  for (z = 0; z < num; ) {

    for (n = 0; (n < ELEMS_PER_BLOCK) && (z < num); n++,z++) {
      a[n] = data[z] - curr;
      curr = data[z];
    }

    max_bits = CalcMinBits(a, bits, n, &global_max, &global_sum, &global_n);
    CARRY_BLOCK_ENCODE_START(n, max_bits);
    for (i=0; i<n; )
    {
      avail = GET_AVAILABLE_BITS;
      table = GET_TRANS_TABLE(avail);
      base= table+(size<<2);       /* row in trans table */
    
      /* 1. Modeling: Find j= the first-fit column in base */	
      for (j=0; j<4; j++)
      {
        size = base[j];
        if (size >avail) 		/* must use next word for data  */
        {
	  avail=32;
	  j=-1;
	  continue;
        }
        if ( (elems=elems_coded(avail,size,bits,i,n-1)) )
          break;
      }
 
      /* 2. Coding: Code elements using row "base" & column "j" */
      WORD_ENCODE(j+1,2);             /* encoding column */
      for ( ; elems ; elems--, i++)   /* encoding d-gaps */
        WORD_ENCODE(a[i],size);
    }

  }
  CARRY_ENCODE_END;

  DPS_FREE(a);
  DPS_FREE(bits);

  return DPS_OK;
}
  
  
      
  
  
