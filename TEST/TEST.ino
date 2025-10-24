/*
 * Pattern Recorder/Player for Iraq Flasher
 * 
 * This sketch records and plays back button press patterns with precise timing.
 * Features:
 * - Records button press/release patterns to 32 slots
 * - Plays back recorded patterns on LED output
 * - CSV export via serial for pattern data
 * - Auto-stop on inactivity
 * - Debounced button inputs
 */

#include <Arduino.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Input/Output pins
const uint8_t BTN_PIN = 4;          // Button to record pattern from
const uint8_t LED_OUT_PIN = 7;      // LED output for playback
// Control buttons
const uint8_t REC_PIN = 10;         // Record button
const uint8_t PLAY_PIN = 11;        // Play button
const uint8_t CLR_PIN = 12;         // Clear slot button
const uint8_t STOP_PIN = 3;         // Stop button
const uint8_t DUMP_PIN = 29;        // Dump to serial button

// Slot selection toggle switches (5 bits = 32 slots)
const uint8_t TGL_PINS[5] = {13, 14, 15, 26, 27};

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
const uint32_t BTN_GLITCH_US = 300; // Glitch filter for main button (microseconds)

const char* CSV_PREFIX = "@RZ1CSV:"; // CSV export prefix

// ============================================================================
// GLOBAL VARIABLES - Pattern Storage
// ============================================================================

// Pattern data storage: each part is a duration in milliseconds (rounded to 5ms)
uint16_t parts[NUM_SLOTS][MAX_PARTS];  // Pattern segments for each slot
uint16_t partCount[NUM_SLOTS] = {0};   // Number of parts in each slot
uint32_t totalMs[NUM_SLOTS] = {0};     // Total duration of each slot

// ============================================================================
// GLOBAL VARIABLES - State Management
// ============================================================================

// Recording/Playing state
bool recording = false;             // Currently recording a pattern
bool playing = false;               // Currently playing a pattern
uint8_t activeSlot = 0;             // Currently active slot (0-31)

// Recording state tracking
bool started = false;               // Recording has started (first button press received)
bool curState = false;              // Current button state during recording
uint32_t lastChangeUs = 0;          // Last state change timestamp (microseconds)
uint32_t lastActivityMs = 0;        // Last activity timestamp (milliseconds)

// Playback state tracking
uint16_t playIndex = 0;             // Current part index during playback
uint8_t playPhase = 0;              // Playback phase (0=playing, 1=advance)
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

// Main button (BTN_PIN) uses custom glitch filter
bool btn_stable = HIGH;             // Stable button state
bool btn_prev = HIGH;               // Previous stable state
bool btn_last_raw = HIGH;           // Last raw reading
uint32_t btn_last_raw_change_us = 0;// Last raw change timestamp

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
 * Reset playback state and turn off LED
 */
void resetPlay() {
  playing = false;
  digitalWrite(LED_OUT_PIN, LOW);
  setStatusIdle();
}

// ============================================================================
// RECORDING FUNCTIONS
// ============================================================================

/**
 * End current pattern part and store its duration
 * Called when button state changes during recording
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
    digitalWrite(LED_OUT_PIN, LOW);
    started = false;
    setStatusIdle();
    return;
  }
  
  // Clip duration to remaining time
  if (r > rem) r = (uint16_t)rem;
  
  // Store the part
  if (partCount[activeSlot] < MAX_PARTS && r > 0) {
    parts[activeSlot][partCount[activeSlot]++] = r;
    totalMs[activeSlot] += r;
  }
  
  lastActivityMs = now;
  lastChangeUs = micros();
  
  // Stop if limits reached
  if (totalMs[activeSlot] >= FILE_MAX_MS || partCount[activeSlot] >= MAX_PARTS) {
    recording = false;
    digitalWrite(LED_OUT_PIN, LOW);
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
    digitalWrite(LED_OUT_PIN, LOW);
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
 * Format: @RZ1CSV:1,slot,count,totalMs,part1,part2,...
 */
void dumpSlot(uint8_t s) {
  stopRec(millis());
  resetPlay();
  
  uint16_t count = partCount[s];
  uint32_t tot = totalMs[s];
  
  Serial.print(CSV_PREFIX);
  Serial.print("1,");
  Serial.print(s);
  Serial.print(",");
  Serial.print(count);
  Serial.print(",");
  Serial.print(tot);
  
  for (uint16_t i = 0; i < count; ++i) {
    Serial.print(",");
    Serial.print(parts[s][i]);
  }
  
  Serial.println();
}

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
  // Configure pins
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_OUT_PIN, OUTPUT);
  digitalWrite(LED_OUT_PIN, LOW);
  
  pinMode(REC_PIN, INPUT_PULLUP);
  pinMode(PLAY_PIN, INPUT_PULLUP);
  pinMode(CLR_PIN, INPUT_PULLUP);
  pinMode(STOP_PIN, INPUT_PULLUP);
  pinMode(DUMP_PIN, INPUT_PULLUP);
  
  for (uint8_t i = 0; i < 5; ++i) {
    pinMode(TGL_PINS[i], INPUT_PULLUP);
  }
  
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
  
  btn_stable = btn_prev = btn_last_raw = HIGH;
  btn_last_raw_change_us = micros();
  
  // Initialize serial for CSV output
  Serial.begin(115200);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  uint32_t now = millis();
  uint32_t nowUs = micros();
  
  // ---- Main button debouncing with glitch filter ----
  bool rawBTN = digitalRead(BTN_PIN);
  if (rawBTN != btn_last_raw) {
    btn_last_raw = rawBTN;
    btn_last_raw_change_us = nowUs;
  }
  
  bool eBTN = false;  // Button event flag
  if ((rawBTN != btn_stable) && (nowUs - btn_last_raw_change_us >= BTN_GLITCH_US)) {
    btn_prev = btn_stable;
    btn_stable = rawBTN;
    eBTN = true;
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
      curState = false;
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
    playPhase = 0;
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
    if (!started) {
      // Waiting for first button press to start recording
      if (eBTN && btn_prev == HIGH && btn_stable == LOW) {
        started = true;
        curState = true;
        lastChangeUs = nowUs;
        lastActivityMs = now;
        digitalWrite(LED_OUT_PIN, HIGH);
      } else {
        // Auto-stop if no activity
        if (now - lastActivityMs >= AUTO_STOP_MS) {
          stopRec(now);
        }
      }
    } else {
      // Recording in progress - track button state changes
      if (eBTN) {
        if (curState && btn_prev == LOW && btn_stable == HIGH) {
          // Button released
          endPart(now);
          curState = false;
          digitalWrite(LED_OUT_PIN, LOW);
        } else if (!curState && btn_prev == HIGH && btn_stable == LOW) {
          // Button pressed
          endPart(now);
          curState = true;
          digitalWrite(LED_OUT_PIN, HIGH);
        }
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
      // Playback complete
      resetPlay();
    } else {
      // Determine LED state (even indices = ON, odd = OFF)
      bool on = (playIndex % 2 == 0);
      uint32_t dur = parts[activeSlot][playIndex];
      
      if (playPhase == 0) {
        // Phase 0: Set LED and wait for duration
        digitalWrite(LED_OUT_PIN, on ? HIGH : LOW);
        if (now - phaseStartMs >= dur) {
          playPhase = 1;
          phaseStartMs = now;
        }
      } else {
        // Phase 1: Advance to next part
        playIndex++;
        playPhase = 0;
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
