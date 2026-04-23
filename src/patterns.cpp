#include "patterns.h"
#include <math.h>

// ============================================================
// Ring metadata — indexed by ring number (0=outer, 1=middle, 2=inner)
// ============================================================
struct RingInfo {
  int start;
  int count;
};
static const RingInfo rings[NUM_RINGS] = {
  { RING_OUTER_START,  RING_OUTER_COUNT  },
  { RING_MIDDLE_START, RING_MIDDLE_COUNT },
  { RING_INNER_START,  RING_INNER_COUNT  },
};

// Map a logical ring position (r, i) to the physical LED index,
// applying the ring's calibration offset. Positive offset rotates clockwise.
static inline int ringLed(int r, int i) {
  const RingInfo& ring = rings[r];
  int off = deviceConfig.ringOffset[r];
  int idx = ((i + off) % ring.count + ring.count) % ring.count;
  if (deviceConfig.ringReverse[r]) idx = ring.count - 1 - idx;
  return ring.start + idx;
}

// ============================================================
// Helper: convert 0xRRGGBB to CRGB
// ============================================================
static CRGB colorFromHex(uint32_t hex) {
  return CRGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// ============================================================
// Helper: blend from green → yellow → red based on fraction 0.0–1.0
// ============================================================
static CRGB countdownColor(float fraction) {
  // fraction: 0.0 = just started (green), 1.0 = time's up (red)
  if (fraction < 0.5f) {
    // green → yellow
    uint8_t r = (uint8_t)(255 * fraction * 2);
    return CRGB(r, 255, 0);
  } else {
    // yellow → red
    uint8_t g = (uint8_t)(255 * (1.0f - fraction) * 2);
    return CRGB(255, g, 0);
  }
}

// ============================================================
// Pattern: Solid
// ============================================================
static void patternSolid(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  CRGB c = colorFromHex(state.color);
  fill_solid(leds, numLeds, c);
}

// ============================================================
// Pattern: Blink
// ============================================================
static void patternBlink(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  unsigned long interval = (unsigned long)(500 / state.speed);
  bool on = ((now - state.startTime) / interval) % 2 == 0;
  if (on) {
    fill_solid(leds, numLeds, colorFromHex(state.color));
  } else {
    fill_solid(leds, numLeds, CRGB::Black);
  }
}

// ============================================================
// Pattern: Pulse (sine wave brightness)
// ============================================================
static void patternPulse(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  float elapsed = (now - state.startTime) / 1000.0f * state.speed;
  float val = (sinf(elapsed * PI * 2.0f) * 0.5f + 0.5f);
  CRGB c = colorFromHex(state.color);
  uint8_t bri = (uint8_t)(val * state.brightness);
  fill_solid(leds, numLeds, c);
  FastLED.setBrightness(bri);
}

// ============================================================
// Pattern: Breathe (cubic ease-in-out)
// ============================================================
static void patternBreathe(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  float elapsed = (now - state.startTime) / 1000.0f * state.speed;
  float t = fmodf(elapsed, 2.0f) / 2.0f;  // 0.0–1.0 over 2 seconds
  // Cubic ease-in-out
  float val;
  if (t < 0.5f) {
    val = 4.0f * t * t * t;
  } else {
    float f = (2.0f * t - 2.0f);
    val = 0.5f * f * f * f + 1.0f;
  }
  val = constrain(val, 0.0f, 1.0f);
  CRGB c = colorFromHex(state.color);
  uint8_t bri = (uint8_t)(val * state.brightness);
  fill_solid(leds, numLeds, c);
  FastLED.setBrightness(bri);
}

// ============================================================
// Pattern: Fade (linear fade to black, then restart)
// ============================================================
static void patternFade(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  unsigned long cycleDuration = (unsigned long)(3000 / state.speed);
  float fraction = (float)((now - state.startTime) % cycleDuration) / cycleDuration;
  float val = 1.0f - fraction;
  CRGB c = colorFromHex(state.color);
  uint8_t bri = (uint8_t)(val * state.brightness);
  fill_solid(leds, numLeds, c);
  FastLED.setBrightness(bri);
}

// ============================================================
// Pattern: Rainbow — each ring gets a full rainbow, with calibration
// ============================================================
static void patternRainbow(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  uint8_t hueOffset = (uint8_t)((now - state.startTime) / (10.0f / state.speed));
  for (int r = 0; r < NUM_RINGS; r++) {
    uint8_t ringHueStart = hueOffset;  // rings now in sync for visual radial alignment
    uint8_t hueStep = 256 / rings[r].count;
    for (int i = 0; i < rings[r].count; i++) {
      leds[ringLed(r, i)] = CHSV(ringHueStart + i * hueStep, 255, 255);
    }
  }
  FastLED.setBrightness(state.brightness);
}

// ============================================================
// Pattern: Chase — all rings share the same angular sweep.
// Because rings have different LED counts, smaller rings advance
// in LED-steps more slowly so each ring completes a full revolution
// in the same wall-clock time.
// ============================================================
static void patternChase(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  // Full revolution takes outer-ring-count * step_ms
  const float stepMs = 100.0f / state.speed;
  const float revMs = RING_OUTER_COUNT * stepMs;
  float t = fmodf((now - state.startTime), revMs) / revMs;  // 0.0..1.0 around the circle
  fill_solid(leds, numLeds, CRGB::Black);
  CRGB c = colorFromHex(state.color);
  for (int r = 0; r < NUM_RINGS; r++) {
    int pos = (int)(t * rings[r].count);
    // Keep trail at ~constant angular width (3 LEDs on outer)
    int trail = max(1, (int)roundf(3.0f * rings[r].count / (float)RING_OUTER_COUNT));
    for (int i = 0; i < trail; i++) {
      leds[ringLed(r, pos + i)] = c;
    }
  }
  FastLED.setBrightness(state.brightness);
}

// ============================================================
// Pattern: Alert (alternating halves flash, per-ring with calibration)
// ============================================================
static void patternAlert(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  unsigned long interval = (unsigned long)(250 / state.speed);
  bool phase = ((now - state.startTime) / interval) % 2 == 0;
  CRGB c = colorFromHex(state.color);
  for (int r = 0; r < NUM_RINGS; r++) {
    int half = rings[r].count / 2;
    for (int i = 0; i < rings[r].count; i++) {
      bool firstHalf = (i < half);
      leds[ringLed(r, i)] = (firstHalf == phase) ? c : CRGB::Black;
    }
  }
  FastLED.setBrightness(state.brightness);
}

// ============================================================
// Pattern: Progress — fills active rings from outer to inner
// ============================================================
static void patternProgress(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  float val = constrain(state.externalValue, 0.0f, 1.0f);
  uint8_t mask = state.ringMask ? state.ringMask : RING_MASK_ALL;
  // Sum LEDs across active rings
  int activeTotal = 0;
  for (int r = 0; r < NUM_RINGS; r++) {
    if (mask & (1 << r)) activeTotal += rings[r].count;
  }
  int litCount = (int)(val * activeTotal + 0.5f);
  CRGB c = colorFromHex(state.color);
  fill_solid(leds, numLeds, CRGB::Black);
  int remaining = litCount;
  for (int r = 0; r < NUM_RINGS && remaining > 0; r++) {
    if (!(mask & (1 << r))) continue;
    int n = min(remaining, rings[r].count);
    for (int i = 0; i < n; i++) {
      leds[ringLed(r, i)] = c;
    }
    remaining -= n;
  }
  FastLED.setBrightness(state.brightness);
}

// ============================================================
// Pattern: Countdown (generic, duration set at trigger time)
// ============================================================
static void patternCountdown(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  unsigned long elapsed = now - state.startTime;

  if (elapsed >= state.duration) {
    // Time's up — flash red
    unsigned long flashPhase = (elapsed - state.duration) / 250;
    if (flashPhase % 2 == 0) {
      fill_solid(leds, numLeds, CRGB::Red);
    } else {
      fill_solid(leds, numLeds, CRGB::Black);
    }
    FastLED.setBrightness(state.brightness);
    return;
  }

  float fraction = (float)elapsed / state.duration;  // 0.0 → 1.0
  float remaining = 1.0f - fraction;

  uint8_t mask = state.ringMask ? state.ringMask : RING_MASK_ALL;
  int activeTotal = 0;
  for (int r = 0; r < NUM_RINGS; r++) {
    if (mask & (1 << r)) activeTotal += rings[r].count;
  }

  // Number of LEDs still lit, filling active rings from outer to inner
  int litCount = (int)(remaining * activeTotal + 0.5f);
  if (litCount < 1 && remaining > 0) litCount = 1;

  CRGB c = countdownColor(fraction);
  fill_solid(leds, numLeds, CRGB::Black);
  int left = litCount;
  for (int r = 0; r < NUM_RINGS && left > 0; r++) {
    if (!(mask & (1 << r))) continue;
    int n = min(left, rings[r].count);
    for (int i = 0; i < n; i++) {
      leds[ringLed(r, i)] = c;
    }
    left -= n;
  }
  FastLED.setBrightness(state.brightness);
}

// ============================================================
// Pattern: Lamp Test — walks one LED at a time through R, G, B.
// Ignores ring calibration — shows the physical daisy-chain order.
// ============================================================
static void patternLampTest(CRGB* leds, int numLeds, PatternState& state, unsigned long now) {
  const unsigned long stepMs = (unsigned long)(120 / state.speed);
  unsigned long step = (now - state.startTime) / stepMs;
  unsigned long total = (unsigned long)numLeds * 3;
  step = step % total;
  int colorPhase = step / numLeds;  // 0=R, 1=G, 2=B
  int pos = step % numLeds;
  CRGB c;
  switch (colorPhase) {
    case 0: c = CRGB::Red;   break;
    case 1: c = CRGB::Blue;  break;
    default: c = CRGB::Green; break;
  }
  fill_solid(leds, numLeds, CRGB::Black);
  leds[pos] = c;
  FastLED.setBrightness(state.brightness);
}

// ============================================================
// Pattern Registry
// ============================================================
PatternDef patternRegistry[] = {
  { "solid",          "Solid",            patternSolid,     false, 0 },
  { "blink",          "Blink",            patternBlink,     false, 0 },
  { "pulse",          "Pulse",            patternPulse,     false, 0 },
  { "breathe",        "Breathe",          patternBreathe,   false, 0 },
  { "fade",           "Fade",             patternFade,      false, 0 },
  { "rainbow",        "Rainbow",          patternRainbow,   false, 0 },
  { "chase",          "Chase",            patternChase,     false, 0 },
  { "alert",          "Alert",            patternAlert,     false, 0 },
  { "progress",       "Progress",         patternProgress,  false, 0 },
  { "lamptest",       "Lamp Test",        patternLampTest,  false, 0 },
  { "countdown_10s",  "Countdown 10s",    patternCountdown, true,  10000UL },
  { "countdown_30s",  "Countdown 30s",    patternCountdown, true,  30000UL },
  { "countdown_1m",   "Countdown 1min",   patternCountdown, true,  60000UL },
  { "countdown_5m",   "Countdown 5min",   patternCountdown, true,  300000UL },
};

const int patternCount = sizeof(patternRegistry) / sizeof(patternRegistry[0]);

// ============================================================
// Core Functions
// ============================================================

void initPatternState(PatternState& state) {
  state.activePatternId = -1;
  state.color = 0xFFFFFF;
  state.brightness = DEFAULT_BRIGHTNESS;
  state.speed = 1.0f;
  state.ringMask = RING_MASK_ALL;
  state.startTime = 0;
  state.duration = 0;
  state.running = false;
  state.phase = 0;
  state.externalValue = 0;
}

void triggerPattern(PatternState& state, int patternId, uint32_t color,
                    uint8_t brightness, float speed,
                    uint8_t ringMask, unsigned long duration) {
  if (patternId < 0 || patternId >= patternCount) return;

  state.activePatternId = patternId;
  state.color = color;
  state.brightness = brightness;
  state.speed = speed;
  state.ringMask = ringMask ? ringMask : RING_MASK_ALL;
  state.startTime = millis();
  state.running = true;
  state.phase = 0;
  state.externalValue = 0;

  // Use pattern's default duration for countdown patterns if none specified
  if (patternRegistry[patternId].hasCountdown && duration == 0) {
    state.duration = patternRegistry[patternId].defaultDuration;
  } else {
    state.duration = duration;
  }

  Serial.printf("[Pattern] Triggered: %s, color=%06X, bri=%d, speed=%.1f, rings=0x%X\n",
                patternRegistry[patternId].name, color, brightness, speed, state.ringMask);
}

void stopPattern(PatternState& state, CRGB* leds, int numLeds) {
  state.running = false;
  state.activePatternId = -1;
  fill_solid(leds, numLeds, CRGB::Black);
  FastLED.show();
  Serial.println("[Pattern] Stopped");
}

void updatePattern(CRGB* leds, int numLeds, PatternState& state) {
  if (!state.running || state.activePatternId < 0) {
    return;
  }
  PatternDef& def = patternRegistry[state.activePatternId];
  def.func(leds, numLeds, state, millis());

  // Lamp test always shows every physical LED regardless of ring mask
  if (strcmp(def.name, "lamptest") == 0) return;

  // Mask off disabled rings
  uint8_t mask = state.ringMask ? state.ringMask : RING_MASK_ALL;
  for (int r = 0; r < NUM_RINGS; r++) {
    if (!(mask & (1 << r))) {
      for (int i = 0; i < rings[r].count; i++) {
        leds[rings[r].start + i] = CRGB::Black;
      }
    }
  }
}

int findPatternByName(const char* name) {
  for (int i = 0; i < patternCount; i++) {
    if (strcmp(patternRegistry[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}
