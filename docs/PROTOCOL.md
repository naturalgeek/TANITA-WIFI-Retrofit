# TANITA BC-601/603/613 SPI Protocol Documentation

This document describes the proprietary SPI protocol used between the TANITA handset and the SD Card PCB.

## Overview

The handset communicates with the SD Card PCB using a custom SPI protocol (NOT standard SD card SPI). The ESP32 must act as an **SPI Slave** to receive commands from the handset (master).

## Physical Layer

- **Interface:** SPI (Serial Peripheral Interface)
- **Voltage:** 3.3V
- **Mode:** SPI Mode 0 (CPOL=0, CPHA=0) - to be confirmed
- **Byte Order:** MSB first
- **Clock Speed:** ~1-4 MHz (estimated)

## Frame Structure

All frames end with a checksum byte calculated as:
```
checksum = ~(sum of all other bytes) & 0xFF
```

Example:
- Frame: `04 12 00 E9`
- Sum: 0x04 + 0x12 + 0x00 = 0x16
- Checksum: ~0x16 & 0xFF = 0xE9 ✓

## Command Types

### 1. Status/Handshake Commands (Ping-Pong)

These occur at startup and before each file operation.

| MOSI (Command) | MISO (Response) | Description |
|----------------|-----------------|-------------|
| `04 12 00 E9` | `04 92 00 69` | Status check 1 |
| `04 13 00 E8` | `04 93 00 68` | Status check 2 |
| `04 11 00 EA` | `04 91 01 69` | Ready check (response has 01 = ready) |

**Pattern:** Response command byte = Request command byte | 0x80

### 2. File Init Command

Before any file operation:
```
MOSI: 07 14 21 3A [3 bytes param] [checksum]
MISO: 04 94 00 67
```

### 3. File Read Operations

#### Open File for Read
```
MOSI: [len] 21 [filepath ASCII] [checksum]
      len = 0x22 or 0x23 depending on path length

MISO: 0D A1 00 00 00 00 00 00 [filesize 4 bytes LE] [checksum]
```

#### Read Data Chunk
```
MOSI: 04 23 [chunk_size] [checksum]
      chunk_size is typically 0x20 (32 bytes)

MISO: [len] A3 00 00 [offset 4 bytes LE] [data bytes] [checksum]
```

#### End Read
```
MOSI: 04 22 00 D9
MISO: 09 A2 00 00 [final_offset 4 bytes LE] [checksum]
```

### 4. File Write Operations

#### Open File for Write
```
MOSI: [len] 21 [filepath ASCII] [checksum]
MISO: 0D A1 00 00 00 00 00 00 [current_size 4 bytes LE] [checksum]
```

#### Allocate Space
```
MOSI: 07 27 [address 2 bytes] 00 00 [checksum]
MISO: 09 A7 00 00 [address 4 bytes LE] [checksum]
```

#### Begin Write
```
MOSI: [len] 41 [filepath ASCII] [extra bytes] [checksum]
MISO: 04 C1 00 3A
```

#### Write Data Chunk
```
MOSI: [len] 24 [data bytes] [checksum]
      len = 0x23 for 32-byte chunks, 0x19 for smaller final chunk

MISO: 09 A4 00 00 [next_address 4 bytes LE] [checksum]
```

#### End Write
```
MOSI: 04 22 00 D9
```

## Virtual Filesystem

The protocol simulates an SD card filesystem:

```
TANITA/
└── GRAPHV1/
    ├── SYSTEM/
    │   ├── SYSTEM.TXT    (system config)
    │   └── PROF1.CSV     (user profile 1)
    │   └── PROF2.CSV     (user profile 2, etc.)
    └── DATA/
        └── DATA1.CSV     (measurement data for profile 1)
```

### SYSTEM.TXT Format
```
SD,TANITA,GRAPHV1
Do not delete, modify, nor remove this system file by yourself!
```

### PROFn.CSV Format (Profile Configuration)
```
{0,16,~1,2,~3,4,MO,"BC-601",DB,"31/08/1985",Bt,0,GE,1,Hm,173.0,AL,2,CS,F4
```

Fields:
- `MO` - Model name
- `DB` - Date of birth (DD/MM/YYYY)
- `Bt` - Body type (0=standard, 1=athlete)
- `GE` - Gender (1=male, 2=female)
- `Hm` - Height in cm
- `AL` - Activity level
- `CS` - Checksum (hex)

### DATAn.CSV Format (Measurement Data)
```
{0,16,~0,2,~1,2,~2,3,~3,4,MO,"BC-601",DT,"01/01/2009",Ti,"08:38:37",
Bt,0,GE,1,AG,23,Hm,173.0,AL,2,Wk,61.9,MI,20.7,FW,12.4,Fr,11.9,
Fl,12.9,FR,8.8,FL,10.5,FT,14.2,mW,51.5,mr,3.0,ml,2.9,mR,9.5,mL,9.0,
mT,27.1,bW,2.7,IF,1,rD,2848,rA,12,ww,63.3,CS,6E
```

#### Measurement Fields

| Field | Description | Unit |
|-------|-------------|------|
| MO | Model | string |
| DT | Date | DD/MM/YYYY |
| Ti | Time | HH:MM:SS |
| Bt | Body type | 0=standard, 1=athlete |
| GE | Gender | 1=male, 2=female |
| AG | Age | years |
| Hm | Height | cm |
| AL | Activity level | 1-5 |
| Wk | Weight | kg |
| MI | Muscle/BMI indicator | % |
| FW | Fat - Whole body | % |
| Fr | Fat - Right arm | % |
| Fl | Fat - Left arm | % |
| FR | Fat - Right leg | % |
| FL | Fat - Left leg | % |
| FT | Fat - Trunk | % |
| mW | Muscle - Whole body | kg |
| mr | Muscle - Right arm | kg |
| ml | Muscle - Left arm | kg |
| mR | Muscle - Right leg | kg |
| mL | Muscle - Left leg | kg |
| mT | Muscle - Trunk | kg |
| bW | Bone mass | kg |
| IF | Physique rating | 1-9 |
| rD | BMR (daily) | kcal |
| rA | Metabolic age | years |
| ww | Body water | % |
| CS | Checksum | hex |

## Communication Sequence

### Startup (Read Profile)
```
1. Ping-Pong handshake (3 exchanges)
2. File init command
3. Open SYSTEM.TXT, read contents
4. Ping-Pong handshake (2 exchanges)
5. File init command
6. Open PROF1.CSV, read contents
```

### After Measurement (Write Data)
```
1. Ping-Pong handshake (multiple exchanges)
2. File init command
3. Open DATA1.CSV for write
4. Allocate space for new data
5. Write file path confirmation
6. Write data in 32-byte chunks
7. End write transaction
```

## Implementation Notes

1. **ESP32 as SPI Slave:** The ESP32 must respond to commands from the handset within the SPI clock cycle. Use hardware SPI slave mode with DMA if possible.

2. **Timing:** Responses must be ready before the next SPI transaction. Pre-calculate expected responses.

3. **Checksum:** Always validate incoming checksums and calculate correct outgoing checksums.

4. **File Simulation:** The ESP32 must simulate the virtual filesystem, providing appropriate responses for file reads and accepting writes.

5. **Data Capture:** The goal is to capture the measurement data from write operations and send it via WiFi instead of storing to SD card.
