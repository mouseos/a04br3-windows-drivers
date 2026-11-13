#!/usr/bin/env python3
"""Signed-renderの保持BT packageからWindows CAT検証対象を確定する。"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def need(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def plan(root, policy_path):
    root = root.resolve(strict=True)
    policy_path = (root / policy_path).resolve(strict=True)
    need(policy_path.is_relative_to(root), 'Policy outside workspace')
    p = read(policy_path)
    need(p['bt_first_install_execute'] is False and 'bt' not in p['install_targets']
         and p['baselines']['bt']['mode'] == 'retained', 'Retained BT only; no first-install fallback')
    order = p['stage_order']
    need(order.count('bt') == 1, 'One declared BT role required')
    bit = 1 << order.index('bt')
    for key in ('first_install_mask', 'deferred_unbound_mask'):
        need(type(p[key]) is int and p[key] >= 0 and not p[key] & bit, 'BT install bit must remain clear')
    pins = {}
    for row in p['input_pins']:
        q = (root / row['path']).resolve(strict=True)
        need(q.is_relative_to(root) and q not in pins and re.fullmatch('[0-9a-f]{64}', row['sha256']),
             'Invalid/duplicate policy input pin')
        pins[q] = row['sha256']

    def pinned(path, expected):
        q = (root / path).resolve(strict=True)
        need(q.is_relative_to(root) and q.is_file() and re.fullmatch('[0-9a-f]{64}', expected or '')
             and pins.get(q) == expected == sha(q), 'BT actual file/policy pin mismatch')
        return q, dict(path=q.relative_to(root).as_posix(), sha256=expected, bytes=q.stat().st_size)

    bt = p['packages']['bt']
    baseline = p['baselines']['bt']
    need(baseline['sys_sha256'] == bt['hashes']['sys']
         and baseline['version_u64'] == bt['version_u64']
         and baseline['date_filetime'] == bt['date_filetime'], 'Retained native BT baseline/package mismatch')
    mp, manifest_ref = pinned(bt['manifest'], bt['manifest_sha256'])
    manifest = read(mp)
    need(manifest['source_commit'] == bt['source_commit'] and manifest['version'] == bt['driver_version']
         and manifest['signature_status'] == 'Valid', 'BT manifest source/version/signature mismatch')
    table = {}
    for row in manifest['files']:
        name = row['path']
        need(name not in table, 'Repeated BT manifest leaf')
        table[name] = row
    checked = {}
    for kind in ('inf', 'sys', 'cat'):
        name = bt['names'][kind]
        need(re.fullmatch('[A-Za-z0-9_.-]+', name or '') and name in table, 'Bound BT leaf name required')
        q, entry = pinned(Path(bt['directory']) / name, bt['hashes'][kind])
        need(q.parent == (root / bt['directory']).resolve(strict=True)
             and table[name]['sha256'] == entry['sha256'] and table[name]['bytes'] == entry['bytes'],
             'BT manifest leaf/actual byte mismatch')
        checked[kind] = entry
    need(len({r['path'] for r in checked.values()}) == 3, 'Distinct BT INF/SYS/CAT required')
    members = [checked['inf'], checked['sys']]
    for row in bt.get('extra_members', []):
        name = row['name']
        need(re.fullmatch('[A-Za-z0-9_.-]+', name or '') and name in table, 'Bound BT extra leaf required')
        _, entry = pinned(Path(bt['directory']) / name, row['sha256'])
        need(table[name]['sha256'] == entry['sha256'] and table[name]['bytes'] == entry['bytes'],
             'BT extra manifest/actual byte mismatch')
        members.append(entry)
    need(len({v['path'] for v in members}) == len(members), 'Duplicate BT member')
    return dict(schema='SIGNED_RENDER_RETAINED_BT_MEMBERSHIP_V1', policy_sha256=sha(policy_path),
                selection='policy.packages.bt', driver_version=bt['driver_version'],
                package_manifest=manifest_ref, catalog=checked['cat'], members=members,
                Windows_CAT_membership_verified=False, hardware_verified=False)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--workspace', type=Path, required=True)
    ap.add_argument('--policy', type=Path, required=True)
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument('--out', type=Path)
    group.add_argument('--verify-plan', type=Path)
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)
    result = plan(root, a.policy)
    path = (root / (a.out or a.verify_plan)).resolve()
    need(path.is_relative_to(root), 'Plan outside workspace')
    if a.verify_plan:
        need(read(path) == result, 'BT verification plan/input changed during signing')
    else:
        with path.open('x', encoding='utf-8') as stream:
            json.dump(result, stream, indent=2)
            stream.write('\n')


if __name__ == '__main__':
    main()
