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
gdk_dep = dependency('gdk-3.0', required : true)
gtk_dep = dependency('gtk+-3.0', required : true)
glib_dep = dependency('glib-2.0', required : true)
gio_dep = dependency('gio-2.0', required : true)
cairo_dep = dependency('cairo', required : true)
pango_dep = dependency('pango', required : true)
pangocairo_dep = dependency('pangocairo', required : true)
gdkpixbuf_dep = dependency('gdk-pixbuf-2.0', required : true)
cc = meson.get_compiler('c')
mdep = cc.find_library('m', required : false)

if pangocairo_dep.found()
  add_project_arguments('-DHAVE_PANGO', language : 'c')
endif

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
                    'libstorycode/slide_render_cairo.c',
                    'libstorycode/imagestore.c',
                    storycode_lex_ch,
                    storycode_parse_ch,
                   ],
                   include_directories : libstorycode_includes,
                   dependencies : [cairo_dep, pango_dep, gdkpixbuf_dep,
                                   pangocairo_dep, mdep, gdk_dep, gio_dep],
                   install : true)

libstorycode_dep = declare_dependency(include_directories : libstorycode_includes,
                                      link_with : libstorycode)


# libstorycode-gtk
libgtkstorycode_includes = include_directories('libstorycode/gtk')

libgtkstorycode = library('gtkstorycode',
                   ['libstorycode/gtk/gtknarrativeview.c'],
                   include_directories : libgtkstorycode_includes,
                   dependencies : [gtk_dep, libstorycode_dep],
                   install : true)

libgtkstorycode_dep = declare_dependency(include_directories : libgtkstorycode_includes,
                                         link_with : libgtkstorycode)


# pdfstorycode
executable('pdfstorycode',
           ['src/pdfstorycode.c',
           ],
           gresources,
           dependencies : [glib_dep, gio_dep, cairo_dep, pango_dep,
                           pangocairo_dep, libstorycode_dep])


# Main program
executable('colloquium',
           ['src/colloquium.c',
            'src/narrative_window.c',
#            'src/slideshow.c',
#            'src/pr_clock.c',
#            'src/slide_window.c',
#            'src/testcard.c',
#            'src/stylesheet_editor.c',
           ],
           gresources,
           dependencies : [gtk_dep, mdep, libstorycode_dep, libgtkstorycode_dep],
           install : true)


# Desktop file
install_data(['data/colloquium.desktop'],
             install_dir : get_option('datadir')+'/applications')


# Icon
install_data(['data/colloquium.svg'],
             install_dir : get_option('datadir')+'/icons/hicolor/scalable/apps')


# Tests
subdir('tests')
