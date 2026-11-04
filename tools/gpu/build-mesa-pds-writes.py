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
#include <stdint.h>
static void a04_pds_construct(const char *kind,unsigned long long gpu,unsigned long long size,unsigned entries,const void *buffer)
{
 const unsigned char *bytes=buffer; unsigned long long hash=14695981039346656037ULL;
 for (unsigned long long i=0;i<size;i++) { hash^=bytes[i]; hash*=1099511628211ULL; }
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\"event\\":\\"pds_construct\\",\\"kind\\":\\"%s\\",\\"gpu_va\\":%llu,\\"size\\":%llu,\\"entries\\":%u,\\"fnv64\\":%llu}\\n",kind,gpu,size,entries,hash)<0 || fclose(f)) abort();
}
static void a04_pds_entry(const char *kind,unsigned long long gpu,unsigned ordinal,unsigned type,unsigned offset,unsigned subtype,unsigned long long value)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\\"event\\\":\\\"pds_entry\\\",\\\"kind\\\":\\\"%s\\\",\\\"gpu_va\\\":%llu,\\\"ordinal\\\":%u,\\\"entry_type\\\":%u,\\\"const_offset\\\":%u,\\\"subtype\\\":%u,\\\"value\\\":%llu}\\n",kind,gpu,ordinal,type,offset,subtype,value)<0 || fclose(f)) abort();
}

static void a04_pds_dep(const char *kind,unsigned long long parent,unsigned offset,unsigned long long info,unsigned long long buffer,unsigned long long size,unsigned block)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\"event\\":\\"pds_dependency\\",\\"kind\\":\\"%s\\",\\"parent_gpu\\":%llu,\\"offset\\":%u,\\"info_gpu\\":%llu,\\"buffer_gpu\\":%llu,\\"buffer_size\\":%llu,\\"block_size\\":%u}\\n",kind,parent,offset,info,buffer,size,block)<0 || fclose(f)) abort();
}
static void a04_pds_fs_meta(unsigned long long parent,unsigned offset,unsigned long long gpu,unsigned value)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\\"event\\\":\\\"pds_fs_meta\\\",\\\"parent_gpu\\\":%llu,\\\"offset\\\":%u,\\\"gpu_va\\\":%llu,\\\"value\\\":%u}\\n",parent,offset,gpu,value)<0 || fclose(f)) abort();
}

static void a04_pds_shader(unsigned long long parent,unsigned offset,unsigned long long shader,unsigned long long entry,unsigned size)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (fprintf(f,"{\\"event\\":\\"pds_shader\\",\\"parent_gpu\\":%llu,\\"offset\\":%u,\\"shader_gpu\\":%llu,\\"entry_gpu\\":%llu,\\"size\\":%u}\\n",parent,offset,shader,entry,size)<0 || fclose(f)) abort();
}

