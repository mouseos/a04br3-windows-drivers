#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument two Mesa command-buffer PDS construction sites; metadata only."""
import argparse,hashlib,json,pathlib,shlex,subprocess
PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('source','build','sipf1','uploads','output'):p.add_argument('--'+n,type=pathlib.Path,required=True)
 a=p.parse_args();src,build,diag,uploads,out=(x.resolve() for x in (a.source,a.build,a.sipf1,a.uploads,a.output))
 git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
 meta=json.loads((uploads/'diagnostic-manifest.json').read_text());old=json.loads((diag/'diagnostic-manifest.json').read_text())
 if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or
     not meta.get('upload_metadata_only') or not meta.get('sipf1_patch') or not meta.get('assertions_enabled_in_hwrt_unit') or
     meta['base_build_ninja_sha256']!=sha(build/'build.ninja') or sha(uploads/'libvulkan_powervr_mesa.so')!=meta['library_sha256'] or
     sha(diag/'libvulkan_powervr_mesa.so')!=old['library_sha256'] or meta['object_sha256']!=old['object_sha256']):raise SystemExit('invalid source or diagnostic')
 original=src/'src/imagination/vulkan/pvr_arch_cmd_buffer.c';text=original.read_text()
 hook='''#include <stdio.h>
#include <stdlib.h>
static void a04_pds_construct(const char *kind,unsigned long long gpu,unsigned long long size,unsigned entries)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\"event\\":\\"pds_construct\\",\\"kind\\":\\"%s\\",\\"gpu_va\\":%llu,\\"size\\":%llu,\\"entries\\":%u}\\n",kind,gpu,size,entries)<0 || fclose(f)) abort();
}
static void a04_pds_entry(const char *kind,unsigned long long gpu,unsigned ordinal,unsigned type,unsigned offset,unsigned subtype)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\\"event\\\":\\\"pds_entry\\\",\\\"kind\\\":\\\"%s\\\",\\\"gpu_va\\\":%llu,\\\"ordinal\\\":%u,\\\"entry_type\\\":%u,\\\"const_offset\\\":%u,\\\"subtype\\\":%u}\\n",kind,gpu,ordinal,type,offset,subtype)<0 || fclose(f)) abort();
}

static void a04_pds_dep(const char *kind,unsigned long long parent,unsigned offset,unsigned long long info,unsigned long long buffer,unsigned long long size,unsigned block)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\"event\\":\\"pds_dependency\\",\\"kind\\":\\"%s\\",\\"parent_gpu\\":%llu,\\"offset\\":%u,\\"info_gpu\\":%llu,\\"buffer_gpu\\":%llu,\\"buffer_size\\":%llu,\\"block_size\\":%u}\\n",kind,parent,offset,info,buffer,size,block)<0 || fclose(f)) abort();
}

static void a04_pds_shader(unsigned long long parent,unsigned offset,unsigned long long shader,unsigned long long entry,unsigned size)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\"event\\":\\"pds_shader\\",\\"parent_gpu\\":%llu,\\"offset\\":%u,\\"shader_gpu\\":%llu,\\"entry_gpu\\":%llu,\\"size\\":%u}\\n",parent,offset,shader,entry,size)<0 || fclose(f)) abort();
}
'''
 pairs=[('''   state->pds_vertex_attrib_offset =
      pvr_bo->dev_addr.addr -''','''   a04_pds_construct("vertex_attributes",pvr_bo->dev_addr.addr,PVR_DW_TO_BYTES(pds_info->data_size_in_dwords),pds_info->entry_count);
   state->pds_vertex_attrib_offset =
      pvr_bo->dev_addr.addr -'''),('''   *descriptor_data_offset_out =
      pvr_bo->dev_addr.addr -''','''   a04_pds_construct("descriptor_mappings",pvr_bo->dev_addr.addr,PVR_DW_TO_BYTES(pds_info->data_size_in_dwords),pds_info->entry_count);
   *descriptor_data_offset_out =
      pvr_bo->dev_addr.addr -''')]
 for before,after in pairs:
  if text.count(before)!=1:raise SystemExit('unexpected Mesa command-buffer source')
  text=text.replace(before,after)
 target='''         pvr_set_usc_execution_address64(&addr, exec_addr.addr);
'''
 insert='''         a04_pds_shader(pvr_bo->dev_addr.addr,doutu_addr->const_offset,vertex_state->shader_bo->dev_addr.addr,exec_addr.addr,vertex_state->shader_bo->size);
'''
 if text.count(target)!=1:raise SystemExit('unexpected vertex shader encoding')
 text=text.replace(target,insert+target)
 for kind in ('spill','scratch'):
  target='''            PVR_WRITE(qword_buffer,
                      %s_info_bo->dev_addr.addr,
                      special_buff_entry->const_offset,''' % kind
  insert='''            a04_pds_dep("%s",pvr_bo->dev_addr.addr,special_buff_entry->const_offset,
                        %s_info_bo->dev_addr.addr,%s_buffer_bo->dev_addr.addr,
                        total_%s_mem_size,%s_block_size);
''' % ((kind,)*5)
  if text.count(target)!=1:raise SystemExit('unexpected special buffer branch')
  text=text.replace(target,insert+target)
 entry='''      const struct pvr_const_map_entry *const entry_header =
         (struct pvr_const_map_entry *)entries;
'''
 if text.count(entry)!=2:raise SystemExit('unexpected constant-map loops')
 text=text.replace(entry,entry+'''      a04_pds_entry("vertex_attributes",pvr_bo->dev_addr.addr,i,entry_header->type,entry_header->const_offset,0);
''',1)
 second=text.find(entry,text.find('static VkResult pvr_setup_descriptor_mappings('))
 if second<0:raise SystemExit('descriptor loop missing')
 text=text[:second]+text[second:].replace(entry,entry+'''      a04_pds_entry("descriptor_mappings",pvr_bo->dev_addr.addr,i,entry_header->type,entry_header->const_offset,entry_header->type==PVR_PDS_CONST_MAP_ENTRY_TYPE_SPECIAL_BUFFER ? ((const struct pvr_const_map_entry_special_buffer *)entry_header)->buffer_type : 0);
''',1)
 out.mkdir(mode=0o700,parents=True,exist_ok=False);modified=out/'pvr_arch_cmd_buffer.c';modified.write_text(hook+text)
 target='src/imagination/vulkan/libvulkan_powervr_mesa.so'
 cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
 line=next(x for x in cmds if 'pvr_arch_cmd_buffer.c.o -c ' in x)
 args=shlex.split(line);args[args.index('-o')+1]=str(out/'pvr_arch_cmd_buffer.c.o');args[-1]=str(modified)
 subprocess.run(args,cwd=build,check=True)
 members=subprocess.check_output(['ar','t',str(diag/'libpowervr_rogue.a')],text=True).splitlines()
 chosen=[x for x in members if x.endswith('/pvr_arch_cmd_buffer.c.o')]
 if len(chosen)!=1:raise SystemExit('unexpected archive layout')
 members=[str(out/'pvr_arch_cmd_buffer.c.o') if x==chosen[0] else str((diag/x).resolve()) for x in members]
 subprocess.run(['ar','crsT',str(out/'libpowervr_rogue.a'),*members],check=True)
 link=next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so '))
 args=shlex.split(link);args[args.index('-o')+1]=str(out/'libvulkan_powervr_mesa.so')
 args=[str(out/'libpowervr_rogue.a') if x=='src/imagination/vulkan/libpowervr_rogue.a' else str(uploads/'pvr_device.c.o') if x=='src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o' else x for x in args]
 subprocess.run(args,cwd=build,check=True)
 meta.update(pds_construct_metadata_only=True,pds_original_sha256=sha(original),pds_source_sha256=sha(modified),pds_object_sha256=sha(out/'pvr_arch_cmd_buffer.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
 (out/'diagnostic-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
