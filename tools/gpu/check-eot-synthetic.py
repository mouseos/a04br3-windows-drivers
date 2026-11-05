#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check private synthetic EOT variants without printing captured addresses."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import struct
import sys

PIN = 'ef11349e8e5d620a2227fac0eed17e7ed1f86428'
EXPECTED = bytes.fromhex('8692401300000000000040ff8692401300000000000041ff48a080c2c14000400000f3ffffffffff')
BACKGROUND = bytes.fromhex('388a8087800800000020f3ffffffffff')


def normalized(data):
    if len(data) != 40:
        raise ValueError('unexpected EOT length')
    out = bytearray(data)
    out[4:8] = out[16:20] = bytes(4)
    return bytes(out)


def check(root):
    root = pathlib.Path(root)
    manifest = json.loads((root / 'run-manifest.json').read_text())
    diag = manifest.get('diagnostic_manifest') or {}
    raw = (root / 'events.jsonl').read_bytes()
    if (manifest['mesa_build_manifest']['mesa_commit'] != PIN or not manifest.get('scratchless_vertex')
            or not diag.get('eot_synthetic_private') or not diag.get('background_shader_private')
            or not diag.get('background_constants_metadata_only') or hashlib.sha256(raw).hexdigest() != manifest['events_sha256']):
        raise ValueError('capture provenance')
    for tool in ('check-spm-load.py', 'check-fragment-background.py'):
        spec = importlib.util.spec_from_file_location('a04_private_check', pathlib.Path(__file__).with_name(tool))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.check(root)
    events = [json.loads(line) for line in raw.splitlines()]
    pixel = next(e for e in events if e['event'] == 'render_pixel_event')
    targets = [e for e in events if e['event'] == 'render_pbe_target'
               and (e['state0'], e['state1']) == (pixel['pbe_state0'], pixel['pbe_state1'])]
    if not targets:
        raise ValueError('missing PBE target')
    for t in targets:
        if ((t['width'], t['height'], t['stride'], t['image_size'], t['layout']) != (64, 64, 64, 16384, 1)
                or t['target_gpu'] != t['image_gpu'] or t['target_gpu'] & 3
                or t['state0'] != (t['target_gpu'] >> 2) & 0xffffffff
                or t['state1'] != 0x82000 | ((t['target_gpu'] >> 34) & 63)):
            raise ValueError('unexpected target state')
    hashes = manifest['private_compiler_sha256']
    bg_events = [e for e in events if e['event'] == 'bg_shader']
    if len(bg_events)!=3 or {e['ordinal'] for e in bg_events}!={0,1,2}:
        raise ValueError('background shader count')
    hbg=14695981039346656037
    for b in BACKGROUND:hbg=((hbg^b)*1099511628211)&((1<<64)-1)
    for e in bg_events:
        name='loadop-%03d.bin'%e['ordinal']
        binary=(root/'private-compiler-candidates'/name).read_bytes()
        if (binary!=BACKGROUND or hashlib.sha256(binary).hexdigest()!=hashes[name] or
            sum(x['event']=='usc_upload_hash' and x['gpu_va']==e['gpu_va'] and x['size']==len(binary) and x['fnv64']==hbg for x in events)!=1):
            raise ValueError('background shader provenance')
    def candidate(i):
        name = 'eot-%03d.bin' % i
        value = (root / 'private-compiler-candidates' / name).read_bytes()
        if hashlib.sha256(value).hexdigest() != hashes[name]:
            raise ValueError('candidate digest mismatch')
        return value
    base = candidate(132)
    if normalized(base) != EXPECTED or base[16:20] != struct.pack('<I', pixel['pbe_state0']) or base[4:8] != struct.pack('<I', pixel['pbe_state1']):
        raise ValueError('EOT immediate mismatch')
    h = 14695981039346656037
    for byte in base:
        h = ((h ^ byte) * 1099511628211) & ((1 << 64) - 1)
    if sum(e['event'] == 'usc_upload_hash' and e['gpu_va'] == pixel['shader_gpu'] and e['size'] == len(base)
           and e['fnv64'] == h for e in events) != 1:
        raise ValueError('EOT upload mismatch')
    for bit in range(64):
        variant = candidate(133 + bit)
        expected = bytearray(base)
        expected[(16 if bit < 32 else 4) + (bit % 32) // 8] ^= 1 << (bit % 8)
        if variant != expected:
            raise ValueError('EOT bit mapping differs')
    for i, words in ((130, (0x12345678, 0x9abcdef0)), (131, (0xdeadbeef, 0x01234567))):
        value = candidate(i)
        if normalized(value) != EXPECTED or value[16:20] != struct.pack('<I', words[0]) or value[4:8] != struct.pack('<I', words[1]):
            raise ValueError('synthetic combination differs')
    return {'pco_fill': manifest['pco_fill'], 'diagnostic_sha256': diag['library_sha256']}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('zero')
    p.add_argument('ones')
    a = p.parse_args()
    try:
        zero, ones = check(a.zero), check(a.ones)
        if (zero['pco_fill'], ones['pco_fill']) != ('00', 'ff') or zero['diagnostic_sha256'] != ones['diagnostic_sha256']:
            raise ValueError('capture pair mismatch')
    except (OSError, ValueError, KeyError, TypeError, StopIteration, json.JSONDecodeError):
        print('invalid EOT synthetic captures', file=sys.stderr)
        return 2
    print(json.dumps({'captures': 2, 'single_bit_variants_per_capture': 64, 'shader_bytes': 40,
                      'gpu_execution_allowed': False,
                      'limitations': ['PBE hardware write bounds, context lifetime and other shader references unproven']}, indent=2))
    return 1


if __name__ == '__main__':
    sys.exit(main())
