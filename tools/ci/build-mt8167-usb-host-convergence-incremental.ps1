# MediaTek MT8167 USB20 leaf only; no parent/hardware operation.
param([Parameter(Mandatory=$true)][string]$PreviousBuildDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory,
      [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$before='5c11a7510d795f5a93bdc6240eef21c60401afe5';$parent='2d6231727e8ed72b6d7dca4148debc101ac5aeef'
$commit=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousBuildDirectory);$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
function Hash($path){(Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Hash "$prior\package\usb-manifest.json") -ne 'dac85b0dcc30256629a79cd6693d91c73cf1c64aff028de5886535523f8e23c8'){throw 'Pinned1008 cache manifest required'}
$m=Get-Content -Raw "$prior\package\usb-manifest.json"|ConvertFrom-Json
if($m.source_commit -ne $before -or $m.driver_version -ne '2026.11.8.1008' -or $m.host_core -ne '1' -or $m.hid_control -ne '1' -or $m.boot_mouse -ne '1'){throw 'Fixed1008 feature set required'}
if((Hash "$prior\mt8167-usb20.sys") -ne 'f05574a72a66216424c61df843fb2a68a3dc208d29cb924eeb774874b3f2abf2'){throw 'Pinned1008 unsigned SYS changed'}
$changed=@(& git -C $root diff --name-only $before HEAD -- drivers/usb/mt8167-usb20)
$allowed=@('wdf_keyboard.c','board_host.c','board_host.h','mt8167-usb20.inf','README.md','HOST-CONVERGENCE.md')|ForEach-Object{'drivers/usb/mt8167-usb20/'+$_}
if($LASTEXITCODE -or @($changed|Where-Object{$_ -notin $allowed}).Count){throw 'Unexpected USB source changes; cache reuse refused'}
$parentChanges=@(& git -C $root diff --name-only $parent HEAD -- drivers/bus/mt8167-shared drivers/audio tools/ci/build-capture-rtc1015.ps1)
if($LASTEXITCODE -or $parentChanges.Count){throw 'Parent1026 code/flags/resources must remain unchanged'}
foreach($name in @('usb_interface.h','usb_admission.h')){
 $path='drivers/bus/mt8167-shared/'+$name
 if((& git -C $root rev-parse "${before}:$path") -ne (& git -C $root rev-parse "HEAD:$path")){throw 'USB parent interface changed'}
}
New-Item -ItemType Directory $out|Out-Null
$objects=@(Get-ChildItem "$prior\mt8167-usb20-*.obj" -File)
if($objects.Count -ne 15){throw 'Bounded1008 object cache mismatch'}
$reused=@()
foreach($f in $objects){
 Copy-Item $f.FullName "$out\$($f.Name)"
 if((Hash "$out\$($f.Name)") -ne (Hash $f.FullName)){throw 'Object copy mismatch'}
 if($f.Name -notin @('mt8167-usb20-wdf_keyboard.obj','mt8167-usb20-board_host.obj')){$reused+=@{path=$f.Name;sha256=(Hash $f.FullName)}}
}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$wdfInc="$inc\wdf\kmdf\1.13";$wdfLib="$kit\Lib\wdf\kmdf\arm\1.13";$km="$kit\Lib\winv6.3\km\arm"
$oldTmp=$env:TMP;$oldTemp=$env:TEMP;$oldRunner=$env:RUNNER_TEMP
try{
 $env:TMP=$out;$env:TEMP=$out;$env:RUNNER_TEMP=$out
 & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MUSB_HOST_CORE_EXPERIMENT=1 -DA04_MUSB_HID_CONTROL_EXPERIMENT=1 -DA04_MUSB_BOOT_MOUSE_EXPERIMENT=1 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c "$root\drivers\usb\mt8167-usb20\wdf_keyboard.c" -o "$out\mt8167-usb20-wdf_keyboard.obj"
 if($LASTEXITCODE){throw 'USB1009 leaf compile failed'}
 foreach($name in @('board_host')) {
  & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MUSB_BOOT_MOUSE_EXPERIMENT=1 "-I$inc\km" "-I$inc\shared" -include ntddk.h -c "$root\drivers\usb\mt8167-usb20\$name.c" -o "$out\mt8167-usb20-$name.obj"
  if($LASTEXITCODE){throw "USB1009 host state compile failed: $name"}
 }

 $link=@('wdf_keyboard','wdf_controller','boot_hid','host_core','host_control','controller','board_host','keyboard','keyboard_session','keyboard_port','keyboard_rx','ep0','ep0_pio')|ForEach-Object{"$out\mt8167-usb20-$_.obj"}
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib $link "$wdfLib\wdfdriverentry.lib" "$wdfLib\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt8167-usb20.map" "/out:$out\mt8167-usb20.sys"
 if($LASTEXITCODE){throw 'USB1009 leaf link failed'}
 $pkg="$out\package";New-Item -ItemType Directory $pkg|Out-Null
 Copy-Item "$out\mt8167-usb20.sys" $pkg
 Copy-Item "$root\drivers\usb\mt8167-usb20\mt8167-usb20.inf" $pkg
 Copy-Item "$root\drivers\usb\mt8167-usb20\README.md" $pkg
 Copy-Item "$root\drivers\usb\mt8167-usb20\BOOT-MOUSE.md" $pkg
 Copy-Item "$root\drivers\usb\mt8167-usb20\HOST-CONVERGENCE.md" $pkg
 $inf=[IO.File]::ReadAllText("$pkg\mt8167-usb20.inf")
 if($inf -notmatch '(?m)^DriverVer=11/07/2026,2026\.11\.8\.1009\r?$'){throw 'USB1009 INF required'}
 $m.source_commit=$commit;$m.driver_date='11/07/2026';$m.driver_version='2026.11.8.1009';$m.driver_ver='11/07/2026,2026.11.8.1009';$m.sys_sha256=(Hash "$out\mt8167-usb20.sys")
 $m|Add-Member -Force -NotePropertyName boot_mouse -NotePropertyValue '1'
 $m|Add-Member -Force -NotePropertyName source_dirty -NotePropertyValue $false
 $m|Add-Member -Force -NotePropertyName init_checkpoint_stage_count -NotePropertyValue 19
 $m|Add-Member -Force -NotePropertyName incremental -NotePropertyValue @{previous_source=$before;previous_manifest_sha256=(Hash "$prior\package\usb-manifest.json");recompiled=@('wdf_keyboard','board_host');reused_objects=$reused;parent_source=$parent;parent_code_flags_resources_unchanged=$true;parent_rebuilt=$false;source_diff_verified=$true}
 $m|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\usb-manifest.json"
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint";if(!$cert.HasPrivateKey){throw 'Existing signing identity required'}
 $sign="$kit\bin\x86\signtool.exe"
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usb20.sys";if($LASTEXITCODE){throw 'SYS signing failed'}
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$pkg" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usb20.cat";if($LASTEXITCODE){throw 'CAT signing failed'}
 & $sign verify /pa /v "$pkg\mt8167-usb20.sys";if($LASTEXITCODE){throw 'SYS signature verify failed'}
 foreach($name in @('mt8167-usb20.sys','mt8167-usb20.inf')){
  & $sign verify /pa /v /c "$pkg\mt8167-usb20.cat" "$pkg\$name";if($LASTEXITCODE){throw 'Catalog membership verify failed'}
 }
 Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
 $files=@(Get-ChildItem $pkg -File|Sort-Object Name|ForEach-Object{@{path=$_.Name;sha256=(Hash $_.FullName)}})
 @{source_commit=$commit;binary_source_commit=$commit;inf_source_commit=$commit;source_dirty=$false;driver_ver='11/07/2026,2026.11.8.1009';driver_manifest='usb-manifest.json';signing_thumbprint=$CertificateThumbprint;signed=$true;hardware_verified=$false;parent_rebuilt=$false;parent_flags_modified=$false;parent_source=$parent;files=$files}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 Write-Output 'MT8167_USB20_1009_HOST_CONVERGENCE_SIGNED_NO_HARDWARE_OPERATION'
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp;$env:RUNNER_TEMP=$oldRunner}
