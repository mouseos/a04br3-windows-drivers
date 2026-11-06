# Build a paired experimental shared-MDL diagnostic. Does not install or flash.
param([Parameter(Mandatory=$true)][string]$Firmware,
      [Parameter(Mandatory=$true)][string]$CopyUsc,
      [Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
if(Test-Path $WorkDirectory){throw 'work directory already exists'}
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
$env:RUNNER_TEMP=(Resolve-Path $WorkDirectory).Path
$env:A04_GE8300_FIRMWARE=(Resolve-Path $Firmware).Path
$env:A04_GE8300_COPY_USC=(Resolve-Path $CopyUsc).Path
foreach($name in @('A04_GE8300_BOOT_TEST','A04_GE8300_ALLOW_MISMATCH','A04_GE8300_COPY_SERVICE','A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC','A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC','A04_GE8300_VENDOR_TILE_DIAGNOSTIC','A04_GE8300_WDDM_VENDOR_TILE','A04_GE8300_WDDM_FENCED_TILE','A04_GE8300_WDDM_COPY','A04_GE8300_VENDOR_FENCED_COPY','A04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC','A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC','A04_GE8300_VENDOR_COLOR_DIAGNOSTIC','A04_GE8300_RENDER_EXPERIMENT','A04_GE8300_RENDER_DIAGNOSTIC','A04_GE8300_SHARED_TRACE_DIAGNOSTIC','A04_GE8300_CLEAR_TEST','A04_GE8300_COPY_TEST','A04_GE8300_LINEAR_TEST')){[Environment]::SetEnvironmentVariable($name,'0','Process')}
foreach($name in @('A04_GE8300_VENDOR_STAGE','A04_GE8300_VENDOR_UNCACHED_STAGE','A04_GE8300_VENDOR_IRQ','A04_GE8300_VENDOR_START_DIAGNOSTIC','A04_GE8300_VENDOR_RENDER_COLD','A04_GE8300_VENDOR_COPY_DIAGNOSTIC','A04_GE8300_VENDOR_COPY_SERVICE','A04_GE8300_SHARED_LEASE','A04_GE8300_SHARED_MMU_PUBLISH','A04_GE8300_SHARED_GPU_DIAGNOSTIC','A04_GE8300_WDDM_DIRECT_EXPERIMENT','A04_GE8300_WDDM_SHARED_LEASE','A04_GE8300_WDDM_SHARED_DIAGNOSTIC','A04_GE8300_WDDM_SHARED_AUTOSTART')){[Environment]::SetEnvironmentVariable($name,'1','Process')}
& "$PSScriptRoot\build-ge8300-provider.ps1"
& "$PSScriptRoot\build-ge8300-status.ps1"
& "$PSScriptRoot\build-ge8300-vendor-copy-test.ps1"
& "$PSScriptRoot\build-wddm-display-package-arm.ps1"
& "$PSScriptRoot\sign-ge8300-display-test.ps1"
$package=Join-Path $env:RUNNER_TEMP 'mt8167-ge8300-display-test-signed'
$copyExe=Join-Path $package 'ge8300\ge8300-vendor-copy-test.exe'
Copy-Item (Join-Path $env:RUNNER_TEMP 'ge8300-vendor-copy-test\ge8300-vendor-copy-test.exe') $copyExe
$cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2((Join-Path $package 'mt8167-display-test.cer'))
$sign="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe"
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $copyExe
if($LASTEXITCODE){throw 'copy test EXE signing failed'}
Compress-Archive -Path "$package\*" -DestinationPath (Join-Path $env:RUNNER_TEMP 'shared-diagnostic-signed.zip')
Get-FileHash (Join-Path $env:RUNNER_TEMP 'shared-diagnostic-signed.zip') -Algorithm SHA256
Write-Host 'TEST ONLY: no OVL switch, no cache/bounce or DMA-stop proof. Verify firmware and USC inputs and preserve UART.'
