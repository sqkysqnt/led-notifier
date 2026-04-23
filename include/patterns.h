#ifndef PATTERNS_H
#define PATTERNS_H

#include "config.h"

// Pattern function signature
typedef void (*PatternFunc)(CRGB* leds, int numLeds, PatternState& state, unsigned long now);

struct PatternDef {
  const char* name;
  const char* displayName;
  PatternFunc func;
  bool hasCountdown;
  unsigned long defaultDuration;  // ms, for countdown patterns
};

// Pattern registry
extern PatternDef patternRegistry[];
extern const int patternCount;

// Core functions
void initPatternState(PatternState& state);
void triggerPattern(PatternState& state, int patternId, uint32_t color,
                    uint8_t brightness, float speed,
                    uint8_t ringMask = RING_MASK_ALL, unsigned long duration = 0);
void stopPattern(PatternState& state, CRGB* leds, int numLeds);
void updatePattern(CRGB* leds, int numLeds, PatternState& state);

// Find pattern index by name, returns -1 if not found
int findPatternByName(const char* name);

#endif
