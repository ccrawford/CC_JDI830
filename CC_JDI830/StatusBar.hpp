#pragma once
#include "JPIGauges.hpp"
#include "Fonts/ArialNB12.h"

// ---------------------------------------------------------------------------
// StatusBar — button labels and mode indicator.
//
// In portrait mode this is a horizontal bar across the bottom.
// In landscape mode it becomes a vertical bar on the side with stacked
// characters (one character per line, centered horizontally).
// ---------------------------------------------------------------------------
class StatusBar : public Gauge{
    private:
        static constexpr int BAR_COLOR = TFT_BLUE;

        // Dynamic button labels — updated by setLabels() when display mode changes.
        // These point to string literals (no allocation needed).
        const char* _stepLabel = "STEP";
        const char* _lfLabel   = "LF";

        // Label positions along the bar's primary axis — set from GaugeLayout.
        // Portrait (horizontal): x-positions within the sprite.
        // Landscape (vertical): y-positions for the start of stacked text.
        int16_t _stepLabelPos = 70;
        int16_t _lfLabelPos   = 250;

        // When true, the bar is vertical and text is drawn as stacked characters.
        bool _vertical = false;

        // Center indicator — short text drawn in reverse video (white-on-black
        // rounded rect) between the two button labels.  Used for mode indicators
        // like "NRM" (normalize) and "ROP" (rich of peak).  Empty string = hidden.
        const char* _centerIndicator = "";

        // Draw a label as stacked vertical characters, centered horizontally.
        // Each character is drawn on its own line with lineH pixel spacing.
        void drawStackedText(const char* text, int16_t startY, int16_t lineH) {
            _sprite.setTextDatum(TC_DATUM);
            int16_t cx = _w / 2;
            int16_t y = startY;
            for (const char* p = text; *p; p++) {
                char ch[2] = { *p, '\0' };
                _sprite.drawString(ch, cx, y);
                y += lineH;
            }
        }

    protected:
        void drawGauge() override {
            _sprite.setTextColor(TFT_WHITE);
            _sprite.loadFont(ArialNB12);
            _sprite.fillSprite(BAR_COLOR);

            if (_vertical) {
                drawVertical();
            } else {
                drawHorizontal();
            }
        }

        // Portrait: horizontal bar across the bottom
        void drawHorizontal() {
            _sprite.setTextDatum(MC_DATUM);
            _sprite.drawString(_stepLabel, _stepLabelPos, _h/2);
            _sprite.drawString(_lfLabel, _lfLabelPos, _h/2);

            // Center indicator — reverse video pill
            if (_centerIndicator && _centerIndicator[0]) {
                int16_t cx = _w / 2;
                int16_t cy = _h / 2;
                int textW = _sprite.textWidth(_centerIndicator);
                int padX = 6;
                int padY = 1;
                int rw = textW + padX * 2;
                int rh = _h - 2 + padY * 2;
                int rx = cx - rw / 2;
                int ry = cy - rh / 2;

                _sprite.fillRoundRect(rx, ry, rw, rh, 3, TFT_WHITE);
                _sprite.setTextColor(BAR_COLOR, TFT_WHITE);
                _sprite.drawString(_centerIndicator, cx, cy);
            }
        }

        // Landscape: vertical bar with stacked characters.
        // Each label is drawn one character per line, centered horizontally.
        void drawVertical() {
            int16_t lineH = _sprite.fontHeight()-4;  // spacing from loaded font
            // Treat the _lfLabelPos as the center point.
            int16_t centeredPos = _lfLabelPos - ((strlen(_lfLabel) * lineH) / 2);
            drawStackedText(_lfLabel, centeredPos, lineH);

            centeredPos = _stepLabelPos - ((strlen(_stepLabel) * lineH) / 2);
            drawStackedText(_stepLabel, centeredPos, lineH);


            // Center indicator — stacked between the two labels, in a
            // reverse-video pill that spans the stacked text height.
            if (_centerIndicator && _centerIndicator[0]) {
                int len = strlen(_centerIndicator);
                int16_t midY = (_lfLabelPos + _stepLabelPos) / 2;
                int totalH = len * lineH;
                int16_t startY = midY - totalH / 2;
                int padX = 2;
                int padY = 2;
                int rw = _w - 2;
                int rh = totalH + padY * 2;
                int rx = 1;
                int ry = startY - padY;

                _sprite.fillRoundRect(rx, ry, rw, rh, 3, TFT_WHITE);
                _sprite.setTextColor(BAR_COLOR, TFT_WHITE);
                drawStackedText(_centerIndicator, startY, lineH);
            }
        }

    public:
    StatusBar(LGFX* lcd)
    : Gauge(lcd) {}

    // Update the button labels shown on the status bar.
    // Pass string literals — the pointers are stored, not copied.
    // Triggers a redraw on next update() if either label changed.
    void setLabels(const char* stepLabel, const char* lfLabel) {
        if (_stepLabel != stepLabel || _lfLabel != lfLabel) {
            _stepLabel = stepLabel;
            _lfLabel   = lfLabel;
            forceDirty();
        }
    }

    // Set the label positions along the bar's primary axis.
    // Called from setupGauges() with values from the active GaugeLayout.
    void setLabelPositions(int16_t stepPos, int16_t lfPos) {
        _stepLabelPos = stepPos;
        _lfLabelPos   = lfPos;
    }

    // Set vertical mode — when true, the bar draws stacked characters.
    void setVertical(bool vertical) { _vertical = vertical; }

    // Set or clear the center indicator text.
    // Pass "" or nullptr to hide it.  Pass a string literal like "NRM" or "ROP"
    // to show it in reverse video between the button labels.
    void setCenterIndicator(const char* text) {
        if (!text) text = "";
        if (_centerIndicator != text) {
            _centerIndicator = text;
            forceDirty();
        }
    }
};
