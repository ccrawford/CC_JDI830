#pragma once

#include <algorithm>   // std::max_element, std::min_element

// ---------------------------------------------------------------------------
// EngineState — all engine parameters in one place.
//
// This is still a plain struct (all public), but it knows how to recompute
// its own derived fields via updateCalculated().  This keeps the calculation
// logic next to the data it operates on — a basic C++ cohesion principle
// called "putting behavior with data."  The struct doesn't depend on
// PlaneProfile; the caller passes in what's needed (cylinder count, etc.).
// ---------------------------------------------------------------------------
struct EngineState {
    static constexpr int MAX_CYLINDERS = 6;

    float egt[MAX_CYLINDERS] = {1462, 1396, 1500, 1401, 1199, 1466};   // Engine gas temp for cylinders
    float cht[MAX_CYLINDERS] = {313,  312,  395,  314,  320,  386};    // Cylinder Head Temp for all cylinders
    float tit1        = 1535;   // Turbine Inlet Temp #1
    float tit2        = 1500;   // Turbine Inlet Temp #2
    float oilT   = 129;    // Oil Temp
    float oilP   = 64;     // Oil Pressure
    bool  isCold = false;   // CHT cooling rate (deg/min) CALCULATED
    float coldRate = 0.0;   // deg/min cooling CALCULATED
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
    float mpg    = 16.0f;   // Miles per gallon passed in.
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
    unsigned long lastCoolingUpdate = 0;  // Used to determine cooling rate
    float chtPrev[MAX_CYLINDERS] = {0};  // Previous CHT snapshot for cooling rate calc

    // Used for display
    bool showNormalized = false;

    // Copies of the selected cylinder's values — updated each frame
    // so BottomBar page pointers stay valid regardless of which cylinder is selected
    float egtSelected = 0;
    float chtSelected = 0;

    // Raw single-value inputs from the sim.  When reportsSingleEgtCht is
    // true, set() stores the sim's one EGT/CHT here instead of writing
    // directly to the per-cylinder arrays.  spreadCylinders() then uses
    // these as the base to generate fake per-cylinder values.
    float egtRaw = 0;
    float chtRaw = 0;

    // Per-cylinder drift state for fake cylinder spread.
    // Each cylinder gets a slowly-wandering offset so the bars don't move
    // in lockstep.  Stored here (not in PlaneProfile) because it's mutable
    // runtime state, not a profile constant.
    float egtDrift[MAX_CYLINDERS] = {0};
    float chtDrift[MAX_CYLINDERS] = {0};

    // Previous raw values — used to detect whether the sim input is changing.
    // When the delta is large, drift is nudged actively (cylinders respond at
    // different rates).  When stable, drift decays toward zero so the bars
    // settle down — just like a real engine at cruise.
    float egtRawPrev = 0;
    float chtRawPrev = 0;

    // -----------------------------------------------------------------
    // updateCalculated — recompute every CALCULATED field from inputs.
    //
    // Call once per frame after new sim values arrive.  numCylinders
    // comes from the active PlaneProfile so this struct doesn't need
    // to know about profiles at all.
    // -----------------------------------------------------------------
    void updateCalculated(int numCylinders) {
        // --- EGT peak & differential ---
        auto egtMax = std::max_element(egt, egt + numCylinders);
        auto egtMin = std::min_element(egt, egt + numCylinders);
        egtPeak    = *egtMax;
        egtPeakCyl = static_cast<int>(egtMax - egt);
        dif        = *egtMax - *egtMin;

        // --- CHT peak ---
        auto chtMax = std::max_element(cht, cht + numCylinders);
        chtPeak    = *chtMax;
        chtPeakCyl = static_cast<int>(chtMax - cht);

        // --- Intercooler effectiveness ---
        cdtLessIat = cdt - iat;

        // --- Fuel calculations ---
        used = fuelCapacity - fuelRem;

        if (ff > 0.01f) {
            endurance = (fuelRem / ff) * 60.0f;   // minutes
        } else {
            endurance = 0;
        }

        // Fuel required & reserve to waypoint
        // req = gallons needed at current ff to cover waypointDist
        // (waypointDist is in NM; groundspeed isn't available yet,
        //  so for now we approximate with ff directly — placeholder)
        if (ff > 0.01f && waypointDist > 0) {
            // TODO: needs groundspeed for a real calculation
            if(mpg != 0) req = waypointDist / mpg;
            else req = 0;
            res = fuelRem - req;
        } else {
            req = 0;
            res = fuelRem;
        }

        // --- Selected cylinder copies ---
        if (selectedCylinder >= 0 && selectedCylinder < numCylinders) {
            egtSelected = egt[selectedCylinder];
            chtSelected = cht[selectedCylinder];
        }

        // --- CHT cooling rate ---
        // Computed in updateCoolingRate() which needs millis() delta,
        // so it stays as a separate call driven by CC_JDI830::update().
    }

