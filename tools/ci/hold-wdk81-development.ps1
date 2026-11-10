# Keep the same SSH VM through incremental builds and device validation.
$ErrorActionPreference='Stop'
$minutes=0
if(![int]::TryParse($env:KEEP_MINUTES,[ref]$minutes) -or $minutes -lt 1 -or $minutes -gt 330){throw 'KEEP_MINUTES must be 1..330'}
$until=[DateTime]::UtcNow.AddMinutes($minutes)
$statePath=Join-Path $env:RUNNER_TEMP 'mt8167-development-state.json'
$stop=Join-Path $env:GITHUB_WORKSPACE 'end-ssh'
$state=[ordered]@{status='setup';available_until_utc=$until.ToString('o');setup_succeeded=$false;setup_error=$null}
$state|ConvertTo-Json|Set-Content -Encoding UTF8 $statePath
try {
 if($env:A04_CACHED_TOOLCHAIN_ONLY -eq '1'){
  $state.setup_succeeded=$true;$state.status='ready-for-cache-restore'
 }else{
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
 if(!(Test-Path "$kit\Include\km\ntddk.h") -or !(Test-Path "$kit\Lib\winv6.3\km\arm\storport.lib")){
  & (Join-Path $PSScriptRoot 'install-wdk81.ps1')
 }
 if(!(Test-Path "$kit\Include\km\ntddk.h") -or !(Test-Path "$kit\Lib\winv6.3\km\arm\storport.lib")){throw 'Required WDK8.1 ARM files are missing'}
 $state.setup_succeeded=$true;$state.status='ready'
 }
} catch {
 $state.status='setup-failed-ssh-remains-open';$state.setup_error=$_.Exception.Message
}
$state|ConvertTo-Json|Set-Content -Encoding UTF8 $statePath
Write-Host $state.status
# Build completion does not end this interval. The primary owner may repair
# setup over SSH, rebuild, collect artifacts, flash and inspect the target.
while([DateTime]::UtcNow -lt $until -and !(Test-Path $stop)){Start-Sleep -Seconds 5}
$state.status=if(Test-Path $stop){'ended-by-owner'}else{'lifetime-ended'}
$state|ConvertTo-Json|Set-Content -Encoding UTF8 $statePath
if(!$state.setup_succeeded){exit 1}
