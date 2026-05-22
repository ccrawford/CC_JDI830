#pragma once

#include "PlaneProfile.hpp"
#include "EngineState.hpp"

// ---------------------------------------------------------------------------
// AlarmManager — priority-ordered engine alarm scanning and dismissal.
//
// The JPI EDM830 displays alarms in the bottom bar when engine parameters
// enter the RED zone of their GaugeRangeDef color bands (or exceed their
// redline for CHT/TIT which use WHITE bands instead of GREEN).
//
// Alarms are checked every frame in priority order (index 0 = highest).
// The first active alarm wins and is displayed.  The user can dismiss
// alarms via the STEP button (future): short press = 10 min snooze,
// long press = dismiss until power cycle.
//
// This class holds NO display logic — it just determines *which* alarm
// (if any) should be shown.  The caller (CC_JDI830::update) reads the
// active alarm and calls BottomBar::showMessage() itself.
// ---------------------------------------------------------------------------

// --- Helper: is a value inside any RED color band? ---
// Scans the GaugeRangeDef's color array for bands colored TFT_RED.
// Returns true if `value` falls within at least one RED band.
inline bool isInRed(float value, const GaugeRangeDef& range) {
    for (uint8_t i = 0; i < range.colorCount; i++) {
        if (range.colors[i].color == TFT_RED &&
            value >= range.colors[i].min &&
            value <= range.colors[i].max) {
            return true;
        }
    }
    return false;
}

// --- Helper: find the upper bound of the GREEN band ---
// Returns the .max of the first GREEN band found, or 0 if none.
// Used by isInRedHigh/Low to distinguish "above green" from "below green".
inline float greenUpperBound(const GaugeRangeDef& range) {
    for (uint8_t i = 0; i < range.colorCount; i++) {
        if (range.colors[i].color == TFT_GREEN)
            return range.colors[i].max;
    }
    return 0;
}

// --- Helper: lower bound of the GREEN band (.min of first GREEN) ---
inline float greenLowerBound(const GaugeRangeDef& range) {
    for (uint8_t i = 0; i < range.colorCount; i++) {
        if (range.colors[i].color == TFT_GREEN)
            return range.colors[i].min;
    }
    return 0;
}

// --- Helper: is value in a RED band ABOVE the green range? ---
// For alarms like "high oil temp" where there are RED bands on both
// ends but only the upper one should trigger this alarm entry.
inline bool isInRedHigh(float value, const GaugeRangeDef& range) {
    float greenMax = greenUpperBound(range);
    for (uint8_t i = 0; i < range.colorCount; i++) {
        if (range.colors[i].color == TFT_RED &&
            range.colors[i].min >= greenMax &&
            value >= range.colors[i].min &&
            value <= range.colors[i].max) {
            return true;
        }
    }
    return false;
}

// --- Helper: is value in a RED band BELOW the green range? ---
// For alarms like "low oil temp" or "low fuel remaining".
inline bool isInRedLow(float value, const GaugeRangeDef& range) {
    float greenMax = greenUpperBound(range);
    for (uint8_t i = 0; i < range.colorCount; i++) {
        if (range.colors[i].color == TFT_RED &&
            range.colors[i].max < greenMax &&
            value >= range.colors[i].min &&
            value <= range.colors[i].max) {
            return true;
        }
    }
    return false;
}

// Hysteresis margin as a fraction of the *trip boundary* (the red/green
// edge), not the green-band span.  Once an alarm has tripped, the value
// must retreat past the boundary by this much before the alarm clears —
// kills boundary flicker without noticeably delaying real alarm clearance.
//
// Earlier this was scaled to the green-band span, but several profiles use
// a sentinel max (e.g. endurance green = 20..9999 min).  That made the
// margin ~300 min wide and pinned the alarm forever once tripped at 0 min.
// Scaling to the boundary instead is stable regardless of the green range:
// 3% of 20 min = 0.6 min, 3% of 11.5 V ≈ 0.35 V, 3% of 200 °F = 6 °F.
//
// Helper: find the red/green boundary on the LOW side (largest RED.max
// below greenMax) or HIGH side (smallest RED.min at-or-above greenMax).
// Returns 0 if no such band exists.
static constexpr float kAlarmHysteresisFraction = 0.03f;

