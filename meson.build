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


# libstorycode
libstorycode_includes = include_directories('libstorycode')

flex = find_program('flex')
bison = find_program('bison')

storycode_parse_ch = custom_target('storycode_parse.c',
                                output : ['storycode_parse.c',
                                          'storycode_parse.h'],
                                input : 'libstorycode/storycode.y',
                                command : [bison, '--defines=@OUTPUT1@',
                                                  '-p', 'sc',
						  '--report=all',
                                                  '--output=@OUTPUT0@',
                                                  '@INPUT@'])

storycode_lex_c = custom_target('storycode_lex.c',
                            output : ['storycode_lex.c', 'storycode_lex.h'],
                            input : ['libstorycode/storycode.l', storycode_parse_ch],
                            command : [flex, '--outfile=@OUTPUT0@',
                                             '--header-file=@OUTPUT1@',
                                             '-P', 'sc',
                                             '@INPUT@'])

libstorycode = library('storycode',
                   ['libstorycode/narrative.c',
                    'libstorycode/slide.c',
                    'libstorycode/presentation.c',
                    'libstorycode/stylesheet.c',
                    'libstorycode/storycode.c',
                    storycode_lex_c,
                   ],
		   include_directories : libstorycode_includes,
		   install : true)

libstorycode_dep = declare_dependency(include_directories : libstorycode_includes,
                                      link_with : libstorycode)


executable('sc2_test',
           ['src/sc2_test.c',
           ],
           gresources,
           dependencies : [gtkdep, libstorycode_dep])

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
