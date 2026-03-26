#pragma once

#include "Arduino.h"

// ---------------------------------------------------------------------------
// ButtonGesture — high-level gestures produced by the ButtonInput class.
//
// The mode state machine (handleGesture) decides what each gesture *means*
// based on the current DisplayMode.  ButtonInput only figures out *what the
// user did* — it doesn't know about modes.
// ---------------------------------------------------------------------------
enum class ButtonGesture : uint8_t {
    NONE,               // no gesture completed this frame

    STEP_TAP,           // STEP pressed and released quickly
    STEP_HOLD,          // STEP held for HOLD_THRESHOLD_MS (fires once)

    LF_TAP,             // LF pressed and released quickly
    LF_HOLD_SHORT,      // LF held for LF_SHORT_HOLD_MS (fires once, 3s)
    LF_HOLD_LONG,       // LF held for HOLD_THRESHOLD_MS (fires once, 5s)
    LF_RELEASE,         // LF released after any hold (for "peek" exit)

    BOTH_TAP,           // both pressed within SIMUL_WINDOW_MS, then released
    BOTH_HOLD,          // both held for HOLD_THRESHOLD_MS (fires once)
    BOTH_RELEASE,       // both released after a hold
};

// ---------------------------------------------------------------------------
// ButtonInput — gesture detection for two buttons (STEP and LF).
//
// This class receives raw press/release events from MobiFlight and produces
// ButtonGesture values.  MobiFlight handles debouncing; this class handles:
//   - tap vs hold classification (with timing thresholds)
//   - simultaneous press detection (both buttons within a small window)
//   - dual-threshold hold for LF (3s short, 5s long)
//
// Usage:
//   1. Call stepChange() / lfChange() when MobiFlight sends button state.
//   2. Call poll() once per frame in update().  It returns the highest-
//      priority gesture that completed since the last poll, or NONE.
//   3. Use isStepDown() / isLfDown() for real-time "while held" queries.
//   4. Call forceRelease() if connection to MobiFlight is lost, to prevent
//      phantom hold gestures from a button stuck in the "down" state.
//
// How simultaneous detection works:
//   When a single button is pressed, we don't immediately classify it.
//   Instead we mark it "pending" and wait up to SIMUL_WINDOW_MS to see
//   if the other button also presses.  If it does, both are reclassified
//   as a BOTH gesture.  If the window expires, the solo press is committed.
//   This adds ~150ms of latency to single-button gestures — imperceptible
//   to the user but necessary to distinguish "both" from "one then the other".
// ---------------------------------------------------------------------------
class ButtonInput {
public:
    // --- Timing constants ---------------------------------------------------
    // All times in milliseconds.  static constexpr = compile-time constant,
    // costs zero RAM on the ESP32 (the values live in flash / get inlined).
    static constexpr uint32_t TAP_THRESHOLD_MS  = 400;   // release within this = tap
    static constexpr uint32_t LF_SHORT_HOLD_MS  = 3000;  // LF normalize toggle (3s)
    static constexpr uint32_t HOLD_THRESHOLD_MS = 5000;   // all other holds (5s per manual)
    static constexpr uint32_t SIMUL_WINDOW_MS   = 150;    // window for simultaneous detection

    // --- Event inputs -------------------------------------------------------

    // Call when STEP button state changes (from MobiFlight set() message ID 20).
    void stepChange(bool pressed, uint32_t now) {
        if (pressed == _stepDown) return;  // duplicate, ignore
        _stepDown = pressed;

        if (pressed) {
            _stepDownAt = now;
            _stepPending = true;       // defer classification until simul window
            _stepHoldFired = false;
        } else {
            // Released — was this part of a "both" gesture?
            if (_bothActive) {
                _handleBothRelease(now);
            } else if (_stepPending) {
                // Released before simul window expired → solo tap candidate
                // Actual tap is emitted by poll() after the window closes,
                // but we can commit early since the button is already released.
                _stepReleasedAt = now;
            } else {
                // Solo press was already committed.  Emit tap or release.
                _stepReleasedAt = now;
            }
        }
    }

