// LED Notifier — ESP32 + WS2812B concentric rings
// Supports generic ESP-WROOM-32 (WiFi) and WT32-ETH01 (Ethernet + WiFi fallback).
// Features: OSC, sACN/E1.31, Web UI, configurable LED pin, OTA.
//
// Build with PlatformIO:
//   pio run -e esp32dev   -t upload
//   pio run -e wt32-eth01 -t upload

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <FastLED.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "patterns.h"
#include "osc_handler.h"
#include "web_server.h"
#include "network.h"
#include "sacn_handler.h"

// ============================================================
// Globals
// ============================================================
CRGB leds[NUM_LEDS];
DeviceConfig deviceConfig;
PatternState currentPattern;
WiFiUDP oscUdp;
WebServer webServer(80);

// ============================================================
// FastLED runtime-pin dispatch
// FastLED requires the data pin as a template parameter, so we
// register one explicit instantiation per allowed GPIO.
// ============================================================
template <uint8_t PIN>
static inline void addWS2812BPin() {
  FastLED.addLeds<WS2812B, PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
}

static void initLedsOnPin(uint8_t pin) {
  switch (pin) {
    case  2: addWS2812BPin< 2>(); break;
    case  4: addWS2812BPin< 4>(); break;
    case  5: addWS2812BPin< 5>(); break;
    case 12: addWS2812BPin<12>(); break;
    case 13: addWS2812BPin<13>(); break;
    case 14: addWS2812BPin<14>(); break;
    case 15: addWS2812BPin<15>(); break;
    case 16: addWS2812BPin<16>(); break;
    case 17: addWS2812BPin<17>(); break;
    case 18: addWS2812BPin<18>(); break;
    case 19: addWS2812BPin<19>(); break;
    case 21: addWS2812BPin<21>(); break;
    case 22: addWS2812BPin<22>(); break;
    case 23: addWS2812BPin<23>(); break;
    case 25: addWS2812BPin<25>(); break;
    case 26: addWS2812BPin<26>(); break;
    case 27: addWS2812BPin<27>(); break;
    case 32: addWS2812BPin<32>(); break;
    case 33: addWS2812BPin<33>(); break;
    default: addWS2812BPin<DEFAULT_LED_PIN>(); break;
  }
}

// ============================================================
// Startup animation — quick color wipe to show board is alive
// ============================================================
static void startupAnimation() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 40, 80);
    FastLED.show();
    delay(25);
  }
  delay(200);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

static void networkConnectedAnimation() {
  for (int j = 0; j < 3; j++) {
    fill_solid(leds, NUM_LEDS, CRGB(0, 80, 0));
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
  }
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== LED Notifier ===");

  // BOOT button — hold during startup to reset WiFi credentials
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Initialize filesystem and load config (need ledPin from config before LED init)
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed!");
  }
  loadConfig(deviceConfig);

  // Initialize LEDs on the configured pin
  Serial.printf("[LED] Using GPIO%u for %d LEDs\n", deviceConfig.ledPin, NUM_LEDS);
  initLedsOnPin(deviceConfig.ledPin);
  FastLED.setBrightness(50);
  startupAnimation();
  FastLED.setBrightness(deviceConfig.globalBrightness);

  // Initialize pattern state
  initPatternState(currentPattern);

  // Check if BOOT button is held — force WiFi portal.
  bool forcePortal = false;
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    Serial.println("[Net] BOOT button detected — hold 2s to reset WiFi...");
    fill_solid(leds, NUM_LEDS, CRGB(80, 0, 80));
    FastLED.show();
    unsigned long holdStart = millis();
    forcePortal = true;
    while (millis() - holdStart < 2000) {
      if (digitalRead(BOOT_BUTTON_PIN) != LOW) { forcePortal = false; break; }
      delay(50);
    }
  }
  if (!forcePortal) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  }

  // Bring up network (Ethernet first on WT32-ETH01, then WiFi fallback)
  String apName = String("LED-Notifier-") + String((uint32_t)ESP.getEfuseMac() & 0xFFFF, HEX);
  if (!initNetwork(apName.c_str(), forcePortal)) {
    Serial.println("[Net] No network — restarting...");
    fill_solid(leds, NUM_LEDS, CRGB(80, 0, 0));
    FastLED.show();
    delay(2000);
    ESP.restart();
  }
  Serial.printf("[Net] Mode=%s IP=%s\n", currentNetworkMode(), currentIP().toString().c_str());
  networkConnectedAnimation();

  // Start mDNS — include MAC suffix for uniqueness across devices
  char mdnsName[64];
  uint32_t macSuffix = (uint32_t)ESP.getEfuseMac() & 0xFFFF;
  snprintf(mdnsName, sizeof(mdnsName), "%s-%04x", deviceConfig.deviceName, macSuffix);
  if (MDNS.begin(mdnsName)) {
    Serial.printf("[mDNS] http://%s.local\n", mdnsName);
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("osc", "udp", deviceConfig.oscPort);
  }

  // Start OTA (if enabled in config)
  if (deviceConfig.otaEnabled) {
    ArduinoOTA.setHostname(mdnsName);
    ArduinoOTA.onStart([]() {
      Serial.println("[OTA] Update starting");
      fill_solid(leds, NUM_LEDS, CRGB(0, 0, 40));
      FastLED.show();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      int lit = (progress * NUM_LEDS) / total;
      for (int i = 0; i < NUM_LEDS; i++) leds[i] = (i < lit) ? CRGB(0, 40, 80) : CRGB::Black;
      FastLED.show();
    });
    ArduinoOTA.onEnd([]() {
      fill_solid(leds, NUM_LEDS, CRGB(0, 80, 0));
      FastLED.show();
      Serial.println("[OTA] Update complete");
    });
    ArduinoOTA.onError([](ota_error_t err) {
      Serial.printf("[OTA] Error %u\n", err);
      fill_solid(leds, NUM_LEDS, CRGB(80, 0, 0));
      FastLED.show();
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] Enabled");
  }

  // Start OSC listener
  initOsc(oscUdp, deviceConfig.oscPort);

  // Start sACN listener (if enabled)
  initSacn(deviceConfig);

  // Start web server
  setupWebRoutes(webServer);
  webServer.begin();
  Serial.println("[Web] Server started on port 80");

  Serial.println("=== Ready ===");
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  webServer.handleClient();
  handleOsc(oscUdp, deviceConfig, currentPattern);
  if (deviceConfig.otaEnabled) ArduinoOTA.handle();

  // sACN takes priority — if an sACN packet arrives, it writes directly
  // to leds[] and we skip the pattern engine for that frame.
  bool sacnUpdated = handleSacn(leds, deviceConfig);
  if (!sacnUpdated) {
    updatePattern(leds, NUM_LEDS, currentPattern);
  }

  // Rate-limit LED updates to ~60fps
  static unsigned long lastShow = 0;
  unsigned long now = millis();
  if (now - lastShow >= 16) {
    FastLED.show();
    lastShow = now;
  }

  delay(1);
}
