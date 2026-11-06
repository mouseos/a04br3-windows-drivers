#!/usr/bin/env python3
"""Produce pinned trust inputs from checked candidate and WDK catalog proof.

Cryptographic trust in a test certificate is not target RT CI acceptance.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import ssl
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT.parents[1]
spec=importlib.util.spec_from_file_location('candidate_builder',ROOT/'tools/winpe/prepare-update-candidates.py')
candidate_builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(candidate_builder)
PAIRS = {'toprgu': ('mt8167-toprgu.sys','mt8167-toprgu.inf','mt8167-toprgu.cat'),
         'parent': ('mt8167-shared.sys','shared-parent.inf','shared-parent.cat'),
         'touch': ('bu21025.sys','bu21025.inf','bu21025.cat')}

def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
    p=argparse.ArgumentParser();p.add_argument('--candidate',type=Path,required=True);p.add_argument('--osslsigncode',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    src=a.candidate.resolve(strict=True);verify=a.osslsigncode.resolve(strict=True);out=a.output.resolve()
    if WORKSPACE not in src.parents or WORKSPACE not in verify.parents or WORKSPACE not in out.parents or not src.is_dir() or not verify.is_file() or out.exists():raise ValueError('input/output rejected')
    manifest=json.loads((src/'manifest.json').read_text());proof=json.loads((src/'catalog-proof.json').read_text(encoding='utf-8-sig'))
    if manifest.get('source_wim_sha256')!=candidate_builder.WIM_HASH or manifest.get('top_zip_sha256')!=candidate_builder.TOP_ZIP_HASH or manifest.get('files')!=candidate_builder.PINS:raise ValueError('unrecognized candidate identity')
    expected={(d,n,c) for d,(sys,inf,c) in PAIRS.items() for n in (sys,inf)}
    obs=proof.get('observations',[])
    if proof.get('version')!=1 or len(obs)!=6 or {(x.get('package'),x.get('name'),x.get('catalog')) for x in obs}!=expected:raise ValueError('catalog observations incomplete')
    for x in obs:
        if not ((x.get('exit_code')==0 and x.get('untrusted_root') is False) or (x.get('exit_code')==1 and x.get('untrusted_root') is True)):raise ValueError('catalog result rejected')
    out.mkdir(parents=True)
    for folder,(sys,inf,cat) in PAIRS.items():
        for name in (sys,inf,cat):
            key=folder+'/'+name; f=(src/key).resolve(strict=True)
            if src not in f.parents or not f.is_file() or digest(f)!=manifest['files'].get(key) or digest(f)!=proof['hashes'].get(key):raise ValueError('candidate/proof mismatch: '+key)
        der=(src/folder/'signer.cer').read_bytes(); signer=manifest['signers'][folder]
        if hashlib.sha256(der).hexdigest()!=signer['sha256'] or hashlib.sha1(der).hexdigest().upper()!=signer['sha1']:raise ValueError('signer mismatch')
        label='shared' if folder=='parent' else folder
        cert=out/(label+'.cer');cert.write_bytes(der);(out/(label+'-cert.sha256')).write_bytes(hashlib.sha256(der).digest())
        pem=out/(label+'.pem');pem.write_text(ssl.DER_cert_to_PEM_cert(der))
        for name in (sys,cat):subprocess.run([str(verify),'verify','-CAfile',str(pem),'-in',str(src/folder/name)],check=True,stdout=subprocess.DEVNULL)
        (out/(label+'-sys.sha256')).write_bytes(bytes.fromhex(manifest['files'][folder+'/'+sys]))
    result={'version':1,'candidate_manifest_sha256':digest(src/'manifest.json'),'catalog_proof_sha256':digest(src/'catalog-proof.json'),
            'files':{f.name:digest(f) for f in out.iterdir() if f.is_file()},'signers':manifest['signers'],
            'status':'three signer pins; catalog membership observed; target RT policy, PnP and rollback unverified'}
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
