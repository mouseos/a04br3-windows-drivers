#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the observational shim in a new private directory; no real GPU I/O."""
import argparse
import hashlib
import json
import os
import pathlib
import subprocess

HERE=pathlib.Path(__file__).resolve().parent
PROBE_PIN='5980bb779d837b9a00e03b799dc8119bee2cae4b'
def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def git(path,*args): return subprocess.check_output(['git','-C',str(path),*args],text=True).strip()
def main():
    p=argparse.ArgumentParser()
    for name in ('mesa-source','mesa-build','shim','probe-source','output'):
        p.add_argument('--'+name,required=True,type=pathlib.Path)
    p.add_argument('--diagnostic',type=pathlib.Path)
    p.add_argument('--dump-control-stream',action='store_true')
    p.add_argument('--pco-fill',choices=('00','ff'))
    a=p.parse_args(); src=a.probe_source.resolve(); mesa=a.mesa_source.resolve(); build=a.mesa_build.resolve(); shim=a.shim.resolve(); out=a.output.resolve()
    if git(src,'rev-parse','HEAD') != PROBE_PIN or git(src,'status','--porcelain','--untracked-files=all'):
        raise SystemExit('probe source must be clean pinned commit')
    meta=json.loads((shim/'build-manifest.json').read_text())
    lib=build/'src/imagination/vulkan/libvulkan_powervr_mesa.so'
    if sha(shim/'shim.so') != meta['shim_sha256'] or sha(shim/'enumeration.so') != meta['enumeration_sha256'] or sha(lib) != meta['vulkan_sha256']:
        raise SystemExit('build artifact hash mismatch')
    diagnostic=None
    if a.diagnostic:
        diag=a.diagnostic.resolve()
        diagnostic=json.loads((diag/'diagnostic-manifest.json').read_text())
        if (diagnostic['mesa_commit'] != meta['mesa_commit'] or
            diagnostic['base_build_ninja_sha256'] != meta['build_ninja_sha256'] or
            sha(diag/'libvulkan_powervr_mesa.so') != diagnostic['library_sha256']):
            raise SystemExit('diagnostic library mismatch')
        lib=diag/'libvulkan_powervr_mesa.so'
    if git(mesa,'rev-parse','HEAD') != meta['mesa_commit'] or git(mesa,'status','--porcelain','--untracked-files=all'):
        raise SystemExit('Mesa source mismatch or dirty')
    out.mkdir(mode=0o700,parents=True,exist_ok=False)
    probe_src=src/'package/powervr/user/powervrprobed_vulkan.c'
    header=src/'package/powervr/user/powervrprobed_triangle_spv.h'
    binary=out/'probe'
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-I'+str(mesa/'include'),str(HERE/'vulkan-capture-probe.c'),str(probe_src),'-ldl','-o',str(binary)],check=True)
    env=os.environ.copy()
    env.pop('A04_PCO_FILL',None)
    if a.pco_fill:
        if not diagnostic or not diagnostic.get('usc_compiler_metadata_only'):
            raise SystemExit('PCO fill requires compiler diagnostic')
        env['A04_PCO_FILL']=a.pco_fill
    elif diagnostic and diagnostic.get('usc_compiler_metadata_only'):
        raise SystemExit('PCO diagnostic requires explicit fill')
    env.pop('PVR_DEBUG',None)
    if a.dump_control_stream: env['PVR_DEBUG']='cs'
    for key in ('PVR_SHIM_QUIRKS','PVR_SHIM_ENHANCEMENTS','PVR_SHIM_MUSTHAVE_QUIRKS'):
        env.pop(key, None)
    env.update(PVR_SHIM_DEVICE_BVNC='22.67.54.30', MESA_SHADER_CACHE_DISABLE='true', LD_PRELOAD=str(shim/'enumeration.so') + ':' + str(shim/'shim.so'), A04_CAPTURE_JSONL=str(out/'events.jsonl'), PVR_I_WANT_A_BROKEN_VULKAN_DRIVER='1')
    if diagnostic and diagnostic.get('upload_metadata_only'):
        (out/'uploads.jsonl').touch(mode=0o600,exist_ok=False)
        env['A04_UPLOAD_TRACE']=str(out/'uploads.jsonl')
    with (out/'probe.log').open('w') as log:
        result=subprocess.run([str(binary),str(lib)],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120,preexec_fn=lambda:os.umask(0o077))
    event=out/'events.jsonl'
    if not event.is_file(): raise SystemExit('probe produced no capture')
    manifest={'mesa_build_manifest':meta,'diagnostic_manifest':diagnostic,'probe_commit':PROBE_PIN,'probe_remote':git(src,'remote','get-url','origin'),
              'probe_source_sha256':sha(probe_src),'probe_header_sha256':sha(src/'package/powervr/user/powervrprobed.h'),'shader_header_sha256':sha(header),
              'harness_sha256':sha(HERE/'vulkan-capture-probe.c'),'probe_binary_sha256':sha(binary),
              'events_sha256':sha(event),'probe_log_sha256':sha(out/'probe.log'), 'control_stream_dump':a.dump_control_stream, 'probe_exit_code':result.returncode, 'shim_bvnc':'22.67.54.30',
              'note':'The shim does not execute GPU work; probe success is not expected.'}
    if a.pco_fill: manifest['pco_fill']=a.pco_fill
    if diagnostic and diagnostic.get('upload_metadata_only'):
        manifest['uploads_sha256']=sha(out/'uploads.jsonl')
    (out/'run-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('capture completed; probe return code:',result.returncode)
if __name__=='__main__': main()
