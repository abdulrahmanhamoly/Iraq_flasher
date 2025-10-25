/*
 * 3-LED Pattern Player for Iraq Flasher (WITH DURATION SUPPORT)
 * 
 * This sketch plays back pre-programmed 3-LED patterns based on button selection.
 * Features:
 * - 18 pre-programmed patterns (modes 1-18)
 * - Each pattern controls 3 LEDs independently using 3-bit states
 * - **Variable duration support** - each LED state has custom timing
 * - Hierarchical mode selection using 7 buttons
 * - BTN_14 and BTN_15 act as mode modifiers
 * - Debounced button inputs
 * - Status LED indicates active playback
 * 
 * Pattern Format:
 * Each pattern element is a 3-bit value (0-7) where:
 * - Bit 0 controls LED 1
 * - Bit 1 controls LED 2
 * - Bit 2 controls LED 3
 * Each state has a corresponding duration in milliseconds
 * 
 * Mode Selection:
 * - BTN_9-13 alone: Modes 2-6 (or mode 1 if none)
 * - BTN_14 + BTN_9-13: Modes 8-12 (or mode 7 if none)
 * - BTN_15 + BTN_9-13: Modes 14-18 (or mode 13 if none)
 */

#include <Arduino.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

const uint8_t LED_OUT_PIN_1 = 7;    // LED 1 output for pattern playback
const uint8_t LED_OUT_PIN_2 = 8;    // LED 2 output for pattern playback
const uint8_t LED_OUT_PIN_3 = 9;    // LED 3 output for pattern playback
const uint8_t STATUS_LED = 5;       // Status indicator (on during playback)

// Mode selection buttons
const uint8_t BTN_9  = 9;
const uint8_t BTN_10 = 10;
const uint8_t BTN_11 = 11;
const uint8_t BTN_12 = 12;
const uint8_t BTN_13 = 13;
const uint8_t BTN_14 = 14;          // Mode modifier (modes 7-12)
const uint8_t BTN_15 = 15;          // Mode modifier (modes 13-18)

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

const unsigned long DEBOUNCE_MS = 50UL;  // Button debounce time

// ============================================================================
// PRE-PROGRAMMED PATTERNS (WITH DURATIONS)
// ============================================================================
// Each pattern is an array of 3-bit LED states (0-7)
// Bit 0 = LED 1 state, Bit 1 = LED 2 state, Bit 2 = LED 3 state
// Each element represents which LEDs should be ON at that step
// Each pattern has a corresponding duration array (in milliseconds)

// Mode 1 pattern - Sequential LED activation
const uint8_t parts_mode1[] = {
  1,0,2,0,4,0,1,2,0,3,0,4,0,5,0,6,0,7,0,6,0,5,0,4,0,3,0,2,0,1,0
};
const uint16_t durations_mode1[] = {
  150,150,150,150,150,150,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,150,150
};
const size_t NUM_MODE1 = sizeof(parts_mode1) / sizeof(parts_mode1[0]);

// Mode 2 pattern - Binary counting
const uint8_t parts_mode2[] = {
  0,1,2,3,4,5,6,7,6,5,4,3,2,1,0,1,3,5,7,2,4,6,0
};
const uint16_t durations_mode2[] = {
  200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,150,150,150,150,150,150,150,200
};
const size_t NUM_MODE2 = sizeof(parts_mode2) / sizeof(parts_mode2[0]);

// Mode 3 pattern - Chase effect
const uint8_t parts_mode3[] = {
  1,2,4,0,2,4,1,0,4,1,2,0,1,2,4,0,7,0,1,2,4
};
const uint16_t durations_mode3[] = {
  100,100,100,50,100,100,100,50,100,100,100,50,100,100,100,100,200,100,100,100,100
};
const size_t NUM_MODE3 = sizeof(parts_mode3) / sizeof(parts_mode3[0]);

