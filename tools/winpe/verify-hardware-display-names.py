#!/usr/bin/env python3
"""Verify device display strings in the exact staged INF inputs, read-only."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT.parents[1]
BOOT_INF_SHA256 = '9bbf73d73c6dcad9f392e294a2cee7ca23209f80e4d36801e6b6a5781acb7192'
EXPECTED = {
    'toprgu/mt8167-toprgu.inf': ('device', 'MediaTek MT8167 TOPRGU Watchdog'),
    'parent/shared-parent.inf': ('device', 'MediaTek MT8167 Shared Resource Controller'),
    'touch/bu21025.inf': ('device', 'ROHM BU21025 Touchscreen'),
}

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()

def inf_strings(path):
    data=path.read_bytes()
    text=data.decode('utf-16') if data.startswith((b'\xff\xfe',b'\xfe\xff')) else data.decode('utf-8-sig')
    sections=[];current=None
    for line in text.splitlines():
        stripped=line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            current=stripped[1:-1].strip().lower()
            if current=='strings':sections.append([])
        elif current=='strings':sections[-1].append(line)
    if len(sections)!=1:raise ValueError('INF Strings section is missing or ambiguous')
    parser=configparser.ConfigParser(interpolation=None,strict=True)
    parser.read_string('[Strings]\n'+'\n'.join(sections[0]))
    return parser['Strings']

def check(path, key, expected):
    value=inf_strings(path).get(key, '').strip()
    if len(value)<2 or value[0]!='"' or value[-1]!='"':raise ValueError('unquoted device description')
    value=value[1:-1]
    if value!=expected or 'a04' in value.lower() or 'experimental' in value.lower():raise ValueError('unexpected hardware display name: '+str(path))
    return value

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--candidate',type=Path,required=True);ap.add_argument('--msdc-inf',type=Path,required=True);a=ap.parse_args()
    candidate=a.candidate.resolve(strict=True);boot=a.msdc_inf.resolve(strict=True)
    if WORKSPACE not in candidate.parents or WORKSPACE not in boot.parents or not candidate.is_dir() or not boot.is_file():raise ValueError('input path rejected')
    manifest=json.loads((candidate/'manifest.json').read_text());result={}
    for name,(key,expected) in EXPECTED.items():
        path=(candidate/name).resolve(strict=True)
        if candidate not in path.parents or digest(path)!=manifest.get('files',{}).get(name):raise ValueError('candidate INF mismatch: '+name)
        result[name]=check(path,key,expected)
    if digest(boot)!=BOOT_INF_SHA256:raise ValueError('MSDC INF mismatch')
    result['msdc']=check(boot,'devicedescription','MediaTek MT8167 MSDC eMMC Host Controller')
    print(json.dumps({'device_display_names':result,'scope':'INF display strings only; device enumeration and installed PnP selection unverified'},indent=2))
if __name__=='__main__':main()
