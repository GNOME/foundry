lib_testsuite_tools = {
  'test-auth-prompt' : {},
  'list-palettes' : {'gtk': true},
  'list-tweaks' : {'gtk': true},
  'test-view' : {'gtk': true, 'text': true},
  'test-terminal' : {'gtk': true},
  'test-diagnostics' : {'text': true},
  'test-build-manager-gtk' : {'gtk': true},
  'test-chat' : {'llm': true, 'gtk': true},
}

foreach test, params: lib_testsuite_tools
  tool_deps = [lib_testsuite_deps]
  if params.get('gtk', false)
    if not get_option('gtk')
      continue
    endif
    tool_deps += [foundry_gtk_dep, dependency('gtk4')]
  endif
  if params.get('text', false) and not get_option('feature-text')
    continue
  endif
  if params.get('llm', false) and not get_option('feature-llm')
    continue
  endif
  tool_exe = executable(test, ['@0@.c'.format(test)],
                 c_args: lib_testsuite_c_args,
           dependencies: tool_deps,
    include_directories: [include_directories('..'), include_directories('.')],
  )
endforeach

if get_option('plugin-ctags') and get_option('feature-text')
  executable('test-ctags', ['test-ctags.c'], dependencies: [foundry_static_dep])
  executable('test-ctags-builder', ['test-ctags-builder.c'], dependencies: [foundry_static_dep])
endif
