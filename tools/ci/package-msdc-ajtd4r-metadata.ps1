# Metadata-only future eMMC package. No SYS compilation, signing, installation or device IO.
param([switch]$EnableMetadataPackage,
 [Parameter(Mandatory=$true)][string]$BaselinePackage,
 [Parameter(Mandatory=$true)][string]$MetadataDirectory,
 [Parameter(Mandatory=$true)][string]$MetadataPlanSha256,
 [Parameter(Mandatory=$true)][string]$DriverVersion,
 [Parameter(Mandatory=$true)][string]$DriverDate,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableMetadataPackage){throw 'Explicit metadata package generation required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $root status --porcelain).Count){throw 'Clean committed packaging source required'}
$cert='BCBF6C456C6F0DD0818434763B189488FBF13570'
function Sha($path){return (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$baseManifest=Join-Path $BaselinePackage 'signed-manifest.json'
if((Sha $baseManifest) -cne '9678bc72f8d99c733fac9bef391d75cbb31d8afe8733e3f504493fd1412be8c1'){throw 'Fixed quiet1.8 signed input required'}
$base=Get-Content $baseManifest -Raw|ConvertFrom-Json
foreach($f in $base.files){if((Sha (Join-Path $BaselinePackage $f.path)) -cne $f.sha256.ToLowerInvariant()){throw 'Baseline package bytes mismatch'}}
$sys=Join-Path $BaselinePackage 'mt8167-msdc.sys'
$sysHash=Sha $sys
if($sysHash -cne 'f92ee2d4e058e345539567153e0b4a20b13669f34f84de017dece2e45707dadd'){throw 'Fixed signed SYS byte required'}
$planPath=Join-Path $MetadataDirectory 'metadata-plan.json'
if($MetadataPlanSha256 -cnotmatch '^[0-9a-f]{64}$' -or (Sha $planPath) -cne $MetadataPlanSha256){throw 'Explicit metadata plan hash required'}
$plan=Get-Content $planPath -Raw|ConvertFrom-Json
if($plan.source_commit -cne '288b9315e74c913c9d5fe064c3cde75ae673eb93' -or $plan.observed_product_name -cne 'AJTD4R' -or $plan.observed_manfid -cne '0x15' -or $plan.observed_oemid -cne '0x0100' -or $null -ne $plan.marketing_part_number -or $plan.evidence_sha256 -cne 'cadb1622ff6bff471037342f77b73080881752516e653473c9361032df3248ee'){throw 'Fixed observed hardware identity proof required'}
$template=Join-Path $MetadataDirectory 'mt8167-msdc.inf.in'
if((Sha $template) -cne '9db522312ba29f58fc9919430ef3c42cb04f02b60acd558d2431dcdca3b76e6b' -or (Sha $template) -cne $plan.template_sha256){throw 'Fixed naming-only template required'}
if($DriverVersion -cnotmatch '^\d+\.\d+\.\d+\.\d+$'){throw 'Four-component version required'}
$parts=@($DriverVersion.Split('.')|ForEach-Object{[int]$_})
if(@($parts|Where-Object{$_ -gt 65535 -or $_ -lt 0}).Count -or ([version]$DriverVersion).CompareTo([version]'0.0.1.8') -le 0){throw 'Explicit roll-forward beyond 1.8 required'}
$date=[DateTime]::ParseExact($DriverDate,'MM/dd/yyyy',[Globalization.CultureInfo]::InvariantCulture)
if($date -lt [DateTime]::ParseExact('11/07/2026','MM/dd/yyyy',[Globalization.CultureInfo]::InvariantCulture) -or $date -gt [DateTime]::UtcNow.Date){throw 'Explicit non-future INF date required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
$original=[IO.File]::ReadAllText((Join-Path $BaselinePackage 'mt8167-msdc.inf'))
$text=[IO.File]::ReadAllText($template)
$restore=$text.Replace('Provider = "mouseos"','Provider = "TAB-A04-BR3 development"').Replace('MediaTek MT8167 MSDC0 / Samsung AJTD4R eMMC','MediaTek MT8167 MSDC eMMC Host Controller').Replace('DiskName = "mouseos MT8167 MSDC0 eMMC Driver"','DiskName = "MediaTek MT8167 MSDC eMMC Driver"')
if($restore -cne $original){throw 'Changes outside the fixed naming fields rejected'}
$oldLine='DriverVer = 11/07/2026,0.0.1.8'
if(([regex]::Matches($text,[regex]::Escape($oldLine))).Count -ne 1){throw 'Fixed baseline DriverVer required'}
$text=$text.Replace($oldLine,"DriverVer = $DriverDate,$DriverVersion")
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$sign="$kit\bin\x86\signtool.exe"
& $sign verify /pa /c (Join-Path $BaselinePackage 'mt8167-msdc.cat') $sys
if($LASTEXITCODE){throw 'Baseline SYS catalog membership failed'}
$signature=Get-AuthenticodeSignature $sys
if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ne $cert){throw 'Existing signed SYS signer mismatch'}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 Copy-Item $sys "$out\mt8167-msdc.sys"
 Copy-Item (Join-Path $BaselinePackage 'development.cer') $out
 [IO.File]::WriteAllText("$out\mt8167-msdc.inf",$text,[Text.Encoding]::ASCII)
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$out" /os:6_3_ARM
 if($LASTEXITCODE){throw 'New metadata INF catalog failed'}
 & $sign sign /fd SHA256 /sha1 $cert /s My "$out\mt8167-msdc.cat"
 if($LASTEXITCODE){throw 'New CAT signing failed'}
 foreach($name in @('mt8167-msdc.inf','mt8167-msdc.sys')){
  & $sign verify /pa /c "$out\mt8167-msdc.cat" "$out\$name"
  if($LASTEXITCODE){throw 'New CAT member verification failed'}
 }
 if((Sha "$out\mt8167-msdc.sys") -cne $sysHash -or (Sha $sys) -cne $sysHash){throw 'Signed SYS byte preservation failed'}
 [ordered]@{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;metadata_only=$true;binary_source_commit=$base.source_commit;baseline_manifest_sha256=(Sha $baseManifest);metadata_plan_sha256=$MetadataPlanSha256;driver_ver="$DriverDate,$DriverVersion";provider='mouseos';observed_product='AJTD4R';manufacturer='Samsung';marketing_part_number=$null;sys_rebuilt=$false;sys_resigned=$false;sys_sha256=$sysHash;dma_timeout_clock_acpi_changed=$false;deployment_performed=$false;hardware_verified=$false;files=@('mt8167-msdc.inf','mt8167-msdc.sys','mt8167-msdc.cat','development.cer')|ForEach-Object{@{path=$_;sha256=(Sha (Join-Path $out $_))}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
