# SPDX-License-Identifier: BSD-2-Clause-Patent
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$WdkRoot,
 [Parameter(Mandatory=$true)][string]$CrtIncludeRoot,
 [Parameter(Mandatory=$true)][string]$SigntoolPath,
 [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateStore='CurrentUser',
 [string]$ReferenceVcRoot='C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.29.30133',
 [string]$ReferenceSdkLibraryRoot='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0',
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
$launcher=Join-Path $root 'tests\atomic_stage_reference_launcher.c'
$sourceFiles=@($source,$header,$PSCommandPath,$test,$launcher,(Join-Path $root 'tests\README-atomic-stage-replace.md')) | ForEach-Object { Leaf $_ }
$compiler=(Get-Command clang -CommandType Application -ErrorAction Stop).Source
$linker=(Get-Command lld-link -CommandType Application -ErrorAction Stop).Source
$referenceCl=Join-Path $ReferenceVcRoot 'bin\Hostx64\x64\cl.exe'
$referenceLink=Join-Path $ReferenceVcRoot 'bin\Hostx64\x64\link.exe'
$tools=@((Leaf $compiler),(Leaf $linker),(Leaf $ArmKernel32Library),(Leaf $X64Kernel32Library),(Leaf $referenceCl),(Leaf $referenceLink))
$signTool=[IO.Path]::GetFullPath($SigntoolPath)
$tools+=,(Leaf $signTool)
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
$cert=Get-Item -LiteralPath "Cert:\$CertificateStore\My\$thumb"
$trusted=Get-Item -LiteralPath "Cert:\$CertificateStore\Root\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotBefore -gt (Get-Date) -or $cert.NotAfter -lt (Get-Date) -or
   [Convert]::ToBase64String($cert.RawData) -cne [Convert]::ToBase64String($trusted.RawData)){throw 'Existing valid BCBF My/Root identity required'}