static unsigned long long a04_fragment_hash(const void *data, unsigned size)
{
 const unsigned char *p=data; unsigned long long h=14695981039346656037ULL;
 for (unsigned i=0;i<size;i++) { h^=p[i]; h*=1099511628211ULL; }
 return h;
}
static void a04_scissor_table(unsigned long long gpu,const void *data,unsigned size)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f || !data || !size) abort();
 if (fprintf(f,"{\\\"event\\\":\\\"scissor_table\\\",\\\"gpu_va\\\":%llu,\\\"size\\\":%u,\\\"fnv64\\\":%llu}\\n",gpu,size,a04_fragment_hash(data,size))<0 || fclose(f)) abort();
}
static void a04_pixel_event(unsigned long long data_gpu,unsigned long long shader_gpu,unsigned shader_size,
                            unsigned long long control,const void *data,unsigned data_size)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f || !data || !data_size) abort();
 if (fprintf(f,"{\\\"event\\\":\\\"render_pixel_event\\\",\\\"data_gpu\\\":%llu,\\\"shader_gpu\\\":%llu,\\\"shader_size\\\":%u,\\\"control\\\":%llu,\\\"data_size\\\":%u,\\\"fnv64\\\":%llu}\\n",data_gpu,shader_gpu,shader_size,control,data_size,a04_fragment_hash(data,data_size))<0 || fclose(f)) abort();
}
static void a04_fragment_link(unsigned long long data,unsigned data_size,unsigned data_written,unsigned long long code,unsigned code_size,unsigned code_written,unsigned long long shader,unsigned long long entry,unsigned shader_size,unsigned doutu_offset,const uint32_t *words)
{
 const char *name=getenv("A04_CAPTURE_JSONL"); FILE *f=name ? fopen(name,"a") : NULL;
 if (!f) abort();
 if (doutu_offset*4u+8u>data_written || code_written!=4u) abort();
 unsigned long long control=((unsigned long long)words[doutu_offset+1]<<32)|words[doutu_offset];
 if (fprintf(f,"{\\"event\\":\\"fragment_shader_link\\",\\"pds_data_gpu\\":%llu,\\"pds_data_size\\":%u,\\"pds_data_written\\":%u,\\"pds_code_gpu\\":%llu,\\"pds_code_size\\":%u,\\"pds_code_written\\":%u,\\"shader_gpu\\":%llu,\\"entry_gpu\\":%llu,\\"shader_size\\":%u,\\"doutu_offset\\":%u,\\"doutu_control\\":%llu,\\"doutu_fnv64\\":%llu,\\"code_fnv64\\":%llu}\\n",data,data_size,data_written,code,code_size,code_written,shader,entry,shader_size,doutu_offset,control & 0x3fe00000000ULL,a04_fragment_hash(&words[doutu_offset],8),a04_fragment_hash(&words[data_written/4u],4))<0 || fclose(f)) abort();
}
'''
 pairs=[('''   state->pds_vertex_attrib_offset =
      pvr_bo->dev_addr.addr -''','''   a04_pds_construct("vertex_attributes",pvr_bo->dev_addr.addr,PVR_DW_TO_BYTES(pds_info->data_size_in_dwords),pds_info->entry_count,dword_buffer);
   state->pds_vertex_attrib_offset =
      pvr_bo->dev_addr.addr -'''),('''   *descriptor_data_offset_out =
      pvr_bo->dev_addr.addr -''','''   a04_pds_construct("descriptor_mappings",pvr_bo->dev_addr.addr,PVR_DW_TO_BYTES(pds_info->data_size_in_dwords),pds_info->entry_count,dword_buffer);
   *descriptor_data_offset_out =
      pvr_bo->dev_addr.addr -''')]
 for before,after in pairs:
  if text.count(before)!=1:raise SystemExit('unexpected Mesa command-buffer source')
  text=text.replace(before,after)
 target='''      if (result != VK_SUCCESS)
         goto err_free_depth_bias_bo;
   }

   util_dynarray_clear(&cmd_buffer->depth_bias_array);'''
 insert='''      if (result != VK_SUCCESS)
         goto err_free_depth_bias_bo;
      a04_scissor_table(sub_cmd->scissor_bo->dev_addr.addr,util_dynarray_begin(&cmd_buffer->scissor_array),cmd_buffer->scissor_array.size);
   }

   util_dynarray_clear(&cmd_buffer->depth_bias_array);'''
 if text.count(target)!=1:raise SystemExit('unexpected scissor upload')
 text=text.replace(target,insert)
 target='''   vk_free(allocator, staging_buffer);

   return result;'''
 insert='''   if (result == VK_SUCCESS)
      a04_pixel_event(device->heaps.pds_heap->base_addr.addr+pds_upload_out->data_offset,
                      usc_eot_program->dev_addr.addr,usc_eot_program->size,pixel_event_program.task_control.src0,
                      staging_buffer,cmd_buffer->device->pixel_event_data_size_in_dwords*4u);
   vk_free(allocator, staging_buffer);

   return result;'''
 if text.count(target)!=1:raise SystemExit('unexpected render pixel event upload')
 text=text.replace(target,insert)
 target='''         pvr_set_usc_execution_address64(&addr, exec_addr.addr);
'''
 insert='''         a04_pds_shader(pvr_bo->dev_addr.addr,doutu_addr->const_offset,vertex_state->shader_bo->dev_addr.addr,exec_addr.addr,vertex_state->shader_bo->size);
