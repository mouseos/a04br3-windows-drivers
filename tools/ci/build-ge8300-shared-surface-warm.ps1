# Explicit warm entry: historical SHA-fixed compiler TOOL, fresh current native/KMD/UMD.
# Cold/source and new strict cache defaults remain in build-ge8300-from-source.ps1.
param([switch]$EnableCandidate,[switch]$RunHostTests,[switch]$UnifiedDisplay,[string]$OutputDirectory,
 [string]$VcToolsDirectory='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.44.35207',[string]$WindowsKit10Version='10.0.22621.0',
 [string]$KernelVcToolsDirectory='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133',
 [switch]$EnablePinnedPrebuiltCompiler,
 [string]$PrebuiltCompilerDirectory='C:/mt8167-public-unified362/source/build/ge8300-source-complete/compiler')
$ErrorActionPreference='Stop'
if(!$EnableCandidate -or !$EnablePinnedPrebuiltCompiler){throw 'Explicit limited development AND pinned-prebuilt compiler opt-in required'}
$scRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $scRoot status --porcelain).Count){throw 'Clean fixed checkout required'}
$scCommit=(& git -C $scRoot rev-parse HEAD)
# A shallow public checkout contains every build source and test fixture.
& python -B "$PSScriptRoot/verify-ge8300-fullsource.py"
if($LASTEXITCODE){throw 'Pinned full-source origin/closure verification failed'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $scRoot 'build\shared-surface-warm'}
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
 $scCompilerOut="$scOut\compiler-tool";$scPrebuiltReceipt="$scOut\prebuilt-compiler-input.json"
 & python -B "$PSScriptRoot\ge8300_prebuilt_compiler.py" --enable-pinned-prebuilt --cache $PrebuiltCompilerDirectory --vc-arm-libraries "$scVc\lib\arm" --sdk-arm-libraries "$scKit81\Lib\winv6.3\um\arm" --ucrt-arm-libraries "$scKit10\Lib\$WindowsKit10Version\ucrt\arm" --stage $scCompilerOut --output $scPrebuiltReceipt
 if($LASTEXITCODE){throw 'Live historical compiler input checks failed; no PCO/SDK rebuild fallback'}
 $scPrebuilt=Get-Content $scPrebuiltReceipt -Raw|ConvertFrom-Json;$scCompilerCommit=$scPrebuilt.CompilerSourceCommit
 if($RunHostTests){
  & "$PSScriptRoot\test-ge8300-texture-ddi.ps1" -OutputDirectory "$scOut\texture-ddi" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version -UmHeaderRoot "$scKit81\Include" -KernelHeaderRoot "$scKit81\Include"
  & python -B "$PSScriptRoot\test-ge8300-fullsource-gate.py" --output "$scOut\source-gate-tests"
  if($LASTEXITCODE){throw 'Wrong-source native compilation map regression failed'}
  & "$PSScriptRoot\test-ge8300-render-residency.ps1" -OutputDirectory "$scOut\host-tests" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version
  & "$PSScriptRoot\test-ge8300-ppp-viewport.ps1" -OutputDirectory "$scOut\host-tests\viewport325" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version
  & "$PSScriptRoot\test-ge8300-rt-signature323.ps1" -OutputDirectory "$scOut\host-tests\signature323" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version
 }
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
 & "$scRoot\drivers\gpu\mt8167\wddm-render\build-native-development-wdk81.ps1" -EnableCandidate -EnableLimitedDevelopmentProfile -PinnedPrebuiltCompiler -CompilerSourceCommit $scCompilerCommit -PrebuiltCompilerReceipt $scPrebuiltReceipt -PrebuiltCompilerReceiptSha256 (Get-FileHash $scPrebuiltReceipt -Algorithm SHA256).Hash -CompilerDirectory $scCompilerOut -CompilerManifestSha256 (Get-FileHash "$scCompilerOut\link-manifest.json" -Algorithm SHA256).Hash -PairDirectory $scCompilerOut -PairManifestSha256 (Get-FileHash "$scCompilerOut\pair-manifest.json" -Algorithm SHA256).Hash -OutputDirectory "$scOut\umd" -UnifiedDisplay:$UnifiedDisplay
 & python -B "$PSScriptRoot\ge8300_prebuilt_compiler.py" --enable-pinned-prebuilt --verify-receipt $scPrebuiltReceipt
 if($LASTEXITCODE){throw 'Pinned compiler inputs changed during warm build'}
 Copy-Item "$scOut\umd\ge8300-umd.dll","$scOut\umd\ge8300-d3d-clear.exe","$scOut\umd\ge8300-d3d-texture.exe","$scOut\umd\ge8300-d3d-texture-boundary.exe","$scOut\umd\ge8300-d3d-ui-texture.exe" $scOut
 $scOutputNames=@('ge8300-render.sys','ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','ge8300-d3d-texture-boundary.exe','ge8300-d3d-ui-texture.exe');if($UnifiedDisplay){$scOutputNames+='mt8167-display-transport.sys'}
 [ordered]@{schema='shared-surface-pinned-prebuilt-warm-v1';unified_display=$UnifiedDisplay.IsPresent;source_commit=$scCommit;source_dirty=$false;bundle_components=@(@{role='native owner';path='native/ge8300-native-owner-core.lib';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\native\ge8300-native-owner-core.lib" -Algorithm SHA256).Hash},@{role='KMD modules';path='modules/ge8300-kmd-clear.lib';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\modules\ge8300-kmd-clear.lib" -Algorithm SHA256).Hash},@{role='UMD';path='ge8300-umd.dll';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\ge8300-umd.dll" -Algorithm SHA256).Hash},@{role='KMD';path='ge8300-render.sys';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\ge8300-render.sys" -Algorithm SHA256).Hash});native_source_map_sha256=(Get-FileHash "$PSScriptRoot\ge8300-native-source-map.json" -Algorithm SHA256).Hash;compiler_source_plan_sha256=(Get-FileHash "$scRoot\third_party\mesa-pco\source-plan.json" -Algorithm SHA256).Hash;fullsource_origins_sha256=(Get-FileHash "$PSScriptRoot\ge8300-fullsource-origins.json" -Algorithm SHA256).Hash;previous_gpu_artifacts_used=$true;previous_driver_objects_used=$false;native_KMD_UMD_objects_reused=0;SDK_rebuilt=0;compiler375_cache_reused=$false;compiler_prebuilt_tool_input_reused=$true;compiler_input_kind='SHA-fixed historical prebuilt compiler tool';CompilerSourceCommit=$scCompilerCommit;includeClosureRecorded=$false;strict_cache_qualified=$false;PCO_rebuilt=0;compiler_compiled_source_commit=$scCompilerCommit;compiler_link_manifest_sha256=(Get-FileHash "$scCompilerOut\link-manifest.json" -Algorithm SHA256).Hash;compiler_pair_manifest_sha256=(Get-FileHash "$scCompilerOut\pair-manifest.json" -Algorithm SHA256).Hash;prebuilt_compiler_receipt_sha256=(Get-FileHash $scPrebuiltReceipt -Algorithm SHA256).Hash;native_build_manifest_sha256=(Get-FileHash "$scOut\native\build-manifest.json" -Algorithm SHA256).Hash;KMD_build_manifest_sha256=(Get-FileHash "$scOut\modules\build-manifest.json" -Algorithm SHA256).Hash;UMD_build_manifest_sha256=(Get-FileHash "$scOut\umd\build-manifest.json" -Algorithm SHA256).Hash;kernel_link_manifest_sha256=(Get-FileHash "$scOut\kernel-link.json" -Algorithm SHA256).Hash;shared_surface_contract='SHARED DEFAULT Texture2D single-mip/slice/sample; SRV-only BGRA/RGBA or RT-only BGRA; existing native storage limits';RT_SRV_implemented=$false;blend_implemented=$false;DWM_verified=$false;private_hives_used=$false;licensed_toolchain_directory=$scVc;windows_ucrt_sdk_version=$WindowsKit10Version;kernel_vc_tools_directory=$scKernelVc;msvc_arm_compiler_sha256=(Get-FileHash $scKernelCl -Algorithm SHA256).Hash;driver_signed=$false;host_tests_requested=[bool]$RunHostTests;host_tests_manifest_sha256=$(if($RunHostTests){(Get-FileHash "$scOut\host-tests\host-tests.json" -Algorithm SHA256).Hash.ToLowerInvariant()}else{$null});hardware_verified=$false;limited_development_profile=$true;standard_profile_qualified=$false;outputs=$scOutputNames|ForEach-Object{@{name=$_;sha256=(Get-FileHash "$scOut\$_" -Algorithm SHA256).Hash.ToLowerInvariant()}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$scOut\warm-build-manifest.json"
 Copy-Item "$PSScriptRoot\ge8300-fullsource-origins.json" "$scOut\source-origins.json"
 'GE8300_PINNED_PREBUILT_WARM_BUILD_COMPLETE' 
}finally{$env:TEMP=$scOldTemp;$env:TMP=$scOldTmp}
