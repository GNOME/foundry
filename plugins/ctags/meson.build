foundry_private_sources += files([
  'plugin.c',
  'plugin-ctags-file.c',
  'plugin-ctags-symbol-provider.c',
])

executable('test-ctags', ['test-ctags.c', 'plugin-ctags-file.c'],
  dependencies: [foundry_deps, contrib_static_dep],
)
