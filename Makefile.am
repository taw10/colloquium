EXTRA_DIST = configure m4/gnulib-cache.m4
SUBDIRS = lib libstorycode harfatum
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = src/colloquium

AM_CFLAGS = -Wall
AM_CPPFLAGS = -DDATADIR=\""$(datadir)"\" -I$(top_builddir)/lib \
              -I$(top_srcdir)/lib
LDADD = $(top_builddir)/lib/libgnu.la @IGNORE_UNUSED_LIBRARIES_CFLAGS@ \
        libstorycode/src/libstorycode.la harfatum/src/libharfatum.la

src_colloquium_SOURCES = src/colloquium.c src/render.c \
                         src/mainwindow.c src/presentation.c \
                         src/stylesheet.c src/loadsave.c src/frame.c \
                         src/slideshow.c

INCLUDES = -Ilibstorycode/src -Iharfatum/src

EXTRA_DIST += src/presentation.h src/render.h \
              src/stylesheet.h src/loadsave.h src/slideshow.h

colloquiumdir = $(datadir)/colloquium
colloquium_DATA = data/colloquium.ui

iconsdir = $(datadir)/icons/hicolor/scalable/apps
icons_DATA = data/colloquium-select.svg data/colloquium-text.svg \
             data/colloquium-image.svg

EXTRA_DIST += $(colloquium_DATA)

noinst_PROGRAMS = tests/render_test tests/render_test_sc1
TESTS = tests/render_test tests/render_test_sc1

tests_render_test_SOURCES = tests/render_test.c src/render.c src/frame.c

tests_render_test_sc1_SOURCES = tests/render_test_sc1.c \
                                src/render.c src/frame.c
