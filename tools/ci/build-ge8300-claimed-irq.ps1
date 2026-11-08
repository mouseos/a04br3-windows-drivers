# Integration-only one-TU KMD change; never a deployable package/reset fix claim.
param([Parameter(Mandatory=$true)][string]$BaselineDirectory,[Parameter(Mandatory=$true)][string]$BaselineSource,[Parameter(Mandatory=$true)][string]$SourceFile,[Parameter(Mandatory=$true)][string]$InputManifest,[Parameter(Mandatory=$true)][string]$SourceCommit,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Input pin mismatch: $p"}}
$inputData=Get-Content -LiteralPath $InputManifest -Raw|ConvertFrom-Json
if(Test-Path $OutputDirectory){throw 'New output required'}
foreach($entry in $inputData.archives.PSObject.Properties){Pin "$BaselineDirectory\$($entry.Name)" $entry.Value}
Pin "$BaselineDirectory\build-manifest.json" $inputData.baseline_build_manifest_sha256
Pin "$BaselineSource\drivers\gpu\mt8167\wddm-render\lifecycle.c" $inputData.baseline_source_sha256
foreach($h in $inputData.headers){Pin "$BaselineSource\$($h.file)" $h.sha256}
New-Item -ItemType Directory $OutputDirectory,"$OutputDirectory\before","$OutputDirectory\after","$OutputDirectory\tmp"|Out-Null
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP="$OutputDirectory\tmp";$env:TEMP=$env:TMP
 $clang='C:\Program Files\LLVM\bin\clang.exe';$llvm='C:\Program Files\LLVM\bin'
 $members=@(& "$llvm\llvm-ar.exe" t "$BaselineDirectory\ge8300-pair-kmd.lib"|ForEach-Object{[IO.Path]::GetFileName($_)})
 if($LASTEXITCODE -or @($members|Sort-Object -Unique).Count -ne $members.Count){throw 'Unique members required'}
 & "$llvm\llvm-ar.exe" x "--output=$OutputDirectory\before" "$BaselineDirectory\ge8300-pair-kmd.lib";if($LASTEXITCODE){throw 'Extract KMD'}
 Pin "$OutputDirectory\before\lifecycle.obj" $inputData.baseline_object_sha256
 foreach($m in $members){Copy-Item "$OutputDirectory\before\$m" "$OutputDirectory\after\$m"}
 $flags=@($inputData.compiler_arguments|ForEach-Object{[string]$_})
 & $clang @flags -iquote "$BaselineSource\drivers\gpu\mt8167\wddm-render" -c $SourceFile -o "$OutputDirectory\after\lifecycle.obj";if($LASTEXITCODE){throw 'ARM lifecycle compile'}
 $rows=@();foreach($m in $members){$old=Sha "$OutputDirectory\before\$m";$new=Sha "$OutputDirectory\after\$m";if($m -cne 'lifecycle.obj' -and $old -cne $new){throw 'Unrelated KMD member changed'};$rows+=@{file=$m;before_sha256=$old;sha256=$new;changed=($old -cne $new)}}
 if(@($rows|Where-Object{$_.changed}).Count -ne 1){throw 'Exactly one changed KMD member required'}
 $paths=@($members|ForEach-Object{"$OutputDirectory\after\$_"});[array]::Reverse($paths)
 & "$llvm\llvm-lib.exe" /machine:arm "/out:$OutputDirectory\ge8300-pair-kmd.lib" @paths;if($LASTEXITCODE){throw 'Archive KMD'}
 $made=@(& "$llvm\llvm-ar.exe" t "$OutputDirectory\ge8300-pair-kmd.lib"|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(($made -join "`n") -cne ($members -join "`n")){throw 'Archive order changed'}
 $link=(Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\arm\link.exe'|Select-Object -First 1).FullName
 if(!$link){throw 'Existing MS ARM linker required'}
 $kit='C:\mt8167-public-fullsource323\wdk81';$libs=@('displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk')|ForEach-Object{"$kit\Lib\winv6.3\km\arm\$_.lib"}
 & $link /nologo /driver /subsystem:native,6.03 /machine:arm /entry:DriverEntry /nodefaultlib /incremental:no /base:0x400000 "$OutputDirectory\ge8300-pair-kmd.lib" "$BaselineDirectory\ge8300-native-owner-core.lib" @libs "/out:$OutputDirectory\ge8300-render.sys" "/map:$OutputDirectory\ge8300-render.map";if($LASTEXITCODE){throw 'SYS relink'}
 $sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 & $sign sign /fd SHA256 /s My /sha1 $thumb "$OutputDirectory\ge8300-render.sys";if($LASTEXITCODE){throw 'Sign SYS'}
 & $sign verify /pa "$OutputDirectory\ge8300-render.sys";if($LASTEXITCODE){throw 'Verify SYS'}
 $sig=Get-AuthenticodeSignature "$OutputDirectory\ge8300-render.sys";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Signer identity mismatch'}
 & "$llvm\llvm-readobj.exe" --file-headers --coff-imports "$OutputDirectory\ge8300-render.sys"|Set-Content "$OutputDirectory\imports.txt";if($LASTEXITCODE){throw 'PE inspect'}
 [ordered]@{schema='gpu332-claimed-irq-one-kmd-tu-integration-only-v1';source_commit=$SourceCommit;source_sha256=(Sha $SourceFile);input_manifest_sha256=(Sha $InputManifest);compiler_arguments=$flags;quoted_include_root=$BaselineSource;compiled_units=1;kmd_members=$rows;retained_kmd_members=@($rows|Where-Object{!$_.changed}).Count;baseline_native_archive_sha256=(Sha "$BaselineDirectory\ge8300-native-owner-core.lib");baseline_kmd_archive_sha256=(Sha "$BaselineDirectory\ge8300-pair-kmd.lib");kmd_archive_sha256=(Sha "$OutputDirectory\ge8300-pair-kmd.lib");sys_sha256=(Sha "$OutputDirectory\ge8300-render.sys");signature_status=[string]$sig.Status;signer_thumbprint=$thumb;device_verified=$false;run135_root_cause_proven=$false;deployable_package=$false;catalog_generated=$false;UMD_rebuilt=$false;PCO_rebuilt=$false;SDK_rebuilt=$false}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$OutputDirectory\build-manifest.json"
 Compress-Archive -Path "$OutputDirectory\ge8300-pair-kmd.lib","$OutputDirectory\ge8300-render.sys","$OutputDirectory\build-manifest.json","$OutputDirectory\imports.txt","$OutputDirectory\after\lifecycle.obj","$OutputDirectory\ge8300-render.map" -DestinationPath "$OutputDirectory\outputs.zip"
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
'CLAIMED_IRQ_1TU_SIGNED_NOT_DEVICE_VERIFIED'