inline float redLowBoundary(const GaugeRangeDef& range) {
    float greenMax = greenUpperBound(range);
    float best = 0;
    bool  found = false;
    for (uint8_t i = 0; i < range.colorCount; i++) {
        if (range.colors[i].color == TFT_RED &&
            range.colors[i].max < greenMax) {
            if (!found || range.colors[i].max > best) {
                best = range.colors[i].max;
                found = true;
            }
        }
    }
    return found ? best : 0;
}

inline float redHighBoundary(const GaugeRangeDef& range) {
    float greenMax = greenUpperBound(range);
    float best = 0;
    bool  found = false;
    for (uint8_t i = 0; i < range.colorCount; i++) {
        if (range.colors[i].color == TFT_RED &&
            range.colors[i].min >= greenMax) {
            if (!found || range.colors[i].min < best) {
                best = range.colors[i].min;
                found = true;
            }
        }
    }
    return found ? best : 0;
}

// --- Hysteresis-aware versions of the IN_RED* checks ---
// `tripped` is the alarm's current latch state.  Returns the new latch
// state (true = alarm is active this frame).
//
// When tripped == false: identical to isInRed*() — alarm trips the moment
// the value enters a RED band.
// When tripped == true: alarm stays on until the value retreats past the
// boundary by kAlarmHysteresisFraction × green-band-span.
inline bool checkInRedHigh(float value, const GaugeRangeDef& range, bool tripped) {
    if (!tripped) return isInRedHigh(value, range);
    // Already tripped on the high side — clear only when value drops below
    // the high red boundary by the hysteresis margin.  Margin is a fraction
    // of the boundary itself, so it doesn't depend on the green span.
    float boundary = redHighBoundary(range);
    if (boundary <= 0) return false;
    float margin = boundary * kAlarmHysteresisFraction;
    return value >= (boundary - margin);
}

inline bool checkInRedLow(float value, const GaugeRangeDef& range, bool tripped) {
    if (!tripped) return isInRedLow(value, range);
    float boundary = redLowBoundary(range);
    if (boundary <= 0) return false;
    float margin = boundary * kAlarmHysteresisFraction;
    return value <= (boundary + margin);
}

// IN_RED can fire from either side; once tripped, stay on until we're
// past whichever boundary we crossed (margin scaled to that boundary).
inline bool checkInRed(float value, const GaugeRangeDef& range, bool tripped) {
    if (!tripped) return isInRed(value, range);
    float low  = redLowBoundary(range);
    float high = redHighBoundary(range);
    bool stillLow  = low  > 0 && value <= low  + low  * kAlarmHysteresisFraction;
    bool stillHigh = high > 0 && value >= high - high * kAlarmHysteresisFraction;
    return stillLow || stillHigh;
}

// ABOVE_REDLINE — 1% margin under redline before clearing.  Redline values
// are large (e.g. 460°F for CHT, 1650°F for TIT) so 1% is several degrees.
inline bool checkAboveRedline(float value, const GaugeRangeDef& range, bool tripped) {
    if (range.redline <= 0) return false;
    if (!tripped) return value >= range.redline;
    return value >= range.redline * 0.99f;
}

