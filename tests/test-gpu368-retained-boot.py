#!/usr/bin/env python3
"""実保存済みboot保持chainのcomponent検証。fresh200やsnapshotを生成しない。"""
import argparse,copy,hashlib,importlib.util,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--workspace',type=Path,required=True);a=p.parse_args();root=a.workspace.resolve(strict=True);repo=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('p200',repo/'tools/recovery/prepare-peripheral-followup200.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def ref(n):return dict(path=n,sha256=hashlib.sha256((root/n).read_bytes()).hexdigest())
def pin(r):
 q=(root/r['path']).resolve();assert q.is_relative_to(root);b=q.read_bytes();assert hashlib.sha256(b).hexdigest()==r['sha256'];return json.loads(b.decode('utf-8-sig'))
def pin_file(r):
 q=(root/r['path']).resolve();assert q.is_relative_to(root);assert hashlib.sha256(q.read_bytes()).hexdigest()==r['sha256']
# このobjectは既履歴componentだけ。fresh_manifest/Recovery boot IDを作らない。
b=dict(physical_read_this_capture=False,no_intervening_boot_or_gpt_write_reported=True,boot_sha256=m.BOOT0943,host_artifact=ref('port/xnu/out/windows-rt-present356-r2/firmware/boot-final/boot.img'),matched_host_seal=ref('port/gsi/logs/ios-feasibility/windows-rt-combined-run177/main-deployment-contract.json'),last_write_seal=ref('port/gsi/logs/ios-feasibility/windows-rt-combined-run177/main-deployment-contract.json'),last_write_result=ref('port/gsi/logs/ios-feasibility/windows-rt-combined-run177/result.json'),trial199_seal=ref('port/gsi/logs/ios-feasibility/windows-rt-usb1121-run199-seal/main-deployment-contract.json'),trial199_controller=ref('port/gsi/logs/ios-feasibility/windows-rt-usb1121-run199/result.json'),retained_steps=[ref('port/gsi/logs/ios-feasibility/windows-rt-combined-run197/result.json')])
t=dict(boot_sha256=m.BOOT0943,boot_path=b['host_artifact']['path'])
e=m.retained_boot_chain_evidence(b,t,pin,pin_file);assert e['physical_read_this_capture'] is False
results=['actual run177 applied boot / sealed host0943 / run197 retained / actual trial199 no-write chain accepted']
for name,change in [('invented read flag',dict(physical_read_this_capture=True)),('unknown continuity',dict(no_intervening_boot_or_gpt_write_reported=False)),('wrong boot',dict(boot_sha256='0'*64))]:
 bad=copy.deepcopy(b);bad.update(change)
 try:m.retained_boot_chain_evidence(bad,t,pin,pin_file)
 except ValueError:results.append(name+' rejected')
 else:raise AssertionError(name)
# 拒否条件だけをin-memoryで変更する。保存controller/receipt原本は変更しない。
for name,key in [('trial199 boot write','boot_written'),('trial199 GPT write','gpt_written')]:
 def negative_pin(r):
  value=pin(r)
  if r==b['trial199_controller']:value[key]=True
  return value
 try:m.retained_boot_chain_evidence(b,t,negative_pin,pin_file)
 except ValueError:results.append(name+' rejected')
 else:raise AssertionError(name)
bad=copy.deepcopy(b);bad['host_artifact']['sha256']='0'*64
try:m.retained_boot_chain_evidence(bad,t,pin,pin_file)
except ValueError:results.append('wrong sealed host pin rejected')
else:raise AssertionError('host pin')
print(json.dumps(dict(cases=results,physical_reads=0,fresh200_created=False,profile_finalized=False,evidence=e)))
