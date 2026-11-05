#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build an isolated Mesa diagnostic that hashes, but never exports, VDM bytes."""
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
    for name in ('source','build','uploads','pds','usc','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
    a=p.parse_args();src,build,uploads,pds,usc,out=(x.resolve() for x in (a.source,a.build,a.uploads,a.pds,a.usc,a.output))
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    meta=json.loads((usc/'diagnostic-manifest.json').read_text());up=json.loads((uploads/'diagnostic-manifest.json').read_text());pd=json.loads((pds/'diagnostic-manifest.json').read_text())
    if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or not meta.get('private_compiler_candidates') or
        not meta.get('pds_code_hash_metadata_only') or not meta.get('pds_construct_metadata_only') or
        meta['base_build_ninja_sha256']!=sha(build/'build.ninja') or sha(usc/'libvulkan_powervr_mesa.so')!=meta['library_sha256'] or
        sha(usc/'pco_binary.c.o')!=meta['pco_object_sha256'] or sha(uploads/'pvr_device.c.o')!=up['upload_object_sha256'] or
        sha(pds/'pvr_arch_cmd_buffer.c.o')!=pd['pds_object_sha256'] or meta['upload_object_sha256']!=up['upload_object_sha256'] or
        meta['pds_object_sha256']!=pd['pds_object_sha256']):raise SystemExit('source or diagnostic mismatch')
    source=src/'src/imagination/vulkan/rogue/pvr_dump_csb.c';text=source.read_text()
    marker='   struct pvr_device *const device = csb->device;'
    if text.count(marker)!=1:raise SystemExit('unexpected dump implementation')
    hook='''#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void a04_vdm_hash(const void *start,const void *next,unsigned type,unsigned bos) {
 const char *name=getenv("A04_VDM_HASH"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 uintptr_t a=(uintptr_t)start,b=(uintptr_t)next; unsigned long long h=14695981039346656037ULL;
 if (bos!=1 || !a || b<a || b-a>4096) { if (fprintf(f,"{\\\"type\\\":%u,\\\"valid\\\":false}\\n",type)<0 || fclose(f)) abort(); return; }
 const unsigned char *p=start; for (uintptr_t i=0;i<b-a;i++) { h^=p[i];h*=1099511628211ULL; }
 if (fprintf(f,"{\\\"type\\\":%u,\\\"valid\\\":true,\\\"bytes\\\":%llu,\\\"fnv64\\\":%llu}\\n",type,(unsigned long long)(b-a),h)<0 || fclose(f)) abort();
}
static void a04_ppp_hash(unsigned long long gpu,const void *data,unsigned long long size) {
 const char *name=getenv("A04_PPP_HASH"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f || !data || !size || size>4096) abort();
 const unsigned char *p=data; unsigned long long h=14695981039346656037ULL;
 for (unsigned long long i=0;i<size;i++) { h^=p[i];h*=1099511628211ULL; }
 if (fprintf(f,"{\\\"gpu_va\\\":%llu,\\\"bytes\\\":%llu,\\\"fnv64\\\":%llu}\\n",gpu,size,h)<0 || fclose(f)) abort();
}
'''
    text=text.replace(marker,marker+'\n   a04_vdm_hash(csb->start,csb->next,csb->stream_type,nr_bos);')
    marker2='''   if (sub_ctx.bo_mapped_in_ctx)
      pvr_dump_field(base_ctx, "<host addr>", "<unmapped>");'''
    if text.count(marker2)!=1:raise SystemExit('unexpected sub-buffer implementation')
    text=text.replace(marker2,'''   if (type==BUFFER_TYPE_PPP && expected_size && expected_size<=real_size)
      a04_ppp_hash(addr.addr,sub_ctx.base.ptr,expected_size);
'''+marker2)
    out.mkdir(mode=0o700,parents=True,exist_ok=False);(out/'pvr_dump_csb.c').write_text(hook+text)
    target='src/imagination/vulkan/libvulkan_powervr_mesa.so'
    cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
    line=next(x for x in cmds if 'rogue_pvr_dump_csb.c.o -c ' in x)
    args=shlex.split(line);args[args.index('-o')+1]=str(out/'pvr_dump_csb.c.o');args[-1]=str(out/'pvr_dump_csb.c')
    subprocess.run(args,cwd=build,check=True)
    members=subprocess.check_output(['ar','t',str(pds/'libpowervr_rogue.a')],text=True).splitlines()
    chosen=[x for x in members if x.endswith('/rogue_pvr_dump_csb.c.o')]
    if len(chosen)!=1:raise SystemExit('unexpected rogue archive')
    members=[str(out/'pvr_dump_csb.c.o') if x==chosen[0] else str(pathlib.Path(x).resolve()) for x in members]
    subprocess.run(['ar','crsT',str(out/'libpowervr_rogue.a'),*members],check=True)
    link=next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so '))
    args=shlex.split(link);args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
    repl={'src/imagination/vulkan/libpowervr_rogue.a':str(out/'libpowervr_rogue.a'),
          'src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o':str(uploads/'pvr_device.c.o'),
          'src/imagination/pco/libpowervr_compiler.a':str(usc/'libpowervr_compiler.a')}
    subprocess.run([repl.get(x,x) for x in args],cwd=build,check=True)
    meta.update(vdm_hash_metadata_only=True,ppp_hash_metadata_only=True,vdm_original_sha256=sha(source),vdm_source_sha256=sha(out/'pvr_dump_csb.c'),
                vdm_object_sha256=sha(out/'pvr_dump_csb.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
    (out/'diagnostic-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
