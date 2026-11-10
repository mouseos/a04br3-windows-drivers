#!/usr/bin/env python3
"""実template125値と失敗receiptの順序を使うguard regression。実hiveはloadしない。"""
import argparse,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--workspace',type=Path,required=True);a=p.parse_args();root=a.workspace.resolve();repo=Path(__file__).resolve().parents[1]
template=json.loads((root/'port/xnu/out/windows-rt-combined197-fresh196/service/peripheral-policy.json').read_text())
receipt=json.loads((root/'port/xnu/out/windows-rt-gpu368-native200-execution/attempt1/native200-readonly/receipt.json').read_text(encoding='utf-8-sig'))
assert receipt['errors']==['Bounded named service value required'] and len(receipt['captured_named_values'])==5 and len(receipt['old_services'])==44
values=template['captured_registry_values'];key='SYSTEM\\CurrentControlSet\\Enum\\ROOT\\SYSTEM\\0001';types=dict(Service=1,ClassGUID=1,Driver=1,HardwareID=7,ConfigFlags=4)
assert values[5]['key']==key and values[5]['name']=='Service'
assert {(v['name'],v['type']) for v in values if v['key']==key}==set(types.items())
def accepts(k,n,t):
 return bool(re.fullmatch(r'SYSTEM\\CurrentControlSet\\Services\\[A-Za-z0-9_-]{1,100}(\\[A-Za-z0-9_-]{1,100})*',k)) or (k==key and types.get(n)==t)
assert all(accepts(v['key'],v['name'],v['type']) for v in values)
for k,n,t in [(key+'\\Child','Service',1),(key.replace('0001','0002'),'Service',1),(key,'Unexpected',1),(key,'HardwareID',1),(key,'ConfigFlags',1)]:assert not accepts(k,n,t)
for name in ['read-fresh200-native-inventory.ps1','prepare-offline-system-service.ps1']:
 s=(repo/'tools/recovery'/name).read_text();assert "$fixedBtRoot=$v.key -ceq '"+key+"'" in s and '@{Service=1;ClassGUID=1;Driver=1;HardwareID=7;ConfigFlags=4}' in s
 assert '$btRootTypes.ContainsKey($v.name)' in s
assert 'Exact BT root named-value type mismatch' in (repo/'tools/recovery/read-fresh200-native-inventory.ps1').read_text()
for name in ['ReadonlyInventory200HiveNative.cs','read-combined196-post.ps1']:
 original=subprocess.check_output(['git','-C',str(repo),'show','68159276:tools/recovery/'+name]);assert original==(repo/'tools/recovery'/name).read_bytes()
print(json.dumps(dict(actual_failed_receipt_confirmed=True,template_named_values=125,fixed_BT_root_typed_values=5,all_actual_template_key_names_covered=True,other_Enum_leaf_child_name_type_rejected=True,CSharp_open_rights_unchanged=True,Windows_rerun=False,native_success_claimed=False)))
