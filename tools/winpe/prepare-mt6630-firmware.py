#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Prepare pinned MT6630 inputs for an offline Windows image; no device I/O."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import urllib.request
import zlib

REV = '0c756f413e7011231c8fb793af0930311023f68d'
BASE = 'https://raw.githubusercontent.com/mouseos/android_vendor_benesse_a04br3/' + REV + '/proprietary/vendor/firmware/'
INPUTS = {
    'WIFI_RAM_CODE_MT6630': '07fe1a225e6b9de6b310894db3b40bcc1cb66107b913e4c43eb6a62089aada22',
    'mt6630_patch_e3_0_hdr.bin': 'cd345f88f69566aa77bf64ec4bb7e3035c5cdf45a9dc6084c8381b55c441752c',
    'mt6630_patch_e3_1_hdr.bin': '45ddf35f02e8ea957be2146fbc7396b14408d0bf66418779e279ff99fa589026',
}
WMT_SHA = '4af7996b89ef110d8f270877f61aaf0b90fde21b6c5b0ed23ccbd32c2989dcee'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def build(inputs):
    for name, expected in INPUTS.items():
        if name not in inputs or sha(inputs[name]) != expected:
            raise ValueError('firmware hash mismatch: ' + name)
    names = list(INPUTS)[1:]
    size = 32 + 64 * len(names) + sum(len(inputs[n]) for n in names)
    result = bytearray(size)
    struct.pack_into('<8s6I', result, 0, b'A04WMT1\0', 1, len(names), size, 64, 32, 0)
    offset = 32 + 64 * len(names)
    for i, name in enumerate(names):
        data = inputs[name]
        start = 32 + 64 * i
        result[start:start + len(name)] = name.encode('ascii')
        struct.pack_into('<4I', result, start + 48, offset, len(data), zlib.crc32(data), 0)
        result[offset:offset + len(data)] = data
        offset += len(data)
    bundle = bytes(result)
    if sha(bundle) != WMT_SHA:
        raise ValueError('WMT bundle hash mismatch')
    return bundle, inputs['WIFI_RAM_CODE_MT6630']

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path, help='new empty staging directory')
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        parser.error('output must be empty')
    inputs = {}
    for name in INPUTS:
        with urllib.request.urlopen(BASE + name, timeout=60) as response:
            inputs[name] = response.read(16 * 1024 * 1024 + 1)
    wmt, wlan = build(inputs)
    args.output.mkdir(parents=True, exist_ok=True)
    for name, data in [('mt6630-wmt.bin', wmt), ('mt6630-wlan.bin', wlan)]:
        (args.output / name).write_bytes(data)
    manifest = {'vendor_revision': REV, 'source_sha256': INPUTS,
                'wmt_sha256': sha(wmt), 'wlan_sha256': sha(wlan),
                'target_directory': r'Windows\System32\drivers\a04br3',
                'note': 'Inputs only; board settings and device ownership remain unverified. Redistribution rights unverified.'}
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
