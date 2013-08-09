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
*/

#include "dps_common.h"
#include "dps_mutex.h"
#include "dps_utils.h"
#include "dps_log.h"

/*
#define DEBUG_LOCK
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/param.h>
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

#ifdef HAVE_SYS_SEM_H
#include <sys/sem.h>
#ifdef __Linux__
#include <linux/sem.h>
#endif
/* The treatment of `union semun' depends architecture. */
/* See man page of `semctl'. */

#ifndef HAVE_UNION_SEMUN

#ifdef SUNOS5
    union semun {
      int val;
      struct semid_ds *buf;
      ushort *array;
    };
#elif defined(__linux__)
      /* according to X/OPEN we have to define it ourselves */
      union semun {
        int val;                    /* value for SETVAL */
        struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
        unsigned short int *array;  /* array for GETALL, SETALL */
        struct seminfo *__buf;      /* buffer for IPC_INFO */
      };
#else
      union semun {
        int                     val;
        struct semid_ds *buf;
        unsigned short *array;
      };
#endif

#endif /* HAVE_UNION_SEMUN */

#endif /* HAVE_SYS_SEM_H */

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif


#ifdef HAVE_SEMAPHORE_H
#include <semaphore.h>
#endif


#ifdef DEBUG_LOCK
#include <assert.h>
#endif


/* libCRYPTO threads locking */
static int dps_libcrypto_thread_setup(void);
static int dps_libcrypto_thread_cleanup(void);
/* /libCRYPTO threads locking */



static DPS_MUTEX *MuMu = NULL;

size_t DpsNsems = DPS_LOCK_MAX;

#if defined(CAS_MUTEX)

void DpsGetSemLimit(void) {
    DpsNsems = (size_t) 0x1000 + DPS_LOCK_MAX;
}

#elif !defined(HAVE_PTHREAD) && defined(HAVE_SYS_SEM_H)

void DpsGetSemLimit(void) {

#ifdef SEMMSL

  DpsNsems = (size_t) SEMMSL;

#elif defined(__OpenBSD__)
  {
        int mib[3], maxsem;
        size_t len;

        mib[0] = CTL_KERN;
        mib[1] = KERN_SEMINFO;
        mib[2] = KERN_SEMINFO_SEMMSL;

        len = sizeof(maxsem);
        if (sysctl(mib, 3, &maxsem, &len, NULL, 0) != -1) {
	  if (maxsem != -1) DpsNsems = (size_t)maxsem;
	  if (DpsNsems == 0) DpsNsems = 0x300 + DPS_LOCK_MAX;
	}
  }

#elif defined(__FreeBSD__)
  {
    int maxsem;
    size_t len;

    len = sizeof(maxsem);
    if (sysctlbyname("kern.ipc.semmsl", &maxsem, &len, NULL, 0) != -1) {
      if (maxsem != -1) DpsNsems = (size_t)maxsem;
      if (DpsNsems == 0) DpsNsems = 0x300 + DPS_LOCK_MAX;
    }
  }
#endif  

}


#else


void DpsGetSemLimit(void) {
  long Nsems = -1;

#ifdef _POSIX_SEM_NSEMS_MAX
  Nsems = _POSIX_SEM_NSEMS_MAX;
#endif

#ifdef SEM_NSEMS_MAX
  if (Nsems == -1) {
    Nsems = SEM_NSEMS_MAX;
  }
#endif

#if defined(HAVE_UNISTD_H) && defined(_SC_SEM_NSEMS_MAX)
  if (Nsems == -1) {
    Nsems = sysconf(_SC_SEM_NSEMS_MAX);
  }
#endif

#ifdef __OpenBSD__
  if (Nsems == -1) {
        int mib[3], maxsem;
        size_t len;

        mib[0] = CTL_KERN;
        mib[1] = KERN_SEMINFO;
        mib[2] = KERN_SEMINFO_SEMMNS;

        len = sizeof(maxsem);
        sysctl(mib, 3, &maxsem, &len, NULL, 0);
	if (maxsem != -1) Nsems = maxsem;
  }
#endif

  if (Nsems != -1) {
    if (Nsems == 0) Nsems = 0x300 + DPS_LOCK_MAX;
    DpsNsems = (size_t) Nsems;
  }
  
}


#endif


#if defined(CAS_MUTEX)

