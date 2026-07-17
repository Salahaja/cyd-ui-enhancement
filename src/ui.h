#ifndef UI_H
#define UI_H
#include <lvgl.h>
#include <stdint.h>

void ui_init();
void ui_add_node(const char* name, uint32_t node_id, const char* last_seen);
void ui_update_node_extra(uint32_t node_id, float snr, int rssi, int bat);
void ui_add_message(const char* sender_hex, uint32_t from_id, uint32_t to_id, const char* text);
void ui_show_message_no_save(const char* sender_hex, uint32_t from_id, uint32_t to_id, const char* text);
void ui_update_mesh_status(bool connected);
void ui_update_local_bat(int bat);
void ui_show_alert(const char* sender_hex, const char* text);

extern lv_obj_t * ta;
extern bool lvgl_lock;
#endif
