project('foundry', 'c',
          version: '1.0.rc',
    meson_version: '>= 1.1',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
)

cc = meson.get_compiler('c')

gnome = import('gnome')
pkgconfig = import('pkgconfig')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('GETTEXT_PACKAGE', 'foundry')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('datadir'), 'locale'))
config_h.set_quoted('LIBEXECDIR', join_paths(get_option('prefix'), get_option('libexecdir')))
add_project_arguments(['-I' + meson.project_build_root()], language: 'c')

# These default values are obtained by running /bin/sh without setting PATH
safe_path = get_option('safe_path')
if safe_path == ''
  if host_machine.system() == 'freebsd'
    safe_path = '/bin:/usr/bin:/usr/local/bin'
  elif host_machine.system() == 'dragonfly'
    safe_path = '/bin:/usr/bin:/usr/local/bin:/usr/pkg/bin'
  elif host_machine.system() == 'netbsd'
    safe_path = '/usr/bin:/bin:/usr/pkg/bin:/usr/local/bin'
  elif host_machine.system() == 'openbsd'
    safe_path = '/usr/bin:/bin:/usr/X11R6/bin:/usr/local/bin'
  else
    safe_path = '/usr/bin:/bin'
  endif
endif
config_h.set_quoted('SAFE_PATH', safe_path)

glib_req_version = '2.82'
glib_req = '>= @0@'.format(glib_req_version)
glib_dep = dependency('gio-2.0', version: glib_req)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_req)

gom_req_version = '0.5.0'
gom_req = '>= @0@'.format(gom_req_version)
gom_dep = dependency('gom-1.0', version: gom_req)

dex_req_version = '0.11'
dex_req = '>= @0@'.format(dex_req_version)
dex_dep = dependency('libdex-1', version: dex_req)

json_req_version = '1.8'
json_req = '>= @0@'.format(json_req_version)
json_dep = dependency('json-glib-1.0', version: json_req)

peas_req_version = '2.0'
peas_req = '>= @0@'.format(peas_req_version)
peas_dep = dependency('libpeas-2', version: peas_req)

sysprof_dep = dependency('sysprof-capture-4')

project_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')

# Detect and set symbol visibility
if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    config_h.set('DLL_EXPORT', true)
    if cc.get_id() == 'msvc'
      config_h.set('_FOUNDRY_EXTERN', '__declspec(dllexport) extern')
    elif cc.has_argument('-fvisibility=hidden')
      config_h.set('_FOUNDRY_EXTERN', '__attribute__((visibility("default"))) __declspec(dllexport) extern')
    endif
  elif cc.has_argument('-fvisibility=hidden')
    config_h.set('_FOUNDRY_EXTERN', '__attribute__((visibility("default"))) extern')
  endif
endif

release_args = []
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
  '-Wl,-z,noexecstack',
]
if not get_option('buildtype').startswith('debug')
  add_global_arguments('-DG_DISABLE_CAST_CHECKS', language: 'c')
  release_args += [ '-DG_DISABLE_ASSERT' ]
  test_link_args += [
    '-Wl,-Bsymbolic',
    '-fno-plt',
  ]
endif
foreach link_arg: test_link_args
  if cc.has_link_argument(link_arg)
    global_link_args += link_arg
  endif
endforeach
add_project_link_arguments(global_link_args, language: 'c')

if cc.has_function('pipe2')
  config_h.set10('HAVE_PIPE2', true)
endif

glib_major_version = glib_req_version.split('.')[0].to_int()
glib_minor_version = glib_req_version.split('.')[1].to_int()
if glib_minor_version % 2 == 1
  glib_minor_version = glib_minor_version + 1
endif
json_major_version = json_req_version.split('.')[0].to_int()
json_minor_version = json_req_version.split('.')[1].to_int()
if json_minor_version % 2 == 1
  json_minor_version = json_minor_version + 1
endif
deprecated_c_args = [
  '-DG_DISABLE_DEPRECATED',
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib_major_version, glib_minor_version),
  '-DJSON_VERSION_MIN_REQUIRED=JSON_VERSION_@0@_@1@'.format(json_major_version, json_minor_version),
]

foundry_sources = []
foundry_private_sources = []
foundry_headers = []

schemas = []

subdir('contrib')
subdir('libfoundry')
if get_option('gtk')
  subdir('libfoundry-gtk')
endif
subdir('plugins')

foundry_static = static_library('foundry-@0@'.format(api_version),
  foundry_sources + foundry_private_sources,
           dependencies: foundry_deps + foundry_static_deps,
  gnu_symbol_visibility: 'hidden',
                 c_args: foundry_c_args,
    include_directories: foundry_include_directories,
)

