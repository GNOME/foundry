lib_test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/data/settings'.format(meson.project_build_root()),
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
  'test-file' : {},
  'test-simple-text-buffer' : {},
  'test-settings' : {},
  'test-flatpak-builder-manifest' : {},
  'test-json' : {},
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
