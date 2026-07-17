#include "ui.h"
#include <Preferences.h>
#include "meshtastic_handler.h"
#include "icon_nodes.h" 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_18);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_24);

#define COLOR_BG          lv_color_hex(0x000000)
#define COLOR_SIDEBAR     lv_color_hex(0x111111)
#define COLOR_CARD        lv_color_hex(0x222222)
#define COLOR_ACCENT      lv_color_hex(0x00FF00)
#define COLOR_TEXT        lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_DIM    lv_color_hex(0xBBBBBB)

static lv_obj_t * tabview = NULL;
static lv_obj_t * node_cont = NULL; 
static lv_obj_t * fav_cont = NULL; 
static lv_obj_t * chat_cont = NULL;
static lv_obj_t * input_cont = NULL;
static lv_obj_t * target_label = NULL;
static lv_obj_t * mesh_status_lbl = NULL;
static lv_obj_t * local_bat_lbl = NULL;
static lv_obj_t * disconnect_modal = NULL;
static lv_obj_t * kb = NULL;
lv_obj_t * ta = NULL; 
bool lvgl_lock = false;
uint32_t current_target_id = 0xFFFFFFFF;
extern uint32_t our_mesh_id;

static int current_node_page = 0;
static uint32_t discovered_nodes[100];
static int total_nodes_count = 0;
static bool ui_busy = false; 

static lv_style_t style_sidebar_btn;
static lv_style_t style_card;

// --- Helpers ---

const char* ui_get_node_name(uint32_t node_id) {
    if (node_id == 0 || ui_busy) return NULL;
    static char f_name[32];
    Preferences p; 
    if(p.begin("ndb", true)) {
        char k[12]; sprintf(k, "%x", node_id);
        String found = p.getString(k, ""); p.end();
        if (found.length() > 0) { strncpy(f_name, found.c_str(), 31); f_name[31] = '\0'; return f_name; }
    }
    return NULL;
}

void ui_show_message_no_save(const char* sender_hex, uint32_t from_id, uint32_t to_id, const char* text) {
    if (!chat_cont || !sender_hex || !text || ui_busy) return;
    bool is_g = (to_id == 0xFFFFFFFF);
    bool is_f = (to_id == current_target_id) || (from_id == current_target_id);
    if (current_target_id == 0xFFFFFFFF) { if (!is_g) return; }
    else { if (is_g || !is_f) return; }
    char d_name[32];
    if(strcmp(sender_hex, "Me") == 0) strcpy(d_name, "Me");
    else { const char* n = ui_get_node_name(from_id); if(n) strncpy(d_name, n, 31); else strncpy(d_name, sender_hex, 31); }
    d_name[31] = '\0';
    lv_obj_t * b = lv_obj_create(chat_cont); if(!b) return;
    lv_obj_set_width(b, 210); lv_obj_set_height(b, LV_SIZE_CONTENT); lv_obj_add_style(b, &style_card, 0);
    if(strcmp(d_name, "Me") == 0) { lv_obj_set_align(b, LV_ALIGN_RIGHT_MID); lv_obj_set_style_bg_color(b, lv_color_hex(0x2D4D2D), 0); }
    lv_obj_t * s = lv_label_create(b); lv_label_set_text(s, d_name); lv_obj_set_style_text_color(s, COLOR_ACCENT, 0); lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_t * t = lv_label_create(b); lv_label_set_text(t, text); lv_obj_set_width(t, 190); lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP); lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0); lv_obj_set_style_text_color(t, COLOR_TEXT, 0); lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 14);
    lv_obj_scroll_to_view(b, LV_ANIM_ON);
}

static void ui_refresh_chat() {
    if (!chat_cont || ui_busy) return;
    ui_busy = true; lv_obj_clean(chat_cont);
    Preferences p; p.begin("m_log", true);
    int total = p.getInt("total", 0);
    int start = (total > 10) ? (total - 10) : 0;
    for(int i = start; i < total; i++) {
        char k[16]; sprintf(k, "m%d", i % 10);
        String val = p.getString(k, "");
        if(val.length() > 0) {
            int sep1 = val.indexOf('|'); int sep2 = val.indexOf('|', sep1+1); int sep3 = val.indexOf('|', sep2+1);
            if(sep3 > 0) {
                uint32_t m_f = (uint32_t)strtoul(val.substring(0, sep1).c_str(), NULL, 16);
                uint32_t m_t = (uint32_t)strtoul(val.substring(sep1+1, sep2).c_str(), NULL, 16);
                ui_show_message_no_save(val.substring(sep2+1, sep3).c_str(), m_f, m_t, val.substring(sep3+1).c_str());
            }
        }
    }
    p.end(); ui_busy = false;
}

