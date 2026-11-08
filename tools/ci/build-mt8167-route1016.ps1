# RT81 USBPORT HCD-only build; existing leaf WDK cache, no SDK or media work.
param([Parameter(Mandatory=$true)][string]$KitRoot,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$ReuseDirectory,[Parameter(Mandatory=$true)][string]$WdfInclude,[Parameter(Mandatory=$true)][string]$UmInclude,[Parameter(Mandatory=$true)][string]$SignTool,[Parameter(Mandatory=$true)][string]$Inf2Cat,[string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(@(& git -C $root status --porcelain).Count -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Clean checkout and new in-tree output required'}
New-Item -ItemType Directory $out|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
function Hash([string]$p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
try{
 $env:TEMP=$out;$env:TMP=$out;$kit=[IO.Path]::GetFullPath($KitRoot);$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$sign=$SignTool
 $common=@('-w','-fms-extensions','-fno-stack-protector','-fno-builtin','-ffunction-sections','-D_KERNEL_MODE','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_MUSB_HOST_CORE_EXPERIMENT=1',"-I$inc\km","-I$inc\shared","-I$UmInclude","-I$WdfInclude",'-include','ntddk.h',"-I$root\drivers\usb\mt8167-usb20")
 $reuse=Get-Content "$ReuseDirectory\reuse-manifest.json" -Raw|ConvertFrom-Json
 if(((& clang --version)[0]) -cne $reuse.compiler_first_line){throw 'Cached compiler drift'}
 $reused=@()
 foreach($x in $reuse.objects){
  if($x.name -notin @('ep0','ep0_pio','musb_async')){throw 'Unexpected reused object'}
  foreach($dep in $x.dependencies.PSObject.Properties){
   $p=Join-Path $root $dep.Name
   if((Hash $p) -cne $dep.Value){throw "Reused source/include drift $($dep.Name)"}
   $old=(& git -C $root rev-parse "$($x.source_commit):$($dep.Name)")
   $new=(& git -C $root rev-parse "HEAD:$($dep.Name)")
   if($LASTEXITCODE -or $old -cne $new){throw 'Reused git closure drift'}
  }
  $obj="$ReuseDirectory\$($x.name).obj"
  if((Hash $obj) -cne $x.object_sha256){throw 'Cached object mismatch'}
  $reused+=@{name=$x.name;sha256=(Hash $obj);source_commit=$x.source_commit;dependencies=$x.dependencies}
 }
 if($reused.Count -ne 3){throw 'Exactly 3 frozen objects required'}
 $names=@('usbport_hcd','root_hub','host_core','pio_transfer','ep0','ep0_pio','musb_async','musb_physical','controller','board_host');$objs=@()
 foreach($name in $names){
  $obj="$out\$name.obj";$objs+=$obj
  if($name -in @('ep0','ep0_pio','musb_async')){Copy-Item "$ReuseDirectory\$name.obj" $obj;continue}
  & clang --target=thumbv7-pc-windows-msvc -D_ARM_ -DARM -mfpu=none -mno-unaligned-access @common -c "$root\drivers\usb\mt8167-usb20\$name.c" -o $obj
  if($LASTEXITCODE){throw "ARM HCD compile failed: $name"}
 }
 @('LIBRARY USBPORT.SYS','EXPORTS',' USBPORT_GetHciMn',' USBPORT_RegisterUSBPortDriver')|Set-Content -Encoding ASCII "$out\usbport.def"
 & lld-link /lib /machine:arm "/def:$out\usbport.def" "/out:$out\usbport81.lib";if($LASTEXITCODE){throw 'USBPORT import library failed'}
 & lld-link /driver /subsystem:native /machine:arm /entry:GsDriverEntry /nodefaultlib /opt:noref @objs "$out\usbport81.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt8167-musb-hcd.map" "/out:$out\mt8167-musb-hcd.sys"
 if($LASTEXITCODE){throw 'ARM USBPORT HCD link failed'}
 # Real WDK-native callback and power-failure contract tests (no kernel run).
 $testobjs=@();foreach($name in @('root_hub','host_core','pio_transfer','ep0','ep0_pio','musb_async')){
  $obj="$out\host-$name.obj";$testobjs+=$obj
  & clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c "$root\drivers\usb\mt8167-usb20\$name.c" -o $obj
  if($LASTEXITCODE){throw 'Host dependency compile failed'}
 }
 & clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c "$root\tests\usb\usbport_hcd_test.c" -o "$out\usbport_hcd_test.obj"
 if($LASTEXITCODE){throw 'Host HCD test compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:main /nodefaultlib /opt:ref @testobjs "$out\usbport_hcd_test.obj" "/out:$out\usbport_hcd_test.exe"
 if($LASTEXITCODE){throw 'Host HCD test link failed'}
 & "$out\usbport_hcd_test.exe";if($LASTEXITCODE){throw "HCD test failed at line $LASTEXITCODE"}
 'WDK81_HCD_ROOT_CALLBACK_TEST_PASS'
 $physicalHost=@();foreach($name in @('controller','board_host')){
  $obj="$out\host-$name.obj";$physicalHost+=$obj
  & clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c "$root\drivers\usb\mt8167-usb20\$name.c" -o $obj
  if($LASTEXITCODE){throw "Physical host dependency failed: $name"}
 }
 & clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c "$root\tests\usb\musb_physical_test.c" -o "$out\musb_physical_test.obj"
 if($LASTEXITCODE){throw 'Physical backend test compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:main /nodefaultlib /opt:ref @testobjs @physicalHost "$out\musb_physical_test.obj" "/out:$out\musb_physical_test.exe"
 if($LASTEXITCODE){throw 'Physical backend test link failed'}
 & "$out\musb_physical_test.exe";if($LASTEXITCODE){throw "Physical backend test failed at line $LASTEXITCODE"}
 'WDK81_MUSB_PHYSICAL_ADMISSION_SHADOW_RUNDOWN_PASS'

 # Production parent methods compiled/executed with real WDK types and fake
 # register operations. The included .inc is the production provider itself.
 $parentHost=@();foreach($name in @('usb_owner','pmic_read','gpio22')){
  $source=if($name -eq 'pmic_read'){"$root\drivers\input\a04br3-buttons\pmic_read.c"}elseif($name -eq 'gpio22'){"$root\drivers\usb\mt8167-usb20\gpio22.c"}else{"$root\drivers\bus\mt8167-shared\usb_owner.c"}
  $obj="$out\host-$name.obj";$parentHost+=$obj
  & clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c $source -o $obj
  if($LASTEXITCODE){throw 'Parent model dependency failed'}
 }
 & clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c "$root\tests\usb\musb_parent_test.c" -o "$out\musb_parent_test.obj"
 if($LASTEXITCODE){throw 'Parent provider test compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:main /nodefaultlib /opt:ref @parentHost "$out\musb_parent_test.obj" "/out:$out\musb_parent_test.exe"
 if($LASTEXITCODE){throw 'Parent provider test link failed'}
 & "$out\musb_parent_test.exe";if($LASTEXITCODE){throw "Parent provider test failed at line $LASTEXITCODE"}
 'WDK81_PRODUCTION_MUSB_PARENT_QUERY_OBSERVE_STOP_PROOF_PASS'
 # Parent1115 is frozen: no parent compile/link/sign or source edit.
 $pkg="$out\package";New-Item -ItemType Directory $pkg|Out-Null
 Copy-Item "$out\mt8167-musb-hcd.sys" $pkg;Copy-Item "$root\drivers\usb\mt8167-usb20\mt8167-musb-hcd.inf" $pkg
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint";if(!$cert.HasPrivateKey){throw 'Existing signing identity required'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-musb-hcd.sys";if($LASTEXITCODE){throw 'SYS signing failed'}
 & $Inf2Cat "/driver:$pkg" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-musb-hcd.cat";if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($name in @('mt8167-musb-hcd.sys','mt8167-musb-hcd.inf')){& $sign verify /pa /v /c "$pkg\mt8167-musb-hcd.cat" "$pkg\$name";if($LASTEXITCODE){throw 'CAT membership failed'}}
 $sig=Get-AuthenticodeSignature "$pkg\mt8167-musb-hcd.sys";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $CertificateThumbprint){throw 'Trust mismatch'}
 Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
 $commit=(& git -C $root rev-parse HEAD)
 @{schema='rt81-usbport-musb-owner1016-v1';source_commit=$commit;source_dirty=$false;usbport_registration_version=200;registration_executed_on_rt81=$false;root_hub_pdo_owner='inbox USBPORT';expected_root_hub_id='USB\ROOT_HUB20';inbox_hub_binding_verified_on_hardware=$false;hardware_verified=$false;physical_backend_consumer_connected=$true;physical_owner_producer_implemented=$true;first_5v_hardware_verification=$false;physical_5v_verified=$false;standard_usb_host_stack=$false;wdk81_host_callback_tests=$true;parent_glue_compiled_only=$false;parent_rebuilt=$false;sdk_rebuilt=$false;driver_version='2026.11.8.1016';signature_status=[string]$sig.Status;signing_thumbprint=$CertificateThumbprint;kit_root=$kit;compiled_ARM_TUs=7;reused_ARM_objects=$reused;hs_bulk_packet=512;EP1_TX_FIFO_offset=64;EP1_RX_FIFO_offset=576;EP1_FIFO_bank_bytes=512;FIFO_geometry_readback_required=$true;TT_route_advertised=$false;HS_interrupt_USBPORT_interval_conversion=$true;root_disable_verified_before_ack=$true;required_root_invalidation_service=$true}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\hcd-manifest.json"
 $files=@(Get-ChildItem $pkg -File|Sort-Object Name|ForEach-Object{@{path=$_.Name;sha256=(Hash $_.FullName)}})
 @{source_commit=$commit;signed=$true;files=$files}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 Compress-Archive "$pkg\*" "$out\hcd1016-signed.zip"
 Compress-Archive "$out\*.obj","$out\*.map","$out\*.exe" "$out\hcd1016-evidence.zip"
 Get-FileHash "$out\hcd1016-signed.zip","$out\hcd1016-evidence.zip" -Algorithm SHA256|ForEach-Object{Write-Output ($_.Path+' '+$_.Hash)}
 'USBPORT_MUSB_HCD1016_ARM_BUILD_SIGN_VERIFY_PASS'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
