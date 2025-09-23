libfoundry_config_conf.set10('FOUNDRY_FEATURE_DAP', true)

foundry_sources += files([
  'foundry-dap-debugger.c',
  'foundry-dap-protocol.c',
])

foundry_private_sources += files([
  'foundry-dap-driver.c',
  'foundry-dap-waiter.c',
])

foundry_headers += files([
  'foundry-dap-debugger.h',
  'foundry-dap-protocol.h',
])

foundry_include_directories += [include_directories('.')]