// ---------------------------------------------------------------------------
// AlarmCheck — how each alarm entry evaluates its condition.
//
// enum class is a "scoped enum" — unlike plain C enums, the values are
// namespaced (AlarmCheck::IN_RED, not just IN_RED) and won't implicitly
// convert to int.  This prevents accidental comparisons with unrelated values.
// ---------------------------------------------------------------------------
enum class AlarmCheck : uint8_t {
    IN_RED,           // value is in ANY red band
    IN_RED_HIGH,      // value is in a red band above the green band
    IN_RED_LOW,       // value is in a red band below the green band
    ABOVE_REDLINE,    // value >= GaugeRangeDef.redline (for CHT/TIT)
};

// ---------------------------------------------------------------------------
// DismissState — tracks whether the user has snoozed/dismissed an alarm.
// ---------------------------------------------------------------------------
enum class DismissState : uint8_t {
    ACTIVE,           // alarm can fire normally
    TIMED,            // dismissed, will reactivate at reactivateAt
    PERMANENT,        // dismissed until power cycle (device reset)
};

// ---------------------------------------------------------------------------
// AlarmDef — one entry in the priority-ordered alarm table.
//
// Built at runtime by buildAlarms() because the pointers target live
// EngineState and PlaneProfile instances whose addresses aren't known
// at compile time.  The struct is small (~28 bytes) and the table is
// fixed-size, so no heap allocation is needed.
// ---------------------------------------------------------------------------
struct AlarmDef {
    const char*          label;        // "CHT", "OIL", "BAT", etc.
    const float*         valuePtr;     // -> EngineState field to monitor
    const GaugeRangeDef* range;        // -> PlaneProfile range for color lookup
    const bool*          available;    // -> PlaneProfile has* flag
    AlarmCheck           check;        // how to evaluate
    int                  decimals;     // for formatting the displayed value
    bool                 timeFormat;   // true = display as H:MM instead of number
    const int*           cylPtr;       // -> cylinder number (CHT only), nullptr for others

    // --- Mutable runtime state ---
    DismissState         dismiss      = DismissState::ACTIVE;
    uint32_t             reactivateAt = 0;  // millis() timestamp for TIMED dismiss

    // Schmitt-trigger latch: once an alarm trips, it remains tripped until
    // the value has retreated past the threshold by a hysteresis margin.
    // Prevents flicker when a value sits right on a boundary (jittery
    // sim signals or values smoothed to within a hair of the limit).
    bool                 tripped      = false;
};

static constexpr int MAX_ALARMS = 13;
static constexpr uint32_t DISMISS_DURATION_MS = 600000;  // 10 minutes

// Startup grace period.  For this long after power-on or profile switch,
// scan() returns false unconditionally — gauges are still ramping up from
// 0 (smoothing settle) and would otherwise trip spurious low alarms (low
// battery, low oil temp/pressure, etc.).  Mirrors real EMS behavior: the
// JPI EDM830 hides values entirely for its first few seconds of boot.
// This will be folded into the proper startup-sequence display later.
static constexpr uint32_t ALARM_STARTUP_GRACE_MS = 5000;

// ---------------------------------------------------------------------------
// AlarmManager — scans alarm table each frame, tracks dismiss state.
// ---------------------------------------------------------------------------
class AlarmManager {
    AlarmDef _alarms[MAX_ALARMS];
    int      _numAlarms       = 0;
    int      _activeAlarmIndex = -1;   // index of currently firing alarm, -1 = none
    uint32_t _graceStartMs    = 0;     // millis() at start of current grace window
    bool     _graceActive     = false; // true while we're inside the grace window

public:
    // -----------------------------------------------------------------
    // buildAlarms — populate the alarm table for the active profile.
    //
    // Called once when the profile changes (from CC_JDI830::setProfile).
    // Each alarm entry is added in priority order (index 0 = highest).
    // Entries for unavailable measurements are skipped entirely — they
    // won't consume a slot or be checked each frame.
    //
    // The lambda `add` captures the local variables by reference ([&]),
    // which lets us write a concise local helper without making it a
    // class method.  Lambdas are just anonymous functions that can see
    // the surrounding scope — the compiler turns them into a small
    // struct with an operator() under the hood.
    // -----------------------------------------------------------------
    // Start (or restart) the post-power-on / post-profile-switch grace
     // window.  scan() will return false until this expires.  Called
    // automatically from buildAlarms(), which itself is called on init
    // and profile switch — so callers usually don't need to invoke this
    // directly.
    void beginGracePeriod(uint32_t now) {
        _graceStartMs = now;
        _graceActive  = true;
    }

