# CLAUDE.md

## What This Is

GW2 in-game event timer addon (Nexus/Raidcore framework). Shows world boss, meta event, convergence, ley-line anomaly, and dragonstorm timers grouped by expansion pack with countdown timers. UI in Turkish. No network required — purely local UTC time calculation.

## Build

**Toolchain:** Visual Studio 2022 (v143), C++17, x64 only. Opens as `GW2-CLE-Timer.sln`.

```
msbuild src\GW2-CLE-Timer.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `build\Release\claymore-event-timer.dll`. Post-build copies to `..\..\addons\claymore-event-timer.dll` (silent fail if dir missing).

**Dependencies:** nlohmann/json (header-only, `include/json.hpp`), Dear ImGui (vendored in `src/imgui/`, Nexus-provided version). `src/nexus/` and `src/mumble/` are vendored headers. No WinHTTP, no network.

## Tests

```
# From src/ directory (VS Developer Command Prompt):
cl /EHsc /std:c++17 /permissive- /DNOMINMAX /MT /utf-8 /I"..\include" test_timer.cpp core\TimerEngine.cpp core\ConfigManager.cpp /Fe:test_timer.exe
test_timer.exe
```

Tests verify: event data loading, world boss per-segment scheduling (Taidha/Svanir/Shatterer at correct UTC offsets), hard boss schedule (Tequatl/TT/Karka), ley-line anomaly 3-location rotation, Auric Basin Octovine timing, per-segment row generation (13+ WB rows not 2), convergence segment filtering (Outer Nayos→SotO, Mount Balrior→JW), dragonstorm filtering (no Marionette/BoLA), day-wrap for >1440min partials, LoadFromMemory.

## Architecture

- **`src/entry.cpp`** — Nexus DLL entry, ImGui rendering. Signature: -77044. Addon dir: `claymore-event-timer`. Keybind: ALT+E. Quick Access icon embedded as `icon_data.h`. Events JSON embedded as `events_default.h` (written to addon dir on first run).
- **`src/core/TimerEngine.h/.cpp`** — Loads wiki event timer JSON (`data/events.json`), maps events to expansion groups via `s_mappings[]` table. Per-segment row generation for boss/ley-line/convergence categories (one row per boss, sorted by next spawn). Meta events show current phase + time remaining. No ImGui dependency — unit testable.
- **`src/core/ConfigManager.h/.cpp`** — JSON config (hidden_events set, show_local_time, window_alpha). Dir: `addons/claymore-event-timer/`.
- **`src/events_default.h`** — Byte array of `data/events.json` for first-run bootstrap. Regenerate: `python3 -c "..."` (see the script that generated it, or run the icon generation pattern from `icon_data.h` on `data/events.json`).

### Event Data Source

`data/events.json` = GW2 wiki `Widget:Event_timer/data.json` v5.2. This is the same data the in-game `/wiki et` timer uses. Format: per-event `segments` (named phases with chatlinks) + `sequences` (partial from midnight + repeating pattern, durations in minutes).

To update: download `https://wiki.guildwars2.com/index.php?title=Widget:Event_timer/data.json&action=raw`, replace `data/events.json`, regenerate `events_default.h`.

### Event Mapping

`s_mappings[]` in `TimerEngine.cpp` maps wiki event keys to our expansion groups and categories. `segmentFilter` splits cross-expansion events (`public-con` → SotO/JW, `public-eotn` → IBS Dragonstorm only).

### Known Limitations (v0.1.0)

- **Leyspring Hollows (Depths of Cruelty)** not in wiki event timer data (released 2026-09-15, 3h cycle). Will be added when wiki updates.
- **Convergence weekly boss rotation** (which boss is active this week) not shown — only the 3h portal timer.
- **LW1 public instances** (Twisted Marionette, Battle for Lion's Arch, Tower of Nightmares) not mapped — only Dragonstorm from that group.
- **Meta highlight**: all meta phases show green "AKTIF" since every phase is non-gap in wiki data. A future `highlightSegment` per meta would show green only during the boss/reward phase.
- Festival events (Dragon Bash, Halloween, Labyrinthine Cliffs) not included.
