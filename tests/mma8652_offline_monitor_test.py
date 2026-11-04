#!/usr/bin/env python3
"""Pure marker gating plus subprocess mocks: no real device is opened."""
import hashlib
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('offline_monitor', root/'tools/recovery/monitor-mma8652-offline.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
token = '0123456789abcdef0123456789abcdef'
g = m.Gate(token)
assert not g.observe(m.payload('11111111111111111111111111111111'))
assert not g.observe(m.payload(token)[:-1])
assert not g.observe(m.payload(token))
assert not g.observe('garbage')
assert not g.observe(m.payload(token))
assert g.observe(m.payload(token))

with tempfile.TemporaryDirectory(prefix='mma-safe-runner-') as d:
    base=Path(d); repo=base/'workspace'/'repo'; tools=repo/'tools'/'recovery'; tools.mkdir(parents=True)
    (repo/'tools'/'uart').mkdir()
    (repo/'tools'/'winpe').mkdir()
    shell=(root/'tools/recovery/run-mma8652-offline-stage.sh').read_text()
    assert 'MMA_CANONICAL_UNBOUNDED_UEFI_NOT_VALIDATED_NO_DEVICE_ACTION' in shell
    shell=shell.replace('echo MMA_CANONICAL_UNBOUNDED_UEFI_NOT_VALIDATED_NO_DEVICE_ACTION >&2\nexit 2\n', '')
    (tools/'run.sh').write_text(shell); (tools/'run.sh').chmod(0o755)
    events=base/'events'
    (tools/'flash-efi.sh').write_text('#!/bin/sh\necho launch >> "$FAKE_EVENTS"\n')
    (tools/'flash-efi.sh').chmod(0o755)
    (tools/'monitor-mma8652-offline.py').write_text('import os,time\nopen(os.environ["FAKE_EVENTS"],"a").write("monitor\\n")\n'
                                                    'time.sleep(30) if os.environ.get("FAKE_SLEEP")=="1" else None\nraise SystemExit(3)\n')
    bins=base/'bin'; bins.mkdir()
    (bins/'fastboot').write_text('#!/bin/sh\ncase "$*" in *devices*) echo "SERIAL fastboot";; *getvar*) echo "product: A04BR3";; *flash*) echo "flash $*" >> "$FAKE_EVENTS";; esac\n')
    (bins/'fastboot').chmod(0o755)
    (bins/'systemctl').write_text('#!/bin/sh\necho inactive\n'); (bins/'systemctl').chmod(0o755)
    boot=base/'test-boot'; boot.write_bytes(b'test boot')
    restore=base/'restore-boot'; restore.write_bytes(b'restore boot')
    esp=base/'test-esp'; resp=base/'restore-esp'
    for f in (esp,resp):
        with f.open('wb') as s: s.truncate(268435456)
    log=base/'uart.log'; log.write_bytes(b'')
    def digest(p):
        h=hashlib.sha256()
        with p.open('rb') as s:
            for chunk in iter(lambda:s.read(1024*1024),b''): h.update(chunk)
        return h.hexdigest()
    env=os.environ.copy(); env.update({'PATH':str(bins)+':'+env['PATH'], 'FAKE_EVENTS':str(events),
        'A04_UART':'/dev/null','A04_UART_LOG':str(log),'A04_OFFLINE_NONCE_LEDGER':str(base/'ledger'),
        'A04_TEST_BOOT_SHA256':digest(boot),'A04_TEST_ESP_SHA256':digest(esp),
        'A04_RESTORE_BOOT_SHA256':digest(restore),'A04_RESTORE_ESP_SHA256':digest(resp)})
    import json
    manifest=base/'manifest.json'; manifest.write_text(json.dumps({'version':1,'token':token,'image_sha256':digest(esp)}))
    cmd=['bash',str(tools/'run.sh'),'SERIAL',str(boot),str(esp),str(restore),str(resp),str(manifest),str(base/'out')]
    # Redirect the hardcoded flash log inside the sandbox as well.
    flashlog=base/'port'/'gsi'/'logs'; flashlog.mkdir(parents=True)
    p=subprocess.run(cmd,env=env,text=True,capture_output=True,timeout=20)
    assert p.returncode==3 and 'NO_RESET_NO_RESTORE' in p.stderr, (p.returncode,p.stdout,p.stderr)
    data=events.read_text(); assert sum(line.startswith('flash ') for line in data.splitlines())==1 and 'monitor' in data and data.count('launch')==1, (data,p.stderr)
    assert 'restore-boot' not in data and 'reset' not in data
    cmd[-1]=str(base/'out2')
    p=subprocess.run(cmd,env=env,text=True,capture_output=True,timeout=20)
    assert p.returncode == 2 and 'MMA_NONCE_ALREADY_USED' in p.stderr
    assert events.read_text() == data
print('MMA8652 offline monitor fail-closed tests passed')
