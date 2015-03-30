##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_define([v_maj], m4_esyscmd([(grep "set(UFO_VERSION_MAJOR" CMakeLists.txt | sed 's,[^[:digit:]],,g' || echo 0) | tr -d '\n']))
m4_define([v_min], m4_esyscmd([(grep "set(UFO_VERSION_MINOR" CMakeLists.txt | sed 's,[^[:digit:]],,g' || echo 0) | tr -d '\n']))
m4_define([v_mic], m4_esyscmd([(grep "set(UFO_VERSION_PATCH" CMakeLists.txt | sed 's,[^[:digit:]],,g' || echo 0) | tr -d '\n']))
m4_define([v_rev], m4_esyscmd([(git rev-list --count HEAD 2>/dev/null || echo 0) | tr -d '\n']))dnl
##--   When released, remove the dnl on the below line
dnl m4_undefine([v_rev])
##--   When doing snapshots - change soname. remove dnl on below line
dnl m4_define([relname], [ver-pre-svn-07])
dnl m4_define([v_rel], [-release relname])
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_ifdef([v_rev], [m4_define([v_ver], [v_maj.v_min.v_mic.v_rev])], [m4_define([v_ver], [v_maj.v_min.v_mic])])
m4_define([lt_cur], m4_eval(v_maj + v_min))
m4_define([lt_rev], v_mic)
m4_define([lt_age], v_min)
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##

AC_PREREQ([2.68])
AC_INIT([ufo], [v_ver], [http://ufo.kit.edu/ufo/newticket])
AC_CONFIG_SRCDIR([ufo/ufo-task-node.h])
AC_CONFIG_HEADERS([common/autotools/config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])

##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_ifdef([v_rev], , [m4_define([v_rev], [0])])
m4_ifdef([v_rel], , [m4_define([v_rel], [])])
AC_DEFINE_UNQUOTED(VMAJ, [v_maj], [Major version])
AC_DEFINE_UNQUOTED(VMIN, [v_min], [Minor version])
AC_DEFINE_UNQUOTED(VMIC, [v_mic], [Micro version])
AC_DEFINE_UNQUOTED(VREV, [v_rev], [Revison])
version_info="lt_cur:lt_rev:lt_age"
release_info="v_rel"
AC_SUBST(version_info)
AC_SUBST(release_info)
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
VMAJ=v_maj
AC_SUBST(VMAJ)

dnl ***********
dnl *** MPI ***
dnl ***********

# If --with-mpi=auto is used, try to find MPI, but use standard C compiler if it is not found.
# If --with-mpi=yes is used, try to find MPI and fail if it isn't found.
# If --with-mpi=no is used, use a standard C compiler instead.
AC_ARG_WITH(mpi, [AS_HELP_STRING([--with-mpi],
    [compile with MPI (parallelization) support. If none is found,
    MPI is not used. Default: no])
],,[with_mpi=no])
#
AX_PROG_CC_MPI([test x"$with_mpi" != xno],[use_mpi=yes],[
  use_mpi=no
  if test x"$with_mpi" = xyes; then
    AC_MSG_FAILURE([MPI compiler requested, but couldn't use MPI.])
  else
    AC_MSG_WARN([No MPI compiler found, won't use MPI.])
  fi
])
AM_CONDITIONAL([WITH_MPI], [test x"$with_mpi" = xyes])

dnl ************
dnl *** KIRO ***
dnl ************

# If --with-kiro=auto is used, try to find KIRO InfiniBand and use it only if found.
# If --with-kiro=yes is used, try to find KIRO InfiniBand and fail if it isn't found.
# If --with-mpi=no is used, don't build with KIRO InfiniBand support.
AC_ARG_WITH(kiro, [AS_HELP_STRING([--with-kiro],
    [locate KIRO InfiniBand library and build with support for InfiniBand
     communication between nodes. Default: auto])
],,[with_kiro=auto])
#
PKG_CHECK_MODULES([KIRO],[kiro >= 1.1],[use_kiro=yes],[use_kiro=no])
if test x"$with_kiro" != xauto; then
    if test x"$with_kiro" = xno; then
        use_kiro=no
    else
        if test x"$use_kiro" != xyes; then
            AC_MSG_FAILURE([KIRO InfiniBand support requested but KIRO library could not be found])
        fi
    fi
fi
AM_CONDITIONAL([WITH_KIRO], [test x"$use_kiro" = xyes]) 

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
LT_INIT

# Checks for libraries.
AM_PATH_GLIB_2_0(2.30.0,,,gmodule gobject gthread gio)
PKG_CHECK_MODULES([JSON_GLIB], [json-glib-1.0 >= 0.7.6])
PKG_CHECK_MODULES([ZMQ3], [libzmq >= 2.1])
PKG_CHECK_MODULES([OPENCL], [OpenCL >= 1.2])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.

dnl **********************************
dnl *** add a few config variables ***
dnl **********************************

AC_DEFINE_UNQUOTED([UFO_VERSION], [VERSION], [the UFO version])

dnl *********************
dnl *** introspection ***
dnl *********************

GOBJECT_INTROSPECTION_CHECK([0.6.7])

dnl ***************
dnl *** gtk-doc ***
dnl ***************

GTK_DOC_CHECK([1.9],[--flavour no-tmpl])

dnl **************
dnl *** python ***
dnl **************

AM_PATH_PYTHON(,[PKG_CHECK_MODULES([PYTHON], [python])], [:])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])

AC_CONFIG_FILES([Makefile
                 common/Makefile
                 common/autotools/Makefile
                 common/autotools/ufo.pc
                 docs/Makefile
                 docs/manual/Makefile
                 tests/Makefile
                 bin/Makefile
                 ufo/Makefile])
AC_OUTPUT