static void ui_close_keyboard() {
    if(!kb || lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) return;
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    if(tabview) {
        lv_obj_set_height(tabview, 215);
        lv_obj_align(tabview, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }
    if(ta) lv_obj_clear_state(ta, LV_STATE_FOCUSED);
}

// --- Callbacks ---

static void node_action_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e); if(!btn) return;
    uint32_t id = (uint32_t)(intptr_t)lv_obj_get_user_data(btn);
    const char * txt = lv_label_get_text(lv_obj_get_child(btn, 0));
    lv_obj_t * modal = lv_obj_get_parent(btn);
    if (strstr(txt, "CHAT")) {
        current_target_id = id; const char* n = ui_get_node_name(id);
        if(target_label) {
            lv_label_set_text_fmt(target_label, "DIRECT: %s", n ? n : "Unknown");
            lv_obj_set_style_text_color(target_label, COLOR_ACCENT, 0);
        }
        lv_tabview_set_act(tabview, 2, LV_ANIM_ON); ui_refresh_chat();
    } else if (strstr(txt, "STAR")) {
        Preferences p; p.begin("nfav", false); char k[12]; sprintf(k, "%x", id); bool f = p.getBool(k, false); p.putBool(k, !f); p.end(); meshtastic_init();
    } else if (strstr(txt, "HIDE")) {
        Preferences p; p.begin("nign", false); char k[12]; sprintf(k, "%x", id); p.putBool(k, true); p.end(); ui_busy = true; if(node_cont) lv_obj_clean(node_cont); meshtastic_init(); ui_busy = false;
    }
    if(modal) lv_obj_del(modal);
}

