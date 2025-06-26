foundry_gtk_sources += files([
  'foundry-menu-manager.c',
  'foundry-shortcut-provider.c',
])

foundry_gtk_headers += files([
  'foundry-menu-manager.h',
  'foundry-shortcut-info.h',
  'foundry-shortcut-provider.h',
])

foundry_gtk_private_sources += files([
  'foundry-shortcut-bundle.c',
  'foundry-shortcut-observer.c',
])

foundry_gtk_deps += [
  dependency('template-glib-1.0', version: '>= 3.37.0'),
]

foundry_gtk_include_directories += [include_directories('.')]
