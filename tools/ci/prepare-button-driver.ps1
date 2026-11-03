# Local diagnostics only: source files are supplied separately from the RT image.
param($InputDirectory = (Join-Path $env:RUNNER_TEMP 'a04br3-button-inject'))
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$package = Join-Path $InputDirectory 'msgpio-package'
New-Item -ItemType Directory -Force $package | Out-Null
$inf = [IO.File]::ReadAllText((Join-Path $InputDirectory 'msgpiowin32.inf'), [Text.Encoding]::Unicode)
if ($inf -notmatch '(?im)^\[Version\]\s*$' -or $inf -match '(?im)^CatalogFile\s*=') { throw 'Unexpected source INF version section' }
$inf = $inf -replace '(?im)^\[Version\]\s*$', "[Version]`r`nCatalogFile=msgpio-test.cat"
[IO.File]::WriteAllText((Join-Path $package 'msgpiowin32.inf'), $inf, [Text.Encoding]::Unicode)
Copy-Item (Join-Path $InputDirectory 'msgpiowin32.sys') $package
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Test catalog generation failed' }
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 WinPE Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'msgpio-test.cat')
if ($LASTEXITCODE -ne 0) { throw 'Test catalog signing failed' }
Write-Host "Experimental local-only package: $package"
