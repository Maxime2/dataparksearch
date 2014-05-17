dnl Process this file with autoconf to produce a configure script.
dnl
AC_INIT([DataparkSearch Engine], [4.54-2014-01-09], [dataparksearch@googlegroups.com], [dpsearch])
dnl AC_INIT([DataparkSearch Engine], [4.53], [dataparksearch@googlegroups.com], [dpsearch])
AC_CONFIG_SRCDIR([TODO])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(include/dps_config.h)
AC_PROG_MAKE_SET
AC_CONFIG_MACRO_DIR([build/m4])

# Set all version vars based on $VERSION. How do we do this more elegant ?
# Remember that regexps needs to quote [ and ] since this is run through m4
DPS_TRIM_VERSION=`echo $VERSION | sed -e "s|\.*pre[[0-9]]*$||"`
DPS_BASE_VERSION=`echo $DPS_TRIM_VERSION | sed -e "s|\.[[^.]]*$||" | sed -e "s|^\(.\)\.\(.\)$|\\1\.0\\2|" | sed -e "s|\.||g"`
dnl DPS_TAIL_VERSION=`echo $DPS_TRIM_VERSION | sed -e "s|^[[0-9]]\.[[0-9]]*\.||" | sed -e "s|^\(.\)$|0\\1|"`
DPS_TAIL_VERSION=`echo $DPS_TRIM_VERSION | sed -e "s|^[[0-9]]\.[[0-9]]*\.||" | sed -e "s|^\(.\)$|0\\1|" | sed -e "s|-.*||" | sed -e "s|^.*\.||"`
DPS_VERSION_ID=${DPS_BASE_VERSION}${DPS_TAIL_VERSION}

AC_DEFINE_UNQUOTED(DPS_VERSION_ID,$DPS_VERSION_ID)
AC_DEFINE_UNQUOTED(DPS_BASE_VERSION,$DPS_BASE_VERSION)
AC_DEFINE_UNQUOTED(DPS_TAIL_VERSION,$DPS_TAIL_VERSION)
AC_SUBST(DPS_BASE_VERSION)
AC_SUBST(DPS_TAIL_VERSION)
AC_SUBST(DPS_VERSION_ID)
AC_SUBST(PORTNAME)

dnl Installation prefix by default
AC_PREFIX_DEFAULT(/usr/local/dpsearch)

dnl Check if build env is sane
AM_SANITY_CHECK

dnl Enable shared libs by default (to disable - place AM_DISABLE_SHARED)
AC_PROG_LIBTOOL
dnl AM_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl Check for programs.
AC_PROG_CC
dnl AX_CHECK_CFLAGS([-std=c99 -pedantic])
AC_PROG_INSTALL

AM_PROG_CC_C_O

LT_INIT

dnl Check if we are little or big endian
AC_C_BIGENDIAN

dnl large file support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

AM_CPPFLAGS=""
dnl OS specific flags and options
case "$host" in
        *-*-irix*)
            CFLAGS="$CFLAGS -D_BSD_COMPAT"
dnl            LDADD="$LDADD -lbsd"
            LDFLAGS="$LDFLAGS -lbsd"
            ;;
	*-*-freebsd*)
	    LIBS="$LIBS -L/usr/local/lib -lc"
	    CFLAGS="$CFLAGS -I/usr/local/include"
	    AM_CPPFLAGS="$AM_CPPFLAGS -I/usr/local/include"
	    ;;
        *-*-solaris*)
            LDFLAGS="$LDFLAGS -lsocket -lresolv -lnsl"
            ;;
        *-*-linux*)
dnl            CFLAGS="$CFLAGS -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
	    LIBS="$LIBS -L/usr/local/lib -lrt -lc -lresolv"
            ;;
esac
dnl platform specific flags and options
case "$host" in
        sparc-*-*)
            CFLAGS="$CFLAGS -xmemalign=1i"
            ;;
esac

dnl Checks for header files.
AC_STDC_HEADERS
AC_CHECK_HEADERS(sys/types.h fcntl.h unistd.h sys/cdefs.h libutil.h bsd/libutil.h \
sys/ioctl.h sys/time.h sys/times.h sys/param.h sys/wait.h limits.h sched.h \
syslog.h select.h sys/select.h sys/socket.h arpa/inet.h sys/un.h netdb.h sys/sysctl.h sys/mman.h sys/shm.h \
netinet/in.h netinet/in_systm.h netinet/tcp.h arpa/nameser.h sys/ipc.h semaphore.h sys/sem.h sys/msg.h \
bsd/stdlib.h)

