engine_smoke = executable(
  'engine_smoke',
  'engine_smoke.cpp',
  dependencies: [micropolis_dep],
)

test('engine_smoke', engine_smoke)

engine_fixes = executable(
  'engine_fixes',
  'engine_fixes.cpp',
  dependencies: [micropolis_dep],
)

test('engine_fixes', engine_fixes)

features = executable(
  'features',
  'features.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('features', features)

regressions = executable(
  'regressions',
  'regressions.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('regressions', regressions)

round3 = executable(
  'round3',
  'round3.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('round3', round3, timeout: 120)

ui_round3 = executable(
  'ui_round3',
  'ui_round3.cpp',
  '../src/about_dialog.cpp',
  '../src/app_window.cpp',
  '../src/assets.cpp',
  '../src/budget_window.cpp',
  '../src/city_session.cpp',
  '../src/evaluation_window.cpp',
  '../src/graphs_window.cpp',
  '../src/map_view.cpp',
  '../src/messages.cpp',
  '../src/new_city_dialog.cpp',
  '../src/overlay_window.cpp',
  '../src/side_widgets.cpp',
  '../src/sound_player.cpp',
  '../src/sprite_art.cpp',
  '../src/tile_atlas.cpp',
  '../src/tool_palette.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [gtkmm_dep, micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('ui_round3', ui_round3, timeout: 180)

round4 = executable(
  'round4',
  'round4.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('round4', round4, timeout: 180)

round5 = executable(
  'round5',
  'round5.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('round5', round5, timeout: 180)

ui_round4 = executable(
  'ui_round4',
  'ui_round4.cpp',
  '../src/about_dialog.cpp',
  '../src/app_window.cpp',
  '../src/assets.cpp',
  '../src/budget_window.cpp',
  '../src/city_session.cpp',
  '../src/evaluation_window.cpp',
  '../src/graphs_window.cpp',
  '../src/map_view.cpp',
  '../src/messages.cpp',
  '../src/new_city_dialog.cpp',
  '../src/overlay_window.cpp',
  '../src/side_widgets.cpp',
  '../src/sound_player.cpp',
  '../src/sprite_art.cpp',
  '../src/tile_atlas.cpp',
  '../src/tool_palette.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [gtkmm_dep, micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('ui_round4', ui_round4, timeout: 180)

round6 = executable(
  'round6',
  'round6.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('round6', round6, timeout: 180)

ui_round6 = executable(
  'ui_round6',
  'ui_round6.cpp',
  '../src/about_dialog.cpp',
  '../src/app_window.cpp',
  '../src/assets.cpp',
  '../src/budget_window.cpp',
  '../src/city_session.cpp',
  '../src/evaluation_window.cpp',
  '../src/graphs_window.cpp',
  '../src/map_view.cpp',
  '../src/messages.cpp',
  '../src/new_city_dialog.cpp',
  '../src/overlay_window.cpp',
  '../src/side_widgets.cpp',
  '../src/sound_player.cpp',
  '../src/sprite_art.cpp',
  '../src/tile_atlas.cpp',
  '../src/tool_palette.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [gtkmm_dep, micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('ui_round6', ui_round6, timeout: 180)

ui_graphs = executable(
  'ui_graphs',
  'ui_graphs.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/graphs_window.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [gtkmm_dep, micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

# Reads pixels from the window. Other GUI tests on the same X server cover
# those pixels, so this one runs by itself.
test('ui_graphs', ui_graphs, timeout: 180, is_parallel: false)

round7 = executable(
  'round7',
  'round7.cpp',
  include_directories: include_directories('../src'),
  cpp_args: [
    '-DLUNDUKE_CITY_SOURCE_DIR="' + meson.project_source_root() + '"',
  ],
)

test('round7', round7)

production_binary = executable(
  'production_binary',
  'production_binary.cpp',
)

test(
  'production_binary',
  production_binary,
  args: [lunduke_city_exe.full_path()],
  depends: lunduke_city_exe,
)

argv_open = executable(
  'argv_open',
  'argv_open.cpp',
  '../src/assets.cpp',
  '../src/city_session.cpp',
  '../src/messages.cpp',
  '../src/sound_player.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
  ],
)

# This test starts the real binary and reads the window tree. It does not
# share the display with the other GUI tests.
test(
  'argv_open',
  argv_open,
  args: [lunduke_city_exe.full_path()],
  depends: lunduke_city_exe,
  timeout: 120,
  is_parallel: false,
)

ui_round7 = executable(
  'ui_round7',
  'ui_round7.cpp',
  '../src/about_dialog.cpp',
  '../src/app_window.cpp',
  '../src/assets.cpp',
  '../src/budget_window.cpp',
  '../src/city_session.cpp',
  '../src/evaluation_window.cpp',
  '../src/graphs_window.cpp',
  '../src/map_view.cpp',
  '../src/messages.cpp',
  '../src/new_city_dialog.cpp',
  '../src/overlay_window.cpp',
  '../src/side_widgets.cpp',
  '../src/sound_player.cpp',
  '../src/sprite_art.cpp',
  '../src/tile_atlas.cpp',
  '../src/tool_palette.cpp',
  '../src/tools.cpp',
  include_directories: include_directories('../src'),
  dependencies: [gtkmm_dep, micropolis_dep, thread_dep],
  cpp_args: [
    '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
    '-DLUNDUKE_CITY_PACKAGE_VERSION="' + lunduke_package_version + '"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('ui_round7', ui_round7, timeout: 180)
