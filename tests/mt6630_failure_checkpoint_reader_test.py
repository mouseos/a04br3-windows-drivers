#!/usr/bin/env python3
"""固定record ABI/decoderとworkerのsource制限検査。Windows実行の代替ではない。"""
from pathlib import Path
import importlib.util,struct,hashlib
r=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('decoder',r/'tools/winpe/decode-mt6630-checkpoint.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
b=bytearray(1624);m.HEADER.pack_into(b,0,0x50433441,1,1624,3,12,10,100,200,1,0,0,0,2,1,0,0,1,1023);m.EVENT.pack_into(b,88,88,2,1,1,0xc0000001,0x0e010180)
x=m.decode(b,11);assert x['generation']==12 and x['freshness']=='newer_than_baseline' and x['driver_build']==1023 and not x['durable_success'] and x['tail_flush_result']=='unobserved'
assert x['events'][0]['stage']=='LAST_OID' and x['events'][0]['raw_status']=='0xc0000001'
for offset,value in [(0,0),(4,2),(8,1623),(12,4),(48,65),(64,11),(76,1),(88,999)]:
 c=bytearray(b);struct.pack_into('<I',c,offset,value)
 try:m.decode(c);raise AssertionError('bad schema accepted')
 except ValueError:pass
worker=(r/'tools/recovery/read-mt6630-failure-checkpoint.ps1').read_text(encoding='utf-8-sig')
helper=(r/'tools/recovery/ReadonlyInventory200HiveNative.cs').read_bytes()
assert hashlib.sha256(helper).hexdigest()=='6963b1f6c0ece21debb4935205da9784385f20e16cf53bb44b53e749d9127a6a'
assert "ControlSet001\\Services\\Mt6630Runtime\\Parameters\\NativeCheckpoint" in worker
assert "@('DriverEntry','Initialize','Scan','RuntimeFailure')" in worker
assert "ReadRawValue($key,'Generation',8)" in worker and 'ReadRawValue($key,$name,1624)' in worker
assert 'read-mt6630-start-checkpoint' not in worker and 'GetValueNames' not in worker
for writer in ['SetValue(', 'Set-Acl', 'RegSetValue', 'Set-ItemProperty', 'New-ItemProperty']:assert writer not in worker
for proof in ['unload_rc','privilege_restore_error','originals_unchanged','manifest_unchanged','RuntimeFailure_first_only_per_adapter','connection_attempt_binding_verified']:assert proof in worker
assert (r/'tools/recovery/read-mt6630-failure-checkpoint.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
print('PASS record ABI/decoder rejection and fixed-reader source guards; Windows parser/RegLoadKey still require actual Windows execution')
