#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9488  _panel_instance;
  lgfx::Bus_SPI        _bus_instance;
  lgfx::Light_PWM      _light_instance;

public:
  // Default constructor — sets up non-pin config (SPI speeds, panel geometry, etc.)
  // Call configurePins() before init() to apply the actual pin assignments from MobiFlight.
  LGFX(void)
  {
    // --- SPI Bus (non-pin defaults) ---
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host    = 0;          // 0 = SPI0, 1 = SPI1
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;   // 40MHz; RP2350 SPI tops out ~62.5MHz
      cfg.freq_read   = 16000000;
      cfg.pin_sclk    = -1;
      cfg.pin_mosi    = -1;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = -1;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // --- Panel (non-pin defaults) ---
    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs       = -1;
      cfg.pin_rst      = -1;
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
      cfg.bus_shared       = true;

      _panel_instance.config(cfg);
    }

    // --- Backlight (non-pin defaults) ---
    {
      auto cfg = _light_instance.config();

      cfg.pin_bl      = -1;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 0;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }

  // Apply pin assignments from MobiFlight config. Call this before init().
  void configurePins(uint8_t sclk, uint8_t mosi, uint8_t dc,
                     uint8_t cs, uint8_t rst, uint8_t bl)
  {
    // Update SPI bus pins
    {
      auto cfg = _bus_instance.config();
      cfg.pin_sclk = sclk;
      cfg.pin_mosi = mosi;
      cfg.pin_miso = -1;       // not used (write-only display)
      cfg.pin_dc   = dc;
      _bus_instance.config(cfg);
    }

    // Update panel pins
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs  = cs;
      cfg.pin_rst = rst;
      _panel_instance.config(cfg);
    }

    // Update backlight pin
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = bl;
      _light_instance.config(cfg);
    }
  }
};
