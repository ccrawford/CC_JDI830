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

    // --- Mutable runtime state ---
    DismissState         dismiss      = DismissState::ACTIVE;
    uint32_t             reactivateAt = 0;  // millis() timestamp for TIMED dismiss
};

static constexpr int MAX_ALARMS = 13;
static constexpr uint32_t DISMISS_DURATION_MS = 600000;  // 10 minutes

// ---------------------------------------------------------------------------
// AlarmManager — scans alarm table each frame, tracks dismiss state.
// ---------------------------------------------------------------------------
class AlarmManager {
    AlarmDef _alarms[MAX_ALARMS];
    int      _numAlarms       = 0;
    int      _activeAlarmIndex = -1;   // index of currently firing alarm, -1 = none

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
    void buildAlarms(const PlaneProfile& prof, EngineState& state) {
        _numAlarms = 0;
        _activeAlarmIndex = -1;

        auto add = [&](const char* label, const float* val,
                       const GaugeRangeDef* range, const bool* avail,
                       AlarmCheck check, int dec, bool timeFmt = false) {
            if (*avail && _numAlarms < MAX_ALARMS) {
                _alarms[_numAlarms++] = {
                    label, val, range, avail, check, dec, timeFmt,
                    DismissState::ACTIVE, 0
                };
            }
        };

        // Priority order per JPI EDM830 manual (highest first):
        add("CHT",    &state.chtPeak,  &prof.cht,       &prof.hasCht,      AlarmCheck::ABOVE_REDLINE, 0);
        add("OIL",    &state.oilT,     &prof.oilT,      &prof.hasOilT,     AlarmCheck::IN_RED_HIGH,   0);
        add("TIT",    &state.tit1,     &prof.tit1,      &prof.hasTit1,     AlarmCheck::ABOVE_REDLINE, 0);
        add("OIL",    &state.oilT,     &prof.oilT,      &prof.hasOilT,     AlarmCheck::IN_RED_LOW,    0);
        add("CLD",    &state.coldRate, &prof.coldRate,   &prof.hasColdRate, AlarmCheck::IN_RED,        0);
        add("DIF",    &state.dif,      &prof.dif,       &prof.hasDif,      AlarmCheck::IN_RED,        0);
        add("BAT",    &state.bat,      &prof.bat,       &prof.hasBat,      AlarmCheck::IN_RED_HIGH,   1);
        add("BAT",    &state.bat,      &prof.bat,       &prof.hasBat,      AlarmCheck::IN_RED_LOW,    1);
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

            switch (a.check) {
                case AlarmCheck::IN_RED:
                    alarming = isInRed(val, *a.range);
                    break;
                case AlarmCheck::IN_RED_HIGH:
                    alarming = isInRedHigh(val, *a.range);
                    break;
                case AlarmCheck::IN_RED_LOW:
                    alarming = isInRedLow(val, *a.range);
                    break;
                case AlarmCheck::ABOVE_REDLINE:
                    alarming = (a.range->redline > 0) && (val >= a.range->redline);
                    break;
            }

            if (alarming) {
                _activeAlarmIndex = i;
                return true;    // highest priority wins — stop scanning
            }
        }
        return false;
    }

    // --- Accessors for the currently active alarm ---
    // Only valid when scan() returned true (i.e. hasActiveAlarm() == true).

    bool        hasActiveAlarm() const { return _activeAlarmIndex >= 0; }
    const char* activeLabel()    const { return _alarms[_activeAlarmIndex].label; }
    float       activeValue()    const { return *_alarms[_activeAlarmIndex].valuePtr; }
    int         activeDecimals() const { return _alarms[_activeAlarmIndex].decimals; }
    bool        isTimeFormat()   const { return _alarms[_activeAlarmIndex].timeFormat; }

    // -----------------------------------------------------------------
    // formatAlarm — writes the alarm display string into the provided
    // buffer.  Handles both numeric ("CHT 412") and time ("LO 0:18")
    // formats.
    //
    // snprintf is the safe version of sprintf — it takes a buffer size
    // and will never write past it, preventing buffer overflows.
    // -----------------------------------------------------------------
    void formatAlarm(char* buf, size_t bufSize) const {
        if (_activeAlarmIndex < 0) { buf[0] = '\0'; return; }

        const AlarmDef& a = _alarms[_activeAlarmIndex];
        float val = *a.valuePtr;

        if (a.timeFormat) {
            // Display as H:MM (endurance is stored as minutes)
            int totalMin = static_cast<int>(val);
            int hours = totalMin / 60;
            int mins  = totalMin % 60;
            snprintf(buf, bufSize, "%s %d:%02d", a.label, hours, mins);
        } else if (a.decimals > 0) {
            snprintf(buf, bufSize, "%s %.1f", a.label, val);
        } else {
            snprintf(buf, bufSize, "%s %d", a.label, static_cast<int>(val));
        }
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
