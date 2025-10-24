/*
 * Multi-LED Pattern Recorder/Player for Iraq Flasher
 * 
 * This sketch records and plays back patterns from 3 buttons controlling 3 LEDs.
 * Features:
 * - Records button press patterns for 3 buttons simultaneously
 * - Each button controls its own LED during playback
 * - Stores up to 32 slots with 1300 pattern segments each
 * - CSV export via serial for pattern data
 * - Auto-stop on inactivity
 * - Debounced button inputs
 */

#include <Arduino.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Button inputs (3 buttons for recording patterns)
const uint8_t BTN_1_PIN = 2;        // Button 1 - controls LED 1
const uint8_t BTN_2_PIN = 3;        // Button 2 - controls LED 2
const uint8_t BTN_3_PIN = 4;        // Button 3 - controls LED 3

// LED outputs (3 LEDs for pattern playback)
const uint8_t LED_1_PIN = 7;        // LED 1 - controlled by Button 1
const uint8_t LED_2_PIN = 8;        // LED 2 - controlled by Button 2
const uint8_t LED_3_PIN = 9;        // LED 3 - controlled by Button 3

// Control buttons
const uint8_t REC_PIN = 10;         // Record button
const uint8_t PLAY_PIN = 11;        // Play button
const uint8_t CLR_PIN = 12;         // Clear slot button
const uint8_t STOP_PIN = 13;        // Stop button
const uint8_t DUMP_PIN = 29;       // Dump to serial button

// Slot selection toggle switches (5 bits = 32 slots)
const uint8_t TGL_PINS[5] = {14, 15, 16, 26, 27};

// Status indicator
const uint8_t STATUS_LED = 5;       // Status LED (blinks during record, solid during play)

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

const uint8_t NUM_SLOTS = 32;       // Number of pattern storage slots
const uint16_t MAX_PARTS = 1300;    // Maximum pattern segments per slot
const uint32_t FILE_MAX_MS = 90000; // Maximum recording duration (90 seconds)
const uint32_t AUTO_STOP_MS = 30000;// Auto-stop after 30s of inactivity
const uint16_t DEBOUNCE_MS = 15;    // Debounce time for control buttons
const uint32_t BTN_GLITCH_US = 300; // Glitch filter for pattern buttons (microseconds)

const char* CSV_PREFIX = "@RZ3CSV:"; // CSV export prefix (3 LEDs)

// ============================================================================
// GLOBAL VARIABLES - Pattern Storage
// ============================================================================

// Pattern data storage: each part contains 3 LED states packed in a byte
// Bit 0 = LED1 state, Bit 1 = LED2 state, Bit 2 = LED3 state
// Duration is stored in milliseconds (rounded to 5ms)
uint16_t parts[NUM_SLOTS][MAX_PARTS];    // Pattern segments (LED states + duration)
uint16_t partCount[NUM_SLOTS] = {0};     // Number of parts in each slot
uint32_t totalMs[NUM_SLOTS] = {0};       // Total duration of each slot

// ============================================================================
// GLOBAL VARIABLES - State Management
// ============================================================================

// Recording/Playing state
bool recording = false;             // Currently recording a pattern
bool playing = false;               // Currently playing a pattern
uint8_t activeSlot = 0;             // Currently active slot (0-31)

// Recording state tracking
bool started = false;               // Recording has started (first button press received)
uint8_t curStates = 0;              // Current button states (3 bits: LED1, LED2, LED3)
uint8_t prevStates = 0;             // Previous button states
uint32_t lastChangeUs = 0;          // Last state change timestamp (microseconds)
uint32_t lastActivityMs = 0;        // Last activity timestamp (milliseconds)

// Playback state tracking
uint16_t playIndex = 0;             // Current part index during playback
uint32_t phaseStartMs = 0;          // Phase start timestamp

// Status LED state
uint32_t statLastMs = 0;            // Last status LED toggle time
bool statLevel = false;             // Current status LED state

// ============================================================================
// DEBOUNCE STRUCTURES
// ============================================================================

