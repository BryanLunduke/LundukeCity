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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.9-7"',
    '-DLUNDUKE_CITY_TEST_HOOKS',
  ],
)

test('ui_round6', ui_round6, timeout: 180)
