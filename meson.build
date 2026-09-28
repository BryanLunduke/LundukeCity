project(
  'lunduke-city',
  'cpp',
  version: '0.3.0',
  default_options: ['cpp_std=c++17', 'warning_level=2'],
  meson_version: '>= 0.60',
)

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
# Icon=lunduke-city. Same hicolor layout as Lunduke Paint.
install_data(
  'data/icons/hicolor/scalable/apps/lunduke-city.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
install_data(
  'data/icons/hicolor/16x16/apps/lunduke-city.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '16x16' / 'apps',
)
install_data(
  'data/icons/hicolor/32x32/apps/lunduke-city.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '32x32' / 'apps',
)
install_data(
  'data/icons/hicolor/256x256/apps/lunduke-city.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '256x256' / 'apps',
)
install_subdir(
  'third_party/micropolis-assets',
  install_dir: get_option('datadir') / 'lunduke-city',
)
install_data(
  'NOTICE',
  'COPYING',
  install_dir: get_option('datadir') / 'doc' / 'lunduke-city',
)
