#!/usr/bin/env python3
"""Host-only crash fixtures; these do not validate NTFS or WinPE durability."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('core', ROOT / 'tools/recovery/toprgu_transaction_core.py')
core = importlib.util.module_from_spec(spec)
spec.loader.exec_module(core)

def snapshot(p):
    return {name: (p/name).read_bytes() if (p/name).exists() else None for name in core.FILES}

def crash(target, journal, phase):
    script = '''import importlib.util, os, sys
s=importlib.util.spec_from_file_location('core',sys.argv[1]); m=importlib.util.module_from_spec(s); s.loader.exec_module(m)
def f(p):
 if p==sys.argv[4]: os._exit(77)
m.recover(sys.argv[2],sys.argv[3],f)
'''
    return subprocess.run([sys.executable, '-c', script, str(ROOT/'tools/recovery/toprgu_transaction_core.py'), str(target), str(journal), phase]).returncode

with tempfile.TemporaryDirectory(dir=ROOT/'build/tmp', prefix='toprgu-crash-') as base:
    base=Path(base)
    for phase in ['temporary:'+x for x in core.FILES if x!='SYSTEM.LOG'] + ['restored:'+x for x in core.FILES]:
        case=base/phase.replace(':','-'); case.mkdir(); target=case/'target'; journal=case/'journal'; target.mkdir(); journal.mkdir()
        for name in core.FILES:
            if name != 'SYSTEM.LOG': (target/name).write_bytes(('original '+name).encode())
        expected=snapshot(target)
        core.prepare(target,journal)
        for name in core.FILES: (target/name).write_bytes(b'partially updated')
        assert crash(target,journal,phase)==77, phase
        assert core.recover(target,journal)
        assert snapshot(target)==expected
        assert core.recover(target,journal)
        assert snapshot(target)==expected
    case=base/'invalid'; case.mkdir(); target=case/'target'; journal=case/'journal'; target.mkdir(); journal.mkdir()
    (target/'SYSTEM').write_bytes(b'hive'); (target/'mt8167-toprgu.sys').write_bytes(b'sys')
    core.prepare(target,journal)
    (target/'SYSTEM').write_bytes(b'changed'); before=snapshot(target)
    (journal/'SYSTEM.original').write_bytes(b'corrupt')
    try: core.recover(target,journal)
    except core.TransactionError: pass
    else: raise AssertionError('corrupt backup accepted')
    assert snapshot(target)==before
    case=base/'partial'; case.mkdir(); target=case/'target'; journal=case/'journal'; target.mkdir(); journal.mkdir()
    (target/'SYSTEM').write_bytes(b'hive'); (target/'mt8167-toprgu.sys').write_bytes(b'sys'); before=snapshot(target)
    def stop(phase): raise RuntimeError('power loss')
    try: core.prepare(target,journal,stop)
    except RuntimeError: pass
    else: raise AssertionError('did not interrupt')
    try: core.recover(target,journal)
    except core.TransactionError: pass
    else: raise AssertionError('incomplete backup accepted')
    assert snapshot(target)==before
print('POSIX fixture: interrupted recovery, repeated recovery, corrupt and incomplete backup: OK')
