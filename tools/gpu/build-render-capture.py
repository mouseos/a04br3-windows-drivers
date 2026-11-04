#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the isolated observational shim against a clean, configured Mesa."""
import argparse
import hashlib
import json
import os
import pathlib
import shlex
import subprocess

PIN = 'ef11349e8e5d620a2227fac0eed17e7ed1f86428'
HERE = pathlib.Path(__file__).resolve().parent

def sha(p):
    return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

def git(src, *args):
    return subprocess.check_output(['git', '-C', str(src), *args], text=True).strip()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, type=pathlib.Path)
    ap.add_argument('--build', required=True, type=pathlib.Path)
    ap.add_argument('--output', required=True, type=pathlib.Path)
    a = ap.parse_args()
    src, build, out = (p.resolve() for p in (a.source, a.build, a.output))
    if git(src, 'rev-parse', 'HEAD') != PIN or git(src, 'status', '--porcelain', '--untracked-files=all'):
        raise SystemExit('Mesa must be the clean pinned commit')
    info = json.loads((build/'meson-info/meson-info.json').read_text())
    if pathlib.Path(info['directories']['source']).resolve() != src:
        raise SystemExit('Meson source mismatch')
    subprocess.run(['ninja', '-j4', '-C', str(build), 'src/imagination/drm-shim/libpowervr_noop_drm_shim.so',
                    'src/imagination/vulkan/libvulkan_powervr_mesa.so'], check=True)
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    base = src/'src/imagination/drm-shim/pvr_noop.c'
    text = base.read_text()
    marker = '#define PVR_IOC_NR(_name)'
    if text.count(marker) != 1: raise SystemExit('unexpected Mesa shim')
    text = text.replace(marker, '#include "' + str(HERE/'pvr-capture-hooks.h') + '"\n' + marker)
    handlers = {'CREATE_BO':'cap_create_bo', 'GET_BO_MMAP_OFFSET':'cap_mmap_bo',
       'CREATE_VM_CONTEXT':'cap_vm_create','DESTROY_VM_CONTEXT':'cap_vm_destroy',
       'VM_MAP':'cap_map','VM_UNMAP':'cap_unmap','CREATE_CONTEXT':'cap_context_create',
       'DESTROY_CONTEXT':'cap_context_destroy','CREATE_FREE_LIST':'cap_fl_create',
       'DESTROY_FREE_LIST':'cap_fl_destroy','CREATE_HWRT_DATASET':'cap_hwrt_create',
       'DESTROY_HWRT_DATASET':'cap_hwrt_destroy','SUBMIT_JOBS':'cap_submit'}
    for name, handler in handlers.items():
        prefix = '[PVR_IOC_NR(' + name + ')] = '
        lines = text.splitlines(keepends=True)
        matches = [i for i,l in enumerate(lines) if prefix in l]
        if len(matches) != 1: raise SystemExit('unexpected ioctl table')
        i = matches[0]; lines[i] = '   ' + prefix + handler + ',\n'; text = ''.join(lines)
    generated = out/'shim.c'; generated.write_text(text)
    commands = subprocess.check_output(['ninja','-C',str(build),'-t','commands',
        'src/imagination/drm-shim/libpowervr_noop_drm_shim.so'], text=True).splitlines()
    compileline = next(x for x in commands if 'pvr_noop.c.o -c ' in x)
    linkline = next(x for x in commands if x.startswith('c++  -o src/imagination/drm-shim/libpowervr_noop_drm_shim.so '))
    args = shlex.split(compileline); args[args.index('-o')+1] = str(out/'shim.o'); args[-1] = str(generated)
    subprocess.run(args, cwd=build, check=True)
    args = shlex.split(linkline); args[args.index('-o')+1] = str(out/'shim.so')
    args = [str(out/'shim.o') if x == 'src/imagination/drm-shim/libpowervr_noop_drm_shim.so.p/pvr_noop.c.o' else x for x in args]
    subprocess.run(args, cwd=build, check=True)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'libdrm'], text=True))
    subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror', *flags, str(HERE/'pvr-fake-enumeration.c'), '-o', str(out/'enumeration.so')], check=True)
    metadata = {'enumeration_sha256':sha(out/'enumeration.so'), 'enumeration_source_sha256':sha(HERE/'pvr-fake-enumeration.c'), 'mesa_commit':PIN, 'mesa_remote':git(src,'remote','get-url','origin'),
                'mesa_shim_sha256':sha(base), 'hook_sha256':sha(HERE/'pvr-capture-hooks.h'),
                'shim_sha256':sha(out/'shim.so'),
                'vulkan_sha256':sha(build/'src/imagination/vulkan/libvulkan_powervr_mesa.so'),
                'meson_options_sha256':sha(build/'meson-info/intro-buildoptions.json'),
                'build_ninja_sha256':sha(build/'build.ninja')}
    metadata['meson_options'] = {item['name']: item['value'] for item in json.loads((build/'meson-info/intro-buildoptions.json').read_text()) if item['name'] in ('buildtype','platforms','gallium-drivers','vulkan-drivers','opengl','egl','glx','tools','build-tests','c_link_args','cpp_link_args')}
    metadata['compiler'] = subprocess.check_output(['cc','--version'], text=True).splitlines()[0]
    metadata['ninja'] = subprocess.check_output(['ninja','--version'], text=True).strip()
    (out/'build-manifest.json').write_text(json.dumps(metadata, indent=2)+'\n')

if __name__ == '__main__': main()
