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
BRIGHTNESS_RUN33_SOURCE='8a79f2735c27cf783b4d1d703b155dd80178f9b3'
BRIGHTNESS_SCOPED_SOURCE='ccef15ae47ebae71577ba266bfa88bcae6476ca8'
BRIGHTNESS_SCOPED_LOG=r'C:\Windows\System32\MT8167EngineProbe\logs\brightness-native.log'
BRIGHTNESS_CLIENT_SOURCES=(BRIGHTNESS_SOURCE,BRIGHTNESS_RUN33_SOURCE,BRIGHTNESS_SCOPED_SOURCE)

def hexpin(value, n=64):
    if not isinstance(value,str) or not re.fullmatch('[0-9a-fA-F]{'+str(n)+'}', value):
        raise ValueError('完全なhex pinが必要です')
    return value.lower()

def load_map(path, root):
    path=path.resolve(strict=True)
    if not path.is_relative_to(root.resolve()):raise ValueError('package mapは作業ルート内が必要です')
    spec=json.loads(path.read_text(encoding='utf-8-sig'))
    if not {'schema','backup_manifest_sha256','packages','clients'}.issubset(spec) or set(spec)-{'schema','backup_manifest_sha256','packages','clients','gpu','sd_tuple','staging_profile'} or spec['schema'] != 1:
        raise ValueError('固定Engine policy map schema不一致')
    hexpin(spec['backup_manifest_sha256'])
    staging=spec.get('staging_profile','offline-all')
    if staging not in ('offline-all','normal-deferred'):raise ValueError('明示staging profile不一致')
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
        run33_display=(role=='display' and source==BRIGHTNESS_RUN33_SOURCE and version=='11/07/2026,0.0.1.5')
        usb1004=(role=='usb' and source=='c04c795caf5b722628174fda86608ab38049b93f' and version=='11/07/2026,2026.11.7.1004')
        if len(parts)!=4 or min(parts)<0 or max(parts)>65535 or parts<minimum or (date!='11/06/2026' and not run33_display and not usb1004):
            raise ValueError('固定target INF最低新版/date不一致: '+role)
        if role=='display' and source==BRIGHTNESS_RUN33_SOURCE and not run33_display:
            raise ValueError('8a79f27 displayは11/07/2026,0.0.1.5だけ許可します')
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
    if 'staging_profile' in spec:
        policy['staging_profile']=staging
        if 'gpu' not in spec:policy['retained_gpu']={'source_commit':'860f697b296558c30ee59d26d2a040dd5af87e0b','driver_version':'0.0.2.5','sys_sha256':'a8f226b33fa11915e88b3424c5ad3ed3aaa62ff35deb7f91902b04e7df4bf090','updated':False}
    if 'sd_tuple' in spec:
        from engine_sd_tuple import load_sd_tuple
        sd_files,sd_components,sd_cert=load_sd_tuple(spec['sd_tuple'],root)
        selected.update(sd_files);components.update(sd_components);certificates['sd_tuple']=sd_cert
        for role in ('parent','sd'):
            item=sd_components[role];date,numbers=item['driver_ver'].split(',');parts=tuple(map(int,numbers.split('.')))
            policy['packages'][role]={**item,'source_commit':item['wrapper_source_commit'],
                'version_u64':sum(v<<((3-i)*16) for i,v in enumerate(parts)),
                'date_filetime':int((datetime.datetime.strptime(date,'%m/%d/%Y')-datetime.datetime(1601,1,1)).total_seconds())*10000000}
        policy['sd_tuple_manifest_sha256']=spec['sd_tuple']['manifest_sha256'].lower()
    if 'gpu' in spec:
        from gpu_rollforward_package import load_package,validate_transition,FILES
        entry=spec['gpu']
        required={'baseline_directory','baseline_source_commit','baseline_manifest_sha256','candidate_directory',
                  'candidate_source_commit','candidate_manifest_sha256','policy_manifest','policy_manifest_sha256'}
        if set(entry)!=required:raise ValueError('GPU固定新旧package/policy pinsを全て明示してください')
        directories=[Path(entry[n+'_directory']).resolve(strict=True) for n in ('baseline','candidate')]
        if any(not p.is_relative_to(root.resolve()) for p in directories):raise ValueError('GPU inputはroot内が必要です')
        repo=Path(__file__).resolve().parents[2]
        baseline=load_package(repo,directories[0]);candidate=load_package(repo,directories[1],candidate=True)
        validate_transition(baseline,candidate)
        for kind,pkg in (('baseline',baseline),('candidate',candidate)):
            if pkg['source']!=hexpin(entry[kind+'_source_commit'],40) or pkg['signed_sha256']!=hexpin(entry[kind+'_manifest_sha256']):
                raise ValueError('GPU新旧署名manifest/source pin不一致')
        inf=(directories[1]/FILES[0]).read_text(encoding='utf-8-sig')
        if not re.search(r'(?im)^ClassGuid\s*=\s*\{4d36e97d-e325-11ce-bfc1-08002be10318\}\s*$',inf):
            raise ValueError('GPUは固定System classだけです')
        policy_path=Path(entry['policy_manifest']).resolve(strict=True)
        if not policy_path.is_relative_to(root.resolve()) or sha(policy_path)!=hexpin(entry['policy_manifest_sha256']):
            raise ValueError('GPU policy-manifest実file SHA pin不一致')
        gpu_policy=json.loads(policy_path.read_text(encoding='utf-8-sig'))
        transition=hashlib.sha256((candidate['source']+baseline['hashes'][FILES[1]]+candidate['hashes'][FILES[1]]+candidate['hashes'][FILES[0]]).encode('ascii')).hexdigest()
        expected_files={n:candidate['verified_hashes'][n] for n in ('provider-package-manifest.json',*FILES)}
        if (gpu_policy.get('schema_version')!=2 or gpu_policy.get('transition_id')!=transition
            or gpu_policy.get('candidate_source_commit')!=candidate['source']
            or gpu_policy.get('baseline_source_commit')!=baseline['source']
            or gpu_policy.get('baseline_driver_ver')!=baseline['driver_ver'] or gpu_policy.get('driver_ver')!=candidate['driver_ver']
            or gpu_policy.get('baseline_sys_sha256')!=baseline['hashes'][FILES[1]]
            or gpu_policy.get('baseline_signed_manifest_sha256')!=baseline['signed_sha256']
            or gpu_policy.get('source_signed_manifest_sha256')!=candidate['signed_sha256']
            or gpu_policy.get('files')!=expected_files):raise ValueError('標準GPU schema2 policy/new/old crosslink不一致')
        def gpu_component(pkg):
            return {'driver_ver':pkg['driver_ver'],'source_commit':pkg['source'],'signed_manifest_sha256':pkg['signed_sha256'],
                    'files':{'gpu/'+n:pkg['hashes'][n] for n in FILES}}
        component=gpu_component(candidate);component['baseline']=gpu_component(baseline)
        component['certificate_thumbprint']='BCBF6C456C6F0DD0818434763B189488FBF13570'
        component['policy_manifest_sha256']=sha(policy_path);component['transition_id']=transition
        components['gpu']=component
        date,numbers=candidate['driver_ver'].split(',')
        def encode_version(pkg):return sum(v << ((3-i)*16) for i,v in enumerate(pkg['parts']))
        policy['packages']['gpu']={**component,'version_u64':encode_version(candidate),
            'date_filetime':int((datetime.datetime.strptime(date,'%m/%d/%Y')-datetime.datetime(1601,1,1)).total_seconds())*10000000}
        base_date=baseline['driver_ver'].split(',')[0]
        policy['gpu_transition']={'candidate_source_commit':candidate['source'],'candidate_sys_sha256':candidate['hashes'][FILES[1]],
            'candidate_cat_sha256':candidate['hashes'][FILES[2]],'policy_sha256':sha(policy_path),'transition_id':transition,
            'baseline_version_u64':encode_version(baseline),
            'baseline_date_filetime':int((datetime.datetime.strptime(base_date,'%m/%d/%Y')-datetime.datetime(1601,1,1)).total_seconds())*10000000,
            'baseline_sys_sha256':baseline['hashes'][FILES[1]],'baseline_firmware_sha256':baseline['hashes'][FILES[3]]}
        for name in FILES:selected['gpu/'+name]=directories[1]/name
        selected['gpu-policy-manifest.json']=policy_path
        selected['gpu-development.cer']=directories[1]/'mt8167-development.cer'
        certificates['gpu']=(directories[1]/'mt8167-development.cer').read_bytes()
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
        brightness_source=entry['source_commit']
        if (sha(package.manifest)!=hexpin(entry['manifest_sha256']) or brightness_source not in BRIGHTNESS_CLIENT_SOURCES
            or package.record.get('source_commit')!=brightness_source or package.record.get('signed') is not True):
            raise ValueError('固定brightness署名wrapper/source pin不一致')
        cert=hexpin(entry['certificate_thumbprint'],40).upper()
        if package.record.get('signing_thumbprint')!=cert:raise ValueError('brightness署名cert metadata不一致')
        certpath,der=package.certificate(cert);image=package.file(BRIGHTNESS_CLIENT);metadata=package.json('manifest.json')
        if (metadata.get('commit')!=brightness_source or metadata.get('dirty') is not False
            or metadata.get('signed') is not True or metadata.get('arguments')!='none'
            or metadata.get('log_path')!=(BRIGHTNESS_SCOPED_LOG if brightness_source==BRIGHTNESS_SCOPED_SOURCE else r'C:\MT8167EngineProbe\brightness-native.log')
            or (brightness_source==BRIGHTNESS_SCOPED_SOURCE and (metadata.get('stdout_tee') is not True or metadata.get('stdout_max_bytes')!=16384 or metadata.get('legacy_root_log_modified') is not False or metadata.get('log_generation')!='Engine-state-archive'))
            or metadata.get('sys_or_cat_modified') is not False or metadata.get('machine')!='ARMNT'
            or metadata.get('exe_sha256','').lower()!=sha(image)):
            raise ValueError('固定brightness client build/log/no-argument契約不一致')
        selected['clients/'+BRIGHTNESS_CLIENT]=image;selected['brightness-client-development.cer']=certpath
        certificates['brightness_client']=der;policy['clients'][BRIGHTNESS_CLIENT]=sha(image)
        binding={'source_commit':brightness_source,'signed_manifest_sha256':sha(package.manifest),
                 'certificate_thumbprint':cert,'exe_sha256':sha(image),'log_path':metadata['log_path']}
        policy['brightness_client']=binding;components['brightness_client']=binding
    return selected,components,certificates,policy

