AC_PREREQ(2.59)

AC_INIT([udisks-glue], [1.2.0], [], [udisks-glue])

AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_HEADER_STDC

PKG_CHECK_MODULES([GLIB],[glib-2.0])
PKG_CHECK_MODULES([DBUS_GLIB], [dbus-glib-1])
PKG_CHECK_MODULES([LIBCONFUSE], [libconfuse])

AC_CONFIG_FILES([\
		 Makefile \
		 man/Makefile \
		 src/Makefile
		 ])
AC_OUTPUT
