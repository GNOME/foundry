foundry_gtk_sources = files([
  'foundry-gtk-init.c',
  'foundry-source-buffer.c',
  'foundry-source-completion-provider.c',
  'foundry-source-language-guesser.c',
  'foundry-source-view.c',
])

foundry_gtk_headers = files([
  'foundry-gtk.h',
  'foundry-gtk-init.h',
  'foundry-source-buffer.h',
  'foundry-source-completion-provider.h',
  'foundry-source-language-guesser.h',
  'foundry-source-view.h',
])

foundry_gtk_private_sources = files([
  'foundry-source-buffer-provider.c',
  'foundry-gtk-plugin.c',
])

foundry_gtk_private_sources += [
  gnome.compile_resources('foundry-gtk-resources', 'foundry-gtk.gresource.xml',
    c_name: '_foundry_gtk',
  ),
]

foundry_gtk_deps = [
  dependency('gtksourceview-5', version: '>= 5.14'),
  dependency('libspelling-1', version: '>= 0.4'),
]