    // -----------------------------------------------------------------
    // updateCoolingRate — compute worst-case CHT cooling across all
    // cylinders, in degrees per minute.
    //
    // Called every frame from updateCalculatedFields(), but only
    // recalculates every COOLING_INTERVAL_MS (3 s).  On the first call
    // (lastCoolingUpdate == 0) it just snapshots CHT and returns so
    // there's no spurious rate at power-up.
    //
    // Only COOLING (temperature dropping) counts.  The reported rate is
    // the worst (fastest) single cylinder, not an average — one cylinder
    // shock-cooling is the real hazard.
    // -----------------------------------------------------------------
    void updateCoolingRate(unsigned long nowMs, int numCylinders) {
        static constexpr unsigned long COOLING_INTERVAL_MS = 3000;

        // Bootstrap: first call after power-up or profile switch —
        // snapshot current CHTs so we have a baseline next time.
        if (lastCoolingUpdate == 0) {
            for (int i = 0; i < numCylinders; i++) chtPrev[i] = cht[i];
            lastCoolingUpdate = nowMs;
            return;
        }

        // Not time yet — skip
        unsigned long elapsed = nowMs - lastCoolingUpdate;
        if (elapsed < COOLING_INTERVAL_MS) return;

        // Compute worst (fastest) cooling rate across all cylinders
        float worstRate = 0.0f;
        float minutes   = elapsed / 60000.0f;

        for (int i = 0; i < numCylinders; i++) {
            float drop = chtPrev[i] - cht[i];   // positive = cooling
            if (drop > 0.0f) {
                float rate = drop / minutes;     // deg/min
                if (rate > worstRate) worstRate = rate;
            }
            chtPrev[i] = cht[i];                 // snapshot for next interval
        }

        coldRate = worstRate;
        isCold   = (coldRate >= 60.0f);          // matches RED zone threshold
        lastCoolingUpdate = nowMs;
    }

    // -----------------------------------------------------------------
    // spreadCylinders — generate fake per-cylinder values from a single
    // reported EGT/CHT (the sim's peak value).
    //
    // Each cylinder has a fixed "personality" offset so it consistently
    // runs hotter or cooler — just like a real engine where cooling
    // airflow and fuel distribution vary by cylinder position.
    //
    // Drift behavior models real thermal response:
    //   - When the sim value is CHANGING, each cylinder's drift is nudged
    //     at a slightly different random rate, simulating different thermal
    //     mass / airflow.  This creates temporary spread during transitions.
    //   - When the sim value is STABLE, drift decays exponentially toward
    //     zero so the bars settle into their personality offsets and hold
    //     still — just like a real engine at cruise.
    //
    // reportedEgt/Cht = the single value from the sim (always the peak).
    // spreadPct       = max spread as a fraction, e.g. 0.09 → 91-100%.
    // -----------------------------------------------------------------
    void spreadCylinders(int numCylinders, float reportedEgt, float reportedCht,
                         float spreadPct) {
        // Fixed per-cylinder personality offsets (fraction of spreadPct).
        // Hand-tuned to look like a real engine: rear cylinders run hotter,
        // front cooler.  The cylinder with 0.0 *is* the peak.
        static constexpr float kPersonality6[] = {0.30f, 0.70f, 0.00f, 0.85f, 1.00f, 0.45f};
        static constexpr float kPersonality4[] = {0.25f, 0.00f, 0.75f, 1.00f};

        const float* personality = (numCylinders >= 6) ? kPersonality6 : kPersonality4;

        // How much the sim value must change per frame to count as "moving".
        // Below this threshold we consider the engine at steady state.
        constexpr float kChangeThreshold = 0.5f;

        // During transitions: max drift and step size per frame.
        // At 10 Hz this means drift builds at ~2°/sec max, capped at ±8° EGT.
        constexpr float kMaxEgtDrift =  8.0f;
        constexpr float kMaxChtDrift =  4.0f;
        constexpr float kDriftStep   =  0.2f;

        // During steady state: exponential decay factor per frame.
        // 0.97 at 10 Hz → drift halves roughly every 2.3 seconds, so bars
        // settle within ~10 seconds of the sim value stabilizing.
        constexpr float kDecay = 0.97f;

        // Detect whether the sim input is changing
        float egtDelta = reportedEgt - egtRawPrev;
        float chtDelta = reportedCht - chtRawPrev;
        bool egtChanging = (egtDelta > kChangeThreshold) || (egtDelta < -kChangeThreshold);
        bool chtChanging = (chtDelta > kChangeThreshold) || (chtDelta < -kChangeThreshold);

        egtRawPrev = reportedEgt;
        chtRawPrev = reportedCht;

        for (int i = 0; i < numCylinders; i++) {
            // Each cylinder gets its own random factor so they don't move
            // in lockstep.  Using a different random call per cylinder per
            // measurement gives 4 independent streams for a 4-cyl engine.
            if (egtChanging) {
                float rnd = (random(201) - 100) / 100.0f;  // -1.0 .. +1.0
                egtDrift[i] += rnd * kDriftStep;
                if (egtDrift[i] >  kMaxEgtDrift) egtDrift[i] =  kMaxEgtDrift;
                if (egtDrift[i] < -kMaxEgtDrift) egtDrift[i] = -kMaxEgtDrift;
            } else {
                // Steady state — decay drift toward zero.  Once it's tiny
                // (< 0.1°) just snap to zero so the display holds rock-steady.
                egtDrift[i] *= kDecay;
                if (egtDrift[i] < 0.1f && egtDrift[i] > -0.1f) egtDrift[i] = 0;
            }

            if (chtChanging) {
                float rnd = (random(201) - 100) / 100.0f;
                chtDrift[i] += rnd * kDriftStep;
                if (chtDrift[i] >  kMaxChtDrift) chtDrift[i] =  kMaxChtDrift;
                if (chtDrift[i] < -kMaxChtDrift) chtDrift[i] = -kMaxChtDrift;
            } else {
                chtDrift[i] *= kDecay;
                if (chtDrift[i] < 0.1f && chtDrift[i] > -0.1f) chtDrift[i] = 0;
            }

            // Final value = peak minus fixed personality offset + drift
            float egtOffset = reportedEgt * spreadPct * personality[i];
            egt[i] = reportedEgt - egtOffset + egtDrift[i];

            float chtOffset = reportedCht * spreadPct * personality[i];
            cht[i] = reportedCht - chtOffset + chtDrift[i];
        }
    }
};