    // Call when LF button state changes (from MobiFlight set() message ID 21).
    void lfChange(bool pressed, uint32_t now) {
        if (pressed == _lfDown) return;  // duplicate, ignore
        _lfDown = pressed;

        if (pressed) {
            _lfDownAt = now;
            _lfPending = true;
            _lfShortHoldFired = false;
            _lfLongHoldFired  = false;
        } else {
            if (_bothActive) {
                _handleBothRelease(now);
            } else if (_lfPending) {
                _lfReleasedAt = now;
            } else {
                _lfReleasedAt = now;
            }
        }
    }

    // --- Polling (call once per frame in update()) --------------------------

    // Returns the highest-priority gesture that completed since last poll,
    // or NONE if nothing happened.  Call this once per frame.
    ButtonGesture poll(uint32_t now) {
        // ---- Check for "both" gestures first (highest priority) ----

        // If both buttons are pending within the simul window, commit as BOTH.
        if (_stepPending && _lfPending) {
            uint32_t earliest = (_stepDownAt < _lfDownAt) ? _stepDownAt : _lfDownAt;
            uint32_t latest   = (_stepDownAt > _lfDownAt) ? _stepDownAt : _lfDownAt;
            if (latest - earliest <= SIMUL_WINDOW_MS) {
                // Both pressed within the window — classify as BOTH.
                _stepPending = false;
                _lfPending   = false;
                _bothActive  = true;
                _bothDownAt  = earliest;
                _bothHoldFired = false;
            }
        }

        // If BOTH is active and both still held, check for hold threshold.
        if (_bothActive && _stepDown && _lfDown) {
            if (!_bothHoldFired && (now - _bothDownAt >= HOLD_THRESHOLD_MS)) {
                _bothHoldFired = true;
                return ButtonGesture::BOTH_HOLD;
            }
            return ButtonGesture::NONE;  // still waiting
        }

        // If BOTH was active but one or both released, check for tap vs hold.
        if (_bothActive && (!_stepDown || !_lfDown)) {
            // Already handled by _handleBothRelease via change events,
            // but check if a queued release gesture is ready.
            if (_pendingGesture != ButtonGesture::NONE) {
                ButtonGesture g = _pendingGesture;
                _pendingGesture = ButtonGesture::NONE;
                return g;
            }
        }

        // ---- Solo STEP gestures ----

        // Check if pending STEP should be committed (simul window expired).
        if (_stepPending && !_lfPending && (now - _stepDownAt > SIMUL_WINDOW_MS)) {
            _stepPending = false;  // commit as solo STEP press
        }

        // STEP is down (solo) — check for hold.
        if (_stepDown && !_stepPending && !_bothActive) {
            if (!_stepHoldFired && (now - _stepDownAt >= HOLD_THRESHOLD_MS)) {
                _stepHoldFired = true;
                return ButtonGesture::STEP_HOLD;
            }
        }

        // STEP was released — check for tap.
        if (!_stepDown && _stepReleasedAt != 0 && !_bothActive) {
            uint32_t duration = _stepReleasedAt - _stepDownAt;
            _stepReleasedAt = 0;  // consume the release event
            if (duration <= TAP_THRESHOLD_MS) {
                return ButtonGesture::STEP_TAP;
            }
            // Longer than tap but didn't fire hold?  Just ignore (edge case).
        }

        // ---- Solo LF gestures ----

        // Check if pending LF should be committed (simul window expired).
        if (_lfPending && !_stepPending && (now - _lfDownAt > SIMUL_WINDOW_MS)) {
            _lfPending = false;  // commit as solo LF press
        }

        // LF is down (solo) — check for short hold (3s) and long hold (5s).
        if (_lfDown && !_lfPending && !_bothActive) {
            uint32_t held = now - _lfDownAt;

            if (!_lfLongHoldFired && held >= HOLD_THRESHOLD_MS) {
                _lfLongHoldFired = true;
                return ButtonGesture::LF_HOLD_LONG;
            }
            if (!_lfShortHoldFired && held >= LF_SHORT_HOLD_MS) {
                _lfShortHoldFired = true;
                return ButtonGesture::LF_HOLD_SHORT;
            }
        }

        // LF was released — check for tap, or emit release-after-hold.
        if (!_lfDown && _lfReleasedAt != 0 && !_bothActive) {
            uint32_t duration = _lfReleasedAt - _lfDownAt;
            _lfReleasedAt = 0;  // consume the release event

            // If a hold gesture was already fired, emit the release event
            // so the state machine can exit "peek" modes.
            if (_lfShortHoldFired || _lfLongHoldFired) {
                return ButtonGesture::LF_RELEASE;
            }
            if (duration <= TAP_THRESHOLD_MS) {
                return ButtonGesture::LF_TAP;
            }
        }

        // ---- Pending gesture queue (from _handleBothRelease) ----
        if (_pendingGesture != ButtonGesture::NONE) {
            ButtonGesture g = _pendingGesture;
            _pendingGesture = ButtonGesture::NONE;
            return g;
        }

        return ButtonGesture::NONE;
    }

