AC_INIT([hdf5-iotest], 0.1)

AC_PROG_CC([mpicc])
AC_CONFIG_MACRO_DIRS([m4])

AC_ARG_ENABLE([darshan],
    AS_HELP_STRING([--enable-darshan], [Enable Darshan profiling]))

AC_ARG_ENABLE([gperftools],
    AS_HELP_STRING([--enable-gperftools], [Enable gperftools profiling]))

AS_IF([test "x$enable_darshan" == "xyes"], [
  AC_SUBST(DISABLE_DARSHAN, "-DENABLE_DARSHAN_PROFILE")
])
AS_IF([test "x$enable_gperftools" == "xyes"], [
  AC_SUBST(DISABLE_GPERFTOOLS, "-DENABLE_GPERFTOOLS_PROFILE")
])

AM_INIT_AUTOMAKE([foreign])
LT_INIT

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
