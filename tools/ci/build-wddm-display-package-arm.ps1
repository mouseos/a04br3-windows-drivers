# Build-only package. Do not install without validated PnP, power and recovery.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
& (Join-Path $PSScriptRoot 'build-wddm-display-arm.ps1')
$build = Join-Path $tempRoot 'a04br3-wddm-display-build-check'
$package = Join-Path $tempRoot 'mt8167-display-package'
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item (Join-Path $build 'wddm-display-build-check.sys') (Join-Path $package 'mt8167-display.sys') -Force
Copy-Item (Join-Path $root 'drivers\gpu\mt8167\wddm-display\mt8167-display.inf') $package -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'MT8167 display catalog generation failed' }
Write-Host 'Build-only MT8167 display package; not tested on hardware or approved for installation.'
