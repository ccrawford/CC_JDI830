#pragma once
#include "JPIGauges.hpp"

// ---------------------------------------------------------------------------
// NumericRowGauge — a horizontal row of per-cylinder numeric values.
//
// Used for: the EGT value row (1462 1396 ...) and CHT value row (313 312 ...)
// on the JPI 830 display.
//
// Displays N values in a row, evenly spaced. One value can be highlighted
// (different color) to indicate peak or selected cylinder.
//
// Like ColumnBarGauge, this manages multiple value pointers.
// ---------------------------------------------------------------------------
class NumericRowGauge : public Gauge {
private:
    int _numValues;
    const float* _valuePtrs[MAX_CYLINDERS];
    float _prevValues[MAX_CYLINDERS];

    int _offset;

    int _highlightIdx;           // index of highlighted value, -1 for none
    int _normalColor;
    int _highlightColor;
    int _decimals;
    

    const uint8_t* _font;
    int16_t _spacing;            // horizontal spacing between values

protected:
    void drawGauge() override {
        float maxTemp = 0;
        
        if (_font) _sprite.loadFont(_font);
        _sprite.setTextDatum(TC_DATUM);  // top-center per value

        for (int i = 0; i < _numValues; i++) {
            float val = _valuePtrs[i] ? *_valuePtrs[i] : 0;
            maxTemp = max(maxTemp, val);

            _sprite.setTextColor( colorForValue(val) );

            int16_t x = _offset + _spacing / 2 + i * _spacing;

            if (_decimals > 0) {
                _sprite.drawFloat(val, _decimals, x, 0);
            } else {
                _sprite.drawNumber((int)val, x, 0);
            }
        }

        // // draw max value
        // int color = colorForValue(maxTemp);
        // if (color == TFT_RED) _sprite.setTextColor(TFT_RED);
        // else _sprite.setTextColor(TFT_CYAN);
        // _sprite.drawNumber((int)maxTemp, (int)(_offset + _spacing /2 + _numValues * _spacing), 0);
    }

public:
    NumericRowGauge(LGFX* lcd)
        : Gauge(lcd)
        , _numValues(6)
        , _highlightIdx(-1)
        , _normalColor(TFT_WHITE)
        , _highlightColor(TFT_RED)
        , _decimals(0)
        , _font(nullptr)
        , _spacing(40)
        , _offset(20)
    {
        for (int i = 0; i < MAX_CYLINDERS; i++) {
            _valuePtrs[i] = nullptr;
            _prevValues[i] = -99999.0f;
        }
    }

    void setNumValues(int n) { _numValues = (n <= MAX_CYLINDERS) ? n : MAX_CYLINDERS; }
    void setValuePtr(int idx, const float* ptr) {
        if (idx < MAX_CYLINDERS) _valuePtrs[idx] = ptr;
    }
    void setHighlight(int idx) { _highlightIdx = idx; _dirty = true; }
    void setDecimals(int d) { _decimals = d; }
    void setFont(const uint8_t* font) { _font = font; }
    void setSpacing(int16_t s, int16_t offset=0) { _spacing = s; _offset = offset; }

    // Override update to check all values
    void update() override {
        for (int i = 0; i < _numValues; i++) {
            if (_valuePtrs[i] && *_valuePtrs[i] != _prevValues[i]) {
                _prevValues[i] = *_valuePtrs[i];
                _dirty = true;
            }
        }

        if (_dirty) {
            _sprite.fillSprite(TFT_BLACK);
            drawGauge();
            _sprite.pushSprite(_x, _y);
            _dirty = false;
        }
    }
};
