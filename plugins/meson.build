fallback_plugins = [
  'buildconfig',
  'content-types',
  'host-sdk',
  'hover-bridge',
  'local-device',
  'no-sdk',
  'no-vcs',
  'tty-auth',
]

if get_option('feature-flatpak')
  fallback_plugins += ['flatpak']
endif

if get_option('feature-git')
  fallback_plugins += ['git']
endif

if get_option('feature-lsp')
  fallback_plugins += ['lsp-bridge']
endif

plugins = [
  'autotools',
  'buildstream',
  'cargo',
  'cmake',
  'codesearch',
  'codespell',
  'deviced',
  'doap',
  'dub',
  'eslint',
  'file-search',
  'flake8',
  'gdb',
  'gdiagnose',
  'gettext',
  'go',
  'gradle',
  'jhbuild',
  'make',
  'maven',
  'meson',
  'npm',
  'phpize',
  'podman',
  'qemu',
  'shellcheck',
  'waf',
]

doc_plugins = [
  'devhelp',
]

llm_plugins = [
  'ollama',
]

lsp_plugins = [
  'astro-ls',
  'bash-language-server',
  'blueprint',
  'clangd',
  'elixir-ls',
  'glsl-language-server',
  'gopls',
  'intelephense',
  'jdtls',
  'jedi-language-server',
  'lua-language-server',
  'mesonlsp',
  'pylsp',
  'pyrefly',
  'ruff',
  'rust-analyzer',
  'serve-d',
  'sourcekit-lsp',
  'ts-language-server',
  'vala-language-server',
  'vhdl-language-server',
  'zls',
]

plugin_manifest_xml = ''

foreach plugin: fallback_plugins
  subdir(join_paths('fallbacks', plugin))
  plugin_manifest_xml += '    <file alias="@0@/@0@.plugin">fallbacks/@0@/@0@.plugin</file>\n'.format(plugin)
endforeach

foreach plugin: plugins
  if get_option('plugin-@0@'.format(plugin))
    subdir(plugin)
    plugin_manifest_xml += '    <file>@0@/@0@.plugin</file>\n'.format(plugin)
  endif
endforeach

foreach plugin: doc_plugins
  if get_option('plugin-@0@'.format(plugin))
    if get_option('feature-docs')
      subdir(plugin)
      plugin_manifest_xml += '    <file>@0@/@0@.plugin</file>\n'.format(plugin)
    endif
  endif
endforeach

foreach plugin: llm_plugins
  if get_option('plugin-@0@'.format(plugin))
    if get_option('feature-llm')
      subdir(plugin)
      plugin_manifest_xml += '    <file>@0@/@0@.plugin</file>\n'.format(plugin)
    endif
  endif
endforeach

foreach plugin: lsp_plugins
  if get_option('plugin-@0@'.format(plugin))
    if get_option('feature-lsp')
      subdir(plugin)
      plugin_manifest_xml += '    <file>@0@/@0@.plugin</file>\n'.format(plugin)
    endif
  endif
endforeach

plugin_manifests_data = configuration_data()
plugin_manifests_data.set('XML', plugin_manifest_xml)
plugins_gresource_xml = configure_file(
          input: 'plugins.gresource.xml.in',
         output: 'plugins.gresource.xml',
  configuration: plugin_manifests_data)

foundry_private_sources += [
  gnome.compile_resources(
    'plugins-resources',
    plugins_gresource_xml,
    c_name: '_plugins',
  )
]

foundry_include_directories += [include_directories('.')]
