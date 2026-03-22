#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9488  _panel_instance;
  lgfx::Bus_SPI        _bus_instance;
  lgfx::Light_PWM      _light_instance;

public:
  LGFX(void)
  {
    // --- SPI Bus ---
    {
      auto cfg = _bus_instance.config();

      // On RP2040/RP2350 with arduino-pico core, you specify the SPI port
      // as a pointer: &SPI for spi0, &SPI1 for spi1.
      // Default Pico SPI0 pins: SCK=18, MOSI=19, MISO=16 (but any valid
      // SPI0 pins work — RP2350 is flexible about pin assignment within a bus).
      cfg.spi_host    = 0;          // 0 = SPI0, 1 = SPI1
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;   // 40MHz; RP2350 SPI tops out ~62.5MHz
      cfg.freq_read   = 16000000;
      cfg.pin_sclk    = 18;         // <<< your SCK pin  (must be valid SPI0 SCK)
      cfg.pin_mosi    = 19;         // <<< your MOSI pin (must be valid SPI0 TX)
      cfg.pin_miso    = 16;         // <<< your MISO pin (must be valid SPI0 RX)
      cfg.pin_dc      = 20;         // <<< your DC/RS pin (any GPIO)

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // --- Panel ---
    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs       = 5;        // <<< your CS pin (any GPIO)
      cfg.pin_rst      = 4;        // <<< your RESET pin (any GPIO)
      cfg.pin_busy     = -1;

      cfg.panel_width  = 320;
      cfg.panel_height = 480;
      cfg.offset_x     = 0;
      cfg.offset_y     = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = false;
      cfg.rgb_order        = false;  // ILI9488 is BGR; toggle if colors look wrong
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;   // set false if display has its own dedicated SPI bus

      _panel_instance.config(cfg);
    }

    // --- Backlight ---
    {
      auto cfg = _light_instance.config();

      cfg.pin_bl      = 22;         // <<< your LED pin, or -1 if hardwired to 3.3V
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 0;          // RP2350 PWM: channels 0-15 (any slice/channel)

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};
