# 固定機能の候補を生成する。配備・証明書生成・runner起動は行わない。
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$Sign,
    [ValidatePattern('^[A-Fa-f0-9]{40}$')]
    [string]$CertificateThumbprint = '2655FC89A7082D3BAB3F26EC556EE8A3C52F4358',
    [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateStore = 'CurrentUser'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Invoke-SourceGit([string[]]$Arguments) {
    $value = & git.exe -C $root @Arguments
    if ($LASTEXITCODE -ne 0) { throw "git failed: $Arguments" }
    return $value
}
function Hash([string]$Path) { return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }
$commit = Invoke-SourceGit @('rev-parse','HEAD')
if (Invoke-SourceGit @('status','--porcelain','--untracked-files=normal')) { throw 'A clean committed checkout is required' }
$baseline = 'd6451d58ca82a4cbc888feaf1b716ce8f3398bc9'
$runtime = @('wdf_toprgu.c','reset_command.c','reset_command.h','watchdog_policy.h','handoff_policy.h','thermal_stop.h','uart_screen.h')
$prefix = 'drivers/system/mt8167-toprgu/'
foreach ($name in $runtime) {
    $path = $prefix + $name
    if ((Invoke-SourceGit @('rev-parse',"${baseline}:$path")) -ne (Invoke-SourceGit @('rev-parse',"${commit}:$path"))) {
        throw "Runtime differs from four-CPU reference: $path"
    }
}
$out = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $out) { throw 'OutputDirectory must be new' }
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inf2cat = Join-Path $kit 'bin\x86\Inf2Cat.exe'
$signtool = Join-Path $kit 'bin\x86\signtool.exe'
$cert = $null
if ($Sign) {
    $cert = Get-Item "Cert:\$CertificateStore\My\$CertificateThumbprint"
    if (!$cert.HasPrivateKey -or $cert.NotBefore -gt [datetime]'2014-01-18' -or
        $cert.NotAfter -lt (Get-Date)) { throw 'Certificate must have a private key and cover RTC 2014 and today' }
}
$flags = [ordered]@{
    A04_TOPRGU_CPU1_HANDOFF = '1'
    A04_TOPRGU_UART_SCREEN = '1'
    A04_TOPRGU_THERMAL_STOP = '0'
    A04_TOPRGU_DEADLINE_SECONDS = '0'
}
$sources = @()
$inputs = @($runtime | ForEach-Object { $prefix + $_ }) + @(
    ($prefix + 'mt8167-toprgu.inf'), 'tools/ci/build-mt8167-toprgu.ps1',
    'tools/ci/build-toprgu-normal-uart.ps1')
