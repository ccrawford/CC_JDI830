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

    // ---- Smoothed (display) values -------------------------------------
    // These are what gauges, alarms, peak detection, lean-find, and the
    // bottom bar all read.  Updated once per frame in updateCalculated()
    // by EMA-blending the matching *Raw value.  Default 0 so the gauges
    // ramp up from rest on power-on (looks like a real instrument booting).
    float egt[MAX_CYLINDERS] = {0};   // Engine gas temp per cylinder (smoothed)
    float cht[MAX_CYLINDERS] = {0};   // Cylinder head temp per cylinder (smoothed)
    float tit1   = 0;     // Turbine Inlet Temp #1 (smoothed)
    float tit2   = 0;     // Turbine Inlet Temp #2 (smoothed)
    float oilT   = 0;     // Oil Temp (smoothed)
    float oilP   = 0;     // Oil Pressure (smoothed)
    bool  isCold = false; // CHT cooling rate triggered (CALCULATED)
    float coldRate = 0.0; // deg/min cooling (CALCULATED)
    float bat    = 0;     // Battery Volts (smoothed)
    float oat    = 0;     // Outside Air Temp (smoothed)
    float dif    = 0;     // Hottest minus coldest EGT (CALCULATED)
    float crb    = 0;     // Carburator Air Temp (smoothed)
    float cdt    = 0;     // Compressor Discharge Temp (smoothed)
    float iat    = 0;     // Intercooler Air Temp (smoothed)
    float cdtLessIat = 0; // Intercooler cooling (CALCULATED)
    float rpm    = 0;     // Engine RPM (smoothed, rounded to nearest 10 for display)
    float rpmSmoothedExact = 0;  // un-rounded EMA accumulator — kept separate so
                                 // the round-to-10 doesn't quantize the smoothing
                                 // back into itself and freeze the gauge.
    float map    = 0;     // Engine Manifold Pressure inHg (smoothed)
    float ff     = 0;     // Fuel Flow GPH (smoothed)

    // ---- Raw values written directly by CC_JDI830::set() ---------------
    // These hold the most recent MobiFlight input.  updateCalculated()
    // blends each into the corresponding smoothed field every frame.
    // Per-cylinder EGT/CHT raws are written by set() (when the sim sends
    // per-cylinder data) or by spreadCylinders() (when the sim sends only
    // a single value and we synthesize per-cylinder variation).
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
        // --- Smoothing pass --------------------------------------------
        // Blend each *Raw value (most recent MobiFlight input) into the
        // matching smoothed field.  Done first so all downstream peak/
        // dif/lean/alarm logic operates on smoothed values consistently.
        //
        // Per-cylinder EGT/CHT raws come either from set() writing real
        // sim values directly, or from spreadCylinders() writing synthetic
        // ones based on egtRaw/chtRaw.  Either way, they're already in
        // egtRawPerCyl[] / chtRawPerCyl[] by the time we get here.
        for (int i = 0; i < numCylinders; i++) {
            emaStep(egt[i], egtRawPerCyl[i], SmoothingAlpha::egt, SmoothingSnap::egt);
            emaStep(cht[i], chtRawPerCyl[i], SmoothingAlpha::cht, SmoothingSnap::cht);
        }
        emaStep(tit1, tit1Raw, SmoothingAlpha::tit,  SmoothingSnap::tit);
        emaStep(tit2, tit2Raw, SmoothingAlpha::tit,  SmoothingSnap::tit);
        emaStep(oilT, oilTRaw, SmoothingAlpha::oilT, SmoothingSnap::oilT);
        emaStep(oilP, oilPRaw, SmoothingAlpha::oilP, SmoothingSnap::oilP);
        emaStep(bat,  batRaw,  SmoothingAlpha::bat,  SmoothingSnap::bat);
        emaStep(oat,  oatRaw,  SmoothingAlpha::oat,  SmoothingSnap::oat);
        emaStep(crb,  crbRaw,  SmoothingAlpha::misc, SmoothingSnap::misc);
        emaStep(cdt,  cdtRaw,  SmoothingAlpha::misc, SmoothingSnap::misc);
        emaStep(iat,  iatRaw,  SmoothingAlpha::misc, SmoothingSnap::misc);
        // RPM smoothing happens on a separate un-rounded accumulator so
        // the round-to-10 doesn't feed back into the EMA and stall the
        // value.  (If we EMA-blended `rpm` directly and then rounded it,
        // each frame's small fractional progress would get snapped away
        // and the gauge would lock at a multiple of 10 even when the
        // raw value is offset — e.g. raw=1031 → smoothed stuck at 1020.)
        //
        // lroundf() rounds to nearest (away from zero on .5).  We use it
        // instead of `(int)(x + 0.5f)` because float storage of e.g.
        // 2350.0f can be 2349.9999..., and the int-cast in ArcGauge's
        // drawNumber() truncates — producing 2349 on the display.
        emaStep(rpmSmoothedExact, rpmRaw, SmoothingAlpha::rpm, SmoothingSnap::rpm);
        rpm = lroundf(rpmSmoothedExact / 10.0f) * 10.0f;
        emaStep(map,  mapRaw,  SmoothingAlpha::map, SmoothingSnap::map);
        emaStep(ff,   ffRaw,   SmoothingAlpha::ff,  SmoothingSnap::ff);

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
        // used = fuelCapacity - fuelRem;

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
