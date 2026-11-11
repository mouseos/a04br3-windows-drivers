#!/usr/bin/env python3
"""One host suite: real C gates, default hold, bounded fake transport, proof decoder."""
import copy,importlib.util,json,os,struct,subprocess,sys,tempfile,time,unittest
from pathlib import Path
REPO=Path(__file__).resolve().parents[1];sys.path.insert(0,str(REPO/'tools/recovery'))
import collection_reboot as cr

def load(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
engine=load('engine',REPO/'tools/recovery/run-gpu368-fresh200.py')
decoder=load('decoder',REPO/'tools/recovery/decode-collection-reboot.py')
old=load('hold',REPO/'tests/gpu368_terminal_hold_test.py')
class CollectionTest(unittest.TestCase):
 def setUp(self):
  (REPO/'build').mkdir(exist_ok=True);self.temp=tempfile.TemporaryDirectory(dir=REPO/'build');self.out=Path(self.temp.name)
 def tearDown(self):self.temp.cleanup()
 def terminal(self):return dict(phase=1,flush=0,run=11,PID=22,code=1,clients={0:dict(pid=100,result=1),1:dict(pid=101,result=4)})
 def uart(self):
  return b''.join(('PB 00000000 %08X %08X\n'%(e,v)).encode() for e,v in [(0x2d3,11),(0x2d4,22)]+[(0x2e0+i,0xaaaaaaaa) for i in range(8)]+[(0x2d0,1),(0x2d1,0)])
 def test_real_c_contract(self):
  binary=self.out/'contract';subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(REPO/'tests/collection_reboot_contract_test.c'),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
 def test_independent_decoder(self):
  raw=struct.pack('<14I',0x31524341,1,216,11,22,1,1,0,0,0,0,0,2,0)+b'a'*64+b'\0'*96
  self.assertEqual(decoder.decode(raw)['result'],1)
  for bad in (raw[:-1],raw[:4]+struct.pack('<I',2)+raw[8:],raw[:-4]+b'\1\0\0\0'):
   with self.assertRaises(ValueError):decoder.decode(bad)
 def test_default_and_pending_admission(self):
  self.assertIsNone(cr.admit({}, {},False));self.assertEqual(cr.macros({}),[])
  with self.assertRaises(ValueError):cr.admit({}, {},True)
  for cap in (None,231):
   p=dict(profile='pending',draft=True,main_execution_approved=False,capture_number=cap)
   with self.assertRaises(ValueError):cr.enable_next_profile(p)
  # Synthetic unit fixture, never an actual capture/profile output.
  p=dict(profile='test-fixture',capture_number=999,draft=False,main_execution_approved=True,service='TEST',data_directory='TEST',first_install_mask=0,deferred_unbound_mask=0,diagnostic_clients=[dict(name=x,phase=1) for x in ('gpu_present','gpu_globaldiag')])
  q=cr.enable_next_profile(p);self.assertEqual(len(cr.macros(q)),2)
  snapshot=cr.enable_next_profile(p,vidpn_uart_snapshot=True);self.assertEqual(len(cr.macros(snapshot)),3);self.assertIn('A04_VIDPN_UART_SNAPSHOT=1',cr.compile_macros(snapshot))
  self.assertTrue({'tools/winpe/vidpn-uart-snapshot.h','tools/winpe/collection-vidpn-uart.h'}.issubset(cr.SOURCE_PATHS))
  self.assertEqual(cr.admit(dict(collection_reboot=cr.CONFIG),q,True),cr.CONFIG)
  with self.assertRaises(ValueError):cr.admit({},q,True)
  closure=cr.source_closure(REPO);sm=dict(collection_reboot=dict(schema='MT8167_COLLECTION_REBOOT_COMPILE_V1',config=cr.CONFIG,headers=closure,macros=['A04_COLLECTION_REBOOT=1','A04_COLLECTION_REBOOT_PROFILE=1']))
  cr.verify_source_pins(REPO,dict(pinned_files=closure),q,sm)
  with self.assertRaises(ValueError):cr.verify_source_pins(REPO,dict(pinned_files=[]),q,sm)
  checkout=self.out/'windows-rt/test-source'
  for ref in closure:
   dest=checkout/ref['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((REPO/ref['path']).read_bytes())
  pins=[dict(path=(checkout/r['path']).relative_to(self.out).as_posix(),sha256=r['sha256']) for r in closure]
  cr.verify_source_pins(checkout,dict(pinned_files=pins),q,sm,workspace=self.out)
  with self.assertRaises((ValueError,FileNotFoundError)):cr.verify_source_pins(checkout,dict(pinned_files=closure),q,sm,workspace=self.out)
  altered=copy.deepcopy(pins);altered[0]['sha256']='0'*64
  with self.assertRaises(ValueError):cr.verify_source_pins(checkout,dict(pinned_files=altered),q,sm,workspace=self.out)

 def test_proof_failure_and_partial(self):
  raw=self.uart();self.assertTrue(cr.proof_events(raw,self.terminal(),'a'*64));self.assertFalse(cr.proof_events(raw[:-35],self.terminal(),'a'*64))
  for bad in (raw.replace(b'0000000B',b'0000000C'),raw.replace(b'AAAAAAAA',b'BBBBBBBB'),raw.replace(b'000002D1 00000000',b'000002D1 00000005'),raw+b'PB 00000000 000002D2 00000005\n'):
   with self.assertRaises(ValueError):cr.proof_events(bad,self.terminal(),'a'*64)
 def test_probe_transient_then_success_and_deadline(self):
  uart=self.out/'uart';uart.write_bytes(self.uart());c=dict(collection_reboot=cr.CONFIG,policy_sha256='a'*64,target_tools='mock',serial='MOCK');now=[0.];calls=[]
  def clock():return now[0]
  def sleep(t):now[0]+=t
  def run(args,**kw):
   calls.append(args);self.assertIn('timeout --foreground',args[2]);self.assertNotIn('RST',args[2]);self.assertNotIn('reboot',args[2])
   if len(calls)==1:raise subprocess.TimeoutExpired(args,kw['timeout'])
   return subprocess.CompletedProcess(args,0)
  r=cr.wait_fastboot(self.out,c,self.out,self.terminal(),uart,0,lambda x:x,run,engine.BOUNDED_DEVICE_SHELL,clock,sleep);self.assertFalse(r['hardware_success']);self.assertEqual(len(calls),2)
  uart.write_bytes(b'');r=cr.wait_fastboot(self.out,c,self.out,self.terminal(),uart,0,lambda x:x,run,engine.BOUNDED_DEVICE_SHELL,clock,sleep)
  self.assertIsNone(r['Windows_shutdown_API_return0']);self.assertFalse(r['UART_shutdown_proof_complete']);self.assertEqual(r['collection_transport_status'],'fastboot-observed-shutdown-unproven')
  incomplete=self.terminal();incomplete['clients']={}
  r=cr.wait_fastboot(self.out,c,self.out,incomplete,uart,0,lambda x:x,run,engine.BOUNDED_DEVICE_SHELL,clock,sleep);self.assertIsNone(r['Windows_shutdown_API_return0']);self.assertFalse(r['hardware_success'])
  uart.write_bytes(b'A04WIN BUGCHECK\n'+self.uart().replace(b'000002D1 00000000',b'000002D1 00000005'))
  r=cr.wait_fastboot(self.out,c,self.out,self.terminal(),uart,0,lambda x:x,run,engine.BOUNDED_DEVICE_SHELL,clock,sleep)
  self.assertIsNone(r['Windows_shutdown_API_return0']);self.assertTrue(r['runtime_fault_observed']);self.assertIsNotNone(r['UART_proof_validation_error']);self.assertFalse(r['hardware_success'])
  c['vidpn_uart_snapshot']=cr.SNAPSHOT_CONFIG
  observed_snapshot=cr.save_vu_snapshot(c,self.out,self.terminal(),b'no VU snapshot\n')
  self.assertFalse(observed_snapshot['transport_complete']);self.assertTrue(observed_snapshot['current_boot_unproven']);self.assertFalse(observed_snapshot['hardware_success'])
  self.assertTrue((self.out/'vidpn-vu-snapshot.json').exists())
  uart.write_bytes(self.uart())

  def absent(*a,**kw):now[0]+=60;raise subprocess.TimeoutExpired(a,1)
  with self.assertRaises(RuntimeError):cr.wait_fastboot(self.out,c,self.out,self.terminal(),uart,0,lambda x:x,absent,engine.BOUNDED_DEVICE_SHELL,clock,sleep)
 def test_process_group_timeout_reaps_descendant(self):
  pidfile=self.out/'pid';other=subprocess.Popen(['sleep','20'])
  try:
   with self.assertRaises(subprocess.TimeoutExpired):engine.run_command_group(['bash','-c','trap "" TERM; sleep 20 & echo $! > "$1"; wait','test',str(pidfile)],timeout=.3,stdout=subprocess.DEVNULL)
   child=int(pidfile.read_text());stat=Path('/proc')/str(child)/'stat'
   for _ in range(30):
    try:state=stat.read_text().split()[2]
    except FileNotFoundError:break
    if state=='Z':break
    time.sleep(.02)
   else:self.fail('descendant survived timeout')
   self.assertIsNone(other.poll())
  finally:other.terminate();other.wait()
if __name__=='__main__':
 suite=unittest.TestSuite([CollectionTest(x) for x in sys.argv[1:]]) if len(sys.argv)>1 else unittest.TestSuite([unittest.defaultTestLoader.loadTestsFromTestCase(CollectionTest),unittest.defaultTestLoader.loadTestsFromTestCase(old.HoldTest)])
 result=unittest.TextTestRunner(verbosity=2).run(suite);sys.exit(not result.wasSuccessful())
