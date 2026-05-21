// Display config for the JC3248W535EN board (AXS15231B, QSPI, 320x480, ESP32-S3).
//
// Strategy: LovyanGFX sprites draw into an Arduino_Canvas PSRAM framebuffer.
// After all gauges update(), CC_JDI830::update() calls _lcd.flush() which
// pushes the framebuffer to the display via Arduino_AXS15231B — the same
// driver proven working in GFXTest.
//
// LovyanGFX's Panel_Device stub intercepts every pixel write and forwards it
// directly into the canvas->getFramebuffer() array. No ESP-IDF SPI calls here.
//
// JC3248W535EN fixed pinout: SCLK=47, D0=21, D1=48, D2=40, D3=39, CS=45, BL=1

#pragma once

#if defined(ESP_PLATFORM)

#include <LovyanGFX.hpp>
#include <Arduino_GFX_Library.h>

// ---------------------------------------------------------------------------
// Arduino_GFX hardware objects — created once, shared between stub and LGFX
// ---------------------------------------------------------------------------
inline Arduino_DataBus* _axs_bus    = nullptr;
inline Arduino_GFX*     _axs_gfx   = nullptr;   // Arduino_AXS15231B
inline Arduino_Canvas*  _axs_canvas = nullptr;   // PSRAM framebuffer

// ---------------------------------------------------------------------------
// Stub LovyanGFX panel — pixel writes go directly into the canvas framebuffer
// ---------------------------------------------------------------------------
namespace lgfx { inline namespace v1 {

class Panel_AXS15231B_Stub : public lgfx::Panel_Device
{
public:
    bool init(bool) override
    {
        // Build the Arduino_GFX stack: QSPI bus → AXS15231B driver → Canvas
        _axs_bus    = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39); // CS,SCK,D0,D1,D2,D3
        _axs_gfx    = new Arduino_AXS15231B(_axs_bus, GFX_NOT_DEFINED, 0, false, 320, 480);
        _axs_canvas = new Arduino_Canvas(320, 480, _axs_gfx, 0, 0, 0);

        if (!_axs_canvas->begin()) {
            Serial.println("AXS stub: Arduino_Canvas::begin() failed");
            return false;
        }
        Serial.println("AXS stub: Arduino_Canvas ready");

        // Backlight
        ::pinMode(1, OUTPUT);
        ::digitalWrite(1, HIGH);

        _axs_canvas->fillScreen(0x0000);
        _axs_canvas->flush();

        _write_depth = lgfx::color_depth_t::rgb565_2Byte;
        _read_depth  = lgfx::color_depth_t::rgb565_2Byte;
        _write_bits  = 16;
        _read_bits   = 16;
        _width  = _cfg.panel_width;   // 320
        _height = _cfg.panel_height;  // 480
        return true;
    }

    void beginTransaction(void) override {}
    void endTransaction(void)   override {}
    lgfx::color_depth_t setColorDepth(lgfx::color_depth_t) override { return lgfx::color_depth_t::rgb565_2Byte; }
    void setInvert(bool)             override {}
    void setRotation(uint_fast8_t r) override
    {
        _rotation = r;
        if (_axs_canvas) _axs_canvas->setRotation(r);
    }
    void setSleep(bool)              override {}
    void setPowerSave(bool)          override {}
    void waitDisplay(void)           override {}
    bool displayBusy(void)           override { return false; }
    void initDMA(void)               override {}
    void waitDMA(void)               override {}
    bool dmaBusy(void)               override { return false; }
    void display(uint_fast16_t, uint_fast16_t, uint_fast16_t, uint_fast16_t) override {}

    void setWindow(uint_fast16_t xs, uint_fast16_t ys,
                   uint_fast16_t xe, uint_fast16_t ye) override
    {
        _win_xs = xs; _win_ys = ys; _win_xe = xe; _win_ye = ye;
    }

