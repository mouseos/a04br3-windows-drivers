# Fixed Native1012 ->1013 scalar ARM incremental relink. No deployment.
param([Parameter(Mandatory=$true)][string]$PreviousBuildDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$before='b21b869348e0608aa0ad08530ba74313130d5196';$commit=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousBuildDirectory);$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$m=Get-Content -Raw "$prior\manifest.json"|ConvertFrom-Json
if($m.source_commit -cne $before -or $m.driver_version -cne '2026.11.6.1012' -or $m.source_dirty -ne $false -or $m.wdk_link_succeeded -ne 1){throw 'Fixed1012 build required'}
if((Get-FileHash "$prior\mt6630-runtime.sys" -Algorithm SHA256).Hash -cne $m.sys_sha256){throw 'Previous SYS changed'}
$changed=@(& git -C $root diff --name-only $before HEAD -- drivers/network/mt6630)
$allowed=@('drivers/network/mt6630/checkpoint.c','drivers/network/mt6630/native_control.c','drivers/network/mt6630/rsn_policy.h','drivers/network/mt6630/station_runtime.c','drivers/network/mt6630/mt6630-runtime.inf','drivers/network/mt6630/CONNECT-DATA-1013.md','drivers/network/mt6630/WPA2.md')
if($LASTEXITCODE -or @($changed|Where-Object{$_ -notin $allowed}).Count){throw 'Unexpected source changes: cannot reuse objects'}
New-Item -ItemType Directory $out,"$out\a04br3-mt6630-core"|Out-Null
$reused=@()
$priorObjects=@(Get-ChildItem "$prior\*.obj" -File)+@(Get-ChildItem "$prior\a04br3-mt6630-core\*.obj" -File)
foreach($f in $priorObjects){
 $rel=$f.FullName.Substring($prior.Length+1);Copy-Item $f.FullName "$out\$rel"
 if((Get-FileHash "$out\$rel" -Algorithm SHA256).Hash -ne (Get-FileHash $f.FullName -Algorithm SHA256).Hash){throw 'Object copy mismatch'}
 if($f.BaseName -notin @('checkpoint','native_control','station_runtime')){$reused+=@{path=$rel;sha256=(Get-FileHash $f.FullName -Algorithm SHA256).Hash}}
}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$overlay="$root\build\ndis-overlay"
if(!(Test-Path "$overlay\ndis.h")){throw 'Pinned source-local overlay required'}
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP=$out;$env:TEMP=$out
 foreach($name in @('checkpoint','native_control','station_runtime')){
  & clang --target=thumbv7-pc-windows-msvc -w -std=c11 -ffunction-sections -fdata-sections -fms-extensions -ffreestanding -O2 -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -D_WINSOCK_DEPRECATED_NO_WARNINGS=1 -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_WIFI_SESSION_DIAGNOSTIC=1 -DA04_WIFI_IRQ_DIAGNOSTIC=1 -DA04_WIFI_CALIBRATION_DIAGNOSTIC=1 "-I$overlay" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\$name.c" -o "$out\$name.obj"
  if($LASTEXITCODE){throw 'Incremental compile failed'}
 }
 $objects=@('checkpoint','ndis_runtime','native_control','native_contract','native_security','station_runtime','native_tx','native_scan','native_scan_bridge','native_nbl','native_receive','native_hif_rx')|ForEach-Object{"$out\$_.obj"}
 $objects+=Get-ChildItem "$out\a04br3-mt6630-core\*.obj"|Where-Object{$_.BaseName -ne 'native_tx'}|ForEach-Object{$_.FullName}
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt6630-runtime.map" "/out:$out\mt6630-runtime.sys"
 if($LASTEXITCODE){throw 'Incremental link failed'}
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
Copy-Item "$root\drivers\network\mt6630\mt6630-runtime.inf" "$out\mt6630-runtime.inf"
$m.source_commit=$commit;$m.source_dirty=$false;$m.driver_version='2026.11.6.1013';$m.driver_ver='11/06/2026,2026.11.6.1013';$m.wifi_source_tree=(& git -C $root rev-parse HEAD:drivers/network/mt6630)
$m.checkpoint_driver_build=1013;$m.sys_sha256=(Get-FileHash "$out\mt6630-runtime.sys" -Algorithm SHA256).Hash;$m.inf_sha256=(Get-FileHash "$out\mt6630-runtime.inf" -Algorithm SHA256).Hash
$m|Add-Member -Force -NotePropertyName incremental -NotePropertyValue @{previous_source=$before;previous_manifest_sha256=(Get-FileHash "$prior\manifest.json" -Algorithm SHA256).Hash;recompiled=@('checkpoint','native_control','station_runtime');reused_objects=$reused;source_diff_verified=$true}
$m|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\manifest.json"
