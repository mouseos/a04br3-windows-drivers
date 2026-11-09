#!/usr/bin/env python3
"""ホスト専用。既存Engine更新経路へ固定private入力を渡す。実機操作は行わない。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile


def sha(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('phase', choices=('inputs', 'plan', 'native', 'media'))
    for name in ('workspace', 'package-map', 'backup', 'previous', 'output', 'media-output'):
        ap.add_argument('--' + name, required=True, type=Path)
    ap.add_argument('--remote-root', required=True)
    ap.add_argument('--wifi-checkpoint', type=Path)
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)
    source = Path(__file__).resolve().parents[2]
    for p in (source, a.package_map, a.backup, a.previous, a.output, a.media_output):
        if not p.resolve().is_relative_to(root):
            ap.error('全入力・出力は作業root内が必要です')
    if not __import__('re').fullmatch(r'C:\\mt8167-peripheral[0-9]+-[0-9a-f]{8}', a.remote_root):
        ap.error('専用remote rootが必要です')
    commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip():
        ap.error('固定clean sourceが必要です')
    out = a.output.resolve()
    out.mkdir(mode=0o700, exist_ok=True)
    os.umask(0o077)
    spec = json.loads(a.package_map.read_text(encoding='utf-8-sig'))
    spec['backup_manifest_sha256'] = sha(a.backup)
    map_path = out / 'local-package-map.json'
    base = a.remote_root
    if a.phase == 'inputs':
        if map_path.exists():
            ap.error('既存入力の上書きは禁止です')
        map_path.write_text(json.dumps(spec, indent=2) + '\n')
        sys.path.insert(0, str(source / 'tools/winpe'))
        from engine_deferred_policy import load_map
        load_map(map_path, root)
        paths = {map_path}
        for entry in [*spec['packages'].values(), *spec['clients'].values()]:
            if 'directory' in entry:
                directory = Path(entry['directory']).resolve(strict=True)
                paths.update(p for p in directory.rglob('*') if p.is_file())
            else:
                paths.add(Path(entry['path']))
        with zipfile.ZipFile(out / 'inputs.zip', 'x', zipfile.ZIP_DEFLATED) as z:
            for p in sorted(paths):
                p = p.resolve(strict=True)
                z.write(p, p.relative_to(root).as_posix())
        ps = r'''$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$base='@BASE@';$w="$base\workspace";$src="$w\windows-rt\source";$commit='@COMMIT@'
if((Get-FileHash "$base\inputs.zip" -Algorithm SHA256).Hash.ToLowerInvariant() -cne '@ZIP@'){throw 'Private input mismatch'}
Expand-Archive "$base\inputs.zip" $w
$mapPath="$w\@MAP@";$m=Get-Content -Raw $mapPath|ConvertFrom-Json
foreach($e in @($m.packages.PSObject.Properties.Value)+@($m.clients.PSObject.Properties.Value)){
 foreach($key in @('directory','path')){if($e.PSObject.Properties.Name -contains $key){$e.$key=$e.$key.Replace('@ROOT@',$w).Replace('/','\')}}
}
$m|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 $mapPath
$repo='D:\a\a04br3-windows-drivers\a04br3-windows-drivers'
& git -C $repo fetch -q --no-tags origin $commit;if($LASTEXITCODE){throw 'Source fetch failed'}
& git -C $repo worktree add -q --detach $src $commit;if($LASTEXITCODE){throw 'Source checkout failed'}
$o="$src\build\peripheral-private"
& "$src\tools\ci\build-mt8167-hardware-probe.ps1" -OutputDirectory $o -AudioMode EnginePcm -DeferredPackageMap $mapPath -WorkspaceRoot $w -Sign -CertificateThumbprint BCBF6C456C6F0DD0818434763B189488FBF13570
$sig=Get-AuthenticodeSignature "$o\MT8167EngineProbe.exe"
if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Service signature mismatch'}
New-Item -ItemType Directory "$base\package"|Out-Null
foreach($n in @('MT8167EngineProbe.exe','hardware-probe-manifest.json','deferred-policy-manifest.json')){Copy-Item "$o\$n" "$base\package\$n"}
Export-Certificate -Cert $sig.SignerCertificate -FilePath "$base\package\mt8167-development.cer"|Out-Null
Compress-Archive "$base\package\*" "$base\service.zip"
@{source_commit=$commit;signature_status=[string]$sig.Status;zip_sha256=(Get-FileHash "$base\service.zip" -Algorithm SHA256).Hash.ToLowerInvariant();status='built-signed-runtime-unverified'}|ConvertTo-Json|Set-Content -Encoding UTF8 "$base\completion.json"
Get-Content "$base\completion.json"
'''
        for k, v in {'BASE': base, 'COMMIT': commit, 'ZIP': sha(out/'inputs.zip'), 'MAP': str(map_path.relative_to(root)).replace('/', '\\'), 'ROOT': str(root)}.items():
            ps = ps.replace('@'+k+'@', v)
        (out/'build-fixed.ps1').write_text(ps)
        return
    if json.loads(map_path.read_text()) != spec:
        ap.error('map/backup入力が変更されています')
    if a.phase == 'native':
        plan = out/'engine-rearm-plan.json'
        b = json.loads(a.backup.read_text())
        entries = {e['source_path']: e for e in b['files']}
        with zipfile.ZipFile(out/'native-inputs.zip', 'x', zipfile.ZIP_DEFLATED) as z:
            z.write(plan, 'engine-rearm-plan.json')
            for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2'):
                e = entries['Windows/System32/config/'+name]
                p = a.backup.parent/e['backup_path']
                if sha(p) != e['sha256']:
                    ap.error('native source hash不一致')
                z.write(p, 'input/'+name)
        hashes = [entries['Windows/System32/config/'+n]['sha256'] for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')]
        ps = r'''$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$base='@BASE@'
if((Get-FileHash "$base\native-inputs.zip" -Algorithm SHA256).Hash.ToLowerInvariant() -cne '@ZIP@'){throw 'Native input mismatch'}
Expand-Archive "$base\native-inputs.zip" "$base\native"
& "$base\workspace\windows-rt\source\tools\recovery\prepare-offline-system-service.ps1" -SourceDirectory "$base\native\input" -OutputDirectory "$base\native\prepared" -ExpectedSystemSha256 '@SYSTEM@' -ExpectedLog1Sha256 '@LOG1@' -ExpectedLog2Sha256 '@LOG2@' -ServiceName MT8167EngineProbe -RearmEngineProbe -EngineRearmPlanPath "$base\native\engine-rearm-plan.json" -ExpectedEngineRearmPlanSha256 '@PLAN@'
$r=Get-Content -Raw "$base\native\prepared\result.json"|ConvertFrom-Json
if($r.status -ne 'verified-clean-system-services-only'){throw 'Native preparation failed'}
Compress-Archive "$base\native\prepared\*" "$base\native-private.zip"
@{status=$r.status;candidate_sha256=$r.candidate.sha256;zip_sha256=(Get-FileHash "$base\native-private.zip" -Algorithm SHA256).Hash.ToLowerInvariant()}|ConvertTo-Json|Set-Content -Encoding UTF8 "$base\native-completion.json"
Get-Content "$base\native-completion.json"
'''
        for k, v in dict(BASE=base, ZIP=sha(out/'native-inputs.zip'), SYSTEM=hashes[0], LOG1=hashes[1], LOG2=hashes[2], PLAN=sha(plan)).items():
            ps = ps.replace('@'+k+'@', v)
        (out/'native-fixed.ps1').write_text(ps)
        return
    artifacts = root/'port/xnu/out'
    opts = {'workspace': root, 'source-commit': commit, 'base': artifacts/'windows-rt-cpu-usb-audit-scope-v2-mapfix', 'tools': artifacts/'windows-rt-tools', 'package': artifacts/'windows-rt-peripherals-f17b743-a016d53', 'audio-mode': 'EnginePcm', 'engine-package': artifacts/'windows-rt-engine-pcm-de214c8-e4dff2b', 'engine-client-package': artifacts/'windows-rt-audio-probe-bounded-9bce146/package', 'toprgu-package': artifacts/'windows-rt-toprgu-normal-uart-0821853', 'firmware': artifacts/'windows-rt-mt6630-runtime-firmware-0c756f4', 'backup': a.backup, 'probe-service': out/'service/MT8167EngineProbe.exe', 'probe-manifest': out/'service/hardware-probe-manifest.json', 'probe-certificate': out/'service/mt8167-development.cer', 'deferred-package-map': map_path, 'out': a.media_output, 'rearm-previous-media': a.previous}
    if a.phase == 'plan':
        opts['write-engine-rearm-plan'] = out/'engine-rearm-plan.json'
    else:
        native = out/'native-prepared'
        result = json.loads((native/'result.json').read_text(encoding='utf-8-sig'))
        opts.update({'engine-rearm-plan': out/'engine-rearm-plan.json', 'engine-rearm-plan-sha256': sha(out/'engine-rearm-plan.json'), 'prepared-system': native/'candidate/SYSTEM', 'prepared-system-manifest': native/'result.json', 'prepared-system-sha256': result['candidate']['sha256']})
    if a.wifi_checkpoint and a.phase=='media':
        opts['preupdate-wifi-checkpoint']=a.wifi_checkpoint
        opts['preupdate-wifi-checkpoint-sha256']=sha(a.wifi_checkpoint)
    cmd = [sys.executable, '-B', str(source/'tools/winpe/build-mt8167-shared-io-update-media.py'), '--private-media']
    for k, v in opts.items():
        cmd.extend(['--'+k, str(v)])
    (out/'tmp').mkdir(exist_ok=True)
    env = dict(os.environ, TMPDIR=str(out/'tmp'), PYTHONDONTWRITEBYTECODE='1')
    subprocess.run(cmd, env=env, check=True)


if __name__ == '__main__':
    main()
