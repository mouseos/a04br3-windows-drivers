# Incremental 1TU client only. No SYS/INF/catalog/driver-control/deployment.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$IncludeRoot='C:\mt8167-minimal-service-host\cache\kit81\Include',
 [string]$LibraryRoot='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm',
 [string]$Signtool='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe',
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh task-local output required'}
New-Item -ItemType Directory "$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $source=Join-Path $root 'drivers\system\mt6392-rtc\rtc_clock_compare.c'
 $common=@('--target=thumbv7-pc-windows-msvc','-ffreestanding','-O2','-fno-builtin','-fno-stack-protector',
 '-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions','-std=c11',
 '-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000',
 '-isystem',"$IncludeRoot\um",'-isystem',"$IncludeRoot\shared",'-Wall','-Wextra','-Werror','-Wno-pragma-pack')
 & 'C:\Program Files\LLVM\bin\clang.exe' @common -c $source -o "$out\rtc_clock_compare.obj"
 if($LASTEXITCODE){throw 'ARM compile failed'}
 $exe=Join-Path $out 'mt6392-clock-compare.exe'
 & 'C:\Program Files\LLVM\bin\lld-link.exe' /machine:arm /subsystem:console,6.03 /entry:mainCRTStartup /nodefaultlib "$out\rtc_clock_compare.obj" "$LibraryRoot\kernel32.lib" "/out:$exe"
 if($LASTEXITCODE){throw 'ARM link failed'}
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
 if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer unavailable'}
 & $Signtool sign /fd SHA256 /sha1 $CertificateThumbprint /s My $exe
 if($LASTEXITCODE){throw 'Sign failed'}
 & $Signtool verify /pa /v $exe
 if($LASTEXITCODE){throw 'Verify failed'}
 $signature=Get-AuthenticodeSignature $exe
 if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Signer mismatch'}
 & 'C:\Program Files\LLVM\bin\llvm-readobj.exe' --file-headers --coff-imports $exe | Set-Content "$out\imports.txt"
 if($LASTEXITCODE){throw 'PE inspection failed'}
 $inputs=@($source,(Join-Path (Split-Path $source) 'rtc_ioctl.h'),(Join-Path (Split-Path $source) 'calendar.h'),$PSCommandPath,"$LibraryRoot\kernel32.lib")
 [ordered]@{schema='mt6392-clock-compare-client-v1';source_pin=(Get-Content (Join-Path $root 'source-pin.txt')).Trim();
   source_inputs=@($inputs|ForEach-Object{@{path=$_;sha256=(Get-FileHash $_ -Algorithm SHA256).Hash}});
   compiler=((& 'C:\Program Files\LLVM\bin\clang.exe' --version)-join "`n");flags=$common;
   signer=$CertificateThumbprint;device_tested=$false;clock_set_requests=0;existing_driver_unchanged=$true;
   outputs=@('mt6392-clock-compare.exe','rtc_clock_compare.obj','imports.txt'|ForEach-Object{@{path=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
 Compress-Archive -Path "$out\mt6392-clock-compare.exe","$out\rtc_clock_compare.obj","$out\imports.txt","$out\signed-manifest.json" -DestinationPath "$out\mt6392-clock-compare-signed.zip"
 Write-Host 'MT6392_CLOCK_COMPARE_SIGNED_NOT_DEVICE_TESTED'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
