#!/usr/bin/env python3
"""Actual sampler/parent include/client with host WDF/NT API models."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import re

p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.out.resolve();o.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMPDIR=str(o),TMP=str(o),TEMP=str(o))
parent=(r/'drivers/bus/mt8167-shared/wdf_parent_probe.c').read_text()
start=parent.index('static int ParentLock(');end=parent.index('static uint32_t ParentPwrapRead',start)
(o/'parent-lock-under-test.h').write_text(parent[start:end])
assert 'query.SendQueryToParentStack=TRUE' in parent[parent.index('&Mt8167ClockSnapshotInterfaceGuid,NULL)'):][:200]
assert 'clockIface.Snapshot=ParentClockSnapshot;' in parent
# Rebinding the actual1.10 leaf must not undo first-read/status behavior.
leaf=(r/'drivers/power/mt8167-thermal/wdf_native_thermal.c').read_text()
leaf=re.sub(r'\n#ifndef MT8167_CLOCK_SNAPSHOT_QUERY\n#define MT8167_CLOCK_SNAPSHOT_QUERY 0\n#endif\n#if MT8167_CLOCK_SNAPSHOT_QUERY\n.*?\n#endif','',leaf,count=1,flags=re.S)
leaf=re.sub(r'#if MT8167_CLOCK_SNAPSHOT_QUERY\n if\(code==MT8167_CLOCK_GET_SNAPSHOT\).*?\n#endif\n','',leaf,count=1,flags=re.S)
original=subprocess.check_output(['git','-C',str(r),'show','740800733fe9741023038026a717fd54d43cd7c3:drivers/power/mt8167-thermal/wdf_native_thermal.c'],text=True)
assert leaf==original, 'non-clock actual1.10 leaf drift'
cc=os.environ.get('CC','clang')
cmd=[cc,'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',
 '-I',str(r/'tests/clock_snapshot_stubs'),'-I',str(o),
 str(r/'tests/mt8167_clock_snapshot_test.c'),str(r/'drivers/bus/mt8167-shared/clock_snapshot.c'),
 str(r/'drivers/system/mt8167-clock/parent_snapshot_client.c'),'-o',str(o/'test')]
subprocess.run(cmd,check=True,env=env)
result=json.loads(subprocess.check_output([str(o/'test'),str(o/'model-only-snapshot.bin')],env=env,text=True))
spec=importlib.util.spec_from_file_location('decode',r/'tools/cpu/decode-mt8167-clock-snapshot.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
decoder_cases=0
def check_decode(*args):
 global decoder_cases
 decoder_cases+=1;return m.decode(*args)
b=(o/'model-only-snapshot.bin').read_bytes();v=check_decode(b)
assert v['effective_CPU_hz'] is None and v['DDR_hz'] is None
assert v['configured_PLL_hz_from_supplied_reference']['armpll'] is None
v=check_decode(b,26000000,'synthetic test: nominal DTS reference, not measured')
assert v['configured_PLL_hz_from_supplied_reference']['armpll']==1001000000
assert v['configured_PLL_hz_from_supplied_reference']['univpll']==1040000000
for pcw,expected in [(0xc8000,1300000000),(0x109a001,500500794)]:
 words=list(struct.unpack('<6I2Q22I',b));words[11]=words[22]=pcw
 assert check_decode(struct.pack('<6I2Q22I',*words),26000000,'synthetic')['configured_PLL_hz_from_supplied_reference']['armpll']==expected
for index in range(11):
 words=list(struct.unpack('<6I2Q22I',b));words[8+11+index]^=1
 assert check_decode(struct.pack('<6I2Q22I',*words),26000000,'synthetic')['configured_PLL_hz_from_supplied_reference']['armpll'] is None
words=list(struct.unpack('<6I2Q22I',b));words[9]=words[20]=1
assert check_decode(struct.pack('<6I2Q22I',*words),26000000,'synthetic')['configured_PLL_hz_from_supplied_reference']['armpll'] is None
for value in [b[:-1],b+b'\0']:
 try:check_decode(value)
 except ValueError:pass
 else:raise AssertionError('invalid length accepted')
result.update(ASan_UBSan=True,decoder_cases=decoder_cases,runner_access=False,real_WDF_runtime=False,
 parent_lock_from_production_source=True,model_binary_is_hardware_evidence=False)
(o/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
