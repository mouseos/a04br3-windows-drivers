#!/usr/bin/env python3
"""Run the real controller with fake transport; no device/VM process may run."""
import copy,importlib.util,json,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
repo=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('hold_controller',repo/'tools/recovery/run-gpu368-fresh200.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def pb(i,e,code):return ('PB %08X %08X %08X\n'%(i,e,code)).encode()
def start(run,pid):return b'A04TRACE ExitBootServices event signaled\n'+pb(0,0x286,run)+pb(0,0x287,pid)
def terminal(code=0,phase=1,flush=0):
 raw=start(11,22)
 if phase:
  raw+=pb(0,0x288,0)+pb(0,0x207,3010)+pb(0,0x10b,3010)
  raw+=b'BOOT_REASON=00000001\nfastboot: processing commands\n'+start(33,44)
  for i in (0,1):raw+=pb(i,0x220,100+i)+pb(i,0x221,code if i==0 else 0)
 return raw+pb(0,0x288,flush)+pb(0,0x207,code)
class Fixture:
 def __init__(self,root,raw,profile='run201'):
  self.root=root;self.raw=raw;self.profile=profile;self.k=m.contract(profile);self.calls=[]
  self.out=root/self.k['runtime_output'];self.out.parent.mkdir(parents=True)
  self.uart=self.write('port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log',b'')
  self.seal=self.write('seal.json',b'host-seal')
  self.write('media/update-esp.img',b'fake-esp');self.write('normal.img',b'fake-normal')
  self.write('target-tools.sh',b'host-only-never-executed')
  self.write('crc.py',b"def decode(raw,token):return b'UPDATE_STATUS=COMMITTED\\nUPDATE_RELEASE_EXIT=0\\nUPDATE_EXIT=0\\n'\n")
  self.c=dict(self.k,planned_windows_reboots=1,media_directory='media',normal_esp='normal.img',target_tools='target-tools.sh',reset_script='reset.sh',serial='HOST_FIXTURE',fresh_manifest='fresh.json',capture_tool='capture.py',crc_decoder='crc.py',ntfs3g='ntfs3g',diagnostics_collector='collector.sh',prepare_token='A'*16,terminal_settle_seconds=3,
              fresh_manifest_sha256='f'*64,policy_sha256='a'*64,esp_sha256=m.sha(root/'media/update-esp.img'),normal_esp_sha256=m.sha(root/'normal.img'))
  self.c['pinned_files']=[dict(path='target-tools.sh',sha256=m.sha(root/'target-tools.sh'))]
  self.args=['--workspace',str(root),'--seal',str(self.seal),'--seal-sha256',m.sha(self.seal),'--output',str(self.out)]
 def write(self,name,data):
  p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);return p
 def fake_run(self,args,**kwargs):
  name=Path(kwargs['stdout'].name).name;self.calls.append((name,args))
  if args[:2]==['bash','-c']:
   # Accept only controller's known transport operations, never execute shell.
   allowed={'prepare-write.log','prepare-to-recovery.log','normal-esp-write.log','planned-restart-resume.log','terminal-to-recovery.log','prepare-capture%d.log'%self.k['capture']}
   assert name in allowed,name
   if name=='prepare-write.log':self.uart.write_bytes(b'A04AU2E '+b'A'*16+b' 00000000\n')
   elif name=='normal-esp-write.log':
    with self.uart.open('ab') as f:f.write(self.raw)
  elif args[:2]==['python3','-B']:
   assert args[3]=='capture',args
   self.write(self.c['capture_output']+'/manifest.json',json.dumps(dict(status='captured-unmounted',read_only=True,unmounted=True,errors=[])).encode())
  else:
   assert args[0]=='bash' and args[1]==str(self.root/'collector.sh'),args
   # Same status contract as the real collector; the held result is not fed
   # directly into a collector that accepts only collection boundaries.
   state=m.read(Path(args[3]));assert state['status'] in m.COLLECTABLE,state
   self.write(self.c['diagnostics_output']+'/capture-index.json',b'{"windows_unmounted":true}')
  return subprocess.CompletedProcess(args,0)
 def run(self,*extra):
  with patch.object(m,'validate_seal',return_value=(self.c,{})),patch.object(m,'run_command_group',side_effect=self.fake_run),patch.object(m.subprocess,'run',side_effect=self.fake_run),patch.object(m.subprocess,'check_output',side_effect=AssertionError('unexpected subprocess')),patch.object(m.time,'sleep'):
   m.main(self.profile,argv=self.args+list(extra))
 def names(self):return [n for n,_ in self.calls]
 def result(self):return m.read(self.out/'result.json')
 def latch(self):return self.root/('port/gsi/logs/ios-feasibility/gpu368-run%d-executions/'%self.k['run'])/(self.k['transaction']+'.json')
