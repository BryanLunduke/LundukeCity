lunduke_city_src = files(
  'main.cpp',
  'app_window.cpp',
  'assets.cpp',
  'budget_window.cpp',
  'city_session.cpp',
  'map_view.cpp',
  'messages.cpp',
  'overlay_window.cpp',
  'side_widgets.cpp',
  'sound_player.cpp',
  'sprite_art.cpp',
  'tile_atlas.cpp',
  'tool_palette.cpp',
  'tools.cpp',
)

executable(
  'lunduke-city',
  lunduke_city_src,
  dependencies: [gtkmm_dep, micropolis_dep, thread_dep],
  cpp_args: '-DLUNDUKE_CITY_ASSET_DIR="' + lunduke_asset_dir + '"',
  install: true,
)
