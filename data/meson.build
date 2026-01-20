subdir('bash-completion')
subdir('settings')

appstream_file = i18n.merge_file(
        input: 'org.gnome.Foundry.metainfo.xml.in',
       output: 'org.gnome.Foundry.metainfo.xml',
       po_dir: '../po',
      install: true,
  install_dir: get_option('datadir') / 'metainfo'
)

appstreamcli = find_program('appstreamcli', required: false, disabler: true)
test('Validate appstream file', appstreamcli,
     args: ['validate', '--no-net', '--explain', appstream_file])
