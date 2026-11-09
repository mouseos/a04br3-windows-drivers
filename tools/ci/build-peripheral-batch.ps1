# Dedicated batch service only; never build/install an old Engine or GPU client.
param([switch]$EnablePeripheralBatch,[Parameter(Mandatory=$true)][string]$Workspace,
 [Parameter(Mandatory=$true)][string]$PackageMap,[Parameter(Mandatory=$true)][string]$OutputDirectory,
 [switch]$Sign,[string]$CertificateThumbprint)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
if(!$EnablePeripheralBatch -or !$Sign -or $CertificateThumbprint -cne $thumb){throw 'Explicit finalized batch and existing BCBF signer required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(@(& git -C $root status --porcelain).Count -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Clean immutable source and checkout-local output required'}
& python -B "$root\tools\winpe\peripheral_batch_policy.py" --workspace $Workspace --map $PackageMap --out $out
if($LASTEXITCODE){throw 'Draft or package contract rejected'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$lib="$kit\Lib\winv6.3\um\arm"
& clang --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DA04_PERIPHERAL_BATCH=1 "-I$inc\um" "-I$inc\shared" "-I$out" -c "$root\tools\winpe\mt8167-peripheral-update.c" -o "$out\service.obj"
if($LASTEXITCODE){throw 'Peripheral service ARM compile failed'}
$exe="$out\MT8167PeripheralBatch.exe"
& lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat "$out\service.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "$lib\setupapi.lib" "$lib\cfgmgr32.lib" "$lib\bcrypt.lib" "$lib\wintrust.lib" "$lib\crypt32.lib" "/out:$exe"
if($LASTEXITCODE){throw 'Peripheral service ARM link failed'}
& "$kit\bin\x86\signtool.exe" sign /fd SHA256 /sha1 $thumb /s My $exe
if($LASTEXITCODE){throw 'Peripheral service signing failed'}
$sig=Get-AuthenticodeSignature $exe
if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Peripheral service signature rejected'}
$policy=Get-Content "$out\peripheral-policy.json" -Raw|ConvertFrom-Json
foreach($p in $policy.packages.PSObject.Properties.Value){
 $d=Join-Path $Workspace $p.directory
 foreach($n in @($p.names.inf,$p.names.sys)){
  & "$kit\bin\x86\signtool.exe" verify /pa /c (Join-Path $d $p.names.cat) (Join-Path $d $n)
  if($LASTEXITCODE){throw 'Required candidate CAT membership failed'}
 }
 $s=Get-AuthenticodeSignature (Join-Path $d $p.names.cat)
 if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw 'Candidate CAT identity mismatch'}
}
[ordered]@{schema='peripheral-batch-service-v1';profile=$policy.profile;source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;service='MT8167PeripheralBatch';signer_thumbprint=$thumb;signature_status='Valid';exe_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();policy_manifest_sha256=(Get-FileHash "$out\peripheral-policy.json" -Algorithm SHA256).Hash.ToLowerInvariant();preservation_plan_sha256=(Get-FileHash "$out\peripheral-preservation-plan.json" -Algorithm SHA256).Hash.ToLowerInvariant();policy=$policy;device_verified=$false}|ConvertTo-Json -Depth 24|Set-Content -Encoding UTF8 "$out\peripheral-service-manifest.json"
