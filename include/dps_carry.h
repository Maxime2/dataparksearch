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
 * word_coder.h -- tools for coding at word levels
 *                 part of "carry"
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

#ifndef _DPS_CARRY_H
#define _DPS_CARRY_H



#define WORD_BUFFER_SIZE 1024

#define UNSIGNED_MASKS							\
/* __mask[i] is 2^i-1 */						\
static unsigned __mask[33]= {						\
  0x00U, 0x01U, 0x03U, 0x07U, 0x0FU,					\
  0x1FU, 0x3FU, 0x7FU, 0xFFU,						\
  0x01FFU, 0x03FFU, 0x07FFU, 0x0FFFU,					\
  0x1FFFU, 0x3FFFU, 0x7FFFU, 0xFFFFU,					\
  0x01FFFFU, 0x03FFFFU, 0x07FFFFU, 0x0FFFFFU,				\
  0x1FFFFFU, 0x3FFFFFU, 0x7FFFFFU, 0xFFFFFFU,				\
  0x01FFFFFFU, 0x03FFFFFFU, 0x07FFFFFFU, 0x0FFFFFFFU,			\
  0x1FFFFFFFU, 0x3FFFFFFFU, 0x7FFFFFFFU, 0xFFFFFFFFU } 

#define GET_AVAILABLE_BITS __wremaining

#define WORD_ENCODE_START(f)						\
{									\
  int __wremaining = 32;						\
  unsigned __value[32];							\
  unsigned __bits[32];							\
  int __pvalue = 0;							\
  FILE *__wfile= (f)
  
/* Write one coded word */  
#define WORD_ENCODE_WRITE						\
do {									\
    register unsigned word; unsigned w;					\
    word= __value[--__pvalue];						\
    for (--__pvalue; __pvalue >=0; __pvalue--)				\
    {									\
      word <<= __bits[__pvalue];					\
      word |= __value[__pvalue];					\
    }									\
    w= word;								\
    (void)fwrite((char*)&w,sizeof(w),1,__wfile);			\
    __wremaining = 32;							\
    __pvalue = 0;							\
} while(0)	
  
/* Encode int x>0 in b bits */
#define WORD_ENCODE(x,b)						\
do {									\
  if (__wremaining<b) WORD_ENCODE_WRITE;				\
  __value[__pvalue]= (x)-1;						\
  __bits[__pvalue++]= (b);						\
  __wremaining -= (b);							\
} while (0)

#define WORD_ZERO_ENCODE(x,b)						\
do {									\
  if (__wremaining<b) WORD_ENCODE_WRITE;				\
  __value[__pvalue]= (x);						\
  __bits[__pvalue++]= (b);						\
  __wremaining -= (b);							\
} while (0)

#define WORD_ENCODE_END							\
  if (__pvalue) WORD_ENCODE_WRITE;					\
}


/* ======================= MACROS FOR WORD DECODING ============ */

#define WORD_DECODE_START(f)						\
{  									\
  UNSIGNED_MASKS;							\
  register int __wremaining = -1; 					\
  register unsigned __wval = 0;						\
  unsigned __buffer[WORD_BUFFER_SIZE];					\
  register unsigned *__buffend= NULL;					\
  register unsigned *__wpos=__buffer+1;					\
  FILE *__wfile= (f)
  
#define GET_NEW_WORD							\
  if (__wpos<__buffend) __wval= *__wpos++;				\
  else									\
  {									\
    int tmp;								\
    if ( (tmp=fread((char*)&__buffer,sizeof(unsigned),WORD_BUFFER_SIZE,__wfile))<1)					\
    {									\
      fprintf (stderr, "Error when reading input file\n");		\
      exit(1);								\
    }									\
    __wpos= __buffer;							\
    __buffend= __buffer+tmp;						\
    __wval= *__wpos++;							\
  }
    
  
#define WORD_DECODE(x,b)						\
do {									\
  if (__wremaining < (b))						\
  {  									\
    GET_NEW_WORD							\
    __wremaining = 32;							\
  }									\
  (x) = (__wval & __mask[b]) + 1;					\
  __wval >>= (b);							\
  __wremaining -= (b);							\
} while (0)

#define WORD_ZERO_DECODE(x,b)						\
do {									\
  if (__wremaining < (b))						\
  {  									\
    GET_NEW_WORD							\
    __wremaining = 32;							\
  }									\
  (x) = __wval & __mask[b];						\
  __wval >>= (b);							\
  __wremaining -= (b);							\
} while (0)

#define WORD_DECODE_END							\
}



