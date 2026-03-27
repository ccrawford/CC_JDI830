# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CC_JDI830 is a standalone JDI EDM830 Engine Monitor replica for MSFS 2024 flight simulator. It runs on an RP2350 (Raspberry Pi Pico 2) with a 3.5" ILI9488 SPI display (320x480), communicating with MSFS via MobiFlight over serial (CmdMessenger protocol).

## Build System

PlatformIO project. The root `platformio.ini` includes all `*platformio.ini` files via glob. The custom device config lives in `CC_JDI830/CC_JDI830_platformio.ini`.

**Target environment**: `cacraw_cc_jdi830_raspberrypico2A_generic` (RP2350A, Earle Philhower Arduino core)

**Build command**: `pio run -e cacraw_cc_jdi830_raspberrypico2A_generic`

**Key libraries**: LovyanGFX (v1.2.19+) for display, Arduino-CmdMessenger for MobiFlight serial protocol.

**Note**: The user handles all builds and compiler checks themselves. Do not run builds.

## Architecture

### Data Flow

```
MSFS 2024 → MobiFlight (serial) → MFCustomDevice → CC_JDI830::set(msgID, value)
  → EngineState (raw + calculated fields) → Gauge sprites → ILI9488 display
```

MobiFlight sends 19 message IDs (0-18) mapping to engine parameters. `CC_JDI830::set()` parses these into `EngineState curState`. Each frame, `updateCalculated()` derives peaks, differentials, fuel calculations, and cooling rates.

### Profile-Driven UI

The display is entirely driven by **PlaneProfile** objects (const, in flash):
- Each profile declares cylinder count, 23+ capability flags (`hasEgt`, `hasMap`, etc.), gauge ranges with color bands, and fuel presets
- `PlaneProfile::isAvailable(DisplayVarId)` is the single source of truth for what a given aircraft supports
- **DisplayConfig** bridges profile capabilities to layout (how many HBars, column widths for 4-cyl vs 6-cyl)
- Profile index arrives as message ID 18; switching profiles rebuilds the entire gauge layout

### Display Components

All gauges inherit from a base class in `JPIGauges.hpp` that owns an LGFX_Sprite, position, range, and value pointer. Gauges only redraw when their value changes (dirty-flag optimization).

- **ArcGauge**: RPM and MAP (circular arcs with needles)
- **ColumnBarGauge**: EGT/CHT vertical columns (6 or 4 cylinders, optional TIT)
- **HBarGauge**: Right-column horizontal bars (oil temp, fuel flow, battery, etc.)
- **ValueGauge**: Simple numeric display (% HP)
- **BottomBar**: Rotating data pages, alarms, or lean-find info

### State Machines

**DisplayMode** controls the overall interaction state:
- `FUEL_SETUP` → power-up fuel entry wizard (multi-phase sub-state machine via `FuelSetupPhase`)
- `AUTO` → bottom bar pages rotate every 5 seconds
- `MANUAL` → user steps through pages with STEP button
- `LEAN_FIND` / `LEAN_PEAK` → lean-find procedure with sub-states via `LeanPhase`

**BottomBarMode** controls what content the bottom bar shows (ALL_DATA, ALARM, LEAN, SETUP, etc.).

### Button Input

Two physical buttons (STEP and LF) with gesture detection in `ButtonInput.hpp`:
- Tap, hold (3s/5s/7s), simultaneous press (150ms window for BOTH_TAP/BOTH_HOLD)
- Gesture enum drives state machine transitions in `handleButtonInput()`

### Cylinder Spreading

When the sim reports only a single EGT/CHT value, `EngineState::spreadCylinders()` generates per-cylinder variation using personality offsets, dynamic drift during transitions, and exponential decay at cruise.

### Alarm System

`AlarmManager.hpp` scans all engine parameters each frame against their GaugeRangeDef RED zones. Returns the highest-priority active alarm. The bottom bar displays it; STEP tap snoozes, STEP hold dismisses.

## Key Design Decisions

- **DisplayVarId** (`DisplayParams.hpp`) is the central enum for all displayable variables. `resolveDisplayVar()` maps ID to label, range, value pointer, decimals, and availability. All variable-switching code must go through this — never create duplicate switch/case mappings for DisplayVarId.
- **Header-only pattern**: Most components (gauges, pages, display params) are header-only `.hpp` files for inlining on the embedded target.
- **Memory budget**: `MF_MAX_DEVICEMEM` is 22KB. `sizeof(CC_JDI830)` is checked at runtime.
- **Color format**: Display uses BGR565 (not RGB). SPI at 40MHz write / 16MHz read.
- **Sprite rendering**: Each gauge owns its own sprite buffer pushed to LCD independently.
