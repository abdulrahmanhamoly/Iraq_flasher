# Iraq Flasher - Complete Documentation

**3-LED Pattern Recorder & Player System - RP2040 Version**

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Complete Changelog](#complete-changelog)
3. [Hardware Requirements](#hardware-requirements)
4. [Pattern Format](#pattern-format)
5. [Usage Guide](#usage-guide)
6. [Memory & Performance](#memory--performance)

---

## Project Overview

The Iraq Flasher is a professional LED pattern recording and playback system for RP2040 microcontrollers.

### Key Features
- **Real-time pattern recording** with 3-button combinations
- **Variable duration support** - precise timing for each LED state
- **18 pre-programmed patterns** with professional effects
- **2000 pattern segments** per slot (expandable to 32 slots)
- **CSV export/import** for pattern sharing
- **RP2040 optimized** - uses only 2.3% of 264KB SRAM

### Use Cases
- 🎵 Music Visualization
- 🚨 Emergency Strobes
- 🚦 Traffic Simulations
- 📡 Morse Code
- ✨ Creative Lighting

---

## Complete Changelog

### From Original to Current Version

#### Phase 1: 3-LED Conversion
**Original:** 1 LED, 1 button, timing-based  
**Changed:** 3 LEDs, 3 buttons, state-based

```cpp
// BEFORE
const uint8_t LED_PIN = 13;
const uint8_t BTN_PIN = 2;
uint16_t timings[MAX_PARTS];

// AFTER
const uint8_t LED_OUT_PIN_1 = 7;
const uint8_t LED_OUT_PIN_2 = 8;
const uint8_t LED_OUT_PIN_3 = 9;
const uint8_t BTN_1_PIN = 4;
const uint8_t BTN_2_PIN = 5;
const uint8_t BTN_3_PIN = 6;
uint8_t parts[NUM_SLOTS][MAX_PARTS];  // 3-bit states
```

#### Phase 2: Arduino Memory Fix
**Issue:** 32×1300 = 41.6KB (exceeds 2KB SRAM)  
**Solution:** Reduced to 4×200 = 800 bytes

```cpp
// BEFORE
const uint8_t NUM_SLOTS = 32;
const uint16_t MAX_PARTS = 1300;

// AFTER
const uint8_t NUM_SLOTS = 4;
const uint16_t MAX_PARTS = 200;
```

#### Phase 3: RP2040 Port
**Platform:** Arduino UNO → RP2040 Zero  
**Memory:** 2KB → 264KB SRAM

```cpp
// RP2040 Configuration
const uint8_t NUM_SLOTS = 1;
const uint16_t MAX_PARTS = 2000;  // 10x capacity
```

#### Phase 4: Duration Support
**Feature:** Added individual timing for each state

```cpp
// Added duration array
uint16_t durations[NUM_SLOTS][MAX_PARTS];

// Recording
parts[slot][idx] = ledState;      // Store state
durations[slot][idx] = duration;  // Store timing

// Playback
uint16_t dur = durations[slot][idx];
delay(dur);  // Use recorded timing
```

#### Phase 5: CSV Format v4
**Format:** Added duration export

```cpp
// BEFORE (v3)
@RZ1CSV:3,0,6,600,1,2,4,1,2,4

// AFTER (v4)
@RZ1CSV:4,0,6,3000,1:500,0:500,2:300,0:200,4:800,0:700
```

#### Phase 6: final_first.ino Update
**Feature:** Added durations to all 18 modes

```cpp
// Added duration arrays
const uint16_t durations_mode1[] = {150,150,150,...};
const uint16_t durations_mode2[] = {200,200,200,...};
// ... all 18 modes

// Updated playback
uint16_t duration = durations[i];
delay(duration);  // Variable timing
```

### Summary Table

| Aspect | Original | Current |
|--------|----------|---------|
| LEDs | 1 | 3 |
| Buttons | 1 | 3 recording + 5 control |
| Platform | Arduino UNO | RP2040 Zero |
| SRAM | 2KB | 264KB |
| Storage | Timing data | States + durations |
| Slots | 32 | 1 (expandable to 32) |
| Segments | 1300 | 2000 |
| Memory | 41.6KB (2050%!) | 6KB (2.3%) |
| Duration | Fixed | Variable per state |
| CSV | Version 3 | Version 4 |
| Patterns | Basic | 18 professional |

---

## Hardware Requirements

### RP2040 Zero
- **SRAM:** 264KB
- **Flash:** 16MB
- **Clock:** 133 MHz
- **GPIO:** 30 pins

### Pin Assignments

**LEDs:**
```
LED_OUT_PIN_1 = GPIO 7
LED_OUT_PIN_2 = GPIO 8
LED_OUT_PIN_3 = GPIO 9
STATUS_LED    = GPIO 5
```

**Recording Buttons (TEST.ino):**
```
BTN_1_PIN = GPIO 4
BTN_2_PIN = GPIO 5
BTN_3_PIN = GPIO 6
```

**Control Buttons (TEST.ino):**
```
REC_PIN   = GPIO 10  (Start/stop recording)
PLAY_PIN  = GPIO 11  (Play pattern)
CLR_PIN   = GPIO 12  (Clear slot)
STOP_PIN  = GPIO 3   (Stop playback)
DUMP_PIN  = GPIO 29  (Export CSV)
```

**Mode Buttons (final_first.ino):**
```
BTN_9-13  = GPIO 9-13   (Mode selection)
BTN_14    = GPIO 14     (Modifier: modes 7-12)
BTN_15    = GPIO 15     (Modifier: modes 13-18)
```

---

## Pattern Format

### 3-Bit LED Encoding

| Value | Binary | LED 1 | LED 2 | LED 3 |
|-------|--------|-------|-------|-------|
| 0 | 000 | OFF | OFF | OFF |
| 1 | 001 | ON | OFF | OFF |
| 2 | 010 | OFF | ON | OFF |
| 3 | 011 | ON | ON | OFF |
| 4 | 100 | OFF | OFF | ON |
| 5 | 101 | ON | OFF | ON |
| 6 | 110 | OFF | ON | ON |
| 7 | 111 | ON | ON | ON |

### CSV Format v4

**Structure:**
```
@RZ1CSV:4,slot,count,totalMs,state1:dur1,state2:dur2,...
```

**Example:**
```
@RZ1CSV:4,0,6,3000,1:500,0:500,2:300,0:200,4:800,0:700
```

**Breakdown:**
- `4` = Format version
- `0` = Slot number
- `6` = Segment count
- `3000` = Total duration (ms)
- `1:500` = LED 1 ON for 500ms
- `0:500` = All OFF for 500ms
- etc.

---

## Usage Guide

### TEST.ino - Recording

**Basic Workflow:**
1. Press **REC** → Status LED blinks
2. Press button combinations with timing
3. Press **REC** or **STOP** → Recording stops
4. Press **PLAY** → Pattern plays back
5. Press **DUMP** → Export to serial (115200 baud)
6. Press **CLR** → Clear pattern

**Recording Tips:**
- Hold buttons for desired duration
- System captures timing automatically
- Minimum 5ms, maximum 65 seconds per state
- Auto-stops after 30s inactivity or 90s total

### final_first.ino - Playback

**Mode Selection:**
- **BTN_9-13 alone:** Modes 2-6 (none = Mode 1)
- **BTN_14 + BTN_9-13:** Modes 8-12 (BTN_14 alone = Mode 7)
- **BTN_15 + BTN_9-13:** Modes 14-18 (BTN_15 alone = Mode 13)

**18 Pattern Modes:**
1. Sequential activation
2. Binary counting
3. Chase effect
4. Random combinations
5. Flashing all
6. LED 1+3 alternating
7. LED 2 blink
8. LED 1+2 blink
9. LED 2+3 blink
10. LED 1+3 blink
11. **Fast strobe** (50ms flash + 300ms pause)
12. All off
13. LED 1 blink
14. LED 3 blink
15. Complex sequence
16. Smooth wave
17. Fast alternating
18. **Breathing effect** (smooth transitions)

### Converting Patterns

**From DUMP:**
```
@RZ1CSV:4,0,6,3000,1:500,0:500,2:300,0:200,4:800,0:700
```

**To Arrays:**
```cpp
const uint8_t parts_mode19[] = {1,0,2,0,4,0};
const uint16_t durations_mode19[] = {500,500,300,200,800,700};
const size_t NUM_MODE19 = 6;

// Add to lookup tables in final_first.ino
```

---

## Memory & Performance

### Memory Usage

**TEST.ino (Current):**
```
Parts:      2KB (2000 × 1 byte)
Durations:  4KB (2000 × 2 bytes)
Total:      ~6KB (2.3% of 264KB SRAM)
```

**Expansion Possibilities:**
```
10 slots × 2000:   60KB (22.7%)
32 slots × 2000:   192KB (72.7%)
32 slots × 1500:   144KB (54.5%)
```

### Performance

**Recording:**
- Button debounce: 15ms
- Glitch filter: 300μs
- Duration resolution: 5ms
- Max recording: 90 seconds

**Playback:**
- Timing precision: ±5ms
- Mode switching: Instant
- Pattern looping: Seamless

---

## Advanced Features

### Duration Specifications
- **Min:** 5ms (rounded)
- **Max:** 65,535ms (~65s)
- **Precision:** ±5ms
- **Storage:** 2 bytes (uint16_t)

### Auto-Stop Features
- 30s inactivity timeout
- 90s maximum duration
- 2000 segments limit

### Debouncing
- Control buttons: 15ms
- Recording buttons: 300μs glitch filter
- Stable state detection

---

## Troubleshooting

**Pattern Not Recording:**
- Check button connections
- Verify STATUS_LED blinks when REC pressed
- Ensure buttons have pull-up resistors

**Pattern Not Playing:**
- Verify LEDs connected to correct pins
- Check pattern was recorded (use DUMP)
- Ensure STATUS_LED solid during playback

**Serial Export Issues:**
- Set baud rate to 115200
- Press DUMP after recording
- Look for lines starting with `@RZ1CSV:`

**Memory Issues:**
- Reduce MAX_PARTS if needed
- Each segment uses 3 bytes
- Monitor SRAM usage

---

## Future Enhancements

**Possible Additions:**
1. Playback speed control (0.5x, 1x, 2x)
2. Pattern chaining
3. Flash storage (save permanently)
4. SD card support (1000+ patterns)
5. USB pattern management
6. OLED display
7. 8 LEDs / 8 buttons support
8. Real-time BPM sync

---

## Files Summary

**TEST/TEST.ino** - Pattern recorder with duration capture  
**final_first/final_first.ino** - 18-mode pattern player  
**COMPLETE_DOCUMENTATION.md** - This file  
**CODE_OVERVIEW.md** - Code structure details  
**RP2040_README.md** - RP2040-specific guide  
**DURATION_FEATURE.md** - Duration feature details  
**DURATION_UPGRADE_SUMMARY.md** - Upgrade summary

---

## Quick Reference

**3-Bit States:** 0=OFF, 1=LED1, 2=LED2, 3=LED1+2, 4=LED3, 5=LED1+3, 6=LED2+3, 7=ALL  
**CSV Format:** `@RZ1CSV:4,slot,count,totalMs,state:dur,state:dur,...`  
**Memory:** 6KB (2.3% of 264KB SRAM)  
**Capacity:** 2000 segments, expandable to 32 slots  
**Serial:** 115200 baud  
**Platform:** RP2040 Zero (264KB SRAM, 133MHz)

---

**Project Status:** ✅ Complete and fully functional  
**Version:** 2.0 (RP2040 with duration support)  
**Last Updated:** October 25, 2025
