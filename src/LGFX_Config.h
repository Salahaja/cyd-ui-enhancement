#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX_ESP2432S028_ST7789 : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX_ESP2432S028_ST7789(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = HSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 20000000; // <--- Lowered to 20MHz for stability
      cfg.freq_read  = 16000000;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc   = 2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = 15;
            cfg.pin_rst  = 4; 
            cfg.memory_width  = 240;
            cfg.memory_height = 320;
            cfg.panel_width  = 240; // controller native width
            cfg.panel_height = 320; // controller native height
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0; // Reset offset, use main.cpp rotation
            cfg.readable = true;
            cfg.invert = false; 
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true; // Set to true for LGFX auto-management
            _panel_instance.config(cfg);
        }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 21;
      cfg.invert = false;
      cfg.freq   = 2000;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      auto cfg = _touch_instance.config();
      cfg.spi_host = VSPI_HOST; // Controller 1 for Touch
      cfg.freq  = 1000000;
      cfg.pin_cs   = 33;
      cfg.pin_mosi = 32;
      cfg.pin_miso = 39;
      cfg.pin_sclk = 25;
      cfg.pin_int  = 36;
      cfg.x_min = 300; // Standard X
      cfg.x_max = 3900;
      cfg.y_min = 3700; // Flipped Y
      cfg.y_max = 200;
      cfg.offset_rotation = 2;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};
