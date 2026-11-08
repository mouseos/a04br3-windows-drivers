# Existing recovered WDK8.1/header/object inputs only. No INF/CAT/sign/deploy.
param([Parameter(Mandatory=$true)][string]$InputDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$inputRoot=[IO.Path]::GetFullPath($InputDirectory);$out=[IO.Path]::GetFullPath($OutputDirectory)
foreach($p in @($inputRoot,$out)){if(!$p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Dedicated checkout-local paths required'}}
if(Test-Path $out){throw 'New output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$cache=Get-Content "$inputRoot\cache\parent-cache.json" -Raw|ConvertFrom-Json
if($cache.source_commit -ne '1d248a71a95360966ff867e4b063504ac764e074'){throw 'Parent1101 cache required'}
$allowed=@('drivers/bus/mt8167-shared/wdf_parent_probe.c','drivers/input/mt8167-accdet/ioctl.h','drivers/input/mt8167-accdet/socket.h','drivers/input/mt8167-accdet/parent.inc','drivers/input/mt8167-accdet/README.md')
$changed=@(& git -C $root diff --name-only $cache.source_commit HEAD -- drivers)
if($LASTEXITCODE -or @($changed|Where-Object{$_ -notin $allowed}).Count){throw 'Unrelated driver source changes forbid cache reuse'}
New-Item -ItemType Directory $out|Out-Null
$objects=@()
foreach($o in $cache.objects){
 $p="$inputRoot\cache\obj\$($o.name).obj"
 if((Get-FileHash $p -Algorithm SHA256).Hash -ne $o.sha256){throw 'Object cache hash mismatch'}
 if($o.name -ne 'parent'){$objects+=$p}
}
$common=@($cache.common);$inc="$inputRoot\wdk\Include";$km="$inputRoot\wdk\Lib\winv6.3\km\arm";$wdf="$inputRoot\wdk\Lib\wdf\kmdf\arm\1.13"
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$inc\km",'-isystem',"$inc\shared",'-isystem',"$inc\um",'-isystem',"$inc\wdf\kmdf\1.13")
foreach($variant in @('disabled','readonly')){
 $flag=if($variant -eq 'readonly'){'1'}else{'0'}
 & clang @common @kernel @($cache.flags) "-DA04_SHARED_ACCDET_READ=$flag" -w -c "$root\drivers\bus\mt8167-shared\wdf_parent_probe.c" -o "$out\parent-$variant.obj"
 if($LASTEXITCODE){throw "Parent compile failed $variant"}
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib "$out\parent-$variant.obj" @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\mt8167-accdet-$variant-compile-only.sys"
 if($LASTEXITCODE){throw "Parent link failed $variant"}
}
& clang @common -w -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -isystem "$inc\um" -isystem "$inc\shared" -c "$root\tools\input\mt8167-accdet-read.c" -o "$out\mt8167-accdet-read.obj"
if($LASTEXITCODE){throw 'Client ARM32 compile failed'}
@{source_commit=$commit;parent_cache_source=$cache.source_commit;compiler=((& clang --version)-join "`n");cache_compiler=$cache.compiler;compiled_parent_variants=2;compiled_client_tus=1;reused_helper_objects=$objects.Count;parent_flags=@($cache.flags);readonly_flag='A04_SHARED_ACCDET_READ=1';compile_only=$true;signed=$false;inf_created=$false;hardware_verified=$false;files=@(Get-ChildItem $out -File|ForEach-Object{@{path=$_.Name;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$out\manifest.json"
Compress-Archive -Path "$out\*" -DestinationPath "$root\build\mt8167-accdet-compile-only.zip"
'MT8167_ACCDET_INCREMENTAL_COMPILE_LINK_COMPLETE'