/**************************************************************************
 *
 * carry_coder.h -- macros for carryover12 coding
 *                  part of "carry"
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


int CalcMinBits(urlid_t *gaps, unsigned char *bits, size_t n, unsigned *global_max, unsigned *global_sum, unsigned *global_n);


int
CarryDecodeFile(unsigned *a,  char *ifile,
   char *ofile, int text_file, int sequence_type);



#define BIT_FOR_ZERO_LEN 6
#define MAX_ELEM_PER_WORD 64
#define MASK_FOR_ZERO_LEN 63
#define TRANS_TABLE_STARTER 33

#define ELEMS_PER_BLOCK	16384
#define BITS_FOR_ELEMS_PER_BLOCK	14

#define CEILLOG_2(x,v)                                                  \
do {                                                                    \
  register int _B_x  = (x) - 1;                                         \
  (v) = 0;                                                              \
  for (; _B_x ; _B_x>>=1, (v)++);                                       \
} while(0)

#define QCEILLOG_2(x,v)							\
do {                                                                    \
  register int _B_x  = (x) - 1;                                         \
  (v) = _B_x>>16 ?							\
         (_B_x>>24 ? 24 + CLOG2TAB[_B_x>>24] : 16 | CLOG2TAB[_B_x>>16]) \
	 :								\
	 (_B_x>>8 ? 8 + CLOG2TAB[_B_x>>8] : CLOG2TAB[_B_x]) ;		\
} while (0)
      


/* ========================================================
 Coding variables:
   trans_B1_30_big[], trans_B1_32_big are left and right transition
     tables (see the paper) for the case when the largest elements 
     occupies more than 16 bits.
   trans_B1_30_small[], trans_B1_32_small are for the otherwise case

   __pc30, __pc32 is points to the left, right tables currently used
   __pcbase points to either __pc30 or __pc32 and represents the
     current transition table used for coding
   ======================================================== */ 

#define CARRY_CODING_VARS						\
unsigned char *__pc30, *__pc32;						\
  	/* point to transition table, 30 and 32 data bits */		\
unsigned char *__pcbase;						\
        /* point to current transition table 		  */		\
									\
									\
/* *_big is transition table for the cases when number of bits		\
 needed to code the maximal value exceeds 16.				\
 *_small are used otherwise.						\
*/									\
unsigned char trans_B1_30_big[]={					\
	 0,0,0,0, 1,2,3,28, 1,2,3,28, 2,3,4,28, 3,4,5,28, 4,5,6,28, 	\
	 5,6,7,28, 6,7,8,28, 6,7,10,28, 8,10,15,28, 9,10,14,28, 	\
	 0,0,0,0, 0,0,0,0, 0,0,0,0, 10,15,16,28, 10,14,15,28, 		\
	 7,10,15,28, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 	\
	 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 		\
	 6,10,16,28, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 4,9,15,28}; 	\
	 								\
unsigned char trans_B1_32_big[]={					\
 	 0,0,0,0, 1,2,3,28, 1,2,3,28, 2,3,4,28, 3,4,5,28, 4,5,6,28,	\
	 5,6,7,28, 6,7,8,28, 7,9,10,28, 7,10,15,28, 8,10,15,28,		\
	 0,0,0,0, 0,0,0,0, 0,0,0,0, 7,10,15,28, 10,15,16,28,		\
	 10,14,15,28, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,	\
	 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,		\
	 6,10,16,28, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 4,10,16,28};	\
	 								\
									\
unsigned char trans_B1_30_small[]={					\
         0,0,0,0, 1,2,3,16, 1,2,3,16, 2,3,4,16, 3,4,5,16, 4,5,6,16,	\
 5,6,7,16, 6,7,8,16, 6,7,10,16, 7,8,10,16, 9,10,14,16, 0,0,0,0,		\
 0,0,0,0, 0,0,0,0, 8,10,15,16, 10,14,15,16,  7,10,15,16, 0,0,0,0,	\
 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,			\
 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,			\
 0,0,0,0, 0,0,0,0, 0,0,0,0, 3,7,10,16};					\
									\
unsigned char trans_B1_32_small[]={					\
 0,0,0,0, 1,2,3,16, 1,2,3,16, 2,3,4,16, 3,4,5,16, 4,5,6,16,		\
 5,6,7,16, 6,7,8,16, 7,9,10,16, 7,10,15,16, 8,10,15,16, 0,0,0,0,	\
 0,0,0,0, 0,0,0,0, 7,10,15,16, 8,10,15,16, 10,14,15,16, 0,0,0,0,	\
 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,		\
 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,		\
 0,0,0,0, 3,7,10,16}
 
