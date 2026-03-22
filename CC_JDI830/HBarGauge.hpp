#pragma once
#include "JPIGauges.hpp"

// ---------------------------------------------------------------------------
// HBarGauge — horizontal bar gauge with color bands and triangle pointer.
//
// Used for: O-T, GPH, REM, OAT-C, BAT on the JPI 830.
// Layout (left to right): numeric value | color bar | label
//
// Typical size: ~100 x 18 pixels per gauge.
// ---------------------------------------------------------------------------
class HBarGauge : public Gauge {
    // C++ note: `public Gauge` means HBarGauge inherits everything from Gauge.
    // "public" inheritance means Gauge's public members stay public in HBarGauge.
    // (If you wrote `class HBarGauge : Gauge` without "public", it would default
    //  to private inheritance in C++ — a common gotcha coming from C.)

private:
    // Layout constants — pixel offsets within the sprite.
    // These define where each element sits. Adjust to match JPI 830 pixel layout.
    static constexpr int16_t BAR_LEFT_MARGIN    = 2;   // bar starts after numeric value
    static constexpr int16_t BAR_RIGHT_MARGIN   = 2;   // bar ends before label
    static constexpr int16_t BAR_TOP     = 5;    // top of the color bar
    static constexpr int16_t BAR_HEIGHT  = 4;    // height of the color bar
    static constexpr int16_t PTR_SIZE    = 6;    // pointer triangle size
    static constexpr float   TEXT_MID    = 0.40f;

    const uint8_t* _valueFont;
    const uint8_t* _labelFont;
    int _decimals;       // -1 = auto (legacy heuristic), 0+ = explicit

protected:
    void drawGauge() override {
        // --- Draw color range bars ---
        for (uint8_t i = 0; i < _rangeCount; i++) {
            int16_t x0 = mapValue(_ranges[i].min, _minVal, _maxVal, BAR_LEFT_MARGIN, _w - BAR_RIGHT_MARGIN);
            int16_t x1 = mapValue(_ranges[i].max, _minVal, _maxVal, BAR_LEFT_MARGIN, _w - BAR_RIGHT_MARGIN);
            _sprite.fillRect(x0, BAR_TOP, x1 - x0, BAR_HEIGHT, _ranges[i].color);
        }

        // --- Draw pointer triangle (points down at the bar) ---
        float curVal = _valuePtr ? *_valuePtr : 0;
        int16_t px = mapValue(curVal, _minVal, _maxVal, BAR_LEFT_MARGIN, _w - BAR_RIGHT_MARGIN);

        // fillTriangle draws a filled triangle given 3 vertices.
        // We make a downward-pointing triangle sitting above the bar.
        _sprite.fillTriangle(
            px,                     BAR_TOP + BAR_HEIGHT + 4,          // bottom point (tip)
            px - PTR_SIZE/2 - 4,    0,   // top-left
            px + PTR_SIZE/2 + 4,    0,   // top-right
            TFT_BLACK
        );

        _sprite.fillTriangle(
            px,              BAR_TOP + BAR_HEIGHT-2,          // bottom point (tip)
            px - PTR_SIZE/2, 1,   // top-left
            px + PTR_SIZE/2, 1,   // top-right
            TFT_WHITE
        );

        int midPt = (int)(_w * TEXT_MID);
        int textTop = BAR_TOP + BAR_HEIGHT + 1;

        // --- Draw numeric value (left side under meter) ---
        // if (_valueFont) _sprite.loadFont(_valueFont);
        _sprite.setTextColor(TFT_WHITE);
        _sprite.setTextDatum(TR_DATUM);  
        _sprite.loadFont(ArialNB12);
        int dec = (_decimals >= 0) ? _decimals : _getDecimalPlaces(curVal);
        _sprite.drawFloat(curVal, dec, midPt - 2, textTop);

        // --- Draw label (right side) ---
        if (_labelFont) _sprite.loadFont(_labelFont);
        _sprite.setTextColor(TFT_WHITE);
        _sprite.setTextDatum(TL_DATUM);  // middle-left alignment
        _sprite.loadFont(ArialN11);
        _sprite.drawString(_label, midPt + 2, textTop);
    }

private:
    // Decide decimal places based on value magnitude (JPI shows 1 decimal for
    // small values like GPH 16.0, 0 for larger like OAT 18)
    int _getDecimalPlaces(float val) {
        if (val < 100.0f) return 1;
        return 0;
    }

public:
    // Constructor — forwards the lcd pointer to the Gauge base class.
    // C++ note: `Gauge(lcd)` calls the base class constructor explicitly.
    // Without this, C++ would try to call Gauge's default constructor,
    // which doesn't exist (we deleted it by defining a parameterized one).
    HBarGauge(LGFX* lcd)
        : Gauge(lcd)
        , _valueFont(nullptr)
        , _labelFont(nullptr)
        , _decimals(-1)
    {}

    void setValueFont(const uint8_t* font) { _valueFont = font; }
    void setLabelFont(const uint8_t* font) { _labelFont = font; }
    void setDecimals(int d) { _decimals = d; }
};