foundry_static_dep = declare_dependency(
           link_whole: foundry_static,
  include_directories: foundry_include_directories,
         dependencies: foundry_deps,
)

foundry_shared = library('foundry-@0@'.format(api_version),
           dependencies: foundry_deps + [foundry_static_dep],
                 c_args: foundry_c_args,
                install: true,
  gnu_symbol_visibility: 'hidden',
                version: '@0@.0.0'.format(api_version),
        darwin_versions: '@0@.0'.format(api_version.to_int() + 1),
)

foundry_shared_dep = declare_dependency(
            link_with: foundry_shared,
  include_directories: foundry_include_directories,
         dependencies: foundry_deps,
)

meson.override_dependency('foundry-@0@'.format(api_version), foundry_shared_dep)
install_headers(foundry_headers, subdir: 'libfoundry-@0@'.format(api_version))

pkgconfig.generate(
   description: 'A shared library for foundry utilities',
     libraries: foundry_shared,
          name: 'foundry',
      filebase: 'libfoundry-' + api_version,
       version: meson.project_version(),
       subdirs: 'libfoundry-@0@'.format(api_version),
      requires: ['gio-2.0', 'libdex-1', 'libpeas-2', 'json-glib-1.0'],
  extra_cflags: ['-I${libdir}/libfoundry-@0@/include'.format(api_version)],
   install_dir: get_option('libdir') / 'pkgconfig'
)

if get_option('introspection').enabled()
  foundry_gir = gnome.generate_gir(foundry_shared,
                sources: [foundry_sources, foundry_headers],
                 header: 'foundry.h',
        export_packages: 'libfoundry-' + api_version,
              nsversion: api_version,
              namespace: 'Foundry',
          symbol_prefix: 'foundry',
      identifier_prefix: 'Foundry',
               includes: ['Gio-2.0', 'Dex-1', 'Peas-2', 'Json-1.0'],
                install: true,
  )
endif

if get_option('gtk')
  foundry_gtk = library('foundry-gtk-@0@'.format(api_version),
    foundry_gtk_sources + foundry_gtk_private_sources,
             dependencies: foundry_gtk_deps + [foundry_shared_dep],
                   c_args: foundry_c_args,
                  install: true,
    gnu_symbol_visibility: 'hidden',
                  version: '@0@.0.0'.format(api_version),
          darwin_versions: '@0@.0'.format(api_version.to_int() + 1),
      include_directories: foundry_gtk_include_directories,
  )

  foundry_gtk_dep = declare_dependency(
              link_with: foundry_gtk,
    include_directories: [foundry_include_directories, foundry_gtk_include_directories],
           dependencies: foundry_gtk_deps,
  )

  meson.override_dependency('foundry-gtk-@0@'.format(api_version), foundry_gtk_dep)
  install_headers(foundry_gtk_headers, subdir: 'libfoundry-gtk-@0@'.format(api_version))

  pkgconfig.generate(
    description: 'Integrates libfoundry into GTK applications',
      libraries: [foundry_gtk],
           name: 'foundry-gtk',
       filebase: 'libfoundry-gtk-' + api_version,
        version: meson.project_version(),
        subdirs: 'libfoundry-gtk-@0@'.format(api_version),
       requires: ['gio-2.0', 'libdex-1', 'libpeas-2', 'json-glib-1.0', 'gtk4', 'gtksourceview-5'],
    install_dir: get_option('libdir') / 'pkgconfig'
  )

  if get_option('introspection').enabled()
    foundry_gtk_gir = gnome.generate_gir(foundry_gtk,
                  sources: [foundry_gtk_sources, foundry_gtk_headers],
                   header: 'foundry-gtk.h',
          export_packages: 'libfoundry-gtk-' + api_version,
                nsversion: api_version,
                namespace: 'FoundryGtk',
            symbol_prefix: 'foundry',
        identifier_prefix: 'Foundry',
                 includes: ['Gio-2.0', 'Dex-1', 'Peas-2', 'Json-1.0', 'Gtk-4.0', 'GtkSource-5', foundry_gir[0]],
                  install: true,
      include_directories: include_directories('.'),
    )
  endif
endif

foundry_exe = executable('foundry', 'foundry.c',
  dependencies: [foundry_static_dep],
        c_args: foundry_c_args,
       install: true,
)

subdir('data')
subdir('testsuite')

if get_option('docs')
 subdir('doc')
endif

configure_file(output: 'config.h', configuration: config_h)

gnome.post_install(
     glib_compile_schemas: true,
    gtk_update_icon_cache: false,
  update_desktop_database: false,
)
