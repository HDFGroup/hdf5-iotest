AC_INIT([hdf5-iotest], [0.1.0], [gheber@hdfgroup.org])

AC_PROG_CC([mpicc])
AC_CONFIG_MACRO_DIRS([m4])

AC_ARG_ENABLE([gperftools],
    AS_HELP_STRING([--enable-gperftools], [Enable gperftools profiling]))

AS_IF([test "x$enable_gperftools" == "xyes"], [
  AC_SUBST(DISABLE_GPERFTOOLS, "-DENABLE_GPERFTOOLS_PROFILE")
])

AM_INIT_AUTOMAKE([foreign])
LT_INIT

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
