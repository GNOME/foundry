# Unified plugin dictionary structure
# Each plugin has:
# - options: array of feature options that must be enabled
# - library: 'gtk' for libfoundry-gtk, 'adw' for libfoundry-adw, omitted for libfoundry
# - fallback: true if this is a fallback plugin (always built)

plugins_dict = {
  # Fallback plugins (always built)
  'buildconfig': {'fallback': true},
  'content-types': {'fallback': true},
  'doc-bridge': {'fallback': true, 'options': ['feature-docs']},
  'flatpak': {'fallback': true, 'options': ['feature-flatpak']},
  'git': {'fallback': true, 'options': ['feature-git']},
  'grep': {'fallback': true},
  'gsettings': {'fallback': true, 'options': ['feature-text']},
  'host-sdk': {'fallback': true},
  'hover-bridge': {'fallback': true, 'options': ['feature-text']},
  'internal-tweaks': {'fallback': true},
  'language-defaults': {'fallback': true, 'options': ['feature-text']},
  'linked-workspace': {'fallback': true},
  'llm-build': {'fallback': true, 'options': ['feature-llm']},
  'local-device': {'fallback': true},
  'lsp-bridge': {'fallback': true, 'options': ['feature-lsp']},
  'no-sdk': {'fallback': true},
  'no-vcs': {'fallback': true, 'options': ['feature-vcs']},
  'plugin-tweaks': {'fallback': true},
  'tty-auth': {'fallback': true},

  # Core plugins (no special requirements)
  'autotools': {},
  'buildstream': {},
  'cargo': {},
  'cmake': {},
  'codesearch': {},
  'codespell': {},
  'deviced': {},
  'doap': {},
  'dub': {},
  'eslint': {},
  'file-search': {},
  'flake8': {},
  'gdiagnose': {},
  'gettext': {},
  'go': {},
  'gradle': {},
  'jhbuild': {},
  'just': {},
  'make': {},
  'maven': {},
  'meson': {},
  'npm': {},
  'phpize': {},
  'podman': {},
  'qemu': {},
  'shellcheck': {},
  'sarif': {},
  'waf': {},

  # Documentation plugins
  'devhelp': {'options': ['feature-docs']},

  # LLM plugins
  'ollama': {'options': ['feature-llm']},

  # Forge plugins
  'gitlab': {'options': ['feature-forge']},

  # LSP plugins
  'astro-ls': {'options': ['feature-lsp']},
  'bash-language-server': {'options': ['feature-lsp']},
  'blueprint': {'options': ['feature-lsp']},
  'clangd': {'options': ['feature-lsp']},
  'elixir-ls': {'options': ['feature-lsp']},
  'glsl-language-server': {'options': ['feature-lsp']},
  'gopls': {'options': ['feature-lsp']},
  'intelephense': {'options': ['feature-lsp']},
  'jdtls': {'options': ['feature-lsp']},
  'jedi-language-server': {'options': ['feature-lsp']},
  'lua-language-server': {'options': ['feature-lsp']},
  'mesonlsp': {'options': ['feature-lsp']},
  'pylsp': {'options': ['feature-lsp']},
  'pyrefly': {'options': ['feature-lsp']},
  'ruff': {'options': ['feature-lsp']},
  'rust-analyzer': {'options': ['feature-lsp']},
  'serve-d': {'options': ['feature-lsp']},
  'sourcekit-lsp': {'options': ['feature-lsp']},
  'ts-language-server': {'options': ['feature-lsp']},
  'ty': {'options': ['feature-lsp']},
  'vala-language-server': {'options': ['feature-lsp']},
  'vhdl-language-server': {'options': ['feature-lsp']},
  'zls': {'options': ['feature-lsp']},

  # Text plugins
  'ctags': {'options': ['feature-text']},
  'editorconfig': {'options': ['feature-text']},
  'modelines': {'options': ['feature-text']},

  # Template plugins
  'make-templates': {'options': ['feature-templates']},
  'meson-templates': {'options': ['feature-templates']},
  'simple-templates': {'options': ['feature-templates']},

  # Debugger plugins (require both debugger and dap features)
  'gdb': {'options': ['feature-debugger', 'feature-dap']},
  'lldb': {'options': ['feature-debugger', 'feature-dap']},

  # GTK plugins (go into libfoundry-gtk)
  'spellcheck': {'library': 'gtk'},
  'word-completion': {'library': 'gtk'},
}

foundry_plugin_manifest_xml = ''

foreach plugin, params: plugins_dict
  # Only plugins that do not have a library specified
  if params.has_key('library')
    continue
  endif

  # Check if plugin option is enabled (skip for fallback plugins)
  is_fallback = params.get('fallback', false)
  if not is_fallback
    if not get_option('plugin-@0@'.format(plugin))
      continue
    endif
  endif

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

  # Build the plugin
  if is_fallback
    subdir(join_paths('fallbacks', plugin))
    foundry_plugin_manifest_xml += '    <file alias="@0@/@0@.plugin">fallbacks/@0@/@0@.plugin</file>\n'.format(plugin)
  else
    subdir(plugin)
    foundry_plugin_manifest_xml += '    <file>@0@/@0@.plugin</file>\n'.format(plugin)
  endif

  config_h.set10('HAVE_PLUGIN_@0@'.format(plugin.replace('-','_').to_upper()), true)
endforeach

foundry_plugin_manifests_data = configuration_data()
foundry_plugin_manifests_data.set('XML', foundry_plugin_manifest_xml)
foundry_plugins_gresource_xml = configure_file(
          input: 'plugins.gresource.xml.in',
         output: 'foundry-plugins.gresource.xml',
  configuration: foundry_plugin_manifests_data)

foundry_private_sources += [
  gnome.compile_resources(
    'foundry-plugins-resources',
    foundry_plugins_gresource_xml,
    c_name: '_foundry_plugins',
  )
]

foundry_include_directories += [include_directories('.')]

if get_option('gtk')
  foundry_gtk_plugin_manifest_xml = ''

  foreach plugin, params: plugins_dict
    # Only process GTK plugins
    if params.get('library', '') != 'gtk'
      continue
    endif

    # Check if plugin option is enabled
    if not get_option('plugin-@0@'.format(plugin))
      continue
    endif

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

    subdir(plugin)
    foundry_gtk_plugin_manifest_xml += '    <file>@0@/@0@.plugin</file>\n'.format(plugin)
  endforeach

  foundry_gtk_plugin_manifests_data = configuration_data()
  foundry_gtk_plugin_manifests_data.set('XML', foundry_gtk_plugin_manifest_xml)
  foundry_gtk_plugins_gresource_xml = configure_file(
            input: 'plugins.gresource.xml.in',
           output: 'foundry-gtk-plugins.gresource.xml',
    configuration: foundry_gtk_plugin_manifests_data)

  foundry_gtk_private_sources += [
    gnome.compile_resources(
      'foundry-gtk-plugins-resources',
      foundry_gtk_plugins_gresource_xml,
      c_name: '_foundry_gtk_plugins',
    )
  ]

  foundry_gtk_include_directories += [include_directories('.')]
endif
