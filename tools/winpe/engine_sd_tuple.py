"""Only Nash's signed SD3/parent1010 tuple; no driver rebuild or path broker."""
from pathlib import Path
import hashlib
import re
from engine_io_package import SignedPackage, check_inf
from private_hardware_probe_input import sha
SOURCE='9e07f0a6cdf20f6efe9a4a75ed5f42fcd6ee583c'
BASELINE='ecaa7b70248230dee2977e1d9057260f400287be'
VERSIONS={'parent':'11/07/2026,2026.11.7.1010','sd':'11/07/2026,2026.11.7.3'}
FILES={'parent':('shared-parent.inf','mt8167-shared.sys','shared-parent.cat'),
       'sd':('mt8167-sd.inf','mt8167-sd.sys','mt8167-sd.cat')}

def load_sd_tuple(entry,root):
    if set(entry)!={'directory','manifest_sha256','source_commit','certificate_thumbprint'} or entry['source_commit']!=SOURCE:
        raise ValueError('SD3/parent1010固定tuple/source入力不一致')
    package=SignedPackage(Path(entry['directory']),root,'tuple-manifest.json');r=package.record
    if sha(package.manifest)!=entry['manifest_sha256'].lower() or not re.fullmatch('[0-9a-fA-F]{64}',entry['manifest_sha256']):raise ValueError('SD tuple manifest hash不一致')
    if (r.get('source_commit')!=SOURCE or r.get('source_dirty') is not False or r.get('signed') is not True
        or r.get('signing_thumbprint')!=entry['certificate_thumbprint'].upper()
        or r.get('parent_driver_ver')!=VERSIONS['parent'] or r.get('sd_driver_ver')!=VERSIONS['sd']
        or r.get('sd_power_enabled') is not True or r.get('thermal_enabled') is not False
        or r.get('audio_parent_dma_source')!=BASELINE or r.get('audio_rebuilt') is not False
        or r.get('wifi_rebuilt') is not False or r.get('emmc_modified') is not False):
        raise ValueError('SD3 matched tuple契約不一致')
    certpath,der=package.certificate(entry['certificate_thumbprint'].upper())
    baseline=package.json('baseline-parent1008-manifest.json');parent=package.json('parent/parent-manifest.json')
    expected=dict(baseline['flags']);expected['A04_SHARED_SD_POWER']='1'
    if baseline.get('source_commit')!=BASELINE or parent.get('source_commit')!=SOURCE or parent.get('flags')!=expected or r.get('parent_flags')!=expected:
        raise ValueError('SD親はecaa1008全flags保持+SD_POWER=1だけが必要です')
    selected={'deferred-sd-development.cer':certpath};components={}
    for role,names in FILES.items():
        files={}
        for name in names:
            rel=role+'/'+name;p=package.file(rel);selected[rel]=p;files[rel]=sha(p)
        check_inf(selected[role+'/'+names[0]],VERSIONS[role],'Host' if role=='sd' else 'Install')
        text=selected[role+'/'+names[0]].read_text(encoding='utf-8-sig')
        hwid,service=('ACPI\\BEN0001','MT8167Shared') if role=='parent' else ('ACPI\\MOU0013','Mt8167Sd')
        if hwid.casefold() not in text.casefold() or not re.search(r'(?im)^AddService\s*=\s*'+service+r'\s*,',text):raise ValueError('SD fixed INF ID/service不一致')
        if role=='sd' and (not re.search(r'(?im)^StartType\s*=\s*3\s*$',text) or 'ACPI\\MOU0001' in text or re.search(r'(?im)^(Include|Needs|CoInstallers32|RegisterDlls)\s*=',text)):
            raise ValueError('SDはMOU0013 nonboot固定INFだけです')
        components[role]={'wrapper_source_commit':SOURCE,'binary_source_commit':SOURCE,'inf_source_commit':SOURCE,
            'driver_ver':VERSIONS[role],'certificate_thumbprint':entry['certificate_thumbprint'].upper(),
            'signed_manifest_sha256':sha(package.manifest),'files':files,'recompiled_by_builder':False}
    selected['parent/parent-manifest.json']=package.file('parent/parent-manifest.json')
    components['sd']['tuple_parent_sys_sha256']=components['parent']['files']['parent/mt8167-shared.sys']
    components['sd']['acpi_prerequisite']='ACPI\\MOU0013 present before install; SD-only qualified firmware required'
    return selected,components,der
