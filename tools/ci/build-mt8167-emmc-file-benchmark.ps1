[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New output under this checkout required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
New-Item -ItemType Directory $out|Out-Null
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$source=Join-Path $root 'tools\winpe\mt8167-emmc-file-benchmark.c'
$flags=@('-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mfpu=none')
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions @flags -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c $source -o "$out\client.obj"
if($LASTEXITCODE){throw 'ARM32 client compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\client.obj" "$lib\kernel32.lib" "/out:$out\mt8167-emmc-file-benchmark.exe"
if($LASTEXITCODE){throw 'ARM32 client link failed'}
$unsigned=(Get-FileHash "$out\mt8167-emmc-file-benchmark.exe" -Algorithm SHA256).Hash
$sign=Join-Path $kit 'bin\x86\signtool.exe';$cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signing key unavailable'}
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My "$out\mt8167-emmc-file-benchmark.exe"
if($LASTEXITCODE){throw 'EXE signing failed'}
& $sign verify /pa /v "$out\mt8167-emmc-file-benchmark.exe"
if($LASTEXITCODE -or (Get-AuthenticodeSignature "$out\mt8167-emmc-file-benchmark.exe").SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw 'Signature validation failed'}
$pe=& llvm-readobj --file-headers --coff-imports "$out\mt8167-emmc-file-benchmark.exe"
if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
$pe|Set-Content -Encoding UTF8 "$out\pe-metadata.txt"
Export-Certificate -Cert $cert -FilePath "$out\development.cer"|Out-Null
@{source_commit=$commit;source_dirty=$false;source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;
 build_script_sha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash;codegen_flags=$flags;
 entry='mainCRTStartup';libraries=@('WDK8.1 um/arm/kernel32.lib');arguments=@('--bench');
 fixed_file='C:\MT8167MsdcFileBenchmark\benchmark.bin';file_bytes=4194304;chunk_bytes=4096;pattern_passes=1;
 clock='GetTickCount milliseconds';phase_check_budget_ms=120000;synchronous_io_hard_timeout_guaranteed=$false;
 no_buffering=$true;write_through=$true;explicit_flush=$true;create_new_only=$true;delete_only_own_created_file=$true;
 raw_disk_access=$false;hardware_verified=$false;signer=$cert.Thumbprint;unsigned_exe_sha256=$unsigned;
 files=@('mt8167-emmc-file-benchmark.exe','development.cer','pe-metadata.txt'|ForEach-Object{@{path=$_;sha256=(Get-FileHash "$out\$_" -Algorithm SHA256).Hash}})
}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$out\manifest.json"
Compress-Archive -Path "$out\mt8167-emmc-file-benchmark.exe","$out\development.cer","$out\pe-metadata.txt","$out\manifest.json" -DestinationPath "$out\mt8167-emmc-file-benchmark-signed.zip"
'MT8167_EMMC_FILE_BENCHMARK_SIGNED_COMPLETE'
