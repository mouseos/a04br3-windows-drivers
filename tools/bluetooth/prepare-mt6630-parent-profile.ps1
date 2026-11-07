# Repackage a verified existing BT parent SYS with a fixed versioned INF only.
# No compiler, BT SYS build, audio/client/USB build, signing or device access.
param([Parameter(Mandatory=$true)][string]$ReuseParentDirectory,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt6630-parent-profile'}
$dirty=@(& git -C $root status --porcelain)
if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Packaging requires a clean fixed source commit'}
& (Join-Path $root 'tools\ci\build-mt8167-first-playback.ps1') -ParentOnly -IntegratedUsb -BluetoothProfile `
  -ReuseParentDirectory $ReuseParentDirectory -OutputDirectory $OutputDirectory
