# SPDX-License-Identifier: BSD-2-Clause-Patent
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$WdkRoot,
 [Parameter(Mandatory=$true)][string]$CrtIncludeRoot,
 [string]$ArmKernel32Library='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm\kernel32.lib',
 [string]$X64Kernel32Library='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\x64\kernel32.lib'
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\')
$out=[IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Leaf([string]$Path) {
 $f=Get-Item -LiteralPath $Path
 [ordered]@{name=$f.Name;path=$f.FullName;bytes=$f.Length;sha256=(Hash $f.FullName)}
}
function NoReparse([string]$Path) {
 $p=$Path
 while($p) {
  if(Test-Path -LiteralPath $p) {
   if(((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Reparse ancestor rejected' }
  }
  $next=[IO.Path]::GetDirectoryName($p);if($p -eq $next){break};$p=$next
 }
}
if(!$out.StartsWith($root+'\out\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)) { throw 'Fresh checkout-local out directory required' }
NoReparse $out
$source=Join-Path $root 'tools\winpe\atomic-stage-replace.c'
$header=Join-Path $root 'drivers\system\mt8167-toprgu\uart_screen.h'
$test=Join-Path $root 'tests\atomic_stage_replace_reference_test.ps1'
$sourceFiles=@($source,$header,$PSCommandPath,$test,(Join-Path $root 'tests\README-atomic-stage-replace.md')) | ForEach-Object { Leaf $_ }
$compiler=(Get-Command clang -CommandType Application -ErrorAction Stop).Source
$linker=(Get-Command lld-link -CommandType Application -ErrorAction Stop).Source
$tools=@((Leaf $compiler),(Leaf $linker),(Leaf $ArmKernel32Library),(Leaf $X64Kernel32Library))
$kit=[IO.Path]::GetFullPath($WdkRoot)
$crt=[IO.Path]::GetFullPath($CrtIncludeRoot)
foreach($p in @((Join-Path $kit 'Include\um\windows.h'),(Join-Path $kit 'Include\shared\winerror.h'),(Join-Path $crt 'stdint.h'))) {
 if(!(Test-Path -LiteralPath $p -PathType Leaf)){throw 'Cached Windows/CRT headers required'}
}
$commit=@(& git -C $root rev-parse HEAD);if($LASTEXITCODE -or $commit.Count -ne 1){throw 'Source commit unavailable'}
$dirty=@(& git --no-optional-locks -C $root status --porcelain --untracked-files=normal);if($LASTEXITCODE){throw 'Source status unavailable'}
if($dirty.Count -gt 0){throw 'Clean committed source required before output creation'}
New-Item -ItemType Directory -Path $out -ErrorAction Stop|Out-Null
New-Item -ItemType Directory -Path (Join-Path $out 'work'),(Join-Path $out 'logs') -ErrorAction Stop|Out-Null
$manifest=[ordered]@{schema='ATOMIC_STAGE_REPLACE_BUILD_V1';success=$false;source_commit=$commit[0];source_dirty=($dirty.Count -gt 0);production=$null;reference=$null;production_reference_mode=$false;deadline_query_required=$true;source_files=@($sourceFiles);tools=$tools;steps=@();reference_tests=$null;GPU_build_count=0;SDK_build_count=0;hardware_verified=$false;signed=$false}
function Save { $manifest|ConvertTo-Json -Depth 16|Set-Content -LiteralPath (Join-Path $out 'manifest.json') -Encoding UTF8 }
function Run([string]$Name,[string]$Exe,[string[]]$Argv) {
 $log=Join-Path $out ('logs\'+$Name+'.log')
 $step=[ordered]@{name=$Name;exit_code=$null;log_sha256=$null};$manifest.steps+=@($step);Save
 $savedPreference=$ErrorActionPreference
 $nativeExit=$null
 try {
  $ErrorActionPreference='Continue'
  $global:LASTEXITCODE=$null
  & $Exe @Argv 2>&1 | Out-File -LiteralPath $log -Encoding UTF8 -ErrorAction Stop
  $nativeExit=$LASTEXITCODE
 } finally {
  $ErrorActionPreference=$savedPreference
  $step.exit_code=$nativeExit
  try { if(Test-Path -LiteralPath $log -PathType Leaf){$step.log_sha256=Hash $log} }
  finally { Save }
 }
 if($null -eq $step.exit_code -or $step.exit_code -ne 0){throw ('Native stage failed: '+$Name)}
}
function RvaOffset([byte[]]$Bytes,[int]$Pe,[uint32]$Rva) {
 $sections=[BitConverter]::ToUInt16($Bytes,$Pe+6)
 $table=$Pe+24+[BitConverter]::ToUInt16($Bytes,$Pe+20)
 for($i=0;$i -lt $sections;$i++) {
  $s=$table+40*$i;$va=[BitConverter]::ToUInt32($Bytes,$s+12)
  $size=[Math]::Max([BitConverter]::ToUInt32($Bytes,$s+8),[BitConverter]::ToUInt32($Bytes,$s+16))
  if($Rva -ge $va -and ([uint64]$Rva) -lt ([uint64]$va+$size)) { return [int]([BitConverter]::ToUInt32($Bytes,$s+20)+$Rva-$va) }
 }
 throw 'Unmapped PE import RVA'
}
function CheckPE([string]$Path,[int]$Machine,[bool]$Reference) {
 $b=[IO.File]::ReadAllBytes($Path)
 if($b.Length -lt 256 -or $b[0] -ne 77 -or $b[1] -ne 90){throw 'MZ required'}
 $pe=[BitConverter]::ToInt32($b,60)
 if($pe -lt 0 -or $pe+256 -gt $b.Length -or [BitConverter]::ToUInt32($b,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($b,$pe+4) -ne $Machine){throw 'Unexpected PE machine'}
 $magic=[BitConverter]::ToUInt16($b,$pe+24)
 if(($Machine -eq 0x1c4 -and $magic -ne 0x10b) -or ($Machine -eq 0x8664 -and $magic -ne 0x20b)){throw 'Unexpected PE optional header'}
 $directory=$pe+24+$(if($magic -eq 0x20b){112}else{96})
 $imports=RvaOffset $b $pe ([BitConverter]::ToUInt32($b,$directory+8))
 $names=@()
 for($i=0;$i -lt 32;$i++) {
  $row=$imports+20*$i;$nameRva=[BitConverter]::ToUInt32($b,$row+12)
  if(!$nameRva){break}
  $off=RvaOffset $b $pe $nameRva;$end=$off
  while($end -lt $b.Length -and $b[$end]){++$end}
  $names+=,[Text.Encoding]::ASCII.GetString($b,$off,$end-$off)
 }
 if($names.Count -ne 1 -or $names[0] -ine 'kernel32.dll'){throw 'Only kernel32.dll imports allowed'}
 $text=[Text.Encoding]::ASCII.GetString($b)
 $wide=[Text.Encoding]::Unicode.GetString($b)
 $productionMarker='A04_ATOMIC_STAGE_V1:LEASE_QUERY:PRODUCTION'+[char]0
 $referenceMarker='A04_ATOMIC_STAGE_V1:TESTCLOCK:REFERENCE'+[char]0
 if($Reference) {
  if(!$text.Contains($referenceMarker) -or $text.Contains($productionMarker)){throw 'Reference marker mismatch'}
 } else {
  if(!$text.Contains($productionMarker) -or $text.Contains($referenceMarker) -or $text.Contains('--testclock') -or $text.Contains('--fault-before-publish') -or $wide.Contains('--testclock') -or $wide.Contains('--fault-before-publish')){throw 'Production marker/reference isolation failed'}
 }
}
$saved=@{};foreach($n in @('TEMP','TMP','TMPDIR')){$saved[$n]=[Environment]::GetEnvironmentVariable($n,'Process')}
try {
 foreach($n in $saved.Keys){[Environment]::SetEnvironmentVariable($n,(Join-Path $out 'work'),'Process')}
 Save
 foreach($variant in @('production','reference')) {
  $reference=$variant -eq 'reference'
  $target=if($reference){'x86_64-pc-windows-msvc'}else{'thumbv7-pc-windows-msvc'}
  $machine=if($reference){'x64'}else{'arm'}
  $library=if($reference){$X64Kernel32Library}else{$ArmKernel32Library}
  $object=Join-Path $out ('work\'+$variant+'.obj')
  $dep=Join-Path $out ('work\'+$variant+'.d')
  $exe=Join-Path $out $(if($reference){'atomic-stage-replace-reference-x64.exe'}else{'atomic-stage-replace.exe'})
  $argsList=@(('--target='+$target),'-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fms-extensions','-Wall','-Wextra','-Werror','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000',('-DA04_ATOMIC_REPLACE_REFERENCE='+[int]$reference),'-MD','-MF',$dep)
  if(!$reference){$argsList+=@('-D_ARM_','-DARM')}
  # Do not add WDK km/crt: its intrin.h conflicts with Clang's ARM builtins.
  foreach($inc in @((Join-Path $kit 'Include\um'),(Join-Path $kit 'Include\shared'),$crt)){$argsList+=@('-isystem',$inc)}
  $argsList+=@('-c',$source,'-o',$object)
  Run ('compile-'+$variant) $compiler $argsList
  Run ('link-'+$variant) $linker @('/subsystem:console,6.3',('/machine:'+$machine),'/entry:mainCRTStartup','/nodefaultlib','/dynamicbase','/nxcompat',$object,$library,('/out:'+$exe))
  CheckPE $exe $(if($reference){0x8664}else{0x1c4}) $reference
  $manifest[$variant]=Leaf $exe;Save
 }
 # The dedicated script executes only the x64 reference, never the ARMNT image.
 $testResult=Join-Path $out 'reference-result.json'
 $step=[ordered]@{name='reference-tests';exit_code=$null;log_sha256=$null};$manifest.steps+=@($step);Save
 try {
  & $test -ReferenceExe $manifest.reference.path -ExpectedReferenceSha256 $manifest.reference.sha256 -ProductionExe $manifest.production.path -ExpectedProductionSha256 $manifest.production.sha256 -ResultPath $testResult *> (Join-Path $out 'logs\reference-tests.log')
  $step.exit_code=0
 } catch { $step.exit_code=1;throw }
 finally { $step.log_sha256=Hash (Join-Path $out 'logs\reference-tests.log');Save }
 $result=Get-Content -LiteralPath $testResult -Raw|ConvertFrom-Json
 if(!$result.success -or $result.reference_sha256 -cne $manifest.reference.sha256 -or $result.production_sha256 -cne $manifest.production.sha256 -or $result.cases.Count -lt 3 -or @($result.cases|Where-Object {-not $_.passed}).Count){throw 'Reference result mismatch'}
 $manifest.reference_tests=[ordered]@{status='passed';reference_sha256=$manifest.reference.sha256;cases=$result.cases.Count;result_sha256=(Hash $testResult);result_path=$testResult}
 foreach($pin in @($sourceFiles)+@($tools)+@($manifest.production,$manifest.reference)) {
  if((Hash $pin.path) -cne $pin.sha256){throw 'Source/tool/output drift during build'}
 }
 $manifest.success=$true;Save
} catch {
 $manifest.success=$false
 # Avoid recording exception text/command lines which can contain private paths.
 Save
 throw 'Atomic helper build or reference tests failed; inspect checkout-local logs'
} finally {
 foreach($n in $saved.Keys){[Environment]::SetEnvironmentVariable($n,$saved[$n],'Process')}
}
