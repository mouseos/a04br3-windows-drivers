param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\build\msdc-boot-write-performance'),
    [switch]$SingleSectorBaseline,
    [switch]$Sign,
    [string]$CertificateThumbprint = '2655FC89A7082D3BAB3F26EC556EE8A3C52F4358',
    [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateLocation = 'CurrentUser'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$required = @('Include\km\ntddk.h','Include\km\storport.h','Include\um\windows.h',
              'Lib\winv6.3\km\arm\storport.lib','Lib\winv6.3\km\arm\ntoskrnl.lib',
              'Lib\winv6.3\km\arm\hal.lib','Lib\winv6.3\um\arm\kernel32.lib','bin\x86\Inf2Cat.exe')
foreach ($name in $required) { if (-not (Test-Path (Join-Path $kit $name))) { throw "Missing WDK8.1 ARM component: $name" } }
$commit = & git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify source commit' }
& git -C $root merge-base --is-ancestor 6b5043e98c853ca6cb4af31f747732198183a933 HEAD
if ($LASTEXITCODE -ne 0) { throw 'Source must contain the reviewed 6b5043e write batching change' }
& git -C $root diff --quiet HEAD -- drivers/storage/mt8167-msdc tools/ci/build-wdk81-probe.ps1 tools/ci/build-msdc-boot-write-performance.ps1 tools/winpe/msdc-file-verify.c
if ($LASTEXITCODE -ne 0) { throw 'Build inputs differ from the fixed source commit' }
if (Test-Path $OutputDirectory) { throw 'Choose a new output directory; do not mix previous artifacts' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$out = (Resolve-Path $OutputDirectory).Path
$work = Join-Path $out 'work'
New-Item -ItemType Directory -Path $work | Out-Null
$oldA04 = @{}
Get-ChildItem Env:A04_* | ForEach-Object { $oldA04[$_.Name] = $_.Value }
$oldTemp = @{ RUNNER_TEMP=$env:RUNNER_TEMP; TEMP=$env:TEMP; TMP=$env:TMP }
# Exact successful isolated passive boot family; exclude inherited scratch/PIO/reset experiments.
$flags = [ordered]@{
    A04_BOOT_EXPERIMENT='1'; A04_BOOT_PASSIVE_EXPERIMENT='1'; A04_PASSIVE_IO='1'
    A04_ENABLE_WRITES='1'; A04_READONLY_DISK_PROBE='1'; A04_DMA_PROBE='1'
    A04_DMA_DISK_PROBE='1'; A04_VIRTUAL_BOUNCE_EXPERIMENT='1'
    A04_SERIAL_QUEUE_EXPERIMENT='1'; A04_ISOLATED_REQUEST_EXPERIMENT='1'
    A04_EMMC_IDENTITY_HANDOFF='1'
    # Three performance settings, recorded explicitly rather than inferred from the branch.
    A04_BOOT_MULTIBLOCK_EXPERIMENT=$(if ($SingleSectorBaseline) { '0' } else { '1' })
    A04_DMA_WRITE_MULTIBLOCK_EXPERIMENT=$(if ($SingleSectorBaseline) { '0' } else { '1' })
    A04_DMA_POLL_TRACE='0'
}
try {
    Get-ChildItem Env:A04_* | ForEach-Object { Remove-Item -LiteralPath ("Env:" + $_.Name) }
    foreach ($name in $flags.Keys) { [Environment]::SetEnvironmentVariable($name,$flags[$name],'Process') }
    $env:RUNNER_TEMP=$work; $env:TEMP=$work; $env:TMP=$work
    # Build only storage. No SDK setup, runner lifecycle, device access or unrelated drivers.
    & (Join-Path $PSScriptRoot 'build-wdk81-probe.ps1')
    $package = Join-Path $out 'package'
    Copy-Item (Join-Path $work 'a04br3-wdk81-probe\package') $package -Recurse
    $verifyObject = Join-Path $out 'msdc-file-verify.obj'
    $verifyExe = Join-Path $out 'msdc-file-verify.exe'
    & clang --target=thumbv7-pc-windows-msvc -O2 -fno-builtin -fno-stack-protector -ffreestanding -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$kit\Include\um" "-I$kit\Include\shared" -c (Join-Path $root 'tools\winpe\msdc-file-verify.c') -o $verifyObject
    if ($LASTEXITCODE -ne 0) { throw 'ARM32 filesystem verifier compilation failed' }
    & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat $verifyObject (Join-Path $kit 'Lib\winv6.3\um\arm\kernel32.lib') "/out:$verifyExe"
    if ($LASTEXITCODE -ne 0) { throw 'ARM32 filesystem verifier link failed' }
    if ($Sign) {
        $cert = Get-Item "Cert:\$CertificateLocation\My\$CertificateThumbprint"
        if (-not $cert.HasPrivateKey) { throw 'Signing certificate has no private key' }
        $signArgs = @('sign','/fd','SHA256','/sha1',$CertificateThumbprint,'/s','My')
        if ($CertificateLocation -eq 'LocalMachine') { $signArgs += '/sm' }
        $signTool = Join-Path $kit 'bin\x86\signtool.exe'
        & $signTool @signArgs (Join-Path $package 'mt8167-msdc.sys')
        if ($LASTEXITCODE -ne 0) { throw 'Storage SYS signing failed' }
        # Rebuild the catalog after SYS signing, then sign the new catalog.
        & (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
        if ($LASTEXITCODE -ne 0) { throw 'Storage catalog regeneration failed' }
        & $signTool @signArgs (Join-Path $package 'mt8167-msdc.cat')
        if ($LASTEXITCODE -ne 0) { throw 'Storage catalog signing failed' }
        & $signTool @signArgs $verifyExe
        if ($LASTEXITCODE -ne 0) { throw 'Filesystem verifier signing failed' }
        Export-Certificate -Cert $cert -FilePath (Join-Path $package 'mt8167-development.cer') -Type CERT | Out-Null
    }
    $hashes = [ordered]@{}
    Get-ChildItem $package -File | Sort-Object Name | ForEach-Object { $hashes['package/' + $_.Name] = (Get-FileHash $_.FullName -Algorithm SHA256).Hash }
    $hashes['msdc-file-verify.exe'] = (Get-FileHash $verifyExe -Algorithm SHA256).Hash
    $sourceHashes = [ordered]@{}
    Get-ChildItem (Join-Path $root 'drivers\storage\mt8167-msdc') -File | Where-Object { $_.Extension -in @('.c','.h','.inf') } | Sort-Object Name | ForEach-Object { $sourceHashes[$_.Name] = (Get-FileHash $_.FullName -Algorithm SHA256).Hash }
    [ordered]@{
        source_commit=$commit; source_hashes=$sourceHashes; build_flags=$flags
        baseline_configuration='isolated passive boot used by 2371bd0 (not a bit-identical old binary)'
        target='WDK8.1 ARM32 Windows RT 8.1'; signed=[bool]$Sign
        signing_thumbprint=$(if ($Sign) { $CertificateThumbprint } else { $null })
        sha256=$hashes; hardware_verified=$false; contents_verified_on_device=$false
    } | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
    Write-Host "Storage package: $package"
    Write-Host "Filesystem verifier: $verifyExe"
} finally {
    Get-ChildItem Env:A04_* | ForEach-Object { Remove-Item -LiteralPath ("Env:" + $_.Name) }
    foreach ($name in $oldA04.Keys) { [Environment]::SetEnvironmentVariable($name,$oldA04[$name],'Process') }
    foreach ($name in $oldTemp.Keys) { [Environment]::SetEnvironmentVariable($name,$oldTemp[$name],'Process') }
}
