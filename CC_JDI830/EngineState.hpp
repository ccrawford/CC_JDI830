#pragma once

// ---------------------------------------------------------------------------
// EngineState — all engine parameters in one place.
//
// In the real project this would be populated from MSFS via
// MobiFlight/SimConnect.  For the demo, values are set in main.cpp.
// ---------------------------------------------------------------------------
struct EngineState {
    float egt[6] = {1462, 1396, 1500, 1401, 1199, 1466};   // Engine gas temp for cylinders
    float cht[6] = {313,  312,  395,  314,  320,  386};    // Cylinder Head Temp for all cylinders
    float tit1        = 1535;   // Turbine Inlet Temp #1
    float tit2        = 1500;   // Turbine Inlet Temp #2
    float oilT   = 129;    // Oil Temp
    float oilP   = 64;     // Oil Pressure
    bool  isCold = false;   // CHT cooling rate (deg/min) CALCULATED
    float bat    = 25.1f;  // Battery Volts
    float oat    = 18;  // Outside Air Temp (C)
    float dif    = 40;  // Hottest minus coldest EGT CALCULATED
    float crb    = 40;  // Carburator Air Temp (only allowed if IAT not present)
    float cdt    = 145;  // COmpressor Discharge Temp (only allowed if CRB not present)
    float iat    = 145;  // Intercooler Air Temp
    float cdtLessIat    = -45;  // Intercooler cooling CALCULATED
    float rpm    = 2570;   // Engine RPM
    float map    = 26.3f;  // Engine Manifold Pressure inHg
    float fuelRem = 58.9f;  // Calculated fuel remaining
    float waypointDist = 0;  // Distnace to next Waypoint (input)
    float req = 12.7f;  // Calculated fuel required to waypoint CALCULATED
    float res = 55.2f;  // Calculated fuel reserve at waypoint CALCULATED
    float mpg    = 16.0f;   // Miles per gallon (calculated) CALCULATED
    float endurance = 185;   // Endurance in minutes time remaining until fuel exhaustion CALCULATED
    float ff    = 16.0f;   // Fuel Flow Gallons per hour
    float used   = 7.2;     // Calculated fuel used (gallons)
    float fuelCapacity = 60.0f;  // Total usable fuel (gallons) — reported by sim
    float egtPeak    = 0;   // Calculated
    int   egtPeakCyl = 0;   // Calculated
    float chtPeak    = 0;   // Calculated
    int   chtPeakCyl = 0;   // Calculated
    float hp     = 72;

    // Used in calculations
    int   selectedCylinder = 3;  // Used for display. Max is number of cylinders + 1 for TIT
    float peakTemp = 0;   // Used in leaning
    bool  peakReached = false;   // Used in leaning

    // Copies of the selected cylinder's values — updated each frame
    // so BottomBar page pointers stay valid regardless of which cylinder is selected
    float egtSelected = 0;
    float chtSelected = 0;
};

// EngineState instance lives inside CC_JDI830 as a member (curState).
