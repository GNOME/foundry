lib_testsuite_tools = {
  'test-auth-prompt' : {},
  'test-diagnostics' : {},
}

if get_option('feature-text') and get_option('gtk')
  executable('test-view', ['test-view.c'],
                 c_args: lib_testsuite_c_args,
           dependencies: lib_testsuite_deps + foundry_gtk_dep,
    include_directories: [include_directories('..'), include_directories('.')],
  )
endif

foreach test, params: lib_testsuite_tools
  tool_exe = executable(test, ['@0@.c'.format(test)],
                 c_args: lib_testsuite_c_args,
           dependencies: lib_testsuite_deps,
    include_directories: [include_directories('..'), include_directories('.')],
  )
endforeach

if get_option('plugin-ctags')
  executable('test-ctags', ['test-ctags.c'],
    dependencies: [foundry_static_dep],
  )
endif
