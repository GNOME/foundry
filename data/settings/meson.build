schema_dir = join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
schema_files = []

schemas = [
  'app.devsuite.foundry.project.gschema.xml',
]

foreach schema : schemas
  schema_files += [configure_file(
          input: schema,
         output: schema,
           copy: true,
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
