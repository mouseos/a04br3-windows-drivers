#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a private Mesa library with assertions enabled in the HWRT TU.

The default applies a single-macrotile SIPF1 diagnostic change. No source
checkout or common build artifact is modified; this is not an upstream patch.
"""
import argparse
import difflib
import hashlib
import json
import pathlib
import shlex
import subprocess

PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
OLD='''   if (PVR_HAS_FEATURE(dev_info, simple_internal_parameter_format)) {
      assert(PVR_GET_FEATURE_VALUE(dev_info,
                                   simple_parameter_format_version,
                                   0) == 2);
      /* Set up 16 macrotiles'''
NEW='''   if (PVR_HAS_FEATURE(dev_info, simple_internal_parameter_format) &&
       PVR_GET_FEATURE_VALUE(dev_info, simple_parameter_format_version, 0) == 1) {
      /* Diagnostic SIPF1: one macrotile covers the complete render target. */
      info->mtile_x1 = info->num_tiles_x;
      info->mtile_y1 = info->num_tiles_y;
      info->mtile_x2 = 0;
      info->mtile_y2 = 0;
      info->mtile_x3 = 0;
      info->mtile_y3 = 0;
      info->x_tile_max = info->num_tiles_x - 1;
      info->y_tile_max = info->num_tiles_y - 1;
   } else if (PVR_HAS_FEATURE(dev_info, simple_internal_parameter_format)) {
      assert(PVR_GET_FEATURE_VALUE(dev_info,
                                   simple_parameter_format_version,
                                   0) == 2);
      /* Set up 16 macrotiles'''
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',type=pathlib.Path,required=True);ap.add_argument('--build',type=pathlib.Path,required=True);ap.add_argument('--output',type=pathlib.Path,required=True);ap.add_argument('--original-assert',action='store_true');a=ap.parse_args()
    src,build,out=(p.resolve() for p in (a.source,a.build,a.output))
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    if git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all'):raise SystemExit('Mesa source mismatch or dirty')
    if pathlib.Path(json.loads((build/'meson-info/meson-info.json').read_text())['directories']['source']).resolve()!=src:raise SystemExit('build source mismatch')
    target='src/imagination/vulkan/libvulkan_powervr_mesa.so'
    subprocess.run(['ninja','-j4','-C',str(build),target],check=True)
    original=src/'src/imagination/vulkan/pvr_arch_job_render.c';source=original.read_text()
    if source.count(OLD)!=1:raise SystemExit('unexpected Mesa source')
    changed=source if a.original_assert else source.replace(OLD,NEW)
    out.mkdir(mode=0o700,parents=True,exist_ok=False)
    modified=out/'pvr_arch_job_render.c';modified.write_text(changed)
    patch=''.join(difflib.unified_diff(source.splitlines(True),changed.splitlines(True),fromfile='a/pvr_arch_job_render.c',tofile='b/pvr_arch_job_render.c'))
    (out/'diagnostic.patch').write_text(patch)
    cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
    line=next(x for x in cmds if 'pvr_arch_job_render.c.o -c ' in x)
    args=shlex.split(line);args[args.index('-o')+1]=str(out/'pvr_arch_job_render.c.o');args[-1]=str(modified);args.insert(args.index('-c'),'-UNDEBUG')
    subprocess.run(args,cwd=build,check=True)
    archive=build/'src/imagination/vulkan/libpowervr_rogue.a'
    members=subprocess.check_output(['ar','t',str(archive)],text=True).splitlines()
    selected=[x for x in members if x.endswith('/pvr_arch_job_render.c.o')]
    if len(selected)!=1:raise SystemExit('unexpected archive layout')
    members=[str(out/'pvr_arch_job_render.c.o') if x==selected[0] else str((build/x).resolve()) for x in members]
    subprocess.run(['ar','crsT',str(out/'libpowervr_rogue.a'),*members],check=True)
    link=next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so '))
    args=shlex.split(link);args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
    args=[str(out/'libpowervr_rogue.a') if x=='src/imagination/vulkan/libpowervr_rogue.a' else x for x in args]
    subprocess.run(args,cwd=build,check=True)
    (out/'diagnostic-manifest.json').write_text(json.dumps({'mesa_commit':PIN,'assertions_enabled_in_hwrt_unit':True,'sipf1_patch':not a.original_assert,'original_sha256':sha(original),'patch_sha256':sha(out/'diagnostic.patch'),'object_sha256':sha(out/'pvr_arch_job_render.c.o'),'library_sha256':sha(out/'libvulkan_powervr_mesa.so'),'base_build_ninja_sha256':sha(build/'build.ninja')},indent=2)+'\n')
if __name__=='__main__':main()
