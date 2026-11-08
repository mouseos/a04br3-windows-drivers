param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$VcToolsDirectory,[Parameter(Mandatory=$true)][string]$UmHeaderRoot,[Parameter(Mandatory=$true)][string]$KernelHeaderRoot)
$ErrorActionPreference='Stop';$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!(Test-Path "$root\tests\ge8300_texture_ddi_test.c") -or !$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'New checkout-local test directory required'}
New-Item -ItemType Directory $out|Out-Null
$kit="${env:ProgramFiles(x86)}\Windows Kits\10";$version='10.0.17763.0';$vc=$VcToolsDirectory;$cl="$vc\bin\Hostx64\x64\cl.exe";$old=$env:PATH;$env:PATH="$vc\bin\Hostx64\x64;$env:PATH"
try{
 $flags=@('/nologo','/std:c11','/Zc:preprocessor-','/TC','/O2','/GS-','/D_WIN32_WINNT=0x0603','/DNTDDI_VERSION=0x06030000','/DD3D_UMD_INTERFACE_VERSION=0x4002','/DDXGKDDI_INTERFACE_VERSION=0x4002','/D_D3D10_CONSTANTS','/D_D3D10_1_CONSTANTS',"/I$vc\include","/I$kit\Include\$version\ucrt","/I$UmHeaderRoot\shared","/I$UmHeaderRoot\um","/I$KernelHeaderRoot\km")
 & $cl @flags "$root\tests\ge8300_texture_ddi_test.c" "$root\drivers\gpu\mt8167\wddm-render\umd\typed_texture.c" "$root\drivers\gpu\mt8167\wddm-render\umd\typed_state.c" "/Fo$out\\" "/Fe$out\texture-ddi.exe" /link "/LIBPATH:$vc\lib\x64" "/LIBPATH:$kit\Lib\$version\ucrt\x64" "/LIBPATH:$kit\Lib\$version\um\x64" kernel32.lib
 if($LASTEXITCODE){throw 'DDI test compile'}
 $text=@(& "$out\texture-ddi.exe");if($LASTEXITCODE){throw 'DDI regression'};$text
 @{schema='texture326-real-ddi-host-test-v1';source_commit=(& git -C $root rev-parse HEAD);source_path='umd/typed_texture.c + umd/typed_state.c';real_ddi_types=$true;real_production_DDI=$true;allocator_fence_and_lookup_mocked=$true;hardware_tested=$false;exit_code=0;stdout=$text}|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 "$out\receipt.json"
}finally{$env:PATH=$old}
