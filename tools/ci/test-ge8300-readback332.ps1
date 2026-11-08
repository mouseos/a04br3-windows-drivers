param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$VcToolsDirectory,[Parameter(Mandatory=$true)][string]$UmHeaderRoot,[Parameter(Mandatory=$true)][string]$KernelHeaderRoot)
$ErrorActionPreference='Stop';$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'New checkout-local output required'}
New-Item -ItemType Directory $out|Out-Null
$kit="${env:ProgramFiles(x86)}\Windows Kits\10";$v='10.0.17763.0';$vc=$VcToolsDirectory;$cl="$vc\bin\Hostx64\x64\cl.exe";$old=$env:PATH;$env:PATH="$vc\bin\Hostx64\x64;$env:PATH"
try {
 $f=@('/nologo','/std:c11','/TC','/O2','/GS-','/D_WIN32_WINNT=0x0603','/DNTDDI_VERSION=0x06030000','/DD3D_UMD_INTERFACE_VERSION=0x4002','/DDXGKDDI_INTERFACE_VERSION=0x4002',"/I$vc\include","/I$kit\Include\$v\ucrt","/I$UmHeaderRoot\shared","/I$UmHeaderRoot\um","/I$KernelHeaderRoot\km")
 & $cl @f "$root\tests\ge8300_readback_ddi_test.c" "$root\drivers\gpu\mt8167\wddm-render\umd\clear_ddi.c" "$root\drivers\gpu\mt8167\wddm-render\clear_protocol.c" "/Fo$out\\" "/Fe$out\readback-ddi.exe" /link "/LIBPATH:$vc\lib\x64" "/LIBPATH:$kit\Lib\$v\ucrt\x64" "/LIBPATH:$kit\Lib\$v\um\x64" kernel32.lib
 if($LASTEXITCODE){throw 'Production DDI compile'}
 & "$out\readback-ddi.exe";if($LASTEXITCODE){throw 'Production DDI regression'}
 & $cl @f "$root\tests\ge8300_readback_wait_test.c" "/Fo$out\wait.obj" "/Fe$out\wait.exe" /link "/LIBPATH:$vc\lib\x64" "/LIBPATH:$kit\Lib\$v\ucrt\x64" "/LIBPATH:$kit\Lib\$v\um\x64" kernel32.lib
 if($LASTEXITCODE){throw 'Production wait compile'}
 & "$out\wait.exe";if($LASTEXITCODE){throw 'Wait regression'}
 @{schema='readback332-production-DDI-v1';exit_code=0;hardware_tested=$false;runtime_callbacks_mocked=$true;production_Lock_Blt_and_wait=$true}|ConvertTo-Json|Set-Content -Encoding UTF8 "$out\receipt.json"
} finally {$env:PATH=$old}
