# Opt-in recipe only. Driver compile provenance stays source30cb, not this recipe.
param([switch]$EnableResume,[switch]$PlanOnly,[Parameter(Mandatory=$true)][string]$SourceDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$HandoffReceipt,
 [ValidateRange(1,4)][int]$NativeJobs=1,[Parameter(Mandatory=$true)][string]$VcToolsDirectory,
 [Parameter(Mandatory=$true)][string]$KernelVcToolsDirectory,[Parameter(Mandatory=$true)][string]$WindowsKit10Version)
$ErrorActionPreference='Stop'
if(!$EnableResume){throw 'Explicit resume required; default cold entry is unchanged'}
$recipeRoot=$PSScriptRoot;$root=[IO.Path]::GetFullPath($SourceDirectory);$out=[IO.Path]::GetFullPath((Join-Path $OutputDirectory 'native'))
$commit='30cb20f79ec3c5f71c056d5783fe6743e92bf2ce'
if((& git -C $root rev-parse HEAD) -cne $commit -or @(& git -C $root status --porcelain).Count){throw 'Clean original source30cb required'}
if(Test-Path "$OutputDirectory/modules"){throw 'Resume supports the native-only checkpoint, not existing KMD outputs'}
$nativeFile="$root/tools/ci/build-ge8300-native-owner-core.ps1";$fullFile="$root/tools/ci/build-ge8300-from-source.ps1"
if((Get-FileHash $nativeFile -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'c06c420aba09c73724aa1a4e2c2d199b2ff8fcda66613856ddd79e85b36b5149' -or (Get-FileHash $fullFile -Algorithm SHA256).Hash.ToLowerInvariant() -cne '6ad1169099eabe726ebd5ac21e7455eeaad8296aac17cf1b201cf437da078b42'){throw 'Pinned source recipe mismatch'}
$handoff=$null
if(!$PlanOnly){
$handoff=Get-Content -Raw $HandoffReceipt|ConvertFrom-Json
if(!$handoff.success -or $handoff.selected_exit_code -ne 0){throw 'Successful completed-TU handoff required'}
if(Get-CimInstance Win32_Process -Filter "ProcessId=$($handoff.parent_pid)"|Where-Object {$_.CommandLine -like '*build-ge8300-dwm-source.ps1*'}){throw 'Original serial scheduler still live'}
}
$WdkRoot="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $WdkRoot 'Include';$kmLib=Join-Path $WdkRoot 'Lib\winv6.3\km\arm'
$gpu=Join-Path $root 'drivers\gpu\mt8167';$resource=(& clang -print-resource-dir)
$sourceMap=Get-Content -Raw "$root/tools/ci/ge8300-native-source-map.json"|ConvertFrom-Json
$WithMdlTarget=$true;$WithOwnerAllocation=$true;$WithFactory=$true;[switch]$UnifiedDisplay=$true;[switch]$LinkCheck=$false
$scRoot=$root;$scOut=[IO.Path]::GetFullPath($OutputDirectory);$scVc=[IO.Path]::GetFullPath($VcToolsDirectory);$scKernelVc=[IO.Path]::GetFullPath($KernelVcToolsDirectory)
$scCommit=$commit;$scKernelCl="$scKernelVc/bin/Hostx64/arm/cl.exe";$scKernelLink="$scKernelVc/bin/Hostx64/arm/link.exe"
if((Get-FileHash $scKernelCl -Algorithm SHA256).Hash.ToLowerInvariant() -cne '697a8f41ff7fcb71e7c6625f5517ef6e2f8ea146d1e351e078317ff92da0839c' -or (Get-FileHash $scKernelLink -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'){throw 'Pinned14.29 required'}
$scKit81=$WdkRoot;$scKit10="${env:ProgramFiles(x86)}\Windows Kits\10";$scCompilerOut="$root/build/compiler-adapters"
$VerifiedCompilerDirectory=$scCompilerOut
$scCompilerReuseReceipt="$scOut/compiler-cache-verification.json";$scCompilerCommit=$commit;$scArchiveReceipt=$null;$RunHostTests=$false
if(!(Test-Path $scCompilerReuseReceipt) -or (Get-FileHash "$scCompilerOut/link-manifest.json" -Algorithm SHA256).Hash.ToLowerInvariant() -cne '4cd723a4786393dcabd85bd53ba9d28a2b86a886f3fe9f865089d2b0e65f9f00'){throw 'Actual previously verified adapter cache required'}
$scPlan=Get-Content "$scRoot/third_party/mesa-pco/source-plan.json" -Raw|ConvertFrom-Json
$ShaderCodecArguments=@();foreach($a in ($scPlan.units|Where-Object{$_.source -ceq 'drivers/gpu/mt8167/wddm-render/compiler/pco_sm2.c'}).arguments){$ShaderCodecArguments+=([string]$a).Replace('${REPO}',$scRoot.Replace('\','/')).Replace('${VC_INCLUDE}',($scVc+'\include').Replace('\','/')).Replace('${UCRT_INCLUDE}',($scKit10+'\Include\'+$WindowsKit10Version+'\ucrt').Replace('\','/')).Replace('${SDK_SHARED}',($scKit81+'\Include\shared').Replace('\','/')).Replace('${SDK_UM}',($scKit81+'\Include\um').Replace('\','/'))}
$raw=[IO.File]::ReadAllText($nativeFile);$start=$raw.IndexOf('$units=@(');$end=$raw.IndexOf('$oldTemp=',$start)
# Evaluate only the exact hash-pinned original unit/flag declarations, not a
# copied list or guessed flags. No compile, SDK scanning or output overwrite.
Invoke-Expression $raw.Substring($start,$end-$start)
$common+="-I$out\generated"
$planUnits=@();foreach($unit in $units){$f=@($common);if($unit -in @('wddm-render\compiler\witness_codec','wddm-render\compiler\witness_info')){$f=@($ShaderCodecArguments)+@('-DNDEBUG','-mfpu=none','-DA04_GE8300_UNIFIED_DISPLAY356=1')};$obj=Join-Path $out ($unit.Replace('\','-')+'.obj');$argsForUnit=@($f)+@('-c',(Join-Path $root $sourceMap.native_unit_sources.$unit),'-o',$obj);$planUnits+=@{unit=$unit;object=$obj;arguments=$argsForUnit}}
$compiler=(Get-Command clang -CommandType Application).Source
@{source_commit=$commit;compiler=$compiler;units=$planUnits}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$out/parallel-plan.json"
if($PlanOnly){return}
& python -B "$recipeRoot/ge8300_native_parallel.py" --enable-resume --plan "$out/parallel-plan.json" --handoff $HandoffReceipt --jobs $NativeJobs --output $out
if($LASTEXITCODE){throw 'Native parallel resume failed; preserve per-TU receipts and outputs'}
$resumed=Get-Content -Raw "$out/parallel-resume.json"|ConvertFrom-Json
$commands=@($planUnits|ForEach-Object{$u=$_;$actual=@($resumed.commands|Where-Object{$_.unit -ceq $u.unit});@{tool='clang';arguments=$(if($actual.Count){$actual[0].arguments}else{$u.arguments})}});$objects=@($planUnits|ForEach-Object{$_.object});$reuse=@();$outputs=@();$transcript=$false
$nativeTail=$raw.Substring($raw.IndexOf(' & llvm-lib /machine:arm'),$raw.LastIndexOf('} finally {')-$raw.IndexOf(' & llvm-lib /machine:arm'))
$nativeTail=$nativeTail.Replace('$MyInvocation.MyCommand.Path',("'"+$nativeFile+"'"))
Invoke-Expression $nativeTail
$nativeManifest=Get-Content -Raw "$out/build-manifest.json"|ConvertFrom-Json
$nativeManifest|Add-Member -NotePropertyName controlled_handoff_resume -NotePropertyValue (Get-Content -Raw "$out/parallel-resume.json"|ConvertFrom-Json)
$nativeManifest|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out/build-manifest.json"
# Avoid the observed PowerShell native-stderr path for later original module
# scripts too: real compiler, identical arguments, raw stdout/stderr receipts.
$global:GeoDwmDirectClang=$compiler;$global:GeoDwmDirectHelper="$recipeRoot/ge8300_native_parallel.py";$global:GeoDwmDirectLogs="$scOut/direct-clang-logs"
function global:clang { & python -B $global:GeoDwmDirectHelper --invoke $global:GeoDwmDirectClang $global:GeoDwmDirectLogs @args; $global:LASTEXITCODE=$LASTEXITCODE }
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$scOut/tmp";$env:TMP=$env:TEMP
 $full=[IO.File]::ReadAllText($fullFile);$from=$full.IndexOf(' & "$scRoot\drivers\gpu\mt8167\wddm-render\build-wdk81.ps1"');$to=$full.LastIndexOf('}finally{')
 $SourceCiRoot="$root/tools/ci"
 $continuation=$full.Substring($from,$to-$from).Replace('$PSScriptRoot','$SourceCiRoot')
 # The pinned original metadata expressions are reused; nested scripts remain
 # the clean source30cb scripts. Only the native stage has been resumed.
 Invoke-Expression $continuation
 $final=Get-Content -Raw "$scOut/source-build-manifest.json"|ConvertFrom-Json
 $retained=@($handoff.completed_objects).Count
 $final.previous_driver_objects_used=$true;$final.native_KMD_UMD_objects_reused=$retained
 $final|Add-Member -NotePropertyName same_invocation_resume -NotePropertyValue @{recipe_source_commit=(& git -C $recipeRoot rev-parse HEAD);compile_source_commit=$commit;native_completed_objects_retained=$retained;retained_objects_recompiled=0;historical_native_exit_codes_recorded=$false;historical_native_include_traces_recorded=$false;handoff_receipt_sha256=(Get-FileHash $HandoffReceipt -Algorithm SHA256).Hash.ToLowerInvariant();native_resume_receipt_sha256=(Get-FileHash "$out/parallel-resume.json" -Algorithm SHA256).Hash.ToLowerInvariant();metadata_reconstructed_after_controlled_handoff=$true}
 $final|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$scOut/source-build-manifest.json"
}finally{Remove-Item Function:\clang -ErrorAction SilentlyContinue;$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
