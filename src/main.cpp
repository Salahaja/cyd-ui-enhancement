#include <Arduino.h>
#include <Preferences.h>
#include "LGFX_Config.h"
#include <lvgl.h>
#include "config.h"
#include "ui.h"
#include "meshtastic_handler.h"

LGFX_ESP2432S028_ST7789 tft;

// Sleep Settings
unsigned long lastInteractionTime = 0;
const unsigned long SLEEP_TIMEOUT = 30000; 
bool isAsleep = false;

void wakeUp() {
  tft.setBrightness(255);
  isAsleep = false;
  lastInteractionTime = millis();
}

void goToSleep() {
  tft.setBrightness(0);
  isAsleep = true;
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY);
  if (touched) {
    lastInteractionTime = millis();
    if (isAsleep) { wakeUp(); data->state = LV_INDEV_STATE_REL; return; }
    data->point.x = touchX;
    data->point.y = touchY;
    data->state = LV_INDEV_STATE_PR;
  } else data->state = LV_INDEV_STATE_REL;
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n[SYSTEM] LGFX ST7789 Booting...");

  // Force initialize NVS
  const char* namespaces[] = {"m_log", "ndb", "nfav", "nign", "nxtra"};
  Preferences p;
  for(int i=0; i<5; i++) { p.begin(namespaces[i], false); p.putInt("init", 1); p.end(); }

  tft.init();
  tft.setRotation(3); // Flip screen 180 degrees (was 1)
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);
  
  lv_init();
  static lv_disp_draw_buf_t draw_buf;
  static lv_color_t buf[320 * 10];
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320; disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush; disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();
  meshtastic_init();
  lastInteractionTime = millis();
}

void loop() {
  if(!lvgl_lock) lv_timer_handler();
  meshtastic_loop();
  if (!isAsleep && (millis() - lastInteractionTime > SLEEP_TIMEOUT)) goToSleep();
  delay(5);
}
