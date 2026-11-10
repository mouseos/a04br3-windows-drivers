# mainのみ実行。既存SDK/import libs、service 1TUのみ。署名済DLL/driver再build0。
param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$ExpectedInputClosureSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$KitHeaderRoot,
 [Parameter(Mandatory=$true)][string]$LibraryRoot,[Parameter(Mandatory=$true)][string]$SignTool,[Parameter(Mandatory=$true)][string]$ReadyPackage)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$root=[IO.Path]::GetFullPath("$PSScriptRoot\..\..");$out=[IO.Path]::GetFullPath($OutputDirectory)
if(@(& git -C $root status --porcelain).Count -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'clean dedicated checkout/new local out required'}
if((Sha "$Inputs\input-closure.json") -cne $ExpectedInputClosureSha256){throw 'input closure drift'}
$c=Get-Content "$Inputs\input-closure.json" -Raw|ConvertFrom-Json
foreach($f in $c.files.PSObject.Properties){$q=[IO.Path]::GetFullPath((Join-Path $Inputs $f.Name));if(!$q.StartsWith([IO.Path]::GetFullPath($Inputs).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Sha $q) -cne $f.Value){throw 'input leaf drift'}}
if((Sha $SignTool) -cne '854c356a9d9977fb2ed22ce04b531ab6f4417da06a4dba6e3d3d628b9cfb988c'){throw 'GPU367/helper approved SignTool required'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
$dll="$Inputs\payload\helper\MT8167UsbAbi3Pnp.dll";$s=Get-AuthenticodeSignature $dll
if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw 'signed helper trust mismatch'}
& $SignTool verify /pa /all $dll;if($LASTEXITCODE){throw 'helper signature verify failed'}
# CAT+member verification of candidate and rollback before service signing.
$p=Get-Content "$Inputs\payload\bundle\plan.json" -Raw|ConvertFrom-Json
foreach($e in @($p.candidate.parent,$p.candidate.hcd,$p.rollback.parent,$p.rollback.legacy)){
 $cat=Join-Path "$Inputs\payload\bundle" $e.names.cat;$cs=Get-AuthenticodeSignature $cat
 if($cs.Status -ne 'Valid' -or $cs.SignerCertificate.Thumbprint -cne $thumb){throw 'catalog signer mismatch'}
 foreach($n in @($e.names.inf,$e.names.sys)){& $SignTool verify /pa /c $cat (Join-Path "$Inputs\payload\bundle" $n);if($LASTEXITCODE){throw 'catalog membership failed'}}
}
# Reuse the already completed private native proof; never touch ready7ec.
$ready=[IO.Path]::GetFullPath($ReadyPackage)
$readyManifest='5447b112e3be16a7b6fd13c1733cc4411bab6b2eee3abaa6da5a4da3ed0f3d7b'
if((Sha "$ready\service-manifest.json") -cne $readyManifest){throw 'exact ready7ec manifest required'}
$old=Get-Content "$ready\service-manifest.json" -Raw|ConvertFrom-Json
if($old.recipe_source_commit -cne '7ec02fc5d4030fe8cedb3c1e4fe135c5804c2869' -or $old.service -cne 'MT8167Usb1121Install198' -or $old.input_closure_sha256 -cne $ExpectedInputClosureSha256 -or $old.profile_sha256 -cne $c.profile_sha256){throw 'ready7ec service/inputs mismatch'}
foreach($f in $old.files.PSObject.Properties){if((Sha (Join-Path $ready $f.Name)) -cne $f.Value){throw 'ready7ec payload drift'}}
foreach($f in $old.native_output_hashes.PSObject.Properties){if((Sha (Join-Path "$ready\native" $f.Name)) -cne $f.Value){throw 'ready7ec native leaf drift'}}
if((Sha "$ready\native\SYSTEM") -cne 'e9fcc62c3abf5464cb4fd3d2c0bd01c80172adc4bae198d8532090d695003b71' -or (Sha "$ready\native\result.json") -cne '0f6570cca2b78d39e7731b677aa24dde5c8269a021364d5caf9a2928aa49c023'){throw 'ready7ec native proof pin mismatch'}
$nr=Get-Content "$ready\native\result.json" -Raw|ConvertFrom-Json
if((Sha "$root\tools\recovery\prepare-usb1121-system198.ps1") -cne $nr.native_recipe_sha256 -or (Sha "$root\tools\recovery\Usb1121OfflineHiveNative.cs") -cne $nr.native_helper_sha256){throw 'native registration/preparer bytes differ from ready7ec'}
if(@(Get-ChildItem "$ready\native" -File).Count -ne @($old.native_output_hashes.PSObject.Properties).Count){throw 'unexpected native members'}
New-Item -ItemType Directory $out|Out-Null
Copy-Item "$Inputs\payload" "$out\payload" -Recurse;Copy-Item "$Inputs\usb1121-service-pins.h" $out
$src="$root\tools\usb\mt8167-usb1121-install-service.c";$name='MT8167Usb1121Install198'
$cc=(Get-Command clang).Source;$ld=(Get-Command lld-link).Source
$argsCC=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fms-extensions','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000',"-I$KitHeaderRoot\um","-I$KitHeaderRoot\shared","-I$out",'-c',$src,'-o',"$out\$name.obj")
$saveTmp=$env:TMP;$saveTemp=$env:TEMP
try{$env:TMP=$out;$env:TEMP=$out
 & $cc @argsCC;if($LASTEXITCODE){throw 'service TU compile failed'}
 $libs=@();$libpins=@();foreach($n in @('kernel32','advapi32','bcrypt','wintrust','crypt32')){$q=Join-Path $LibraryRoot ($n+'.lib');$libs+=$q;$libpins+=@{name=$n;sha256=(Sha $q)}}
 $argsLD=@('/subsystem:console,6.3','/machine:arm','/entry:mainCRTStartup','/nodefaultlib','/dynamicbase','/nxcompat',"$out\$name.obj")+$libs+@("/out:$out\payload\$name.exe")
 & $ld @argsLD;if($LASTEXITCODE){throw 'service link failed'}
 $unsigned=Sha "$out\payload\$name.exe"
 & $SignTool sign /fd SHA256 /s My /sha1 $thumb "$out\payload\$name.exe";if($LASTEXITCODE){throw 'service signing failed'}
 & $SignTool verify /pa /all "$out\payload\$name.exe";if($LASTEXITCODE){throw 'service signature failed'}
 $pebytes=[IO.File]::ReadAllBytes("$out\payload\$name.exe");$pe=[BitConverter]::ToInt32($pebytes,60)
 if([BitConverter]::ToUInt32($pebytes,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($pebytes,$pe+4) -ne 0x1c4 -or [BitConverter]::ToUInt16($pebytes,$pe+24) -ne 0x10b){throw 'service ARMNT/PE32 required'}
 $sig=Get-AuthenticodeSignature "$out\payload\$name.exe";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'service trust mismatch'}
 Copy-Item -LiteralPath "$ready\native" -Destination "$out\native" -Recurse
 foreach($f in $old.native_output_hashes.PSObject.Properties){if((Sha (Join-Path "$out\native" $f.Name)) -cne $f.Value){throw 'native exact copy mismatch'}}
 $files=[ordered]@{};Get-ChildItem "$out\payload" -Recurse -File|ForEach-Object{$files[$_.FullName.Substring($out.Length+1).Replace('\','/')]=Sha $_.FullName}
 $contract=Get-Content "$root\tools\usb\usb1121-uart198-contract.json" -Raw|ConvertFrom-Json
 $headers=[ordered]@{};foreach($n in @('tools/usb/usb1121-service-uart.h','tools/usb/usb1121-system-volume-flush.h','drivers/system/mt8167-toprgu/uart_screen.h')){$headers[$n]=Sha (Join-Path $root $n)}
 $manifest=[ordered]@{uart_contract=$contract;uart_contract_sha256=(Sha "$root\tools\usb\usb1121-uart198-contract.json");source_header_hashes=$headers;native_reused_byte_identical=$true;native_preparation_source_commit='7ec02fc5d4030fe8cedb3c1e4fe135c5804c2869';native_ready_manifest_sha256=$readyManifest;native_output_hashes=$old.native_output_hashes;native_preparation_invocations=0;schema='MT8167_USB1121_SIGNED_SERVICE198_V1';source_commit=(& git -C $root rev-parse HEAD);source_sha256=(Sha $src);input_closure_sha256=$ExpectedInputClosureSha256;profile_sha256=$c.profile_sha256;fresh_sha256=$c.fresh_sha256;native_receipt_sha256=$c.native_sha256;service=$name;signature_status='Valid';signer=$thumb;unsigned_exe_sha256=$unsigned;signed_exe_sha256=(Sha "$out\payload\$name.exe");files=$files;native_result_sha256=(Sha "$out\native\result.json");prepared_SYSTEM_sha256=(Sha "$out\native\SYSTEM");compile_args=$argsCC;link_args=$argsLD;compiler_sha256=(Sha $cc);linker_sha256=(Sha $ld);signtool_sha256=(Sha $SignTool);import_libraries=$libpins;service_compile_TUs=1;driver_rebuilds=0;helper_rebuilds=0;SDK_setup=$false;hardware_applied=$false;power_verified=$false}
 [IO.File]::WriteAllText("$out\service-manifest.json",($manifest|ConvertTo-Json -Depth 30),(New-Object Text.UTF8Encoding($false)))
 if((Sha "$ready\service-manifest.json") -cne $readyManifest -or (Sha "$ready\native\SYSTEM") -cne $old.prepared_SYSTEM_sha256 -or (Sha "$ready\payload\MT8167Usb1121Install198.exe") -cne $old.signed_exe_sha256){throw 'ready7ec changed during UART build'}
 Compress-Archive -Path "$out\payload","$out\native","$out\service-manifest.json" -DestinationPath "$out\usb1121-service198-signed.zip"
}finally{$env:TMP=$saveTmp;$env:TEMP=$saveTemp}
