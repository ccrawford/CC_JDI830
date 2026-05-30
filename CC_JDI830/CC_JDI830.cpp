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
    gauge.clearColorRanges();
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
    _rpmGauge.setValueFont(arcValueFont);
    _rpmGauge.setLabelFont(ArialNB12);
    _rpmGauge.setLabelY(L.rpmArc.labelY);
    _rpmGauge.setHPCutout(L.rpmHpCutout.x, L.rpmHpCutout.y,
                          L.rpmHpCutout.w, L.rpmHpCutout.h);
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
        _mapGauge.setDecimals(1);
        _mapGauge.setValueFont(arcValueFont);
        _mapGauge.setLabelFont(ArialNB12);
        _mapGauge.setLabelY(L.mapArc.labelY);
        _mapGauge.setHPCutout(L.mapHpCutout.x, L.mapHpCutout.y,
                              L.mapHpCutout.w, L.mapHpCutout.h);
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

    _profileIndex = index;
    activeProfile = ALL_PROFILES[index];
    curState.lastCoolingUpdate = 0;          // force cooling rate re-bootstrap

    rebuildLayout();

    // Build the alarm table for this profile — pointers into curState
    // and the new profile are captured so scan() can check them each frame.
    _alarmMgr.buildAlarms(*activeProfile, curState);
    _bottomBarMode = BottomBarMode::ALL_DATA;

    // If the EGT-switch cycle was active, rebuild its page against the new
    // profile (slot count, gauge color ranges, TIT presence may all differ).
    if (_egtCycleActive) {
        _egtCycleIdx = 0;
        refreshEgtCyclePage();
    }
}

// ---------------------------------------------------------------------------
// setLayout — switch screen orientation at runtime.
// No-op if the requested mode is already active.  Requires an active profile
// (rebuildLayout() dereferences activeProfile via buildDefaultConfig()).
// ---------------------------------------------------------------------------
void CC_JDI830::setLayout(LayoutMode mode) {
    if (mode == _layoutMode) return;
    if (!activeProfile) return;   // begin() hasn't run yet — _layoutMode set, will take effect at first setProfile()
    _layoutMode = mode;
    rebuildLayout();
}

// ---------------------------------------------------------------------------
// rebuildLayout — clear the screen, rebuild the DisplayConfig for the current
// profile + layout mode, re-init all gauge sprites with new dimensions, and
// force a full redraw.  Shared by setProfile() and setLayout() since both
// invalidate every sprite on screen.
// ---------------------------------------------------------------------------
void CC_JDI830::rebuildLayout() {
    _lcd.fillScreen(TFT_BLACK);

    _displayCfg = buildDefaultConfig(*activeProfile, _layoutMode);
    _lcd.setRotation(_displayCfg.layout->rotation);

    setupGauges();

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
    _pinDC   = dc;
    _pinCS   = cs;
    _pinRST  = rst;
    _pinBL   = bl;
}

