# SPDX-License-Identifier: MIT
# DOD only. Reuse an existing small WDK81 closure; never install an SDK or build PCO.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$KitRoot,
 [Parameter(Mandatory=$true)][string]$VcToolsRoot,
 [Parameter(Mandatory=$true)][string]$UcrtIncludeRoot,
 [Parameter(Mandatory=$true)][string]$Inf2CatPath,
 [Parameter(Mandatory=$true)][string]$SignToolPath,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit DOD rotation candidate switch required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$builder=Join-Path $PSScriptRoot 'build-wddm-display-arm.ps1'
$flagNames=@([regex]::Matches([IO.File]::ReadAllText($builder),'\$env:(A04_GE8300_[A-Z0-9_]+)')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
if($flagNames.Count -lt 20){throw 'Unexpected GPU flag contract'}
$previous=@{};$flags=@{}
$names=@($flagNames)+@('A04_MT8167_DISPLAY_BRIGHTNESS','TEMP','TMP','RUNNER_TEMP')
foreach($n in $names){$previous[$n]=[Environment]::GetEnvironmentVariable($n,'Process')}
New-Item -ItemType Directory $out,"$out\tmp","$out\package"|Out-Null
try {
 foreach($n in $flagNames){[Environment]::SetEnvironmentVariable($n,'0','Process');$flags[$n]=0}
 $env:A04_MT8167_DISPLAY_BRIGHTNESS='1';$flags['A04_MT8167_DISPLAY_BRIGHTNESS']=1
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$out
 & $builder -KitRoot $KitRoot -VcToolsRoot $VcToolsRoot -UcrtIncludeRoot $UcrtIncludeRoot -BuildDirectory "$out\objects"
 if($LASTEXITCODE){throw 'DOD compile/link failed'}
 $pkg="$out\package"
 Copy-Item "$out\objects\wddm-display-build-check.sys" "$pkg\mt8167-display.sys"
 Copy-Item "$root\drivers\gpu\mt8167\wddm-display\mt8167-display.inf" $pkg
 $ver=[regex]::Match([IO.File]::ReadAllText("$pkg\mt8167-display.inf"),'(?m)^DriverVer=(.+)$').Groups[1].Value.Trim()
 if($ver -cne '11/08/2026,0.0.1.7'){throw 'Wrong DOD version'}
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 $machine=Test-Path "Cert:\LocalMachine\My\$thumb"
 $cert=if($machine){Get-Item "Cert:\LocalMachine\My\$thumb"}else{Get-Item "Cert:\CurrentUser\My\$thumb"}
 if(!$cert.HasPrivateKey){throw 'Existing signer required'}
 $store=@();if($machine){$store=@('/sm')}
 $unsigned=Sha "$pkg\mt8167-display.sys"
 & $SignToolPath sign /fd SHA256 /sha1 $thumb /s My @store "$pkg\mt8167-display.sys"
 if($LASTEXITCODE){throw 'SYS signing failed'}
 & $Inf2CatPath "/driver:$pkg" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'CAT generation failed; do not change host clock'}
 & $SignToolPath sign /fd SHA256 /sha1 $thumb /s My @store "$pkg\mt8167-display.cat"
 if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($n in @('mt8167-display.sys','mt8167-display.cat')){
  $sig=Get-AuthenticodeSignature "$pkg\$n"
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $thumb){throw "Invalid signature $n"}
 }
 foreach($n in @('mt8167-display.inf','mt8167-display.sys')){
  & $SignToolPath verify /pa /c "$pkg\mt8167-display.cat" "$pkg\$n"
  if($LASTEXITCODE){throw 'CAT membership failed'}
 }
 $sys=[IO.File]::ReadAllBytes("$pkg\mt8167-display.sys");$pe=[BitConverter]::ToInt32($sys,60)
 if([BitConverter]::ToUInt16($sys,$pe+4) -ne 0x1c4){throw 'ARMNT SYS required'}
 Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
 $inputs=@("$VcToolsRoot\bin\Hostx64\arm\cl.exe","$VcToolsRoot\bin\Hostx64\x64\link.exe",$SignToolPath,$Inf2CatPath)
 $inputs+=@(Get-ChildItem "$KitRoot\Lib\winv6.3\km\arm" -Filter '*.lib'|Where-Object{$_.Name -in @('displib.lib','ntoskrnl.lib','hal.lib','bufferoverflowfastfailk.lib','BufferOverflowGDI.lib','libcntpr.lib')}|ForEach-Object{$_.FullName})
 [ordered]@{schema='mt8167-display-rotation4-v1';source_commit=$commit;binary_source_commit=$commit;driver_ver=$ver;
 hardware='MediaTek MT8167 display';presentation='CPU';effective_flags=$flags;
 certificate_thumbprint=$thumb;signature_status='Valid';catalog_membership_verified=$true;machine='ARMNT';
 hardware_verified=$false;unsigned_sys_sha256=$unsigned;sdk_installed=$false;pco_rebuilt=$false;
 kit_root=$KitRoot;vc_tools_root=$VcToolsRoot;ucrt_include_root=$UcrtIncludeRoot;
 baseline_source_commit='6a99ec7d444180e150208d508dfaf203eb734d77';baseline_objects_available=$false;
 objects=@(Get-ChildItem "$out\objects" -Filter '*.obj'|ForEach-Object{@{name=$_.Name;sha256=(Sha $_.FullName)}});
 build_inputs=@($inputs|ForEach-Object{@{path=$_;sha256=(Sha $_)}});
 files=@(Get-ChildItem $pkg -File|ForEach-Object{@{path=$_.Name;sha256=(Sha $_.FullName)}})
 }|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 Compress-Archive "$pkg\*" "$out\package.zip"
 Compress-Archive "$out\objects\*" "$out\objects.zip"
 Get-FileHash "$out\package.zip","$out\objects.zip" -Algorithm SHA256
 Write-Output 'DOD_ROTATION4_SIGNED_NOT_DEVICE_VERIFIED'
}finally{foreach($n in $names){[Environment]::SetEnvironmentVariable($n,$previous[$n],'Process')}}
