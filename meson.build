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
poppler_dep = dependency('poppler-glib', required : true)
md4c_dep = dependency('md4c', required: false)
cc = meson.get_compiler('c')
mdep = cc.find_library('m', required : false)

if pangocairo_dep.found()
  add_project_arguments('-DHAVE_PANGO', language : 'c')
endif

if md4c_dep.found()
  add_project_arguments('-DHAVE_MD4C', language: 'c')
endif

# Compiled-in resources
gresources = gnome.compile_resources('colloquium-resources',
                                     'data/colloquium.gresource.xml',
                                     source_dir: 'data', c_name: 'colloquium')


# libstorycode
libstorycode_includes = include_directories('libstorycode')

libstorycode = library('storycode',
                   ['libstorycode/narrative.c',
                    'libstorycode/slide.c',
                    'libstorycode/stylesheet.c',
                    'libstorycode/slide_render_cairo.c',
                    'libstorycode/narrative_render_cairo.c',
                   ],
                   include_directories : libstorycode_includes,
                   dependencies : [cairo_dep, pango_dep, gdkpixbuf_dep,
                                   pangocairo_dep, mdep, gdk_dep, gio_dep,
                                   poppler_dep, md4c_dep],
                   install : true)

libstorycode_dep = declare_dependency(include_directories : libstorycode_includes,
                                      link_with : libstorycode)


# libstorycode-gtk
libgtkstorycode_includes = include_directories('libstorycode/gtk')

libgtkstorycode = library('gtkstorycode',
                   ['libstorycode/gtk/gtknarrativeview.c',
                    'libstorycode/gtk/timing_ruler.c',
                    'libstorycode/gtk/gtkslideview.c'],
                   include_directories : libgtkstorycode_includes,
                   dependencies : [gtk_dep, libstorycode_dep, mdep],
                   install : true)

libgtkstorycode_dep = declare_dependency(include_directories : libgtkstorycode_includes,
                                         link_with : libgtkstorycode)


# Main program
executable('colloquium',
           ['src/colloquium.c',
            'src/narrative_window.c',
            'src/slideshow.c',
            'src/pr_clock.c',
            'src/slide_window.c',
            'src/testcard.c',
            'src/print.c',
            'src/stylesheet_editor.c',
           ],
           gresources,
           dependencies : [gtk_dep, mdep, libstorycode_dep, libgtkstorycode_dep, md4c_dep],
           install : true)


# Desktop file
install_data(['data/colloquium.desktop'],
             install_dir : get_option('datadir')+'/applications')


# Icon
install_data(['data/colloquium.svg'],
             install_dir : get_option('datadir')+'/icons/hicolor/scalable/apps')
