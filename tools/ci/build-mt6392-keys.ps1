# MT6392 child only; no parent/firmware/SDK build or device operation.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside dedicated checkout'}
if(Test-Path $out){throw 'Use new output'}
if(@(& git -C $root status --porcelain).Count){throw 'Source must be committed'}
$commit=(& git -C $root rev-parse HEAD).Trim()
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
$libs=Join-Path $kit 'Lib\winv6.3\km\arm';$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
New-Item -ItemType Directory "$out\package","$out\obj","$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fms-extensions','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none',
 '-D__ldrexd=__mt6392_keys_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13')
 foreach($p in @('km','shared','um','wdf\kmdf\1.13')){$common+=@('-isystem',(Join-Path $inc $p))}
 $sources=@('drivers/input/mt6392-keys/wdf_keys.c','drivers/input/a04br3-buttons/button_state.c','drivers/input/a04br3-buttons/button_pipeline.c');$objects=@();$compiled=@()
 foreach($rel in $sources){
  $src=Join-Path $root $rel;$obj=Join-Path "$out\obj" (([IO.Path]::GetFileNameWithoutExtension($src))+'.obj')
  & clang @common -c $src -o $obj
  if($LASTEXITCODE){throw "Compile failed: $rel"};$objects+=$obj
  $compiled+=@{path=$rel;source_sha256=(Get-FileHash $src -Algorithm SHA256).Hash;object_sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 }
 $sys=Join-Path "$out\package" 'mt6392-keys.sys'
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$libs\ntoskrnl.lib" "$libs\hal.lib" "$libs\bufferoverflowfastfailk.lib" "$libs\libcntpr.lib" "/out:$sys"
 if($LASTEXITCODE){throw 'Link failed'}
 Copy-Item "$root\drivers\input\mt6392-keys\mt6392-keys.inf" "$out\package"
 $sign=Join-Path $kit 'bin\x86\signtool.exe'
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $sys
 if($LASTEXITCODE){throw 'SYS sign failed'}
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$out\package" /os:6_3_ARM
 if($LASTEXITCODE){throw 'Catalog failed'}
 $cat=Join-Path "$out\package" 'mt6392-keys.cat'
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $cat
 if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($p in @($sys,$cat)){
  & $sign verify /pa /q $p
  if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Signer mismatch'}
 }
 foreach($n in @('mt6392-keys.sys','mt6392-keys.inf')){
  & $sign verify /pa /q /c $cat "$out\package\$n"
  if($LASTEXITCODE){throw 'CAT membership failed'}
 }
 $pe=& llvm-readobj --file-headers $sys
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'Not ARMNT'}
 $inputs=@(Get-ChildItem "$root\drivers\input\mt6392-keys" -File|ForEach-Object {@{path=$_.FullName.Substring($root.Length+1).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 foreach($rel in @('drivers/bus/mt8167-shared/client_interface.h','drivers/input/a04br3-buttons/shared_parent_contract.h','drivers/input/a04br3-buttons/button_pipeline.h','drivers/input/a04br3-buttons/button_state.h','tools/ci/build-mt6392-keys.ps1')){$inputs+=@{path=$rel;sha256=(Get-FileHash (Join-Path $root $rel) -Algorithm SHA256).Hash}}
 @{source_commit=$commit;source_dirty=$false;version='0.0.1.0';target='MT6392\Buttons';service='Mt6392Keys';parent_baseline='1d248a71a95360966ff867e4b063504ac764e074';parent_interface_version=1;parent_changed=$false;firmware_changed=$false;device_tested=$false;wake_implemented=$false;PMIC_settings_written=$false;hardware_MMIO_mapped=$false;compiler=(& clang --version)[0];compiled=$compiled;inputs=$inputs;signer=$CertificateThumbprint;codegen=$common;
  outputs=@(Get-ChildItem "$out\package" -File|ForEach-Object {@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
 Write-Output 'MT6392_KEYS_SIGNED_NOT_DEVICE_TESTED'
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
