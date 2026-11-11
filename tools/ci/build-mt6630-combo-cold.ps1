# Source-only cold provider; no driver baseline, retained object, signing or SDK setup.
param([string]$OutputDirectory,[string]$WdkRoot,[string]$WdfRoot,
 [string]$ClangPath='clang',[string]$LinkPath='link.exe')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt6630-combo-cold'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)){throw 'Fresh checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or $commit -notmatch '^[0-9a-f]{40}$'){throw 'Fixed source required'}
$dirty=@(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty.Count){throw 'Clean committed checkout required'}
if(!$WdkRoot){$WdkRoot="${env:ProgramFiles(x86)}\Windows Kits\8.1"}
if(!$WdfRoot){$WdfRoot=$WdkRoot}
$WdkRoot=[IO.Path]::GetFullPath($WdkRoot);$WdfRoot=[IO.Path]::GetFullPath($WdfRoot)
$inc=Join-Path $WdkRoot 'Include';$km=Join-Path $WdkRoot 'Lib\winv6.3\km\arm'
$includes=@((Join-Path $inc 'km'),(Join-Path $inc 'shared'),(Join-Path $inc 'um'),(Join-Path $WdfRoot 'Include\wdf\kmdf\1.13'))
foreach($p in $includes){if(!(Test-Path -LiteralPath $p -PathType Container)){throw "Include root missing: $p"}}
$clang=(Get-Command $ClangPath -CommandType Application -ErrorAction Stop).Source
$link=(Get-Command $LinkPath -CommandType Application -ErrorAction Stop).Source
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$toolPins=@(foreach($p in @($clang,$link)){@{path=$p;sha256=(Sha $p)}})
$libs=@();$libPins=@()
foreach($name in @('ndis','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk')){
 $p=Join-Path $km ($name+'.lib');$libs+=@($p);$libPins+=@{path=$p;sha256=(Sha $p)}
}
$inf=Join-Path $root 'drivers\network\mt6630\mt6630-combo-runtime.inf'
$version=[regex]::Matches([IO.File]::ReadAllText($inf),'(?im)^\s*DriverVer\s*=\s*(\d{2}/\d{2}/\d{4}),\s*(\d+\.\d+\.\d+\.\d+)\s*$')
if($version.Count -ne 1){throw 'Unique INF DriverVer required'}
$planPath=Join-Path $PSScriptRoot 'mt6630-combo-cold-plan.json'
$plan=Get-Content -LiteralPath $planPath -Raw|ConvertFrom-Json
if($plan.schema -cne 'mt6630-combo-cold-plan-v1' -or $plan.objects.Count -ne 43){throw '43-TU public combo plan required'}
$seen=@{};$seenObj=@{}
foreach($x in $plan.objects){
 if($x.source -cnotmatch '^drivers/(network|bluetooth)/mt6630/[a-z0-9_]+\.c$' -or $x.object -cnotmatch '^provider/[a-z0-9_-]+\.obj$' -or $seen.ContainsKey($x.source) -or $seenObj.ContainsKey($x.object)){throw 'Invalid/duplicate public TU'}
 $seen[$x.source]=$true;$seenObj[$x.object]=$true
 if(!(Test-Path -LiteralPath (Join-Path $root $x.source) -PathType Leaf)){throw 'Public TU missing'}
}
$overlay=Join-Path $out 'ndis-overlay';$objDir=Join-Path $out 'obj\provider';$pkg=Join-Path $out 'package\provider'
New-Item -ItemType Directory -Force $overlay,$objDir,$pkg|Out-Null
$header=[IO.File]::ReadAllText((Join-Path $inc 'km\ndis.h'))
$old='#define NDIS_API_VERSION_AVAILABLE(major,minor) ((((0x ## major) << 8) + (0x ## minor)) >= NDIS_MIN_API)'
$new="#define A04_NDIS_HEX_I(x) 0x ## x`r`n#define A04_NDIS_HEX(x) A04_NDIS_HEX_I(x)`r`n#define NDIS_API_VERSION_AVAILABLE(major,minor) (((A04_NDIS_HEX(major) << 8) + A04_NDIS_HEX(minor)) >= NDIS_MIN_API)"
if(!$header.Contains($old)){throw 'Unrecognized WDK8.1 NDIS header'}
[IO.File]::WriteAllText((Join-Path $overlay 'ndis.h'),$header.Replace($old,$new),[Text.Encoding]::ASCII)
$flags=@("-I$overlay",'--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-ffunction-sections','-fdata-sections','-w','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13')
foreach($p in $includes){$flags+=@('-isystem',$p)}
$flags+=@('-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1')
$objects=@();$records=@();$commands=@()
$priorTmp=$env:TMP;$priorTemp=$env:TEMP
$tmp=Join-Path $out 'tmp';New-Item -ItemType Directory $tmp|Out-Null
$env:TMP=$tmp;$env:TEMP=$tmp
Push-Location $root
try {
 foreach($x in $plan.objects){
  $src=Join-Path $root $x.source;$obj=Join-Path "$out\obj" $x.object;$dep=$obj+'.d'
  $argsCompile=@($flags)+@('-MD','-MF',$dep,'-c',$src,'-o',$obj)
  $commands+=@(@{tool=$clang;arguments=$argsCompile})
  & $clang @argsCompile
  if($LASTEXITCODE){throw ('Cold compile failed: '+$x.source)}
  $objects+=@($obj);$records+=@{source=$x.source;source_sha256=(Sha $src);object=$x.object;sha256=(Sha $obj);bytes=(Get-Item $obj).Length;dependency_file=$dep;dependency_sha256=(Sha $dep);compiled=$true;compiled_source_commit=$commit}
 }
 $sys=Join-Path $pkg 'mt6630-combo-runtime.sys'
 $argsLink=@('/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/opt:ref','/incremental:no')+$objects+$libs+@("/out:$sys","/map:$out\provider.map")
 $commands+=@(@{tool=$link;arguments=$argsLink})
 & $link @argsLink
 if($LASTEXITCODE){throw 'Cold provider link failed'}
 Copy-Item -LiteralPath $inf -Destination $pkg
 foreach($p in $toolPins+$libPins){if((Sha $p.path) -cne $p.sha256){throw 'Tool/library changed'}}
 if((& git -C $root rev-parse HEAD).Trim() -cne $commit -or @(& git -C $root status --porcelain --untracked-files=normal).Count){throw 'Source changed'}
 $outputs=@(Get-ChildItem -LiteralPath $pkg -File|ForEach-Object {@{path=$_.Name;bytes=$_.Length;sha256=(Sha $_.FullName)}})
 @{schema='mt6630-combo-cold-unsigned-v1';source_commit=$commit;source_dirty=$false;driver_date=$version[0].Groups[1].Value;version=$version[0].Groups[2].Value;driver_ver=($version[0].Groups[1].Value+','+$version[0].Groups[2].Value);plan_sha256=(Sha $planPath);recipe_sha256=(Sha $PSCommandPath);objects=$records;flags=$flags;commands=$commands;tools=$toolPins;libraries=$libPins;include_roots=$includes;ndis_original_sha256=(Sha (Join-Path $inc 'km\ndis.h'));ndis_overlay_sha256=(Sha (Join-Path $overlay 'ndis.h'));new_compiled_units=43;reused_units=0;private_baseline_required=$false;SDK_setup=$false;hardware_verified=$false;signed=$false;historical_binary_identity_claimed=$false;unsigned_outputs=$outputs}|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 (Join-Path $out 'unsigned-manifest.json')
 Write-Output 'MT6630_COMBO_COLD_UNSIGNED_NOT_HARDWARE_VERIFIED'
} finally {Pop-Location;$env:TMP=$priorTmp;$env:TEMP=$priorTemp}
