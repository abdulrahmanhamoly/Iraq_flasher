# 3-LED Iraq Flasher - Code Overview

## Project Structure

This project contains two complementary Arduino sketches for 3-LED pattern recording and playback:

### 1. TEST/TEST.ino - 3-LED Pattern Recorder/Player (RP2040 Version)
**Purpose:** Records 3-button patterns that control 3 LEDs and plays them back
**Platform:** RP2040 Zero (264KB SRAM)

**Key Features:**
- Records 3-bit LED states (0-7) from 3 button combinations
- Each LED state represents which of the 3 LEDs should be ON
- Bit 0 = LED 1, Bit 1 = LED 2, Bit 2 = LED 3
- Single slot with 2000 pattern segments (10x Arduino capacity)
- Maximum recording duration: 90 seconds per slot
- Auto-stop after 30 seconds of inactivity
- CSV export via serial for 3-bit pattern data
- Debounced button inputs with glitch filtering
- **Memory Usage:** ~2KB (0.8% of 264KB SRAM)
- **Expandable to:** 32 slots × 5000 segments = 160KB

**Hardware Requirements:**
- **LED Outputs:** LED_OUT_PIN_1 (7), LED_OUT_PIN_2 (8), LED_OUT_PIN_3 (9)
- **Recording Buttons:** BTN_1_PIN (4), BTN_2_PIN (5), BTN_3_PIN (6)
- **Control Buttons:**
  - REC_PIN (10): Start/stop recording
  - PLAY_PIN (11): Play recorded pattern
  - CLR_PIN (12): Clear slot
  - STOP_PIN (3): Stop recording/playback
  - DUMP_PIN (29): Export pattern to serial
- **Slot Selection:** Not used in single-slot mode (kept for future expansion)
- **Status LED:** STATUS_LED (5): Blinks during record, solid during play

**Usage:**
1. Press REC to start recording (slot selection not needed in single-slot mode)
2. Press any combination of BTN_1, BTN_2, BTN_3 to create patterns
3. Each button combination sets which LEDs should be ON
4. Press REC again or STOP to end recording
5. Press PLAY to play back the recorded pattern
6. Press DUMP to export pattern data via serial (115200 baud)
7. Press CLR to clear the pattern

**Pattern Format:**
- Patterns are stored as arrays of 3-bit values (0-7)
- 0 = all LEDs OFF, 1 = LED 1 ON, 2 = LED 2 ON, 3 = LED 1+2 ON, etc.
- 7 = all LEDs ON
- CSV format: `@RZ1CSV:3,slot,count,totalMs,ledState1,ledState2,...`

**Hardware Notes:**
- **RP2040 optimized** - uses only 2KB of 264KB SRAM (0.8%)
- Single slot mode with 2000 segments (10x Arduino capacity)
- Expandable to 32 slots × 5000 segments = 160KB
- Toggle switches kept for future multi-slot expansion
- **All compilation errors resolved** - code compiles successfully

### 2. final_first/final_first.ino - 3-LED Pattern Player
**Purpose:** Plays back pre-programmed 3-LED patterns based on button selection

**Key Features:**
- 18 pre-programmed patterns (modes 1-18)
- Each pattern controls 3 LEDs independently using 3-bit states
- Hierarchical mode selection using 7 buttons
- BTN_14 and BTN_15 act as mode modifiers
- Debounced button inputs
- Status LED indicates active playback

**Hardware Requirements:**
- **LED Outputs:** LED_OUT_PIN_1 (7), LED_OUT_PIN_2 (8), LED_OUT_PIN_3 (9)
- **Status LED:** STATUS_LED (5): On during playback
- **Mode Selection:** BTN_9-15: 7 buttons for mode selection

**Mode Selection:**
- **BTN_9-13 alone:** Modes 2-6 (or mode 1 if none pressed)
- **BTN_14 + BTN_9-13:** Modes 8-12 (or mode 7 if BTN_14 alone)
- **BTN_15 + BTN_9-13:** Modes 14-18 (or mode 13 if BTN_15 alone)

