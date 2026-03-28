#pragma once
#include "JPIGauges.hpp"
#include "Fonts/ArialN11.h"

// ---------------------------------------------------------------------------
// ArcGauge — circular arc gauge with color bands, needle, and numeric readout.
//
// Used for: RPM, MAP on the JPI 830.
// The arc sweeps from startAngle to endAngle (degrees, 0 = right/3-o'clock,
// clockwise). Color bands are drawn as arc segments. A triangular pointer
// rotates around the arc. The numeric value is displayed in the center.
//
// LovyanGFX angles: 0° = 3 o'clock, increases clockwise.
// The JPI 830 RPM arc starts at about 135° (7 o'clock) and sweeps CW to
// about 45° (1 o'clock) — a 270° sweep.
// ---------------------------------------------------------------------------
class ArcGauge : public Gauge
{
private:
    // Arc geometry
    float _startAngle; // arc start in degrees (where min value sits)
    float _endAngle;   // arc end in degrees (where max value sits)
    bool _inverted;
    int16_t _cx, _cy; // center of the arc within the sprite
    int16_t _outerR;  // outer radius of the color band
    int16_t _innerR;  // inner radius of the color band (band thickness = outer - inner)

    // Needle
    int _needleColor;

    // Redline marker value (the red X on the JPI)

    // Fonts
    const uint8_t *_valueFont;
    const uint8_t *_labelFont;

    // Explicit label y-position within the sprite.  When set to the
    // sentinel value, drawLabel() falls back to computing it from
    // _cy/_innerR/_inverted (the original behavior).
    static constexpr int16_t LABEL_Y_AUTO = -9999;
    int16_t _labelY = LABEL_Y_AUTO;

    // Number of decimal places for value display
    int _decimals;

protected:
    void drawGauge() override
    {
        drawColorBands();
        drawTicks();
        drawNeedle();
        drawRedlineMark();
        drawValue();
        drawLabel();
        drawHPCutouts();
    }

private:

    // The horsepower gets drawn on top of the gap between gauges. To avoid soem flickering, we'll 
    // draw a transparent cutout for it in our sprite. This is a total hack. I mean, we should at 
    // least pass in the areas to block out.
    void drawHPCutouts() {
        _sprite.fillRect(0,0,40,25, TFT_TRANSPARENT);
        _sprite.fillRect(_w - 40, 0, _w, 25, TFT_TRANSPARENT);
    }


    // Convert a gauge value to an angle on the arc.
    float valueToAngle(float val)
    {
        float t = (val - _minVal) / (_maxVal - _minVal);
        t = constrain(t, 0.0f, 1.0f);
        return _startAngle + t * angleSweep();
    }

    // Total arc sweep in degrees. Handles wrapping past 360.
    // positive value: CW, negative CCW
    float angleSweep()
    {
        float sweep = _endAngle - _startAngle;
        if (!_inverted)
        {
            if (sweep < 0)
                sweep += 360.0f;
        }
        else if (sweep > 0)
            sweep -= 360.f;

        return sweep;
    }

    void drawColorBands()
    {
        for (uint8_t i = 0; i < _rangeCount; i++)
        {
            float a0 = valueToAngle(_ranges[i].min);
            float a1 = valueToAngle(_ranges[i].max);

            // Serial.printf("a0: %f  a1: %f\n", a0, a1);
            if (a0 > a1) std::swap(a0, a1);

            // LovyanGFX fillArc(cx, cy, outerR, innerR, startAngle, endAngle, color)
            // Draws a filled arc segment — perfect for gauge color bands.
            _sprite.fillArc(_cx, _cy, _outerR, _innerR, a0, a1, _ranges[i].color);
        }
    }

    void drawTicks()
    {
        // Draw small tick marks around the outside of the arc.
        // We'll draw them as short radial lines at regular intervals.

        float sweep = abs(angleSweep());  // We don't care about direction here.
        int numTicks = 20; // major divisions
        float tickStep = sweep / numTicks;

        for (int i = 0; i <= numTicks; i++)
        {
            float angle = min(_startAngle, _endAngle) + i * tickStep;
            if ((!_inverted && angle >= valueToAngle(_redlineVal)) || (_inverted && angle <= valueToAngle(_redlineVal)))
                continue;
            float rad = angle * DEG_TO_RAD;
            int tickLen = 4 + i % 2 * 4;

            // Tick from just outside the arc band
            int16_t x0 = _cx + cos(rad) * (_innerR);
            int16_t y0 = _cy + sin(rad) * (_innerR);
            int16_t x1 = _cx + cos(rad) * (_innerR - tickLen);
            int16_t y1 = _cy + sin(rad) * (_innerR - tickLen);
            _sprite.drawLine(x0, y0, x1, y1, TFT_WHITE);
        }
    }

