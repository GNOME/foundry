foundry_sources += files([
  'foundry-git-vcs-blame.c',
  'foundry-git-vcs-branch.c',
  'foundry-git-vcs-file.c',
  'foundry-git-vcs-remote.c',
  'foundry-git-vcs-signature.c',
  'foundry-git-vcs-tag.c',
  'foundry-git-vcs.c',
])

foundry_private_sources += files([
  'foundry-git-file-list.c',
  'foundry-git-error.c',
  'foundry-git-time.c',
])

foundry_headers += files([
  'foundry-git-vcs-blame.h',
  'foundry-git-vcs-branch.h',
  'foundry-git-vcs-file.h',
  'foundry-git-vcs-remote.h',
  'foundry-git-vcs-signature.h',
  'foundry-git-vcs-tag.h',
  'foundry-git-vcs.h',
])

foundry_deps += [
  dependency('libgit2', version: '>= 1.6'),
  dependency('libssh2'),
]

foundry_include_directories += [include_directories('.')]
