# Meson file for Colloquium
project('colloquium', 'c',
        version : '0.9.0',
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
gtk_dep = dependency('gtk4', required : true)
glib_dep = dependency('glib-2.0', required : true)
gio_dep = dependency('gio-2.0', required : true)
cairo_dep = dependency('cairo', required : true)
pango_dep = dependency('pango', required : true)
gdkpixbuf_dep = dependency('gdk-pixbuf-2.0', required : true)
rsvg_dep = dependency('librsvg-2.0', required : true)
poppler_dep = dependency('poppler-glib', required : true)
md4c_dep = dependency('md4c', required: false)
cc = meson.get_compiler('c')
mdep = cc.find_library('m', required : false)

if md4c_dep.found()
  add_project_arguments('-DHAVE_MD4C', language: 'c')
endif

# Compiled-in resources
gresources = gnome.compile_resources('colloquium-resources',
                                     'data/colloquium.gresource.xml',
                                     source_dir: 'data', c_name: 'colloquium')


# Main program
executable('colloquium',
           ['src/colloquium.c',
            'src/narrative_window.c',
            'src/slide_window.c',
            'src/print.c',
            'src/narrative.c',
            'src/slide.c',
            'src/slideview.c',
            'src/thumbnailwidget.c',
            'src/slide_sorter.c',
            'src/prefswindow.c',
            'src/timer.c',
            'src/timer_bar.c',
            'src/timer_window.c',
           ],
           gresources,
           dependencies : [gtk_dep, mdep, md4c_dep, poppler_dep, rsvg_dep],
           install : true)


# Desktop file
install_data(['data/uk.me.bitwiz.colloquium.desktop'],
             install_dir : get_option('datadir')+'/applications')


# Icon
install_data(['data/uk.me.bitwiz.colloquium.svg'],
             install_dir : get_option('datadir')+'/icons/hicolor/scalable/apps')


# GSettings schema
install_data(['data/uk.me.bitwiz.colloquium.gschema.xml'],
             install_dir : join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))


gnome.post_install(glib_compile_schemas: true,
                   gtk_update_icon_cache: true,
                   update_desktop_database: true)
