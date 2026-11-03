$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$out = Join-Path $env:RUNNER_TEMP 'a04br3-button-probe-service'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\button-probe-service.c" -o "$out\probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 service compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\probe.obj" (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'setupapi.lib') (Join-Path $lib 'advapi32.lib') "/out:$out\button-probe-service.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 service link failed' }
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 WinPE Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My "$out\button-probe-service.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 service signing failed' }
Write-Host "Experimental executable: $out\button-probe-service.exe"
