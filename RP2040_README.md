# Iraq Flasher - RP2040 Version

## Overview
This is the RP2040-optimized version of the Iraq Flasher pattern recorder/player. It takes advantage of the RP2040's 264KB SRAM to support much larger pattern storage.

## Current Configuration
- **Platform:** RP2040 Zero (or any RP2040-based board)
- **Memory:** 264KB SRAM available
- **Slots:** 1 slot (expandable to 32)
- **Pattern Segments:** 2000 per slot (10x Arduino capacity)
- **Memory Usage:** ~2KB (0.75% of available SRAM)

## Hardware Requirements

### Pin Assignments
- **LED Outputs:**
  - LED 1: GPIO 7
  - LED 2: GPIO 8
  - LED 3: GPIO 9

- **Recording Buttons:**
  - Button 1: GPIO 4
  - Button 2: GPIO 5
  - Button 3: GPIO 6

- **Control Buttons:**
  - REC: GPIO 10 (Start/stop recording)
  - PLAY: GPIO 11 (Play recorded pattern)
  - CLR: GPIO 12 (Clear slot)
  - STOP: GPIO 3 (Stop recording/playback)
  - DUMP: GPIO 29 (Export pattern via serial)

- **Status LED:** GPIO 5 (blinks during record, solid during play)

- **Toggle Switches (for future expansion):**
  - TGL1: GPIO 13
  - TGL2: GPIO 14

## Features

### Current Implementation
- Records 3-bit LED states (0-7) from 3-button combinations
- Single slot with 2000 pattern segments
- 90-second maximum recording duration
- Auto-stop after 30 seconds of inactivity
- CSV export via USB serial (115200 baud)
- Debounced button inputs with glitch filtering

### Pattern Format
Each pattern segment is a 3-bit value (0-7) representing LED states:
```
0 = 000 = All LEDs OFF
1 = 001 = LED 1 ON
2 = 010 = LED 2 ON
3 = 011 = LED 1 + LED 2 ON
4 = 100 = LED 3 ON
5 = 101 = LED 1 + LED 3 ON
6 = 110 = LED 2 + LED 3 ON
7 = 111 = All LEDs ON
```

### CSV Export Format
```
@RZ1CSV:3,slot,count,totalMs,ledState1,ledState2,...
```
Example:
```
@RZ1CSV:3,0,24,2400,1,0,2,0,4,0,3,0,5,0,6,0,7,0
```

## Usage

### Recording a Pattern
1. Press **REC** button to start recording
2. Press any combination of **BTN_1**, **BTN_2**, **BTN_3** to create patterns
3. Each button combination sets which LEDs should be ON
4. Press **REC** again or **STOP** to end recording
5. Recording auto-stops after 30 seconds of inactivity

### Playing Back a Pattern
1. Press **PLAY** button to start playback
2. Pattern plays in a loop
3. Press **STOP** to end playback

### Exporting Pattern Data
1. Press **DUMP** button
2. Open serial monitor at 115200 baud
3. Copy the CSV data for use in other applications

### Clearing a Pattern
1. Press **CLR** button to clear the current slot

## Expansion Possibilities

The RP2040's memory allows for significant expansion:

### Multi-Slot Mode (32 slots)
```cpp
const uint8_t NUM_SLOTS = 32;        // 32 pattern slots
const uint16_t MAX_PARTS = 2000;     // 2000 segments per slot
// Total memory: 32 × 2000 = 64KB (24% of SRAM)
```

### Maximum Capacity Mode
```cpp
const uint8_t NUM_SLOTS = 32;        // 32 pattern slots
const uint16_t MAX_PARTS = 5000;     // 5000 segments per slot
// Total memory: 32 × 5000 = 160KB (60% of SRAM)
```

### Enhanced Features (Future)
- **8 LEDs / 8 Buttons:** Support 8-bit patterns (0-255)
- **Variable Timing:** Store duration for each segment
- **Pattern Chaining:** Link multiple patterns together
- **Flash Storage:** Save patterns permanently to 16MB flash
- **USB Interface:** Pattern management via USB
- **SD Card:** Store 1000+ patterns externally
- **Display:** OLED/LCD for pattern visualization

## Compilation

### Arduino IDE
1. Install **Arduino-Pico** board support
2. Select **Raspberry Pi Pico** or **Waveshare RP2040-Zero**
3. Upload the sketch

### PlatformIO
```ini
[env:rp2040]
platform = raspberrypi
board = waveshare_rp2040_zero
framework = arduino
```

## Memory Usage

Current configuration:
- **Pattern Storage:** 2KB (1 slot × 2000 segments × 1 byte)
- **Metadata:** ~100 bytes
- **Total:** ~2.1KB (0.8% of 264KB SRAM)

With 32 slots:
- **Pattern Storage:** 64KB (32 slots × 2000 segments × 1 byte)
- **Total:** ~64KB (24% of 264KB SRAM)

Maximum configuration:
- **Pattern Storage:** 160KB (32 slots × 5000 segments × 1 byte)
- **Total:** ~160KB (60% of 264KB SRAM)

## Differences from Arduino Version

| Feature | Arduino UNO | RP2040 Zero |
|---------|-------------|-------------|
| SRAM | 2KB | 264KB |
| Slots | 4 | 1 (expandable to 32+) |
| Segments/Slot | 200 | 2000 (expandable to 5000+) |
| Total Capacity | 800 bytes | 2KB (expandable to 160KB) |
| USB | Via UART | Native USB |
| Clock Speed | 16 MHz | 133 MHz |

## Troubleshooting

### Pattern Not Recording
- Check that buttons are properly connected with pull-up resistors
- Verify button pins match your hardware configuration
- Ensure STATUS_LED blinks when recording starts

### Pattern Not Playing
- Verify LEDs are connected to correct GPIO pins
- Check that pattern was successfully recorded (use DUMP to verify)
- Ensure STATUS_LED is solid during playback

### Serial Export Not Working
- Open serial monitor at 115200 baud
- Press DUMP button after recording
- Look for lines starting with `@RZ1CSV:`

## License
Open source - modify and use as needed for your projects.
