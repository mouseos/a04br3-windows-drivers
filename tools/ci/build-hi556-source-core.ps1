# Explicit ARM32 source-core build only: no SYS/INF, device or runner launch.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$IdentityReadExperiment)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)){throw 'Require a new output directory inside this checkout'}
$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or $commit -notmatch '^[0-9a-f]{40}$'){throw 'Fixed source required'}
$dirty=(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty){throw 'Clean committed source required'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
if(!(Test-Path "$kit\Include\km\ntddk.h")){throw 'WDK8.1 installation required'}
New-Item -ItemType Directory -Path $out,"$out\tmp"|Out-Null
$priorTemp=$env:TEMP;$priorTmp=$env:TMP;$priorRunner=$env:RUNNER_TEMP
$flag=[int][bool]$IdentityReadExperiment
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$env:TEMP
 & clang --target=thumbv7-pc-windows-msvc -std=c11 -O2 -ffreestanding -fno-stack-protector -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -Wall -Wextra -Werror "-DA04_HI556_IDENTITY_EXPERIMENT=$flag" -c "$root\drivers\camera\hi556\hi556_identity.c" -o "$out\hi556_identity.obj"
 if($LASTEXITCODE){throw 'ARM32 identity core compilation failed'}
 & lld-link /lib /machine:arm "/out:$out\hi556_identity.lib" "$out\hi556_identity.obj"
 if($LASTEXITCODE){throw 'Source-core archive failed'}
} finally {$env:TEMP=$priorTemp;$env:TMP=$priorTmp;$env:RUNNER_TEMP=$priorRunner}
[ordered]@{
 status='source-core-only-no-windows-owner-or-camera-driver';source_commit=$commit;source_dirty=$false
 model='Hynix HI556';i2c_bus=1;i2c_address_7bit=40;chip_id=1366
 identity_read_experiment=$flag;default_enabled=$false;hardware_verified=$false;bindable=$false
 vendor_source='7c4782127d4ea11965be00ce1343056be9bfb769'
 board_linux_source='3d2c74e252d6f34a5980deda96bdc04064108d74'
 source_sha256=(Get-FileHash "$root\drivers\camera\hi556\hi556_identity.c" -Algorithm SHA256).Hash
 header_sha256=(Get-FileHash "$root\drivers\camera\hi556\hi556_identity.h" -Algorithm SHA256).Hash
 recipe_sha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash
 object_sha256=(Get-FileHash "$out\hi556_identity.obj" -Algorithm SHA256).Hash
 archive_sha256=(Get-FileHash "$out\hi556_identity.lib" -Algorithm SHA256).Hash
 kernel_codegen_flags=@('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none')
 blockers=@('Common I2C1 owner with MMA; Windows power/MCLK lease and bounded I/O provider absent','No KS/AVStream/CSI DMA implementation','Front camera identity unknown; no second HI556 binding','No OTP/calibration reader or fabricated defaults')
}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$out\manifest.json"
