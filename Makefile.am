EXTRA_DIST = configure m4/gnulib-cache.m4
SUBDIRS = lib
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = src/colloquium

AM_CFLAGS = -Wall
AM_CPPFLAGS = -DDATADIR=\""$(datadir)"\" -I$(top_builddir)/lib -I$(top_srcdir)/lib
LDADD = $(top_builddir)/lib/libgnu.a @IGNORE_UNUSED_LIBRARIES_CFLAGS@

src_colloquium_SOURCES = src/colloquium.c src/presentation.c src/mainwindow.c \
                         src/slide_render.c src/objects.c src/slideshow.c \
                         src/stylesheet.c

INCLUDES = "-I$(top_srcdir)/data"

EXTRA_DIST += src/presentation.h src/mainwindow.h src/slide_render.h \
              src/objects.h src/slideshow.h src/stylesheet.h

colloquiumdir = $(datadir)/colloquium
colloquium_DATA = data/colloquium.ui

iconsdir = $(datadir)/icons/hicolor/scalable/apps
icons_DATA = data/colloquium-select.svg data/colloquium-text.svg

EXTRA_DIST += $(colloquium_DATA)
