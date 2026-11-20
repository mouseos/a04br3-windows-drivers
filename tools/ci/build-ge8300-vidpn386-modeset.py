#!/usr/bin/env python3
"""Build ge8300 modeset386 client over the 385 package (no KMD change).

Compiles drivers/gpu/mt8167/wddm-render/testclient/dxgi-modeset386.c with
MSVC cl (ARM, freestanding mainCRTStartup like present356), links
mt8167-dxgi-modeset386.exe, adds it to a copy of package385 as package386,
then re-catalogs and re-signs. KMD/UMD/FW/transport bytes are retained from
385; only INF (0.0.3.85 -> 0.0.3.86), CAT, and the added client change.
"""
import argparse, glob, hashlib, json, shutil, struct, subprocess, zipfile
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
old = base / 'package385'
out = t / 'build'
pkg = out / 'package386'
NEW_VERSION = '0.0.3.86'
INF_BEFORE = 'DriverVer=11/11/2026,0.0.3.85'
INF_AFTER = 'DriverVer=11/11/2026,0.0.3.86'


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def load(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))


def pin(p, h):
    if sha(p) != h:
        raise ValueError('Input pin: ' + str(p))


def run(c):
    subprocess.run([str(x) for x in c], check=True)


if out.exists():
    raise ValueError('New output required')
pin(old / 'signed-manifest.json', '86387115880b728d4a3c3d1ed695fe8b34407aa86fd141945dc5095ba0ada3c6')
m = load(old / 'signed-manifest.json')
for x in m['outputs'] + m.get('retained_provenance', []):
    pin(old / x['name'], x['sha256'])