    void buildAlarms(const PlaneProfile& prof, EngineState& state) {
        _numAlarms = 0;
        _activeAlarmIndex = -1;
        beginGracePeriod(millis());

        auto add = [&](const char* label, const float* val,
                       const GaugeRangeDef* range, const bool* avail,
                       AlarmCheck check, int dec, bool timeFmt = false,
                       const int* cyl = nullptr) {
            if (*avail && _numAlarms < MAX_ALARMS) {
                _alarms[_numAlarms++] = {
                    label, val, range, avail, check, dec, timeFmt, cyl,
                    DismissState::ACTIVE, 0, false
                };
            }
        };

        // Priority order per JPI EDM830 manual (highest first):
        // CHT gets a cylinder pointer so the alarm display can show which cylinder
        // is over-temp (e.g. "CHT 3").  chtPeakCyl is 0-based; the draw function
        // adds 1 for the 1-based display number.
        add("CHT",    &state.chtPeak,  &prof.cht,       &prof.hasCht,      AlarmCheck::ABOVE_REDLINE, 0, false, &state.chtPeakCyl);
        add("OIL",    &state.oilT,     &prof.oilT,      &prof.hasOilT,     AlarmCheck::IN_RED_HIGH,   0);
        add("TIT",    &state.tit1,     &prof.tit1,      &prof.hasTit1,     AlarmCheck::ABOVE_REDLINE, 0);
        add("OIL",    &state.oilT,     &prof.oilT,      &prof.hasOilT,     AlarmCheck::IN_RED_LOW,    0);
        add("CLD",    &state.coldRate, &prof.coldRate,   &prof.hasColdRate, AlarmCheck::IN_RED,        0);
        add("DIF",    &state.dif,      &prof.dif,       &prof.hasDif,      AlarmCheck::IN_RED,        0);
        add("VOLTS",    &state.bat,      &prof.bat,       &prof.hasBat,      AlarmCheck::IN_RED_HIGH,   1);
        add("VOLTS",    &state.bat,      &prof.bat,       &prof.hasBat,      AlarmCheck::IN_RED_LOW,    1);
        add("MAP",    &state.map,      &prof.map,       &prof.hasMap,      AlarmCheck::IN_RED,        1);
        add("O-P",    &state.oilP,     &prof.oilP,      &prof.hasOilP,     AlarmCheck::IN_RED,        0);
        add("LO REM", &state.fuelRem, &prof.fuelRem,    &prof.hasFuelRem,  AlarmCheck::IN_RED_LOW,    1);
        add("LO",     &state.endurance,&prof.endurance,  &prof.hasEndurance,AlarmCheck::IN_RED_LOW,    0, true);
        add("FF",     &state.ff,       &prof.ff,        &prof.hasFf,       AlarmCheck::IN_RED_LOW,    1);
    }

