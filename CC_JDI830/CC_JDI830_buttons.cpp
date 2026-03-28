// ---------------------------------------------------------------------------
// CC_JDI830_buttons.cpp — Button handling, gesture state machine, and
// related interaction logic for the CC_JDI830 engine monitor.
//
// This is a second translation unit for the CC_JDI830 class.  It implements
// member functions that deal with button input, the mode state machine,
// fuel setup wizard display, status bar labels, and debug overlay.
// The linker combines this with CC_JDI830.cpp — no header changes needed.
// ---------------------------------------------------------------------------

#include "CC_JDI830.h"
#include <Arduino.h>

void CC_JDI830::stepButtonStateChange(bool pressed) {
    _buttonInput.stepChange(pressed, millis());
}

void CC_JDI830::lfButtonStateChange(bool pressed) {
    _buttonInput.lfChange(pressed, millis());
}


// ---------------------------------------------------------------------------
// updateLeanFinder — runs every frame while in LEAN_FIND mode.
//
// Two jobs:
//   1. Data-driven phase transitions (e.g. PRE_LEAN timer, EGT peak detect)
//   2. Update the bottom bar display — but only when the phase changes.
//
// Button-driven transitions happen in handleGesture() and change _leanPhase
// directly.  We detect those here too via the _prevLeanPhase comparison.
// ---------------------------------------------------------------------------
void CC_JDI830::updateLeanFinder() {
    uint32_t now = millis();

    // --- Data-driven transitions ---
    int nCyl = activeProfile->numCylinders;

    // PRE_LEAN → LEANING after 900ms (let the "ROP" message display)
    if (_leanPhase == LeanPhase::PRE_LEAN
        && (now - _leanPhaseStartTime >= 900)) {
        _leanPhase = LeanPhase::LEANING;
    }

    // PEAK_FOUND → FINALIZING after 900ms (let "LEANEST" display)
    if (_leanPhase == LeanPhase::PEAK_FOUND
        && (now - _leanPhaseStartTime >= 900)) {
        _leanPhase = LeanPhase::FINALIZING;
    }

    // Keep _leanDelta updated every frame during finalizing.
    // Delta is negative (below peak), e.g. peak was 1500, now 1475 → delta = -25.
    if (_leanPhase == LeanPhase::FINALIZING && _leanPeakedCyl >= 0) {
        _leanDelta = curState.egt[_leanPeakedCyl] - _leanCylPeak[_leanPeakedCyl];
    }

    // LEANING: track per-cylinder EGT peaks and detect when one peaks out.
    //
    // Step 1 — Arm: once any cylinder rises >= LEAN_ARM_RISE above its
    //          baseline, set _leanArmed.  Until armed, we ignore drops
    //          (the engine may still be stabilizing).
    // Step 2 — Peak: once armed, if any cylinder's current EGT drops
    //          >= LEAN_PEAK_DROP below its tracked peak, that cylinder
    //          has peaked → transition to PEAK_FOUND.
    if (_leanPhase == LeanPhase::LEANING) {
        // Find the hottest cylinder and update per-cylinder peaks
        int hotCyl = 0;
        for (int i = 0; i < nCyl; i++) {
            if (curState.egt[i] > _leanCylPeak[i])
                _leanCylPeak[i] = curState.egt[i];

            if (curState.egt[i] > curState.egt[hotCyl])
                hotCyl = i;

            // Check for arming: any cylinder risen enough from baseline
            if (!_leanArmed && (curState.egt[i] - _leanBaseline[i] >= LEAN_ARM_RISE))
                _leanArmed = true;
        }

        // Once armed, flash the cylinder ID box over the hottest EGT.
        // The flash follows whichever cylinder is currently hottest.
        if (_leanArmed) {
            _egtChtBars.setFlashCylinder(hotCyl);

            // Check if any cylinder has dropped from its peak
            for (int i = 0; i < nCyl; i++) {
                if (_leanCylPeak[i] - curState.egt[i] >= LEAN_PEAK_DROP) {
                    _leanPeakedCyl = i;
                    _egtChtBars.clearFlashCylinder();
                    _egtChtBars.setSelectedCylinder(i);
                    _leanPhase = LeanPhase::PEAK_FOUND;
                    break;
                }
            }
        }
    }

    // --- Update display only when phase changes ---
    if (_leanPhase == _prevLeanPhase) return;
    _prevLeanPhase = _leanPhase;
    _leanPhaseStartTime = now;  // reset timer for the new phase

    switch (_leanPhase) {
        case LeanPhase::PRE_LEAN:
            _bottomBar.showMessage("ROP", TFT_WHITE);
            break;

        case LeanPhase::LEANING:
            // Snapshot current EGTs as baselines and reset tracking state
            for (int i = 0; i < nCyl; i++) {
                _leanBaseline[i] = curState.egt[i];
                _leanCylPeak[i]  = curState.egt[i];
            }
            _leanArmed = false;
            _leanPeakedCyl = -1;

            // Clear the "ROP" message so the dual page can draw
            _bottomBar.clearMessage();
            // Dual display: peak EGT on the left, fuel flow on the right
            _bottomBar.setPage({
                BottomMode::DUAL,
                drawDualLabelValue,
                makeValueDef("EGT", &curState.egtPeak, 0, nullptr, activeProfile->egt),
                drawDualLabelValue,
                makeValueDef("FF", &curState.ff, 1, nullptr, activeProfile->ff)
            });
            break;

        case LeanPhase::PEAK_FOUND:
            _bottomBar.showMessage("LEANEST", TFT_WHITE, 900);
            break;

        case LeanPhase::FINALIZING:
            // Dual display: degrees below peak (negative) + fuel flow
            _bottomBar.clearMessage();
            _bottomBar.setPage({
                BottomMode::DUAL,
                drawDualLabelValue,
                makeValueDef("EGT", &_leanDelta, 0, nullptr, activeProfile->egt),
                drawDualLabelValue,
                makeValueDef("FF", &curState.ff, 1, nullptr, activeProfile->ff)
            });
            break;
    }
}


