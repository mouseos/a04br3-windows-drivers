# SPDX-License-Identifier: MIT
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$KitHeaderRoot,
 [Parameter(Mandatory=$true)][string]$KmLibraryRoot,
 [Parameter(Mandatory=$true)][string]$WdfLibraryRoot,
 [Parameter(Mandatory=$true)][string]$BatteryClassLibrary,
 [Parameter(Mandatory=$true)][string]$UmLibraryRoot,
 [Parameter(Mandatory=$true)][string]$VcInclude,
 [Parameter(Mandatory=$true)][string]$UcrtInclude,
 [Parameter(Mandatory=$true)][string]$ClangPath,
 [Parameter(Mandatory=$true)][string]$LinkPath,
 [Parameter(Mandatory=$true)][string]$SignToolPath,
 [Parameter(Mandatory=$true)][string]$Inf2CatPath,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit battery first-failure candidate required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
New-Item -ItemType Directory $out,"$out\package","$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$src="$root\drivers\power\lc709203f"
 # Every existing charger/broker/trace profile is explicitly off, independent of env.
 $flags=@{};$defs=@()
 foreach($m in [regex]::Matches([IO.File]::ReadAllText("$src\wdf_gauge.c"),'(?m)^#ifndef (A04_[A-Z0-9_]+)')){$flags[$m.Groups[1].Value]=0}
 foreach($key in @($flags.Keys|Sort-Object)){$defs+="-D$key=0"}
 $common=@('--target=thumbv7-pc-windows-msvc','-O1','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$KitHeaderRoot\shared","-I$KitHeaderRoot\um","-I$VcInclude","-I$UcrtInclude")
 $baseline=Get-Content "$BaselineDirectory\package\signed-manifest.json" -Raw|ConvertFrom-Json
 if($baseline.driver_version -cne '11/08/2026,0.0.1.7' -or $baseline.signature_status -cne 'Valid' -or $baseline.source_commit.Trim() -cne '64cce0738845cce5c8d90066137383cc968f4c47' -or $baseline.compiler_sha256 -cne (Sha $ClangPath)){throw 'Baseline source/compiler pin'}
 & git -C $root diff --exit-code 64cce0738845cce5c8d90066137383cc968f4c47 HEAD -- drivers/power/lc709203f/gauge.c drivers/power/lc709203f/gauge.h drivers/power/lc709203f/mt8167_i2c.c drivers/power/lc709203f/mt8167_i2c.h drivers/power/lc709203f/battery_class.c drivers/power/lc709203f/battery_class.h drivers/power/lc709203f/gauge.h
 if($LASTEXITCODE){throw 'Reused TU dependency changed'}
 $objects=@();$tu=@()
 foreach($name in @('gauge','mt8167_i2c','battery_class','wdf_gauge')){
  $obj="$out\$name.obj"
  if($name -in @('gauge','mt8167_i2c','battery_class')){
   $prior=@($baseline.translation_units|Where-Object {$_.name -ceq $name})
   if($prior.Count -ne 1 -or $prior[0].source_sha256 -cne (Sha "$src\$name.c") -or $prior[0].object_sha256 -cne (Sha "$BaselineDirectory\$name.obj")){throw 'Baseline object pin'}
   Copy-Item "$BaselineDirectory\$name.obj" $obj
   $tu+=@{name=$name;source_sha256=(Sha "$src\$name.c");object_sha256=(Sha $obj);reused_from_source='64cce0738845cce5c8d90066137383cc968f4c47'}
  }else{
   $args=$common+@('-D_KERNEL_MODE','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13',"-I$KitHeaderRoot\km","-I$KitHeaderRoot\wdf\kmdf\1.13")+$defs+@('-c',"$src\$name.c",'-o',$obj)
   & $ClangPath @args;if($LASTEXITCODE){throw "Notify driver TU failed: $name"}
   $tu+=@{name=$name;source_sha256=(Sha "$src\$name.c");object_sha256=(Sha $obj);arguments=$args}
  }
  $objects+=$obj
 }
 $libs=@("$WdfLibraryRoot\wdfdriverentry.lib","$WdfLibraryRoot\wdfldr.lib","$KmLibraryRoot\ntoskrnl.lib","$KmLibraryRoot\hal.lib",$BatteryClassLibrary,"$KmLibraryRoot\bufferoverflowfastfailk.lib","$KmLibraryRoot\libcntpr.lib")
 foreach($lp in $libs){
  $old=@($baseline.libraries|Where-Object {$_.name -ceq [IO.Path]::GetFileName($lp)})
  if($old.Count -ne 1 -or $old[0].sha256 -cne (Sha $lp)){throw "Retained link library pin: $lp"}
 }
 & $LinkPath /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects @libs "/out:$out\package\lc709203f.sys"
 if($LASTEXITCODE){throw 'Temperature SYS link failed'}
 if([IO.File]::ReadAllText("$src\lc709203f.inf") -notmatch '(?m)^DriverVer=11/10/2026,0\.0\.1\.8\r?$'){throw 'Formal1.8 INF required'}
 Copy-Item "$src\lc709203f.inf" "$out\package"
 & git -C $root diff --exit-code 64cce0738845cce5c8d90066137383cc968f4c47 HEAD -- tools/winpe/lc709203f-temperature.c
 if($LASTEXITCODE){throw 'Temperature consumer source changed'}
 $temp=@($baseline.files|Where-Object {$_.name -ceq 'lc709203f-temperature.exe'})
 if($temp.Count -ne 1 -or $temp[0].sha256 -cne (Sha "$BaselineDirectory\package\lc709203f-temperature.exe")){throw 'Temperature EXE pin'}
 Copy-Item "$BaselineDirectory\package\lc709203f-temperature.exe" "$out\package"
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$store=@();if(Test-Path "Cert:\LocalMachine\My\$thumb"){$store=@('/sm')}
 $unsigned=@{}
 foreach($name in @('lc709203f.sys')){
  $unsigned[$name]=Sha "$out\package\$name"
  & $SignToolPath sign /fd SHA256 /sha1 $thumb /s My @store "$out\package\$name";if($LASTEXITCODE){throw 'Sign failed'}
 }
 & $Inf2CatPath "/driver:$out\package" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'CAT generation failed'}
 & $SignToolPath sign /fd SHA256 /sha1 $thumb /s My @store "$out\package\lc709203f.cat"
 if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($name in @('lc709203f.sys','lc709203f.cat','lc709203f-temperature.exe')){
  $sig=Get-AuthenticodeSignature "$out\package\$name"
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Invalid signature/identity'}
 }
 foreach($name in @('lc709203f.sys','lc709203f.inf')){
  & $SignToolPath verify /pa /c "$out\package\lc709203f.cat" "$out\package\$name"
  if($LASTEXITCODE){throw 'CAT membership failed'}
 }
 [ordered]@{schema='lc709203f-first-failure-observe-v1';source_commit=$commit;baseline_source='64cce0738845cce5c8d90066137383cc968f4c47';
 profile='FirstFailureStatusNotifyTemperatureReadOnly';driver_version='11/10/2026,0.0.1.8';effective_flags=$flags;
 charger_register_write=$false;gauge_register_write=$false;new_resource_owner=$false;hardware_tested=$false;
 standard_interface='BatteryMiniSetStatusNotify/DisableStatusNotify plus unchanged BatteryTemperature';compiled_driver_TUs=1;reused_driver_objects=3;temperature_consumer_reused=$true;
 certificate_thumbprint=$thumb;signature_status='Valid';catalog_membership_verified=$true;unsigned=$unsigned;
 compiler_sha256=(Sha $ClangPath);linker_sha256=(Sha $LinkPath);translation_units=$tu;
 libraries=@($libs|ForEach-Object{@{name=[IO.Path]::GetFileName($_);sha256=(Sha $_)}});
 files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{name=$_.Name;sha256=(Sha $_.FullName)}})
 }|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
 Compress-Archive "$out\package\*" "$out\package.zip"
 Compress-Archive "$out\*.obj" "$out\objects.zip"
 Get-FileHash "$out\package.zip","$out\objects.zip" -Algorithm SHA256
 Write-Output 'LC709203F_FIRST_FAILURE_SIGNED_NOT_DEVICE_TESTED'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
