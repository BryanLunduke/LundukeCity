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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.8-2"',
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
    '-DLUNDUKE_CITY_PACKAGE_VERSION="0.8-2"',
  ],
)

test('regressions', regressions)
