/* Copyright (C) 2004-2006 Datapark corp. All rights reserved.
   Based on Electric Fence 2.2 by Bruce Perens

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
#ifndef _DPS_EFENCE_H
#define _DPS_EFENCE_H

#include <sys/types.h>
#include <sys/param.h>
#include <stdarg.h>

/*
 * ef_number is the largest unsigned integer we'll need. On systems that
 * support 64-bit pointers, this may be "unsigned long long".
 */
#if defined(USE_LONG_LONG)
typedef unsigned long long	ef_number;
#else
typedef unsigned long		ef_number;
#endif

/*
 * NBBY is the number of bits per byte. Some systems define it in
 * <sys/param.h> .
 */
#ifndef	NBBY
#define	NBBY	8
#endif

/*
 * This is used to declare functions with "C" linkage if we are compiling
 * with C++ .
 */
#ifdef	__cplusplus
#define	C_LINKAGE	"C"
#else
#define	C_LINKAGE
#endif

void			Page_AllowAccess(void * address, size_t size);
void *			Page_Create(size_t size);
void			Page_Delete(void * address, size_t size);
void			Page_DenyAccess(void * address, size_t size);
size_t			Page_Size(void);

void			EF_Abort(const char * message, ...);
void			EF_Abortv(const char * message, va_list args);
void			EF_Exit(const char * message, ...);
void			EF_Exitv(const char * message, va_list args);
void			EF_Print(const char * message, ...);
void			EF_Printv(const char * message, va_list args);
void			EF_InternalError(const char * message, ...);

extern C_LINKAGE void _DpsFree(void * address, const char *filename, size_t fileline);
extern C_LINKAGE void *_DpsRealloc(void * oldBuffer, size_t newSize, const char *filename, size_t fileline);
extern C_LINKAGE void *_DpsMalloc(size_t size, const char *filename, size_t fileline);
extern C_LINKAGE void *_DpsCalloc(size_t nelem, size_t elsize, const char *filename, size_t fileline);
extern C_LINKAGE void *_DpsValloc (size_t size, const char *filename, size_t fileline);
extern C_LINKAGE char *_DpsStrdup(const char * str, const char *filename, size_t fileline);
extern C_LINKAGE void DpsEfenceCheckLeaks(void);
extern C_LINKAGE void *_DpsXmalloc(size_t size, const char *filename, size_t fileline);
extern C_LINKAGE void *_DpsXrealloc(void *ptr, size_t newsize, const char *filename, size_t fileline);

#define DpsFree(x)       _DpsFree(x, __FILE__, __LINE__)
#define DpsRealloc(x, y) _DpsRealloc(x, y, __FILE__, __LINE__)
#define DpsMalloc(x)     _DpsMalloc(x, __FILE__, __LINE__)
#define DpsCalloc(x, y)  _DpsCalloc(x, y, __FILE__, __LINE__)
#define DpsValloc(x)     _DpsValloc(x, __FILE__, __LINE__)
#define DpsStrdup(x)     _DpsStrdup(x, __FILE__, __LINE__)
#define DpsMemalign(x,y) _DpsMemalign(x, y, __FILE__, __LINE__)

#define DpsXrealloc(x, y) _DpsXrealloc(x, y, __FILE__, __LINE__)
#define DpsXmalloc(x)    _DpsXmalloc(x, __FILE__, __LINE__)

#endif /* _DPS_EFENCE_H */
