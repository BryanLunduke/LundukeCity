# Lunduke City

Lunduke City is a windowed city-building game for LCOS. Version 0.1 is a
gtkmm 3 shell in the classic menu / funds / tool-palette / map layout,
wired to the Micropolis simulation engine.

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

The installed binary name is `lunduke-city`. A desktop entry and icon are
installed with `ninja -C build install`.

`meson test -C build` runs a headless engine check (new city, place a road,
tick once).

## What 0.1 does

- Menu bar: System, Options, Disasters, Windows.
- Status line: funds, city name, date.
- Left tool palette (2×8). The selected tool is highlighted, and the message
  bar shows its price (for example `Power lines: $5`).
- Minimap (click to jump) and R / C / I demand meters (green / blue / yellow).
- Scrolling map drawn from the live engine tiles.
- New city, load, and save (`.cty`, the engine's city format).
- Simulation ticks on a timer. Speed is Pause, Slow, Medium, or Fast.
- Tools spend funds through the engine. Query reports the tile under the cursor.
- Disasters, tax rate, and a short evaluation window call into the engine.

## Engine

Sources live in `third_party/micropolis/MicropolisEngine/`, taken from
[SimHacker/micropolis](https://github.com/SimHacker/micropolis) commit
`c98f6b08519887b450d9be198bfca5237aab6d0c`, path
`MicropolisCore/src/MicropolisEngine/src`. They are unmodified and compiled
as a static library. Provenance details are in
[third_party/micropolis/README.md](third_party/micropolis/README.md).

## Next steps

- Draw with the original 16-pixel tile artwork (those images are not in this
  tree) instead of the procedural stand-in.
- Animate trains, ships, aircraft, tornadoes, and the monster as sprites
  rather than colored markers. Their effects on the map already come from
  the engine.
- A full budget book (funding sliders for roads, police, and fire) instead
  of the tax-rate control.
- Separate map windows for power, water, pollution, crime, land value, and
  traffic. The engine already keeps those layers.
- Ship a few sample cities and the scenario set, with the same license
  notices as the engine.
- Sound. The engine asks for sounds by name; nothing plays them yet.
