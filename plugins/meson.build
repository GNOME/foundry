fallback_plugins = [
  'buildconfig',
  'content-types',
  'host-sdk',
  'hover-bridge',
  'local-device',
  'lsp-bridge',
  'no-sdk',
  'no-vcs',
  'tty-auth',
]

plugins = [
  'astro-ls',
  'autotools',
  'bash-language-server',
  'blueprint',
  'buildstream',
  'cargo',
  'clangd',
  'cmake',
  'codesearch',
  'codespell',
  'devhelp',
  'deviced',
  'doap',
  'dub',
  'elixir-ls',
  'eslint',
  'file-search',
  'flake8',
  'flatpak',
  'gdb',
  'gdiagnose',
  'gettext',
  'git',
  'glsl-language-server',
  'go',
  'gopls',
  'gradle',
  'intelephense',
  'jdtls',
  'jedi-language-server',
  'jhbuild',
  'lua-language-server',
  'make',
  'maven',
  'meson',
  'mesonlsp',
  'npm',
  'ollama',
  'phpize',
  'podman',
  'pylsp',
  'pyrefly',
  'qemu',
  'ruff',
  'rust-analyzer',
  'serve-d',
  'shellcheck',
  'sourcekit-lsp',
  'ts-language-server',
  'vala-language-server',
  'vhdl-language-server',
  'waf',
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
