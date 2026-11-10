# Main-owned build/sign only. Never invokes install-usb1121-first.ps1 or PnP.
param([Parameter(Mandatory=$true)][string]$BundleDirectory,
 [Parameter(Mandatory=$true)][string]$Csc,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$ExpectedCscSha256,[string]$ExpectedSignToolSha256,[switch]$BuildAndSign)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$pins=Get-Content "$PSScriptRoot\usb1121-helper42796448681-inputs.json" -Raw|ConvertFrom-Json
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Check($p,$h){if($h -cnotmatch '^[0-9a-f]{64}$' -or (Sha $p) -cne $h){throw "Pinned input mismatch: $p"}}
if(@(& git -C $root status --porcelain).Count -or $LASTEXITCODE){throw 'Clean committed dedicated checkout required'}
foreach($f in $pins.source_files.PSObject.Properties){Check (Join-Path $root $f.Name) $f.Value}
Check (Join-Path $BundleDirectory 'plan.json') $pins.install_plan_sha256
$plan=Get-Content (Join-Path $BundleDirectory 'plan.json') -Raw|ConvertFrom-Json
if($plan.schema -cne 'MT8167_USB_ABI3_FIRST_INSTALL_V1' -or $plan.owner_ABI -ne 3 -or $plan.root_PDO_creation -or $plan.force_host -or $plan.source_ON){throw 'Dedicated ABI3 plan required'}
if($plan.signed_tuple_manifest_sha256 -cne $pins.signed_tuple_manifest_sha256 -or $plan.capture_sha256 -cne $pins.capture_sha256 -or $plan.native_receipt_sha256 -cne $pins.native_receipt_sha256){throw 'Fresh196/tuple provenance mismatch'}
foreach($f in $pins.install_bundle_files.PSObject.Properties){Check (Join-Path $BundleDirectory $f.Name) $f.Value}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\out\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New dedicated checkout-local out directory required'}
# The frozen90e helper recipe is the only compile/sign implementation used.
# Its no-BuildAndSign mode is plan-only, not a usable signed helper.
if(!$BuildAndSign){
 & "$PSScriptRoot\build-usb1121-pnp-helper.ps1" -Csc $Csc -SignTool $SignTool -OutputDirectory $out
 if(!$?){throw 'Frozen helper plan failed'}
 return
}
Check $Csc $ExpectedCscSha256;Check $SignTool $ExpectedSignToolSha256
$commit=(& git -C $root rev-parse HEAD).Trim()
& "$PSScriptRoot\build-usb1121-pnp-helper.ps1" -Csc $Csc -SignTool $SignTool -OutputDirectory $out -BuildAndSign
if(!$?){throw 'Frozen helper build/sign failed'}
$dll=Join-Path $out 'MT8167UsbAbi3Pnp.dll';$manifest=Join-Path $out 'helper-manifest.json'
$m=Get-Content $manifest -Raw|ConvertFrom-Json
Check $dll $m.dll_sha256;Check $Csc $ExpectedCscSha256;Check $SignTool $ExpectedSignToolSha256
if($m.schema -cne 'MT8167_USB_ABI3_PNP_HELPER_V1' -or $m.source_sha256 -cne $pins.source_files.'tools/usb/UsbAbi3Pnp.cs' -or $m.platform -cne 'ARM' -or !$m.build_requested){throw 'Helper source/build manifest mismatch'}
$sig=Get-AuthenticodeSignature -LiteralPath $dll
if($sig.Status -ne 'Valid' -or !$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -cne $pins.signer){throw 'Existing BCBF signature identity required'}
# Static PE/CLR checks only: never load an ARM PnP DLL in the build VM.
$b=[IO.File]::ReadAllBytes($dll);$pe=[BitConverter]::ToUInt32($b,60)
if([BitConverter]::ToUInt32($b,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($b,$pe+4) -ne 0x1c4){throw 'ARMNT PE required'}
$opt=$pe+24
if([BitConverter]::ToUInt16($b,$opt) -ne 0x10b -or [BitConverter]::ToUInt32($b,$opt+92) -lt 15 -or [BitConverter]::ToUInt32($b,$opt+96+14*8) -eq 0){throw 'ARM managed PE32 CLR header required'}
if(((& git -C $root rev-parse HEAD).Trim()) -cne $commit -or @(& git -C $root status --porcelain).Count){throw 'Source changed during build'}
$framework=@();$base=Split-Path $Csc
foreach($name in @('csc.rsp','csc.exe.config','mscorlib.dll','System.dll','System.Core.dll')){
 $file=Join-Path $base $name;if(Test-Path -LiteralPath $file -PathType Leaf){$framework+=@{path=$file;sha256=(Sha $file)}}
}
$m|Add-Member -NotePropertyName frozen_helper_source_commit -NotePropertyValue $pins.frozen_source_commit
$m|Add-Member -NotePropertyName fixed_recipe_source_commit -NotePropertyValue $commit
$m|Add-Member -NotePropertyName install_plan_sha256 -NotePropertyValue $pins.install_plan_sha256
$m|Add-Member -NotePropertyName input_pins_sha256 -NotePropertyValue (Sha "$PSScriptRoot\usb1121-helper42796448681-inputs.json")
$m|Add-Member -NotePropertyName certificate_status -NotePropertyValue ([string]$sig.Status)
$m|Add-Member -NotePropertyName observed_framework_inputs -NotePropertyValue $framework
$m|Add-Member -NotePropertyName SDK_setup -NotePropertyValue $false
$m|Add-Member -NotePropertyName device_operations -NotePropertyValue $false
$m|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $manifest
@{schema='USB1121_HELPER_BUILD_RECEIPT_V1';source_snapshot=$pins.frozen_source_commit;recipe_source=$commit;helper_manifest_sha256=(Sha $manifest);DLL_sha256=(Sha $dll);signature_status=[string]$sig.Status;signer=$sig.SignerCertificate.Thumbprint;ARMNT_managed_static_verified=$true;ARM_runtime_verified=$false;driver_rebuilt=$false;SDK_rebuilt=$false;PnP_invoked=$false;source_ON=$false}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$out\build-receipt.json"
Compress-Archive "$out\MT8167UsbAbi3Pnp.dll","$out\helper-manifest.json","$out\build-plan.json","$out\build-receipt.json" "$out\usb1121-helper-signed.zip"
Get-FileHash "$out\usb1121-helper-signed.zip",$manifest,$dll -Algorithm SHA256
'USB1121_HELPER_SIGNED_STATIC_CHECKED_NOT_INSTALLED_OR_ARM_RUNTIME_VERIFIED'
