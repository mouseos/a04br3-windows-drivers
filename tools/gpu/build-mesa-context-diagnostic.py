#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument pinned Mesa render context-switch dependencies; metadata only."""
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
    for name in ('source','build','uploads','pds','usc','output'):p.add_argument('--'+name,required=True,type=pathlib.Path)
    a=p.parse_args();src,build,uploads,pds,usc,out=(x.resolve() for x in (a.source,a.build,a.uploads,a.pds,a.usc,a.output))
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    meta=json.loads((usc/'diagnostic-manifest.json').read_text());up=json.loads((uploads/'diagnostic-manifest.json').read_text());pd=json.loads((pds/'diagnostic-manifest.json').read_text())
    if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or
        not meta.get('usc_compiler_metadata_only') or meta['base_build_ninja_sha256']!=sha(build/'build.ninja') or
        sha(usc/'libvulkan_powervr_mesa.so')!=meta['library_sha256'] or sha(usc/'pco_binary.c.o')!=meta['pco_object_sha256'] or
        sha(pds/'libvulkan_powervr_mesa.so')!=pd['library_sha256'] or sha(pds/'pvr_arch_cmd_buffer.c.o')!=pd['pds_object_sha256'] or
        sha(uploads/'pvr_device.c.o')!=up['upload_object_sha256'] or up['upload_object_sha256']!=meta['upload_object_sha256'] or
        pd['pds_object_sha256']!=meta['pds_object_sha256']):raise SystemExit('invalid diagnostic input')
    source=src/'src/imagination/vulkan/pvr_arch_job_context.c';text=source.read_text()
    hook='''#include <stdio.h>
#include <stdlib.h>
static void a04_ctx_component(const char *kind,unsigned index,unsigned long long gpu,unsigned long long size) {
 const char *name=getenv("A04_CAPTURE_JSONL");FILE *f=name ? fopen(name,"a") : NULL;
 if (!f || !gpu || !size) abort();
 if (fprintf(f,"{\\"event\\":\\"ctx_component\\",\\"kind\\":\\"%s\\",\\"index\\":%u,\\"gpu_va\\":%llu,\\"size\\":%llu}\\n",kind,index,gpu,size)<0 || fclose(f)) abort();
}
'''
    marker='''   q_dst = &static_state->vdm_ctx_state_base_addr;'''
    add='''   a04_ctx_component("vdm_state",0,ctx->ctx_switch.vdm_state_bo->vma->dev_addr.addr,ctx->ctx_switch.vdm_state_bo->vma->size);
   a04_ctx_component("geom_state",0,ctx->ctx_switch.geom_state_bo->vma->dev_addr.addr,ctx->ctx_switch.geom_state_bo->vma->size);
   for (unsigned n=0;n<ARRAY_SIZE(ctx->ctx_switch.programs);n++) {
      struct pvr_render_ctx_programs *c=&ctx->ctx_switch.programs[n];
      struct pvr_pds_upload *p[4]={&c->pt.pds_store_program,&c->pt.pds_resume_program,&c->sr.pds.store_program,&c->sr.pds.load_program};
      const char *dk[4]={"pt_store_data","pt_resume_data","sr_store_data","sr_load_data"};
      const char *ck[4]={"pt_store_code","pt_resume_code","sr_store_code","sr_load_code"};
      unsigned long long base=ctx->device->heaps.pds_heap->base_addr.addr;
      a04_ctx_component("pt_state",n,c->pt.store_resume_state_bo->vma->dev_addr.addr,c->pt.store_resume_state_bo->vma->size);
      a04_ctx_component("sr_state",n,c->sr.store_load_state_bo->vma->dev_addr.addr,c->sr.store_load_state_bo->vma->size);
      a04_ctx_component("sr_store_usc",n,c->sr.usc.store_program_bo->dev_addr.addr,c->sr.usc.store_program_bo->size);
      a04_ctx_component("sr_load_usc",n,c->sr.usc.load_program_bo->dev_addr.addr,c->sr.usc.load_program_bo->size);
      for (unsigned j=0;j<4;j++) {
         a04_ctx_component(dk[j],n,base+p[j]->data_offset,p[j]->data_size*4u);
         a04_ctx_component(ck[j],n,base+p[j]->code_offset,p[j]->code_size*4u);
      }
   }
'''
    if text.count(marker)!=1:raise SystemExit('unexpected Mesa context source')
    text=text.replace(marker,add+marker)
    out.mkdir(mode=0o700,parents=True,exist_ok=False);(out/'pvr_arch_job_context.c').write_text(hook+text)
    target='src/imagination/vulkan/libvulkan_powervr_mesa.so'
    cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
    line=next(x for x in cmds if 'pvr_arch_job_context.c.o -c ' in x)
    args=shlex.split(line);args[args.index('-o')+1]=str(out/'pvr_arch_job_context.c.o');args[-1]=str(out/'pvr_arch_job_context.c')
    subprocess.run(args,cwd=build,check=True)
    members=subprocess.check_output(['ar','t',str(pds/'libpowervr_rogue.a')],text=True).splitlines()
    chosen=[x for x in members if x.endswith('/pvr_arch_job_context.c.o')]
    if len(chosen)!=1:raise SystemExit('unexpected rogue archive')
    members=[str(out/'pvr_arch_job_context.c.o') if x==chosen[0] else str(pathlib.Path(x).resolve()) for x in members]
    subprocess.run(['ar','crsT',str(out/'libpowervr_rogue.a'),*members],check=True)
    link=next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so '))
    args=shlex.split(link);args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
    repl={'src/imagination/vulkan/libpowervr_rogue.a':str(out/'libpowervr_rogue.a'),
          'src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o':str(uploads/'pvr_device.c.o'),
          'src/imagination/pco/libpowervr_compiler.a':str(usc/'libpowervr_compiler.a')}
    args=[repl.get(x,x) for x in args];subprocess.run(args,cwd=build,check=True)
    meta.update(context_component_metadata_only=True,context_original_sha256=sha(source),context_source_sha256=sha(out/'pvr_arch_job_context.c'),
                context_object_sha256=sha(out/'pvr_arch_job_context.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
    (out/'diagnostic-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
