# Cached USB1009 leaf only; no shared parent, SDK install or device access.
param([Parameter(Mandatory=$true)][string]$CacheDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$KitRoot,[string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory);$cache=[IO.Path]::GetFullPath($CacheDirectory)
if(@(& git -C $root status --porcelain).Count -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Clean checkout and new in-tree output required'}
function Hash([string]$p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$baseline='204c46c88e1c16a4a12c75d5bb5e868615ccb7b0'
$changed=@(& git -C $root diff --name-only $baseline HEAD -- drivers/bus drivers/audio)
if($LASTEXITCODE -or $changed.Count){throw 'Parent/audio must remain unchanged'}
$cm=Get-Content "$cache\cache-manifest.json" -Raw|ConvertFrom-Json
if($cm.source_commit -cne $baseline -or $cm.object_count -ne 15){throw 'Exact USB1009 cache required'}
foreach($f in $cm.files){if((Hash (Join-Path $cache $f.path)) -cne $f.sha256){throw 'Cached member hash mismatch'}}
$compile=@('wdf_keyboard','controller','keyboard','keyboard_session','keyboard_port','keyboard_rx')
$all=@('wdf_keyboard','wdf_controller','boot_hid','host_core','host_control','controller','board_host','keyboard','keyboard_session','keyboard_port','keyboard_rx','ep0','ep0_pio')
# Unchanged TUs have no composite-dependent layout. WDF context is private.
foreach($name in $all|Where-Object{$_ -notin $compile}){
 $p='drivers/usb/mt8167-usb20/'+$name+'.c'
 if((& git -C $root rev-parse "${baseline}:$p") -cne (& git -C $root rev-parse "HEAD:$p")){throw 'Reused TU source changed'}
}
$kit=[IO.Path]::GetFullPath($KitRoot);$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$wf="$kit\Lib\wdf\kmdf\arm\1.13";$sign="$kit\bin\x86\signtool.exe"
New-Item -ItemType Directory $out|Out-Null
$reused=@();foreach($name in $all|Where-Object{$_ -notin $compile}){$p="$cache\mt8167-usb20-$name.obj";Copy-Item $p $out;$reused+=@{name=$name;sha256=(Hash $p)}}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP=$out;$env:TMP=$out
 foreach($name in $compile){
  & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MUSB_HOST_CORE_EXPERIMENT=1 -DA04_MUSB_HID_CONTROL_EXPERIMENT=1 -DA04_MUSB_BOOT_MOUSE_EXPERIMENT=1 -DA04_MUSB_BOOT_COMPOSITE_EXPERIMENT=1 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -include ntddk.h -c "$root\drivers\usb\mt8167-usb20\$name.c" -o "$out\mt8167-usb20-$name.obj"
  if($LASTEXITCODE){throw "USB composite compile failed: $name"}
 }
 $objs=@($all|ForEach-Object{"$out\mt8167-usb20-$_.obj"})
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objs "$wf\wdfdriverentry.lib" "$wf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt8167-usb20.map" "/out:$out\mt8167-usb20.sys"
 if($LASTEXITCODE){throw 'USB composite link failed'}
 $unsigned=Hash "$out\mt8167-usb20.sys";$pkg="$out\package";New-Item -ItemType Directory $pkg|Out-Null
 Copy-Item "$out\mt8167-usb20.sys" $pkg;Copy-Item "$root\drivers\usb\mt8167-usb20\mt8167-usb20.inf" $pkg
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint";if(!$cert.HasPrivateKey){throw 'Existing private signing identity required'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usb20.sys";if($LASTEXITCODE){throw 'SYS sign failed'}
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$pkg" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usb20.cat";if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($name in @('mt8167-usb20.sys','mt8167-usb20.inf')){& $sign verify /pa /v /c "$pkg\mt8167-usb20.cat" "$pkg\$name";if($LASTEXITCODE){throw 'CAT membership failed'}}
 $sig=Get-AuthenticodeSignature "$pkg\mt8167-usb20.sys";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $CertificateThumbprint){throw 'Signed SYS trust mismatch'}
 Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
 $commit=(& git -C $root rev-parse HEAD)
 @{schema='usb1010-boot-composite-incremental-v1';source_commit=$commit;source_dirty=$false;baseline_source=$baseline;baseline_cache_manifest_sha256=(Hash "$cache\cache-manifest.json");driver_ver='11/08/2026,2026.11.8.1010';recompiled=$compile;reused_objects=$reused;unsigned_sys_sha256=$unsigned;parent_rebuilt=$false;audio_rebuilt=$false;kit_root=$kit;ntddi='0x06030000';hardware_verified=$false;standard_usb_host_stack=$false;direct_boot_keyboard_mouse_only=$true}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\usb-manifest.json"
 $files=@(Get-ChildItem $pkg -File|Sort-Object Name|ForEach-Object{@{path=$_.Name;sha256=(Hash $_.FullName)}})
 @{source_commit=$commit;binary_source_commit=$commit;inf_source_commit=$commit;driver_ver='11/08/2026,2026.11.8.1010';signing_thumbprint=$CertificateThumbprint;signature_status=[string]$sig.Status;signed=$true;hardware_verified=$false;files=$files}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 'USB1010_COMPILE_LINK_SYS_CAT_SIGN_MEMBERSHIP_VERIFIED'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
