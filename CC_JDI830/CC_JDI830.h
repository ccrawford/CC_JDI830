#pragma once

#include "Arduino.h"

// Display hardware
#include "35tftspi480x320.h"

// Display setup
#include "DisplayConfig.hpp"

// Gauge types
#include "ArcGauge.hpp"
#include "HBarGauge.hpp"
#include "ColumnBarGauge.hpp"
#include "ValueGauge.hpp"
#include "BottomBarPages.hpp"
#include "StatusBar.hpp"

// Data types
#include "EngineState.hpp"
#include "PlaneProfiles.hpp"
#include "DisplayParams.hpp"
#include "AlarmManager.hpp"
#include "ButtonInput.hpp"

#define TFT_TRANSPARENT TFT_PINK

// ---------------------------------------------------------------------------
// BottomBarMode — which system currently "owns" the bottom bar.
//
// The bottom bar is shared between several features: rotating data pages,
// engine alarms, and (future) lean finder and device setup.  Only one mode
// is active at a time.  The three data modes (ALL_DATA, FUEL_DATA,
// CYLINDER_DETAIL) each rotate through a different set of BottomPage
// definitions; ALARM takes over when a parameter enters the RED zone.
// ---------------------------------------------------------------------------
enum class BottomBarMode : uint8_t {
    ALL_DATA,          // rotating pages showing all engine data (current behavior)
    FUEL_DATA,         // rotating pages showing fuel-related data only (future)
    CYLINDER_DETAIL,   // rotating pages showing per-cylinder detail (future)
    ALARM,             // AlarmManager is displaying a warning
    LEAN,              // future: lean finder
    SETUP,             // future: device setup
};

// ---------------------------------------------------------------------------
// DisplayMode — what the user is currently *doing* (interaction state).
//
// This is separate from BottomBarMode, which tracks what *content* the bottom
// bar shows.  One DisplayMode may map to different BottomBarMode values —
// e.g. both AUTO and MANUAL use BottomBarMode::ALL_DATA, but with different
// page-rotation behavior.
//
// The handleGesture() method uses this enum as the outer switch to decide
// what each ButtonGesture means in context.
// ---------------------------------------------------------------------------
enum class DisplayMode : uint8_t {
    FUEL_SETUP,        // power-up fuel entry wizard (runs before normal operation)
    AUTO,              // parameters rotate automatically
    MANUAL,            // user manually steps through parameters with STEP
    LEAN_FIND,         // lean find procedure is active
    LEAN_PEAK,         // showing peak EGT while LF is held (during lean find)
    PILOT_PROGRAM,     // pilot programming mode (future)
};

// ---------------------------------------------------------------------------
// FuelSetupPhase — sub-states of the power-up fuel entry wizard.
//
// On power-up the EDM shows "FUEL" for 1 second, then flashes "FILL? N".
// The user either taps STEP to skip (no fuel added) or taps LF to choose
// one of three fill options: MAIN only, MAIN+AUX, or manual adjust (+/-).
// Tapping STEP from any fill choice accepts it and exits to MANUAL mode.
// ---------------------------------------------------------------------------
enum class FuelSetupPhase : uint8_t {
    SHOW_FUEL,         // displaying "FUEL" for 1 second
    ASK_FILL,          // flashing "FILL? N" — STEP=skip, LF=yes
    FILL_MAIN,         // showing "FILL xx" (main tank preset) — STEP=accept
    FILL_MAIN_AUX,     // showing "FILL xx" (main+aux preset) — STEP=accept
    FILL_ADJUST,       // showing "FILL +" with adjustable value — LF=adjust, STEP=enter fine mode
    FILL_ADJUST_FINE,  // fine adjustment: LF tap = -0.1, LF hold = +0.1 (accelerates to +1.0)
};

