# Source-only recipe. Executes locally only when main invokes -BuildAndSign.
# No SDK setup, cache writes, network, installation, DIF, role or VBUS command.
param(
 [Parameter(Mandatory=$true)][string]$ParentSignedManifest,
 [Parameter(Mandatory=$true)][string]$HcdSignedManifest,
 [Parameter(Mandatory=$true)][string]$ReuseDirectory,
 [Parameter(Mandatory=$true)][string]$KitInclude,
 [Parameter(Mandatory=$true)][string]$UmInclude,
 [Parameter(Mandatory=$true)][string]$WdfInclude,
 [Parameter(Mandatory=$true)][string]$KernelLibraryDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$Inf2Cat,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [switch]$BuildAndSign)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\out\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local out subdirectory required'}
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function RequireHash($p,$h){if(!(Test-Path -LiteralPath $p -PathType Leaf) -or (Sha $p) -cne $h){throw "Pinned input drift: $p"}}
function Value($obj,$key){if($null -eq $obj){return $null};$p=$obj.PSObject.Properties[$key];if($null -eq $p){return $null};return $p.Value}
function CheckClean(){if(@(& git -C $root status --porcelain).Count -or $LASTEXITCODE){throw 'Clean committed source required'}}
CheckClean
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE){throw 'git source identity unavailable'}
$lockPath="$PSScriptRoot\musb1121-hcd1019-inputs.json"
$lock=Get-Content -LiteralPath $lockPath -Raw|ConvertFrom-Json
if($lock.schema -cne 'MT8167_MUSB1121_HCD1019_INPUT_CLOSURE_V1' -or $lock.owner_ABI -ne 3){throw 'Unexpected closure/ABI'}
RequireHash $ParentSignedManifest $lock.provenance.parent1120.manifest_sha256
RequireHash $HcdSignedManifest $lock.provenance.hcd1018.manifest_sha256
$parent=Get-Content -LiteralPath $ParentSignedManifest -Raw|ConvertFrom-Json
$hcd=Get-Content -LiteralPath $HcdSignedManifest -Raw|ConvertFrom-Json
if($parent.source_commit -cne $lock.provenance.parent1120.source_commit -or
   $hcd.source_commit -cne $lock.provenance.hcd1018.source_commit){throw 'Signed baseline source drift'}
