#!/usr/bin/env python3
"""固定5packageの実署名manifestからEngine延期installのcompile-time policyを作る。"""
import argparse
import datetime
import hashlib
import json
import re
from pathlib import Path
from engine_io_package import SignedPackage, check_inf
from private_hardware_probe_input import sha

# Target paths/IDs are source constants, never supplied by a package or command.
TARGETS = {
    'parent': ('ACPI\\BEN0001', 'shared-parent', 'mt8167-shared', 'MT8167Shared', (2026,11,7,1006)),
    'audio': ('MT8167\\AFE', 'mt8167-afe-engine-pcm', 'mt8167-audio', 'Mt8167Audio', (2026,11,7,1004)),
    'usb': ('MT8167\\USB20', 'mt8167-usb20', 'mt8167-usb20', 'A04Usb20Keyboard', (2026,11,6,1003)),
    'wifi': ('A04BR3\\MT6630SDIO', 'mt6630-runtime', 'mt6630-runtime', 'MT6630Runtime', (2026,11,6,1003)),
    'display': ('ACPI\\MOU0007', 'mt8167-display', 'mt8167-display', 'MT8167Display', (0,0,1,4)),
}
CLIENTS = ('mt6630-wlan-probe.exe', 'mt8167-audio-engine-probe.exe')
BRIGHTNESS_CLIENT='mt8167-brightness-native.exe'
BRIGHTNESS_SOURCE='4fafe9a76271531c36a6071b3926e3ce2dd77a8e'

def hexpin(value, n=64):
    if not isinstance(value,str) or not re.fullmatch('[0-9a-fA-F]{'+str(n)+'}', value):
        raise ValueError('完全なhex pinが必要です')
    return value.lower()

