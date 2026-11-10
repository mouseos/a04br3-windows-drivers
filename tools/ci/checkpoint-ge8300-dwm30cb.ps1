# Explicit process-local handoff; never cancel the selected in-flight TU.
param([switch]$EnableHandoff,[Parameter(Mandatory=$true)][int]$BuildProcessId,
 [Parameter(Mandatory=$true)][string]$SourceDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$Receipt)
$ErrorActionPreference='Stop'
if(!$EnableHandoff -or (Test-Path $Receipt)){throw 'Explicit fresh handoff required'}
$source=[IO.Path]::GetFullPath($SourceDirectory);$out=[IO.Path]::GetFullPath($OutputDirectory)
if((& git -C $source rev-parse HEAD) -cne '30cb20f79ec3c5f71c056d5783fe6743e92bf2ce' -or @(& git -C $source status --porcelain).Count){throw 'Original clean source30cb required'}
$parent=Get-CimInstance Win32_Process -Filter "ProcessId=$BuildProcessId"
if(!$parent -or $parent.Name -cne 'powershell.exe' -or !$parent.CommandLine.Contains('build-ge8300-dwm-source.ps1') -or !$parent.CommandLine.Contains('mt8167-dwm30cb20f7-source')){throw 'Wrong build process'}
if(Test-Path "$out/modules"){throw 'Only native pre-archive handoff is supported'}
$child=@(Get-CimInstance Win32_Process -Filter "ParentProcessId=$BuildProcessId"|Where-Object{$_.Name -eq 'clang.exe' -and $_.CommandLine.Contains('dwm-source\native')})
if($child.Count -ne 1){throw 'Exactly one current native compiler required; re-observe rather than cancel'}
$c=$child[0];$p=[Diagnostics.Process]::GetProcessById($c.ProcessId);$handle=$p.Handle
$record=[ordered]@{schema='dwm30cb-native-process-handoff-v1';source_commit='30cb20f79ec3c5f71c056d5783fe6743e92bf2ce';started_utc=[DateTime]::UtcNow.ToString('o');parent_pid=$BuildProcessId;selected_current_pid=$c.ProcessId;selected_current_command=$c.CommandLine;selected_creation=$c.CreationDate;selected_cpu_100ns=([long]$c.KernelModeTime+[long]$c.UserModeTime);success=$false;prior_exit_codes_recorded=$false;prior_include_traces_recorded=$false}
try{
 $p.WaitForExit();$record.selected_exit_code=$p.ExitCode
 if($p.ExitCode -ne 0){throw 'Selected TU failed; original failure must be retained'}
 # The selected TU has finished. Only now stop the serial scheduler. A possible
 # newly-started successor is explicitly incomplete and must never be reused.
 Stop-Process -Id $BuildProcessId -ErrorAction Stop
 $successors=@(Get-CimInstance Win32_Process -Filter "ParentProcessId=$BuildProcessId"|Where-Object{$_.Name -eq 'clang.exe'})
 $record.incomplete_successors=@($successors|Select-Object ProcessId,CommandLine,CreationDate)
 foreach($next in $successors){if(!$next.CommandLine.Contains('dwm-source\native')){throw 'Unexpected successor; do not stop unrelated process'};Stop-Process -Id $next.ProcessId -ErrorAction SilentlyContinue}
 $incompletePaths=@($successors|ForEach-Object{if($_.CommandLine -notmatch '\s-o\s+(?:"([^"]+)"|(\S+))'){throw 'Successor output absent'};if($Matches[1]){[IO.Path]::GetFullPath($Matches[1])}else{[IO.Path]::GetFullPath($Matches[2])}})
 $record.incomplete_output_paths=$incompletePaths
 $record.completed_objects=@(Get-ChildItem "$out/native" -Filter *.obj | Where-Object{$_.Length -gt 0 -and $_.FullName -notin $incompletePaths} | ForEach-Object{@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
 $record.tool_hashes=@('clang','llvm-lib','llvm-readobj'|ForEach-Object{$t=(Get-Command $_).Source;@{path=$t;sha256=(Get-FileHash $t -Algorithm SHA256).Hash.ToLowerInvariant()}})
 $record.success=$true
}finally{$record.ended_utc=[DateTime]::UtcNow.ToString('o');$record|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $Receipt}
