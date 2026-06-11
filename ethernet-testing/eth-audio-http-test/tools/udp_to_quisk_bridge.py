#!/usr/bin/env python3
"""Minimal UDP audio bridge for future Quisk integration.

Current behavior:
- Receives RP2350 UDP broadcast packets on one port.
- Validates and strips the custom packet header.
- Forwards raw packed 24-bit audio payload to localhost UDP output.

This keeps firmware simple while allowing host-side protocol adaptation.
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys

HEADER_FMT = "<IBBBBIIIHH"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
MAGIC = 0x30445541


def parse_packet(packet: bytes) -> tuple[bytes, int] | None:
    if len(packet) < HEADER_SIZE:
        return None
    (
        magic,
        _version,
        channels,
        bytes_per_sample,
        _flags,
        _sample_rate_hz,
        sequence,
        _timestamp_us,
        frame_count,
        _reserved,
    ) = struct.unpack_from(HEADER_FMT, packet)

    if magic != MAGIC:
        return None

    payload = packet[HEADER_SIZE:]
    expected = frame_count * channels * bytes_per_sample
    if len(payload) != expected:
        return None

    return payload, sequence


def main() -> int:
    parser = argparse.ArgumentParser(description="Bridge RP2350 UDP audio to localhost UDP")
    parser.add_argument("--in-bind", default="0.0.0.0", help="Input bind address")
    parser.add_argument("--in-port", type=int, default=4951, help="Input UDP port")
    parser.add_argument("--out-host", default="127.0.0.1", help="Output host")
    parser.add_argument("--out-port", type=int, default=4952, help="Output UDP port")
    args = parser.parse_args()

    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    rx.bind((args.in_bind, args.in_port))

    tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    print(
        f"Bridge: listening {args.in_bind}:{args.in_port} -> "
        f"{args.out_host}:{args.out_port}"
    )

    last_seq = None
    dropped = 0
    forwarded = 0

    try:
        while True:
            packet, _addr = rx.recvfrom(4096)
            parsed = parse_packet(packet)
            if parsed is None:
                continue
            payload, seq = parsed

            if last_seq is not None and seq != ((last_seq + 1) & 0xFFFFFFFF):
                dropped += 1
            last_seq = seq

            tx.sendto(payload, (args.out_host, args.out_port))
            forwarded += 1

            if forwarded % 200 == 0:
                print(f"forwarded={forwarded} seq_gaps={dropped}")
    except KeyboardInterrupt:
        print("\nBridge stopped")
    finally:
        rx.close()
        tx.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