link = a.msvc_root / 'bin/Hostx64/arm/link.exe'
pin(link, 'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
cl = a.msvc_root / 'bin/Hostx64/arm/cl.exe'
pin(cl, '697a8f41ff7fcb71e7c6625f5517ef6e2f8ea146d1e351e078317ff92da0839c')
out.mkdir()
pkg.mkdir()
repo = Path(__file__).resolve().parents[2]
src_file = repo / 'drivers/gpu/mt8167/wddm-render/testclient/dxgi-modeset386.c'
wdk = a.wdk81 / 'Include'
vc_inc = a.msvc_root / 'include'
ucrt = sorted(Path('C:/Program Files (x86)/Windows Kits/10/Include').glob('*/ucrt/stddef.h'))
assert ucrt, 'UCRT headers required'
ucrt_inc = ucrt[-1].parent
ulib_dirs = sorted(Path('C:/Program Files (x86)/Windows Kits/10/Lib').glob('*/um/arm/kernel32.lib'))
assert ulib_dirs, 'Kit10 um/arm libs required'
ulib = ulib_dirs[-1].parent
assert (ulib / 'gdi32.lib').exists() and (ulib / 'setupapi.lib').exists(), 'CCD link libs required'
for must in [src_file, wdk / 'km', wdk / 'shared', wdk / 'um', vc_inc, ucrt_inc]:
    assert must.exists(), 'Missing input: ' + str(must)
head = subprocess.run(['git', '-C', str(repo), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
assert head == a.recipe_commit, 'Recipe checkout required'
assert subprocess.run(['git', '-C', str(repo), 'status', '--porcelain', '--untracked-files=no'], capture_output=True, text=True, check=True).stdout.strip() == '', 'Clean tree required'
blob = subprocess.run(['git', '-C', str(repo), 'show', a.recipe_commit + ':drivers/gpu/mt8167/wddm-render/testclient/dxgi-modeset386.c'], capture_output=True, check=True).stdout
assert src_file.read_bytes() == blob, 'Source matches recipe commit'
mem_src = repo / 'drivers/gpu/mt8167/wddm-render/testclient/memory.c'
mem_blob = subprocess.run(['git', '-C', str(repo), 'show', a.recipe_commit + ':drivers/gpu/mt8167/wddm-render/testclient/memory.c'], capture_output=True, check=True).stdout
assert mem_src.read_bytes() == mem_blob, 'Memory helper matches recipe commit'
sign = a.signtool
thumb = a.signer_thumbprint
if not __import__('re').fullmatch(r'[0-9A-Fa-f]{40}', thumb or ''):
    raise ValueError('Explicit 40-hex signer thumbprint required')
obj = out / 'dxgi-modeset386.obj'
mem_obj = out / 'modeset-memory.obj'
flags = ['/nologo', '/c', '/Gy', '/GS-', '/Zl', '/D_ARM_', '/DARM', '/D_WIN32_WINNT=0x0603', '/I' + str(wdk / 'km'), '/I' + str(wdk / 'shared'), '/I' + str(wdk / 'um'), '/I' + str(vc_inc), '/I' + str(ucrt_inc), '/Fo' + str(obj), str(src_file)]
run([cl, *flags])
assert obj.stat().st_size > 0, 'Empty object'
run([cl, '/nologo', '/c', '/Gy', '/GS-', '/Zl', '/D_ARM_', '/DARM', '/I' + str(vc_inc), '/I' + str(ucrt_inc), '/Fo' + str(mem_obj), str(mem_src)])
assert mem_obj.stat().st_size > 0, 'Empty memory object'
run([link, '/nologo', '/machine:arm', '/subsystem:console,6.03', '/entry:mainCRTStartup', '/nodefaultlib', '/largeaddressaware', '/tsaware:no', str(obj), str(mem_obj), *[str(ulib / (n + '.lib')) for n in ['kernel32', 'user32', 'gdi32', 'setupapi']], '/out:' + str(out / 'mt8167-dxgi-modeset386.exe')])
run([sign, 'sign', '/fd', 'SHA256', '/sha1', thumb, out / 'mt8167-dxgi-modeset386.exe'])
run([sign, 'verify', '/pa', out / 'mt8167-dxgi-modeset386.exe'])
for f in old.iterdir():
    if f.is_file() and f.name not in ['ge8300-render.cat', 'ge8300-render.inf', 'signed-manifest.json', 'mt8167-development.cer'] and not f.name.endswith('-build-manifest.json'):
        shutil.copyfile(f, pkg / f.name)
shutil.copyfile(a.signer_cert, pkg / 'mt8167-development.cer')
for d in old.iterdir():
    if d.is_dir():
        shutil.copytree(d, pkg / d.name)
shutil.copyfile(old / 'signed-manifest.json', pkg / 'baseline385-signed-manifest.json')
shutil.copyfile(out / 'mt8167-dxgi-modeset386.exe', pkg / 'mt8167-dxgi-modeset386.exe')
b = (old / 'ge8300-render.inf').read_bytes()
enc = 'utf-16' if b[:2] in [b'\xff\xfe', b'\xfe\xff'] else 'utf-8-sig'
s = b.decode(enc)
if s.count(INF_BEFORE) != 1:
    raise ValueError('Fixed385 DriverVer required')
(pkg / 'ge8300-render.inf').write_text(s.replace(INF_BEFORE, INF_AFTER), encoding=enc)
cat = a.wdk81 / 'bin/x86/Inf2Cat.exe'
run([cat, '/driver:' + str(pkg), '/os:6_3_ARM', '/uselocaltime'])
run([sign, 'sign', '/fd', 'SHA256', '/sha1', thumb, pkg / 'ge8300-render.cat'])
for n in ['ge8300-render.inf', 'ge8300-render.sys', 'ge8300-umd.dll', 'ge8300-mips.fw']:
    run([sign, 'verify', '/pa', '/c', pkg / 'ge8300-render.cat', pkg / n])
for f in sorted((old / 'display-transport').iterdir()):
    if f.is_file():
        target = pkg / 'display-transport' / f.name
        shutil.copyfile(f, target)
        assert sha(target) == sha(f), 'Retained transport restore: ' + f.name
for n in ['ge8300-render.inf', 'ge8300-render.sys', 'ge8300-umd.dll', 'ge8300-mips.fw']:
    run([sign, 'verify', '/pa', '/c', pkg / 'ge8300-render.cat', pkg / n])
new = dict(m)
new.update(schema='vidpn386-modeset-v1', version=NEW_VERSION, candidate_version=386, signer_thumbprint=thumb, package_source_commit=a.recipe_commit, link_recipe_source_commit=a.recipe_commit, compiled_units=[{'source': 'drivers/gpu/mt8167/wddm-render/testclient/dxgi-modeset386.c', 'object': 'dxgi-modeset386.obj', 'tool': 'cl', 'arguments': flags[1:]}, {'source': 'drivers/gpu/mt8167/wddm-render/testclient/memory.c', 'object': 'modeset-memory.obj', 'tool': 'cl', 'arguments': ['/nologo', '/c', '/Gy', '/GS-', '/Zl', '/D_ARM_', '/DARM']}], reused_385_compiled_units=[t for t in m.get('compiled_units', [])], compiler_reexecuted=True, source_changed=True, native_owner_changed=False, linker_changed=False, runtime_build_id='GPU386-modeset-v1', inf_driverver='11/11/2026,0.0.3.86', baseline_manifest_file='baseline385-signed-manifest.json', baseline_manifest_sha256=sha(pkg / 'baseline385-signed-manifest.json'), hardware_verified=False, runtime_verified=False)
new['outputs'] = [{'name': x['name'], 'sha256': sha(pkg / x['name'])} for x in m['outputs']] + [{'name': 'mt8167-dxgi-modeset386.exe', 'sha256': sha(pkg / 'mt8167-dxgi-modeset386.exe')}]
new['retained_provenance'] = [{'name': f.name, 'sha256': sha(f)} for f in pkg.iterdir() if f.is_file() and f.name not in {x['name'] for x in new['outputs']}]
(out / 'build-manifest.json').write_text(json.dumps(new, indent=2) + '\n')
(out / 'build-manifest.json').rename(pkg / 'vidpn386-modeset-build-manifest.json')
new['build_manifest_sha256'] = sha(pkg / 'vidpn386-modeset-build-manifest.json')
(pkg / 'signed-manifest.json').write_text(json.dumps(new, indent=2) + '\n')
with zipfile.ZipFile(t / 'outputs.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(out.rglob('*')):
        if f.is_file():
            z.write(f, f.relative_to(out).as_posix())
print('VIDPN386_SIGNED_MODESET_COMPLETE', sha(pkg / 'signed-manifest.json'), sha(pkg / 'mt8167-dxgi-modeset386.exe'))
