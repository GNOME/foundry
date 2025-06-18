foundry_private_sources += files([
  'plugin.c',
  'plugin-git-build-addin.c',
  'plugin-git-file-list.c',
  'plugin-git-vcs.c',
  'plugin-git-vcs-blame.c',
  'plugin-git-vcs-file.c',
  'plugin-git-vcs-provider.c',
  'plugin-git-error.c',
])

foundry_deps += [
  dependency('libgit2', version: '>= 1.6'),
]