foreach ($path in $inputs) {
    $sources += [ordered]@{path=$path; git_blob=(Invoke-SourceGit @('rev-parse',"${commit}:$path")); sha256=(Hash (Join-Path $root $path))}
}
# 継承されたTOPRGU設定を除去し、終了時は元へ戻す。
$saved = @{}
foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'A04_TOPRGU_*' -or $_.Name -in @('RUNNER_TEMP','TEMP','TMP') })) {
    $saved[$item.Name] = $item.Value
}
New-Item -ItemType Directory -Path $out | Out-Null
$work = Join-Path $out 'work'
$package = Join-Path $out 'package'
New-Item -ItemType Directory -Path $work,$package | Out-Null
try {
    foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'A04_TOPRGU_*' })) { Remove-Item "Env:$($item.Name)" }
    foreach ($key in $flags.Keys) { Set-Item "Env:$key" $flags[$key] }
    foreach ($key in @('RUNNER_TEMP','TEMP','TMP')) { Set-Item "Env:$key" $work }
    & (Join-Path $PSScriptRoot 'build-mt8167-toprgu.ps1')
    foreach ($name in @('mt8167-toprgu.sys','mt8167-toprgu.inf')) {
        Copy-Item (Join-Path $work "mt8167-toprgu\$name") (Join-Path $package $name)
    }
    # 旧normalの0.0.10.0からrankを下げない。元INFの他の設定は保持。
    $inf = Join-Path $package 'mt8167-toprgu.inf'
    $text = [IO.File]::ReadAllText($inf)
    if ([regex]::Matches($text,'(?m)^DriverVer=[^\r\n]+').Count -ne 1) { throw 'Unexpected INF DriverVer' }
    [IO.File]::WriteAllText($inf,([regex]::Replace($text,'(?m)^DriverVer=[^\r\n]+','DriverVer=11/06/2026,0.0.11.0')),[Text.Encoding]::ASCII)
    if ($Sign) {
        $storeArgs = @('/s','My','/sha1',$CertificateThumbprint)
        if ($CertificateStore -eq 'LocalMachine') { $storeArgs += '/sm' }
        & $signtool sign /fd SHA256 @storeArgs (Join-Path $package 'mt8167-toprgu.sys')
        if ($LASTEXITCODE -ne 0) { throw 'SYS signing failed' }
    }
    & $inf2cat "/driver:$package" /os:6_3_ARM /uselocaltime
    if ($LASTEXITCODE -ne 0) { throw 'Catalog generation failed' }
    if ($Sign) {
        & $signtool sign /fd SHA256 @storeArgs (Join-Path $package 'mt8167-toprgu.cat')
        if ($LASTEXITCODE -ne 0) { throw 'CAT signing failed' }
        Export-Certificate -Cert $cert -FilePath (Join-Path $out 'development-public.cer') | Out-Null
    }
    if (Invoke-SourceGit (@('diff','--name-only','HEAD','--') + $inputs)) { throw 'Inputs changed during build' }
    foreach ($source in $sources) {
        if ((Hash (Join-Path $root $source.path)) -ne $source.sha256) { throw 'Input bytes changed during build' }
    }
    $files = @(Get-ChildItem -LiteralPath $package -File | Sort-Object Name | ForEach-Object {
        [ordered]@{path=('package/' + $_.Name); bytes=$_.Length; sha256=(Hash $_.FullName)}
    })
    $manifest = [ordered]@{
        schema_version=1; status='build-candidate-not-device-validated'; source_commit=$commit
        normal_runtime_reference=$baseline
        winpe_uart_reference='7109a9009b0a755532d2aa4058d8e3c2620cd61a'
        critical_flags=$flags; required_active_processors=4; ownership_cpu=1
        target='thumbv7-pc-windows-msvc'; wdk='8.1'; kmdf='1.13'
        compile_options=@('-w','-fms-extensions','-fno-stack-protector')
        compile_defines=@('_KERNEL_MODE','_ARM_','ARM','KMDF_VERSION_MAJOR=1','KMDF_VERSION_MINOR=13','NTDDI_VERSION=0x06030000','_WIN32_WINNT=0x0603')
        link_options=@('/driver','/subsystem:native','/machine:arm','/entry:FxDriverEntry','/nodefaultlib')
        link_libraries=@('wdfdriverentry.lib','wdfldr.lib','ntoskrnl.lib','hal.lib','bufferoverflowfastfailk.lib')
        clang_version=(@(& clang --version) -join "`n")
        linker_version=(@(& lld-link --version) -join "`n")
        driver_version='11/06/2026,0.0.11.0'; catalog_os='6_3_ARM'
        source_files=$sources; package_files=$files
        signed=[bool]$Sign; certificate=$null
        preservation=[ordered]@{runtime_blobs_equal_normal_reference=$true; reset_parser=$true; power_transition_watchdog_save_rearm=$true; deadline_disables_watchdog=$false; thermal_halt_interface=$false}
        device_validation='Not run; sleep/resume, installed binary provenance and EnginePcm coexistence require owner validation'
    }
    if ($Sign) {
        $manifest.certificate = [ordered]@{thumbprint=$cert.Thumbprint; subject=$cert.Subject; not_before=$cert.NotBefore.ToUniversalTime().ToString('o'); not_after=$cert.NotAfter.ToUniversalTime().ToString('o'); public_cer_sha256=(Hash (Join-Path $out 'development-public.cer')); timestamped=$false}
    }
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
    Write-Host "CANDIDATE=$out"
} finally {
    foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'A04_TOPRGU_*' -or $_.Name -in @('RUNNER_TEMP','TEMP','TMP') })) { Remove-Item "Env:$($item.Name)" }
    foreach ($key in $saved.Keys) { Set-Item "Env:$key" $saved[$key] }
}
