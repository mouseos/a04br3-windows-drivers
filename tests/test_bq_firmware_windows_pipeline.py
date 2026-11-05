#!/usr/bin/env python3
"""Link actual UEFI producer and Windows consumer with a fake I2C bus."""
import argparse
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--uefi-pkg',required=True,type=Path);args=p.parse_args()
uefi=args.uefi_pkg.resolve();workspace=root.parents[1].resolve()
if not uefi.is_relative_to(workspace):raise SystemExit('UEFI source outside workspace')
lib=uefi/'Library/Bq24160HandoffLib';inc=uefi/'Include';win=root/'drivers/power/bq24160'
renames=['-Da04_bq_step=firmware_bq_step','-Da04_bq_publish_handoff=firmware_bq_publish_handoff','-Da04_bq_adopt_handoff=firmware_bq_adopt_handoff']
with tempfile.TemporaryDirectory(prefix='bq-cross-',dir=root/'build') as td:
    td=Path(td); objects=[]
    for name in ('charger','BqFirmwareOwner'):
        obj=td/(name+'.o');subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(inc),*renames,'-c',str(lib/(name+'.c')),'-o',str(obj)],check=True);objects.append(str(obj))
    cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(inc),'-I'+str(win),str(root/'tests/bq24160_cross_firmware_test.c'),*objects]
    cmd += [str(win/(n+'.c')) for n in ('charger','admission','bqhr_diagnostic','owner_broker','broker_mailbox','broker_handoff','broker_lifecycle','owner_transfer')]
    cmd += [str(root/'drivers/power/lc709203f/gauge.c'),'-o',str(td/'test')]
    subprocess.run(cmd,check=True);subprocess.run([str(td/'test')],check=True)
print('Actual UEFI producer to Windows admission, broker and fake I2C: PASS (host only)')
