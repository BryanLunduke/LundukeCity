# Lunduke City

Lunduke City is a windowed city-building game for LCOS. Version 0.7
(Debian package 0.7-3, Meson project version 0.7.3) is the LCOS 0.7
release track: classic menu / funds / tool-palette / map layout, wired
to the Micropolis simulation engine. The map uses the Micropolis
16-pixel tiles, and trains, ships, aircraft, tornadoes, and the monster
are drawn from the engine's sprite frames.

The interface is original. It does not use Tcl/Tk. Lunduke City is an
independent project and is not affiliated with or endorsed by Electronic Arts.
The engine's copyright and additional terms are in [NOTICE](NOTICE). The
GNU GPL version 3 is in [COPYING](COPYING).

## Build

Dependencies on Debian or Ubuntu:

```bash
sudo apt install build-essential meson ninja-build pkg-config libgtkmm-3.0-dev
```

Requires a C++17 compiler.

```bash
meson setup build
ninja -C build
./build/src/lunduke-city
```

The installed binary name is `lunduke-city`. A desktop entry and the
roadmap-blueprint icon are installed with `ninja -C build install`
(`Icon=lunduke-city`: scalable SVG from `data/lunduke-city.svg`, plus
hicolor PNGs at 16, 32, 48, 64, 128, and 256).

`meson test -C build` runs headless checks: the engine smoke test, the
engine fixes (problem votes and sprite cleanup), plus budget funding,
overlay samples, tile animation, a sprite report, and sound startup
(playback is skipped cleanly when no audio device is available).

## What 0.7-3 fixes

- Traffic, fountains, and smokestacks animate. Each session tick runs
  the engine's tile animation after the simulation step.
- The vendored engine's yearly problem vote stays inside the problem
  table.
- Leaving a city frees the engine's sprites as well as its map arrays.
- The engine sources are a modified version of the upstream program.
  The changes are listed under Engine below and in [NOTICE](NOTICE).

## What 0.7-2 adds

- System → New City is a three-step wizard: name, then difficulty,
  terrain, and seed, then generate. Difficulty is the engine game level
  (easy $20,000, medium $10,000, hard $5,000). Terrain choices are the
  generator's island, river, lake, and tree controls. The seed is passed
  to `generateSomeCity` (0 takes one from the clock).
- Windows → Graphs charts population (from the residential, commercial,
  and industrial histories), cash flow, crime, and pollution for the
  10-year and 120-year engine scales. It keeps updating while the city runs.
- Windows → Evaluation shows the engine score, mayor opinion, the worst
  problems, and the city statistics.
- System → Rename City renames the open city and the window title.
- Windows → About Lunduke City is a dialog with the 0.7-2 version, a
  short description, the GPL and NOTICE pointers, and a statement that
  the project is independent of Electronic Arts.

## What 0.3 adds

- Options → Zoom in is Ctrl and the +/= key, with no Shift. Keypad plus
  and Ctrl-minus still zoom.
- The map outlines the footprint of the selected build tool under the
  pointer (zones, roads, rail, wire, park, bulldozer, and the other
  placeable tools). Query does not.
- System → Play Scenario… lists the eight engine scenarios (Dullsville
  through Rio de Janeiro) and starts the one you pick.
- The desktop and window icon are the roadmap blueprint (`lunduke-city`).
  The window sets that name with `set_default_icon_name` and `set_icon_name`.

## What 0.2 does

- Menu bar: System, Options, Disasters, Windows. Options includes speed,
  zoom, and mute. The menus and the rest of the window chrome follow the
  system GTK theme.
- Status line: funds, city name, date.
- Left tool palette (2×8). The selected tool is highlighted, and the message
  bar shows its price (for example `Power lines: $5`).
- Minimap (click to jump) and R / C / I demand meters (green / blue / yellow).
- Scrolling map drawn from live engine tile indices through the Micropolis
  16-pixel atlas (`tiles.png`, the same artwork as `res/hexa.*`).
- Trains, helicopters, airplanes, ships, the monster, tornadoes, explosions,
  and buses use the engine sprite frames (`obj*.png`) instead of flat markers.
- New city, load, and save (`.cty`, the engine's city format).
- Simulation ticks on a timer. Speed is Pause, Slow, Medium, or Fast.
- Tools spend funds through the engine. Query reports the tile under the cursor.
- Disasters and a short evaluation window call into the engine.
- Windows → Budget opens the funding book: tax rate, and road, police, and
  fire funding, with taxes collected, cash flow, and the requested and
  funded amounts from the engine.
- Windows menu map views for power, water, pollution, crime, land value,
  and traffic. Water is the engine's water tiles; the others read the
  engine's layers.
- Sounds. The engine asks by name (`Siren`, `HonkHonkLow`, and the rest).
  Matching PCM files in `res/sounds/` play through PulseAudio when a device
  is open. Mute is under Options. With no device, or no matching file, the
  request is ignored.

## Engine

Sources live in `third_party/micropolis/MicropolisEngine/`, taken from
[SimHacker/micropolis](https://github.com/SimHacker/micropolis) commit
`c98f6b08519887b450d9be198bfca5237aab6d0c`, path
`MicropolisCore/src/MicropolisEngine/src`, and then modified. They are
compiled as a static library. This is not the original engine program.
The modifications, required to be marked by GPL section 7 and the
additional terms in [NOTICE](NOTICE), are:

- `voteProblems()` wraps its problem index when the index reaches
  `PROBNUM`, so a yearly evaluation does not read one entry past
  `problemTable` or overwrite `problemOrder[0]`.
- `destroy()` frees sprites on the active list and the free list, as
  well as the map arrays.

Provenance details are in
[third_party/micropolis/README.md](third_party/micropolis/README.md).

Tiles, sprites, and sounds are vendored under
[third_party/micropolis-assets/](third_party/micropolis-assets/README.md)
from the same upstream commit. They are unmodified.

## Next steps

- Ship a few sample cities, with the same license notices as the engine.
  The scenario set (`res/snro.*`) is already included.
