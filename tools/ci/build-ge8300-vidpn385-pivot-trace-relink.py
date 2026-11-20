#!/usr/bin/env python3
"""Rebuild ge8300-render.sys with real panel frequencies over debug UART (385, pivot/source diagnostic).

Recompiles only drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx with A04_GE8300_VIDPN_TRACE=1,
swaps the TU into the retained KMD archive, relinks, re-signs and re-catalogs.
Diagnostic only: identical VidPn logic to 384 plus E8 pivot-type/id and EA constraining-source-size logging. Keeps 384 frequencies and trace points.
"""
import argparse, hashlib, json, shutil, struct, subprocess, zipfile
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--task-root', type=Path, required=True)
p.add_argument('--recipe-commit', required=True)
p.add_argument('--msvc-root', type=Path, required=True)
p.add_argument('--kit-lib', type=Path, required=True)
p.add_argument('--signtool', type=Path, required=True)
p.add_argument('--wdk81', type=Path, required=True)
p.add_argument('--signer-thumbprint', required=True)
p.add_argument('--signer-cert', type=Path, required=True)
a = p.parse_args()
t = a.task_root
base = t / 'baseline'
old = base / 'package384'
out = t / 'build'
pkg = out / 'package385'
NEW_VERSION = '0.0.3.85'
INF_BEFORE = 'DriverVer=11/11/2026,0.0.3.84'
INF_AFTER = 'DriverVer=11/11/2026,0.0.3.85'


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def load(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))


def pin(p, h):
    if sha(p) != h:
        raise ValueError('Input pin: ' + str(p))


def run(c):
    subprocess.run([str(x) for x in c], check=True)


def header(p):
    b = p.read_bytes()
    at = struct.unpack_from('<I', b, 60)[0]
    assert b[at:at + 4] == b'PE\0\0'
    o = at + 24
    machine, ns = struct.unpack_from('<HH', b, at + 4)
    size = struct.unpack_from('<H', b, at + 20)[0]
    return {'machine': machine,
            'characteristics': struct.unpack_from('<H', b, at + 22)[0],
            'magic': struct.unpack_from('<H', b, o)[0],
            'subsystem': struct.unpack_from('<H', b, o + 68)[0],
            'dll_characteristics': struct.unpack_from('<H', b, o + 70)[0],
            'sections': {b[o + size + i * 40:o + size + i * 40 + 8].rstrip(b'\0').decode():
                         struct.unpack_from('<I', b, o + size + i * 40 + 36)[0] for i in range(ns)}}


if out.exists():
    raise ValueError('New output required')
pin(old / 'signed-manifest.json', 'a6ac399881420f55a51850f5e60c69c88961b216029b67078652f2c7a526cdc0')
m = load(old / 'signed-manifest.json')
for x in m['outputs'] + m.get('retained_provenance', []):
    pin(old / x['name'], x['sha256'])