**Pre-programmed Patterns:**
- **Mode 1:** Sequential LED activation (1,2,4,1+2,3,4,5,6,7,...)
- **Mode 2:** Binary counting (0,1,2,3,4,5,6,7,6,5,4,3,2,1,...)
- **Mode 3:** Chase effect (1,2,4, then 2,4,1, then 4,1,2,...)
- **Mode 4:** Random combinations (1,3,7,2,6,4,5,1,3,...)
- **Mode 5:** Flashing all LEDs (0,7,1,2,4,3,5,6,7,...)
- **Mode 6:** LED 1 and 3 alternating (1,4,5,1,4,5,...)
- **Mode 7:** LED 2 only (2,0,2,0,2,0,...)
- **Mode 8:** LED 1 and 2 (3,0,3,0,3,0,...)
- **Mode 9:** LED 2 and 3 (6,0,6,0,6,0,...)
- **Mode 10:** LED 1 and 3 (5,0,5,0,5,0,...)
- **Mode 11:** All LEDs (7,0,7,0,7,0,...)
- **Mode 12:** No LEDs (0,0,0,0,0,0,...)
- **Mode 13:** LED 1 only (1,0,1,0,1,0,...)
- **Mode 14:** LED 3 only (4,0,4,0,4,0,...)
- **Mode 15:** Complex sequence (7,1,3,5,6,2,4,7,...)
- **Mode 16:** Complex counting (1,2,3,4,5,6,7,7,6,5,4,3,2,1,...)
- **Mode 17:** Fast alternating (1,2,4,3,5,7,2,4,6,1,3,5,4,6,2,1,7,...)
- **Mode 18:** All off (0,0,0,0,0,0,...)

**Usage:**
1. Press button combination to select mode
2. Pattern plays automatically in a loop
3. Press different button combination to change mode
4. Each LED turns on/off according to the 3-bit pattern data

## Relationship Between Files

The two sketches are complementary:
- **TEST.ino** is used to **record** new patterns from 3-button combinations
- **final_first.ino** is used to **play back** pre-recorded patterns

The patterns in `final_first.ino` demonstrate various 3-LED combinations. Users can record their own patterns using `TEST.ino` and export them via the DUMP function to create new pattern data.

## 3-Bit LED State Format

Both sketches use 3-bit values (0-7) to represent LED states:
```cpp
0 = 000 = All LEDs OFF
1 = 001 = LED 1 ON
2 = 010 = LED 2 ON
3 = 011 = LED 1 + LED 2 ON
4 = 100 = LED 3 ON
5 = 101 = LED 1 + LED 3 ON
6 = 110 = LED 2 + LED 3 ON
7 = 111 = All LEDs ON
```

**CSV Export Format (from TEST.ino):**
```
@RZ1CSV:3,slot,count,totalMs,ledState1,ledState2,...
```
Example:
```
@RZ1CSV:3,0,24,2400,1,0,2,0,4,0,3,0,5,0,6,0,7,0,6,0,5,0,4,0,3,0
```

## Code Organization

Both files follow a consistent structure:
1. **File header** - Description and features
2. **Pin definitions** - All hardware pin assignments
3. **Configuration constants** - Timing and limits
4. **Data structures** - Pattern storage and state variables
5. **Helper functions** - Utility functions
6. **Core logic** - Recording/playback state machines
7. **Arduino setup()** - Pin configuration and initialization
8. **Arduino loop()** - Main execution loop

## Integration Testing

To test the integration:
1. Upload TEST.ino and record patterns using the 3 buttons
2. Use DUMP function to export patterns via serial
3. Convert the CSV data to array format for final_first.ino
4. Upload final_first.ino to test pattern playback
5. Verify all 3 LEDs respond correctly to the pattern data
