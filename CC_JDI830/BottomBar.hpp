#pragma once
#include "JPIGauges.hpp"

enum class BottomMode : uint8_t
{
    DUAL,   // Two numeric values
    SINGLE, // one value, centered
    MESSAGE // text alert/notification/instruction
};

struct BottomValueDef
{
    const char  *label;
    const float *ptr;
    int         decimals;
    const char  *units;      // optional units string drawn after value (e.g. "GAL", "°F")
    ColorRange  colors[3];
    uint8_t     colorCount;
};

// Look up the display color for a value based on its color ranges.
// Free function so both BottomBar and external draw functions can use it.
inline int colorForDef(const BottomValueDef& def, float val) {
    for (int i = 0; i < def.colorCount; i++) {
        if (val >= def.colors[i].min && val <= def.colors[i].max) {
            int color = def.colors[i].color;
            return color == TFT_GREEN ? TFT_WHITE : color;
        }
    }
    return TFT_WHITE;
}

// Draw function signature — each layout style implements this.
// Parameters:
//   sprite  — the BottomBar's sprite to draw into
//   def     — the value definition (label, pointer, colors, etc.)
//   xLeft   — left edge of the region to draw in
//   xRight  — right edge of the region to draw in
//   h       — height of the sprite
using BottomDrawFn = void(*)(LGFX_Sprite& sprite, const BottomValueDef& def,
                              int16_t xLeft, int16_t xRight, int16_t h);

struct BottomPage {
    BottomMode     mode;
    BottomDrawFn   leftDraw;     // draw function for left (or only) slot
    BottomValueDef left;
    BottomDrawFn   rightDraw;    // draw function for right slot (DUAL only)
    BottomValueDef right;
};

class BottomBar : public Gauge {
    //  Current page data
    BottomPage _page;

    // Message override
    bool        _messageActive;
    const char* _messageText;
    int         _messageColor;
    uint32_t    _messageExpiry;    // millis() timestamp. 0=sticky

    // Message font — the only font BottomBar owns, since draw functions
    // bring their own fonts
    const uint8_t* _messageFont;

    // Second value change tracker (base class only has one _prevValue)
    float _prevRight = -99999.0f;

protected:
    void drawGauge() override {

        if (_page.mode == BottomMode::MESSAGE || isMessageActive())
        {
            drawMessage();
            return;
        }

        if (_page.mode == BottomMode::SINGLE)
        {
            if (_page.leftDraw)
                _page.leftDraw(_sprite, _page.left, 0, _w, _h);
            return;
        }

        if (_page.mode == BottomMode::DUAL)
        {
            if (_page.leftDraw)
                _page.leftDraw(_sprite, _page.left, 0, _w / 2, _h);
            if (_page.rightDraw)
                _page.rightDraw(_sprite, _page.right, _w / 2, _w, _h);
            return;
        }
    }

    void drawMessage() {
        _sprite.fillSprite(TFT_BLACK);
        if (_messageFont) _sprite.loadFont(_messageFont);
        _sprite.setTextColor(_messageColor);
        _sprite.setTextDatum(MC_DATUM);
        _sprite.drawString(_messageText, _w / 2, _h / 2);
    }

public:
    BottomBar(LGFX* lcd)
        : Gauge(lcd)
        , _page{}
        , _messageActive(false), _messageText(""), _messageColor(TFT_RED)
        , _messageExpiry(0)
        , _messageFont(nullptr)
    {}

    void setMessageFont(const uint8_t* font) { _messageFont = font; }

    // --- Page control ---
    void setPage(const BottomPage& page) {
        _page = page;
        _prevValue = -99999.0f;
        _prevRight = -99999.0f;
        _dirty = true;
    }

    // --- Message override ---
    // duration_ms = 0 means sticky until clearMessage()
    void showMessage(const char* text, int color = TFT_RED,
                     uint32_t duration_ms = 0) {
        _messageText   = text;
        _messageColor  = color;
        _messageActive = true;
        _messageExpiry = duration_ms ? (millis() + duration_ms) : 0;
        _dirty = true;
    }

    void clearMessage() {
        _messageActive = false;
        _dirty = true;
    }

    bool isMessageActive() const { return _messageActive; }

    // --- Override update to handle message expiry + dual value tracking ---
    void update() override {
        // Check if timed message has expired
        if (_messageActive && _messageExpiry && millis() > _messageExpiry) {
            _messageActive = false;
            _dirty = true;
        }

        // Track value changes (skip if message is covering everything)
        if (!_messageActive) {
            if (_page.left.ptr) {
                float cur = *_page.left.ptr;
                if (cur != _prevValue) {
                    _prevValue = cur;
                    _dirty = true;
                }
            }
            if (_page.mode == BottomMode::DUAL && _page.right.ptr) {
                float cur = *_page.right.ptr;
                if (cur != _prevRight) {
                    _prevRight = cur;
                    _dirty = true;
                }
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
