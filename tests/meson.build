basic_rendering = ['../src/render.c', '../src/frame.c',
                   '../src/sc_parse.c', '../src/imagestore.c',
                   '../src/sc_interp.c', '../src/utils.c']

e = executable('storycode_test', 'storycode_test.c', '../src/sc_parse.c',
               dependencies : [gtkdep])
test('Simple StoryCode parsing', e)

e = executable('render_test', 'render_test.c', basic_rendering,
               dependencies : [gtkdep])
test('Simple rendering', e)

e = executable('render_test_sc1', 'render_test_sc1.c', basic_rendering,
               dependencies : [gtkdep])
test('Simple StoryCode rendering', e)
