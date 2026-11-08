# RT81 USBPORT HCD-only build; existing leaf WDK cache, no SDK or media work.
param([Parameter(Mandatory=$true)][string]$KitRoot,[Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(@(& git -C $root status --porcelain).Count -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Clean checkout and new in-tree output required'}
New-Item -ItemType Directory $out|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
function Hash([string]$p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
try{
 $env:TEMP=$out;$env:TMP=$out;$kit=[IO.Path]::GetFullPath($KitRoot);$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$sign="$kit\bin\x86\signtool.exe"
 $common=@('-w','-fms-extensions','-fno-stack-protector','-fno-builtin','-ffunction-sections','-D_KERNEL_MODE','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_MUSB_HOST_CORE_EXPERIMENT=1',"-I$inc\km","-I$inc\shared","-I$inc\um","-I$inc\wdf\kmdf\1.13",'-include','ntddk.h',"-I$root\drivers\usb\mt8167-usb20")
 $names=@('usbport_hcd','root_hub','host_core','pio_transfer');$objs=@()
 foreach($name in $names){
  $obj="$out\$name.obj";$objs+=$obj
  & clang --target=thumbv7-pc-windows-msvc -D_ARM_ -DARM -mfpu=none -mno-unaligned-access @common -c "$root\drivers\usb\mt8167-usb20\$name.c" -o $obj
  if($LASTEXITCODE){throw "ARM HCD compile failed: $name"}
 }
 @('LIBRARY USBPORT.SYS','EXPORTS',' USBPORT_GetHciMn',' USBPORT_RegisterUSBPortDriver')|Set-Content -Encoding ASCII "$out\usbport.def"
 & lld-link /lib /machine:arm "/def:$out\usbport.def" "/out:$out\usbport81.lib";if($LASTEXITCODE){throw 'USBPORT import library failed'}
 & lld-link /driver /subsystem:native /machine:arm /entry:GsDriverEntry /nodefaultlib /opt:noref @objs "$out\usbport81.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt8167-usbport-hcd.map" "/out:$out\mt8167-usbport-hcd.sys"
 if($LASTEXITCODE){throw 'ARM USBPORT HCD link failed'}
 # Real WDK-native callback and power-failure contract tests (no kernel run).
 $testobjs=@();foreach($name in @('root_hub','host_core','pio_transfer')){
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
 # Compile opted-in parent glue against WDK/WDF 8.1. Do not link/replace the
 # current parent: main must merge this switch into its newer parent source.
 & clang --target=thumbv7-pc-windows-msvc -D_ARM_ -DARM -mfpu=none -mno-unaligned-access @common -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DA04_SHARED_USBPORT_CHILD=1 -DA04_SHARED_QUERY_EXPERIMENTAL=1 -DA04_SHARED_CHILD_PROBE=1 -c "$root\drivers\bus\mt8167-shared\wdf_parent_probe.c" -o "$out\parent-usbport-optin.obj"
 if($LASTEXITCODE){throw 'Parent USBPORT opt-in compile failed'}
 $pkg="$out\package";New-Item -ItemType Directory $pkg|Out-Null
 Copy-Item "$out\mt8167-usbport-hcd.sys" $pkg;Copy-Item "$root\drivers\usb\mt8167-usb20\mt8167-usbport-hcd.inf" $pkg
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint";if(!$cert.HasPrivateKey){throw 'Existing signing identity required'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usbport-hcd.sys";if($LASTEXITCODE){throw 'SYS signing failed'}
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$pkg" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$pkg\mt8167-usbport-hcd.cat";if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($name in @('mt8167-usbport-hcd.sys','mt8167-usbport-hcd.inf')){& $sign verify /pa /v /c "$pkg\mt8167-usbport-hcd.cat" "$pkg\$name";if($LASTEXITCODE){throw 'CAT membership failed'}}
 $sig=Get-AuthenticodeSignature "$pkg\mt8167-usbport-hcd.sys";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $CertificateThumbprint){throw 'Trust mismatch'}
 Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
 $commit=(& git -C $root rev-parse HEAD)
 @{schema='rt81-usbport-hcd1012-v1';source_commit=$commit;source_dirty=$false;usbport_registration_version=200;registration_executed_on_rt81=$false;root_hub_pdo_owner='inbox USBPORT';expected_root_hub_id='USB\ROOT_HUB20';inbox_hub_binding_verified_on_hardware=$false;hardware_verified=$false;physical_backend_connected=$false;physical_5v_verified=$false;standard_usb_host_stack=$false;wdk81_host_callback_tests=$true;parent_glue_compiled_only=$true;parent_rebuilt=$false;sdk_rebuilt=$false;driver_version='2026.11.8.1012';signature_status=[string]$sig.Status;signing_thumbprint=$CertificateThumbprint;kit_root=$kit}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\hcd-manifest.json"
 $files=@(Get-ChildItem $pkg -File|Sort-Object Name|ForEach-Object{@{path=$_.Name;sha256=(Hash $_.FullName)}})
 @{source_commit=$commit;signed=$true;files=$files}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 'USBPORT_HCD1012_ARM_BUILD_SIGN_VERIFY_PASS'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
