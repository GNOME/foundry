if not get_option('feature-debugger')
  error('-Dfeature-dap=true requires -Dfeature-debugger=true')
endif

libfoundry_config_conf.set10('FOUNDRY_FEATURE_DAP', true)

foundry_sources += files([
  'foundry-dap-client.c',
  'foundry-dap-debugger.c',
  'foundry-dap-event.c',
  'foundry-dap-initialize-request.c',
  'foundry-dap-output-event.c',
  'foundry-dap-protocol-message.c',
  'foundry-dap-request.c',
  'foundry-dap-response.c',
  'foundry-dap-unknown-event.c',
  'foundry-dap-unknown-request.c',
  'foundry-dap-unknown-response.c',
])

foundry_private_sources += files([
  'foundry-dap-input-stream.c',
  'foundry-dap-output-stream.c',
  'foundry-dap-waiter.c',
])

foundry_headers += files([
  'foundry-dap.h',
  'foundry-dap-client.h',
  'foundry-dap-debugger.h',
  'foundry-dap-event.h',
  'foundry-dap-initialize-request.h',
  'foundry-dap-output-event.h',
  'foundry-dap-protocol-message.h',
  'foundry-dap-request.h',
  'foundry-dap-response.h',
  'foundry-dap-unknown-event.h',
  'foundry-dap-unknown-request.h',
  'foundry-dap-unknown-response.h',
])

foundry_include_directories += [include_directories('.')]
