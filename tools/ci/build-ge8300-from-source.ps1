# One checkout -> compiler/pair/native/KMD/UMD/client. No cached driver inputs.
param([switch]$EnableCandidate,[switch]$RunHostTests,[switch]$UnifiedDisplay,[string]$OutputDirectory,
 [string]$VcToolsDirectory,[string]$WindowsKit10Version,
 [ValidateRange(1,8)][int]$CompilerJobs=2,
 [string]$VerifiedCompilerDirectory,[string]$ExpectedCompilerCacheManifestSha256,
 [string]$KernelVcToolsDirectory,[switch]$ArchiveCompilerCache,[switch]$AllowAdapterDerivedCompilerCache)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit limited development source build required'}
$scRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $scRoot status --porcelain).Count){throw 'Clean fixed checkout required'}
$scCommit=(& git -C $scRoot rev-parse HEAD)
# A shallow public checkout contains every build source and test fixture.
& python -B "$PSScriptRoot/verify-ge8300-fullsource.py"
if($LASTEXITCODE){throw 'Pinned full-source origin/closure verification failed'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $scRoot 'build\ge8300-source'}
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
# Omitted kernel parameter preserves the previous CLI/default tool selection.
$scKernelVc=$scVc
if($KernelVcToolsDirectory){$scKernelVc=[IO.Path]::GetFullPath($KernelVcToolsDirectory)}
$scKernelCl="$scKernelVc\bin\Hostx64\arm\cl.exe";$scKernelLink="$scKernelVc\bin\Hostx64\arm\link.exe"
if($KernelVcToolsDirectory){
 foreach($pin in @(@{path=$scKernelCl;sha='697a8f41ff7fcb71e7c6625f5517ef6e2f8ea146d1e351e078317ff92da0839c'},@{path=$scKernelLink;sha='f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'})){
  if((Get-FileHash $pin.path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pin.sha){throw 'Explicit kernel toolchain requires pinned MSVC14.29; no fallback'}
 }
}
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
foreach($scRequired in @($scCl,$scKernelCl,$scKernelLink,"$scKernelVc\include\vcruntime.h","$scVc\include\vcruntime.h","$scVc\lib\arm\libcmt.lib","$scKit81\Lib\winv6.3\km\arm\displib.lib","$scKit10\Include\$WindowsKit10Version\ucrt\stdio.h","$scKit10\Lib\$WindowsKit10Version\ucrt\arm\libucrt.lib")){if(!(Test-Path $scRequired)){throw "Required toolchain input missing: $scRequired"}}
# Stop on the known conventional UM lookup failure before the one-time375 build.
foreach($scUmName in @('kernel32.lib','synchronization.lib','setupapi.lib','dxgi.lib','d3d11.lib','d3d9.lib','user32.lib')){
 if(!(Test-Path "$scKit81/Lib/winv6.3/um/arm/$scUmName")){throw "Existing ARM UM library lookup missing: $scUmName; restore the saved kit/junction, do not rebuild SDK"}
}
foreach($scVcName in @('libcmt.lib','libvcruntime.lib','libcpmt.lib')){
 if(!(Test-Path "$scVc/lib/arm/$scVcName")){throw "Existing PCO ARM API library missing: $scVcName"}
}
foreach($scKmName in @('displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib')){
 if(!(Test-Path "$scKit81/Lib/winv6.3/km/arm/$scKmName")){throw "Existing kernel ARM library missing: $scKmName"}
}
New-Item -ItemType Directory $scOut,"$scOut\tmp"|Out-Null
$scOldTemp=$env:TEMP;$scOldTmp=$env:TMP
try{
 $env:TEMP="$scOut\tmp";$env:TMP=$env:TEMP
 if($RunHostTests){
  & "$PSScriptRoot\test-ge8300-texture-ddi.ps1" -OutputDirectory "$scOut\texture-ddi" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version -UmHeaderRoot "$scKit81\Include" -KernelHeaderRoot "$scKit81\Include"
  & python -B "$PSScriptRoot\test-ge8300-fullsource-gate.py" --output "$scOut\source-gate-tests"
  if($LASTEXITCODE){throw 'Wrong-source native compilation map regression failed'}
  & "$PSScriptRoot\test-ge8300-render-residency.ps1" -OutputDirectory "$scOut\host-tests" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version
  & "$PSScriptRoot\test-ge8300-ppp-viewport.ps1" -OutputDirectory "$scOut\host-tests\viewport325" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version
  & "$PSScriptRoot\test-ge8300-rt-signature323.ps1" -OutputDirectory "$scOut\host-tests\signature323" -VcToolsDirectory $scVc -WindowsKit10Version $WindowsKit10Version
 }
 # No native/KMD/UMD reuse parameters are forwarded. Every internal-header
 # consumer and all67 native TUs are rebuilt from this one clean checkout.
 $scCompilerOut="$scOut\compiler";$scCompilerReuseReceipt=$null;$scCompilerCommit=$scCommit
 if($VerifiedCompilerDirectory){
 $scDerivedArgs=@();if($AllowAdapterDerivedCompilerCache){$scDerivedArgs+='--allow-adapter-derived'}
  if($ExpectedCompilerCacheManifestSha256 -notmatch '^[a-fA-F0-9]{64}$'){throw 'Exact compiler cache manifest hash required'}
  $scCompilerOut=[IO.Path]::GetFullPath($VerifiedCompilerDirectory)
  $scCompilerReuseReceipt="$scOut\compiler-cache-verification.json"
  & python -B "$PSScriptRoot\ge8300_compiler_cache.py" --cache $scCompilerOut --expected-link-sha256 $ExpectedCompilerCacheManifestSha256 --vc-include "$scVc\include" --ucrt-include "$scKit10\Include\$WindowsKit10Version\ucrt" --sdk-shared "$scKit81\Include\shared" --sdk-um "$scKit81\Include\um" --vc-arm-libraries "$scVc\lib\arm" --sdk-arm-libraries "$scKit81\Lib\winv6.3\um\arm" --ucrt-arm-libraries "$scKit10\Lib\$WindowsKit10Version\ucrt\arm" --output $scCompilerReuseReceipt @scDerivedArgs
  if($LASTEXITCODE){throw 'PCO375 all-source/include/flags/tools/API cache verification failed; no fallback rebuild'}
  $scCompilerCommit=(Get-Content "$scCompilerOut\link-manifest.json" -Raw|ConvertFrom-Json).source_commit
 }
 if(!$VerifiedCompilerDirectory){
  & python -B "$PSScriptRoot\build-ge8300-compiler-source.py" --vc-include "$scVc\include" --ucrt-include "$scKit10\Include\$WindowsKit10Version\ucrt" --sdk-shared "$scKit81\Include\shared" --sdk-um "$scKit81\Include\um" --vc-arm-libraries "$scVc\lib\arm" --sdk-arm-libraries "$scKit81\Lib\winv6.3\um\arm" --ucrt-arm-libraries "$scKit10\Lib\$WindowsKit10Version\ucrt\arm" --output $scCompilerOut --jobs $CompilerJobs
  if($LASTEXITCODE){throw 'Source compiler closure failed'}
 }
 # Capture all375 objects,10 libraries,pair,raw dependencies and evidence before
 # any driver/client failure can interrupt the build. This never recompiles.
 $scArchiveReceipt=$null
 if($ArchiveCompilerCache){
  $scArchiveReceipt="$scOut/compiler375-cache.zip.receipt.json"
  & python -B "$PSScriptRoot/archive-ge8300-compiler-cache.py" --cache $scCompilerOut --expected-link-sha256 (Get-FileHash "$scCompilerOut/link-manifest.json" -Algorithm SHA256).Hash --output "$scOut/compiler375-cache.zip"
  if($LASTEXITCODE){throw 'Full compiler cache archive verification failed; preserve existing compiler directory and archive without rebuilding'}
 }
 $scPlan=Get-Content "$scRoot\third_party\mesa-pco\source-plan.json" -Raw|ConvertFrom-Json
 $scCodecFlags=@();foreach($a in ($scPlan.units|Where-Object{$_.source -ceq 'drivers/gpu/mt8167/wddm-render/compiler/pco_sm2.c'}).arguments){$scCodecFlags+=([string]$a).Replace('${REPO}',$scRoot.Replace('\','/')).Replace('${VC_INCLUDE}',($scVc+'\include').Replace('\','/')).Replace('${UCRT_INCLUDE}',($scKit10+'\Include\'+$WindowsKit10Version+'\ucrt').Replace('\','/')).Replace('${SDK_SHARED}',($scKit81+'\Include\shared').Replace('\','/')).Replace('${SDK_UM}',($scKit81+'\Include\um').Replace('\','/'))}
 & "$PSScriptRoot\build-ge8300-native-owner-core.ps1" -EnableCandidate -ExpectedSourceCommit $scCommit -OutputDirectory "$scOut\native" -WithMdlTarget -WithOwnerAllocation -WithFactory -UnifiedDisplay:$UnifiedDisplay -ShaderCodecArguments $scCodecFlags
 & "$scRoot\drivers\gpu\mt8167\wddm-render\build-wdk81.ps1" -EnableCandidate -OutputDirectory "$scOut\modules" -MicrosoftArmCompiler $scKernelCl -MicrosoftArmCompilerSha256 (Get-FileHash $scKernelCl -Algorithm SHA256).Hash -MicrosoftCrtInclude "$scKernelVc\include" -MicrosoftUcrtInclude "$scKit10\Include\$WindowsKit10Version\ucrt" -UnifiedDisplay:$UnifiedDisplay
 & "$scRoot\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$scOut\modules\ge8300-kmd-clear.lib" -NativeLibrary "$scOut\native\ge8300-native-owner-core.lib" -OutputFile "$scOut\ge8300-render.sys" -MapFile "$scOut\ge8300-render.map" -MetadataFile "$scOut\kernel-link.json" -MicrosoftLinker $scKernelLink -UnifiedDisplay:$UnifiedDisplay
 if($UnifiedDisplay){
  # The transport's shared scanout calls the source-built DMA/MMU helpers.
  # Archive its two entry objects and let LINK pull only referenced native units.
  & llvm-lib /machine:arm "/out:$scOut\modules\ge8300-display-transport.lib" "$scOut\modules\transport356.obj" "$scOut\modules\scanout_vidmm356.obj"
  if($LASTEXITCODE){throw 'Display transport source archive failed'}
  & "$scRoot\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$scOut\modules\ge8300-display-transport.lib" -NativeLibrary "$scOut\native\ge8300-native-owner-core.lib" -OutputFile "$scOut\mt8167-display-transport.sys" -MapFile "$scOut\transport.map" -MetadataFile "$scOut\transport-link.json" -MicrosoftLinker $scKernelLink -UnifiedDisplay
 }
 $scUmdCacheArgs=@{}
 if($scCompilerReuseReceipt){$scUmdCacheArgs.CompilerReuseReceipt=$scCompilerReuseReceipt;$scUmdCacheArgs.CompilerReuseReceiptSha256=(Get-FileHash $scCompilerReuseReceipt -Algorithm SHA256).Hash}
 & "$scRoot\drivers\gpu\mt8167\wddm-render\build-native-development-wdk81.ps1" -EnableCandidate -EnableLimitedDevelopmentProfile -SourceCompiler -CompilerSourceCommit $scCompilerCommit -CompilerDirectory $scCompilerOut -CompilerManifestSha256 (Get-FileHash "$scCompilerOut\link-manifest.json" -Algorithm SHA256).Hash -PairDirectory $scCompilerOut -PairManifestSha256 (Get-FileHash "$scCompilerOut\pair-manifest.json" -Algorithm SHA256).Hash -OutputDirectory "$scOut\umd" -UnifiedDisplay:$UnifiedDisplay @scUmdCacheArgs
 Copy-Item "$scOut\umd\ge8300-umd.dll","$scOut\umd\ge8300-d3d-clear.exe","$scOut\umd\ge8300-d3d-texture.exe","$scOut\umd\ge8300-d3d-texture-boundary.exe","$scOut\umd\ge8300-d3d-ui-texture.exe","$scOut\umd\mt8167-dxgi-present356.exe" $scOut
 if((& git -C $scRoot rev-parse HEAD) -cne $scCommit -or @(& git -C $scRoot status --porcelain).Count){throw 'Fixed source changed during build'}
 foreach($scChild in @("$scOut/native/build-manifest.json","$scOut/modules/build-manifest.json","$scOut/umd/build-manifest.json")){
  $scChildData=Get-Content $scChild -Raw|ConvertFrom-Json
  if($scChildData.source_commit -cne $scCommit -or $scChildData.source_dirty){throw 'Native/KMD/UMD manifest source differs from fixed HEAD'}
 }
 $scOutputNames=@('ge8300-render.sys','ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','ge8300-d3d-texture-boundary.exe','ge8300-d3d-ui-texture.exe','mt8167-dxgi-present356.exe');if($UnifiedDisplay){$scOutputNames+='mt8167-display-transport.sys'}
 [ordered]@{schema='single-checkout-source-build327-v1';unified_display=$UnifiedDisplay.IsPresent;source_commit=$scCommit;source_dirty=$false;runtime_source_reference='2f210c9f05e784a1245170a1ae525b0b9507d8b0';settle_client_source_reference='bdc6e061ddbaf22121286c81528159ccb55293c0';runtime379_source_closure=$UnifiedDisplay.IsPresent;runtime_byte_reproducibility_claimed=$false;blend_execution_profile='ONE/ZERO or premultiplied ONE/INV_SRC_ALPHA ADD; not general blend';bundle_components=@(@{role='native owner';path='native/ge8300-native-owner-core.lib';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\native\ge8300-native-owner-core.lib" -Algorithm SHA256).Hash},@{role='KMD modules';path='modules/ge8300-kmd-clear.lib';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\modules\ge8300-kmd-clear.lib" -Algorithm SHA256).Hash},@{role='UMD';path='ge8300-umd.dll';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\ge8300-umd.dll" -Algorithm SHA256).Hash},@{role='KMD';path='ge8300-render.sys';source_commit=$scCommit;sha256=(Get-FileHash "$scOut\ge8300-render.sys" -Algorithm SHA256).Hash});native_source_map_sha256=(Get-FileHash "$PSScriptRoot\ge8300-native-source-map.json" -Algorithm SHA256).Hash;compiler_source_plan_sha256=(Get-FileHash "$scRoot\third_party\mesa-pco\source-plan.json" -Algorithm SHA256).Hash;fullsource_origins_sha256=(Get-FileHash "$PSScriptRoot\ge8300-fullsource-origins.json" -Algorithm SHA256).Hash;previous_gpu_artifacts_used=[bool]$VerifiedCompilerDirectory;previous_driver_objects_used=$false;native_KMD_UMD_objects_reused=0;SDK_rebuilt=0;compiler375_cache_reused=[bool]$VerifiedCompilerDirectory;compiler_compiled_source_commit=$scCompilerCommit;compiler_link_manifest_sha256=(Get-FileHash "$scCompilerOut\link-manifest.json" -Algorithm SHA256).Hash;compiler_pair_manifest_sha256=(Get-FileHash "$scCompilerOut\pair-manifest.json" -Algorithm SHA256).Hash;compiler_reuse_receipt_sha256=$(if($scCompilerReuseReceipt){(Get-FileHash $scCompilerReuseReceipt -Algorithm SHA256).Hash}else{$null});native_build_manifest_sha256=(Get-FileHash "$scOut\native\build-manifest.json" -Algorithm SHA256).Hash;KMD_build_manifest_sha256=(Get-FileHash "$scOut\modules\build-manifest.json" -Algorithm SHA256).Hash;UMD_build_manifest_sha256=(Get-FileHash "$scOut\umd\build-manifest.json" -Algorithm SHA256).Hash;kernel_link_manifest_sha256=(Get-FileHash "$scOut\kernel-link.json" -Algorithm SHA256).Hash;shared_surface_contract='SHARED DEFAULT Texture2D single-mip/slice/sample; SRV-only BGRA/RGBA, RT-only BGRA, or RT+SRV descriptor-extent BGRA; existing native storage limits';RT_SRV_implemented=$true;RT_SRV_profile='DEFAULT descriptor-extent BGRA single mip/slice/sample; same allocation; no concurrent read/write';blend_implemented=$true;blend_scope='BGRA8 premultiplied ONE/INV_SRC_ALPHA ADD RGB+A only';scissor_implemented=$true;partial_RT_staging_copy_implemented=$true;render_geometry_profile='descriptor dimensions; pitch128; <=4096000 bytes; <=4096 edges';DWM_verified=$false;private_hives_used=$false;licensed_toolchain_directory=$scVc;compiler_vc_tools_directory=$scVc;kernel_vc_tools_directory=$scKernelVc;kernel_linker_sha256=(Get-FileHash $scKernelLink -Algorithm SHA256).Hash;compiler_cache_archive_sha256=$(if($scArchiveReceipt){(Get-FileHash "$scOut/compiler375-cache.zip" -Algorithm SHA256).Hash}else{$null});compiler_cache_archive_receipt_sha256=$(if($scArchiveReceipt){(Get-FileHash $scArchiveReceipt -Algorithm SHA256).Hash}else{$null});transport_link_manifest_sha256=$(if($UnifiedDisplay){(Get-FileHash "$scOut/transport-link.json" -Algorithm SHA256).Hash}else{$null});windows_ucrt_sdk_version=$WindowsKit10Version;msvc_arm_compiler_sha256=(Get-FileHash $scKernelCl -Algorithm SHA256).Hash;driver_signed=$false;host_tests_requested=[bool]$RunHostTests;host_tests_manifest_sha256=$(if($RunHostTests){(Get-FileHash "$scOut\host-tests\host-tests.json" -Algorithm SHA256).Hash.ToLowerInvariant()}else{$null});hardware_verified=$false;limited_development_profile=$true;standard_profile_qualified=$false;outputs=$scOutputNames|ForEach-Object{@{name=$_;sha256=(Get-FileHash "$scOut\$_" -Algorithm SHA256).Hash.ToLowerInvariant()}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$scOut\source-build-manifest.json"
 Copy-Item "$PSScriptRoot\ge8300-fullsource-origins.json" "$scOut\source-origins.json"
 'GE8300_SINGLE_CHECKOUT_SOURCE_BUILD_COMPLETE' 
}finally{$env:TEMP=$scOldTemp;$env:TMP=$scOldTmp}
