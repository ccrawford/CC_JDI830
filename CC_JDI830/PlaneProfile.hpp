#pragma once

#include "JPIGauges.hpp"   // ColorRange

// Forward-declare so isAvailable() can take a DisplayVarId parameter
// without creating a circular include (DisplayParams.hpp includes us).
enum class DisplayVarId : uint8_t;

// ---------------------------------------------------------------------------
// GaugeRangeDef — range, redline, and color bands for one measurement.
//
// This is pure data — it describes how a gauge should be scaled and colored,
// independent of the gauge type (arc, bar, column, bottom bar, etc.).
// The same GaugeRangeDef can feed both a main gauge and its bottom bar page.
// ---------------------------------------------------------------------------
struct GaugeRangeDef {
    float min;
    float max;
    float redline;              // 0 = no redline
    ColorRange colors[MAX_COLOR_RANGES];
    uint8_t colorCount;
};

// ---------------------------------------------------------------------------
// PlaneProfile — compile-time description of an aircraft's engine and
// instrumentation.
//
// This defines what measurements are *possible* for a given airframe and
// their operating ranges.  A separate UserConfig (not yet implemented)
// will let the user disable individual measurements at runtime.
//
// Profiles are intended to be const/constexpr globals that live in flash.
// At runtime, a `const PlaneProfile*` pointer selects the active profile.
// ---------------------------------------------------------------------------
struct PlaneProfile {
    const char* name;           // e.g. "Cessna T206H Turbo Stationair"

    // --- Engine geometry ---
    int  numCylinders;          // 4 or 6 typically

    // --- Cylinder data source ---
    // Most MSFS planes only report a single EGT and CHT value, which we
    // interpret as the peak cylinder.  When reportsSingleEgtCht is true,
    // the display generates fake per-cylinder values by spreading them
    // below the reported peak.  fakeCylSpreadPct controls the spread:
    // e.g. 0.15 means fake cylinders range from 85% to 100% of peak.
    bool  reportsSingleEgtCht;
    float fakeCylSpreadPct;     // 0.0–1.0, only used when reportsSingleEgtCht

    // --- Measurement availability ---
    // These flags indicate whether the aircraft/sim provides each measurement.
    // false = gauge and bottom bar page won't be shown.
    bool hasEgt;
    bool hasCht;
    bool hasTit1;
    bool hasTit2;
    bool hasOilT;
    bool hasOilP;
    bool hasBat;
    bool hasOat;
    bool hasCrb;                // Carburetor air temp (mutually exclusive with CDT/IAT)
    bool hasCdt;                // Compressor discharge temp (turbo)
    bool hasIat;                // Intercooler air temp (turbo)
    bool hasMap;
    bool hasFf;
    bool hasFuelRem;
    bool hasReq;                // Fuel required to waypoint
    bool hasRes;                // Fuel reserve at waypoint
    bool hasMpg;
    bool hasEndurance;
    bool hasUsed;               // Fuel used
    bool hasHp;
    bool hasDif;                // EGT differential (hottest - coldest)
    bool hasColdRate;                

    // --- Gauge ranges ---
    // Each GaugeRangeDef holds the scale range, redline, and color bands
    // for one measurement.  Gauges and bottom bar pages both read from these.
    GaugeRangeDef rpm;
    GaugeRangeDef map;
    GaugeRangeDef oilT;
    GaugeRangeDef oilP;
    GaugeRangeDef bat;
    GaugeRangeDef oat;
    GaugeRangeDef egt;
    GaugeRangeDef cht;
    GaugeRangeDef tit1;
    GaugeRangeDef tit2;
    GaugeRangeDef ff;
    GaugeRangeDef fuelRem;
    GaugeRangeDef used;
    GaugeRangeDef endurance;
    GaugeRangeDef hp;
    GaugeRangeDef crb;
    GaugeRangeDef cdt;
    GaugeRangeDef iat;
    GaugeRangeDef dif;
    GaugeRangeDef req;
    GaugeRangeDef coldRate;

    // Note: fuelCapacity is in EngineState, not here — it's a sim-reported
    // value, not a profile constant.  setupGauges() patches fuelRem.max
    // from curState.fuelCapacity at runtime.

    // --- Availability lookup by DisplayVarId ---
    // Single source of truth for mapping a DisplayVarId to its has* flag.
    // This eliminates duplicate switch statements in DisplayParams and
    // DisplayConfig — any code that needs to check "does this profile
    // support variable X?" calls this instead of switching on has* itself.
    //
    // Note: this is a member function on a struct full of const instances,
    // which works fine in C++ — the compiler just passes a hidden `this`
    // pointer.  Since the profiles live in flash, the `this` pointer
    // points to flash, and the bool fields are read directly from there.
    bool isAvailable(DisplayVarId id) const;
};