AC_CHECK_HEADERS(netinet/ip.h resolv.h,,,
[
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
]
)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_LONG_LONG_INT
PGAC_UNION_SEMUN
# Check if we have socklen_t
AC_CACHE_CHECK([for socklen_t], ac_cv_have_socklen_t,
AC_TRY_COMPILE(
[#include <sys/types.h>
#include <sys/socket.h>],
[socklen_t s;],
ac_cv_have_socklen_t=yes, ac_cv_have_socklen_t=no))

if test "$ac_cv_have_socklen_t" = "no"
then
  AC_DEFINE(socklen_t, int)
fi


dnl Check for tm_gmtoff in struct tm
AC_MSG_CHECKING(for tm_gmtoff in struct tm)
AC_TRY_COMPILE([#include <time.h>],
[struct tm tm;
tm.tm_gmtoff=0;
],
AC_DEFINE(HAVE_TM_GMTOFF)
AC_MSG_RESULT(yes),
AC_MSG_RESULT(no))


dnl Check if libbsd is required, e.g. on Linux
AC_CHECK_LIB(bsd, mergesort, CTOR_LIBS=`pkg-config libbsd-ctor --libs` LIBS="$LIBS -lbsd $CTOR_LIBS")


dnl Check for library functions
AC_CHECK_FUNCS(strdup strndup strstr strnlen strcasecmp strncasecmp bzero strtok_r strcasestr timegm)
AC_CHECK_FUNCS(regcomp socket vsnprintf snprintf mergesort heapsort setproctitle)
AC_CHECK_FUNCS(setenv putenv break)
AC_CHECK_FUNCS(unsetenv daemon)
AC_CHECK_FUNC(inet_addr, , AC_CHECK_LIB(xnet, inet_addr, LIBS="$LIBS -lxnet"))
dnl AC_CHECK_FUNC(setproctitle, , AC_CHECK_LIB(util, setproctitle, LIBS="$LIBS -lutil"))
AC_CHECK_FUNC(sqrt, , AC_CHECK_LIB(m, sqrt, LIBS="$LIBS -lm"))


dnl Check for libtre
tre=yes
AC_MSG_CHECKING(for libtre)
AC_ARG_WITH(tre,
[  --without-tre           disable support for libtre],
[tre=$withval],
[tre=yes])
  
AC_MSG_RESULT([$tre])

if test "$tre" != "no" ; then
	AC_CHECK_HEADERS(tre/regex.h)
	if test "$ac_cv_header_tre_regex_h" = "yes"; then
		LIBS="$LIBS -ltre"
	fi
fi

dnl Check for libbind or libdns
MODLIB=""
LIBLIB=""
AC_CHECK_LIB(cares, ares_init, AC_DEFINE(HAVE_LIBCARES) LIBS="$LIBS -lcares",
 AC_CHECK_LIB(ares, ares_init, AC_DEFINE(HAVE_LIBARES) LIBS="$LIBS -lares",
	AC_CHECK_LIB(bind, getaddrinfo, AC_DEFINE(HAVE_LIBBIND) LIBS="$LIBS -lbind",
		AC_CHECK_LIB(dns, getaddrinfo, AC_DEFINE(HAVE_LIBDNS) LIBS="$LIBS -ldns")
	)

 )
)

dnl Check for libextractor
libextractor=yes
AC_MSG_CHECKING(for libextractor)
AC_ARG_WITH(libextractor,
[  --without-libextractor  disable support for libextractor],
[libextractor=$withval],
[libextractor=yes])
AC_MSG_RESULT([$libextractor])
if test "$libextractor"  != "no" ; then
   AC_CHECK_LIB(extractor, EXTRACTOR_plugin_add_defaults, AC_DEFINE(HAVE_LIBEXTRACTOR) LIBS="$LIBS -lextractor",
   	AC_CHECK_LIB(extractor, EXTRACTOR_loadDefaultLibraries, AC_DEFINE(HAVE_LIBEXTRACTOR) LIBS="$LIBS -lextractor")
	)
fi


dnl AC_CHECK_LIB(bind, getaddrinfo, AC_DEFINE(HAVE_LIBBIND) [
dnl 	if test "$pthreads" != no; then
dnl 		LIBLIB="-lbind_r"
dnl 		MODLIB="-lbind"
dnl 	else
dnl 		LIBS="$LIBS -lbind"
dnl 	fi
dnl ],
dnl 	AC_CHECK_LIB(dns, getaddrinfo, AC_DEFINE(HAVE_LIBDNS) LIBS="$LIBS -ldns")
dnl )
AC_SUBST(LIBLIB)
AC_SUBST(MODLIB)

AC_CHECK_FUNCS(hstrerror getaddrinfo)
AC_CHECK_FUNC_PROTO(inet_net_pton, arpa/inet.h)
AC_CHECK_FUNC_PROTO([pthread_setconcurrency], [pthread.h])
AC_CHECK_FUNC_PROTO([pthread_yield], [pthread.h])
AC_CHECK_FUNC_PROTO([pthread_tryjoin_np], [pthread.h])
AC_CHECK_FUNC_PROTO([thr_setconcurrency], [thread.h])


dnl Check sizeof_type
AC_CHECK_SIZEOF(char*)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(void*)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(time_t)
dnl AC_CHECK_SIZEOF(off_t)



debug=no
AC_MSG_CHECKING(for debug info)
AC_ARG_WITH(debug,
    [  --with-debug            Build with debugging info],
    [debug=$withval],
    [debug=no])
if test "$debug" = "yes"
then
  CFLAGS="$CFLAGS -O0 -g"
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi


optimise=no
AC_MSG_CHECKING(for compilation optimisation)
AC_ARG_WITH(optimise,
    [  --with-optimise         Build with compilation optimisation],
    [optimise=$withval],
    [optimise=no])
if test "$optimise" = "yes"; then
  CFLAGS="$CFLAGS -O2"
  AC_MSG_RESULT([yes, -O2])
elif test "$optimise" != "no"; then
  CFLAGS="$CFLAGS -O$optimise"
  AC_MSG_RESULT([yes, -O$optimise])
else
  # No optimisation
  AC_MSG_RESULT(default)
fi



dnl Check for libc performance
AC_MSG_CHECKING(for libc functions performance, please be patient)
AC_RUN_IFELSE([AC_LANG_SOURCE([
#include "confdefs.h"
#define DPS_CONFIGURE
#include "src/dp.c"
])], AC_MSG_RESULT(done), AC_MSG_FAILURE(unable to complete tests), AC_MSG_RESULT(cross-compiling?)
)

dnl Check for libraries
dnl Next line was commented out by Kir 
dnl Does inet_addr and inet_aton belongs to one lib? 
dnl If so we don't need to check for inet_aton
dnl AC_CHECK_FUNCS(inet_addr inet_aton)
dnl sinclude(net.m4)
AC_LIBRARY_NET


dnl Determine where getopt function denined
AC_MSG_CHECKING(for getopt location)
AC_TRY_LINK([#include <unistd.h>],
[getopt(0, NULL, "h?");
],
	AC_MSG_RESULT(unistd.h),
AC_TRY_LINK([#include <getopt.h>],
[getopt(0, NULL, "h?");
],
	AC_DEFINE(HAVE_GETOPT_H)
	AC_MSG_RESULT(getopt.h),
	AC_MSG_RESULT(not found)
	)
	)

dnl Determine signal handling standard
AC_TYPE_SIGNAL
AC_MSG_CHECKING(for POSIX signals)
AC_TRY_LINK([#include <signal.h>],
[   sigset_t ss;
    struct sigaction sa;
    sigemptyset(&ss); sigsuspend(&ss);
    sigaction(SIGINT, &sa, (struct sigaction *) 0);
    sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
],
    AC_DEFINE(HAVE_POSIX_SIGNALS)
    AC_MSG_RESULT(yes),
    AC_MSG_RESULT(no))

	
dnl check for paranoia support
paranoia=no
AC_MSG_CHECKING(for paranoia support)
AC_ARG_ENABLE(paranoia,
[  --enable-paranoia       Enable paranoia support. Stack will be checked like with libparanoia 
                          This works only on Intel architecture using gcc. Optimisation should be switched off.],
[
  if test "$enableval" != "no"; then
    AC_MSG_RESULT($enableval)
    CFLAGS="$CFLAGS -O0"
    AC_DEFINE(WITH_PARANOIA)
  else
    AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])


dnl check needs for -lc
sharedmem=no
cok=no
if test "$sharedmem" = no; then
	AC_MSG_CHECKING(for POSIX shared memory library)
	AC_TRY_LINK([
#include <sys/types.h>
#include <sys/mman.h>
],
[
 shm_open("/tmp", 0, 0644);
], cok=yes 
   sharedmem=posix
   AC_DEFINE(HAVE_SHAREDMEM_POSIX)
   AC_MSG_RESULT(ok)
)
	if test "$cok" = no; then
		ac_save_libs="$LIBS"
		LIBS="$LIBS -lc" 
		AC_TRY_LINK([
#include <sys/types.h>
#include <sys/mman.h>
],
[
 shm_open("/tmp", 0, 0644);
], cok=yes 
   sharedmem=posix
   AC_DEFINE(HAVE_SHAREDMEM_POSIX)
   AC_MSG_RESULT(-lc), 
   LIBS="$ac_save_libs"
)
	fi
	if test "$cok" = no; then
		ac_save_libs="$LIBS"
		LIBS="$LIBS -lrt" 
		AC_TRY_LINK([
#include <sys/types.h>
#include <sys/mman.h>
],
[
 shm_open("/tmp", 0, 0644);
], cok=yes 
   sharedmem=posix
   AC_DEFINE(HAVE_SHAREDMEM_POSIX)
   AC_MSG_RESULT(-lrt), 
   LIBS="$ac_save_libs"
)
	fi
fi 

if test "$sharedmem" = no; then
	AC_MSG_CHECKING(for SystemV shared memory library)
	AC_TRY_LINK([
#include <sys/types.h>
#include <sys/shm.h>
],
[
 shmat(1, 0, 0);
],cok=yes
  sharedmem=sysV
  AC_DEFINE(HAVE_SHAREDMEM_SYSV)
  AC_MSG_RESULT(ok))
	if test "$cok" = no; then
		ac_save_libs="$LIBS"
		LIBS="$LIBS -lc" 
		AC_TRY_LINK([
#include <sys/types.h>
#include <sys/shm.h>
],
[
 shmat(1, 0, 0);
],cok=yes 
  sharedmem=sysV
  AC_DEFINE(HAVE_SHAREDMEM_SYSV)
  AC_MSG_RESULT(-lc), 
  LIBS="$ac_save_libs"
)
	fi
	if test "$cok" = no; then
		ac_save_libs="$LIBS"
		LIBS="$LIBS -lrt" 
		AC_TRY_LINK([
#include <sys/types.h>
#include <sys/shm.h>
],
[
 shmat(1, 0, 0);
],cok=yes 
  sharedmem=sysV
  AC_DEFINE(HAVE_SHAREDMEM_SYSV)
  AC_MSG_RESULT(-lrt), 
  LIBS="$ac_save_libs"
)
	fi
fi 

if test "$sharedmem" = no; then
	AC_MSG_ERROR([No shared memory support found])
fi


syslog=yes
AC_MSG_CHECKING(for syslog support)
AC_ARG_ENABLE(syslog,
[  --enable-syslog         Use syslog (default)
  --enable-syslog=LOG_FACILITY
                          Use syslog facility LOG_FACILITY, which should be
			  valid name defined in /usr/include/sys/syslog.h
  --disable-syslog        Use stdout/stderr], syslog=$enableval)

if test "$syslog" != no; then
    AC_DEFINE(WITH_SYSLOG)
    if test "$syslog" != yes; then
	AC_DEFINE_UNQUOTED(LOG_FACILITY, $syslog, syslog facility)
	AC_TRY_RUN([
	    #include <sys/syslog.h>
	    int main(void){
	    openlog("Test", LOG_NDELAY, LOG_FACILITY);
	    closelog();
	    return 0;
	}
	],
	AC_MSG_RESULT([enabled (will use $syslog facility)]),
	[
	AC_MSG_RESULT(error)
	echo "You probably gave wrong facility argument ($syslog)"
	echo "to --enable-syslog. This argument should be one defined in"
	echo "/usr/include/sys/syslog.h, for example LOG_DAEMON or LOG_LOCAL7."
	AC_MSG_ERROR([** Unknown syslog facility given **])
	],
	[echo $ac_n "cross compiling; assumed OK... $ac_c"]
	)
    else
	AC_MSG_RESULT(enabled)
    fi
else
    AC_MSG_RESULT([disabled (will use stdout/stderr)])
fi

trace=no
AC_MSG_CHECKING(for tracing support)
AC_ARG_ENABLE(trace,
[  --enable-trace          Use tracing (for debug purpose), disabled by default], trace=$enableval)

if test "$trace" != no; then
	AC_DEFINE(WITH_TRACE)
	AC_MSG_RESULT(enabled)
else
	AC_MSG_RESULT(disabled)
fi


googlegrp=no
AC_MSG_CHECKING(for Google-like groupping)
AC_ARG_ENABLE(googlegrp,
[  --enable-googlegrp      Use Google-like groupping by site (two result from each), disabled by default], googlegrp=$enableval)

if test "$googlegrp" != no; then
	AC_DEFINE(WITH_GOOGLEGRP)
	AC_MSG_RESULT(enabled)
else
	AC_MSG_RESULT(disabled)
fi

multidbaddr=no
AC_MSG_CHECKING(for multi DBAddr support)
AC_ARG_ENABLE(multidbaddr,
[  --enable-multidbaddr    Enable support for multi DBAddr, disabled by default], multidbaddr=$enableval)

if test "$multidbaddr" != no; then
	AC_DEFINE(WITH_MULTIDBADDR)
	AC_MSG_RESULT(enabled)
else
	AC_MSG_RESULT(disabled)
fi



dnl Relevance related
rel_distance_factor=25
rel_position_factor=1.5
rel_wrd_cnt_factor=3.5
rel_unicnt_factor=2000
pophops_factor=0.1


pophops=no
AC_MSG_CHECKING(for take hops for the Neo PopRank calculation)
AC_ARG_ENABLE(pophops,
[  --enable-pophops                      Take hops for the Neo Popularity Rank calculation], pophops=$enableval)
if test "$pophops" = yes; then
	AC_DEFINE(WITH_POPHOPS)
fi
AC_MSG_RESULT([$pophops])

AC_MSG_CHECKING(for hops factor in the Neo PopRank)
AC_ARG_WITH(pophopsfactor,
[  --with-pophopsfactor=NUM   Define NUM as factor of hops in the Neo Popularity Rank calculation. 
                          Default value is 0.1.],
[
	if test "x$withval" != "x"; then
		pophops_factor="$withval"
		AC_MSG_RESULT($withval)
  	else
    		AC_MSG_RESULT($pophops_factor)
  	fi
],[
  	AC_MSG_RESULT($pophops_factor)
])
AC_DEFINE_UNQUOTED(DPS_POPHOPS_FACTOR, $pophops_factor)


sort=heap
AC_MSG_CHECKING(for search results sorting method)
AC_ARG_ENABLE(sort,
[  --enable-sort=[heap|quick]        Select method for search results sorting, heap by default], rel=$enableval)

if test "$sort" = heap; then
	AC_DEFINE(HEAP_SORTING)
elif test "$sort" = quick; then
	AC_DEFINE(QUICK_SORTING)
else
	AC_MSG_ERROR([Results sorting method should be only one of heap or quick])
fi
	AC_MSG_RESULT([$sort])

rel=full
AC_MSG_CHECKING(for relevance method)
AC_ARG_ENABLE(rel,
[  --enable-rel=[full|fast|ultra]        Select method for relevance calculation, full by default], rel=$enableval)

if test "$rel" = full; then
	AC_DEFINE(FULL_RELEVANCE)
elif test "$rel" = fast; then
	AC_DEFINE(FAST_RELEVANCE)
elif test "$rel" = ultra; then
	AC_DEFINE(ULTRA_RELEVANCE)
else
	AC_MSG_ERROR([Relevance method should be only one of fast, full or ultra])
fi
	AC_MSG_RESULT([$rel])



reldistance=yes
AC_MSG_CHECKING(for average word distance in relevency)
AC_ARG_ENABLE(reldistance,
[  --disable-reldistance   Disable average word distance calculation in relevance
                          enabled by default], reldistance=$enableval)

if test "$reldistance" != no; then
	AC_DEFINE(WITH_REL_DISTANCE)
	AC_MSG_RESULT(enabled)
else
	AC_MSG_RESULT(disabled)
fi

relposition=yes
AC_MSG_CHECKING(for first word position in relevency)
AC_ARG_ENABLE(relposition,
[  --disable-relposition   Disable first word position calculation in relevance
                          enabled by default], relposition=$enableval)

if test "$relposition" != no; then
	AC_DEFINE(WITH_REL_POSITION)
	AC_MSG_RESULT(enabled)
else
	AC_MSG_RESULT(disabled)
fi

relwrdcount=yes
AC_MSG_CHECKING(for word count in relevency)
AC_ARG_ENABLE(relwrdcount,
[  --disable-relwrdcount   Disable word count calculation in relevance
                          enabled by default], relwrdcount=$enableval)

if test "$relwrdcount" != no; then
	AC_DEFINE(WITH_REL_WRDCOUNT)
	AC_MSG_RESULT(enabled)
else
	AC_MSG_RESULT(disabled)
fi


relposition=13
AC_MSG_CHECKING(for best first word position in relevance)
AC_ARG_WITH(bestpos,
[  --with-bestpos=NUM      Define NUM as best first word position for relevance calculation. 
                          Default value is 13.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		relposition="$withval"
  	fi
	AC_MSG_RESULT($relposition)
],[
  	AC_MSG_RESULT($relposition)
])
AC_DEFINE_UNQUOTED(DPS_BEST_POSITION, $relposition)


rel_avgpos=390
AC_MSG_CHECKING(for best average word position in relevance)
AC_ARG_WITH(avgpos,
[  --with-avgpos=NUM       Define NUM as best average word position for relevance calculation. 
                          Default value is 390.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_avgpos="$withval"
  	fi
	AC_MSG_RESULT($rel_avgpos)
],[
  	AC_MSG_RESULT($rel_avgpos)
])
AC_DEFINE_UNQUOTED(DPS_AVG_POSITION, $rel_avgpos)


rel_wrd_cnt=30
AC_MSG_CHECKING(for best words count in relevance)
AC_ARG_WITH(bestwrdcnt,
[  --with-bestwrdcnt=NUM   Define NUM as best words count for relevance calculation. 
                          Default value is 30.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_wrd_cnt="$withval"
	fi
    	AC_MSG_RESULT($rel_wrd_cnt)
],[
  	AC_MSG_RESULT($rel_wrd_cnt)
])
AC_DEFINE_UNQUOTED(DPS_BEST_WRD_CNT, $rel_wrd_cnt)


rel_best_dist=20
AC_MSG_CHECKING(for best average distance between words in relevance)
AC_ARG_WITH(avgdist,
[  --with-avgdist=NUM   Define NUM as best words count for relevance calculation. 
                          Default value is 20.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_best_dist="$withval"
	fi
    	AC_MSG_RESULT($rel_best_dist)
],[
  	AC_MSG_RESULT($rel_best_dist)
])
AC_DEFINE_UNQUOTED(DPS_AVG_DISTANCE, $rel_best_dist)


AC_MSG_CHECKING(for word distance factor in relevance)
AC_ARG_WITH(distfactor,
[  --with-distfactor=NUM   Define NUM as factor of word distance in relevance calculation. 
                          Default value is 25.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_distance_factor="$withval"
  	fi
	AC_MSG_RESULT($rel_distance_factor)
],[
  	AC_MSG_RESULT($rel_distance_factor)
])
AC_DEFINE_UNQUOTED(DPS_DISTANCE_FACTOR, $rel_distance_factor)

rel_less_distance_factor="1.0"
AC_MSG_CHECKING(for word less distance factor in relevance)
AC_ARG_WITH(lessdistfactor,
[  --with-lessdistfactor=NUM   Define NUM as factor of word distance in relevance calculation. 
                          Default value is 1.0.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_less_distance_factor="$withval"
  	fi
	AC_MSG_RESULT($rel_less_distance_factor)
],[
  	AC_MSG_RESULT($rel_less_distance_factor)
])
AC_DEFINE_UNQUOTED(DPS_LESS_DISTANCE_FACTOR, $rel_less_distance_factor)


AC_MSG_CHECKING(for first word position factor in relevance)
AC_ARG_WITH(posfactor,
[  --with-posfactor=NUM    Define NUM as factor of first word position in relevance calculation. 
                          Default value is 1.5.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_position_factor="$withval"
  	fi
    	AC_MSG_RESULT($rel_position_factor)
],[
  	AC_MSG_RESULT($rel_position_factor)
])
AC_DEFINE_UNQUOTED(DPS_POSITION_FACTOR, $rel_position_factor)


rel_less_position_factor="0.31"
AC_MSG_CHECKING(for first word less position factor in relevance)
AC_ARG_WITH(lessposfactor,
[  --with-lessposfactor=NUM    Define NUM as factor of first word position in relevance calculation. 
                          Default value is 0.31.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_less_position_factor="$withval"
  	fi
    	AC_MSG_RESULT($rel_less_position_factor)
],[
  	AC_MSG_RESULT($rel_less_position_factor)
])
AC_DEFINE_UNQUOTED(DPS_LESS_POSITION_FACTOR, $rel_less_position_factor)


AC_MSG_CHECKING(for word count factor in relevance)
AC_ARG_WITH(wrdcntfactor,
[  --with-wrdcntfactor=NUM Define NUM as factor of word count in relevance calculation. 
                          Default value is 3.5.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_wrd_cnt_factor="$withval"
  	fi
    	AC_MSG_RESULT($rel_wrd_cnt_factor)
],[
  	AC_MSG_RESULT($rel_wrd_cnt_factor)
])
AC_DEFINE_UNQUOTED(DPS_WRD_CNT_FACTOR, $rel_wrd_cnt_factor)


rel_less_wrd_cnt_factor="70.5"
AC_MSG_CHECKING(for word less count factor in relevance)
AC_ARG_WITH(lesswrdcntfactor,
[  --with-lesswrdcntfactor=NUM Define NUM as factor of word count in relevance calculation. 
                          Default value is 70.5.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_less_wrd_cnt_factor="$withval"
  	fi
    	AC_MSG_RESULT($rel_less_wrd_cnt_factor)
],[
  	AC_MSG_RESULT($rel_less_wrd_cnt_factor)
])
AC_DEFINE_UNQUOTED(DPS_LESS_WRD_CNT_FACTOR, $rel_less_wrd_cnt_factor)


rel_unicnt_border="10"
AC_MSG_CHECKING(for non-uniform word count border value)
AC_ARG_WITH(wrduniborder,
[  --with-wrduniborder=NUM Define NUM as border value for non-uniform word count distribution in relevance calculation. 
                          Default value is 10.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_unicnt_border="$withval"
  	fi
    	AC_MSG_RESULT($rel_unicnt_border)
],[
  	AC_MSG_RESULT($rel_unicnt_border)
])
AC_DEFINE_UNQUOTED(DPS_UNICNT_BORDER, $rel_unicnt_border)


rel_unicnt_factor="2000"
AC_MSG_CHECKING(for non-uniform word count factor in relevance)
AC_ARG_WITH(wrdunifactor,
[  --with-wrdunifactor=NUM Define NUM as factor of non-uniform word count distribution in relevance calculation. 
                          Default value is 2000.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_unicnt_factor="$withval"
  	fi
    	AC_MSG_RESULT($rel_unicnt_factor)
],[
  	AC_MSG_RESULT($rel_unicnt_factor)
])
AC_DEFINE_UNQUOTED(DPS_UNICNT_FACTOR, $rel_unicnt_factor)


rel_less_unicnt_factor="20"
AC_MSG_CHECKING(for non-uniform words less count factor in relevance)
AC_ARG_WITH(lesswrdunifactor,
[  --with-lesswrdunifactor=NUM Define NUM as factor of non-uniform words count distribution in relevance calculation. 
                          Default value is 20.],
[
	if test "x$withval" != "x" && test "x$withval" != "xyes"; then
		rel_less_unicnt_factor="$withval"
  	fi
    	AC_MSG_RESULT($rel_less_unicnt_factor)
],[
  	AC_MSG_RESULT($rel_less_unicnt_factor)
])
AC_DEFINE_UNQUOTED(DPS_LESS_UNICNT_FACTOR, $rel_less_unicnt_factor)


reltrack=no
AC_MSG_CHECKING(for relevance summands tracking)
AC_ARG_ENABLE(reltrack,
[  --enable-reltrack       Enable tracking of relevance summands], reltrack=$enableval)

if test "$reltrack" != no; then
	AC_DEFINE(WITH_REL_TRACK)
	
fi
AC_MSG_RESULT($reltrack)



dnl Hack to build statically linked binary distributions
STATIC_LFLAGS=""
static_lflags=no
AC_MSG_CHECKING(for -all-static linker flags)
AC_ARG_ENABLE(all-static,
[  --enable-all-static     build statically linked binaries],static_lflags=$enableval)
if test "$static_lflags" != no; then
	AC_MSG_RESULT(enabled)
	STATIC_LFLAGS="-all-static"
	LINKING="static"
else
	AC_MSG_RESULT([disabled])
	LINKING="dynamic"
fi
AC_SUBST(STATIC_LFLAGS)
AC_SUBST(LINKING)

PTHREAD_CFLAGS=""
PTHREAD_LFLAGS=""
PTHREAD_LDADD=""

dnl #sinclude(threads.m4)
pthreads=yes
AC_MSG_CHECKING(for pthreads)
AC_ARG_ENABLE(pthreads,
[  --disable-pthreads      disable POSIX pthreads],pthreads=$enableval)

if test "$pthreads" != no; then

    if test -z "$host"; then
    	AC_MSG_ERROR(host is not set. Make sure to run config.guess)
    fi
    case "$host" in
        *-*-bsdi*)
            PTHREAD_CFLAGS="-D_REENTRANT -D_THREAD_SAFE"
            ;;
        *-*-darwin*)
            PTHREAD_CFLAGS="-D_THREAD_SAFE"
# causes Carbon.p complaints?
#           PTHREAD_CFLAGS="-D_REENTRANT -D_THREAD_SAFE"
            ;;
        *-*-freebsd4*)
#            PTHREAD_CFLAGS="-D_REENTRANT -D_THREAD_SAFE -pthread"
            PTHREAD_CFLAGS="-D_REENTRANT -D_THREAD_SAFE"
#            PTHREAD_LFLAGS="-pthread"
#            PTHREAD_LDADD="-lpthread"
             ;;
        *-*-freebsd5*)
            PTHREAD_CFLAGS="-D_REENTRANT -D_THREAD_SAFE"
#            PTHREAD_LFLAGS="-pthread"
#            PTHREAD_LDADD="-lc_r"
             ;;
        *-*-netbsd*)
            PTHREAD_CFLAGS="-I/usr/pkg/include -D_REENTRANT"
	    AM_CPPFLAGS="$AM_CPPFLAGS -I/usr/pkg/include"
            PTHREAD_LDADD="-L/usr/pkg/lib -lpthread -lsem"
            ;;
        *-*-openbsd*)
            PTHREAD_CFLAGS="-D_REENTRANT"
            PTHREAD_LFLAGS="-pthread"
            PTHREAD_LDADD="-lpthread"
            ;;
        *-*-solaris*)
            PTHREAD_CFLAGS="-D_REENTRANT -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE -D_POSIX_PTHREAD_SEMANTICS"
            PTHREAD_LDADD="-lpthread -lposix4"
            ;;
        *-*-sysv5*)
            PTHREAD_CFLAGS="-D_REENTRANT -Kthread"
            PTHREAD_LDADD=""
            ;;
        *-*-irix*)
            PTHREAD_CFLAGS="-D_SGI_MP_SOURCE -D_POSIX_C_SOURCE=199506L -D_POSIX_THREAD_SAFE_FUNCTIONS"
            PTHREAD_LDADD="-lpthread"
            ;;
        *-*-aix*)
            PTHREAD_CFLAGS="-D_REENTRANT -mthreads"
            PTHREAD_LDADD="-lpthread"
            ;;
        *-*-hpux11*)
            PTHREAD_CFLAGS="-D_REENTRANT"
            PTHREAD_LDADD="-L/usr/lib -lpthread"
            ;;
        *-*-qnx*)
            PTHREAD_CFLAGS=""
            PTHREAD_LDADD=""
            ;;
        *)
