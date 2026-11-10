# main所有の既存build環境用。GPU/driver/SDKを再buildせず、単一native helperだけを生成する。
param([Parameter(Mandatory=$true)][string]$Cl,
 [Parameter(Mandatory=$true)][string]$Link,
 [Parameter(Mandatory=$true)][string]$VcInclude,
 [Parameter(Mandatory=$true)][string]$CrtInclude,
 [Parameter(Mandatory=$true)][string]$Kit81Include,
 [Parameter(Mandatory=$true)][string]$ArmUmLib,
 [Parameter(Mandatory=$true)][string]$Rt81Gdi32,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [switch]$BuildAndSign)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\out\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'checkout内outの新規出力が必要'}
if(@(& git -C $root status --porcelain).Count -or $LASTEXITCODE){throw '固定commitのclean checkoutが必要'}
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$contract=Get-Content (Join-Path $root 'tools\gpu\dxg-global-diag198-contract.json') -Raw|ConvertFrom-Json
if((Sha $Rt81Gdi32) -cne $contract.actual_gdi32_sha256){throw 'actual RT8.1 gdi32 export proof不一致'}
if((Sha (Join-Path $Kit81Include 'km\d3dkmthk.h')) -cne $contract.wdk81_d3dkmthk_sha256){throw 'WDK8.1実型header pin不一致'}
$source=Join-Path $root 'tools\gpu\dxg-global-diag-reader.c';$format=Join-Path $root 'tools\gpu\dxg-global-diag-format.h'
New-Item -ItemType Directory $out|Out-Null
$plan=[ordered]@{schema='DXG_GLOBAL_DIAG198_BUILD_V1';source_commit=(& git -C $root rev-parse HEAD).Trim();
 source_sha256=(Sha $source);format_sha256=(Sha $format);contract_sha256=(Sha (Join-Path $root 'tools\gpu\dxg-global-diag198-contract.json'));
 API_Type=9;raw_capacity=262144;actual_gdi32_sha256=(Sha $Rt81Gdi32);Cl_sha256=(Sha $Cl);Link_sha256=(Sha $Link);
 compiled_TUs=0;driver_rebuilt=$false;executed=$false;hardware_verified=$false;RT81_exe_imports_verified=$false;build_requested=[bool]$BuildAndSign}
$plan|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\build-plan.json"
if(!$BuildAndSign){Write-Output 'GLOBAL_DIAG198_PLAN_ONLY_NOT_BUILT';return}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP=$out;$env:TMP=$out
 $compileArgs=@('/nologo','/c','/TC','/O1','/Oi','/GS-','/Zl','/D_ARM_','/DARM','/D_WIN32_WINNT=0x0603','/DNTDDI_VERSION=0x06030000','/D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1',
  "/I$VcInclude","/I$CrtInclude","/I$Kit81Include\um","/I$Kit81Include\shared","/I$Kit81Include\km","/Fo$out\reader.obj",$source)
 & $Cl @compileArgs *> "$out\compile.log"
 if($LASTEXITCODE){throw 'ARM helper compile失敗。compile.logを保持する'}
 & $Link /nologo /machine:arm /subsystem:console,6.03 /entry:mainCRTStartup /nodefaultlib /largeaddressaware /tsaware:no "$out\reader.obj" (Join-Path $ArmUmLib 'kernel32.lib') "/out:$out\dxg-global-diag198.exe" *> "$out\link.log"
 if($LASTEXITCODE){throw 'ARM helper link失敗'}
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 & $SignTool sign /fd SHA256 /sha1 $thumb "$out\dxg-global-diag198.exe" *> "$out\sign.log"
 if($LASTEXITCODE){throw 'helper署名失敗'}
 & $SignTool verify /pa /v "$out\dxg-global-diag198.exe" *> "$out\verify-signature.log"
 if($LASTEXITCODE){throw 'helper署名検証失敗'}
 $signature=Get-AuthenticodeSignature "$out\dxg-global-diag198.exe"
 if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -cne $thumb){throw '署名者不一致'}
 $plan.compiled_TUs=1;$plan.exe_sha256=Sha "$out\dxg-global-diag198.exe";$plan.signer=$thumb
 $plan.SignTool_sha256=Sha $SignTool
 $plan.wdk_d3dkmthk_sha256=Sha (Join-Path $Kit81Include 'km\d3dkmthk.h')
 $plan|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
 Write-Output 'GLOBAL_DIAG198_SIGNED_NOT_RUN_EXE_IMPORT_VERIFICATION_PENDING'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
