# SPDX-License-Identifier: MIT
# Build the display-only companion to the separately owned GE8300 render adapter.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit companion display candidate switch required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE){throw 'Cannot resolve source'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh output directory inside checkout required'}
$flags=@{}
# Reset every GPU feature recognized by this fixed builder, including inherited flags.
$builder=Join-Path $PSScriptRoot 'build-wddm-display-arm.ps1'
$flagNames=@([regex]::Matches([IO.File]::ReadAllText($builder),'\$env:(A04_GE8300_[A-Z0-9_]+)')|ForEach-Object {$_.Groups[1].Value}|Sort-Object -Unique)
if($flagNames.Count -lt 20){throw 'Unexpected display build flag contract'}
$names=@($flagNames)+@('A04_MT8167_DISPLAY_BRIGHTNESS','TEMP','TMP','RUNNER_TEMP')
$previous=@{}
foreach($name in $names){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
try {
 foreach($name in $flagNames){[Environment]::SetEnvironmentVariable($name,'0','Process');$flags[$name]=0}
 $env:A04_MT8167_DISPLAY_BRIGHTNESS='1';$flags['A04_MT8167_DISPLAY_BRIGHTNESS']=1
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$out
 & (Join-Path $PSScriptRoot 'build-wddm-display-package-arm.ps1')
 $pkg=Join-Path $out 'mt8167-display-package'
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86"
 $machine=Test-Path "Cert:\LocalMachine\My\$CertificateThumbprint"
 $cert=if($machine){Get-Item "Cert:\LocalMachine\My\$CertificateThumbprint"}else{Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"}
 if(!$cert.HasPrivateKey){throw 'Signing private key unavailable'}
 $store=@();if($machine){$store=@('/sm')}
 & "$kit\signtool.exe" sign /fd SHA256 /sha1 $CertificateThumbprint /s My @store "$pkg\mt8167-display.sys"
 if($LASTEXITCODE){throw 'SYS signing failed'}
 & "$kit\Inf2Cat.exe" "/driver:$pkg" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Catalog generation failed'}
 & "$kit\signtool.exe" sign /fd SHA256 /sha1 $CertificateThumbprint /s My @store "$pkg\mt8167-display.cat"
 if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($name in @('mt8167-display.sys','mt8167-display.cat')){
  $sig=Get-AuthenticodeSignature (Join-Path $pkg $name)
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Signature validation failed'}
 }
 foreach($name in @('mt8167-display.inf','mt8167-display.sys')){
  & "$kit\signtool.exe" verify /pa /c "$pkg\mt8167-display.cat" (Join-Path $pkg $name)
  if($LASTEXITCODE){throw 'Catalog membership failed'}
 }
 Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
 @{source_commit=$commit;binary_source_commit=$commit;inf_source_commit=$commit;
   hardware='MediaTek MT8167 DISP_PWM0';driver_ver='11/07/2026,0.0.1.6';
   certificate_thumbprint=$CertificateThumbprint;effective_flags=$flags;
   presentation='CPU';separate_ge8300_render_owner=$true;hardware_verified=$false;
   files=@(Get-ChildItem $pkg -File|ForEach-Object {@{path=$_.Name;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
 }|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 Compress-Archive -Path "$pkg\*" -DestinationPath (Join-Path $out 'package.zip')
 Get-FileHash (Join-Path $out 'package.zip') -Algorithm SHA256
 Write-Output 'CPU_DISPLAY_COMPANION_SIGNED_NOT_DEVICE_VERIFIED'
} finally {
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
}
