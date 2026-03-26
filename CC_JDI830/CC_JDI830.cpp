#include "CC_JDI830.h"
#include "allocateMem.h"
#include "commandmessenger.h"

#include <Arduino.h>

// Font includes
#include "Fonts/SevenSeg42.h"
#include "Fonts/SevenSeg32.h"
#include "Fonts/ArialNB12.h"
#include "Fonts/ArialN11.h"
#include "Fonts/ArialN16.h"
#include "Fonts/ArialNI16.h"
#include "Fonts/ArialNI36.h"


// ---------------------------------------------------------------------------
// applyRangeDef — applies a GaugeRangeDef's range, redline, and color bands
// to any Gauge.  Keeps setupGauges() from repeating this pattern 10+ times.
// ---------------------------------------------------------------------------
void CC_JDI830::applyRangeDef(Gauge& gauge, const GaugeRangeDef& def) {
    gauge.setRange(def.min, def.max);
    if (def.redline > 0) gauge.setRedline(def.redline);
    for (uint8_t i = 0; i < def.colorCount; i++) {
        gauge.addColorRange(def.colors[i].min, def.colors[i].max,
                            def.colors[i].color);
    }
}

// ---------------------------------------------------------------------------
// setupRightBars — (re)configures the right-column HBar gauges from the
// current rightBarSelections[] array.  Safe to call at any time to apply
// new selections without restarting.
// ---------------------------------------------------------------------------
void CC_JDI830::setupRightBars() {
    const PlaneProfile& p = *activeProfile;

    int barH = 29;
    int barW = 80;
    int barX = 220;
    int barY = -20;
    int barYSpacing = 34;

    // for (int i = 0; i < NUM_RIGHT_BAR_SLOTS; i++) {
    for (int i = 0; i < _displayCfg.numRightBarSlots ; i++) {


        // DisplayVarInfo info = resolveDisplayVar(rightBarSelections[i], p, curState);
        DisplayVarInfo info = resolveDisplayVar(_displayCfg.rightBarSelections[i], p, curState);

        HBarGauge& bar = _rightBars[i];
        bar.clearColorRanges();
        bar.setPosition(barX, barY + (i + 1) * barYSpacing);
        bar.setLabel(info.label);
        bar.setValuePtr(info.valuePtr);
        bar.setLabelFont(ArialN11);
        bar.setValueFont(ArialNB12);
        bar.setDecimals(info.decimals);
        if (info.range) {
            applyRangeDef(bar, *info.range);

            // Special case: FUEL_REM range max comes from the sim, not the profile
            if (_displayCfg.rightBarSelections[i] == DisplayVarId::FUEL_REM) {
                bar.setRange(0, curState.fuelCapacity);
            }
        }
        bar.init(barW, barH);
    }
}

