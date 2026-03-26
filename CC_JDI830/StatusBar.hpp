#pragma once
#include "JPIGauges.hpp"
#include "Fonts/ArialNB12.h"

class StatusBar : public Gauge{
    private:
        static constexpr int BAR_COLOR = TFT_BLUE;

        // Dynamic button labels — updated by setLabels() when display mode changes.
        // These point to string literals (no allocation needed).
        const char* _stepLabel = "STEP";
        const char* _lfLabel   = "LF";

        // Center indicator — short text drawn in reverse video (white-on-black
        // rounded rect) between the two button labels.  Used for mode indicators
        // like "NRM" (normalize) and "ROP" (rich of peak).  Empty string = hidden.
        const char* _centerIndicator = "";

    protected:
        void drawGauge() override {
            _sprite.setTextColor(TFT_WHITE);
            _sprite.loadFont(ArialNB12);
            _sprite.setTextDatum(MC_DATUM);
            _sprite.fillSprite(BAR_COLOR);

            _sprite.drawString(_stepLabel, 70, _h/2);
            _sprite.drawString(_lfLabel, 250, _h/2);

            // Center indicator — reverse video (white text on black pill)
            if (_centerIndicator && _centerIndicator[0]) {
                int16_t cx = _w / 2;
                int16_t cy = _h / 2;
                int textW = _sprite.textWidth(_centerIndicator);
                int padX = 6;   // horizontal padding around text
                int padY = 1;   // vertical padding
                int rw = textW + padX * 2;
                int rh = _h - 2 + padY * 2;
                int rx = cx - rw / 2;
                int ry = cy - rh / 2;

                _sprite.fillRoundRect(rx, ry, rw, rh, 3, TFT_WHITE);
                _sprite.setTextColor(BAR_COLOR, TFT_WHITE);
                _sprite.drawString(_centerIndicator, cx, cy);
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