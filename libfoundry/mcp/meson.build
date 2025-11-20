libfoundry_config_conf.set10('FOUNDRY_FEATURE_MCP', true)

if not get_option('feature-llm')
  error('-Dfeature-mcp=true requires -Dfeature-llm=true')
endif

foundry_sources += files([
  'foundry-mcp-server.c',
])

foundry_headers += files([
  'foundry-mcp-server.h',
])

foundry_include_directories += [include_directories('.')]
