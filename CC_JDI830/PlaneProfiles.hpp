#pragma once

#include "PlaneProfile.hpp"

// ---------------------------------------------------------------------------
// Plane profiles — one const instance per supported aircraft.
//
// These are pure data, intended to live in flash.  Add new profiles by
// copying an existing one and adjusting the values.
//
// Color convention used here (matches JPI style):
//   GREEN  = normal operating range
//   YELLOW = caution
//   RED    = danger / redline
//   WHITE  = informational (no limit significance)
//
// GaugeRangeDef layout:
//   { min, max, redline, { {colorMin,colorMax,color}, ... }, colorCount }
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// A2A Comanche 250 — naturally aspirated 6-cylinder O-540
//
// The current development aircraft.  All values/ranges in the existing
// code were tuned for this plane.
// ---------------------------------------------------------------------------
static const PlaneProfile PROFILE_A2A_COMANCHE = {
    .name = "A2A Comanche 250",

    .numCylinders         = 6,
    .reportsSingleEgtCht  = false,
    .fakeCylSpreadPct     = 0.0f,

    // --- Availability ---
    .hasEgt       = true,
    .hasCht       = true,
    .hasTit1      = false,     // naturally aspirated — no TIT
    .hasTit2      = false,
    .hasOilT      = true,
    .hasOilP      = true,
    .hasBat       = true,
    .hasOat       = true,
    .hasCrb       = true,      // carbureted
    .hasCdt       = false,
    .hasIat       = false,
    .hasMap       = true,
    .hasFf       = true,
    .hasFuelRem   = true,
    .hasReq       = true,
    .hasRes       = true,
    .hasMpg       = true,
    .hasEndurance = true,
    .hasUsed      = true,
    .hasHp        = true,
    .hasDif       = true,
    .hasColdRate  = true,

    // --- Fuel tank presets ---
    .fuelMain = 60.0f,       // Comanche 250 main tanks (2 × 30 gal)
    .fuelAux  = 0.0f,        // no aux tanks

    // --- Gauge ranges ---
    .rpm     = { 0, 2900, 2580,    { {0, 2580, TFT_GREEN}, {2580, 2900, TFT_RED} }, 2 },
    .map     = { 15, 33, 31,       { {15, 31, TFT_GREEN}, {31, 33, TFT_RED} }, 2 },
    .oilT    = { 100, 250, 0,      { {100, 120, TFT_RED}, {120, 220, TFT_GREEN}, {220, 250, TFT_RED} }, 3 },
    .oilP    = { 0, 115, 20,      { {0, 20, TFT_RED}, {20, 120, TFT_GREEN} }, 2 },
    .bat     = { 12, 16, 0,        { {11, 12, TFT_RED}, {12, 14, TFT_GREEN}, {14, 16, TFT_RED} }, 3 },
    .oat     = { -20, 50, 0,       { {-20, 50, TFT_GREEN} }, 1 },
    .egt     = { 850, 1700, 1650,  { {0, 1650, TFT_WHITE}, {1650, 2600, TFT_RED} }, 2 },
    .cht     = { 200, 600, 380,    { {0, 380, TFT_WHITE}, {380, 1000, TFT_RED} }, 2 },
    .tit1    = {},                  // no TIT
    .tit2    = {},
    .ff      = { 0, 30, 0,         { {0, 2.5, TFT_RED}, {2.5, 30, TFT_GREEN} }, 2 },
    .fuelRem = { 0, 70, 0,         { {0, 10, TFT_RED}, {10, 70, TFT_GREEN} }, 2 },  // max patched from sim at runtime
    .used    = { 0, 9999, 0,       { {0, 9999, TFT_GREEN} }, 1},
    .endurance = { 0, 9999, 0,     { {0, 20, TFT_RED}, {20, 9999, TFT_GREEN} }, 2},
    .hp      = { 0, 100, 0,        { {0, 100, TFT_WHITE} }, 1 },
    .crb     = { -30, 50, 0,       { {-30, -10, TFT_YELLOW}, {-10, 40, TFT_GREEN}, {40, 50, TFT_YELLOW} }, 3 },
    .cdt     = {},
    .iat     = {},
    .dif     = { 0, 500, 0,        { {0, 300, TFT_GREEN}, {300, 500, TFT_RED} }, 2 },
    .coldRate= { 0, 100, 60,        { {0, 40, TFT_GREEN}, {40,60, TFT_YELLOW}, {60,200, TFT_RED} }, 3 },
};

