$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04br3-volume-guard'
New-Item -ItemType Directory -Force -Path $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\guard-windows-volume.c') -o (Join-Path $out 'guard.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM32 guard compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'guard.obj') (Join-Path $lib 'kernel32.lib') "/out:$out\guard-windows-volume.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 guard link failed' }
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 WinPE Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $out 'guard-windows-volume.exe')
if ($LASTEXITCODE -ne 0) { throw 'ARM32 guard signing failed' }
Write-Host "Experimental executable: $out\guard-windows-volume.exe"
