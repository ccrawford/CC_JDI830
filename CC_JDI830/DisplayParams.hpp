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
//   3. Add a case in PlaneProfile::isAvailable()
//   4. If it needs a GaugeRangeDef, add one in PlaneProfile and PlaneProfiles
//   5. If it needs a value, add a float in EngineState
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
    FUEL_REQ,
    COLD,
    FUEL_USED,
    FUEL_RES,          // fuel reserve at waypoint (calculated)
    INTERCOOLER_EFF,   // intercooler cooling effectiveness: CDT - IAT (calculated)
    // ---
    COUNT          // must be last — gives the total number of variables
};

// ---------------------------------------------------------------------------
// PlaneProfile::isAvailable — single source of truth for "does this profile
// support variable X?"
//
// Defined here (not in PlaneProfile.hpp) because it needs the full
// DisplayVarId enum, and DisplayParams.hpp already includes PlaneProfile.hpp.
// This is a common C++ pattern for breaking circular dependencies: you
// *declare* a method in the struct's header, then *define* it in a later
// header that has visibility of the types it needs.
// ---------------------------------------------------------------------------
inline bool PlaneProfile::isAvailable(DisplayVarId id) const {
    switch (id) {
        case DisplayVarId::OIL_TEMP:   return hasOilT;
        case DisplayVarId::OIL_PRESS:  return hasOilP;
        case DisplayVarId::BATTERY:    return hasBat;
        case DisplayVarId::OAT:        return hasOat;
        case DisplayVarId::FUEL_FLOW:  return hasFf;
        case DisplayVarId::FUEL_REM:   return hasFuelRem;
        case DisplayVarId::HP:         return hasHp;
        case DisplayVarId::CARB_TEMP:  return hasCrb;
        case DisplayVarId::CDT:        return hasCdt;
        case DisplayVarId::IAT:        return hasIat;
        case DisplayVarId::EGT_DIF:    return hasDif;
        case DisplayVarId::MAP:        return hasMap;
        case DisplayVarId::FUEL_REQ:   return hasReq;
        case DisplayVarId::FUEL_USED:  return hasUsed;
        case DisplayVarId::COLD:       return hasColdRate;
        case DisplayVarId::FUEL_RES:   return hasRes;
        case DisplayVarId::INTERCOOLER_EFF: return hasCdt && hasIat;  // derived — no dedicated flag
        default:                       return false;
    }
}

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
//
// The `available` field comes from prof.isAvailable() — one place to maintain.
// ---------------------------------------------------------------------------
inline DisplayVarInfo resolveDisplayVar(DisplayVarId id,
                                        const PlaneProfile& prof,
                                        EngineState& state) {
    bool avail = prof.isAvailable(id);
    //                  label    range           valuePtr         dec  available
    switch (id) {
        case DisplayVarId::OIL_TEMP:
            return { "O-T",   &prof.oilT,    &state.oilT,    0, avail };
        case DisplayVarId::OIL_PRESS:
            return { "O-P",   &prof.oilP,    &state.oilP,    0, avail };
        case DisplayVarId::BATTERY:
            return { "BAT",   &prof.bat,     &state.bat,     1, avail };
        case DisplayVarId::OAT:
            return { "OAT-C", &prof.oat,     &state.oat,     0, avail }; // No decimals for OAT
        case DisplayVarId::FUEL_FLOW:
            return { "GPH",   &prof.ff,      &state.ff,      1, avail };
        case DisplayVarId::FUEL_REM:
            return { "REM",   &prof.fuelRem, &state.fuelRem, 1, avail };
        case DisplayVarId::HP:
            return { "HP",    &prof.hp,      &state.hp,      0, avail };
        case DisplayVarId::CARB_TEMP:
            return { "CRB",   &prof.crb,     &state.crb,     0, avail };
        case DisplayVarId::CDT:
            return { "CDT",   &prof.cdt,     &state.cdt,     0, avail };
        case DisplayVarId::IAT:
            return { "IAT",   &prof.iat,     &state.iat,     0, avail };
        case DisplayVarId::EGT_DIF:
            return { "DIF",   &prof.dif,     &state.dif,     0, avail };
        case DisplayVarId::MAP:
            return { "MAP",   &prof.map,     &state.map,     1, avail };
        case DisplayVarId::FUEL_REQ:
            return { "REQ",   &prof.req,     &state.req,     1, avail };
        case DisplayVarId::FUEL_USED:
            return { "USD",   &prof.used,    &state.used,    1, avail };
        case DisplayVarId::COLD:
            return { "CLD",   &prof.coldRate,    &state.coldRate,    0, avail };
        case DisplayVarId::FUEL_RES:
            return { "RES",   &prof.res,         &state.res,         1, avail };
        case DisplayVarId::INTERCOOLER_EFF:
            return { "C-I",   &prof.intercoolerEff, &state.cdtLessIat, 0, avail };
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
        auto id = (DisplayVarId)i;
        if (prof.isAvailable(id)) {
            out[n++] = id;
        }
    }
    return n;
}