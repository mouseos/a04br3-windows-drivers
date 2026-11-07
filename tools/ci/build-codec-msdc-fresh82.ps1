# Incremental service-only build. Drivers/client are existing signed immutable inputs.
param([Parameter(Mandatory=$true)][string]$PackageMap,
      [Parameter(Mandatory=$true)][string]$WorkspaceRoot,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$spec=Get-Content $PackageMap -Raw|ConvertFrom-Json
if($spec.transaction -ne 'codec1026-msdc-quiet-fresh82' -or !$spec.backup_manifest -or !$spec.backup_manifest_sha256){throw 'fresh82 binding required; draft is not buildable'}
if(@(& git -C $root status --porcelain).Count){throw 'Committed clean service source required'}
$cert='BCBF6C456C6F0DD0818434763B189488FBF13570'
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
& "$PSScriptRoot\build-mt8167-hardware-probe.ps1" -AudioMode EnginePcm -DeferredPackageMap $PackageMap -WorkspaceRoot $WorkspaceRoot -OutputDirectory $out -Sign -CertificateThumbprint $cert
if($LASTEXITCODE){throw 'Service build failed'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x86\signtool.exe'
& $sign verify /pa /v (Join-Path $out 'MT8167EngineProbe.exe')
if($LASTEXITCODE){throw 'Service signature verify failed'}
# Existing signed SYS and INF membership are rechecked before packaging any service.
$policy=Get-Content (Join-Path $out 'deferred-policy-manifest.json') -Raw|ConvertFrom-Json
foreach($role in @('parent','audio','emmc')){
 $source=if($role -eq 'emmc'){$spec.packages.emmc.directory}else{Join-Path $spec.packages.capture.directory $role}
 $names=@($policy.packages.$role.files.PSObject.Properties.Name|ForEach-Object{Split-Path $_ -Leaf})
 $catalog=@($names|Where-Object{$_.EndsWith('.cat')})
 foreach($name in @($names|Where-Object{!$_.EndsWith('.cat')})){
  & $sign verify /pa /v /c (Join-Path $source $catalog[0]) (Join-Path $source $name)
  if($LASTEXITCODE){throw "Fixed $role CAT membership failed: $name"}
 }
}
Export-Certificate -Cert (Get-Item "Cert:\CurrentUser\My\$cert") -FilePath (Join-Path $out 'development.cer')|Out-Null
$names=@('MT8167EngineProbe.exe','hardware-probe-manifest.json','deferred-policy-manifest.json','development.cer')
[ordered]@{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;transaction=$spec.transaction;
 signer=$cert;backup_manifest_sha256=$policy.backup_manifest_sha256;recovery_boot_id=$policy.recovery_boot_id;
 signed=$true;drivers_rebuilt=$false;client_resigned=$false;hardware_verified=$false;deployment_performed=$false;
 files=@($names|ForEach-Object{@{path=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash.ToLowerInvariant()}})
}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 (Join-Path $out 'signed-manifest.json')
$zipFiles=@($names|ForEach-Object{Join-Path $out $_})+@((Join-Path $out 'signed-manifest.json'))
Compress-Archive -Path $zipFiles -DestinationPath (Join-Path $out 'codec-msdc-engine-service-signed.zip')
Write-Host 'CODEC1026_MSDC_QUIET_FRESH82_SERVICE_SIGNED'
