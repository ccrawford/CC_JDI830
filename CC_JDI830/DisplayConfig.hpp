#pragma once

#include "DisplayParams.hpp"

// ---------------------------------------------------------------------------
// LayoutMode — which screen layout to use.
//
// Each mode implies a different arrangement of gauges on the screen.
// PORTRAIT is the original JPI 830 layout (320×480).  LANDSCAPE is a future
// option for wider screens or rotated orientation.
// ---------------------------------------------------------------------------
enum class LayoutMode : uint8_t {
    PORTRAIT,       // 320×480, current default
    LANDSCAPE,      // future: 480×320 rotated
};

// ---------------------------------------------------------------------------
// DisplayConfig — resolved display layout for the current session.
//
// This sits between the PlaneProfile (what the aircraft *can* show) and the
// rendering code (what actually appears on screen).  It is rebuilt whenever
// the profile changes, and eventually when the user changes preferences.
//
// The idea is separation of concerns:
//   PlaneProfile  → "what's physically possible"  (aircraft data, const)
//   UserPrefs     → "what the user wants"          (future, persisted)
//   DisplayConfig → "what to actually render"      (derived, mutable)
//
// Today, buildDefaultConfig() creates this from just a PlaneProfile.
// Later the flow becomes:  PlaneProfile + UserPrefs → buildConfig() → DisplayConfig.
// ---------------------------------------------------------------------------

// Maximum number of right-column HBar slots any layout could ever need.
// This sets the size of the fixed array — the actual slot count is
// displayConfig.numRightBarSlots, which may be smaller.
static constexpr int MAX_RIGHT_BAR_SLOTS = 10;

struct DisplayConfig {
    LayoutMode layoutMode = LayoutMode::PORTRAIT;

    // --- Right-column HBar gauges ---
    int          numRightBarSlots = 5;
    DisplayVarId rightBarSelections[MAX_RIGHT_BAR_SLOTS] = {};

    // --- Right-column positioning (pixels) ---
    // These are the layout constants that setupRightBars() uses.
    // Pulled out here so that different layouts can override them.
    int barX        = 220;
    int barY        = -20;
    int barW        = 80;
    int barH        = 29;
    int barYSpacing = 34;

    // --- EGT/CHT column bar width ---
    // For portrait: 310 for 6-cyl, ~205 for 4-cyl.  Landscape may differ.
    int egtChtWidth = 310;
};

// ---------------------------------------------------------------------------
// buildDefaultConfig — creates a DisplayConfig from a PlaneProfile.
//
// This is the "resolution" step: it looks at what the profile supports and
// picks sensible defaults for gauge count, selection, and positioning.
//
// Later, a buildConfig(profile, userPrefs) overload can layer in user
// choices on top of these defaults.
// ---------------------------------------------------------------------------
inline DisplayConfig buildDefaultConfig(const PlaneProfile& profile) {
    DisplayConfig cfg;
    cfg.layoutMode = LayoutMode::PORTRAIT;

    // 4-cylinder engines use a narrower EGT/CHT column, freeing ~105px
    // of vertical space on the right side for additional HBar rows.
    bool compact = (profile.numCylinders <= 4);
    cfg.egtChtWidth = compact ? 205 : 310;

    // With the shorter column bar area, we can fit more hBar rows.
    // 6-cyl: 5 rows (classic JPI 830 layout)
    // 4-cyl: 7 rows (two extra rows in the freed space)
    cfg.numRightBarSlots = compact ? 10 : 5;

    // Tighter spacing when we have more rows so they still fit
    if (compact) {
        cfg.barYSpacing = 28;
        cfg.barH        = 24;
    }

    // Build a default gauge selection from what the profile supports.
    // Start with the "classic" order, skip anything unavailable, then
    // fill remaining slots from whatever else is available.
    //
    // Priority order — matches the real JPI 830 default:
    static constexpr DisplayVarId PREFERRED_ORDER[] = {
        DisplayVarId::OIL_TEMP,
        DisplayVarId::OIL_PRESS,
        DisplayVarId::FUEL_FLOW,
        DisplayVarId::FUEL_REM,
        DisplayVarId::OAT,
        DisplayVarId::BATTERY,
        DisplayVarId::CARB_TEMP,
        DisplayVarId::COLD,
        DisplayVarId::EGT_DIF,
        DisplayVarId::FUEL_REQ,
        DisplayVarId::FUEL_USED,
        DisplayVarId::CDT,
        DisplayVarId::IAT,
        DisplayVarId::HP,
    };

    int slot = 0;
    for (auto id : PREFERRED_ORDER) {
        if (slot >= cfg.numRightBarSlots) break;
        if (profile.isAvailable(id)) {
            cfg.rightBarSelections[slot++] = id;
        }
    }

    // If we didn't fill all slots (unlikely), leave the rest zeroed —
    // setupRightBars() should check slot < numRightBarSlots.
    cfg.numRightBarSlots = slot;  // trim to actual count

    return cfg;
}