    void drawNeedle()
    {

        // Change the needle to a pointer.

        float curVal = _valuePtr ? *_valuePtr : _minVal;
        float angle = valueToAngle(curVal);
        float rad = angle * DEG_TO_RAD;

        // Draw the black outline.

        // Pointer tip
        int16_t tx = _cx + cos(rad) * (_innerR - 3);
        int16_t ty = _cy + sin(rad) * (_innerR - 3);

        // Pointer base
        float needBaseRad = 0.12f;
        int bx0 = _cx + cos(rad + needBaseRad) * (_outerR + 9);
        int by0 = _cy + sin(rad + needBaseRad) * (_outerR + 9);
        int bx1 = _cx + cos(rad - needBaseRad) * (_outerR + 9);
        int by1 = _cy + sin(rad - needBaseRad) * (_outerR + 9);

        _sprite.fillTriangle(tx, ty, bx0, by0, bx1, by1, TFT_BLACK);

        // Draw the actual pointer
        // Pointer tip
        tx = _cx + cos(rad) * _innerR;
        ty = _cy + sin(rad) * _innerR;

        // Needle base
        needBaseRad = 0.06f;
        bx0 = _cx + cos(rad + needBaseRad) * (_outerR + 6);
        bx1 = _cx + cos(rad - needBaseRad) * (_outerR + 6);
        by0 = _cy + sin(rad + needBaseRad) * (_outerR + 6);
        by1 = _cy + sin(rad - needBaseRad) * (_outerR + 6);

        _sprite.fillTriangle(tx, ty, bx0, by0, bx1, by1, _needleColor);

        // Small circle at center pivot
        // _sprite.fillCircle(_cx, _cy, 3, TFT_WHITE);
    }

    void drawRedlineMark()
    {
        if (!_hasRedline)
            return;

        float angle = valueToAngle(_redlineVal);
        float rad = angle * DEG_TO_RAD;

        int16_t x1 = _cx + cos(rad) * (_outerR + 8);
        int16_t x2 = _cx + cos(rad) * (_innerR - 8);
        int16_t y1 = _cy + sin(rad) * (_outerR + 8);
        int16_t y2 = _cy + sin(rad) * (_innerR - 8);

        _sprite.drawLine(x1, y1, x2, y2, TFT_RED);

        // Draw redline value
        _sprite.loadFont(ArialN11);
        _sprite.setTextColor(TFT_RED);
        _sprite.setTextDatum(CL_DATUM);
        x1 = _cx + cos(rad) * (_outerR + 12);
        y1 = _cy + sin(rad) * (_outerR + 12);
        if(_decimals)_sprite.drawFloat(_redlineVal, _decimals, x1, y1);
        else _sprite.drawNumber(_redlineVal, x1, y1);
    }

    void drawValue()
    {
        int yOffset = 0;

        // Large numeric value centered in the arc
        if (_valueFont) _sprite.loadFont(_valueFont);

        if (_inverted) {
            _sprite.setTextDatum(TC_DATUM);
            yOffset = 6;
        } else { 
            _sprite.setTextDatum(BC_DATUM);
        }

        float curVal = _valuePtr ? *_valuePtr : 0;

        int color = colorForValue(curVal);
        // Serial.printf("Val: %f, color: %d\n",curVal, color);
        _sprite.setTextColor(color);

        if (_decimals > 0)
        {
            _sprite.drawFloat(curVal, _decimals, _cx, _cy + yOffset);
        }
        else
        {
            _sprite.drawNumber((int)curVal, _cx, _cy + yOffset);
        }
    }

    void drawLabel()
    {
        // Label text (e.g. "RPM", "MAP") — positioned either by explicit
        // _labelY from the layout, or computed from arc geometry.
        if (_labelFont)
            _sprite.loadFont(_labelFont);
        _sprite.setTextColor(TFT_WHITE);
        _sprite.setTextDatum(TC_DATUM); // top-center
        int16_t ly = (_labelY != LABEL_Y_AUTO)
            ? _labelY
            : _cy + (_inverted ? _innerR - 30 : -_innerR + 10);
        _sprite.drawString(_label, _cx, ly);
    }

public:
    ArcGauge(LGFX *lcd)
        : Gauge(lcd), _startAngle(135.0f) // default: JPI-style 270° sweep
          ,
          _endAngle(45.0f), _inverted(false), _cx(0), _cy(0), _outerR(60), _innerR(55)
          , _needleColor(TFT_WHITE)
          , _valueFont(nullptr), _labelFont(nullptr), _decimals(0)
    {
    }

    // --- Configuration ---
    void setArcGeometry(int16_t cx, int16_t cy, int16_t outerR, int16_t innerR)
    {
        _cx = cx;
        _cy = cy;
        _outerR = outerR;
        _innerR = innerR;
    }
    void setAngles(float startDeg, float endDeg)
    {
        _startAngle = startDeg;
        _endAngle = endDeg;
    }
    void setInverted(bool inverted) { _inverted = inverted; }
    void setNeedle(int color)
    {
        _needleColor = color;
    }

    void setDecimals(int d) { _decimals = d; }
    void setValueFont(const uint8_t *font) { _valueFont = font; }
    void setLabelFont(const uint8_t *font) { _labelFont = font; }
    void setLabelY(int16_t y) { _labelY = y; }
};
