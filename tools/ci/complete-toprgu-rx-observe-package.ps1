# 署名済みRX12 SYSを保持し、postdated INFで止まったcatalog工程だけを完了する。
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$CompiledOutput,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedBuildManifestSha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$CertificateThumbprint
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$head=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean committed packaging source required'}
$CompiledOutput=[IO.Path]::GetFullPath($CompiledOutput);$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$manifest=Join-Path $CompiledOutput 'manifest.json'
if((Hash $manifest) -cne $ExpectedBuildManifestSha256){throw 'Original build manifest SHA mismatch'}
$m=Get-Content $manifest -Raw|ConvertFrom-Json
if($m.schema -cne 'TOPRGU_RX_OBSERVE_BUILD_SIGN_V1' -or $m.success -ne $false -or $m.failure -cne 'Stage failed; retain completed artifacts: catalog12' -or $m.runtime_source_commit -cne '797121bc8ba5a2c31e09ed3372afa585600d3b4b' -or !$m.non_signature_payload.unchanged){throw 'Only division-free RX12 catalog failure accepted'}
if(@($m.steps).Count -ne 3 -or $m.steps[0].name -cne 'generic-build' -or $m.steps[0].exit_code -ne 0 -or $m.steps[1].name -cne 'sign-sys' -or $m.steps[1].exit_code -ne 0 -or $m.steps[2].name -cne 'catalog12' -or $m.steps[2].exit_code -ne 1){throw 'Completed compile and SYS signing evidence required'}
$thumb=$CertificateThumbprint.ToUpperInvariant()
if($m.certificate.thumbprint -cne $thumb -or $m.certificate.store -cne 'CurrentUser'){throw 'Original CurrentUser signing identity required'}
foreach($item in @($m.actual_dependencies)+@($m.link_libraries)+@($m.tools.PSObject.Properties|ForEach-Object Value)){
 if((Hash $item.path) -cne $item.sha256){throw ('Original input drift: '+$item.path)}
}
$oldSource=Join-Path (Split-Path $CompiledOutput) 'source'
if((& git -C $oldSource rev-parse HEAD).Trim() -cne $m.source_commit){throw 'Original build checkout changed'}
foreach($row in $m.source_files){if((Hash (Join-Path $oldSource $row.path)) -cne $row.sha256){throw 'Original source drift'}}
foreach($name in @('wdf_toprgu.c','rx_observe.h','reset_command.c','reset_command.h','watchdog_policy.h','handoff_policy.h','thermal_stop.h','uart_screen.h')){
 $p='drivers/system/mt8167-toprgu/'+$name
 if((& git -C $root rev-parse ($head+':'+$p)) -cne (& git -C $oldSource rev-parse ($m.source_commit+':'+$p))){throw 'Packaging source runtime differs'}
}
$sys=Join-Path $CompiledOutput 'package\mt8167-toprgu.sys';$inf=Join-Path $CompiledOutput 'package\mt8167-toprgu.inf'
foreach($p in @($sys,$inf)){$row=@($m.outputs|Where-Object path -CEQ $p);if($row.Count -ne 1 -or (Hash $p) -cne $row[0].sha256){throw 'Original package byte mismatch'}}
$sig=Get-AuthenticodeSignature -LiteralPath $sys
if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Original signed SYS identity invalid'}
$text=[IO.File]::ReadAllText($inf)
if([regex]::Matches($text,'(?m)^DriverVer=11/12/2026,0\.0\.12\.0\r?$').Count -ne 1){throw 'Exact original postdated DriverVer required'}
if(Test-Path $OutputDirectory){throw 'New output required; old failure remains immutable'}
New-Item -ItemType Directory $OutputDirectory|Out-Null
$package=Join-Path $OutputDirectory 'package';New-Item -ItemType Directory $package|Out-Null
Copy-Item $sys (Join-Path $package 'mt8167-toprgu.sys')
[IO.File]::WriteAllText((Join-Path $package 'mt8167-toprgu.inf'),($text.Replace('DriverVer=11/12/2026,0.0.12.0','DriverVer=11/06/2026,0.0.12.0')),[Text.Encoding]::ASCII)
$r=[ordered]@{schema='TOPRGU_RX_OBSERVE_PACKAGE_COMPLETION_V1';success=$false;signed=$false;source_commit=$head;compiled_source_commit=$m.source_commit;runtime_source_commit=$m.runtime_source_commit;build_manifest=$manifest;build_manifest_sha256=$ExpectedBuildManifestSha256;critical_flags=$m.critical_flags;driver_version='0.0.12.0';inf_driverver='11/06/2026,0.0.12.0';steps=@();outputs=@();compile_count=0;SYS_resign_count=0;input_SYS_sha256=(Hash $sys);non_signature_payload=$m.non_signature_payload;hardware_verified=$false;device_operations=0}
function Save{$r|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'manifest.json')}
function Tool($name,$exe,$arguments){
 $log=Join-Path $OutputDirectory ($name+'.log');$step=[ordered]@{name=$name;exit_code=$null;log_sha256=$null};$r.steps+=@($step);Save
 & $exe @arguments *> $log
 $step.exit_code=$LASTEXITCODE;$step.log_sha256=Hash $log;Save
 if($step.exit_code -ne 0){throw ('Package stage failed: '+$name)}
}
try{
 Save
 $cat=Join-Path $package 'mt8167-toprgu.cat';$signedSYS=Join-Path $package 'mt8167-toprgu.sys'
 Tool 'catalog' $m.tools.inf2cat.path @("/driver:$package",'/os:6_3_ARM','/uselocaltime')
 Tool 'sign-cat' $m.tools.signtool.path @('sign','/fd','SHA256','/s','My','/sha1',$thumb,$cat)
 Tool 'verify-sys' $m.tools.signtool.path @('verify','/pa','/v',$signedSYS)
 Tool 'verify-cat' $m.tools.signtool.path @('verify','/pa','/v',$cat)
 foreach($name in @('mt8167-toprgu.sys','mt8167-toprgu.inf')){Tool ('member-'+$name) $m.tools.signtool.path @('verify','/pa','/v','/c',$cat,(Join-Path $package $name))}
 if((Hash $signedSYS) -cne $r.input_SYS_sha256 -or (Hash $manifest) -cne $ExpectedBuildManifestSha256){throw 'Original SYS or manifest drift'}
 $catSig=Get-AuthenticodeSignature $cat
 if($catSig.Status -ne 'Valid' -or $catSig.SignerCertificate.Thumbprint -cne $thumb){throw 'Catalog identity invalid'}
 $r.success=$true;$r.signed=$true;$r.signer_thumbprint=$thumb
}catch{$r.failure=$_.Exception.Message;throw}
finally{$r.outputs=@(Get-ChildItem $package -File|Sort-Object Name|ForEach-Object {[ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Hash $_.FullName)}});Save}
'TOPRGU_RX12_PACKAGE_COMPLETED_WITHOUT_RECOMPILE_NOT_HARDWARE_VERIFIED'
