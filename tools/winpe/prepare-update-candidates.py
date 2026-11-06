#!/usr/bin/env python3
"""Extract and pin the exact WIM-tested parent/touch and TOPRGU packages.

This creates local candidate inputs only; it does not authorize installation.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT.parents[1]
name_spec = importlib.util.spec_from_file_location('hardware_names', ROOT/'tools/winpe/verify-hardware-display-names.py')
hardware_names = importlib.util.module_from_spec(name_spec); name_spec.loader.exec_module(hardware_names)
WIM_HASH = '66b1db86efc8f4451ca487bd4837d249423edd4e87f5e93255599311d6674752'
TOP_ZIP_HASH = 'b8ff87573ef02e0c39767aba062dcd3e00ae8613445999b259576bf472d00571'
PINS = {
    'parent/mt8167-shared.sys': '2d70526cd7b8d4ab8c09cb8a5a6e4299d7546c2d8537416cdefd6aea873a9cdb',
    'parent/shared-parent.inf': '15e667621c99aefa46c830db176215892762919d737b7e0f4f4d6ae5615715b7',
    'parent/shared-parent.cat': 'eb38fda894f5d37057b6f91f83496be814b26e5ea00686f7aa1393d3da4f7533',
    'touch/bu21025.sys': '049f08ba2b7ea82943f97c22da118dfa466d98732fe75d309c1ac6f429bd6db3',
    'touch/bu21025.inf': 'ca780add5a40d4fa0e04914e83b510b7c5cadbc0bac378be43a87d3f71bebbcf',
    'touch/bu21025.cat': '072915730b1c83acd4a82b2cbc50ae447908b12dd02c53dd0936d5fb7684c640',
    'toprgu/mt8167-toprgu.sys': 'e2e2ca36ddb51ddbb80d34fbb5c522dd357d25e1f778839d0e347463f049751d',
    'toprgu/mt8167-toprgu.inf': 'd53f1c077777343d95d2c2214dfb543acf7ab106844a4f938cf65e11ad0c4ebc',
    'toprgu/mt8167-toprgu.cat': '596743bc2fa18ebd101e0de460a919bcb883b2a83efd2691fccc2a7f60e5e969',
    'toprgu/mt8167-toprgu.cer': 'f5b4961d1cfe9aba7a2af00e32e7b1a8cb9f97c7d60c967ce21c0b65b48e2407',
}

def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def main():
    ap = argparse.ArgumentParser()
    for name in ('wim', 'top-zip', 'wimlib', 'osslsigncode', 'output'):
        ap.add_argument('--'+name, type=Path, required=True)
    a = ap.parse_args()
    wim, topzip, wimlib, verify = (x.resolve(strict=True) for x in (a.wim, a.top_zip, a.wimlib, a.osslsigncode))
    out = a.output.resolve()
    if any(WORKSPACE not in x.parents or not x.is_file() for x in (wim, topzip, wimlib, verify)) or WORKSPACE not in out.parents or out.exists():
        raise ValueError('input/output path rejected')
    if digest(wim) != WIM_HASH or digest(topzip) != TOP_ZIP_HASH:
        raise ValueError('source identity mismatch')
    out.mkdir(parents=True)
    scratch = out/'scratch'; scratch.mkdir()
    env = dict(os.environ, TMPDIR=str(scratch))
    subprocess.run([str(wimlib), 'extract', str(wim), '1', '/a04br3/mt8167-shared', '/a04br3/bu21025-shared', '/Windows/System32/Drivers/mt8167-toprgu.sys', '/a04br3/a04-msdc-probe.inf', '--dest-dir='+str(scratch), '--no-acls'], env=env, check=True)
    for folder in ('parent','touch','toprgu'):
        (out/folder).mkdir()
    for name in ('mt8167-shared.sys','shared-parent.inf','shared-parent.cat'):
        shutil.copyfile(scratch/'mt8167-shared'/name,out/'parent'/name)
    for name in ('bu21025.sys','bu21025.inf','bu21025.cat'):
        shutil.copyfile(scratch/'bu21025-shared'/name,out/'touch'/name)
    with zipfile.ZipFile(topzip) as z:
        for name in ('mt8167-toprgu.sys','mt8167-toprgu.inf','mt8167-toprgu.cat','mt8167-toprgu.cer'):
            with z.open(name) as src, (out/'toprgu'/name).open('xb') as dst:
                shutil.copyfileobj(src,dst)
    for name, expected in PINS.items():
        if digest(out/name) != expected: raise ValueError('candidate hash mismatch: '+name)
    if digest(scratch/'mt8167-toprgu.sys') != PINS['toprgu/mt8167-toprgu.sys']:
        raise ValueError('TOPRGU WIM and package differ')
    descriptions = {}
    for name,(key,expected) in hardware_names.EXPECTED.items():
        descriptions[name]=hardware_names.check(out/name,key,expected)
    msdc=scratch/'a04-msdc-probe.inf'
    if digest(msdc)!=hardware_names.BOOT_INF_SHA256:raise ValueError('MSDC WIM INF differs')
    descriptions['msdc']=hardware_names.check(msdc,'devicedescription','MediaTek MT8167 MSDC eMMC Host Controller')
    signers = {}
    for folder, sysfile, catfile in (('toprgu','mt8167-toprgu.sys','mt8167-toprgu.cat'),('parent','mt8167-shared.sys','shared-parent.cat'),('touch','bu21025.sys','bu21025.cat')):
        d=out/folder; signature=scratch/(folder+'.p7'); pem=d/'signer.pem'; der=d/'signer.cer'
        subprocess.run([str(verify),'extract-signature','-in',str(d/sysfile),'-out',str(signature)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['openssl','pkcs7','-inform','DER','-in',str(signature),'-print_certs','-out',str(pem)],check=True)
        subprocess.run(['openssl','x509','-in',str(pem),'-outform','DER','-out',str(der)],check=True)
        if folder=='toprgu' and der.read_bytes()!=(d/'mt8167-toprgu.cer').read_bytes():raise ValueError('TOPRGU certificate differs')
        for name in (sysfile,catfile):
            subprocess.run([str(verify),'verify','-CAfile',str(pem),'-in',str(d/name)],check=True,stdout=subprocess.DEVNULL)
        signers[folder]={'sha256':digest(der),'sha1':hashlib.sha1(der.read_bytes()).hexdigest().upper()}
    result={'source_wim_sha256':WIM_HASH,'top_zip_sha256':TOP_ZIP_HASH,'files':PINS,'signers':signers,'device_display_names':descriptions,
            'status':'candidate hashes and signatures verified; catalog membership, offline binding, transaction recovery and target RT acceptance still require verification'}
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
