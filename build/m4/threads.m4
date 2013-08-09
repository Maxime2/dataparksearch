dnl Copyright (c) 2005,2011 DataPark Ltd. All reserved.
dnl
dnl  Copyright (c) 1999, 2000 Sascha Schumann. All rights reserved.
dnl 
dnl  Redistribution and use in source and binary forms, with or without
dnl  modification, are permitted provided that the following conditions
dnl  are met:
dnl 
dnl  1. Redistributions of source code must retain the above copyright
dnl     notice, this list of conditions and the following disclaimer.
dnl 
dnl  2. Redistributions in binary form must reproduce the above copyright
dnl     notice, this list of conditions and the following disclaimer in
dnl     the documentation and/or other materials provided with the
dnl     distribution.
dnl 
dnl  THIS SOFTWARE IS PROVIDED BY SASCHA SCHUMANN ``AS IS'' AND ANY
dnl  EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
dnl  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
dnl  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL SASCHA SCHUMANN OR
dnl  HIS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
dnl  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
dnl  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
dnl  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
dnl  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
dnl  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
dnl  OF THE POSSIBILITY OF SUCH DAMAGE.

dnl
dnl PTHREADS_CHECK_COMPILE
dnl
dnl Check whether the current setup can use POSIX threads calls
dnl
AC_DEFUN([PTHREADS_CHECK_COMPILE], [
dnl it was AC_TRY_RUN below
AC_TRY_COMPILE( [
#include <pthread.h>
#include <stddef.h>],[

    pthread_t thd;
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    return pthread_create(&thd, NULL, NULL, NULL);
  ], [ 
  pthreads_working="yes"
  ], [
  pthreads_working="no"
  ], pthreads_working="no" ) ] )dnl
dnl
dnl PTHREADS_CHECK_LINK
AC_DEFUN([PTHREADS_CHECK_LINK], [
AC_TRY_LINK( [
#include <pthread.h>
] ,
[
        pthread_t th;
	pthread_attr_t type;
	pthread_join(th,0);
	pthread_attr_init(&type);
	pthread_cleanup_push(0,0);
	pthread_create(0,0,0,0);
	pthread_cleanup_pop(0);
], pthreads_working="yes", 
   pthreads_working="no"
) ] )dnl
dnl
dnl PTHREADS_CHECK()
dnl
dnl Try to find a way to enable POSIX threads
dnl
dnl  Magic flags
dnl  -kthread          gcc (FreeBSD)
dnl  -Kthread          UDK cc (UnixWare)
dnl  -mt               WorkShop cc (Solaris)
dnl  -mthreads         gcc (AIX)
dnl  -pthread          gcc (Linux, FreeBSD, NetBSD, OpenBSD)
dnl  -pthreads         gcc (Solaris) 
dnl  -qthreaded        AIX cc V5
dnl  -threads          gcc (HP-UX)
dnl
AC_DEFUN([PTHREADS_CHECK],[

save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
PTHREADS_ASSIGN_VARS
PTHREADS_CHECK_COMPILE
LIBS="$save_LIBS"
CFLAGS="$save_CFLAGS"

AC_CACHE_CHECK(for pthreads_cflags,ac_cv_pthreads_cflags,[
ac_cv_pthreads_cflags=""
if test "$pthreads_working" != "yes"; then
  for flag in "-kthread -pthread -pthreads -mthreads -Kthread -threads -mt -qthreaded"; do 
    ac_save="$CFLAGS"
    CFLAGS="$CFLAGS $flag"
    PTHREADS_CHECK_COMPILE
    if test "$pthreads_working" = "yes"; then
      ac_cv_pthreads_cflags="$flag"
      PTHREAD_LFLAGS="$PTHREAD_LFLAGS $flag"
      AC_MSG_RESULT([$flag])
      break
    else
      CFLAGS="$ac_save"
    fi
  done
fi
])

lib_list=""
case "$host" in
       *-*-freebsd4*)
	    lib_list="pthread pthreads c_r"
            ;;
       *-*-freebsd5*)
	    lib_list="pthread pthreads c_r"
            ;;
       *)
	    lib_list="pthread pthreads c_r"
            ;;
esac

AC_CACHE_CHECK(for pthreads_lib, ac_cv_pthreads_lib,[
ac_cv_pthreads_lib=""
PTHREADS_CHECK_LINK
if test "$pthreads_working" != "yes"; then
  for lib in $lib_list; do
    ac_save="$LIBS"
    LIBS="$LIBS -l$lib"
    PTHREADS_CHECK_LINK
    LIBS="$ac_save"
    if test "$pthreads_working" = "yes"; then
      ac_cv_pthreads_lib="$lib"
      PTHREAD_LDADD="$PTHREAD_LDADD -l$lib"
      AC_MSG_RESULT([$lib])
      break
    fi
  done
fi
])

dnl if test "$pthreads_working" = "yes"; then
dnl  AC_MSG_RESULT([POSIX Threads found])
dnl else
dnl   AC_MSG_ERROR([POSIX Threads not found])
dnl fi

LIBS="$save_LIBS"
CFLAGS="$save_CFLAGS"

])dnl
dnl
dnl
AC_DEFUN([PTHREADS_ASSIGN_VARS],[
if test -n "$ac_cv_pthreads_lib"; then
  LIBS="$LIBS -l$ac_cv_pthreads_lib"
fi

LIBS="$LIBS $PTHREAD_LDADD"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
LFLAGS="$LFLAGS $PTHREAD_LFLAGS"

if test -n "$ac_cv_pthreads_cflags"; then
  CFLAGS="$CFLAGS $ac_cv_pthreads_cflags"
fi
])dnl