void CC_JDI830::setupGauges() {
    const PlaneProfile& p = *activeProfile;

    // --- RPM Arc Gauge ---
    _rpmGauge.setPosition(10, 1);
    _rpmGauge.setLabel("RPM");
    _rpmGauge.setValuePtr(&curState.rpm);
    _rpmGauge.setArcGeometry(75, 85, 75, 70);
    _rpmGauge.setAngles(182, 358);
    _rpmGauge.setNeedle(55, TFT_WHITE);
    _rpmGauge.setValueFont(SevenSeg32);
    _rpmGauge.setLabelFont(ArialNB12);
    applyRangeDef(_rpmGauge, p.rpm);
    _rpmGauge.init(205, 90);

    // --- MAP Arc Gauge ---
    if (p.hasMap) {
        _mapGauge.setPosition(10, 95);
        _mapGauge.setLabel("MAP");
        _mapGauge.setValuePtr(&curState.map);
        _mapGauge.setArcGeometry(75, 2, 75, 70);
        _mapGauge.setInverted(true);
        _mapGauge.setAngles(178, 2);
        _mapGauge.setNeedle(55, TFT_WHITE);
        _mapGauge.setDecimals(1);
        _mapGauge.setValueFont(SevenSeg32);
        _mapGauge.setLabelFont(ArialNB12);
        applyRangeDef(_mapGauge, p.map);
        _mapGauge.init(205, 90);
    }

    // --- %HP Value Gauge ---
    if (p.hasHp) {
        _hpPct.setPosition(2, 185);
        _hpPct.setLabel("% HP");
        _hpPct.setValuePtr(&curState.hp);
        _hpPct.setValueFont(ArialN16);
        _hpPct.setLabelFont(ArialN11);
        _hpPct.setValueColor(TFT_WHITE);
        _hpPct.setLayout(35, LABEL_RIGHT);
        _hpPct.init(90, 20);
    }

    // --- HBar Gauges (right column) — configured from rightBarSelections[] ---
    setupRightBars();

    // --- EGT/CHT Column Bars (with optional TIT) ---
    // Layout is computed automatically from sprite size, cylinder count, and TIT count.
    _egtChtBars.setPosition(5, 205);
    _egtChtBars.setNumCylinders(p.numCylinders);
    _egtChtBars.setEgtRange(p.egt.min, p.egt.max);
    _egtChtBars.setChtRange(p.cht.min, p.cht.max);
    if (p.egt.redline > 0) _egtChtBars.setEgtRedline(p.egt.redline);
    if (p.cht.redline > 0) _egtChtBars.setChtRedline(p.cht.redline);
    _egtChtBars.setEgtColor(TFT_SKYBLUE);
    _egtChtBars.setChtColor(TFT_WHITE);
    _egtChtBars.setScaleFont(ArialN11);
    _egtChtBars.setLabelFont(ArialN11);
    _egtChtBars.setValueFont(ArialN11);
    for (int i = 0; i < p.numCylinders; i++) {
        _egtChtBars.setEgtPtr(i, &curState.egt[i]);
        _egtChtBars.setChtPtr(i, &curState.cht[i]);
    }
    if (p.hasTit1) {
        _egtChtBars.setTitRange(p.tit1.min, p.tit1.max);
        if (p.tit1.redline > 0) _egtChtBars.setTitRedline(p.tit1.redline);
        _egtChtBars.setTitColor(TFT_CYAN);
        _egtChtBars.addTit(&curState.tit1, "T");
    }
    if (p.hasTit2) {
        _egtChtBars.addTit(&curState.tit2, "T2");
    }
    _egtChtBars.init((p.numCylinders == 6) ? 310: 205, 165);  // computeLayout() handles everything

    // --- Bottom bar ---
    _numBottomPages = buildBottomPages(_bottomPages, p, curState);
    _bottomBar.setPosition(5, 380);
    _bottomBar.setMessageFont(ArialNI36);
    _bottomBar.init(310, 45);
    if (_numBottomPages > 0)
        _bottomBar.setPage(_bottomPages[0]);

    _statusBar.setPosition(0,430);
    _statusBar.init(320, 16);    
}