#            PTHREAD_CFLAGS="-D_REENTRANT"
#            PTHREAD_LDADD="-lpthread"
            PTHREAD_CFLAGS="-pthread"
            PTHREAD_LDADD="-pthread"
            ;;
    esac

    PTHREADS_CHECK
    if test "$pthreads_working" = "yes"; then
	  AC_MSG_RESULT([             POSIX Threads found])
    else
	  AC_MSG_ERROR([POSIX Threads not found])
    fi

dnl    ac_save_libs="$LIBS"
dnl    ac_save_cflags="$CFLAGS"
dnl    LIBS="$LIBS $PTHREAD_LFLAGS $PTHREAD_LDADD"
dnl    CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
dnl    
dnl    AC_TRY_LINK([#include <pthread.h>],
dnl	[
dnl		pthread_attr_t type;
dnl		pthread_attr_init(&type);
dnl        ],AC_MSG_RESULT([yes]), 
dnl          AC_MSG_ERROR([POSIX thread support not found])
dnl    )
dnl
dnl    CFLAGS="$ac_save_cflags"
dnl    LIBS="$ac_save_libs"
    PTHREAD_CFLAGS="-DHAVE_PTHREAD $PTHREAD_CFLAGS"

else
dnl    PTHREAD_LFLAGS="-lc"
    LIBS="$LIBS -lc"
    AC_MSG_RESULT([disabled])
fi


AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_LFLAGS)
AC_SUBST(PTHREAD_LDADD)


if test "$pthreads" = yes ; then

_CFLAGS="$CFLAGS"
_LIBS="$LIBS"

CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
LIBS="$LIBS $PTHREAD_LFLAGS $PTHREAD_LDADD"

AC_CHECK_FUNCS(strerror_r gmtime_r)
AX_FUNC_WHICH_GETHOSTBYNAME_R


CFLAGS="$_CFLAGS"
LIBS="$_LIBS"

fi

dnl Check for system qsort_r
DPSAC_QSORT_R


parser=yes
AC_MSG_CHECKING(for external parsers support)
AC_ARG_ENABLE(parser,
[  --disable-parser        disable external parsers support], parser=$enableval)

if test "$parser" != no; then
    AC_DEFINE(WITH_PARSER)
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT([no])
fi


mp3=yes
AC_MSG_CHECKING(for MP3 ID3 tags support)
AC_ARG_ENABLE(mp3,
[  --disable-mp3           disable MP3 tags support ], mp3=$enableval)

if test "$mp3" != no; then
    AC_DEFINE(WITH_MP3)
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT([no])
fi


file=yes
AC_MSG_CHECKING(for file: URL schema support)
AC_ARG_ENABLE(file,
[  --disable-file          disable file:/ URL scheme support], file=$enableval)

if test "$file" != no; then
    AC_DEFINE(WITH_FILE)
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT([no])
fi