// Mode 4 pattern - Random combinations
const uint8_t parts_mode4[] = {
  1,3,7,0,2,6,4,0,5,1,3,0,6,2,4,0,7,5,1,0,3,7,2,0
};
const uint16_t durations_mode4[] = {
  120,180,250,100,150,200,180,100,220,120,180,100,200,150,180,100,300,220,120,100,180,250,150,100
};
const size_t NUM_MODE4 = sizeof(parts_mode4) / sizeof(parts_mode4[0]);

// Mode 5 pattern - Flashing all LEDs
const uint8_t parts_mode5[] = {
  0,7,0,7,0,1,2,4,0,3,5,6,0,7,0,1,4,0,2,5,0,3,6,0
};
const uint16_t durations_mode5[] = {
  100,100,100,100,200,150,150,150,100,150,150,150,100,200,100,150,150,100,150,150,100,150,150,100
};
const size_t NUM_MODE5 = sizeof(parts_mode5) / sizeof(parts_mode5[0]);

// Mode 6 pattern - LED 1 and 3 alternating
const uint8_t parts_mode6[] = {
  1,4,0,1,4,0,5,0,5,0,1,4,0,5,0,1,0,4,0,5,1,4,0
};
const uint16_t durations_mode6[] = {
  120,120,80,120,120,80,200,80,200,80,120,120,80,200,80,120,80,120,80,200,120,120,80
};
const size_t NUM_MODE6 = sizeof(parts_mode6) / sizeof(parts_mode6[0]);

// Mode 7 pattern - LED 2 only (blink)
const uint8_t parts_mode7[] = {
  2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0
};
const uint16_t durations_mode7[] = {
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100
};
const size_t NUM_MODE7 = sizeof(parts_mode7) / sizeof(parts_mode7[0]);

// Mode 8 pattern - LED 1 and 2 (blink)
const uint8_t parts_mode8[] = {
  3,0,3,0,3,0,3,0,3,0,3,0,3,0,3,0,3,0,3,0,3,0,3,0,3,0
};
const uint16_t durations_mode8[] = {
  150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150
};
const size_t NUM_MODE8 = sizeof(parts_mode8) / sizeof(parts_mode8[0]);

// Mode 9 pattern - LED 2 and 3 (blink)
const uint8_t parts_mode9[] = {
  6,0,6,0,6,0,6,0,6,0,6,0,6,0,6,0,6,0,6,0,6,0,6,0,6,0
};
const uint16_t durations_mode9[] = {
  200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200
};
const size_t NUM_MODE9 = sizeof(parts_mode9) / sizeof(parts_mode9[0]);

// Mode 10 pattern - LED 1 and 3 (blink)
const uint8_t parts_mode10[] = {
  5,0,5,0,5,0,5,0,5,0,5,0,5,0,5,0,5,0,5,0,5,0,5,0,5,0
};
const uint16_t durations_mode10[] = {
  180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180,180
};
const size_t NUM_MODE10 = sizeof(parts_mode10) / sizeof(parts_mode10[0]);

// Mode 11 pattern - All LEDs (fast strobe)
const uint8_t parts_mode11[] = {
  7,0,7,0,7,0,7,0,7,0,7,0,7,0,7,0,7,0,7,0,7,0,7,0,7,0
};
const uint16_t durations_mode11[] = {
  50,50,50,50,50,300,50,50,50,50,50,300,50,50,50,50,50,300,50,50,50,50,50,300,50,50
};
const size_t NUM_MODE11 = sizeof(parts_mode11) / sizeof(parts_mode11[0]);

