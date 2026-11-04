#!/usr/bin/env python3
"""Read-only inventory of known local automatic reset sources; never a safety proof."""
import json
import os
from pathlib import Path
import subprocess

UNITS = ('a04br3-stuck-guard.service', 'a04br3-ce66352-recovery.service')
SCRIPTS = ('stuck-guard.py', 'a04br3_trial_guardian.py', 'run-display-winpe-test.sh')

def classify_unit(properties):
    load = properties.get('LoadState')
    if load == 'not-found':
        return 'not-found'
    if load != 'loaded' or properties.get('ActiveState') != 'inactive':
        return 'hazard-or-unknown'
    state = properties.get('UnitFileState')
    if state not in ('masked', 'masked-runtime'):
        return 'can-be-started-or-unknown'
    return 'inactive-and-masked'

def main():
    units = {}
    for name in UNITS:
        try:
            p = subprocess.run(['systemctl', '--user', 'show', name, '-p', 'LoadState',
                                '-p', 'ActiveState', '-p', 'SubState', '-p', 'UnitFileState'],
                               capture_output=True, text=True, timeout=5)
            props = dict(line.split('=', 1) for line in p.stdout.splitlines() if '=' in line)
            units[name] = {'properties': props, 'assessment': classify_unit(props) if p.returncode == 0 else 'unknown'}
        except (OSError, subprocess.TimeoutExpired):
            units[name] = {'assessment': 'unknown'}
    processes, unreadable = [], 0
    for p in Path('/proc').iterdir():
        if not p.name.isdecimal() or int(p.name) == os.getpid():
            continue
        try:
            args = (p/'cmdline').read_bytes().split(b'\0')
            # Match individual executable/script arguments only, never arbitrary shell text.
            names = [os.path.basename(os.fsdecode(a)) for a in args if a]
            if any(name in SCRIPTS for name in names): processes.append({'pid': int(p.name), 'scripts': [n for n in names if n in SCRIPTS]})
        except FileNotFoundError:
            continue
        except OSError:
            unreadable += 1
    result = {'known_user_units': units, 'matching_processes': processes,
              'unreadable_processes': unreadable, 'permission_to_service': False,
              'note': 'Snapshot only: system services, other hosts, future starts and hardware resets are not ruled out.'}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 2
if __name__ == '__main__': raise SystemExit(main())
