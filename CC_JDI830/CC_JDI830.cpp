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

    drawStatic();
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

void CC_JDI830::update()
{
    uint32_t now = millis();

    // Recompute all derived fields (peaks, diffs, fuel calcs, fake cylinders)
    updateCalculatedFields();

    // Update simulated values every 100ms for demo purposes
    /* if (now - _lastUpdate > 200) {
        _lastUpdate = now;

        // Bounce RPM between 2000 and 2700
        // curState.rpm += _rpmDelta;
        // if (curState.rpm > 2800) { curState.rpm = 2700; _rpmDelta = -10.0f; }
        // if (curState.rpm < 2000) { curState.rpm = 2000; _rpmDelta = 10.0f; }

        curState.map += 0.1f;
        if (curState.map > 33) curState.map =14.0f;

        curState.egt[4] += 10;
        if (curState.egt[4] > 1800) curState.egt[4] = 850;

        curState.cht[1] += 10;
        if (curState.cht[1] > 500) curState.cht[1] = 250;

        curState.hp += 1;
        if (curState.hp > 100) curState.hp = 0;

        // Slowly vary EGT on cylinder 3
        curState.egt[2] = 1500 + sin(now / 2000.0f) * 100;

        curState.fuelRem -= 0.1;
        if (curState.fuelRem < 0) curState.fuelRem = 60;

        // Recompute peaks
        int nCyl = activeProfile->numCylinders;
        auto egtIt = std::max_element(curState.egt, curState.egt + nCyl);
        curState.egtPeak    = *egtIt;
        curState.egtPeakCyl = egtIt - curState.egt;

        auto chtIt = std::max_element(curState.cht, curState.cht + nCyl);
        curState.chtPeak    = *chtIt;
        curState.chtPeakCyl = chtIt - curState.cht;

        curState.selectedCylinder = 3;
        _egtChtBars.setSelectedCylinder(curState.selectedCylinder);

        // Keep selected-cylinder copies up to date
        curState.egtSelected = curState.egt[curState.selectedCylinder];
        curState.chtSelected = curState.cht[curState.selectedCylinder];

//        _egtChtBars.setSelectedCylinder(curState.chtPeakCyl);
    }
*/

    // --- Bottom bar mode dispatch ---
    // In data modes, scan for alarms each frame.  If one fires, save the
    // current mode and switch to ALARM.  In ALARM mode, keep scanning —
    // if the condition clears (and isn't dismissed), restore the previous
    // data mode so page rotation resumes.
    //
    // The `switch` here is the idiomatic C++ way to dispatch on an enum
    // class.  Each case handles one mode's behavior.  LEAN/SETUP are
    // stubs — they just break (do nothing) so alarms don't interrupt
    // those future features.
    switch (_bottomBarMode) {
        case BottomBarMode::ALL_DATA:
        case BottomBarMode::FUEL_DATA:
        case BottomBarMode::CYLINDER_DETAIL:
            // Scan for alarms while in a data-display mode
            if (_alarmMgr.scan(now)) {
                _prevDataMode = _bottomBarMode;
                _bottomBarMode = BottomBarMode::ALARM;
                _alarmMgr.formatAlarm(_alarmBuf, sizeof(_alarmBuf));
                _bottomBar.showMessage(_alarmBuf, TFT_RED);
            } else {
                // No alarm — auto-rotate pages as before
                if (now - _lastPageChange > PAGE_ROTATE_MS) {
                    _lastPageChange = now;
                    _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
                    _bottomBar.setPage(_bottomPages[_currentBottomPage]);
                }
            }
            break;

        case BottomBarMode::ALARM:
            // Re-scan each frame: alarm may clear if value returns to normal
            if (_alarmMgr.scan(now)) {
                // Still alarming — update the message (value may have changed)
                _alarmMgr.formatAlarm(_alarmBuf, sizeof(_alarmBuf));
                _bottomBar.showMessage(_alarmBuf, TFT_RED);
            } else {
                // Alarm cleared — restore previous data mode
                _bottomBarMode = _prevDataMode;
                _bottomBar.clearMessage();
                _lastPageChange = now;  // reset rotation timer
            }
            break;

        case BottomBarMode::LEAN:
        case BottomBarMode::SETUP:
            // Future modes — no alarm scanning while active
            break;
    }

    // Update all gauges — each one only redraws if its value changed
    _rpmGauge.update();
    if (activeProfile->hasMap) _mapGauge.update();
    for (int i = 0; i < _displayCfg.numRightBarSlots; i++) _rightBars[i].update();
    _egtChtBars.update();

    _bottomBar.update();
    _hpPct.update();

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
            _alarmMgr.formatAlarm(_alarmBuf, sizeof(_alarmBuf));
            _bottomBar.showMessage(_alarmBuf, TFT_RED);
        } else {
            _bottomBarMode = _prevDataMode;
            _bottomBar.clearMessage();
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
            _alarmMgr.formatAlarm(_alarmBuf, sizeof(_alarmBuf));
            _bottomBar.showMessage(_alarmBuf, TFT_RED);
        } else {
            _bottomBarMode = _prevDataMode;
            _bottomBar.clearMessage();
            _lastPageChange = millis();
        }
    }
}
