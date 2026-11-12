# SPDX-License-Identifier: BSD-2-Clause-Patent
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$ReferenceExe,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedReferenceSha256,
 [Parameter(Mandatory=$true)][string]$ProductionExe,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedProductionSha256,
 [Parameter(Mandatory=$true)][string]$ResultPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$fixed='C:\mt8167-resume235-native-43253681009\atomic-reference'
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function NoReparse([string]$Path){
 $p=$Path
 while($p){
  if(Test-Path -LiteralPath $p){if(((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Test path reparse rejected'}}
  $next=[IO.Path]::GetDirectoryName($p);if($p -eq $next){break};$p=$next
 }
}
$resultFile=[IO.Path]::GetFullPath($ResultPath)
if(!$resultFile.StartsWith($root+'\out\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $resultFile)){throw 'Fresh checkout-local result required'}
NoReparse $resultFile;NoReparse $fixed
if((Hash $ReferenceExe) -cne $ExpectedReferenceSha256 -or (Hash $ProductionExe) -cne $ExpectedProductionSha256){throw 'Actual helper byte pin mismatch'}
$rb=[IO.File]::ReadAllBytes($ReferenceExe);$pe=[BitConverter]::ToInt32($rb,60)
if([BitConverter]::ToUInt16($rb,$pe+4) -ne 0x8664 -or !([Text.Encoding]::ASCII.GetString($rb)).Contains(('A04_ATOMIC_STAGE_V1:TESTCLOCK:REFERENCE'+[char]0))){throw 'x64 reference required; production must never execute'}
if(![Environment]::Is64BitOperatingSystem -or ([IO.DriveInfo]::new('C:\')).DriveFormat -ine 'NTFS'){throw 'x64 Windows and NTFS C required'}
$casesRoot=Join-Path $fixed 'cases'
if(Test-Path -LiteralPath $casesRoot){throw 'Existing cases are evidence; rerun forbidden'}
New-Item -ItemType Directory -Path $casesRoot -ErrorAction Stop|Out-Null
$result=[ordered]@{schema='ATOMIC_STAGE_REPLACE_REFERENCE_TEST_V1';success=$false;reference_sha256=$ExpectedReferenceSha256;production_sha256=$ExpectedProductionSha256;test_script_sha256=(Hash $PSCommandPath);root=$fixed;production_executed=$false;cases=@()}
function Save {$result|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $resultFile -Encoding UTF8}
function Check([bool]$Value,[string]$Message){if(!$Value){throw $Message}}
function RunCase([string]$Name,[string]$Mode,[int]$Code,[uint64]$Start=0,[uint64]$Publish=0) {
 $dir=Join-Path $casesRoot $Name
 New-Item -ItemType Directory -Path $dir -ErrorAction Stop|Out-Null
 $src=Join-Path $dir 'new source.bin';$dst=Join-Path $dir 'old target.bin';$tmp=$dst+'.a04-stage.tmp'
 $new=[Text.Encoding]::UTF8.GetBytes(('new-'+$Name+'-')*8192)
 $old=[Text.Encoding]::UTF8.GetBytes('original-target-must-survive')
 [IO.File]::WriteAllBytes($src,$new)
 if($Mode -ne 'new-target'){[IO.File]::WriteAllBytes($dst,$old)}
 $before=if(Test-Path -LiteralPath $dst){Hash $dst}else{$null}
 $tempBefore=$null;$lock=$null
 if($Mode -eq 'existing-temp'){[IO.File]::WriteAllBytes($tmp,[Text.Encoding]::ASCII.GetBytes('pre-existing-evidence'));$tempBefore=Hash $tmp}
 if($Mode -in @('oversize','maxsize')){$f=[IO.File]::Open($src,[IO.FileMode]::Open,[IO.FileAccess]::Write);try{$f.SetLength($(if($Mode -eq 'maxsize'){67108864}else{67108865}))}finally{$f.Dispose()}}
 if($Mode -eq 'empty'){[IO.File]::WriteAllBytes($src,[byte[]]@())}
 if($Mode -eq 'directory'){New-Item -ItemType Directory -Path (Join-Path $dir 'dest-directory')|Out-Null;$dst=Join-Path $dir 'dest-directory';$tmp=$dst+'.a04-stage.tmp';$before=$null}
 if($Mode -eq 'missing-source'){$src=Join-Path $dir 'missing.bin'}
 if($Mode -eq 'parent-reparse'){
  $real=Join-Path $dir 'real';New-Item -ItemType Directory -Path $real|Out-Null
  $junction=Join-Path $dir 'junction';New-Item -ItemType Junction -Path $junction -Value $real -ErrorAction Stop|Out-Null
  $dst=Join-Path $junction 'target.bin';$tmp=$dst+'.a04-stage.tmp';$before=$null
 }
 if($Mode -eq 'locked-target'){$lock=[IO.File]::Open($dst,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)}
 $argDst=$dst
 if($Mode -eq 'ads'){$argDst=$dst+':stream'}
 if($Mode -eq 'reserved'){$argDst=Join-Path $dir 'CON.txt'}
 if($Mode -eq 'trailing-dot'){$argDst=$dst+'.'}
 if($Mode -eq 'outside-prefix'){$argDst='C:\Windows\System32\a04-reference-must-not-touch.bin'}
 $argv=@($src,$argDst,'--testclock',[string]$Start,[string]$Publish)
 if($Mode -eq 'fault'){$argv+='--fault-before-publish'}
 if($Mode -eq 'extra-arg'){$argv+='--not-an-option'}
 if($Mode -eq 'check-lease'){$argv=@('--check-lease')}
 $row=[ordered]@{name=$Name;expected_exit=$Code;actual_exit=$null;passed=$false;old_sha256=$before;source_sha256=$null;target_sha256=$null;temp_sha256=$null}
 $result.cases+=@($row);Save
 try {
  & $ReferenceExe @argv *> (Join-Path $dir 'stdout.log')
  $row.actual_exit=$LASTEXITCODE
  Check ($row.actual_exit -eq $Code) ('Unexpected exit: '+$Name)
  if(Test-Path -LiteralPath $src -PathType Leaf){$row.source_sha256=Hash $src}
  if(Test-Path -LiteralPath $dst -PathType Leaf){$row.target_sha256=Hash $dst}
  if(Test-Path -LiteralPath $tmp -PathType Leaf){$row.temp_sha256=Hash $tmp}
  if($Code -eq 0){
   Check ($row.target_sha256 -ceq $row.source_sha256) 'Published bytes differ'
   Check (!(Test-Path -LiteralPath $tmp)) 'Successful publish left temp'
  } else {
   if($before){Check ($row.target_sha256 -ceq $before) 'Old target changed on rejection/fault'}
   elseif($Mode -ne 'directory'){Check (!(Test-Path -LiteralPath $dst)) 'Unexpected target created'}
   if($Mode -eq 'existing-temp'){Check ($row.temp_sha256 -ceq $tempBefore) 'Existing temp evidence changed'}
   elseif($Code -in @(8,10,90)) {Check ($row.temp_sha256 -ceq $row.source_sha256) 'Flushed temp evidence missing/different'}
   else {Check (!(Test-Path -LiteralPath $tmp)) 'Rejected input created temp'}
  }
  $row.passed=$true
 } finally {if($null -ne $lock){$lock.Dispose()};Save}
}
try {
 Save
 RunCase 'replace-existing' 'replace' 0
 RunCase 'publish-new-target' 'new-target' 0
 RunCase 'empty-source' 'empty' 0
 RunCase 'fault-before-publish' 'fault' 90
 RunCase 'existing-temp-rejected' 'existing-temp' 5
 RunCase 'missing-source-no-temp' 'missing-source' 4
 RunCase 'start-limit' 'start-limit' 3 1680000 1680000
 RunCase 'publish-limit' 'publish-limit' 8 0 1740000
 RunCase 'below-both-limits' 'replace' 0 1679999 1739999
 RunCase 'extra-argument' 'extra-arg' 2
 RunCase 'production-check-mode-rejected-in-reference' 'check-lease' 2
 RunCase 'ads-rejected' 'ads' 2
 RunCase 'dos-device-rejected' 'reserved' 2
 RunCase 'trailing-dot-rejected' 'trailing-dot' 2
 RunCase 'outside-prefix-rejected' 'outside-prefix' 2
 RunCase 'directory-rejected' 'directory' 4
 RunCase 'parent-reparse-rejected' 'parent-reparse' 4
 RunCase 'source-too-large' 'oversize' 4
 RunCase 'maximum-source-size' 'maxsize' 0
 RunCase 'locked-target' 'locked-target' 10
 Check ((Hash $ReferenceExe) -ceq $ExpectedReferenceSha256) 'Reference executable drift'
 Check ((Hash $ProductionExe) -ceq $ExpectedProductionSha256) 'Production executable drift'
 $result.success=$true;Save
} catch {$result.success=$false;Save;throw 'Reference test failed; preserve cases and result for inspection'}
