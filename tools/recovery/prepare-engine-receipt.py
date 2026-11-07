#!/usr/bin/env python3
"""Prepare hash-pinned fixed receipt inputs locally. No adb/SSH/device operation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

NAMES=('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--backup',type=Path,required=True);ap.add_argument('--backup-sha256',required=True)
    ap.add_argument('--policy-hash',required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--profile',choices=('run47-sdon','legacy-five','legacy-gpu-six'),default='run47-sdon')
    a=ap.parse_args();repo=Path(__file__).resolve().parents[2];root=repo.parents[1];backup=a.backup.resolve(strict=True);out=a.out.resolve()
    if not (root/'windows-rt/AGENTS.md').is_file() or not backup.is_relative_to(root) or not out.is_relative_to(root) or out.exists():raise ValueError('Fresh private output and backup inside workspace required')
    if not re.fullmatch('[0-9a-f]{64}',a.policy_hash) or not re.fullmatch('[0-9a-f]{64}',a.backup_sha256) or sha(backup)!=a.backup_sha256:raise ValueError('Explicit backup/policy SHA256 required')
    b=json.loads(backup.read_text());entries={x['source_path']:x for x in b['files']}
    if b.get('status')!='captured-unmounted' or b.get('errors') or b.get('unmounted') is not True or b.get('read_only') is not True:raise ValueError('Completed read-only unmounted capture required')
    if subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True).strip():raise ValueError('Use a clean fixed source checkout')
    out.mkdir(mode=0o700);pins={}
    with zipfile.ZipFile(out/'private-input.zip','w',zipfile.ZIP_DEFLATED) as z:
        for name in NAMES:
            e=entries['Windows/System32/config/'+name];p=(backup.parent/e['backup_path']).resolve(strict=True)
            if not p.is_relative_to(backup.parent.resolve()) or sha(p)!=e['sha256'] or p.stat().st_size!=e['bytes']:raise ValueError('Private input file path/hash/size mismatch')
            pins[name]=e['sha256'];z.write(p,'input/'+name)
    commit=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
    task='mt8167-fixed-receipt-'+a.backup_sha256[:16]+'-'+a.policy_hash[:8]
    base='C:\\'+task
    params={'SourceDirectory':base+'\\inputs\\input','OutputDirectory':base+'\\result',
            'ExpectedSystemSha256':pins['SYSTEM'],'ExpectedLog1Sha256':pins['SYSTEM.LOG1'],'ExpectedLog2Sha256':pins['SYSTEM.LOG2'],
            'ExpectedPolicyHash':a.policy_hash,'BackupManifestSha256':a.backup_sha256,'Profile':a.profile}
    (out/'native-parameters.json').write_text(json.dumps(params,indent=2)+'\n')
    tools=('read-fixed-engine-receipt.ps1','Read-EngineLastDiagnostic.ps1','OfflineSystemHiveNative.cs','decode-engine-receipt.py')
    provenance={'schema':1,'source_commit':commit,'remote_base':base,'backup_manifest_sha256':a.backup_sha256,'policy_hash':a.policy_hash,'recovery_boot_id':b['recovery_boot_id'],'source_files':pins,
        'input_zip_sha256':sha(out/'private-input.zip'),'native_parameters_sha256':sha(out/'native-parameters.json'),
        'tool_sha256':{n:sha(Path(__file__).parent/n) for n in tools},'wifi_decoder_source_commit':'bafafa9ea6d94fe6357616a3edfff66e568bfc7c',
        'wifi_decoder_sha256':sha(repo/'tools/winpe/decode-mt6630-checkpoint.py'),'private_do_not_distribute':True,'device_access':False}
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    for p in out.iterdir():p.chmod(0o600)
    print(json.dumps({'status':'private-native-receipt-inputs-prepared','provenance_sha256':sha(out/'provenance.json')}))
if __name__=='__main__':main()
