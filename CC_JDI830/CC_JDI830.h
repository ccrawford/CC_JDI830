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

// Data types
#include "EngineState.hpp"
#include "PlaneProfiles.hpp"
#include "DisplayParams.hpp"
#include "AlarmManager.hpp"

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

    // Bottom bar page rotation state
    BottomPage _bottomPages[MAX_BOTTOM_PAGES];
    int        _numBottomPages   = 0;
    int        _currentBottomPage = 0;
    uint32_t   _lastPageChange   = 0;
    static constexpr uint32_t PAGE_ROTATE_MS = 5000;

    // Bottom bar mode — which system owns the bottom bar right now
    BottomBarMode _bottomBarMode = BottomBarMode::ALL_DATA;
    BottomBarMode _prevDataMode  = BottomBarMode::ALL_DATA;  // saved when entering ALARM

    // Alarm manager
    AlarmManager _alarmMgr;
    char         _alarmBuf[20];   // buffer for formatted alarm message

    // Current profile index (tracks activeProfile for change detection)
    // Starts at -1 so the first setProfile() call always applies.
    int _profileIndex = -1;

    // Demo animation state
    uint32_t _lastUpdate = 0;
    float    _rpmDelta   = 10.0f;

    void updateCalculatedFields();

    // Private helpers
    void setupGauges();
    void drawStatic();
    static void applyRangeDef(Gauge& gauge, const GaugeRangeDef& def);
};
