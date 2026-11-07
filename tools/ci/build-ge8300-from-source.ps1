# One checkout -> compiler/pair/native/KMD/UMD/client. No cached driver inputs.
param([switch]$EnableCandidate,[string]$OutputDirectory,
 [string]$VcToolsDirectory,[string]$WindowsKit10Version,
 [ValidateRange(1,8)][int]$CompilerJobs=2)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit limited development source build required'}
$scRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $scRoot status --porcelain).Count){throw 'Clean fixed checkout required'}
$scCommit=(& git -C $scRoot rev-parse HEAD)
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
 & "$PSScriptRoot\build-ge8300-native-owner-core.ps1" -EnableCandidate -ExpectedSourceCommit $scCommit -OutputDirectory "$scOut\native" -WithMdlTarget -WithOwnerAllocation -WithFactory
 & "$scRoot\drivers\gpu\mt8167\wddm-render\build-wdk81.ps1" -EnableCandidate -OutputDirectory "$scOut\modules" -MicrosoftArmCompiler $scCl -MicrosoftArmCompilerSha256 (Get-FileHash $scCl -Algorithm SHA256).Hash -MicrosoftCrtInclude "$scVc\include"
 & "$scRoot\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$scOut\modules\ge8300-kmd-clear.lib" -NativeLibrary "$scOut\native\ge8300-native-owner-core.lib" -OutputFile "$scOut\ge8300-render.sys" -MapFile "$scOut\ge8300-render.map" -MetadataFile "$scOut\kernel-link.json"
 & python -B "$PSScriptRoot\build-ge8300-compiler-source.py" --vc-include "$scVc\include" --ucrt-include "$scKit10\Include\$WindowsKit10Version\ucrt" --sdk-shared "$scKit81\Include\shared" --sdk-um "$scKit81\Include\um" --vc-arm-libraries "$scVc\lib\arm" --sdk-arm-libraries "$scKit81\Lib\winv6.3\um\arm" --ucrt-arm-libraries "$scKit10\Lib\$WindowsKit10Version\ucrt\arm" --output "$scOut\compiler" --jobs $CompilerJobs
 if($LASTEXITCODE){throw 'Source compiler closure failed'}
 & "$scRoot\drivers\gpu\mt8167\wddm-render\build-native-development-wdk81.ps1" -EnableCandidate -EnableLimitedDevelopmentProfile -SourceCompiler -CompilerDirectory "$scOut\compiler" -CompilerManifestSha256 (Get-FileHash "$scOut\compiler\link-manifest.json" -Algorithm SHA256).Hash -PairDirectory "$scOut\compiler" -PairManifestSha256 (Get-FileHash "$scOut\compiler\pair-manifest.json" -Algorithm SHA256).Hash -OutputDirectory "$scOut\umd"
 Copy-Item "$scOut\umd\ge8300-umd.dll","$scOut\umd\ge8300-d3d-clear.exe" $scOut
 [ordered]@{schema='single-checkout-source-build-v1';source_commit=$scCommit;source_dirty=$false;native_source_map_sha256=(Get-FileHash "$PSScriptRoot\ge8300-native-source-map.json" -Algorithm SHA256).Hash;compiler_source_plan_sha256=(Get-FileHash "$scRoot\third_party\mesa-pco\source-plan.json" -Algorithm SHA256).Hash;previous_gpu_artifacts_used=$false;private_hives_used=$false;licensed_toolchain_directory=$scVc;windows_ucrt_sdk_version=$WindowsKit10Version;msvc_arm_compiler_sha256=(Get-FileHash $scCl -Algorithm SHA256).Hash;driver_signed=$false;hardware_verified=$false;limited_development_profile=$true;standard_profile_qualified=$false;outputs=@('ge8300-render.sys','ge8300-umd.dll','ge8300-d3d-clear.exe')|ForEach-Object{@{name=$_;sha256=(Get-FileHash "$scOut\$_" -Algorithm SHA256).Hash.ToLowerInvariant()}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$scOut\source-build-manifest.json"
 'GE8300_SINGLE_CHECKOUT_SOURCE_BUILD_COMPLETE'
}finally{$env:TEMP=$scOldTemp;$env:TMP=$scOldTmp}