static inline char CAS(dps_mutex_t *target, dps_mutex_t exchange, dps_mutex_t compare) {
  char ret;
  __asm__ __volatile__ (
			"lock\n\t"
			"cmpxchg %3,%1\n\t"
			"sete %0\n\t"
			: "=q" (ret), "+m" (*target), "+a" (compare)
			: "r" (exchange)
			: "cc", "memory");
  return ret;
}

void DpsInitMutexes(void) {
	size_t i;
  
	DpsGetSemLimit();

	MuMu = (DPS_MUTEX*)DpsMalloc((DpsNsems + 1) * sizeof(DPS_MUTEX));
	if (MuMu == NULL) {
	  fprintf(stderr, "DataparkSearch: Can't alloc for %zu mutexes\n", DpsNsems);
	  exit(1);
	}

	for(i = 0; i < DpsNsems; i++) {
		InitMutex(&MuMu[i].mutex);
		MuMu[i].cnt = 0;
	}
	
	dps_libcrypto_thread_setup();
}

void DpsDestroyMutexes(void) {
	int i;
	dps_libcrypto_thread_cleanup();
	if (MuMu) {
	  for(i=0;i<DPS_LOCK_MAX;i++){
		DestroyMutex(&MuMu[i].mutex);
	  }
	  DPS_FREE(MuMu);
	}
}


static void DpsCAS_lock(DPS_AGENT *A, dps_mutex_t *mut) {
  while(!CAS(mut, A, NULL)) {
#if defined(HAVE_PTHREAD) && defined(HAVE_PTHREAD_YIELD_PROTO)
    pthread_yield();
    if (CAS(mut, A, NULL)) break;
#endif
    DPS_MSLEEP(50 + random() % 200);
  }
}

static void DpsCAS_unlock(DPS_AGENT *A, dps_mutex_t *mut) {
  while(!CAS(mut, NULL, A));
}


#elif !defined(HAVE_PTHREAD) && defined(HAVE_SYS_SEM_H)

int semid;

void DPS_MUTEX_LOCK(DPS_AGENT *A, dps_mutex_t *x) {
  struct sembuf waitop;
  
  waitop.sem_num = (unsigned short)*x;
  waitop.sem_op = -1;
  waitop.sem_flg = SEM_UNDO;
  semop(semid, &waitop, 1);
}

void DPS_MUTEX_UNLOCK(DPS_AGENT *A, dps_mutex_t *x) {
  struct sembuf postop;
  
  postop.sem_num = (unsigned short)*x;
  postop.sem_op = 1;
  postop.sem_flg = SEM_UNDO;
  semop(semid, &postop, 1);
}

__C_LINK void __DPSCALL DpsInitMutexes(void) {
  char sem_name[PATH_MAX];
  int oflag, fd;
  union semun arg;
  size_t i;

  DpsGetSemLimit();

  MuMu = (DPS_MUTEX*)DpsMalloc((DpsNsems + 1) * sizeof(DPS_MUTEX));
  if (MuMu == NULL) {
    fprintf(stderr, "DataparkSearch: Can't alloc for %d mutexes\n", DpsNsems);
    exit(1);
  }

  dps_snprintf(sem_name, PATH_MAX,"%s%sSEM-V", DPS_VAR_DIR, DPSSLASHSTR);
  if ((fd = open(sem_name, O_RDWR | O_CREAT, (mode_t)0644)) < 0) {
    dps_strerror(NULL, 0, "%s open failed", sem_name);
    exit(1);
  }
  close(fd);
  
  oflag = IPC_CREAT | IPC_EXCL | 
#if defined(SEM_R) && defined(SEM_A)
    (SEM_R | SEM_A | SEM_R>>3 | SEM_R>>6)
#else
    0644
#endif
;
  if ((semid = semget(ftok(sem_name, 0), 1, oflag)) >= 0) {
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);
  } else if (errno == EEXIST) {
    semid = semget(ftok(sem_name, 0), 1, 
#if defined(SEM_R) && defined(SEM_A)
		   (SEM_R | SEM_A | SEM_R>>3 | SEM_R>>6)
#else
		   0644
#endif
		   );
  } else {
    dps_strerror(NULL, 0, "can't create semaphore %s", sem_name);
/*    unlink(sem_name);*/
    exit(1);
  }

  for(i = 0; i < DpsNsems; i++) {
    MuMu[i].mutex = (dps_mutex_t) i;
    MuMu[i].cnt = 0;
  }
  
  dps_libcrypto_thread_setup();
