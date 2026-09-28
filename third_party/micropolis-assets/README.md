# Micropolis tiles, sprites, and sounds

These files are the classic Micropolis map artwork, mobile and disaster
sprites, tile resources, and sound effects. Lunduke City draws and plays
them. Lunduke City is an independent project and is not affiliated with
Electronic Arts.

| | |
| --- | --- |
| Upstream | https://github.com/SimHacker/micropolis |
| Commit | `c98f6b08519887b450d9be198bfca5237aab6d0c` (2026-02-10) |
| License | GPL-3.0-or-later, plus the additional terms in the top-level `NOTICE` |

Copyright (C) 1989–2007 Electronic Arts Inc. and 2007 Don Hopkins.

## What was copied

Unmodified, from `MicropolisCore/src/` at that commit:

| Local path | Upstream path |
| --- | --- |
| `images/micropolisEngine/tiles.png` | `images/micropolisEngine/tiles.png` |
| `images/micropolisEngine/obj*.png` | `images/micropolisEngine/obj*.png` |
| `res/hexa.*` | `res/hexa.*` |
| `res/sounds/*.wav` | `res/sounds/*.wav` |
| `res/snro.111` … `res/snro.888` | `res/snro.111` … `res/snro.888` |

Nothing in this directory was edited. The additional terms require modified
versions to be marked; these bytes are the upstream files.

`tiles.png` is 256×960, 8-bit RGB: 960 tiles of 16×16 pixels, 16 tiles per
row, tile index `0..959` in row-major order. That is the classic map atlas
(the same artwork the `res/hexa.*` resources encode). Lunduke City draws
the live map from engine tile indices through this atlas. Indices past the
end of the sheet (the later church extension) are drawn as clear land.

`obj<type>-<frame>.png` are the sprite frames. Type numbers match the
engine (`1` train through `8` bus). Frame files are 0-based; the engine's
`frame` field is 1-based (`0` means inactive).

`res/sounds/*.wav` are the named effects. The engine asks for sounds such
as `Siren` and `HonkHonkLow`. Playback matches those names to a PCM wav
(case-insensitive, ignoring hyphens). `FogHornLow` has no wav in this set,
so that request is skipped. μ-law files (`explosion-hi.wav`,
`honkhonk-hi.wav`, `quack.wav`) are kept for provenance; the PCM twins are
what playback uses.

Tool-icon and window-chrome images from upstream are not included. Lunduke
City keeps the system GTK theme.
