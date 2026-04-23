#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FastLED.h>

// ============================================================
// Hardware Configuration
// Default pin comes from build flags (platformio.ini):
//   -DDEFAULT_LED_PIN=N
// The actual pin used at runtime is stored in DeviceConfig.ledPin.
// ============================================================
#ifndef DEFAULT_LED_PIN
  #define DEFAULT_LED_PIN 2
#endif
#define LED_COLOR_ORDER   GRB     // WS2812B standard

// ============================================================
// Ring layout — three concentric rings, daisy-chained
// Order along the data line: outer (24) → middle (16) → inner (7)
// If your wiring is different, swap the RING_*_START offsets.
// ============================================================
#define RING_OUTER_COUNT  24
#define RING_MIDDLE_COUNT 16
#define RING_INNER_COUNT  7

#define RING_OUTER_START  0
#define RING_MIDDLE_START (RING_OUTER_START + RING_OUTER_COUNT)
#define RING_INNER_START  (RING_MIDDLE_START + RING_MIDDLE_COUNT)

#define NUM_LEDS          (RING_OUTER_COUNT + RING_MIDDLE_COUNT + RING_INNER_COUNT)  // 47
#define NUM_RINGS         3

// BOOT button pin differs by board:
//   ESP32-C3 XIAO:              GPIO9
//   ESP-WROOM-32 / WT32-ETH01:  GPIO0
//   ESP32-S3 dev boards:        GPIO0
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define BOOT_BUTTON_PIN   9
#else
  #define BOOT_BUTTON_PIN   0
#endif

// ============================================================
// Defaults
// ============================================================
#define DEFAULT_BRIGHTNESS    128
#define DEFAULT_OSC_PORT      9000
#define DEFAULT_DEVICE_NAME   "led-notifier"
#define CONFIG_FILE           "/config.json"

// ============================================================
// Limits
// ============================================================
#define MAX_MAPPINGS          32
#define MAX_OSC_ADDR_LEN      64
#define MAX_PATTERN_NAME_LEN  24
#define MAX_DEVICE_NAME_LEN   32

// ============================================================
// Data Structures
// ============================================================

// Ring mask: bit 0 = outer (24), bit 1 = middle (16), bit 2 = inner (7)
#define RING_MASK_ALL   0x07
#define RING_MASK_OUTER 0x01
#define RING_MASK_MIDDLE 0x02
#define RING_MASK_INNER 0x04

struct OscMapping {
  char oscAddress[MAX_OSC_ADDR_LEN];
  char patternName[MAX_PATTERN_NAME_LEN];
  uint32_t color;           // 0xRRGGBB
  uint8_t brightness;       // 0-255
  float speed;              // multiplier: 1.0 = normal
  uint8_t ringMask;         // which rings to light (see RING_MASK_* above)
  bool useOscColor;         // override color from OSC args
  bool useOscBrightness;    // override brightness from OSC args
  bool active;              // slot in use
};

struct DeviceConfig {
  uint8_t globalBrightness;
  uint16_t oscPort;
  char deviceName[MAX_DEVICE_NAME_LEN];
  OscMapping mappings[MAX_MAPPINGS];
  uint8_t mappingCount;
  bool otaEnabled;            // enable ArduinoOTA
  int8_t ringOffset[NUM_RINGS];  // per-ring rotation offset in LEDs (signed)
  bool ringReverse[NUM_RINGS];   // per-ring direction reversal
  uint8_t ledPin;             // GPIO for WS2812B data line

  // sACN / E1.31 receive
  bool sacnEnabled;
  uint16_t sacnUniverse;      // 1..63999
  uint16_t sacnStartAddr;     // 1..512 (DMX start channel)
  uint8_t sacnPriority;       // 0..200, we keep only highest-priority source
  bool sacnMulticast;         // true = multicast, false = unicast (listen-only)
};

// Runtime state for the active pattern
struct PatternState {
  int8_t activePatternId;     // index into pattern registry, -1 = none
  uint32_t color;             // current color (0xRRGGBB)
  uint8_t brightness;         // current brightness
  float speed;                // current speed multiplier
  uint8_t ringMask;           // which rings to light
  unsigned long startTime;    // millis() when pattern was triggered
  unsigned long duration;     // total ms for countdown patterns
  bool running;
  float phase;                // generic phase accumulator
  float externalValue;        // for progress pattern (0.0–1.0)
};

// ============================================================
// Globals (defined in LED_Notifier.ino)
// ============================================================
extern CRGB leds[];
extern DeviceConfig deviceConfig;
extern PatternState currentPattern;

// ============================================================
// Config functions (config.cpp)
// ============================================================
void loadConfig(DeviceConfig& cfg);
void saveConfig(const DeviceConfig& cfg);
void resetConfigDefaults(DeviceConfig& cfg);

#endif