http=yes
AC_MSG_CHECKING([for http: (and ftp: - via proxy) URL schema support])
AC_ARG_ENABLE(http,
[  --disable-http          disable http:// URL scheme support], http=$enableval)

if test "$http" != no; then
    AC_DEFINE(WITH_HTTP)
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT([no])
fi


ftp=yes
AC_MSG_CHECKING(for ftp: URL schema support)
AC_ARG_ENABLE(ftp,
[  --disable-ftp           disable ftp:// URL scheme support], ftp=$enableval)

if test "$ftp" != no; then
    AC_DEFINE(WITH_FTP)
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT([no])
fi




news=yes
AC_MSG_CHECKING(for news: URL schema support)
AC_ARG_ENABLE(news,
[  --disable-news          disable news:// URL schema support], news=$enableval)

if test "$news" != no; then
    AC_DEFINE(WITH_NEWS)
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT([no])
fi


gprofiler=no
AC_MSG_CHECKING(for Google profiler support)
AC_ARG_ENABLE(gprofiler,
[  --enable-gprofiler        enable Google profiler support],gprofiler=$enableval)

if test "$gprofiler" == yes; then
	LIBS="-lprofiler $LIBS"
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT([no])
fi

tcmalloc=no
TCMALLOC_LFLAGS=""
AC_MSG_CHECKING(for libtcmalloc)
AC_ARG_WITH(tcmalloc,
[  --without-tcmalloc           disable support for libtcmalloc],
[tcmalloc=$withval],
[tcmalloc=no])

AC_MSG_RESULT([$tcmalloc])
if test "$tcmalloc" != "no" ; then
	if test "$tcmalloc" = "minimal"; then
		AC_CHECK_LIB(tcmalloc_minimal, malloc, TCMALLOC_LFLAGS="-ltcmalloc_minimal")
	else
		AC_CHECK_LIB(tcmalloc, malloc, TCMALLOC_LFLAGS="-ltcmalloc")
	fi
fi
AC_SUBST(TCMALLOC_LFLAGS)


dmalloc=no
AC_MSG_CHECKING(for DMALLOC memory debugger support)
AC_ARG_ENABLE(dmalloc,
[  --enable-dmalloc        enable DMALLOC memory debugger support],dmalloc=$enableval)

if test "$dmalloc" != no; then
	AC_DEFINE(DMALLOC)
	if test "$pthreads" != no; then
		DMALLOC_LFLAGS="-L/usr/local/lib -ldmallocth"
	else
		DMALLOC_LFLAGS="$LIBS -L/usr/local/lib -ldmalloc"
	fi
	DMALLOC_INCLUDE="-I/usr/local/include"
	AM_CPPFLAGS="$AM_CPPFLAGS -I/usr/local/include"
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT([no])
fi
AC_SUBST(DMALLOC_LFLAGS)
AC_SUBST(DMALLOC_INCLUDE)


boehmgc=no
AC_MSG_CHECKING(for Boehm's garbage collector support)
AC_ARG_ENABLE(boehmgc,
[  --enable-boehmgc        enable Boehm's garbage collector support],boehmgc=$enableval)

if test "$boehmgc" != no; then
	AC_DEFINE(BOEHMGC)
	if test "$boehmgc" != yes; then
		BOEHMGC_INCLUDE="-I$boehmgc/include"
	        AM_CPPFLAGS="$AM_CPPFLAGS -I$boehmgc/include"
		BOEHMGC_LFLAGS="-L$boehmgc/lib -lgc"
	else
		BOEHMGC_INCLUDE="-I/usr/local/include"
	        AM_CPPFLAGS="$AM_CPPFLAGS -I/usr/local/include"
		BOEHMGC_LFLAGS="-L/usr/local/lib -lgc"
	fi
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT([no])
fi
AC_SUBST(BOEHMGC_LFLAGS)
AC_SUBST(BOEHMGC_INCLUDE)



efence=no
AC_MSG_CHECKING(for Electricfence memory debugger support)
AC_ARG_ENABLE(efence,
[  --enable-efence         enable Electricfence memory debugger support],efence=$enableval)

if test "$efence" != no; then
	if test "$pthreads" != no; then
		LIBS="$LIBS -L/usr/local/lib -lefence_r"
	else
		LIBS="$LIBS -L/usr/local/lib -lefence"
	fi
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT([no])
fi

memdebug=no
AC_MSG_CHECKING(for internal memory debugger support)
AC_ARG_ENABLE(memdebug,
[  --enable-memdebug       enable internal memory debugger support],memdebug=$enableval)

if test "$memdebug" != no; then
        AC_DEFINE(EFENCE)
        AC_MSG_RESULT(yes)
else
        AC_MSG_RESULT([no])
fi

filedebug=no
AC_MSG_CHECKING(for internal file handlers debugger support)
AC_ARG_ENABLE(filedebug,
[  --enable-filedebug      enable internal file jandlers debugger support],filedebug=$enableval)

if test "$filedebug" != no; then
        AC_DEFINE(FILENCE)
        AC_MSG_RESULT(yes)
else
        AC_MSG_RESULT([no])
fi




ADDCHARSETS=""
chasen=no
AC_MSG_CHECKING(for ChaSen system support)
AC_ARG_ENABLE(chasen,
[  --enable-chasen         enable ChaSen japanese morphological analysis system support],chasen=$enableval)

if test "$chasen" != no; then
	LIBS_SAVE="$LIBS"
	CFLAGS_SAVE="$CFLAGS"
	LIBS="$LIBS -L/usr/local/lib -lchasen"
	CFLAGS="$CFLAGS -I/usr/local/include"
	AM_CPPFLAGS="$AM_CPPFLAGS -I/usr/local/include"

AC_TRY_LINK([#include <stdio.h>
#include <chasen.h>
],
[       char            *chasen_argv[] = { "chasen", "-F \"%m \"", NULL };
	chasen_getopt_argv(chasen_argv, NULL);
],
	AC_DEFINE(CHASEN)
	AC_DEFINE(HAVE_CHARSET_japanese)
	ADDCHARSETS="japanese"
	AC_MSG_RESULT(yes),

	LIBS="$LIBS -lstdc++"
AC_TRY_LINK([#include <stdio.h>
#include <chasen.h>
],
[       char            *chasen_argv[] = { "chasen", "-F \"%m \"", NULL };
	chasen_getopt_argv(chasen_argv, NULL);
],
	AC_DEFINE(CHASEN)
	AC_DEFINE(HAVE_CHARSET_japanese)
	ADDCHARSETS="japanese"
	AC_MSG_RESULT(yes),

	LIBS="$LIBS_SAVE"
	CFLAGS="$CFLAGS_SAVE"
	AC_MSG_RESULT(no))
	)

else
	AC_MSG_RESULT([no])
fi


mecab=no
AC_MSG_CHECKING(for MeCab system support)
AC_ARG_ENABLE(mecab,
[  --enable-mecab          enable MeCab japanese morphological analyzer support],mecab=$enableval)

if test "$mecab" != no; then
	LIBS_SAVE="$LIBS"
	CFLAGS_SAVE="$CFLAGS"
	LIBS="$LIBS -L/usr/local/lib -lmecab -lstdc++"
	CFLAGS="$CFLAGS -I/usr/local/include"

AC_TRY_LINK([#include <stdio.h>
#include <mecab.h>
],
[       char            *mecab_argv[] = { "mecab", "-F \"%m \" -E \" \" -B \" \" -Iline", NULL };
	mecab_t *m = mecab_new (2, mecab_argv);
],
	AC_DEFINE(MECAB)
	AC_DEFINE(HAVE_CHARSET_japanese)
	ADDCHARSETS="japanese"
	MECAB_LFLAGS="-L/usr/local/lib -lmecab -lstdc++"
	MECAB_INCLUDE="-I/usr/local/include"
	AM_CPPFLAGS="$AM_CPPFLAGS -I/usr/local/include"
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no))

	LIBS="$LIBS_SAVE"
	CFLAGS="$CFLAGS_SAVE"

else
	AC_MSG_RESULT([no])
fi
AC_SUBST(MECAB_LFLAGS)
AC_SUBST(MECAB_INCLUDE)


AC_MSG_CHECKING(for Openssl support)
AC_ARG_WITH(openssl,
[  --with-openssl[=DIR]    Include Openssl support.  DIR is the Openssl 
                          base install directory, defaults to /usr/local/ssl.],
[
  if test "$withval" != "no"; then
        if test "$withval" = "yes"; then
                if test -f /usr/include/openssl/ssl.h; then
                        SSL_INCDIR=/usr/include
		elif test -f /usr/sfw/include/openssl/ssl.h; then
			SSL_INCDIR=/usr/sfw/include
                else
                        SSL_INCDIR=/usr/local/ssl/include
                fi
                if test -f /usr/local/lib/libssl.a; then
                        SSL_LIBDIR=/usr/local/lib
                elif test -f /usr/lib/libssl.a; then
                        SSL_LIBDIR=/usr/lib
		elif test -f /usr/sfw/lib/libssl.a; then
			SSL_LIBDIR=/usr/sfw/lib
                else
                        SSL_LIBDIR=/usr/local/ssl/lib
                fi
        else
                SSL_INCDIR=$withval/include
                test -d $withval/include/openssl && SSL_INCDIR=$withval/include
                SSL_LIBDIR=$withval/lib
        fi
    SSL_INCLUDE="-I$SSL_INCDIR"
    SSL_LFLAGS="-L$SSL_LIBDIR -lssl -lcrypto"
	if test -f /usr/kerberos/include/krb5.h; then
		SSL_INCLUDE="$SSL_INCLUDE -I/usr/kerberos/include"
	fi
    AC_MSG_RESULT(yes)
    AC_DEFINE(WITH_HTTPS)
  else
    AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])

AC_SUBST(SSL_INCLUDE)
AC_SUBST(SSL_LFLAGS)


zlib=yes
AC_MSG_CHECKING(for zlib)
AC_ARG_WITH(zlib,
[  --without-zlib          disable HTTP Content-Encoding support
                            and cache mode indexes compression],
[zlib=$withval],
[zlib=yes])
  
AC_MSG_RESULT([$zlib])
  
if test "$zlib" = "no" ; then
  dnl to avoid bash error
  x=$x
elif test "$zlib" = "yes" ; then
  AC_CHECK_LIB(z, inflate, AC_DEFINE(HAVE_ZLIB) LIBS="$LIBS -lz",
	LIBS_save="$LIBS"
	LIBS="$LIBS -L/usr/local/lib"
	AC_CHECK_LIB(z,inflate,AC_DEFINE(HAVE_ZLIB) LIBS="$LIBS -lz",
		LIBS="$LIBS_save"
		AC_MSG_ERROR([Unable to find working zlib library])
		)
	)
else
  CFLAGS="$CFLAGS -I$zlib/include"
  LIBS="$LIBS -L$zlib/lib -lz"
  AC_CHECK_LIB(z,inflate,AC_DEFINE(HAVE_ZLIB),AC_MSG_ERROR([Unable to find working zlib library under $zlib]))
  AM_CPPFLAGS="$AM_CPPFLAGS -I$zlib/include"
fi

dnl aspell related stuff
dnl aspell=auto
LIBS_save="$LIBS"

AC_ARG_WITH(aspell,
[  --with-aspell           enable automatic words correction using aspell],
[aspell=$withval],
[aspell=auto])
  
dnl AC_MSG_RESULT([$aspell])

if test "$aspell" = "no" ; then
  dnl to avoid bash error
  x=$x
elif test "$aspell" = "yes"  -o "$aspell" = "auto"; then

    aspell=no
    MY_DIRS="/usr/local/include /usr/include /opt/local/include /opt/include"
    for DIR in $MY_DIRS; do
       	if test -f $DIR/aspell.h; then
		AM_CPPFLAGS="$AM_CPPFLAGS -I$DIR"
		aspell=yes
		break;
	fi
    done

    if test "$aspell" = "yes" -o "$aspell" = "auto"; then

	aspell=no
	MY_DIRS="/usr/lib /usr/local/lib /opt/local/lib /opt/lib"
    	for DIR in $MY_DIRS; do
		LIBS="$LIBS -L$DIR"
		case "$host" in
			*-*-freebsd*)
				LIBS="$LIBS -laspell -lstdc++ -lintl"
			;;
		*)
			LIBS="$LIBS -laspell -lstdc++"
			;;
		esac
		AC_CHECK_LIB(aspell, new_aspell_config, AC_DEFINE(HAVE_ASPELL) aspell=yes break;, LIBS="$LIBS_save")
		AS_UNSET([ac_cv_lib_aspell_new_aspell_config])
    	done

	AC_MSG_CHECKING(for aspell)
	if test "$aspell" != "yes" ; then
		AC_MSG_ERROR([Unable to find working aspell library or stdc++ library])
	fi
	AC_MSG_RESULT([$aspell])

    else
	AC_MSG_CHECKING(for aspell)
	AC_MSG_RESULT([no])
    fi


else
  CFLAGS="$CFLAGS -I$aspell/include"
  case "$host" in
	*-*-freebsd*)
  		LIBS="$LIBS -L$aspell/lib -laspell -lstdc++ -lintl"
		;;
	*)
  		LIBS="$LIBS -L$aspell/lib -laspell -lstdc++"
		;;
  esac
  AC_CHECK_LIB(aspell,new_aspell_config,AC_DEFINE(HAVE_ASPELL),AC_MSG_ERROR([Unable to find working aspell library under $aspell or stdc++ library]))
  AM_CPPFLAGS="$AM_CPPFLAGS -I$aspell/include"
fi


readline=no
AC_MSG_CHECKING(for readline support)
AC_ARG_WITH(readline,
[  --with-readline       enable readline support for SQL monitor],
[readline=$withval],
[readline=no]
)
  if test "$readline" != "no" ; then
    AC_DEFINE(HAVE_READLINE)
    LIBS="$LIBS -lreadline -ltermcap"
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi

#AC_DIVERT_PUSH(0)
ifdef([AC_DIVERSION_NOTICE], [AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)],
			      [AC_DIVERT_PUSH(NOTICE)])
define(DCHARSETS_AVAILABLE,chinese japanese euc-kr gujarati tscii)
CHARSETS_AVAILABLE="DCHARSETS_AVAILABLE"
#AC_DIVERT_POP
AC_DIVERT_POP()dnl

AC_ARG_WITH(extra-charsets,
  [  --with-extra-charsets=CHARSET[,CHARSET,...]
                          Use additional non-default charsets:
                          none, all or a comma separated list from this set:
                          DCHARSETS_AVAILABLE],
  [extra_charsets="$withval"],
  [extra_charsets="none"])

AC_MSG_CHECKING(for additional character sets)

if test "$extra_charsets" = none; then
  CHARSETS=""
else
  if test "$extra_charsets" = all; then
    CHARSETS="$CHARSETS_AVAILABLE"
  else
    CHARSETS=`echo $extra_charsets | sed -e 's/,/ /g'`
  fi
fi

for cs in $CHARSETS
do
  case $cs in 
    chinese)
      AC_DEFINE(HAVE_CHARSET_chinese)
      ;;
    euc-kr)
      AC_DEFINE(HAVE_CHARSET_euc_kr)
      ;;
    japanese)
      AC_DEFINE(HAVE_CHARSET_japanese)
      ;;
    gujarati)
      AC_DEFINE(HAVE_CHARSET_gujarati)
      ;;
    tscii)
      AC_DEFINE(HAVE_CHARSET_tscii)
      ;;
    *)
      AC_MSG_ERROR([Charset $cs not available. (Available: $CHARSETS_AVAILABLE).]);
  esac
  ADDCHARSETS="$ADDCHARSETS $cs"
