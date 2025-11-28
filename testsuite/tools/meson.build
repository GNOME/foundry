readline_dep = dependency('readline', required: false)
if not readline_dep.found()
  readline_dep = disabler()
endif

# Unified tool dictionary structure
# Each tool has:
# - options: array of feature options that must be enabled (including 'gtk')
# - plugins: array of plugin options that must be enabled
# - sources: array of additional source files
# - dependencies: array of additional dependencies
# - gresource: true if this tool needs gresource compilation

tools_dict = {
  # Core tools (no special requirements)
  'gir-dump': {},
  'test-auth-prompt': {},

  # GTK tools
  'list-palettes': {'options': ['gtk']},
  'list-tweaks': {'options': ['gtk']},
  'test-build-manager-gtk': {'options': ['gtk']},
  'test-file-search-gtk': {'options': ['gtk']},
  'test-terminal': {'options': ['gtk']},

  # GTK + Text feature tools
  'test-view': {'options': ['gtk', 'feature-text']},

  # Text feature tools
  'test-diagnostics': {'options': ['feature-text']},

  # GTK + Forge feature tools
  'test-forge-issues-gtk': {'options': ['gtk', 'feature-forge']},

  # GTK + LLM feature tools
  'test-chat': {'options': ['gtk', 'feature-llm']},

  # Git feature tools
  'test-git-monitor': {'options': ['feature-git']},
  'print-project-diff': {'options': ['feature-git']},
  'print-simple-diff': {'options': ['feature-git']},

  # GTK + Git feature tools
  'test-git-commit-builder-gtk': {'options': ['gtk', 'feature-git']},
  'test-vcs-clone-gtk': {'options': ['gtk', 'feature-git']},

  # DAP + Debugger feature tools
  'test-dap': {'options': ['feature-dap', 'feature-debugger']},
  'fdb': {
    'options': ['feature-dap', 'feature-debugger'],
    'sources': ['egg-line.c'],
    'dependencies': [readline_dep],
  },

  # GTK + Debugger feature tools
  'test-debugger-gtk': {
    'options': ['gtk', 'feature-debugger'],
    'gresource': true,
  },

  # GTK
  'test-progress-icon': {
    'options': ['gtk'],
    'sources': ['../../libfoundry-adw/foundry-progress-icon.c'],
  },

  # Flatpak feature tools
  'test-flatpak-builder-serialize': {'options': ['feature-flatpak']},

  # CTags plugin tools
  'test-ctags': {'plugins': ['plugin-ctags'], 'options': ['feature-text']},
  'test-ctags-builder': {'plugins': ['plugin-ctags'], 'options': ['feature-text']},
}

foreach tool, params: tools_dict
  # Check if all required feature options are enabled
  required_options = params.get('options', [])
  has_all_options = true
  foreach option: required_options
    if not get_option(option)
      has_all_options = false
      break
    endif
  endforeach

  if not has_all_options
    continue
  endif

  # Check if all required plugin options are enabled
  required_plugins = params.get('plugins', [])
  has_all_plugins = true
  foreach plugin: required_plugins
    if not get_option(plugin)
      has_all_plugins = false
      break
    endif
  endforeach

  if not has_all_plugins
    continue
  endif

  # Build dependencies
  tool_deps = [lib_testsuite_deps]
  if 'gtk' in required_options
    tool_deps += [foundry_gtk_dep, dependency('gtk4')]
    if gsv_dep.found()
      tool_deps += [gsv_dep]
    endif
  endif
  if 'adwaita' in required_options
    tool_deps += [foundry_adw_dep]
  endif

  # Add extra dependencies
  extra_deps = params.get('dependencies', [])
  foreach dep: extra_deps
    if dep.found()
      tool_deps += [dep]
    endif
  endforeach

  # Handle gresource compilation
  sources = params.get('sources', [])
  if params.get('gresource', false)
    gresource = gnome.compile_resources('@0@-resources'.format(tool),
                                        '@0@.gresource.xml'.format(tool),
                                        source_dir: '.',
                                        c_name: tool.replace('-', '_'))
    sources += gresource
  endif

  # Build the tool
  include_dirs = [include_directories('..'), include_directories('.')]
  if 'adwaita' in required_options
    include_dirs += [include_directories('../libfoundry-adw')]
  endif
  executable(tool, ['@0@.c'.format(tool)] + sources,
             c_args: lib_testsuite_c_args,
             dependencies: tool_deps,
             include_directories: include_dirs,
  )
endforeach