static void node_click_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e); lv_obj_t * card = lv_event_get_current_target(e); if(!card) return;
    uint32_t id = (uint32_t)(intptr_t)lv_obj_get_user_data(card);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t * m = lv_obj_create(lv_layer_top()); lv_obj_set_size(m, 220, 180); lv_obj_center(m);
        lv_obj_set_style_bg_color(m, COLOR_SIDEBAR, 0); lv_obj_set_style_border_color(m, COLOR_ACCENT, 0); lv_obj_set_style_border_width(m, 2, 0);
        const char * opts[] = {LV_SYMBOL_ENVELOPE " CHAT", LV_SYMBOL_GPS " STAR", LV_SYMBOL_CLOSE " HIDE", "CANCEL"};
        for(int i=0; i<4; i++) {
            lv_obj_t * b = lv_btn_create(m); lv_obj_set_size(b, 180, 35); lv_obj_align(b, LV_ALIGN_TOP_MID, 0, i * 40);
            lv_obj_set_style_bg_color(b, COLOR_CARD, 0); lv_obj_set_user_data(b, (void*)(intptr_t)id);
            lv_obj_add_event_cb(b, node_action_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_center(lv_label_create(b)); lv_label_set_text(lv_obj_get_child(b, 0), opts[i]);
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        lv_obj_t * m = lv_obj_create(lv_layer_top()); lv_obj_set_size(m, 260, 210); lv_obj_center(m);
        lv_obj_set_user_data(m, (void*)(intptr_t)777);
        lv_obj_set_style_bg_color(m, COLOR_SIDEBAR, 0); lv_obj_set_style_border_color(m, COLOR_ACCENT, 0); lv_obj_set_style_border_width(m, 2, 0);
        const char* n = ui_get_node_name(id);
        lv_obj_t * t = lv_label_create(m); lv_label_set_text(t, n?n:"Unknown Node"); lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0); lv_obj_set_style_text_color(t, COLOR_TEXT, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 10);
        Preferences p; p.begin("nxtra", true); char k[12]; sprintf(k, "%x", id); String ex = p.getString(k, "0|0|0"); p.end();
        float snr = 0; int rssi = 0, bat = 0; int s1 = ex.indexOf('|'); int s2 = ex.indexOf('|', s1+1);
        if(s1 > 0 && s2 > 0) { snr = ex.substring(0, s1).toFloat(); rssi = ex.substring(s1+1, s2).toInt(); bat = ex.substring(s2+1).toInt(); }
        lv_obj_t * il = lv_label_create(m); char ib[160]; snprintf(ib, sizeof(ib), "ID: !%x\n\nSNR:  %d.%d dB\nRSSI: %d dBm\nBAT:  %d%%\n\nStatus: Active", id, (int)snr, (int)(abs(snr-(int)snr)*10), rssi, bat);
        lv_label_set_text(il, ib); lv_obj_set_style_text_line_space(il, 5, 0); lv_obj_set_style_text_color(il, COLOR_TEXT_DIM, 0); lv_obj_align(il, LV_ALIGN_TOP_LEFT, 20, 50);
        lv_obj_t * bk = lv_btn_create(m); lv_obj_set_size(bk, 110, 35); lv_obj_align(bk, LV_ALIGN_BOTTOM_LEFT, 10, -10); lv_obj_set_style_bg_color(bk, COLOR_ACCENT, 0);
        lv_obj_add_event_cb(bk, [](lv_event_t*ev){ lv_obj_del(lv_obj_get_parent(lv_event_get_target(ev))); }, LV_EVENT_CLICKED, NULL);
        lv_obj_center(lv_label_create(bk)); lv_label_set_text(lv_obj_get_child(bk, 0), "CLOSE");
        lv_obj_t * re = lv_btn_create(m); lv_obj_set_size(re, 110, 35); lv_obj_align(re, LV_ALIGN_BOTTOM_RIGHT, -10, -10); lv_obj_set_style_bg_color(re, COLOR_CARD, 0);
        lv_obj_set_user_data(re, (void*)(intptr_t)id); lv_obj_add_event_cb(re, [](lv_event_t*ev){ uint32_t nid = (uint32_t)(intptr_t)lv_obj_get_user_data(lv_event_get_target(ev)); meshtastic_request_metrics(nid); lv_label_set_text(lv_obj_get_child(lv_event_get_target(ev), 0), "SENDING..."); }, LV_EVENT_CLICKED, NULL);
        lv_obj_center(lv_label_create(re)); lv_label_set_text(lv_obj_get_child(re, 0), "REFRESH");
    }
}

static void send_message() {
    if(ui_busy || !ta) return;
    const char * txt = lv_textarea_get_text(ta);
    if(strlen(txt) > 0) {
        meshtastic_send_text(txt, current_target_id); ui_add_message("Me", 0, current_target_id, txt); lv_textarea_set_text(ta, "");
        ui_close_keyboard();
    }
}

static void kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY) send_message();
    else if(code == LV_EVENT_CANCEL) ui_close_keyboard();
}

static void ta_event_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_FOCUSED) { 
        if(!kb || !tabview || !chat_cont) return;
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(tabview, 100); 
        lv_obj_align(tabview, LV_ALIGN_TOP_RIGHT, 0, 25); 
        uint32_t cnt = lv_obj_get_child_cnt(chat_cont); if(cnt > 0) lv_obj_scroll_to_view(lv_obj_get_child(chat_cont, cnt-1), LV_ANIM_ON); 
    }
}

static void page_nav_cb(lv_event_t * e) {
    if(!node_cont) return;
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    current_node_page += dir; if(current_node_page < 0) current_node_page = 0;
    lv_obj_clean(node_cont); meshtastic_init();
}

static void global_reset_cb(lv_event_t * e) { if(ui_busy) return; current_target_id = 0xFFFFFFFF; lv_label_set_text(target_label, "GLOBAL CHANNEL"); lv_obj_set_style_text_color(target_label, COLOR_TEXT_DIM, 0); ui_refresh_chat(); }