// ---------------------------------------------------------------------------
// setProfile — switch to a different PlaneProfile by index.
// Short-circuits if the index hasn't actually changed (MobiFlight can
// send redundant updates).  After switching, re-runs setupGauges() and
// forces every gauge dirty so the new layout draws on the next update().
// ---------------------------------------------------------------------------
void CC_JDI830::setProfile(int index) {
    if (index < 0 || index >= NUM_PROFILES) return;
    if (index == _profileIndex) return;       // no-op if unchanged

    _lcd.fillScreen(TFT_BLACK);
    // Erase right-bar gauges that may not exist in the new layout.
    // Must happen before we rebuild _displayCfg, while the old slot count
    // is still valid.
    int oldSlots = _displayCfg.numRightBarSlots;

    _profileIndex = index;
    activeProfile = ALL_PROFILES[index];

    _displayCfg = buildDefaultConfig(*activeProfile);

    // If the new layout has fewer slots, erase the ones that are going away
    // for (int i = _displayCfg.numRightBarSlots; i < oldSlots; i++)
    //     _rightBars[i].erase();

    setupGauges();

    // Build the alarm table for this profile — pointers into curState
    // and the new profile are captured so scan() can check them each frame.
    _alarmMgr.buildAlarms(*activeProfile, curState);
    _bottomBarMode = BottomBarMode::ALL_DATA;

    // setupGauges() reconfigures ranges/colors/labels but the float
    // values haven't changed, so the base Gauge::update() won't see
    // a value delta and _dirty stays false.  Force a full redraw.
    _rpmGauge.forceDirty();
    _mapGauge.forceDirty();
    _hpPct.forceDirty();
    _egtChtBars.forceDirty();
    _bottomBar.forceDirty();
    for (int i = 0; i < _displayCfg.numRightBarSlots; i++)
        _rightBars[i].forceDirty();
}

void CC_JDI830::drawStatic() {
    _lcd.drawLine(6,93, 163, 93, TFT_LIGHTGRAY);
    _lcd.drawLine(6,94, 163, 94, TFT_LIGHTGRAY);
}

CC_JDI830::CC_JDI830(uint8_t sclk, uint8_t mosi, uint8_t dc, uint8_t cs, uint8_t rst, uint8_t bl)
{
    _pinSCLK = sclk;
    _pinMOSI = mosi;
    _pinDC = dc;
    _pinCS = cs;
    _pinRST = rst;
    _pinBL = bl;
}

void CC_JDI830::begin()
{
    // Apply pin config from MobiFlight before initializing the display hardware
    _lcd.configurePins(_pinSCLK, _pinMOSI, _pinDC, _pinCS, _pinRST, _pinBL);

    // Serial.printf("sclk: %d mosi: %d dc: %d cs: %d rst: %d bl: %d\n", _pinSCLK, _pinMOSI, _pinDC, _pinCS, _pinRST, _pinBL);

    _lcd.init();
    _lcd.setRotation(0);  // portrait mode, 320x480
    _lcd.fillScreen(TFT_BLACK);

    // Load default profile (later: read saved index from NVS here)
    setProfile(2);

    // Device starts in FUEL_SETUP mode — record start time for the
    // 1-second "FUEL" splash, then the wizard takes over.
    _fuelSetupStartTime = millis();

//    drawStatic();
}

void CC_JDI830::attach()
{
}

void CC_JDI830::detach()
{
    if (!_initialised)
        return;
    _initialised = false;
}

