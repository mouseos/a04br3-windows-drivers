#!/usr/bin/env python3
"""実captureの固定SHA/三本とreader契約を検査。hiveを開かずreceiptを作らない。"""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--workspace',type=Path,required=True);a=p.parse_args();root=a.workspace.resolve(strict=True);repo=Path(__file__).resolve().parents[1]
manifest=root/'port/xnu/out/private-windows-rt-prebackup-run200/manifest.json';raw=manifest.read_bytes();h=hashlib.sha256(raw).hexdigest();assert h=='fbba59d9561ca61f9ff1b41e6bbe0a451330b91741abf487a5caaeeb420b2ed9';m=json.loads(raw)
assert m['status']=='captured-unmounted' and m['read_only'] is True and m['unmounted'] is True and m['errors']==[] and m['recovery_boot_id']=='64657ae7-390c-431b-ae4d-ae5de209793b'
for name in ['SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2']:
 rows=[r for r in m['files'] if r['source_path']=='Windows/System32/config/'+name];assert len(rows)==1;r=rows[0];b=(manifest.parent/r['backup_path']).read_bytes();assert len(b)==r['bytes'] and hashlib.sha256(b).hexdigest()==r['sha256']
s=(repo/'tools/recovery/read-fresh200-native-inventory.ps1').read_text();cs=(repo/'tools/recovery/ReadonlyInventory200HiveNative.cs').read_text()
assert h in s and 'MT8167CombinedFollowup200' in s
assert 'capture_contract' not in s and 'GPU368_manifest' not in s and 'ExpectedPolicySha256' in s
assert '[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\\clone\\SYSTEM")' in s
assert '.SetValue(' not in s and '.CreateSubKey(' not in s and 'Start-Service' not in s
assert 'full_service_preservation_equal=$null' in s and 'preservation_comparison_performed=$false' in s
assert 'ReadRawValue($key,$v.name,4096)' in s and 'raw_hex=$value.RawHex' in s
assert 'source_manifest_sha256=$ExpectedManifestSha256' in s and 'Original trio changed' in s
assert 'records_not_assumed_current_boot=$true' in s
assert 'if(writable) throw' in cs and 'PrivateMounts.Contains(name)' in cs and 'PrivateRoots.Contains' in cs
assert 'disposition!=2' in cs and 'parent.GetSubKeyNames()' in cs and 'CheckLink(h)' in cs
assert 'RegSetValue' not in cs and 'RegDelete' not in cs and 'SetAccessControl' not in cs
old=subprocess.check_output(['git','-C',str(repo),'show','0dc155ac:tools/recovery/prepare-offline-system-service.ps1'],text=True)
now=(repo/'tools/recovery/prepare-offline-system-service.ps1').read_text()
assert old[old.index('function Peripheral-CapturedValues('):old.index('function Check-RelatedFingerprints(')].strip()==now[now.index('function Peripheral-CapturedValues('):now.index('# fresh200 only:')].strip()
assert 'Peripheral-CapturedValues200 $Root' in now and '2048' in now[now.index('function Peripheral-CapturedValues200('):now.index('function Check-RelatedFingerprints(')]
# これはソースガード検査でありPowerShell/CLR実行検証ではない。
print(json.dumps(dict(status='host-source-contracts-passed',actual_fresh200_manifest_sha256=h,actual_trio_files_verified=3,readonly_guard_contracts_checked=True,old_named_value_function_unchanged=True,Windows_parser_run=False,Windows_CLR_compiled=False,hive_loaded=False,native_receipt_created=False,device_operations=0)))