void CC_JDI830::begin()
{
    _lcd.configurePins(_pinSCLK, _pinMOSI, _pinDC, _pinCS, _pinRST, _pinBL);
    _lcd.init();
    _lcd.setBrightness(255);
    _lcd.fillScreen(TFT_BLACK);

    setProfile(0);

    _fuelSetupStartTime = millis();

    pinMode(STEP_PIN, INPUT_PULLUP);
    pinMode(LF_PIN, INPUT_PULLUP);
    pinMode(SCAN_FF_PIN,  INPUT_PULLUP);
    pinMode(SCAN_EGT_PIN, INPUT_PULLUP);
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
        // Writes the *raw* per-cylinder buffer.  EngineState::updateCalculated()
        // EMA-blends these into the smoothed egt[] array each frame.
        // When the profile says the sim only reports one EGT, stash it in
        // egtRaw and let spreadCylinders() build the per-cylinder values
        // (which it now also writes into egtRawPerCyl[]).
       cmdMessenger.sendCmd(kStatus,setPoint);
       int nCyl = activeProfile->numCylinders;
       if (strchr(setPoint, '|')) {
           char* tok = strtok(setPoint, "|");
           for (int i = 0; i < nCyl && tok != nullptr; i++) {
               curState.egtRawPerCyl[i] = strtof(tok, nullptr);
               tok = strtok(nullptr, "|");
            }
        } else {
            float val = strtof(setPoint, nullptr);
            if (activeProfile->reportsSingleEgtCht) {
                curState.egtRaw = val;
            } else {
                // Profile expects per-cylinder data but we only got one value.
                // Write to cyl 0 only so the lopsided display makes it obvious
                // something's misconfigured.
                curState.egtRawPerCyl[0] = val;
            }
        }
        break;
    }

    case 1: {
        // CHT — same pipe-delimited or single-value format as EGT.
        // Writes the raw per-cylinder buffer; see case 0 for details.
        int nCyl = activeProfile->numCylinders;
        if (strchr(setPoint, '|')) {
            char* tok = strtok(setPoint, "|");
            for (int i = 0; i < nCyl && tok != nullptr; i++) {
                curState.chtRawPerCyl[i] = strtof(tok, nullptr);
                tok = strtok(nullptr, "|");
            }
        } else {
            float val = strtof(setPoint, nullptr);
            if (activeProfile->reportsSingleEgtCht) {
                curState.chtRaw = val;
            } else {
                curState.chtRawPerCyl[0] = val;
            }
        }
        break;
    }

    case 2:
        curState.tit1Raw = strtof(setPoint, nullptr);
        break;

    case 3:
        curState.tit2Raw = strtof(setPoint, nullptr);
        break;

    case 4:
        curState.oilTRaw = strtof(setPoint, nullptr);
        break;

    case 5:
        curState.oilPRaw = strtof(setPoint, nullptr);
        break;

    case 6:
        curState.batRaw = strtof(setPoint, nullptr);
        break;

    case 7:
        curState.oatRaw = strtof(setPoint, nullptr);
        break;

    case 8:
        curState.crbRaw = strtof(setPoint, nullptr);
        break;

    case 9:
        curState.cdtRaw = strtof(setPoint, nullptr);
        break;

    case 10:
        curState.iatRaw = strtof(setPoint, nullptr);
        break;

    case 11:
        curState.rpmRaw = strtof(setPoint, nullptr);
        break;

    case 12:
        curState.mapRaw = strtof(setPoint, nullptr);
        break;

    case 13:
        curState.fuelRem = strtof(setPoint, nullptr);
        break;

    case 14:
        curState.waypointDist = strtof(setPoint, nullptr);
        break;

    case 15:
        curState.ffRaw = strtof(setPoint, nullptr);
        break;

    case 16: {
        float newCap = strtof(setPoint, nullptr);
        if (newCap != curState.fuelCapacity) {
            curState.fuelCapacity = newCap;
            setupRightBars();  // FUEL_REM bar range tracks sim-reported capacity
        }
        break;
    }

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

    case 23: {
        // 3-position select switch: 0=EGT (temps), 1=ALL, 2=FF (fuel)
        int val = atoi(setPoint);
        if (val >= 0 && val <= 2)
            _scanSwitch = static_cast<ScanSwitch>(val);
        break;
    }

    case 24:
        curState.used = strtof(setPoint, nullptr);
        break;

    case 25: {
        // Screen layout: 0 = portrait, 1 = landscape
        int val = atoi(setPoint);
        if (val == 0 || val == 1)
            setLayout(static_cast<LayoutMode>(val));
        break;
    }

    case 26:
        curState.fuelPRaw = strtof(setPoint, nullptr);
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

    curState.updateCalculated(nCyl, millis());

    // --- Box cylinder + cycle live values -------------------------------
    // Outside EGT-switch mode the boxed cylinder follows the hottest EGT
    // (already computed into egtPeakCyl above).  Inside EGT-switch mode,
    // _egtCycleIdx names the active slot — for cylinder slots, write the
    // cycle pointers from egt[]/cht[]; for TIT slots, point egtCycle at
    // the TIT value (chtCycle stays at the last cylinder value but the
    // SINGLE TIT page ignores it).
    if (_egtCycleActive && activeProfile) {
        int idx = _egtCycleIdx;
        if (idx < nCyl) {
            curState.selectedCylinder = idx;
            curState.egtCycle = curState.egt[idx];
            curState.chtCycle = curState.cht[idx];
        } else {
            bool isT2 = (idx == nCyl + 1)
                     || (idx == nCyl && !activeProfile->hasTit1);
            curState.egtCycle = isT2 ? curState.tit2 : curState.tit1;
            // chtCycle unused on TIT slots
        }
    } else {
        // Normal operation: box tracks hottest cylinder.  Only push to the
        // gauge when the value changes — setSelectedCylinder() unconditionally
        // marks the gauge dirty.
        int hot = curState.egtPeakCyl;
        if (hot >= 0 && hot < nCyl && curState.selectedCylinder != hot) {
            curState.selectedCylinder = hot;
            _egtChtBars.setSelectedCylinder(hot);
        }
    }

    // --- CHT cooling rate (worst cylinder, deg/min) ---
    curState.updateCoolingRate(millis(), nCyl);
}

