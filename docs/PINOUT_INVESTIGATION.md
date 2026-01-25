# Connector Pinout Investigation Guide

## The Connector

Based on the photos, the SD Card PCB has a 6-7 pin connector with colored wires going to the handset. You'll need to identify which wire corresponds to which SPI signal.

## Expected Signals

For SPI communication, we need:
| Signal | Description |
|--------|-------------|
| VCC | 3.3V Power (likely red or orange wire) |
| GND | Ground |
| MOSI | Master Out Slave In (data TO the SD PCB) |
| MISO | Master In Slave Out (data FROM the SD PCB) |
| SCLK | SPI Clock |
| CS | Chip Select (active low) |

## Investigation Methods

### Method 1: Visual Inspection

From the PCB top photo, I can see "VCC" labeled near the connector. Trace other signals by following PCB traces to:
- The main microcontroller
- The SD card slot pins (standard SD card pinout is documented)

### Method 2: Multimeter Continuity Test

With the board powered OFF:
1. Set multimeter to continuity mode
2. Find GND by testing for continuity to the SD card slot's GND pins
3. Find VCC by testing for continuity to bypass capacitors' positive side
4. SPI signals will go to the microcontroller

### Method 3: Logic Analyzer / Oscilloscope

With the board connected to the handset (powered on):
1. Connect logic analyzer probes to each wire
2. Trigger a measurement on the scale
3. Identify signals by their characteristics:
   - **SCLK**: Regular clock pulses
   - **MOSI**: Data changing on clock edges (commands from handset)
   - **MISO**: Data changing on clock edges (responses from SD PCB)
   - **CS**: Goes low during transactions

### Method 4: Compare with SD Card Pinout

Standard SD card SPI pinout (looking at card from front, contacts up):
```
Pin 1: CS (Chip Select)
Pin 2: MOSI (Data In)
Pin 3: GND
Pin 4: VCC (3.3V)
Pin 5: SCLK (Clock)
Pin 6: GND
Pin 7: MISO (Data Out)
```

However, remember the TANITA uses a **proprietary protocol**, not standard SD SPI. The microcontroller on the SD PCB translates between the proprietary protocol and actual SD card access.

## Wire Color Observations

From the photos, the visible wire colors are approximately:
- Red/Orange
- Yellow
- Green
- Blue
- Purple/Pink
- (possibly more)

Common conventions (may not apply):
- Red = VCC
- Black = GND
- Other colors = signals

## Recommended Approach

1. **Identify VCC and GND first** using multimeter
2. **Use logic analyzer** to capture actual SPI communication
3. Compare captured data with the protocol documentation
4. Once you identify MOSI (data from handset), you should see the known command sequences

## Safety Notes

- The TANITA operates at 3.3V - do NOT connect 5V signals
- Always verify power connections before connecting your ESP32
- Start with the capture sketch to verify wiring before using the full firmware

## ESP32 Default Pin Mapping

Once you identify the signals, connect them to these ESP32 pins:

| Signal | ESP32 GPIO | Notes |
|--------|------------|-------|
| VCC | 3.3V pin | Power from TANITA or external |
| GND | GND | Common ground |
| MOSI | GPIO23 | Can be changed in config.h |
| MISO | GPIO19 | Can be changed in config.h |
| SCLK | GPIO18 | Can be changed in config.h |
| CS | GPIO5 | Can be changed in config.h |

## Example Logic Analyzer Capture

When you capture SPI data, you should see patterns like:

**Ping-Pong Handshake:**
```
MOSI: 04 12 00 E9
MISO: 04 92 00 69
```

**File Path Request:**
```
MOSI: 23 21 54 41 4E 49 54 41 2F ...  (TANITA/...)
```

If your MOSI/MISO are swapped, swap your connections!
