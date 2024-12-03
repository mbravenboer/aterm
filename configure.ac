# $Id: configure.ac 24415 2007-12-12 14:20:55Z eriks $

m4_include(float-order.m4)

m4_pattern_forbid([^META_])

AC_INIT
META_SETUP
META_C_SETUP
AC_PROG_LIBTOOL

CURDATE=`date`
AC_SUBST([CURDATE])

AC_C_BIGENDIAN
AX_C_FLOAT_WORD_ORDER_BIG

AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([lrand48 srand48])

AC_CHECK_ALIGNOF([void *])
AC_CHECK_SIZEOF([void *])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([double])

AC_CONFIG_FILES([aterm.spec])

AC_OUTPUT
