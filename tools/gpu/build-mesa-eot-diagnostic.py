#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build isolated Mesa diagnostic to generate private synthetic EOT shaders."""
import argparse
import hashlib
import json
import pathlib
import shlex
import subprocess

PIN = 'ef11349e8e5d620a2227fac0eed17e7ed1f86428'


def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'base', 'pds', 'uploads', 'usc', 'output'):
        p.add_argument('--' + name, type=pathlib.Path, required=True)
    a = p.parse_args()
    src, build, base, pds, up, usc, out = (x.resolve() for x in
        (a.source, a.build, a.base, a.pds, a.uploads, a.usc, a.output))
    git = lambda *x: subprocess.check_output(['git', '-C', str(src), *x], text=True).strip()
    meta = json.loads((base / 'diagnostic-manifest.json').read_text())
    if (git('rev-parse', 'HEAD') != PIN or git('status', '--porcelain', '--untracked-files=all')
            or not meta.get('spm_load_metadata_only') or not meta.get('private_compiler_candidates')
            or sha(build / 'build.ninja') != meta['base_build_ninja_sha256']
            or sha(base / 'libvulkan_powervr_mesa.so') != meta['library_sha256']
            or sha(pds / 'pvr_arch_cmd_buffer.c') != meta['pds_source_sha256']
            or sha(pds / 'pvr_arch_cmd_buffer.c.o') != meta['pds_object_sha256']
            or sha(up / 'pvr_device.c.o') != meta['upload_object_sha256']
            or sha(usc / 'pco_binary.c.o') != meta['pco_object_sha256']
            or sha(base / 'pvr_arch_spm.c.o') != meta['spm_object_sha256']):
        raise SystemExit('source or diagnostic mismatch')
    text = (pds / 'pvr_arch_cmd_buffer.c').read_text()
    marker = '''   usc_temp_count = pco_shader_data(eot)->common.temps;'''
    if text.count(marker) != 1:
        raise SystemExit('unexpected EOT generator')
    injection = r'''   /* Synthetic and captured PBE inputs stay in a private directory. */
   if (emit_count == 1 && !has_tile_buffers) {
      static int a04_done;
      if (!a04_done) {
         a04_done = 1;
         const char *dir = getenv("A04_PCO_CANDIDATE_DIR");
         if (!dir) abort();
         for (unsigned i = 0; i < 197; ++i) {
            uint32_t words[2];
            if (i < 130) {
               unsigned bit = i % 65;
               words[0] = words[1] = i < 65 ? 0 : ~0u;
               if (bit) words[(bit-1)/32] ^= 1u << ((bit-1)%32);
            } else if (i < 132) {
               words[0] = i == 130 ? 0x12345678u : 0xdeadbeefu;
               words[1] = i == 130 ? 0x9abcdef0u : 0x01234567u;
            } else {
               unsigned bit = i - 132;
               words[0] = pbe_cs_words[0];
               words[1] = pbe_cs_words[1];
               if (bit) words[(bit-1)/32] ^= 1u << ((bit-1)%32);
            }
            struct pvr_eot_props probe = { .emit_count = 1, .state_words = words };
            pco_shader *candidate = pvr_usc_eot(device->pdevice->pco_ctx, &probe,
                                                  &device->pdevice->dev_info);
            char path[4096];
            if (!candidate || snprintf(path, sizeof(path), "%s/eot-%03u.bin", dir, i) >= (int)sizeof(path)) abort();
            FILE *f = fopen(path, "wx");
            size_t n = pco_shader_binary_size(candidate);
            if (!f || !n || n > 4096 || fwrite(pco_shader_binary_data(candidate), 1, n, f) != n || fclose(f)) abort();
            ralloc_free(candidate);
         }
      }
   }
'''
    text = text.replace(marker, injection + marker)
    bg_marker = '   *addr_out = clear_bo->dev_addr;'
    if text.count(bg_marker) != 1:
        raise SystemExit('unexpected background constants')
    bg_hook = r'''   {
      const char *name = getenv("A04_CAPTURE_JSONL");
      FILE *f = name ? fopen(name, "a") : NULL;
      if (!f) abort();
      if (fprintf(f, "{\"event\":\"bg_constants\",\"gpu_va\":%llu,\"size\":%u,\"clears\":%u,\"textures\":%u,\"tiles\":%u,\"shareds\":%u,\"fnv64\":%llu}\n",
                  (unsigned long long)clear_bo->dev_addr.addr, buffer_size, next_clear_consts,
                  texture_count, load_op->num_tile_buffers, load_op->shareds_count,
                  a04_fragment_hash(buffer, buffer_size)) < 0 || fclose(f)) abort();
   }
'''
    text = text.replace(bg_marker, bg_hook + bg_marker)
    start = text.index('static VkResult pvr_load_op_pds_data_create_and_upload(')
    end = text.index('static VkResult\npvr_load_op_data_create_and_upload(', start)
    section = text[start:end]
    marker2 = '   vk_free(&cmd_buffer->vk.pool->alloc, staging_buffer);\n\n   return VK_SUCCESS;'
    if section.count(marker2) != 1:
        raise SystemExit('unexpected background PDS data')
    bg_pds = r'''   {
      const char *name = getenv("A04_CAPTURE_JSONL"); FILE *f = name ? fopen(name, "a") : NULL;
      if (!f) abort();
      if (fprintf(f, "{\"event\":\"bg_pds_data\",\"gpu_va\":%llu,\"size\":%u,\"constants_gpu\":%llu,\"shareds\":%u,\"fnv64\":%llu}\n",
                  (unsigned long long)(device->heaps.pds_heap->base_addr.addr+pds_upload_out->data_offset),
                  staging_buffer_size, (unsigned long long)constants_addr.addr,
                  load_op->shareds_count, a04_fragment_hash(staging_buffer, staging_buffer_size)) < 0 || fclose(f)) abort();
   }
'''
    section = section.replace(marker2, bg_pds + marker2)
    text = text[:start] + section + text[end:]
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    (out / 'pvr_arch_cmd_buffer.c').write_text(text)
    target = 'src/imagination/vulkan/libvulkan_powervr_mesa.so'
    cmds = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True).splitlines()
    args = shlex.split(next(x for x in cmds if 'pvr_arch_cmd_buffer.c.o -c ' in x))
    args[args.index('-o') + 1] = str(out / 'pvr_arch_cmd_buffer.c.o')
    args[-1] = str(out / 'pvr_arch_cmd_buffer.c')
    subprocess.run(args, cwd=build, check=True)
    mrt_source = src / 'src/imagination/vulkan/pvr_arch_mrt.c'
    mrt = mrt_source.read_text()
    mrt_marker = '   load_op->temps_count = pco_shader_data(loadop)->common.temps;'
    if mrt.count(mrt_marker) != 1:
        raise SystemExit('unexpected background shader generator')
    mrt_hook = r'''   {
      static unsigned seq;
      const char *dir = getenv("A04_PCO_CANDIDATE_DIR");
      const char *name = getenv("A04_CAPTURE_JSONL");
      char path[4096]; size_t n = pco_shader_binary_size(loadop);
      if (!dir || !name || !n || n > 4096 || snprintf(path, sizeof(path), "%s/loadop-%03u.bin", dir, seq) >= (int)sizeof(path)) abort();
      FILE *f = fopen(path, "wx");
      if (!f || fwrite(pco_shader_binary_data(loadop), 1, n, f) != n || fclose(f)) abort();
      f = fopen(name, "a"); if (!f) abort();
      if (fprintf(f, "{\"event\":\"bg_shader\",\"ordinal\":%u,\"gpu_va\":%llu,\"size\":%zu,\"clear_mask\":%u,\"load_mask\":%u,\"pds_gpu\":%llu}\n",
                  seq, (unsigned long long)load_op->usc_frag_prog_bo->dev_addr.addr, n,
                  load_op->clears_loads_state.rt_clear_mask, load_op->clears_loads_state.rt_load_mask,
                  (unsigned long long)(device->heaps.pds_heap->base_addr.addr+load_op->pds_frag_prog.data_offset)) < 0 || fclose(f)) abort();
      ++seq;
   }
'''
    mrt = '#include <stdio.h>\n#include <stdlib.h>\n' + mrt.replace(mrt_marker, mrt_hook + mrt_marker)
    start = mrt.index('static VkResult pvr_pds_fragment_program_create_and_upload(')
    end = mrt.index('VkResult\npvr_arch_load_op_shader_generate(', start)
    section = mrt[start:end]
    marker3 = '   vk_free2(&device->vk.alloc, allocator, staging_buffer);\n\n   return VK_SUCCESS;'
    if section.count(marker3) != 1:
        raise SystemExit('unexpected background fragment PDS')
    pds_hook = r'''   {
      const char *name = getenv("A04_CAPTURE_JSONL"); FILE *f = name ? fopen(name, "a") : NULL;
      const unsigned char *p = (const unsigned char *)staging_buffer;
      unsigned long long h = 14695981039346656037ULL;
      for (unsigned i=0; i<program.data_size*4; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
      if (!f) abort();
      if (fprintf(f, "{\"event\":\"bg_frag_pds\",\"gpu_va\":%llu,\"shader_gpu\":%llu,\"control\":%llu,\"size\":%u,\"fnv64\":%llu}\n",
                  (unsigned long long)(device->heaps.pds_heap->base_addr.addr+pds_frag_prog->data_offset),
                  (unsigned long long)shader_bo->dev_addr.addr,
                  (unsigned long long)program.usc_task_control.src0, program.data_size*4, h) < 0 || fclose(f)) abort();
   }
'''
    mrt = mrt[:start] + section.replace(marker3, pds_hook + marker3) + mrt[end:]
    (out / 'pvr_arch_mrt.c').write_text(mrt)
    args = shlex.split(next(x for x in cmds if 'pvr_arch_mrt.c.o -c ' in x))
    args[args.index('-o') + 1] = str(out / 'pvr_arch_mrt.c.o')
    args[-1] = str(out / 'pvr_arch_mrt.c')
    subprocess.run(args, cwd=build, check=True)
    members = subprocess.check_output(['ar', 't', str(base / 'libpowervr_rogue.a')], text=True).splitlines()
    chosen = [x for x in members if x.endswith('/pvr_arch_cmd_buffer.c.o')]
    if len(chosen) != 1 or pathlib.Path(chosen[0]).resolve() != (pds / 'pvr_arch_cmd_buffer.c.o'):
        raise SystemExit('unexpected archive')
    mrt_member = [x for x in members if x.endswith('/pvr_arch_mrt.c.o')]
    if len(mrt_member) != 1:
        raise SystemExit('unexpected MRT archive')
    replacements = {chosen[0]: str(out / 'pvr_arch_cmd_buffer.c.o'), mrt_member[0]: str(out / 'pvr_arch_mrt.c.o')}
    subprocess.run(['ar', 'crsT', str(out / 'libpowervr_rogue.a'), *[
        replacements.get(x, str(pathlib.Path(x).resolve())) for x in members]], check=True)
    args = shlex.split(next(x for x in cmds if x.startswith('c++  -o src/imagination/vulkan/libvulkan_powervr_mesa.so ')))
    args[args.index('-o') + 1] = str(out / 'libvulkan_powervr_mesa.so')
    repl = {'src/imagination/vulkan/libpowervr_rogue.a': str(out / 'libpowervr_rogue.a'),
            'src/imagination/vulkan/libvulkan_powervr_mesa.so.p/pvr_device.c.o': str(up / 'pvr_device.c.o'),
            'src/imagination/pco/libpowervr_compiler.a': str(usc / 'libpowervr_compiler.a')}
    subprocess.run([repl.get(x, x) for x in args], cwd=build, check=True)
    meta.update(background_shader_private=True, mrt_original_sha256=sha(mrt_source), mrt_source_sha256=sha(out / 'pvr_arch_mrt.c'), mrt_object_sha256=sha(out / 'pvr_arch_mrt.c.o'), background_constants_metadata_only=True, eot_synthetic_private=True, eot_source_sha256=sha(out / 'pvr_arch_cmd_buffer.c'),
                eot_object_sha256=sha(out / 'pvr_arch_cmd_buffer.c.o'),
                library_sha256=sha(out / 'libvulkan_powervr_mesa.so'))
    (out / 'diagnostic-manifest.json').write_text(json.dumps(meta, indent=2) + '\n')


if __name__ == '__main__':
    main()
