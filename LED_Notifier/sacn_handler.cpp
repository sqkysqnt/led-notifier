#include "sacn_handler.h"
#include <ESPAsyncE131.h>

static ESPAsyncE131* e131 = nullptr;
static unsigned long lastPacketMs = 0;
static uint8_t activeSourcePriority = 0;
static unsigned long activeSourceMs = 0;

void stopSacn() {
  if (e131) {
    delete e131;
    e131 = nullptr;
  }
  lastPacketMs = 0;
  activeSourcePriority = 0;
  activeSourceMs = 0;
}

void initSacn(const DeviceConfig& cfg) {
  stopSacn();
  if (!cfg.sacnEnabled) return;

  // Single universe — 47 LEDs * 3 channels = 141 bytes, fits in one.
  e131 = new ESPAsyncE131(1);
  bool ok;
  if (cfg.sacnMulticast) {
    ok = e131->begin(E131_MULTICAST, cfg.sacnUniverse, 1);
  } else {
    ok = e131->begin(E131_UNICAST);  // listens on the default sACN port 5568
  }
  if (ok) {
    Serial.printf("[sACN] Listening: univ=%u start=%u prio=%u mode=%s\n",
                  cfg.sacnUniverse, cfg.sacnStartAddr, cfg.sacnPriority,
                  cfg.sacnMulticast ? "multicast" : "unicast");
  } else {
    Serial.println("[sACN] Failed to start listener");
    stopSacn();
  }
}

String sacnMulticastAddress(uint16_t universe) {
  // Standard sACN multicast: 239.255.<univ_hi>.<univ_lo>
  uint8_t hi = (universe >> 8) & 0xFF;
  uint8_t lo = universe & 0xFF;
  char buf[20];
  snprintf(buf, sizeof(buf), "239.255.%u.%u", hi, lo);
  return String(buf);
}

bool handleSacn(CRGB* leds, const DeviceConfig& cfg) {
  if (!e131 || !cfg.sacnEnabled) return false;
  if (e131->isEmpty()) return false;

  e131_packet_t pkt;
  e131->pull(&pkt);

  // Check universe
  uint16_t univ = htons(pkt.universe);
  if (univ != cfg.sacnUniverse) return false;

  // Priority filtering: highest active source wins.
  // If a higher-priority source goes silent for 2.5s, lower sources can take over.
  uint8_t pktPriority = pkt.priority;
  unsigned long now = millis();
  if (pktPriority < activeSourcePriority && (now - activeSourceMs) < 2500) {
    return false;  // ignore lower-priority source while higher one is active
  }
  activeSourcePriority = pktPriority;
  activeSourceMs = now;
  lastPacketMs = now;

  // property_values[0] is the DMX start-code; actual channels start at [1].
  // startAddr (1..512) is the first channel; channel N is property_values[N].
  uint16_t start = cfg.sacnStartAddr;
  if (start < 1) start = 1;
  for (int i = 0; i < NUM_LEDS; i++) {
    uint16_t ch = start + i * 3;
    if (ch + 2 > 512) { leds[i] = CRGB::Black; continue; }
    leds[i].r = pkt.property_values[ch];
    leds[i].g = pkt.property_values[ch + 1];
    leds[i].b = pkt.property_values[ch + 2];
  }
  return true;
}

bool sacnIsActive(unsigned long keepaliveMs) {
  if (!e131 || lastPacketMs == 0) return false;
  return (millis() - lastPacketMs) < keepaliveMs;
}
