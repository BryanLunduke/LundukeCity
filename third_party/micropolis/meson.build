micropolis_inc = include_directories('MicropolisEngine/src')

micropolis_src = files(
  'MicropolisEngine/src/allocate.cpp',
  'MicropolisEngine/src/animate.cpp',
  'MicropolisEngine/src/budget.cpp',
  'MicropolisEngine/src/connect.cpp',
  'MicropolisEngine/src/disasters.cpp',
  'MicropolisEngine/src/evaluate.cpp',
  'MicropolisEngine/src/fileio.cpp',
  'MicropolisEngine/src/generate.cpp',
  'MicropolisEngine/src/graph.cpp',
  'MicropolisEngine/src/initialize.cpp',
  'MicropolisEngine/src/main.cpp',
  'MicropolisEngine/src/map.cpp',
  'MicropolisEngine/src/message.cpp',
  'MicropolisEngine/src/micropolis.cpp',
  'MicropolisEngine/src/position.cpp',
  'MicropolisEngine/src/power.cpp',
  'MicropolisEngine/src/random.cpp',
  'MicropolisEngine/src/resource.cpp',
  'MicropolisEngine/src/scan.cpp',
  'MicropolisEngine/src/simulate.cpp',
  'MicropolisEngine/src/sprite.cpp',
  'MicropolisEngine/src/stubs.cpp',
  'MicropolisEngine/src/tool.cpp',
  'MicropolisEngine/src/traffic.cpp',
  'MicropolisEngine/src/update.cpp',
  'MicropolisEngine/src/utilities.cpp',
  'MicropolisEngine/src/zone.cpp',
)

# Vendored engine, with the Lunduke City edits described in
# third_party/micropolis/README.md and NOTICE. -Wno-register keeps the
# historical `register` keyword (legal in this engine, removed from
# C++17) quiet.
micropolis_lib = static_library(
  'micropolis',
  micropolis_src,
  include_directories: micropolis_inc,
  cpp_args: [
    '-Wno-register',
    '-Wno-unused-parameter',
    '-Wno-unused-variable',
    '-Wno-unused-but-set-variable',
    '-Wno-sign-compare',
    '-Wno-missing-field-initializers',
    '-Wno-implicit-fallthrough',
  ],
  override_options: ['warning_level=1', 'werror=false'],
)

micropolis_dep = declare_dependency(
  link_with: micropolis_lib,
  include_directories: micropolis_inc,
)
