#!/usr/bin/env python3
"""Send deterministic binary payloads to the W6300 TCP echo server."""

from __future__ import annotations

import argparse
import ipaddress
import socket
import sys
import time


DEFAULT_SIZES = (1, 64, 512, 1460, 2048, 4096, 16384, 65536)


def recv_exact(connection: socket.socket, length: int) -> bytes:
    """Read exactly length bytes, allowing TCP recv() to return partial data."""
    received = bytearray()
    while len(received) < length:
        chunk = connection.recv(length - len(received))
        if not chunk:
            raise ConnectionError(
                f"peer closed after {len(received)} of {length} expected bytes"
            )
        received.extend(chunk)
    return bytes(received)


def make_payload(size: int, iteration: int) -> bytes:
    # Binary pattern includes 0x00 and is repeatable for each run.
    return bytes((offset * 131 + (offset >> 2) + iteration * 17) & 0xFF
                 for offset in range(size))


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def positive_float(value: str) -> float:
    parsed = float(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def ipv4_address(value: str) -> str:
    try:
        return str(ipaddress.IPv4Address(value))
    except ipaddress.AddressValueError as error:
        raise argparse.ArgumentTypeError("must be an IPv4 address") from error


def run(args: argparse.Namespace) -> int:
    sizes = args.size if args.size else list(DEFAULT_SIZES)
    total_bytes = 0

    for size in sizes:
        durations = []
        for iteration in range(args.count):
            payload = make_payload(size, iteration)
            started = time.perf_counter()
            try:
                source_address = (args.source, 0) if args.source else None
                with socket.create_connection(
                    (args.host, args.port),
                    timeout=args.timeout,
                    source_address=source_address,
                ) as connection:
                    connection.settimeout(args.timeout)
                    connection.sendall(payload)
                    echoed = recv_exact(connection, len(payload))
            except OSError as error:
                raise ConnectionError(
                    f"size={size} iteration={iteration + 1}: {error}"
                ) from error

            elapsed = time.perf_counter() - started
            if echoed != payload:
                mismatch = next(
                    index for index, (sent, got) in enumerate(zip(payload, echoed))
                    if sent != got
                )
                raise ValueError(
                    f"size={size} iteration={iteration + 1}: byte mismatch at "
                    f"offset {mismatch} (sent 0x{payload[mismatch]:02X}, "
                    f"received 0x{echoed[mismatch]:02X})"
                )

            durations.append(elapsed)
            total_bytes += size

        average_ms = sum(durations) * 1000.0 / len(durations)
        print(
            f"PASS size={size} count={args.count} bytes={size * args.count} "
            f"rtt_ms_avg={average_ms:.3f} "
            f"min={min(durations) * 1000.0:.3f} "
            f"max={max(durations) * 1000.0:.3f}"
        )

    print(f"All tests passed: {len(sizes)} size(s), {args.count} connection(s) each, "
          f"{total_bytes} payload bytes")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.0.10", help="W6300 IPv4 address")
    parser.add_argument("--port", type=positive_int, default=5000, help="TCP port")
    parser.add_argument("--source", type=ipv4_address,
                        help="local IPv4 address to bind (useful with multiple adapters)")
    parser.add_argument("--timeout", type=positive_float, default=5.0,
                        help="connect and receive timeout in seconds")
    parser.add_argument("--count", type=positive_int, default=1,
                        help="new TCP connections to test for each size")
    parser.add_argument("--size", type=positive_int, action="append",
                        help="payload size; repeat this option for multiple sizes")
    return parser.parse_args()


def main() -> int:
    try:
        return run(parse_args())
    except KeyboardInterrupt:
        print("Interrupted", file=sys.stderr)
        return 130
    except (ConnectionError, OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