// ---------------------------------------------------------------------------
// pageMatchesSwitch — does this page's ScanGroup pass the current switch?
//
// ScanGroup is a bitmask: TEMP=0x01, FUEL=0x02, BOTH=0x03.
// ScanSwitch::ALL shows everything; EGT shows pages with TEMP bit;
// FF shows pages with FUEL bit.  BOTH pages always pass.
// ---------------------------------------------------------------------------
static bool pageMatchesSwitch(ScanGroup group, ScanSwitch sw)
{
    if (sw == ScanSwitch::ALL) return true;
    uint8_t mask = (sw == ScanSwitch::EGT) ? 0x01 : 0x02;
    return (static_cast<uint8_t>(group) & mask) != 0;
}

// ---------------------------------------------------------------------------
// advanceBottomPage — manual mode: advance to next page that matches the
// current scan switch position.
// ---------------------------------------------------------------------------
void CC_JDI830::advanceBottomPage()
{
    if (_numBottomPages <= 0) return;
    for (int i = 0; i < _numBottomPages; i++) {
        _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
        if (pageMatchesSwitch(_bottomPages[_currentBottomPage].group, _scanSwitch))
            break;
    }
    showCurrentPage();
}

// ---------------------------------------------------------------------------
// advanceBottomPageAuto — advance to the next non-excluded page.
//
// Used by the AUTO mode timer.  Skips pages that the user has excluded.
// The loop is bounded by _numBottomPages to prevent infinite looping if
// every excludable page is excluded (non-excludable pages always remain).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// advanceBottomPageAuto — advance to next page that is not excluded AND
// matches the current scan switch position.
// ---------------------------------------------------------------------------
void CC_JDI830::advanceBottomPageAuto()
{
    if (_numBottomPages <= 0) return;
    for (int i = 0; i < _numBottomPages; i++) {
        _currentBottomPage = (_currentBottomPage + 1) % _numBottomPages;
        if (!_excluded[_currentBottomPage] &&
            pageMatchesSwitch(_bottomPages[_currentBottomPage].group, _scanSwitch))
            break;
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

// ---------------------------------------------------------------------------
// EGT-switch cylinder/TIT cycle
//
// When the scan switch is in EGT, the bottom bar walks each cylinder's
// EGT/CHT (DUAL page) and then any TITs (SINGLE page).  The boxed
// cylinder-number indicator under the column-bar gauge tracks the cycle.
//
// We synthesize _egtCyclePage in place rather than baking N per-cylinder
// entries into _bottomPages[].  The actual values come from
// curState.egtCycle / curState.chtCycle, which updateCalculatedFields()
// writes each frame from the appropriate cylinder (or TIT) — so the
// bottom bar's value-change detector sees live updates between page
// advances without us having to rebuild the page.
// ---------------------------------------------------------------------------

// Total slots in the cycle: one per cylinder + one per TIT (0, 1, or 2).
static inline int egtCycleSlotCount(const PlaneProfile& p)
{
    int n = p.numCylinders;
    if (p.hasTit1) n++;
    if (p.hasTit2) n++;
    return n;
}

void CC_JDI830::enterEgtCycle()
{
    _egtCycleActive = true;
    _egtCycleIdx    = 0;
    refreshEgtCyclePage();
}

void CC_JDI830::advanceEgtCycle(int dir)
{
    if (!activeProfile) return;
    int slots = egtCycleSlotCount(*activeProfile);
    if (slots <= 0) return;

    // (idx + dir + slots) % slots — adding `slots` keeps the result
    // non-negative even when dir is -1 (C++ % on negatives is
    // implementation-defined for sign, so we sidestep it).
    _egtCycleIdx = ((_egtCycleIdx + dir) % slots + slots) % slots;
    refreshEgtCyclePage();
}

void CC_JDI830::refreshEgtCyclePage()
{
    if (!activeProfile) return;
    const PlaneProfile& p = *activeProfile;
    int nCyl  = p.numCylinders;
    int slots = egtCycleSlotCount(p);
    if (slots <= 0) return;

    // Defensive clamp — profile may have changed underneath us, leaving
    // _egtCycleIdx past the new slot count.
    if (_egtCycleIdx >= slots) _egtCycleIdx = 0;
    int idx = _egtCycleIdx;

    if (idx < nCyl) {
        // Cylinder slot — DUAL page with EGT N / CHT N.
        // Update selected cylinder so the column-bar box tracks the cycle.
        // egtSelected/chtSelected get refreshed in updateCalculated() but
        // we also feed egtCycle/chtCycle to make the bottom-bar page point
        // at fresh values immediately on this frame.
        curState.selectedCylinder = idx;
        _egtChtBars.setSelectedCylinder(idx);

        snprintf(_egtCycleLabelL, sizeof(_egtCycleLabelL), "EGT%d", idx + 1);
        snprintf(_egtCycleLabelR, sizeof(_egtCycleLabelR), "CHT%d", idx + 1);

        _egtCyclePage.mode      = BottomMode::DUAL;
        _egtCyclePage.leftDraw  = drawDualLabelValue;
        _egtCyclePage.left      = makeValueDef(_egtCycleLabelL, &curState.egtCycle,
                                               0, nullptr, p.egt);
        _egtCyclePage.rightDraw = drawDualLabelValue;
        _egtCyclePage.right     = makeValueDef(_egtCycleLabelR, &curState.chtCycle,
                                               0, nullptr, p.cht);
        _egtCyclePage.nonExcludable = true;
        _egtCyclePage.group     = ScanGroup::TEMP;
    } else {
        // TIT slot — SINGLE page.  Suppress the column-bar box.
        _egtChtBars.setSelectedCylinder(-1);

        // Slot ordering: cylinders, then TIT1 (if present), then TIT2.
        bool isT2 = (idx == nCyl + 1) || (idx == nCyl && !p.hasTit1);
        const float* ptr = isT2 ? &curState.tit2 : &curState.tit1;
        const GaugeRangeDef& rng = isT2 ? p.tit2 : p.tit1;
        snprintf(_egtCycleLabelL, sizeof(_egtCycleLabelL), "%s", isT2 ? "TIT2" : "TIT");

        _egtCyclePage.mode      = BottomMode::SINGLE;
        _egtCyclePage.leftDraw  = drawSingleValueUnits;
        _egtCyclePage.left      = makeValueDef(_egtCycleLabelL, ptr, 0, nullptr, rng);
        _egtCyclePage.rightDraw = nullptr;
        _egtCyclePage.right     = BottomValueDef{};
        _egtCyclePage.nonExcludable = true;
        _egtCyclePage.group     = ScanGroup::TEMP;
    }

    _bottomBar.setPage(_egtCyclePage, false);
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
    bool stepBtn = !digitalRead(STEP_PIN);
    if(stepBtn != _stepButtonLastState) 
    {
        stepButtonStateChange(stepBtn);
        _stepButtonLastState = stepBtn;
    }

    bool lfBtn = !digitalRead(LF_PIN);
    if(lfBtn != _lfButtonLastState) {
        lfButtonStateChange(lfBtn);
        _lfButtonLastState = lfBtn;
    }

    // 3-way scan switch — read hardware pins directly (active-low, pull-up).
    // FF pin low → fuel pages; EGT pin low → temp pages; neither → all pages.
    {
        ScanSwitch hw;
        if      (!digitalRead(SCAN_FF_PIN))  hw = ScanSwitch::FF;
        else if (!digitalRead(SCAN_EGT_PIN)) hw = ScanSwitch::EGT;
        else                                 hw = ScanSwitch::ALL;
        if (hw != _scanSwitch) {
            _scanSwitch = hw;
            if (hw == ScanSwitch::EGT) {
                // Entering EGT: take over the bottom bar with the
                // per-cylinder/TIT cycle.  Auto-rotate timing is reused.
                enterEgtCycle();
                _lastPageChange = millis();
            } else {
                // Leaving EGT: release the cycle.  The per-frame default
                // in updateCalculatedFields() will restore the box to the
                // hottest cylinder on the next pass.
                _egtCycleActive = false;
                if (_autoScan) advanceBottomPageAuto();
                else           advanceBottomPage();
            }
        }
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
                        if (_egtCycleActive) advanceEgtCycle(+1);
                        else                 advanceBottomPageAuto();
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
    // drawDebugState(gesture);
}