// ---------------------------------------------------------------------------
// updateFuelSetupDisplay — render the fuel setup wizard in the bottom bar.
//
// Uses BottomBar::showMessage() to display text for each phase.  Called
// every frame during FUEL_SETUP so flashing text toggles and the adjust
// value stays current.
//
// Real JPI 830 display behavior per phase:
//   SHOW_FUEL:     "FUEL" (solid, 1 second)
//   ASK_FILL:      "FILL? N" (flashing)
//   FILL_MAIN:     "FILL 60" (main tank gallons)
//   FILL_MAIN_AUX: "FILL 86" (main + aux gallons)
//   FILL_ADJUST:   "FILL + 58.9" (adjustable, LF to change)
// ---------------------------------------------------------------------------
void CC_JDI830::updateFuelSetupDisplay(uint32_t now) {
    // Note: showMessage() stores the pointer, not a copy.  For phases that
    // build dynamic text we use _fuelSetupBuf (a class member) so the
    // pointer remains valid when BottomBar::update() draws later.  For
    // static strings (literals) we pass them directly — they live in flash.

    switch (_fuelSetupPhase) {
        case FuelSetupPhase::SHOW_FUEL:
            _bottomBar.showMessage("FUEL", TFT_WHITE);
            break;

        case FuelSetupPhase::ASK_FILL: {
            // Flash "FILL? N" — ~500ms on, ~500ms off.
            bool visible = ((now / 500) % 2 == 0);
            if (visible) {
                _bottomBar.showMessage("FILL?  N", TFT_YELLOW);
            } else {
                _bottomBar.showMessage(" ", TFT_BLACK);
            }
            break;
        }

        case FuelSetupPhase::FILL_MAIN:
            snprintf(_fuelSetupBuf, sizeof(_fuelSetupBuf),
                     "FILL  %d", (int)activeProfile->fuelMain);
            _bottomBar.showMessage(_fuelSetupBuf, TFT_WHITE);
            break;

        case FuelSetupPhase::FILL_MAIN_AUX:
            snprintf(_fuelSetupBuf, sizeof(_fuelSetupBuf),
                     "FILL  %d",
                     (int)(activeProfile->fuelMain + activeProfile->fuelAux));
            _bottomBar.showMessage(_fuelSetupBuf, TFT_WHITE);
            break;

        case FuelSetupPhase::FILL_ADJUST:
            snprintf(_fuelSetupBuf, sizeof(_fuelSetupBuf),
                     "FILL + %.1f", _fuelAdjustValue);
            _bottomBar.showMessage(_fuelSetupBuf, TFT_WHITE);
            break;

        case FuelSetupPhase::FILL_ADJUST_FINE:
            snprintf(_fuelSetupBuf, sizeof(_fuelSetupBuf),
                     "FILL  %.1f", _fuelAdjustValue);
            _bottomBar.showMessage(_fuelSetupBuf, TFT_WHITE);
            break;
    }
}



