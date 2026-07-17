#include "meshtastic_handler.h"
#include "ui.h"
#include "config.h"

const size_t PB_BUFSIZE = 512;
const uint8_t MT_MAGIC_1 = 0x94;
const uint8_t MT_MAGIC_2 = 0xC3;

uint8_t packet_buffer[1024];
unsigned long last_node_activity = 0;
uint32_t our_mesh_id = 0; 

void send_mesh_envelope(uint8_t* pb_data, uint16_t pb_len) {
    Serial2.write(MT_MAGIC_1);
    Serial2.write(MT_MAGIC_2);
    Serial2.write((pb_len >> 8) & 0xFF);
    Serial2.write(pb_len & 0xFF);
    Serial2.write(pb_data, pb_len);
}

void meshtastic_init() {
    Serial2.begin(115200, SERIAL_8N1, MESH_RX, MESH_TX);
    meshtastic_ToRadio greeting = meshtastic_ToRadio_init_default;
    greeting.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    greeting.want_config_id = 1;
    uint8_t buf[PB_BUFSIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if (pb_encode(&os, meshtastic_ToRadio_fields, &greeting)) {
        send_mesh_envelope(buf, os.bytes_written);
    }
    last_node_activity = millis();
}

void meshtastic_loop() {
    static uint8_t state = 0;
    static uint16_t payload_len = 0;
    static uint16_t payload_idx = 0;
    static bool was_connected = false;
    static unsigned long last_heartbeat_sent = 0;

    bool is_connected = (millis() - last_node_activity < 30000);
    if(is_connected != was_connected) {
        lvgl_lock = true;
        ui_update_mesh_status(is_connected);
        was_connected = is_connected;
        lvgl_lock = false;
    }

    if (millis() - last_heartbeat_sent > 10000) {
        last_heartbeat_sent = millis();
        meshtastic_ToRadio hb = meshtastic_ToRadio_init_default;
        hb.which_payload_variant = meshtastic_ToRadio_heartbeat_tag;
        uint8_t buf[64];
        pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
        if (pb_encode(&os, meshtastic_ToRadio_fields, &hb)) {
            send_mesh_envelope(buf, os.bytes_written);
        }
    }

    while (Serial2.available()) {
        uint8_t c = Serial2.read();
        switch (state) {
            case 0: if (c == MT_MAGIC_1) state = 1; break;
            case 1: if (c == MT_MAGIC_2) state = 2; else state = 0; break;
            case 2: payload_len = (c << 8); state = 3; break;
            case 3: 
                payload_len |= c; payload_idx = 0;
                if (payload_len > 1024 || payload_len == 0) state = 0; else state = 4;
                break;
            case 4:
                packet_buffer[payload_idx++] = c;
                if (payload_idx >= payload_len) {
                    last_node_activity = millis();
                    lvgl_lock = true; // LOCK UI
                    meshtastic_FromRadio fr = meshtastic_FromRadio_init_default;
                    pb_istream_t is = pb_istream_from_buffer(packet_buffer, payload_len);
                    if (pb_decode(&is, meshtastic_FromRadio_fields, &fr)) {
                        if (fr.which_payload_variant == meshtastic_FromRadio_packet_tag) {
                            if (fr.packet.rx_snr != 0 || fr.packet.rx_rssi != 0) ui_update_node_extra(fr.packet.from, fr.packet.rx_snr, fr.packet.rx_rssi, 0);
                            if (fr.packet.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
                                if (fr.packet.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
                                    char msg[128]; uint16_t len = fr.packet.decoded.payload.size; if (len > 127) len = 127; memcpy(msg, fr.packet.decoded.payload.bytes, len); msg[len] = '\0';
                                    char sender_id[12]; sprintf(sender_id, "!%08x", fr.packet.from);
                                    ui_add_message(sender_id, fr.packet.from, fr.packet.to, msg);
                                    ui_show_alert(sender_id, msg);
                                }
                                else if (fr.packet.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP) {
                                    meshtastic_Telemetry tel = meshtastic_Telemetry_init_default;
                                    pb_istream_t is_t = pb_istream_from_buffer(fr.packet.decoded.payload.bytes, fr.packet.decoded.payload.size);
                                    if (pb_decode(&is_t, meshtastic_Telemetry_fields, &tel)) {
                                        if (tel.which_variant == meshtastic_Telemetry_device_metrics_tag) {
                                            int bat = tel.variant.device_metrics.battery_level;
                                            if (fr.packet.from == our_mesh_id) ui_update_local_bat(bat);
                                            else ui_update_node_extra(fr.packet.from, 0, 0, bat);
                                        }
                                    }
                                }
                            }
                        } 
                        else if (fr.which_payload_variant == meshtastic_FromRadio_my_info_tag) our_mesh_id = fr.my_info.my_node_num;
                        else if (fr.which_payload_variant == meshtastic_FromRadio_node_info_tag) {
                             uint32_t node_num = fr.node_info.num; char d_name[32] = "";
                             if (fr.node_info.has_user) { if (strlen(fr.node_info.user.long_name) > 0) strncpy(d_name, fr.node_info.user.long_name, 31); else if (strlen(fr.node_info.user.short_name) > 0) strncpy(d_name, fr.node_info.user.short_name, 31); }
                             if (strlen(d_name) == 0) sprintf(d_name, "!%08x", node_num);
                             ui_add_node(d_name, node_num, "Active");
                             int bat = fr.node_info.has_device_metrics ? fr.node_info.device_metrics.battery_level : 0;
                             if (node_num == our_mesh_id) ui_update_local_bat(bat);
                             else ui_update_node_extra(node_num, fr.node_info.snr, 0, bat);
                        }
                    }
                    state = 0;
                    lvgl_lock = false; // UNLOCK UI
                }
                break;
        }
    }
}

void meshtastic_send_text(const char* text, uint32_t dest) {
    meshtastic_ToRadio tr = meshtastic_ToRadio_init_default;
    tr.which_payload_variant = meshtastic_ToRadio_packet_tag;
    tr.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    tr.packet.to = dest;
    tr.packet.from = our_mesh_id;
    tr.packet.decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    tr.packet.decoded.payload.size = strlen(text);
    memcpy(tr.packet.decoded.payload.bytes, text, strlen(text));
    uint8_t buf[PB_BUFSIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if (pb_encode(&os, meshtastic_ToRadio_fields, &tr)) {
        send_mesh_envelope(buf, os.bytes_written);
    }
}

void meshtastic_request_metrics(uint32_t dest) {
    meshtastic_ToRadio tr = meshtastic_ToRadio_init_default;
    tr.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    tr.want_config_id = 1;
    uint8_t buf[PB_BUFSIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if (pb_encode(&os, meshtastic_ToRadio_fields, &tr)) {
        send_mesh_envelope(buf, os.bytes_written);
    }
}
