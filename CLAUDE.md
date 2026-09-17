# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

GW2 in-game event timer addon (Nexus/Raidcore framework). Renders a wiki-style timeline showing world boss, meta event, convergence, ley-line anomaly, and dragonstorm timers grouped by expansion. UI strings are Turkish without diacritics (e.g. "BASLADI", "seffafligi", "Yerel"). No network — purely local UTC time calculation against `data/events.json`.

## Build

**Toolchain:** Visual Studio 2022 (v143), C++17, x64 only. Opens as `GW2-CLE-Timer.sln`.

```
msbuild src\GW2-CLE-Timer.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `build\Release\claymore-event-timer.dll`. Post-build copies to `..\..\addons\claymore-event-timer.dll` (silent fail if dir missing).

**Dependencies:** nlohmann/json (header-only, `include/json.hpp`), Dear ImGui 1.80 (vendored in `src/imgui/`, Nexus-provided — uses pre-1.82 API like `ImDrawCornerFlags_Left`). `src/nexus/` and `src/mumble/` are vendored headers. No WinHTTP, no network.

## Tests

Both test executables must be built from `src/` using a VS Developer Command Prompt (they use `_mkgmtime` and relative path `../data/events.json`).

```
# Timer engine tests
cl /EHsc /std:c++17 /permissive- /DNOMINMAX /MT /utf-8 /I"..\include" test_timer.cpp core\TimerEngine.cpp core\ConfigManager.cpp /Fe:test_timer.exe
test_timer.exe

# Track manager tests
cl /EHsc /std:c++17 /permissive- /DNOMINMAX /MT /utf-8 /I"..\include" test_track.cpp core\TimerEngine.cpp core\TrackManager.cpp /Fe:test_track.exe
test_track.exe
```

Tests verify: event data loading, world boss per-segment scheduling (Taidha/Svanir/Shatterer at correct UTC offsets), hard boss schedule (Tequatl/TT/Karka), ley-line anomaly 3-location rotation, Auric Basin Octovine timing, per-segment row generation (13+ WB rows not 2), convergence segment filtering (Outer Nayos→SotO, Mount Balrior→JW), dragonstorm filtering (no Marionette/BoLA), day-wrap for >1440min partials, LoadFromMemory, track add/remove, threshold-based notifications (10/2/0), retroactive suppression, LoadPairs cascade suppression, tick jumps.

## Architecture

- **`src/entry.cpp`** — Nexus DLL entry, ImGui timeline rendering (~800 lines). Signature: -77044. Addon dir: `claymore-event-timer`. Keybind: ALT+E. Quick Access icon embedded as `icon_data.h`. Events JSON embedded as `events_default.h` (written to addon dir on first run). Contains the toast notification system (10s auto-dismiss, click to copy waypoint + dismiss).
- **`src/core/TimerEngine.h/.cpp`** — Loads wiki event timer JSON, maps events to expansion groups via `s_mappings[]` table. The timeline UI in `entry.cpp` calls `GetPhaseAt()` per-minute over a ±60min window around now (`HALF_WINDOW`), drawing colored segment bars. `GenerateBossRows`/`GenerateMetaRow`/`Update`/`GetGroups` exist and are tested but **not used by the UI** — do not assume editing row generation changes what renders on screen.
- **`src/core/TrackManager.h/.cpp`** — Per-segment event tracking with threshold notifications. Users right-click a timeline segment to track it. Thresholds: `{remindMinutes, min(2, remindMinutes), 0}`. Reminders dedup on occurrence-start `time_t`, not on minutes-until. `LoadPairs` sets `m_needsSuppression` so the first `Tick` pre-marks already-passed thresholds (prevents a 3-toast cascade on addon load). `AddTrack` does the same pre-marking for segments already inside the remind window.
- **`src/core/ConfigManager.h/.cpp`** — JSON config persisted to `addons/claymore-event-timer/config.json`. Stores: hidden_events set, show_local_time, window_alpha, window position/size rect, remind_minutes, tracked event pairs.

### Versioning

`entry.cpp` has both `VER_MAJOR/VER_MINOR/VER_BUILD` (int constants for Nexus API) and `CLE_VERSION_STR` (string literal). Both must be updated together. `CLE_VERSION_STR` is baked into the window title string used in `Begin()`, `GUI_RegisterCloseOnEscape`, and `GUI_DeregisterCloseOnEscape` — all three must match exactly. Commit convention: `fix: vX.Y.Z - summary` or `feat: vX.Y.Z - summary`.

### Event Data Source

`data/events.json` = GW2 wiki `Widget:Event_timer/data.json` v5.2. This is the same data the in-game `/wiki et` timer uses. Format: per-event `segments` (named phases with chatlinks and `bg` RGB color) + `sequences` (partial from midnight + repeating pattern, durations in minutes).

To update: download the wiki data JSON, replace `data/events.json`, regenerate `events_default.h`.

### Embedded Data Regeneration

`events_default.h` embeds `data/events.json` as a C byte array (`EVENTS_DEFAULT_JSON` / `EVENTS_DEFAULT_JSON_SIZE`). Regenerate with:

```
python3 -c "d=open('data/events.json','rb').read(); print('#pragma once\n#include <cstdint>\n#include <cstddef>\n\n// Embedded events.json - %d bytes\nstatic const uint8_t EVENTS_DEFAULT_JSON[] = {\n  %s\n};\nstatic const size_t EVENTS_DEFAULT_JSON_SIZE = %d;' % (len(d), ','.join('0x%02X'%b for b in d), len(d)))" > src/events_default.h
```

`icon_data.h` uses the same pattern (`ICON_CLE_PNG` / `ICON_CLE_PNG_SIZE`) for the Quick Access icon PNG.

### Event Mapping

`s_mappings[]` in `TimerEngine.cpp` maps wiki event keys to expansion groups, categories, display order, segment filters, highlight segments, and map IDs. `segmentFilter` has two different effects: in `GenerateBossRows` it excludes non-matching segments from rows; in the timeline UI (`RenderTimelineBar`) it dims them to gap color but still draws them. The config visibility key for filtered events is `wikiKey + "#" + segmentFilter`.

### Known Limitations

- **Leyspring Hollows (Depths of Cruelty)** not in wiki event timer data (released 2026-09-15, 3h cycle). Will be added when wiki updates.
- **Convergence weekly boss rotation** (which boss is active this week) not shown — only the 3h portal timer.
- **LW1 public instances** (Twisted Marionette, Battle for Lion's Arch, Tower of Nightmares) not mapped — only Dragonstorm from that group.
- Festival events (Dragon Bash, Halloween, Labyrinthine Cliffs) not included.