static void nav_click_cb(lv_event_t * e) {
    ui_close_keyboard(); 
    if(ui_busy || !tabview) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e); lv_tabview_set_act(tabview, idx, LV_ANIM_ON);
    lv_obj_t * side = lv_obj_get_parent(lv_event_get_target(e)); if(!side) return;
    for(uint32_t i=0; i<lv_obj_get_child_cnt(side); i++) {
        lv_obj_t * b = lv_obj_get_child(side, i); if(!b) continue;
        lv_obj_t * l = lv_obj_get_child(b, 0); if(!l) continue;
        if(i == idx) lv_obj_set_style_text_color(l, COLOR_ACCENT, 0); else lv_obj_set_style_text_color(l, COLOR_TEXT_DIM, 0);
    }
}

void ui_styles_init() {
    lv_style_init(&style_sidebar_btn); lv_style_set_bg_opa(&style_sidebar_btn, 0); lv_style_set_border_width(&style_sidebar_btn, 0); lv_style_set_text_color(&style_sidebar_btn, COLOR_TEXT_DIM);
    lv_style_init(&style_card); lv_style_set_bg_color(&style_card, COLOR_CARD); lv_style_set_bg_opa(&style_card, 255); lv_style_set_radius(&style_card, 4); lv_style_set_border_width(&style_card, 0); lv_style_set_pad_all(&style_card, 8);
}

