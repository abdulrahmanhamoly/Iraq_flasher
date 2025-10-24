# Iraq Flasher - Code Overview

## Project Structure

This project contains two complementary Arduino sketches for LED pattern recording and playback:

### 1. TEST/TEST.ino - Pattern Recorder/Player
**Purpose:** Records button press patterns and plays them back

**Key Features:**
- Records timing patterns from a single button (BTN_PIN)
- Stores up to 32 slots with 1300 pattern segments each
- Maximum recording duration: 90 seconds per slot
- Auto-stop after 30 seconds of inactivity
- CSV export via serial for pattern data
- Debounced button inputs with glitch filtering

**Hardware Requirements:**
- BTN_PIN (4): Button to record pattern from
- LED_OUT_PIN (7): LED output for playback
- REC_PIN (10): Start/stop recording
- PLAY_PIN (11): Play recorded pattern
- CLR_PIN (12): Clear slot
- STOP_PIN (3): Stop recording/playback
- DUMP_PIN (29): Export pattern to serial
- TGL_PINS (13,14,15,26,27): 5-bit slot selection (32 slots)
- STATUS_LED (5): Status indicator (blinks during record, solid during play)

**Usage:**
1. Select slot using toggle switches (TGL_PINS)
2. Press REC to start recording
3. Press BTN_PIN to create pattern (timing between presses is recorded)
4. Press REC again or STOP to end recording
5. Press PLAY to play back the recorded pattern
6. Press DUMP to export pattern data via serial (115200 baud)

**Pattern Format:**
- Patterns are stored as arrays of durations (milliseconds)
- Even indices = button pressed (LED ON)
- Odd indices = button released (LED OFF)
- Durations are rounded to nearest 5ms

### 2. final_first/final_first.ino - Pattern Player
**Purpose:** Plays pre-programmed LED patterns based on button selection

**Key Features:**
- 18 pre-programmed patterns (modes 1-18)
- Hierarchical mode selection using 7 buttons
- BTN_14 and BTN_15 act as mode modifiers
- Debounced button inputs
- Status LED indicates active playback

**Hardware Requirements:**
- LED_OUT_PIN (7): LED output for pattern playback
- STATUS_LED (5): Status indicator (on during playback)
- BTN_9-15: Mode selection buttons

**Mode Selection:**
- **BTN_9-13 alone:** Modes 2-6 (or mode 1 if none pressed)
- **BTN_14 + BTN_9-13:** Modes 8-12 (or mode 7 if BTN_14 alone)
- **BTN_15 + BTN_9-13:** Modes 14-18 (or mode 13 if BTN_15 alone)

**Usage:**
1. Press button combination to select mode
2. Pattern plays automatically in a loop
3. Press different button combination to change mode
4. LED turns on/off according to pattern timing

## Relationship Between Files

The two sketches are complementary:
- **TEST.ino** is used to **record** new patterns from real button presses
- **final_first.ino** is used to **play back** pre-recorded patterns

The patterns in `final_first.ino` (parts_mode1 through parts_mode18) were likely recorded using `TEST.ino` and then exported via the DUMP function, which outputs CSV data that can be converted into the array format used in `final_first.ino`.

## Pattern Data Format

Both sketches use the same pattern representation:
```cpp
const uint16_t pattern[] = {duration1, duration2, duration3, ...};
```
- Even indices (0, 2, 4, ...): LED ON duration in milliseconds
- Odd indices (1, 3, 5, ...): LED OFF duration in milliseconds

Example:
```cpp
{100, 50, 100, 50}  // ON 100ms, OFF 50ms, ON 100ms, OFF 50ms
```

## Code Organization

Both files now follow a consistent structure:
1. **File header** - Description and features
2. **Pin definitions** - All hardware pin assignments
3. **Configuration constants** - Timing and limits
4. **Data structures** - Pattern storage and state variables
5. **Helper functions** - Utility functions
6. **Core logic** - Recording/playback state machines
7. **Arduino setup()** - Pin configuration and initialization
8. **Arduino loop()** - Main execution loop

All code is properly commented with:
- Section headers using `// ====` dividers
- Function documentation with `/** ... */` blocks
- Inline comments explaining complex logic
- Clear variable names and formatting
