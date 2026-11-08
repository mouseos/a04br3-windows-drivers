# Real Windows/WDK structure wrapper regression; no GPU code generation.
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!(($out+'\').StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean source required'}
$vc='C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.29.30133'
$kit="${env:ProgramFiles(x86)}\Windows Kits\10";$version='10.0.22621.0';$inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include"
$cl="$vc\bin\Hostx64\x64\cl.exe";$oldPath=$env:PATH;$env:PATH="$vc\bin\Hostx64\x64;$env:PATH"
New-Item -ItemType Directory $out|Out-Null
try {
 $oldSource=(& git -C $root show '3f8291d094c01206dfaa0a22d1fdccd6eecc5823:drivers/gpu/mt8167/wddm-render/umd/tokenized_shader.c')
 if($LASTEXITCODE){throw 'Old source regression provenance required'}
 # Put a copy beside the original source so its relative includes are exact.
 # Include root source path explicitly instead of touching the clean checkout.
 [IO.File]::WriteAllLines("$out\old-tokenized.c",$oldSource,[Text.UTF8Encoding]::new($false))
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