    // -----------------------------------------------------------------
    // scan — check all alarms in priority order, find the highest-
    // priority active alarm.  Called every frame.
    //
    // Returns true if an alarm is active (the caller should display it).
    // The `switch` on AlarmCheck dispatches to the appropriate helper —
    // this is the idiomatic C++ replacement for if/else chains on an
    // enum, and the compiler can warn if a case is missing.
    // -----------------------------------------------------------------
    bool scan(uint32_t now) {
        _activeAlarmIndex = -1;

        // Startup grace: ignore all alarms while smoothed values are
        // still ramping up from 0.  Once the window closes, clear the
        // flag and proceed normally.  Don't reset hysteresis latches
        // here — they're already false from buildAlarms().
        if (_graceActive) {
            if (now - _graceStartMs < ALARM_STARTUP_GRACE_MS) {
                return false;
            }
            _graceActive = false;
        }

        for (int i = 0; i < _numAlarms; i++) {
            AlarmDef& a = _alarms[i];

            // Skip permanently dismissed alarms
            if (a.dismiss == DismissState::PERMANENT) continue;

            // Check if timed dismiss has expired
            if (a.dismiss == DismissState::TIMED) {
                if (now < a.reactivateAt) continue;
                a.dismiss = DismissState::ACTIVE;   // timer expired, re-enable
            }

            // Skip if measurement became unavailable (shouldn't happen mid-flight
            // but defensive coding for profile hot-swap)
            if (!*a.available) continue;

            float val = *a.valuePtr;
            bool alarming = false;

            // Hysteresis: each check*() consults a.tripped to decide
            // whether to apply the wider "clear" threshold or the normal
            // "trip" threshold.  We update a.tripped with the result so
            // the next frame sees the new latch state.
            switch (a.check) {
                case AlarmCheck::IN_RED:
                    alarming = checkInRed(val, *a.range, a.tripped);
                    break;
                case AlarmCheck::IN_RED_HIGH:
                    alarming = checkInRedHigh(val, *a.range, a.tripped);
                    break;
                case AlarmCheck::IN_RED_LOW:
                    alarming = checkInRedLow(val, *a.range, a.tripped);
                    break;
                case AlarmCheck::ABOVE_REDLINE:
                    alarming = checkAboveRedline(val, *a.range, a.tripped);
                    break;
            }
            a.tripped = alarming;

            // Highest priority wins for the *displayed* alarm, but keep
            // scanning so every lower-priority alarm's hysteresis latch
            // still gets updated this frame.  Otherwise a masked alarm
            // could stay tripped forever.
            if (alarming && _activeAlarmIndex < 0) {
                _activeAlarmIndex = i;
            }
        }
        return _activeAlarmIndex >= 0;
    }

    // --- Accessors for the currently active alarm ---
    // Only valid when scan() returned true (i.e. hasActiveAlarm() == true).

    bool        hasActiveAlarm()  const { return _activeAlarmIndex >= 0; }
    const char* activeLabel()     const { return _alarms[_activeAlarmIndex].label; }
    float       activeValue()     const { return *_alarms[_activeAlarmIndex].valuePtr; }
    int         activeDecimals()  const { return _alarms[_activeAlarmIndex].decimals; }
    bool        isTimeFormat()    const { return _alarms[_activeAlarmIndex].timeFormat; }

    // Full access to the active alarm definition — used by the alarm draw
    // function to read label, value, cylinder number, time format, etc.
    const AlarmDef* activeAlarmDef() const {
        return (_activeAlarmIndex >= 0) ? &_alarms[_activeAlarmIndex] : nullptr;
    }

    // -----------------------------------------------------------------
    // Dismiss methods — called from button handlers (future STEP button).
    //
    // dismissTimed: snooze the currently displayed alarm for 10 minutes.
    //   After dismissal, scan() will skip this alarm until the timer
    //   expires, allowing the next-priority alarm to show (if any).
    //
    // dismissPermanent: silence the alarm until power cycle.
    //   Use for known conditions the pilot has acknowledged.
    // -----------------------------------------------------------------
    void dismissTimed(uint32_t now) {
        if (_activeAlarmIndex >= 0) {
            _alarms[_activeAlarmIndex].dismiss = DismissState::TIMED;
            _alarms[_activeAlarmIndex].reactivateAt = now + DISMISS_DURATION_MS;
        }
    }

    void dismissPermanent() {
        if (_activeAlarmIndex >= 0) {
            _alarms[_activeAlarmIndex].dismiss = DismissState::PERMANENT;
        }
    }
};
