# Meson file for Colloquium
project('colloquium', 'c',
        version : '0.5.0',
        license : 'GPL3+',
        default_options : ['buildtype=debugoptimized'])

datadir=join_paths(get_option('datadir'), 'colloquium')

add_project_arguments('-DPACKAGE_VERSION="'+meson.project_version()+'"', language : 'c')
add_project_arguments('-DDATADIR="'+join_paths(get_option('prefix'), datadir)+'"',
                      language : 'c')
add_project_arguments('-DLOCALEDIR="'+join_paths(get_option('prefix'), get_option('localedir'))+'"',
                      language : 'c')


# Localisation
subdir('po')


# Dependencies
gnome = import('gnome')
gtk_dep = dependency('gtk+-3.0', required : true)
glib_dep = dependency('glib-2.0', required : true)
gio_dep = dependency('gio-2.0', required : true)
cairo_dep = dependency('cairo', required : true)
pango_dep = dependency('pango', required : true)
pangocairo_dep = dependency('pangocairo', required : true)
gdkpixbuf_dep = dependency('gdk-pixbuf-2.0', required : true)
cc = meson.get_compiler('c')


# Compiled-in resources
gresources = gnome.compile_resources('colloquium-resources',
                                     'data/colloquium.gresource.xml',
                                     source_dir: 'data', c_name: 'colloquium')


# libstorycode
libstorycode_includes = include_directories('libstorycode')

flex = find_program('flex')
bison = find_program('bison')

flex_gen = generator(flex,
                     output : ['@BASENAME@_lex.c', '@BASENAME@_lex.h'],
                     arguments : ['--outfile=@OUTPUT0@',
                                  '--header-file=@OUTPUT1@',
                                  '@INPUT@'])

bison_gen = generator(bison,
                      output : ['@BASENAME@_parse.c', '@BASENAME@_parse.h'],
                      arguments : ['--output=@OUTPUT0@',
                                   '--defines=@OUTPUT1@',
                                   '--report=all',
                                   '@INPUT@'])

storycode_parse_ch = bison_gen.process('libstorycode/storycode.y')
storycode_lex_ch = flex_gen.process('libstorycode/storycode.l')

libstorycode = library('storycode',
                   ['libstorycode/narrative.c',
                    'libstorycode/slide.c',
                    'libstorycode/presentation.c',
                    'libstorycode/stylesheet.c',
                    'libstorycode/storycode.c',
                    storycode_lex_ch,
                    storycode_parse_ch,
                   ],
                   include_directories : libstorycode_includes,
                   install : true)

libstorycode_dep = declare_dependency(include_directories : libstorycode_includes,
                                      link_with : libstorycode)


# libstorycode-cairo
libstorycode_cairo_includes = include_directories('libstorycode/cairo')

libstorycode_cairo = library('storycode-cairo',
                   ['libstorycode/cairo/render.c',
                   ],
                   include_directories : libstorycode_cairo_includes,
                   dependencies : [cairo_dep, pango_dep, gdkpixbuf_dep,
                                   pangocairo_dep,
                                   libstorycode_dep],
                   install : true)

libstorycode_cairo_dep = declare_dependency(include_directories : libstorycode_cairo_includes,
                                            link_with : libstorycode_cairo)


# libstorycode-gtk
#libstorycode_gtk_includes = include_directories('libstorycode/gtk')
#
#libstorycode_gtk = library('storycode-gtk',
#                   ['libstorycode/gtk/gtknarrativeview.c',
#                    'libstorycode/gtk/gtkslideview.c',
#                   ],
#                   include_directories : libstorycode_gtk_includes,
#                   dependencies : [gtk_dep, libstorycode_dep,
#                                   libstorycode_cairo_dep],
#                   install : true)
#
#libstorycode_gtk_dep = declare_dependency(include_directories : libstorycode_gtk_includes,
#                                            link_with : libstorycode_gtk)


# pdfstorycode
executable('pdfstorycode',
           ['src/pdfstorycode.c',
           ],
           gresources,
           dependencies : [glib_dep, gio_dep, cairo_dep, pango_dep,
                           pangocairo_dep,
                           libstorycode_dep, libstorycode_cairo_dep])


# Main program
#executable('colloquium',
#           ['src/colloquium.c',
#            'src/narrative_window.c',
#            'src/render.c',
#            'src/slideshow.c',
#            'src/debugger.c',
#            'src/pr_clock.c',
#            'src/sc_editor.c',
#            'src/slide_window.c',
#            'src/frame.c',
#            'src/presentation.c',
#            'src/sc_interp.c',
#            'src/testcard.c',
#            'src/imagestore.c',
#            'src/print.c',
#            'src/sc_parse.c',
#            'src/utils.c',
#            'src/stylesheet.c',
#            'src/stylesheet_editor.c',
#           ],
#           gresources,
#           dependencies : [gtkdep, mdep, jsondep],
#           install : true)


# Desktop file
install_data(['data/colloquium.desktop'],
             install_dir : get_option('datadir')+'/applications')


# Icon
install_data(['data/colloquium.svg'],
             install_dir : get_option('datadir')+'/icons/hicolor/scalable/apps')


# Tests
subdir('tests')
