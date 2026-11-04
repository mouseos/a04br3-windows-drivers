#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Trace Mesa upload extents without exporting bytes or host pointers."""
import argparse
import hashlib
import json
import pathlib
import shlex
import subprocess
PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','build','sipf1','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
    a=p.parse_args(); src,build,diag,out=(x.resolve() for x in (a.source,a.build,a.sipf1,a.output))
    git=lambda *args:subprocess.check_output(['git','-C',str(src),*args],text=True).strip()
    meta=json.loads((diag/'diagnostic-manifest.json').read_text())
    if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or
        not meta.get('sipf1_patch') or not meta.get('assertions_enabled_in_hwrt_unit') or
        meta['mesa_commit']!=PIN or sha(diag/'libvulkan_powervr_mesa.so')!=meta['library_sha256'] or
        sha(build/'build.ninja')!=meta['base_build_ninja_sha256']):raise SystemExit('invalid diagnostic input')
    source=(src/'src/imagination/vulkan/pvr_device.c').read_text()
    hook='''#include <stdio.h>
#include <stdlib.h>
static void a04_upload_range(const char *kind, unsigned long long gpu, unsigned long long size)
{
   const char *name=getenv("A04_UPLOAD_TRACE");
   FILE *f=name ? fopen(name,"a") : NULL;
   if (!f) abort();
   if (fprintf(f,"{\\\"kind\\\":\\\"%s\\\",\\\"gpu_va\\\":%llu,\\\"size\\\":%llu}\\n",kind,gpu,size)<0 || fclose(f)) abort();
   name=getenv("A04_CAPTURE_JSONL");
   f=name ? fopen(name,"a") : NULL;
   if (!f) abort();
   if (fprintf(f,"{\\\"event\\\":\\\"upload\\\",\\\"kind\\\":\\\"%s\\\",\\\"gpu_va\\\":%llu,\\\"size\\\":%llu}\\n",kind,gpu,size)<0 || fclose(f)) abort();
}
static void a04_usc_hash(unsigned long long gpu, const void *data, unsigned size)
{
   const unsigned char *p=data; unsigned long long h=14695981039346656037ULL;
   for (unsigned i=0;i<size;i++) { h^=p[i]; h*=1099511628211ULL; }
   const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
   if (!f) abort();
   if (fprintf(f,"{\\"event\\":\\"usc_upload_hash\\",\\"gpu_va\\":%llu,\\"size\\":%u,\\"fnv64\\":%llu}\\n",gpu,size,h)<0 || fclose(f)) abort();
}
'''
    # Only report the explicitly uploaded code/data extents. No source or CPU
    # destination pointers, bytes, or full BOs enter the trace.
    u='''   memcpy(map, code, code_size);

   *pvr_bo_out = suballoc_bo;'''
    uc='''   memcpy(map, code, code_size);
   a04_upload_range("usc_code", (unsigned long long)suballoc_bo->dev_addr.addr, code_size);
   a04_usc_hash((unsigned long long)suballoc_bo->dev_addr.addr, code, code_size);

   *pvr_bo_out = suballoc_bo;'''
    d='''      memcpy(map, data, data_size);

      pds_upload_out->data_offset'''
    dc='''      memcpy(map, data, data_size);
      a04_upload_range("pds_data", (unsigned long long)pds_upload_out->pvr_bo->dev_addr.addr, data_size);

      pds_upload_out->data_offset'''
    c='''      memcpy((uint8_t *)map + code_offset, code, code_size);

      pds_upload_out->code_offset'''
    cc='''      memcpy((uint8_t *)map + code_offset, code, code_size);
      a04_upload_range("pds_code", (unsigned long long)pds_upload_out->pvr_bo->dev_addr.addr + code_offset, code_size);

      pds_upload_out->code_offset'''
    for old,new in ((u,uc),(d,dc),(c,cc)):
        if source.count(old)!=1:raise SystemExit('unexpected upload implementation')
        source=source.replace(old,new)
    out.mkdir(mode=0o700,parents=True,exist_ok=False)
    (out/'pvr_device.c').write_text(hook+source)
    cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands','src/imagination/vulkan/libvulkan_powervr_mesa.so'],text=True).splitlines()
    line=next(x for x in cmds if 'pvr_device.c.o -c ' in x)
    args=shlex.split(line);args[args.index('-o')+1]=str(out/'pvr_device.c.o');args[-1]=str(out/'pvr_device.c')
    subprocess.run(args,cwd=build,check=True)
    link=next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so '))
    args=shlex.split(link);args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
    args=[str(diag/'libpowervr_rogue.a') if x=='src/imagination/vulkan/libpowervr_rogue.a' else str(out/'pvr_device.c.o') if x=='src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o' else x for x in args]
    subprocess.run(args,cwd=build,check=True)
    meta.update(upload_metadata_only=True,upload_source_sha256=sha(out/'pvr_device.c'),upload_object_sha256=sha(out/'pvr_device.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
    (out/'diagnostic-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
