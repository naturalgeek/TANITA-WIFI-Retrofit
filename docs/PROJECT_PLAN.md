# TANITA WiFi Retrofit - Project Plan

## Project Overview

**Goal:** Replace the SD Card PCB in TANITA BC-601/603/613 body scales with an ESP32 to send weight/body composition data via WiFi instead of storing to SD card.

**Repository:** https://github.com/naturalgeek/TANITA-WIFI-Retrofit

---

## Current Status (June 2026)

### Completed ✅

| Task | Status | Notes |
|------|--------|-------|
| Protocol reverse engineering | ✅ Done | SPI captures analyzed, frame format decoded |
| Protocol documentation | ✅ Done | `docs/PROTOCOL.md` - complete reference |
| Checksum algorithm | ✅ Done | `~(sum of bytes) & 0xFF` |
| Command/response mapping | ✅ Done | All commands documented |
| Data format parsing | ✅ Done | CSV key-value format understood |
| ESP32 firmware (capture) | ✅ Done | `firmware/tanita_spi_capture/` |
| ESP32 firmware (WiFi) | ✅ Done | `firmware/tanita_wifi/` |
| Wiring guide | ✅ Done | `docs/WIRING_GUIDE.md` with ASCII diagrams |
| Pinout investigation guide | ✅ Done | `docs/PINOUT_INVESTIGATION.md` |
| PlatformIO config | ✅ Done | `firmware/platformio.ini` |

### In Progress 🔄

| Task | Status | Blocker |
|------|--------|---------|
| Hardware wiring verification | 🔄 Pending | Need to identify actual TANITA connector pinout |
| Real-world testing | 🔄 Pending | Requires hardware setup |

### Not Started ⏳

| Task | Priority | Complexity |
|------|----------|------------|
| Verify SPI mode (CPOL/CPHA) | High | Low |
| Test with actual scale | High | Medium |
| Validate protocol responses | High | Medium |
| WiFi endpoint integration | Medium | Low |
| Power consumption optimization | Low | Medium |
| Custom PCB design | Low | High |
| 3D printed enclosure | Low | Medium |

---

## Milestones

### Milestone 1: Hardware Validation 🎯
**Target:** Verify ESP32 can communicate with TANITA handset

- [ ] Identify connector pinout with multimeter/logic analyzer
- [ ] Wire ESP32 to TANITA cable
- [ ] Upload `tanita_spi_capture` sketch
- [ ] Verify SPI transactions appear in serial monitor
- [ ] Confirm command/response patterns match documentation

### Milestone 2: Protocol Verification 🎯
**Target:** ESP32 successfully responds to all TANITA commands

- [ ] Test ping-pong handshake
- [ ] Test file read operations (SYSTEM.TXT, PROF1.CSV)
- [ ] Test file write operations (DATA1.CSV)
- [ ] Scale completes measurement without errors
- [ ] Capture and parse measurement data

### Milestone 3: WiFi Integration 🎯
**Target:** Measurement data sent to server

- [ ] Configure WiFi credentials
- [ ] Set up HTTPS endpoint
- [ ] Test data transmission
- [ ] Verify JSON payload format
- [ ] Handle connection failures gracefully

### Milestone 4: Production Ready 🎯
**Target:** Reliable, permanent installation

- [ ] Long-term stability testing
- [ ] Power from TANITA (not USB)
- [ ] Design custom adapter PCB
- [ ] Create 3D printed enclosure
- [ ] Document final installation process

---

## Task Backlog (Linear Import Format)

### Epic: Hardware Setup

```
[HIGH] Identify TANITA connector pinout
- Use multimeter to find VCC and GND
- Use logic analyzer to identify SPI signals
- Document wire color to signal mapping
Labels: hardware, blocking
Estimate: 2h

[HIGH] Wire ESP32 to TANITA
- Connect all 6 signals
- Verify no shorts
- Test power options (USB vs TANITA)
Labels: hardware
Estimate: 1h
Depends: Identify connector pinout

[MEDIUM] Verify SPI electrical characteristics
- Measure clock frequency
- Confirm 3.3V levels
- Check SPI mode (CPOL/CPHA)
Labels: hardware, investigation
Estimate: 1h
```

