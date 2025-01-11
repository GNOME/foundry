if not get_option('plugin-flatpak')
  error('plugin-deviced requires plugin-flatpak')
endif

foundry_deps += [
  dependency('libdeviced', version: '>= 3.27.4'),
]

foundry_private_sources += files([
  'plugin.c',
  'plugin-deviced-device.c',
  'plugin-deviced-device-provider.c',
])