def cbytes(digest):return '{'+','.join('0x'+digest[i:i+2] for i in range(0,64,2))+'}'

def generate(policy):
    if not 5<=len(policy['packages'])<=7:raise ValueError('固定target数は5..7だけです')
    lines=['/* Generated fixed-target policy; compile into the signed Engine service. */',
           '#define DEFERRED_TARGET_COUNT '+str(len(policy['packages'])), '#define DEFERRED_SD_UPDATE '+('1' if 'sd' in policy['packages'] else '0'), '#define DEFERRED_GPU_UPDATE '+('1' if 'gpu' in policy['packages'] else '0'),
           'static const struct deferred_pin deferred_pins[DEFERRED_TARGET_COUNT]={']
    for role in (*TARGETS,'gpu','sd'):
        if role not in policy['packages']:continue
        p=policy['packages'][role]
        names=list(p['files'])
        lines.append('{'+str(p['version_u64'])+'ULL,'+str(p['date_filetime'])+'ULL,{'+','.join(cbytes(p['files'][n]) for n in names)+'}},')
    lines+=['};']
    if 'gpu_transition' in policy:
        g=policy['gpu_transition'];lines+=['static const struct deferred_pin deferred_gpu_baseline={'+str(g['baseline_version_u64'])+'ULL,'+str(g['baseline_date_filetime'])+'ULL,{{0},'+cbytes(g['baseline_sys_sha256'])+',{0},'+cbytes(g['baseline_firmware_sha256'])+'}};']
    lines+=['static const char deferred_token[]="'+hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const BYTE deferred_client_hashes['+str(len(policy['clients']))+'][32]={'+','.join(cbytes(policy['clients'][n]) for n in (*CLIENTS,BRIGHTNESS_CLIENT) if n in policy['clients'])+'};']
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
