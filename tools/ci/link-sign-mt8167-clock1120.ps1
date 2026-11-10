# Fixed-source incremental parent1120/thermal1.11 only. No driver install,
# SDK setup, shared-cache mutation, signing-key export or device operations.
param([Parameter(Mandatory=$true)][string]$InitialCompileDirectory,
 [Parameter(Mandatory=$true)][string]$ParentRecompileDirectory,
 [Parameter(Mandatory=$true)][string]$ReuseDirectory,
 [Parameter(Mandatory=$true)][string]$KernelLibraryDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$Inf2Cat,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out) -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source/new checkout-local output required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$commit=(& git -C $root rev-parse HEAD).Trim()
$reuse=Get-Content "$PSScriptRoot\clock-snapshot-reuse1120.json" -Raw|ConvertFrom-Json
$rt=Get-Content "$PSScriptRoot\clock-snapshot-rt81-contract.json" -Raw|ConvertFrom-Json
$compiles=@{}
foreach($dir in @($InitialCompileDirectory,$ParentRecompileDirectory)){
 $m=Get-Content "$dir\compile-recipe.json" -Raw|ConvertFrom-Json
 if(!$m.compiled){throw 'Real compiled inputs required'}
 foreach($c in $m.commands){
  if((Sha (Join-Path $root $c.source)) -cne $c.source_sha256){throw 'Compiled runtime source drift'}
  $name=[IO.Path]::GetFileNameWithoutExtension($c.args[-1])
  $compiles[$name]=@{path=$c.args[-1];source_commit=$m.source_commit;source=$c.source;source_sha256=$c.source_sha256;args=$c.args}
 }
}
# The corrected parent/sampler profile must preserve the actual1117 safe flags.
foreach($n in @('parent','clock_snapshot','parent-flag-off')){
 if(!$compiles.ContainsKey($n)){throw 'Missing corrected object'}
 foreach($f in @('-O2','-mfpu=none','-mno-unaligned-access','-fno-builtin')){
  if($f -notin $compiles[$n].args){throw 'Parent1117 compiler profile not preserved'}
 }
}
$objects=@{parent=@();thermal=@()};$provenance=@()
foreach($x in $reuse.objects){
 $p="$ReuseDirectory\$($x.scope)\$($x.name).obj"
 if((Sha $p) -cne $x.sha256){throw 'Unchanged helper object mismatch'}
 foreach($d in $x.dependencies.PSObject.Properties){
  if((Sha (Join-Path $root $d.Name)) -cne $d.Value){throw 'Helper include/source drift'}
 }
 $objects[$x.scope]+=$p;$provenance+=$x
}
if($objects.parent.Count -ne 13 -or $objects.thermal.Count -ne 3){throw 'Exact13parent/3thermal helpers required'}
foreach($lib in $reuse.kernel_libraries.PSObject.Properties){
 if((Sha (Join-Path $KernelLibraryDirectory $lib.Name)) -cne $lib.Value){throw 'Existing kernel library drift'}
}
foreach($lib in $reuse.wdf_libraries.PSObject.Properties){
 if((Sha "$ReuseDirectory\lib\wdf\$($lib.Name)") -cne $lib.Value){throw 'Existing WDF library drift'}
}
New-Item -ItemType Directory "$out\package\parent","$out\package\thermal","$out\evidence","$out\tmp"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$libs=@("$ReuseDirectory\lib\wdf\wdfdriverentry.lib","$ReuseDirectory\lib\wdf\wdfldr.lib")+
 @($reuse.kernel_libraries.PSObject.Properties|ForEach-Object{Join-Path $KernelLibraryDirectory $_.Name})
$objects.parent=@($compiles.parent.path,$compiles.clock_snapshot.path)+$objects.parent
$objects.thermal=@($compiles.wdf_native_thermal.path,$compiles.parent_snapshot_client.path)+$objects.thermal
$names=@{parent='mt8167-shared';thermal='mt8167-thermal'}
$imports=@{};$signedFiles=@()
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
foreach($kind in @('parent','thermal')){
 $sys="$out\package\$kind\$($names[$kind]).sys"
 $kindObjects=@($objects[$kind])
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @kindObjects @libs "/out:$sys" "/map:$out\evidence\$kind.map"
 if($LASTEXITCODE){throw "Incremental link failed: $kind"}
 $pe=& llvm-readobj --file-headers --coff-imports $sys
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
 $pe|Set-Content -Encoding UTF8 "$out\evidence\$kind-pe.txt"
 $table=@{};$dll=$null
 foreach($line in $pe){
  if($line -match '^\s*Name:\s+(\S+)'){$dll=$Matches[1].ToLowerInvariant();$table[$dll]=@()}
  if($line -match '^\s*Symbol:\s+(\S+)\s+\('){
   $symbol=$Matches[1]
   if(!$dll -or !$rt.actual_RT81_exports.$dll -or $symbol -notin $rt.actual_RT81_exports.$dll.symbols -or
      ($symbol -notin $rt.baseline.$kind.imports.$dll -and $symbol -notin $rt.allowed_added_imports.$kind.$dll)){throw "Actual RT81/baseline import mismatch: $dll $symbol"}
   $table[$dll]+=$symbol
  }
 }
 if(!$table.Count){throw 'Missing import metadata'}
 $imports[$kind]=$table
}
# Package sources have new versions; original1117/1.10 INF/SYS/CAT stay intact.
Copy-Item "$root\drivers\bus\mt8167-shared\prototype\shared-parent-clock1120.inf" "$out\package\parent"
Copy-Item "$root\drivers\power\mt8167-thermal\mt8167-thermal-clock111.inf" "$out\package\thermal"
$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey){throw 'Existing signer required; never export key'}
foreach($kind in @('parent','thermal')){
 $sys="$out\package\$kind\$($names[$kind]).sys";$unsigned=Sha $sys
 & $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
 & $Inf2Cat "/driver:$out\package\$kind" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 $cat=(Get-ChildItem "$out\package\$kind\*.cat").FullName
 if(@($cat).Count -ne 1){throw 'Exactly one CAT required'}
 & $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($p in @($sys,$cat)){
  & $SignTool verify /pa /v $p
  if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature/trust mismatch'}
 }
 foreach($p in @($sys,(Get-ChildItem "$out\package\$kind\*.inf").FullName)){
  & $SignTool verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}
 }
 $signedFiles+=@{kind=$kind;unsigned_sha256=$unsigned;SYS_sha256=(Sha $sys);CAT_sha256=(Sha $cat)}
}
@{schema='MT8167_CLOCK1120_INCREMENTAL_SIGNED_V1';source_commit=$commit;
 parent_version='2026.11.10.1120';thermal_version='0.0.1.11';clock_query_ABI=1;resource_contract=1117;
 thermal_v3_build=1104;HP_v2_build=1107;read_only=$true;new_mapping_count=0;hardware_writes=0;
 reused_helpers=$provenance;compiled_objects=$compiles;imports=$imports;actual_RT81_contract=$rt;
 signer=$thumb;key_exported=$false;signed_files=$signedFiles;
 actual_clock_observed=$false;effective_CPU_Hz=$null;DDR_Hz=$null;hardware_verified=$false;deployment_performed=$false;
 files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})
}|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\clock1120-parent-thermal-signed.zip"
Compress-Archive "$out\evidence\*" "$out\clock1120-link-evidence.zip"
Get-FileHash "$out\clock1120-parent-thermal-signed.zip","$out\clock1120-link-evidence.zip" -Algorithm SHA256
'CLOCK1120_SIGNED_NOT_DEVICE_OR_FREQUENCY_VERIFIED'