void CC_JDI830::set(int16_t messageID, char *setPoint)
{
    /* **********************************************************************************
        Each messageID has it's own value
        check for the messageID and define what to do.
        Important Remark!
        MessageID == -2 will be send from the board when PowerSavingMode is set
            Message will be "0" for leaving and "1" for entering PowerSavingMode
        MessageID == -1 will be send from the connector when Connector stops running
        Put in your code to enter this mode (e.g. clear a display)

    ********************************************************************************** */
    // Track connection liveness — any set() call means MobiFlight is talking.
    _lastSetTime = millis();
    if (_connectionLost) {
        _connectionLost = false;
        // Connection restored — ButtonInput was force-released, state is clean.
    }

    // do something according your messageID
    switch (messageID) {
    case -1:
        // tbd., gets called when MobiFlight shuts down
        break;
    case -2:
        // tbd., gets called when PowerSavingMode is entered
        break;

    case 0: {
        // EGT — either "val1|val2|...|valN" per cylinder, or a single value.
        // When the profile says the sim only reports one EGT, stash it in
        // egtRaw and let spreadCylinders() build the per-cylinder array.
        // When the sim sends real per-cylinder data, write directly.
       cmdMessenger.sendCmd(kStatus,setPoint);
       int nCyl = activeProfile->numCylinders;
       if (strchr(setPoint, '|')) {
           char* tok = strtok(setPoint, "|");
           for (int i = 0; i < nCyl && tok != nullptr; i++) {
                Serial.printf("tok %d:%s\n",i, tok);
               curState.egt[i] = strtof(tok, nullptr);
               tok = strtok(nullptr, "|");
               Serial.printf("token %d: %f\n",i, curState.egt[i]);
            }
        } else {
            float val = strtof(setPoint, nullptr);
            if (activeProfile->reportsSingleEgtCht) {
                curState.egtRaw = val;
            } else {
                // Profile expects per-cylinder data but we only got one value.
                // Write to cyl 0 only so the lopsided display makes it obvious
                // something's misconfigured.
                curState.egt[0] = val;
            }
        }
        char buf[255];
        sprintf(buf, "1: %f 2:%f 3:%f 4:%f 5:%f 6:%f\n", curState.egt[0],curState.egt[1],curState.egt[2],curState.egt[3],curState.egt[4],curState.egt[5]);
        break;
    }

    case 1: {
        // CHT — same pipe-delimited or single-value format as EGT
        int nCyl = activeProfile->numCylinders;
        if (strchr(setPoint, '|')) {
            char* tok = strtok(setPoint, "|");
            for (int i = 0; i < nCyl && tok != nullptr; i++) {
                curState.cht[i] = strtof(tok, nullptr);
                tok = strtok(nullptr, "|");
            }
        } else {
            float val = strtof(setPoint, nullptr);
            if (activeProfile->reportsSingleEgtCht) {
                curState.chtRaw = val;
            } else {
                curState.cht[0] = val;
            }
        }
        break;
    }

    case 2:
        curState.tit1 = strtof(setPoint, nullptr);
        break;

    case 3:
        curState.tit2 = strtof(setPoint, nullptr);
        break;

    case 4:
        curState.oilT = strtof(setPoint, nullptr);
        break;

    case 5:
        curState.oilP = strtof(setPoint, nullptr);
        break;

    case 6:
        curState.bat = strtof(setPoint, nullptr);
        break;

    case 7:
        curState.oat = strtof(setPoint, nullptr);
        break;

    case 8:
        curState.crb = strtof(setPoint, nullptr);
        break;

    case 9:
        curState.cdt = strtof(setPoint, nullptr);
        break;

    case 10:
        curState.iat = strtof(setPoint, nullptr);
        break;

    case 11:
        curState.rpm = strtof(setPoint, nullptr);
        break;

    case 12:
        curState.map = strtof(setPoint, nullptr);
        break;

    case 13:
        curState.fuelRem = strtof(setPoint, nullptr);
        break;

    case 14:
        curState.waypointDist = strtof(setPoint, nullptr);
        break;

    case 15:
        curState.ff = strtof(setPoint, nullptr);
        break;

    case 16:
        curState.fuelCapacity = strtof(setPoint, nullptr);
        break;

    case 17:
        curState.hp = strtof(setPoint, nullptr);
        break;

    case 18:
        setProfile(atoi(setPoint));
        break;
    
    // Message ID 19 removed — normalize toggle is now driven by LF hold (3s).

    case 20:
        stepButtonStateChange((bool)atoi(setPoint));
        break;

    case 21:
        lfButtonStateChange((bool)atoi(setPoint));
        break;

    default:
        break;
    }
}

void CC_JDI830::updateCalculatedFields()
{
    int nCyl = activeProfile->numCylinders;

    // If the sim only reports a single EGT/CHT, generate fake per-cylinder
    // values from the raw sim input before computing peaks/diffs.
    if (activeProfile->reportsSingleEgtCht) {
        curState.spreadCylinders(nCyl,
                                 curState.egtRaw, curState.chtRaw,
                                 activeProfile->fakeCylSpreadPct);
    }

    curState.updateCalculated(nCyl);
}

