/* Copyright (C) 2013 Maxim Zakharov. All rights reserved.
   Copyright (C) 2003-2011 DataPark Ltd. All rights reserved.
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

#ifndef _DPS_MUTEX_H
#define _DPS_MUTEX_H

#include <sys/types.h>

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef WITH_HTTPS
#include <openssl/err.h>
#endif

#if defined(__i386) || defined(__x86_64__) || defined(__amd64__) || defined(__ia64__)

#define CAS_MUTEX 1

#endif

#if defined(CAS_MUTEX)

#define dps_mutex_t             DPS_AGENT*
#define InitMutex(x)            (*(x) = NULL)
#define DestroyMutex(x)
#define DPS_MUTEX_LOCK(A,x)       DpsCAS_lock(A,x)
#define DPS_MUTEX_UNLOCK(A,x)     DpsCAS_unlock(A,x)
#if defined HAVE_PTHREAD
#define DPS_THREAD_ID           (pthread_self())
#else
#define DPS_THREAD_ID           ((unsigned long)1117)
#endif

#elif defined HAVE_PTHREAD
#include <pthread.h>
#define dps_mutex_t		pthread_mutex_t
#define InitMutex(x)		pthread_mutex_init(x,NULL)
#define DestroyMutex(x)		pthread_mutex_destroy(x)
#define DPS_MUTEX_LOCK(A,x)	pthread_mutex_lock(x)
#define DPS_MUTEX_UNLOCK(A,x)	pthread_mutex_unlock(x)
#define DPS_THREAD_ID           pthread_self()

#elif defined HAVE_SYS_SEM_H
#include <sys/ipc.h>
#include <sys/sem.h>
#define dps_mutex_t             int
void InitMutex(dps_mutex_t *);
void DPS_MUTEX_LOCK(DPS_AGENT *A, dps_mutex_t *x);
void DPS_MUTEX_UNLOCK(DPS_AGENT *A, dps_mutex_t *x);
#define DPS_THREAD_ID           ((unsigned long)1117)


#elif defined HAVE_SEMAPHORE_H
#include <semaphore.h>
#define dps_mutex_t             sem_t*
void InitMutex(dps_mutex_t *);
#define DestroyMutex(x)         sem_close(x)
#define DPS_MUTEX_LOCK(A,x)       sem_wait(x)
#define DPS_MUTEX_UNLOCK(A,x)     sem_post(x)
#define DPS_THREAD_ID           ((unsigned long)1117)


#else
#define dps_mutex_t		int
#define InitMutex(x)		*(x)=0
#define DestroyMutex(x)
#define DPS_MUTEX_LOCK(A,x)
#define DPS_MUTEX_UNLOCK(A,x)
#define DPS_THREAD_ID           ((unsigned long)1117)
#endif


typedef struct {
  int handle;
  int cnt;
  dps_mutex_t mutex;
} DPS_MUTEX;


#if 1
#define DPS_HALF ((DpsNsems - DPS_LOCK_MAX) >> 1)
#define DPS_LOCK_CACHED_N(n)  ((DpsNsems == DPS_LOCK_MAX) ? DPS_LOCK_CACHED : (DPS_LOCK_MAX + (n % DPS_HALF)))
#define DPS_LOCK_BASE_N(n)    ((DpsNsems == DPS_LOCK_MAX) ? DPS_LOCK_BASE   : (DPS_LOCK_MAX + DPS_HALF + (n % DPS_HALF)))
#else
#define DPS_LOCK_CACHED_N(n)  ((DpsNsems == DPS_LOCK_MAX) ? DPS_LOCK_CACHED : (DPS_LOCK_MAX + (n % (DpsNsems - DPS_LOCK_MAX))))
#endif


/* MUTEX stuff to lock dangerous places in multi-threaded mode */

extern __C_LINK int __DPSCALL DpsSetLockProc(DPS_ENV * Conf,
		void (*proc)(DPS_AGENT *A, int command, size_t type, const char *fname, int lineno));
extern __C_LINK void __DPSCALL DpsLockProc(DPS_AGENT *A, int command, size_t type, const char *fn, int ln);

extern __C_LINK void __DPSCALL DpsInitMutexes(void);
extern __C_LINK void __DPSCALL DpsDestroyMutexes(void);
extern void DpsGetSemLimit(void);


extern size_t DpsNsems;

#define DPS_GETLOCK(A,mutex)		if(A->Conf->LockProc)A->Conf->LockProc(A,DPS_LOCK,mutex,__FILE__,__LINE__)
#define DPS_RELEASELOCK(A,mutex)	if(A->Conf->LockProc)A->Conf->LockProc(A,DPS_UNLOCK,mutex,__FILE__,__LINE__)

/* Locking commands */
#define DPS_LOCK		1
#define DPS_UNLOCK		2

/* Accept global locking */
void DpsAcceptMutexChildCleanup(void);
void DpsAcceptMutexCleanup(void);
void DpsAcceptMutexInit(const char *var_dir, const char *app); 
void DpsAcceptMutexChildInit(void);
void DpsAcceptMutexLock(DPS_AGENT *Agent);
void DpsAcceptMutexUnlock(DPS_AGENT *Agent);

#endif
