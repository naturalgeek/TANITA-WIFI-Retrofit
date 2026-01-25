# TANITA WiFi Retrofit - Wiring Guide

## Overview

This guide explains how to connect an ESP32 development board to replace the SD Card PCB in your TANITA BC-601/603/613 body scale.

```
┌─────────────────┐                      ┌─────────────────┐
│                 │    6-wire cable      │                 │
│  TANITA Scale   │◄────────────────────►│   ESP32 Board   │
│   (Handset)     │   (SPI + Power)      │                 │
│                 │                      │                 │
└─────────────────┘                      └─────────────────┘
        │                                        │
        │                                        │
   [Measurements]                         [WiFi to Server]
```

---

## Required Components

| Item | Quantity | Notes |
|------|----------|-------|
| ESP32 Dev Board | 1 | ESP32-WROOM-32 or similar |
| Dupont Wires (Female-Female) | 6 | For connecting to existing cable |
| Breadboard | 1 | Optional, for prototyping |
| USB Cable | 1 | For programming/power |

---

## The TANITA Connector Cable

The SD Card PCB connects to the handset via a 6-wire cable with a white JST-style connector.

### Cable Wire Colors (from photos)

```
┌───────────────────────────────────────┐
│         TANITA Cable Connector        │
│                                       │
│   ┌───┬───┬───┬───┬───┬───┐          │
│   │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │          │
│   └───┴───┴───┴───┴───┴───┘          │
│     │   │   │   │   │   │            │
│     │   │   │   │   │   └─ Wire 6    │
│     │   │   │   │   └───── Wire 5    │
│     │   │   │   └───────── Wire 4    │
│     │   │   └───────────── Wire 3    │
│     │   └───────────────── Wire 2    │
│     └───────────────────── Wire 1    │
│                                       │
│   Colors observed (verify yours!):    │
│   Orange, Yellow, Green, Blue,        │
│   Purple/Pink, Red                    │
└───────────────────────────────────────┘
```

### Identifying the Signals

**YOU MUST VERIFY THESE WITH A MULTIMETER/LOGIC ANALYZER!**

Typical SPI signal assignment (needs verification):

```
┌─────────┬──────────┬─────────────────────────────┐
│ Pin #   │ Signal   │ How to Identify             │
├─────────┼──────────┼─────────────────────────────┤
│ 1       │ VCC      │ 3.3V when powered on        │
│ 2       │ GND      │ Continuity to battery (-)   │
│ 3       │ MOSI     │ Data pulses FROM handset    │
│ 4       │ MISO     │ Data pulses TO handset      │
│ 5       │ SCLK     │ Regular clock signal        │
│ 6       │ CS       │ Goes LOW during transfers   │
└─────────┴──────────┴─────────────────────────────┘
```

---

## ESP32 Pinout Reference

### ESP32-WROOM-32 DevKit V1 (30-pin)

```
                    ┌───────────────────┐
                    │       USB         │
                    │      ┌───┐        │
              EN ──┤1    ─┤   ├─    30├── D23 (MOSI) ◄── TANITA MOSI
             VP  ──┤2     │   │     29├── D22
             VN  ──┤3     │ESP│     28├── TX0
            D34  ──┤4     │32 │     27├── RX0
            D35  ──┤5     │   │     26├── D21
            D32  ──┤6     │   │     25├── D19 (MISO) ──► TANITA MISO
            D33  ──┤7     └───┘     24├── D18 (SCLK) ◄── TANITA SCLK
            D25  ──┤8               23├── D5  (CS)   ◄── TANITA CS
            D26  ──┤9               22├── D17
            D27  ──┤10              21├── D16
            D14  ──┤11              20├── D4
            D12  ──┤12              19├── D2  (LED)
            D13  ──┤13              18├── D15
            GND  ──┤14  ◄── TANITA  17├── GND
            VIN  ──┤15      GND     16├── 3V3 ◄── TANITA VCC (if needed)
                    └───────────────────┘
```

### ESP32-WROOM-32 DevKit (38-pin)

