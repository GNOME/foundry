if not get_option('feature-git')
  error('-Dplugin-gitlab=true requires -Dfeature-git=true')
endif

if not get_option('feature-forge')
  error('-Dplugin-gitlab=true requires -Dfeature-forge=true')
endif

foundry_private_sources += files([
  'plugin.c',
  'plugin-gitlab-error.c',
  'plugin-gitlab-forge.c',
  'plugin-gitlab-issue.c',
  'plugin-gitlab-key-rotator.c',
  'plugin-gitlab-listing.c',
  'plugin-gitlab-merge-request.c',
  'plugin-gitlab-project.c',
  'plugin-gitlab-user.c',
  'plugin-gitlab-util.c',
])

schemas += files(['app.devsuite.foundry.gitlab.gschema.xml.in'])
