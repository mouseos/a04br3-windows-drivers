# mainが既存Windows build環境で実行する。driver/SDK/PCO/UMDを再buildしない。
param([Parameter(Mandatory=$true)][string]$Csc,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$BuildAndSign)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\out\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'checkout内outの新規出力が必要'}
if(@(& git -C $root status --porcelain).Count -or $LASTEXITCODE){throw 'commit済みのclean sourceが必要'}
$source=Join-Path $root 'tools\usb\UsbAbi3Pnp.cs';$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
New-Item -ItemType Directory $out|Out-Null
$plan=@{source_commit=(& git -C $root rev-parse HEAD).Trim();source_sha256=(Sha $source);platform='ARM';
 driver_rebuilt=$false;installation_performed=$false;build_requested=[bool]$BuildAndSign}
$plan|ConvertTo-Json|Set-Content "$out\build-plan.json"
if(!$BuildAndSign){Write-Output 'USB_HELPER_PLAN_ONLY_NOT_BUILT';return}
$tmp=$env:TMP;$temp=$env:TEMP
try{
 $env:TMP=$out;$env:TEMP=$out
 & $Csc /nologo /target:library /platform:arm /optimize+ "/out:$out\MT8167UsbAbi3Pnp.dll" $source
 if($LASTEXITCODE){throw 'ARM managed helper compile失敗'}
 & $SignTool sign /fd SHA256 /sha1 $thumb "$out\MT8167UsbAbi3Pnp.dll"
 if($LASTEXITCODE){throw 'helper署名失敗'}
 & $SignTool verify /pa /v "$out\MT8167UsbAbi3Pnp.dll"
 if($LASTEXITCODE){throw 'helper署名検証失敗'}
 $plan.schema='MT8167_USB_ABI3_PNP_HELPER_V1';$plan.dll_sha256=Sha "$out\MT8167UsbAbi3Pnp.dll"
 $plan.Csc_sha256=Sha $Csc;$plan.SignTool_sha256=Sha $SignTool;$plan.signer=$thumb
 $plan|ConvertTo-Json|Set-Content -Encoding UTF8 "$out\helper-manifest.json"
 Write-Output 'USB_HELPER_ARM_SIGNED_NOT_INSTALLED'
}finally{$env:TMP=$tmp;$env:TEMP=$temp}