```
                    ┌─────────────────────┐
                    │        USB          │
                    │       ┌───┐         │
            3V3  ──┤1     ─┤   ├─     38├── GND
             EN  ──┤2      │   │      37├── D23 (MOSI) ◄── TANITA MOSI
            VP   ──┤3      │ESP│      36├── D22
            VN   ──┤4      │32 │      35├── TX0
            D34  ──┤5      │   │      34├── RX0
            D35  ──┤6      │   │      33├── D21
            D32  ──┤7      └───┘      32├── D19 (MISO) ──► TANITA MISO
            D33  ──┤8                 31├── D18 (SCLK) ◄── TANITA SCLK
            D25  ──┤9                 30├── D5  (CS)   ◄── TANITA CS
            D26  ──┤10                29├── D17
            D27  ──┤11                28├── D16
            D14  ──┤12                27├── D4
            D12  ──┤13                26├── D2  (LED)
            GND  ──┤14                25├── D15
            D13  ──┤15                24├── GND
            D9   ──┤16                23├── D3
            D10  ──┤17                22├── D1
            D11  ──┤18                21├── CMD
            VIN  ──┤19                20├── CLK
                    └─────────────────────┘
```

---

## Wiring Diagram

### Connection Overview

```
┌────────────────────────────────────────────────────────────────────┐
│                                                                    │
│   TANITA SD PCB Connector              ESP32 DevKit                │
│   (Cable from Handset)                                             │
│                                                                    │
│   ┌─────────────┐                     ┌─────────────────┐          │
│   │             │                     │                 │          │
│   │  Pin 1: VCC ├────────────────────►│ 3V3 (optional)  │          │
│   │             │    (red wire?)      │                 │          │
│   │             │                     │                 │          │
│   │  Pin 2: GND ├────────────────────►│ GND             │          │
│   │             │    (black wire?)    │                 │          │
│   │             │                     │                 │          │
│   │  Pin 3: MOSI├────────────────────►│ GPIO23          │          │
│   │             │    (data to ESP)    │                 │          │
│   │             │                     │                 │          │
│   │  Pin 4: MISO│◄────────────────────┤ GPIO19          │          │
│   │             │    (data from ESP)  │                 │          │
│   │             │                     │                 │          │
│   │  Pin 5: SCLK├────────────────────►│ GPIO18          │          │
│   │             │    (clock)          │                 │          │
│   │             │                     │                 │          │
│   │  Pin 6: CS  ├────────────────────►│ GPIO5           │          │
│   │             │    (chip select)    │                 │          │
│   │             │                     │                 │          │
│   └─────────────┘                     └─────────────────┘          │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘

Signal Direction:
    ────► = Signal flows TO ESP32 (inputs)
    ◄──── = Signal flows FROM ESP32 (outputs)
```

### Breadboard Layout

```
        ┌──────────────────────────────────────────────────────────┐
        │                      BREADBOARD                          │
        │                                                          │
        │   1  5    10   15   20   25   30   35   40   45   50    │
        │   ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐    │
        │ A │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │    │
        │   ├──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┤    │
        │ B │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │    │
        │   ├──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┤    │
        │ C │▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│  │    │
        │   ├──┼──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┼──┼──┤    │
        │ D │  │░░░░░░░░░░░ ESP32 ░░░░░░░░░░░░░░░░░░░░│  │  │    │
        │   ├──┼──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┼──┼──┤    │
        │ E │▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│▓▓│  │    │
        │   └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘    │
        │                        │                                 │
        │            Wires to TANITA connector                     │
        │                        │                                 │
        │            ┌───────────┴───────────┐                     │
        │            │  ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐                   │
        │            │  │G│ │V│ │M│ │M│ │S│ │C│                   │
        │            │  │N│ │C│ │O│ │I│ │C│ │S│                   │
        │            │  │D│ │C│ │S│ │S│ │L│ │ │                   │
        │            │  └─┘ └─┘ │I│ │O│ │K│ └─┘                   │
        │            │          └─┘ └─┘ └─┘                        │
        │            └─────────────────────────────────────────────│
        │                                                          │
        └──────────────────────────────────────────────────────────┘

        ▓▓ = ESP32 pins
        Wire connections at specific pin positions
```