// ---------------------------------------------------------------------------
// handleGesture — the mode state machine.
//
// This is the core transition table: given the current DisplayMode and a
// ButtonGesture, decide what to do.  It's a switch-within-switch — the
// outer switch is the current mode, the inner switch is the gesture.
//
// The compiler turns nested switches into jump tables on ESP32/Xtensa,
// so this is as efficient as a function-pointer table but much more readable.
//
// Alarms take priority: if an alarm is showing, STEP gestures are intercepted
// for dismiss/snooze before normal mode handling.
// ---------------------------------------------------------------------------
void CC_JDI830::handleGesture(ButtonGesture gesture) {

    // --- Alarm intercept (highest priority) ---
    // When an alarm is active, STEP controls alarm dismissal regardless
    // of the current display mode.
    if (_bottomBarMode == BottomBarMode::ALARM) {
        switch (gesture) {
            case ButtonGesture::STEP_TAP:
                onStepPress();       // dismiss for 10 minutes
                return;
            case ButtonGesture::STEP_HOLD:
                onStepLongPress();   // dismiss permanently
                return;
            default:
                break;  // other gestures fall through to normal mode handling
        }
    }

    // --- Mode-specific handling ---
    switch (_displayMode) {

        case DisplayMode::FUEL_SETUP:
            switch (_fuelSetupPhase) {
                case FuelSetupPhase::SHOW_FUEL:
                    // "FUEL" is showing for 1 second — no button input accepted.
                    break;

                case FuelSetupPhase::ASK_FILL:
                    // "FILL? N" is flashing.  STEP = skip (no fuel added), LF = yes.
                    if (gesture == ButtonGesture::STEP_TAP) {
                        // No fuel added — exit to MANUAL mode.
                        _displayMode = DisplayMode::MANUAL;
                        _autoScan = false;
                        _manualStartTime = millis();
                        _bottomBar.clearMessage();
                        _fuelSetupPhase = FuelSetupPhase::ASK_FILL;
                    } else if (gesture == ButtonGesture::LF_TAP) {
                        // User wants to enter fuel — show first choice.
                        _fuelSetupPhase = FuelSetupPhase::FILL_MAIN;
                    }
                    break;

                case FuelSetupPhase::FILL_MAIN:
                    // Showing "FILL xx" (main tank value).
                    // STEP = accept this value, LF = next choice.
                    if (gesture == ButtonGesture::STEP_TAP) {
                        curState.fuelRem = activeProfile->fuelMain;
                        curState.used = 0;
                        _displayMode = DisplayMode::MANUAL;
                        _autoScan = false;
                        _manualStartTime = millis();
                        _bottomBar.clearMessage();
                        _fuelSetupPhase = FuelSetupPhase::ASK_FILL;
                    } else if (gesture == ButtonGesture::LF_TAP) {
                        // Advance to next choice: MAIN+AUX (if aux exists) or ADJUST.
                        if (activeProfile->fuelAux > 0) {
                            _fuelSetupPhase = FuelSetupPhase::FILL_MAIN_AUX;
                        } else {
                            _fuelSetupPhase = FuelSetupPhase::FILL_ADJUST_FINE;
                            _fuelAdjustValue = curState.fuelRem;  // start from current
                        }
                    }
                    break;

                case FuelSetupPhase::FILL_MAIN_AUX:
                    // Showing "FILL xx" (main + aux total).
                    // STEP = accept, LF = next choice (FILL +).
                    if (gesture == ButtonGesture::STEP_TAP) {
                        curState.fuelRem = activeProfile->fuelMain + activeProfile->fuelAux;
                        curState.used = 0;
                        _displayMode = DisplayMode::MANUAL;
                        _autoScan = false;
                        _manualStartTime = millis();
                        _bottomBar.clearMessage();
                        _fuelSetupPhase = FuelSetupPhase::ASK_FILL;
                    } else if (gesture == ButtonGesture::LF_TAP) {
                        _fuelSetupPhase = FuelSetupPhase::FILL_ADJUST_FINE;
                        _fuelAdjustValue = curState.fuelRem;
                    }
                    break;

                case FuelSetupPhase::FILL_ADJUST:
                    // Showing "FILL +" with adjustable value.
                    // STEP = enter fine-adjust mode (0.1 gal increments).
                    // LF = cycle through coarse options (same as other FILL phases).
                    if (gesture == ButtonGesture::STEP_TAP) {
                        _fuelSetupPhase = FuelSetupPhase::FILL_ADJUST_FINE;
                        _fuelFineHoldStart = 0;
                        _fuelFineRepeatTime = 0;
                    } else if (gesture == ButtonGesture::LF_TAP) {
                        _fuelAdjustValue += 1.0f;  // increment by 1 gallon
                    }
                    break;

                case FuelSetupPhase::FILL_ADJUST_FINE:
                    // Fine adjustment mode: 0.1 gal resolution.
                    // STEP tap = accept and save the value, exit to MANUAL.
                    // LF tap  = subtract 0.1 gal.
                    // LF hold = auto-repeat add handled in update() (not gestures).
                    if (gesture == ButtonGesture::STEP_TAP) {
                        curState.fuelRem = _fuelAdjustValue;
                        curState.used = 0;
                        _displayMode = DisplayMode::MANUAL;
                        _autoScan = false;
                        _manualStartTime = millis();
                        _bottomBar.clearMessage();
                        _fuelSetupPhase = FuelSetupPhase::ASK_FILL;
                    } else if (gesture == ButtonGesture::LF_TAP) {
                        _fuelAdjustValue -= FUEL_FINE_STEP_SMALL;
                        if (_fuelAdjustValue < 0) _fuelAdjustValue = 0;
                    }
                    break;
            }
            break;

        case DisplayMode::AUTO:
            switch (gesture) {
                case ButtonGesture::STEP_TAP:
                    // Switch to manual mode — stop auto-rotation.
                    _displayMode = DisplayMode::MANUAL;
                    _autoScan = false;
                    _manualStartTime = millis();  // start 2-min auto-switch timer
                    break;

                case ButtonGesture::LF_TAP:
                    // Enter lean find mode.
                    _displayMode = DisplayMode::LEAN_FIND;
                    _leanPhase = LeanPhase::PRE_LEAN;
                    _prevLeanPhase = LeanPhase::LEANING;  // force mismatch so first frame triggers display
                    _leanPhaseStartTime = millis();
                    _autoScan = false;
                    _bottomBarMode = BottomBarMode::LEAN;
                    break;

                case ButtonGesture::LF_HOLD_SHORT:
                    // Toggle normalize/percentage view (3-second hold).
                    curState.showNormalized = !curState.showNormalized;
                    break;

                case ButtonGesture::BOTH_HOLD:
                    _displayMode = DisplayMode::FUEL_SETUP;
                    _autoScan = false;
                    break;

                case ButtonGesture::BOTH_HOLD_LONG:
                    // Enter pilot programming mode (future).
                    _displayMode = DisplayMode::PILOT_PROGRAM;
                    _autoScan = false;
                    _bottomBarMode = BottomBarMode::SETUP;
                    // TODO: enterPilotProgram()
                    break;

                default:
                    break;
            }
            break;

        case DisplayMode::MANUAL:
            switch (gesture) {
                case ButtonGesture::STEP_TAP:
                    // Advance to next parameter in the sequence.
                    advanceBottomPage();
                    _manualStartTime = millis();  // reset 2-min auto-switch timer
                    break;

                case ButtonGesture::STEP_HOLD:
                    // Rapidly display previous parameters (backward scroll).
                    // For now, go back one page.  A "rapid repeat" could be
                    // implemented later with a timer while the button is held.
                    _manualStartTime = millis();  // reset 2-min auto-switch timer
                    if (_numBottomPages > 0) {
                        _currentBottomPage = (_currentBottomPage - 1 + _numBottomPages)
                                             % _numBottomPages;
                        showCurrentPage();
                        _lastPageChange = millis();
                    }
                    break;

                case ButtonGesture::LF_TAP:
                    // Enter lean find mode.
                    _displayMode = DisplayMode::LEAN_FIND;
                    _leanPhase = LeanPhase::PRE_LEAN;
                    _prevLeanPhase = LeanPhase::LEANING;  // force mismatch so first frame triggers display
                    _leanPhaseStartTime = millis();
                    _bottomBarMode = BottomBarMode::LEAN;
                    break;

                case ButtonGesture::LF_HOLD_SHORT:
                    // Toggle normalize/percentage view (3-second hold).
                    curState.showNormalized = !curState.showNormalized;
                    break;

                case ButtonGesture::BOTH_TAP:
                    // Toggle include/exclude current parameter from AUTO rotation.
                    toggleParamExclude();
                    break;

                case ButtonGesture::BOTH_HOLD:
                    _displayMode = DisplayMode::FUEL_SETUP;
                    _autoScan = false;
                    break;

                case ButtonGesture::BOTH_HOLD_LONG:
                    // Enter pilot programming mode (future).
                    _displayMode = DisplayMode::PILOT_PROGRAM;
                    _autoScan = false;
                    _bottomBarMode = BottomBarMode::SETUP;
                    // TODO: enterPilotProgram()
                    break;

                default:
                    break;
            }
            break;

        case DisplayMode::LEAN_FIND:
            switch (gesture) {
                case ButtonGesture::LF_HOLD_LONG:
                    // Hold LF to peek at absolute peak EGT (only during finalizing)
                    if (_leanPhase == LeanPhase::FINALIZING) {
                        _displayMode = DisplayMode::LEAN_PEAK;
                        // Show absolute peak EGT + FF while held
                        _bottomBar.clearMessage();
                        _bottomBar.setPage({
                            BottomMode::DUAL,
                            drawDualLabelValue,
                            makeValueDef("PK", &_leanCylPeak[_leanPeakedCyl], 0, nullptr, activeProfile->egt),
                            drawDualLabelValue,
                            makeValueDef("FF", &curState.ff, 1, nullptr, activeProfile->ff)
                        });
                    }
                    break;

                case ButtonGesture::STEP_TAP:
                    // Exit lean find, return to AUTO mode.
                    _leanPhase = LeanPhase::PRE_LEAN;
                    _egtChtBars.clearFlashCylinder();
                    _egtChtBars.setSelectedCylinder(-1);
                    _bottomBar.clearMessage();
                    _displayMode = DisplayMode::AUTO;
                    _autoScan = true;
                    _bottomBarMode = _prevDataMode;
                    _lastPageChange = millis();
                    showCurrentPage();
                    break;

                case ButtonGesture::BOTH_HOLD:
                    // Toggle Rich Of Peak / Lean Of Peak (only before leaning).
                    break;

                default:
                    break;
            }
            break;

        case DisplayMode::LEAN_PEAK:
            // Only LF release matters — go back to LEAN_FIND.
            // The _leanPhase is unchanged, so updateLeanFinder() will
            // restore the correct bottom bar display on the next phase
            // change detection.  Force it by resetting _prevLeanPhase.
            if (gesture == ButtonGesture::LF_RELEASE) {
                _displayMode = DisplayMode::LEAN_FIND;
                _prevLeanPhase = LeanPhase::PRE_LEAN;  // force display refresh
            }
            break;

        case DisplayMode::PILOT_PROGRAM:
            // Future: pilot programming mode button handling.
            // STEP tap = advance to next item
            // LF tap/hold = increment/decrement values, toggle Y/N
            // BOTH hold at "END? Y" = next programming mode
            // For now, STEP tap exits back to AUTO as an escape hatch.
            switch (gesture) {
                case ButtonGesture::STEP_TAP:
                    _displayMode = DisplayMode::AUTO;
                    _autoScan = true;
                    _bottomBarMode = _prevDataMode;
                    _lastPageChange = millis();
                    showCurrentPage();
                    break;

                default:
                    break;
            }
            break;
    }

    // Labels are updated every frame by updateStatusLabels() in update(),
    // so we don't need to set them here after every transition.
}