void CC_JDI830::advanceBottomPage()
{
     _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
     _bottomBar.setPage(_bottomPages[_currentBottomPage]);
}

void CC_JDI830::stepButtonStateChange(bool pressed) {
    _buttonInput.stepChange(pressed, millis());
}

void CC_JDI830::lfButtonStateChange(bool pressed) {
    _buttonInput.lfChange(pressed, millis());
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
                    } else if (gesture == ButtonGesture::LF_TAP) {
                        // Advance to next choice: MAIN+AUX (if aux exists) or ADJUST.
                        if (activeProfile->fuelAux > 0) {
                            _fuelSetupPhase = FuelSetupPhase::FILL_MAIN_AUX;
                        } else {
                            _fuelSetupPhase = FuelSetupPhase::FILL_ADJUST;
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
                    } else if (gesture == ButtonGesture::LF_TAP) {
                        _fuelSetupPhase = FuelSetupPhase::FILL_ADJUST;
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
                    _autoScan = false;
                    _bottomBarMode = BottomBarMode::LEAN;
                    // TODO: enterLeanFind() — set up lean find display
                    break;

                case ButtonGesture::LF_HOLD_SHORT:
                    // Toggle normalize/percentage view (3-second hold).
                    curState.showNormalized = !curState.showNormalized;
                    break;

                case ButtonGesture::BOTH_HOLD:
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
                        _bottomBar.setPage(_bottomPages[_currentBottomPage]);
                        _lastPageChange = millis();
                    }
                    break;

                case ButtonGesture::LF_TAP:
                    // Enter lean find mode.
                    _displayMode = DisplayMode::LEAN_FIND;
                    _leanPhase = LeanPhase::PRE_LEAN;
                    _bottomBarMode = BottomBarMode::LEAN;
                    // TODO: enterLeanFind()
                    break;

                case ButtonGesture::LF_HOLD_SHORT:
                    // Toggle normalize/percentage view (3-second hold).
                    curState.showNormalized = !curState.showNormalized;
                    break;

                case ButtonGesture::BOTH_TAP:
                    // Toggle include/exclude current parameter from AUTO rotation.
                    // TODO: toggleParamExclude() — needs a per-param include/exclude array
                    break;

                case ButtonGesture::BOTH_HOLD:
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
                case ButtonGesture::STEP_TAP:
                    // Exit lean find, return to AUTO mode.
                    _displayMode = DisplayMode::AUTO;
                    _autoScan = true;
                    _bottomBarMode = _prevDataMode;
                    _lastPageChange = millis();
                    _bottomBar.setPage(_bottomPages[_currentBottomPage]);
                    // TODO: exitLeanFind()
                    break;

                case ButtonGesture::LF_HOLD_LONG:
                    // Show peak EGT while held (only after peak is found).
                    if (_leanPhase == LeanPhase::PEAK_FOUND) {
                        _displayMode = DisplayMode::LEAN_PEEK;
                        // TODO: showPeakEGT()
                    }
                    break;

                case ButtonGesture::BOTH_HOLD:
                    // Toggle Rich Of Peak / Lean Of Peak (only before leaning).
                    if (_leanPhase == LeanPhase::PRE_LEAN) {
                        // TODO: toggleRichLeanOfPeak()
                    }
                    break;

                default:
                    break;
            }
            break;

        case DisplayMode::LEAN_PEEK:
            switch (gesture) {
                case ButtonGesture::LF_RELEASE:
                    // LF released — return to lean find display.
                    _displayMode = DisplayMode::LEAN_FIND;
                    // TODO: hidePeakEGT()
                    break;

                default:
                    break;
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
                    _bottomBar.setPage(_bottomPages[_currentBottomPage]);
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
                step = "EXIT";
                lf   = "PEAK";
                break;

            case DisplayMode::LEAN_PEEK:
                step = "";
                lf   = "(hold)";
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
    } else if (_displayMode == DisplayMode::LEAN_FIND
            || _displayMode == DisplayMode::LEAN_PEEK) {
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
        case DisplayMode::LEAN_PEEK:      modeStr = "PEEK";    break;
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
        case ButtonGesture::BOTH_RELEASE:    gestStr = "B_REL";    break;
    }

    // Build the debug line:  "AUTO  S_TAP  S:0 L:1  CONN"
    char buf[64];
    snprintf(buf, sizeof(buf), "%-6s %-6s S:%d L:%d %s",
             modeStr, gestStr,
             _buttonInput.isStepDown() ? 1 : 0,
             _buttonInput.isLfDown() ? 1 : 0,
             _connectionLost ? "LOST" : "CONN");

    // Draw directly to LCD (not a sprite) in the bottom 15 rows.
    // Black fill first to clear previous text, then white text on black.
    _lcd.fillRect(0, 465, 320, 15, TFT_BLACK);
    _lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    _lcd.setTextDatum(ML_DATUM);  // middle-left
    _lcd.setTextSize(1);          // built-in font, small
    _lcd.drawString(buf, 4, 472);
}


