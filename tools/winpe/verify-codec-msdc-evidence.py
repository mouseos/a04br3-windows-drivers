#!/usr/bin/env python3
"""既存の主担当回収ファイルだけで照合する。デバイス・UARTポートは開かない。"""
import argparse,hashlib,json,re
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--media',type=Path,required=True);ap.add_argument('--backup',type=Path,required=True);ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
 ap.add_argument('--uart-file',type=Path);ap.add_argument('--uart-start',type=int);ap.add_argument('--uart-end',type=int)
 a=ap.parse_args();root=a.workspace.resolve(strict=True)
 for p in [a.media,a.backup,*([a.uart_file] if a.uart_file else [])]:
  if not p.resolve(strict=True).is_relative_to(root):raise ValueError('root内既存証拠だけ使用します')
 out=a.output.resolve()
 if not out.is_relative_to(root) or out.exists():raise ValueError('root内新規出力が必要です')
 m=json.loads(a.media.read_text());b=json.loads(a.backup.read_text());policy=m['probe']['deferred_update']
 if policy.get('transaction')!='codec1026-msdc-quiet-fresh84' or b.get('status')!='captured-unmounted' or b['recovery_boot_id']==policy['recovery_boot_id']:raise ValueError('異なるbootで回収した適用後backupが必要です')
 records={}
 for f in b['files']:
  p=(a.backup.parent/f['backup_path']).resolve(strict=True)
  if not p.is_relative_to(a.backup.parent.resolve()) or p.stat().st_size!=f['bytes'] or sha(p)!=f['sha256'].lower():raise ValueError('回収file hash不一致')
  records[f['source_path'].replace('\\','/').lower()]=(p,f)
 expected={**{n.split('/')[-1]:h for p in policy['packages'].values() for n,h in p['files'].items() if n.endswith('.sys')},**policy['retained_files']}
 hashes={}
 for name,digest in expected.items():
  path=('windows/system32/' if name=='ge8300-umd.dll' else 'windows/system32/drivers/')+name
  item=records.get(path);hashes[path]={'observed':bool(item),'match':bool(item and item[1]['sha256'].lower()==digest)}
 def text(name):
  entry=records.get(name.lower());return entry[0].read_text(errors='replace') if entry else ''
 capture=text('Windows/System32/MT8167CaptureProbe/capture.log')
 if not capture:capture=text('Windows/System32/MT8167EngineProbe/logs/capture.stdout.txt')
 passes=[tuple(int(x,16) for x in row) for row in re.findall(r'MT8167CAP pass-dma-irq a=([0-9A-F]{8}) b=([0-9A-F]{8}) c=([0-9A-F]{8})',capture)]
 aggregate=re.findall(r'MT8167CAP pcm-aggregate a=([0-9A-F]{8}) b=([0-9A-F]{8}) c=([0-9A-F]{8})',capture)
 capture_gate=('MT8167CAP end a=00000000 b=00000000' in capture and len(passes)==2 and {x[0] for x in passes}=={0,1} and all(x[1]>0 and x[2]>0 for x in passes) and len(aggregate)==1 and int(aggregate[0][2],16)>0)
 service=text('Windows/System32/MT8167EngineProbe/logs/service.log')
 io_gate=bool(re.search(r'ID=0000000C EVT=00000123 CODE=00000000',service))
 uart={'observed':False,'error_path_exercised':False,'speed_improvement_measured':False}
 if a.uart_file:
  if a.uart_start is None or a.uart_end is None or not 0<=a.uart_start<a.uart_end<=a.uart_file.stat().st_size:raise ValueError('主担当が分離したbootのUART byte範囲が必要です')
  with a.uart_file.open('rb') as f:f.seek(a.uart_start);data=f.read(a.uart_end-a.uart_start)
  uart.update(observed=True,range_start=a.uart_start,range_end=a.uart_end,range_sha256=hashlib.sha256(data).hexdigest(),normal_tags={s:data.count(s.encode()) for s in ('A04WIN StartIo','A04WIN blocks','A04WIN CDB')},dma_poll_tags=data.count(b'A04WIN dma poll'),note='文字混線による欠落があり得る。タグ不存在だけで抑制・エラー保持成功とは判定しない。')
 result={'media_manifest_sha256':sha(a.media),'after_backup_manifest_sha256':sha(a.backup),'installed_files':hashes,'all_observed_sys_match':all(v['match'] for k,v in hashes.items() if k.endswith('.sys')),'all_retained_files_observed_and_match':all(v['match'] for v in hashes.values()),'codec_ks_dma_irq_aggregate_gate':bool(capture_gate),'acoustic_quality_verified':False,'raw_pcm_persisted':False,'ordinary_file_reads_completed':io_gate,'physical_emmc_throughput_measured':False,'error_emission_verified_on_hardware':False,'uart':uart,'reboot_count_must_be_confirmed_by_primary_boot_windows':True}
 out.write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
