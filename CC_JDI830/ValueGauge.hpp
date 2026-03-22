#pragma once
#include "JPIGauges.hpp"

// ---------------------------------------------------------------------------
// ValueGauge — large numeric readout with a label.
//
// Used for: the EGT/CHT readout at the bottom of the JPI 830 display.
// Shows a label (e.g. "EGT") and a large numeric value (e.g. "1199").
//
// setLayout(gapCenter, side) positions the label and value on either side
// of a central gap point.  'side' indicates which side the LABEL is on:
//   LABEL_LEFT  — label to the left of gapCenter, value to the right
//   LABEL_RIGHT — label to the right of gapCenter, value to the left
// ---------------------------------------------------------------------------

// Which side of the value the label sits on
enum LabelSide : uint8_t {
    LABEL_LEFT,
    LABEL_RIGHT
};

class ValueGauge : public Gauge {
private:
    const uint8_t* _valueFont;
    const uint8_t* _labelFont;
    int _valueColor;
    int _decimals;

    int16_t   _gapCenter;  // x midpoint of the gap between label and value
    LabelSide _labelSide;  // which side the label is drawn on

protected:
    void drawGauge() override {
        const int border = 3;   // half-gap padding on each side of gapCenter

        float curVal = _valuePtr ? *_valuePtr : 0;

        if (_labelSide == LABEL_LEFT) {
            // Label to the left of the gap, value to the right
            _sprite.setTextColor(TFT_WHITE);
            if (_labelFont) _sprite.loadFont(_labelFont);
            _sprite.setTextDatum(MR_DATUM);
            _sprite.drawString(_label, _gapCenter - border, _h / 2);

            if (_valueFont) _sprite.loadFont(_valueFont);
            _sprite.setTextDatum(ML_DATUM);
            _sprite.setTextColor(colorForValue(curVal));
            if (_decimals > 0) {
                _sprite.drawFloat(curVal, _decimals, _gapCenter + border, _h / 2);
            } else {
                _sprite.drawNumber((int)curVal, _gapCenter + border, _h / 2);
            }
        } else {
            // Value to the left of the gap, label to the right
            if (_valueFont) _sprite.loadFont(_valueFont);
            _sprite.setTextDatum(MR_DATUM);
            _sprite.setTextColor(colorForValue(curVal));
            if (_decimals > 0) {
                _sprite.drawFloat(curVal, _decimals, _gapCenter - border, _h / 2);
            } else {
                _sprite.drawNumber((int)curVal, _gapCenter - border, _h / 2);
            }

            _sprite.setTextColor(TFT_WHITE);
            if (_labelFont) _sprite.loadFont(_labelFont);
            _sprite.setTextDatum(ML_DATUM);
            _sprite.drawString(_label, _gapCenter + border, _h / 2);
        }
    }

public:
    ValueGauge(LGFX* lcd)
        : Gauge(lcd)
        , _valueFont(nullptr), _labelFont(nullptr)
        , _valueColor(TFT_WHITE)
        , _decimals(0)
        , _gapCenter(30), _labelSide(LABEL_LEFT)
    {}

    void setValueFont(const uint8_t* font) { _valueFont = font; }
    void setLabelFont(const uint8_t* font) { _labelFont = font; }
    void setValueColor(int color) { _valueColor = color; }
    void setDecimals(int d) { _decimals = d; }

    // gapCenter: x coordinate of the midpoint between label and value
    // side:      LABEL_LEFT or LABEL_RIGHT — which side the label is on
    void setLayout(int16_t gapCenter, LabelSide side = LABEL_LEFT) {
        _gapCenter = gapCenter;
        _labelSide = side;
    }
};