RequireHash (Join-Path $root $lock.provenance.hcd1018.evidence_path) $lock.provenance.hcd1018.recipe_sha256
function NormalizeParentArgs($baselineArgs){
 $result=@();for($i=0;$i -lt $baselineArgs.Count;$i++){
  $arg=$baselineArgs[$i];if($arg -ceq '-c'){break}
  if($arg -ceq '-isystem'){
   if(++$i -ge $baselineArgs.Count){throw 'Truncated signed baseline args'}
   $p=$baselineArgs[$i].Replace('\\','\')
   if($p.EndsWith('\km')){$token='@KM_INCLUDE@'}
   elseif($p.EndsWith('\shared')){$token='@SHARED_INCLUDE@'}
   elseif($p.EndsWith('\um')){$token='@UM_INCLUDE@'}
   elseif($p.EndsWith('\headers\wdf')){$token='@WDF_INCLUDE@'}
   else{throw 'Unknown signed baseline include path'}
   $result+=@('-isystem',$token)
  }else{$result+=$arg}
 };return $result
}
foreach($row in @($lock.compile|Where-Object{$_.kind -ceq 'parent'})){
 $signed=Value $parent.compiled_objects $row.name
 $normal=@(NormalizeParentArgs $signed.args)
 if($normal.Count -ne @($row.args).Count){throw 'Signed parent compile arg count drift'}
 for($i=0;$i -lt $normal.Count;$i++){if($normal[$i] -cne $row.args[$i]){throw 'Signed parent compile arg drift'}}
}
# Every new TU is hash-locked, including transitive project includes from all
# literal conditional branches. SDK hashes are the original1117 closure used
# by signed1120, not a new kit or a claim about the selected device driver.
foreach($row in $lock.compile){foreach($dep in $row.project_dependencies.PSObject.Properties){RequireHash (Join-Path $root $dep.Name) $dep.Value}}
foreach($row in $lock.host_sources){foreach($dep in $row.project_dependencies.PSObject.Properties){RequireHash (Join-Path $root $dep.Name) $dep.Value}}
foreach($row in $lock.reused_parent_helpers){
 RequireHash "$ReuseDirectory\parent\$($row.name).obj" $row.sha256
 foreach($dep in $row.dependencies.PSObject.Properties){RequireHash (Join-Path $root $dep.Name) $dep.Value}
}
if(@($lock.compile).Count -ne 12 -or @($lock.reused_parent_helpers).Count -ne 13){throw 'Exact12 compile/13 unchanged parent helper closure required'}
foreach($entry in $lock.sdk_headers.PSObject.Properties){
 $parts=$entry.Name -split '/',2
 $base=switch($parts[0]){
  'km'{Join-Path $KitInclude 'km'}
  'shared'{Join-Path $KitInclude 'shared'}
  'um'{$UmInclude}
  'wdf'{$WdfInclude}
  default{throw 'Unknown SDK header scope'}
 }
 RequireHash (Join-Path $base $parts[1]) $entry.Value
}
foreach($entry in $lock.kernel_libraries.PSObject.Properties){RequireHash (Join-Path $KernelLibraryDirectory $entry.Name) $entry.Value}
foreach($entry in $lock.wdf_libraries.PSObject.Properties){RequireHash "$ReuseDirectory\lib\wdf\$($entry.Name)" $entry.Value}
foreach($kind in @('parent','hcd')){
 $pkg=Value $lock.packages $kind;RequireHash (Join-Path $root $pkg.inf) $pkg.inf_sha256
 $version=Value $lock ($kind+'_version')
 if((Get-Content (Join-Path $root $pkg.inf) -Raw) -notmatch ('(?m)^DriverVer=11/10/2026,'+[regex]::Escape($version)+'\s*$')){throw 'INF candidate version drift'}
}
function MapArgs($argsIn){
 $a=@();foreach($x in $argsIn){
  $a+=$x.Replace('@KM_INCLUDE@',(Join-Path $KitInclude 'km')).Replace('@SHARED_INCLUDE@',(Join-Path $KitInclude 'shared')).Replace('@UM_INCLUDE@',$UmInclude).Replace('@WDF_INCLUDE@',$WdfInclude).Replace('@USB_SOURCE@',"$root\drivers\usb\mt8167-usb20")
 };return $a
}
New-Item -ItemType Directory "$out\tmp","$out\obj","$out\host","$out\evidence","$out\package\parent","$out\package\hcd"|Out-Null
$oldTmp=$env:TMP;$oldTemp=$env:TEMP;$transcript=$false
try{
 $env:TMP="$out\tmp";$env:TEMP=$env:TMP
 $commands=@();foreach($row in $lock.compile){$commands+=@{kind=$row.kind;name=$row.name;source=$row.source;args=@(MapArgs $row.args)+@('-MD','-MF',"$out\evidence\$($row.kind)-$($row.name).d",'-c',(Join-Path $root $row.source),'-o',"$out\obj\$($row.kind)-$($row.name).obj");project_dependencies=$row.project_dependencies}}
 @{schema='MT8167_MUSB1121_HCD1019_PLAN_V1';source_commit=$commit;closure_sha256=(Sha $lockPath);owner_ABI=3;commands=$commands;reused_helpers=$lock.reused_parent_helpers;build_requested=[bool]$BuildAndSign;SDK_setup=$false;deployment=$false;StageOnly=$false;DIF_INSTALLDEVICE=$false;source_ON_requested=$false;selected_device_driver_assumption='USB20 keyboard1010; HCD1018 adoption not assumed'}|ConvertTo-Json -Depth 30|Set-Content -Encoding UTF8 "$out\compile-plan.json"
 if(!$BuildAndSign){'INPUTS_VALIDATED_PLAN_ONLY_NO_COMPILER_SIGNER_OR_DEVICE_EXECUTED';return}
 Start-Transcript -Path "$out\evidence\build-sign.log"|Out-Null;$transcript=$true
 $clang=(Get-Command clang -ErrorAction Stop).Source;$bin=Split-Path $clang
 $link=Join-Path $bin 'lld-link.exe';$readobj=Join-Path $bin 'llvm-readobj.exe'
 foreach($tool in @($clang,$link,$readobj,$SignTool,$Inf2Cat)){if(!(Test-Path -LiteralPath $tool -PathType Leaf)){throw 'Existing tool unavailable; do not setup SDK/tools'}}
 $compiler=& $clang --version;if($LASTEXITCODE -or $compiler[0] -cne $lock.compiler_first_line){throw 'Use the pinned existing compiler, not a new setup'}
 $tools=@($clang,$link,$readobj,$SignTool,$Inf2Cat)|ForEach-Object{@{path=$_;sha256=(Sha $_)}}
 $compiled=@();foreach($c in $commands){
  $ca=@($c.args);& $clang @ca;if($LASTEXITCODE){throw "Compile failed: $($c.name)"}
  $obj="$out\obj\$($c.kind)-$($c.name).obj"
  $meta=& $readobj --file-headers $obj;if($LASTEXITCODE -or ($meta -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT object required'}
  $compiled+=@{kind=$c.kind;name=$c.name;source=$c.source;source_sha256=(Sha (Join-Path $root $c.source));object_sha256=(Sha $obj);args=$ca;project_dependencies=$c.project_dependencies;compiler_dependency_file_sha256=(Sha "$out\evidence\$($c.kind)-$($c.name).d")}
 }
 # Re-run the existing seven deterministic models with real WDK types on the
 # Windows host. These are x64 process tests, not kernel/USB hardware tests.
 $common=@('-w','-fms-extensions','-fno-stack-protector','-fno-builtin','-ffunction-sections','-D_KERNEL_MODE','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_MUSB_HOST_CORE_EXPERIMENT=1',"-I$KitInclude\km","-I$KitInclude\shared","-I$UmInclude","-I$WdfInclude",'-include','ntddk.h',"-I$root\drivers\usb\mt8167-usb20")
 $hostArgs=@('--target=x86_64-pc-windows-msvc','-D_AMD64_','-DAMD64','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13')+$common
 $hostObjects=@();foreach($name in @('root_hub','host_core','pio_transfer','ep0','ep0_pio','musb_async','controller','board_host')){
  $obj="$out\host\$name.obj";& $clang @hostArgs -c "$root\drivers\usb\mt8167-usb20\$name.c" -o $obj;if($LASTEXITCODE){throw 'Host model dependency compile failed'};$hostObjects+=$obj
 }
 $parentHost=@();foreach($src in @('drivers/bus/mt8167-shared/usb_owner.c','drivers/input/a04br3-buttons/pmic_read.c','drivers/usb/mt8167-usb20/gpio22.c')){
  $obj="$out\host\$([IO.Path]::GetFileNameWithoutExtension($src)).obj";& $clang @hostArgs -c (Join-Path $root $src) -o $obj;if($LASTEXITCODE){throw 'Parent host dependency compile failed'};$parentHost+=$obj
 }
 $passed=@();foreach($test in $lock.host_tests){
  $obj="$out\host\$test.obj";$exe="$out\host\$test.exe"
  & $clang @hostArgs -c "$root\tests\usb\$test.c" -o $obj;if($LASTEXITCODE){throw "Host test compile failed: $test"}
  $deps=if($test -ceq 'musb_parent_test'){$parentHost}else{$hostObjects}
  & $link /subsystem:console /machine:x64 /entry:main /nodefaultlib /opt:ref @deps $obj "/out:$exe";if($LASTEXITCODE){throw "Host test link failed: $test"}
  & $exe;if($LASTEXITCODE){throw "Host model failed: $test line$LASTEXITCODE"};$passed+=$test
 }
 $parentObjects=@("$out\obj\parent-parent.obj","$out\obj\parent-clock_snapshot.obj")+@($lock.reused_parent_helpers|ForEach-Object{"$ReuseDirectory\parent\$($_.name).obj"})
 $kernelLibs=@($lock.kernel_libraries.PSObject.Properties|ForEach-Object{Join-Path $KernelLibraryDirectory $_.Name})
 $wdfLibs=@("$ReuseDirectory\lib\wdf\wdfdriverentry.lib","$ReuseDirectory\lib\wdf\wdfldr.lib")
 & $link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @parentObjects @wdfLibs @kernelLibs "/out:$out\package\parent\mt8167-shared.sys" "/map:$out\evidence\parent.map"
 if($LASTEXITCODE){throw 'Parent1121 link failed'}
 @('LIBRARY USBPORT.SYS','EXPORTS',' USBPORT_GetHciMn',' USBPORT_RegisterUSBPortDriver')|Set-Content -Encoding ASCII "$out\obj\usbport.def"
 & $link /lib /machine:arm "/def:$out\obj\usbport.def" "/out:$out\obj\usbport81.lib";if($LASTEXITCODE){throw 'USBPORT import library generation failed'}
 $hcdObjects=@($commands|Where-Object{$_.kind -ceq 'hcd'}|ForEach-Object{"$out\obj\hcd-$($_.name).obj"})
 & $link /driver /subsystem:native /machine:arm /entry:GsDriverEntry /nodefaultlib /opt:noref @hcdObjects "$out\obj\usbport81.lib" @kernelLibs "/out:$out\package\hcd\mt8167-musb-hcd.sys" "/map:$out\evidence\hcd.map"
 if($LASTEXITCODE){throw 'HCD1019 link failed'}
 $imports=@{};foreach($kind in @('parent','hcd')){
  $pkg=Value $lock.packages $kind;$sys="$out\package\$kind\$($pkg.sys)"
  $pe=& $readobj --file-headers --coff-imports $sys
  if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'Linked ARMNT required'}
  $pe|Set-Content -Encoding UTF8 "$out\evidence\$kind-pe.txt"
  $table=@{};$dll=$null
  foreach($line in $pe){
   if($line -match '^\s*Name:\s+(\S+)'){$dll=$Matches[1].ToLowerInvariant();$table[$dll]=@()}
   if($line -match '^\s*Symbol:\s+(\S+)\s+\('){
    $symbol=$Matches[1];$actual=Value $lock.rt81_contract.actual_RT81_exports $dll
    $baseKind=Value $lock.rt81_contract.baseline $kind;$baseline=Value $baseKind.imports $dll
    $added=Value (Value $lock.rt81_contract.allowed_added_imports $kind) $dll
    if(!$actual -or $symbol -notin @($actual.symbols) -or ($symbol -notin @($baseline) -and $symbol -notin @($added))){throw "RT81/baseline import rejected: $dll $symbol"}
    $table[$dll]+=$symbol
   }
  }
  if(!$table.Count){throw 'Missing imports'};$imports[$kind]=$table
 }
 # The exact project closure is checked again after compilation and before
 # signing. No old-parent/new-HCD tuple or drifted source can be published.
 CheckClean
 foreach($row in $lock.compile){foreach($dep in $row.project_dependencies.PSObject.Properties){RequireHash (Join-Path $root $dep.Name) $dep.Value}}
 foreach($row in $lock.host_sources){foreach($dep in $row.project_dependencies.PSObject.Properties){RequireHash (Join-Path $root $dep.Name) $dep.Value}}
 foreach($row in $lock.reused_parent_helpers){RequireHash "$ReuseDirectory\parent\$($row.name).obj" $row.sha256}
 foreach($entry in $lock.kernel_libraries.PSObject.Properties){RequireHash (Join-Path $KernelLibraryDirectory $entry.Name) $entry.Value}
 foreach($entry in $lock.wdf_libraries.PSObject.Properties){RequireHash "$ReuseDirectory\lib\wdf\$($entry.Name)" $entry.Value}
 if(((& git -C $root rev-parse HEAD).Trim()) -cne $commit){throw 'Source commit changed during build'}
 $thumb=$lock.signer.ToUpperInvariant();$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
 if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing unexpired signer required; never export key'}
 $signed=@();foreach($kind in @('parent','hcd')){
  $pkg=Value $lock.packages $kind;$directory="$out\package\$kind";$sys=Join-Path $directory $pkg.sys;$cat=Join-Path $directory $pkg.cat
  RequireHash (Join-Path $root $pkg.inf) $pkg.inf_sha256
  Copy-Item -LiteralPath (Join-Path $root $pkg.inf) -Destination $directory
  $unsigned=Sha $sys
  & $SignTool sign /fd SHA256 /s My /sha1 $thumb $sys;if($LASTEXITCODE){throw 'SYS signing failed'}
  & $Inf2Cat "/driver:$directory" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat failed'}
  if(@(Get-ChildItem "$directory\*.cat").Count -ne 1 -or !(Test-Path -LiteralPath $cat)){throw 'INF-derived CAT mismatch'}
  & $SignTool sign /fd SHA256 /s My /sha1 $thumb $cat;if($LASTEXITCODE){throw 'CAT signing failed'}
  foreach($file in @($sys,$cat)){
   & $SignTool verify /pa /v $file
   $sig=Get-AuthenticodeSignature $file
   if($LASTEXITCODE -or $sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Signer/trust mismatch'}
  }
  foreach($file in @($sys,(Join-Path $directory ([IO.Path]::GetFileName($pkg.inf))))){& $SignTool verify /pa /v /c $cat $file;if($LASTEXITCODE){throw 'CAT membership failed'}}
  RequireHash (Join-Path $directory ([IO.Path]::GetFileName($pkg.inf))) $pkg.inf_sha256
  $signed+=@{kind=$kind;source_commit=$commit;owner_ABI=3;version=(Value $lock ($kind+'_version'));INF_source=$pkg.inf;INF_sha256=$pkg.inf_sha256;unsigned_SYS_sha256=$unsigned;SYS_sha256=(Sha $sys);CAT_sha256=(Sha $cat)}
 }
 @{schema='MT8167_MUSB1121_HCD1019_SIGNED_TUPLE_V1';source_commit=$commit;closure_sha256=(Sha $lockPath);owner_ABI=3;parent_version=$lock.parent_version;hcd_version=$lock.hcd_version;compiled_objects=$compiled;reused_parent_helpers=$lock.reused_parent_helpers;imports=$imports;baseline_provenance=$lock.provenance;host_models_passed=$passed;compiler=$compiler;tools=$tools;signer=$thumb;key_exported=$false;signed_tuple=$signed;SDK_setup=$false;HCD_objects_reused=0;StageOnly=$false;DIF_INSTALLDEVICE=$false;deployment_performed=$false;source_ON_requested=$false;device_selected_driver_observed=$false;device_selected_driver_user_report='keyboard1010';HCD1018_adoption_assumed=$false;hardware_verified=$false;physical_power_delivery_improved=$false;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 40|Set-Content -Encoding UTF8 "$out\signed-tuple-manifest.json"
 Compress-Archive "$out\package","$out\signed-tuple-manifest.json","$out\compile-plan.json" "$out\musb1121-hcd1019-signed-tuple.zip"
 CheckClean
}finally{
 if($transcript){Stop-Transcript|Out-Null};$env:TMP=$oldTmp;$env:TEMP=$oldTemp
}
if($BuildAndSign){
 Compress-Archive "$out\obj","$out\host","$out\evidence" "$out\musb1121-hcd1019-evidence.zip"
 Get-FileHash "$out\musb1121-hcd1019-signed-tuple.zip","$out\musb1121-hcd1019-evidence.zip" -Algorithm SHA256
 'MATCHED_ABI3_TUPLE_SIGNED_HOST_MODELS_ONLY_NOT_INSTALLED_OR_DEVICE_VERIFIED'
}