// ---------------------------------------------------------------------------
// updateStatusLabels — set button labels and center indicator each frame.
//
// Called once per frame in update().  This single function replaces all the
// scattered setLabels() calls so labels always reflect the actual combined
// state of DisplayMode + FuelSetupPhase + BottomBarMode.
//
// Center indicator:
//   "NRM" — when showNormalized is active (EGT/CHT bars in normalize mode)
//   "ROP" — when in lean-find mode and Rich-of-Peak is selected
//   ""    — otherwise (hidden)
// ---------------------------------------------------------------------------
void CC_JDI830::updateStatusLabels() {
    const char* step = "STEP";
    const char* lf   = "LF";
    const char* center = "";

    // Alarm overrides the STEP label regardless of display mode
    if (_bottomBarMode == BottomBarMode::ALARM) {
        step = "CLEAR";
        lf   = "";
    } else {
        switch (_displayMode) {
            case DisplayMode::FUEL_SETUP:
                switch (_fuelSetupPhase) {
                    case FuelSetupPhase::SHOW_FUEL:
                        step = "";
                        lf   = "";
                        break;
                    case FuelSetupPhase::ASK_FILL:
                        step = "EXIT";
                        lf   = "REFUEL";
                        break;
                    case FuelSetupPhase::FILL_MAIN:
                    case FuelSetupPhase::FILL_MAIN_AUX:
                        step = "SAVE";
                        lf   = "CHANGE";
                        break;
                    case FuelSetupPhase::FILL_ADJUST:
                        step = "ADJUST";
                        lf   = "CHANGE";
                        break;
                    case FuelSetupPhase::FILL_ADJUST_FINE:
                        step = "SAVE";
                        lf   = "CHANGE";
                        break;
                }
                break;

            case DisplayMode::AUTO:
                step = "STEP";
                lf   = "LF";
                break;

            case DisplayMode::MANUAL:
                step = "STEP";
                lf   = "LF";
                break;

            case DisplayMode::LEAN_FIND:
            {
                switch (_leanPhase) {
                    case LeanPhase::PRE_LEAN:
                    case LeanPhase::LEANING:
                        step = "EXIT";
                        lf   = "";
                        break;
                    case LeanPhase::PEAK_FOUND:
                        step = "EXIT";
                        lf   = "PEAK";
                        break;
                    case LeanPhase::FINALIZING:
                        step = "EXIT";
                        lf   = "PEAK";
                        break;
                }
                break;
            }

            case DisplayMode::LEAN_PEAK:
                step = "EXIT";
                lf   = "PEAK";
                break;

            case DisplayMode::PILOT_PROGRAM:
                step = "NEXT";
                lf   = "ADJ";
                break;
        }
    }

    // Center indicator: NRM when normalize is active, ROP during lean-find
    // (future: "LOP" for lean-of-peak — currently only ROP is implemented)
    if (curState.showNormalized) {
        center = "NRM";
    } else if (_displayMode == DisplayMode::LEAN_FIND || _displayMode == DisplayMode::LEAN_PEAK)
    {
        // TODO: check a richOfPeak flag once lean-find is fully implemented.
        // For now, always show "ROP" when in lean-find mode.
        center = "ROP";
    }

    _statusBar.setLabels(step, lf);
    _statusBar.setCenterIndicator(center);
}

