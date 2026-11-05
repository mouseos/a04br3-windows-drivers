#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument SPM allocation extents in an isolated Mesa library; metadata only."""
import argparse,hashlib,json,pathlib,shlex,subprocess
PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('source','build','uploads','usc','vdm','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
 a=p.parse_args();src,build,up,usc,vdm,out=(x.resolve() for x in (a.source,a.build,a.uploads,a.usc,a.vdm,a.output))
 git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
 meta=json.loads((vdm/'diagnostic-manifest.json').read_text());um=json.loads((up/'diagnostic-manifest.json').read_text());cm=json.loads((usc/'diagnostic-manifest.json').read_text())
 if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or not meta.get('pbe_target_metadata_only') or
     not meta.get('ppp_hash_metadata_only') or meta['base_build_ninja_sha256']!=sha(build/'build.ninja') or
     sha(vdm/'libvulkan_powervr_mesa.so')!=meta['library_sha256'] or sha(vdm/'pvr_dump_csb.c.o')!=meta['vdm_object_sha256'] or
     sha(up/'pvr_device.c.o')!=um['upload_object_sha256'] or sha(usc/'pco_binary.c.o')!=cm['pco_object_sha256'] or
     meta['upload_object_sha256']!=um['upload_object_sha256'] or meta['pco_object_sha256']!=cm['pco_object_sha256']):
  raise SystemExit('source or diagnostic mismatch')
 source=src/'src/imagination/vulkan/pvr_arch_spm.c';text=source.read_text()
 hook='''#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void a04_spm_load(unsigned long long scratch,unsigned long long scratch_size,unsigned long long used,
                         unsigned long long constants,unsigned long long constants_size,unsigned long long pds,
                         unsigned pds_size,unsigned width,unsigned height,unsigned samples,unsigned outputs,unsigned tiles,
                         unsigned long long bg0,unsigned long long bg1,unsigned long long bg3) {
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\\"event\\\":\\\"spm_load\\\",\\\"scratch_gpu\\\":%llu,\\\"scratch_size\\\":%llu,\\\"used\\\":%llu,\\\"constants_gpu\\\":%llu,\\\"constants_size\\\":%llu,\\\"pds_gpu\\\":%llu,\\\"pds_size\\\":%u,\\\"width\\\":%u,\\\"height\\\":%u,\\\"samples\\\":%u,\\\"outputs\\\":%u,\\\"tiles\\\":%u,\\\"bg0\\\":%llu,\\\"bg1\\\":%llu,\\\"bg3\\\":%llu}\\n",scratch,scratch_size,used,constants,constants_size,pds,pds_size,width,height,samples,outputs,tiles,bg0,bg1,bg3)<0 || fclose(f)) abort();
}
'''
 marker='''   return VK_SUCCESS;

err_free_consts_buffer:'''
 if text.count(marker)!=1:raise SystemExit('unexpected SPM function')
 text=text.replace(marker,'''   a04_spm_load(rstate->scratch_buffer->bo->vma->dev_addr.addr,rstate->scratch_buffer->size,
                next_scratch_buffer_addr.addr-rstate->scratch_buffer->bo->vma->dev_addr.addr,
                spm_bgobj_state->consts_buffer->vma->dev_addr.addr,consts_buffer_size,
                device->heaps.pds_heap->base_addr.addr+pds_texture_data_upload.data_offset,
                pds_texture_data_upload.data_size*4u,framebuffer_size.width,framebuffer_size.height,
                hw_render->sample_count,props.output_reg_count,hw_render->tile_buffers_count,
                spm_bgobj_state->pds_reg_values[0],spm_bgobj_state->pds_reg_values[1],spm_bgobj_state->pds_reg_values[2]);
'''+marker)
 out.mkdir(mode=0o700,parents=True,exist_ok=False);(out/'pvr_arch_spm.c').write_text(hook+text)
 target='src/imagination/vulkan/libvulkan_powervr_mesa.so'
 cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
 line=next(x for x in cmds if 'pvr_arch_spm.c.o -c ' in x);args=shlex.split(line);args[args.index('-o')+1]=str(out/'pvr_arch_spm.c.o');args[-1]=str(out/'pvr_arch_spm.c')
 subprocess.run(args,cwd=build,check=True)
 members=subprocess.check_output(['ar','t',str(vdm/'libpowervr_rogue.a')],text=True).splitlines();chosen=[x for x in members if x.endswith('/pvr_arch_spm.c.o')]
 if len(chosen)!=1:raise SystemExit('unexpected archive')
 subprocess.run(['ar','crsT',str(out/'libpowervr_rogue.a'),*[str(out/'pvr_arch_spm.c.o') if x==chosen[0] else str(pathlib.Path(x).resolve()) for x in members]],check=True)
 link=next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so '));args=shlex.split(link);args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
 repl={'src/imagination/vulkan/libpowervr_rogue.a':str(out/'libpowervr_rogue.a'),
       'src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o':str(up/'pvr_device.c.o'),
       'src/imagination/pco/libpowervr_compiler.a':str(usc/'libpowervr_compiler.a')}
 subprocess.run([repl.get(x,x) for x in args],cwd=build,check=True)
 meta.update(spm_load_metadata_only=True,spm_original_sha256=sha(source),spm_source_sha256=sha(out/'pvr_arch_spm.c'),
             spm_object_sha256=sha(out/'pvr_arch_spm.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
 (out/'diagnostic-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
