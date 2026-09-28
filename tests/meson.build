engine_smoke = executable(
  'engine_smoke',
  'engine_smoke.cpp',
  dependencies: [micropolis_dep],
)

test('engine_smoke', engine_smoke)

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
  cpp_args: '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
)

test('features', features)