done

AC_MSG_RESULT($ADDCHARSETS)



apache=no
AC_MSG_CHECKING(for Apache module support)
AC_ARG_ENABLE(apache-module,
[  --enable-apache-module[[=DIR]]
                          enable building of Apache module.  DIR is the Apache
                          base install directory, defaults to /usr/local/apache.],
[
	apache=$enableval
])

  if test "$apache" != "no"; then
        if test "$apache" = "yes"; then
	    PATH_DIRS=`echo $PATH | tr : \ `
	    MY_DIRS="/usr/sbin /usr/local/sbin /usr/local/apache/bin /home/httpd/bin /httpd/bin /usr/bin"
	    for DIR in $PATH_DIRS $MY_DIRS; do
        	if test -f $DIR/apxs2; then
            		APXS=$DIR/apxs2
			APACHE_VER=2
			break;
		fi
	    done
	    if test -z "$APXS"; then
	    	for DIR in $PATH_DIRS $MY_DIRS; do
        		if test -f $DIR/apxs; then
            			APXS=$DIR/apxs
				break;
			fi
	    	done
	    fi
	    for DIR in $PATH_DIRS $MY_DIRS; do
        	if test -f $DIR/apr-1-config; then
			APRCONFIG=$DIR/apr-1-config
			break;
		fi
	    done
	    if test -z "$APXS"; then
		AC_MSG_ERROR([apxs not found in $PATH_DIRS $MY_DIRS])
	    fi
        elif test "$apache" != ""; then
	    MY_DIRS="$apache/bin $apache/sbin $apache"
	    for DIR in $MY_DIRS; do
		if test -f $DIR/apxs2; then
		    APXS=$DIR/apxs2
		    break;
		fi
	    done
	    if test -z "$APXS"; then
	    	for DIR in $MY_DIRS; do
			if test -f $DIR/apxs; then
		    		APXS=$DIR/apxs
		    		break;
			fi
	    	done
	    fi
	    for DIR in $MY_DIRS; do
		if test -f $DIR/apr-1-config; then
			APRCONFIG=$DIR/apr-1-config
			break;
		fi
	    done
	    if test -z "$APXS"; then
		AC_MSG_ERROR([apxs not found in $MY_DIRS])
	    fi
	fi
	APACHE_INCDIR=`$APXS -q INCLUDEDIR`
	if test $? -ne 0; then
		AC_MSG_ERROR([Seems that apache module can't be compiled on your system, apxs is $APXS, includes at $APACHE_INCDIR])
	fi
	APACHE_MODDIR=`$APXS -q LIBEXECDIR`
	APACHE_CFLAGS=`$APXS -q CFLAGS`
	APACHE_CFLAGS_SHLIB=`$APXS -q CFLAGS_SHLIB`
	APACHE_LDFLAGS_SHLIB=`$APXS -q LDFLAGS_SHLIB`
	APACHE_LIBS_SHLIB=`$APXS -q LIBS_SHLIB`
	APACHE_MODULE="mod_dpsearch.so"
	USE_APACHE_MODULE=1

	AC_MSG_RESULT([yes, apxs is $APXS, apr-config is $APRCONFIG, includes at $APACHE_INCDIR])

	ac_save_cflags="$CFLAGS"
	CFLAGS="$CFLAGS -I$APACHE_INCDIR"
	if test "$APACHE_VER" = "2" -a -x "$APRCONFIG"; then
		APACHE_CFLAGS="$APACHE_CFLAGS `$APRCONFIG --includes`"
	fi

AC_MSG_CHECKING([for Apache version])
	if test -z "$APACHE_VER"; then
AC_TRY_COMPILE([#include <httpd.h> 
#include <mpm.h> 
#include <apr.h>], [
	apr_byte_t b;
], [
	APACHE_VER=2
], [
	APACHE_VER=1
])
	fi

dnl	ac_save_cflags="$CFLAGS"
	if test "$APACHE_VER" = "1" -a -x "$APRCONFIG"; then
        	CFLAGS="$CFLAGS `$APRCONFIG --includes`"
	        AC_TRY_COMPILE([#include <httpd.h> 
#include <mpm.h> 
#include <apr.h>], [
        	        apr_byte_t b;
	        ], [
        	        APACHE_VER=2
			APACHE_CFLAGS="$APACHE_CFLAGS `$APRCONFIG --includes`"
dnl	        	CFLAGS="$CFLAGS `$APRCONFIG --includes`"
	        ], [
        	        APACHE_VER=1
dnl			CFLAGS="$ac_save_cflags"

	        ])
	fi

	if test "$APACHE_VER" = "2"; then
		APACHE_NOTEST_CPPFLAGS=`$APXS -q NOTEST_CPPFLAGS`
		APACHE_EXTRA_CPPFLAGS=`$APXS -q EXTRA_CPPFLAGS`
		APACHE_EXTRA_CLAGS=`$APXS -q EXTRA_CFLAGS`
		AC_SUBST(APACHE_NOTEST_CPPFLAGS)
		AC_SUBST(APACHE_EXTRA_CPPFLAGS)
		AC_SUBST(APACHE_EXTRA_CFLAGS)
		AC_MSG_CHECKING([for Apache version >=2.4])
        	CFLAGS="$CFLAGS `$APRCONFIG --includes`"
		AC_TRY_COMPILE([
#include <httpd.h>
#include <http_request.h>],
[
request_rec *r;
r->connection->client_ip[0] = 0;
],
		AC_DEFINE(APACHE2_4)
		AC_MSG_RESULT([yes]),
		AC_MSG_RESULT([no]))


	fi
	if test "$APACHE_VER" = "1"; then
		APACHE_EXTRA_CFLAGS="$APACHE_CFLAGS"
		AC_SUBST(APACHE_EXTRA_CFLAGS)
	fi

	CFLAGS="$ac_save_cflags"

	AC_SUBST(APACHE_VER)
	AC_SUBST(APACHE_CFLAGS)
	AC_SUBST(APACHE_CFLAGS_SHLIB)
	AC_SUBST(APACHE_LDFLAGS_SHLIB)
	AC_SUBST(APACHE_LIBS_SHLIB)
	AC_SUBST(APACHE_INCDIR)
	AC_SUBST(APACHE_MODDIR)
	AC_SUBST(APXS)
	AC_MSG_RESULT($APACHE_VER)

  else
	AC_MSG_RESULT(no)
	APACHE_MODULE=""
	USE_APACHE_MODULE=0
  fi

AC_SUBST(APACHE_MODULE)
AC_SUBST(USE_APACHE_MODULE)

AM_CONDITIONAL(APACHEMODULE, test "x$apache" != 'xno')
AM_CONDITIONAL(APACHE2, test "x$APACHE_VER" != 'x1')


apachecacheonly=0
AC_MSG_CHECKING(for cache mode only version of mod_dpsearch)
if test "x$apache" != 'xno'; then

AC_ARG_ENABLE(apachecacheonly,
[  --enable-apachecacheonly enable cache mode only version of mod_dpsearch.],
[
	apachecacheonly="$enableval"
        AC_MSG_RESULT($enableval)
	AC_DEFINE(APACHE_CACHEONLY)
],[
  	AC_MSG_RESULT(no)
])

else
	AC_MSG_RESULT(no)
fi


threadsless=no
if test "x$apache" != 'xno' && test "x$pthreads" != 'xno'; then
	threadsless=yes
fi
AC_MSG_CHECKING(for threadsless version of libdpsearch)
AC_ARG_ENABLE(threadsless,
[  --enable-threadsless    enable threadsless version of libdpsearch.  
			  This is mandatory for the perl interface.],
[
	threadsless="$enableval"
        AC_MSG_RESULT($threadsless)
],[
  	AC_MSG_RESULT($threadsless)
])

AM_CONDITIONAL(THREADSLESS, test "x$threadsless" != 'xno')


php=no
AC_MSG_CHECKING(for PHP module support)
AC_ARG_ENABLE(php-module,
[  --enable-php-module[[=DIR]]
                          enable building of PHP module.  DIR is the php-config
                          install directory, defaults to /usr/local/bin.],
[
	php=$enableval
])

  if test "$php" != "no"; then
        if test "$php" = "yes"; then
	    PATH_DIRS=`echo $PATH | tr : \ `
	    MY_DIRS="/usr/bin /usr/local/php/bin /usr/local/bin"
	    for DIR in $PATH_DIRS $MY_DIRS; do
        	if test -f $DIR/php-config; then
		    	PHPCONFIG=$DIR/php-config
			break;
		fi
	    done
	    if test -z "$PHPCONFIG"; then
		AC_MSG_ERROR([php-config not found in $PATH_DIRS $MY_DIRS])
	    fi
        elif test "$php" != ""; then
	    MY_DIRS="$php $php/bin"
	    for DIR in $MY_DIRS; do
		if test -f $DIR/php-config; then
		    PHPCONFIG=$DIR/php-config
		    break;
		fi
	    done
	    if test -z "$PHPCONFIG"; then
		AC_MSG_ERROR([php-config not found in $MY_DIRS])
	    fi
	fi
	PHP_INCLUDES=`$PHPCONFIG --includes 2>/dev/null`
	PHP_EXTENSION_DIR=`$PHPCONFIG --extension-dir 2>/dev/null`
	PHP_MODULE="dpsearch.so"
	AC_MSG_RESULT(yes)

  else
	AC_MSG_RESULT(no)
  fi
AC_SUBST(PHP_INCLUDES)
AC_SUBST(PHP_EXTENSION_DIR)
AC_SUBST(PHP_MODULE)
AM_CONDITIONAL(PHPMODULE, test "x$php" != 'xno')


idn=no
AC_MSG_CHECKING(for Internationalized Domain Names support)
AC_ARG_ENABLE(idn,
[  --enable-idn[[=DIR]]      enable support for Internationalized Domain Names.  
			  DIR is the GNU libidn library base install directory.],
[
  	if test "$enableval" != "no"; then
	        if test "$enableval" = "yes"; then
			case "$host" in
			*-*-solaris*)
				IDN_CFLAGS=""
				IDN_LIBS="-lidnkit"
				AC_DEFINE(WITH_IDNKIT)
				;;
			*-*-freebsd*)
				if test -f /usr/local/include/idn-int.h; then
					IDN_CFLAGS="-I/usr/local/include"
					IDN_LIBS="-L/usr/local/lib -lidn -liconv"
					IDN_INCLUDES="$IDN_CFLAGS"
				else
					IDN_CFLAGS=`pkg-config libidn --cflags`
					IDN_INCLUDES=`pkg-config libidn --cflags-only-I`
					IDN_LIBS=`pkg-config libidn --libs`
				fi
				AC_DEFINE(WITH_IDN)
				;;
			*-*-linux*)
				if test -f /usr/include/idn-int.h; then
					IDN_CFLAGS=""
					IDN_LIBS="-lidn"
				else
					IDN_CFLAGS=`pkg-config libidn --cflags`
					IDN_INCLUDES=`pkg-config libidn --cflags-only-I`
					IDN_LIBS=`pkg-config libidn --libs`
				fi
				AC_DEFINE(WITH_IDN)
				;;
			*)
				if test -f /usr/local/include/idn-int.h; then
					IDN_CFLAGS="-I/usr/local/include"
					IDN_LIBS="-L/usr/local/lib -lidn"
					IDN_INCLUDES="$IDN_CFLAGS"
				else
					IDN_CFLAGS=`pkg-config libidn --cflags`
					IDN_INCLUDES=`pkg-config libidn --cflags-only-I`
					IDN_LIBS=`pkg-config libidn --libs`
				fi
				AC_DEFINE(WITH_IDN)
				;;
			esac
			CFLAGS="$CFLAGS $IDN_CFLAGS"
			LIBS="$LIBS $IDN_LIBS"
			AM_CPPFLAGS="$AM_CPPFLAGS $IDN_INCLUDES"
		else
			if test -f $enableval/include/idn-int.h; then
				CFLAGS="$CFLAGS -I$withval/include"
				AM_CPPFLAGS="$AM_CPPFLAGS -I$withval/include"
			else
				AC_MSG_RESULT(no)
				AC_MSG_ERROR(Invalid GNU libidn directory - unable to find idn-int.h under $enableval)
				$enableval=""
			fi
			if test -f $withval/lib/libidn.a; then
        	        	LIBS="$LIBS -L$withval/lib -lidn"
			else
				AC_MSG_RESULT(no)
				AC_MSG_ERROR(Invalid GNU libidn directory - unable to find libidn.a under $enableval)
				$withval=""
			fi
		    	AC_DEFINE(WITH_IDN)
		fi
	    AC_MSG_RESULT($enableval)
	else
	    AC_MSG_RESULT(no)
	fi
	idn=$enableval
],[
  	AC_MSG_RESULT(no)
])




