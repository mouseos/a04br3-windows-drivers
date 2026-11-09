param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path $out){throw 'New client output required'}
if(@(& git -C $root status --porcelain).Count){throw 'Clean source required'}
New-Item -ItemType Directory $out|Out-Null;$env:TMP=$out;$env:TEMP=$out
$kit='C:\mt8167-minimal-service-host\cache\kit81\Include';$lib='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm';$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$source="$root\tools\winpe\mt6630-bluetooth-status.c";$name='mt6630-bluetooth-status'
& clang --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$kit\um" "-I$kit\shared" -c $source -o "$out\$name.obj";if($LASTEXITCODE){throw 'BT status TU compile failed'}
& lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat "$out\$name.obj" "$lib\kernel32.lib" "$lib\setupapi.lib" "$lib\cfgmgr32.lib" "/out:$out\$name.exe";if($LASTEXITCODE){throw 'BT status link failed'}
& $sign sign /fd SHA256 /s My /sha1 $thumb "$out\$name.exe";if($LASTEXITCODE){throw 'BT status signing failed'}
$s=Get-AuthenticodeSignature "$out\$name.exe";if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw 'BT status signer mismatch'}
@{schema='mt6630-standard-bluetooth-status-wait35-v2';source_commit=(& git -C $root rev-parse HEAD);source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant();exe="$name.exe";exe_sha256=(Get-FileHash "$out\$name.exe" -Algorithm SHA256).Hash.ToLowerInvariant();directory='port/xnu/out/windows-rt-combined165-fresh164/bt-status-client';signer_thumbprint=$thumb;signature_status='Valid';arguments=@('--status');source_TUs=1;timeout_ms=45000;software_wait_ms=35000;max_samples=141;registration_execute=$false;hardware_verified=$false;device_verified=$false}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
Compress-Archive "$out\*" "$root\build\bt-status165.zip"
'BT_STATUS_SIGNED_NO_DEVICE_TEST'
