# Fixed Render-only service. No driver compilation or deployment.
param([switch]$EnableRenderProfile,
 [Parameter(Mandatory=$true)][string]$Workspace,
 [Parameter(Mandatory=$true)][string]$PackageMap,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [switch]$Sign,[string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
if(!$EnableRenderProfile){throw 'Explicit Render profile opt-in required'}
if($Sign -and $CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$'){throw 'Explicit signing certificate thumbprint required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $root status --porcelain).Count){throw 'Clean immutable source required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside source checkout'}
& python -B "$root\tools\winpe\render_bound_policy.py" --enable-render-profile --workspace $Workspace --package-map $PackageMap --output $out
if($LASTEXITCODE){throw 'Fixed package policy rejected'}
New-Item -ItemType Directory "$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$lib="$kit\Lib\winv6.3\um\arm"
 $source="$root\tools\winpe\mt8167-render-probe.c"
 & clang --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DA04_RENDER_PROFILE=1 -DA04_RENDER_BIND_UPDATE=1 -DA04_HARDWARE_PROBE_ENGINE_PCM=1 -DA04_ENGINE_DEFERRED_UPDATE=1 "-I$inc\um" "-I$inc\shared" "-I$out" -c $source -o "$out\service.obj"
 if($LASTEXITCODE){throw 'Render-only service ARM compilation failed'}
 $policy=Get-Content -Raw "$out\render-policy-manifest.json"|ConvertFrom-Json
 $compareSource=Join-Path (Join-Path $Workspace $policy.comparator.directory) 'compare-files.exe'
 if((Get-FileHash $compareSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $policy.comparator.exe_sha256){throw 'Existing comparator input hash mismatch'}
 $compareSig=Get-AuthenticodeSignature $compareSource
 if($compareSig.Status -ne 'Valid' -or $compareSig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Existing comparator signature mismatch'}
 Copy-Item $compareSource "$out\compare-files.exe"
 $exe="$out\GE8300RenderModel.exe"
 & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat "$out\service.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "$lib\setupapi.lib" "$lib\bcrypt.lib" "$lib\wintrust.lib" "/out:$exe"
 if($LASTEXITCODE){throw 'Render-only service ARM link failed'}
 if($Sign){
  $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
  if(!$cert.HasPrivateKey -or (Get-Date) -lt $cert.NotBefore -or (Get-Date) -gt $cert.NotAfter){throw 'Valid private signing certificate required'}
  foreach($image in @($exe)){
   & "$kit\bin\x86\signtool.exe" sign /fd SHA256 /sha1 $CertificateThumbprint /s My $image
   if($LASTEXITCODE){throw 'Render service/comparator signing failed'}
   $check=Get-AuthenticodeSignature $image
   if($check.Status -ne 'Valid' -or $check.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Service/comparator signature rejected'}
  }
  $sig=Get-AuthenticodeSignature $exe
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Final service signature rejected'}
  Export-Certificate -Cert $cert -FilePath "$out\mt8167-development.cer"|Out-Null
 }
 & llvm-readobj --file-headers --coff-imports $exe | Set-Content "$out\pe-imports.txt"
 if($LASTEXITCODE){throw 'Service PE inspection failed'}
 $policy=Get-Content -Raw "$out\render-policy-manifest.json"|ConvertFrom-Json
 [ordered]@{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;profile='render-bound-update-v9';service='GE8300RenderModel';signed=[bool]$Sign;signing_thumbprint=$(if($Sign){$sig.SignerCertificate.Thumbprint}else{$null});signature_status=$(if($Sign){[string]$sig.Status}else{'Unsigned'});exe_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();policy_manifest_sha256=(Get-FileHash "$out\render-policy-manifest.json" -Algorithm SHA256).Hash.ToLowerInvariant();policy=$policy;comparator=@{exe='compare-files.exe';exe_sha256=(Get-FileHash "$out\compare-files.exe" -Algorithm SHA256).Hash.ToLowerInvariant();source_sha256=(Get-FileHash "$root\tools\winpe\compare-files.c" -Algorithm SHA256).Hash.ToLowerInvariant();signature_status=$(if($Sign){[string](Get-AuthenticodeSignature "$out\compare-files.exe").Status}else{'Unsigned'});signer_thumbprint=$(if($Sign){(Get-AuthenticodeSignature "$out\compare-files.exe").SignerCertificate.Thumbprint}else{$null});comparison='full bytes through exact EOF; no C runtime or external tool'};normal_install_allowed=$true;device_verified=$false}|ConvertTo-Json -Depth 14|Set-Content -Encoding UTF8 "$out\render-service-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
