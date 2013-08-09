/* Copyright (C) 2006-2008 DataPark Ltd. All rights reserved.

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
#ifndef _DPS_FILENCE_H
#define _DPS_FILENCE_H

#ifdef FILENCE

#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(USE_LONG_LONG)
typedef unsigned long long	fe_number;
#else
typedef unsigned long		fe_number;
#endif

extern int _DpsOpen2(const char *path, int flags, char *filename, size_t fileline);
extern int _DpsOpen3(const char *path, int flags, int mode, char *filename, size_t fileline);
extern int _DpsClose(int fd, char *filename, size_t fileline);
extern void DpsFilenceCheckLeaks(FILE *f);

#define DpsOpen2(n, f)    _DpsOpen2(n, f, __FILE__, __LINE__)
#define DpsOpen3(n, f, m) _DpsOpen3(n, f, m, __FILE__, __LINE__)
#define DpsClose(d)       _DpsClose(d, __FILE__, __LINE__)

void			FE_Abort(const char * message, ...);
void			FE_Abortv(const char * message, va_list args);
void			FE_Exit(const char * message, ...);
void			FE_Exitv(const char * message, va_list args);
void			FE_Print(const char * message, ...);
void			FE_Printv(const char * message, va_list args);
void			FE_InternalError(const char * message, ...);

#else

#define DpsOpen2(n, f)    open(n, f)
#define DpsOpen3(n, f, m) open(n, f, m)
#define DpsClose(d)       close(d)

#endif


#endif /* _DPS_FILENCE_H */

