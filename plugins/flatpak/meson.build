foundry_private_sources += files([
  'plugin.c',
  'plugin-flatpak.c',
  'plugin-flatpak-build-addin.c',
  'plugin-flatpak-config-provider.c',
  'plugin-flatpak-json-manifest.c',
  'plugin-flatpak-manifest.c',
  'plugin-flatpak-prepare-stage.c',
  'plugin-flatpak-sdk.c',
  'plugin-flatpak-sdk-install.c',
  'plugin-flatpak-sdk-provider.c',
  'plugin-flatpak-util.c',
])

foundry_deps += [
  dependency('flatpak'),
]

schemas += files(['app.devsuite.foundry.flatpak.gschema.xml.in'])
