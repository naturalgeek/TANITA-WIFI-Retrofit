# TANITA WiFi Retrofit - Project Plan

## Project Overview

**Goal:** Replace the SD Card PCB in TANITA BC-601/603/613 body scales with an ESP32 to send weight/body composition data via WiFi instead of storing to SD card.

**Repository:** https://github.com/naturalgeek/TANITA-WIFI-Retrofit

**Two tools, two jobs** (don't confuse them):
- **Grabber** (`firmware/tanita_spi_sniffer`) — passively taps the live bus to *document* the protocol from real traffic. The original SD PCB stays connected.
- **Emulator** (`firmware/tanita_spi_capture`, `firmware/tanita_wifi`) — *impersonates* the SD card and answers the handset. This is what eventually *replaces* the card and adds WiFi.

---

## Current Status (June 2026)

### Completed ✅

| Task | Status | Notes |
|------|--------|-------|
| Protocol reverse engineering | ✅ Done | SPI captures analyzed, frame format decoded |
| Protocol documentation | ✅ Done | `docs/PROTOCOL.md` — curated reference |
| Checksum algorithm | ✅ **Verified** | `~(sum of bytes) & 0xFF`; decoder self-test = 0 failures across 78 real frames |
| Command/response mapping | ✅ Done | All observed commands documented |
| Data format parsing | ✅ Done | CSV key-value format understood |
| **Passive SPI grabber firmware** | ✅ Done | `firmware/tanita_spi_sniffer/` — ESP32-S3 dual-SPI-slave, listens only |
| **Capture decoder / doc generator** | ✅ Done | `tools/decode_capture.py` — validates checksums, reconstructs files, emits `PROTOCOL_OBSERVED.md`; ships `--selftest` |
| **Observed-protocol doc (auto-gen)** | ✅ Done | `docs/PROTOCOL_OBSERVED.md` generated from the bundled capture |
| **Capture workflow** | ✅ Done | `docs/CAPTURE_WORKFLOW.md` — tap → flash → capture → decode |
| **KiCad schematic (tap)** | ✅ Done | `hardware/tanita_sniffer.kicad_sch` (+ generator) |
| **Breadboard wiring drawing** | ✅ Done | `hardware/tanita_sniffer_breadboard.svg` (+ generator) |
| Signal naming confirmed (silkscreen) | ✅ Done | `SIMO`=MOSI, `SOMI`=MISO, `SCK`, `CS`, `GND`, `VCC`; 2nd (SD-side) bus identified |
| ESP32 firmware (emulator) | ✅ Done | `firmware/tanita_spi_capture/` |
| ESP32 firmware (WiFi) | ✅ Done | `firmware/tanita_wifi/` |
| Wiring guide | ✅ Done | `docs/WIRING_GUIDE.md` with ASCII diagrams |
| Pinout investigation guide | ✅ Done | `docs/PINOUT_INVESTIGATION.md` |
| PlatformIO config | ✅ Done | `firmware/platformio.ini` — per-env source filters (`sniffer`/`capture`/`wifi`) |

### In Progress / Pending hardware 🔄

| Task | Status | Blocker |
|------|--------|---------|
| Confirm exact CN1 wire→pin order | 🔄 Pending | Multimeter + logic analyzer on the physical unit |
| Compile-test all firmware | 🔄 Pending | No ESP toolchain in the dev env yet (`pio run`) |
| Open KiCad schematic in KiCad | 🔄 Pending | Targets format `20231120` (KiCad 8); not yet opened |
| Run the grabber on real hardware | 🔄 Pending | Requires wired tap + a measurement cycle |
| Real-world testing | 🔄 Pending | Requires hardware setup |

### Not Started ⏳

| Task | Priority | Complexity |
|------|----------|------------|
| Verify SPI mode (CPOL/CPHA) on scope | High | Low |
| Capture real power-on + weigh-in with grabber | High | Low |
| Reconcile `PROTOCOL.md` vs captured `PROTOCOL_OBSERVED.md` | High | Low |
| Validate emulator against the live scale | High | Medium |
| WiFi endpoint integration | Medium | Low |
| Power consumption optimization | Low | Medium |
| Custom PCB design | Low | High |
| 3D printed enclosure | Low | Medium |

---

## Milestones

### Milestone 1: Bench bring-up & wire identification 🎯
**Target:** Know exactly which CN1 wire is which, and the bus's electrical params.

- [ ] Identify GND/VCC with multimeter; SCK/CS/SIMO/SOMI with logic analyzer
- [ ] Document wire-colour → signal mapping for your unit
- [ ] Measure clock frequency and confirm 3.3 V levels
- [ ] Confirm SPI mode (CPOL/CPHA) on a scope

### Milestone 2: Passive protocol grab & documentation 🎯  *(recommended first — non-invasive)*
**Target:** Ground-truth protocol docs generated from real traffic, SD PCB untouched.

- [ ] `pio run -e sniffer -t upload` (compile-test first)
- [ ] Wire the high-impedance tap (see `hardware/` schematic + breadboard SVG)
- [ ] Capture a full **power-on** (reads SYSTEM.TXT, PROF*.CSV)
- [ ] Capture a full **weigh-in** (writes DATA*.CSV)
- [ ] `decode_capture.py capture.txt --md docs/PROTOCOL_OBSERVED.md` → 0 checksum failures
- [ ] Reconcile `docs/PROTOCOL.md` with the captured `PROTOCOL_OBSERVED.md` (capture wins)

### Milestone 3: Emulator validation 🎯
**Target:** ESP32 impersonates the SD card; the scale completes a measurement.

- [ ] `pio run -e capture -t upload`
- [ ] Verify ping-pong handshake (0x12/0x92, 0x13/0x93, 0x11/0x91)
- [ ] Verify file-read responses (SYSTEM.TXT, PROF1.CSV)
- [ ] Verify file-write capture (DATA1.CSV)
- [ ] Scale completes measurement without errors

### Milestone 4: WiFi Integration 🎯
**Target:** Measurement data sent to a server.

- [ ] Configure WiFi credentials (`config.h`)
- [ ] Stand up an HTTPS endpoint and verify JSON payload
- [ ] Test data transmission with real measurements
- [ ] Handle connection failures gracefully

### Milestone 5: Production Ready 🎯
**Target:** Reliable, permanent installation.

- [ ] Long-term stability testing
- [ ] Power from TANITA (not USB)
- [ ] Design custom adapter PCB
- [ ] Create 3D printed enclosure
- [ ] Document final installation process

---

## Task Backlog (Linear Import Format)

### Epic: Hardware Setup

```
[HIGH] Identify TANITA connector pinout      [partially done]
- Signal NAMES confirmed from SD-PCB silkscreen (SIMO/SOMI/SCK/CS/GND/VCC)
- Still need: wire-colour -> pin-order mapping on the physical CN1
- Use multimeter (GND/VCC) + logic analyzer (SPI lines)
Labels: hardware, blocking
Estimate: 1h

[HIGH] Wire the passive tap
- Tap SCK/CS/SIMO/SOMI + common GND in parallel with the SD PCB
- Do NOT connect handset VCC; power ESP32 from USB
- See hardware/ schematic + breadboard SVG
Labels: hardware
Estimate: 1h
Depends: Identify connector pinout

[MEDIUM] Verify SPI electrical characteristics
- Measure clock frequency, confirm 3.3V levels, check CPOL/CPHA
Labels: hardware, investigation
Estimate: 1h
```

### Epic: Toolchain / CI

```
[HIGH] Compile-test all three sketches
- pio run -e sniffer / -e capture / -e wifi
- Firmware was authored without a local ESP toolchain; never built yet
Labels: firmware, blocking
Estimate: 1h

[LOW] Open hardware/tanita_sniffer.kicad_sch in KiCad 8
- Confirm it loads + ERC clean; regenerate via gen_schematic.py if needed
Labels: hardware, verification
Estimate: 0.5h
```

### Epic: Protocol Grab & Documentation

```
[HIGH] Capture real traffic with the grabber
- Flash tanita_spi_sniffer, record power-on + a full weigh-in at 921600 baud
- pio device monitor -e sniffer | tee capture.txt
Labels: firmware, protocol, testing
Depends: Wire the passive tap
Estimate: 1h

[HIGH] Decode + regenerate observed protocol doc
- decode_capture.py capture.txt --md docs/PROTOCOL_OBSERVED.md
- Expect 0 checksum failures; reconstructed files match declared sizes
Labels: tooling, protocol
Estimate: 0.5h

[MEDIUM] Reconcile curated vs observed protocol
- Diff PROTOCOL.md against PROTOCOL_OBSERVED.md; capture is source of truth
- Commit the capture under protocol/ as evidence
Labels: documentation, protocol
Estimate: 1h
```

### Epic: Emulator Validation

```
[HIGH] Validate handshake + file read/write against the live scale
- Upload tanita_spi_capture; scale must complete a measurement
- Verify handshake, SYSTEM.TXT/PROF1.CSV reads, DATA1.CSV write
Labels: firmware, protocol, testing
Depends: Capture real traffic with the grabber
Estimate: 3h

[MEDIUM] Handle edge cases
- Multiple profiles (PROF2-4), guest mode, error recovery
Labels: firmware, protocol
Estimate: 3h
```

### Epic: WiFi Integration

```
[MEDIUM] Configure WiFi connection
- Set credentials in config.h, test stability + reconnection
Labels: firmware, wifi
Estimate: 1h

[MEDIUM] Set up test endpoint
- Simple HTTPS server, accept JSON POST, log received data
Labels: backend, testing
Estimate: 2h

[MEDIUM] Test data transmission
- Verify JSON format + all measurement fields with real measurements
Labels: firmware, testing
Estimate: 2h

[LOW] Add data buffering
- Store measurements offline, retry failed sends, SPIFFS persistence
Labels: firmware, enhancement
Estimate: 4h
```

### Epic: Production Hardening

```
[LOW] Optimize power consumption
- Measure current draw, sleep modes, battery impact
Labels: firmware, optimization
Estimate: 4h

[LOW] Design adapter PCB
- Schematic -> layout -> prototype (start from hardware/ KiCad files)
Labels: hardware, pcb
Estimate: 8h

[LOW] Create enclosure
- Measure available space, design 3D model, print + test fit
Labels: hardware, mechanical
Estimate: 4h

[LOW] Write installation guide
- Step-by-step instructions, photos, troubleshooting
Labels: documentation
Estimate: 2h
```

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| SPI timing too fast for ESP32 slave | Medium | High | DMA + queued transactions (`QUEUE_DEPTH`); grabber confirms achievable rate |
| Wrong SPI mode → garbage capture | Medium | Medium | Confirm CPOL/CPHA on scope before trusting captures |
| Grabber misses back-to-back transactions | Low | Medium | Raise `QUEUE_DEPTH`; decoder flags checksum gaps |
| Protocol has undocumented commands | Low | Medium | Grabber + decoder surface unknown opcodes; capture more scenarios |
| Firmware untested on hardware (not yet built) | High | Medium | Compile-test, then bench-validate on a real unit |
| TANITA power insufficient for ESP32 | Medium | Medium | Use external/USB power |
| Different TANITA models differ | Low | High | Re-grab per model; protocol is data-driven |

---

## Technical Debt

1. **Firmware never compiled or hardware-tested** — authored without an ESP toolchain; build + bench-validate before trusting.
2. **KiCad schematic not opened in KiCad** — generated to format `20231120`; verify load + ERC.
3. **Breadboard wire colours are illustrative** — real CN1 colour order must be confirmed per unit.
4. **SPI Mode hardcoded** — Mode 0 assumed in both sniffer and emulator; may need configuration.
5. **No persistent storage (WiFi path)** — measurements lost if WiFi fails.
6. **Hardcoded profile data in emulator** — should read from the actual scale config.
7. **No OTA updates** — requires USB for firmware updates.

---

## Resources

- Curated protocol spec: `docs/PROTOCOL.md`
- Auto-generated observed protocol: `docs/PROTOCOL_OBSERVED.md`
- Capture workflow: `docs/CAPTURE_WORKFLOW.md`
- Wiring guide: `docs/WIRING_GUIDE.md`
- Pinout investigation: `docs/PINOUT_INVESTIGATION.md`
- Hardware (schematic + breadboard + BOM): `hardware/`
- Capture decoder: `tools/decode_capture.py` (`--selftest`)
- Firmware READMEs: `firmware/README.md`, `firmware/tanita_spi_sniffer/README.md`
- Hardware photos: `photos/`

---

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Bench bring-up + wire ID | 0.5-1 day | Logic analyzer, multimeter |
| Passive grab + doc reconciliation | 0.5-1 day | Wired tap, compiled sniffer |
| Emulator validation | 2-3 days | Captured protocol, working hardware |
| WiFi integration | 1 day | Validated emulator |
| Production hardening | 1-2 weeks | Optional |

**Minimum viable prototype:** ~1 week from hardware setup start.

---

_Last updated: 2026-06-14 — reflects the passive grabber + decoder + hardware tooling merged in PR #2._
