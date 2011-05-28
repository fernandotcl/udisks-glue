AC_PREREQ(2.59c)

AC_INIT([udisks-glue], [1.3.1], [], [udisks-glue])

AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC_C99
if test "x$ac_cv_prog_cc_c99" = "xno"; then
    AC_MSG_ERROR([C compiler must be C99 compliant])
fi
AC_HEADER_STDC

PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([DBUS_GLIB], [dbus-glib-1])
PKG_CHECK_MODULES([LIBCONFUSE], [libconfuse])

AC_CONFIG_FILES([Makefile man/Makefile src/Makefile])
AC_OUTPUT
