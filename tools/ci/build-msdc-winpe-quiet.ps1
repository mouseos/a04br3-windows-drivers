# WinPE preparation package only. Reuse the fixed signed quiet1.8 SYS byte-for-byte.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$StoragePackage,
 [Parameter(Mandatory=$true)][string]$StorageManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$msdcRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$msdcOut=[IO.Path]::GetFullPath($OutputDirectory)
$msdcInput=[IO.Path]::GetFullPath($StoragePackage)
if(!$msdcOut.StartsWith($msdcRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $msdcOut)){throw 'New output inside checkout required'}
$msdcCommit=(& git -C $msdcRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $msdcRoot status --porcelain).Count){throw 'Clean fixed source required'}
$msdcManifestPath=Join-Path $msdcInput 'signed-manifest.json'
if($StorageManifestSha256 -cnotmatch '^[0-9a-f]{64}$' -or (Get-FileHash $msdcManifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $StorageManifestSha256){throw 'Explicit signed package manifest pin mismatch'}
$msdcManifest=Get-Content $msdcManifestPath -Raw|ConvertFrom-Json
if($msdcManifest.source_commit -cne 'e2db926f1ddd12b4cae341a7df50f832e20c16e5' -or $msdcManifest.source_dirty -or $msdcManifest.driver_version -ne '0.0.1.8' -or !$msdcManifest.logs_only_change -or $msdcManifest.normal_io_trace_enabled -or $msdcManifest.dma_poll_trace_enabled){throw 'Fixed logs-only quiet1.8 package required'}
if($msdcManifest.flags.A04_DMA_POLL_TRACE -ne '0' -or $msdcManifest.flags.A04_MSDC_VERBOSE_IO_TRACE -ne '0'){throw 'Both routine trace modes must be off'}
$msdcFiles=@{}
foreach($msdcName in @('mt8167-msdc.sys','mt8167-msdc.inf','mt8167-msdc.cat')){
 $msdcEntry=@($msdcManifest.files|Where-Object{$_.path -ceq $msdcName})
 if($msdcEntry.Count -ne 1){throw 'Missing or duplicate package pin'}
 $msdcFiles[$msdcName]=(Get-FileHash (Join-Path $msdcInput $msdcName) -Algorithm SHA256).Hash.ToLowerInvariant()
 if($msdcFiles[$msdcName] -cne $msdcEntry[0].sha256.ToLowerInvariant()){throw 'Package file hash mismatch'}
}
if($msdcFiles['mt8167-msdc.sys'] -cne 'f92ee2d4e058e345539567153e0b4a20b13669f34f84de017dece2e45707dadd'){throw 'Fixed driver byte pin mismatch'}
if($CertificateThumbprint -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Existing signing identity required'}
$msdcKit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$msdcSign=Join-Path $msdcKit 'bin\x86\signtool.exe'
$msdcCert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
if(!$msdcCert.HasPrivateKey -or $msdcCert.NotAfter -le (Get-Date)){throw 'Signing identity unavailable'}
foreach($msdcName in @('mt8167-msdc.sys','mt8167-msdc.cat')){
 & $msdcSign verify /pa (Join-Path $msdcInput $msdcName)
 if($LASTEXITCODE -or (Get-AuthenticodeSignature (Join-Path $msdcInput $msdcName)).SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Input signer invalid'}
}
foreach($msdcName in @('mt8167-msdc.sys','mt8167-msdc.inf')){
 & $msdcSign verify /pa /c (Join-Path $msdcInput 'mt8167-msdc.cat') (Join-Path $msdcInput $msdcName)
 if($LASTEXITCODE){throw 'Input CAT membership invalid'}
}
$msdcPkg=Join-Path $msdcOut 'package'
New-Item -ItemType Directory $msdcPkg|Out-Null
# Retain the tested WinPE drvload path. INF bytes stay exact; a new CAT binds
# the alias a04-msdc-probe.inf. Never rename an INF under its original catalog.
Copy-Item (Join-Path $msdcInput 'mt8167-msdc.inf') (Join-Path $msdcPkg 'a04-msdc-probe.inf')
Copy-Item (Join-Path $msdcInput 'mt8167-msdc.sys') (Join-Path $msdcPkg 'mt8167-msdc.sys')
& (Join-Path $msdcKit 'bin\x86\Inf2Cat.exe') "/driver:$msdcPkg" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE){throw 'WinPE alias CAT generation failed'}
& $msdcSign sign /fd SHA256 /sha1 $CertificateThumbprint /s My (Join-Path $msdcPkg 'mt8167-msdc.cat')
if($LASTEXITCODE){throw 'WinPE alias CAT signing failed'}
foreach($msdcName in @('a04-msdc-probe.inf','mt8167-msdc.sys')){
 & $msdcSign verify /pa /c (Join-Path $msdcPkg 'mt8167-msdc.cat') (Join-Path $msdcPkg $msdcName)
 if($LASTEXITCODE){throw 'WinPE alias CAT membership failed'}
}
$msdcPins=[ordered]@{}
foreach($msdcPair in @(@('INF','a04-msdc-probe.inf'),@('SYS','mt8167-msdc.sys'),@('CAT','mt8167-msdc.cat'))){
 $msdcPins[$msdcPair[0]]=(Get-FileHash (Join-Path $msdcPkg $msdcPair[1]) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$msdcHeader=Join-Path $msdcOut 'msdc-winpe-storage-pins.h'
@($msdcPins.Keys|ForEach-Object{'#define A04_WINPE_MSDC_'+$_+'_SHA256 "'+$msdcPins[$_]+'"'})|Set-Content -Encoding ASCII $msdcHeader
$msdcInc=Join-Path $msdcKit 'Include';$msdcLib=Join-Path $msdcKit 'Lib\winv6.3\um\arm'
& clang --target=thumbv7-pc-windows-msvc -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$msdcInc\um" "-I$msdcInc\shared" "-I$msdcOut" -c "$msdcRoot\tools\winpe\observe-msdc-storage.c" -o "$msdcOut\observe-msdc-storage.obj"
if($LASTEXITCODE){throw 'Pinned ARM observer compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$msdcOut\observe-msdc-storage.obj" "$msdcLib\kernel32.lib" "$msdcLib\advapi32.lib" "$msdcLib\cfgmgr32.lib" "/out:$msdcPkg\observe-msdc-storage.exe"
if($LASTEXITCODE){throw 'Pinned ARM observer link failed'}
& $msdcSign sign /fd SHA256 /sha1 $CertificateThumbprint /s My "$msdcPkg\observe-msdc-storage.exe"
if($LASTEXITCODE){throw 'Observer signing failed'}
foreach($msdcName in @('observe-msdc-storage.exe','mt8167-msdc.sys','mt8167-msdc.cat')){
 & $msdcSign verify /pa (Join-Path $msdcPkg $msdcName)
 if($LASTEXITCODE -or (Get-AuthenticodeSignature (Join-Path $msdcPkg $msdcName)).SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Output signer invalid'}
}
$msdcPe=& llvm-readobj --file-headers --coff-imports "$msdcPkg\observe-msdc-storage.exe"
if($LASTEXITCODE -or ($msdcPe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT observer required'}
$msdcPe|Set-Content -Encoding UTF8 "$msdcOut\observer-pe.txt"
Export-Certificate -Cert $msdcCert -FilePath "$msdcPkg\msdc-development.cer"|Out-Null
if((Get-FileHash "$msdcPkg\mt8167-msdc.sys" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $msdcFiles['mt8167-msdc.sys']){throw 'Reused driver bytes changed'}
[ordered]@{schema=1;source_commit=$msdcCommit;source_dirty=$false;driver_source_commit=$msdcManifest.source_commit;
 driver_version='0.0.1.8';input_signed_manifest_sha256=$StorageManifestSha256;driver_recompiled=$false;driver_resigned=$false;
 observer_source_commit='c10ace9f26b0a9e389dd2dcc6ba77cc92b9b68c3';observer_change='generated immutable package pins only';
 signer=$CertificateThumbprint;normal_io_trace_enabled=$false;dma_poll_trace_enabled=$false;
 error_init_checkpoint_trace_retained=$true;dma_logic_modified=$false;timeouts_modified=$false;pio_fallback_added=$false;
 winpe_only=$true;installed_windows_update=$false;hardware_verified=$false;performance_change_measured=$false;
 pin_header_sha256=(Get-FileHash $msdcHeader -Algorithm SHA256).Hash.ToLowerInvariant();pins=$msdcPins;
 files=@(Get-ChildItem $msdcPkg -File|ForEach-Object{@{path=$_.Name;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$msdcPkg\signed-manifest.json"
Copy-Item $msdcHeader $msdcPkg
Compress-Archive -Path "$msdcPkg\*" -DestinationPath "$msdcOut\msdc-winpe-quiet-signed.zip"
Get-FileHash "$msdcOut\msdc-winpe-quiet-signed.zip" -Algorithm SHA256|ConvertTo-Json
'MSDC_WINPE_QUIET_COMPLETE'
