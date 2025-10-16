foundry_private_sources += files([
  'plugin.c',
  'plugin-cursor-client.c',
  'plugin-cursor-llm-model.c',
  'plugin-cursor-llm-provider.c',
])

foundry_deps += [
  dependency('libsoup-3.0'),
]

schemas += files(['app.devsuite.foundry.cursor.gschema.xml.in'])
