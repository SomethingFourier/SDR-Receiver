#!/usr/bin/env python3
"""Receive and validate RP2350 UDP audio broadcast packets.

Packet format (little-endian):
  u32 magic ('AUD0' = 0x30445541)
  u8  version
  u8  channels
  u8  bytes_per_sample
  u8  flags (bit0: signed little-endian PCM)
  u32 sample_rate_hz
  u32 sequence
  u32 timestamp_us
  u16 frame_count
  u16 reserved
  payload: frame_count * channels * bytes_per_sample
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
import wave
from dataclasses import dataclass
from pathlib import Path

HEADER_FMT = "<IBBBBIIIHH"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
MAGIC = 0x30445541


@dataclass
class Header:
    magic: int
    version: int
    channels: int
    bytes_per_sample: int
    flags: int
    sample_rate_hz: int
    sequence: int
    timestamp_us: int
    frame_count: int
    reserved: int


class Stats:
    def __init__(self) -> None:
        self.start = time.monotonic()
        self.last = self.start
        self.packets = 0
        self.bytes = 0
        self.gaps = 0
        self.bad = 0
        self.expected_seq: int | None = None

    def note_packet(self, header: Header, packet_len: int) -> None:
        self.packets += 1
        self.bytes += packet_len
        if self.expected_seq is not None and header.sequence != self.expected_seq:
            self.gaps += 1
        self.expected_seq = (header.sequence + 1) & 0xFFFFFFFF

    def note_bad(self) -> None:
        self.bad += 1

    def maybe_print(self, interval_s: float) -> None:
        now = time.monotonic()
        if now - self.last < interval_s:
            return
        elapsed = now - self.start
        rate = self.packets / elapsed if elapsed > 0 else 0.0
        mbps = (self.bytes * 8.0) / elapsed / 1e6 if elapsed > 0 else 0.0
        print(
            f"stats: packets={self.packets} bad={self.bad} gaps={self.gaps} "
            f"rate={rate:.1f} pkt/s throughput={mbps:.3f} Mb/s"
        )
        self.last = now


def parse_header(data: bytes) -> Header | None:
    if len(data) < HEADER_SIZE:
        return None
    fields = struct.unpack_from(HEADER_FMT, data)
    hdr = Header(*fields)
    if hdr.magic != MAGIC:
        return None
    return hdr


def signed24_to_i32(sample3: bytes) -> int:
    raw = sample3[0] | (sample3[1] << 8) | (sample3[2] << 16)
    if raw & 0x800000:
        raw -= 1 << 24
    return raw


def dump_first_frame(payload: bytes, channels: int, bytes_per_sample: int) -> str:
    if channels < 1 or bytes_per_sample != 3 or len(payload) < channels * bytes_per_sample:
        return ""
    samples = []
    for ch in range(channels):
        off = ch * 3
        samples.append(signed24_to_i32(payload[off : off + 3]))
    return " first_frame=" + ",".join(str(s) for s in samples)


def main() -> int:
    parser = argparse.ArgumentParser(description="Receive RP2350 UDP audio packets")
    parser.add_argument("--bind", default="0.0.0.0", help="Bind address (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=4951, help="UDP port (default: 4951)")
    parser.add_argument("--stats-interval", type=float, default=1.0, help="Stats print interval in seconds")
    parser.add_argument(
        "--raw-out",
        type=Path,
        help="Optional output file for raw packed 24-bit payload bytes",
    )
    parser.add_argument(
        "--print-frames",
        action="store_true",
        help="Print first frame of each packet for quick sanity checking",
    )
    parser.add_argument(
        "--wav-out",
        type=Path,
        help="Optional output file for writing received audio to a WAV file",
    )
    args = parser.parse_args()

    out_fp = None
    if args.raw_out is not None:
        out_fp = args.raw_out.open("wb")
        
    wav_fp = None
    wav_header_set = False
    if args.wav_out is not None:
        wav_fp = wave.open(str(args.wav_out), "wb")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.bind, args.port))

    print(f"Listening on {args.bind}:{args.port}")
    stats = Stats()

    try:
        while True:
            packet, addr = sock.recvfrom(4096)
            hdr = parse_header(packet)
            if hdr is None:
                stats.note_bad()
                continue

            payload = packet[HEADER_SIZE:]
            expected = hdr.frame_count * hdr.channels * hdr.bytes_per_sample
            if len(payload) != expected:
                stats.note_bad()
                continue

            if out_fp is not None:
                out_fp.write(payload)
                
            if wav_fp is not None:
                if not wav_header_set:
                    wav_fp.setnchannels(hdr.channels)
                    wav_fp.setsampwidth(hdr.bytes_per_sample)
                    wav_fp.setframerate(hdr.sample_rate_hz)
                    wav_header_set = True
                wav_fp.writeframes(payload)

            stats.note_packet(hdr, len(packet))
            if args.print_frames:
                frame_info = dump_first_frame(payload, hdr.channels, hdr.bytes_per_sample)
                print(
                    f"seq={hdr.sequence} sr={hdr.sample_rate_hz} frames={hdr.frame_count}"
                    f" ch={hdr.channels} bps={hdr.bytes_per_sample}{frame_info}"
                )
            stats.maybe_print(args.stats_interval)
    except KeyboardInterrupt:
        print("\nStopped")
    finally:
        if out_fp is not None:
            out_fp.close()
        if wav_fp is not None:
            wav_fp.close()
        sock.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
