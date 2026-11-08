param(
 [Parameter(Mandatory=$true)][string]$KitRoot,
 [Parameter(Mandatory=$true)][string]$UmInclude,
 [Parameter(Mandatory=$true)][string]$VcToolsRoot,
 [Parameter(Mandatory=$true)][string]$UcrtInclude,
 [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside project'}
$src=Join-Path $root 'drivers/gpu/mt8167/wddm-display'
New-Item -ItemType Directory -Force $out|Out-Null
$cl=Join-Path $VcToolsRoot 'bin/Hostx64/arm/cl.exe'
if(!(Test-Path $cl)){throw 'Use existing ARM MSVC; no SDK setup'}
& $cl /nologo /c /kernel /GS- /Zl /D_KERNEL_MODE /D_ARM_ /DARM /DNTDDI_VERSION=0x06030000 /D_WIN32_WINNT=0x0603 /DDXGKDDI_INTERFACE_VERSION=0x4002 /DA04_MT8167_DISPLAY_BRIGHTNESS=1 /DA04_GE8300_WDDM_DIRECT_EXPERIMENT=0 /DA04_GE8300_WDDM_SHARED_AUTOSTART=0 /DA04_GE8300_WDDM_VENDOR_TILE=0 /DA04_GE8300_WDDM_COPY=0 "/I$KitRoot/Include/km" "/I$KitRoot/Include/shared" "/I$UmInclude" "/I$VcToolsRoot/include" "/I$UcrtInclude" "/I$src" "/Fo$out/unified_display.obj" "$src/unified_display.cxx"
if($LASTEXITCODE){throw 'ARM unified bridge compile'}
@{schema='unified-display17-compile-v1';source_commit=(& git -C $root rev-parse HEAD);source_sha256=(Get-FileHash "$src/unified_display.cxx" -Algorithm SHA256).Hash.ToLowerInvariant();header_sha256=(Get-FileHash "$src/unified_display.h" -Algorithm SHA256).Hash.ToLowerInvariant();object_sha256=(Get-FileHash "$out/unified_display.obj" -Algorithm SHA256).Hash.ToLowerInvariant();brightness=1;registered=$false;linked=$false;hardware_verified=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$out/compile-receipt.json"