class HoldTest(unittest.TestCase):
 def setUp(self):
  (repo/'build').mkdir(exist_ok=True);self.temp=tempfile.TemporaryDirectory(dir=repo/'build',prefix='hold-');self.root=Path(self.temp.name)
 def tearDown(self):self.temp.cleanup()
 def held(self,code=0,phase=1,profile='run201'):
  f=Fixture(self.root,terminal(code,phase),profile);f.run();r=f.result()
  self.assertEqual(r['status'],m.HOLD_STATUS);self.assertEqual(r['terminal']['code'],code)
  self.assertFalse(r['hardware_success']);self.assertTrue(r['safe_flush_terminal'])
  self.assertNotIn('terminal-to-recovery.log',f.names());self.assertFalse((self.root/f.c['capture_output']).exists())
  return f
 def test_success_hold_and_collect_only_preserves_originals_and_latch(self):
  f=self.held(profile='run203');paths=[f.out/'result.json',f.out/'terminal-proof.json',f.out/'normal-uart-deinterleaved.log',f.latch()]
  original={p:p.read_bytes() for p in paths};f.calls.clear()
  f.run('--collect-only','--validate-only');self.assertEqual(f.calls,[])
  self.assertFalse((f.out/'collection').exists());f.run('--collect-only')
  self.assertEqual(f.names(),['terminal-to-recovery.log','prepare-capture204.log','capture204.log','collect-combined200.log'])
  self.assertTrue(m.read(f.out/'collection/result.json')['collection_only'])
  for p,data in original.items():self.assertEqual(p.read_bytes(),data)
  f.calls.clear()
  with self.assertRaises((ValueError,FileExistsError)):f.run('--collect-only')
  self.assertEqual(f.calls,[])
 def test_failed_diagnostic_terminal_holds_and_resume_collects_without_reexecution(self):
  f=self.held(21);f.calls.clear();f.run('--resume-collect')
  self.assertNotIn('prepare-write.log',f.names());self.assertNotIn('normal-esp-write.log',f.names())
  self.assertEqual(m.read(f.out/'collection/result.json')['collection_boundary'],'diagnostic-terminal-await-disk-verification')
 def test_failed_install_terminal_holds_without_planned_restart(self):
  f=self.held(31,0);self.assertNotIn('planned-restart-resume.log',f.names())
  f.calls.clear();f.run('--collect-only')
  self.assertEqual(m.read(f.out/'collection/result.json')['collection_boundary'],'install-failed-await-disk-verification')
 def test_flush_failure_never_holds_or_recovers(self):
  f=Fixture(self.root,terminal(21,flush=5))
  with self.assertRaises(RuntimeError):f.run()
  self.assertNotIn('terminal-to-recovery.log',f.names());self.assertFalse((f.out/'terminal-proof.json').exists())
 def test_panic_and_unexpected_restart_keep_recovery_even_in_terminal_chunk(self):
  cases=[(start(1,2)+b'A04WIN BUGCHECK DEAD0001\n','runtime-exception-await-collection'),
         (terminal()+b'A04WIN BUGCHECK DEAD0001\n','runtime-exception-await-collection'),
         (terminal()+b'BOOT_REASON=00000001\n','unexpected-second-runtime-restart'),
         (terminal()+b'BOOT_REASON=','unexpected-second-runtime-restart'),
         (start(1,2)+b'BOOT_REASON=00000001\n','runtime-restarted-requires-install-disk-proof')]
  for i,(raw,boundary) in enumerate(cases):
   with self.subTest(boundary=boundary,i=i):
    f=Fixture(self.root/str(i),raw);f.run()
    self.assertIn('terminal-to-recovery.log',f.names());self.assertEqual(f.result()['collection_boundary'],boundary)
    self.assertFalse((f.out/'terminal-proof.json').exists())
 def test_tampering_or_candidate_rerun_rejected_before_transport(self):
  f=self.held();uart=f.out/'normal-uart-deinterleaved.log';original=uart.read_bytes();latch=f.latch().read_bytes();f.calls.clear()
  with self.assertRaises(ValueError):f.run()
  self.assertEqual(f.calls,[]);self.assertEqual(f.latch().read_bytes(),latch)
  uart.write_bytes(original+b'changed\n')
  with self.assertRaises(ValueError):f.run('--collect-only')
  self.assertEqual(f.calls,[]);uart.write_bytes(original)
  p=f.out/'terminal-proof.json';proof=m.read(p);proof['terminal']['flush']=5;p.write_text(json.dumps(proof))
  with self.assertRaises(ValueError):f.run('--collect-only')
  self.assertEqual(f.calls,[])
  # Recompute hashes after changing metadata: semantic UART/flush replay,
  # rather than a digest mismatch alone, must still reject collection.
  state=f.result();state['terminal']=copy.deepcopy(proof['terminal']);state['terminal_proof_sha256']=m.sha(p)
  (f.out/'result.json').write_text(json.dumps(state))
  with self.assertRaises(ValueError):f.run('--collect-only')
  self.assertEqual(f.calls,[])
if __name__=='__main__':unittest.main()
