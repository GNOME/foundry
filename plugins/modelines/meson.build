foundry_private_sources += files([
  'modeline.c',
  'plugin.c',
])

executable('test-modeline', ['test-modeline.c'],
  dependencies: [dependency('glib-2.0')],
  include_directories: include_directories('../../contrib'),
)
