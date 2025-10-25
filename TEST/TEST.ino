/*
 * 3-LED Pattern Recorder/Player for Iraq Flasher (RP2040 Version)
 * 
 * This sketch records and plays back 3-button patterns that control 3 LEDs.
 * Optimized for RP2040 Zero with 264KB SRAM
 * 
 * Features:
 * - Records 3-bit LED states (0-7) from 3 button combinations
 * - Each LED state represents which of the 3 LEDs should be ON
 * - **Records individual duration for each LED state** (NEW!)
 * - Single slot with 2000 pattern segments (expandable to 32 slots)
 * - Maximum recording duration: 90 seconds per slot
 * - Plays back recorded patterns with precise timing
 * - CSV export via serial for 3-bit pattern data WITH durations
 * - Auto-stop on inactivity
 * - Debounced button inputs
 */

#include <Arduino.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Input/Output pins
const uint8_t LED_OUT_PIN_1 = 7;   // LED 1 output for playback
const uint8_t LED_OUT_PIN_2 = 8;   // LED 2 output for playback
const uint8_t LED_OUT_PIN_3 = 9;   // LED 3 output for playback
// Recording buttons (3 buttons instead of 1)
const uint8_t BTN_1_PIN = 4;        // Button 1 for recording pattern
const uint8_t BTN_2_PIN = 6;        // Button 2 for recording pattern
const uint8_t BTN_3_PIN = 2;        // Button 3 for recording pattern
// Control buttons
const uint8_t REC_PIN = 10;         // Record button (start/stop recording)
const uint8_t PLAY_PIN = 11;        // Play button
const uint8_t CLR_PIN = 12;         // Clear slot button
const uint8_t STOP_PIN = 3;         // Stop button
const uint8_t DUMP_PIN = 13;        // Dump to serial button (GP13 - FIXED for Pico)

// Slot selection toggle switches (not used in single-slot mode, but kept for future expansion)
const uint8_t TGL_PINS[2] = {14, 15};  // Can be expanded to 5 pins for 32 slots

// Status indicator
const uint8_t STATUS_LED = 5;       // Status LED (blinks during record, solid during play)

// ============================================================================
// CONFIGURATION CONSTANTS (RP2040 Optimized)
// ============================================================================

const uint8_t NUM_SLOTS = 1;        // Single slot mode (expandable to 32 with RP2040's memory)
const uint16_t MAX_PARTS = 2000;    // 2000 pattern segments (10x Arduino capacity)
const uint32_t FILE_MAX_MS = 90000; // Maximum recording duration (90 seconds)
const uint32_t AUTO_STOP_MS = 30000;// Auto-stop after 30s of inactivity
const uint16_t DEBOUNCE_MS = 15;    // Debounce time for control buttons
const uint32_t BTN_GLITCH_US = 300; // Glitch filter for main button (microseconds)

const char* CSV_PREFIX = "@RZ1CSV:"; // CSV export prefix

// ============================================================================
// GLOBAL VARIABLES - Pattern Storage
// ============================================================================

// Pattern data storage: each part is a 3-bit LED state (0-7) where:
// Bit 0 = LED 1 state, Bit 1 = LED 2 state, Bit 2 = LED 3 state
uint8_t parts[NUM_SLOTS][MAX_PARTS];      // 3-bit LED states for each slot
uint16_t durations[NUM_SLOTS][MAX_PARTS]; // Duration in ms for each state (NEW!)
uint16_t partCount[NUM_SLOTS] = {0};      // Number of parts in each slot
uint32_t totalMs[NUM_SLOTS] = {0};        // Total duration of each slot

// ============================================================================
// GLOBAL VARIABLES - State Management
// ============================================================================

// Recording/Playing state
bool recording = false;             // Currently recording a pattern
bool playing = false;               // Currently playing a pattern
uint8_t activeSlot = 0;             // Currently active slot (always 0 in single-slot mode)

// Recording state tracking (3 buttons)
bool started = false;               // Recording has started (first button press received)
uint8_t curState = 0;               // Current 3-bit button state (0-7)
uint8_t prevState = 0;              // Previous 3-bit button state
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