---

## Step-by-Step Wiring Instructions

### Step 1: Prepare the ESP32

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   1. If using a new ESP32, first test it:                   │
│      - Connect USB cable                                    │
│      - Upload blink sketch                                  │
│      - Verify LED blinks                                    │
│                                                             │
│   2. Note your ESP32's pin layout                           │
│      - Check if it's 30-pin or 38-pin variant               │
│      - Identify GPIO23, GPIO19, GPIO18, GPIO5, GND, 3V3     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Step 2: Identify TANITA Cable Signals

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   USE A MULTIMETER TO IDENTIFY EACH WIRE!                   │
│                                                             │
│   With scale POWERED OFF:                                   │
│   ┌─────────────────────────────────────────────────────┐   │
│   │ 1. Set multimeter to continuity mode                │   │
│   │ 2. Find GND: Test each wire against battery (-)     │   │
│   │ 3. Find VCC: Test against battery (+) or capacitors │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
│   With scale POWERED ON (careful!):                         │
│   ┌─────────────────────────────────────────────────────┐   │
│   │ 1. Set multimeter to DC voltage mode                │   │
│   │ 2. Measure each wire against GND                    │   │
│   │ 3. VCC should show ~3.3V constant                   │   │
│   │ 4. Other wires may show varying voltages            │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
│   Best method - LOGIC ANALYZER:                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │ 1. Connect logic analyzer to all signal wires       │   │
│   │ 2. Power on scale and trigger a measurement         │   │
│   │ 3. SCLK = regular clock pattern                     │   │
│   │ 4. CS = goes LOW during each transaction            │   │
│   │ 5. MOSI = data bursts (commands from handset)       │   │
│   │ 6. MISO = data bursts (responses to handset)        │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Step 3: Make the Connections

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   IMPORTANT: Double-check all connections before powering!  │
│                                                             │
│   Connection Table:                                         │
│   ┌─────────────────┬──────────────┬───────────────────┐   │
│   │  TANITA Wire    │  ESP32 Pin   │  Wire Color*      │   │
│   ├─────────────────┼──────────────┼───────────────────┤   │
│   │  GND            │  GND         │  (verify yours)   │   │
│   │  VCC (3.3V)     │  3V3**       │  (verify yours)   │   │
│   │  MOSI           │  GPIO23      │  (verify yours)   │   │
│   │  MISO           │  GPIO19      │  (verify yours)   │   │
│   │  SCLK           │  GPIO18      │  (verify yours)   │   │
│   │  CS             │  GPIO5       │  (verify yours)   │   │
│   └─────────────────┴──────────────┴───────────────────┘   │
│                                                             │
│   * Wire colors vary - always verify with multimeter!       │
│   ** VCC connection optional if ESP32 is USB powered        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Step 4: Power Options

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   OPTION A: USB Power (Recommended for testing)             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                                                     │   │
│   │   [TANITA]──GND──────────┬────[ESP32 GND]          │   │
│   │            MOSI──────────┼────[ESP32 GPIO23]       │   │
│   │            MISO──────────┼────[ESP32 GPIO19]       │   │
│   │            SCLK──────────┼────[ESP32 GPIO18]       │   │
│   │            CS────────────┼────[ESP32 GPIO5]        │   │
│   │            VCC           │                          │   │
│   │             │            │                          │   │
│   │           (not          [USB]                       │   │
│   │          connected)       │                         │   │
│   │                        [5V Power]                   │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
│   OPTION B: TANITA Power (Final installation)               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                                                     │   │
│   │   [TANITA]──GND──────────────[ESP32 GND]           │   │
│   │            VCC (3.3V)────────[ESP32 3V3]           │   │
│   │            MOSI──────────────[ESP32 GPIO23]        │   │
│   │            MISO──────────────[ESP32 GPIO19]        │   │
│   │            SCLK──────────────[ESP32 GPIO18]        │   │
│   │            CS────────────────[ESP32 GPIO5]         │   │
│   │                                                     │   │
│   │   Note: TANITA must provide enough current!         │   │
│   │   ESP32 needs ~250mA average, peaks to 500mA        │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Complete Wiring Schematic

