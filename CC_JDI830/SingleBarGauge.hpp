#pragma once
#include "JPIGauges.hpp"

// ---------------------------------------------------------------------------
// SingleBarGauge — a single segmented vertical bar with a numeric value
// displayed above it, a label below, and a scale on the right side.
//
// Used for: TIT (turbine inlet temperature) or any standalone vertical bar.
// Uses the base class _valuePtr, _minVal, _maxVal, and color ranges.
//
// Layout (left to right):
//   [bar] [gap] [scale labels]
//
// The bar is drawn as stacked blocks (matching the JPI 830 segmented look).
// A numeric readout of the current value is drawn above the bar.
// A label (e.g. "T") is drawn below the bar.
// The scale shows the min value at the bottom and the redline value (in red).
// ---------------------------------------------------------------------------
class SingleBarGauge : public Gauge {
private:
    int16_t _barWidth;
    int16_t _barHeight;       // height of each discrete block

    static constexpr int16_t BLOCK_GAP  = 2;
    static constexpr int16_t SCALE_GAP  = 3;  // gap between bar and scale labels

    // Chart area within the sprite
    int16_t _chartLeft;       // left margin so bar/redline aren't clipped
    int16_t _chartTop;
    int16_t _chartBottom;

    const uint8_t* _valueFont;
    const uint8_t* _labelFont;
    const uint8_t* _scaleFont;

    int _barColor;

protected:
    void drawGauge() override {
        float val = _valuePtr ? *_valuePtr : 0;

        // Bar center X (offset by _chartLeft so nothing is clipped at left edge)
        int16_t barX = _chartLeft;
        int16_t barCenterX = barX + _barWidth / 2;

        // --- Numeric value at top (aligned with ColumnBarGauge's EGT row) ---
        if (_valueFont) _sprite.loadFont(_valueFont);
        _sprite.setTextDatum(BC_DATUM);
        _sprite.setTextColor(colorForValue(val));
        int16_t fontH = _sprite.fontHeight();
        _sprite.drawNumber((int)val, barCenterX, _chartTop - 2 - fontH);

        // --- Dashed center line (drawn first, underneath the bar segments) ---
        for (int16_t y = _chartTop; y <= _chartBottom; y += 6) {
            _sprite.drawPixel(barCenterX, y, TFT_LIGHTGRAY);
            _sprite.drawPixel(barCenterX, y + 1, TFT_LIGHTGRAY);
        }

        // --- Segmented bar (drawn on top of dotted line) ---
        int barColor = colorForValue(val, _barColor);
        int16_t barTop = mapValue(val, _minVal, _maxVal, _chartBottom, _chartTop);

        for (int16_t y = _chartBottom; y >= barTop; y -= (_barHeight + BLOCK_GAP)) {
            int16_t blockH = _barHeight;
            if (y - blockH < barTop) blockH = y - barTop;
            if (blockH <= 0) break;
            _sprite.fillRect(barX, y - blockH, _barWidth, blockH, barColor);
        }

        // --- Redline marker ---
        if (_hasRedline) {
            int16_t ry = mapValue(_redlineVal, _minVal, _maxVal, _chartBottom, _chartTop);
            _sprite.drawFastHLine(barX - 2, ry, _barWidth + 4, TFT_RED);
        }

        // --- Scale labels on right side ---
        if (_scaleFont) _sprite.loadFont(_scaleFont);
        int16_t scaleX = barX + _barWidth + SCALE_GAP;
        _sprite.setTextDatum(ML_DATUM);

        // Min value at bottom
        _sprite.setTextColor(TFT_WHITE);
        _sprite.drawNumber((int)_minVal, scaleX, _chartBottom);

        // Redline value
        if (_hasRedline) {
            int16_t ry = mapValue(_redlineVal, _minVal, _maxVal, _chartBottom, _chartTop);
            _sprite.setTextColor(TFT_RED);
            _sprite.drawNumber((int)_redlineVal, scaleX, ry);
        }

        // --- Label below the bar ---
        if (_labelFont) _sprite.loadFont(_labelFont);
        _sprite.setTextDatum(TC_DATUM);
        _sprite.setTextColor(TFT_WHITE);
        _sprite.drawString(_label, barCenterX, _chartBottom + 2);
    }

public:
    SingleBarGauge(LGFX* lcd)
        : Gauge(lcd)
        , _barWidth(14)
        , _barHeight(4)
        , _chartLeft(0)
        , _chartTop(16)
        , _chartBottom(0)
        , _valueFont(nullptr)
        , _labelFont(nullptr)
        , _scaleFont(nullptr)
        , _barColor(TFT_CYAN)
    {}

    void setBarSize(int16_t width, int16_t height) {
        _barWidth = width;
        _barHeight = height;
    }

    // Set the pixel layout for the bar within the sprite.
    // left = left margin (prevents clipping), top = Y for max value, bottom = Y for min value.
    void setChartArea(int16_t left, int16_t top, int16_t bottom) {
        _chartLeft = left;
        _chartTop = top;
        _chartBottom = bottom;
    }

    void setValueFont(const uint8_t* font) { _valueFont = font; }
    void setLabelFont(const uint8_t* font) { _labelFont = font; }
    void setScaleFont(const uint8_t* font) { _scaleFont = font; }
    void setBarColor(int color) { _barColor = color; }
};
