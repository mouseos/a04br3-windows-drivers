# 固定90e installerの通常Windows用SCM client。PnP実装は変更しない。
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$base='C:\Windows\System32\MT8167Usb1121Install198'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$c=Get-Content "$base\profile.json" -Raw|ConvertFrom-Json
if($c.schema -ceq 'MT8167_USB1121_FRESH198_SERVICE_V1'){
 if($c.fresh_sha256 -cne '4cfe2edfded6ff752ff145b46c33af910b30370e99079f9c75704cf17e2467a3'){throw 'historical fresh198 pin mismatch'}
}elseif($c.schema -ceq 'MT8167_USB1121_REBOUND_SERVICE_V2'){
 if($c.fresh_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $c.final_gpu_manifest_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $c.final_policy_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $c.target_gpu_version -cne '0.0.3.69' -or $c.integration_phase -cne 'after-gpu369-before-final-flush'){throw 'final baseline/GPU369 binding required'}
}else{throw 'unsupported exact-pinned USB profile'}
foreach($f in $c.files.PSObject.Properties){
 $p=[IO.Path]::GetFullPath((Join-Path $base $f.Name))
 if(!$p.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase) -or (Sha $p) -cne $f.Value){throw 'profile payload drift'}
}
$tx="$base\transaction";$mode=if(Test-Path "$tx\journal.json"){'Resume'}else{'Apply'}
# C側のexact profile hashと取得済みfresh receiptへ結合。GPU同一phaseでは最終369 byte成立後だけ進む。
foreach($f in $c.retained_files.PSObject.Properties){if((Sha (Join-Path 'C:\' $f.Name)) -cne $f.Value){throw 'actual retained GPU/system file changed'}}
$post=Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' -Name DisableAutoAcpiPostDeivce
if($post.DisableAutoAcpiPostDeivce -ne 0){throw 'POST0 changed'}
try{
 & "$base\bundle\recipe\install-usb1121-first.ps1" -Mode $mode -BundleDirectory "$base\bundle" -ExpectedPlanSha256 $c.plan_sha256 -ToolManifest "$base\helper\helper-manifest.json" -ExpectedToolManifestSha256 $c.helper_manifest_sha256 -TransactionDirectory $tx | Out-File "$base\logs\installer-result.txt" -Encoding utf8
 if(!$?){throw 'installer failed'}
 $j=Get-Content "$tx\journal.json" -Raw|ConvertFrom-Json
 if($j.reboot_required -and $j.pending_reboot_boot){exit 3010}
 if($j.phase -notin @('await-host-role-or-parent-restart-not-hardware-success','hcd-selected-start-unverified','HCD-selected-and-started-power-delivery-unverified')){throw ('unexpected installer phase: '+$j.phase)}
 # host PDO待ちは成功・給電証明と扱わない。自動反復しない。
 exit 0
}catch{$_|Out-File "$base\logs\failure.txt" -Encoding utf8;exit 1}