```
                            ┌──────────────────────────────────┐
                            │         ESP32 DevKit             │
                            │                                  │
                            │    ┌────────────────────────┐    │
    ┌───────────┐           │    │                        │    │
    │  TANITA   │           │    │      ┌────────┐        │    │
    │  Handset  │           │    │      │ ESP32  │        │    │
    │           │           │    │      │ WROOM  │        │    │
    │   ┌───┐   │           │    │      └────────┘        │    │
    │   │MCU│   │           │    │                        │    │
    │   └─┬─┘   │           │    │  3V3  ●────────────────┼───►│ VCC (optional)
    │     │     │           │    │                        │    │
    │   ──┴──   │           │    │  GND  ●────────────────┼───►│ GND
    └─────┬─────┘           │    │                        │    │
          │                 │    │  G23  ●────────────────┼───►│ MOSI
          │                 │    │       (VSPI MOSI)      │    │
    ┌─────┴─────┐           │    │                        │    │
    │ 6-wire    │           │    │  G19  ●────────────────┼───►│ MISO
    │ cable     │           │    │       (VSPI MISO)      │    │
    │           │           │    │                        │    │
    │ VCC ──────┼───────────┼────┼──────────► (optional)  │    │
    │ GND ──────┼───────────┼────┼──► GND                 │    │
    │ MOSI ─────┼───────────┼────┼──► GPIO23              │    │
    │ MISO ─────┼───────────┼────┼──► GPIO19              │    │
    │ SCLK ─────┼───────────┼────┼──► GPIO18              │    │
    │ CS ───────┼───────────┼────┼──► GPIO5               │    │  G18  ●────────────────┼───►│ SCLK
    │           │           │    │       (VSPI CLK)       │    │
    └───────────┘           │    │                        │    │
                            │    │  G5   ●────────────────┼───►│ CS
                            │    │       (VSPI CS)        │    │
                            │    │                        │    │
                            │    │  G2   ● (onboard LED)  │    │
                            │    │                        │    │
                            │    │  USB ━━━━━━━━━━━━━━━━━ │    │
                            │    │    (for programming)   │    │
                            │    │                        │    │
                            │    └────────────────────────┘    │
                            │                                  │
                            └──────────────────────────────────┘
```

---

## Signal Verification Checklist

After wiring, verify each connection:

```
┌─────────────────────────────────────────────────────────────┐
│                     VERIFICATION CHECKLIST                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  □ GND connected: TANITA GND ─── ESP32 GND                  │
│    Test: Continuity between both GND points                 │
│                                                             │
│  □ MOSI connected: TANITA MOSI ─── ESP32 GPIO23             │
│    Test: With logic analyzer, see commands from handset     │
│                                                             │
│  □ MISO connected: TANITA MISO ─── ESP32 GPIO19             │
│    Test: ESP32 can send responses to handset                │
│                                                             │
│  □ SCLK connected: TANITA SCLK ─── ESP32 GPIO18             │
│    Test: See clock signal on logic analyzer                 │
│                                                             │
│  □ CS connected: TANITA CS ─── ESP32 GPIO5                  │
│    Test: Goes LOW during SPI transactions                   │
│                                                             │
│  □ Power: Either USB or TANITA VCC (not both!)              │
│    Test: ESP32 powers on, LED blinks                        │
│                                                             │
│  □ No shorts between adjacent wires                         │
│    Test: No continuity between signal wires                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Troubleshooting Wiring Issues

### Problem: No SPI Data Received

```
┌─────────────────────────────────────────────────────────────┐
│  Possible Causes:                                           │
│                                                             │
│  1. MOSI/MISO swapped                                       │
│     → Swap GPIO23 and GPIO19 connections                    │
│                                                             │
│  2. CS not connected or wrong pin                           │
│     → Verify CS goes to GPIO5                               │
│     → Check CS goes LOW during transactions                 │
│                                                             │
│  3. GND not connected                                       │
│     → Verify common ground between TANITA and ESP32         │
│                                                             │
│  4. SCLK not connected                                      │
│     → Verify clock signal reaches GPIO18                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Problem: Garbled Data