// ---------------------------------------------------------------------------
// LeanPhase — sub-state within LEAN_FIND mode.
//
// The lean find procedure has its own progression.  The outer state machine
// knows we're "in lean find"; this enum tracks *where* in the procedure.
// Only meaningful when DisplayMode == LEAN_FIND or LEAN_PEAK.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ScanSwitch — position of the 3-way select switch on the instrument panel.
//
// Filters which scanner pages are shown.  Received from MobiFlight as
// message ID 23 (0=EGT, 1=ALL, 2=FF).
// ---------------------------------------------------------------------------
enum class ScanSwitch : uint8_t {
    EGT = 0,   // Temperature position — show TEMP + BOTH pages
    ALL = 1,   // All position — show everything
    FF  = 2,   // Fuel Flow position — show FUEL + BOTH pages
};

enum class LeanPhase : uint8_t {
    PRE_LEAN,          // before leaning begins — can toggle Rich/Lean of Peak
    LEANING,           // user is leaning mixture, watching EGT rise
    PEAK_FOUND,        // peak detected, showing degrees below peak
    FINALIZING,        // showing delta from peak EGT + FF (hold LF for absolute)
};

class CC_JDI830
{
public:
    CC_JDI830(uint8_t SCLK, uint8_t MOSI, uint8_t DC, uint8_t CS, uint8_t RST, uint8_t BL);
    void begin();
    void attach();
    void detach();
    void set(int16_t messageID, char *setPoint);
    void update();

    // Re-configure the right-column HBar gauges from rightBarSelections[].
    // Safe to call at any time to apply new selections without restarting.
    void setupRightBars();

    // Engine state — all live values in one place
    EngineState curState;

    // Active plane profile — use setProfile() to switch aircraft
    const PlaneProfile* activeProfile = nullptr;
    void setProfile(int index);

    // STEP button actions — call from physical button ISR, MobiFlight
    // message handler, or any other input source.
    void onStepPress();       // short press: dismiss alarm 10 min, or advance page
    void onStepLongPress();   // long press: dismiss alarm permanently

private:
    bool    _initialised;
    uint8_t _pinSCLK, _pinMOSI, _pinDC, _pinCS, _pinRST, _pinBL;

    // Display hardware
    LGFX _lcd;

    // Display config
    DisplayConfig _displayCfg;

    // Gauge instances
    ArcGauge        _rpmGauge{&_lcd};
    ArcGauge        _mapGauge{&_lcd};
    HBarGauge       _rightBars[MAX_RIGHT_BAR_SLOTS] = {
        HBarGauge(&_lcd), HBarGauge(&_lcd), HBarGauge(&_lcd), HBarGauge(&_lcd), HBarGauge(&_lcd)
        ,HBarGauge(&_lcd), HBarGauge(&_lcd), HBarGauge(&_lcd), HBarGauge(&_lcd), HBarGauge(&_lcd)
    };
    ColumnBarGauge  _egtChtBars{&_lcd};
    ValueGauge      _hpPct{&_lcd};
    BottomBar       _bottomBar{&_lcd};
    StatusBar       _statusBar{&_lcd};

    // Bottom bar page rotation state
    BottomPage _bottomPages[MAX_BOTTOM_PAGES];
    bool       _excluded[MAX_BOTTOM_PAGES] = {};  // per-page exclusion from AUTO rotation
    int        _numBottomPages   = 0;
    int        _currentBottomPage = 0;
    uint32_t   _lastPageChange   = 0;
    static constexpr uint32_t PAGE_ROTATE_MS = 5000;

    // Bottom bar mode — which system owns the bottom bar right now
    BottomBarMode _bottomBarMode = BottomBarMode::ALL_DATA;
    BottomBarMode _prevDataMode  = BottomBarMode::ALL_DATA;  // saved when entering ALARM

    // Alarm manager
    AlarmManager _alarmMgr;

    // Auto vs Manual scan
    bool _autoScan = false;  // starts false — device begins in FUEL_SETUP then MANUAL

    // 3-position select switch — filters which scanner pages are visible
    ScanSwitch _scanSwitch = ScanSwitch::ALL;

    // Current profile index (tracks activeProfile for change detection)
    // Starts at -1 so the first setProfile() call always applies.
    int _profileIndex = -1;


    void updateCalculatedFields();

    // Button / gesture input
    ButtonInput _buttonInput;
    bool _lfButtonLastState = 0;
    bool _stepButtonLastState = 0;
    void stepButtonStateChange(bool);
    void lfButtonStateChange(bool);

