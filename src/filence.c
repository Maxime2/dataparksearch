/* Copyright (C) 2006-2007 Datapark corp. All rights reserved.

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
#include "dps_charsetutils.h"
#include "dps_utils.h"

#ifdef FILENCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_PTHREAD
# include <pthread.h>
# include <semaphore.h>
#endif

#define	MEMORY_CREATION_SIZE 1 * 1024 * 1024
#define DPS_FILENAMELEN      32

enum _Mode {
  NOT_IN_USE = 0,
  FREE,
  ALLOCATED
};
typedef enum _Mode	Mode;

struct _Slot {
  int    fd;
  Mode	 mode;
  char   filename[DPS_FILENAMELEN];
  char   *path;
  size_t fileline;
};
typedef struct _Slot	Slot;

static const char	version[] = "\n  Internal file handle debugger\n";

static Slot *		allocationList = NULL;
static size_t		allocationListSize = 0;
static size_t		slotCount = 0;
static size_t		unUsedSlots = 0;
static size_t		slotsPerPage = 0;
/*static int		internalUse = 0;
static int		noAllocationListProtection = 0;*/
static size_t		bytesPerPage = 0;

#ifdef HAVE_PTHREAD
static sem_t      FE_sem = { 0 };
static int        semEnabled = 0;
static pthread_t  semThread = (pthread_t) -1;
static int        semDepth = 0;
#endif

static void lock(void) {
#ifdef HAVE_PTHREAD
	/* Are we using a semaphore? */
	if (!semEnabled)
		return;

	/* Do we already have the semaphore? */
	if (semThread == pthread_self()) {
		/* Increment semDepth - push one stack level */
		semDepth++;
		return;
	}

	/* Wait for the semaphore. */
	while (sem_wait(&FE_sem) < 0)
		/* try again */;

	/* Let everyone know who has the semaphore. */
	semThread = pthread_self();
	semDepth++;
#endif	/* HAVE_PTHREAD */
}

static void release(void) {
#ifdef	HAVE_PTHREAD
	/* Are we using a semaphore? */
	if (!semEnabled)
		return;

	/* Do we have the semaphore?  Cannot free it if we don't. */
	if (semThread != pthread_self()) {
		if ( semThread == 0 )
			FE_InternalError(
			 "Releasing semaphore that wasn't locked.");

		else
			FE_InternalError(
			 "Semaphore doesn't belong to thread.");
	}

	/* Make sure this is positive as well. */
	if (semDepth <= 0)
		FE_InternalError("Semaphore depth");
	/* Decrement semDepth - popping one stack level */
	semDepth--;

	/* Only actually free the semaphore when we've reached the top */
	/* of our call stack. */
	if (semDepth == 0) {
		/* Zero this before actually free'ing the semaphore. */
		semThread = (pthread_t) 0;
		if (sem_post(&FE_sem) < 0)
			FE_InternalError("Failed to post the semaphore.");
	}
#endif /* HAVE_PTHREAD */
}




#if defined(_SC_PAGESIZE)
static size_t Page_Size(void) {
	return (size_t)sysconf(_SC_PAGESIZE);
}
#elif defined(_SC_PAGE_SIZE)
static size_t Page_Size(void) {
	return (size_t)sysconf(_SC_PAGE_SIZE);
}
#else
/* extern int	getpagesize(); */
static size_t Page_Size(void) {
	return getpagesize();
}
#endif




static void initialize(void) {
	size_t	size = MEMORY_CREATION_SIZE;
	size_t	slack;
	Slot *	slot;

	FE_Print(version);

	lock();

	/*
	 * Get the run-time configuration of the virtual memory page size.
 	 */
	bytesPerPage = Page_Size();

	/*
	 * Figure out how many Slot structures to allocate at one time.
	 */
	slotCount = slotsPerPage = bytesPerPage / sizeof(Slot);
	allocationListSize = bytesPerPage;

	if ( allocationListSize > size )
		size = allocationListSize;

	if ( (slack = size % bytesPerPage) != 0 )
		size += bytesPerPage - slack;

	/*
	 * Allocate memory, and break it up into two malloc buffers. The
	 * first buffer will be used for Slot structures, the second will
	 * be marked free.
	 */
	slot = allocationList = (Slot *)DpsMalloc(size);
	memset((char *)allocationList, 0, allocationListSize);

	/*
	 * Account for the two slot structures that we've used.
	 */
	unUsedSlots = slotCount;

	release();
#ifdef HAVE_PTHREAD
	if (!semEnabled) {
		semEnabled = 1;
		if (sem_init(&FE_sem, 0, 1) < 0) {
		  semEnabled = 0;
		}
	}
#endif
}

