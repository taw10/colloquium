dnl Process this file with autoconf to produce a configure script.

AC_INIT(colloquium, 0.0.1, taw@bitwiz.org.uk)
AC_CONFIG_AUX_DIR([build-aux])
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_MACRO_DIR(m4)

AC_PROG_CC
gl_EARLY
AM_PROG_CC_C_O
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
LT_INIT
AM_SILENT_RULES([yes])

AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h unistd.h])
AC_C_CONST
AC_FUNC_MALLOC
AC_TYPE_SIGNAL

gl_INIT


AM_PATH_GTK_3_0(3.0.0,
[ ],
[
   AC_MSG_ERROR([GTK+3 is required.])
])


PKG_CHECK_MODULES([libPNG], [libpng >= 1.2.0],
[  ],
[
   AC_MSG_ERROR([LibPNG is required.])
])


PKG_CHECK_MODULES([Cairo], [cairo >= 1.2.0],
[  ],
[
   AC_MSG_ERROR([Cairo is required.])
])


PKG_CHECK_MODULES([GDK_pixbuf], [gdk-pixbuf], [],
[
   PKG_CHECK_MODULES([GDK_pixbuf_2], [gdk-pixbuf-2.0],
   [   ],
   [
      AC_MSG_ERROR([GDK-pixbuf is required.])
   ])
])


PKG_CHECK_MODULES([DBUS], [dbus-1 >= 1.0.0],
[  ],
[
   AC_MSG_ERROR([dbus is required.])
])



gl_IGNORE_UNUSED_LIBRARIES

LIBHARFATUM_CFLAGS="$CFLAGS -D_GNU_SOURCE $Cairo_CFLAGS"
LIBHARFATUM_LIBS="$LIBS $Cairo_LIBS"
AC_SUBST([LIBHARFATUM_LIBS])

CFLAGS="$CFLAGS $GTK_CFLAGS -D_GNU_SOURCE $libPNG_CFLAGS $Cairo_CFLAGS"
CFLAGS="$CFLAGS $GDK_pixbuf_CFLAGS $GDK_pixbuf_2_CFLAGS $DBUS_CFLAGS"

LIBS="$LIBS -lm -lz $GTK_LIBS $libPNG_LIBS $Cairo_LIBS $GDK_pixbuf_LIBS"
LIBS="$LIBS $GDK_pixbuf_2_LIBS $DBUS_LIBS $LDFLAGS"


AC_CONFIG_FILES(Makefile lib/Makefile)
AC_OUTPUT
