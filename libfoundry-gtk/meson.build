foundry_gtk_include_directories = [include_directories('.')]

foundry_gtk_sources = files([
  'foundry-gtk-init.c',
])

foundry_gtk_headers = files([
  'foundry-gtk.h',
  'foundry-gtk-init.h',
])

foundry_gtk_private_sources = [
  gnome.compile_resources('foundry-gtk-resources', 'foundry-gtk.gresource.xml',
    c_name: '_foundry_gtk',
  ),
]

foundry_gtk_private_sources += files([
  'foundry-gtk-plugin.c',
])

foundry_gtk_deps = [
  dependency('gtksourceview-5', version: '>= 5.14'),
  dependency('libspelling-1', version: '>= 0.4'),
]

subdir('completion')
subdir('editing')
subdir('shortcuts')
