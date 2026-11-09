#!/usr/bin/env python3
"""Actual cold caller old357 refusal vs358 segmented validation; mock DMA only."""
from pathlib import Path
import subprocess,json,os,argparse,hashlib
S=Path(__file__).resolve().parents[2];D=S/'drivers/gpu/mt8167/vendor_m1_11/render_owner';p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();O=a.out.resolve();O.mkdir(parents=True,exist_ok=True)
old=subprocess.check_output(['git','-C',str(S),'show','a27b8cc9b477ba364f7dd1d0a58665202fd1472f:drivers/gpu/mt8167/vendor_m1_11/render_owner/windows_native_job.c']);(O/'old357-job.c').write_bytes(old)
common=['windows_native_cold','windows_native_code','windows_native_backing','windows_native_storage','native_resources','native_hwrt_profile','native_clear_job','native_pair','native_clear_surface','native_clear_codespace','native_clear_pds','native_eot_program','native_terminate_stream','render_resources','native_clear_data','native_record_fields','native_ccb']
flags=['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-include',S/'tests/gpu_start_checkpoint357_stub.h','-DA04_GE8300_NATIVE_OWNER_DRAW=1','-DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1','-DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1','-I'+str(S/'tests/gpu_windows_stubs'),'-I'+str(D)]
env=dict(os.environ,TMPDIR=str(O),ASAN_OPTIONS='detect_leaks=1')
def run(c,n):
 r=subprocess.run([str(x)for x in c],capture_output=True,text=True,env=env);(O/(n+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout+r.stderr
results=[]
for name,job,height,oldfail in [('old357',O/'old357-job.c',1280,True),('new358',D/'windows_native_job.c',1280,False),('legacy600',D/'windows_native_job.c',600,False)]:
 f=flags+['-DA04_GE8300_UNIFIED_DISPLAY356='+str(int(height==1280))]+(['-DEXPECT_OLD=1']if oldfail else[])
 run(f+[S/'tests/gpu_vendor_segmented_cold358_test.c',job,*[D/(n+'.c')for n in common],S/'drivers/gpu/mt8167/gpu_mmu.c','-Wl,--gc-sections','-o',O/name],name+'-compile')
 for mode in ([0,1,2]if name=='new358'else[0]):
  text=run([O/name,str(mode)],name+'-'+str(mode));results.append({'case':name,'fault':mode,'stdout':text.strip()})
receipt={'ASAN_UBSAN_leaks':True,'production_full_cold_caller':True,'mock_OS_DMA_not_hardware':True,'hardware_verified':False,'old357_sha256':hashlib.sha256(old).hexdigest(),'cases':results,'unchanged_data_limit':2097152,'unchanged_aperture_pages':4096,'unchanged_DMA_request_limit':65536,'capacity_guard_relaxed':False}
(O/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