dnl check building in FreeBSD ports
AM_CONDITIONAL(PORTS, test "x$PORTNAME" != 'x')



dnl Check for some DBMS backend
dnl NOTE: we can use several different backend at the same time

have_db=no
DBTYPE=

AC_MSG_CHECKING(for MySQL support)
AC_ARG_WITH(mysql,
[
What DBMS support do you want (you may select several):
  --with-mysql[=DIR]      Include MySQL support.  DIR is the MySQL base
                          install directory, default is to search through
                          a number of common places for the MySQL files.],
[
  if test "$withval" != "no"; then
    MYSQL_INCLUDE=""
    MYSQL_LFLAGS=""
  	if test "$withval" = "yes"; then

		PATH_DIRS=`echo $PATH | tr : \ `
	    	MY_DIRS="/usr/bin /usr/local/bin"
	    	MYSQL_CONFIG=""
	    	for DIR in $PATH_DIRS $MY_DIRS; do
        		if test -f $DIR/mysql_config; then
            			MYSQL_CONFIG=$DIR/mysql_config
				break;
			fi
	    	done

		if test "$MYSQL_CONFIG" != ""; then
			MYSQL_INCLUDE=`$MYSQL_CONFIG --include`
			MYSQL_LFLAGS=`$MYSQL_CONFIG --libs`
		elif test -f /usr/local/mysql/include/mysql/mysql.h; then
			MYSQL_INCLUDE="-I/usr/local/mysql/include/mysql"
			MYSQL_LFLAGS="-L/usr/local/mysql/lib/mysql -lmysqlclient"
			withval="/usr/local/mysql"
		elif test -f /usr/include/mysql/mysql.h; then
			MYSQL_INCLUDE="-I/usr/include/mysql"
			MYSQL_LFLAGS="-L/usr/lib/mysql -lmysqlclient"
			withval="/usr"
		elif test -f /usr/include/mysql.h; then
			MYSQL_INCLUDE="-I/usr/include"
			MYSQL_LFLAGS="-L/usr/lib -lmysqlclient"
			withval="/usr"
		elif test -f /usr/local/include/mysql/mysql.h; then
			MYSQL_INCLUDE="-I/usr/local/include/mysql"
			MYSQL_LFLAGS="-L/usr/local/lib/mysql -lmysqlclient"
			withval="/usr/local"
		elif test -f /usr/local/include/mysql.h; then
			MYSQL_INCLUDE="-I/usr/local/include"
			MYSQL_LFLAGS="-L/usr/local/lib -lmysqlclient"
			withval="/usr/local"
		elif test -f /Library/MySQL/include/mysql/mysql.h; then
			MYSQL_INCLUDE="-I/Library/MySQL/include/mysql"
			MYSQL_LFLAGS=="-L/Library/MySQL/lib/mysql -lmysqlclient"
			withval="/Library/MySQL"
		else
			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Unknown MySQL directory - unable to find mysql.h)
		fi
   	else
		if test -f $withval/include/mysql/mysql.h; then
   			MYSQL_INCDIR=$withval/include/mysql
   			MYSQL_LIBDIR=$withval/lib/mysql
		elif test -f $withval/include/mysql.h; then
   			MYSQL_INCDIR=$withval/include
   			MYSQL_LIBDIR=$withval/lib
		else
			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid MySQL directory - unable to find mysql.h under $withval)
		fi
	    	MYSQL_INCLUDE="-I$MYSQL_INCDIR"
    		MYSQL_LFLAGS="-L$MYSQL_LIBDIR -lmysqlclient"
   	fi

    AC_DEFINE(HAVE_DP_MYSQL)
    AC_MSG_RESULT($withval)
    have_db=yes
    dnl MySQL requires -lm sometime
    AC_CHECK_FUNC(floor, , AC_CHECK_LIB(m, floor, MYSQL_LFLAGS="$MYSQL_LFLAGS -lm"))
    dnl MySQL 3.23 requires -lz sometime
    AC_CHECK_FUNC(compress, , AC_CHECK_LIB(z, compress, MYSQL_LFLAGS="$MYSQL_LFLAGS -lz"))
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="mysql"
    else
       DBTYPE="$DBTYPE-mysql"
    fi

  else
  	AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(MYSQL_LFLAGS)
AC_SUBST(MYSQL_INCLUDE)


