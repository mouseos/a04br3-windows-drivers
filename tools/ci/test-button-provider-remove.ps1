$ErrorActionPreference='Stop'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$base=Join-Path $tempRoot 'a04-provider-remove-test'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
New-Item -ItemType Directory -Force "$base\Windows\inf","$base\src\button-provider" | Out-Null
Copy-Item "$tempRoot\a04-compare-x64\compare-files.exe" "$base\src\compare-files.exe" -Force
[IO.File]::WriteAllText("$base\src\button-provider\a04-button-provider.inf",'exact package',[Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$base\Windows\inf\oem1.inf",'exact package',[Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$base\Windows\inf\oem2.inf",'exact package',[Text.Encoding]::ASCII)
$batch=Get-Content (Join-Path $root 'tools\winpe\startnet-remove-button-provider.cmd') -Raw
$batch=$batch.Replace('C:\Windows',"$base\Windows").Replace('X:\a04br3',"$base\src")
$batch=$batch -replace '(?m)^wpeinit\r?\n','' -replace '(?m)^drvload .*\r?\nif errorlevel 1 goto storagefail\r?\n','' -replace '(?m)^.*guard-windows-volume.exe\r?\nif errorlevel 1 goto guardfail\r?\n','' -replace '(?m)^mountvol C: /p\r?\n',''
$batch=$batch -replace '(?m)^dism.exe .*$',"cmd /c echo MOCK_DISM > `"$base\src\button-provider-remove.log`""
[IO.File]::WriteAllText("$base\remove.cmd",$batch,[Text.Encoding]::ASCII)
$out=& cmd.exe /c "$base\remove.cmd"
if (-not ($out -match 'BUTTON_PROVIDER_REMOVE_MATCH_REJECTED')) { throw 'Multiple matching packages accepted' }
if (Test-Path "$base\src\button-provider-remove.log") { throw 'DISM ran on ambiguous match' }
Remove-Item "$base\Windows\inf\oem2.inf"
$out=& cmd.exe /c "$base\remove.cmd"
if (-not ($out -match 'BUTTON_PROVIDER_REMOVE_DRIVER=oem1.inf')) { throw 'Wrong target selected' }
if (-not ($out -match 'BUTTON_PROVIDER_WINDOWS_REMOVED')) { throw ($out -join ' ') }
Write-Output 'BUTTON_PROVIDER_REMOVE_GUARD_TEST_OK'
$global:LASTEXITCODE=0
