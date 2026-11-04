$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include'
$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out=Join-Path $tempRoot 'mma8652-clean-check'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\check-volume-clean.c') -o (Join-Path $out 'clean.obj')
if ($LASTEXITCODE -ne 0) { throw 'Clean-check compile failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'clean.obj') (Join-Path $lib 'kernel32.lib') "/out:$out\check-volume-clean.exe"
if ($LASTEXITCODE -ne 0) { throw 'Clean-check link failed' }
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 Volume Clean Check Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $out 'check-volume-clean.cer') | Out-Null
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $out 'check-volume-clean.exe')
if ($LASTEXITCODE -ne 0) { throw 'Clean-check signing failed' }
Get-FileHash -Algorithm SHA256 (Join-Path $out 'check-volume-clean.exe')
Write-Host 'Test signature only; Windows RT acceptance and target filesystem checks unverified.'
