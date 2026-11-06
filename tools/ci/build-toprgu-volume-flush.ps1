$ErrorActionPreference='Stop'
if (!$env:RUNNER_TEMP) { throw 'RUNNER_TEMP is required' }
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$out=Join-Path $env:RUNNER_TEMP 'toprgu-volume-flush'
if (Test-Path $out) { throw 'Output already exists' }
New-Item -ItemType Directory $out | Out-Null
$inc=Join-Path $kit 'Include'
$um=Join-Path $kit 'Lib\winv6.3\um\arm'
& clang --target=thumbv7-pc-windows-msvc -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\flush-toprgu-volume.c" -o "$out\flush.obj"
if($LASTEXITCODE) { throw 'ARM compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\flush.obj" "$um\kernel32.lib" "$um\advapi32.lib" "/out:$out\flush-toprgu-volume.exe"
if($LASTEXITCODE) { throw 'ARM link failed' }
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 TOPRGU Volume Flush Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath "$out\flush-test.cer" | Out-Null
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My "$out\flush-toprgu-volume.exe"
if($LASTEXITCODE) { throw 'test signing failed' }
Write-Host "TEST_CERT_THUMBPRINT=$($cert.Thumbprint)"
& llvm-readobj --file-headers --coff-imports "$out\flush-toprgu-volume.exe"
if($LASTEXITCODE) { throw 'PE inspection failed' }
Get-FileHash -Algorithm SHA256 "$out\flush-toprgu-volume.exe"