// Debounce structure for button state tracking
struct Deb {
  bool stable;                      // Stable (debounced) state
  bool prev;                        // Previous stable state
  uint32_t t;                       // Last change timestamp
};

// Debounce instances for control buttons
Deb dbRec, dbPlay, dbClr, dbStop, dbDump;

// Pattern buttons use custom glitch filter
bool btn1_stable = HIGH, btn1_prev = HIGH, btn1_last_raw = HIGH;
bool btn2_stable = HIGH, btn2_prev = HIGH, btn2_last_raw = HIGH;
bool btn3_stable = HIGH, btn3_prev = HIGH, btn3_last_raw = HIGH;
uint32_t btn1_last_raw_change_us = 0;
uint32_t btn2_last_raw_change_us = 0;
uint32_t btn3_last_raw_change_us = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Set status LED to idle (off) if not recording or playing
 */
void setStatusIdle() {
  if (!recording && !playing) {
    digitalWrite(STATUS_LED, LOW);
    statLevel = false;
  }
}

/**
 * Round milliseconds to nearest 5ms increment
 * This reduces storage requirements and noise in timing data
 */
inline uint16_t round5(uint32_t ms) {
  return (uint16_t)(((ms + 2) / 5) * 5);
}

/**
 * Update debounce state for a button
 * Returns true if stable state changed
 */
bool upd(Deb& d, bool raw, uint32_t now) {
  if (raw != d.stable) {
    if (now - d.t >= DEBOUNCE_MS) {
      d.prev = d.stable;
      d.stable = raw;
      d.t = now;
      return true;
    }
  } else {
    d.t = now;
  }
  return false;
}

/**
 * Read slot selection from toggle switches
 * Returns 0-31 based on which switches are LOW
 */
uint8_t readSlot() {
  uint8_t v = 0;
  for (uint8_t i = 0; i < 5; ++i) {
    if (digitalRead(TGL_PINS[i]) == LOW) {
      v |= (1 << i);
    }
  }
  return v;
}

/**
 * Pack 3 LED states into a byte
 * Bit 0 = LED1, Bit 1 = LED2, Bit 2 = LED3
 */
inline uint8_t packLedStates(bool led1, bool led2, bool led3) {
  return (led1 ? 1 : 0) | ((led2 ? 1 : 0) << 1) | ((led3 ? 1 : 0) << 2);
}

/**
 * Unpack 3 LED states from a byte
 * Returns array of {led1, led2, led3}
 */
void unpackLedStates(uint8_t packed, bool& led1, bool& led2, bool& led3) {
  led1 = (packed & 1) != 0;
  led2 = (packed & 2) != 0;
  led3 = (packed & 4) != 0;
}

/**
 * Update all LED outputs based on packed state
 */
void updateLeds(uint8_t packedState) {
  bool led1, led2, led3;
  unpackLedStates(packedState, led1, led2, led3);
  digitalWrite(LED_1_PIN, led1 ? HIGH : LOW);
  digitalWrite(LED_2_PIN, led2 ? HIGH : LOW);
  digitalWrite(LED_3_PIN, led3 ? HIGH : LOW);
}

/**
 * Reset playback state and turn off all LEDs
 */
void resetPlay() {
  playing = false;
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);
  digitalWrite(LED_3_PIN, LOW);
  setStatusIdle();
}

// ============================================================================
// RECORDING FUNCTIONS
// ============================================================================

// Pattern data format:
// Bits 0-12: Duration in 5ms increments (0-8191 * 5ms = 0-40.95s per segment)
// Bits 13-15: LED states (Bit 13=LED1, Bit 14=LED2, Bit 15=LED3)
// Example: 0xE005 = LED1+LED2+LED3 ON (0xE000) + 5ms duration (0x0005)

const uint16_t DURATION_MASK = 0x1FFF;  // Bits 0-12
const uint16_t LED_STATE_MASK = 0xE000;  // Bits 13-15
const uint8_t LED1_BIT = 13;
const uint8_t LED2_BIT = 14;
const uint8_t LED3_BIT = 15;

/**
 * Create pattern entry from LED states and duration
 */
