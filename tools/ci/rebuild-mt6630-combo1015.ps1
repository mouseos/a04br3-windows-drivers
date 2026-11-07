# Frozen MT6630 combo2008 -> cleanup1015 private incremental build.
# Reuses only unchanged core inputs; all NDIS TUs see the new broker layout.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$ToolchainDirectory,
 [Parameter(Mandatory=$true)][string]$SdkHeadersDirectory,
 [Parameter(Mandatory=$true)][string]$CrtHeadersDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$base='0a64c81ff872636221994c8fb5d200f6394367c1';$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory)
if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne 'F53805D1E35C206260B0AE3280695EA044F6D2CB39E43EF681701F6EA7E2E413'){throw 'Frozen combo2008 cache pin mismatch'}
$m=Get-Content "$prior\cache-manifest.json" -Raw|ConvertFrom-Json
foreach($f in $m.files){if((Get-FileHash "$prior\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Prior object cache changed'}}
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
if($old.source_commit -ne $base -or $m.source_commit -ne $base){throw 'Wrong source generation'}
$changed=@(& git -C $root diff --name-only $base HEAD -- drivers)
$allowed=@('drivers/bluetooth/mt6630/CLEANUP-1015.md','drivers/bluetooth/mt6630/broker_lifetime.h','drivers/network/mt6630/checkpoint.c','drivers/network/mt6630/combo_broker.c','drivers/network/mt6630/combo_broker.h','drivers/network/mt6630/ndis_runtime.c','drivers/network/mt6630/mt6630-combo-runtime.inf')
if($LASTEXITCODE -or @($changed|Where-Object{$_ -notin $allowed}).Count){throw 'Reused core/client source dependency changed'}
foreach($p in @(@{root=$ToolchainDirectory;hash='3A0870CB8D233686223206E52E9F88F5A6553FD09D77DA1A9160B96F32859EA2'},@{root=$SdkHeadersDirectory;hash='78ED33B4C112A16053572DB34E8DB4F3CF077F9CA0AB38ED8D49117B4911BC3F'},@{root=$CrtHeadersDirectory;hash='B75AE4E718976E5DB3CE535D9DE730249D19490AB49C48F46467FE98547A379C'})){
 if((Get-FileHash "$($p.root)\files-manifest.json" -Algorithm SHA256).Hash -ne $p.hash){throw 'Private SDK/compiler manifest mismatch'}
 $files=Get-Content "$($p.root)\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$($p.root)\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Private SDK/compiler bytes changed'}}
}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\provider","$out\tmp"|Out-Null
$reused=@();$objects=@();$compiled=@()
foreach($r in $old.reused_objects|Where-Object{$_.object.StartsWith('core/') -or $_.object -eq 'bt-transport.obj'}){
 Copy-Item "$prior\$($r.object)" "$out\$($r.object)"
 if((Get-FileHash "$out\$($r.object)" -Algorithm SHA256).Hash -ne $r.sha256){throw 'Reused object copy mismatch'}
 $objects+="$out\$($r.object)";$reused+=$r
}
if($reused.Count -ne 30){throw 'Expected unchanged29 core +1 BT transport objects'}
$inc="$SdkHeadersDirectory\wdk\Include";$km="$ToolchainDirectory\wdk\Lib\winv6.3\km\arm"
$flags=@('--target=thumbv7-pc-windows-msvc','-resource-dir',"$SdkHeadersDirectory\clang-resource",'-nostdinc','-isystem',"$SdkHeadersDirectory\clang-resource\include",'-isystem',"$CrtHeadersDirectory\msvc-include",'-isystem',"$CrtHeadersDirectory\atlmfc-include",'-isystem',"$CrtHeadersDirectory\ucrt",'-fms-compatibility-version=19.44.35229','-std=c11','-w','-ffunction-sections','-fdata-sections','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1',"-I$SdkHeadersDirectory\ndis-overlay","-I$inc\km","-I$inc\shared","-I$inc\um")
$savedTemp=$env:TEMP;$savedTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($name in @('checkpoint','ndis_runtime','native_control','native_contract','native_security','station_runtime','native_tx','native_scan','native_scan_bridge','native_nbl','native_receive','native_hif_rx','combo_broker')){
  $relative="drivers/network/mt6630/$name.c";$obj="$out\ndis\$name.obj"
  & "$ToolchainDirectory\bin\clang.exe" @flags -c "$root\$relative" -o $obj;if($LASTEXITCODE){throw "Incremental compile failed $name"}
  $objects+=$obj;$compiled+=@{source=$relative;source_sha256=(Get-FileHash "$root\$relative" -Algorithm SHA256).Hash;object="ndis/$name.obj";sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 }
 & "$ToolchainDirectory\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\provider\mt6630-combo-runtime.sys"
 if($LASTEXITCODE){throw 'Incremental provider link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\provider"
 @{source_commit=$commit;source_dirty=$false;previous_source=$base;reused_objects=$reused;compiled_units=$compiled;compile_flags=$flags;implicit_sdk_headers_disabled=$true;compiler_sha256=(Get-FileHash "$ToolchainDirectory\bin\clang.exe" -Algorithm SHA256).Hash;parent_built=$false;runtime_driver_build=1015;cleanup_dedicated_slot=$true;cleanup_after_ordinary_drain=$true;exclusive_control_file=$true;cleanup_oom_retained_until_retry_or_halt=$true;stock_mt6630_cold_merge_requested=$true;stock_merge_hardware_ack_verified=$false;standard_stack_compatible=$false;hardware_verified=$false;sys_sha256=(Get-FileHash "$out\provider\mt6630-combo-runtime.sys" -Algorithm SHA256).Hash;inf_sha256=(Get-FileHash "$out\provider\mt6630-combo-runtime.inf" -Algorithm SHA256).Hash}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT6630_COMBO1015_INCREMENTAL_PROVIDER_LINKED'
}finally{$env:TEMP=$savedTemp;$env:TMP=$savedTmp}
