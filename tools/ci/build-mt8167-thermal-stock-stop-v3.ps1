# thermal停止proof v3。親1TUだけ増分、他13helperは固定source/hashで再利用。
param([Parameter(Mandatory=$true)][string]$ParentInputDirectory,
 [Parameter(Mandatory=$true)][string]$KitIncludeDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$Inf2Cat,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$cache=Get-Content "$ParentInputDirectory\cache\parent-cache.json" -Raw|ConvertFrom-Json
if($cache.source_commit -cne '1d248a71a95360966ff867e4b063504ac764e074'){throw 'Fixed parent cache required'}
if(((& clang --version)[0]) -cne (($cache.compiler -split "`n")[0])){throw 'Compiler version drift'}
# Walk all repo-local quoted includes of reused C objects. Header/source
# identity against their recorded source commit is required, not filename reuse.
function Check-Closure($source,$base){
 $todo=[Collections.Generic.Queue[string]]::new();$todo.Enqueue([IO.Path]::GetFullPath($source));$seen=@{}
 while($todo.Count){
  $path=$todo.Dequeue();if($seen.ContainsKey($path)){continue};$seen[$path]=$true
  if(!$path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Local include escaped repo'}
  $rel=$path.Substring($root.Length+1).Replace('\','/')
  $old=(& git -C $root rev-parse "$($base):$rel");if($LASTEXITCODE){throw 'Missing baseline dependency'}
  $new=(& git -C $root rev-parse "HEAD:$rel");if($LASTEXITCODE -or $old -cne $new){throw "Reused dependency changed $rel"}
  foreach($m in [regex]::Matches((Get-Content $path -Raw),'(?m)^\s*#\s*include\s*"([^"]+)"')){
   $dep=[IO.Path]::GetFullPath((Join-Path (Split-Path $path) $m.Groups[1].Value))
   if(!(Test-Path $dep)){throw "Missing local include $dep"};$todo.Enqueue($dep)
  }
 }
 return @($seen.Keys|Sort-Object|ForEach-Object{$_.Substring($root.Length+1).Replace('\','/')})
}
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package\parent"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$inc=$KitIncludeDirectory;$wdk="$ParentInputDirectory\wdk";$km="$wdk\Lib\winv6.3\km\arm";$wdf="$wdk\Lib\wdf\kmdf\arm\1.13"
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$inc\km",'-isystem',"$inc\shared",'-isystem',"$inc\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$parentFlags=@($cache.flags)+@('-DA04_SHARED_ACCDET_READ=1','-DA04_SHARED_HP_PCM=1')

$reused=@();$compiled=@();$sdk=@();$sdkAbsent=@()
foreach($prop in $cache.sdk.PSObject.Properties){
 $old=$prop.Name;$p=$null
 if($old -match '\\Include\\(.+)$'){
  $tail=$Matches[1];$p=if($tail.StartsWith('wdf\')){"$wdk\Include\$tail"}else{"$inc\$tail"}
 }elseif($old -match '\\Lib\\(.+)$'){$p="$wdk\Lib\$($Matches[1])"}
 if($p -and !(Test-Path $p)){$sdkAbsent+=$p;continue}
 if($p){if((Sha $p) -cne $prop.Value.ToLowerInvariant()){throw 'WDK cache identity mismatch'};$sdk+=@{path=$p;sha256=(Sha $p)}}
}
$parentObjects=@()
foreach($x in $cache.objects){
 if($x.name -eq 'parent'){continue};$p="$ParentInputDirectory\cache\obj\$($x.name).obj"
 if((Sha $p) -cne $x.sha256.ToLowerInvariant()){throw 'Parent helper hash mismatch'}
 $source=if($x.name -eq 'pmic_read'){"$root\drivers\input\a04br3-buttons\pmic_read.c"}elseif($x.name -eq 'gpio22'){"$root\drivers\usb\mt8167-usb20\gpio22.c"}elseif($x.name -eq 'sd_power_owner'){"$root\drivers\storage\mt8167-sd\power_owner.c"}else{"$root\drivers\bus\mt8167-shared\$($x.name).c"}
 $deps=Check-Closure $source $cache.source_commit
 $parentObjects+=$p;$reused+=@{kind='parent';name=$x.name;sha256=(Sha $p);dependencies=$deps;source_commit=$cache.source_commit}
}
if($parentObjects.Count -ne 13){throw 'Exactly13 parent helpers required'}
$parentObj="$out\obj\parent.obj"
& clang @($cache.common) @kernel @parentFlags -w -c "$root\drivers\bus\mt8167-shared\wdf_parent_probe.c" -o $parentObj
if($LASTEXITCODE){throw 'HP parent compile failed'};$compiled+='parent'
$parentObjects=@($parentObj)+$parentObjects
$libs=@("$km\ntoskrnl.lib","$km\hal.lib","$km\bufferoverflowfastfailk.lib","$km\libcntpr.lib")
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @parentObjects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" @libs "/out:$out\package\parent\mt8167-shared.sys"
if($LASTEXITCODE){throw 'HP parent link failed'}
Copy-Item "$root\drivers\bus\mt8167-shared\prototype\shared-parent-hp1107.inf" "$out\package\parent"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing BCBF signer required'}
$unsigned=@()
foreach($f in @("$out\package\parent\mt8167-shared.sys")){
 $unsigned+=@{path=$f.Substring($out.Length+9);sha256=(Sha $f)}
 & $SignTool sign /fd SHA256 /sha1 $thumb /s My $f;if($LASTEXITCODE){throw 'PE sign failed'}
 & $SignTool verify /pa /v $f;if($LASTEXITCODE -or (Get-AuthenticodeSignature $f).SignerCertificate.Thumbprint -cne $thumb){throw 'PE signature mismatch'}
 $pe=& llvm-readobj --file-headers --coff-imports $f
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
 $pe|Set-Content -Encoding UTF8 ($f+'.pe.txt')
}
foreach($kind in @('parent')){
 $dir="$out\package\$kind"
 & $Inf2Cat "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Catalog generation failed'}
 $cat=@(Get-ChildItem $dir -Filter '*.cat');if($cat.Count -ne 1){throw 'One CAT required'}
 & $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat[0].FullName;if($LASTEXITCODE){throw 'CAT sign failed'}
 & $SignTool verify /pa /v $cat[0].FullName
 if($LASTEXITCODE -or (Get-AuthenticodeSignature $cat[0].FullName).SignerCertificate.Thumbprint -cne $thumb){throw 'CAT signature mismatch'}
 foreach($f in Get-ChildItem $dir -File|Where-Object{$_.Extension -in @('.sys','.inf')}){
  & $SignTool verify /pa /v /c $cat[0].FullName $f.FullName;if($LASTEXITCODE){throw 'CAT membership failed'}
 }
}
@{schema='mt8167-thermal-stock-stop-v3';source_commit=$commit;parent_version='2026.11.8.1107';candidate_only=$true;signed=$true;hardware_verified=$false;thermal_record_versions=@(2,3);thermal_interface_build=1104;hp_interface_build=1107;audio_rebuilt=$false;compiled=$compiled;reused=$reused;sdk=$sdk;signer_thumbprint=$thumb;unsigned=$unsigned;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\thermal-stock-stop-v3-signed.zip"
Compress-Archive "$out\obj\*" "$out\thermal-stock-stop-v3-objects.zip"
Get-FileHash "$out\thermal-stock-stop-v3-signed.zip","$out\thermal-stock-stop-v3-objects.zip"