#define CLOG2TAB_VAR							\
unsigned char CLOG2TAB[]={						\
0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 }





#define GET_TRANS_TABLE(avail) avail<2? (avail=30, __pc30) : (avail-=2, __pc32)
  

/* ======================= MACROS FOR carryover12 ENCODING ===== */

/* Setting variables for encoding     				
   f is output file description
   pn is (pointer to) number of elements
   max_bits is bits needed to code the largest element 
*/
#define CARRY_ENCODE_START(f) 			                        \
{   									\
  CARRY_CODING_VARS;							\
  WORD_ENCODE_START(f);		

/* Finish encoding, writing working word to output */
#define CARRY_ENCODE_END						\
  WORD_ENCODE_END;							\
}
 
#define CARRY_BLOCK_ENCODE_START(n,max_bits)				\
do {									\
  __pc30= max_bits<=16? trans_B1_30_small: trans_B1_30_big;		\
  __pc32= max_bits<=16? trans_B1_32_small: trans_B1_32_big;		\
  __pcbase= __pc30;							\
  /* write number of elements */					\
  if (n==ELEMS_PER_BLOCK)						\
    WORD_ENCODE(1,1);							\
  else									\
  {									\
    WORD_ENCODE(2,1); 							\
    WORD_ZERO_ENCODE(n,BITS_FOR_ELEMS_PER_BLOCK);			\
  }									\
  WORD_ENCODE( (max_bits<=16? 1 : 2), 1);				\
} while(0)

/* ======================= MACROS FOR carryover12 DECODING ===== */

#define CARRY_DECODE_START(f) 			                        \
{   									\
  CARRY_CODING_VARS;							\
  register int  __wbits= TRANS_TABLE_STARTER;				\
  WORD_DECODE_START(f);	
  
#define CARRY_BLOCK_DECODE_START(n)					\
do  {									\
  int tmp;								\
  WORD_DECODE(n, 1);							\
  if (n==2)								\
    WORD_ZERO_DECODE(n, BITS_FOR_ELEMS_PER_BLOCK);			\
  else								\
    n= ELEMS_PER_BLOCK;						\
  WORD_DECODE(tmp, 1);						\
  __pc30= tmp==1? trans_B1_30_small: trans_B1_30_big;			\
  __pc32= tmp==1? trans_B1_32_small: trans_B1_32_big;			\
  __pcbase= __pc30;							\
  CARRY_DECODE_GET_SELECTOR						\
} while (0)  						


#define CARRY_DECODE_END						\
  WORD_DECODE_END;							\
}

#define CARRY_DECODE_GET_SELECTOR					\
    if (__wremaining>=2)						\
    {									\
      __pcbase= __pc32;							\
      __wbits= __pcbase[(__wbits<<2)+(__wval & 3)];			\
      __wval >>= 2;							\
      __wremaining -= 2;						\
      if (__wremaining<__wbits)						\
      {									\
	GET_NEW_WORD							\
	__wremaining = 32;						\
      }									\
    }									\
    else								\
    {									\
      __pcbase= __pc30;							\
      GET_NEW_WORD							\
      __wbits= __pcbase[(__wbits<<2)+(__wval & 3)];			\
      __wval >>= 2;							\
      __wremaining = 30; 						\
    }								

#define CARRY_DECODE(x)							\
do {									\
  if (__wremaining < __wbits)						\
  {									\
    CARRY_DECODE_GET_SELECTOR						\
  }									\
  (x)= (__wval & __mask[__wbits]) +1;					\
  __wval >>= __wbits;							\
  __wremaining -= __wbits;						\
} while(0)






/**************************************************************************
 *
 * uncompress_io.h -- io uncompressed files
 * 
 * CarryOver is the demonstration of inverted list coding scheme
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


#define LIMIT 10000000
#define MAXN 1000000
#define DOCGAP 0
#define DOCNUM 1
#define ENCODING 0
#define DECODING 1
#define MAXFILENAME 255
#define MAXVALUE 0x10000000U

FILE *
OpenFile(char *filename, char *mode);

int
ReadDocGaps(unsigned int *a, unsigned int *n, FILE *f,
  int text_file, int flag, unsigned *global_curr);

int
WriteDocGaps(FILE *f, unsigned int *a, unsigned int n, char *filename,
  int text_file, int flag, unsigned *global_curr);

int
CreateDocGaps(unsigned *a, unsigned *nn, double Pr);
  

/*******************************/

int DpsCarryLimitWrite(DPS_AGENT *Indexer, FILE *f, urlid_t *data, size_t num);
 

#endif  
