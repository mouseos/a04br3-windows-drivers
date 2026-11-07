# Immutable WDK membership receipt for the signed 91ee3ce6 WinPE package.
param([Parameter(Mandatory=$true)][string]$Package,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if(Test-Path $Output){throw 'New receipt required'}
$manifest=Join-Path $Package 'signed-manifest.json'
$mh=(Get-FileHash $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
if($mh -cne '11c68361ebc405d81a1b32820813964f6471880787fd863a2327a777b6a73fcd'){throw 'Fixed signed package required'}
$m=Get-Content $manifest -Raw|ConvertFrom-Json
if($m.source_commit -cne '91ee3ce6f773f97d8c6964df1a73e2283ec610b4'){throw 'Fixed package source mismatch'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
$sign="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe"
foreach($e in $m.files){if((Get-FileHash (Join-Path $Package $e.path) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $e.sha256){throw 'Package hash mismatch'}}
foreach($name in @('mt8167-msdc.sys','observe-msdc-storage.exe','mt8167-msdc.cat')){
 & $sign verify /pa /v (Join-Path $Package $name)
 if($LASTEXITCODE -or (Get-AuthenticodeSignature (Join-Path $Package $name)).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature/signer verification failed'}
}
foreach($name in @('mt8167-msdc.sys','a04-msdc-probe.inf')){
 & $sign verify /pa /v /c (Join-Path $Package 'mt8167-msdc.cat') (Join-Path $Package $name)
 if($LASTEXITCODE){throw 'SYS/alias INF CAT membership failed'}
}
@{schema=1;status='verified';package_source_commit=$m.source_commit;package_manifest_sha256=$mh;signer=$thumb;
 signatures=@('mt8167-msdc.sys','observe-msdc-storage.exe','mt8167-msdc.cat');catalog_members=@('mt8167-msdc.sys','a04-msdc-probe.inf');
 verification_recipe_sha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant();files=$m.files;
 sdk='WDK8.1 signtool';hardware_verified=$false;hardware_operations=$false}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $Output
Get-FileHash $Output -Algorithm SHA256|Select-Object Hash|ConvertTo-Json
