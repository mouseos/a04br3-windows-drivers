#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate observed ioctl object lifetimes; never authorize GPU replay."""
import argparse
import json
import pathlib
import sys

GPU_LIMIT = 1 << 40


def check(path):
    vms, bos, maps, contexts, freelists, hwrts = set(), {}, {}, {}, {}, {}
    jobs = submits = refs = unresolved = full_checks = start_checks = zeroed_bos = 0
    uploads = unmapped_uploads = ambiguous_uploads = 0
    pds_entries = 0
    pds_deps = unmapped_deps = 0
    pds_shaders = unmapped_shaders = 0
    fragment_links = fragment_unmapped = fragment_missing_uploads = 0
    live_uploads = []
    open_submit = None
    for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
        e = json.loads(line); op = e['event']
        if op == 'vm_create':
            if e['handle'] in vms: raise ValueError('duplicate vm')
            vms.add(e['handle'])
        elif op == 'vm_destroy':
            if e['handle'] not in vms: raise ValueError('unknown vm')
            vms.remove(e['handle'])
            maps = {k:v for k,v in maps.items() if k[0] != e['handle']}
            live_uploads = [u for u in live_uploads if u[0][0] != e['handle']]
        elif op == 'bo_create':
            if (e['fd'], e['handle']) in bos or e['size'] <= 0 or e.get('initial_origin') != 'zeroed_by_shim': raise ValueError('invalid BO')
            zeroed_bos += 1
            bos[(e['fd'], e['handle'])] = e['size']
        elif op == 'bo_close':
            key = (e['fd'], e['handle'])
            if key not in bos: raise ValueError('unknown BO close')
            del bos[key]
        elif op == 'bo_cpu_access_requested':
            if (e['fd'],e['handle']) not in bos: raise ValueError('unknown BO')
        elif op == 'vm_map':
            vm, va, size, off = e['vm'], e['va'], e['size'], e['offset']
            if (vm not in vms or (e['fd'], e['handle']) not in bos or va <= 0 or size <= 0 or
                va >= GPU_LIMIT or size > GPU_LIMIT-va or off < 0 or
                off + size > bos[(e['fd'], e['handle'])]): raise ValueError('invalid mapping')
            if any(k[0] == vm and va < k[1]+v and k[1] < va+size for k,v in maps.items()):
                raise ValueError('overlapping mappings')
            maps[(vm,va)] = size
        elif op == 'vm_unmap':
            key = (e['vm'],e['va'])
            if maps.get(key) != e['size']: raise ValueError('invalid unmap')
            del maps[key]
            live_uploads = [u for u in live_uploads if u[0] != key]
        elif op == 'context_create':
            if e['vm'] not in vms or e['handle'] in contexts: raise ValueError('invalid context')
            contexts[e['handle']] = e
        elif op == 'context_destroy':
            del contexts[e['handle']]
        elif op == 'freelist_create':
            if e['vm'] not in vms or e['handle'] in freelists: raise ValueError('invalid freelist')
            freelists[e['handle']] = e
        elif op == 'freelist_destroy':
            del freelists[e['handle']]
        elif op == 'hwrt_create':
            if e['handle'] in hwrts or len(e['freelists']) != 2 or len(e['rt']) != 2 or any(len(x)!=3 for x in e['rt']) or len(e['merge']) != 6: raise ValueError('invalid hwrt')
            hwrts[e['handle']] = e
        elif op == 'hwrt_destroy':
            del hwrts[e['handle']]
        elif op in ('upload','pds_construct'):
            addr, size = e['gpu_va'], e['size']
            if e['kind'] not in ('pds_data','pds_code','usc_code','vertex_attributes','descriptor_mappings') or addr <= 0 or size <= 0 or addr >= GPU_LIMIT or size > GPU_LIMIT-addr:
                raise ValueError('invalid upload')
            matches=[k for k,v in maps.items() if addr >= k[1] and size <= v and addr-k[1] <= v-size]
            uploads += 1
            if not matches: unmapped_uploads += 1
            elif len(matches) > 1: ambiguous_uploads += 1
            if len(matches) == 1:
                key = matches[0]
                live_uploads = [u for u in live_uploads if u[0] != key or not (addr < u[1]+u[2] and u[1] < addr+size)]
                live_uploads.append((key,addr,size,e['kind']))
        elif op == 'fragment_shader_link':
            ranges = [(e['pds_data_gpu'], e['pds_data_size'], e['pds_data_written'], 'pds_data'),
                      (e['pds_code_gpu'], e['pds_code_size'], e['pds_code_written'], 'pds_code'),
                      (e['shader_gpu'], e['shader_size'], 0, 'usc_code')]
            fragment_vms = []
            for addr, size, written, kind in ranges:
                if addr <= 0 or addr >= GPU_LIMIT or size <= 0 or size > GPU_LIMIT-addr or written < 0 or written > size:
                    raise ValueError('invalid fragment range')
                matches = {k[0] for k,v in maps.items() if addr >= k[1] and size <= v and addr-k[1] <= v-size}
                fragment_vms.append(matches)
            if not ranges[0][2] or not ranges[1][2] or e['doutu_offset'] < 0 or e['doutu_offset']*4+8 > ranges[0][2]:
                raise ValueError('invalid fragment DOUTU')
            if not e['shader_gpu'] <= e['entry_gpu'] < e['shader_gpu']+e['shader_size']:
                raise ValueError('invalid fragment shader entry')
            common = fragment_vms[0] & fragment_vms[1] & fragment_vms[2]
            fragment_links += 1
            if len(common) != 1:
                fragment_unmapped += 1
            else:
                vm = next(iter(common))
                for addr,size,written,kind in ranges:
                    matches = [u for u in live_uploads if u[0][0] == vm and u[1] == addr and u[3] == kind and (u[2] == written if written else u[2] <= size)]
                    if len(matches) != 1 or (kind == 'usc_code' and not addr <= e['entry_gpu'] < addr+matches[0][2]):
                        fragment_missing_uploads += 1
        elif op == 'pds_shader':
            parent,shader,entry,size=e['parent_gpu'],e['shader_gpu'],e['entry_gpu'],e['size']
            if parent<=0 or shader<=0 or shader>=GPU_LIMIT or size<=0 or size>GPU_LIMIT-shader or entry<shader or entry-shader>=size:
                raise ValueError('invalid PDS shader')
            vma={k[0] for k,v in maps.items() if parent>=k[1] and parent-k[1]<v}
            vmb={k[0] for k,v in maps.items() if shader>=k[1] and size<=v and shader-k[1]<=v-size}
            pds_shaders+=1
            if len(vma & vmb)!=1: unmapped_shaders+=1
        elif op == 'pds_dependency':
            if e['kind'] not in ('spill','scratch') or e['buffer_size']<=0 or e['block_size']<=0 or e['offset']<0:
                raise ValueError('invalid PDS dependency')
            pds_deps += 1
            ranges=((e['parent_gpu'],1),(e['info_gpu'],12),(e['buffer_gpu'],e['buffer_size']))
            range_vms=[]
            for addr,size in ranges:
                if addr<=0 or addr>=GPU_LIMIT or size>GPU_LIMIT-addr:
                    raise ValueError('invalid PDS dependency range')
                range_vms.append({k[0] for k,v in maps.items() if addr>=k[1] and size<=v and addr-k[1]<=v-size})
            if len(range_vms[0] & range_vms[1] & range_vms[2])!=1:
                unmapped_deps += 1
        elif op == 'pds_entry':
            if e['kind'] not in ('vertex_attributes','descriptor_mappings') or e['ordinal']<0 or e['entry_type']<0:
                raise ValueError('invalid PDS entry')
            pds_entries += 1
        elif op == 'pco_binary':
            if e['stage'] < 0 or e['size'] <= 0 or e['internal'] not in (0,1) or not 0 <= e['fnv64'] < 1<<64:
                raise ValueError('invalid compiler binary metadata')
        elif op == 'usc_upload_hash':
            if e['gpu_va'] <= 0 or e['gpu_va'] >= GPU_LIMIT or e['size'] <= 0 or e['size'] > GPU_LIMIT-e['gpu_va'] or not 0 <= e['fnv64'] < 1<<64:
                raise ValueError('invalid USC upload hash')
        elif op == 'submit_begin':
            if open_submit is not None: raise ValueError('nested submit')
            open_submit = [e['count'],0]; submits += 1
        elif op == 'job':
            if open_submit is None or e['index'] != open_submit[1]: raise ValueError('job sequence')
            open_submit[1] += 1; jobs += 1
            c = contexts.get(e['context'])
            if c is None or c['vm'] not in vms: raise ValueError('job context not live')
            if e['type'] not in (0,1,2,3) or c['type'] != (0 if e['type'] < 2 else e['type']-1): raise ValueError('context type mismatch')
            if e['type'] in (0,1):
                h = hwrts.get(e['hwrt'])
                if h is None or e['rt_index'] not in (0,1): raise ValueError('job hwrt not live')
                vm = c['vm']
                def mapped(addr, length):
                    return any(k[0] == vm and addr >= k[1] and length <= v and addr-k[1] <= v-length for k,v in maps.items())
                for f_id in h['freelists']:
                    f = freelists.get(f_id)
                    if f is None or f['vm'] != vm: raise ValueError('freelist not live')
                    refs += 1; full_checks += 1
                    if not mapped(f['gpu'], f['max']*4): unresolved += 1
                refs += 1; full_checks += 1
                if not mapped(h['tpc'], h['tpc_size']): unresolved += 1
                # These fields lack a recorded extent. Count start-address checks
                # only and retain the global non-exportable status.
                for addr in [h['vheap'],h['rtc'],*h['rt'][e['rt_index']]]:
                    if addr:
                        refs += 1; start_checks += 1
                        if not mapped(addr,1): unresolved += 1
            elif e['hwrt'] or e['rt_index']: raise ValueError('non-render hwrt')
        elif op == 'submit_end':
            if open_submit is None or open_submit[0] != open_submit[1]: raise ValueError('incomplete submit')
            open_submit = None
        else:
            raise ValueError('unknown event')
    if open_submit is not None or not submits: raise ValueError('incomplete trace')
    return {'submits':submits, 'jobs':jobs, 'live_bos_at_end':len(bos),
            'zeroed_bo_allocations':zeroed_bos, 'uploads_seen':uploads, 'pds_entries_seen':pds_entries,'pds_dependencies_seen':pds_deps,'pds_dependency_unmapped':unmapped_deps,'pds_shader_links':pds_shaders,'pds_shader_unmapped':unmapped_shaders,
            'fragment_links':fragment_links, 'fragment_unmapped':fragment_unmapped, 'fragment_missing_uploads':fragment_missing_uploads,
            'uploads_unmapped_at_observation':unmapped_uploads, 'uploads_ambiguous_vm':ambiguous_uploads, 'full_extent_checks':full_checks,
            'start_only_checks':start_checks, 'unresolved_address_checks':unresolved,
            'fixture_exportable':False,
            'reasons':['CPU writes and byte-level BO origin are untracked',
                       'DRM fd close and imported BO provenance are not observed',
                       'command streams and indirect GPU references are not decoded',
                       'some HWRT extents and GPU writes are unknown',
                       'shim records requested ioctls and does not execute GPU jobs']}


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('trace'); a=ap.parse_args()
    try:
        print(json.dumps(check(a.trace),indent=2)); return 1
    except (ValueError,KeyError,TypeError,OSError,json.JSONDecodeError):
        print('invalid trace', file=sys.stderr); return 2
if __name__ == '__main__': sys.exit(main())
