# 1Win32 TU only. Existing headers/libs/signer; no device/RF action on build host.
param([Parameter(Mandatory=$true)][string]$KitHeaderRoot,[Parameter(Mandatory=$true)][string]$LibraryRoot,
 [Parameter(Mandatory=$true)][string]$SigntoolPath,[Parameter(Mandatory=$true)][string]$GeneratedHeaderDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out) -or @(& git -C $root status --porcelain).Count){throw 'Clean source and new checkout-local output required'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$source="$root\tools\winpe\mt6630-wlan-scan-followup.c";$header=Join-Path $GeneratedHeaderDirectory 'peripheral-service-path-policy.h'
if(!(Test-Path $header) -or (Get-Item $header).Length -gt 65536){throw 'Pinned generated fixed paths required'}
New-Item -ItemType Directory $out|Out-Null
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP=$out;$env:TEMP=$out
 & clang --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$KitHeaderRoot\um" "-I$KitHeaderRoot\shared" "-I$GeneratedHeaderDirectory" -c $source -o "$out\scan.obj"
 if($LASTEXITCODE){throw 'Actual ARM WLAN TU compile failed'}
 & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat "$out\scan.obj" "$LibraryRoot\kernel32.lib" "$LibraryRoot\advapi32.lib" "/out:$out\mt6630-wlan-scan-followup.exe"
 if($LASTEXITCODE){throw 'ARM WLAN link failed'}
 & $SigntoolPath sign /fd SHA256 /s My /sha1 $thumb "$out\mt6630-wlan-scan-followup.exe";if($LASTEXITCODE){throw 'BCBF signing failed'}
 $sig=Get-AuthenticodeSignature "$out\mt6630-wlan-scan-followup.exe";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Existing signer/trust mismatch'}
 $image=[IO.File]::ReadAllBytes("$out\mt6630-wlan-scan-followup.exe");$wide=[Text.Encoding]::Unicode.GetString($image)
 if(!$wide.Contains('C:\Windows\System32\MT8167CombinedFollowup124\logs\wlan-scan.txt')){throw 'Compiled fixed UTF16 log path mismatch'}
 $m=[ordered]@{schema='combined124-single-wlan-scan-v1';source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant();path_header_sha256=(Get-FileHash $header -Algorithm SHA256).Hash.ToLowerInvariant();exe='mt6630-wlan-scan-followup.exe';exe_sha256=(Get-FileHash "$out\mt6630-wlan-scan-followup.exe" -Algorithm SHA256).Hash.ToLowerInvariant();signer_thumbprint=$thumb;signature_status='Valid';one_shot=$true;arguments=@();interface_target='fixed MT6630 existing NetCfgInstanceId; exactly one match';completion_deadline_ms=30000;outer_timeout_ms=60000;ssid_bssid_guid_credentials_logged=$false;UART_used=$false;scanned_on_build_host=$false;device_verified=$false;headers=$KitHeaderRoot;arm_libs=$LibraryRoot;SDK_rebuilt=$false;ntddi='0x06030000'}
 $m|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
 & llvm-readobj --file-headers --coff-imports "$out\mt6630-wlan-scan-followup.exe"|Set-Content "$out\pe-imports.txt";if($LASTEXITCODE){throw 'PE import inspect failed'}
 'COMBINED124_WLAN_SCAN_SIGNED_NOT_RF_TESTED'
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
