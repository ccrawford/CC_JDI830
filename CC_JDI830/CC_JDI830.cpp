#include "CC_JDI830.h"
#include "allocateMem.h"
#include "commandmessenger.h"

#include <Arduino.h>

// Font includes — only fonts used in setupGauges() and setupRightBars()
#include "Fonts/SevenSeg30.h"
#include "Fonts/SevenSeg32.h"
#include "Fonts/ArialNB12.h"
#include "Fonts/ArialN11.h"
#include "Fonts/ArialN16.h"
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

    int barH        = _displayCfg.barH;
    int barW        = _displayCfg.barW;
    int barX        = _displayCfg.barX;
    int barY        = _displayCfg.barY;
    int barYSpacing = _displayCfg.barYSpacing;

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
    const GaugeLayout&  L = *_displayCfg.layout;

    // Pick arc value font based on orientation — landscape arcs are smaller
    const uint8_t* arcValueFont =
        (L.screenW > L.screenH) ? SevenSeg30 : SevenSeg32;

    // --- RPM Arc Gauge ---
    _rpmGauge.setPosition(L.rpm.x, L.rpm.y);
    _rpmGauge.setLabel("RPM");
    _rpmGauge.setValuePtr(&curState.rpm);
    _rpmGauge.setArcGeometry(L.rpmArc.cx, L.rpmArc.cy,
                              L.rpmArc.outerR, L.rpmArc.innerR);
    _rpmGauge.setAngles(L.rpmArc.startAngle, L.rpmArc.endAngle);
    _rpmGauge.setNeedle(L.rpmArc.needleLen, TFT_WHITE);
    _rpmGauge.setValueFont(arcValueFont);
    _rpmGauge.setLabelFont(ArialNB12);
    _rpmGauge.setLabelY(L.rpmArc.labelY);
    applyRangeDef(_rpmGauge, p.rpm);
    _rpmGauge.init(L.rpm.w, L.rpm.h);

    // --- MAP Arc Gauge ---
    if (p.hasMap) {
        _mapGauge.setPosition(L.map.x, L.map.y);
        _mapGauge.setLabel("MAP");
        _mapGauge.setValuePtr(&curState.map);
        _mapGauge.setArcGeometry(L.mapArc.cx, L.mapArc.cy,
                                  L.mapArc.outerR, L.mapArc.innerR);
        _mapGauge.setInverted(L.mapInverted);
        _mapGauge.setAngles(L.mapArc.startAngle, L.mapArc.endAngle);
        _mapGauge.setNeedle(L.mapArc.needleLen, TFT_WHITE);
        _mapGauge.setDecimals(1);
        _mapGauge.setValueFont(arcValueFont);
        _mapGauge.setLabelFont(ArialNB12);
        _mapGauge.setLabelY(L.mapArc.labelY);
        applyRangeDef(_mapGauge, p.map);
        _mapGauge.init(L.map.w, L.map.h);
    }

    // --- %HP Value Gauge ---
    if (p.hasHp) {
        _hpPct.setPosition(L.hpPct.x, L.hpPct.y);
        _hpPct.setLabel("% HP");
        _hpPct.setValuePtr(&curState.hp);
        _hpPct.setValueFont(ArialN16);
        _hpPct.setLabelFont(ArialN11);
        _hpPct.setValueColor(TFT_WHITE);
        _hpPct.setLayout(L.hpGapCenter, LABEL_RIGHT);
        _hpPct.init(L.hpPct.w, L.hpPct.h);
    }

    // --- HBar Gauges (right column) — configured from rightBarSelections[] ---
    setupRightBars();

    // --- EGT/CHT Column Bars (with optional TIT) ---
    // Layout is computed automatically from sprite size, cylinder count, and TIT count.
    _egtChtBars.setPosition(L.egtCht.x, L.egtCht.y);
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
    _egtChtBars.init(_displayCfg.egtChtWidth, L.egtCht.h);

    // --- Bottom bar ---
    _numBottomPages = buildBottomPages(_bottomPages, p, curState);
    memset(_excluded, 0, sizeof(_excluded));  // reset all exclusions on profile change / power-up
    _bottomBar.setPosition(L.bottomBar.x, L.bottomBar.y);
    _bottomBar.setMessageFont(ArialNI36);
    _bottomBar.init(L.bottomBar.w, L.bottomBar.h);
    if (_numBottomPages > 0)
        _bottomBar.setPage(_bottomPages[0]);

    _statusBar.setPosition(L.statusBar.x, L.statusBar.y);
    _statusBar.setLabelPositions(L.stepLabelPos, L.lfLabelPos);
    _statusBar.setVertical(L.statusVertical);
    _statusBar.init(L.statusBar.w, L.statusBar.h);
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
    curState.lastCoolingUpdate = 0;          // force cooling rate re-bootstrap

    _displayCfg = buildDefaultConfig(*activeProfile);
    _lcd.setRotation(_displayCfg.layout->rotation);

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
    _statusBar.forceDirty();

    drawStatic();
}

