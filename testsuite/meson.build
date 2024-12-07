lib_test_env = [
  'G_DEBUG=gc-friendly',
  'MALLOC_CHECK_=2',
]

lib_testsuite_c_args = [
  '-DG_ENABLE_DEBUG',
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
]

lib_testsuite = {
  'test-cli-command' : {},
  'test-future-list-model' : {},
  'test-simple-text-buffer' : {},
}

lib_testsuite_deps = [
  foundry_static_dep,
]

foreach test, params: lib_testsuite
  test_exe = executable(test, ['@0@.c'.format(test)],
                 c_args: lib_testsuite_c_args,
           dependencies: lib_testsuite_deps,
    include_directories: [include_directories('..'), include_directories('.')],
  )
  if not params.get('skip', false)
    test(test, test_exe, env: lib_test_env)
  endif
endforeach