/*
 * allocateMoreSlots is called when there are only enough slot structures
 * left to support the allocation of a single malloc buffer.
 */
static void allocateMoreSlots(void) {
	size_t	newSize = allocationListSize + bytesPerPage;
	void *	newAllocation;
	void *	oldAllocation = allocationList;

	newAllocation = DpsMalloc(newSize);
	dps_memmove(newAllocation, allocationList, allocationListSize);
	memset(&(((char *)newAllocation)[allocationListSize]), 0, bytesPerPage);

	allocationList = (Slot *)newAllocation;
	allocationListSize = newSize;
	slotCount += slotsPerPage;
	unUsedSlots += slotsPerPage;

/*	DpsSort(allocationList, slotCount, sizeof(Slot), (qsort_cmp)cmp_Slot);*/
	DpsFree(oldAllocation);

}


int _DpsOpen2(const char *path, int flags, char *filename, size_t fileline) {
	register Slot *	slot;
	register size_t	count;
	Slot *		fullSlot = NULL;
	int		address;

	if ( allocationList == NULL )
		initialize();

	lock();

	if ( unUsedSlots < 2 ) {
		allocateMoreSlots();
	}
	
	for ( slot = allocationList, count = slotCount ; count > 0; count-- ) {
	  if ( slot->mode == FREE || slot->mode == NOT_IN_USE) {
	    fullSlot = slot;
	    break;
	  }
	  slot++;
	}
	if ( !fullSlot )
		FE_InternalError("No empty file slot.");

	fullSlot->fileline = fileline;
	dps_strncpy(fullSlot->filename, filename, DPS_FILENAMELEN);
	fullSlot->path = DpsStrdup(path);
	fullSlot->fd = address = open(path, flags);

	if (address > 0) {
	  fullSlot->mode = ALLOCATED;
	  unUsedSlots--;
	}

	release();

	return address;
}

int _DpsOpen3(const char *path, int flags, int mode, char *filename, size_t fileline) {
	register Slot *	slot;
	register size_t	count;
	Slot *		fullSlot = NULL;
	int		address;

	if ( allocationList == NULL )
		initialize();

	lock();

	if ( unUsedSlots < 2 ) {
		allocateMoreSlots();
	}
	
	for ( slot = allocationList, count = slotCount ; count > 0; count-- ) {
	  if ( slot->mode == FREE || slot->mode == NOT_IN_USE) {
	    fullSlot = slot;
	    break;
	  }
	  slot++;
	}
	if ( !fullSlot )
		FE_InternalError("No empty file slot.");

	fullSlot->fileline = fileline;
	dps_strncpy(fullSlot->filename, filename, DPS_FILENAMELEN);
	fullSlot->path = DpsStrdup(path);
	fullSlot->fd = address = open(path, flags, mode);

	if (address > 0) {
	  fullSlot->mode = ALLOCATED;
	  unUsedSlots--;
	}

	release();

	return address;
}


static Slot * slotForUserFd(int fd) {
	register Slot *	slot = allocationList;
	register size_t	count = slotCount;

	for ( ; count > 0; count-- ) {
		if ( slot->fd == fd )
			return slot;
		slot++;
	}

	return 0;
}

extern int _DpsClose(int fd,  char *filename, size_t fileline) {
	Slot *	slot;
	int     ret;

	if ( allocationList == NULL )
	  FE_Abort("DpsClose() called before first DpsOpen*() at %s:%d.", filename, fileline);

	lock();

	slot = slotForUserFd(fd);

	if ( !slot )
	  FE_Abort("DpsClose(%d): descriptor not from DpsOpen*() at %s:%d.", fd, filename, fileline);

	if ( slot->mode != ALLOCATED ) {
	  FE_Abort("DpsFree(%d): closing closed descriptor at %s:%d.", fd, filename, fileline);
	}
	slot->mode = FREE;
	DPS_FREE(slot->path);

	ret = close(slot->fd);

	release();

	return ret;
}


