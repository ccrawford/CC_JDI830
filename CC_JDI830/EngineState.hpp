#pragma once

#include <algorithm>   // std::max_element, std::min_element
#include <math.h>      // lroundf

// ---------------------------------------------------------------------------
// emaStep — one step of an exponential moving average, with optional
// "snap" escape for large excursions.
//
// alpha ∈ (0, 1].  Higher alpha = more responsive (less smoothing);
// lower alpha = smoother (slower to track).  Call once per frame.
//
// snapDelta: if |raw - smoothed| exceeds this, bypass the EMA and just
// copy raw into smoothed.  This avoids the ugly slow-ramp behavior when
// the source makes a real, deliberate jump (throttle snap, profile
// switch, gauge sweeping up from 0 at boot, etc.) and prevents the
// displayed value from missing every intermediate reading during the
// ramp.  Pass 0 (or a very large number) to disable snapping.
//
// `float&` is a C++ reference — like a pointer that can't be null and
// can't be rebound.  Compiles down to the same code as `float*` but
// reads more naturally for in-place modification.  `inline` lets the
// compiler drop this directly into each call site with zero call
// overhead — critical on RP2350 where we run it ~20× per frame.
// ---------------------------------------------------------------------------
inline void emaStep(float& smoothed, float raw, float alpha, float snapDelta = 0.0f) {
    // fabsf is the float-flavored absolute value.  Using the float variant
    // (not fabs, which is double) avoids a hidden float→double→float
    // round-trip on the Cortex-M33 — cheap, but free is cheaper.
    if (snapDelta > 0.0f && fabsf(raw - smoothed) > snapDelta) {
        smoothed = raw;
        return;
    }
    smoothed += alpha * (raw - smoothed);
}

// ---------------------------------------------------------------------------
// SmoothingAlpha — per-parameter EMA strengths.
//
// `static constexpr float` at struct scope = compile-time constant with
// no storage cost (better than #define: type-safe, scoped, debuggable).
// Tuned for ~10 Hz update rate.  Settle-to-95% rough rule of thumb:
//   alpha 0.05 → ~1 s   alpha 0.15 → ~300 ms   alpha 0.30 → ~150 ms
// ---------------------------------------------------------------------------
struct SmoothingAlpha {
    static constexpr float bat   = 0.05f;   // voltage barely moves in real life
    static constexpr float rpm   = 0.35f;   // pilot expects RPM to track throttle
    static constexpr float map   = 0.30f;
    static constexpr float oilT  = 0.05f;   // slow thermal mass
    static constexpr float oilP  = 0.15f;
    static constexpr float ff    = 0.20f;
    static constexpr float egt   = 0.15f;
    static constexpr float cht   = 0.10f;
    static constexpr float tit   = 0.15f;
    static constexpr float oat   = 0.05f;
    static constexpr float misc  = 0.15f;   // crb / cdt / iat
};

