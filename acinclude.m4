sinclude(build/m4/docbook.m4)
sinclude(build/m4/net.m4)
sinclude(build/m4/threads.m4)
sinclude(build/m4/qsort_r.m4)
sinclude(build/m4/reuseport.m4)
sinclude(build/m4/ax_func_which_gethostbyname_r.m4)
dnl builtin([include], [docbook.m4])
dnl builtin([include], [net.m4])
dnl builtin([include], [threads.m4])
dnl dnl builtin([include], [libtool15.m4])
dnl dnl builtin([include], [largefiles.m4])

dnl Macro to check for a function prototype in a given header.
dnl
AC_DEFUN([AC_CHECK_FUNC_PROTO],
        [AC_CACHE_CHECK(for $1 function prototype in $2, ac_cv_have_$1_proto,
                AC_EGREP_CPP([$1],
[
#define _GNU_SOURCE
#define __USE_GNU
#include <$2>
],
                        ac_cv_have_$1_proto=yes,
                        ac_cv_have_$1_proto=no))
        if test $ac_cv_have_$1_proto = yes ; then
                ac_tr_func=HAVE_`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`_PROTO
                AC_DEFINE_UNQUOTED($ac_tr_func)
        fi
])

# PGAC_UNION_SEMUN
# ----------------
# Check if `union semun' exists. Define HAVE_UNION_SEMUN if so.
# If it doesn't then one could define it as
# union semun { int val; struct semid_ds *buf; unsigned short *array; }
AC_DEFUN([PGAC_UNION_SEMUN],
[AC_CHECK_TYPES([union semun], [], [],
[#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>])])
# PGAC_UNION_SEMUN


# AX_CHECK_CFLAGS(ADDITIONAL-CFLAGS, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND)
#
# checks whether the $(CC) compiler accepts the ADDITIONAL-CFLAGS
# if so, they are added to the CXXFLAGS
AC_DEFUN([AX_CHECK_CFLAGS],
[
  AC_MSG_CHECKING([whether compiler accepts "$1"])
  cat > conftest.c++ << EOF
  int main(){
    return 0;
  }
EOF
  if $CC $CPPFLAGS $CFLAGS -o conftest.o conftest.c++ [$1] > /dev/null 2>&1
  then
    AC_MSG_RESULT([yes])
    CFLAGS="${CFLAGS} [$1]"
    [$2]
  else
    AC_MSG_RESULT([no])
   [$3]
  fi
])dnl AX_CHECK_CFLAGS

AC_DEFUN([DP_TAR_FLAGS],
[
	_dp_tar_flags='-jcPf -ycPf'
	AC_MSG_CHECKING([tar flags for bzip2 compressed bundle])
	if test -n "${am_cv_dp_tar_flags}"; then
	   	dp_tar_flags=$am_cv_dp_tar_flags
	else
		for dp_tar_flags in $_dp_tar_flags; do
    		    # tar/untar a dummy directory, and stop if the command works.
    		    rm -rf conftest.dir conftest.tar.bz2
    		    mkdir conftest.dir
    		    echo GrepMe > conftest.dir/file
    		    AM_RUN_LOG([tar $dp_tar_flags conftest.tar.bz2 conftest.dir])
    		    rm -rf conftest.dir
    		    if test -s conftest.tar.bz2; then
      		       AM_RUN_LOG([tar -xf conftest.tar.bz2])
      		       AM_RUN_LOG([cat conftest.dir/file])
     		       grep GrepMe conftest.dir/file >/dev/null 2>&1 && break
    		    fi
		done
		rm -rf conftest.dir conftest.tar.bz2
	fi
	AC_CACHE_VAL([am_cv_dp_tar_flags], [am_cv_dp_tar_flags=$dp_tar_flags])
	AC_MSG_RESULT([$dp_tar_flags])
	AC_SUBST([dp_tar_flags])
])dnl DP_TAR_FLAGS
