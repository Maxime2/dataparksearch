# Check if SO_REUSEPORT is actually supported

AC_DEFUN([DPSAC_SO_REUSEPORT],[


AC_CACHE_CHECK([whether SO_REUSEPORT defined and works],
  [ac_cv_so_reuseport],
  [AC_LANG_PUSH([C])
   AC_RUN_IFELSE(
    [AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
    ]], [[
  int s = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (void*)&on, (socklen_t)sizeof(on)) < 0)
    return (1);
  return (0);
]])],
  [ac_cv_so_reuseport=yes],
  [ac_cv_so_reuseport=no])
  AC_LANG_POP([C])
  ]
)
if test "$ac_cv_so_reuseport" = yes; then
  AC_DEFINE([HAVE_SO_REUSEPORT], [1], [Define if SO_REUSEPORT defined and works])
fi


])
#DPSAC_SO_REUSEPORT
