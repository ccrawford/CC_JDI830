#pragma once

#include "PlaneProfile.hpp"
#include "EngineState.hpp"

// ---------------------------------------------------------------------------
// DisplayParams — central registry of displayable engine variables.
//
// Every variable that can appear on a gauge (right-column HBar, bottom bar,
// etc.) is listed in the DisplayVarId enum.  resolveDisplayVar() maps an ID
// to its label, GaugeRangeDef, live value pointer, decimal places, and
// availability for the active PlaneProfile.
//
// To add a new variable:
//   1. Add an entry to DisplayVarId (before COUNT)
//   2. Add a case in resolveDisplayVar() with label, range, pointer, decimals
//   3. If it needs a GaugeRangeDef, add one in PlaneProfile and PlaneProfiles
//   4. If it needs a value, add a float in EngineState
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// DisplayVarId — identifies a displayable measurement.
// ---------------------------------------------------------------------------
enum class DisplayVarId : uint8_t {
    OIL_TEMP,
    OIL_PRESS,
    BATTERY,
    OAT,
    FUEL_FLOW,
    FUEL_REM,
    HP,
    CARB_TEMP,
    CDT,
    IAT,
    EGT_DIF,
    MAP,
    // ---
    COUNT          // must be last — gives the total number of variables
};

// ---------------------------------------------------------------------------
// DisplayVarInfo — resolved description of one displayable variable.
//
// resolveDisplayVar() fills this from a DisplayVarId + the active
// PlaneProfile + EngineState.
// ---------------------------------------------------------------------------
struct DisplayVarInfo {
    const char*          label;     // short label shown on the gauge
    const GaugeRangeDef* range;     // scale, redline, color bands
    float*               valuePtr;  // pointer to the live value in EngineState
    int                  decimals;  // number of decimal places to display
    bool                 available; // false if the profile doesn't support this measurement
};

// ---------------------------------------------------------------------------
// resolveDisplayVar — maps a DisplayVarId to its concrete display info.
//
// Returns a DisplayVarInfo with `available = false` if the active profile
// doesn't support the requested measurement (e.g. CARB_TEMP on a turbo plane).
// ---------------------------------------------------------------------------
inline DisplayVarInfo resolveDisplayVar(DisplayVarId id,
                                        const PlaneProfile& prof,
                                        EngineState& state) {
    //                  label    range           valuePtr         dec  available
    switch (id) {
        case DisplayVarId::OIL_TEMP:
            return { "O-T",   &prof.oilT,    &state.oilT,    0, prof.hasOilT };
        case DisplayVarId::OIL_PRESS:
            return { "O-P",   &prof.oilP,    &state.oilP,    0, prof.hasOilP };
        case DisplayVarId::BATTERY:
            return { "BAT",   &prof.bat,     &state.bat,     1, prof.hasBat };
        case DisplayVarId::OAT:
            return { "OAT-C", &prof.oat,     &state.oat,     1, prof.hasOat };
        case DisplayVarId::FUEL_FLOW:
            return { "GPH",   &prof.ff,      &state.ff,      1, prof.hasFf };
        case DisplayVarId::FUEL_REM:
            return { "REM",   &prof.fuelRem, &state.fuelRem, 1, prof.hasFuelRem };
        case DisplayVarId::HP:
            return { "HP",    &prof.hp,      &state.hp,      0, prof.hasHp };
        case DisplayVarId::CARB_TEMP:
            return { "CRB",   &prof.crb,     &state.crb,     0, prof.hasCrb };
        case DisplayVarId::CDT:
            return { "CDT",   &prof.cdt,     &state.cdt,     0, prof.hasCdt };
        case DisplayVarId::IAT:
            return { "IAT",   &prof.iat,     &state.iat,     0, prof.hasIat };
        case DisplayVarId::EGT_DIF:
            return { "DIF",   &prof.dif,     &state.dif,     0, prof.hasDif };
        case DisplayVarId::MAP:
            return { "MAP",   &prof.map,     &state.map,     1, prof.hasMap };
        default:
            return { "???",   nullptr,       nullptr,        0, false };
    }
}

// ---------------------------------------------------------------------------
// decimalsFor — returns the canonical decimal count for a variable.
// Use this anywhere you need to format a value for display, so all gauges
// showing the same variable stay in sync.
// ---------------------------------------------------------------------------
inline int decimalsFor(DisplayVarId id, const PlaneProfile& prof,
                       EngineState& state) {
    return resolveDisplayVar(id, prof, state).decimals;
}

// ---------------------------------------------------------------------------
// getAvailableDisplayVars — fills an array with the IDs that the active
// profile supports.  Useful for building a selection UI later.
//
// Returns the count of available variables written to `out`.
// Caller must provide an array of at least (int)DisplayVarId::COUNT elements.
// ---------------------------------------------------------------------------
inline int getAvailableDisplayVars(DisplayVarId* out,
                                   const PlaneProfile& prof,
                                   EngineState& state) {
    int n = 0;
    for (uint8_t i = 0; i < (uint8_t)DisplayVarId::COUNT; i++) {
        DisplayVarInfo info = resolveDisplayVar((DisplayVarId)i, prof, state);
        if (info.available) {
            out[n++] = (DisplayVarId)i;
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
// getDisplayVarLabel — returns the display label for a variable ID.
// Handy for a selection menu without needing the full resolve.
// ---------------------------------------------------------------------------
inline const char* getDisplayVarLabel(DisplayVarId id) {
    switch (id) {
        case DisplayVarId::OIL_TEMP:   return "O-T";
        case DisplayVarId::OIL_PRESS:  return "O-P";
        case DisplayVarId::BATTERY:    return "BAT";
        case DisplayVarId::OAT:        return "OAT-C";
        case DisplayVarId::FUEL_FLOW:  return "GPH";
        case DisplayVarId::FUEL_REM:   return "REM";
        case DisplayVarId::HP:         return "HP";
        case DisplayVarId::CARB_TEMP:  return "CRB";
        case DisplayVarId::CDT:        return "CDT";
        case DisplayVarId::IAT:        return "IAT";
        case DisplayVarId::EGT_DIF:    return "DIF";
        case DisplayVarId::MAP:        return "MAP";
        default:                       return "???";
    }
}
