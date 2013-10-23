# Based on an idea from http://www.freag.net/en/t/4geq9/using_autotools
#
# qsort_r definitions for GNU C and BSD differ, other platforms may not
# have it at all. This function defines 'HAVE_QSORT_R' if the function
# is available, and either 'QSORT_R_BSD' or 'QSORT_R_GNUC' depending on
# the calling convention that's used.


AC_DEFUN([DPSAC_QSORT_R],[

AC_CHECK_FUNC(qsort_r,
[AC_DEFINE(HAVE_QSORT_R,[1],
[whether or not qsort_r is available] ) ])

if test "$ac_cv_func_qsort_r" == yes ; then
AC_LANG_PUSH([C])
AC_MSG_CHECKING([that qsort_r is BSD version])
AC_RUN_IFELSE(
[AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdlib.h>
int unsorted[16]={1,3,5,7,9,11,13,15,2,4,6,8,10,12,14,16};
int bsd_sort_compare(void *a, const void *b, const void *c)
{
const int *p1, *p2;
if(a != (void *)unsorted)
{
exit(2);
}

p1 = (const int *)b;
p2 = (const int *)c;
if(*p1 > *p2)
{
return 1;
}
else if(*p2 > *p1)
{
return -1;
}

return 0;
}
int main()
{
int i1;
qsort_r(unsorted, 16, sizeof(unsorted[0]), (void *)unsorted,
bsd_sort_compare);

for(i1; i1 < 16; i1++)
{
if(unsorted[i1 - 1] + 1 != unsorted[i1])
{
exit(3);
}
}

return 0;
}]] )],
[
AC_MSG_RESULT([yes])
AC_DEFINE([QSORT_R_BSD],[1],[BSD version of qsort_r])
],
[
AC_MSG_RESULT([no])
AC_DEFINE([QSORT_R_GNUC],[1],[GNUC version of qsort_r])
] )
AC_LANG_POP([C])
else
AC_MSG_WARN([using internal qsort_r implementation])
fi

])
#DPSAC_QSORT_R