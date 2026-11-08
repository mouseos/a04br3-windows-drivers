# 既存1101 helper cacheを読むだけ。親/子の2 TUのみ、SDK install/実機操作なし。
param([Parameter(Mandatory=$true)][string]$ParentInputDirectory,
 [Parameter(Mandatory=$true)][string]$ThermalCacheDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$Inf2Cat)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh dedicated output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$cache=Get-Content "$ParentInputDirectory\cache\parent-cache.json" -Raw|ConvertFrom-Json
$tc=Get-Content "$ThermalCacheDirectory\thermal-cache.json" -Raw|ConvertFrom-Json
$base='1d248a71a95360966ff867e4b063504ac764e074'
if($cache.source_commit -cne $base -or $tc.source_commit -cne $base){throw 'Fixed helper source required'}
$allowed=@('drivers/bus/mt8167-shared/wdf_parent_probe.c','drivers/bus/mt8167-shared/thermal_native_parent.inc','drivers/bus/mt8167-shared/thermal_parent_pio_interface.h','drivers/bus/mt8167-shared/prototype/shared-parent-thermal1104.inf','drivers/power/mt8167-thermal/wdf_native_thermal.c','drivers/power/mt8167-thermal/mt8167-thermal-native.inf','drivers/power/mt8167-thermal/PARENT1104.md','drivers/input/mt8167-accdet/ioctl.h','drivers/input/mt8167-accdet/socket.h','drivers/input/mt8167-accdet/parent.inc','drivers/input/mt8167-accdet/README.md','drivers/input/mt8167-accdet/mt8167-shared-accdet1102.inf')
$allowed+=@('drivers/bus/mt8167-shared/prototype/shared-parent-thermal1103.inf','drivers/power/mt8167-thermal/PARENT1103.md','drivers/input/mt8167-accdet/status.h','drivers/input/mt8167-accdet/mt8167-shared-accdet1103.inf')
$changed=@(& git -C $root diff --name-only $base HEAD -- drivers)
if($LASTEXITCODE -or @($changed|Where-Object {$_ -notin $allowed}).Count){throw 'Helper source/include drift forbids reuse'}
$inc="$ParentInputDirectory\wdk\Include";$km="$ParentInputDirectory\wdk\Lib\winv6.3\km\arm";$wdf="$ParentInputDirectory\wdk\Lib\wdf\kmdf\arm\1.13"
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$inc\km",'-isystem',"$inc\shared",'-isystem',"$inc\um",'-isystem',"$inc\wdf\kmdf\1.13")
$libs=@("$wdf\wdfdriverentry.lib","$wdf\wdfldr.lib","$km\ntoskrnl.lib","$km\hal.lib","$km\bufferoverflowfastfailk.lib","$km\libcntpr.lib")
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package\parent","$out\package\thermal"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$reused=@();$parentObjects=@();$thermalObjects=@();$compiled=@()
 foreach($x in $cache.objects){
  if($x.name -ceq 'parent'){continue};$p="$ParentInputDirectory\cache\obj\$($x.name).obj"
  if((Sha $p) -cne $x.sha256.ToLowerInvariant()){throw 'Parent object hash mismatch'}
  $parentObjects+=$p;$reused+=@{kind='parent';name=$x.name;sha256=(Sha $p)}
 }
 if($parentObjects.Count -ne 13){throw '13 parent helpers required'}
 foreach($x in $tc.objects){
  if($x.name -cnotin @('thermal','boot_record','readout')){throw 'Unexpected child object'}
  $p="$ThermalCacheDirectory\$($x.name).obj";if((Sha $p) -cne $x.sha256){throw 'Thermal cache hash mismatch'}
  $thermalObjects+=$p;$reused+=@{kind='thermal';name=$x.name;sha256=(Sha $p)}
 }
 if($thermalObjects.Count -ne 3){throw 'Three child helpers required'}
 foreach($item in @(@{name='parent';path='drivers/bus/mt8167-shared/wdf_parent_probe.c';flags=(@($cache.flags)+@('-DA04_SHARED_ACCDET_READ=1'))},@{name='wdf_native_thermal';path='drivers/power/mt8167-thermal/wdf_native_thermal.c';flags=@()})){
  $obj="$out\obj\$($item.name).obj";$src=Join-Path $root $item.path
  $args=@($cache.common)+$kernel+@($item.flags)+@('-w','-c',$src,'-o',$obj)
  & clang @args;if($LASTEXITCODE){throw 'Parent/thermal compile failed'}
  $compiled+=@{name=$item.name;path=$item.path;source_sha256=(Sha $src);object_sha256=(Sha $obj);arguments=$args}
 }
 $parentObjects=@("$out\obj\parent.obj")+$parentObjects;$thermalObjects=@("$out\obj\wdf_native_thermal.obj")+$thermalObjects
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @parentObjects @libs "/out:$out\package\parent\mt8167-shared.sys"
 if($LASTEXITCODE){throw 'Parent link failed'}
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @thermalObjects @libs "/out:$out\package\thermal\mt8167-thermal.sys"
 if($LASTEXITCODE){throw 'Thermal link failed'}
 Copy-Item "$root\drivers\bus\mt8167-shared\prototype\shared-parent-thermal1104.inf" "$out\package\parent"
 Copy-Item "$root\drivers\power\mt8167-thermal\mt8167-thermal-native.inf" "$out\package\thermal\mt8167-thermal.inf"
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$unsigned=@()
 foreach($dir in @('parent','thermal')){
  $p="$out\package\$dir";$sys=@(Get-ChildItem $p -Filter '*.sys')[0];$unsigned+=@{path="$dir/$($sys.Name)";sha256=(Sha $sys.FullName)}
  & $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys.FullName;if($LASTEXITCODE){throw 'SYS signing failed'}
  & $Inf2Cat "/driver:$p" /os:6_3_ARM;if($LASTEXITCODE){throw 'CAT generation failed'}
  $cat=@(Get-ChildItem $p -Filter '*.cat');if($cat.Count -ne 1){throw 'One catalog required'}
  & $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat[0].FullName;if($LASTEXITCODE){throw 'CAT sign failed'}
  foreach($f in Get-ChildItem $p -File){
   if($f.Extension -in @('.sys','.cat')){
    & $SignTool verify /pa /q $f.FullName
    if($LASTEXITCODE -or (Get-AuthenticodeSignature $f.FullName).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature mismatch'}
   }
   if($f.Extension -in @('.sys','.inf')){
    & $SignTool verify /pa /q /c $cat[0].FullName $f.FullName;if($LASTEXITCODE){throw 'CAT membership failed'}
   }
  }
 }
 $m=[ordered]@{schema='mt8167-parent-owned-thermal1104';source_commit=$commit;baseline1102='4e253a5c5f53ccdc34c8aaa92296986d53f72d6a';cache_source=$base;parent_flags=(@($cache.flags)+@('-DA04_SHARED_ACCDET_READ=1'));compiled=$compiled;reused=$reused;compiled_TUs=2;reused_objects=16;compiler=((& clang --version)-join "`n");unsigned=$unsigned;signer=$thumb;parent_version='2026.11.8.1104';thermal_version='0.0.1.8';firmware_changed=$false;eight_parent_ranges_preserved=$true;child_resource_count=0;native_interface_version=3;native_interface_parent_build=1104;accdet_status_ioctl='0x00226558';accdet_status_schema=1;accdet_status_bytes=68;accdet_qualification_relaxed=$false;gpio_write_added=$false;source_deltas=@('149e6b61b46c18367a1416238d138f9c8cf496c8','7f7ce5875efb04c0949c4f41b4333c8c9da346d3');hardware_tested=$false;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package").Length+1).Replace('\','/');sha256=(Sha $_.FullName)}})}
 $m|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
 Compress-Archive "$out\package\*" "$out\package.zip";Compress-Archive "$out\obj\*" "$out\objects.zip"
 Get-FileHash "$out\package.zip","$out\objects.zip" -Algorithm SHA256
 'MT8167_PARENT1104_THERMAL18_SIGNED_NOT_DEVICE_TESTED'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