// Per-parameter snap thresholds.  If the gap between raw and smoothed
// exceeds this, emaStep() abandons the gradual blend and snaps directly
// to the raw value.  Tuned to be larger than any real-world jitter but
// smaller than a "deliberate" change (throttle snap, profile switch,
// power-on ramp from 0).  Order of magnitude rule of thumb: ~10× the
// expected noise amplitude on each signal.
struct SmoothingSnap {
    static constexpr float bat   =   3.0f;   // volts — anything >3V off is a real event
    static constexpr float rpm   = 300.0f;
    static constexpr float map   =   5.0f;   // inHg
    static constexpr float oilT  =  40.0f;   // °F
    static constexpr float oilP  =  20.0f;   // psi
    static constexpr float ff    =   5.0f;   // gph
    static constexpr float egt   = 200.0f;   // °F
    static constexpr float cht   = 100.0f;   // °F
    static constexpr float tit   = 200.0f;   // °F
    static constexpr float oat   =  20.0f;   // °C
    static constexpr float misc  =  40.0f;   // crb / cdt / iat — °F
};

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

    // ---- Display values (refreshed at ~4 Hz) ----------------------------
    // These are the public fields that gauges, alarms, peak detection,
    // lean-find, and the bottom bar all read.  They're snapshots of the
    // *Smoothed accumulators below, copied every DISPLAY_REFRESH_MS to
    // match the real EDM830's ~4 Hz update cadence.  Holding gauges still
    // between refreshes makes them feel like a real instrument instead
    // of jittering with every frame.
    float egt[MAX_CYLINDERS] = {0};   // Engine gas temp per cylinder
    float cht[MAX_CYLINDERS] = {0};   // Cylinder head temp per cylinder
    float tit1   = 0;     // Turbine Inlet Temp #1
    float tit2   = 0;     // Turbine Inlet Temp #2
    float oilT   = 0;     // Oil Temp
    float oilP   = 0;     // Oil Pressure
    bool  isCold = false; // CHT cooling rate triggered (CALCULATED)
    float coldRate = 0.0; // deg/min cooling (CALCULATED)
    float bat    = 0;     // Battery Volts
    float oat    = 0;     // Outside Air Temp
    float dif    = 0;     // Hottest minus coldest EGT (CALCULATED)
    float crb    = 0;     // Carburator Air Temp
    float cdt    = 0;     // Compressor Discharge Temp
    float iat    = 0;     // Intercooler Air Temp
    float cdtLessIat = 0; // Intercooler cooling (CALCULATED)
    float rpm    = 0;     // Engine RPM (rounded to nearest 10 for display)
    float map    = 0;     // Engine Manifold Pressure inHg
    float ff     = 0;     // Fuel Flow GPH

    // ---- EMA accumulators (updated every frame) -------------------------
    // The smoothing runs at full frame rate (~100 Hz) on these so the
    // EMA sees every MobiFlight sample.  Display fields above are
    // refreshed from these on the 4 Hz tick.  Keeping the EMA at full
    // rate gives much better noise rejection than running it at 4 Hz:
    // each MobiFlight delta contributes to the average.
    float egtSmoothed[MAX_CYLINDERS] = {0};
    float chtSmoothed[MAX_CYLINDERS] = {0};
    float tit1Smoothed = 0;
    float tit2Smoothed = 0;
    float oilTSmoothed = 0;
    float oilPSmoothed = 0;
    float batSmoothed  = 0;
    float oatSmoothed  = 0;
    float crbSmoothed  = 0;
    float cdtSmoothed  = 0;
    float iatSmoothed  = 0;
    float rpmSmoothed  = 0;   // un-rounded; rpm = round(rpmSmoothed, 10) at refresh
    float mapSmoothed  = 0;
    float ffSmoothed   = 0;

    // ---- Raw values written directly by CC_JDI830::set() ---------------
    // Hold the most recent MobiFlight input.  EMA pass blends each into
    // the matching *Smoothed accumulator every frame.  Per-cylinder
    // EGT/CHT raws come from set() (per-cylinder data) or from
    // spreadCylinders() (synthetic per-cylinder from a single value).
    float egtRawPerCyl[MAX_CYLINDERS] = {0};
    float chtRawPerCyl[MAX_CYLINDERS] = {0};
    float tit1Raw = 0;
    float tit2Raw = 0;
    float oilTRaw = 0;
    float oilPRaw = 0;
    float batRaw  = 0;
    float oatRaw  = 0;
    float crbRaw  = 0;
    float cdtRaw  = 0;
    float iatRaw  = 0;
    float rpmRaw  = 0;
    float mapRaw  = 0;
    float ffRaw   = 0;

    // 4 Hz display refresh state
    static constexpr unsigned long DISPLAY_REFRESH_MS = 250;  // 4 Hz
    unsigned long lastDisplayRefreshMs = 0;

    // ---- Fuel / range fields (not smoothed — change slowly already) -----
    float fuelRem = 0;       // Fuel remaining (gallons) — reported by sim
    float waypointDist = 0;  // Distance to next waypoint (NM)
    float req = 0;           // Fuel required to waypoint (CALCULATED)
    float res = 0;           // Fuel reserve at waypoint (CALCULATED)
    float mpg = 0;           // Miles per gallon (input)
    float endurance = 0;     // Minutes of fuel remaining (CALCULATED)
    float used   = 0;        // Fuel used (gallons) — reported by sim
    float fuelCapacity = 0;  // Total usable fuel (gallons) — reported by sim
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

    // Live values for the EGT-switch cylinder/TIT cycle.  Updated each frame
    // by CC_JDI830 from egt[cycleIdx]/cht[cycleIdx] (cylinder slots) or from
    // tit1/tit2 (TIT slots).  The bottom-bar synthesized page points at these
    // so its value-change tracker sees live updates between page advances.
    float egtCycle = 0;
    float chtCycle = 0;

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
    // updateCalculated — two-tier update:
    //   (1) EMA pass: every call, blend *Raw → *Smoothed.  This is what
    //       gives us full-rate noise rejection — every MobiFlight delta
    //       contributes to the average.
    //   (2) Display refresh: every DISPLAY_REFRESH_MS (~4 Hz), copy
    //       *Smoothed into the public display fields and recompute peaks,
    //       differentials, fuel calcs, etc.  This matches the real
    //       EDM830's ~4 Hz refresh and stops gauges from jittering on
    //       every frame.
    //
    // nowMs is millis() from the caller, used to time the 4 Hz gate.
    // -----------------------------------------------------------------
    void updateCalculated(int numCylinders, unsigned long nowMs) {
        // --- Tier 1: EMA pass (every frame) ----------------------------
        // Per-cylinder EGT/CHT raws come either from set() writing real
        // sim values directly, or from spreadCylinders() writing synthetic
        // ones based on egtRaw/chtRaw.  Either way, they're already in
        // egtRawPerCyl[] / chtRawPerCyl[] by the time we get here.
        for (int i = 0; i < numCylinders; i++) {
            emaStep(egtSmoothed[i], egtRawPerCyl[i], SmoothingAlpha::egt, SmoothingSnap::egt);
            emaStep(chtSmoothed[i], chtRawPerCyl[i], SmoothingAlpha::cht, SmoothingSnap::cht);
        }
        emaStep(tit1Smoothed, tit1Raw, SmoothingAlpha::tit,  SmoothingSnap::tit);
        emaStep(tit2Smoothed, tit2Raw, SmoothingAlpha::tit,  SmoothingSnap::tit);
        emaStep(oilTSmoothed, oilTRaw, SmoothingAlpha::oilT, SmoothingSnap::oilT);
        emaStep(oilPSmoothed, oilPRaw, SmoothingAlpha::oilP, SmoothingSnap::oilP);
        emaStep(batSmoothed,  batRaw,  SmoothingAlpha::bat,  SmoothingSnap::bat);
        emaStep(oatSmoothed,  oatRaw,  SmoothingAlpha::oat,  SmoothingSnap::oat);
        emaStep(crbSmoothed,  crbRaw,  SmoothingAlpha::misc, SmoothingSnap::misc);
        emaStep(cdtSmoothed,  cdtRaw,  SmoothingAlpha::misc, SmoothingSnap::misc);
        emaStep(iatSmoothed,  iatRaw,  SmoothingAlpha::misc, SmoothingSnap::misc);
        emaStep(rpmSmoothed,  rpmRaw,  SmoothingAlpha::rpm,  SmoothingSnap::rpm);
        emaStep(mapSmoothed,  mapRaw,  SmoothingAlpha::map,  SmoothingSnap::map);
        emaStep(ffSmoothed,   ffRaw,   SmoothingAlpha::ff,   SmoothingSnap::ff);

        // --- Tier 2 gate: only refresh display fields at 4 Hz ----------
        // unsigned subtraction handles millis() wraparound correctly
        // (wraps every ~49 days — well past any flight).
        if (nowMs - lastDisplayRefreshMs < DISPLAY_REFRESH_MS) {
            return;
        }
        lastDisplayRefreshMs = nowMs;

        // --- Snapshot smoothed → display fields ------------------------
        for (int i = 0; i < numCylinders; i++) {
            egt[i] = egtSmoothed[i];
            cht[i] = chtSmoothed[i];
        }
        tit1 = tit1Smoothed;
        tit2 = tit2Smoothed;
        oilT = oilTSmoothed;
        oilP = oilPSmoothed;
        bat  = batSmoothed;
        oat  = oatSmoothed;
        crb  = crbSmoothed;
        cdt  = cdtSmoothed;
        iat  = iatSmoothed;
        map  = mapSmoothed;
        ff   = ffSmoothed;
        // RPM: round un-rounded EMA accumulator to nearest 10 for display.
        // lroundf rounds to nearest (away from zero on .5) — produces a
        // clean integer so the subsequent (int) cast in ArcGauge's
        // drawNumber() can't truncate 2350.0f to 2349.
        rpm = lroundf(rpmSmoothed / 10.0f) * 10.0f;

        // --- EGT peak & differential (uses display values) -------------
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
        if (ff > 0.01f) {
            endurance = (fuelRem / ff) * 60.0f;   // minutes
        } else {
            endurance = 0;
        }

        // Fuel required & reserve to waypoint.
        // req = waypointDist (NM) / mpg (NM per gal) = gallons to waypoint.
        // res = what's left in the tank after that burn.
        if (waypointDist > 0 && mpg > 0.01f) {
            req = waypointDist / mpg;
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

            // Final value = peak minus fixed personality offset + drift.
            // Write into the *raw* per-cylinder arrays — the EMA pass in
            // updateCalculated() will smooth them into egt[]/cht[] next.
            float egtOffset = reportedEgt * spreadPct * personality[i];
            egtRawPerCyl[i] = reportedEgt - egtOffset + egtDrift[i];

            float chtOffset = reportedCht * spreadPct * personality[i];
            chtRawPerCyl[i] = reportedCht - chtOffset + chtDrift[i];
        }
    }
};
