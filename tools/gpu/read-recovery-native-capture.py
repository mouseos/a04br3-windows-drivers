#!/usr/bin/env python3
"""捕捉器自身のTLV形式を検査する。実GPU成功の判定は行わない。"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def parse(raw,resources_directory=None,allow_incomplete=False):
    pos=0;events=[];pending=set();seen=set();resources=set();read_total=0;kick_read={};topologies={}
    while pos<len(raw):
        if len(raw)-pos<32:raise ValueError("truncated header")
        magic,version,kind,seq,rc,error,count=struct.unpack_from("<8sIIIiiI",raw,pos);pos+=32
        if magic!=b"A04RGCAP" or version!=1 or kind not in (1,2,3,4,5,6,7) or not (seq==0 if kind in (4,6) else 1<=seq<=32):
            raise ValueError("invalid header")
        if count!={1:5,2:1,3:2,4:1,5:1,6:1,7:1}[kind]:raise ValueError("invalid component count")
        components={};offsets={}
        for unused in range(count):
            if len(raw)-pos<8:raise ValueError("truncated TLV")
            tag,size=struct.unpack_from("<II",raw,pos);pos+=8
            if tag in components or size>(0x40000 if kind==3 and tag==8 else 0x1000) or size>len(raw)-pos:raise ValueError("invalid TLV")
            offsets[tag]=pos
            components[tag]=raw[pos:pos+size];pos+=size
        if kind==7:
            if set(components)!={12} or len(components[12])!=64:raise ValueError("alias metadata")
            wire,action,identity,pmr,cpu,size,fd,flags,attempts,live,offset=struct.unpack("<II4Q4IQ",components[12])
            if wire!=1 or action not in (1,2) or not identity or size!=4096 or not flags&16 or flags&2 or not 1<=attempts<=32 or live>8 or offset!=pmr*4096 or rc not in (0,-1):
                raise ValueError("alias contract")
            events.append({"phase":"readonly-alias","seq":seq,"action":"map" if action==1 else "unmap",
                "generation":identity,"pmr":hex(pmr),"cpu_va":hex(cpu),"size":size,"fd":fd,
                "allocation_flags":hex(flags),"attempts":attempts,"live":live,"mmap_offset":offset,
                "result":rc,"errno_only_if_failed":error if rc<0 else None,
                "coverage":"only successful resource records establish captured bytes"})
            continue
        if kind==6:
            if set(components)!={11} or len(components[11])<56:raise ValueError("topology metadata")
            w,fd,identity,pmr,size,chunk,phys,virt,pointer=struct.unpack_from("<IIQQQQIIQ",components[11])
            if w!=1 or not identity or not 0<phys<=256 or not 0<virt<=4096 or not chunk or chunk%4096 or size!=chunk*virt or len(components[11])!=56+phys*4:
                raise ValueError("topology bounds")
            indices=struct.unpack_from(f"<{phys}I",components[11],56)
            if len(set(indices))!=phys or any(x>=virt for x in indices):raise ValueError("topology indices")
            key=(fd,identity)
            if key in topologies:raise ValueError("duplicate topology generation")
            if not events or events[-1]["phase"]!="memory-bridge":raise ValueError("topology lacks allocation journal")
            allocation=events[-1];inp=bytes.fromhex(allocation["input_hex"]);out=bytes.fromhex(allocation["output_hex"])
            if allocation["group"]!=6 or allocation["function"] not in (9,10) or allocation["fd"]!=fd or allocation["ioctl_result"]<0 or len(inp) not in (56,60) or len(out)!=12 or struct.unpack("<QI",out)!=(pmr,0) or struct.unpack_from("<QQIIQ",inp)!=(size,chunk,phys,virt,pointer):
                raise ValueError("topology allocation mismatch")
            topologies[key]=(pmr,size,chunk,set(indices))
            events.append({"phase":"pmr-topology","fd":fd,"generation":identity,"pmr":hex(pmr),
                "logical_size":size,"chunk_size":chunk,"physical_chunks":phys,"virtual_chunks":virt,
                "mapping_table_pointer":hex(pointer),"backed_virtual_indices":list(indices),
                "witness":"same-process fault-safe copies before/after successful NewRamBackedPMR agree"})
            continue
        if kind==4:
            if set(components)!={9} or len(components[9])<24:raise ValueError("bridge journal")
            data=components[9];group,fn,fd,ni,no,unknown=struct.unpack_from("<6I",data)
            if ni>64 or no>32 or len(data)!=24+ni+no:raise ValueError("bridge journal size")
            event={"phase":"memory-bridge","group":group,"function":fn,"fd":fd,
                "ioctl_result":rc,"errno_only_if_failed":error if rc<0 else None,"unknown_before":unknown,
                "input_hex":data[24:24+ni].hex(),"output_hex":data[24+ni:].hex()}
            # common_srvcore_bridge.h: packed CONNECT input16/output17.
            if group==1 and fn==0 and ni==16 and no==17:
                inp=data[24:24+ni];out=data[24+ni:];flags,options,version,build=struct.unpack("<4I",inp)
                event["phase"]="connect"
                event["client"]={"flags":hex(flags),"build_options":hex(options),
                    "ddk_version_raw":hex(version),"ddk_version":f"{version>>16}.{version&65535}","ddk_build":build}
                event["kernel_arch_raw"]=out[0] if rc>=0 else None
                event["bridge_error"]=struct.unpack_from("<I",out,13)[0] if rc>=0 else None
            events.append(event)
            continue
        if kind==3:
            if set(components)!={7,8} or len(components[7]) not in (104,160):raise ValueError("resource metadata")
            m=components[7];wire,status=struct.unpack_from("<II",m)
            pmr,cpu,gpu,offset,pmr_size,cpu_size,gpu_size=struct.unpack_from("<7Q",m,8)
            n,prot,source,unknown=struct.unpack_from("<4I",m,64)
            heap,mapping,fd,cpu_fd=struct.unpack_from("<QQII",m,80)
            if wire not in (1,2) or len(m)!={1:104,2:160}[wire] or status>(7 if wire==1 else 8) or n!=len(components[8]) or n%4:raise ValueError("resource data length")
            if (status in (0,1,8))!=(n>0):raise ValueError("resource status/data mismatch")
            if n and (source not in (1,2) or not prot&1 or cpu%4 or offset>pmr_size or
                      n>pmr_size-offset or n>gpu_size or n>cpu_size):raise ValueError("resource provenance/bounds")
            if wire==1 and n and (status==0)!=(n==gpu_size):raise ValueError("resource coverage/status")
            read_total+=n;kick_read[seq]=kick_read.get(seq,0)+n
            if read_total>16*1024*1024 or kick_read[seq]>4*1024*1024:raise ValueError("resource read cap")
            resources.add(seq);event={"phase":"resource","seq":seq,"status":status,
                "reason":["full-span","prefix-only","no-cpu-mapping","unsafe-or-sparse", "budget", "ambiguous", "unaligned", "journal-lost", "backed-interval"][status],
                "pmr":hex(pmr),"cpu_va":hex(cpu),"gpu_va":hex(gpu),"pmr_offset":offset,
                "pmr_size":pmr_size,"cpu_mapping_size":cpu_size,"gpu_mapping_size":gpu_size,
                "cpu_prot":prot,"source":source,"unknown_count":unknown,"heap":hex(heap),
                "mapping":hex(mapping),"drm_fd":fd,"cpu_fd":cpu_fd,"bytes":n,
                "sha256":hashlib.sha256(components[8]).hexdigest(),"capture_data_offset":offsets[8],
                "reference_coverage":"unknown: full-span does not mean every packet reference was resolved"}
            if wire==2:
                identity,parent_gpu,parent_offset,parent_size,chunk=struct.unpack_from("<5Q",m,104)
                topology=topologies.get((fd,identity))
                if not topology or topology[:3]!=(pmr,pmr_size,chunk):raise ValueError("missing topology witness")
                if offset<parent_offset or gpu!=parent_gpu+offset-parent_offset or offset-parent_offset>parent_size or gpu_size>parent_size-(offset-parent_offset):
                    raise ValueError("backed interval parent bounds")
                if offset>pmr_size or gpu_size>pmr_size-offset:raise ValueError("backed interval PMR bounds")
                if n and (status!=8 or any(x not in topology[3] for x in range(offset//chunk,(offset+n-1)//chunk+1))):
                    raise ValueError("snapshot touches unbacked chunk")
                cpu_base,cpu_offset=struct.unpack_from("<QQ",m,144)
                if n and (offset<cpu_offset or cpu!=cpu_base+offset-cpu_offset or offset-cpu_offset>cpu_size or n>cpu_size-(offset-cpu_offset)):
                    raise ValueError("backed interval CPU VMA bounds")
                event.update({"cpu_mapping_base":hex(cpu_base),"cpu_mapping_pmr_offset":cpu_offset,"generation":identity,"parent_gpu_va":hex(parent_gpu),
                    "parent_pmr_offset":parent_offset,"parent_gpu_mapping_size":parent_size,
                    "chunk_size":chunk,"coverage":"only this backed interval; holes/other intervals remain unknown"})
            if resources_directory is not None and n:
                target=resources_directory/f"resource-{seq:02d}-{len(events):04d}.bin"
                with target.open("xb") as stream:stream.write(components[8])
                event["extracted_path"]=str(target)
            events.append(event);continue
        if kind==5:
            if set(components)!={10} or len(components[10])!=32:raise ValueError("memory summary")
            wire,unknown,total,kick,pmrs,gpus,cpus,journal=struct.unpack("<8I",components[10])
            if wire!=1 or total!=read_total or kick!=kick_read.get(seq,0):raise ValueError("memory summary counters")
            resources.add(seq);events.append({"phase":"memory-summary","seq":seq,"unknown_count":unknown,
                "total_bytes_read":total,"kick_bytes_read":kick,"pmrs":pmrs,"gpu_ranges":gpus,
                "cpu_mappings":cpus,"journal_events":journal,"reference_coverage":"unknown"});continue
        event={"phase":"pre" if kind==1 else "post","seq":seq,"ioctl_result":rc,
               "errno_only_if_failed":error if rc<0 else None}
        if kind==1:
            if seq in seen or set(components)!={1,2,3,4,5}:raise ValueError("pre pairing")
            if [len(components[x]) for x in (1,2,3,5)]!=[32,0x160,0x88,0x150]:raise ValueError("payload lengths")
            if len(components[4]) not in (0,0x150):raise ValueError("PR length")
            pending.add(seq);seen.add(seq)
            event["payloads"]={}
            for tag,name in [(3,"ta"),(4,"pr"),(5,"three")]:
                data=components[tag]
                event["payloads"][name]={"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest(),
                    "hex":data.hex(),"words32":[f"0x{x[0]:08x}" for x in struct.iter_unpack("<I",data)]}
            event["input_hex"]=components[2].hex()
        else:
            if seq not in pending or set(components)!={6} or len(components[6])!=12:raise ValueError("post pairing")
            pending.remove(seq)
            event["output_hex"]=components[6].hex()
            event["bridge_error"]=struct.unpack_from("<I",components[6],8)[0] if rc>=0 else None
        events.append(event)
    complete=not pending and bool(seen) and resources<=seen
    if not complete and not allow_incomplete:raise ValueError("incomplete or empty capture")
    return {"capture_sha256":hashlib.sha256(raw).hexdigest(),"events":events,
            "complete_capture":complete,"missing_post_sequences":sorted(pending),"ta_pre_sequences":sorted(seen),
            "resource_bytes":read_total,"resource_reference_coverage":"unknown; assess all referenced ranges and reasons",
            "gpu_success":"unknown: also require original harness readback and phase logs"}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("capture",type=Path)
    ap.add_argument("--resources-directory",type=Path,help="新規directoryへresource内容だけを抽出")
    ap.add_argument("--allow-incomplete",action="store_true",help="完全なTLVだけの診断結果を返す。capture成功にはしない")
    args=ap.parse_args()
    try:
        if args.resources_directory is not None:args.resources_directory.mkdir(exist_ok=False)
        result=parse(args.capture.read_bytes(),args.resources_directory,args.allow_incomplete)
    except (ValueError,OSError) as exc:ap.error(str(exc))
    print(json.dumps(result,indent=2))
if __name__=="__main__":main()
