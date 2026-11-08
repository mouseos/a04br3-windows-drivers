# Compile the Microsoft-derived display-only code with the Windows 8.1 ABI.
# This build check is not an installable device driver or GPU acceleration.
param([string]$KitRoot,[string]$VcToolsRoot,[string]$UcrtIncludeRoot,[string]$BuildDirectory,[string]$ReuseReceipt,[string]$ReuseReceiptSha256)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$src = Join-Path $root 'drivers\gpu\mt8167\wddm-display'
$kit = if($KitRoot){$KitRoot}else{"${env:ProgramFiles(x86)}\Windows Kits\8.1"}
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
if($VcToolsRoot){$vc=Get-Item $VcToolsRoot}else{
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -property installationPath
if (-not $vs) { throw 'Visual Studio with ARM32 MSVC is required' }
$tools = Join-Path $vs 'VC\Tools\MSVC'
$vc = Get-ChildItem $tools -Directory | Sort-Object Name -Descending | Where-Object { Test-Path (Join-Path $_.FullName 'bin\Hostx64\arm\cl.exe') } | Select-Object -First 1
if (-not $vc) { throw 'ARM32 MSVC compiler not found' }
 }
$cl = Join-Path $vc.FullName 'bin\Hostx64\arm\cl.exe'
$link = Join-Path $vc.FullName 'bin\Hostx64\x64\link.exe'
if (-not (Test-Path $link)) { throw 'MSVC linker not found' }
if($UcrtIncludeRoot){$ucrt=Get-Item $UcrtIncludeRoot}else{
$ucrtRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
$ucrt = Get-ChildItem $ucrtRoot -Directory | Sort-Object Name -Descending | Where-Object { Test-Path (Join-Path $_.FullName 'ucrt\stddef.h') } | Select-Object -First 1
if (-not $ucrt) { throw 'Windows UCRT headers not found' }
 }
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = if($BuildDirectory){$BuildDirectory}else{Join-Path $tempRoot 'a04br3-wddm-display-build-check'}
New-Item -ItemType Directory -Force $out | Out-Null
$objects = @()
$brightness = if ($env:A04_MT8167_DISPLAY_BRIGHTNESS -eq '1') { '1' } else { '0' }
$sharedDiagnostic = if ($env:A04_GE8300_WDDM_SHARED_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$sharedFullCommon = if ($env:A04_GE8300_WDDM_SHARED_FULL_COMMON -eq '1') { '1' } else { '0' }
$commonFrame = if ($env:A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$idleDiagnostic = if ($env:A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC -eq '1') { '1' } else { '0' }
if ($idleDiagnostic -eq '1' -and $env:A04_GE8300_WDDM_LIVE_FULL_PRESENT -ne '1') { throw 'Idle transition diagnostic requires A04_GE8300_WDDM_LIVE_FULL_PRESENT' }
$presentTraceAll = if ($env:A04_GE8300_PRESENT_TRACE_ALL -eq '1') { '1' } else { '0' }
$liveFull = if ($env:A04_GE8300_WDDM_LIVE_FULL_PRESENT -eq '1') { '1' } else { '0' }
if ($liveFull -eq '1' -and ($env:A04_GE8300_WDDM_WIDE_REPEAT -ne '1' -or $env:A04_GE8300_WDDM_GPU_DIRECT_POST -ne '1')) { throw 'Live presents require repeat and direct POST' }
$wideRepeat = if ($env:A04_GE8300_WDDM_WIDE_REPEAT -eq '1') { '1' } else { '0' }
if ($wideRepeat -eq '1' -and $env:A04_GE8300_WDDM_WIDE_FRAME -ne '1') { throw 'Repeat requires wide frames' }
$wideFrame = if ($env:A04_GE8300_WDDM_WIDE_FRAME -eq '1') { '1' } else { '0' }
if ($wideFrame -eq '1' -and $commonFrame -ne '1') { throw 'Wide frame requires common frame support' }

$gceLease = if ($env:A04_GE8300_WDDM_GCE_LEASE -eq '1') { '1' } else { '0' }
$presentCapture = if ($env:A04_GE8300_WDDM_PRESENT_CAPTURE -eq '1') { '1' } else { '0' }
$explicitGpuFlush = if ($env:A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH -eq '1') { '1' } else { '0' }
$ovlObserve = if ($env:A04_GE8300_WDDM_OVL_OBSERVE -eq '1') { '1' } else { '0' }
$gpuPost = if ($env:A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$gpuDirectPost = if ($env:A04_GE8300_WDDM_GPU_DIRECT_POST -eq '1') { '1' } else { '0' }
if ($gpuDirectPost -eq '1' -and $gpuPost -ne '1') { throw 'GPU direct POST requires the GPU POST diagnostic' }
$sharedDisplayCommon = if ($env:A04_GE8300_SHARED_DISPLAY_COMMON -eq '1') { '1' } else { '0' }
$sharedCommonPage = if ($env:A04_GE8300_WDDM_SHARED_COMMON_PAGE -eq '1') { '1' } else { '0' }
$sharedPresent = if ($env:A04_GE8300_WDDM_SHARED_PRESENT -eq '1') { '1' } else { '0' }
if ($sharedPresent -eq '1' -and $sharedDiagnostic -ne '1') { throw 'Shared present requires shared diagnostic' }
$sharedAutostart = if ($env:A04_GE8300_WDDM_SHARED_AUTOSTART -eq '1') { '1' } else { '0' }
if ($sharedAutostart -eq '1' -and $sharedDiagnostic -ne '1') { throw 'Shared autostart requires shared diagnostic' }
$sharedLease = if ($env:A04_GE8300_WDDM_SHARED_LEASE -eq '1') { '1' } else { '0' }
$direct = if ($env:A04_GE8300_WDDM_DIRECT_EXPERIMENT -eq '1') { '1' } else { '0' }
$fencedTile = if ($env:A04_GE8300_WDDM_FENCED_TILE -eq '1') { '1' } else { '0' }
if ($sharedDiagnostic -eq '1' -and $sharedLease -ne '1') { throw 'Shared diagnostic requires shared lease' }
if ($sharedLease -eq '1' -and $direct -ne '1') { throw 'Shared lease requires direct resource experiment' }
$vendorTile = if ($env:A04_GE8300_WDDM_VENDOR_TILE -eq '1') { '1' } else { '0' }
if ($fencedTile -eq '1' -and $vendorTile -ne '1') { throw 'Fenced tile requires vendor tile' }
$gpuCopy = if ($env:A04_GE8300_WDDM_COPY -eq '1') { '1' } else { '0' }
# The receipt pins actual objects plus all external/project header inputs.
function A04Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function A04HeaderDigest {
    $files=@()
    foreach($dir in @($inc,(Join-Path $vc.FullName 'include'),(Join-Path $ucrt.FullName 'ucrt'),(Join-Path $root 'drivers'))){
        $files+=@(Get-ChildItem $dir -Recurse -File|Where-Object{$_.Extension -in @('.h','.hxx','.hpp','.inc')})
    }
    $lines=@($files|Sort-Object FullName|ForEach-Object{$_.FullName.ToLowerInvariant()+' '+(A04Sha $_.FullName)})
    $sha=[Security.Cryptography.SHA256]::Create()
    try{([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
}
$compileId='msvc-arm-kernel-gs0-zl-rt81-display-v1'
$headerDigest=A04HeaderDigest
$compilerDigest=A04Sha $cl
$effectiveFlags=@{}
foreach($match in [regex]::Matches([IO.File]::ReadAllText($PSCommandPath),'\$env:(A04_(?:GE8300_[A-Z0-9_]+|MT8167_DISPLAY_BRIGHTNESS))')){
    $key=$match.Groups[1].Value;$effectiveFlags[$key]=[int]([Environment]::GetEnvironmentVariable($key,'Process') -eq '1')
}
$baseline=$null;$objectRecords=@()
if($ReuseReceipt){
    if($ReuseReceiptSha256 -notmatch '^[a-f0-9]{64}$' -or (A04Sha $ReuseReceipt) -cne $ReuseReceiptSha256){throw 'Object receipt pin mismatch'}
    $baseline=Get-Content $ReuseReceipt -Raw|ConvertFrom-Json
    if($baseline.compile_id -cne $compileId -or $baseline.compiler_sha256 -cne $compilerDigest -or $baseline.header_digest -cne $headerDigest){throw 'Object compile/header inputs changed'}
    foreach($key in $effectiveFlags.Keys){if($baseline.effective_flags.$key -ne $effectiveFlags[$key]){throw "Object feature flags changed: $key"}}
    if($baseline.source_commit -notmatch '^[a-f0-9]{40}$'){throw 'Invalid baseline source'}
}
foreach ($name in @('bdd','bdd_ddi','bdd_dmm','bdd_util','bltfuncs','blthw','memory','ge8300_present','ge8300_trace','ge8300_display_resources','mt8167_brightness')) {
    $obj = Join-Path $out "$name.obj"
    $retained=$false
    $input=Join-Path $src "$name.cxx"
    if($baseline){
        $entry=@($baseline.objects|Where-Object{$_.name -ceq "$name.obj"})
        if($entry.Count -ne 1){throw 'Incomplete baseline object receipt'}
        if($entry[0].source_sha256 -ceq (A04Sha $input)){
            $old=Join-Path (Split-Path $ReuseReceipt) "$name.obj"
            if((A04Sha $old) -cne $entry[0].sha256){throw 'Retained object hash mismatch'}
            Copy-Item $old $obj;$retained=$true
        }
    }
    if(!$retained){
    & $cl /nologo /c /kernel /GS- /Zl /D_KERNEL_MODE /D_ARM_ /DARM /DNTDDI_VERSION=0x06030000 /D_WIN32_WINNT=0x0603 "/DA04_MT8167_DISPLAY_BRIGHTNESS=$brightness" "/DA04_GE8300_WDDM_PRESENT_CAPTURE=$presentCapture" "/DA04_GE8300_WDDM_EXPLICIT_GPU_FLUSH=$explicitGpuFlush" "/DA04_GE8300_WDDM_OVL_OBSERVE=$ovlObserve" "/DA04_GE8300_WDDM_GPU_POST_DIAGNOSTIC=$gpuPost" "/DA04_GE8300_WDDM_GPU_DIRECT_POST=$gpuDirectPost" "/DA04_GE8300_WDDM_GCE_LEASE=$gceLease" "/DA04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC=$commonFrame" "/DA04_GE8300_WDDM_WIDE_FRAME=$wideFrame" "/DA04_GE8300_WDDM_WIDE_REPEAT=$wideRepeat" "/DA04_GE8300_WDDM_LIVE_FULL_PRESENT=$liveFull" "/DA04_GE8300_PRESENT_TRACE_ALL=$presentTraceAll" "/DA04_GE8300_IDLE_TRANSITION_DIAGNOSTIC=$idleDiagnostic" "/DA04_GE8300_SHARED_DISPLAY_COMMON=$sharedDisplayCommon" "/DA04_GE8300_WDDM_SHARED_DIAGNOSTIC=$sharedDiagnostic" "/DA04_GE8300_WDDM_SHARED_AUTOSTART=$sharedAutostart" "/DA04_GE8300_WDDM_SHARED_PRESENT=$sharedPresent" "/DA04_GE8300_WDDM_SHARED_COMMON_PAGE=$sharedCommonPage" "/DA04_GE8300_WDDM_SHARED_FULL_COMMON=$sharedFullCommon" "/DA04_GE8300_WDDM_SHARED_LEASE=$sharedLease" "/DA04_GE8300_WDDM_DIRECT_EXPERIMENT=$direct" "/DA04_GE8300_WDDM_FENCED_TILE=$fencedTile" "/DA04_GE8300_WDDM_COPY=$gpuCopy" "/DA04_GE8300_WDDM_VENDOR_TILE=$vendorTile" "/I$inc\km" "/I$inc\shared" "/I$inc\um" "/I$($vc.FullName)\include" "/I$($ucrt.FullName)\ucrt" "/I$src" "/Fo$obj" (Join-Path $src "$name.cxx")
    if ($LASTEXITCODE -ne 0) { throw "ARM32 display source compilation failed: $name" }
    }
    $objects += $obj
    $objectRecords+=@{name="$name.obj";source_sha256=(A04Sha $input);sha256=(A04Sha $obj);retained=$retained;origin_source_commit=$(if($retained){$baseline.source_commit}else{(& git -C $root rev-parse HEAD)})}
}
@{source_commit=(& git -C $root rev-parse HEAD);compile_id=$compileId;compiler_sha256=$compilerDigest;header_digest=$headerDigest;effective_flags=$effectiveFlags;objects=$objectRecords}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\object-receipt.json"

if ($direct -eq '1') {
    foreach ($name in @('vendor_m1_11\windows_shared_scanout','vendor_m1_11\windows_shared_ovl','dma_pages','gpu_mmu')) {
        $obj = Join-Path $out "$($name.Replace('\','-')).obj"
        $gpu = Join-Path $root 'drivers\gpu\mt8167'
        & $cl /nologo /c /TC /std:c11 /kernel /GS- /Zl /D_KERNEL_MODE /D_ARM_ /DARM /DNTDDI_VERSION=0x06030000 /D_WIN32_WINNT=0x0603 "/I$inc\km" "/I$inc\shared" "/I$inc\um" "/I$($vc.FullName)\include" "/I$($ucrt.FullName)\ucrt" "/Fo$obj" (Join-Path $gpu "$name.c")
        if ($LASTEXITCODE -ne 0) { throw "ARM32 shared scanout compilation failed: $name" }
        $objects += $obj
    }
}
& $link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects (Join-Path $lib 'displib.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') (Join-Path $lib 'BufferOverflowGDI.lib') (Join-Path $lib 'libcntpr.lib') "/out:$out\wddm-display-build-check.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 display build-check link failed' }
Write-Host 'Display-only build check; no INF, no GPU acceleration, do not install.'

@{ status='compile-link-only-not-device-validated'; hardware='MediaTek MT8167 DISP_PWM0'; brightness_enabled=[int]$brightness; supported_percent='1..100'; required_firmware_flag='A04BR3_DISPLAY_BRIGHTNESS_RESOURCES'; brightness_source_sha256=(Get-FileHash (Join-Path $src 'mt8167_brightness.cxx') -Algorithm SHA256).Hash; sys_sha256=(Get-FileHash (Join-Path $out 'wddm-display-build-check.sys') -Algorithm SHA256).Hash } | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $out 'brightness-build-manifest.json')
