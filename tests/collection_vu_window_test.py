#!/usr/bin/env python3
"""One host integration run reusing existing real8715 formatter bytes; no build/device."""
import hashlib,importlib.util,json,re,subprocess,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[1];sys.path.insert(0,str(repo/'tools/recovery'))
import collection_reboot as cr
sp=importlib.util.spec_from_file_location('engine',repo/'tools/recovery/run-gpu368-fresh200.py');engine=importlib.util.module_from_spec(sp);sp.loader.exec_module(engine)
def main():
 fixture=Path(sys.argv[1]).resolve(strict=True);out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=False)
 raw=fixture.read_bytes();prefix=b'prior UART preserved\n';policy='a'*64
 terminal=dict(phase=1,flush=0,run=51760,PID=1428,code=1,clients={0:dict(pid=100,result=1),1:dict(pid=101,result=4)})
 events=[(0x2d3,51760),(0x2d4,1428)]+[(0x2e0+i,0xaaaaaaaa) for i in range(8)]+[(0x2d0,1),(0x2d1,0)]
 shutdown=b''.join(('PB 00000000 %08X %08X\n'%(e,v)).encode() for e,v in events)
 c=dict(collection_reboot=cr.CONFIG,vidpn_uart_snapshot=cr.SNAPSHOT_CONFIG,policy_sha256=policy,target_tools='NEVER_EXECUTED',serial='HOST_FIXTURE')
 def run(args,**kw):
  assert 'timeout --foreground' in args[2] and 'getvar product' in args[2] and 'RST' not in args[2]
  return subprocess.CompletedProcess(args,0)
 checks=[]
 def case(name,data,accepted):
  d=out/name;d.mkdir();uart=d/'source-uart';uart.write_bytes(prefix+data)
  result=cr.wait_fastboot(out,c,d,terminal,uart,len(prefix),lambda b:re.sub(engine.STORAGE_TRACE,b'',b),run,engine.BOUNDED_DEVICE_SHELL,storage_trace=engine.STORAGE_TRACE)
  side=json.loads((d/'vidpn-vu-window.json').read_text());decoded=json.loads((d/'vidpn-vu-snapshot.json').read_text())
  assert decoded['transport_complete']==accepted and decoded['current_boot_unproven'] and not decoded['hardware_success']
  assert side['normal_offset']==len(prefix) and side['normal_end']==len(prefix)+len(data)
  assert side['full_raw_sha256']==hashlib.sha256(data).hexdigest()
  assert (d/'collection-reboot-uart-raw.log').read_bytes()==data and uart.read_bytes()==prefix+data
  selected=(d/'vidpn-vu-window.uart').read_bytes();assert side['window_sha256']==hashlib.sha256(selected).hexdigest()
  for marker in side['boot_markers']:assert uart.read_bytes()[marker['original_start']:marker['original_end']]==b'BOOT_REASON'
  bounds=side['window'];assert re.sub(engine.STORAGE_TRACE,b'',uart.read_bytes()[bounds['original_start']:bounds['original_end']])==selected
  assert result['fastboot_identity_verified'] and not result['hardware_success']
  checks.append(name);return side,decoded
 noise=b'A04WIN MSDC noise\n';boot=b'BOOT_REASON=00000004\n'
 data=noise+raw+shutdown+boot+b'fastboot: processing commands\n'
 # Production callsite path, complete VU followed by a normal requested reboot.
 side,decoded=case('complete-then-BOOT_REASON',data,True)
 assert side['normal_shutdown_API_return0_recorded'] and side['reboot_classification']=='shutdown-API0-and-reboot-observed'
 assert not side['post_boundary_VU_present'] and len(decoded['records'])==15
 case('missing-END-before-boot',noise+b''.join(raw.splitlines(keepends=True)[:-1])+boot,False)
 case('two-streams-before-boot',noise+raw+raw+boot,False)
 side,_=case('later-boot-stream-visible',noise+raw+boot+raw,True);assert side['post_boundary_VU_present'] and side['later_vu_candidates']
 side,_=case('exception-reboot-visible',noise+raw+b'A04WIN BUGCHECK\n'+boot,True);assert side['exception_before_boundary_observed'] and side['reboot_classification']=='exception-evidence-and-reboot-observed'
 # Exact offset mapping when the existing storage deinterleaver removes an inline trace.
 interleaved=raw[:30]+b'A04WIN StartIo fn=00000001 opcode=00000002\n'+raw[30:]
 side,_=case('storage-deinterleave-provenance',noise+interleaved+boot,True);assert side['removed_storage_ranges']
 (out/'result.json').write_text(json.dumps(dict(passed=True,checks=checks,fixture_sha256=hashlib.sha256(raw).hexdigest(),host_compile_count=0,C_compile_count=0,device_operations=0),indent=2)+'\n')
 print('PASS: existing formatter bytes, production wait_fastboot window integration; %d checks'%len(checks))
if __name__=='__main__':main()
