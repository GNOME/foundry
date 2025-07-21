if not get_option('feature-vcs')
  error('-Dfeature-git=true requires -Dfeature-vcs=true')
endif

libfoundry_config_conf.set10('FOUNDRY_FEATURE_GIT', true)

foundry_sources += files([
  'foundry-git-blame.c',
  'foundry-git-branch.c',
  'foundry-git-commit.c',
  'foundry-git-cloner.c',
  'foundry-git-file.c',
  'foundry-git-reference.c',
  'foundry-git-remote.c',
  'foundry-git-signature.c',
  'foundry-git-tag.c',
  'foundry-git-uri.c',
  'foundry-git-vcs.c',
])

foundry_private_sources += files([
  'foundry-git-file-list.c',
  'foundry-git-error.c',
  'foundry-git-repository.c',
  'foundry-git-time.c',
])

foundry_headers += files([
  'foundry-git.h',
  'foundry-git-blame.h',
  'foundry-git-branch.h',
  'foundry-git-commit.h',
  'foundry-git-cloner.h',
  'foundry-git-file.h',
  'foundry-git-reference.h',
  'foundry-git-remote.h',
  'foundry-git-signature.h',
  'foundry-git-tag.h',
  'foundry-git-uri.h',
  'foundry-git-vcs.h',
])

foundry_deps += [
  dependency('libgit2', version: '>= 1.6'),
  dependency('libssh2'),
]

foundry_include_directories += [include_directories('.')]