// ---------------------------------------------------------------------------
// Cessna T206H Turbo Stationair — turbocharged 6-cylinder IO-540
//
// Turbocharged, so has TIT1, CDT, IAT, and MAP is relevant.
// MSFS provides per-cylinder EGT/CHT for this aircraft.
// ---------------------------------------------------------------------------
static const PlaneProfile PROFILE_CESSNA_T206H = {
    .name = "Cessna T206H",

    .numCylinders         = 6,
    .reportsSingleEgtCht  = false,
    .fakeCylSpreadPct     = 0.0f,

    // --- Availability ---
    .hasEgt       = true,
    .hasCht       = true,
    .hasTit1      = true,
    .hasTit2      = true,  // Not really. just testing
    .hasOilT      = true,
    .hasOilP      = true,
    .hasBat       = true,
    .hasOat       = true,
    .hasCrb       = false,     // turbo — no carb
    .hasCdt       = true,      // compressor discharge temp
    .hasIat       = true,      // intercooler air temp
    .hasMap       = true,
    .hasFf        = true,
    .hasFuelRem   = true,
    .hasReq       = true,
    .hasRes       = true,
    .hasMpg       = true,
    .hasEndurance = true,
    .hasUsed      = true,
    .hasHp        = true,
    .hasDif       = true,
    .hasColdRate  = true,

    // --- Fuel tank presets ---
    .fuelMain = 61.0f,       // T206H main tanks (2 × 30.5 gal usable)
    .fuelAux  = 25.0f,       // T206H aux tanks (2 × 12.5 gal usable)

    // --- Gauge ranges ---
    .rpm     = { 0, 2900, 2700,    { {0, 2700, TFT_GREEN}, {2700, 2900, TFT_RED} }, 2 },
    .map     = { 15, 38, 36,       { {15, 36, TFT_GREEN}, {36, 40, TFT_RED} }, 2 },
    .oilT    = { 100, 250, 0,      { {100, 120, TFT_RED}, {120, 220, TFT_GREEN}, {220, 250, TFT_RED} }, 3 },
    .oilP    = { 0, 115, 100,      { {0, 25, TFT_RED}, {25, 100, TFT_GREEN}, {100, 115, TFT_RED} }, 3 },
    .bat     = { 20, 32, 0,        { {20, 22, TFT_RED}, {22, 30, TFT_GREEN}, {30, 32, TFT_RED} }, 3 },
    .oat     = { -20, 50, 0,       { {-20, 50, TFT_GREEN} }, 1 },
    .egt     = { 850, 1700, 1650,  { {0, 1650, TFT_WHITE}, {1650, 2600, TFT_RED} }, 2 },
    .cht     = { 200, 600, 380,    { {0, 380, TFT_WHITE}, {380, 1000, TFT_RED} }, 2 },
    .tit1    = { 850, 1700, 1650,  { {0, 1650, TFT_CYAN}, {1650, 2000, TFT_RED} }, 2 },
    .tit2    = { 850, 1700, 1650,  { {0, 1650, TFT_CYAN}, {1650, 2000, TFT_RED} }, 2 },
    .ff      = { 0, 30, 0,         { {0, 5, TFT_RED}, {5, 30, TFT_GREEN} }, 2 },
    .fuelRem = { 0, 92, 0,         { {0, 5, TFT_RED}, {5, 92, TFT_GREEN} }, 2 },
    .used    = { 0, 9999, 0,       { {0, 9999, TFT_GREEN} }, 1},
    .endurance = { 0, 9999, 0,     { {0, 20, TFT_RED}, {20, 9999, TFT_GREEN} }, 2},
    .hp      = { 0, 100, 0,        { {0, 100, TFT_WHITE} }, 1 },
    .crb     = {},                  // turbo — no carb
    .cdt     = { 0, 500, 450,      { {0, 400, TFT_GREEN}, {400, 450, TFT_YELLOW}, {450, 500, TFT_RED} }, 3 },
    .iat     = { 0, 300, 250,      { {0, 200, TFT_GREEN}, {200, 250, TFT_YELLOW}, {250, 300, TFT_RED} }, 3 },
    .dif     = { 0, 500, 0,        { {0, 300, TFT_GREEN}, {300, 500, TFT_RED} }, 2 },
    .coldRate= { 0, 100, 60,        { {0, 40, TFT_GREEN}, {40,60, TFT_YELLOW}, {60,200, TFT_RED} }, 3 },
};