    // Display mode state machine
    DisplayMode _displayMode = DisplayMode::FUEL_SETUP;  // starts with fuel wizard
    LeanPhase   _leanPhase   = LeanPhase::PRE_LEAN;
    LeanPhase   _prevLeanPhase   = LeanPhase::PRE_LEAN;
    uint32_t    _leanPhaseStartTime = 0;  // millis() when current lean phase began

    // Lean finder tracking state
    // Baselines are the EGT values when lean find starts.  A cylinder is
    // "armed" (ready to detect peak) once it rises >= LEAN_ARM_RISE above
    // its baseline.  Per-cylinder peaks track the highest EGT seen so far.
    static constexpr float LEAN_ARM_RISE   = 15.0f;  // °F rise to arm
    static constexpr float LEAN_PEAK_DROP  =  1.0f;  // °F drop from peak to declare peaked
    float    _leanBaseline[MAX_CYLINDERS] = {};   // EGT snapshot when entering LEANING
    float    _leanCylPeak[MAX_CYLINDERS]  = {};   // highest EGT seen per cylinder during LEANING
    bool     _leanArmed       = false; // true once any cylinder rises >= LEAN_ARM_RISE
    int      _leanPeakedCyl   = -1;    // which cylinder peaked first (-1 = none yet)

    float    _leanDelta       = 0.0f;   // live: current EGT minus peak for peaked cylinder
    void updateLeanFinder();



    void handleGesture(ButtonGesture gesture);
    void updateStatusLabels();

    // Fuel setup wizard state
    FuelSetupPhase _fuelSetupPhase = FuelSetupPhase::SHOW_FUEL;
    uint32_t _fuelSetupStartTime = 0;  // millis() when SHOW_FUEL began
    float    _fuelAdjustValue = 0;     // working value for FILL_ADJUST (+/-)
    char     _fuelSetupBuf[32] = {};   // persistent buffer for fuel setup display text
    void updateFuelSetupDisplay(uint32_t now);



    // Fine-adjustment hold-repeat state (FILL_ADJUST_FINE phase).
    // While LF is held, increments +0.1 every 300ms.  After 3 seconds
    // of continuous hold the step size jumps to +1.0 per 300ms.
    // Releasing LF resets the step size back to 0.1.
    uint32_t _fuelFineRepeatTime = 0;   // millis() of last auto-increment
    uint32_t _fuelFineHoldStart  = 0;   // millis() when LF was first held
    static constexpr uint32_t FUEL_FINE_REPEAT_MS    = 300;
    static constexpr uint32_t FUEL_FINE_ACCEL_MS     = 3000;
    static constexpr float    FUEL_FINE_STEP_SMALL   = 0.1f;
    static constexpr float    FUEL_FINE_STEP_LARGE   = 1.0f;

    // Auto-switch: after fuel setup, enters MANUAL then AUTO after 2 minutes
    // of no STEP button activity.  Any STEP tap in MANUAL resets the timer.
    static constexpr uint32_t AUTO_SWITCH_MS = 120000;  // 2 minutes
    uint32_t _manualStartTime = 0;  // set to millis() when entering MANUAL

    // Connection-loss detection — if no set() message arrives for this long,
    // we assume MobiFlight disconnected and force-release all buttons.
    static constexpr uint32_t CONNECTION_TIMEOUT_MS = 10000;
    uint32_t _lastSetTime    = 0;     // millis() of most recent set() call
    bool     _connectionLost = false; // true after CONNECTION_TIMEOUT_MS silence
    
    // Debug overlay — draws state info to the bottom ~15px of the LCD.
    // Set to false to disable (zero cost when off — the compiler removes the code).
    static constexpr bool DEBUG_OVERLAY = true;
    void drawDebugState(ButtonGesture gesture);

    // Private helpers
    void advanceBottomPage();
    void advanceBottomPageAuto();  // advance skipping excluded pages (AUTO mode)
    void showCurrentPage();        // push current page to bottom bar with excluded flag
    void toggleParamExclude();     // toggle exclude on current page (BOTH_TAP in MANUAL)
    void setupGauges();
    void drawStatic();
    static void applyRangeDef(Gauge& gauge, const GaugeRangeDef& def);
};
