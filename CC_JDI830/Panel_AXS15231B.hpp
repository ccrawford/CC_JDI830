// LovyanGFX panel driver for the AXS15231B controller (320x480, QSPI) on ESP32-S3.
//
// Modeled after Panel_NV3041A — both are QSPI panels on ESP32-S3 that use the same
// LovyanGFX Bus_SPI QSPI path. The init sequence is ported from the Arduino_GFX
// axs15231b_320480_type1_init_operations table.
//
// QSPI pin wiring for JC3248W535EN:
//   SCLK=47, D0=21, D1=48, D2=40, D3=39, CS=45, RST=GFX_NOT_DEFINED, BL=1

#pragma once

#if defined(ESP_PLATFORM)

#include <lgfx/v1/panel/Panel_LCD.hpp>

namespace lgfx
{
    inline namespace v1
    {
        struct Panel_AXS15231B : public Panel_LCD
        {
        protected:
            static constexpr uint8_t CMD_SLPOUT    = 0x11;
            static constexpr uint8_t CMD_DISPON    = 0x29;
            static constexpr uint8_t CMD_DISPOFF   = 0x28;
            static constexpr uint8_t CMD_SLPIN     = 0x10;
            static constexpr uint8_t CMD_INVOFF    = 0x20;
            static constexpr uint8_t CMD_INVON     = 0x21;
            static constexpr uint8_t CMD_CASET     = 0x2A;
            static constexpr uint8_t CMD_RASET     = 0x2B;
            static constexpr uint8_t CMD_RAMWR     = 0x2C;
            static constexpr uint8_t CMD_MADCTL    = 0x36;
            static constexpr uint8_t CMD_COLMOD    = 0x3A;

            static constexpr uint8_t CMD_MADCTL_MY  = 0x80;
            static constexpr uint8_t CMD_MADCTL_MX  = 0x40;
            static constexpr uint8_t CMD_MADCTL_MV  = 0x20;
            static constexpr uint8_t CMD_MADCTL_RGB = 0x00;
            static constexpr uint8_t CMD_MADCTL_BGR = 0x08;

            bool _in_transaction = false;

        public:
            Panel_AXS15231B()
            {
                _cfg.memory_width  = _cfg.panel_width  = 320;
                _cfg.memory_height = _cfg.panel_height = 480;
                _cfg.rgb_order     = false; // AXS15231B is RGB natively
            }

            bool init(bool use_reset) override;

            void beginTransaction() override;
            void endTransaction()   override;

            color_depth_t setColorDepth(color_depth_t depth) override;
            void setInvert(bool invert)    override;
            void setSleep(bool flg)        override;
            void setPowerSave(bool flg)    override;
            void waitDisplay()             override;
            bool displayBusy()             override;

            void setWindow(uint_fast16_t xs, uint_fast16_t ys,
                           uint_fast16_t xe, uint_fast16_t ye) override;
            void drawPixelPreclipped(uint_fast16_t x, uint_fast16_t y,
                                     uint32_t rawcolor) override;
            void writeFillRectPreclipped(uint_fast16_t x, uint_fast16_t y,
                                         uint_fast16_t w, uint_fast16_t h,
                                         uint32_t rawcolor) override;
            void writeBlock(uint32_t rawcolor, uint32_t len) override;
            void writePixels(pixelcopy_t *param, uint32_t len, bool use_dma) override;
            void writeImage(uint_fast16_t x, uint_fast16_t y,
                            uint_fast16_t w, uint_fast16_t h,
                            pixelcopy_t *param, bool use_dma) override;

            uint32_t readCommand(uint_fast16_t cmd, uint_fast8_t index,
                                 uint_fast8_t len) override;
            uint32_t readData(uint_fast8_t index, uint_fast8_t len) override;
            void readRect(uint_fast16_t x, uint_fast16_t y,
                          uint_fast16_t w, uint_fast16_t h,
                          void *dst, pixelcopy_t *param) override;

        protected:
            void update_madctl() override;
            void write_cmd(uint8_t cmd);
            void start_qspi();
            void end_qspi();
            void write_bytes(const uint8_t *data, uint32_t len, bool use_dma);
        };

    } // v1
} // lgfx

#endif // ESP_PLATFORM
