# USB1011 bounded URB/root-hub adapter build. No device access, parent or SDK rebuild.
param([Parameter(Mandatory=$true)][string]$CacheDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$KitRoot,[string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory);$cache=[IO.Path]::GetFullPath($CacheDirectory)
if(@(& git -C $root status --porcelain).Count -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Clean checkout and new in-tree output required'}
function Hash([string]$p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$baseline='204c46c88e1c16a4a12c75d5bb5e868615ccb7b0'
if(@(& git -C $root diff --name-only $baseline HEAD -- drivers/bus drivers/audio).Count){throw 'Parent/audio must remain unchanged'}
$cm=Get-Content "$cache\cache-manifest.json" -Raw|ConvertFrom-Json
if($cm.source_commit -cne $baseline -or $cm.object_count -ne 15){throw 'Exact USB1009 leaf cache required'}
foreach($f in $cm.files){if((Hash (Join-Path $cache $f.path)) -cne $f.sha256){throw 'Cached member hash mismatch'}}
$compile=@('wdf_keyboard','controller','keyboard','keyboard_session','keyboard_port','keyboard_rx','root_hub','wdk_urb')
$all=@('wdf_keyboard','wdf_controller','boot_hid','host_core','host_control','controller','board_host','keyboard','keyboard_session','keyboard_port','keyboard_rx','ep0','ep0_pio','root_hub','wdk_urb')
foreach($name in $all|Where-Object{$_ -notin $compile}){
 $p='drivers/usb/mt8167-usb20/'+$name+'.c'
 if((& git -C $root rev-parse "${baseline}:$p") -cne (& git -C $root rev-parse "HEAD:$p")){throw 'Reused translation unit changed'}
}
$kit=[IO.Path]::GetFullPath($KitRoot);$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$wf="$kit\Lib\wdf\kmdf\arm\1.13";$sign="$kit\bin\x86\signtool.exe"
New-Item -ItemType Directory $out|Out-Null
$reused=@();foreach($name in $all|Where-Object{$_ -notin $compile}){$p="$cache\mt8167-usb20-$name.obj";Copy-Item $p $out;$reused+=@{name=$name;sha256=(Hash $p)}}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP=$out;$env:TMP=$out
 $common=@('-fms-extensions','-fno-stack-protector','-fno-builtin','-D_KERNEL_MODE','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_MUSB_HOST_CORE_EXPERIMENT=1',"-I$inc\km","-I$inc\shared","-I$inc\um","-I$inc\wdf\kmdf\1.13",'-include','ntddk.h')
 foreach($name in $compile){
  & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -w @common -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DA04_MUSB_HID_CONTROL_EXPERIMENT=1 -DA04_MUSB_BOOT_MOUSE_EXPERIMENT=1 -DA04_MUSB_BOOT_COMPOSITE_EXPERIMENT=1 -c "$root\drivers\usb\mt8167-usb20\$name.c" -o "$out\mt8167-usb20-$name.obj"
  if($LASTEXITCODE){throw "ARM compile failed: $name"}
 }
 # Tests execute x64 code on the build host with real WDK81 URBs, not stubs.
 $testnames=@('wdk_urb','root_hub','host_control','host_core','ep0','ep0_pio')
 $testobjs=@();foreach($name in $testnames){
  $obj="$out\test-$name.obj";$testobjs+=$obj
  & clang --target=x86_64-pc-windows-msvc -w @common -D_AMD64_ -DAMD64 -I"$root\drivers\usb\mt8167-usb20" -c "$root\drivers\usb\mt8167-usb20\$name.c" -o $obj
  if($LASTEXITCODE){throw "WDK host test compile failed: $name"}
 }
 & clang --target=x86_64-pc-windows-msvc -w @common -D_AMD64_ -DAMD64 -I"$root\drivers\usb\mt8167-usb20" -c "$root\tests\usb\wdk_urb_test.c" -o "$out\wdk_urb_test.obj"
 if($LASTEXITCODE){throw 'WDK test source compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:main /nodefaultlib @testobjs "$out\wdk_urb_test.obj" "/out:$out\wdk_urb_test.exe"
 if($LASTEXITCODE){throw 'WDK test link failed'}
 & "$out\wdk_urb_test.exe";if($LASTEXITCODE){throw "WDK URB test failed at line $LASTEXITCODE"}
 'WDK81_URB_NATIVE_TEST_PASS'
 $objs=@($all|ForEach-Object{"$out\mt8167-usb20-$_.obj"})
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:noref @objs "$wf\wdfdriverentry.lib" "$wf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt8167-usb20.map" "/out:$out\mt8167-usb20.sys"
 if($LASTEXITCODE){throw 'USB1011 ARM link failed'}
 $unsigned=Hash "$out\mt8167-usb20.sys";$pkg="$out\package";New-Item -ItemType Directory $pkg|Out-Null
 Copy-Item "$out\mt8167-usb20.sys" $pkg;Copy-Item "$root\drivers\usb\mt8167-usb20\mt8167-usb20.inf" $pkg
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint";if(!$cert.HasPrivateKey){throw 'Existing signing identity required'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usb20.sys";if($LASTEXITCODE){throw 'SYS signing failed'}
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$pkg" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usb20.cat";if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($name in @('mt8167-usb20.sys','mt8167-usb20.inf')){& $sign verify /pa /v /c "$pkg\mt8167-usb20.cat" "$pkg\$name";if($LASTEXITCODE){throw 'CAT membership failed'}}
 $sig=Get-AuthenticodeSignature "$pkg\mt8167-usb20.sys";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $CertificateThumbprint){throw 'Signed SYS trust mismatch'}
 Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
 $commit=(& git -C $root rev-parse HEAD)
 @{schema='usb1011-wdk81-urb-root-hub-v1';source_commit=$commit;source_dirty=$false;baseline_source=$baseline;baseline_cache_manifest_sha256=(Hash "$cache\cache-manifest.json");recompiled=$compile;reused_objects=$reused;unsigned_sys_sha256=$unsigned;parent_rebuilt=$false;sdk_rebuilt=$false;hardware_verified=$false;physical_5v_verified=$false;standard_usb_host_stack=$false;wdk81_native_urb_tests_passed=$true;adapter_linked=$true;adapter_runtime_connected=$false;direct_boot_keyboard_mouse_only=$true}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\usb-manifest.json"
 $files=@(Get-ChildItem $pkg -File|Sort-Object Name|ForEach-Object{@{path=$_.Name;sha256=(Hash $_.FullName)}})
 @{source_commit=$commit;signing_thumbprint=$CertificateThumbprint;signature_status=[string]$sig.Status;signed=$true;hardware_verified=$false;files=$files}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 'USB1011_ARM_BUILD_SIGN_VERIFY_PASS'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