// Main buttons (3 buttons) use custom glitch filter
uint8_t btn1_stable = HIGH, btn1_prev = HIGH, btn1_last_raw = HIGH;
uint8_t btn2_stable = HIGH, btn2_prev = HIGH, btn2_last_raw = HIGH;
uint8_t btn3_stable = HIGH, btn3_prev = HIGH, btn3_last_raw = HIGH;
uint32_t btn1_last_raw_change_us = 0;
uint32_t btn2_last_raw_change_us = 0;
uint32_t btn3_last_raw_change_us = 0;
uint32_t last_button_change_us = 0; // Last time any button changed

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
 * In single-slot mode, always returns 0
 * Toggle switches kept for future multi-slot expansion
 */
uint8_t readSlot() {
  return 0;  // Single slot mode - always use slot 0
  
  // For future multi-slot expansion, uncomment:
  // uint8_t v = 0;
  // for (uint8_t i = 0; i < 2; ++i) {
  //   if (digitalRead(TGL_PINS[i]) == LOW) {
  //     v |= (1 << i);
  //   }
  // }
  // return v;
}

/**
 * Reset playback state and turn off LED
 */
void resetPlay() {
  playing = false;
  digitalWrite(LED_OUT_PIN_1, LOW);
  digitalWrite(LED_OUT_PIN_2, LOW);
  digitalWrite(LED_OUT_PIN_3, LOW);
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
    digitalWrite(STATUS_LED, LOW);
    started = false;
    setStatusIdle();
    return;
  }
  
  // Clip duration to remaining time
  if (r > rem) r = (uint16_t)rem;
  
  // Store the 3-bit LED state AND its duration
  if (partCount[activeSlot] < MAX_PARTS && r > 0) {
    uint16_t idx = partCount[activeSlot];
    parts[activeSlot][idx] = curState;      // Store LED state
    durations[activeSlot][idx] = r;         // Store duration (NEW!)
    partCount[activeSlot]++;
    totalMs[activeSlot] += r;
  }
  
  lastActivityMs = now;
  lastChangeUs = micros();
  
  // Stop if limits reached
  if (totalMs[activeSlot] >= FILE_MAX_MS || partCount[activeSlot] >= MAX_PARTS) {
    recording = false;
    digitalWrite(STATUS_LED, LOW);
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
    digitalWrite(STATUS_LED, LOW);
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
 * Dump slot data to serial in CSV format with durations
 * Format: @RZ1CSV:4,slot,count,totalMs,state1:dur1,state2:dur2,...
 */
void dumpSlot(uint8_t s) {
  stopRec(millis());
  resetPlay();
  
  uint16_t count = partCount[s];
  uint32_t tot = totalMs[s];
  
  Serial.print(CSV_PREFIX);
  Serial.print("4,");  // Format version 4 = 3-bit LED with durations
  Serial.print(s);
  Serial.print(",");
  Serial.print(count);
  Serial.print(",");
  Serial.print(tot);
  
  for (uint16_t i = 0; i < count; ++i) {
    Serial.print(",");
    Serial.print(parts[s][i]);        // 3-bit LED state (0-7)
    Serial.print(":");
    Serial.print(durations[s][i]);    // Duration in ms (NEW!)
  }
  
  Serial.println();
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
  
  // Configure recording button pins
  pinMode(BTN_1_PIN, INPUT_PULLUP);
  pinMode(BTN_2_PIN, INPUT_PULLUP);
  pinMode(BTN_3_PIN, INPUT_PULLUP);
  
  pinMode(REC_PIN, INPUT_PULLUP);
  pinMode(CLR_PIN, INPUT_PULLUP);
  pinMode(STOP_PIN, INPUT_PULLUP);
  pinMode(DUMP_PIN, INPUT_PULLUP);
  
  for (uint8_t i = 0; i < 2; ++i) {
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
  
  // Initialize 3-button states
  btn1_stable = btn1_prev = btn1_last_raw = HIGH;
  btn1_last_raw_change_us = micros();
  btn2_stable = btn2_prev = btn2_last_raw = HIGH;
  btn2_last_raw_change_us = micros();
  btn3_stable = btn3_prev = btn3_last_raw = HIGH;
  btn3_last_raw_change_us = micros();
  last_button_change_us = micros();
  
  // Initialize serial for CSV output
  Serial.begin(115200);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  uint32_t now = millis();
  uint32_t nowUs = micros();
  
  // ---- 3-Button debouncing with glitch filter ----
  bool rawBtn1 = digitalRead(BTN_1_PIN);
  bool rawBtn2 = digitalRead(BTN_2_PIN);
  bool rawBtn3 = digitalRead(BTN_3_PIN);
  
  // Track button 1
  if (rawBtn1 != btn1_last_raw) {
    btn1_last_raw = rawBtn1;
    btn1_last_raw_change_us = nowUs;
  }
  if ((rawBtn1 != btn1_stable) && (nowUs - btn1_last_raw_change_us >= BTN_GLITCH_US)) {
    btn1_prev = btn1_stable;
    btn1_stable = rawBtn1;
  }
  
  // Track button 2
  if (rawBtn2 != btn2_last_raw) {
    btn2_last_raw = rawBtn2;
    btn2_last_raw_change_us = nowUs;
  }
  if ((rawBtn2 != btn2_stable) && (nowUs - btn2_last_raw_change_us >= BTN_GLITCH_US)) {
    btn2_prev = btn2_stable;
    btn2_stable = rawBtn2;
  }
  
  // Track button 3
  if (rawBtn3 != btn3_last_raw) {
    btn3_last_raw = rawBtn3;
    btn3_last_raw_change_us = nowUs;
  }
  if ((rawBtn3 != btn3_stable) && (nowUs - btn3_last_raw_change_us >= BTN_GLITCH_US)) {
    btn3_prev = btn3_stable;
    btn3_stable = rawBtn3;
  }
  
  // Check if any button state changed
  bool anyButtonChanged = (btn1_stable != btn1_prev) || (btn2_stable != btn2_prev) || (btn3_stable != btn3_prev);
  if (anyButtonChanged) {
    last_button_change_us = nowUs;
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
      curState = 0;
      prevState = 0;
      lastChangeUs = nowUs;
      lastActivityMs = now;
      // Set initial LED state based on current button states
      curState = (btn1_stable == LOW ? 1 : 0) | 
                 (btn2_stable == LOW ? 2 : 0) | 
                 (btn3_stable == LOW ? 4 : 0);
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
    // Update current LED state based on button states
    uint8_t newState = (btn1_stable == LOW ? 1 : 0) | 
                       (btn2_stable == LOW ? 2 : 0) | 
                       (btn3_stable == LOW ? 4 : 0);
    
    if (!started) {
      // Waiting for first button change to start recording
      if (newState != curState) {
        started = true;
        curState = newState;
        lastChangeUs = nowUs;
        lastActivityMs = now;
        // Set LEDs to show current state
        digitalWrite(STATUS_LED, HIGH);
      } else {
        // Auto-stop if no activity
        if (now - lastActivityMs >= AUTO_STOP_MS) {
          stopRec(now);
        }
      }
    } else {
      // Recording in progress - track state changes
      if (newState != curState) {
        // State changed, record the previous state with duration
        endPart(now);
        prevState = curState;
        curState = newState;
        lastChangeUs = nowUs;
        lastActivityMs = now;
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
      // Get current 3-bit LED state and its duration
      uint8_t ledState = parts[activeSlot][playIndex];
      uint16_t duration = durations[activeSlot][playIndex];  // Use recorded duration!
      
      // Set individual LEDs based on bit values
      digitalWrite(LED_OUT_PIN_1, (ledState & 1) ? HIGH : LOW);
      digitalWrite(LED_OUT_PIN_2, (ledState & 2) ? HIGH : LOW);
      digitalWrite(LED_OUT_PIN_3, (ledState & 4) ? HIGH : LOW);
      
      if (playPhase == 0) {
        // Phase 0: Wait for recorded duration
        if (now - phaseStartMs >= duration) {
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