// Mode 12 pattern - All off (idle)
const uint8_t parts_mode12[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
const uint16_t durations_mode12[] = {
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100
};
const size_t NUM_MODE12 = sizeof(parts_mode12) / sizeof(parts_mode12[0]);

// Mode 13 pattern - LED 1 only (blink)
const uint8_t parts_mode13[] = {
  1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0
};
const uint16_t durations_mode13[] = {
  120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120
};
const size_t NUM_MODE13 = sizeof(parts_mode13) / sizeof(parts_mode13[0]);

// Mode 14 pattern - LED 3 only (blink)
const uint8_t parts_mode14[] = {
  4,0,4,0,4,0,4,0,4,0,4,0,4,0,4,0,4,0,4,0,4,0,4,0,4,0
};
const uint16_t durations_mode14[] = {
  140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140
};
const size_t NUM_MODE14 = sizeof(parts_mode14) / sizeof(parts_mode14[0]);

// Mode 15 pattern - Complex sequence
const uint8_t parts_mode15[] = {
  7,1,3,5,0,6,2,4,0,7,0,1,0,3,0,5,0,6,0,2,0,4,0,7,0
};
const uint16_t durations_mode15[] = {
  200,100,100,100,100,100,100,100,100,200,100,150,100,150,100,150,100,150,100,150,100,150,100,200,100
};
const size_t NUM_MODE15 = sizeof(parts_mode15) / sizeof(parts_mode15[0]);

// Mode 16 pattern - Smooth wave
const uint8_t parts_mode16[] = {
  1,2,3,4,5,6,7,0,0,7,6,5,4,3,2,1,0,1,3,7,0,2,6,0,4,5,0
};
const uint16_t durations_mode16[] = {
  80,80,80,80,80,80,80,200,200,80,80,80,80,80,80,80,150,100,100,150,100,100,150,100,100,150,200
};
const size_t NUM_MODE16 = sizeof(parts_mode16) / sizeof(parts_mode16[0]);

// Mode 17 pattern - Fast alternating
const uint8_t parts_mode17[] = {
  1,2,4,0,3,5,7,0,2,4,6,0,1,3,5,0,4,6,0,2,0,1,7,0
};
const uint16_t durations_mode17[] = {
  60,60,60,40,60,60,60,40,60,60,60,40,60,60,60,40,60,60,40,60,40,60,100,40
};
const size_t NUM_MODE17 = sizeof(parts_mode17) / sizeof(parts_mode17[0]);

// Mode 18 pattern - Breathing effect
const uint8_t parts_mode18[] = {
  0,1,3,7,3,1,0,2,6,7,6,2,0,4,5,7,5,4,0
};
const uint16_t durations_mode18[] = {
  300,200,200,300,200,200,300,200,200,300,200,200,300,200,200,300,200,200,300
};
const size_t NUM_MODE18 = sizeof(parts_mode18) / sizeof(parts_mode18[0]);

// ============================================================================
// PATTERN LOOKUP TABLES
// ============================================================================

// Array of pointers to pattern arrays (indexed by mode number)
const uint8_t* const modeParts[] = {
  nullptr,        // Mode 0 (unused)
  parts_mode1,
  parts_mode2,
  parts_mode3,
  parts_mode4,
  parts_mode5,
  parts_mode6,
  parts_mode7,
  parts_mode8,
  parts_mode9,
  parts_mode10,
  parts_mode11,
  parts_mode12,
  parts_mode13,
  parts_mode14,
  parts_mode15,
  parts_mode16,
  parts_mode17,
  parts_mode18
};

// Array of pattern lengths (indexed by mode number)
const size_t modeLengths[] = {
  0,              // Mode 0 (unused)
  NUM_MODE1,
  NUM_MODE2,
  NUM_MODE3,
  NUM_MODE4,
  NUM_MODE5,
  NUM_MODE6,
  NUM_MODE7,
  NUM_MODE8,
  NUM_MODE9,
  NUM_MODE10,
  NUM_MODE11,
  NUM_MODE12,
  NUM_MODE13,
  NUM_MODE14,
  NUM_MODE15,
  NUM_MODE16,
  NUM_MODE17,
  NUM_MODE18
};

// Array of pointers to duration arrays (indexed by mode number)
const uint16_t* const modeDurations[] = {
  nullptr,           // Mode 0 (unused)
  durations_mode1,
  durations_mode2,
  durations_mode3,
  durations_mode4,
  durations_mode5,
  durations_mode6,
  durations_mode7,
  durations_mode8,
  durations_mode9,
  durations_mode10,
  durations_mode11,
  durations_mode12,
  durations_mode13,
  durations_mode14,
  durations_mode15,
  durations_mode16,
  durations_mode17,
  durations_mode18
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

volatile uint8_t currentMode = 1;   // Currently selected mode (1-18)
volatile bool modeChanged = false;  // Flag indicating mode change

// ============================================================================
// BUTTON READING FUNCTIONS
// ============================================================================

/**
 * Read all button states and return as a bitmask
 * Bit 0 = BTN_9, Bit 1 = BTN_10, ..., Bit 6 = BTN_15
 * LOW (pressed) = 1, HIGH (released) = 0
 */
static inline uint8_t readSwitchMask() {
  uint8_t m = 0;
  if (digitalRead(BTN_9)  == LOW) m |= (1 << 0);
  if (digitalRead(BTN_10) == LOW) m |= (1 << 1);
  if (digitalRead(BTN_11) == LOW) m |= (1 << 2);
  if (digitalRead(BTN_12) == LOW) m |= (1 << 3);
  if (digitalRead(BTN_13) == LOW) m |= (1 << 4);
  if (digitalRead(BTN_14) == LOW) m |= (1 << 5);
  if (digitalRead(BTN_15) == LOW) m |= (1 << 6);
  return m;
}

/**
 * Convert button mask to mode number (1-18)
 * Hierarchical selection:
 * - BTN_14 pressed: modes 7-12
 * - BTN_15 pressed: modes 13-18
 * - Neither: modes 1-6
 * Within each group, BTN_9-13 select specific mode
 */
static inline uint8_t modeFromMask(uint8_t m) {
  bool s14 = (m & (1 << 5)) != 0;  // BTN_14 pressed?
  bool s15 = (m & (1 << 6)) != 0;  // BTN_15 pressed?

  // BTN_14 pressed: modes 7-12
  if (s14) {
    if (m & (1 << 0)) return 8;   // BTN_9
    if (m & (1 << 1)) return 9;   // BTN_10
    if (m & (1 << 2)) return 10;  // BTN_11
    if (m & (1 << 3)) return 11;  // BTN_12
    if (m & (1 << 4)) return 12;  // BTN_13
    return 7;                     // BTN_14 alone
  }

  // BTN_15 pressed: modes 13-18
  if (s15) {
    if (m & (1 << 0)) return 14;  // BTN_9
    if (m & (1 << 1)) return 15;  // BTN_10
    if (m & (1 << 2)) return 16;  // BTN_11
    if (m & (1 << 3)) return 17;  // BTN_12
    if (m & (1 << 4)) return 18;  // BTN_13
    return 13;                    // BTN_15 alone
  }

  // No modifier: modes 1-6
  if (m & (1 << 0)) return 2;     // BTN_9
  if (m & (1 << 1)) return 3;     // BTN_10
  if (m & (1 << 2)) return 4;     // BTN_11
  if (m & (1 << 3)) return 5;     // BTN_12
  if (m & (1 << 4)) return 6;     // BTN_13

  return currentMode;             // No buttons pressed, keep current mode
}

// ============================================================================
// MODE SELECTION LOGIC
// ============================================================================

/**
 * Poll buttons and update mode if changed
 * Uses debouncing and arming logic to prevent spurious mode changes
 * Mode changes only when buttons transition from all-released to pressed
 */
void pollButtonsAndMaybeChangeMode() {
  static uint8_t lastRaw = 0;           // Last raw button reading
  static uint8_t stable = 0;            // Debounced stable state
  static unsigned long tmark = 0;       // Time of last change
  static bool armed = true;             // Armed for mode change

  uint8_t raw = readSwitchMask();

  // Debounce logic
  if (raw != lastRaw) {
    lastRaw = raw;
    tmark = millis();
  } else if ((millis() - tmark) >= DEBOUNCE_MS && raw != stable) {
    stable = raw;

    // Arm when all buttons released
    if (stable == 0) {
      armed = true;
    }
    // Change mode when armed and buttons pressed
    else if (armed) {
      uint8_t nm = modeFromMask(stable);
      currentMode = nm;
      modeChanged = true;
      armed = false;
    }
  }
}

/**
 * Wait for specified duration while checking for mode changes
 * Returns true if mode changed during wait, false otherwise
 */
bool waitWithButtonCheck(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    pollButtonsAndMaybeChangeMode();
    if (modeChanged) return true;
    delay(8);  // Small delay to prevent busy-waiting
  }
  return false;
}

// ============================================================================
// PATTERN PLAYBACK
// ============================================================================

/**
 * Play the current mode's pattern in a loop with variable durations
 * Exits when mode changes
 * Each LED state displays for its specified duration
 */
void runCurrentModePattern() {
  uint8_t mode = currentMode;
  const uint8_t* pattern = modeParts[mode];
  const uint16_t* durations = modeDurations[mode];
  size_t len = modeLengths[mode];
  
  // Skip if invalid pattern
  if (pattern == nullptr || durations == nullptr || len == 0) return;
  
  // Turn on status LED during playback
  digitalWrite(STATUS_LED, HIGH);
  
  // Loop pattern until mode changes
  while (!modeChanged) {
    for (size_t i = 0; i < len; ++i) {
      // Extract 3-bit LED state from pattern data
      uint8_t ledState = pattern[i];
      
      // Set individual LEDs based on bit values
      // Bit 0 = LED 1, Bit 1 = LED 2, Bit 2 = LED 3
      digitalWrite(LED_OUT_PIN_1, (ledState & 1) ? HIGH : LOW);
      digitalWrite(LED_OUT_PIN_2, (ledState & 2) ? HIGH : LOW);
      digitalWrite(LED_OUT_PIN_3, (ledState & 4) ? HIGH : LOW);
      
      // Wait for the specific duration for this LED state
      uint16_t duration = durations[i];
      if (waitWithButtonCheck(duration)) break;
    }
  }
  
  // Turn off status LED when done
  digitalWrite(STATUS_LED, LOW);
}

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
  // Configure LED output pins
  pinMode(LED_OUT_PIN_1, OUTPUT);
  pinMode(LED_OUT_PIN_2, OUTPUT);
  pinMode(LED_OUT_PIN_3, OUTPUT);
  digitalWrite(LED_OUT_PIN_1, LOW);
  digitalWrite(LED_OUT_PIN_2, LOW);
  digitalWrite(LED_OUT_PIN_3, LOW);
  
  // Configure status LED
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // Configure button input pins with pullups
  pinMode(BTN_9,  INPUT_PULLUP);
  pinMode(BTN_10, INPUT_PULLUP);
  pinMode(BTN_11, INPUT_PULLUP);
  pinMode(BTN_12, INPUT_PULLUP);
  pinMode(BTN_13, INPUT_PULLUP);
  pinMode(BTN_14, INPUT_PULLUP);
  pinMode(BTN_15, INPUT_PULLUP);

  // Initialize to mode 1
  currentMode = 1;
  modeChanged = false;
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Clear mode change flag
  modeChanged = false;
  
  // Run the current mode's pattern (loops until mode changes)
  runCurrentModePattern();
  
  // Ensure all LEDs are off between mode changes
  digitalWrite(LED_OUT_PIN_1, LOW);
  digitalWrite(LED_OUT_PIN_2, LOW);
  digitalWrite(LED_OUT_PIN_3, LOW);
}
