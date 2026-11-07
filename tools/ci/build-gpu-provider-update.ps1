# Only the fixed GPU-mode DeviceInstall EXE and its pinned package. No SYS build,
# offline hive editing, Engine service change, deployment or runner operations.
param([Parameter(Mandatory=$true)][string]$CandidateDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory,
      [Parameter(Mandatory=$true)][string]$BaselineDirectory,
      [switch]$Sign,
      [string]$CertificateThumbprint='2655FC89A7082D3BAB3F26EC556EE8A3C52F4358')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'OutputDirectory must be new and inside this checkout'
}
& python -B (Join-Path $root 'tools\winpe\prepare-gpu-provider-policy.py') --candidate $CandidateDirectory --baseline $BaselineDirectory --output $out
if($LASTEXITCODE -ne 0){throw 'Real fixed GPU candidate is required; policy preparation failed'}
New-Item -ItemType Directory (Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
    $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
    $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
    $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
    $source=Join-Path $root 'tools\winpe\mt8167-device-install.c'
    & clang --target=thumbv7-pc-windows-msvc -O2 -fno-builtin -fno-stack-protector -w -fms-extensions -ffreestanding `
      -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DA04_GPU_PROVIDER_UPDATE=1 `
      "-I$inc\um" "-I$inc\shared" "-I$out" -c $source -o (Join-Path $out 'installer.obj')
    if($LASTEXITCODE -ne 0){throw 'GPU-mode installer ARM32 compile failed'}
    $exe=Join-Path $out 'MT8167DeviceInstall.exe'
    & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat `
      (Join-Path $out 'installer.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'advapi32.lib') `
      (Join-Path $lib 'setupapi.lib') (Join-Path $lib 'wintrust.lib') (Join-Path $lib 'bcrypt.lib') "/out:$exe"
    if($LASTEXITCODE -ne 0){throw 'GPU-mode installer ARM32 link failed'}
    if($Sign){
        $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
        if(!$cert.HasPrivateKey){throw 'Installer signing key is unavailable'}
        & (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $CertificateThumbprint /s My $exe
        if($LASTEXITCODE -ne 0){throw 'GPU-mode installer signing failed'}
    }
    $imports=@(& llvm-readobj --file-headers --coff-imports $exe)
    if($LASTEXITCODE -ne 0){throw 'Installer PE inspection failed'}
    $imports|Set-Content -Encoding UTF8 (Join-Path $out 'pe-imports.txt')
    $policy=Get-Content -Raw (Join-Path $out 'policy-manifest.json')|ConvertFrom-Json
    @{source_commit=(& git -C $root rev-parse HEAD);source_dirty=(@(& git -C $root status --porcelain).Count -ne 0);
      mode='fixed_gpu_provider_deferred_install';service='MT8167DeviceInstall';hardware_id='ACPI\MOU0006';
      candidate=$policy;signed=[bool]$Sign;exe_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash;
      policy_schema=2;data_directory=$policy.data_directory;data_directory_suffix=$policy.data_directory_suffix;
      transition_id=$policy.transition_id;baseline_sys_sha256=$policy.baseline_sys_sha256;baseline_driver_ver=$policy.baseline_driver_ver;
      actual_install_verified=$false;runtime_candidate_verified=$false;reboot_performed=$false;
      uart='optional';disklog='mandatory'}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 (Join-Path $out 'installer-manifest.json')
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