'''
 if text.count(target)!=1:raise SystemExit('unexpected vertex shader encoding')
 text=text.replace(target,insert+target)
 frag_start=text.index('setup_pds_fragment_program(')
 frag_end=text.index('setup_pds_coeff_program(',frag_start)
 fragment=text[frag_start:frag_end]
 target='''   return pvr_arch_cmd_buffer_upload_pds(
      cmd_buffer,
      &pds_fragment_program_buffer[0],
      program->data_size,
      16,
      &pds_fragment_program_buffer[program->data_size],
      program->code_size,
      16,
      16,
      pds_fragment_program);'''
 replacement='''   VkResult capture_result=pvr_arch_cmd_buffer_upload_pds(
      cmd_buffer,
      &pds_fragment_program_buffer[0],
      program->data_size,
      16,
      &pds_fragment_program_buffer[program->data_size],
      program->code_size,
      16,
      16,
      pds_fragment_program);
   if (capture_result==VK_SUCCESS && fragment_shader_state->shader_bo) {
      a04_fragment_link(cmd_buffer->device->heaps.pds_heap->base_addr.addr+pds_fragment_program->data_offset,
                        pds_fragment_program->data_size*4u,program->data_size*4u,
                        cmd_buffer->device->heaps.pds_heap->base_addr.addr+pds_fragment_program->code_offset,
                        pds_fragment_program->code_size*4u,program->code_size*4u,
                        fragment_shader_state->shader_bo->dev_addr.addr,
                        fragment_shader_state->shader_bo->dev_addr.addr+state->gfx_pipeline->fs_data.common.entry_offset,
                        fragment_shader_state->shader_bo->size,program->doutu_offset,pds_fragment_program_buffer);
   }
   return capture_result;'''
 if fragment.count(target)!=1:raise SystemExit('unexpected fragment program')
 text=text[:frag_start]+fragment.replace(target,replacement)+text[frag_end:]
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
 target='''            PVR_WRITE(qword_buffer,
                      fs_meta_bo->dev_addr.addr,
                      special_buff_entry->const_offset,'''
 insert='''            a04_pds_fs_meta(pvr_bo->dev_addr.addr,special_buff_entry->const_offset,fs_meta_bo->dev_addr.addr,fs_meta);
'''
 if text.count(target)!=1:raise SystemExit('unexpected FS metadata branch')
 text=text.replace(target,insert+target)
 entry='''      const struct pvr_const_map_entry *const entry_header =
         (struct pvr_const_map_entry *)entries;
'''
 if text.count(entry)!=2:raise SystemExit('unexpected constant-map loops')
 text=text.replace(entry,entry+'''      a04_pds_entry("vertex_attributes",pvr_bo->dev_addr.addr,i,entry_header->type,entry_header->const_offset,0,entry_header->type==PVR_PDS_CONST_MAP_ENTRY_TYPE_LITERAL32 ? ((const struct pvr_const_map_entry_literal32 *)entry_header)->literal_value : entry_header->type==PVR_PDS_CONST_MAP_ENTRY_TYPE_DOUTU_ADDRESS ? ((const struct pvr_const_map_entry_doutu_address *)entry_header)->doutu_control : 0);
''',1)
 second=text.find(entry,text.find('static VkResult pvr_setup_descriptor_mappings('))
 if second<0:raise SystemExit('descriptor loop missing')
 text=text[:second]+text[second:].replace(entry,entry+'''      a04_pds_entry("descriptor_mappings",pvr_bo->dev_addr.addr,i,entry_header->type,entry_header->const_offset,entry_header->type==PVR_PDS_CONST_MAP_ENTRY_TYPE_SPECIAL_BUFFER ? ((const struct pvr_const_map_entry_special_buffer *)entry_header)->buffer_type : 0,entry_header->type==PVR_PDS_CONST_MAP_ENTRY_TYPE_LITERAL32 ? ((const struct pvr_const_map_entry_literal32 *)entry_header)->literal_value : 0);
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
 meta.update(pds_construct_metadata_only=True,pds_fs_meta_metadata_only=True,scissor_metadata_only=True,render_pixel_event_metadata_only=True,pds_original_sha256=sha(original),pds_source_sha256=sha(modified),pds_object_sha256=sha(out/'pvr_arch_cmd_buffer.c.o'),library_sha256=sha(out/'libvulkan_powervr_mesa.so'))
 (out/'diagnostic-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