/*	atexit( DpsDestroyMutexes );*/

}

__C_LINK  void __DPSCALL DpsDestroyMutexes(void) {
  char sem_name[PATH_MAX];
  dps_snprintf(sem_name, PATH_MAX, "%s%sSEM-V", DPS_VAR_DIR, DPSSLASHSTR);
/*  unlink(sem_name);*/
  semctl(semid, 0, IPC_RMID);
  DPS_FREE(MuMu);
  dps_libcrypto_thread_cleanup();
}


#elif !defined(HAVE_PTHREAD) && defined(HAVE_SEMAPHORE_H)

__C_LINK void __DPSCALL DpsInitMutexes(void) {
  char sem_name[PATH_MAX];
  size_t i;

  DpsGetSemLimit();

  MuMu = (DPS_MUTEX*)DpsMalloc((DpsNsems + 1) * sizeof(DPS_MUTEX));
  if (MuMu == NULL) {
    fprintf(stderr, "DataparkSearch: Can't alloc for %d mutexes\n", DpsNsems);
    exit(1);
  }
  for(i = 0; i < DpsNsems; i++) {
    dps_snprintf(sem_name, PATH_MAX,"%s%sSEM-P.%d", DPS_VAR_DIR, DPSSLASHSTR, i);
    MuMu[i].mutex = sem_open(sem_name, O_CREAT, (mode_t)0644, 1);
    MuMu[i].cnt = 0;
  }
  dps_libcrypto_thread_setup();
/*	atexit( DpsDestroyMutexes );*/
}

__C_LINK  void __DPSCALL DpsDestroyMutexes(void) {
  char sem_name[PATH_MAX];
  size_t i;

  for(i = 0; i < DPS_LOCK_MAX; i++) {
    dps_snprintf(sem_name, PATH_MAX,"%s%sSEM-P.%d", DPS_VAR_DIR, DPSSLASHSTR, i);
    sem_close(MuMu[i].mutex);
    /*	  sem_unlink(sem_name);*/
  }
  DPS_FREE(MuMu);
  dps_librypto_thread_cleanup();
}

#else



__C_LINK void __DPSCALL DpsInitMutexes(void) {
	size_t i;
  
	DpsGetSemLimit();

	MuMu = (DPS_MUTEX*)DpsMalloc((DpsNsems + 1) * sizeof(DPS_MUTEX));
	if (MuMu == NULL) {
	  fprintf(stderr, "DataparkSearch: Can't alloc for %d mutexes\n", DpsNsems);
	  exit(1);
	}

	for(i = 0; i < DpsNsems; i++) {
		InitMutex(&MuMu[i].mutex);
		MuMu[i].cnt = 0;
	}

	dps_libcrypto_thread_setup();
}

__C_LINK  void __DPSCALL DpsDestroyMutexes(void) {
	int i;
	if (MuMu) {
	  for(i=0;i<DPS_LOCK_MAX;i++){
		DestroyMutex(&MuMu[i].mutex);
	  }
	  DPS_FREE(MuMu);
	}
	dps_libcrypto_thread_cleanup();
}

#endif



/* CALL-BACK Locking function */
__C_LINK void __DPSCALL DpsLockProc(DPS_AGENT *A, int command, size_t type, const char *fn, int ln) {

  int u;

#ifdef DEBUG_LOCK
  int handle = A ? A->handle : -1;
  unsigned long ticks = DpsStartTimer();

  if (type != DPS_LOCK_THREAD)
	fprintf(
#ifdef WITH_TRACE
		A->TR
#else
		stderr
#endif
		, "%lu [%d]{%02d} %2d Try %s\t%s\t%d\n", ticks, (int)getpid(), handle, type, (command==DPS_LOCK) ? "lock\t" : "unlock\t", fn, ln);
#ifdef WITH_TRACE
  fflush(A->TR);
#endif
#endif
	switch(command){
		case DPS_LOCK:
		        if (A->Locked[type] == 0) {
			  DPS_MUTEX_LOCK(A, &MuMu[type].mutex);
			}
			A->Locked[type]++;
#ifdef DEBUG_LOCK
/*			assert(A->Locked[type] == 1);*/
#endif
			break;
		case DPS_UNLOCK:
			A->Locked[type]--;
			u = (A->Locked[type] == 0);
			if (u) DPS_MUTEX_UNLOCK(A, &MuMu[type].mutex); 
			break;
	}
#ifdef DEBUG_LOCK
  if (type != DPS_LOCK_THREAD)
	fprintf(
#ifdef WITH_TRACE
		A->TR
#else
		stderr
#endif
		, "%lu [%d]{%02d} %2d %s\t%s\t%d\n", 
		ticks, (int)getpid(), handle, type, (command==DPS_LOCK) ? "locked\t" : ((u) ? "unlocked\t" : "still locked"), fn, ln);
#ifdef WITH_TRACE
  fflush(A->TR);
#endif
#endif

}