void create_sidebar() {
    lv_obj_t * side = lv_obj_create(lv_scr_act()); if(!side) return;
    lv_obj_set_size(side, 50, 240); lv_obj_align(side, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(side, COLOR_SIDEBAR, 0); lv_obj_set_style_border_side(side, LV_BORDER_SIDE_RIGHT, 0); lv_obj_set_style_border_width(side, 1, 0); lv_obj_set_style_border_color(side, lv_color_hex(0x1A1A1A), 0); lv_obj_clear_flag(side, LV_OBJ_FLAG_SCROLLABLE); 
    const char * icons[] = {LV_SYMBOL_LIST, LV_SYMBOL_GPS, LV_SYMBOL_ENVELOPE, LV_SYMBOL_SETTINGS};
    for(int i=0; i<4; i++) {
        lv_obj_t * btn = lv_btn_create(side); lv_obj_set_size(btn, 50, 45); lv_obj_add_style(btn, &style_sidebar_btn, 0); lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, i * 50 + 10);
        lv_obj_center(lv_label_create(btn));
        lv_label_set_text(lv_obj_get_child(btn, 0), icons[i]); lv_obj_set_style_text_font(lv_obj_get_child(btn, 0), &lv_font_montserrat_18, 0);
        if(i == 0) lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), COLOR_ACCENT, 0);
        lv_obj_add_event_cb(btn, nav_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

void ui_init() {
    ui_styles_init(); lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BG, 0); lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_t * hdr = lv_obj_create(lv_scr_act()); lv_obj_set_size(hdr, 270, 25); lv_obj_align(hdr, LV_ALIGN_TOP_RIGHT, 0, 0); lv_obj_set_style_bg_color(hdr, COLOR_BG, 0); lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_add_event_cb(hdr, [](lv_event_t*e){ ui_close_keyboard(); }, LV_EVENT_CLICKED, NULL); 
    lv_obj_t * title = lv_label_create(hdr); lv_label_set_text(title, "MESH TERMINAL"); lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0); lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    local_bat_lbl = lv_label_create(hdr); lv_label_set_text(local_bat_lbl, LV_SYMBOL_BATTERY_3 " ---%"); lv_obj_set_style_text_font(local_bat_lbl, &lv_font_montserrat_12, 0); lv_obj_set_style_text_color(local_bat_lbl, COLOR_TEXT_DIM, 0); lv_obj_align(local_bat_lbl, LV_ALIGN_RIGHT_MID, -5, 0);
    create_sidebar(); tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 0); lv_obj_set_size(tabview, 270, 215); lv_obj_align(tabview, LV_ALIGN_BOTTOM_RIGHT, 0, 0); lv_obj_set_style_bg_color(tabview, COLOR_BG, 0); lv_obj_set_style_bg_color(lv_tabview_get_content(tabview), COLOR_BG, 0);
    lv_obj_t * t1 = lv_tabview_add_tab(tabview, "Nodes"); lv_obj_t * t_f = lv_tabview_add_tab(tabview, "Favs"); lv_obj_t * t2 = lv_tabview_add_tab(tabview, "Chat"); lv_obj_t * t3 = lv_tabview_add_tab(tabview, "Settings");
    lv_obj_set_style_bg_color(t1, COLOR_BG, 0); lv_obj_set_style_bg_color(t_f, COLOR_BG, 0); lv_obj_set_style_bg_color(t2, COLOR_BG, 0); lv_obj_set_style_bg_color(t3, COLOR_BG, 0);
    node_cont = lv_obj_create(t1); lv_obj_set_size(node_cont, 270, 175); lv_obj_align(node_cont, LV_ALIGN_TOP_MID, 0, 0); lv_obj_set_style_bg_opa(node_cont, 0, 0); lv_obj_set_style_border_width(node_cont, 0, 0); lv_obj_set_flex_flow(node_cont, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_all(node_cont, 5, 0); lv_obj_set_style_pad_gap(node_cont, 5, 0); lv_obj_set_scrollbar_mode(node_cont, LV_SCROLLBAR_MODE_ON);
    lv_obj_t * p_nav = lv_obj_create(t1); lv_obj_set_size(p_nav, 270, 35); lv_obj_align(p_nav, LV_ALIGN_BOTTOM_MID, 0, 0); lv_obj_set_style_bg_color(p_nav, COLOR_SIDEBAR, 0); lv_obj_set_style_border_width(p_nav, 0, 0); lv_obj_set_flex_flow(p_nav, LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(p_nav, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t * pr = lv_btn_create(p_nav); lv_obj_center(lv_label_create(pr)); lv_label_set_text(lv_obj_get_child(pr, 0), LV_SYMBOL_PREV); lv_obj_add_event_cb(pr, page_nav_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
    lv_obj_center(lv_label_create(p_nav)); lv_label_set_text(lv_obj_get_child(p_nav, 1), "Page 1");
    lv_obj_t * nx = lv_btn_create(p_nav); lv_obj_center(lv_label_create(nx)); lv_label_set_text(lv_obj_get_child(nx, 0), LV_SYMBOL_NEXT); lv_obj_add_event_cb(nx, page_nav_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    fav_cont = lv_obj_create(t_f); lv_obj_set_size(fav_cont, 270, 210); lv_obj_align(fav_cont, LV_ALIGN_TOP_MID, 0, 0); lv_obj_set_style_bg_opa(fav_cont, 0, 0); lv_obj_set_style_border_width(fav_cont, 0, 0); lv_obj_set_flex_flow(fav_cont, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_all(fav_cont, 5, 0); lv_obj_set_style_pad_gap(fav_cont, 5, 0); lv_obj_set_scrollbar_mode(fav_cont, LV_SCROLLBAR_MODE_ON);
    target_label = lv_label_create(t2); lv_label_set_text(target_label, "GLOBAL CHANNEL"); lv_obj_set_style_text_font(target_label, &lv_font_montserrat_14, 0); lv_obj_set_style_text_color(target_label, COLOR_TEXT_DIM, 0); lv_obj_align(target_label, LV_ALIGN_TOP_LEFT, 10, 5); lv_obj_add_flag(target_label, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(target_label, global_reset_cb, LV_EVENT_CLICKED, NULL);
    chat_cont = lv_obj_create(t2); lv_obj_set_size(chat_cont, 270, 135); lv_obj_align(chat_cont, LV_ALIGN_TOP_MID, 0, 25); lv_obj_set_flex_flow(chat_cont, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_bg_opa(chat_cont, 0, 0); lv_obj_set_style_border_width(chat_cont, 0, 0); lv_obj_set_style_pad_gap(chat_cont, 8, 0); lv_obj_set_scrollbar_mode(chat_cont, LV_SCROLLBAR_MODE_ON);
    ta = lv_textarea_create(t2); lv_obj_set_size(ta, 260, 35); lv_obj_align(ta, LV_ALIGN_BOTTOM_MID, 0, -5); lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, NULL); lv_obj_set_style_bg_color(ta, COLOR_CARD, 0); lv_obj_set_style_border_width(ta, 0, 0); lv_obj_set_style_text_color(ta, COLOR_TEXT, 0); lv_textarea_set_placeholder_text(ta, "Message...");
    mesh_status_lbl = lv_label_create(t3); lv_label_set_text(mesh_status_lbl, "Disconnected"); lv_obj_set_style_text_font(mesh_status_lbl, &lv_font_montserrat_20, 0); lv_obj_set_style_text_color(mesh_status_lbl, lv_color_hex(0xFF0000), 0); lv_obj_align(mesh_status_lbl, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_t * r_b = lv_btn_create(t3); lv_obj_set_size(r_b, 140, 45); lv_obj_align(r_b, LV_ALIGN_CENTER, 0, 20); lv_obj_set_style_bg_color(r_b, COLOR_CARD, 0); lv_obj_add_event_cb(r_b, (lv_event_cb_t)[](lv_event_t*e){ meshtastic_init(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_center(lv_label_create(r_b)); lv_label_set_text(lv_obj_get_child(r_b, 0), "RECONNECT");
    kb = lv_keyboard_create(lv_scr_act()); lv_keyboard_set_textarea(kb, ta); lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL); lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    ui_refresh_chat();
}

void ui_update_mesh_status(bool connected) {
    if(!mesh_status_lbl) return;
    if(connected) { lv_label_set_text(mesh_status_lbl, "Connected"); lv_obj_set_style_text_color(mesh_status_lbl, COLOR_ACCENT, 0); if(disconnect_modal) { lv_obj_del(disconnect_modal); disconnect_modal = NULL; } }
    else if(!disconnect_modal) {
        disconnect_modal = lv_obj_create(lv_layer_top()); lv_obj_set_size(disconnect_modal, 320, 240); lv_obj_set_style_bg_color(disconnect_modal, lv_color_black(), 0); lv_obj_set_style_bg_opa(disconnect_modal, 200, 0);
        lv_obj_t * w = lv_label_create(disconnect_modal); lv_label_set_text(w, LV_SYMBOL_WARNING "  LINK LOST"); lv_obj_set_style_text_font(w, &lv_font_montserrat_20, 0); lv_obj_set_style_text_color(w, lv_color_hex(0xFF0000), 0); lv_obj_align(w, LV_ALIGN_CENTER, 0, -30);
        lv_obj_t * b = lv_btn_create(disconnect_modal); lv_obj_set_size(b, 160, 50); lv_obj_align(b, LV_ALIGN_CENTER, 0, 30); lv_obj_set_style_bg_color(b, COLOR_ACCENT, 0);
        lv_obj_add_event_cb(b, (lv_event_cb_t)[](lv_event_t*e){ meshtastic_init(); }, LV_EVENT_CLICKED, NULL);
        lv_obj_center(lv_label_create(b)); lv_label_set_text(lv_obj_get_child(b, 0), "RECONNECT"); lv_obj_set_style_text_color(lv_obj_get_child(b, 0), lv_color_black(), 0);
    }
}

void ui_update_local_bat(int bat) {
    if(!local_bat_lbl) return;
    const char* s = LV_SYMBOL_BATTERY_3; if(bat < 20) s = LV_SYMBOL_BATTERY_EMPTY; else if(bat < 50) s = LV_SYMBOL_BATTERY_1; else if(bat < 80) s = LV_SYMBOL_BATTERY_2;
    lv_label_set_text_fmt(local_bat_lbl, "%s %d%%", s, bat);
}

void ui_update_node_extra(uint32_t node_id, float snr, int rssi, int bat) {
    if(ui_busy) return;
    Preferences p; p.begin("nxtra", false); char k[12]; sprintf(k, "%x", node_id); String ex = p.getString(k, "0|0|0"); float o_snr = 0; int o_rssi = 0, o_bat = 0; int s1 = ex.indexOf('|'); int s2 = ex.indexOf('|', s1+1);
    if(s1 > 0 && s2 > 0) { o_snr = ex.substring(0, s1).toFloat(); o_rssi = ex.substring(s1+1, s2).toInt(); o_bat = ex.substring(s2+1).toInt(); }
    float f_snr = (snr != 0) ? snr : o_snr; int f_rssi = (rssi != 0) ? rssi : o_rssi; int f_bat = (bat != 0) ? bat : o_bat;
    char v[64]; snprintf(v, sizeof(v), "%.1f|%d|%d", f_snr, f_rssi, f_bat); p.putString(k, v); p.end();
    lv_obj_t * top = lv_layer_top();
    for(uint32_t i=0; i<lv_obj_get_child_cnt(top); i++) {
        lv_obj_t * m = lv_obj_get_child(top, i); if(!m) continue;
        if((uint32_t)(intptr_t)lv_obj_get_user_data(m) == 777) {
            lv_obj_t * il = lv_obj_get_child(m, 1); if(!il) continue;
            char lb[160]; snprintf(lb, sizeof(lb), "ID: !%x\n\nSNR:  %d.%d dB\nRSSI: %d dBm\nBAT:  %d%%\n\nStatus: Active", node_id, (int)f_snr, (int)(abs(f_snr-(int)f_snr)*10), f_rssi, f_bat);
            lv_label_set_text(il, lb);
        }
    }
}

static void alert_timer_cb(lv_timer_t * timer) {
    lv_obj_t * alert = (lv_obj_t *)timer->user_data; if(!alert) return;
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, alert); lv_anim_set_values(&a, 10, -60); lv_anim_set_time(&a, 500); lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_ready_cb(&a, [](lv_anim_t * anim){ lv_obj_del((lv_obj_t *)anim->var); }); lv_anim_start(&a);
}

static void alert_click_cb(lv_event_t * e) {
    lv_obj_t * alert = lv_event_get_target(e); if(!alert) return;
    uint32_t node_id = (uint32_t)(intptr_t)lv_obj_get_user_data(alert);
    lv_tabview_set_act(tabview, 2, LV_ANIM_ON); current_target_id = node_id; const char* n = ui_get_node_name(node_id);
    lv_label_set_text_fmt(target_label, "DIRECT: %s", n ? n : "Unknown"); lv_obj_set_style_text_color(target_label, COLOR_ACCENT, 0); ui_refresh_chat(); lv_obj_del(alert);
}

void ui_show_alert(const char* sender_hex, const char* text) {
    if(lv_tabview_get_tab_act(tabview) == 2 || ui_busy) return;
    uint32_t id = 0; if (sender_hex[0] == '!') id = (uint32_t)strtoul(sender_hex + 1, NULL, 16);
    const char* name = ui_get_node_name(id);
    lv_obj_t * alert = lv_obj_create(lv_layer_top()); if(!alert) return;
    lv_obj_set_size(alert, 240, 55); lv_obj_align(alert, LV_ALIGN_TOP_RIGHT, -10, -70); 
    lv_obj_set_style_bg_color(alert, COLOR_CARD, 0); lv_obj_set_style_border_color(alert, COLOR_ACCENT, 0); lv_obj_set_style_border_width(alert, 1, 0); lv_obj_set_style_radius(alert, 8, 0); lv_obj_set_style_pad_all(alert, 8, 0); 
    lv_obj_add_flag(alert, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_user_data(alert, (void*)(intptr_t)id); lv_obj_add_event_cb(alert, alert_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * s = lv_label_create(alert); lv_label_set_text_fmt(s, LV_SYMBOL_BELL "  %s", name?name:sender_hex); lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0); lv_obj_set_style_text_color(s, COLOR_ACCENT, 0); lv_obj_align(s, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t * b = lv_label_create(alert); lv_label_set_text(b, text); lv_obj_set_width(b, 220); lv_label_set_long_mode(b, LV_LABEL_LONG_DOT); lv_obj_set_style_text_color(b, COLOR_TEXT, 0); lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, alert); lv_anim_set_values(&a, -70, 10); lv_anim_set_time(&a, 500); lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y); lv_anim_start(&a); lv_timer_create(alert_timer_cb, 3500, alert);
}

void ui_add_node(const char* name, uint32_t node_id, const char* last_seen) {
    if (!node_cont || !fav_cont || ui_busy) return;
    if (node_id == our_mesh_id && our_mesh_id != 0) return;
    bool k = false; for(int i=0; i<total_nodes_count; i++) { if(discovered_nodes[i] == node_id) { k = true; break; } }
    if(!k && total_nodes_count < 100) discovered_nodes[total_nodes_count++] = node_id;
    Preferences pi; pi.begin("nign", false); char ik[12]; sprintf(ik, "%x", node_id); bool ign = pi.getBool(ik, false); pi.end(); if(ign) return;
    Preferences pf; pf.begin("nfav", false); char fk[12]; sprintf(fk, "%x", node_id); bool fav = pf.getBool(fk, false); pf.end();
    if (node_id != 0 && name[0] != '!') { Preferences pd; pd.begin("ndb", false); char dk[12]; sprintf(dk, "%x", node_id); pd.putString(dk, name); pd.end(); }
    auto make_c = [&](lv_obj_t* par) {
        lv_obj_t * c = lv_obj_create(par); if(!c) return;
        lv_obj_set_size(c, 240, 45); lv_obj_add_style(c, &style_card, 0); lv_obj_set_user_data(c, (void*)(intptr_t)node_id); lv_obj_add_event_cb(c, node_click_cb, LV_EVENT_ALL, NULL);
        lv_obj_t * b = lv_obj_create(c); lv_obj_set_size(b, 3, 25); lv_obj_align(b, LV_ALIGN_LEFT_MID, -5, 0); lv_obj_set_style_bg_color(b, COLOR_ACCENT, 0); lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_t * nl = lv_label_create(c); lv_label_set_text(nl, name); lv_obj_set_style_text_font(nl, &lv_font_montserrat_14, 0); lv_obj_set_style_text_color(nl, COLOR_TEXT, 0); lv_obj_align(nl, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t * i = lv_label_create(c); lv_label_set_text(i, last_seen); lv_obj_set_style_text_font(i, &lv_font_montserrat_14, 0); lv_obj_set_style_text_color(i, COLOR_ACCENT, 0); lv_obj_align(i, LV_ALIGN_RIGHT_MID, 0, 0);
        if(fav) { lv_obj_set_style_border_side(c, LV_BORDER_SIDE_RIGHT, 0); lv_obj_set_style_border_width(c, 3, 0); lv_obj_set_style_border_color(c, COLOR_ACCENT, 0); }
    };
    if(fav) {
        lv_obj_t * fc = NULL; for(uint32_t i=0; i<lv_obj_get_child_cnt(fav_cont); i++) { lv_obj_t * ch = lv_obj_get_child(fav_cont, i); if(ch && lv_obj_get_user_data(ch) == (void*)(intptr_t)node_id) { fc = ch; break; } }
        if(fc && lv_obj_get_child_cnt(fc) > 3) lv_label_set_text(lv_obj_get_child(fc, 3), last_seen); else make_c(fav_cont);
    }
    int my_i = -1; for(int i=0; i<total_nodes_count; i++) if(discovered_nodes[i] == node_id) my_i = i;
    int st = current_node_page * 10;
    if(my_i >= st && my_i < st + 10) {
        lv_obj_t * mc = NULL; for(uint32_t i=0; i<lv_obj_get_child_cnt(node_cont); i++) { lv_obj_t * ch = lv_obj_get_child(node_cont, i); if(ch && lv_obj_get_user_data(ch) == (void*)(intptr_t)node_id) { mc = ch; break; } }
        if(mc && lv_obj_get_child_cnt(mc) > 3) lv_label_set_text(lv_obj_get_child(mc, 3), last_seen); else make_c(node_cont);
    }
}

void ui_add_message(const char* sender_hex, uint32_t from_id, uint32_t to_id, const char* text) {
    if (!chat_cont || ui_busy) return; if (lv_obj_get_child_cnt(chat_cont) > 10) lv_obj_del(lv_obj_get_child(chat_cont, 0));
    static int msg_count = 0; static bool init_prefs = false;
    Preferences p; p.begin("m_log", false); if(!init_prefs) { msg_count = p.getInt("total", 0); init_prefs = true; }
    char k[16], v[200]; sprintf(k, "m%d", msg_count % 10); snprintf(v, sizeof(v), "%x|%x|%s|%s", from_id, to_id, sender_hex, text); p.putString(k, v);
    msg_count++; p.putInt("total", msg_count); p.end();
    ui_show_message_no_save(sender_hex, from_id, to_id, text);
}