```
┌─────────────────────────────────────────────────────────────┐
│  Possible Causes:                                           │
│                                                             │
│  1. Wrong SPI mode                                          │
│     → Try different modes in code (0, 1, 2, 3)              │
│                                                             │
│  2. Signal integrity issues                                 │
│     → Use shorter wires                                     │
│     → Add 10K pull-up on CS line                            │
│                                                             │
│  3. Voltage level mismatch                                  │
│     → Verify TANITA uses 3.3V (not 5V)                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Problem: Scale Shows Error

```
┌─────────────────────────────────────────────────────────────┐
│  Possible Causes:                                           │
│                                                             │
│  1. ESP32 not responding fast enough                        │
│     → Check serial output for received commands             │
│     → Verify responses are being sent                       │
│                                                             │
│  2. Wrong response data                                     │
│     → Check protocol implementation                         │
│     → Verify checksum calculations                          │
│                                                             │
│  3. MISO not connected properly                             │
│     → ESP32 can receive but can't respond                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Physical Installation Notes

### Prototype Setup (for testing)

```
    ┌────────────────────────────────────────────────────┐
    │                                                    │
    │   ┌─────────┐        Dupont         ┌─────────┐   │
    │   │ TANITA  │◄─────  Wires  ───────►│ ESP32   │   │
    │   │ Cable   │        (loose)        │ on      │   │
    │   │Connector│                       │Breadboard   │
    │   └─────────┘                       └─────────┘   │
    │                                          │        │
    │                                        [USB]      │
    │                                          │        │
    │                                    [Computer]     │
    │                                                    │
    └────────────────────────────────────────────────────┘
```

### Final Installation (inside scale)

```
    ┌────────────────────────────────────────────────────┐
    │                     TANITA Scale Housing           │
    │                                                    │
    │   ┌─────────────────────────────────────────────┐ │
    │   │                                             │ │
    │   │   Original SD PCB location                  │ │
    │   │   ┌─────────────────────────────┐           │ │
    │   │   │                             │           │ │
    │   │   │   ESP32 Mini/Compact board  │           │ │
    │   │   │   (e.g., ESP32-C3 Mini)     │           │ │
    │   │   │                             │           │ │
    │   │   │   Soldered connections      │           │ │
    │   │   │   or custom PCB adapter     │           │ │
    │   │   │                             │           │ │
    │   │   └─────────────────────────────┘           │ │
    │   │                                             │ │
    │   └─────────────────────────────────────────────┘ │
    │                                                    │
    └────────────────────────────────────────────────────┘

    Note: For final installation, consider:
    - ESP32-C3 Mini for smaller footprint
    - Custom PCB adapter matching original connector
    - Proper wire strain relief
    - Insulation from other components
```

---

## Quick Reference Card

```
╔═══════════════════════════════════════════════════════════════╗
║           TANITA WiFi Retrofit - Quick Wiring Reference       ║
╠═══════════════════════════════════════════════════════════════╣
║                                                               ║
║   TANITA Signal    ──────────►    ESP32 GPIO                  ║
║   ───────────────────────────────────────────                 ║
║   GND              ──────────►    GND                         ║
║   VCC (3.3V)       ──────────►    3V3 (optional)              ║
║   MOSI (to ESP)    ──────────►    GPIO23                      ║
║   MISO (from ESP)  ◄──────────    GPIO19                      ║
║   SCLK (clock)     ──────────►    GPIO18                      ║
║   CS (chip sel)    ──────────►    GPIO5                       ║
║                                                               ║
║   ⚠️  Always verify pinout with multimeter first!             ║
║   ⚠️  TANITA uses 3.3V logic - do NOT use 5V!                 ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
```

---

## Next Steps

After wiring is complete:

1. Upload the `tanita_spi_capture` sketch
2. Open Serial Monitor at 115200 baud
3. Power on the TANITA scale
4. You should see SPI transaction logs
5. If you see recognizable data (commands, file paths), wiring is correct!
6. Then proceed to the full `tanita_wifi` firmware
