#!/usr/bin/env python3
"""Observed PnP binding -> deferred standard install -> one reboot -> GPU readback."""
import argparse, hashlib, json, re, struct, subprocess
from pathlib import Path
from render_profile_policy import (SIGNER, BOOT, BASE_SYS, TARGETS, package,
                                  require, rooted, jsonfile, hexpin, sha, version, cbytes)
from engine_io_package import require_armnt

PROFILE = 'render-bound-update-v8'
COLOR = '4f55794471a4008c1ceaa3a1da2679c0f3fefd2a'
BASE_NAMES = {'parent':'mt8167-shared.sys', 'display':'mt8167-display.sys',
              'render':'mt8167-ge8300.sys'}
BASE_SERVICES = {'parent':'Mt8167Shared', 'display':'MT8167Display', 'render':'MT8167GE8300'}

def load_map(path, root):
    root = root.resolve(strict=True)
    spec = jsonfile(rooted(root, path))
    require(set(spec) == {'schema','profile','backup','backup_sha256','bindings','packages','boot','client','comparator'}, 'Fixed binding-update input fields required')
    require(spec['schema'] == 1 and spec['profile'] == PROFILE, 'Explicit bound-update profile required')
    backup = rooted(root, spec['backup'])
    require(backup.name == 'manifest.json' and sha(backup) == hexpin(spec['backup_sha256']), 'Fresh backup pin mismatch')
    b = jsonfile(backup)
    require(b.get('status') == 'captured-unmounted', 'Completed unmounted backup required')
    source = {}
    for e in b['files']:
        if not e.get('backup_path'):
            continue
        f = rooted(root, backup.parent / e['backup_path'])
        require(sha(f) == e['sha256'] and f.stat().st_size == e['bytes'], 'Fresh backup bytes mismatch')
        source[e['source_path'].replace('\\','/').lower()] = e
    receipt = rooted(root, spec['bindings']['path'])
    require(sha(receipt) == hexpin(spec['bindings']['sha256']), 'Binding receipt pin mismatch')
    r = jsonfile(receipt)
    require(r.get('source_manifest_sha256') == sha(backup) and not r.get('errors') and
            all(r.get(k) == 0 for k in ('load_rc','unload_rc','privilege_restore_error')), 'Native binding read incomplete')
    require(set(r['nodes']) == {'BEN0001','MOU0007','MOU0006'}, 'Fixed three-node receipt required')
    baselines = {}
    for role, target in TARGETS.items():
        rows = r['nodes'][target[0].split('\\')[1]]
        require(len(rows) == 1, 'Ambiguous observed device binding')
        row = rows[0]
        require(target[0].lower() in [x.lower() for x in row['HardwareID']], 'Binding hardware ID mismatch')
        modern_render=role=='render' and row['Service'].lower()=='ge8300render'
        require(modern_render or row['Service'].lower() == BASE_SERVICES[role].lower(), 'Unexpected existing service')
        expected_class = TARGETS['display' if role == 'display' or modern_render else 'parent'][2]
        require(row['ClassGUID'].lower() == expected_class.lower(), 'Unexpected existing class')
        selected = row['selected']
        num, date = version('11/07/2026,' + selected['DriverVersion'])
        require(struct.unpack('<Q', bytes.fromhex(selected['DriverDateData']))[0] == date, 'Observed driver date mismatch')
        sys_name='ge8300-render.sys' if modern_render else BASE_NAMES[role]
        e = source['windows/system32/drivers/' + sys_name]
        baselines[role] = {'driver_version':selected['DriverVersion'], 'version_u64':num,
                           'date_filetime':date, 'service':row['Service'], 'class_guid':row['ClassGUID'],
                           'sys_name':sys_name, 'sys_sha256':e['sha256'], 'inf':selected['InfPath']}
    known_render={'0.0.3.8':'4784de5269595c8c98e261793ffc4979c026f2d974ea6898317bf382ea11c245',
                  '0.0.3.7':'74fa75a21253129ee371b658f53c180af171b3cd46448718ba7a735d64d2d6f9',
                  '0.0.3.3':'51f92e0975a5274b620769e2c6b45628ae3effd5ef82da080b9af939dd1f4f55',
                  '0.0.3.4':'88909fbd30228c3de9a48a9ef8206a267848d9209d1373998073c05c16fce17f',
                  '0.0.3.5':'e2e62def79e347a6b10f3f04dbb714eb62d85006aedda45fe337240198f8ac4a',
                  '0.0.3.6':'4caec0e7ef7714db918745cd43de41dfc83920128fd96ccf9de491b53433b116'}
    expected_gpu=known_render.get(baselines['render']['driver_version']) if baselines['render']['service'].lower()=='ge8300render' else BASE_SYS
    require(expected_gpu is not None and baselines['render']['sys_sha256']==expected_gpu, 'Existing GPU baseline version/bytes mismatch')
    repo = Path(__file__).resolve().parents[2]
    require(set(spec['packages']) == set(TARGETS), 'Exactly three signed packages required')
    packages = {k: package(root, spec['packages'][k], k, repo, render_color=(k=='render')) for k in TARGETS}
    boot = rooted(root, spec['boot']); bm = jsonfile(boot)
    require(bm.get('uefi_source_commit') == BOOT, 'Matched GPU-domain firmware required')
    image = rooted(root, boot.parent / 'boot.img')
    require(sha(image) == bm['boot_img_sha256'], 'Matched firmware hash mismatch')
    cd = rooted(root, spec['client']['directory']); cm = cd/'signed-client-manifest.json'
    require(sha(cm) == hexpin(spec['client']['manifest_sha256']), 'Signed client manifest mismatch')
    cr = jsonfile(cm); build = cd/'client-build-manifest.json'
    require(cr.get('source_commit') == COLOR and cr.get('color_pair_profile') is True and
            cr.get('signature_status') == 'Valid' and cr.get('signer_thumbprint') == SIGNER and
            cr.get('fixed_arguments') == '--hardware=PowerVR' and cr.get('retry_count') == 0 and
            cr.get('external_timeout_ms') == 120000, 'Fixed standard COLOR client contract mismatch')
    require(sha(build) == hexpin(cr['build_manifest_sha256']) and jsonfile(build)['source_commit'] == COLOR, 'Client build provenance mismatch')
    exe = cd/'ge8300-d3d-clear.exe'; require(sha(exe) == hexpin(cr['signed_sha256']), 'Client bytes mismatch'); require_armnt(exe)
    compdir = rooted(root, spec['comparator']['directory']); compm = compdir/'render-service-manifest.json'
    require(sha(compm) == hexpin(spec['comparator']['manifest_sha256']), 'Comparator provenance mismatch')
    cp = jsonfile(compm)['comparator']; compare = compdir/'compare-files.exe'
    require(cp['signature_status'] == 'Valid' and cp['signer_thumbprint'] == SIGNER and sha(compare) == cp['exe_sha256'], 'Existing signed comparator mismatch')
    return {'schema':1, 'profile':PROFILE, 'default_off':True,
            'backup_path':backup.relative_to(root).as_posix(), 'backup_manifest_sha256':sha(backup),
            'binding_receipt_sha256':sha(receipt), 'baselines':baselines, 'packages':packages,
            'client':{'directory':cd.relative_to(root).as_posix(),'source_commit':COLOR,'sha256':sha(exe),
                      'manifest_sha256':sha(cm),'arguments':'--hardware=PowerVR','timeout_ms':120000},
            'comparator':{'directory':compdir.relative_to(root).as_posix(),'manifest_sha256':sha(compm),**cp},
            'boot':{'source_commit':BOOT,'manifest_sha256':sha(boot),'sha256':sha(image),'path':image.relative_to(root).as_posix()},
            'normal_install_allowed':True,
            'sequence':['WinPE stages fixed files and new GE8300RenderNode service only',
                        'Normal OS selects exact signed INF via standard DIF, without live device restart',
                        'One planned reboot, then exact binding/file gates and one standard DXGI COLOR client']}

