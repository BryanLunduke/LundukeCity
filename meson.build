project(
  'lunduke-city',
  'cpp',
  version: '0.9.7',
  default_options: ['cpp_std=c++17', 'warning_level=2'],
  meson_version: '>= 0.60',
)

# Debian package version. The Meson project version is 0.9.7.
lunduke_package_version = '0.9.7-1'

gtkmm_dep = dependency('gtkmm-3.0')
thread_dep = dependency('threads')
lunduke_asset_dir = meson.project_source_root() / 'third_party' / 'micropolis-assets'

subdir('third_party/micropolis')
subdir('src')
subdir('tests')

install_data(
  'data/lunduke-city.desktop',
  install_dir: get_option('datadir') / 'applications',
)
# C-4 (round 15): AppStream metadata.
install_data(
  'data/com.lunduke.LundukeCity.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo',
)
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test(
    'metainfo',
    appstreamcli,
    args: ['validate', '--no-net', files('data/com.lunduke.LundukeCity.metainfo.xml')],
  )
endif
install_data(
  'data/lunduke-city.xml',
  install_dir: get_option('datadir') / 'mime' / 'packages',
)
# Icon=lunduke-city. Scalable SVG plus hicolor PNGs.
install_data(
  'data/lunduke-city.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
foreach size : ['16', '32', '48', '64', '128', '256']
  install_data(
    'data/icons/hicolor/' + size + 'x' + size + '/apps/lunduke-city.png',
    install_dir: get_option('datadir') / 'icons' / 'hicolor' / (size + 'x' + size) / 'apps',
  )
endforeach
install_subdir(
  'third_party/micropolis-assets',
  install_dir: get_option('datadir') / 'lunduke-city',
)
install_data(
  'NOTICE',
  'COPYING',
  install_dir: get_option('datadir') / 'doc' / 'lunduke-city',
)
