#!/usr/bin/env python3
"""Compile production UMD shared create/open and KMD create/open/close on a host.
Only OS allocation/handle/synchronization services and native owner ops are mocked.
No hardware, scheduler concurrency, cross-process OS integration or DWM claim.
"""
import argparse, pathlib, subprocess
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'drivers/gpu/mt8167/wddm-render'
a=argparse.ArgumentParser();a.add_argument('--output',type=pathlib.Path,required=True);out=a.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=True)
def section(file,begin,end):
 s=(p/file).read_text();return s[s.index(begin):s.index(end,s.index(begin))]
code=(root/'tests/shared_surface_host_shim.h').read_text()
code+=section('umd/clear_ddi.c','HRESULT geo_umd_create_shared(', 'HRESULT geo_umd_open_primary(')
code+=section('kmd_clear.c','static NTSTATUS GeoCreateAllocationImpl(', 'static NTSTATUS GeoBuildPagingBufferImpl(')
code+=section('kmd_objects.c','NTSTATUS APIENTRY GeoOpenAllocation(', 'struct geo_allocation *GeoResolveReadAllocation(')
code+=(root/'tests/shared_surface_host_cases.h').read_text()
c=out/'production-shared.c';c.write_text(code)
for unified in [0,1]:
 exe=out/f'shared-{unified}'
 cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-multichar','-fsanitize=address,undefined','-fno-omit-frame-pointer',f'-DA04_GE8300_UNIFIED_DISPLAY356={unified}','-I'+str(p),str(c),str(p/'clear_engine.c'),str(p/'clear_protocol.c'),'-o',str(exe)]
 subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
print('PASS production shared UMD/KMD descriptor, existing owner identity, process-open lifetime; mocked OS/native services; both geometries')
