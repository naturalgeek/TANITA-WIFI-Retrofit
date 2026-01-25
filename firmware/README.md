# TANITA WiFi Retrofit - ESP32 Firmware

This firmware allows an ESP32 to replace the SD Card PCB in TANITA BC-601/603/613 body scales, enabling wireless transmission of measurement data.

## Hardware Requirements

- ESP32 development board (ESP32-WROOM-32 recommended)
- Level shifter if your ESP32 is 5V logic (the TANITA uses 3.3V)
- Connecting wires

## Wiring

Connect the ESP32 to the TANITA handset connector (the cable that normally goes to the SD Card PCB):

| TANITA Signal | ESP32 GPIO | Function |
|---------------|------------|----------|
| MOSI | GPIO23 | Data from handset to ESP32 |
| MISO | GPIO19 | Data from ESP32 to handset |
| SCLK | GPIO18 | SPI clock (from handset) |
| CS | GPIO5 | Chip select (active low) |
| VCC | 3.3V | Power |
| GND | GND | Ground |

**Important:** The TANITA handset is the SPI master; the ESP32 runs as an SPI slave.

## Sketches

### 1. tanita_spi_capture (Start here!)

Simple debug sketch for testing your wiring and capturing protocol data. This sketch:
- Captures all SPI transactions
- Logs everything to Serial (115200 baud)
- Responds to the TANITA protocol
- Displays captured measurement data

**Use this first** to verify your hardware connection works!

### 2. tanita_wifi

Full implementation with WiFi transmission. Features:
- Complete protocol implementation
- WiFi connectivity
- HTTPS POST of measurement data as JSON
- Measurement data parsing

## Building

### Using PlatformIO (Recommended)

```bash
cd firmware

# Build capture sketch
pio run -d tanita_spi_capture

# Upload capture sketch
pio run -d tanita_spi_capture --target upload

# Monitor serial output
pio device monitor -b 115200

# For the WiFi version:
pio run -d tanita_wifi
pio run -d tanita_wifi --target upload
```

### Using Arduino IDE

1. Install ESP32 board support
2. Open the `.ino` file
3. Select "ESP32 Dev Module" as board
4. Upload

## Configuration

Edit `tanita_wifi/config.h`:

```cpp
// WiFi settings
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"

// API endpoint
#define API_ENDPOINT      "https://your-server.com/api/weight"
#define API_KEY           "your-api-key-here"

// Pin mapping (if different from defaults)
#define PIN_SPI_MOSI      23
#define PIN_SPI_MISO      19
#define PIN_SPI_SCLK      18
#define PIN_SPI_CS        5
```

## Protocol Overview

The TANITA uses a proprietary SPI protocol (NOT standard SD card SPI):

### Frame Format
```
[length] [command] [payload...] [checksum]
```

- `length`: Total frame length in bytes
- `command`: Operation code
- `payload`: Variable length data
- `checksum`: `~(sum of all other bytes) & 0xFF`

### Key Commands

| Cmd | Description |
|-----|-------------|
| 0x11, 0x12, 0x13 | Status/handshake |
| 0x14 | File operation init |
| 0x21 | Read file path |
| 0x22 | End transfer |
| 0x23 | Request data chunk |
| 0x24 | Write data chunk |
| 0x27 | Allocate space |
| 0x41 | Write file path |

Responses have bit 7 set: `response = command | 0x80`

### Measurement Data Format

Data is CSV with key-value pairs:
```
MO,"BC-601",DT,"01/01/2024",Ti,"08:30:00",Wk,65.5,FW,18.2,...
```

Key fields:
- `Wk`: Weight (kg)
- `FW`: Body fat (%)
- `mW`: Muscle mass (kg)
- `bW`: Bone mass (kg)
- `ww`: Body water (%)
- `rD`: BMR (kcal)
- `rA`: Metabolic age

## SPI Slave Considerations

The ESP32 runs as an SPI slave, which has timing challenges:

1. **Response Timing**: The response must be ready in the TX buffer before the master starts the next transaction
2. **Full Duplex**: SPI is simultaneous bidirectional - the first response may contain garbage
3. **DMA Required**: Use DMA for reliable data transfer

The handset appears to use separate transactions for command and response, giving us time to prepare responses.

## Troubleshooting

### No data received
- Check wiring (especially MOSI, SCLK, CS)
- Verify 3.3V power
- Check that CS pin is correctly connected

### Garbled data
- Try different SPI modes (currently using Mode 0)
- Check clock polarity with oscilloscope/logic analyzer
- Verify ground connection

### Scale shows error
- The response timing may be off
- Check checksum calculations
- Verify file content responses

### WiFi connection fails
- Check SSID and password in config.h
- Ensure router is 2.4GHz (ESP32 doesn't support 5GHz)

## Data Flow

```
[TANITA Scale] --weighs--> [Handset MCU]
                               |
                          SPI (proprietary)
                               |
                               v
                          [ESP32 Slave]
                               |
                           WiFi/HTTPS
                               |
                               v
                        [Your Server]
```

## JSON Output Format

The WiFi sketch sends data as JSON:
```json
{
  "model": "BC-601",
  "date": "01/01/2024",
  "time": "08:30:00",
  "weight": 65.5,
  "bodyFat": 18.2,
  "muscleMass": 51.5,
  "boneMass": 2.7,
  "bodyWater": 55.3,
  "bmr": 1650,
  "metabolicAge": 28,
  "physiqueRating": 5,
  "fatSegments": {
    "rightArm": 15.2,
    "leftArm": 14.8,
    "rightLeg": 18.5,
    "leftLeg": 18.2,
    "trunk": 16.8
  },
  "muscleSegments": {
    "rightArm": 3.1,
    "leftArm": 3.0,
    "rightLeg": 9.5,
    "leftLeg": 9.3,
    "trunk": 27.5
  }
}
```

## License

MIT License - See repository root for details.