// ---------------------------------------------------------------------------
// drawDebugState — LCD debug overlay for development.
//
// Draws current mode, gesture, connection status, and raw button state
// directly to the bottom ~15 rows of the LCD (y = 465–480).  This writes
// directly to the LCD (not through a sprite) since the debug area isn't
// owned by any gauge.
//
// Controlled by the DEBUG_OVERLAY constexpr bool — when false, the compiler
// eliminates this code entirely (dead code elimination).
// ---------------------------------------------------------------------------
void CC_JDI830::drawDebugState(ButtonGesture gesture) {
    if constexpr (!DEBUG_OVERLAY) return;

    // Map enums to short display strings.  These are just for debug — no
    // need for a fancy toString() method.
    const char* modeStr = "???";
    switch (_displayMode) {
        case DisplayMode::FUEL_SETUP:     modeStr = "FUEL";    break;
        case DisplayMode::AUTO:           modeStr = "AUTO";    break;
        case DisplayMode::MANUAL:         modeStr = "MANUAL";  break;
        case DisplayMode::LEAN_FIND:      modeStr = "LEAN";    break;
        case DisplayMode::LEAN_PEAK:      modeStr = "PEAK";    break;
        case DisplayMode::PILOT_PROGRAM:  modeStr = "PROG";    break;
    }

    const char* gestStr = "";
    switch (gesture) {
        case ButtonGesture::NONE:            gestStr = "";          break;
        case ButtonGesture::STEP_TAP:        gestStr = "S_TAP";    break;
        case ButtonGesture::STEP_HOLD:       gestStr = "S_HOLD";   break;
        case ButtonGesture::LF_TAP:          gestStr = "LF_TAP";   break;
        case ButtonGesture::LF_HOLD_SHORT:   gestStr = "LF_H3";    break;
        case ButtonGesture::LF_HOLD_LONG:    gestStr = "LF_H5";    break;
        case ButtonGesture::LF_RELEASE:      gestStr = "LF_REL";   break;
        case ButtonGesture::BOTH_TAP:        gestStr = "B_TAP";    break;
        case ButtonGesture::BOTH_HOLD:       gestStr = "B_HOLD";   break;
        case ButtonGesture::BOTH_HOLD_LONG:  gestStr = "B_HOLD_LONG";   break;
        case ButtonGesture::BOTH_RELEASE:    gestStr = "B_REL";    break;
    }

    // Build the debug line:  "AUTO  S_TAP  S:0 L:1  CONN"
    char buf[64];
    snprintf(buf, sizeof(buf), "%-6s %-6s S:%d L:%d %s",
             modeStr, gestStr,
             _buttonInput.isStepDown() ? 1 : 0,
             _buttonInput.isLfDown() ? 1 : 0,
             _connectionLost ? "LOST" : "CONN");

    // Draw directly to LCD (not a sprite) in the bottom rows.
    // Black fill first to clear previous text, then white text on black.
    const GaugeLayout& L = *_displayCfg.layout;
    _lcd.fillRect(0, L.debugY, L.screenW, 15, TFT_BLACK);
    _lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    _lcd.setTextDatum(ML_DATUM);  // middle-left
    _lcd.setTextSize(1);          // built-in font, small
    _lcd.drawString(buf, 4, L.debugY + 7);
}

