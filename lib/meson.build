api_version = '1'

foundry_debug_conf = configuration_data()
foundry_debug_conf.set10('ENABLE_TRACING', get_option('development'))
foundry_debug_conf.set('BUGREPORT_URL', 'https://gitlab.gnome.org/chergert/foundry/issues')
foundry_debug_h = configure_file(
          input: 'foundry-debug.h.in',
         output: 'foundry-debug.h',
  configuration: foundry_debug_conf,
        install: true,
    install_dir: get_option('includedir') / 'libfoundry-@0@'.format(api_version)
)

foundry_sources += files([
  'foundry-context.c',
  'foundry-contextual.c',
  'foundry-inhibitor.c',
  'foundry-init.c',
  'foundry-service.c',
  'foundry-util.c',
])

foundry_private_sources += files([
  'foundry-action-muxer.c',
])

foundry_private_sources += [
  gnome.gdbus_codegen('foundry-ipc',
             sources: 'app.devsuite.foundry.xml',
    interface_prefix: 'app.devsuite.foundry.',
           namespace: 'FoundryIpc',
  ),

  gnome.compile_resources('foundry-resources', 'foundry.gresource.xml',
    c_name: '_foundry',
  ),

  gnome.genmarshal('foundry-marshal',
              internal: true,
                prefix: 'foundry_marshal',
               sources: ['foundry-marshal.defs'],
    valist_marshallers: true,
  ),
]

foundry_headers += files([
  'foundry.h',
  'foundry-context.h',
  'foundry-contextual.h',
  'foundry-inhibitor.h',
  'foundry-init.h',
  'foundry-service.h',
  'foundry-types.h',
  'foundry-util.h',
  'foundry-version-macros.h',
])

version_split = meson.project_version().split('.')
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
if version_split[2][0] in ['a','b','r']
  version_conf.set('MICRO_VERSION', 0)
else
  version_conf.set('MICRO_VERSION', version_split[2])
endif

configure_file(
          input: 'foundry-version.h.in',
         output: 'foundry-version.h',
  configuration: version_conf,
        install: true,
    install_dir: get_option('includedir') / 'libfoundry-@0@'.format(api_version)
)

foundry_deps = [
  glib_dep,
  gio_unix_dep,
  gom_dep,
  dex_dep,
  json_dep,
  jsonrpc_dep,
  peas_dep,
  sysprof_dep,
]

foundry_static_deps = [
  contrib_static_dep,
]

foundry_c_args = [
  '-DFOUNDRY_COMPILATION',
  '-DG_LOG_DOMAIN="Foundry"',
]

foundry_include_directories = [
  include_directories('.'),
  include_directories('..' / 'contrib'),
]

subdir('build')
subdir('cli')
subdir('code-action')
subdir('commands')
subdir('completion')
subdir('config')
subdir('dap')
subdir('dependencies')
subdir('diagnostics')
subdir('debugger')
subdir('device')
subdir('documentation')
subdir('editing')
subdir('files')
subdir('flatpak')
subdir('jsonrpc')
subdir('logging')
subdir('lsp')
subdir('markup')
subdir('operations')
subdir('platform')
subdir('plugins')
subdir('reflection')
subdir('run')
subdir('sdk')
subdir('search')
subdir('settings')
subdir('vcs')

if get_option('gtk')
  subdir('gtk')
endif
