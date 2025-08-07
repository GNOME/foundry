foundry_private_sources += files([
  'plugin.c',
  'plugin-meson-project-template.c',
  'plugin-meson-template-locator.c',
  'plugin-meson-template-provider.c',
])

flatpak_json_data = configuration_data()
flatpak_json_data.set('gnome_sdk_version', get_option('gnome_sdk_version'))
flatpak_json_data.set('freedesktop_sdk_version', get_option('freedesktop_sdk_version'))
flatpak_json = configure_file(
  input: 'resources/flatpak.json.in',
  output: 'flatpak.json',
  configuration: flatpak_json_data)

foundry_private_sources += [
  gnome.compile_resources(
    'plugin-meson-templates-resources',
    'meson-templates.gresource.xml',
    c_name: '_plugin_meson_templates',
    dependencies: [flatpak_json],
  )
]