    // --- Real-time state queries --------------------------------------------

    bool isStepDown() const { return _stepDown; }
    bool isLfDown()   const { return _lfDown; }
    bool areBothDown() const { return _stepDown && _lfDown; }

    // --- Connection-loss safety ---------------------------------------------

    // Call this when MobiFlight connection is lost (no set() in 10+ seconds).
    // Resets all button state to released, cancels any in-progress gestures.
    void forceRelease() {
        _stepDown = false;
        _lfDown   = false;
        _stepPending = false;
        _lfPending   = false;
        _bothActive  = false;
        _stepHoldFired   = false;
        _lfShortHoldFired = false;
        _lfLongHoldFired  = false;
        _bothHoldFired   = false;
        _stepReleasedAt  = 0;
        _lfReleasedAt    = 0;
        _pendingGesture  = ButtonGesture::NONE;
    }

private:
    // --- Raw button state ---
    bool _stepDown = false;
    bool _lfDown   = false;

    // --- Press timestamps (millis) ---
    uint32_t _stepDownAt = 0;
    uint32_t _lfDownAt   = 0;

    // --- Release timestamps (0 = no pending release) ---
    uint32_t _stepReleasedAt = 0;
    uint32_t _lfReleasedAt   = 0;

    // --- Pending flags (waiting for simul window to expire) ---
    bool _stepPending = false;
    bool _lfPending   = false;

    // --- Hold-fired flags (fire each hold gesture only once per press) ---
    bool _stepHoldFired    = false;
    bool _lfShortHoldFired = false;  // 3s threshold
    bool _lfLongHoldFired  = false;  // 5s threshold
    bool _bothHoldFired    = false;

    // --- Both-button tracking ---
    bool     _bothActive = false;    // true when both buttons are classified as BOTH
    uint32_t _bothDownAt = 0;        // timestamp of the earlier button press

    // --- Queued gesture (for deferred release events) ---
    ButtonGesture _pendingGesture = ButtonGesture::NONE;

    // --- Internal helpers ---

    // Called when one button releases while _bothActive is true.
    void _handleBothRelease(uint32_t now) {
        // Wait until *both* buttons are released before emitting the gesture.
        if (_stepDown || _lfDown) return;

        _bothActive = false;

        if (_bothHoldFired) {
            // Hold was already emitted — now emit the release.
            _pendingGesture = ButtonGesture::BOTH_RELEASE;
        } else {
            // Both pressed and released without reaching hold threshold → BOTH_TAP.
            uint32_t duration = now - _bothDownAt;
            if (duration <= TAP_THRESHOLD_MS) {
                _pendingGesture = ButtonGesture::BOTH_TAP;
            }
            // If duration > TAP but < HOLD, it's an incomplete gesture — ignore.
        }

        // Clear release timestamps so solo logic doesn't also fire.
        _stepReleasedAt = 0;
        _lfReleasedAt   = 0;
    }
};
