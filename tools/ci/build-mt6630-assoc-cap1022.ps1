# Only source/include-dependent objects of fixed1020 cache; no SDK setup or device use.
param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$PreviousOutputDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$cm=Get-Content "$Inputs\closure-manifest.json" -Raw|ConvertFrom-Json
foreach($f in $cm.files){if((Sha (Join-Path $Inputs $f.path)) -cne $f.sha256){throw 'Input closure hash mismatch'}}
$old=Get-Content "$PreviousOutputDirectory\package\signed-manifest.json" -Raw|ConvertFrom-Json
if($old.source_commit -cne '37d1ac179f6ae549fb068ebb8f7e3eb1a619066b'){throw 'Fixed1022 source cache required'}
function ClosureChanged($source){
 $todo=[Collections.Generic.Queue[string]]::new();$todo.Enqueue([IO.Path]::GetFullPath($source));$seen=@{}
 while($todo.Count){$p=$todo.Dequeue();if($seen.ContainsKey($p)){continue};$seen[$p]=$true
  if(!$p.StartsWith($root+'\')){throw 'Source include escaped'};$rel=$p.Substring($root.Length+1).Replace('\','/')
  $a=& git -C $root rev-parse "$($old.source_commit):$rel" 2>$null
  if($LASTEXITCODE){return $true};$b=& git -C $root rev-parse "HEAD:$rel"
  if($LASTEXITCODE -or $a -cne $b){return $true}
  foreach($m in [regex]::Matches((Get-Content $p -Raw),'(?m)^\s*#\s*include\s*"([^"]+)"')){$q=[IO.Path]::GetFullPath((Join-Path (Split-Path $p) $m.Groups[1].Value));if(Test-Path $q){$todo.Enqueue($q)}}
 };return $false
}
$oldRoot='C:\mt6630-stockpcm1019-cache-42429390679\restore'
$flags=@($old.ndis_flags|ForEach-Object{$_.Replace('\\','\').Replace($oldRoot,$Inputs)})
$coreFlags=@($old.compiler_flags|ForEach-Object{$_.Replace('\\','\').Replace($oldRoot,$Inputs)})
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package"|Out-Null;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$objects=@();$compiled=@();$reused=@();$objectHashes=@()
foreach($r in $old.objects){
 $p=$r.object
 if((Sha $p) -cne $r.sha256.ToLowerInvariant()){throw 'Cached object mismatch'}
 if(ClosureChanged "$root\$($r.source)"){
  $p="$out\obj\$([IO.Path]::GetFileName($r.object))";$args=if($r.object.Replace('\','/').Contains('/core/')){$coreFlags}else{$flags}
  & clang @args -c "$root\$($r.source)" -o $p;if($LASTEXITCODE){throw "Compile failed $($r.source)"};$compiled+=$r.source
 }else{$reused+=$r.source}
 $objects+=$p;$objectHashes+=@{source=$r.source;object=$p;sha256=(Sha $p)}
}
if($objects.Count -ne 41 -or $compiled.Count -ne 2 -or $reused.Count -ne 39){throw 'Expected2-compile/39-reuse standalone closure'}
$sys="$out\package\mt6630-runtime.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref @objects "$Inputs\km\ndis.lib" "$Inputs\km\ntoskrnl.lib" "$Inputs\km\hal.lib" "$Inputs\km\bufferoverflowfastfailk.lib" "$Inputs\km\libcntpr.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Standalone WiFi link failed'}
Copy-Item "$root\drivers\network\mt6630\mt6630-runtime.inf" "$out\package"
$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$catTool='C:\mt8167-hp1033\inputs\signing\bin\Inf2Cat.exe'
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb";if(!$cert.HasPrivateKey){throw 'Existing signer required'}
$unsigned=Sha $sys
& $sign sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS signing failed'}
& $catTool "/driver:$out\package" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
$cat="$out\package\mt6630-runtime.cat"
& $sign sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT signing failed'}
foreach($p in @($sys,$cat)){& $sign verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature identity failed'}}
foreach($p in @($sys,"$out\package\mt6630-runtime.inf")){& $sign verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT member failed'}}
$pe=& llvm-readobj --file-headers --coff-imports $sys;if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'};$pe|Set-Content -Encoding UTF8 "$out\package\mt6630-runtime.pe.txt"
@{schema='mt6630-assoc-cap1022';source_commit=$commit;baseline_source=$old.source_commit;version='2026.11.8.1022';checkpoint=1022;compiled=$compiled;reused=$reused;objects=$objectHashes;compiler_flags=$coreFlags;ndis_flags=$flags;signed=$true;signer_thumbprint=$thumb;unsigned_sys_sha256=$unsigned;hardware_verified=$false;association_verified=$false;communication_verified=$false;parent_rebuilt=$false;audio1033_modified=$false;sdk_rebuilt=$false;files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{path=$_.Name;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\wifi1022-signed.zip"
Compress-Archive "$out\obj\*" "$out\wifi1022-objects.zip"
Get-FileHash "$out\wifi1022-signed.zip","$out\wifi1022-objects.zip" -Algorithm SHA256
'MT6630_ASSOC1022_SIGNED_NOT_DEVICE_TESTED'
