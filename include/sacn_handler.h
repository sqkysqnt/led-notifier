#ifndef SACN_HANDLER_H
#define SACN_HANDLER_H

#include "config.h"

// Start/stop the sACN receiver based on cfg.
// Safe to call multiple times; tears down the previous instance before rebinding.
void initSacn(const DeviceConfig& cfg);
void stopSacn();

// Poll for incoming sACN packets.
// When a packet arrives, writes RGB triples from DMX channels
// (cfg.sacnStartAddr .. start+NUM_LEDS*3-1) into leds[].
// Returns true if a packet was consumed this call (so main loop can skip pattern update).
bool handleSacn(CRGB* leds, const DeviceConfig& cfg);

// Computed multicast address for the current universe, e.g. "239.255.0.1"
String sacnMulticastAddress(uint16_t universe);

// Whether an sACN packet arrived within the last keepaliveMs (default ~2s).
bool sacnIsActive(unsigned long keepaliveMs = 2000);

#endif
