i18n = import('i18n')

add_project_arguments('-DGETTEXT_PACKAGE="colloquium"',
                      language : 'c')

i18n.gettext(meson.project_name(),
             preset : 'glib',
             args : '--directory='+meson.source_root())
