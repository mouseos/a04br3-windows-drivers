# Fixed HCD-only incremental build. No parent, SDK setup, device or role action.
param([Parameter(Mandatory=$true)][string]$TaskRoot,[Parameter(Mandatory=$true)][string]$KitRoot,[Parameter(Mandatory=$true)][string]$UmInclude,[Parameter(Mandatory=$true)][string]$SignTool,[Parameter(Mandatory=$true)][string]$Inf2Cat)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$env:PYTHONUTF8='1'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$task=[IO.Path]::GetFullPath($TaskRoot);$out="$task\build"
if(!(Test-Path $task) -or (Test-Path $out) -or @(& git -C $root status --porcelain).Count){throw 'Existing isolated task / clean source / new output required'}
function Hash($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
New-Item -ItemType Directory $out|Out-Null;$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP=$out;$env:TEMP=$out;$inc=Join-Path $KitRoot 'Include';$km=Join-Path $KitRoot 'Lib\winv6.3\km\arm'
 $reuse=Get-Content "$task\reuse-objects.json" -Raw|ConvertFrom-Json
 if(((& clang --version)[0]) -cne 'clang version 20.1.8'){throw 'Fixed cache compiler drift'}
 $common=@('-w','-fms-extensions','-fno-stack-protector','-fno-builtin','-ffunction-sections','-D_KERNEL_MODE','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_MUSB_HOST_CORE_EXPERIMENT=1',"-I$inc\km","-I$inc\shared","-I$UmInclude",'-include','ntddk.h',"-I$root\drivers\usb\mt8167-usb20")
 $names=@('usbport_hcd','root_hub','host_core','pio_transfer','ep0','ep0_pio','musb_async','musb_physical','controller','board_host');$objs=@();$compiled=@();$reused=@()
 foreach($name in $names){
  $src="$root\drivers\usb\mt8167-usb20\$name.c";$obj="$out\$name.obj";$objs+=$obj;$cache=@($reuse.objects|Where-Object {$_.name -ceq $name})
  if($cache.Count -eq 1){$c=$cache[0];$old="$task\reuse\$name.obj";if((Hash $old) -cne $c.sha256 -or (Hash $src) -cne $c.source_sha256){throw 'Reused object/source mismatch'}
   foreach($d in $c.project_dependencies.PSObject.Properties){if((Hash (Join-Path $root $d.Name)) -cne $d.Value){throw 'Reused source/header closure drift'}}
   Copy-Item $old $obj;$reused+=@{name=$name;sha256=(Hash $obj);source_sha256=(Hash $src)}
  }else{
   & clang --target=thumbv7-pc-windows-msvc -D_ARM_ -DARM -mfpu=none -mno-unaligned-access @common -c $src -o $obj;if($LASTEXITCODE){throw "ARM compile failed: $name"}
   $compiled+=@{name=$name;source_sha256=(Hash $src);object_sha256=(Hash $obj)}
  }
 }
 if($compiled.Count -ne 4 -or $reused.Count -ne 6){throw 'Exact four-TU/six-reuse closure required'}
 @('LIBRARY USBPORT.SYS','EXPORTS',' USBPORT_GetHciMn',' USBPORT_RegisterUSBPortDriver')|Set-Content -Encoding ASCII "$out\usbport.def"
 & lld-link /lib /machine:arm "/def:$out\usbport.def" "/out:$out\usbport81.lib";if($LASTEXITCODE){throw 'Import library failed'}
 $libs=@("$out\usbport81.lib","$km\ntoskrnl.lib","$km\hal.lib","$km\bufferoverflowfastfailk.lib","$km\libcntpr.lib")
 & lld-link /driver /subsystem:native /machine:arm /entry:GsDriverEntry /nodefaultlib /opt:noref @objs @libs "/map:$out\mt8167-musb-hcd.map" "/out:$out\mt8167-musb-hcd.sys";if($LASTEXITCODE){throw 'ARM link failed'}
 $hostobjs=@();foreach($name in @('root_hub','host_core','pio_transfer','ep0','ep0_pio','musb_async','controller','board_host')){
  $obj="$out\host-$name.obj";$hostobjs+=$obj;& clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c "$root\drivers\usb\mt8167-usb20\$name.c" -o $obj;if($LASTEXITCODE){throw 'WDK model dependency failed'}
 }
 $tests=@('usbport_hcd_test','hcd_fair_test','musb_physical_test','musb_fair_test','stack_fair_test','ep0_class_stack_test');$passed=@()
 foreach($test in $tests){
  & clang --target=x86_64-pc-windows-msvc -D_AMD64_ -DAMD64 @common -c "$root\tests\usb\$test.c" -o "$out\$test.obj";if($LASTEXITCODE){throw "WDK model compile failed: $test"}
  & lld-link /subsystem:console /machine:x64 /entry:main /nodefaultlib /opt:ref @hostobjs "$out\$test.obj" "/out:$out\$test.exe";if($LASTEXITCODE){throw "WDK model link failed: $test"}
  & "$out\$test.exe";if($LASTEXITCODE){throw "WDK model failed: $test line$LASTEXITCODE"};$passed+=$test
 }
 $prior="$task\baseline\mt8167-musb-hcd.sys";$baseline=Get-Content "$task\baseline-inputs.json" -Raw|ConvertFrom-Json
 if((Hash $prior) -cne $baseline.SYS_sha256){throw 'Frozen1017 SYS pin mismatch'}
 $sig=Get-AuthenticodeSignature $prior;if($sig.Status -ne 'Valid'){throw 'Frozen1017 signer trust required'};$identity=$sig.SignerCertificate.Thumbprint
 $cert=Get-Item "Cert:\CurrentUser\My\$identity";if(!$cert.HasPrivateKey){throw 'Existing identity unavailable; no export permitted'}
 $pkg="$out\package";New-Item -ItemType Directory $pkg|Out-Null;Copy-Item "$out\mt8167-musb-hcd.sys" $pkg;Copy-Item "$root\drivers\usb\mt8167-usb20\mt8167-musb-hcd.inf" $pkg
 & $SignTool sign /fd SHA256 /s My /sha1 $identity "$pkg\mt8167-musb-hcd.sys";if($LASTEXITCODE){throw 'SYS sign failed'}
 & $Inf2Cat "/driver:$pkg" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $SignTool sign /fd SHA256 /s My /sha1 $identity "$pkg\mt8167-musb-hcd.cat";if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($member in @('mt8167-musb-hcd.sys','mt8167-musb-hcd.inf')){& $SignTool verify /pa /c "$pkg\mt8167-musb-hcd.cat" "$pkg\$member";if($LASTEXITCODE){throw 'CAT membership failed'}}
 foreach($member in @('mt8167-musb-hcd.sys','mt8167-musb-hcd.cat')){$s=Get-AuthenticodeSignature "$pkg\$member";if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $identity){throw 'Signature trust/identity mismatch'}}
 $source=(& git -C $root rev-parse HEAD).Trim();$manifest=@{schema='mt8167-musb-ep0-nak-stall1018-v1';source_commit=$source;source_dirty=$false;parent_base_source='b2c1a3136a5b8eadf1900f71a8d330a6b0a31ac8';inherited_HCD_source='abb2db82c372cf3634e52e7ccd043a41153d237b';driver_version='2026.11.10.1018';compiled=$compiled;reused=$reused;signature_status='Valid';signer_same_as_frozen1017=$true;SDK_setup=$false;parent_rebuilt=$false;host_role_changed=$false;new_hardware_resources=$false;hardware_verified=$false;physical_power_delivery_improved=$false;model_tests_passed=$passed;library_hashes=@($libs|ForEach-Object{@{path=$_;sha256=(Hash $_)}})}
 $manifest|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\hcd-manifest.json"
 @{schema=1;source_commit=$source;signed=$true;files=@(Get-ChildItem $pkg -File|Sort-Object Name|ForEach-Object{@{path=$_.Name;sha256=(Hash $_.FullName)}})}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 & (Join-Path (Split-Path (Get-Command clang.exe).Source) 'llvm-readobj.exe') --file-headers --coff-imports "$pkg\mt8167-musb-hcd.sys"|Set-Content "$out\pe-imports.txt";if($LASTEXITCODE){throw 'PE inspection failed'}
 Compress-Archive "$pkg\*" "$task\hcd1018-signed.zip";Compress-Archive "$out\*.obj","$out\*.map","$out\*.exe","$out\pe-imports.txt" "$task\hcd1018-evidence.zip"
 'HCD1018_FOUR_ARM_TUS_SIX_REUSE_SIX_WDK_MODELS_SIGNED_NOT_DEVICE_TESTED'
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