__C_LINK int __DPSCALL DpsSetLockProc(DPS_ENV * Conf,
		__C_LINK void (*proc)(DPS_AGENT *A, int command, size_t type, const char *f, int l)) {
	
	Conf->LockProc = proc;
	return(0);
}

/*
   Accept global locking
*/ 

#if 1 /*defined(CAS_MUTEX)*/

static dps_mutex_t *accept_mutex = (dps_mutex_t *) -1;
static int have_accept_mutex;
static sigset_t accept_block_mask;
static sigset_t accept_previous_mask;

void DpsAcceptMutexCleanup(void) {
    if (accept_mutex != (dps_mutex_t *)-1
	&& munmap((caddr_t) accept_mutex, sizeof(*accept_mutex))) {
	perror("munmap");
    }
    accept_mutex = (dps_mutex_t *)-1;
}

static void dps_remove_sync_sigs(sigset_t *sig_mask) {
#ifdef SIGABRT
    sigdelset(sig_mask, SIGABRT);
#endif
#ifdef SIGBUS
    sigdelset(sig_mask, SIGBUS);
#endif
#ifdef SIGEMT
    sigdelset(sig_mask, SIGEMT);
#endif
#ifdef SIGFPE
    sigdelset(sig_mask, SIGFPE);
#endif
#ifdef SIGILL
    sigdelset(sig_mask, SIGILL);
#endif
#ifdef SIGIOT
    sigdelset(sig_mask, SIGIOT);
#endif
#ifdef SIGPIPE
    sigdelset(sig_mask, SIGPIPE);
#endif
#ifdef SIGSEGV
    sigdelset(sig_mask, SIGSEGV);
#endif
#ifdef SIGSYS
    sigdelset(sig_mask, SIGSYS);
#endif
#ifdef SIGTRAP
    sigdelset(sig_mask, SIGTRAP);
#endif

/* APR logic to remove SIGUSR2 not copied */
}

