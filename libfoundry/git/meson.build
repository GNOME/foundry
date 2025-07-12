foundry_sources += files([
  'foundry-git-blame.c',
  'foundry-git-branch.c',
  'foundry-git-file.c',
  'foundry-git-reference.c',
  'foundry-git-remote.c',
  'foundry-git-signature.c',
  'foundry-git-tag.c',
  'foundry-git-vcs.c',
])

foundry_private_sources += files([
  'foundry-git-file-list.c',
  'foundry-git-error.c',
  'foundry-git-time.c',
])

foundry_headers += files([
  'foundry-git-blame.h',
  'foundry-git-branch.h',
  'foundry-git-file.h',
  'foundry-git-reference.h',
  'foundry-git-remote.h',
  'foundry-git-signature.h',
  'foundry-git-tag.h',
  'foundry-git-vcs.h',
])

foundry_deps += [
  dependency('libgit2', version: '>= 1.6'),
  dependency('libssh2'),
]

foundry_include_directories += [include_directories('.')]
