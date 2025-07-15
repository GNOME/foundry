if not get_option('feature-dap')
  error('GDB plugin requires -Ddap=true')
endif

foundry_private_sources += files([
  'plugin.c',
  'plugin-gdb-attach-request.c',
  'plugin-gdb-debugger.c',
  'plugin-gdb-debugger-provider.c',
  'plugin-gdb-launch-request.c',
])
