# SPDX-License-Identifier: MIT
# Three user-mode TUs only. No SDK installer, driver build, registry or device access.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselineSource,
 [Parameter(Mandatory=$true)][string]$KitHeaderRoot,
 [Parameter(Mandatory=$true)][string]$VcToolsRoot,
 [Parameter(Mandatory=$true)][string]$UcrtInclude,
 [Parameter(Mandatory=$true)][string]$LibraryRoot,
 [Parameter(Mandatory=$true)][string]$SignToolPath,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit display mode client candidate required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$source=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$cl="$VcToolsRoot\bin\Hostx64\arm\cl.exe";$link="$VcToolsRoot\bin\Hostx64\x64\link.exe"
$names=@('mt8167-display-rotation-probe','mt8167-display-rotation-session','mt8167-display-rotation-user')
$libs=@('kernel32.lib','user32.lib','advapi32.lib','wtsapi32.lib','ntdll.lib')|ForEach-Object{Join-Path $LibraryRoot $_}
New-Item -ItemType Directory $out,"$out\package","$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$importLib="$out\rt81-seh-import.lib"
& "$VcToolsRoot\bin\Hostx64\x64\lib.exe" /nologo /machine:arm "/def:$root\tools\display\rt81-seh.def" "/out:$importLib"
if($LASTEXITCODE){throw 'Real RT81 SEH export import library failed'}
$libs+=@($importLib)
 $baseline=Get-Content "$BaselineDirectory\package\signed-manifest.json" -Raw|ConvertFrom-Json
 if($baseline.source_commit.Trim() -cne 'd84cc5723bca17c1472f1df2275dadb6f5fb05bb' -or $baseline.source_sha256 -cne (Sha $BaselineSource) -or $baseline.compiler_sha256 -cne (Sha $cl) -or $baseline.linker_sha256 -cne (Sha $link)){throw 'Baseline source/compiler pin'}
 if($baseline.plan_sha256 -cne (Sha "$root\tools\display\rotation_plan.h")){throw 'Mode/restore plan drift'}
 $records=@()
 for($role=1;$role -le 3;$role++){
  $name=$names[$role-1];$obj="$out\$name.obj";$exe="$out\package\$name.exe"
  if($role -gt 1){
   $pre=@('/nologo','/TC','/EP','/O1','/GS-','/Gs65536','/Zl','/Oi-','/D_ARM_','/DARM',"/DROTATION_PROBE_ROLE=$role","/I$KitHeaderRoot\shared","/I$KitHeaderRoot\um","/I$VcToolsRoot\include","/I$UcrtInclude")
   $old=(& $cl @pre $BaselineSource)-join "`n";if($LASTEXITCODE){throw 'Baseline preprocess failed'}
   $new=(& $cl @pre "$root\tools\display\rotation_probe.c")-join "`n";if($LASTEXITCODE){throw 'Current role preprocess failed'}
   # /EP retains blank lines for excluded role blocks. Remove only empty
   # lines/trailing whitespace, never merge/split nonempty token lines.
   $old=($old -split "`n"|Where-Object {$_.Trim().Length}|ForEach-Object {$_.TrimEnd()})-join "`n"
   $new=($new -split "`n"|Where-Object {$_.Trim().Length}|ForEach-Object {$_.TrimEnd()})-join "`n"
   if($old -cne $new){throw 'Unchanged-role preprocessing mismatch'}
   $prior=@($baseline.files|Where-Object {$_.role -eq $role});if($prior.Count -ne 1){throw 'Baseline role record'}
   if($prior[0].sha256 -cne (Sha "$BaselineDirectory\package\$name.exe")){throw 'Reused EXE pin'}
   Copy-Item "$BaselineDirectory\package\$name.exe" $exe
   $sig=Get-AuthenticodeSignature $exe;if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Reused EXE signer'}
   $record=$prior[0];$record|Add-Member -NotePropertyName reused_from_source -NotePropertyValue 'd84cc5723bca17c1472f1df2275dadb6f5fb05bb'
   $record|Add-Member -NotePropertyName role_preprocessing_identical -NotePropertyValue $true
   $records+=$record;continue
  }
  $args=@('/nologo','/TC','/c','/O1','/GS-','/Gs65536','/Zl','/Oi-','/D_ARM_','/DARM',"/DROTATION_PROBE_ROLE=$role","/I$KitHeaderRoot\shared","/I$KitHeaderRoot\um","/I$VcToolsRoot\include","/I$UcrtInclude","/Fo$obj","$root\tools\display\rotation_probe.c")
  & $cl @args;if($LASTEXITCODE){throw "Role$role ARM compile failed"}
  & $link /nologo /machine:arm /subsystem:windows,6.03 /entry:ProbeEntry /nodefaultlib /dynamicbase /nxcompat $obj @libs "/out:$exe" "/map:$out\$name.map"
  if($LASTEXITCODE){throw "Role$role ARM link failed"}
  $unsigned=Sha $exe;$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
  $machine=Test-Path "Cert:\LocalMachine\My\$thumb";$store=@();if($machine){$store=@('/sm')}
  & $SignToolPath sign /fd SHA256 /sha1 $thumb /s My @store $exe;if($LASTEXITCODE){throw 'Sign failed'}
  & $SignToolPath verify /pa $exe;if($LASTEXITCODE){throw 'Signature trust failed'}
  $sig=Get-AuthenticodeSignature $exe;if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Signer mismatch'}
  $b=[IO.File]::ReadAllBytes($exe);$pe=[BitConverter]::ToInt32($b,60)
  if([BitConverter]::ToUInt16($b,$pe+4) -ne 0x1c4){throw 'ARMNT required'}
  $records+=@{name="$name.exe";role=$role;compile_arguments=$args;object_sha256=(Sha $obj);unsigned_sha256=$unsigned;sha256=(Sha $exe);signature_status='Valid';signer_thumbprint=$thumb}
 }
 [ordered]@{schema='mt8167-display-rotation-probe-v2-jobfix';source_commit=$source;hardware_tested=$false;os_mode_switch_tested=$false;SDK_installed=$false;driver_rebuilt=$false;compiled_wrapper_TUs=1;reused_signed_EXEs=2;caller_job_contract_updated_source=$true;caller_service_rebuild_required=$true;
  certificate_thumbprint=$thumb;fixed_directory='Windows\System32\MT8167DisplayRotationProbeV1';command_arguments=@();
  compiler_sha256=(Sha $cl);linker_sha256=(Sha $link);source_sha256=(Sha "$root\tools\display\rotation_probe.c");plan_sha256=(Sha "$root\tools\display\rotation_plan.h");
  libraries=@($libs|ForEach-Object{@{name=[IO.Path]::GetFileName($_);sha256=(Sha $_)}});files=$records
 }|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
 Compress-Archive "$out\package\*" "$out\package.zip"
 Compress-Archive "$out\*.obj","$out\*.map" "$out\objects.zip"
 Get-FileHash "$out\package.zip","$out\objects.zip" -Algorithm SHA256
 Write-Output 'DISPLAY_ROTATION_CLIENT_SIGNED_NOT_EXECUTED'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