void CC_JDI830::drawStatic() {
    const GaugeLayout& L = *_displayCfg.layout;
    for (int i = 0; i < L.numStaticLines; i++) {
        const StaticLine& sl = L.staticLines[i];
        bool horizontal = (sl.y0 == sl.y1);
        _lcd.drawLine(sl.x0, sl.y0, sl.x1, sl.y1, TFT_LIGHTGRAY);
        // Draw a second line 1px offset for visibility (2px wide)
        if (horizontal)
            _lcd.drawLine(sl.x0, sl.y0 + 1, sl.x1, sl.y1 + 1, TFT_LIGHTGRAY);
        else
            _lcd.drawLine(sl.x0 + 1, sl.y0, sl.x1 + 1, sl.y1, TFT_LIGHTGRAY);
    }
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
    // Rotation is set in setProfile() from the active GaugeLayout,
    // so the first setProfile(0) call below handles it.
    _lcd.fillScreen(TFT_BLACK);

    // Load default profile (later: read saved index from NVS here)
    setProfile(0);

    // Device starts in FUEL_SETUP mode — record start time for the
    // 1-second "FUEL" splash, then the wizard takes over.
    _fuelSetupStartTime = millis();

    pinMode(0, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
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
            //    Serial.printf("tok %d:%s\n",i, tok);
               curState.egt[i] = strtof(tok, nullptr);
         //      tok = strtok(nullptr, "|");
         //      Serial.printf("token %d: %f\n",i, curState.egt[i]);
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

    // case 20:
    //     stepButtonStateChange((bool)atoi(setPoint));
    //     break;

    // case 21:
    //     lfButtonStateChange((bool)atoi(setPoint));
    //     break;

    case 22:
        curState.mpg = strtof(setPoint, nullptr);
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


    // --- CHT cooling rate (worst cylinder, deg/min) ---
    curState.updateCoolingRate(millis(), nCyl);
}

void CC_JDI830::advanceBottomPage()
{
     _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
     showCurrentPage();
}

// ---------------------------------------------------------------------------
// advanceBottomPageAuto — advance to the next non-excluded page.
//
// Used by the AUTO mode timer.  Skips pages that the user has excluded.
// The loop is bounded by _numBottomPages to prevent infinite looping if
// every excludable page is excluded (non-excludable pages always remain).
// ---------------------------------------------------------------------------
void CC_JDI830::advanceBottomPageAuto()
{
    if (_numBottomPages <= 0) return;
    for (int i = 0; i < _numBottomPages; i++) {
        _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
        if (!_excluded[_currentBottomPage]) break;
    }
    showCurrentPage();
}

// ---------------------------------------------------------------------------
// showCurrentPage — push the current bottom page to the BottomBar widget,
// including the excluded flag so the draw functions can render the dot prefix.
// ---------------------------------------------------------------------------
void CC_JDI830::showCurrentPage()
{
    _bottomBar.setPage(_bottomPages[_currentBottomPage],
                       _excluded[_currentBottomPage]);
}

// ---------------------------------------------------------------------------
// toggleParamExclude — toggle the current page's excluded-from-AUTO status.
//
// Called on BOTH_TAP in MANUAL mode.  Non-excludable pages (EGT, CHT, TIT,
// OIL_TEMP) silently ignore the toggle.  The display updates immediately so
// the dot-before-label indicator appears or disappears.
// ---------------------------------------------------------------------------
void CC_JDI830::toggleParamExclude()
{
    if (_numBottomPages <= 0) return;
    if (_bottomPages[_currentBottomPage].nonExcludable) return;

    _excluded[_currentBottomPage] = !_excluded[_currentBottomPage];
    showCurrentPage();
}

// handleGesture, updateFuelSetupDisplay, updateStatusLabels, drawDebugState,
// onStepPress, onStepLongPress, stepButtonStateChange, lfButtonStateChange
// are in CC_JDI830_buttons.cpp

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

    // Change: Let's try managing buttons locally. 
    bool stepBtn = !digitalRead(0);
    if(stepBtn != _stepButtonLastState) 
    {
        stepButtonStateChange(stepBtn);
        _stepButtonLastState = stepBtn;
    }

    bool lfBtn = !digitalRead(14);
    if(lfBtn != _lfButtonLastState) {
        lfButtonStateChange(lfBtn);
        _lfButtonLastState = lfBtn;
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

    if(_displayMode == DisplayMode::LEAN_FIND
       || _displayMode == DisplayMode::LEAN_PEAK) {
        updateLeanFinder();
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
                        advanceBottomPageAuto();
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
                showCurrentPage();
                _lastPageChange = now;  // reset rotation timer
            }
            break;

        case BottomBarMode::LEAN:
            break;
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

    // HP% is drawn last so it paints over any overlapping arc sprites.
    // In landscape mode the HP% gauge sits between the two arcs, overlapping
    // their sprite rectangles.  Force it dirty so it always repaints on top.
    if (_displayCfg.layout->screenW > _displayCfg.layout->screenH)
        _hpPct.forceDirty();
    _hpPct.update();

    // Debug overlay — shows mode, gesture, button state, connection status.
  //  drawDebugState(gesture);
}

