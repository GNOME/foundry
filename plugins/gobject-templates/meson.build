if not get_option('plugin-meson-templates')
  error('plugin-gobject-templates requires -Dplugin-meson-tempaltes=true')
endif

foundry_private_sources += files([
  'plugin.c',
  'plugin-gobject-code-template.c',
  'plugin-gobject-template-provider.c',
])

foundry_private_sources += [
  gnome.compile_resources(
    'plugin-gobject-templates-resources',
    'gobject-templates.gresource.xml',
    c_name: '_plugin_gobject_templates',
  )
]
