#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Capture typed context register values in an isolated pinned Mesa library."""
import argparse
import hashlib
import json
import pathlib
import shlex
import subprocess

PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
def sha(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source','build','context','uploads','usc','output'):p.add_argument('--'+n,required=True,type=pathlib.Path)
    a=p.parse_args();src,build,context,up,usc,out=(x.resolve() for x in (a.source,a.build,a.context,a.uploads,a.usc,a.output))
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    m=json.loads((context/'diagnostic-manifest.json').read_text())
    if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or
        not m.get('context_component_metadata_only') or m['base_build_ninja_sha256']!=sha(build/'build.ninja') or
        sha(context/'libvulkan_powervr_mesa.so')!=m['library_sha256'] or
        sha(context/'pvr_arch_job_context.c')!=m['context_source_sha256'] or
        sha(context/'pvr_arch_job_context.c.o')!=m['context_object_sha256'] or
        sha(up/'pvr_device.c.o')!=m['upload_object_sha256'] or sha(usc/'pco_binary.c.o')!=m['pco_object_sha256']):
        raise SystemExit('invalid context diagnostic')
    source=(context/'pvr_arch_job_context.c').read_text()
    marker='''   }
}

static void pvr_render_ctx_ws_create_info_init('''
    if source.count(marker)!=1:raise SystemExit('unexpected context register source')
    hook=r'''   }
   for (unsigned i=0;i<2;i++) {
      const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name?fopen(name,"a"):NULL;
      if (!f) abort();
      if (fprintf(f,"{\"event\":\"ctx_static\",\"index\":%u,\"vdm\":%llu,\"geom\":%llu,\"store0\":%llu,\"store1\":%u,\"store2\":%llu,\"resume0\":%llu,\"resume1\":%u,\"resume2\":%llu}\n",
         i,(unsigned long long)static_state->vdm_ctx_state_base_addr,
         (unsigned long long)static_state->geom_ctx_state_base_addr,
         (unsigned long long)static_state->geom_state[i].vdm_ctx_store_task0,
         static_state->geom_state[i].vdm_ctx_store_task1,
         (unsigned long long)static_state->geom_state[i].vdm_ctx_store_task2,
         (unsigned long long)static_state->geom_state[i].vdm_ctx_resume_task0,
         static_state->geom_state[i].vdm_ctx_resume_task1,
         (unsigned long long)static_state->geom_state[i].vdm_ctx_resume_task2)<0 || fclose(f)) abort();
   }
}

static void pvr_render_ctx_ws_create_info_init('''
    source=source.replace(marker,hook)
    out.mkdir(mode=0o700,parents=True,exist_ok=False);(out/'pvr_arch_job_context.c').write_text(source)
    target='src/imagination/vulkan/libvulkan_powervr_mesa.so'
    cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
    args=shlex.split(next(x for x in cmds if 'pvr_arch_job_context.c.o -c ' in x))
    args[args.index('-o')+1]=str(out/'pvr_arch_job_context.c.o');args[-1]=str(out/'pvr_arch_job_context.c')
    subprocess.run(args,cwd=build,check=True)
    members=subprocess.check_output(['ar','t',str(context/'libpowervr_rogue.a')],text=True).splitlines()
    found=[x for x in members if x.endswith('/pvr_arch_job_context.c.o')]
    if len(found)!=1 or pathlib.Path(found[0]).resolve()!=(context/'pvr_arch_job_context.c.o'):
        raise SystemExit('unexpected archive')
    subprocess.run(['ar','crsT',str(out/'libpowervr_rogue.a'),*[str(out/'pvr_arch_job_context.c.o') if x==found[0] else str(pathlib.Path(x).resolve()) for x in members]],check=True)
    args=shlex.split(next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so ')))
    args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
    repl={'src/imagination/vulkan/libpowervr_rogue.a':str(out/'libpowervr_rogue.a'),
          'src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o':str(up/'pvr_device.c.o'),
          'src/imagination/pco/libpowervr_compiler.a':str(usc/'libpowervr_compiler.a')}
    subprocess.run([repl.get(x,x) for x in args],cwd=build,check=True)
    m.update(context_static_metadata_only=True,context_static_source_sha256=sha(out/'pvr_arch_job_context.c'),
             context_static_object_sha256=sha(out/'pvr_arch_job_context.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
    (out/'diagnostic-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
if __name__=='__main__':main()