$codeSigning=$false
foreach($extension in $cert.Extensions){
 if($extension.Oid.Value -eq '2.5.29.37'){
  foreach($usage in $extension.EnhancedKeyUsages){if($usage.Value -eq '1.3.6.1.5.5.7.3.3'){$codeSigning=$true}}
 }
}
if(!$codeSigning){throw 'Existing BCBF CodeSigning EKU required'}
$referenceLibraries=@((Join-Path $ReferenceVcRoot 'lib\x64\libcmt.lib'),(Join-Path $ReferenceVcRoot 'lib\x64\libvcruntime.lib'),(Join-Path $ReferenceVcRoot 'lib\x64\oldnames.lib'),(Join-Path $ReferenceSdkLibraryRoot 'ucrt\x64\libucrt.lib'),(Join-Path $ReferenceSdkLibraryRoot 'um\x64\kernel32.lib'))
foreach($lib in $referenceLibraries){$tools+=,(Leaf $lib)}
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
function SignedPayload([byte[]]$Unsigned,[byte[]]$Signed) {
 $pe=[BitConverter]::ToInt32($Unsigned,60);$optional=$pe+24
 $checksum=$optional+64;$security=$optional+128
 if([BitConverter]::ToUInt16($Unsigned,$optional) -ne 0x10b -or
    [BitConverter]::ToUInt32($Unsigned,$security) -or [BitConverter]::ToUInt32($Unsigned,$security+4)){throw 'Unsigned PE32 input required'}
 $offset=[BitConverter]::ToUInt32($Signed,$security);$size=[BitConverter]::ToUInt32($Signed,$security+4)
 if($offset -lt $Unsigned.Length -or ($offset%8) -or $size -lt 8 -or ([uint64]$offset+$size) -ne $Signed.Length){throw 'Unexpected appended Authenticode layout'}
 for($i=$Unsigned.Length;$i -lt $offset;$i++){if($Signed[$i]){throw 'Nonzero signing padding'}}
 $before=[byte[]]$Unsigned.Clone();$after=New-Object byte[] $Unsigned.Length
 [Array]::Copy($Signed,$after,$Unsigned.Length)
 foreach($a in @($before,$after)){
  for($i=0;$i -lt 4;$i++){$a[$checksum+$i]=0}
  for($i=0;$i -lt 8;$i++){$a[$security+$i]=0}
 }
 $hasher=[Security.Cryptography.SHA256]::Create()
 try{
  $left=([BitConverter]::ToString($hasher.ComputeHash($before))).Replace('-','').ToLowerInvariant()
  $right=([BitConverter]::ToString($hasher.ComputeHash($after))).Replace('-','').ToLowerInvariant()
 }finally{$hasher.Dispose()}
 if($left -cne $right){throw 'Signing changed production payload'}
 return [ordered]@{unchanged=$true;unsigned_bytes=$Unsigned.Length;normalized_unsigned_sha256=$left;normalized_signed_sha256=$right;certificate_offset=$offset;certificate_bytes=$size}
}
function AssertReferenceCases([object[]]$Cases) {
 $expected=[ordered]@{
  'replace-existing'=0; 'publish-new-target'=0; 'empty-source'=0
  'fault-before-publish'=90; 'existing-temp-rejected'=5
  'missing-source-no-temp'=4; 'start-limit'=3; 'publish-limit'=8
  'below-both-limits'=0; 'extra-argument'=2
  'production-check-mode-rejected-in-reference'=2; 'ads-rejected'=2
  'dos-device-rejected'=2; 'trailing-dot-rejected'=2
  'outside-prefix-rejected'=2; 'directory-rejected'=4
  'parent-reparse-rejected'=4; 'source-too-large'=4
  'maximum-source-size'=0; 'locked-target'=10
 }
 if($Cases.Count -ne $expected.Count){throw 'Fixed 20-case atomic reference suite required'}
 $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
 foreach($case in $Cases){
  if($case.name -isnot [string] -or !($seen.Add($case.name))){throw 'Duplicate or invalid atomic reference case name'}
 }
 foreach($name in $expected.Keys){if(!$seen.Contains($name)){throw 'Fixed atomic reference case names incomplete'}}
 foreach($case in $Cases){
  if($case.passed -isnot [bool] -or !$case.passed -or
     ($case.expected_exit -isnot [int] -and $case.expected_exit -isnot [long]) -or
     ($case.actual_exit -isnot [int] -and $case.actual_exit -isnot [long]) -or
     $case.expected_exit -ne $expected[$case.name] -or $case.actual_exit -ne $case.expected_exit){throw 'Fixed atomic reference case outcome mismatch'}
 }
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
  if(!$reference){$argsList+=@('-D_ARM_','-DARM')}else{$argsList+=@('-DmainCRTStartup=a04_atomic_reference_entry')}
  # Do not add WDK km/crt: its intrin.h conflicts with Clang's ARM builtins.
  foreach($inc in @((Join-Path $kit 'Include\um'),(Join-Path $kit 'Include\shared'),$crt)){$argsList+=@('-isystem',$inc)}
  $argsList+=@('-c',$source,'-o',$object)
  Run ('compile-'+$variant) $compiler $argsList
  if($reference){
   $shim=Join-Path $out 'work\reference-launcher.obj'
   Run 'compile-reference-launcher' $referenceCl @('/nologo','/c','/MT','/O2',('/Fo'+$shim),$launcher)
   $rsp=Join-Path $out 'work\reference-link.rsp'
   $linkArgs=@('/SUBSYSTEM:CONSOLE,6.3','/MACHINE:X64','/INCREMENTAL:NO','/NODEFAULTLIB','/DYNAMICBASE','/NXCOMPAT',('/OUT:"'+$exe+'"'),('/LIBPATH:"'+(Join-Path $ReferenceVcRoot 'lib\x64')+'"'),('/LIBPATH:"'+(Join-Path $ReferenceSdkLibraryRoot 'ucrt\x64')+'"'),('/LIBPATH:"'+(Join-Path $ReferenceSdkLibraryRoot 'um\x64')+'"'),('"'+$object+'"'),('"'+$shim+'"'))
   $linkArgs+=@($referenceLibraries|ForEach-Object{'"'+$_+'"'})
   [IO.File]::WriteAllLines($rsp,$linkArgs,[Text.Encoding]::ASCII)
   Run 'link-reference-standard-crt' $referenceLink @(('@'+$rsp))
   $manifest.reference_launcher=[ordered]@{mode='standard-CRT-calls-same-C-entry';source_sha256=(Hash $launcher);object_sha256=(Hash $shim);response_file_sha256=(Hash $rsp)}
  }else{
   Run ('link-'+$variant) $linker @('/subsystem:console,6.3',('/machine:'+$machine),'/entry:mainCRTStartup','/nodefaultlib','/dynamicbase','/nxcompat',$object,$library,('/out:'+$exe))
  }
  CheckPE $exe $(if($reference){0x8664}else{0x1c4}) $reference
  if(!$reference){
   $unsignedBytes=[IO.File]::ReadAllBytes($exe)
   $cer=Join-Path $out 'atomic-stage-replace.cer'
   [IO.File]::WriteAllBytes($cer,$cert.RawData)
   $storeArgs=@('/s','My','/sha1',$thumb);if($CertificateStore -eq 'LocalMachine'){$storeArgs+='/sm'}
   Run 'sign-production' $signTool (@('sign','/fd','SHA256')+$storeArgs+@($exe))
   Run 'verify-production' $signTool @('verify','/pa','/v',$exe)
   $sig=Get-AuthenticodeSignature -LiteralPath $exe
   if($sig.Status -ne 'Valid' -or !$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -cne $thumb -or
      [Convert]::ToBase64String($sig.SignerCertificate.RawData) -cne [Convert]::ToBase64String($cert.RawData)){throw 'Production Authenticode/BCBF verification failed'}
   $payload=SignedPayload $unsignedBytes ([IO.File]::ReadAllBytes($exe))
   CheckPE $exe 0x1c4 $false
   $manifest.signing=[ordered]@{schema='ATOMIC_STAGE_AUTHENTICODE_V1';policy='Authenticode';digest='SHA256';thumbprint=$thumb;public_certificate=(Leaf $cer);private_key_exported=$false;new_identity_created=$false;non_signature_payload=$payload}
   $manifest.signatures=@([ordered]@{name='atomic-stage-replace.exe';status=[string]$sig.Status;thumbprint=$thumb;sha256=(Hash $exe);certificate_sha256=(Hash $cer)})
   $manifest.signed=$true
  }
  # This is the final signed production hash; the unchanged 20-case script binds it.
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
 AssertReferenceCases @($result.cases)
 if(!$result.success -or $result.reference_sha256 -cne $manifest.reference.sha256 -or $result.production_sha256 -cne $manifest.production.sha256){throw 'Reference result mismatch'}
 $manifest.reference_tests=[ordered]@{status='passed';reference_sha256=$manifest.reference.sha256;cases=$result.cases.Count;result_sha256=(Hash $testResult);result_path=$testResult}
 foreach($pin in @($sourceFiles)+@($tools)+@($manifest.production,$manifest.reference)) {
  if((Hash $pin.path) -cne $pin.sha256){throw 'Source/tool/output drift during build'}
 }
 if((Hash $manifest.signing.public_certificate.path) -cne $manifest.signing.public_certificate.sha256 -or
    [Convert]::ToBase64String((Get-Item -LiteralPath "Cert:\$CertificateStore\Root\$thumb").RawData) -cne [Convert]::ToBase64String($cert.RawData)){throw 'BCBF certificate drift during build'}
 $manifest.success=$true;Save
} catch {
 $manifest.success=$false
 # Avoid recording exception text/command lines which can contain private paths.
 Save
 throw 'Atomic helper build or reference tests failed; inspect checkout-local logs'
} finally {
 foreach($n in $saved.Keys){[Environment]::SetEnvironmentVariable($n,$saved[$n],'Process')}
}
