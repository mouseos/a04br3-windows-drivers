param([Parameter(Mandatory=$true)][string]$Certificate)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm'
$out = Join-Path $env:TEMP 'mt8167-copy-backup'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\copy-backup.c') -o "$out\copy.obj"
if ($LASTEXITCODE) { throw 'ARM backup helper compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\copy.obj" "$lib\kernel32.lib" "/out:$out\copy-backup.exe"
if ($LASTEXITCODE) { throw 'ARM backup helper link failed' }
$cert = Get-PfxCertificate $Certificate
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My "$out\copy-backup.exe"
if ($LASTEXITCODE) { throw 'Backup helper signing failed' }
Get-FileHash -Algorithm SHA256 "$out\copy-backup.exe" | Format-List
