# WDDM MDL ownership + copy334 low-level closure; WDDM consumer wiring remains separate.
param([Parameter(Mandatory=$true)][string]$TaskRoot,[Parameter(Mandatory=$true)][string]$SourceCommit)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Pinned input mismatch: $p"}}
$root="$TaskRoot\source";$base="$TaskRoot\baseline";$out="$TaskRoot\build";$manifest=Get-Content "$TaskRoot\compile-input.json" -Raw|ConvertFrom-Json
if(Test-Path $out){throw 'New integration build output required'}
foreach($e in $manifest.baseline_archives.PSObject.Properties){Pin "$base\$($e.Name)" $e.Value}
New-Item -ItemType Directory $out,"$out\before","$out\native","$out\tmp"|Out-Null
$llvm='C:\Program Files\LLVM\bin';$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP="$out\tmp";$env:TEMP=$env:TMP
 $members=@(& "$llvm\llvm-ar.exe" t "$base\ge8300-native-owner-core.lib"|ForEach-Object{[IO.Path]::GetFileName($_)})
 if($LASTEXITCODE -or @($members|Sort-Object -Unique).Count -ne 61 -or $members.Count -ne 61){throw 'Exact61 unique native members'}
 & "$llvm\llvm-ar.exe" x "--output=$out\before" "$base\ge8300-native-owner-core.lib";if($LASTEXITCODE){throw 'Extract native baseline'}
 foreach($m in $manifest.native_members){Pin "$out\before\$($m.name)" $m.sha256;Copy-Item "$out\before\$($m.name)" "$out\native\$($m.name)"}
 $compiled=@();foreach($u in $manifest.compiled_units){
  $file=Join-Path $root $u.source;Pin $file $u.sha256
  $flags=@($u.arguments|ForEach-Object{([string]$_).Replace('\\','\').Replace('C:\mt8167-readback-safety332\source',$root)})
  & "$llvm\clang.exe" @flags -c $file -o "$out\native\$($u.object)";if($LASTEXITCODE){throw "Actual ARM compile failed: $($u.source)"}
  $compiled+=@{source=$u.source;source_sha256=(Sha $file);object=$u.object;sha256=(Sha "$out\native\$($u.object)");arguments=$flags}
 }
 $rows=@();foreach($name in $members){$a=Sha "$out\before\$name";$b=Sha "$out\native\$name";if($name -cnotin @($manifest.compiled_units.object) -and $a -cne $b){throw 'Unrelated native byte change'};$rows+=@{name=$name;before_sha256=$a;sha256=$b;compiled=($name -cin @($manifest.compiled_units.object))}}
 $expected=@($members)+@($manifest.compiled_units.object|Where-Object{$_ -cnotin $members})
 $paths=@($expected|ForEach-Object{"$out\native\$_"});[array]::Reverse($paths)
 & "$llvm\llvm-lib.exe" /machine:arm "/out:$out\ge8300-native-owner-core.lib" @paths;if($LASTEXITCODE){throw 'Native archive'}
 $made=@(& "$llvm\llvm-ar.exe" t "$out\ge8300-native-owner-core.lib"|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(($made -join "`n") -cne ($expected -join "`n")){throw 'Native member order changed'}
 $link=(Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\arm\link.exe'|Select-Object -First 1).FullName
 if(!$link){throw 'Existing MS ARM linker required'}
 $kit='C:\mt8167-public-fullsource323\wdk81';$libs=@('displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk')|ForEach-Object{"$kit\Lib\winv6.3\km\arm\$_.lib"}
 & $link /nologo /driver /subsystem:native,6.03 /machine:arm /entry:DriverEntry /nodefaultlib /incremental:no /base:0x400000 "$base\ge8300-pair-kmd.lib" "$out\ge8300-native-owner-core.lib" @libs "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map";if($LASTEXITCODE){throw 'Integration SYS link'}
 $sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 & $sign sign /fd SHA256 /s My /sha1 $thumb "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'BCBF SYS sign'}
 & $sign verify /pa "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS trust verify'}
 $sig=Get-AuthenticodeSignature "$out\ge8300-render.sys";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Exact signer mismatch'}
 & "$llvm\llvm-readobj.exe" --file-headers --coff-imports "$out\ge8300-render.sys"|Set-Content "$out\imports.txt";if($LASTEXITCODE){throw 'PE inspect'}
 [ordered]@{schema='copy334-wddm-physical-aperture-integration-only-v1';copy_consumer_base='0bfc91cf9a87c98b0ca1d540d00726159614072d';copy_lowlevel_connected=$true;WDDM_copy_frontend_connected=$false;submit_gate_released=$false;native_member_count=$made.Count;source_commit=$SourceCommit;compiled_units=$compiled;native_members=$rows;retained_native_members=@($rows|Where-Object{!$_.compiled}).Count;native_archive_sha256=(Sha "$out\ge8300-native-owner-core.lib");retained_kmd_archive_sha256=(Sha "$base\ge8300-pair-kmd.lib");sys_sha256=(Sha "$out\ge8300-render.sys");signature_status=[string]$sig.Status;signer_thumbprint=$thumb;integration_only=$true;device_verified=$false;reset_root_cause_proven=$false;catalog_generated=$false;UMD_rebuilt=$false;SDK_rebuilt=$false;PCO_rebuilt=$false;DMA_api='WDDM1.x MAP_APERTURE_SEGMENT locked physical MDL/PFN; no HAL packet mapping';opaque_SG_Reserved_used=$false;temporary_MMU_revoke_required=$false;WDDM_residency_lease_retained=$true}|ConvertTo-Json -Depth 14|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 Compress-Archive -Path "$out\native","$out\ge8300-render.sys","$out\ge8300-native-owner-core.lib","$out\ge8300-render.map","$out\build-manifest.json","$out\imports.txt" -DestinationPath "$TaskRoot\outputs.zip"
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
'COPY334_WDDM_APERTURE_SIGNED_INTEGRATION_ONLY'