inline uint16_t makePatternEntry(uint8_t ledStates, uint16_t duration5ms) {
  return (ledStates << 13) | (duration5ms & DURATION_MASK);
}

/**
 * Extract LED states from pattern entry
 */
inline uint8_t getLedStates(uint16_t entry) {
  return (entry >> 13) & 0x07;  // 3 bits
}

/**
 * Extract duration from pattern entry
 */
inline uint16_t getDuration(uint16_t entry) {
  return entry & DURATION_MASK;  // 13 bits
}

/**
 * End current pattern part and store its duration with LED states
 * Called when any button state changes during recording
 */
void endPart(uint32_t now) {
  if (!started) return;
  
  // Calculate duration since last change
  uint32_t durMs = (micros() - lastChangeUs + 500) / 1000;
  uint16_t r = round5(durMs);
  
  // Ignore very short durations (noise)
  if (r == 0) {
    lastActivityMs = now;
    lastChangeUs = micros();
    return;
  }
  
  // Check remaining recording time
  uint32_t rem = (FILE_MAX_MS > totalMs[activeSlot]) ? (FILE_MAX_MS - totalMs[activeSlot]) : 0;
  if (rem == 0) {
    recording = false;
    updateLeds(0);  // Turn off all LEDs
    started = false;
    setStatusIdle();
    return;
  }
  
  // Convert duration to 5ms units (max 8191 * 5ms = 40955ms per segment)
  uint16_t duration5ms = r / 5;
  if (duration5ms > DURATION_MASK) duration5ms = DURATION_MASK;
  
  // Clip duration to remaining time
  if (r > rem) {
    duration5ms = (rem / 5) & DURATION_MASK;
  }
  
  // Create pattern entry with current LED states
  uint8_t ledStates = curStates;
  uint16_t patternEntry = makePatternEntry(ledStates, duration5ms);
  
  // Store the part
  if (partCount[activeSlot] < MAX_PARTS && duration5ms > 0) {
    parts[activeSlot][partCount[activeSlot]++] = patternEntry;
    totalMs[activeSlot] += r;
  }
  
  lastActivityMs = now;
  lastChangeUs = micros();
  
  // Stop if limits reached
  if (totalMs[activeSlot] >= FILE_MAX_MS || partCount[activeSlot] >= MAX_PARTS) {
    recording = false;
    updateLeds(0);  // Turn off all LEDs
    started = false;
    setStatusIdle();
  }
}

/**
 * Stop recording and finalize current pattern
 */
void stopRec(uint32_t now) {
  if (recording) {
    endPart(now);
    recording = false;
    updateLeds(0);  // Turn off all LEDs
    started = false;
    setStatusIdle();
  }
}

// ============================================================================
// SLOT MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * Clear all data from a slot
 * Stops recording/playing if this slot is active
 */
void clearSlot(uint8_t s) {
  partCount[s] = 0;
  totalMs[s] = 0;
  
  if (recording && activeSlot == s) stopRec(millis());
  if (playing && activeSlot == s) resetPlay();
}

/**
 * Dump slot data to serial in CSV format
 * Format: @RZ3CSV:3,slot,count,totalMs,ledStates1,duration1,ledStates2,duration2,...
 * Where ledStates is a number 0-7 representing which LEDs are on
 */
