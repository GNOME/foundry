if not get_option('feature-text')
  error('-Dgtk=true requires -Dfeature-text=true')
endif

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

if get_option('commonmark')
  libcmark_dep = dependency('libcmark', version: '>= 0.29.0')
else
  libcmark_dep = disabler()
endif

if get_option('webkit')
  webkit_dep = dependency('webkitgtk-6.0')
else
  webkit_dep = disabler()
endif

config_h.set10('HAVE_WEBKIT', webkit_dep.found())
config_h.set10('HAVE_CMARK', libcmark_dep.found())

foundry_gtk_deps = [
  dependency('gtksourceview-5', version: '>= 5.14'),
  dependency('vte-2.91-gtk4', version: '>= 0.80'),

  libcmark_dep,
  webkit_dep,
]

subdir('editing')
subdir('markup')
subdir('platform')
subdir('shortcuts')
subdir('terminal')