    // Main path: pushSprite calls writeImage with the sprite's pixel data.
    // fp_copy converts sprite pixels (palette/raw) to RGB565 in LovyanGFX byte order.
    // Arduino_Canvas::draw16bitRGBBitmap expects the same native RGB565, so no swap needed.
    void writeImage(uint_fast16_t x, uint_fast16_t y,
                    uint_fast16_t w, uint_fast16_t h,
                    lgfx::pixelcopy_t *param, bool) override
    {
        _win_xs = x; _win_ys = y; _win_xe = x + w - 1; _win_ye = y + h - 1;
        if (!_axs_canvas) return;

        uint16_t *row_buf = (uint16_t *)malloc(w * 2);
        if (!row_buf) return;
        for (uint_fast16_t row = 0; row < h; row++) {
            param->fp_copy(row_buf, 0, w, param);
            _axs_canvas->draw16bitRGBBitmap(x, y + row, row_buf, w, 1);
        }
        free(row_buf);
    }

    void writePixels(lgfx::pixelcopy_t *param, uint32_t len, bool) override
    {
        uint_fast16_t w = _win_xe - _win_xs + 1;
        uint_fast16_t h = (w > 0) ? (len / w) : 0;
        if (!h) return;
        writeImage(_win_xs, _win_ys, w, h, param, false);
    }

    void writeFillRectPreclipped(uint_fast16_t x, uint_fast16_t y,
                                 uint_fast16_t w, uint_fast16_t h,
                                 uint32_t rawcolor) override
    {
        if (!_axs_canvas) return;
        // rawcolor is already RGB565 (LovyanGFX internal format)
        _axs_canvas->fillRect(x, y, w, h, (uint16_t)rawcolor);
    }

    void writeBlock(uint32_t rawcolor, uint32_t len) override
    {
        writeFillRectPreclipped(_win_xs, _win_ys,
                                _win_xe - _win_xs + 1,
                                _win_ye - _win_ys + 1,
                                rawcolor);
    }

    void drawPixelPreclipped(uint_fast16_t x, uint_fast16_t y,
                             uint32_t rawcolor) override
    {
        if (_axs_canvas) _axs_canvas->drawPixel(x, y, (uint16_t)rawcolor);
    }

    void writeImageARGB(uint_fast16_t, uint_fast16_t, uint_fast16_t, uint_fast16_t,
                        lgfx::pixelcopy_t*) override {}
    void copyRect(uint_fast16_t, uint_fast16_t, uint_fast16_t, uint_fast16_t,
                  uint_fast16_t, uint_fast16_t) override {}
    uint32_t readCommand(uint_fast16_t, uint_fast8_t = 0, uint_fast8_t = 4) override { return 0; }
    uint32_t readData(uint_fast8_t = 0, uint_fast8_t = 4) override { return 0; }
    void readRect(uint_fast16_t, uint_fast16_t, uint_fast16_t, uint_fast16_t,
                  void*, lgfx::pixelcopy_t*) override {}
    void writeCommand(uint32_t, uint_fast8_t) override {}
    void writeData(uint32_t, uint_fast8_t) override {}

private:
    uint_fast16_t _win_xs = 0, _win_ys = 0, _win_xe = 0, _win_ye = 0;
};

}} // namespace lgfx::v1

// ---------------------------------------------------------------------------
// LGFX class — same public interface as the ILI9488 version.
// After all gauge updates, caller must invoke flush() to push the canvas.
// ---------------------------------------------------------------------------
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_AXS15231B_Stub _panel_instance;

public:
    LGFX()
    {
        auto cfg = _panel_instance.config();
        cfg.panel_width   = 320;
        cfg.panel_height  = 480;
        cfg.memory_width  = 320;
        cfg.memory_height = 480;
        cfg.readable      = false;
        cfg.invert        = false;
        cfg.rgb_order     = false;
        cfg.dlen_16bit    = false;
        cfg.bus_shared    = false;
        _panel_instance.config(cfg);
        setPanel(&_panel_instance);
    }

    // No-op — pins are hardcoded on the JC3248W535EN board
    void configurePins(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {}

    // Push the canvas framebuffer to the physical display.
    // Call once per frame after all gauge update() calls complete.
    void flush()
    {
        if (_axs_canvas) _axs_canvas->flush();
    }
};

#endif // ESP_PLATFORM
