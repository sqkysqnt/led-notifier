#ifndef OSC_HANDLER_H
#define OSC_HANDLER_H

#include <WiFiUdp.h>
#include "config.h"

void initOsc(WiFiUDP& udp, uint16_t port);
void handleOsc(WiFiUDP& udp, DeviceConfig& cfg, PatternState& state);

#endif
