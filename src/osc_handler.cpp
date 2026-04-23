#include "osc_handler.h"
#include "patterns.h"
#include <OSCMessage.h>

void initOsc(WiFiUDP& udp, uint16_t port) {
  udp.begin(port);
  Serial.printf("[OSC] Listening on UDP port %d\n", port);
}

// Trigger a pattern based on a mapping, optionally extracting args from the OSC message
static void triggerFromMapping(const OscMapping& mapping, OSCMessage& msg, PatternState& state) {
  int patternId = findPatternByName(mapping.patternName);
  if (patternId < 0) {
    Serial.printf("[OSC] Unknown pattern: %s\n", mapping.patternName);
    return;
  }

  uint32_t color = mapping.color;
  uint8_t brightness = mapping.brightness;
  float speed = mapping.speed;
  unsigned long duration = 0;

  // Check for OSC argument overrides
  int argCount = msg.size();

  if (mapping.useOscColor && argCount >= 3) {
    // Expect 3 int args: R, G, B
    if (msg.isInt(0) && msg.isInt(1) && msg.isInt(2)) {
      uint8_t r = constrain(msg.getInt(0), 0, 255);
      uint8_t g = constrain(msg.getInt(1), 0, 255);
      uint8_t b = constrain(msg.getInt(2), 0, 255);
      color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
  }

  if (mapping.useOscBrightness) {
    // Look for a float arg for brightness (0.0–1.0)
    for (int i = 0; i < argCount; i++) {
      if (msg.isFloat(i)) {
        float val = constrain(msg.getFloat(i), 0.0f, 1.0f);
        brightness = (uint8_t)(val * 255);
        break;
      }
    }
  }

  // For progress pattern, extract float value from first arg
  if (strcmp(mapping.patternName, "progress") == 0 && argCount >= 1 && msg.isFloat(0)) {
    state.externalValue = constrain(msg.getFloat(0), 0.0f, 1.0f);
  }

  triggerPattern(state, patternId, color, brightness, speed, mapping.ringMask, duration);
}

void handleOsc(WiFiUDP& udp, DeviceConfig& cfg, PatternState& state) {
  int packetSize = udp.parsePacket();
  if (packetSize == 0) return;

  OSCMessage msg;
  while (packetSize--) {
    msg.fill(udp.read());
  }

  if (msg.hasError()) {
    Serial.printf("[OSC] Parse error: %d\n", msg.getError());
    return;
  }

  // Built-in: /led/off and /clear both stop the current pattern
  if (msg.fullMatch("/led/off") || msg.fullMatch("/clear")) {
    stopPattern(state, leds, NUM_LEDS);
    return;
  }

  // ============================================================
  // Predefined shortcut endpoints
  // ============================================================
  auto triggerByName = [&](const char* name, uint32_t color, uint8_t mask,
                           unsigned long duration = 0) {
    int id = findPatternByName(name);
    if (id < 0) return;
    triggerPattern(state, id, color, 255, 1.0f, mask, duration);
  };

  if (msg.fullMatch("/go")) {
    triggerByName("solid", 0x00FF00, RING_MASK_INNER);
    return;
  }
  if (msg.fullMatch("/standby")) {
    triggerByName("solid", 0xFF6600, RING_MASK_INNER);
    return;
  }
  if (msg.fullMatch("/ambient")) {
    triggerByName("solid", 0xFFFFFF, RING_MASK_ALL);
    return;
  }
  if (msg.fullMatch("/30stimer")) {
    int id = findPatternByName("countdown_30s");
    if (id >= 0) triggerPattern(state, id, 0xFFFFFF, 255, 1.0f, RING_MASK_ALL, 30000UL);
    return;
  }
  if (msg.fullMatch("/lamptest")) {
    triggerByName("lamptest", 0xFFFFFF, RING_MASK_ALL);
    return;
  }
  if (msg.fullMatch("/rainbow")) {
    triggerByName("rainbow", 0xFFFFFF, RING_MASK_ALL);
    return;
  }

  // Built-in: /led/brightness sets global brightness
  if (msg.fullMatch("/led/brightness")) {
    if (msg.size() >= 1 && msg.isFloat(0)) {
      cfg.globalBrightness = (uint8_t)(constrain(msg.getFloat(0), 0.0f, 1.0f) * 255);
      FastLED.setBrightness(cfg.globalBrightness);
      Serial.printf("[OSC] Global brightness: %d\n", cfg.globalBrightness);
    } else if (msg.size() >= 1 && msg.isInt(0)) {
      cfg.globalBrightness = constrain(msg.getInt(0), 0, 255);
      FastLED.setBrightness(cfg.globalBrightness);
      Serial.printf("[OSC] Global brightness: %d\n", cfg.globalBrightness);
    }
    return;
  }

  // Built-in: /led/pattern directly selects a pattern by name
  if (msg.fullMatch("/led/pattern")) {
    if (msg.size() >= 1 && msg.isString(0)) {
      char patName[MAX_PATTERN_NAME_LEN];
      msg.getString(0, patName, MAX_PATTERN_NAME_LEN);
      int id = findPatternByName(patName);
      if (id >= 0) {
        // Optional second int arg: ringMask (bit 0=outer, 1=middle, 2=inner)
        uint8_t rmask = state.ringMask ? state.ringMask : RING_MASK_ALL;
        if (msg.size() >= 2 && msg.isInt(1)) {
          rmask = (uint8_t)(msg.getInt(1) & 0x07);
        }
        triggerPattern(state, id, state.color, state.brightness, state.speed, rmask);
      } else {
        Serial.printf("[OSC] Unknown pattern: %s\n", patName);
      }
    }
    return;
  }

  // Match against configured mappings
  for (int i = 0; i < cfg.mappingCount; i++) {
    if (!cfg.mappings[i].active) continue;
    if (msg.fullMatch(cfg.mappings[i].oscAddress)) {
      triggerFromMapping(cfg.mappings[i], msg, state);
      return;
    }
  }

  // Log unmatched messages for debugging
  char addr[MAX_OSC_ADDR_LEN];
  msg.getAddress(addr, 0, MAX_OSC_ADDR_LEN);
  Serial.printf("[OSC] Unmatched: %s (%d args)\n", addr, msg.size());
}
