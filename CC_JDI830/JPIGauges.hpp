#pragma once
#include <LovyanGFX.hpp>

// Maximum number of color ranges any gauge can have (green/yellow/red bands etc.)
static constexpr int MAX_COLOR_RANGES = 8;

// A colored band on a gauge scale. For example, green from 0-2500 RPM, red from 2500-2700.
struct ColorRange
{
    float min;
    float max;
    int color; // RGB565 — must be int (not uint32_t) so LovyanGFX template
               // resolves to the RGB565 path. uint32_t triggers RGB888 interpretation.
};

// ---------------------------------------------------------------------------
// Gauge — abstract base class for all JPI-style instrument gauges.
//
// Each gauge owns an LGFX_Sprite (8-bit palette) that it draws into.
// On each frame, call update() — it checks whether the watched value
// has changed and redraws only if needed, then pushes the sprite to
// the display at its assigned (x, y) position.
//
// Subclasses must implement:
//   drawGauge() — render the gauge content into _sprite
// ---------------------------------------------------------------------------
class Gauge
{
protected:
    LGFX_Sprite _sprite; // off-screen buffer for this gauge
    LGFX *_lcd;          // pointer to the display device

    // Position on the physical display where this gauge's sprite is placed
    int16_t _x, _y;
    // Sprite dimensions (set by subclass in init)
    int16_t _w, _h;

    // Scale range
    float _minVal;
    float _maxVal;

    // Pointer into curState — the gauge reads this each frame
    const float *_valuePtr;
    float _prevValue;

    // Label (e.g. "RPM", "EGT", "O-T")
    const char *_label;

    // Color ranges on the scale (green/yellow/red bands)
    ColorRange _ranges[MAX_COLOR_RANGES];
    uint8_t _rangeCount;

    // Redlines
    float _redlineVal;
    bool _hasRedline;

    // Dirty flag — true when the gauge needs to redraw
    bool _dirty;

    // Used for debugging, showBounds will draw a rectangle around the sprite boundary
    bool _showBounds;

    // Subclasses implement this to draw gauge content into _sprite.
    // The sprite is already cleared when this is called.
    virtual void drawGauge() = 0;

public:
    // Constructor — takes a pointer to the display and the value to watch.
    //
    // C++ note: the `: _sprite(lcd)` part is a "member initializer list".
    // LGFX_Sprite needs a pointer to its parent display at construction time
    // so it knows where pushSprite() should go. The initializer list runs
    // BEFORE the constructor body — it's the only way to initialize members
    // that don't have a default constructor.
    Gauge(LGFX *lcd)
        : _sprite(lcd), _lcd(lcd), _x(0), _y(0), _w(0), _h(0), _minVal(0), _maxVal(100), _valuePtr(nullptr), _prevValue(-99999.0f) // force first draw
          ,
          _label(""), _rangeCount(0), _dirty(true), _redlineVal(0), _hasRedline(false), _showBounds(false)

    {
    }

    // Virtual destructor — required when you delete through a base pointer.
    // Without this, only the base class destructor runs, leaking subclass
    // resources. With it, C++ calls the most-derived destructor first, then
    // walks up the chain. The LGFX_Sprite destructor frees the sprite buffer.
    virtual ~Gauge() = default;

    // --- Configuration (call before init) ---

    void setPosition(int16_t x, int16_t y)
    {
        _x = x;
        _y = y;
    }
    void setRange(float minVal, float maxVal)
    {
        _minVal = minVal;
        _maxVal = maxVal;
    }
    void setLabel(const char *label) { _label = label; }
    void setValuePtr(const float *ptr)
    {
        _valuePtr = ptr;
        _dirty = true;
    }

    void addColorRange(float min, float max, int color)
    {
        if (_rangeCount < MAX_COLOR_RANGES)
        {
            _ranges[_rangeCount].min = min;
            _ranges[_rangeCount].max = max;
            _ranges[_rangeCount].color = color;
            _rangeCount++;
        }
    }

    // Reset color ranges so the gauge can be reconfigured at runtime.
    void clearColorRanges() { _rangeCount = 0; }

    // Create the sprite buffer. Call once after setting dimensions.
    // Returns false if memory allocation fails (sprite too large for RAM).
    bool init(int16_t w, int16_t h)
    {
        _w = w;
        _h = h;
        _sprite.setColorDepth(8); // 8-bit palette mode — 1 byte per pixel
        void *buf = _sprite.createSprite(w, h);
        return (buf != nullptr);
    }

    void setRedline(float val)
    {
        _redlineVal = val;
        _hasRedline = true;
    }

    // Force a full redraw on next update (e.g. after screen clear)
    void forceDirty() { _dirty = true; }

    // Called each frame. Checks if value changed, redraws if needed,
    // and pushes sprite to display.
    virtual void update()
    {

        if (_valuePtr)
        {
            float cur = *_valuePtr;
            if (cur != _prevValue)
            {
                _prevValue = cur;
                _dirty = true;
            }
        }

        if (_dirty)
        {
            _sprite.fillSprite(TFT_BLACK); // clear before drawing

            if (_showBounds)
            {
                _sprite.drawRect(0, 0, _w, _h, TFT_PINK);
            }

            drawGauge();
            _sprite.pushSprite(_x, _y);
            _dirty = false;
        }
    }

    // --- Utility for subclasses ---

    // Return the color for the range that `value` falls into.
    // Checks each ColorRange — if min <= value <= max, returns that range's color.
    // If no range matches, returns `defaultColor` (white by default).
    // Green ranges display as white for numeric readouts — "in the green"
    // means normal, so the text stays neutral/white.
    int colorForValue(float value, int defaultColor = TFT_WHITE) const
    {
        for (uint8_t i = 0; i < _rangeCount; i++)
        {
            if (value >= _ranges[i].min && value <= _ranges[i].max)
            {
                int color = _ranges[i].color;
                return color == TFT_GREEN ? TFT_WHITE : color;
            }
        }
        return defaultColor;
    }

    // Map a value to a pixel position within a given pixel range.
    // Like Arduino map() but float-based and clamped.
    static int16_t mapValue(float value, float inMin, float inMax,
                            int16_t outMin, int16_t outMax)
    {
        if (value <= inMin)
            return outMin;
        if (value >= inMax)
            return outMax;
        return outMin + (int16_t)((value - inMin) / (inMax - inMin) * (outMax - outMin));
    }
};
