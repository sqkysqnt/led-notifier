// LED Notifier — ESP32-C3 + 24x WS2812B Ring
// WiFiManager + OSC Control + Web Configuration
//
// Libraries required (Arduino IDE Library Manager):
//   - FastLED (>= 3.7.0)
//   - WiFiManager by tzapu (>= 2.0.17)
//   - OSC by Adrian Freed & Yotam Mann (CNMAT)
//   - ArduinoJson (>= 7.0)
//
// Board: ESP32C3 Dev Module (esp32 by Espressif >= 3.0.0)
// Partition: Default 4MB with spiffs

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <FastLED.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "patterns.h"
#include "osc_handler.h"
#include "web_server.h"

// ============================================================
// Globals
// ============================================================
CRGB leds[NUM_LEDS];
DeviceConfig deviceConfig;
PatternState currentPattern;
WiFiUDP oscUdp;
WebServer webServer(80);

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

// ============================================================
// WiFi connected animation — green flash
// ============================================================
static void wifiConnectedAnimation() {
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

  // Initialize LEDs
  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  startupAnimation();

  // Initialize filesystem and load config
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed!");
  }
  loadConfig(deviceConfig);
  FastLED.setBrightness(deviceConfig.globalBrightness);

  // Initialize pattern state
  initPatternState(currentPattern);

  // WiFiManager
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback([](WiFiManager* wm) {
    Serial.println("[WiFi] Config portal started");
    // Show orange while in portal mode
    fill_solid(leds, NUM_LEDS, CRGB(80, 40, 0));
    FastLED.show();
  });

  // Check if BOOT button is held — force config portal.
  // GPIO0 on WROOM-32 can be held low by the USB-serial DTR line,
  // so require a sustained 2-second press to avoid false triggers.
  bool forcePortal = false;
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    Serial.println("[WiFi] BOOT button detected — hold 2s to reset WiFi...");
    fill_solid(leds, NUM_LEDS, CRGB(80, 0, 80));
    FastLED.show();
    unsigned long holdStart = millis();
    forcePortal = true;
    while (millis() - holdStart < 2000) {
      if (digitalRead(BOOT_BUTTON_PIN) != LOW) {
        forcePortal = false;
        break;
      }
      delay(50);
    }
  }
  if (forcePortal) {
    Serial.println("[WiFi] BOOT button confirmed — forcing config portal");
    wm.resetSettings();
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  }

  // Attempt auto-connect; if fails, start config portal
  String apName = String("LED-Notifier-") + String((uint32_t)ESP.getEfuseMac() & 0xFFFF, HEX);
  if (!wm.autoConnect(apName.c_str())) {
    Serial.println("[WiFi] Failed to connect, restarting...");
    fill_solid(leds, NUM_LEDS, CRGB(80, 0, 0));
    FastLED.show();
    delay(2000);
    ESP.restart();
  }

  // Connected!
  Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  WiFi.setSleep(false);  // Disable WiFi power save — required for reliable incoming connections
  wifiConnectedAnimation();

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
  updatePattern(leds, NUM_LEDS, currentPattern);

  // Rate-limit LED updates to ~60fps
  static unsigned long lastShow = 0;
  unsigned long now = millis();
  if (now - lastShow >= 16) {
    FastLED.show();
    lastShow = now;
  }

  // Yield to WiFi/system tasks — prevents WDT on ESP-WROOM-32
  // and ensures UDP/TCP packets are processed promptly
  delay(1);
}
