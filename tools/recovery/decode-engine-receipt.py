#!/usr/bin/env python3
"""Decode only fixed private native receipts; no registry/device access."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

NODE=('observed','discovery_rc','status_cr','status_valid','status','problem','property_cr','property_type','property_bytes','nt_valid','ntstatus')
CLIENT=('observed','created','resumed','wait_finished','exit_valid','exit_code','result')
VOLUME=('attempted','identity_error','open_error','flush_error','close_error','elapsed_ms','returned')
STREAM=('error','file_size_low','file_size_high','captured_bytes','truncated')
AUDIO=('schema','bytes','inf_revision','stage','result','system_time_low','system_time_high','dma_called','description_version','scatter_gather','address_width','map_registers','adapter_present','operations_present')

def value(values,name,kind=None):
    item=values.get(name,{})
    if not item.get('available',True) or 'value' not in item:return None
    if kind and item.get('kind')!=kind:raise ValueError('Fixed receipt type mismatch: '+name)
    v=item['value']
    if kind=='DWord':
        if not isinstance(v,int) or not -(2**31)<=v<2**32:raise ValueError('Invalid DWORD')
        return v & 0xffffffff
    return v

def binary(values,name):
    v=value(values,name,'Binary')
    if v is None:return None
    if not isinstance(v,str) or len(v)>131072 or not re.fullmatch('[0-9a-fA-F]*',v) or len(v)%2:raise ValueError('Invalid bounded hex record')
    return bytes.fromhex(v)

def record(values,name,fields):
    data=binary(values,name)
    if data is None:return {'available':False}
    if len(data)!=4*len(fields):raise ValueError('Fixed binary length mismatch: '+name)
    return {'available':True,**dict(zip(fields,struct.unpack('<'+'I'*len(fields),data)))}

def decode(receipt,out,policy_hash,audio_revision=1007,wifi_build=1006,wifi_baseline=None):
    if (receipt.get('status')!='fixed-receipt-read-unloaded' or receipt.get('errors') or receipt.get('policy_hash')!=policy_hash
        or any(receipt.get(k)!=0 for k in ('load_rc','unload_rc','privilege_restore_error'))):raise ValueError('Native receipt not eligible/unloaded or stale policy')
    result={'scope':'persisted fixed observations only; not whole-volume health or all-driver success','policy_hash':policy_hash,'backup_manifest_sha256':receipt['backup_manifest_sha256'],'live_query':False,'phases':{}}
    for phase in ('Install','Diagnostic'):
        r=receipt.get('engine',{}).get(phase,{})
        if not r.get('present'):result['phases'][phase]={'present':False};continue
        v=r['values'];schema=value(v,'Schema','DWord');count=value(v,'TargetCount','DWord');clients=value(v,'ClientCount','DWord')
        if schema not in (1,2) or count not in ((5,6) if schema==1 else (5,6,7)) or clients not in (2,3) or value(v,'PolicyHash','String')!=policy_hash:raise ValueError('Bounded phase schema/count/policy mismatch')
        rec={'present':True,'schema':schema,'complete':value(v,'Complete','DWord'),'run_id':value(v,'RunId','DWord'),'target_count':count,'client_count':clients,'nodes':{},'clients':{},'streams':{}}
        for key in ('ServiceResult','FileLogError','UartError','SnapshotWriteError'):
            rec[key]=value(v,key,'DWord')
        if schema==2:
            rec['CompletionResult']=value(v,'CompletionResult','DWord');rec['volume_flush']=record(v,'SystemVolumeFlush',VOLUME);rec['volume_flush_error']=value(v,'SystemVolumeFlushError','DWord')
            vr=rec['volume_flush'];vr['operation_succeeded']=(all(vr.get(k)==0 for k in ('identity_error','open_error','flush_error','close_error')) if vr.get('available') and vr.get('attempted')==vr.get('returned')==1 else None)
            vr['clean_unmount_or_hardware_health_proven']=False
        ids=['ACPI\\BEN0001','MT8167\\AFE','MT8167\\USB20','A04BR3\\MT6630SDIO','ACPI\\MOU0007']
        for i in range(count):
            hwid=value(v,f'Node{i}HardwareId','String')
            if i<5 and hwid!=ids[i]:raise ValueError('Fixed base node mismatch')
            if i==5 and (hwid not in ('ACPI\\MOU0006','ACPI\\MOU0013') or (schema==1 and hwid!='ACPI\\MOU0006')):raise ValueError('Fixed optional node mismatch')
            if i==6 and (hwid!='ACPI\\MOU0013' or value(v,'Node5HardwareId','String')!='ACPI\\MOU0006'):raise ValueError('GPU/SD order mismatch')
            node={'hardware_id':hwid}
            for when in ('Before','After'):
                n=record(v,f'Node{i}{when}',NODE)
                if n.get('available'):
                    if any(n[k] not in (0,1) for k in ('observed','status_valid','nt_valid')):raise ValueError('Invalid node validity bits')
                    n['problem_status_hex']=f"0x{n['ntstatus']:08x}" if n['nt_valid'] else None
                node[when.lower()]=n
            rec['nodes'][str(i)]=node
        for i in range(clients):
            rec['clients'][str(i)]=record(v,f'Client{i}',CLIENT)
            for stream in ('Stdout','Stderr'):
                name=f'Client{i}{stream}';data=binary(v,name);info=record(v,name+'Info',STREAM);rec['streams'][name]={'available':data is not None,'metadata':info}
                if data is not None:
                    if info.get('available') and info.get('error')==0 and len(data)!=info['captured_bytes']:raise ValueError('Captured stream length mismatch')
                    file=phase+'-'+name+'.txt';(out/file).write_bytes(data);rec['streams'][name].update(file=file,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
        result['phases'][phase]=rec
    a=receipt.get('audio',{});result['audio_start']={'present':bool(a.get('present')),'valid':False,'expected_inf_revision':audio_revision}
    if a.get('present'):
        v=a['values'];a=record(v,'NativeStart',AUDIO);a['complete']=value(v,'NativeStartComplete','DWord');a['valid']=bool(a.get('available') and a['complete']==1 and a['schema']==1 and a['bytes']==56 and a['inf_revision']==audio_revision)
        a['expected_inf_revision']=audio_revision;a['result_hex']=f"0x{a['result']:08x}" if a.get('available') else None;result['audio_start']=a
    w=receipt.get('wifi',{});result['wifi']={'present':bool(w.get('present')),'expected_driver_build':wifi_build,'records':{}}
    if w.get('present'):
        spec=importlib.util.spec_from_file_location('wifi_checkpoint',Path(__file__).resolve().parents[1]/'winpe/decode-mt6630-checkpoint.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        for name in ('DriverEntry','Initialize'):
            data=binary(w['values'],name)
            if data is None:result['wifi']['records'][name]={'available':False};continue
            parsed=mod.decode(data,wifi_baseline);parsed['expected_build_matches']=parsed['driver_build']==wifi_build;result['wifi']['records'][name]=parsed
            (out/(name+'.bin')).write_bytes(data);(out/(name+'-decoded.json')).write_text(json.dumps(parsed,indent=2)+'\n')
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--receipt',type=Path,required=True);ap.add_argument('--receipt-sha256',required=True);ap.add_argument('--policy-hash',required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--audio-revision',type=int,choices=(1006,1007),default=1007);ap.add_argument('--wifi-build',type=int,choices=(1003,1004,1005,1006),default=1006);ap.add_argument('--wifi-baseline-generation',type=int)
    a=ap.parse_args();root=Path(__file__).resolve().parents[4];receipt=a.receipt.resolve(strict=True);out=a.out.resolve()
    if not (root/'windows-rt/AGENTS.md').is_file() or not receipt.is_relative_to(root) or not out.is_relative_to(root) or out.exists():raise ValueError('New private root-contained output and receipt required')
    if not re.fullmatch('[0-9a-f]{64}',a.policy_hash) or hashlib.sha256(receipt.read_bytes()).hexdigest()!=a.receipt_sha256:raise ValueError('Receipt/policy pin mismatch')
    out.mkdir(mode=0o700);r=decode(json.loads(receipt.read_text(encoding='utf-8-sig')),out,a.policy_hash,a.audio_revision,a.wifi_build,a.wifi_baseline_generation);(out/'summary.json').write_text(json.dumps(r,indent=2)+'\n')
    for p in out.iterdir():p.chmod(0o600)
    print(json.dumps({'status':'decoded-fixed-observations-not-driver-success','summary_sha256':hashlib.sha256((out/'summary.json').read_bytes()).hexdigest()}))
if __name__=='__main__':main()
