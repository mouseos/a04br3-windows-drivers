#!/usr/bin/env python3
"""実CRCの変更前拒否を保持し、対象の復帰後に新世代をread-only回収する。"""
import argparse,hashlib,importlib.util,json,re,subprocess,sys,time
from pathlib import Path
HERE=Path(__file__).resolve().parent

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def need(ok,message):
 if not ok:raise ValueError(message)
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def load(name,p):
 spec=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def inside(root,p):
 p=(root/p).resolve();need(p.is_relative_to(root),'Workspace-local paths required');return p

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--seal',type=Path,required=True);ap.add_argument('--seal-sha256',required=True)
 ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--runtime-sha256',required=True);ap.add_argument('--output',type=Path,required=True)
 ap.add_argument('--wait-seconds',type=int,default=1800);ap.add_argument('--poll-seconds',type=int,default=15);ap.add_argument('--validate-only',action='store_true')
 a=ap.parse_args();root=a.workspace.resolve(strict=True);seal=inside(root,a.seal);runtime=inside(root,a.runtime)
 need(sha(seal)==a.seal_sha256 and sha(runtime/'result.json')==a.runtime_sha256,'Explicit old seal/runtime SHA mismatch')
 c=read(seal);old=read(runtime/'result.json');match=re.fullmatch(r'SIGNED_RENDER_RUN([0-9]{3})_MAIN_SEAL_V1',c['schema'])
 need(match and c['status']=='validated-main-seal' and old['status']=='pipeline-stopped' and old['prior_status']=='prepare-observing' and old['seal_sha256']==a.seal_sha256 and old['transaction']==c['transaction'],'Only a real stopped render preparation')
 need(old['boot_written'] is False and old['gpt_written'] is False,'No ambiguous boot/GPT writes accepted')
 number=int(match[1])+1;need(235<=number<=999 and c['capture_output']=='port/xnu/out/private-windows-rt-prebackup-run'+str(number),'Exact next capture namespace required')
 need(0<a.wait_seconds<=19800 and 5<=a.poll_seconds<=60,'Bounded transport wait required')
 pins={x['path']:x['sha256'] for x in c['pinned_files']}
 def pinned(key):
  p=inside(root,c[key]);need(c[key] in pins and sha(p)==pins[c[key]],'Pinned tool changed: '+key);return p
 decoder=load('rejected_crc',pinned('crc_decoder'))
 report=decoder.decode((runtime/'prepare-uart-storage-deinterleaved.log').read_bytes(),c['prepare_token'])
 need(report==(runtime/'prepare-report.txt').read_bytes(),'Saved report differs from CRC evidence')
 lines=report.decode().splitlines()
 need(all(lines.count(x)==1 for x in ('UPDATE_STATUS=BASELINE_REJECTED_NO_CHANGE','UPDATE_RELEASE_EXIT=0','UPDATE_EXIT=6')) and 'UPDATE_STATUS=COMMITTED' not in lines,'Only CRC-proven clean pre-copy rejection')
 capture=pinned('capture_tool');ntfs=pinned('ntfs3g');engine=load('rejected_engine',pinned('controller_engine'))
 fresh=inside(root,c['capture_output']);supp=fresh.with_name(fresh.name+'-gpu-assets');usb=fresh.with_name(fresh.name+'-usb1121-exe')
 need(all(not p.exists() for p in (fresh,supp,usb)),'Never overwrite capture evidence')
 tool_pins={p:sha(p) for p in (Path(__file__),capture,ntfs,inside(root,c['controller_engine']),inside(root,c['crc_decoder']),HERE/'capture-peripheral-preservation.py',HERE/'capture-usb1121-existing-exe.py')}
 if a.validate_only:print('CLEAN_REJECTION_AND_NEXT_CAPTURE_VALIDATED_NO_DEVICE_OPERATION');return
 out=inside(root,a.output);need(not out.exists(),'New watcher output required');out.mkdir(parents=True,mode=0o700)
 state=dict(schema='CLEAN_REJECTION_CAPTURE_WAIT_V1',status='waiting-for-transport',old_seal_sha256=a.seal_sha256,old_runtime_sha256=a.runtime_sha256,next_capture_number=number,hardware_verified=False,boot_written=False,gpt_written=False)
 def save(): (out/'result.json').write_text(json.dumps(state,indent=2)+'\n')
 def cmd(args,name=None,timeout=20,check=True):
  result=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
  if name:(out/name).write_bytes(result.stdout)
  if check:result.check_returncode()
  return result
 def adb(*args,**kw):return cmd(['adb','-s',c['serial'],*args],**kw)
 def probe(*args):
  try:return adb(*args,timeout=5,check=False).stdout.strip()
  except subprocess.TimeoutExpired:return b''
 state['tool_pins']=[dict(path=p.relative_to(root).as_posix(),sha256=h) for p,h in tool_pins.items()]
 def cancelled():
  if not (out/'cancel').exists():return False
  state['status']='cancelled-before-capture';save();return True
 save()
 try:
  deadline=time.monotonic()+a.wait_seconds;mode=None
  while time.monotonic()<deadline:
   if cancelled():return
   try:
    if probe('get-state')==b'recovery':mode='recovery';break
    result=cmd(['fastboot','-s',c['serial'],'getvar','product'],timeout=5,check=False)
    if result.returncode==0:
     need(re.search(rb'product:\s*A04BR3(?:\s|$)',result.stdout),'Unexpected target product');mode='fastboot';break
   except subprocess.TimeoutExpired:pass
   time.sleep(min(a.poll_seconds,max(0,deadline-time.monotonic())))
  need(mode is not None,'Transport wait expired; no capture or storage write performed')
  if cancelled():return
  if mode=='fastboot':
   try:cmd(['fastboot','-s',c['serial'],'oem','reboot-recovery'],'to-recovery.log')
   except (subprocess.TimeoutExpired,subprocess.CalledProcessError) as exc:
    state['recovery_transition_ack']=type(exc).__name__+'; observing Recovery without resending';save()
  ready=time.monotonic()+120
  while time.monotonic()<ready:
   if cancelled():return
   if probe('get-state')==b'recovery':break
   time.sleep(2)
  need(probe('get-state')==b'recovery','Recovery did not appear')
  if probe('shell','id','-u')!=b'0':
   try:adb('root',name='adb-root.log',timeout=10,check=False)
   except subprocess.TimeoutExpired:pass
  ready=time.monotonic()+30
  while time.monotonic()<ready:
   if cancelled():return
   if probe('shell','id','-u')==b'0':break
   time.sleep(1)
  need(probe('shell','id','-u')==b'0','Recovery root not ready')
  if cancelled():return
  need(all(sha(p)==h for p,h in tool_pins.items()),'Capture tool inputs changed while waiting')
  recovery=load('rejected_capture_backend',capture).Recovery(c['serial']);identity=recovery.identity();recovery.require_unmounted(identity)
  if cancelled():return
  state.update(status='capturing-new-generation',recovery_boot_id=identity['recovery_boot_id']);save()
  adb('push',str(ntfs),'/tmp/rt-ntfs3g',name='push-ntfs-helper.log',timeout=120);adb('shell','chmod','755','/tmp/rt-ntfs3g')
  args=[sys.executable,'-B',str(capture),'capture','--serial',c['serial'],'--out',str(fresh)]
  for name in engine.TARGET_SYS:args+=['--target-sys',name+'.sys']
  args+=['--allow-missing-sys','--engine-probe-state','--sd-component-state','--allow-missing-engine-service-log','--mount-driver','ntfs-3g','--ntfs3g-remote','/tmp/rt-ntfs3g']
  cmd(args,'capture.log',timeout=1800)
  mf=fresh/'manifest.json';m=read(mf);need(m['status']=='captured-unmounted' and m['unmounted'] is True and m['read_only'] is True and m['errors']==[],'New read-only capture incomplete')
  cmd([sys.executable,'-B',str(HERE/'capture-peripheral-preservation.py'),'--manifest',str(mf),'--manifest-sha256',sha(mf),'--out',str(supp),'--gpu-runtime-assets','--include-system-dxgi'],'supplement.log',timeout=1800)
  cmd([sys.executable,'-B',str(HERE/'capture-usb1121-existing-exe.py'),'--manifest',str(mf),'--manifest-sha256',sha(mf),'--out',str(usb),'--include-usb1121-existing-exe'],'usb-witness.log',timeout=1800)
  state.update(status='new-capture-and-supplements-ready',capture_manifest=str(mf.relative_to(root)),capture_manifest_sha256=sha(mf),supplement_sha256=sha(supp/'manifest.json'),usb_witness_sha256=sha(usb/'manifest.json'));save()
 except Exception as exc:
  state.update(status='stopped-preserve-evidence',error=str(exc));save();raise
if __name__=='__main__':main()
