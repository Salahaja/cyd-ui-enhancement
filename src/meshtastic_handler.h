#ifndef MESHTASTIC_HANDLER_H
#define MESHTASTIC_HANDLER_H

#include <Arduino.h>
#include <stdint.h>
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include "mesh.pb.h"

void meshtastic_init();
void meshtastic_loop();
void meshtastic_send_text(const char* text, uint32_t dest = 0xFFFFFFFF);
void meshtastic_request_metrics(uint32_t dest);
extern uint32_t our_mesh_id;

#endif
