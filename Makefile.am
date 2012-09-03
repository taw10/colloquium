EXTRA_DIST = configure m4/gnulib-cache.m4
SUBDIRS = lib
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = src/colloquium

AM_CFLAGS = -Wall
AM_CPPFLAGS = -DDATADIR=\""$(datadir)"\" -I$(top_builddir)/lib \
              -I$(top_srcdir)/lib
LDADD = $(top_builddir)/lib/libgnu.a @IGNORE_UNUSED_LIBRARIES_CFLAGS@

src_colloquium_SOURCES = src/colloquium.c src/storycode.c src/render.c \
                         src/layout.c

INCLUDES = "-I$(top_srcdir)/data"

EXTRA_DIST += src/layout.h src/presentation.h src/render.h src/storycode.h \
              src/stylesheet.h

colloquiumdir = $(datadir)/colloquium
colloquium_DATA = data/colloquium.ui

iconsdir = $(datadir)/icons/hicolor/scalable/apps
icons_DATA = data/colloquium-select.svg data/colloquium-text.svg \
             data/colloquium-image.svg

EXTRA_DIST += $(colloquium_DATA)

noinst_PROGRAMS = tests/storycode_test tests/render_test tests/render_test_sc1
TESTS = tests/storycode_test tests/render_test tests/render_test_sc1

tests_storycode_test_SOURCES = tests/storycode_test.c src/storycode.c

tests_render_test_SOURCES = tests/render_test.c src/storycode.c src/render.c \
                            src/layout.c

tests_render_test_sc1_SOURCES = tests/render_test_sc1.c src/storycode.c \
                                src/render.c src/layout.c
