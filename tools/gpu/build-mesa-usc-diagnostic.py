#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build private PCO fill-pattern and compiler-output hash instrumentation."""
import argparse
import hashlib
import json
import pathlib
import re
import shlex
import subprocess

PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','build','sipf1','uploads','pds','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
    a=p.parse_args();src,build,sipf,uploads,pds,out=(x.resolve() for x in (a.source,a.build,a.sipf1,a.uploads,a.pds,a.output))
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    meta=json.loads((pds/'diagnostic-manifest.json').read_text())
    up=json.loads((uploads/'diagnostic-manifest.json').read_text())
    if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or
        not meta.get('pds_construct_metadata_only') or not meta.get('sipf1_patch') or
        not meta.get('assertions_enabled_in_hwrt_unit') or meta['base_build_ninja_sha256']!=sha(build/'build.ninja') or
        sha(pds/'libvulkan_powervr_mesa.so')!=meta['library_sha256'] or
        sha(pds/'pvr_arch_cmd_buffer.c.o')!=meta['pds_object_sha256'] or
        sha(uploads/'libvulkan_powervr_mesa.so')!=up['library_sha256'] or
        sha(uploads/'pvr_device.c.o')!=up.get('upload_object_sha256') or
        meta.get('upload_object_sha256')!=up.get('upload_object_sha256') or
        meta.get('object_sha256')!=up.get('object_sha256') or
        sha(sipf/'libvulkan_powervr_mesa.so')!=json.loads((sipf/'diagnostic-manifest.json').read_text())['library_sha256']):
        raise SystemExit('invalid source or diagnostic')
    source=src/'src/imagination/pco/pco_binary.c'; text=source.read_text()
    pattern=r'(ptr = util_dynarray_grow\(buf, uint8_t, (igrp->enc\.len\.[a-z_]+(?:\[p\])?)\);)'
    text,count=re.subn(pattern,r'\1\n      memset(ptr, a04_pco_fill(), \2);',text)
    if count!=6:raise SystemExit('unexpected PCO encoding layout')
    hook='''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int a04_pco_fill(void) {
 const char *v=getenv("A04_PCO_FILL");
 if (!v || (strcmp(v,"00") && strcmp(v,"ff"))) abort();
 return v[0]=='f' ? 255 : 0;
}
static void a04_pco_hash(unsigned stage, int internal, const void *data, unsigned size) {
 const unsigned char *p=data; unsigned long long h=14695981039346656037ULL;
 for (unsigned i=0;i<size;i++) { h^=p[i]; h*=1099511628211ULL; }
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\"event\\":\\"pco_binary\\",\\"stage\\":%u,\\"internal\\":%d,\\"size\\":%u,\\"fnv64\\":%llu}\\n",stage,internal,size,h)<0 || fclose(f)) abort();
}
'''
    marker='   if (pco_should_print_binary(shader))'
    if text.count(marker)!=1:raise SystemExit('unexpected PCO binary function')
    text=text.replace(marker,'   a04_pco_hash(shader->stage, shader->is_internal, shader->binary.data, shader->binary.size);\n\n'+marker)
    out.mkdir(mode=0o700,parents=True,exist_ok=False); (out/'pco_binary.c').write_text(hook+text)
    target='src/imagination/vulkan/libvulkan_powervr_mesa.so'
    cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
    line=next(x for x in cmds if 'pco_binary.c.o -c ' in x)
    args=shlex.split(line);args[args.index('-o')+1]=str(out/'pco_binary.c.o');args[-1]=str(out/'pco_binary.c')
    subprocess.run(args,cwd=build,check=True)
    archive=build/'src/imagination/pco/libpowervr_compiler.a'
    members=subprocess.check_output(['ar','t',str(archive)],text=True).splitlines()
    chosen=[x for x in members if x.endswith('/pco_binary.c.o')]
    if len(chosen)!=1:raise SystemExit('unexpected compiler archive')
    members=[str(out/'pco_binary.c.o') if x==chosen[0] else str((build/x).resolve()) for x in members]
    subprocess.run(['ar','crsT',str(out/'libpowervr_compiler.a'),*members],check=True)
    link=next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so '))
    args=shlex.split(link);args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
    repl={'src/imagination/vulkan/libpowervr_rogue.a':str(pds/'libpowervr_rogue.a'),
          'src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o':str(uploads/'pvr_device.c.o'),
          'src/imagination/pco/libpowervr_compiler.a':str(out/'libpowervr_compiler.a')}
    args=[repl.get(x,x) for x in args];subprocess.run(args,cwd=build,check=True)
    meta.update(usc_compiler_metadata_only=True,pco_original_sha256=sha(source),pco_source_sha256=sha(out/'pco_binary.c'),
                pco_object_sha256=sha(out/'pco_binary.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
    (out/'diagnostic-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
