#pragma once

#include "Arduino.h"

// Display hardware
#include "35tftspi480x320.h"

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

    // --- Right-column gauge selections (mutable at runtime) ---
    // Change these to reconfigure which gauges appear in the right column.
    // Call setupRightBars() after changing to apply the new layout.
    static constexpr int NUM_RIGHT_BAR_SLOTS = 5;
    DisplayVarId rightBarSelections[NUM_RIGHT_BAR_SLOTS] = {
        DisplayVarId::OIL_TEMP,
        DisplayVarId::FUEL_FLOW,
        DisplayVarId::FUEL_REM,
        DisplayVarId::OAT,
        DisplayVarId::BATTERY,
    };

    // Engine state — all live values in one place
    EngineState curState;

    // Active plane profile — use setProfile() to switch aircraft
    const PlaneProfile* activeProfile = nullptr;
    void setProfile(int index);

private:
    bool    _initialised;
    uint8_t _pinSCLK, _pinMOSI, _pinDC, _pinCS, _pinRST, _pinBL;

    // Display hardware
    LGFX _lcd;

    // Gauge instances
    ArcGauge        _rpmGauge{&_lcd};
    ArcGauge        _mapGauge{&_lcd};
    HBarGauge       _rightBars[NUM_RIGHT_BAR_SLOTS] = {
        HBarGauge(&_lcd), HBarGauge(&_lcd), HBarGauge(&_lcd),
        HBarGauge(&_lcd), HBarGauge(&_lcd)
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

    // Current profile index (tracks activeProfile for change detection)
    // Starts at -1 so the first setProfile() call always applies.
    int _profileIndex = -1;

    // Demo animation state
    uint32_t _lastUpdate = 0;
    float    _rpmDelta   = 10.0f;

    // Private helpers
    void setupGauges();
    void drawStatic();
    static void applyRangeDef(Gauge& gauge, const GaugeRangeDef& def);
};