void CC_JDI830::update()
{
    uint32_t now = millis();

    // --- Connection-loss detection ---
    // If MobiFlight stops sending data, release any "stuck" buttons.
    if (!_connectionLost && _lastSetTime > 0
        && (now - _lastSetTime >= CONNECTION_TIMEOUT_MS)) {
        _connectionLost = true;
        _buttonInput.forceRelease();
    }

    // --- Fuel setup timed transitions ---
    // The "FUEL" splash shows for 1 second, then auto-advances to "FILL? N".
    if (_displayMode == DisplayMode::FUEL_SETUP
        && _fuelSetupPhase == FuelSetupPhase::SHOW_FUEL
        && (now - _fuelSetupStartTime >= 1000)) {
        _fuelSetupPhase = FuelSetupPhase::ASK_FILL;
    }

    // --- Fuel setup bottom bar rendering ---
    // Uses showMessage() to display the current phase text.  Called every
    // frame so the flashing "FILL? N" text toggles and the adjust value
    // stays up to date.
    if (_displayMode == DisplayMode::FUEL_SETUP) {
        updateFuelSetupDisplay(now);

        // --- Fine-adjust hold-repeat ---
        // In FILL_ADJUST_FINE, holding LF auto-increments fuel at 300ms
        // intervals.  After 3 seconds the step jumps from 0.1 to 1.0 gal.
        // Releasing LF resets the acceleration so next hold starts at 0.1.
        if (_fuelSetupPhase == FuelSetupPhase::FILL_ADJUST_FINE) {
            if (_buttonInput.isLfDown()) {
                // Start tracking on first frame where LF is down
                if (_fuelFineHoldStart == 0) {
                    _fuelFineHoldStart  = now;
                    _fuelFineRepeatTime = now;  // first tick fires immediately
                }
                // Auto-repeat at FUEL_FINE_REPEAT_MS intervals
                if (now - _fuelFineRepeatTime >= FUEL_FINE_REPEAT_MS) {
                    _fuelFineRepeatTime = now;
                    float step = ((now - _fuelFineHoldStart) >= FUEL_FINE_ACCEL_MS)
                                 ? FUEL_FINE_STEP_LARGE
                                 : FUEL_FINE_STEP_SMALL;
                    _fuelAdjustValue += step;
                }
            } else {
                // LF released — reset acceleration so next hold starts small
                _fuelFineHoldStart  = 0;
                _fuelFineRepeatTime = 0;
            }
        }
    }

    // --- Button gesture polling ---
    // poll() checks timing thresholds and returns the next completed gesture.
    ButtonGesture gesture = _buttonInput.poll(now);
    if (gesture != ButtonGesture::NONE) {
        handleGesture(gesture);
    }

    // --- Auto-switch: MANUAL → AUTO after 2 minutes of inactivity ---
    // The device starts in MANUAL mode.  If the user doesn't interact for
    // AUTO_SWITCH_MS, it automatically enters AUTO mode with page rotation.
    if (_displayMode == DisplayMode::MANUAL
        && _manualStartTime > 0
        && (now - _manualStartTime >= AUTO_SWITCH_MS)) {
        _displayMode = DisplayMode::AUTO;
        _autoScan = true;
        _lastPageChange = now;
    }

    // Recompute all derived fields (peaks, diffs, fuel calcs, fake cylinders)
    updateCalculatedFields();

    // --- Bottom bar mode dispatch ---
    // In data modes, scan for alarms each frame.  If one fires, save the
    // current mode and switch to ALARM.  In ALARM mode, keep scanning —
    // if the condition clears (and isn't dismissed), restore the previous
    // data mode so page rotation resumes.
    switch (_bottomBarMode) {
        case BottomBarMode::ALL_DATA:
        case BottomBarMode::FUEL_DATA:
        case BottomBarMode::CYLINDER_DETAIL:
            // Scan for alarms while in a data-display mode
            if (_alarmMgr.scan(now)) {
                _prevDataMode = _bottomBarMode;
                _bottomBarMode = BottomBarMode::ALARM;
                _bottomBar.showAlarm(_alarmMgr.activeAlarmDef(), _alarmMgr.activeAlarmDef()->valuePtr);
            } else {
                // No alarm — display data pages
                if (_autoScan) {
                    if (now - _lastPageChange > PAGE_ROTATE_MS) {
                        _lastPageChange = now;
                        advanceBottomPage();
                    }
                }
            }
            break;

        case BottomBarMode::ALARM:
            // Re-scan each frame: alarm may clear if value returns to normal
            if (_alarmMgr.scan(now)) {
                // Only call showAlarm() if the active alarm changed (e.g. a
                // higher-priority alarm appeared).  If it's the same alarm,
                // leave BottomBar alone so the flash timer keeps running.
                const AlarmDef* newDef = _alarmMgr.activeAlarmDef();
                if (newDef != _bottomBar.alarmDef()) {
                    _bottomBar.showAlarm(newDef, newDef->valuePtr);
                }
            } else {
                // Alarm cleared — restore previous data mode
                _bottomBarMode = _prevDataMode;
                _bottomBar.clearAlarm();
                _bottomBar.setPage(_bottomPages[_currentBottomPage]);
                _lastPageChange = now;  // reset rotation timer
            }
            break;

        case BottomBarMode::LEAN:
        case BottomBarMode::SETUP:
            // Future modes — no alarm scanning while active
            break;
    }

    // --- Status bar labels + center indicator ---
    // Called every frame so labels always match the current state, even
    // after timed transitions (auto-switch, alarm clear) that don't go
    // through handleGesture().
    updateStatusLabels();

    // Update all gauges — each one only redraws if its value changed
    _rpmGauge.update();
    _egtChtBars.setNormalize(curState.showNormalized);
    if (activeProfile->hasMap) _mapGauge.update();
    for (int i = 0; i < _displayCfg.numRightBarSlots; i++) _rightBars[i].update();
    _egtChtBars.update();

    _bottomBar.update();
    _statusBar.update();
    _hpPct.update();

    // Debug overlay — shows mode, gesture, button state, connection status.
    drawDebugState(gesture);
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
            _bottomBar.setPage(_bottomPages[_currentBottomPage]);
            _lastPageChange = millis();
        }
    } else if (_bottomBarMode != BottomBarMode::ALARM) {
        // Advance to next page in current rotation
        if (_numBottomPages > 0) {
            _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
            _bottomBar.setPage(_bottomPages[_currentBottomPage]);
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
            _bottomBar.setPage(_bottomPages[_currentBottomPage]);
            _lastPageChange = millis();
        }
    }
}