void DpsAcceptMutexInit(const char *var_dir, const char *app) {
    int fd;

    fd = open("/dev/zero", O_RDWR);
    if (fd == -1) {
	perror("open(/dev/zero)");
	exit(DPS_ERROR);
    }
    accept_mutex = (dps_mutex_t *) mmap((caddr_t) 0, sizeof(*accept_mutex),
				 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (accept_mutex == (dps_mutex_t *) -1) {
	perror("mmap /dev/zero");
	exit(DPS_ERROR);
    }
    close(fd);
    InitMutex(accept_mutex);
    sigfillset(&accept_block_mask);
    sigdelset(&accept_block_mask, SIGHUP);
    sigdelset(&accept_block_mask, SIGTERM);
    sigdelset(&accept_block_mask, SIGUSR1);
    dps_remove_sync_sigs(&accept_block_mask);
} 

void DpsAcceptMutexLock(DPS_AGENT *Agent) {
    if (sigprocmask(SIG_BLOCK, &accept_block_mask, &accept_previous_mask)) {
	perror("sigprocmask(SIG_BLOCK)");
	exit(DPS_ERROR);
    }
    DPS_MUTEX_LOCK(Agent, accept_mutex);
    have_accept_mutex = 1;
}

void DpsAcceptMutexUnlock(DPS_AGENT *Agent) {
    DPS_MUTEX_UNLOCK(Agent, accept_mutex);
    have_accept_mutex = 0;
    if (sigprocmask(SIG_SETMASK, &accept_previous_mask, NULL)) {
	perror("sigprocmask(SIG_SETMASK)");
	exit(DPS_ERROR);
    }
}


#else /* 1 defined(CAS_MUTEX)*/

static struct flock lock_it;
static struct flock unlock_it;

static int lock_fd = -1;

#define DpsAcceptMutexChildCleanup(x)
/*#define DpsAcceptMutexCleanup(x)*/
#define DpsAcceptMutexChildInit(x)

void DpsAcceptMutexInit(const char *var_dir, const char *app) {
  char lock_fname[PATH_MAX];

    lock_it.l_whence = SEEK_SET;	/* from current point */
    lock_it.l_start = 0;		/* -"- */
    lock_it.l_len = 0;			/* until end of file */
    lock_it.l_type = F_WRLCK;		/* set exclusive/write lock */
    lock_it.l_pid = 0;			/* pid not actually interesting */
    unlock_it.l_whence = SEEK_SET;	/* from current point */
    unlock_it.l_start = 0;		/* -"- */
    unlock_it.l_len = 0;		/* until end of file */
    unlock_it.l_type = F_UNLCK;		/* set exclusive/write lock */
    unlock_it.l_pid = 0;		/* pid not actually interesting */
#if 1
    dps_snprintf(lock_fname, sizeof(lock_fname), "%s/%s.lock", var_dir, app);
    lock_fd = open(lock_fname, O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (lock_fd == -1) {
	fprintf(stderr, "Cannot open lock file: %s\n", lock_fname);
	exit(DPS_ERROR);
    }
    unlink(lock_fname);
#endif
}

void DpsAcceptMutexCleanup(void) {
  close(lock_fd);
  lock_fd = -1;
}

void DpsAcceptMutexLock(DPS_AGENT *Agent) {
    int ret;

    while ((ret = fcntl(lock_fd, F_SETLKW, &lock_it)) < 0 && errno == EINTR) {
	/* nop */
    }

    if (ret < 0) {
      DpsLog(Agent, DPS_LOG_ERROR,
		    "fcntl: F_SETLKW: Error getting accept lock, exiting!  "
		    "Perhaps you need to use the LockFile directive to place "
		    "your lock file on a local disk!");
	exit(DPS_ERROR);
    }
}

void DpsAcceptMutexUnlock(DPS_AGENT *Agent) {
    int ret;

    while ((ret = fcntl(lock_fd, F_SETLKW, &unlock_it)) < 0 && errno == EINTR) {
	/* nop */
    }
    if (ret < 0) {
      DpsLog(Agent, DPS_LOG_ERROR,
		    "fcntl: F_SETLKW: Error freeing accept lock, exiting!  "
		    "Perhaps you need to use the LockFile directive to place "
		    "your lock file on a local disk!");
	exit(DPS_ERROR);
    }
}


#endif /* HAVE_PTHREAD */


/* libCRYPTO threads locking */

static dps_mutex_t *mutex_buf = NULL;

static void handle_error(const char *file, int lineno, const char *msg) {
  fprintf(stderr, "** %s:%d %s\n", file, lineno, msg);
#if defined WITH_HTTPS
  ERR_print_errors_fp(stderr);
#endif
}

static unsigned long id_function(void) {
  return ((unsigned long)DPS_THREAD_ID);
}

static void locking_function(int mode, int n, const char *file, int line) {
#if defined HAVE_PTHREAD && defined WITH_HTTPS
  if (mode & CRYPTO_LOCK)
    DPS_MUTEX_LOCK((dps_mutex_t)id_function(), &mutex_buf[n]);
  else
    DPS_MUTEX_UNLOCK((dps_mutex_t)id_function(), &mutex_buf[n]);
#endif
}

int dps_libcrypto_thread_setup(void) {
#if defined WITH_HTTPS
  int i;
 
  mutex_buf = (dps_mutex_t *)malloc(CRYPTO_num_locks(  ) * sizeof(dps_mutex_t));
  if (!mutex_buf)
    return 0;
  for (i = 0;  i < CRYPTO_num_locks(  );  i++)
    InitMutex(&mutex_buf[i]);
  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_locking_callback(locking_function);
#endif
  return 1;
}
 
int dps_libcrypto_thread_cleanup(void) {
#if defined WITH_HTTPS
  int i;
 
  if (!mutex_buf)
    return 0;
  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  for (i = 0;  i < CRYPTO_num_locks(  );  i++)
    DestroyMutex(&mutex_buf[i]);
  free(mutex_buf);
  mutex_buf = NULL;
#endif
  return 1;
}

/* /libCRYPTO threads locking */
