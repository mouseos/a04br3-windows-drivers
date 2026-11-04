#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the observational shim in a new private directory; no real GPU I/O."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess

HERE=pathlib.Path(__file__).resolve().parent
PROBE_PIN='5980bb779d837b9a00e03b799dc8119bee2cae4b'
def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def git(path,*args): return subprocess.check_output(['git','-C',str(path),*args],text=True).strip()
def prepare_scratchless(src,out):
    """Make an isolated probe copy with a validated alternate vertex shader."""
    shader=HERE/'shaders/triangle-scratchless.vert'
    glsl=shutil.which('glslangValidator');validator=shutil.which('spirv-val')
    if not glsl or not validator:raise ValueError('shader tools unavailable')
    binary=out/'triangle-scratchless.spv'
    subprocess.run([glsl,'-V','--target-env','vulkan1.0','-o',str(binary),str(shader)],check=True,stdout=subprocess.DEVNULL)
    subprocess.run([validator,'--target-env','vulkan1.0',str(binary)],check=True)
    data=binary.read_bytes()
    if not data or len(data)%4:raise ValueError('invalid SPIR-V')
    words=struct.unpack('<%dI'%(len(data)//4),data)
    root=src/'package/powervr/user';dest=out/'private-probe-source';dest.mkdir(mode=0o700)
    for name in ('powervrprobed_vulkan.c','powervrprobed.h','powervrprobed_roa.h','powervrprobed_roa_spv.h'):
        shutil.copyfile(root/name,dest/name)
    header=(root/'powervrprobed_triangle_spv.h').read_text()
    replacement='static const uint32_t kTriangleVertSpv[] = {\n'+''.join(
        '    '+', '.join('0x%08xu'%w for w in words[i:i+8])+',\n' for i in range(0,len(words),8))+'};'
    header,count=re.subn(r'static const uint32_t kTriangleVertSpv\[\] = \{.*?\};',replacement,header,flags=re.S)
    if count!=1:raise ValueError('unexpected shader header')
    (dest/'powervrprobed_triangle_spv.h').write_text(header)
    return dest/'powervrprobed_vulkan.c',dest/'powervrprobed_triangle_spv.h',{
        'source_sha256':sha(shader),'spirv_sha256':sha(binary),
        'glslang_sha256':sha(pathlib.Path(glsl).resolve()),'spirv_val_sha256':sha(pathlib.Path(validator).resolve())}
def main():
    p=argparse.ArgumentParser()
    for name in ('mesa-source','mesa-build','shim','probe-source','output'):
        p.add_argument('--'+name,required=True,type=pathlib.Path)
    p.add_argument('--diagnostic',type=pathlib.Path)
    p.add_argument('--dump-control-stream',action='store_true')
    p.add_argument('--pco-fill',choices=('00','ff'))
    p.add_argument('--pco-ir',action='store_true',help='record private compiler IR in probe.log')
    p.add_argument('--scratchless-vertex',action='store_true',help='use isolated scratchless triangle shader')
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
    variant=None
    if a.scratchless_vertex:
        if not diagnostic or not diagnostic.get('private_compiler_candidates') or not a.pco_fill:
            raise SystemExit('scratchless variant requires private compiler diagnostic')
        probe_src,header,variant=prepare_scratchless(src,out)
    binary=out/'probe'
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-I'+str(mesa/'include'),str(HERE/'vulkan-capture-probe.c'),str(probe_src),'-ldl','-o',str(binary)],check=True)
    env=os.environ.copy()
    env.pop('PCO_DEBUG_PRINT',None)
    if a.pco_ir:
        if not diagnostic or not diagnostic.get('private_compiler_candidates') or not a.pco_fill:
            raise SystemExit('PCO IR requires the private compiler diagnostic')
        env['PCO_DEBUG_PRINT']='vs,fs,nir,passes'
    env.pop('A04_PCO_FILL',None)
    env.pop('A04_PCO_CANDIDATE_DIR',None)
    if diagnostic and diagnostic.get('private_compiler_candidates'):
        (out/'private-compiler-candidates').mkdir(mode=0o700)
        env['A04_PCO_CANDIDATE_DIR']=str(out/'private-compiler-candidates')
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
    env.pop('A04_PDS_CODE_HASH',None)
    if diagnostic and diagnostic.get('pds_code_hash_metadata_only'):
        (out/'pds-code-hashes.jsonl').touch(mode=0o600,exist_ok=False)
        env['A04_PDS_CODE_HASH']=str(out/'pds-code-hashes.jsonl')
    env.pop('A04_VDM_HASH',None)
    if diagnostic and diagnostic.get('vdm_hash_metadata_only'):
        if not a.dump_control_stream:raise SystemExit('VDM hash diagnostic requires control stream dump')
        (out/'vdm-hashes.jsonl').touch(mode=0o600,exist_ok=False)
        env['A04_VDM_HASH']=str(out/'vdm-hashes.jsonl')
    env.pop('A04_PPP_HASH',None)
    if diagnostic and diagnostic.get('ppp_hash_metadata_only'):
        if not a.dump_control_stream:raise SystemExit('PPP hash diagnostic requires control stream dump')
        (out/'ppp-hashes.jsonl').touch(mode=0o600,exist_ok=False)
        env['A04_PPP_HASH']=str(out/'ppp-hashes.jsonl')
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
    if variant:manifest['scratchless_vertex']=variant
    if a.pco_ir: manifest['private_pco_ir']=True
    if diagnostic and diagnostic.get('private_compiler_candidates'):
        manifest['private_compiler_sha256']={p.name:sha(p) for p in sorted((out/'private-compiler-candidates').glob('*.bin'))}
    if diagnostic and diagnostic.get('upload_metadata_only'):
        manifest['uploads_sha256']=sha(out/'uploads.jsonl')
    if diagnostic and diagnostic.get('pds_code_hash_metadata_only'):
        manifest['pds_code_hashes_sha256']=sha(out/'pds-code-hashes.jsonl')
    if diagnostic and diagnostic.get('vdm_hash_metadata_only'):
        manifest['vdm_hashes_sha256']=sha(out/'vdm-hashes.jsonl')
    if diagnostic and diagnostic.get('ppp_hash_metadata_only'):
        manifest['ppp_hashes_sha256']=sha(out/'ppp-hashes.jsonl')
    (out/'run-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('capture completed; probe return code:',result.returncode)
if __name__=='__main__': main()
