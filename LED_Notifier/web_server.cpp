#include "web_server.h"
#include "web_ui.h"
#include "patterns.h"
#include "network.h"
#include "sacn_handler.h"
#include <ArduinoJson.h>
#include <WiFi.h>

static WebServer* _server = nullptr;

// ============================================================
// CORS headers for convenience (same-network requests)
// ============================================================
static void sendCors() {
  _server->sendHeader("Access-Control-Allow-Origin", "*");
  _server->sendHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
  _server->sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================
// GET / — serve config page
// ============================================================
static void handleRoot() {
  _server->send_P(200, "text/html", INDEX_HTML);
}

// ============================================================
// GET /api/config — return full config as JSON
// ============================================================
static void handleGetConfig() {
  sendCors();
  JsonDocument doc;
  doc["globalBrightness"] = deviceConfig.globalBrightness;
  doc["oscPort"] = deviceConfig.oscPort;
  doc["deviceName"] = deviceConfig.deviceName;
  doc["otaEnabled"] = deviceConfig.otaEnabled;
  JsonArray offs = doc["ringOffset"].to<JsonArray>();
  for (int i = 0; i < NUM_RINGS; i++) offs.add((int)deviceConfig.ringOffset[i]);
  JsonArray revs = doc["ringReverse"].to<JsonArray>();
  for (int i = 0; i < NUM_RINGS; i++) revs.add(deviceConfig.ringReverse[i]);
  doc["ledPin"] = deviceConfig.ledPin;
  doc["sacnEnabled"] = deviceConfig.sacnEnabled;
  doc["sacnUniverse"] = deviceConfig.sacnUniverse;
  doc["sacnStartAddr"] = deviceConfig.sacnStartAddr;
  doc["sacnPriority"] = deviceConfig.sacnPriority;
  doc["sacnMulticast"] = deviceConfig.sacnMulticast;
  doc["sacnMulticastAddr"] = sacnMulticastAddress(deviceConfig.sacnUniverse);

  JsonArray mappings = doc["mappings"].to<JsonArray>();
  for (int i = 0; i < deviceConfig.mappingCount; i++) {
    const OscMapping& slot = deviceConfig.mappings[i];
    if (!slot.active) continue;
    JsonObject m = mappings.add<JsonObject>();
    m["osc"] = slot.oscAddress;
    m["pattern"] = slot.patternName;
    char colorBuf[7];
    snprintf(colorBuf, sizeof(colorBuf), "%06X", (unsigned int)slot.color);
    m["color"] = colorBuf;
    m["brightness"] = slot.brightness;
    m["speed"] = slot.speed;
    m["ringMask"] = slot.ringMask;
    m["useOscColor"] = slot.useOscColor;
    m["useOscBrightness"] = slot.useOscBrightness;
  }

  String json;
  serializeJson(doc, json);
  _server->send(200, "application/json", json);
}

// ============================================================
// PUT /api/config — replace full config
// ============================================================
static void handlePutConfig() {
  sendCors();
  if (!_server->hasArg("plain")) {
    _server->send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, _server->arg("plain"));
  if (err) {
    _server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  bool prevOta = deviceConfig.otaEnabled;
  uint8_t prevLedPin = deviceConfig.ledPin;
  DeviceConfig prevSacn;
  prevSacn.sacnEnabled = deviceConfig.sacnEnabled;
  prevSacn.sacnUniverse = deviceConfig.sacnUniverse;
  prevSacn.sacnStartAddr = deviceConfig.sacnStartAddr;
  prevSacn.sacnPriority = deviceConfig.sacnPriority;
  prevSacn.sacnMulticast = deviceConfig.sacnMulticast;
  deviceConfig.globalBrightness = doc["globalBrightness"] | deviceConfig.globalBrightness;
  deviceConfig.oscPort = doc["oscPort"] | deviceConfig.oscPort;
  strncpy(deviceConfig.deviceName,
          doc["deviceName"] | deviceConfig.deviceName,
          MAX_DEVICE_NAME_LEN);
  deviceConfig.otaEnabled = doc["otaEnabled"] | deviceConfig.otaEnabled;
  if (doc["ringOffset"].is<JsonArray>()) {
    JsonArray offs = doc["ringOffset"];
    for (int i = 0; i < NUM_RINGS && i < (int)offs.size(); i++) {
      deviceConfig.ringOffset[i] = (int8_t)offs[i].as<int>();
    }
  }
  if (doc["ringReverse"].is<JsonArray>()) {
    JsonArray revs = doc["ringReverse"];
    for (int i = 0; i < NUM_RINGS && i < (int)revs.size(); i++) {
      deviceConfig.ringReverse[i] = revs[i].as<bool>();
    }
  }
  deviceConfig.ledPin = doc["ledPin"] | deviceConfig.ledPin;
  deviceConfig.sacnEnabled = doc["sacnEnabled"] | deviceConfig.sacnEnabled;
  deviceConfig.sacnUniverse = doc["sacnUniverse"] | deviceConfig.sacnUniverse;
  deviceConfig.sacnStartAddr = doc["sacnStartAddr"] | deviceConfig.sacnStartAddr;
  deviceConfig.sacnPriority = doc["sacnPriority"] | deviceConfig.sacnPriority;
  deviceConfig.sacnMulticast = doc["sacnMulticast"] | deviceConfig.sacnMulticast;

  bool otaChanged = (prevOta != deviceConfig.otaEnabled);
  bool ledPinChanged = (prevLedPin != deviceConfig.ledPin);
  bool sacnChanged = (prevSacn.sacnEnabled != deviceConfig.sacnEnabled ||
                      prevSacn.sacnUniverse != deviceConfig.sacnUniverse ||
                      prevSacn.sacnStartAddr != deviceConfig.sacnStartAddr ||
                      prevSacn.sacnMulticast != deviceConfig.sacnMulticast);

  FastLED.setBrightness(deviceConfig.globalBrightness);

  // Rebuild mappings
  JsonArray mappings = doc["mappings"];
  deviceConfig.mappingCount = 0;
  for (JsonObject m : mappings) {
    if (deviceConfig.mappingCount >= MAX_MAPPINGS) break;
    OscMapping& slot = deviceConfig.mappings[deviceConfig.mappingCount];
    strncpy(slot.oscAddress, m["osc"] | "", MAX_OSC_ADDR_LEN);
    strncpy(slot.patternName, m["pattern"] | "solid", MAX_PATTERN_NAME_LEN);
    const char* colorStr = m["color"] | "FFFFFF";
    slot.color = strtoul(colorStr, NULL, 16);
    slot.brightness = m["brightness"] | 255;
    slot.speed = m["speed"] | 1.0f;
    slot.ringMask = m["ringMask"] | RING_MASK_ALL;
    slot.useOscColor = m["useOscColor"] | false;
    slot.useOscBrightness = m["useOscBrightness"] | false;
    slot.active = true;
    deviceConfig.mappingCount++;
  }

  saveConfig(deviceConfig);

  // Apply sACN changes immediately (no reboot needed)
  if (sacnChanged) initSacn(deviceConfig);

  bool rebootNeeded = otaChanged || ledPinChanged;
  if (rebootNeeded) {
    _server->send(200, "application/json", "{\"ok\":true,\"rebootRequired\":true}");
  } else {
    _server->send(200, "application/json", "{\"ok\":true}");
  }
}

// ============================================================
// GET /api/config/export — download config as a file
// ============================================================
static void handleExportConfig() {
  sendCors();
  _server->sendHeader("Content-Disposition", "attachment; filename=\"led-notifier-config.json\"");
  JsonDocument doc;
  doc["globalBrightness"] = deviceConfig.globalBrightness;
  doc["oscPort"] = deviceConfig.oscPort;
  doc["deviceName"] = deviceConfig.deviceName;
  doc["otaEnabled"] = deviceConfig.otaEnabled;
  JsonArray offs = doc["ringOffset"].to<JsonArray>();
  for (int i = 0; i < NUM_RINGS; i++) offs.add((int)deviceConfig.ringOffset[i]);
  JsonArray revs = doc["ringReverse"].to<JsonArray>();
  for (int i = 0; i < NUM_RINGS; i++) revs.add(deviceConfig.ringReverse[i]);
  doc["ledPin"] = deviceConfig.ledPin;
  doc["sacnEnabled"] = deviceConfig.sacnEnabled;
  doc["sacnUniverse"] = deviceConfig.sacnUniverse;
  doc["sacnStartAddr"] = deviceConfig.sacnStartAddr;
  doc["sacnPriority"] = deviceConfig.sacnPriority;
  doc["sacnMulticast"] = deviceConfig.sacnMulticast;
  JsonArray mappings = doc["mappings"].to<JsonArray>();
  for (int i = 0; i < deviceConfig.mappingCount; i++) {
    const OscMapping& slot = deviceConfig.mappings[i];
    if (!slot.active) continue;
    JsonObject m = mappings.add<JsonObject>();
    m["osc"] = slot.oscAddress;
    m["pattern"] = slot.patternName;
    char colorBuf[7];
    snprintf(colorBuf, sizeof(colorBuf), "%06X", (unsigned int)slot.color);
    m["color"] = colorBuf;
    m["brightness"] = slot.brightness;
    m["speed"] = slot.speed;
    m["ringMask"] = slot.ringMask;
    m["useOscColor"] = slot.useOscColor;
    m["useOscBrightness"] = slot.useOscBrightness;
  }
  String json;
  serializeJsonPretty(doc, json);
  _server->send(200, "application/json", json);
}

// ============================================================
// GET /api/patterns — list available patterns
// ============================================================
static void handleGetPatterns() {
  sendCors();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < patternCount; i++) {
    JsonObject p = arr.add<JsonObject>();
    p["name"] = patternRegistry[i].name;
    p["displayName"] = patternRegistry[i].displayName;
    p["hasCountdown"] = patternRegistry[i].hasCountdown;
  }
  String json;
  serializeJson(doc, json);
  _server->send(200, "application/json", json);
}

// ============================================================
// POST /api/test — trigger a pattern for preview
// ============================================================
static void handleTest() {
  sendCors();
  if (!_server->hasArg("plain")) {
    _server->send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, _server->arg("plain"));
  if (err) {
    _server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char* patternName = doc["pattern"] | "solid";
  int patternId = findPatternByName(patternName);
  if (patternId < 0) {
    _server->send(404, "application/json", "{\"error\":\"Unknown pattern\"}");
    return;
  }

  const char* colorStr = doc["color"] | "FFFFFF";
  uint32_t color = strtoul(colorStr, NULL, 16);
  uint8_t brightness = doc["brightness"] | 200;
  float speed = doc["speed"] | 1.0f;
  uint8_t ringMask = doc["ringMask"] | RING_MASK_ALL;

  // For progress pattern, accept an initial value
  if (doc["value"].is<float>()) {
    currentPattern.externalValue = constrain((float)doc["value"], 0.0f, 1.0f);
  }

  triggerPattern(currentPattern, patternId, color, brightness, speed, ringMask);
  _server->send(200, "application/json", "{\"ok\":true}");
}

// ============================================================
// POST /api/stop — stop current pattern
// ============================================================
static void handleStop() {
  sendCors();
  stopPattern(currentPattern, leds, NUM_LEDS);
  _server->send(200, "application/json", "{\"ok\":true}");
}

// ============================================================
// GET /api/status — device info
// ============================================================
static void handleStatus() {
  sendCors();
  JsonDocument doc;
  doc["ip"] = currentIP().toString();
  doc["network"] = currentNetworkMode();
  doc["deviceName"] = deviceConfig.deviceName;
  doc["uptime"] = millis();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["rssi"] = (strcmp(currentNetworkMode(), "wifi") == 0) ? WiFi.RSSI() : 0;
  doc["oscPort"] = deviceConfig.oscPort;
  doc["numLeds"] = NUM_LEDS;
  doc["ledPin"] = deviceConfig.ledPin;
  doc["sacnEnabled"] = deviceConfig.sacnEnabled;
  doc["sacnActive"] = sacnIsActive();

  if (currentPattern.running && currentPattern.activePatternId >= 0) {
    doc["activePattern"] = patternRegistry[currentPattern.activePatternId].displayName;
  } else {
    doc["activePattern"] = "None";
  }

  String json;
  serializeJson(doc, json);
  _server->send(200, "application/json", json);
}

// ============================================================
// POST /api/reboot
// ============================================================
static void handleReboot() {
  sendCors();
  _server->send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

// ============================================================
// OPTIONS handler for CORS preflight
// ============================================================
static void handleOptions() {
  sendCors();
  _server->send(204);
}

// ============================================================
// Setup all routes
// ============================================================
void setupWebRoutes(WebServer& server) {
  _server = &server;

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_PUT, handlePutConfig);
  server.on("/api/config/export", HTTP_GET, handleExportConfig);
  server.on("/api/patterns", HTTP_GET, handleGetPatterns);
  server.on("/api/test", HTTP_POST, handleTest);
  server.on("/api/stop", HTTP_POST, handleStop);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/reboot", HTTP_POST, handleReboot);

  // Catch-all for unmatched routes (e.g. /favicon.ico)
  server.onNotFound([]() {
    if (_server->method() == HTTP_OPTIONS) {
      handleOptions();
    } else {
      _server->send(404, "text/plain", "Not found");
    }
  });

  // CORS preflight for all API routes
  server.on("/api/config", HTTP_OPTIONS, handleOptions);
  server.on("/api/patterns", HTTP_OPTIONS, handleOptions);
  server.on("/api/test", HTTP_OPTIONS, handleOptions);
  server.on("/api/stop", HTTP_OPTIONS, handleOptions);
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/reboot", HTTP_OPTIONS, handleOptions);

  Serial.println("[Web] Routes registered");
}
