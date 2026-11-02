#!/usr/bin/env python3
"""Capture the opt-in A04BR3 UART framebuffer monitor as a monochrome PBM."""
import argparse
import os
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--log', type=Path, required=True, help='existing UART collector log')
p.add_argument('--uart', default='/dev/ttyUSB0')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--layer', type=int, choices=(0, 3), default=0)
p.add_argument('--timeout', type=float, default=45)
a = p.parse_args()
if a.output.exists():
    raise SystemExit('Output already exists')
marker = b'A04FBHEX 800 1280 ' if a.layer == 0 else b'A04FB3HEX 800 1280 '
with a.log.open('rb') as stream:
    stream.seek(0, os.SEEK_END)
    subprocess.run(['stty', '-F', a.uart, '921600', 'cs8', '-cstopb', '-parenb', 'raw', '-echo', 'clocal'], check=True)
    with open(a.uart, 'wb', buffering=0) as uart:
        uart.write(b'\nA04BR3!FB!\n' if a.layer == 0 else b'\nA04BR3!F3!\n')
    end = time.monotonic() + a.timeout
    data = bytearray()
    while time.monotonic() < end:
        data.extend(stream.read())
        start = data.find(marker)
        if start >= 0:
            payload = bytes(data[start + len(marker):]).split(b'\n', 1)[0].replace(b'\r', b'').replace(b'\0', b'')
            if len(payload) >= 256000:
                if len(payload) != 256000 or any(c not in b'0123456789ABCDEF' for c in payload):
                    raise SystemExit('Invalid framebuffer payload')
                # PBM uses 1 for black; the wire format uses 1 for non-black.
                pixels = bytes(x ^ 0xff for x in bytes.fromhex(payload.decode('ascii')))
                with a.output.open('xb') as out:
                    out.write(b'P4\n800 1280\n' + pixels)
                print(f'Saved {a.output}')
                break
        elif len(data) > 65536:
            del data[:-len(marker)]
        time.sleep(.1)
    else:
        raise SystemExit('Timed out waiting for framebuffer data')
