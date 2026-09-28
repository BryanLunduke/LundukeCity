# Micropolis simulation engine

This directory vendors the C++ simulation core from Micropolis, the
city-building engine released by Don Hopkins under the GNU General
Public License.

| | |
| --- | --- |
| Upstream | https://github.com/SimHacker/micropolis |
| Commit | `c98f6b08519887b450d9be198bfca5237aab6d0c` (2026-02-10) |
| Upstream path | `MicropolisCore/src/MicropolisEngine/src` |
| Local path | `third_party/micropolis/MicropolisEngine/src` |
| License | GPL-3.0-or-later, plus the additional terms in `NOTICE` |

Copyright (C) 1989–2007 Electronic Arts Inc. and 2007 Don Hopkins.
See the header of each source file and the top-level `NOTICE`.

## What was imported

Only the simulation sources (`*.cpp` / `*.h` under `MicropolisEngine/src`)
are imported. The upstream Python extension, SWIG bindings, and Tcl/Tk
interface are not part of this tree and are not required to build or run
Lunduke City.

The Lunduke City build compiles these sources as a static library with
`-Wno-register` so the historical `register` storage class still accepted
by this compiler does not clutter the log.

## Local modifications

This is a modified version of the upstream engine. Each change is marked
with a `Lunduke City:` comment at the changed line.

- `evaluate.cpp`, `voteProblems()`: the vote loop wrapped with
  `problem > PROBNUM`, so it read `problemTable[PROBNUM]` past the end of
  the table and could increment `problemVotes[PROBNUM]`, which overwrites
  `problemOrder[0]`. It now wraps at `PROBNUM`. The bug dates to the 1989
  C code (`VoteProblems` in `s_eval.c`).
- `micropolis.cpp`, `destroy()`: freed only the map arrays and left a
  `TODO`, so live sprites and the free-sprite pool leaked when the engine
  was destroyed. It now frees both.

## What the UI uses

Lunduke City constructs a `Micropolis` object, calls `simInit()` and
`generateSomeCity()`, places tools with `doTool()` / `toolDrag()`, and
advances time with `simTick()`. Map tiles, funds, the date, demand
valves, and messages are read back through the engine's public fields
and the scripting callback hook (`callbackHook`). The UI does not call
`environmentInit()` and does not look up `res/stri` string tables.
