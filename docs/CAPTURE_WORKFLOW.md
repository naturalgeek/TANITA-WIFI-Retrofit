# Capture Workflow — from bus tap to accurate protocol docs

End-to-end recipe for grabbing the TANITA SPI protocol and turning the raw
capture into verified documentation.

```
 ┌────────────┐   ribbon    ┌────────────┐
 │  handset   │◄───────────►│  SD-Card   │   (untouched, still works)
 │ (SPI master)│   CN1 tap  │   PCB      │
 └─────┬──────┘             └────────────┘
       │ high-Z parallel tap (SCK,CS,SIMO,SOMI,GND)
       ▼
 ┌────────────┐   USB 921600   ┌──────────┐   decode_capture.py   ┌────────────────────┐
 │  ESP32-S3  │───────────────►│  host PC │──────────────────────►│ PROTOCOL_OBSERVED.md│
 │ dual SPI   │  raw frames    │ capture  │  annotate + verify    │ + reconstructed     │
 │  slave     │                │  .txt    │  checksum + files     │   virtual files     │
 └────────────┘                └──────────┘                       └────────────────────┘
```

## 1. Wire the tap

Follow `hardware/README.md`. Confirm **SCK** and **CS** first (the sniffer can't
frame without them). A logic analyzer on all six CN1 wires for one measurement
cycle is the quickest way to identify them. **Common the grounds.**

## 2. Flash the grabber

```bash
cd firmware
pio run -e sniffer -t upload
```

## 3. Record real traffic

```bash
pio device monitor -e sniffer | tee capture.txt
```

Exercise every protocol path you want documented:

- **Power-on** → reads `SYSTEM.TXT`, `PROF*.CSV` (status pings + reads).
- **Complete a weigh-in** → writes `DATA*.CSV` (allocate + write chunks).
- **Add/edit a user profile** → profile writes.

Let it run through a full cycle, then stop the monitor (Ctrl-C).

## 4. Decode and verify

```bash
# annotated transaction log + reconstructed files to stdout
python3 tools/decode_capture.py capture.txt

# regenerate the observed-protocol document
python3 tools/decode_capture.py capture.txt --md docs/PROTOCOL_OBSERVED.md
```

The decoder:

- validates every frame's length + checksum (flags `BAD_CHECKSUM`),
- labels each opcode (`OPEN_READ`, `READ_DATA`, `WRITE_CHUNK`, …),
- reconstructs the virtual files and compares declared vs actual size,
- emits a Markdown report built **only** from observed frames.

A clean capture has **zero checksum failures** and reconstructs files whose byte
length matches the `FILE_INFO` declared size. If you see failures, suspect wrong
SPI mode, a swapped SIMO/SOMI, or missed transactions (raise `QUEUE_DEPTH`).

## 5. Validate the decoder itself

The decoder ships with a self-test against the known-good bundled dump:

```bash
python3 tools/decode_capture.py --selftest
```

Expected: 78 frames, 0 checksum failures, `SYSTEM.TXT`/`PROF1.CSV` reconstructed
byte-exact — proves the frame parser before you trust it on new captures.

## 6. Promote to canonical docs

`docs/PROTOCOL.md` is the curated, hand-maintained spec.
`docs/PROTOCOL_OBSERVED.md` is machine-generated ground truth from a capture.
When they disagree, **the capture wins** — update `PROTOCOL.md` to match, and
keep the capture that proves it under `protocol/`.
