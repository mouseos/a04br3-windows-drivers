#!/usr/bin/env python3
"""既存UART regular fileの限定snapshotを読み、既知数値だけ出力する。device/VM操作なし。"""
import argparse
import json
import os
from pathlib import Path
import re
import stat

SOURCE='73427b486f3e621dd998774f38f396fbac54a61d'
LABELS=frozenset('BEGIN UART_OPEN_ERROR SAVE_ERROR WLANSVC_ERROR DLL_ERROR NORMAL_OS_REQUIRED API_MISSING OPEN_ERROR ENUM_ERROR INTERFACES EVENT_ERROR BASELINE_ERROR BASELINE_INVALID NOTIFY_ERROR REQUEST_ERROR REQUEST_ACCEPTED WAIT_STATUS UNREGISTER_ERROR COMPLETE_EVENT FAIL_EVENT FAIL_REASON BSS_ERROR BSS_TOTAL FRESH_KNOWN BSS_NEW_IDENTITIES BSS_NEW_OR_UPDATED BSS_INVALID SCAN_EXIT UART_ERROR END'.split())
WLAN=re.compile(rb'A04WLAN RUN=([0-9]{1,10}) SEQ=([0-9]{1,10}) IF=([0-9]{1,10}) ([A-Z_]{1,32})=([0-9]{1,10})')
PNP=re.compile(rb'A04WIFI PNP=([0-9A-F]{8}) MATCHES=([0-9A-F]{8}) STATUS=([0-9A-F]{8}) PROBLEM=([0-9A-F]{8})')
FW=re.compile(rb'A04WIFI FW WMT=([0-9A-F]{8}) ERR=([0-9A-F]{8}) WLAN=([0-9A-F]{8}) ERR=([0-9A-F]{8})')
# 同じ既知3patternのみ除去。未知の混入は復元せず捨てる。rawを出力/複製しない。
STORAGE=re.compile(rb'A04WIN (?:StartIo fn=[0-9A-F]{8} opcode=[0-9A-F]{8}|blocks lba=[0-9A-F]{8} count=[0-9A-F]{8}|CDB=[0-9A-F]{8} status=[0-9A-F]{8})\r?\n')
ERRORS={'WLANSVC_ERROR':'wlan_service','DLL_ERROR':'wlan_api','OPEN_ERROR':'wlan_handle_not_80211_auth','ENUM_ERROR':'interface_enumeration','EVENT_ERROR':'scan_observer','BASELINE_ERROR':'scan_cache','NOTIFY_ERROR':'scan_notification','REQUEST_ERROR':'scan_request','UNREGISTER_ERROR':'scan_notification','BSS_ERROR':'scan_results'}

def parse_line(line):
    m=WLAN.fullmatch(line)
    if m:
        run,seq,index,label,value=m.groups();label=label.decode('ascii')
        if label not in LABELS:return None
        run,seq,index,value=map(int,(run,seq,index,value))
        if max(run,seq,value)>0xffffffff or index>15:return None
        return dict(kind='scan_probe',run=run,sequence=seq,interface_index=index,event=label,value=value)
    m=PNP.fullmatch(line)
    if m:return dict(kind='pnp',**{k:int(v,16) for k,v in zip(('configret','matches','devnode_status','problem'),m.groups())})
    m=FW.fullmatch(line)
    if m:return dict(kind='firmware_files',**{k:int(v,16) for k,v in zip(('wmt_bytes','wmt_error','wlan_bytes','wlan_error'),m.groups())})
    return None


