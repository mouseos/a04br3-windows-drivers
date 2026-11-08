# Keep a cache-only development host available without installing a toolchain.
$ErrorActionPreference='Stop'
$minutes=0
if(![int]::TryParse($env:KEEP_MINUTES,[ref]$minutes) -or $minutes -lt 1 -or $minutes -gt 330){throw 'KEEP_MINUTES must be 1..330'}
$until=[DateTime]::UtcNow.AddMinutes($minutes)
$statePath=Join-Path $env:RUNNER_TEMP 'mt8167-development-state.json'
$stop=Join-Path $env:GITHUB_WORKSPACE 'end-ssh'
$state=[ordered]@{status='awaiting-cache-restore';mode='cache-only';available_until_utc=$until.ToString('o');setup_succeeded=$false;setup_performed=$false}
$state|ConvertTo-Json|Set-Content -Encoding UTF8 $statePath
Write-Host 'Cache-only SSH host waiting for owner-managed restore.'
while([DateTime]::UtcNow -lt $until -and !(Test-Path $stop)){Start-Sleep -Seconds 5}
$state.status=if(Test-Path $stop){'ended-by-owner'}else{'lifetime-ended'}
$state|ConvertTo-Json|Set-Content -Encoding UTF8 $statePath
