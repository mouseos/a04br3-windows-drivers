#!/usr/bin/env python3
"""Build a local, experimental smaller ESP; never access a block device."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import stat
import struct
import subprocess

from pyfatfs.PyFatFS import PyFatFS

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT.parents[1]

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def regular(path):
    p = path.resolve(strict=True)
    if WORKSPACE not in p.parents or not stat.S_ISREG(p.stat().st_mode):
        raise ValueError('input must be a regular file inside workspace')
    return p

def inventory(tool, wim):
    text = subprocess.run([str(tool), 'dir', str(wim), '1', '--detailed'],
                          check=True, capture_output=True, text=True).stdout
    records = {}
    for block in text.split('----------------------------------------------------------------------------'):
        path = re.search(r'Full Path\s*= "([^"]+)"', block)
        if not path:
            continue
        name = path.group(1)
        if name in records or '"' in name or '\n' in name:
            raise ValueError('ambiguous WIM path')
        attrs = re.search(r'Attributes\s*= (0x[0-9a-fA-F]+)', block)
        sd = re.search(r'Security Descriptor\s*= ([^\r\n]+)', block)
        streams = re.findall(r'Hash\s*= (0x[0-9a-fA-F]+)', block)
        if not attrs:
            raise ValueError('incomplete WIM metadata')
        records[name] = (int(attrs.group(1), 16), sd.group(1) if sd else None, tuple(streams))
    return records

def prune_candidates(records):
    result = []
    for name, (attrs, _, _) in records.items():
        if not attrs & 0x10:
            continue
        lower = name.lower()
        parts = lower.split('/')
        if (any(x in ('fr-fr', 'es-es') for x in parts) or
            (lower.startswith('/windows/winsxs/') and
             ('_fr-fr_' in lower or '_es-es_' in lower)) or
            lower == '/windows/winsxs/manifestcache' or
            '/nv_disp.inf_arm_' in lower):
            result.append(name)
    selected = []
    for name in sorted(result, key=lambda x: (x.count('/'), x.lower())):
        if not any(name.lower().startswith(x.lower() + '/') for x in selected):
            selected.append(name)
    if not selected or '/Windows/WinSxS/ManifestCache' not in selected:
        raise ValueError('unexpected WIM layout')
    return selected

def under(name, selected):
    low = name.lower()
    return any(low == p.lower() or low.startswith(p.lower() + '/') for p in selected)

def fat_files(fat):
    out = {}
    for directory, _, files in fat.walk():
        for entry in files:
            name = directory.rstrip('/') + '/' + entry.name
            h = hashlib.sha256()
            with fat.openbin(name, 'r') as f:
                for chunk in iter(lambda: f.read(1024 * 1024), b''):
                    h.update(chunk)
            out[name.lower()] = (name, h.hexdigest())
    return out

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--wim', type=Path, required=True)
    p.add_argument('--wim-sha256', required=True)
    p.add_argument('--esp', type=Path, required=True)
    p.add_argument('--esp-sha256', required=True)
    p.add_argument('--wimlib', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    wim, esp, tool = map(regular, (a.wim, a.esp, a.wimlib))
    out = a.output.resolve()
    if WORKSPACE not in out.parents or out.exists():
        raise ValueError('output must be a new workspace directory')
    if sha(wim) != a.wim_sha256.lower() or sha(esp) != a.esp_sha256.lower():
        raise ValueError('input hash mismatch')
    if esp.stat().st_size != 256 * 1024 * 1024:
        raise ValueError('unexpected ESP size')
    out.mkdir(parents=True)
    originals = out / 'originals'; originals.mkdir()
    shutil.copyfile(wim, originals / 'boot.wim')
    shutil.copyfile(esp, originals / 'esp.img')
    if sha(originals / 'boot.wim') != sha(wim) or sha(originals / 'esp.img') != sha(esp):
        raise ValueError('original backup verification failed')
    with PyFatFS(str(esp), read_only=True, preserve_case=True) as fat:
        before_fat = fat_files(fat)
    original_boot = before_fat.get('/sources/boot.wim')
    if not original_boot or original_boot[1] != sha(wim):
        raise ValueError('ESP and input WIM mismatch')
    bcd = before_fat['/efi/microsoft/boot/bcd'][1]
    records = inventory(tool, wim)
    removals = prune_candidates(records)
    (out / 'wim-removed-paths.json').write_text(json.dumps(removals, indent=2) + '\n')
    working = out / 'working.wim'; shutil.copyfile(wim, working)
    commands = ''.join('delete --recursive "' + x + '"\n' for x in removals)
    env = dict(os.environ, TMPDIR=str(out))
    subprocess.run([str(tool), 'update', str(working), '1', '--check'], input=commands,
                   text=True, env=env, check=True)
    new_wim = out / 'maintenance.wim'
    subprocess.run([str(tool), 'export', str(working), '1', str(new_wim), '--boot',
                    '--compress=LZX', '--check'], env=env, check=True)
    subprocess.run([str(tool), 'verify', str(new_wim)], env=env, check=True)
    after = inventory(tool, new_wim)
    expected = {n: v for n, v in records.items() if not under(n, removals)}
    if after != expected:
        missing = sorted(set(expected) - set(after))[:10]
        extra = sorted(set(after) - set(expected))[:10]
        changed = [n for n in expected.keys() & after.keys() if expected[n] != after[n]][:10]
        raise ValueError(f'WIM retained metadata differs: missing={missing}, extra={extra}, changed={changed}')
    output = out / 'maintenance-esp.img'
    stage = out / 'stage.json'
    stage.write_text(json.dumps({'version': 1, 'token': secrets.token_hex(16),
                                 'wim_sha256': sha(new_wim)}) + '\n')
    subprocess.run([os.environ.get('PYTHON', 'python3'), str(ROOT / 'tools/winpe/prepare-mma8652-esp.py'),
                    '--source-esp', str(esp), '--source-sha256', sha(esp),
                    '--verified-winpe-bcd-sha256', bcd, '--wim', str(new_wim),
                    '--stage-manifest', str(stage), '--output', str(output)], env=env, check=True)
    # Remove only boot-manager locale files outside English and Japanese, and
    # East Asian fonts not used by those languages. Keep every other file.
    font_drop = {'chs_boot.ttf', 'cht_boot.ttf', 'kor_boot.ttf', 'malgun_boot.ttf',
                 'msjh_boot.ttf', 'msyh_boot.ttf'}
    fat_removed = []
    with PyFatFS(str(output), preserve_case=True) as fat:
        for key, (name, _) in before_fat.items():
            bits = key.strip('/').split('/')
            locale = (len(bits) == 2 and bits[1] == 'bootmgr.efi.mui' and bits[0] not in ('en-us', 'ja-jp')) or \
                     (len(bits) == 4 and bits[:2] == ['efi', 'boot'] and bits[3] == 'bootarm.efi.mui' and bits[2] not in ('en-us', 'ja-jp'))
            font = len(bits) == 5 and bits[:4] == ['efi','microsoft','boot','fonts'] and bits[4] in font_drop
            if locale or font:
                fat.remove(name); fat_removed.append(name)
    check = subprocess.run(['fsck.vfat', '-a', str(output)], capture_output=True, text=True)
    if check.returncode not in (0, 1):
        raise ValueError('FAT repair failed')
    check = subprocess.run(['fsck.vfat', '-vn', str(output)], capture_output=True, text=True, check=True)
    m = re.search(r'(\d+)/(\d+) clusters', check.stdout)
    if not m:
        raise ValueError('FAT free-cluster count unavailable')
    with output.open('rb') as f:
        boot = f.read(512)
    cluster = struct.unpack_from('<H', boot, 11)[0] * boot[13]
    free = (int(m.group(2)) - int(m.group(1))) * cluster
    with PyFatFS(str(output), read_only=True, preserve_case=True) as fat:
        after_fat = fat_files(fat)
    expected_fat = {k: v for k, v in before_fat.items() if v[0] not in fat_removed}
    expected_fat['/sources/boot.wim'] = (original_boot[0], sha(new_wim))
    additions = sorted(after_fat.keys() - expected_fat.keys())
    if (any(not re.search(r'/fsck[0-9]{4}\.[0-9]{3}$', k) for k in additions) or
        any(k not in after_fat or after_fat[k][1] != v[1] for k, v in expected_fat.items())):
        raise ValueError('ESP retained-file verification failed')
    # fsck may preserve an orphan chain as a new FSCK file; retain and record it.
    fsck_preserved = {after_fat[k][0]: after_fat[k][1] for k in additions}
    # The replacement helper wrote an intermediate manifest before pruning.
    output.with_suffix(output.suffix + '.json').unlink()
    result = {'version': 1, 'source_wim_sha256': sha(wim), 'source_esp_sha256': sha(esp),
              'original_wim_copy_sha256': sha(originals / 'boot.wim'),
              'original_esp_copy_sha256': sha(originals / 'esp.img'),
              'wim_sha256': sha(new_wim), 'esp_sha256': sha(output), 'bcd_sha256': bcd,
              'wimlib_sha256': sha(tool),
              'wim_removed_paths_file': 'wim-removed-paths.json', 'fat_removed': fat_removed,
              'fsck_preserved_files': fsck_preserved,
              'fat_free_bytes': free, 'source_wim_bytes': wim.stat().st_size,
              'output_wim_bytes': new_wim.stat().st_size,
              'status': 'experimental; boot, DISM, signature and power-loss recovery unverified'}
    (out / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    working.unlink()
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
