#!/usr/bin/env python3
"""Independently check the generated HID short-item layout and physical ranges."""
import subprocess

data = subprocess.check_output(['./build/touch_hid_dump'])
g = {'page': 0, 'size': 0, 'count': 0, 'report': 0, 'physical': None}
usages, fields, offsets = [], {}, {}
pos = 0
while pos < len(data):
    header = data[pos]
    pos += 1
    length = (0, 1, 2, 4)[header & 3]
    assert header != 0xfe and pos + length <= len(data)
    value = int.from_bytes(data[pos:pos + length], 'little')
    pos += length
    typ, tag = (header >> 2) & 3, header >> 4
    if typ == 1:
        key = {0: 'page', 4: 'physical', 7: 'size', 8: 'report', 9: 'count'}.get(tag)
        if key:
            g[key] = value
    elif typ == 2 and tag == 0:
        usages.append(value)
    elif typ == 0:
        if tag in (8, 11):
            kind = 'input' if tag == 8 else 'feature'
            key = (kind, g['report'])
            offset = offsets.get(key, 0)
            if not (value & 1):
                assert len(usages) == 1 and g['count'] == 1
                fields[(kind, g['report'], g['page'], usages[0])] = (
                    offset, g['size'], g['physical'])
            offsets[key] = offset + g['size'] * g['count']
        usages.clear()
assert offsets == {('input', 1): 72, ('feature', 2): 8}
assert fields[('input', 1, 0x0d, 0x42)][:2] == (0, 1)
assert fields[('input', 1, 0x0d, 0x51)][:2] == (8, 8)
assert fields[('input', 1, 0x01, 0x30)] == (16, 16, 700)
assert fields[('input', 1, 0x01, 0x31)] == (32, 16, 500)
assert fields[('input', 1, 0x0d, 0x56)][:2] == (48, 16)
assert fields[('input', 1, 0x0d, 0x54)][:2] == (64, 8)
assert fields[('feature', 2, 0x0d, 0x55)][:2] == (0, 8)
print('HID descriptor layout and supplied physical ranges verified')
