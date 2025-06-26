lib_testsuite_tools = {
  'test-auth-prompt' : {},
}

foreach test, params: lib_testsuite_tools
  tool_exe = executable(test, ['@0@.c'.format(test)],
                 c_args: lib_testsuite_c_args,
           dependencies: lib_testsuite_deps,
    include_directories: [include_directories('..'), include_directories('.')],
  )
endforeach
