# 固定aa661 UMD入力と現在HEADのnative/KMD全compile。PCO library不要。
# warm196 driver object resumeは禁止。UMDのみ旧sourceを明記した固定成果物として使用。
# Cold/source and new strict cache defaults remain in build-ge8300-from-source.ps1.
param([switch]$EnableCandidate,[switch]$UnifiedDisplay,[string]$OutputDirectory,
 [string]$VcToolsDirectory='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.44.35207',[string]$WindowsKit10Version='10.0.22621.0',
 [string]$KernelVcToolsDirectory='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133',
 [switch]$EnableFixedAa661Umd,
 [string]$Warm196Directory='C:/mt8167-shared-surface-warm196-products')
$ErrorActionPreference='Stop'
if(!$EnableCandidate -or !$EnableFixedAa661Umd){throw 'Explicit limited development AND fixed historical UMD opt-in required'}
$scRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $scRoot status --porcelain).Count){throw 'Clean fixed checkout required'}
$scCommit=(& git -C $scRoot rev-parse HEAD)
# A shallow public checkout contains every build source and test fixture.
& python -B "$PSScriptRoot/verify-ge8300-fullsource.py"
if($LASTEXITCODE){throw 'Pinned full-source origin/closure verification failed'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $scRoot 'build\integrated-kernel-aa661-umd'}
$scOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!$scOut.StartsWith($scRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $scOut)){throw 'Fresh checkout-local output required'}
$scKit81="${env:ProgramFiles(x86)}\Windows Kits\8.1";$scKit10="${env:ProgramFiles(x86)}\Windows Kits\10"
if(!$VcToolsDirectory){
 $scVswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
 $scVs=(& $scVswhere -latest -products '*' -property installationPath)
 $scVcCandidates=Get-ChildItem "$scVs\VC\Tools\MSVC" -Directory|Sort-Object Name -Descending
 $VcToolsDirectory=($scVcCandidates|Where-Object{(Test-Path "$($_.FullName)\bin\Hostx64\arm\cl.exe") -and (Test-Path "$($_.FullName)\lib\arm\libcmt.lib")}|Select-Object -First 1).FullName
}
if(!$VcToolsDirectory){throw 'Existing licensed MSVC ARM32 toolchain required; no SDK/toolchain installation is performed'}
$scVc=[IO.Path]::GetFullPath($VcToolsDirectory);$scCl="$scVc\bin\Hostx64\arm\cl.exe"
$scKernelVc=[IO.Path]::GetFullPath($KernelVcToolsDirectory);$scKernelCl="$scKernelVc\bin\Hostx64\arm\cl.exe"
foreach($pin in @(@{path=$scKernelCl;sha='697a8f41ff7fcb71e7c6625f5517ef6e2f8ea146d1e351e078317ff92da0839c'},@{path="$scKernelVc\bin\Hostx64\arm\link.exe";sha='f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'})){if((Get-FileHash $pin.path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pin.sha){throw 'Kernel MSVC14.29 tool pin failed'}}
# Newest SDKs may omit ARM32 UCRT libraries despite installing the headers.
# Select a complete preinstalled ARM32 pair; never install a replacement SDK.
if(!$WindowsKit10Version){
 $WindowsKit10Version=(Get-ChildItem "$scKit10\Lib" -Directory|Where-Object{
  $_.Name -match '^10[.]0[.][0-9]+[.]0$' -and
  (Test-Path "$($_.FullName)\ucrt\arm\libucrt.lib") -and
  (Test-Path "$scKit10\Include\$($_.Name)\ucrt\stdio.h")
 }|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1).Name
}
if(!$WindowsKit10Version){throw 'Existing Windows SDK with ARM32 UCRT headers/libraries required'}
foreach($scRequired in @($scCl,"$scVc\include\vcruntime.h","$scVc\lib\arm\libcmt.lib","$scKit81\Lib\winv6.3\km\arm\displib.lib","$scKit10\Include\$WindowsKit10Version\ucrt\stdio.h","$scKit10\Lib\$WindowsKit10Version\ucrt\arm\libucrt.lib")){if(!(Test-Path $scRequired)){throw "Required toolchain input missing: $scRequired"}}
New-Item -ItemType Directory $scOut,"$scOut\tmp"|Out-Null
$scOldTemp=$env:TEMP;$scOldTmp=$env:TMP
try{
 $env:TEMP="$scOut\tmp";$env:TMP=$env:TEMP
 & python -B "$PSScriptRoot/ge8300_fixed_umd.py" --enable-fixed-umd --warm-directory $Warm196Directory --output "$scOut/umd-input"
 if($LASTEXITCODE){throw 'Fixed aa661 UMD proof failed; no PCO/UMD rebuild fallback'}
 $scPlan=Get-Content "$scRoot\third_party\mesa-pco\source-plan.json" -Raw|ConvertFrom-Json
 $scCodecFlags=@();foreach($a in ($scPlan.units|Where-Object{$_.source -ceq 'drivers/gpu/mt8167/wddm-render/compiler/pco_sm2.c'}).arguments){$scCodecFlags+=([string]$a).Replace('${REPO}',$scRoot.Replace('\','/')).Replace('${VC_INCLUDE}',($scVc+'\include').Replace('\','/')).Replace('${UCRT_INCLUDE}',($scKit10+'\Include\'+$WindowsKit10Version+'\ucrt').Replace('\','/')).Replace('${SDK_SHARED}',($scKit81+'\Include\shared').Replace('\','/')).Replace('${SDK_UM}',($scKit81+'\Include\um').Replace('\','/'))}
 & "$PSScriptRoot\build-ge8300-native-owner-core.ps1" -EnableCandidate -ExpectedSourceCommit $scCommit -OutputDirectory "$scOut\native" -WithMdlTarget -WithOwnerAllocation -WithFactory -UnifiedDisplay:$UnifiedDisplay -ShaderCodecArguments $scCodecFlags
 & "$scRoot\drivers\gpu\mt8167\wddm-render\build-wdk81.ps1" -EnableCandidate -OutputDirectory "$scOut\modules" -MicrosoftArmCompiler $scKernelCl -MicrosoftArmCompilerSha256 (Get-FileHash $scKernelCl -Algorithm SHA256).Hash -MicrosoftCrtInclude "$scKernelVc\include" -MicrosoftUcrtInclude "$scKit10\Include\$WindowsKit10Version\ucrt" -UnifiedDisplay:$UnifiedDisplay
 & "$scRoot\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$scOut\modules\ge8300-kmd-clear.lib" -NativeLibrary "$scOut\native\ge8300-native-owner-core.lib" -OutputFile "$scOut\ge8300-render.sys" -MapFile "$scOut\ge8300-render.map" -MetadataFile "$scOut\kernel-link.json" -MicrosoftLinker "$scKernelVc\bin\Hostx64\arm\link.exe" -UnifiedDisplay:$UnifiedDisplay
 if($UnifiedDisplay){
  # The transport's shared scanout calls the source-built DMA/MMU helpers.
  # Archive its two entry objects and let LINK pull only referenced native units.
  & llvm-lib /machine:arm "/out:$scOut\modules\ge8300-display-transport.lib" "$scOut\modules\transport356.obj" "$scOut\modules\scanout_vidmm356.obj"
  if($LASTEXITCODE){throw 'Display transport source archive failed'}
  & "$scRoot\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$scOut\modules\ge8300-display-transport.lib" -NativeLibrary "$scOut\native\ge8300-native-owner-core.lib" -OutputFile "$scOut\mt8167-display-transport.sys" -MapFile "$scOut\transport.map" -MetadataFile "$scOut\transport-link.json" -MicrosoftLinker "$scKernelVc\bin\Hostx64\arm\link.exe" -UnifiedDisplay
 }
 Copy-Item "$scOut/umd-input/ge8300-umd.dll" "$scOut/ge8300-umd.dll"
 $scNames=@('ge8300-render.sys','ge8300-umd.dll');if($UnifiedDisplay){$scNames+='mt8167-display-transport.sys'}
 [ordered]@{schema='integrated-kernel-fixed-aa661-umd-v1';source_commit=$scCommit;UMDSourceCommit='aa6613149e09b7b7be718c90103a34323c362917';CompilerSourceCommit='f4b92404c4bbe139524a997c5427a8245560e522';source_dirty=$false;native_KMD_objects_reused=0;UMD_recompiled=$false;PCO_rebuilt=0;PCO_libraries_required=$false;SDK_rebuilt=0;warm196_driver_object_resume_used=$false;kernel_vc_tools_directory=$scKernelVc;compile_time_external_UMD_include_closure_recorded=$false;native_manifest_sha256=(Get-FileHash "$scOut/native/build-manifest.json" -Algorithm SHA256).Hash;KMD_manifest_sha256=(Get-FileHash "$scOut/modules/build-manifest.json" -Algorithm SHA256).Hash;kernel_link_manifest_sha256=(Get-FileHash "$scOut/kernel-link.json" -Algorithm SHA256).Hash;transport_link_manifest_sha256=$(if($UnifiedDisplay){(Get-FileHash "$scOut/transport-link.json" -Algorithm SHA256).Hash}else{$null});UMD_receipt_sha256=(Get-FileHash "$scOut/umd-input/receipt.json" -Algorithm SHA256).Hash;source_origins_sha256=(Get-FileHash "$PSScriptRoot/ge8300-fullsource-origins.json" -Algorithm SHA256).Hash;native_map_sha256=(Get-FileHash "$PSScriptRoot/ge8300-native-source-map.json" -Algorithm SHA256).Hash;signed=$false;hardware_verified=$false;DWM_verified=$false;RT_SRV_implemented=$false;blend_implemented=$false;outputs=@($scNames|ForEach-Object{@{name=$_;sha256=(Get-FileHash "$scOut/$_" -Algorithm SHA256).Hash}})}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$scOut/integrated-build-manifest.json"
 'GE8300_INTEGRATED_KERNEL_FIXED_UMD_COMPLETE'
}finally{$env:TEMP=$scOldTemp;$env:TMP=$scOldTmp}
