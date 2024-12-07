subdir('host')
subdir('jhbuild')
subdir('local-device')

foundry_private_sources += [
  gnome.compile_resources(
    'plugins-resources',
    'plugins.gresource.xml',
    c_name: '_plugins',
  )
]

foundry_include_directories += [include_directories('.')]