def generate(p):
    token = hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    lines = ['/* Bound-driver update; private stage data is separately pinned. */',
             '#define DEFERRED_TARGET_COUNT 3', '#define DEFERRED_GPU_UPDATE 0',
             '#define DEFERRED_SD_UPDATE 0', '#define DEFERRED_BRIGHTNESS_SCOPED_LOG 0',
             '#define RENDER_OFFLINE_DIAGNOSTIC 0', 'static const struct deferred_pin deferred_pins[3]={']
    for role in TARGETS:
        v = p['packages'][role]; hashes = list(v['files'].values()) + ['0'*64]*(5-len(v['files']))
        lines.append('{'+str(v['version_u64'])+'ULL,'+str(v['date_filetime'])+'ULL,{'+','.join(cbytes(h) for h in hashes)+'}},')
    lines += ['};','static const struct deferred_pin deferred_baselines[3]={']
    for role in TARGETS:
        v=p['baselines'][role]
        lines.append('{'+str(v['version_u64'])+'ULL,'+str(v['date_filetime'])+'ULL,{{0},'+cbytes(v['sys_sha256'])+'}},')
    lines += ['};']
    for field, cname in [('sys_name','sysnames'),('class_guid','classes'),('service','services')]:
        values=[p['baselines'][k][field] for k in TARGETS]
        require(all(re.fullmatch(r'[A-Za-z0-9_.{}-]{1,64}',v) for v in values),'Invalid fixed baseline identifier')
        lines.append('static const WCHAR *const deferred_baseline_'+cname+'[3]={'+','.join('L"'+v+'"' for v in values)+'};')
    # Kept for the frozen non-bound branch, not used by this profile.
    lines += ['static const struct deferred_pin deferred_render_baseline={0};',
              'static const char deferred_token[]="'+token+'";',
              'static const BYTE deferred_client_hashes[1][32]={'+cbytes(p['client']['sha256'])+'};']
    return '\n'.join(lines)+'\n'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--enable-render-profile',action='store_true')
    for name in ('workspace','package-map','output'):ap.add_argument('--'+name,type=Path,required=True)
    a=ap.parse_args();require(a.enable_render_profile,'Explicit opt-in required');p=load_map(a.package_map,a.workspace)
    out=a.output.resolve();require(out.is_relative_to(a.workspace.resolve()) and not out.exists(),'Fresh workspace output required')
    out.mkdir(parents=True);(out/'render-deferred-policy.h').write_text(generate(p));(out/'render-policy-manifest.json').write_text(json.dumps(p,indent=2)+'\n')

if __name__=='__main__':main()
