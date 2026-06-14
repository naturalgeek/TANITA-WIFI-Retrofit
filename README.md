# TANITA-WIFI-Retrofit
I'am aiming to build a retrofit PCB to add Wifi to the Tanita 601,603 and 613 Body Scales.

# Current status
In development

# Hot it might work
basically there is a microcontroller in the handset of the scale. This microcontroller communicates to another microcontroller on a separate PCB containting the SD Cardslot via SPI. This is not SPI-SD, the communication happens in a properitary format. This SD-Card PCB can probably be replaced by something like a ESP32 which handles the communication to the mainboard and sends data to some HTTPS endpoint instead of writing it to a memory card.

# Protocol grabber

To document the proprietary protocol from real traffic (instead of guessing),
this repo includes a **passive SPI bus grabber**: an ESP32-S3 high-impedance tap
that listens to the handset ↔ SD-PCB bus while the original SD PCB stays
connected, plus a host decoder that reconstructs files and verifies checksums.

| Piece | Path |
|-------|------|
| Passive sniffer firmware (ESP32-S3, dual SPI slave) | [`firmware/tanita_spi_sniffer/`](firmware/tanita_spi_sniffer/) |
| Capture decoder / protocol-doc generator | [`tools/decode_capture.py`](tools/decode_capture.py) |
| Tap schematic (KiCad 8) + wiring | [`hardware/`](hardware/) |
| Step-by-step capture workflow | [`docs/CAPTURE_WORKFLOW.md`](docs/CAPTURE_WORKFLOW.md) |
| Curated protocol spec | [`docs/PROTOCOL.md`](docs/PROTOCOL.md) |
| Auto-generated observed protocol | [`docs/PROTOCOL_OBSERVED.md`](docs/PROTOCOL_OBSERVED.md) |

Quick check that the decoder is sound (runs against the bundled capture):

```bash
python3 tools/decode_capture.py --selftest
```

> Note: the existing `firmware/tanita_spi_capture/` sketch is an **emulator**
> (it impersonates the SD card and fabricates replies) — useful for *replacing*
> the card, but the **grabber** above is the tool for *documenting* the protocol.