void dumpSlot(uint8_t s) {
  stopRec(millis());
  resetPlay();
  
  uint16_t count = partCount[s];
  uint32_t tot = totalMs[s];
  
  Serial.print(CSV_PREFIX);
  Serial.print("3,");  // 3 LEDs
  Serial.print(s);
  Serial.print(",");
  Serial.print(count);
  Serial.print(",");
  Serial.print(tot);
  
  for (uint16_t i = 0; i < count; ++i) {
    Serial.print(",");
    Serial.print(getLedStates(parts[s][i]));  // LED states (0-7)
    Serial.print(",");
    Serial.print(getDuration(parts[s][i]) * 5);  // Duration in ms (multiply by 5)
  }
  
  Serial.println();
}

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
  // Configure pattern button pins
  pinMode(BTN_1_PIN, INPUT_PULLUP);
  pinMode(BTN_2_PIN, INPUT_PULLUP);
  pinMode(BTN_3_PIN, INPUT_PULLUP);
  
  // Configure LED output pins
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(LED_3_PIN, OUTPUT);
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);
  digitalWrite(LED_3_PIN, LOW);
  
  // Configure control button pins
  pinMode(REC_PIN, INPUT_PULLUP);
  pinMode(PLAY_PIN, INPUT_PULLUP);
  pinMode(CLR_PIN, INPUT_PULLUP);
  pinMode(STOP_PIN, INPUT_PULLUP);
  pinMode(DUMP_PIN, INPUT_PULLUP);
  
  // Configure slot selection pins
  for (uint8_t i = 0; i < 5; ++i) {
    pinMode(TGL_PINS[i], INPUT_PULLUP);
  }
  
  // Configure status LED
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  // Initialize debounce states
  uint32_t now = millis();
  dbRec.stable = dbRec.prev = HIGH;
  dbRec.t = now;
  dbPlay.stable = dbPlay.prev = HIGH;
  dbPlay.t = now;
  dbClr.stable = dbClr.prev = HIGH;
  dbClr.t = now;
  dbStop.stable = dbStop.prev = HIGH;
  dbStop.t = now;
  dbDump.stable = dbDump.prev = HIGH;
  dbDump.t = now;
  
  // Initialize pattern button states
  btn1_stable = btn1_prev = btn1_last_raw = HIGH;
  btn2_stable = btn2_prev = btn2_last_raw = HIGH;
  btn3_stable = btn3_prev = btn3_last_raw = HIGH;
  btn1_last_raw_change_us = micros();
  btn2_last_raw_change_us = micros();
  btn3_last_raw_change_us = micros();
  
  // Initialize serial for CSV output
  Serial.begin(115200);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  uint32_t now = millis();
  uint32_t nowUs = micros();
  
  // ---- Pattern buttons debouncing with glitch filter ----
  
  // Button 1
  bool rawBtn1 = digitalRead(BTN_1_PIN);
  if (rawBtn1 != btn1_last_raw) {
    btn1_last_raw = rawBtn1;
    btn1_last_raw_change_us = nowUs;
  }
  if ((rawBtn1 != btn1_stable) && (nowUs - btn1_last_raw_change_us >= BTN_GLITCH_US)) {
    btn1_prev = btn1_stable;
    btn1_stable = rawBtn1;
  }
  
  // Button 2
  bool rawBtn2 = digitalRead(BTN_2_PIN);
  if (rawBtn2 != btn2_last_raw) {
    btn2_last_raw = rawBtn2;
    btn2_last_raw_change_us = nowUs;
  }
  if ((rawBtn2 != btn2_stable) && (nowUs - btn2_last_raw_change_us >= BTN_GLITCH_US)) {
    btn2_prev = btn2_stable;
    btn2_stable = rawBtn2;
  }
  
  // Button 3
  bool rawBtn3 = digitalRead(BTN_3_PIN);
  if (rawBtn3 != btn3_last_raw) {
    btn3_last_raw = rawBtn3;
    btn3_last_raw_change_us = nowUs;
  }
  if ((rawBtn3 != btn3_stable) && (nowUs - btn3_last_raw_change_us >= BTN_GLITCH_US)) {
    btn3_prev = btn3_stable;
    btn3_stable = rawBtn3;
  }
  
  // ---- Control button debouncing ----
  bool rREC = digitalRead(REC_PIN);
  bool rPLAY = digitalRead(PLAY_PIN);
  bool rCLR = digitalRead(CLR_PIN);
  bool rSTOP = digitalRead(STOP_PIN);
  bool rDUMP = digitalRead(DUMP_PIN);
  
  bool eREC = upd(dbRec, rREC, now);
  bool ePLAY = upd(dbPlay, rPLAY, now);
  bool eCLR = upd(dbClr, rCLR, now);
  bool eSTOP = upd(dbStop, rSTOP, now);
  bool eDUMP = upd(dbDump, rDUMP, now);
  
  uint8_t sel = readSlot();
  
  // Detect button presses (HIGH to LOW transitions)
  bool recP = (eREC && dbRec.prev == HIGH && dbRec.stable == LOW);
  bool playP = (ePLAY && dbPlay.prev == HIGH && dbPlay.stable == LOW);
  bool clrP = (eCLR && dbClr.prev == HIGH && dbClr.stable == LOW);
  bool stopP = (eSTOP && dbStop.prev == HIGH && dbStop.stable == LOW);
  bool dumpP = (eDUMP && dbDump.prev == HIGH && dbDump.stable == LOW);

  // ---- Handle control button presses ----
  
  // REC button: toggle recording
  if (recP) {
    resetPlay();
    if (!recording) {
      // Start recording
      activeSlot = sel;
      partCount[activeSlot] = 0;
      totalMs[activeSlot] = 0;
      recording = true;
      started = false;
      curStates = 0;
      prevStates = 0;
      lastChangeUs = nowUs;
      lastActivityMs = now;
    } else {
      // Stop recording
      stopRec(now);
    }
  }
  
  // STOP button: stop recording or playback
  if (stopP) {
    if (recording) {
      stopRec(now);
    } else if (playing) {
      resetPlay();
    }
  }
  
  // PLAY button: start playback if slot has data
  if (playP && partCount[sel] > 0) {
    stopRec(now);
    activeSlot = sel;
    playing = true;
    playIndex = 0;
    phaseStartMs = now;
  }
  
  // CLEAR button: clear selected slot
  if (clrP) {
    clearSlot(sel);
  }
  
  // DUMP button: export slot to serial
  if (dumpP) {
    dumpSlot(sel);
  }

  // ---- Recording state machine ----
  if (recording) {
    // Update current LED states based on button states
    uint8_t newStates = packLedStates(!btn1_stable, !btn2_stable, !btn3_stable);
    
    if (!started) {
      // Waiting for first button press to start recording
      if (newStates != 0) {  // Any button pressed
        started = true;
        curStates = newStates;
        prevStates = newStates;
        lastChangeUs = nowUs;
        lastActivityMs = now;
        updateLeds(curStates);
      } else {
        // Auto-stop if no activity
        if (now - lastActivityMs >= AUTO_STOP_MS) {
          stopRec(now);
        }
      }
    } else {
      // Recording in progress - track button state changes
      if (newStates != curStates) {
        // Button state changed, record the previous state
        endPart(now);
        curStates = newStates;
        updateLeds(curStates);
      }
      
      // Auto-stop conditions
      if (now - lastActivityMs >= AUTO_STOP_MS) {
        stopRec(now);
      }
      if (totalMs[activeSlot] >= FILE_MAX_MS || partCount[activeSlot] >= MAX_PARTS) {
        stopRec(now);
      }
    }
  }

  // ---- Playback state machine ----
  if (playing && partCount[activeSlot] > 0) {
    if (playIndex >= partCount[activeSlot]) {
      // Playback complete, loop back to beginning
      playIndex = 0;
      phaseStartMs = now;
    }
    
    if (playIndex < partCount[activeSlot]) {
      // Get current pattern entry
      uint16_t entry = parts[activeSlot][playIndex];
      uint8_t ledStates = getLedStates(entry);
      uint16_t duration5ms = getDuration(entry);
      
      // Update LEDs immediately
      updateLeds(ledStates);
      
      // Check if it's time to advance to next segment
      if (now - phaseStartMs >= (uint32_t)duration5ms * 5) {
        playIndex++;
        phaseStartMs = now;
      }
    }
  }

  // ---- Status LED management ----
  if (recording) {
    // Blink during recording (120ms interval)
    if (now - statLastMs >= 120) {
      statLastMs = now;
      statLevel = !statLevel;
      digitalWrite(STATUS_LED, statLevel ? HIGH : LOW);
    }
  } else if (playing) {
    // Solid on during playback
    if (!statLevel) {
      statLevel = true;
      digitalWrite(STATUS_LED, HIGH);
    }
  } else {
    // Off when idle
    if (statLevel) {
      statLevel = false;
      digitalWrite(STATUS_LED, LOW);
    }
  }
}
