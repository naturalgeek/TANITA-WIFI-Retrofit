# tanita_spi_sniffer — passive SPI grabber (ESP32-S3)

Passively taps the handset ↔ SD-PCB SPI bus and streams every CS-framed
transaction over USB serial. **Listens only** — both ESP32 SPI peripherals run
as slaves with `miso_io_num = -1`, so nothing is ever driven onto the live bus.
The original SD PCB stays connected and the scale keeps working.

This is the tool to use for **documenting** the protocol. (The sibling
`tanita_spi_capture` sketch is an *emulator* that pretends to be the SD card and
fabricates replies — useful for replacement, wrong for reverse-engineering.)

## How it works

Each logical message in the TANITA protocol is its own CS pulse and is
meaningful in only one direction. To capture full duplex we run **two** SPI
slaves on the **same** SCK and CS:

| Peripheral | Listens on | Captures |
|-----------|------------|----------|
| SPI2 | `SIMO`/MOSI (GPIO11) | handset → SD |
| SPI3 | `SOMI`/MISO (GPIO13) | SD → handset |

A CS pulse ends both transactions at once, giving a byte-aligned MOSI/MISO pair.
The host decoder picks the active direction per pulse by checksum.

Pins are defined at the top of `tanita_spi_sniffer.ino` (defaults match
`hardware/README.md`). SPI mode 0 is assumed — confirm CPOL/CPHA with a scope if
you see garbage.

## Build / flash / capture

```bash
cd firmware
pio run -e sniffer -t upload          # build + flash the ESP32-S3
pio device monitor -e sniffer | tee capture.txt   # record at 921600 baud
```

Trigger traffic on the scale: power-on reads `SYSTEM.TXT` + profiles; completing
a measurement writes `DATAn.CSV`. Capture a full power-on **and** a weigh-in.

## Output format

One line per CS transaction (`#` lines are metadata):

```
<seq>\t<t_us>\t<MOSI hex bytes>\t|\t<SOMI hex bytes>
```

Example (idle line shows as 00s; decoder keeps the framed side):

```
0   1240   04 12 00 E9    |   00 00 00 00
1   1530   00 00 00 00    |   04 92 00 69
```

## Decode

```bash
python3 ../tools/decode_capture.py capture.txt              # annotated log + files
python3 ../tools/decode_capture.py capture.txt --md ../docs/PROTOCOL_OBSERVED.md
```

See `docs/CAPTURE_WORKFLOW.md` for the full procedure.
