EXTRA_DIST = configure m4/gnulib-cache.m4
SUBDIRS = lib harfatum
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = 

AM_CFLAGS = -Wall
AM_CPPFLAGS = -DDATADIR=\""$(datadir)"\" -I$(top_builddir)/lib \
              -I$(top_srcdir)/lib
LDADD = $(top_builddir)/lib/libgnu.la @IGNORE_UNUSED_LIBRARIES_CFLAGS@ \
        harfatum/src/libharfatum.la src/default_stylesheet.o

src_colloquium_SOURCES = src/colloquium.c src/render.c \
                         src/mainwindow.c src/presentation.c \
                         src/frame.c src/sc_parse.c \
                         src/slideshow.c src/wrap.c src/sc_interp.c \
                         src/imagestore.c src/notes.c src/pr_clock.c \
                         src/inhibit_screensaver.c src/slide_sorter.c

INCLUDES = -Iharfatum/src

EXTRA_DIST += src/presentation.h src/render.h src/wrap.h \
              src/slideshow.h src/sc_parse.h src/sc_interp.h \
              src/imagestore.h src/notes.h src/pr_clock.h \
              src/inhibit_screensaver.h src/slide_sorter.h

src/default_stylesheet.o: src/default_stylesheet.sty
	ld -r -b binary -o src/default_stylesheet.o src/default_stylesheet.sty

colloquiumdir = $(datadir)/colloquium
colloquium_DATA = data/colloquium.ui
EXTRA_DIST += $(colloquium_DATA)

iconsdir = $(datadir)/icons/hicolor/scalable/apps
icons_DATA = data/colloquium.svg
EXTRA_DIST += $(icons_DATA)

desktopdir = $(datadir)/applications
desktop_DATA = colloquium.desktop
EXTRA_DIST += $(desktop_DATA)

noinst_PROGRAMS = tests/storycode_test
#tests/render_test tests/render_test_sc1
TESTS = tests/storycode_test tests/render_test tests/render_test_sc1

tests_storycode_test_SOURCES = tests/storycode_test.c src/sc_parse.c

tests_render_test_SOURCES = tests/render_test.c src/render.c src/frame.c \
                            src/wrap.c src/sc_parse.c src/imagestore.c \
                            src/loadsave.c

tests_render_test_sc1_SOURCES = tests/render_test_sc1.c src/sc_parse.c \
                                src/render.c src/frame.c src/wrap.c \
                                src/imagestore.c \
                                src/loadsave.c
