# One parent TU + one diagnostic client TU, existing WDK/WDF/helper closure.
param([Parameter(Mandatory=$true)][string]$InputDirectory,
 [Parameter(Mandatory=$true)][string]$ArmUserLibraryDirectory,
 [Parameter(Mandatory=$true)][string]$SigntoolPath,
 [Parameter(Mandatory=$true)][string]$Inf2CatPath,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean source required'}
$allowed=@('drivers/input/mt8167-accdet/ioctl.h','drivers/input/mt8167-accdet/parent.inc','drivers/input/mt8167-accdet/status.h','drivers/input/mt8167-accdet/README.md','drivers/input/mt8167-accdet/mt8167-shared-accdet1103.inf')
$changed=@(& git -C $root diff --name-only 4e253a5c5f53ccdc34c8aaa92296986d53f72d6a HEAD -- drivers/bus drivers/input)
if($LASTEXITCODE -or @($changed|Where-Object{$_ -notin $allowed}).Count){throw 'Unrelated parent/child changes forbidden'}
$cache=Get-Content "$InputDirectory\cache\parent-cache.json" -Raw|ConvertFrom-Json
if($cache.source_commit -ne '1d248a71a95360966ff867e4b063504ac764e074'){throw 'Pinned parent1101 helper cache required'}
New-Item -ItemType Directory "$out\obj","$out\package\parent","$out\package\client","$out\tmp"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$objects=@();foreach($o in $cache.objects){
 $file="$InputDirectory\cache\obj\$($o.name).obj"
 if((Get-FileHash $file -Algorithm SHA256).Hash -ne $o.sha256){throw 'Helper cache hash mismatch'}
 if($o.name -ne 'parent'){$objects+=$file}
}
$inc="$InputDirectory\wdk\Include";$km="$InputDirectory\wdk\Lib\winv6.3\km\arm";$wdf="$InputDirectory\wdk\Lib\wdf\kmdf\arm\1.13"
$common=@($cache.common)
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$inc\km",'-isystem',"$inc\shared",'-isystem',"$inc\um",'-isystem',"$inc\wdf\kmdf\1.13")
& clang @common @kernel @($cache.flags) -DA04_SHARED_ACCDET_READ=1 -w -c "$root\drivers\bus\mt8167-shared\wdf_parent_probe.c" -o "$out\obj\parent.obj"
if($LASTEXITCODE){throw 'ACCDET parent TU failed'}
$sys="$out\package\parent\mt8167-shared.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib "$out\obj\parent.obj" @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Parent link failed'}
Copy-Item "$root\drivers\input\mt8167-accdet\mt8167-shared-accdet1103.inf" "$out\package\parent"
& clang @common -w -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -isystem "$inc\um" -isystem "$inc\shared" -c "$root\tools\input\mt8167-accdet-status.c" -o "$out\obj\status-client.obj"
if($LASTEXITCODE){throw 'Status client compile failed'}
$client="$out\package\client\mt8167-accdet-status.exe"
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\obj\status-client.obj" "$ArmUserLibraryDirectory\kernel32.lib" "$ArmUserLibraryDirectory\setupapi.lib" "/out:$client"
if($LASTEXITCODE){throw 'Status client link failed'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signing identity required'}
foreach($f in @($sys,$client)){
 & $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My $f;if($LASTEXITCODE){throw 'Sign failed'}
 & $SigntoolPath verify /pa /v $f;if($LASTEXITCODE){throw 'Verify failed'}
 if((Get-AuthenticodeSignature $f).SignerCertificate.Thumbprint -ne $thumb){throw 'Signer mismatch'}
 $pe=& llvm-readobj --file-headers --coff-imports $f
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
 $pe|Set-Content -Encoding UTF8 ($f+'.pe.txt')
}
& $Inf2CatPath "/driver:$out\package\parent" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
$cat="$out\package\parent\mt8167-shared-accdet1103.cat"
& $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT signing failed'}
& $SigntoolPath verify /pa /v $cat
if($LASTEXITCODE -or (Get-AuthenticodeSignature $cat).SignerCertificate.Thumbprint -ne $thumb){throw 'CAT signature failed'}
foreach($f in @($sys,"$out\package\parent\mt8167-shared-accdet1103.inf")){
 & $SigntoolPath verify /pa /v /c $cat $f;if($LASTEXITCODE){throw 'CAT membership failed'}
}
Export-Certificate -Cert $cert -FilePath "$out\package\development.cer"|Out-Null
$files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
@{source_commit=$commit;baseline_parent_source='4e253a5c5f53ccdc34c8aaa92296986d53f72d6a';baseline_parent_signed_sys_sha256='0b98648791827433a252f2787b97ab326aa90f4e4707dce27e42aa628e5bb502';baseline_parent_flags=@($cache.flags);readonly_flag='A04_SHARED_ACCDET_READ=1';new_diagnostic_ioctl='0x00226558';socket_abi_bytes=16;socket_abi_version=1;socket_abi_changed=$false;diagnostic_bytes=68;diagnostic_version=1;compiled_parent_tus=1;compiled_client_tus=1;reused_helpers=$objects.Count;driver_version='2026.11.8.1103';gpio_pmic_afe_writes_added=$false;hardware_verified=$false;qualification_relaxed=$false;signed=$true;signer_thumbprint=$thumb;files=$files}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive -Path "$out\package\*" -DestinationPath "$out\mt8167-accdet1103-signed.zip"
'MT8167_ACCDET1103_SIGNED_COMPLETE'
