# Hardware — Passive SPI Protocol Grabber

A high-impedance tap that lets an **ESP32-S3** listen to the SPI bus between the
TANITA handset and the original SD-Card PCB **without disturbing it**. The SD
PCB stays plugged in and the scale keeps working normally; the ESP32 only
listens and streams what it sees over USB.

## Signal map (from the SD-PCB silkscreen)

The bridge MCU on the SD PCB uses TI-style SPI names. Connector **CN1** carries
the handset link:

| CN1 signal | Meaning | = standard SPI | Direction |
|------------|---------|----------------|-----------|
| `SCK`  | serial clock          | SCLK | handset → SD (master drives) |
| `CS`   | chip select, active low | CS  | handset → SD |
| `SIMO` | Slave-In-Master-Out   | **MOSI** | handset → SD |
| `SOMI` | Slave-Out-Master-In   | **MISO** | SD → handset |
| `GND`  | ground                | GND  | — |
| `VCC`  | ~3.3 V rail           | —    | **do not tap** |

> The MCU has a *second* SPI bus to the on-board SD card (silkscreen `S.SIMO`,
> `S.SOMI`, `S.SCK`). We tap **CN1 (the handset side)**, not the card side.

⚠️ **The exact wire-colour → pin order on CN1 must be confirmed on your unit**
before trusting it — see "Identify the wires" below. The grabber self-verifies
once running (it will only see valid framed traffic when SCK/CS are correct).

## Connections

| TANITA CN1 | via | ESP32-S3 GPIO | Role |
|------------|-----|---------------|------|
| `SCK`  | R1 100 Ω | **GPIO12** | shared SPI clock (both peripherals) |
| `CS`   | R2 100 Ω | **GPIO10** | shared chip select (active low) |
| `SIMO` | R3 100 Ω | **GPIO11** | MOSI → SPI2 listens here |
| `SOMI` | R4 100 Ω | **GPIO13** | MISO → SPI3 listens here |
| `GND`  | —        | **GND**    | **required** common ground |
| `VCC`  | —        | *(leave unconnected)* | power ESP32 from USB instead |

`R1–R4` are optional ~100 Ω in-line series resistors (protection / impedance
isolation). 0 Ω / plain wire works too because the ESP32 pins are pure inputs
(`miso_io_num = -1`, so nothing is ever driven back onto the bus).

## ASCII schematic

```
 TANITA handset  <-------- ribbon -------->  SD-Card PCB (stays connected)
                         CN1 (tap here)
                         |
   CN1.SCK  o---[R1 100]---+----------------> ESP32-S3 GPIO12   (SCK,  shared)
   CN1.CS   o---[R2 100]---+----------------> ESP32-S3 GPIO10   (CS_n, shared)
   CN1.SIMO o---[R3 100]---+----------------> ESP32-S3 GPIO11   (MOSI -> SPI2)
   CN1.SOMI o---[R4 100]---+----------------> ESP32-S3 GPIO13   (MISO -> SPI3)
   CN1.GND  o-------------------------------- ESP32-S3 GND      (common)
   CN1.VCC  x  (DO NOT CONNECT)               ESP32-S3 5V <- USB
```

The two ESP32 SPI peripherals share SCK + CS and each watch one data line, so a
single CS pulse yields a byte-aligned MOSI/MISO pair. Neither peripheral drives
a pin, so the tap is invisible to the live bus.

## Bill of materials

| Ref | Part | Notes |
|-----|------|-------|
| U1 | ESP32-S3 DevKitC (or any dual-SPI ESP32-S3) | needs SPI2 **and** SPI3 free |
| J1 | wires/clip to CN1 (JST-PH 7-pin on the SD PCB) | tap in parallel |
| R1–R4 | 100 Ω 0603/through-hole | optional but recommended |

## Schematic files

- `tanita_sniffer.kicad_sch` — KiCad 8 schematic (open with `tanita_sniffer.kicad_pro`).
- `gen_schematic.py` — regenerates the `.kicad_sch`; the netlist lives in code,
  so edit pins/nets there and re-run `python3 hardware/gen_schematic.py`.

Targets KiCad 8 (file format `20231120`). If your KiCad version refuses to open
it, the wiring tables above are the authoritative netlist.

## Identify the wires (do this first)

A dual-slave sniffer must know which CN1 wire is **SCK** and which is **CS** to
frame transactions. To confirm on your unit:

1. **GND / VCC** — power off, multimeter continuity: GND rings to the SD-card
   shield and bypass-cap ground; VCC to the cap positive.
2. **SCK** — power on, scope/logic-analyzer: a steady burst clock during a
   measurement save or on power-up.
3. **CS** — idles high, pulses low around each clock burst.
4. **SIMO vs SOMI** — capture both data lines; the one carrying the known
   command `04 12 00 E9` (handset→SD ping) is **SIMO/MOSI**; the line replying
   `04 92 00 69` is **SOMI/MISO**. If reversed, swap GPIO11/GPIO13.

A cheap 8-channel logic analyzer + `sigrok`/PulseView on all six wires for one
measurement cycle is the fastest way to lock this down before soldering.

## Safety

- TANITA logic is **3.3 V** — never feed 5 V into these pins.
- **Common the grounds.** No shared GND = garbage capture and possible damage.
- Do **not** tap VCC; powering the ESP32 from USB while also backfeeding the
  handset rail can fight the scale's regulator.
