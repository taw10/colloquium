# Meson file for Colloquium
project('colloquium', 'c',
        version : '0.5.0',
        license : 'GPL3+',
        default_options : ['buildtype=debugoptimized'])

gnome = import('gnome')

datadir=join_paths(get_option('datadir'), 'colloquium')

add_project_arguments('-DPACKAGE_VERSION="'+meson.project_version()+'"', language : 'c')
add_project_arguments('-DDATADIR="'+join_paths(get_option('prefix'), datadir)+'"',
                      language : 'c')
add_project_arguments('-DLOCALEDIR="'+join_paths(get_option('prefix'), get_option('localedir'))+'"',
                      language : 'c')

# Localisation
subdir('po')

# Dependencies
gtkdep = dependency('gtk+-3.0')
jsondep = dependency('json-glib-1.0')
cc = meson.get_compiler('c')
mdep = cc.find_library('m', required : false)

gresources = gnome.compile_resources('colloquium-resources',
                                     'data/colloquium.gresource.xml',
                                     source_dir: 'data', c_name: 'colloquium')

flex = find_program('flex')
bison = find_program('bison')

storycode_tab_ch = custom_target('storycode.tab.c',
                                output : ['storycode.tab.c',
                                          'storycode.tab.h'],
                                input : 'src/storycode.y',
                                command : [bison, '--defines=@OUTPUT1@',
                                                  '-p', 'sc',
						  '--report=all',
                                                  '--output=@OUTPUT0@',
                                                  '@INPUT@'])

storycode_c = custom_target('storycode.c',
                            output : ['storycode.c', 'storycode.h'],
                            input : ['src/storycode.l', storycode_tab_ch],
                            command : [flex, '--outfile=@OUTPUT0@',
                                             '--header-file=@OUTPUT1@',
                                             '-P', 'sc',
                                             '@INPUT@'])

executable('sc2_test',
           ['src/sc2_test.c',
	    storycode_c,
	   ],
	   gresources,
	   dependencies : [gtkdep])

# Main program
executable('colloquium',
           ['src/colloquium.c',
            'src/narrative_window.c',
            'src/render.c',
            'src/slideshow.c',
            'src/debugger.c',
            'src/pr_clock.c',
            'src/sc_editor.c',
            'src/slide_window.c',
            'src/frame.c',
            'src/presentation.c',
            'src/sc_interp.c',
            'src/testcard.c',
            'src/imagestore.c',
            'src/print.c',
            'src/sc_parse.c',
            'src/utils.c',
            'src/stylesheet.c',
            'src/stylesheet_editor.c',
           ],
           gresources,
           dependencies : [gtkdep, mdep, jsondep],
           install : true)

# Desktop file
install_data(['data/colloquium.desktop'],
             install_dir : get_option('datadir')+'/applications')

# Icon
install_data(['data/colloquium.svg'],
             install_dir : get_option('datadir')+'/icons/hicolor/scalable/apps')

# Tests
subdir('tests')