// ---------------------------------------------------------------------------
// Cessna 172S Skyhawk — naturally aspirated 4-cylinder IO-360
//
// No turbo, so no TIT, CDT, or IAT.  Has carburetor heat/temp.
// MSFS only reports a single EGT/CHT, so we fake per-cylinder values.
// ---------------------------------------------------------------------------
static const PlaneProfile PROFILE_CESSNA_172S = {
    .name = "Cessna 172S",

    .numCylinders         = 4,
    .reportsSingleEgtCht  = true,
    .fakeCylSpreadPct     = 0.09f,  // fake cylinders ~91-100% of reported peak (carb: ~126°F DIF at 1400 EGT)

    // --- Availability ---
    .hasEgt       = true,
    .hasCht       = true,
    .hasTit1      = false,
    .hasTit2      = false,
    .hasOilT      = true,
    .hasOilP      = true,
    .hasBat       = true,
    .hasOat       = true,
    .hasCrb       = true,      // carbureted
    .hasCdt       = false,
    .hasIat       = false,
    .hasMap       = true,     
    .hasFf        = true,
    .hasFuelRem   = true,
    .hasReq       = true,
    .hasRes       = true,
    .hasMpg       = true,
    .hasEndurance = true,
    .hasUsed      = true,
    .hasHp        = false,
    .hasDif       = true,
    .hasColdRate  = true,

    // --- Fuel tank presets ---
    .fuelMain = 53.0f,       // 172S main tanks (2 × 26.5 gal usable)
    .fuelAux  = 0.0f,        // no aux tanks

    // --- Gauge ranges ---
    .rpm     = { 0, 2800, 2700,    { {0, 2700, TFT_GREEN}, {2700, 2800, TFT_RED} }, 2 },
    .map     = {},
    .oilT    = { 100, 250, 0,      { {100, 120, TFT_RED}, {120, 220, TFT_GREEN}, {220, 350, TFT_RED} }, 3 },
    .oilP    = { 0, 115, 100,      { {0, 25, TFT_RED}, {25, 100, TFT_GREEN}, {100, 115, TFT_RED} }, 3 },
    .bat     = { 20, 32, 0,        { {20, 22, TFT_RED}, {22, 30, TFT_GREEN}, {30, 32, TFT_RED} }, 3 },
    .oat     = { -20, 50, 0,       { {-20, 50, TFT_GREEN} }, 1 },
    .egt     = { 800, 1700, 1550,  { {0, 1550, TFT_WHITE}, {1550, 2000, TFT_RED} }, 2 },
    .cht     = { 200, 600, 400,    { {0, 400, TFT_WHITE}, {400, 600, TFT_RED} }, 2 },
    .tit1    = {},
    .tit2    = {},
    .ff      = { 0, 20, 0,         { {0, 3, TFT_RED}, {3, 15, TFT_GREEN} }, 2 },
    .fuelRem = { 0, 56, 0,         { {0, 8, TFT_RED}, {8, 56, TFT_GREEN} }, 2 },
    .used    = { 0, 9999, 0,       { {0, 9999, TFT_GREEN} }, 1},
    .endurance = { 0, 9999, 0,       { {0, 20, TFT_RED}, {20, 9999, TFT_GREEN} }, 2},
    .hp      = { 0, 100, 0,        { {0, 100, TFT_WHITE} }, 1 },
    .crb     = { -30, 50, 0,       { {-30, -10, TFT_YELLOW}, {-10, 40, TFT_GREEN}, {40, 50, TFT_YELLOW} }, 3 },
    .cdt     = {},
    .iat     = {},
    .dif     = { 0, 400, 0,        { {0, 200, TFT_GREEN}, {200, 400, TFT_RED} }, 2 },
    .coldRate= { 0, 100, 60,        { {0, 40, TFT_GREEN}, {40,60, TFT_YELLOW}, {60,200, TFT_RED} }, 3 },
};

// ---------------------------------------------------------------------------
// Profile table — add new profiles here.
// The active profile is selected by index at runtime.
// ---------------------------------------------------------------------------
static const PlaneProfile* const ALL_PROFILES[] = {
    &PROFILE_A2A_COMANCHE,
    &PROFILE_CESSNA_T206H,
    &PROFILE_CESSNA_172S,
};
static constexpr int NUM_PROFILES = sizeof(ALL_PROFILES) / sizeof(ALL_PROFILES[0]);
