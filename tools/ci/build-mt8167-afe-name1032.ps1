# 既存1027のKS名寿命のみ。固定audio cacheから依存差分TUだけbuild。親/HP/SDKはbuildしない。
param([Parameter(Mandatory=$true)][string]$ParentInputDirectory,
 [Parameter(Mandatory=$true)][string]$AudioCache,
 [Parameter(Mandatory=$true)][string]$KitIncludeDirectory,[Parameter(Mandatory=$true)][string]$PortclsLibrary,
 [Parameter(Mandatory=$true)][string]$SignTool,[Parameter(Mandatory=$true)][string]$Inf2Cat,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$parent=Get-Content "$ParentInputDirectory\cache\parent-cache.json" -Raw|ConvertFrom-Json
$old=Get-Content "$AudioCache\package\signed-manifest.json" -Raw|ConvertFrom-Json
$ac=Get-Content "$AudioCache\object-cache-manifest.json" -Raw|ConvertFrom-Json
$base='81f20c785cebf7e7a65778a3b3a10687bb6bcb54'
if($old.source_commit -cne $base -or $ac.source_commit -cne $base){throw 'Fixed final1027 audio cache required'}
if(((& clang --version)[0]) -cne (($parent.compiler -split "`n")[0])){throw 'Compiler drift'}
foreach($f in $old.files){if((Sha (Join-Path "$AudioCache\package" $f.path)) -cne $f.sha256){throw 'HP package drift'}}
function Closure-Same($source,$base){
 $todo=[Collections.Generic.Queue[string]]::new();$todo.Enqueue([IO.Path]::GetFullPath($source));$seen=@{}
 while($todo.Count){$p=$todo.Dequeue();if($seen.ContainsKey($p)){continue};$seen[$p]=$true
  if(!$p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Include escaped source'}
  $rel=$p.Substring($root.Length+1).Replace('\','/')
  $a=& git -C $root rev-parse "$($base):$rel";if($LASTEXITCODE){return $false}
  $b=& git -C $root rev-parse "HEAD:$rel";if($LASTEXITCODE -or $a -cne $b){return $false}
  foreach($m in [regex]::Matches((Get-Content $p -Raw),'(?m)^\s*#\s*include\s*"([^"]+)"')){
   $dep=[IO.Path]::GetFullPath((Join-Path (Split-Path $p) $m.Groups[1].Value));if(!(Test-Path $dep)){throw 'Missing dependency'};$todo.Enqueue($dep)
  }
 };return $true
}
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package\audio"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$inc=$KitIncludeDirectory;$wdk="$ParentInputDirectory\wdk";$km="$wdk\Lib\winv6.3\km\arm"
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$inc\km",'-isystem',"$inc\shared",'-isystem',"$inc\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$flags=@($old.baseline_audio_flags)+@('-DA04_AUDIO_ACCDET_JACK=1')

foreach($prop in $parent.sdk.PSObject.Properties){
 $p=$null;$oldPath=$prop.Name
 if($oldPath -match '\\Include\\(.+)$'){$tail=$Matches[1];$p=if($tail.StartsWith('wdf\')){"$wdk\Include\$tail"}else{"$inc\$tail"}}
 elseif($oldPath -match '\\Lib\\(.+)$'){$p="$wdk\Lib\$($Matches[1])"}
 if($p -and (Test-Path $p) -and (Sha $p) -cne $prop.Value.ToLowerInvariant()){throw 'WDK SDK identity drift'}
}
$common=@($parent.common|Where-Object{$_ -ne '-std=c11'})
$objects=@();$reused=@();$compiled=@();$audioRoot="$root\drivers\audio\mt8167-afe"
foreach($name in @('dl1','dl1_engine','shared_audio','audio_parent_client','audio_parent_query','capture_parent_query','ul_engine','clock_owner','adapter','wavert_miniport','wavert_registration','wavert_dma','common_dma','dma_adapter','capture_latency_gate','stream_transport','pnp_backend','afe_mapping','irq_windows','wave_dma_live','topology_miniport','start_record','capture_owner')){
 $cpp=$name -in @('adapter','wavert_miniport','wavert_registration','wavert_dma','common_dma','dma_adapter','capture_latency_gate','stream_transport','pnp_backend','afe_mapping','irq_windows','wave_dma_live','topology_miniport','start_record','capture_owner')
 $source=if($name -eq 'clock_owner'){"$root\drivers\bus\mt8167-shared\clock_owner.c"}else{"$audioRoot\$name.$(if($cpp){'cpp'}else{'c'})"}
 $base=$ac.source_commit
 $cached="$AudioCache\obj\$name.obj"
 {$x=@($ac.objects|Where-Object{$_.name -ceq "$name.obj"});if($x.Count -ne 1 -or (Sha $cached) -cne $x[0].sha256.ToLowerInvariant()){throw 'Audio helper cache mismatch'}}
 if(Closure-Same $source $base){$objects+=$cached;$reused+=@{name=$name;source_commit=$base;sha256=(Sha $cached)};continue}
 $obj="$out\obj\$name.obj";$lang=if($cpp){@('-std=c++14','-fms-compatibility','-fno-exceptions','-fno-rtti')}else{@('-std=c11')}
 & clang @common @kernel @flags @lang -w -c $source -o $obj
 if($LASTEXITCODE){throw "Compile failed $name"};$objects+=$obj;$compiled+=$name
}
$sys="$out\package\audio\mt8167-audio.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects $PortclsLibrary "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Audio link failed'}
Copy-Item "$audioRoot\mt8167-afe-jack1032.inf" "$out\package\audio"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer required'}
$unsigned=Sha $sys
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& $Inf2Cat "/driver:$out\package\audio" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat failed'}
$cat="$out\package\audio\mt8167-afe-jack1032.cat"
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($p in @($sys,$cat)){& $SignTool verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature mismatch'}}
foreach($p in @($sys,"$out\package\audio\mt8167-afe-jack1032.inf")){& $SignTool verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}}
$pe=& llvm-readobj --file-headers --coff-imports $sys
if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'};$pe|Set-Content -Encoding UTF8 ($sys+'.pe.txt')
@{schema='mt8167-afe-name1032';source_commit=$commit;audio_version='2026.11.8.1032';baseline_audio=$base;signed=$true;hardware_verified=$false;parent_rebuilt=$false;audio_flags=$flags;compiled=$compiled;reused=$reused;unsigned_sys_sha256=$unsigned;signer_thumbprint=$thumb;headphone_added=$false;subdevice_capacity=4;parent_accdet_schema=1;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\afe-name1032-signed.zip"
Compress-Archive "$out\obj\*" "$out\afe-name1032-objects.zip"
Get-FileHash "$out\afe-name1032-signed.zip","$out\afe-name1032-objects.zip" -Algorithm SHA256
'MT8167_AFE_NAME1032_SIGNED_NOT_DEVICE_TESTED'
