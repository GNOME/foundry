fallback_plugins = [
  'buildconfig',
  'content-types',
  'host-sdk',
  'local-device',
  'no-sdk',
  'no-vcs',
]

plugins = [
  'codespell',
  'deviced',
  'flatpak',
  'gdiagnose',
  'git',
  'jhbuild',
  'meson',
  'podman',
]

plugin_manifest_xml = ''

foreach plugin: fallback_plugins
  subdir(join_paths('fallbacks', plugin))
  plugin_manifest_xml += '    <file alias="@0@/@0@.plugin">fallbacks/@0@/@0@.plugin</file>\n'.format(plugin)
endforeach

foreach plugin: plugins
  if get_option('plugin-@0@'.format(plugin))
    subdir(plugin)
    plugin_manifest_xml += '    <file>@0@/@0@.plugin</file>\n'.format(plugin)
  endif
endforeach

plugin_manifests_data = configuration_data()
plugin_manifests_data.set('XML', plugin_manifest_xml)
plugins_gresource_xml = configure_file(
          input: 'plugins.gresource.xml.in',
         output: 'plugins.gresource.xml',
  configuration: plugin_manifests_data)

foundry_private_sources += [
  gnome.compile_resources(
    'plugins-resources',
    plugins_gresource_xml,
    c_name: '_plugins',
  )
]

foundry_include_directories += [include_directories('.')]