def observe(stream,start,end,max_events=4096):
    """入力byte順。run IDはGetTickCountなのでboot identityとは扱わない。"""
    stream.seek(start);offset=start;buffer=b'';events=[];oversized=False;dropping=start>0;stop=False
    while offset<end and not stop:
        chunk=stream.read(min(65536,end-offset))
        if not chunk:break
        offset+=len(chunk);buffer+=chunk
        # 行境界の既知storage混入を同じchunk bufferでだけ除去。
        # source byte offsetはchunk単位の上限。架空のwall-clock timestampは付けない。
        buffer=STORAGE.sub(b'',buffer)
        while b'\n' in buffer:
            line,buffer=buffer.split(b'\n',1)
            if dropping:dropping=False;continue
            if oversized:oversized=False;continue
            if len(line)>512:continue
            item=parse_line(line.rstrip(b'\r'))
            if item:
                item['order']=len(events);item['source_byte_upper_bound']=offset;events.append(item)
                if len(events)>=max_events:stop=True;break
        if len(buffer)>4096:buffer=b'';oversized=True
    failures=[];scan_runs=[];active={}
    for e in events:
        if e['kind']=='scan_probe':
            key=(e['run'],e['interface_index'])
            if e['event']=='BEGIN' or key not in active:
                row=dict(run=e['run'],interface_index=e['interface_index'],observed_labels={},duplicate_or_reordered_sequence=False,last_sequence=-1)
                scan_runs.append(row);active[key]=row
            row=active[key]
            if e['sequence']<=row['last_sequence']:row['duplicate_or_reordered_sequence']=True
            row['last_sequence']=e['sequence'];row['observed_labels'][e['event']]=e['value']
            if e['event'] in ERRORS and e['value']:
                failures.append(dict(order=e['order'],scope=ERRORS[e['event']],code_kind='WIN32_ERROR',code=e['value']))
            elif e['event']=='FAIL_REASON' and e['value']:
                failures.append(dict(order=e['order'],scope='scan_notification',code_kind='WLAN_REASON_CODE',code=e['value']))
        elif e['kind']=='pnp' and (e['configret'] or e['problem']):
            failures.append(dict(order=e['order'],scope='pnp_only',code_kind='CONFIGRET',code=e['configret'],problem=e['problem']))
        elif e['kind']=='firmware_files':
            for name in ('wmt_error','wlan_error'):
                if e[name]:failures.append(dict(order=e['order'],scope='firmware_file_metadata_not_boot',code_kind='WIN32_ERROR',code=e[name]))
    for r in scan_runs:
        v=r.pop('observed_labels');r.pop('last_sequence')
        # COMPLETE_EVENTだけでもscan callbackを観測。通信成功ではない。
        r['scan_complete_callback_observed']=v.get('COMPLETE_EVENT')==1
        r['scan_failure_callback_observed']=v.get('FAIL_EVENT')==1
        r['bss_list_observed']=v.get('BSS_ERROR')==0 and 'BSS_TOTAL' in v
        r['scan_exit']=v.get('SCAN_EXIT');r['probe_end']=v.get('END')
        r['not_a_connection_probe']=True
    return dict(schema='mt6630-1027-existing-uart-observer-v1',source_contract_commit=SOURCE,
        byte_range=dict(start=start,snapshot_end=end,read_through=offset),event_limit_reached=stop,
        timeline_order='file order only; byte bounds are chunk upper bounds, no wall clock/boot identity',
        timeline=events,scan_probe_runs=scan_runs,failures=failures,
        stages={
            'scan':'scan_callback_observed' if any(r['scan_complete_callback_observed'] for r in scan_runs) else 'scan_events_only' if scan_runs else 'unobserved_in_selected_bytes',
            'association':'unverified_no_1027_uart_event',
            'authentication':'unverified_no_1027_uart_event',
            'key':'unverified_no_1027_uart_event',
            'controlled_port':'unverified_no_1027_uart_event',
            'IP':'unverified_no_1027_uart_event'},
        checkpoint_stages=[],NTSTATUS_events=[],connection_test_performed='unknown_from_this_uart_contract',
        communication_success_claimed=False,
        limitations=[
            '1027 NativeCheckpoint stage/NTSTATUS/backend/OID records are registry-only, not UART output.',
            'A04WLAN is a scan-only probe. OPEN_ERROR is WlanOpenHandle, not 802.11 authentication.',
            'No recognized event is not proof no connection test occurred or that a driver failed.',
            'No device serial/boot identity is bound; history/repeated probes may coexist. Use a main-selected byte range.',
            'Unknown/partial/overlong lines and unknown interleaving are discarded without copying text.',
            'Input growth after snapshot is excluded; no wait, device access, reset, scan or connection request.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',required=True,type=Path)
    p.add_argument('--uart-file',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    p.add_argument('--start-byte',type=int)
    p.add_argument('--max-bytes',type=int,default=8*1024*1024)
    a=p.parse_args();root=a.workspace.resolve(strict=True)
    try:
        src=a.uart_file.resolve(strict=True);dst=a.output.resolve()
        if not src.is_relative_to(root) or not dst.is_relative_to(root) or dst==src:raise ValueError()
        if not stat.S_ISREG(src.stat().st_mode) or not 1<=a.max_bytes<=128*1024*1024:raise ValueError()
        with src.open('rb') as f:
            info=os.fstat(f.fileno())
            if not stat.S_ISREG(info.st_mode):raise ValueError()
            size=info.st_size;start=max(0,size-a.max_bytes) if a.start_byte is None else a.start_byte
            if not 0<=start<=size:raise ValueError()
            report=observe(f,start,min(size,start+a.max_bytes))
        # 新規JSONだけ。パス/raw/hash/SSID/MAC/IP/鍵を出力へ含めない。
        with dst.open('x',encoding='utf-8') as f:
            os.chmod(dst,0o600);json.dump(report,f,indent=2);f.write('\n')
    except (OSError,ValueError):
        p.exit(2,'入力/出力条件を確認してください。既存regular file・root内の新規出力のみ許可します。\n')
    print('既存UARTの数値snapshotを保存。接続/認証/IP通信の成功判定はしていません。')

if __name__=='__main__':main()