AC_MSG_CHECKING(for PostgreSQL support)
AC_ARG_WITH(pgsql,
[  --with-pgsql[=DIR]      Include PostgreSQL support.  DIR is the PostgreSQL
                          base install directory, defaults to /usr/local/pgsql.],
[
  if test "$withval" != "no"; then

  	if test "$withval" != "yes"; then
   		test -f $withval/include/libpq-fe.h && PGSQL_INCDIR=$withval/include
   		test -d $withval/include/pgsql && PGSQL_INCDIR=$withval/include/pgsql
   		test -f $withval/lib/libpq.a && PGSQL_LIBDIR=$withval/lib
   		test -d $withval/lib/pgsql && PGSQL_LIBDIR=$withval/lib/pgsql
	else
	    PATH_DIRS=`echo $PATH | tr : \ `
	    MY_DIRS="/usr/bin /usr/local/pgsql/bin /usr/local/bin"
	    for DIR in $PATH_DIRS $MY_DIRS; do
        	if test -f $DIR/pg_config; then
		    	PGCONFIG=$DIR/pg_config
			break;
		fi
	    done
	    if test -z "$PGCONFIG"; then
dnl 		AC_MSG_ERROR([pg_config not found in $PATH_DIRS $MY_DIRS])
		PGSQL_INCDIR=/usr/local/include/pgsql
		test -f /usr/include/pgsql/libpq-fe.h && PGSQL_INCDIR=/usr/include/pgsql
		test -f /usr/local/pgsql/include/libpq-fe.h && PGSQL_INCDIR=/usr/local/pgsql/include
		test -f /usr/include/postgresql/libpq-fe.h && PGSQL_INCDIR=/usr/include/postgresql
	        test -f /usr/local/include/postgresql/libpq-fe.h && PGSQL_INCDIR=/usr/local/include/postgresql
		test -f /usr/local/include/libpq-fe.h && PGSQL_INCDIR=/usr/local/include
		PGSQL_LIBDIR=/usr/local/lib
		test -f /usr/lib/libpq.a && PGSQL_LIBDIR=/usr/lib
		test -f /usr/local/pgsql/lib/libpq.a && PGSQL_LIBDIR=/usr/local/pgsql/lib
	    fi
	    PGSQL_INCDIR=`$PGCONFIG --includedir 2>/dev/null`
	    PGSQL_LIBDIR=`$PGCONFIG --libdir 2>/dev/null`
dnl 	    PGSQL_CF_CFLAGS=`$PGCONFIG --cflags 2>/dev/null`
dnl 	    PGSQL_CF_LFLAGS=`$PGCONFIG --ldflags 2>/dev/null`
	    PGSQL_CF_LIBS=`$PGCONFIG --libs 2>/dev/null`
  	fi

    if test "x$PGSQL_INCDIR" = "x" ; then
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(Unable to find libpq-fe.h)
    fi

    PGSQL_INCLUDE="-I$PGSQL_INCDIR"
    if test "$LINKING" != "dynamic"; then
	    PGSQL_LFLAGS="-L$PGSQL_LIBDIR -lpq $PGSQL_CF_LIBS"
    else
	    PGSQL_LFLAGS="-L$PGSQL_LIBDIR -lpq"
    fi
    AC_DEFINE(HAVE_DP_PGSQL)
    AC_MSG_RESULT(yes)
    have_db=yes
    
    AC_CHECK_FUNC(crypt, , AC_CHECK_LIB(crypt, crypt, PGSQL_LFLAGS="$PGSQL_LFLAGS -lcrypt"))

    if test "x$DBTYPE" = "x" ; then
       DBTYPE="pqsql"
    else
       DBTYPE="$DBTYPE-pqsql"
    fi

  else
    AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(PGSQL_LFLAGS)
AC_SUBST(PGSQL_INCLUDE)


AC_MSG_CHECKING(for mSQL support)
AC_ARG_WITH(msql,
[  --with-msql[=DIR]       Include mSQL support.  DIR is the mSQL base
                          install directory, defaults to /usr/local/Hughes.],
[
  if test "$withval" != "no"; then

    if test "$withval" = "yes"; then
	MSQL_INCDIR=/usr/local/Hughes/include
	MSQL_LIBDIR=/usr/local/Hughes/lib
    else
	MSQL_INCDIR=$withval/include
        MSQL_LIBDIR=$withval/lib
    fi
    MSQL_INCLUDE="-I$MSQL_INCDIR"
    MSQL_LFLAGS="-L$MSQL_LIBDIR -lmsql"

    AC_DEFINE(HAVE_DP_MSQL)
    AC_MSG_RESULT(yes)
    have_db=yes
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="msql"
    else
       DBTYPE="$DBTYPE-msql"
    fi

  else
  	AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(MSQL_LFLAGS)
AC_SUBST(MSQL_INCLUDE)


AC_MSG_CHECKING(for iODBC support)
AC_ARG_WITH(iodbc,
[  --with-iodbc[=DIR]      Include iODBC support.  DIR is the iODBC base
                          install directory, defaults to /usr/local.],
[
	if test "$withval" = "yes"; then
		withval=/usr/local
	fi

	if test "$withval" != "no"; then
		if test -f $withval/include/isql.h; then
			IODBC_INCDIR=$withval/include
			IODBC_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid iODBC directory - unable to find isql.h)
		fi
		IODBC_LFLAGS="-L$IODBC_LIBDIR -liodbc"
		IODBC_INCLUDE="-I$IODBC_INCDIR"
		AC_DEFINE(HAVE_IODBC)
		AC_MSG_RESULT(yes)
		have_db=yes
		if test "x$DBTYPE" = "x" ; then
			DBTYPE="iODBC"
		else
			DBTYPE="$DBTYPE-iODBC"
		fi
	fi
],[
	AC_MSG_RESULT(no)
])
AC_SUBST(IODBC_LFLAGS)
AC_SUBST(IODBC_INCLUDE)


AC_MSG_CHECKING(for unixODBC support)
AC_ARG_WITH(unixODBC,
[  --with-unixODBC[=DIR]   Include unixODBC support.  DIR is the unixODBC base
                          install directory, defaults to /usr/local.],
[

if test "$withval" != "no"; then
	if test "$withval" = "yes"; then
		if test -f /usr/include/sql.h; then
			UNIXODBC_INCDIR=/usr/include
			UNIXODBC_LIBDIR=/usr/lib
			withval="/usr"
		elif test -f /usr/local/include/sql.h; then
			UNIXODBC_INCDIR=/usr/local/include
			UNIXODBC_LIBDIR=/usr/local/lib
			withval="/usr/local"
		else
			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Unknown unixODBC directory - unable to find sql.h)
		fi
	else
		if test -f $withval/include/sql.h; then
			UNIXODBC_INCDIR=$withval/include
			UNIXODBC_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid unixODBC directory - unable to find sql.h)
		fi
	fi
	have_db=yes
	UNIXODBC_LFLAGS="-L$UNIXODBC_LIBDIR -lodbc"
	UNIXODBC_INCLUDE="-I$UNIXODBC_INCDIR"
	AC_DEFINE(HAVE_UNIXODBC)
	AC_MSG_RESULT($withval)
	if test "x$DBTYPE" = "x" ; then
		DBTYPE="unixODBC"
	else
		DBTYPE="$DBTYPE-unixODBC"
	fi
fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(UNIXODBC_LFLAGS)
AC_SUBST(UNIXODBC_INCLUDE)

AC_MSG_CHECKING(for IBM DB2 support)
AC_ARG_WITH(db2,
[  --with-db2[=DIR]        Include db2 support.  DIR is the DB2 base
                          install directory, defaults to /home/db2],
[
	if test "$withval" = "yes"; then
		withval=/usr/local
	fi

	if test "$withval" != "no"; then
		if test -f $withval/include/sqlcli1.h; then
			DB2_INCDIR=$withval/include
			DB2_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid DB2 directory - unable to find sqlcli1.h)
		fi
		DB2_LFLAGS="-L$DB2_LIBDIR -ldb2"
		DB2_INCLUDE="-I$DB2_INCDIR"
		AC_DEFINE(HAVE_DB2)
		AC_MSG_RESULT(yes)
		have_db=yes
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="DB2"
    else
       DBTYPE="$DBTYPE-DB2"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(DB2_LFLAGS)
AC_SUBST(DB2_INCLUDE)


AC_MSG_CHECKING(for Solid support)
AC_ARG_WITH(solid,
[  --with-solid[=DIR]      Include Solid support.  DIR is the Solid base
                          install directory, defaults to /usr/local.],
[
	if test "$withval" = "yes"; then
		withval=/usr/local
	fi

	if test "$withval" != "no"; then
		if test -f $withval/include/cli0cli.h; then
			SOLID_INCDIR=$withval/include
			SOLID_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid Solid directory - unable to find cli0cli.h)
		fi
		SOLID_LFLAGS="-L$SOLID_LIBDIR -lsolcli -lm"
		SOLID_INCLUDE="-I$SOLID_INCDIR"
		AC_DEFINE(HAVE_SOLID)
		AC_MSG_RESULT(yes)
		have_db=yes
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="solid"
    else
       DBTYPE="$DBTYPE-solid"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(SOLID_LFLAGS)
AC_SUBST(SOLID_INCLUDE)


AC_MSG_CHECKING(for OpenLink ODBC support)
AC_ARG_WITH(openlink,
[  --with-openlink[=DIR]   Include OpenLink ODBC support. 
                          DIR is the base OpenLink ODBC install directory],
[
  if test "$withval" != "no"; then
  	if test "$withval" = "yes"; then

		if test -f /usr/local/virtuoso-ent/odbcsdk/include/isql.h; then
			VIRT_INCDIR=/usr/local/virtuoso-ent/odbcsdk/include/
			VIRT_LIBDIR=/usr/local/virtuoso-ent/odbcsdk/lib/
		elif test -f /usr/local/virtuoso-lite/odbcsdk/include/isql.h; then
			VIRT_INCDIR=/usr/local/virtuoso-lite/odbcsdk/include/
			VIRT_LIBDIR=/usr/local/virtuoso-lite/odbcsdk/lib/
		elif test -f /usr/local/virtuoso/odbcsdk/include/isql.h; then
			VIRT_INCDIR=/usr/local/virtuoso/odbcsdk/include/
			VIRT_LIBDIR=/usr/local/virtuoso/odbcsdk/lib/
		else
    		AC_MSG_RESULT(no)
    		AC_MSG_ERROR(Invalid OpenLink ODBC directory - unable to find isql.h)
		fi
   	else
		if test -f $withval/odbcsdk/include/isql.h; then
   			VIRT_INCDIR=$withval/odbcsdk/include/
   			VIRT_LIBDIR=$withval/odbcsdk/lib/
		elif test -f $withval/include/isql.h; then
			VIRT_INCDIR=$withval/include/
			VIRT_LIBDIR=$withval/lib/
		else
    		AC_MSG_RESULT(no)
    		AC_MSG_ERROR(Invalid OpenLink ODBC directory - unable to find isql.h under $withval)
		fi
   	fi
    VIRT_INCLUDE="-I$VIRT_INCDIR"
    VIRT_LFLAGS="-L$VIRT_LIBDIR -liodbc"

    AC_DEFINE(HAVE_VIRT)
    AC_MSG_RESULT(yes)
    have_db=yes
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="OpenLinkODBC"
    else
       DBTYPE="$DBTYPE-OpenLinkODBC"
    fi

  else
  	AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(VIRT_LFLAGS)
AC_SUBST(VIRT_INCLUDE)


AC_MSG_CHECKING(for EasySoft ODBC support)
AC_ARG_WITH(easysoft,
[  --with-easysoft[=DIR]   Include EasySoft ODBC support. 
                          DIR is the base EasySoft ODBC install directory],
[
  if test "$withval" != "no"; then
  	if test "$withval" = "yes"; then

		if test -f /usr/local/easysoft/oob/client/include/sql.h; then
			EASYSOFT_INCDIR=/usr/local/easysoft/oob/client/include/
			EASYSOFT_LFLAGS="-L/usr/local/easysoft/oob/client/lib/ -L/usr/local/easysoft/lib"
		else
    		AC_MSG_RESULT(no)
    		AC_MSG_ERROR(Invalid EasySoft ODBC directory - unable to find sql.h)
		fi
   	else
		if test -f $withval/easysoft/oob/client/include/sql.h; then
			EASYSOFT_INCDIR=$withval/easysoft/oob/client/include/
			EASYSOFT_LFLAGS="-L$withval/easysoft/oob/client/lib/ -L$withval/easysoft/lib"
		else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR(Invalid EasySoft ODBC directory - unable to find sql.h under $withval)
		fi
	fi
    EASYSOFT_INCLUDE="-I$EASYSOFT_INCDIR"
    EASYSOFT_LFLAGS="$EASYSOFT_LFLAGS -lesoobclient -lesrpc -lessupp -lesextra"

    AC_DEFINE(HAVE_EASYSOFT)
    AC_MSG_RESULT(yes)
    have_db=yes
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="EasySoftODBC"
    else
       DBTYPE="$DBTYPE-EasySoftODBC"
    fi

  else
  	AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(EASYSOFT_LFLAGS)
AC_SUBST(EASYSOFT_INCLUDE)

AC_MSG_CHECKING(for SAPDB ODBC support)
AC_ARG_WITH(sapdb,
[  --with-sapdb[=DIR]      Include SAPDB support.  DIR is the SAPDB base
                          install directory, defaults to /usr/sapdb.],
[
	if test "$withval" = "yes"; then
		withval=/usr/sapdb
	fi

	if test "$withval" != "no"; then
		if test -f $withval/incl/sql.h; then
			SAPDB_INCDIR=$withval/incl
			SAPDB_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid SAPDB directory - unable to find sql.h)
		fi
		SAPDB_LFLAGS="-L$SAPDB_LIBDIR -lpthread -lsqlod -lsqlrte -lm"
		SAPDB_INCLUDE="-I$SAPDB_INCDIR"
		AC_DEFINE(HAVE_SAPDB)
		AC_MSG_RESULT(yes)
		have_db=yes
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="SAPDB"
    else
       DBTYPE="$DBTYPE-SAPDB"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(SAPDB_LFLAGS)
AC_SUBST(SAPDB_INCLUDE)


AC_MSG_CHECKING(for InterBase support)
AC_ARG_WITH(ibase,
[  --with-ibase[=DIR]      Include InterBase support.  DIR is the InterBase
                          install directory, defaults to /usr/interbase.],
[
	if test "$withval" = "yes"; then
		withval=/usr/interbase
	fi

	if test "$withval" != "no"; then
		if test -f $withval/include/ibase.h; then
			IBASE_INCDIR=$withval/include
			IBASE_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid InterBase directory - unable to find ibase.h)
		fi
		IBASE_LFLAGS="-L$IBASE_LIBDIR -lgds"
		IBASE_INCLUDE="-I$IBASE_INCDIR"
		AC_DEFINE(HAVE_IBASE)
		AC_MSG_RESULT($withval)
		have_db=yes
		dnl IB requires -lcrypt sometime
		AC_CHECK_FUNC(crypt, , AC_CHECK_LIB(crypt, crypt, IBASE_LFLAGS="$IBASE_LFLAGS -lcrypt"))
		dnl IB requires -ldl sometime
		AC_CHECK_FUNC(dlopen, , AC_CHECK_LIB(dl, dlopen, IBASE_LFLAGS="$IBASE_LFLAGS -ldl"))

		dnl InterBase lib on FreeBSD is in old aout format              
		dnl so we need to pass -aout flag to ranlib
		dnl if echo $build_os | grep -q freebsdelf; then
		dnl	RANLIB="$RANLIB -aout"
		dnl	CFLAGS="$CFLAGS -aout"
		dnl	AM_PROG_LIBTOOL
		dnl fi
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="ibase"
    else
       DBTYPE="$DBTYPE-ibase"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(IBASE_LFLAGS)
AC_SUBST(IBASE_INCLUDE)

AC_MSG_CHECKING(for Ct-Lib support)
AC_ARG_WITH(ctlib,
[  --with-ctlib[=DIR]      Include Ct-Lib support.  DIR is the Ct-Lib
                          install directory, defaults to /opt/sybase.],
[
	if test "$withval" = "yes"; then
		withval=/opt/sybase
	fi

	if test "$withval" != "no"; then
		if test -f $withval/include/ctpublic.h; then
			CTLIB_INCDIR=$withval/include
			CTLIB_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid Ct-Lib directory - unable to find ctpublic.h)
		fi
		CTLIB_LFLAGS="-L$CTLIB_LIBDIR -lct -lcs -lsybtcl -lcomn -lintl"
		CTLIB_INCLUDE="-I$CTLIB_INCDIR"
		AC_DEFINE(HAVE_CTLIB)
		AC_MSG_RESULT(yes)
		have_db=yes
		dnl ctlib requires -ldl sometime
		AC_CHECK_FUNC(dlopen, , AC_CHECK_LIB(dl, dlopen, CTLIB_LFLAGS="$CTLIB_LFLAGS -ldl"))
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="ctlib"
    else
       DBTYPE="$DBTYPE-ctlib"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(CTLIB_LFLAGS)
AC_SUBST(CTLIB_INCLUDE)


AC_MSG_CHECKING(for FreeTDS Ct-Lib support)
AC_ARG_WITH(freetds,
[  --with-freetds[=DIR]    Include FreeTDS Ct-Lib support.  DIR is the FreeTDS
                          install directory, defaults to /usr/local.],
[
	if test "$withval" = "yes"; then
		withval=/usr/local
	fi

	if test "$withval" != "no"; then
		if test -f $withval/include/ctpublic.h; then
			CTLIB_INCDIR=$withval/include
			CTLIB_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid FreeTDS directory - unable to find ctpublic.h)
		fi
		CTLIB_LFLAGS="-L$CTLIB_LIBDIR -lct"
		CTLIB_INCLUDE="-I$CTLIB_INCDIR"
		AC_DEFINE(HAVE_CTLIB)
		AC_MSG_RESULT(yes)
		have_db=yes
		AC_CHECK_FUNC(dlopen, , AC_CHECK_LIB(dl, dlopen, CTLIB_LFLAGS="$CTLIB_LFLAGS -ldl"))
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="freetds"
    else
       DBTYPE="$DBTYPE-freetds"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(CTLIB_LFLAGS)
AC_SUBST(CTLIB_INCLUDE)


AC_MSG_CHECKING(for sqlite support)
AC_ARG_WITH(sqlite,
[  --with-sqlite[=DIR]    Include sqlite support.  DIR is the sqlite
                          install directory, defaults to /usr/local.],
[
	if test "$withval" = "yes"; then
		withval=/usr/local
	fi

	if test "$withval" != "no"; then
		if test -f $withval/include/sqlite.h; then
			SQLITE_INCDIR=$withval/include
			SQLITE_LIBDIR=$withval/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid sqlite directory $withval - unable to find sqlite.h)
		fi
		SQLITE_LFLAGS="-L$SQLITE_LIBDIR -lsqlite"
		SQLITE_INCLUDE="-I$SQLITE_INCDIR"
		AC_DEFINE(HAVE_SQLITE)
		AC_MSG_RESULT(yes)
		have_db=yes


		if test "x$DBTYPE" = "x" ; then
			DBTYPE="sqlite"
		else
			DBTYPE="$DBTYPE-sqlite"
		fi
	else
		AC_MSG_RESULT(no)
	fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(SQLITE_LFLAGS)
AC_SUBST(SQLITE_INCLUDE)


AC_MSG_CHECKING(for sqlite3 support)
AC_ARG_WITH(sqlite3,
[  --with-sqlite3[=DIR]   Include sqlite3 support.  DIR is the sqlite3
                          install directory, defaults to "/usr /usr/local".],
[

	path=""

	if test "$withval" = "yes"; then
		withval="/usr /usr/local"
	fi

	if test "$withval" != "no"; then

		for d in $withval; do
			if test -f $d/include/sqlite3.h; then
				SQLITE3_INCDIR=$d/include
				SQLITE3_LIBDIR=$d/lib
				SQLITE3_LFLAGS="-L$SQLITE3_LIBDIR -lsqlite3"
				SQLITE3_INCLUDE="-I$SQLITE3_INCDIR"
				path=$d
				AC_DEFINE(HAVE_SQLITE3)
				AC_MSG_RESULT(yes, $d)
				have_db=yes
 			fi
	        done

		if test "$path" = ""; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid sqlite3 directory $withval - unable to find sqlite3.h)
		fi

		if test "x$DBTYPE" = "x" ; then
			DBTYPE="sqlite3"
		else
			DBTYPE="$DBTYPE-sqlite3"
		fi
	else
		AC_MSG_RESULT(no)
	fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(SQLITE3_LFLAGS)
AC_SUBST(SQLITE3_INCLUDE)


AC_MSG_CHECKING(for Oracle7 support)
AC_ARG_WITH(oracle7,
[  --with-oracle7[=DIR]    Include Oracle 7.3 support.  DIR is the Oracle
                          home directory, defaults to ORACLE_HOME [$ORACLE_HOME]],
[
	if test "$withval" = "yes"; then
		withval="$ORACLE_HOME"
	fi

	if test "$withval" != "no"; then
		if test -f $withval/rdbms/demo/ocidfn.h; then
			ORACLE7_INCDIR=$withval/rdbms/demo/
			ORACLE7_LIBDIR1=$withval/lib
			ORACLE7_LIBDIR2=$withval/rdbms/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid ORACLE directory - unable to find ocidfn.h)
		fi

        ORACLEINST_TOP=$withval
        if test -f "$ORACLEINST_TOP/rdbms/lib/sysliblist"
        then
          ORA_SYSLIB="`cat $ORACLEINST_TOP/rdbms/lib/sysliblist`"
        elif test -f "$ORACLEINST_TOP/lib/sysliblist"
            then
          ORA_SYSLIB="`cat $ORACLEINST_TOP/lib/sysliblist`"
        else
          ORA_SYSLIB="-lm"
        fi
          
		ORACLE7_LFLAGS="-L$ORACLE7_LIBDIR1 -L$ORACLE7_LIBDIR2 \
            -lclient -lsqlnet -lncr -lsqlnet -lclient -lcommon \
            -lgeneric \
            -lepc -lnlsrtl3 -lc3v6 -lcore3 -lnlsrtl3 \
            $ORA_SYSLIB -lcore3 $ORA_SYSLIB"
		ORACLE7_INCLUDE="-I$ORACLE7_INCDIR "
 		AC_DEFINE(HAVE_ORACLE7)
		AC_MSG_RESULT(yes)
		have_db=yes
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="oracle7"
    else
       DBTYPE="$DBTYPE-oracle7"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(ORACLE7_LFLAGS)
AC_SUBST(ORACLE7_INCLUDE)


AC_MSG_CHECKING(for Oracle8 support)
AC_ARG_WITH(oracle8,
[  --with-oracle8[=DIR]    Include Oracle8 support.  DIR is the Oracle
                          home directory, defaults to $ORACLE_HOME or
                          /oracle8/app/oracle/product/8.0.5.],
[
	if test "$withval" = "yes"; then
		withval="$ORACLE_HOME"
	    	DIRS="$ORACLE_HOME /usr/oracle8-client /usr/local/oracle8-client"
		for DIR in $DIRS; do
			if test -f $DIR/rdbms/demo/oci.h; then
				withval=$DIR
				break;
			fi
		done
	fi

	if test "$withval" != "no"; then
		if test -f $withval/rdbms/demo/oci.h; then
			ORACLE8_INCDIR1=$withval/rdbms/demo/
			ORACLE8_INCDIR2=$withval/rdbms/public/
			ORACLE8_INCDIR3=$withval/network/public/
			ORACLE8_INCDIR4=$withval/plsql/public/
			ORACLE8_LIBDIR1=$withval/lib
			ORACLE8_LIBDIR2=$withval/rdbms/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid ORACLE directory - unable to find oci.h)
		fi
                if test -f $withval/lib/libwtc8.so; then
                         ORACLE8_LIBS="-lwtc8"
                fi

		ORACLE8_LFLAGS="-L$ORACLE8_LIBDIR1 -L$ORACLE8_LIBDIR2$ORACLE8_LIBS  -lclntsh -lmm -lepc -lclient -lvsn -lcommon -lgeneric -lcore4 -lnlsrtl3 -lm -lnetv2 -lnttcp -lnetwork -lncr -lsql"
		ORACLE8_INCLUDE="-I$ORACLE8_INCDIR1 -I$ORACLE8_INCDIR2 -I$ORACLE8_INCDIR3 -I$ORACLE8_INCDIR4"
 		AC_DEFINE(HAVE_ORACLE8)
		AC_MSG_RESULT(yes)
		have_db=yes
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="oracle8"
    else
       DBTYPE="$DBTYPE-oracle8"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(ORACLE8_LFLAGS)
AC_SUBST(ORACLE8_INCLUDE)


AC_MSG_CHECKING(for Oracle8i support)
AC_ARG_WITH(oracle8i,
[  --with-oracle8i[=DIR]   Include Oracle8i support.  DIR is the Oracle
                          home directory, defaults to $ORACLE_HOME],
[
	if test "$withval" = "yes"; then
		withval="$ORACLE_HOME"
	    	DIRS="$ORACLE_HOME /usr/oracle8-client /usr/local/oracle8-client"
		for DIR in $DIRS; do
			if test -f $DIR/rdbms/demo/oci.h; then
				withval=$DIR
				break;
			fi
		done
	fi

	if test "$withval" != "no"; then
		if test -f $withval/rdbms/demo/oci.h; then
			ORACLE8_INCDIR1=$withval/rdbms/demo/
			ORACLE8_INCDIR2=$withval/rdbms/public/
			ORACLE8_INCDIR3=$withval/network/public/
			ORACLE8_INCDIR4=$withval/plsql/public/
			ORACLE8_LIBDIR1=$withval/lib
			ORACLE8_LIBDIR2=$withval/rdbms/lib
		else
    			AC_MSG_RESULT(no)
			AC_MSG_ERROR(Invalid ORACLE directory - unable to find oci.h)
		fi
		ORACLE8_LFLAGS="-L$ORACLE8_LIBDIR1 -L$ORACLE8_LIBDIR2 -lclntsh -lmm -lm"
		ORACLE8_INCLUDE="-I$ORACLE8_INCDIR1 -I$ORACLE8_INCDIR2 -I$ORACLE8_INCDIR3 -I$ORACLE8_INCDIR4"
 		AC_DEFINE(HAVE_ORACLE8)
		AC_MSG_RESULT(yes)
		have_db=yes
	fi
    if test "x$DBTYPE" = "x" ; then
       DBTYPE="oracle8i"
    else
       DBTYPE="$DBTYPE-oracle8i"
    fi

],[
	AC_MSG_RESULT(no)
])
AC_SUBST(ORACLE8_LFLAGS)
AC_SUBST(ORACLE8_INCLUDE)


oracle_instantclient=no
AC_MSG_CHECKING(for Oracle Instant Client support)
AC_ARG_WITH(oracle-instantclient,
[  --with-oracle-instantclient[=DIR]   Include Oracle Instant Client support.  DIR is the Oracle
                          home directory, defaults to /usr],
[
       if test "$withval" = "yes"; then
            	withval="/usr"
	    	DIRS="$ORACLE_HOME /usr/oracle8-client /usr/local/oracle8-client"
		for DIR in $DIRS; do
			if test -f $DIR/rdbms/demo/oci.h; then
				withval=$DIR
                       		ORACLE8_INCDIR="$withval/rdbms/demo -I$withval/rdbms/public"
                       		ORACLE8_LIBDIR=$withval/lib
				break;
			elif test -f $DIR/include/oracle/oci.h; then
				withval=$DIR
                       		ORACLE8_INCDIR=$withval/include/oracle
                       		ORACLE8_LIBDIR=$withval/lib
				break;
			fi
		done
       elif test "$withval" != "no"; then
               if test -f $withval/include/oracle/oci.h; then
                       ORACLE8_INCDIR=$withval/include/oracle
                       ORACLE8_LIBDIR=$withval/lib
               else
                       AC_MSG_RESULT(no)
                       AC_MSG_ERROR(Invalid ORACLE directory - unable to find oci.h)
               fi
       fi

       if test "$withval" != "no"; then
       	case "$host" in
		*-*-freebsd*)
	        	ORACLE8_LFLAGS="-L$ORACLE8_LIBDIR -lclntsh -lm -lcompat"
			;;
		*)
	        	ORACLE8_LFLAGS="-L$ORACLE8_LIBDIR -lclntsh -lm"
			;;
       	esac
       	ORACLE8_INCLUDE="-I$ORACLE8_INCDIR"
       	AC_DEFINE(HAVE_ORACLE8)
       	AC_MSG_RESULT(yes)
       	have_db=yes
   	if test "x$DBTYPE" = "x" ; then
       		DBTYPE="oracle8i"
    	else
       		DBTYPE="$DBTYPE-oracle8i"
    	fi
       else
       	AC_MSG_RESULT(no)
       fi

],[
       AC_MSG_RESULT(no)
])
AC_SUBST(ORACLE8_LFLAGS)
AC_SUBST(ORACLE8_INCLUDE)



#
# Check for DocBook and tools
#
PGAC_PROG_JADE
PGAC_CHECK_DOCBOOK([4.1 3.1 3.0])
PGAC_PATH_DOCBOOK_STYLESHEETS
PGAC_PATH_COLLATEINDEX
doc_FILES_EN="HTML_DEP_NOJADE_EN"
doc_FILES_RU="HTML_DEP_NOJADE_RU"
if test "x$JADE" != "x" && test x$have_docbook != "xno" ; then
	doc_FILES_EN="HTML_DEP_EN"
	doc_FILES_RU="HTML_DEP_RU"
fi

# FIXME: add latex checking
AC_SUBST(doc_FILES_EN)
AC_SUBST(doc_FILES_RU)
AC_SUBST(COLLATEINDEX)





dnl Check if one of databases was selected
if test "$have_db" != "yes"; then
   db_PROGS="dpguesser$ac_exeext dpconv$ac_exeext dpurl2text$ac_exeext"
else	
   db_PROGS="indexer$ac_exeext searchd$ac_exeext splitter$ac_exeext stored$ac_exeext cached$ac_exeext dpguesser$ac_exeext dpconv$ac_exeext dpurl2text$ac_exeext"
fi

AC_SUBST(CFLAGS)
AC_SUBST(LFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(db_PROGS)
AC_DEFINE_UNQUOTED(DPS_DBTYPE,"$DBTYPE")
AC_SUBST(DBTYPE)
CDIR=`pwd`
AC_SUBST(CDIR)
AC_OUTPUT(Makefile build/Makefile include/Makefile src/Makefile doc/Makefile etc/Makefile dnl
  misc/Makefile scripts/Makefile create/Makefile dnl
  dnl create/db2/Makefile 
  create/ibase/Makefile 
  dnl create/msql/Makefile dnl
  create/mssql/Makefile create/mysql/Makefile create/oracle/Makefile dnl
  create/pgsql/Makefile 
  dnl create/sapdb/Makefile create/solid/Makefile dnl
  dnl create/sybase/Makefile create/virtuoso/Makefile 
  create/sqlite/Makefile dnl
  create/mimer/Makefile dnl
  scripts/dps-config scripts/run-splitter dnl
dnl  build/dpsearch.spec
  perl/Makefile.PL test/Makefile)dnl php/Makefile)


if test "$have_db" != "yes"; then
	echo
	echo
	echo "No database selected! indexering part will not be compiled"
	echo "If it's incorrect, use --with-XXX where XXX is your preferred database type"
fi

echo
echo "**************************************************"
echo "*                                                *"
echo "*     Now run make && make install               *"
echo "*     If it fails, try to use GNU make,          *"
echo "*     often installed as gmake.                  *"
echo "*                                                *"
echo "*   Thank you for using DataparkSearch Engine!   *"
echo "**************************************************"
echo

