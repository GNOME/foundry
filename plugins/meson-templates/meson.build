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

gtk4_project_conf = configuration_data()
gtk4_project_conf.set('is_adwaita', 0)
gtk4_project = configure_file(
          input: 'gtk4.project.in',
         output: 'gtk4.project',
  configuration: gtk4_project_conf,
)

gnome_project_conf = configuration_data()
gnome_project_conf.set('is_adwaita', 1)
gnome_project = configure_file(
          input: 'gtk4.project.in',
         output: 'gnome.project',
  configuration: gnome_project_conf,
)

foundry_private_sources += [
  gnome.compile_resources(
    'plugin-meson-templates-resources',
    'meson-templates.gresource.xml',
    c_name: '_plugin_meson_templates',
    dependencies: [flatpak_json, gnome_project, gtk4_project],
  )
]
