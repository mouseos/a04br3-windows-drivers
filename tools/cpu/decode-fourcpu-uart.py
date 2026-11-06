#!/usr/bin/env python3
"""Decode CRC-protected CPU diagnostics from an explicitly selected boot span."""
import argparse, json, re, sys, zlib
from pathlib import Path
PAT=re.compile(rb'^A04CU1(?: [0-9A-F]{8}){8}$')
def decode(data):
    groups={};bad=0
    for line in data.splitlines():
        line=line.rstrip(b'\r')
        if b'A04CU1' not in line:continue
        if not PAT.fullmatch(line):bad+=1;continue
        body,crc=line.rsplit(b' ',1)
        if zlib.crc32(body)&0xffffffff != int(crc,16):bad+=1;continue
        run,seq,cpu,observed,loops,state,elapsed=(int(v,16) for v in body.split()[1:])
        groups.setdefault(run,{}).setdefault((seq,cpu),[]).append((observed,loops,state,elapsed))
    out=[]
    for run,records in sorted(groups.items()):
        contradictions=[list(k) for k,v in records.items() if len(set(v))!=1]
        starts=records.get((0,0xffffffff),[]);ends=records.get((0xffffffff,0xffffffff),[])
        entries=[k[0] for k in records if k[1] in range(4) and k[0] not in (0,0xffffffff)]
        last=max(entries,default=0);core=[]
        for cpu in range(4):
            vals=records.get((last,cpu),[]);v=vals[0] if vals else None
            core.append({'cpu':cpu,'sequence':last,'copies':len(vals),'observed':v[0] if v else None,'iterations':v[1] if v else None,'state':v[2] if v else None,'elapsed_ms':v[3] if v else None})
        end=ends[0] if ends else None
        seqs=sorted(set(entries));progress_ok=len(seqs)>=5 and seqs==list(range(1,last+1))
        if progress_ok:
            for cpu in range(4):
                prior=None
                for seq in seqs:
                    vals=records.get((seq,cpu),[])
                    if not vals:progress_ok=False;break
                    value=vals[0]
                    if value[2] not in (2,3) or value[0]!=cpu or not value[1]:progress_ok=False;break
                    if prior and (value[1]<prior[1] or (prior[2]!=3 and value[1]==prior[1])):progress_ok=False;break
                    prior=value
                if not progress_ok:break
        passed=(not contradictions and len(starts)>=3 and len(ends)>=3 and starts[0]==(0,0,0,0) and end is not None and end[0]==15 and end[1]==4 and end[2]==0 and end[3]>=60000 and progress_ok and all(c['copies']>=3 and c['observed']==c['cpu'] and c['iterations'] and c['state']==3 and c['elapsed_ms']>=60000 for c in core))
        out.append({'run_id':f'{run:08X}','result':'pass_60s' if passed else 'incomplete_or_failed','summary':list(end) if end else None,'start_copies':len(starts),'end_copies':len(ends),'cores':core,'contradictory_records':contradictions,'progress_complete':progress_ok})
    return {'runs':out,'invalid_lines':bad,'interpretation':'Four logical CPUs under this 60-second workload only; not long-term, physical identity, IRQ, suspend, or reboot stability.'}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('--start-offset',required=True,type=int);p.add_argument('--end-offset',type=int);a=p.parse_args()
    size=a.log.stat().st_size;end=size if a.end_offset is None else a.end_offset
    if a.start_offset<0 or end<a.start_offset or end>size:p.error('invalid boot span')
    with a.log.open('rb') as f:f.seek(a.start_offset);data=f.read(end-a.start_offset)
    result=decode(data);result['span']={'start':a.start_offset,'end':end};print(json.dumps(result,ensure_ascii=False,indent=2))
    return 0 if len(result['runs'])==1 and result['runs'][0]['result']=='pass_60s' else 1
if __name__=='__main__':sys.exit(main())