void DpsFilenceCheckLeaks(FILE *f) {

	if ( allocationList == NULL ) {
/*		FE_Abort("DpsFilenceCheckLeaks() called before first DpsOpen..().");*/
	        return;
	}

	lock();

	{
	  register Slot *	slot = allocationList;
	  register size_t	count = slotCount;
	  for ( ; count > 0; count-- ) {
	    if ( slot->mode == ALLOCATED ) {
	      fprintf(f ? f : stderr, "Unclosed FD.0x%x:%s at %s:%d\n", 
		      slot->fd, DPS_NULL2EMPTY(slot->path), slot->filename, slot->fileline);
	    }
	    slot++;
	  }
	}

	release();

  return;
}


/* Filence print */



#define	NUMBER_BUFFER_SIZE	(sizeof(fe_number) * NBBY)

static void do_abort(void) {
	/*
	 * I use kill(getpid(), SIGILL) instead of abort() because some
	 * mis-guided implementations of abort() flush stdio, which can
	 * cause malloc() or free() to be called.
	 */
	kill(getpid(), SIGILL);
	/* Just in case something handles SIGILL and returns, exit here. */
	_exit(-1);
}

static void
printNumber(fe_number number, fe_number base)
{
	char		buffer[NUMBER_BUFFER_SIZE];
	char *		s = &buffer[NUMBER_BUFFER_SIZE];
	int		size;
	
	do {
		fe_number	digit;

		if ( --s == buffer )
			FE_Abort("Internal error printing number.");

		digit = number % base;

		if ( digit < 10 )
			*s = '0' + digit;
		else
			*s = 'a' + digit - 10;

	} while ( (number /= base) > 0 );

	size = &buffer[NUMBER_BUFFER_SIZE] - s;

	if ( size > 0 )
		write(2, s, size);
}

void
FE_Printv(const char * pattern, va_list args)
{
	static const char	bad_pattern[] =
	 "\nBad pattern specifier %%%c in FE_Print().\n";
	const char *	s = pattern;
	char		c;

	while ( (c = *s++) != '\0' ) {
		if ( c == '%' ) {
			c = *s++;
			switch ( c ) {
			case '%':
				(void) write(2, &c, 1);
				break;
			case 'a':
				/*
				 * Print an address passed as a void pointer.
				 * The type of fe_number must be set so that
				 * it is large enough to contain all of the
				 * bits of a void pointer.
				 */
				printNumber(
				 (fe_number)va_arg(args, void *)
				,0x10);
				break;
			case 's':
				{
					const char *	string;
					size_t		length;

					string = va_arg(args, char *);
					length = dps_strlen(string);

					(void) write(2, string, length);
				}
				break;
			case 'd':
				{
					int	n = va_arg(args, int);

					if ( n < 0 ) {
						char	cc = '-';
						write(2, &cc, 1);
						n = -n;
					}
					printNumber(n, 10);
				}
				break;
			case 'x':
				printNumber(va_arg(args, u_int), 0x10);
				break;
			case 'c':
				{
					char	c = va_arg(args, int); /* Note: char is promoted to int. */
					
					(void) write(2, &c, 1);
				}
				break;
			default:
				{
					FE_Print(bad_pattern, c);
				}
		
			}
		}
		else
			(void) write(2, &c, 1);
	}
}

void
FE_Abortv(const char * pattern, va_list args)
{
	FE_Print("\nFileDebug Aborting: ");
	FE_Printv(pattern, args);
	FE_Print("\n");
	do_abort();
}

void
FE_Abort(const char * pattern, ...)
{
	va_list	args;

	va_start(args, pattern);
	FE_Abortv(pattern, args);
	/* Not reached: va_end(args); */
}

void
FE_Exitv(const char * pattern, va_list args)
{
	FE_Print("\nFileDebug Exiting: ");
	FE_Printv(pattern, args);
	FE_Print("\n");

	/*
	 * I use _exit() because the regular exit() flushes stdio,
	 * which may cause malloc() or free() to be called.
	 */
	_exit(-1);
}

void
FE_Exit(const char * pattern, ...)
{
	va_list	args;

	va_start(args, pattern);

	FE_Exitv(pattern, args);

	/* Not reached: va_end(args); */
}

void
FE_Print(const char * pattern, ...)
{
	va_list	args;

	va_start(args, pattern);
	FE_Printv(pattern, args);
	va_end(args);
}

void
FE_InternalError(const char * pattern, ...)
{
	va_list	args;

	FE_Print("\nInternal error in file allocator: ");
	va_start(args, pattern);
	FE_Printv(pattern, args);
	FE_Print("\n");
	va_end(args);
	do_abort();
}


#endif /* FILENCE */