pin(base / 'ge8300-kmd-clear.lib', '12c4503000b44f5303a5b0ae5cfed771529ffd222f422118a9f6fab447eb3f54')
pin(base / 'ge8300-native-owner-core.lib', 'c7dcbffb1d3de151e22796c0494c033502570950d0afebb4162a6a2f598fa836')
link = a.msvc_root / 'bin/Hostx64/arm/link.exe'
pin(link, 'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
cl = a.msvc_root / 'bin/Hostx64/arm/cl.exe'
pin(cl, '697a8f41ff7fcb71e7c6625f5517ef6e2f8ea146d1e351e078317ff92da0839c')
out.mkdir()
pkg.mkdir()
repo = Path(__file__).resolve().parents[2]
root = repo
src_file = root / 'drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx'
wddm_display = root / 'drivers/gpu/mt8167/wddm-display'
wdk = a.wdk81 / 'Include'
vc_inc = a.msvc_root / 'include'
ucrt = sorted(Path('C:/Program Files (x86)/Windows Kits/10/Include').glob('*/ucrt/stddef.h'))
assert ucrt, 'UCRT headers required'
ucrt_inc = ucrt[-1].parent
for must in [src_file, wddm_display, wdk / 'km', wdk / 'shared', wdk / 'um', vc_inc, ucrt_inc]:
    assert must.exists(), 'Missing input: ' + str(must)
head = subprocess.run(['git', '-C', str(repo), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
assert head == a.recipe_commit, 'Recipe checkout required'
assert subprocess.run(['git', '-C', str(repo), 'status', '--porcelain', '--untracked-files=no'], capture_output=True, text=True, check=True).stdout.strip() == '', 'Clean tree required'
blob = subprocess.run(['git', '-C', str(repo), 'show', a.recipe_commit + ':drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx'], capture_output=True, check=True).stdout
assert src_file.read_bytes() == blob, 'Source matches recipe commit'
obj = out / 'display_bdd_dmm.obj'
flags = ['/nologo', '/c', '/kernel', '/Gy', '/GS-', '/Zl', '/D_KERNEL_MODE', '/D_ARM_', '/DARM', '/DNTDDI_VERSION=0x06030000', '/D_WIN32_WINNT=0x0603', '/DDXGKDDI_INTERFACE_VERSION=0x4002', '/DA04_MT8167_DISPLAY_BRIGHTNESS=1', '/DA04_GE8300_UNIFIED_DISPLAY356=1', '/DA04_GE8300_WDDM_DIRECT_EXPERIMENT=0', '/DA04_GE8300_WDDM_SHARED_AUTOSTART=0', '/DA04_GE8300_WDDM_VENDOR_TILE=0', '/DA04_GE8300_WDDM_COPY=0', '/DA04_GE8300_VIDPN_TRACE=1', '/I' + str(wdk / 'km'), '/I' + str(wdk / 'shared'), '/I' + str(wdk / 'um'), '/I' + str(vc_inc), '/I' + str(ucrt_inc), '/I' + str(wddm_display), '/Fo' + str(obj), str(src_file)]
run([cl, *flags])
assert obj.stat().st_size > 0, 'Empty object'
lib_exe = a.msvc_root / 'bin/Hostx64/x64/lib.exe'
kmd = out / 'ge8300-kmd-clear.lib'
shutil.copyfile(base / 'ge8300-kmd-clear.lib', kmd)
listed = subprocess.run([str(lib_exe), '/LIST', str(kmd)], capture_output=True, text=True, check=True).stdout
victims = [l.strip() for l in listed.splitlines() if l.strip().lower().endswith('display_bdd_dmm.obj')]
if len(victims) != 1:
    raise ValueError('bdd_dmm archive member ambiguous: ' + repr(victims))
run([lib_exe, '/REMOVE:' + victims[0], str(kmd)])
native = base / 'ge8300-native-owner-core.lib'
shutil.copyfile(native, out / native.name)
kit = a.kit_lib
kargs = ['/nologo', '/driver', '/subsystem:native,6.03', '/machine:arm', '/entry:DriverEntry', '/nodefaultlib', '/incremental:no', '/largeaddressaware', '/tsaware:no', '/base:0x400000', str(obj), str(kmd), str(out / native.name), *[str(kit / (n + '.lib')) for n in ['displib', 'ntoskrnl', 'hal', 'libcntpr', 'bufferoverflowfastfailk']], '/out:' + str(out / 'ge8300-render.sys'), '/map:' + str(out / 'ge8300-render.map')]
run([link, *kargs])
h = header(out / 'ge8300-render.sys')
if (h['machine'], h['magic'], h['characteristics'], h['subsystem'], h['dll_characteristics']) != (0x1c4, 0x10b, 0x122, 1, 0x140):
    raise ValueError('RT81 baseline PE contract not restored')
for n in ['.text', '.rdata', '.data', '.pdata']:
    if not h['sections'].get(n, 0) & 0x08000000:
        raise ValueError('Baseline nonpageable section contract: ' + n)
sign = a.signtool
thumb = a.signer_thumbprint
if not __import__('re').fullmatch(r'[0-9A-Fa-f]{40}', thumb or ''):
    raise ValueError('Explicit 40-hex signer thumbprint required')
run([sign, 'sign', '/fd', 'SHA256', '/sha1', thumb, out / 'ge8300-render.sys'])
run([sign, 'verify', '/pa', out / 'ge8300-render.sys'])
for f in old.iterdir():
    if f.is_file() and f.name not in ['ge8300-render.sys', 'ge8300-render.cat', 'ge8300-render.inf', 'signed-manifest.json', 'mt8167-development.cer'] and not f.name.endswith('-build-manifest.json'):
        shutil.copyfile(f, pkg / f.name)
shutil.copyfile(a.signer_cert, pkg / 'mt8167-development.cer')
for d in old.iterdir():
    if d.is_dir():
        shutil.copytree(d, pkg / d.name)
shutil.copyfile(old / 'signed-manifest.json', pkg / 'baseline384-signed-manifest.json')
shutil.copyfile(out / 'ge8300-render.sys', pkg / 'ge8300-render.sys')
b = (old / 'ge8300-render.inf').read_bytes()
enc = 'utf-16' if b[:2] in [b'\xff\xfe', b'\xfe\xff'] else 'utf-8-sig'
s = b.decode(enc)
before = 'DriverVer=11/11/2026,0.0.3.83'
after = 'DriverVer=11/11/2026,0.0.3.84'
if s.count(before) != 1:
    raise ValueError('Fixed384 DriverVer required')
(pkg / 'ge8300-render.inf').write_text(s.replace(before, after), encoding=enc)
cat = a.wdk81 / 'bin/x86/Inf2Cat.exe'
run([cat, '/driver:' + str(pkg), '/os:6_3_ARM', '/uselocaltime'])
run([sign, 'sign', '/fd', 'SHA256', '/sha1', thumb, pkg / 'ge8300-render.cat'])
for n in ['ge8300-render.inf', 'ge8300-render.sys', 'ge8300-umd.dll', 'ge8300-mips.fw']:
    run([sign, 'verify', '/pa', '/c', pkg / 'ge8300-render.cat', pkg / n])
# Inf2Cat processes the whole package dir and regenerates the retained
# display-transport catalog as a side effect. Restore the baseline bytes so
# the only deltas vs the 380 package are INF/SYS/CAT.
for f in sorted((old / 'display-transport').iterdir()):
    if f.is_file():
        target = pkg / 'display-transport' / f.name
        shutil.copyfile(f, target)
        assert sha(target) == sha(f), 'Retained transport restore: ' + f.name
for n in ['ge8300-render.inf', 'ge8300-render.sys', 'ge8300-umd.dll', 'ge8300-mips.fw']:
    run([sign, 'verify', '/pa', '/c', pkg / 'ge8300-render.cat', pkg / n])
new = dict(m)
new.update(schema='vidpn385-pivot-trace-v1', version=NEW_VERSION, candidate_version=385, signer_thumbprint=thumb, package_source_commit=a.recipe_commit, link_recipe_source_commit=a.recipe_commit, compiled_units=[{'source': 'drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx', 'object': 'display_bdd_dmm.obj', 'tool': 'cl', 'arguments': flags[1:]}], reused_384_compiled_units=[t for t in m.get('compiled_units', []) if t.get('source') != 'drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx'], compiler_reexecuted=True, source_changed=True, native_owner_changed=False, linker_changed=False, runtime_build_id='GPU385-pivot-trace-v1', inf_driverver='11/11/2026,0.0.3.85', baseline_manifest_file='baseline384-signed-manifest.json', baseline_manifest_sha256=sha(pkg / 'baseline384-signed-manifest.json'), PE_contract=h, hardware_verified=False, runtime_verified=False)
new['outputs'] = [{'name': x['name'], 'sha256': sha(pkg / x['name'])} for x in m['outputs']]
new['retained_provenance'] = [{'name': f.name, 'sha256': sha(f)} for f in pkg.iterdir() if f.is_file() and f.name not in {x['name'] for x in new['outputs']}]
(out / 'build-manifest.json').write_text(json.dumps(new, indent=2) + '\n')
(out / 'build-manifest.json').rename(pkg / 'vidpn385-pivot-trace-build-manifest.json')
new['build_manifest_sha256'] = sha(pkg / 'vidpn385-pivot-trace-build-manifest.json')
(pkg / 'signed-manifest.json').write_text(json.dumps(new, indent=2) + '\n')
with zipfile.ZipFile(t / 'outputs.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(out.rglob('*')):
        if f.is_file():
            z.write(f, f.relative_to(out).as_posix())
print('VIDPN385_SIGNED_RELINK_COMPLETE', sha(pkg / 'signed-manifest.json'), sha(pkg / 'ge8300-render.sys'))
