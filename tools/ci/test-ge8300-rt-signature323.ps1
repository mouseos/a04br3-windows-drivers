# Real Windows/WDK structure wrapper regression; no GPU code generation.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$VcToolsDirectory,[string]$WindowsKit10Version)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!(($out+'\').StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean source required'}
if(!$VcToolsDirectory){
 $vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
 $vs=(& $vswhere -latest -products '*' -property installationPath)
 $VcToolsDirectory=(Get-ChildItem "$vs\VC\Tools\MSVC" -Directory|Sort-Object Name -Descending|Where-Object{Test-Path "$($_.FullName)\bin\Hostx64\x64\cl.exe"}|Select-Object -First 1).FullName
}
if(!$VcToolsDirectory){throw 'Existing x64 MSVC required; no installer is invoked'}
$vc=[IO.Path]::GetFullPath($VcToolsDirectory)
$kit="${env:ProgramFiles(x86)}\Windows Kits\10";if(!$WindowsKit10Version){$WindowsKit10Version=(Get-ChildItem "$kit\Lib" -Directory|Where-Object{Test-Path "$($_.FullName)\ucrt\x64\libucrt.lib"}|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1).Name};$version=$WindowsKit10Version;$inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include"
$cl="$vc\bin\Hostx64\x64\cl.exe";$oldPath=$env:PATH;$env:PATH="$vc\bin\Hostx64\x64;$env:PATH"
New-Item -ItemType Directory $out|Out-Null
try {
 $fixture="$root\tests\fixtures\tokenized_shader_before323.c"
 if((Get-FileHash $fixture -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'c838cb69ea91369084b1d8b4a8ea313d981f7b35925c9b8bc12a97dc6a16890e'){throw 'Pinned pre323 source fixture mismatch'}
 # This source fixture replaces a git-show dependency on unrelated history.
 Copy-Item $fixture "$out\old-tokenized.c"
 $base="$root\drivers\gpu\mt8167\wddm-render";$records=@()
 foreach($mode in @('old','fixed')){
  $flags=@('/nologo','/std:c11','/Zc:preprocessor-','/TC','/O2','/GS-','/D_WIN32_WINNT=0x0603','/DNTDDI_VERSION=0x06030000','/DD3D_UMD_INTERFACE_VERSION=0x4002','/D_D3D10_CONSTANTS','/D_D3D10_1_CONSTANTS',"/I$vc\include","/I$kit\Include\$version\ucrt","/I$inc\shared","/I$inc\um","/I$inc\km","/I$base\umd")
  $source="$base\umd\tokenized_shader.c";if($mode -eq 'old'){$source="$out\old-tokenized.c";$flags+='/DGEO_EXPECT_OLD_TYPE_REJECTION=1'}
  & $cl @flags "$root\tests\ge8300_rt_signature_wrapper_test.c" $source "$base\compiler\sm4.c" "$base\compiler\sm2.c" "/Fo$out\\" "/Fe$out\$mode.exe" /link "/LIBPATH:$vc\lib\x64" "/LIBPATH:$kit\Lib\$version\ucrt\x64" "/LIBPATH:${env:ProgramFiles(x86)}\Windows Kits\8.1\Lib\winv6.3\um\x64" kernel32.lib
  if($LASTEXITCODE){throw "Wrapper compile $mode"}
  $text=@(& "$out\$mode.exe");if($LASTEXITCODE){throw "Wrapper regression $mode"};$text
  $records+=@{mode=$mode;exit_code=0;stdout=$text;source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;executable_sha256=(Get-FileHash "$out\$mode.exe" -Algorithm SHA256).Hash}
 }
 @{schema='rt-signature323-wrapper-regression-v1';source_commit=$commit;real_wdk_signatures=$true;real_production_wrapper=$true;real_sm4_decoder=$true;pco_generation_stubbed=$true;gpu_runtime_tested=$false;runs=$records}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\host-manifest.json"
}finally{$env:PATH=$oldPath}