def load_map(path, root):
    path=path.resolve(strict=True)
    if not path.is_relative_to(root.resolve()):raise ValueError('package mapは作業ルート内が必要です')
    spec=json.loads(path.read_text(encoding='utf-8-sig'))
    if set(spec) != {'schema','backup_manifest_sha256','packages','clients'} or spec['schema'] != 1:
        raise ValueError('固定Engine policy map schema不一致')
    hexpin(spec['backup_manifest_sha256'])
    if set(spec['packages']) != set(TARGETS) or set(spec['clients']) not in (set(CLIENTS),set((*CLIENTS,BRIGHTNESS_CLIENT))):
        raise ValueError('固定5targetと固定2clientをすべて明示してください')
    selected={};components={};certificates={};policy={'schema':1,'mode':'engine_fixed_deferred_v1',
        'backup_manifest_sha256':spec['backup_manifest_sha256'].lower(),'packages':{},'clients':{}}
    for role,(hwid,inf,sys,service,minimum) in TARGETS.items():
        entry=spec['packages'][role]
        if set(entry) != {'directory','manifest_sha256','source_commit','certificate_thumbprint','driver_ver'}:
            raise ValueError('package入力fields不一致: '+role)
        package=SignedPackage(Path(entry['directory']),root)
        if sha(package.manifest) != hexpin(entry['manifest_sha256']):raise ValueError('署名manifest pin不一致')
        source=hexpin(entry['source_commit'],40)
        if package.record.get('source_commit',package.record.get('inf_source_commit')) != source or package.record.get('signed') is False or package.record.get('source_dirty') is True:
            raise ValueError('署名package source不一致')
        cert=hexpin(entry['certificate_thumbprint'],40).upper()
        certpath,der=package.certificate(cert);certificates[role]=der
        if role=='parent':
            capnames=[n for n in ('parent-manifest.json','runtime-parent-manifest.json','parent/parent-manifest.json') if n in package.files]
            if len(capnames)!=1:raise ValueError('hash済み単一parent capability manifestが必要です')
            capabilities=package.json(capnames[0]);flags=capabilities.get('flags',{})
            if (capabilities.get('source_dirty') is not False or flags.get('A04_SHARED_USB_ONLY')!='0'
                or any(flags.get(n)!='1' for n in ('A04_SHARED_ENGINE_PCM','A04_SHARED_WIFI_RUNTIME','A04_SHARED_TOUCH_D0_EXPERIMENT','A04_SHARED_BUTTON_D0_EXPERIMENT','A04_AUDIO_CHILD_RESOURCES','A04_USB_GPIO22_SOURCE_EXPERIMENT'))):
                raise ValueError('既存touch/buttons/Engine/USB/WiFi保持parent profileが必要です')
        version=entry['driver_ver'];date,numbers=version.split(',');parts=tuple(map(int,numbers.split('.')))
        if len(parts)!=4 or min(parts)<0 or max(parts)>65535 or parts<minimum or date!='11/06/2026':
            raise ValueError('固定target INF最低新版/date不一致: '+role)
        names=(inf+'.inf',sys+'.sys',inf+'.cat')
        # Flat and component-folder signed outputs only; no caller-defined target path.
        chosen=[]
        for name in names:
            matches=[n for n in (name,role+'/'+name,'package/'+name) if n in package.files]
            if len(matches)!=1:raise ValueError('固定package artifactの欠落/重複: '+name)
            chosen.append(package.file(matches[0]))
        check_inf(chosen[0],version)
        text=chosen[0].read_text()
        if (hwid.casefold() not in text.casefold() or
            not re.search(r'(?im)^AddService\s*=\s*'+re.escape(service)+r'\s*,',text) or
            re.search(r'(?im)(CoInstallers32|RegisterDlls|RunPreSetupCommands|RunPostSetupCommands)',text)):
            raise ValueError('固定INF hardware/service契約不一致: '+role)
        includes=re.findall(r'(?im)^(Include|Needs)\s*=\s*([^\r\n]+)',text)
        expected=[('Include','ks.inf,wdmaudio.inf'),('Needs','KS.Registration,WDMAUDIO.Registration')] if role=='audio' else []
        if includes!=expected:raise ValueError('固定INF Include/Needs不一致: '+role)
        files={role+'/'+name:sha(p) for name,p in zip(names,chosen)}
        selected.update({role+'/'+name:p for name,p in zip(names,chosen)})
        selected['deferred-'+role+'-development.cer']=certpath
        components[role]={'wrapper_source_commit':source,
            'binary_source_commit':package.record.get('binary_source_commit',package.record.get('driver_source_commit',source)),
            'inf_source_commit':package.record.get('inf_source_commit',package.record.get('template_commit',source)),'driver_ver':version,'certificate_thumbprint':cert,
            'signed_manifest_sha256':sha(package.manifest),'files':files}
        policy['packages'][role]={**components[role],'source_commit':source,
            'version_u64':sum(v << ((3-i)*16) for i,v in enumerate(parts)),
            'date_filetime':int((datetime.datetime.strptime(date,'%m/%d/%Y')-datetime.datetime(1601,1,1)).total_seconds())*10000000}
    for name in CLIENTS:
        entry=spec['clients'][name]
        if set(entry)!={'path','sha256'}:raise ValueError('固定client入力fields不一致')
        p=Path(entry['path']).resolve(strict=True)
        if p.name!=name or not p.is_relative_to(root.resolve()) or sha(p)!=hexpin(entry['sha256']):
            raise ValueError('固定client path/hash不一致')
        selected['clients/'+name]=p;policy['clients'][name]=sha(p)
    if BRIGHTNESS_CLIENT in spec['clients']:
        entry=spec['clients'][BRIGHTNESS_CLIENT]
        if set(entry)!={'directory','manifest_sha256','source_commit','certificate_thumbprint'}:
            raise ValueError('固定brightness署名package fields不一致')
        package=SignedPackage(Path(entry['directory']),root)
        if (sha(package.manifest)!=hexpin(entry['manifest_sha256']) or entry['source_commit']!=BRIGHTNESS_SOURCE
            or package.record.get('source_commit')!=BRIGHTNESS_SOURCE or package.record.get('signed') is not True):
            raise ValueError('4fafe9a brightness署名wrapper/source pin不一致')
        cert=hexpin(entry['certificate_thumbprint'],40).upper()
        if package.record.get('signing_thumbprint')!=cert:raise ValueError('brightness署名cert metadata不一致')
        certpath,der=package.certificate(cert);image=package.file(BRIGHTNESS_CLIENT);metadata=package.json('manifest.json')
        if (metadata.get('commit')!=BRIGHTNESS_SOURCE or metadata.get('dirty') is not False
            or metadata.get('signed') is not True or metadata.get('arguments')!='none'
            or metadata.get('log_path')!=r'C:\MT8167EngineProbe\brightness-native.log'
            or metadata.get('sys_or_cat_modified') is not False or metadata.get('machine')!='ARMNT'
            or metadata.get('exe_sha256','').lower()!=sha(image)):
            raise ValueError('固定brightness client build/log/no-argument契約不一致')
        selected['clients/'+BRIGHTNESS_CLIENT]=image;selected['brightness-client-development.cer']=certpath
        certificates['brightness_client']=der;policy['clients'][BRIGHTNESS_CLIENT]=sha(image)
        binding={'source_commit':BRIGHTNESS_SOURCE,'signed_manifest_sha256':sha(package.manifest),
                 'certificate_thumbprint':cert,'exe_sha256':sha(image),'log_path':metadata['log_path']}
        policy['brightness_client']=binding;components['brightness_client']=binding
    return selected,components,certificates,policy

def cbytes(digest):return '{'+','.join('0x'+digest[i:i+2] for i in range(0,64,2))+'}'

def generate(policy):
    lines=['/* Generated fixed-target policy; compile into the signed Engine service. */',
           'static const struct deferred_pin deferred_pins[5]={']
    for role in TARGETS:
        p=policy['packages'][role]
        names=list(p['files'])
        lines.append('{'+str(p['version_u64'])+'ULL,'+str(p['date_filetime'])+'ULL,{'+','.join(cbytes(p['files'][n]) for n in names)+'}},')
    lines+=['};','static const char deferred_token[]="'+hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const BYTE deferred_client_hashes['+str(len(policy['clients']))+'][32]={'+','.join(cbytes(policy['clients'][n]) for n in (*CLIENTS,BRIGHTNESS_CLIENT) if n in policy['clients'])+'};']
    return '\n'.join(lines)+'\n'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--package-map',type=Path,required=True)
    ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();root=a.workspace.resolve(strict=True);out=a.output.resolve()
    if not out.is_relative_to(root) or out.exists():raise ValueError('新規root内出力が必要です')
    _,_,_,policy=load_map(a.package_map,root)
    out.mkdir(parents=True)
    (out/'engine-deferred-policy.h').write_text(generate(policy))
    (out/'deferred-policy-manifest.json').write_text(json.dumps(policy,indent=2)+'\n')
if __name__=='__main__':main()
