# One native TU/archive member, existing327 KMD/UMD/PCO/SDK unchanged.
param([Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$KernelKitRoot,[Parameter(Mandatory=$true)][string]$MicrosoftLinkPath,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$commit=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Baseline pin mismatch $p"}}
$base=[IO.Path]::GetFullPath($BaselineBuildDirectory);$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'New checkout-local output'}
Pin "$base\ge8300-native-owner-core.lib" '42144323b4393e9e419b8b90e077f33da9ba97b843eda507112ebc981a74f187'
Pin "$base\ge8300-pair-kmd.lib" 'cd628e21645e3874ade711ebfc850629314a73403136f6cb4fba320ff29b1ebf'
Pin "$base\ge8300-render.sys" '4392efe1d9ad1bf8d254db3f3565cf9d7730c0f4edbfae0960b53e23ed63146a'
Pin "$base\build-manifest.json" '1c74110363c68d2270513d3754f06f554632b36d184fa3803290f8cd07fc47c6'
New-Item -ItemType Directory $out,"$out\tmp","$out\before","$out\native"|Out-Null
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP="$out\tmp";$env:TEMP=$env:TMP
 $manifest=Get-Content "$base\build-manifest.json" -Raw|ConvertFrom-Json
 $flags=@(($manifest.compiled_units|Where-Object {$_.role -ceq 'native'}|Select-Object -First 1).arguments|ForEach-Object {([string]$_).Replace('C:\mt8167-texture327\source',$root).Replace('C:\mt8167-texture326\source',$root).Replace('C:\mt8167-blacktriangle325\source',$root)})
 $members=@(& llvm-ar t "$base\ge8300-native-owner-core.lib"|ForEach-Object {[IO.Path]::GetFileName($_)})
 if($LASTEXITCODE -or @($members|Sort-Object -Unique).Count -ne $members.Count){throw 'Unique archive members required'}
 & llvm-ar x "--output=$out\before" "$base\ge8300-native-owner-core.lib";if($LASTEXITCODE){throw 'Archive extract'}
 foreach($member in $members){Copy-Item "$out\before\$member" "$out\native\$member"}
 $changed='vendor_m1_11-native_owner_mdl.obj';$source="$root\drivers\gpu\mt8167\vendor_m1_11\render_owner\native_owner_mdl.c"
 if($changed -notin $members){throw 'Exact native MDL member required'}
 & clang @flags -c $source -o "$out\native\$changed";if($LASTEXITCODE){throw 'ARM native MDL TU compile'}
 $rows=@();foreach($member in $members){$before=Sha "$out\before\$member";$after=Sha "$out\native\$member";if($member -cne $changed -and $before -cne $after){throw 'Unrelated object change'};$rows+=@{name=$member;before_sha256=$before;sha256=$after;changed=($before -cne $after)}}
 if(@($rows|Where-Object {$_.changed}).Count -ne 1){throw 'Exactly one changed native member'}
 $paths=@($members|ForEach-Object {"$out\native\$_"});[array]::Reverse($paths)
 & llvm-lib /machine:arm "/out:$out\ge8300-native-owner-core.lib" @paths;if($LASTEXITCODE){throw 'Native archive'}
 $made=@(& llvm-ar t "$out\ge8300-native-owner-core.lib"|ForEach-Object {[IO.Path]::GetFileName($_)})
 if(($made -join "`n") -cne ($members -join "`n")){throw 'Archive order/shape changed'}
 $kit=[IO.Path]::GetFullPath($KernelKitRoot);$libs=@('displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk')|ForEach-Object {"$kit\Lib\winv6.3\km\arm\$_.lib"}
 & $MicrosoftLinkPath /nologo /driver /subsystem:native,6.03 /machine:arm /entry:DriverEntry /nodefaultlib /incremental:no /base:0x400000 "$base\ge8300-pair-kmd.lib" "$out\ge8300-native-owner-core.lib" @libs "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE){throw 'SYS relink'}
 & $SignToolPath sign /fd SHA256 /s My /sha1 BCBF6C456C6F0DD0818434763B189488FBF13570 "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'BCBF SYS sign'}
 & $SignToolPath verify /pa "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS trust'}
 $sig=Get-AuthenticodeSignature "$out\ge8300-render.sys";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Signer mismatch'}
 & llvm-readobj --file-headers --coff-imports "$out\ge8300-render.sys"|Set-Content "$out\imports.txt";if($LASTEXITCODE){throw 'PE inspect'}
 [ordered]@{schema='gpu327-dma-irql-one-tu-integration-only-v1';source_commit=$commit;source_sha256=(Sha $source);compiled_units=1;compiled_source='drivers/gpu/mt8167/vendor_m1_11/render_owner/native_owner_mdl.c';compiler_arguments=$flags;native_members=$rows;native_member_count=$members.Count;unchanged_native_members=@($rows|Where-Object {!$_.changed}).Count;baseline_kmd_archive_sha256=(Sha "$base\ge8300-pair-kmd.lib");baseline_sys_sha256=(Sha "$base\ge8300-render.sys");native_archive_sha256=(Sha "$out\ge8300-native-owner-core.lib");sys_sha256=(Sha "$out\ge8300-render.sys");signature_status=[string]$sig.Status;signer_thumbprint=$sig.SignerCertificate.Thumbprint;device_verified=$false;reset_cause_proven=$false;deployable_package=$false;catalog_generated=$false;PCO_rebuilt=$false;SDK_rebuilt=$false;UMD_rebuilt=$false;client_rebuilt=$false;baseline_kit=$kit}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 Compress-Archive -Path "$out\ge8300-render.sys","$out\ge8300-native-owner-core.lib","$out\imports.txt","$out\build-manifest.json","$out\native\$changed","$out\ge8300-render.map" -DestinationPath "$out\outputs.zip"
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
'DMA_IRQL_1TU_SIGNED_INTEGRATION_ARTIFACT_NOT_DEVICE_VERIFIED'
