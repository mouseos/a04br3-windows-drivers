# Client-only ARM incremental build; no parent, audio, GPU, or device operations.
param([Parameter(Mandatory=$true)][string]$BaselineCapturePackage,
 [Parameter(Mandatory=$true)][string]$BaselineManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
& git -C $root diff --quiet 2d6231727e8ed72b6d7dca4148debc101ac5aeef HEAD -- drivers
if($LASTEXITCODE){throw 'Client-only candidate must preserve all baseline driver sources'}
$manifest=Join-Path $BaselineCapturePackage 'signed-manifest.json'
if($BaselineManifestSha256 -cne '3d75c2ab05948b0863403962be653b8f8ec9a9da6f1bbff9eba00cbf338c85f8' -or (Get-FileHash $manifest -Algorithm SHA256).Hash.ToLowerInvariant() -cne $BaselineManifestSha256){throw 'Fixed capture1026 source manifest required'}
$m=Get-Content $manifest -Raw|ConvertFrom-Json
if($m.source_commit -cne '2d6231727e8ed72b6d7dca4148debc101ac5aeef' -or $m.source_dirty){throw 'Fixed signed driver provenance required'}
foreach($e in $m.files){$p=Join-Path $BaselineCapturePackage $e.path;if((Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant() -cne $e.sha256.ToLowerInvariant()){throw 'Baseline file hash mismatch'}}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm';$sign=Join-Path $kit 'bin\x86\signtool.exe'
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signing identity required'}
New-Item -ItemType Directory "$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 & clang --target=thumbv7-pc-windows-msvc -O2 -w -fno-builtin -fno-vectorize -fno-slp-vectorize -mfpu=none -mno-unaligned-access -fms-extensions -ffreestanding -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\audio\mt8167-audio-capture-probe.c" -o "$out\client.obj"
 if($LASTEXITCODE){throw 'Capture client ARM compile failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\client.obj" "$lib\kernel32.lib" "$lib\setupapi.lib" "$lib\cfgmgr32.lib" "/out:$out\mt8167-capture-probe.exe"
 if($LASTEXITCODE){throw 'Capture client link failed'}
 & $sign sign /fd SHA256 /sha1 $thumb /s My "$out\mt8167-capture-probe.exe"
 if($LASTEXITCODE){throw 'Capture client signing failed'}
 & $sign verify /pa /v "$out\mt8167-capture-probe.exe"
 if($LASTEXITCODE -or (Get-AuthenticodeSignature "$out\mt8167-capture-probe.exe").SignerCertificate.Thumbprint -cne $thumb){throw 'Capture client signer verification failed'}
 $pe=& llvm-readobj --file-headers --coff-imports "$out\mt8167-capture-probe.exe"
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT client required'}
 $pe|Set-Content -Encoding UTF8 "$out\client-pe.txt"
 Export-Certificate -Cert $cert -FilePath "$out\development.cer"|Out-Null
 @{schema=1;source_commit=$commit;source_dirty=$false;driver_source_commit=$m.source_commit;baseline_manifest_sha256=$BaselineManifestSha256;baseline_client_sha256=(Get-FileHash (Join-Path $BaselineCapturePackage 'client\mt8167-capture-probe.exe') -Algorithm SHA256).Hash.ToLowerInvariant();drivers_rebuilt=$false;driver_sources_unchanged=$true;parent_sys_sha256=(Get-FileHash (Join-Path $BaselineCapturePackage 'parent\mt8167-shared.sys') -Algorithm SHA256).Hash.ToLowerInvariant();audio_sys_sha256=(Get-FileHash (Join-Path $BaselineCapturePackage 'audio\mt8167-audio.sys') -Algorithm SHA256).Hash.ToLowerInvariant();signer=$thumb;hardware_verified=$false;capture_succeeded=$false;deployment_performed=$false;source_sha256=(Get-FileHash "$root\tools\audio\mt8167-audio-capture-probe.c" -Algorithm SHA256).Hash.ToLowerInvariant();files=@(@{path='mt8167-capture-probe.exe';sha256=(Get-FileHash "$out\mt8167-capture-probe.exe" -Algorithm SHA256).Hash.ToLowerInvariant()},@{path='development.cer';sha256=(Get-FileHash "$out\development.cer" -Algorithm SHA256).Hash.ToLowerInvariant()})}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\signed-client-manifest.json"
 Compress-Archive -Path "$out\mt8167-capture-probe.exe","$out\development.cer","$out\signed-client-manifest.json","$out\client-pe.txt" -DestinationPath "$out\capture-client-signed.zip"
 Get-FileHash "$out\capture-client-signed.zip" -Algorithm SHA256|Select-Object Hash|ConvertTo-Json
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
