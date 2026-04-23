#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

void resetConfigDefaults(DeviceConfig& cfg) {
  cfg.globalBrightness = DEFAULT_BRIGHTNESS;
  cfg.oscPort = DEFAULT_OSC_PORT;
  strncpy(cfg.deviceName, DEFAULT_DEVICE_NAME, MAX_DEVICE_NAME_LEN);
  cfg.mappingCount = 0;
  cfg.otaEnabled = false;
  for (int i = 0; i < NUM_RINGS; i++) {
    cfg.ringOffset[i] = 0;
    cfg.ringReverse[i] = false;
  }
  cfg.ledPin = DEFAULT_LED_PIN;
  cfg.sacnEnabled = false;
  cfg.sacnUniverse = 1;
  cfg.sacnStartAddr = 1;
  cfg.sacnPriority = 100;
  cfg.sacnMulticast = true;
  memset(cfg.mappings, 0, sizeof(cfg.mappings));
}

void loadConfig(DeviceConfig& cfg) {
  resetConfigDefaults(cfg);

  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) {
    Serial.println("[Config] No config file found, using defaults");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    Serial.printf("[Config] JSON parse error: %s, using defaults\n", err.c_str());
    return;
  }

  cfg.globalBrightness = doc["globalBrightness"] | DEFAULT_BRIGHTNESS;
  cfg.oscPort = doc["oscPort"] | DEFAULT_OSC_PORT;
  strncpy(cfg.deviceName,
          doc["deviceName"] | DEFAULT_DEVICE_NAME,
          MAX_DEVICE_NAME_LEN);
  cfg.otaEnabled = doc["otaEnabled"] | false;
  JsonArray offs = doc["ringOffset"];
  JsonArray revs = doc["ringReverse"];
  for (int i = 0; i < NUM_RINGS; i++) {
    cfg.ringOffset[i] = (i < (int)offs.size()) ? (int8_t)offs[i].as<int>() : 0;
    cfg.ringReverse[i] = (i < (int)revs.size()) ? revs[i].as<bool>() : false;
  }
  cfg.ledPin = doc["ledPin"] | DEFAULT_LED_PIN;
  cfg.sacnEnabled = doc["sacnEnabled"] | false;
  cfg.sacnUniverse = doc["sacnUniverse"] | 1;
  cfg.sacnStartAddr = doc["sacnStartAddr"] | 1;
  cfg.sacnPriority = doc["sacnPriority"] | 100;
  cfg.sacnMulticast = doc["sacnMulticast"] | true;

  JsonArray mappings = doc["mappings"];
  cfg.mappingCount = 0;
  for (JsonObject m : mappings) {
    if (cfg.mappingCount >= MAX_MAPPINGS) break;
    OscMapping& slot = cfg.mappings[cfg.mappingCount];

    strncpy(slot.oscAddress, m["osc"] | "", MAX_OSC_ADDR_LEN);
    strncpy(slot.patternName, m["pattern"] | "solid", MAX_PATTERN_NAME_LEN);

    // Parse color from hex string "RRGGBB"
    const char* colorStr = m["color"] | "FFFFFF";
    slot.color = strtoul(colorStr, NULL, 16);

    slot.brightness = m["brightness"] | 255;
    slot.speed = m["speed"] | 1.0f;
    slot.ringMask = m["ringMask"] | RING_MASK_ALL;
    slot.useOscColor = m["useOscColor"] | false;
    slot.useOscBrightness = m["useOscBrightness"] | false;
    slot.active = true;
    cfg.mappingCount++;
  }

  Serial.printf("[Config] Loaded %d mappings\n", cfg.mappingCount);
}

void saveConfig(const DeviceConfig& cfg) {
  JsonDocument doc;
  doc["globalBrightness"] = cfg.globalBrightness;
  doc["oscPort"] = cfg.oscPort;
  doc["deviceName"] = cfg.deviceName;
  doc["otaEnabled"] = cfg.otaEnabled;
  JsonArray offs = doc["ringOffset"].to<JsonArray>();
  for (int i = 0; i < NUM_RINGS; i++) offs.add((int)cfg.ringOffset[i]);
  JsonArray revs = doc["ringReverse"].to<JsonArray>();
  for (int i = 0; i < NUM_RINGS; i++) revs.add(cfg.ringReverse[i]);
  doc["ledPin"] = cfg.ledPin;
  doc["sacnEnabled"] = cfg.sacnEnabled;
  doc["sacnUniverse"] = cfg.sacnUniverse;
  doc["sacnStartAddr"] = cfg.sacnStartAddr;
  doc["sacnPriority"] = cfg.sacnPriority;
  doc["sacnMulticast"] = cfg.sacnMulticast;

  JsonArray mappings = doc["mappings"].to<JsonArray>();
  for (int i = 0; i < cfg.mappingCount; i++) {
    const OscMapping& slot = cfg.mappings[i];
    if (!slot.active) continue;

    JsonObject m = mappings.add<JsonObject>();
    m["osc"] = slot.oscAddress;
    m["pattern"] = slot.patternName;

    // Store color as hex string
    char colorBuf[7];
    snprintf(colorBuf, sizeof(colorBuf), "%06X", (unsigned int)slot.color);
    m["color"] = colorBuf;

    m["brightness"] = slot.brightness;
    m["speed"] = slot.speed;
    m["ringMask"] = slot.ringMask;
    m["useOscColor"] = slot.useOscColor;
    m["useOscBrightness"] = slot.useOscBrightness;
  }

  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) {
    Serial.println("[Config] Failed to open config file for writing");
    return;
  }

  serializeJsonPretty(doc, file);
  file.close();
  Serial.println("[Config] Saved");
}
