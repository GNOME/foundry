schema_dir = join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
schema_files = []

schemas += files([
  'org.gnome.foundry.gschema.xml.in',
  'org.gnome.foundry.build.gschema.xml.in',
  'org.gnome.foundry.network.gschema.xml.in',
  'org.gnome.foundry.project.gschema.xml.in',
  'org.gnome.foundry.run.gschema.xml.in',
])

if get_option('feature-forge')
  schemas += files([
    'org.gnome.foundry.forge.gschema.xml.in',
  ])
endif

if get_option('feature-lsp')
  schemas += files([
    'org.gnome.foundry.lsp.gschema.xml.in',
  ])
endif

if get_option('feature-text')
  schemas += files([
    'org.gnome.foundry.text.gschema.xml.in',
  ])
endif

if get_option('gtk')
  schemas += files([
    'org.gnome.foundry.terminal.gschema.xml.in',
  ])
endif

foreach schema : schemas
  schema_files += [configure_file(
             copy: true,
            input: schema,
           output: '@BASENAME@',
          install: true,
      install_dir: schema_dir,
  )]
endforeach

gschemas_compiled = custom_target('gschemas.compiled',
            output: 'gschemas.compiled',
           command: ['glib-compile-schemas', meson.current_build_dir()],
      depend_files: schema_files,
  build_by_default: true,
           install: false,
)

gschema_devenv = environment()
gschema_devenv.set('GSETTINGS_SCHEMA_DIR', meson.current_build_dir())
meson.add_devenv(gschema_devenv)