### Epic: Protocol Validation

```
[HIGH] Test SPI capture sketch
- Upload tanita_spi_capture.ino
- Monitor serial output
- Compare with expected protocol
Labels: firmware, testing
Estimate: 1h
Depends: Wire ESP32 to TANITA

[HIGH] Validate handshake sequence
- Verify 0x12/0x92, 0x13/0x93, 0x11/0x91 exchanges
- Check timing requirements
- Debug if handshake fails
Labels: firmware, protocol
Estimate: 2h

[HIGH] Test file read responses
- SYSTEM.TXT read sequence
- PROF1.CSV read sequence
- Verify checksum calculations
Labels: firmware, protocol
Estimate: 2h

[HIGH] Test measurement data capture
- Complete full measurement on scale
- Verify DATA1.CSV write capture
- Parse measurement values
Labels: firmware, protocol
Estimate: 2h

[MEDIUM] Handle edge cases
- Multiple profiles (PROF2, PROF3, PROF4)
- Guest mode measurements
- Error recovery
Labels: firmware, protocol
Estimate: 3h
```

### Epic: WiFi Integration

```
[MEDIUM] Configure WiFi connection
- Set credentials in config.h
- Test connection stability
- Handle reconnection
Labels: firmware, wifi
Estimate: 1h

[MEDIUM] Set up test endpoint
- Create simple HTTPS server
- Accept JSON POST
- Log received data
Labels: backend, testing
Estimate: 2h

[MEDIUM] Test data transmission
- Verify JSON format
- Check all measurement fields
- Test with real measurements
Labels: firmware, testing
Estimate: 2h

[LOW] Add data buffering
- Store measurements if offline
- Retry failed transmissions
- Persistent storage (SPIFFS)
Labels: firmware, enhancement
Estimate: 4h
```

### Epic: Production Hardening

```
[LOW] Optimize power consumption
- Measure current draw
- Implement sleep modes
- Test battery impact
Labels: firmware, optimization
Estimate: 4h

[LOW] Design adapter PCB
- Create schematic
- Design PCB layout
- Order prototype
Labels: hardware, pcb
Estimate: 8h

[LOW] Create enclosure
- Measure available space
- Design 3D model
- Print and test fit
Labels: hardware, mechanical
Estimate: 4h

[LOW] Write installation guide
- Step-by-step instructions
- Photos of installation
- Troubleshooting section
Labels: documentation
Estimate: 2h
```

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| SPI timing too fast for ESP32 | Medium | High | Use DMA, optimize ISR |
| Protocol has undocumented commands | Low | Medium | Capture more scenarios |
| TANITA power insufficient for ESP32 | Medium | Medium | Use external power |
| Different TANITA models have different protocols | Low | High | Test with multiple units |

---

## Technical Debt

1. **SPI Mode hardcoded** - Currently using Mode 0, may need configuration
2. **No persistent storage** - Measurements lost if WiFi fails
3. **Hardcoded profile data** - Should read from actual scale config
4. **No OTA updates** - Requires USB for firmware updates

---

## Resources

- Protocol documentation: `docs/PROTOCOL.md`
- Wiring guide: `docs/WIRING_GUIDE.md`
- Firmware README: `firmware/README.md`
- Hardware photos: `photos/`

---

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Hardware setup | 1-2 days | Logic analyzer, multimeter |
| Protocol validation | 2-3 days | Working hardware |
| WiFi integration | 1 day | Validated protocol |
| Production hardening | 1-2 weeks | Optional |

**Minimum viable prototype:** ~1 week from hardware setup start
