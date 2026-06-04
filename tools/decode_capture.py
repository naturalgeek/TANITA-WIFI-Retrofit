#!/usr/bin/env python3
"""
TANITA BC-601/603/613 SPI capture decoder.

This is the "protocol grabber" companion tool. The ESP32-S3 sniffer
(firmware/tanita_spi_sniffer) passively taps the handset <-> SD-PCB SPI bus
and streams raw, CS-framed transactions over USB serial. This script turns
that raw stream into:

  1. A human-readable, annotated transaction log.
  2. Reconstructed virtual files (SYSTEM.TXT, PROFn.CSV, DATAn.CSV ...).
  3. A generated Markdown protocol report (PROTOCOL_OBSERVED.md) built ONLY
     from what was actually observed -- the "accurate protocol documentation
     based on the grabbing".

It accepts two input formats so it can be validated against ground truth:

  * raw    -- the sniffer line format (see firmware), one CS transaction per
              line:   <seq>\\t<t_us>\\t<mosi hex>\\t|\\t<somi hex>
  * legacy -- the hand-annotated dumps already in protocol/*.txt, lines like
              ">>mosi: 04 12 00 E9"  /  "<<miso: 04 92 00 69"

Frame format (confirmed against protocol/tanita_full_spi_dump.txt):
    [len] [cmd] [payload...] [checksum]
    len      = total frame length INCLUDING the len byte and the checksum
    checksum = ~(sum of every byte except the checksum) & 0xFF
    requests have cmd MSB clear; responses have cmd MSB set (cmd | 0x80)

Usage:
    python3 tools/decode_capture.py CAPTURE.txt              # auto-detect format
    python3 tools/decode_capture.py CAPTURE.txt --format raw
    python3 tools/decode_capture.py CAPTURE.txt --md docs/PROTOCOL_OBSERVED.md
    python3 tools/decode_capture.py --selftest               # parse bundled dump
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass, field

# --------------------------------------------------------------------------
# Low-level frame handling
# --------------------------------------------------------------------------


def checksum(data: bytes) -> int:
    """Protocol checksum: bitwise NOT of the byte sum, low 8 bits."""
    return (~sum(data)) & 0xFF


def frame_is_valid(frame: bytes) -> bool:
    """A frame is valid when len matches and the checksum verifies."""
    if len(frame) < 3:
        return False
    if frame[0] != len(frame):
        return False
    return checksum(frame[:-1]) == frame[-1]


def trim_to_frame(line: bytes):
    """
    Given the raw bytes clocked on one line during a CS transaction, return the
    leading protocol frame if this line carried it, else None.

    The sniffer captures the whole CS window on both lines; the inactive line
    is idle (0x00 or 0xFF). The active line begins with a valid [len]...[cksum]
    frame, possibly followed by idle padding bytes we discard.
    """
    if not line:
        return None
    declared = line[0]
    if declared < 3 or declared > len(line):
        return None
    candidate = line[:declared]
    if frame_is_valid(candidate):
        return candidate
    return None


# --------------------------------------------------------------------------
# Semantic layer
# --------------------------------------------------------------------------

REQ_NAMES = {
    0x11: "STATUS_READY",
    0x12: "STATUS_1",
    0x13: "STATUS_2",
    0x14: "FILE_INIT",
    0x21: "OPEN_READ",
    0x22: "END_TRANSFER",
    0x23: "READ_CHUNK",
    0x24: "WRITE_CHUNK",
    0x27: "ALLOCATE",
    0x41: "OPEN_WRITE",
}

RESP_NAMES = {
    0x91: "ACK_READY",
    0x92: "ACK_STATUS_1",
    0x93: "ACK_STATUS_2",
    0x94: "ACK_FILE_INIT",
    0xA1: "FILE_INFO",
    0xA2: "ACK_END",
    0xA3: "READ_DATA",
    0xA4: "ACK_WRITE",
    0xA7: "ACK_ALLOCATE",
    0xC1: "ACK_OPEN_WRITE",
}


def le32(b: bytes, off: int) -> int:
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)


def ascii_run(b: bytes) -> str:
    out = []
    for c in b:
        if 0x20 <= c <= 0x7E:
            out.append(chr(c))
        else:
            break
    return "".join(out)


@dataclass
class Frame:
    seq: int
    t_us: int
    direction: str  # "REQ" or "RESP"
    raw: bytes
    name: str = "?"
    detail: str = ""


def classify(frame: bytes) -> tuple[str, str, str]:
    """Return (direction, name, human detail) for a validated frame."""
    cmd = frame[1]
    body = frame[2:-1]  # payload between cmd and checksum

    if cmd & 0x80:  # response
        name = RESP_NAMES.get(cmd, f"RESP_0x{cmd:02X}")
        if cmd == 0xA1:  # file info: size LE32 at body[6:10]
            size = le32(frame, 8) if len(frame) >= 13 else -1
            return "RESP", name, f"size={size}"
        if cmd == 0xA3:  # read data: offset LE32 at body[2:6], then data
            offset = le32(frame, 4)
            data = frame[8:-1]
            return "RESP", name, f"offset={offset} datalen={len(data)} text={ascii_run(data)!r}"
        if cmd in (0xA2, 0xA4, 0xA7):  # addr/offset LE32 at frame[4:8]
            addr = le32(frame, 4)
            label = {0xA2: "final_offset", 0xA4: "next_addr", 0xA7: "addr"}[cmd]
            return "RESP", name, f"{label}={addr}"
        return "RESP", name, ""

    # request
    name = REQ_NAMES.get(cmd, f"REQ_0x{cmd:02X}")
    if cmd in (0x21, 0x41):  # open file: ASCII path
        path = ascii_run(body)
        extra = body[len(path):]
        det = f"path={path!r}"
        if extra:
            det += f" mode={extra.hex(' ')}"
        return "REQ", name, det
    if cmd == 0x23:  # read chunk: requested size at body[0]
        return "REQ", name, f"req_size={body[0] if body else 0}"
    if cmd == 0x24:  # write chunk: ASCII/CSV payload
        return "REQ", name, f"datalen={len(body)} text={ascii_run(body)!r}"
    if cmd == 0x27:  # allocate
        return "REQ", name, f"params={body.hex(' ')}"
    if cmd == 0x14:  # file init
        return "REQ", name, f"params={body.hex(' ')}"
    return "REQ", name, ""


# --------------------------------------------------------------------------
# Input parsers -> list[Frame]
# --------------------------------------------------------------------------

_HEX = re.compile(r"[0-9A-Fa-f]{2}")


def _hexbytes(s: str) -> bytes:
    return bytes(int(x, 16) for x in _HEX.findall(s))


def parse_raw(text: str) -> list[Frame]:
    """
    Sniffer raw format, one CS transaction per line:
        <seq>\\t<t_us>\\t<mosi hex>\\t|\\t<somi hex>
    Whitespace-tolerant; '#' lines are comments.
    """
    frames: list[Frame] = []
    for ln in text.splitlines():
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        if "|" not in ln:
            continue
        left, right = ln.split("|", 1)
        cols = left.split(None, 2)
        if len(cols) < 3:
            continue
        try:
            seq = int(cols[0])
            t_us = int(cols[1])
        except ValueError:
            continue
        mosi = _hexbytes(cols[2])
        somi = _hexbytes(right)
        for line_bytes in (mosi, somi):
            fr = trim_to_frame(line_bytes)
            if fr:
                d, name, detail = classify(fr)
                frames.append(Frame(seq, t_us, d, fr, name, detail))
                break  # only one line is active per CS transaction
    return frames


def parse_legacy(text: str) -> list[Frame]:
    """Hand-annotated dump format: '>>mosi: ..' / '<<miso: ..' lines."""
    frames: list[Frame] = []
    seq = 0
    for ln in text.splitlines():
        m = re.match(r"\s*(>>mosi|<<miso)\s*:\s*(.*)", ln)
        if not m:
            continue
        raw = _hexbytes(m.group(2))
        if not raw:
            continue
        # legacy lines already contain exactly one frame (no idle padding)
        if not frame_is_valid(raw):
            # tolerate trailing junk by trimming to declared length
            fr = trim_to_frame(raw)
            if not fr:
                frames.append(Frame(seq, seq, "?", raw, "BAD_CHECKSUM",
                                    f"declared_len={raw[0] if raw else 0}"))
                seq += 1
                continue
            raw = fr
        d, name, detail = classify(raw)
        frames.append(Frame(seq, seq, d, raw, name, detail))
        seq += 1
    return frames


def detect_format(text: str) -> str:
    if re.search(r"(>>mosi|<<miso)\s*:", text):
        return "legacy"
    return "raw"


# --------------------------------------------------------------------------
# File reconstruction
# --------------------------------------------------------------------------


@dataclass
class VFile:
    path: str
    mode: str  # "read" or "write"
    chunks: dict = field(default_factory=dict)  # offset -> bytes (reads)
    wbuf: bytearray = field(default_factory=bytearray)  # writes
    size: int = -1

    def content(self) -> bytes:
        if self.mode == "write":
            return bytes(self.wbuf)
        out = bytearray()
        for off in sorted(self.chunks):
            out += self.chunks[off]
        return bytes(out)


def reconstruct(frames: list[Frame]) -> list[VFile]:
    files: list[VFile] = []
    cur: VFile | None = None
    last_read_off = 0
    for fr in frames:
        cmd = fr.raw[1]
        if cmd == 0x21:  # open read
            cur = VFile(path=ascii_run(fr.raw[2:-1]), mode="read")
            files.append(cur)
            last_read_off = 0
        elif cmd == 0x41:  # open / finalize write
            path = ascii_run(fr.raw[2:-1])
            if cur is None or cur.path != path or cur.mode != "write":
                cur = VFile(path=path, mode="write")
                files.append(cur)
        elif cmd == 0xA1 and cur:  # file info -> size
            cur.size = le32(fr.raw, 8) if len(fr.raw) >= 13 else -1
        elif cmd == 0xA3 and cur and cur.mode == "read":  # read data chunk
            end_off = le32(fr.raw, 4)
            data = fr.raw[8:-1]
            start = end_off - len(data)
            cur.chunks[start] = data
            last_read_off = end_off
        elif cmd == 0x24 and cur and cur.mode == "write":  # write data chunk
            cur.wbuf += fr.raw[2:-1]
    return files


# --------------------------------------------------------------------------
# Reporting
# --------------------------------------------------------------------------


def annotated_log(frames: list[Frame]) -> str:
    lines = []
    for fr in frames:
        arrow = ">>" if fr.direction == "REQ" else "<<"
        lines.append(
            f"[{fr.seq:>5}] {fr.t_us:>10}us {arrow} {fr.name:<15} "
            f"{fr.raw.hex(' ')}"
            + (f"   ; {fr.detail}" if fr.detail else "")
        )
    return "\n".join(lines)


def render_markdown(frames: list[Frame], files: list[VFile], source: str) -> str:
    from collections import Counter, OrderedDict

    counts = Counter((fr.direction, fr.raw[1], fr.name) for fr in frames)
    example = OrderedDict()
    for fr in frames:
        key = (fr.direction, fr.raw[1])
        if key not in example:
            example[key] = fr

    out = []
    out.append("# TANITA SPI Protocol — Observed\n")
    out.append(f"_Auto-generated by `tools/decode_capture.py` from `{source}`._\n")
    out.append(f"- Total framed transactions: **{len(frames)}**")
    bad = sum(1 for fr in frames if fr.name == "BAD_CHECKSUM")
    out.append(f"- Checksum failures: **{bad}**\n")

    out.append("## Observed opcodes\n")
    out.append("| Dir | Cmd | Name | Count | Example frame |")
    out.append("|-----|-----|------|------:|---------------|")
    for (direction, cmd, name), n in sorted(counts.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        ex = example.get((direction, cmd))
        exhex = ex.raw.hex(" ") if ex else ""
        out.append(f"| {direction} | `0x{cmd:02X}` | {name} | {n} | `{exhex}` |")
    out.append("")

    out.append("## Command details (first observed example)\n")
    for (direction, cmd), fr in example.items():
        if fr.name == "BAD_CHECKSUM":
            continue
        out.append(f"### {fr.name} (`0x{cmd:02X}`, {direction})")
        out.append(f"```\n{fr.raw.hex(' ')}\n```")
        if fr.detail:
            out.append(f"- {fr.detail}")
        out.append("")

    out.append("## Reconstructed virtual files\n")
    if not files:
        out.append("_None reconstructed._\n")
    for vf in files:
        content = vf.content()
        try:
            text = content.decode("ascii")
        except UnicodeDecodeError:
            text = content.decode("latin-1")
        out.append(f"### `{vf.path}`  ({vf.mode}, {len(content)} bytes"
                   + (f", declared size={vf.size}" if vf.size >= 0 else "") + ")")
        out.append("```\n" + text.rstrip("\n") + "\n```\n")

    return "\n".join(out)


# --------------------------------------------------------------------------
# Self-test against bundled ground truth
# --------------------------------------------------------------------------


def selftest() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    dump = os.path.join(here, "..", "protocol", "tanita_full_spi_dump.txt")
    dump = os.path.normpath(dump)
    if not os.path.exists(dump):
        print(f"SELFTEST: missing {dump}", file=sys.stderr)
        return 2
    with open(dump) as f:
        text = f.read()
    frames = parse_legacy(text)

    bad = [fr for fr in frames if fr.name == "BAD_CHECKSUM"]
    print(f"SELFTEST: parsed {len(frames)} frames from bundled dump")
    print(f"SELFTEST: checksum failures = {len(bad)}")
    for fr in bad:
        print("   BAD:", fr.raw.hex(" "), fr.detail)

    files = reconstruct(frames)
    ok = True
    expected_system = b"SD,TANITA,GRAPHV1\r\nDo not delete, modify, nor remove this system file by yourself!\r\n"
    for vf in files:
        print(f"SELFTEST: file {vf.path} ({vf.mode}) -> {len(vf.content())} bytes, "
              f"declared {vf.size}")
        if "SYSTEM.TXT" in vf.path:
            if vf.content() != expected_system:
                ok = False
                print("   MISMATCH SYSTEM.TXT:")
                print("   got:", vf.content())
            elif vf.size >= 0 and vf.size != len(vf.content()):
                ok = False
                print(f"   SIZE MISMATCH: declared {vf.size} != {len(vf.content())}")

    if bad:
        ok = False
    print("SELFTEST:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Decode a TANITA SPI capture.")
    ap.add_argument("capture", nargs="?", help="capture file (raw or legacy)")
    ap.add_argument("--format", choices=["auto", "raw", "legacy"], default="auto")
    ap.add_argument("--md", metavar="FILE", help="write Markdown protocol report")
    ap.add_argument("--quiet", action="store_true", help="suppress annotated log")
    ap.add_argument("--selftest", action="store_true",
                    help="parse bundled dump and verify the decoder")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()
    if not args.capture:
        ap.error("capture file required (or use --selftest)")

    with open(args.capture) as f:
        text = f.read()
    fmt = args.format if args.format != "auto" else detect_format(text)
    frames = parse_raw(text) if fmt == "raw" else parse_legacy(text)
    files = reconstruct(frames)

    if not args.quiet:
        print(annotated_log(frames))
        print()
        for vf in files:
            print(f"--- {vf.path} ({vf.mode}, {len(vf.content())} bytes) ---")
            sys.stdout.buffer.write(vf.content())
            print()

    if args.md:
        md = render_markdown(frames, files, os.path.basename(args.capture))
        with open(args.md, "w") as f:
            f.write(md)
        print(f"\nWrote {args.md}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
