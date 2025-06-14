contrib_sources = [
  'ide-fuzzy-mutable-index.c',
  'eggflattenlistmodel.c',
  'eggmaplistmodel.c',
  'eggrbtree.c',
  'xml-reader.c',
]

contrib_deps = [
  glib_dep,
  dependency('libxml-2.0'),
]

contrib_static = static_library('contrib', contrib_sources,
           dependencies: contrib_deps,
  gnu_symbol_visibility: 'hidden',
)

contrib_static_dep = declare_dependency(
           link_whole: contrib_static,
  include_directories: include_directories('.'),
         dependencies: contrib_deps,
)

subdir('eggbitset')
