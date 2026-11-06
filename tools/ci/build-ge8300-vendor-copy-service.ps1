# WinPE-only service and signed test package. Use an isolated runner directory.
param([Parameter(Mandatory=$true)][string]$Firmware,
      [Parameter(Mandatory=$true)][string]$CopyUsc,
      [Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
if(Test-Path $WorkDirectory){throw 'work directory already exists'}
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
$env:RUNNER_TEMP=(Resolve-Path $WorkDirectory).Path
$env:A04_GE8300_FIRMWARE=(Resolve-Path $Firmware).Path
$env:A04_GE8300_COPY_USC=(Resolve-Path $CopyUsc).Path
foreach($name in @('A04_GE8300_BOOT_TEST','A04_GE8300_ALLOW_MISMATCH','A04_GE8300_CLEAR_TEST','A04_GE8300_COPY_TEST','A04_GE8300_LINEAR_TEST','A04_GE8300_COPY_SERVICE','A04_GE8300_SHARED_TRACE_DIAGNOSTIC','A04_GE8300_RENDER_EXPERIMENT','A04_GE8300_RENDER_DIAGNOSTIC','A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC','A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC','A04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC','A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC','A04_GE8300_VENDOR_TILE_DIAGNOSTIC','A04_GE8300_VENDOR_COLOR_DIAGNOSTIC')){[Environment]::SetEnvironmentVariable($name,'0','Process')}
foreach($name in @('A04_GE8300_VENDOR_STAGE','A04_GE8300_VENDOR_UNCACHED_STAGE','A04_GE8300_VENDOR_IRQ','A04_GE8300_VENDOR_START_DIAGNOSTIC','A04_GE8300_VENDOR_RENDER_COLD','A04_GE8300_VENDOR_COPY_DIAGNOSTIC','A04_GE8300_VENDOR_COPY_SERVICE')){[Environment]::SetEnvironmentVariable($name,'1','Process')}
& "$PSScriptRoot\build-ge8300-provider.ps1"
& "$PSScriptRoot\build-ge8300-status.ps1"
& "$PSScriptRoot\build-ge8300-vendor-copy-test.ps1"
& "$PSScriptRoot\sign-ge8300-test.ps1"
$pkg=Join-Path $env:RUNNER_TEMP 'mt8167-ge8300-test-signed'
$exe=Join-Path $pkg 'ge8300-vendor-copy-test.exe'
Copy-Item (Join-Path $env:RUNNER_TEMP 'ge8300-vendor-copy-test\ge8300-vendor-copy-test.exe') $exe
$cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2((Join-Path $pkg 'ge8300-test.cer'))
$sign="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe"
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $exe
if($LASTEXITCODE){throw 'copy test EXE signing failed'}
Compress-Archive -Path "$pkg\*" -DestinationPath (Join-Path $env:RUNNER_TEMP 'signed.zip')
Get-FileHash (Join-Path $env:RUNNER_TEMP 'signed.zip') -Algorithm SHA256
Write-Host 'TEST ONLY: signature acceptance, copy, cache and surprise removal are unverified.'