// ---------------------------------------------------------------------------
// onStepPress — short press of the STEP button.
//
// If an alarm is showing, dismiss it for 10 minutes (the next-priority
// alarm will appear if one exists).  Otherwise, advance the bottom bar
// to the next page in the current data mode's rotation.
//
// Call this from whichever input source you end up using — physical
// button ISR, MobiFlight set() message, etc.
// ---------------------------------------------------------------------------
void CC_JDI830::onStepPress() {
    if (_bottomBarMode == BottomBarMode::ALARM && _alarmMgr.hasActiveAlarm()) {
        _alarmMgr.dismissTimed(millis());

        // Immediately re-scan: if another alarm is pending it takes over,
        // otherwise we drop back to the data mode.
        if (_alarmMgr.scan(millis())) {
            _bottomBar.showAlarm(_alarmMgr.activeAlarmDef(), _alarmMgr.activeAlarmDef()->valuePtr);
        } else {
            _bottomBarMode = _prevDataMode;
            _bottomBar.clearAlarm();
            showCurrentPage();
            _lastPageChange = millis();
        }
    } else if (_bottomBarMode != BottomBarMode::ALARM) {
        // Advance to next page in current rotation
        if (_numBottomPages > 0) {
            _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
            showCurrentPage();
            _lastPageChange = millis();
        }
    }
}

// ---------------------------------------------------------------------------
// onStepLongPress — long press of the STEP button.
//
// Permanently dismisses the current alarm (until power cycle / profile
// change).  If another alarm is pending, it appears immediately.
// ---------------------------------------------------------------------------
void CC_JDI830::onStepLongPress() {
    if (_bottomBarMode == BottomBarMode::ALARM && _alarmMgr.hasActiveAlarm()) {
        _alarmMgr.dismissPermanent();

        if (_alarmMgr.scan(millis())) {
            _bottomBar.showAlarm(_alarmMgr.activeAlarmDef(), _alarmMgr.activeAlarmDef()->valuePtr);
        } else {
            _bottomBarMode = _prevDataMode;
            _bottomBar.clearAlarm();
            showCurrentPage();
            _lastPageChange = millis();
        }
    }
}
