# Host-only actual engine regression. No driver load or hardware operations.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$VcToolsDirectory,[Parameter(Mandatory=$true)][string]$WindowsKit10Version)
$ErrorActionPreference='Stop'
$hrRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $hrRoot status --porcelain).Count){throw 'Clean source required'}
$hrOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!$hrOut.StartsWith($hrRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $hrOut)){throw 'New checkout-local output required'}
$hrKit="${env:ProgramFiles(x86)}\Windows Kits";$hrCl="$VcToolsDirectory\bin\Hostx64\x64\cl.exe"
foreach($hrNeed in @($hrCl,"$VcToolsDirectory\include\vcruntime.h","$VcToolsDirectory\lib\x64\libcmt.lib","$hrKit\10\Lib\$WindowsKit10Version\ucrt\x64\libucrt.lib")){if(!(Test-Path $hrNeed)){throw "Existing x64 host-test toolchain missing: $hrNeed"}}
New-Item -ItemType Directory $hrOut,"$hrOut\tmp"|Out-Null
$hrOldTemp=$env:TEMP;$hrOldTmp=$env:TMP
try{
 $env:TEMP="$hrOut\tmp";$env:TMP=$env:TEMP
 $hrSources=@('tests\ge8300_render_residency_test.c','drivers\gpu\mt8167\wddm-render\clear_engine.c','drivers\gpu\mt8167\wddm-render\clear_protocol.c')
 $hrFiles=@($hrSources|ForEach-Object{Join-Path $hrRoot $_})
 & $hrCl /nologo /TC /std:c11 /O2 /MT /c "/I$VcToolsDirectory\include" "/I$hrKit\10\Include\$WindowsKit10Version\ucrt" "/Fo$hrOut\\" @hrFiles
 if($LASTEXITCODE){throw 'Host test compile failed'}
 & "$VcToolsDirectory\bin\Hostx64\x64\link.exe" /nologo /machine:x64 /subsystem:console "/out:$hrOut\render-residency-test.exe" "$hrOut\ge8300_render_residency_test.obj" "$hrOut\clear_engine.obj" "$hrOut\clear_protocol.obj" "/LIBPATH:$VcToolsDirectory\lib\x64" "/LIBPATH:$hrKit\10\Lib\$WindowsKit10Version\ucrt\x64" "/LIBPATH:$hrKit\8.1\Lib\winv6.3\um\x64"
 if($LASTEXITCODE){throw 'Host test link failed'}
 $hrOutput=@(& "$hrOut\render-residency-test.exe");$hrExit=$LASTEXITCODE
 $hrOutput|Set-Content "$hrOut\stdout.txt"
 if($hrExit){throw 'Host residency regression failed'}
 [ordered]@{schema='ge8300-host-residency-v1';source_commit=(& git -C $hrRoot rev-parse HEAD);source_dirty=$false;test_exit_code=$hrExit;test_source='tests/ge8300_render_residency_test.c';test_source_sha256=(Get-FileHash $hrFiles[0] -Algorithm SHA256).Hash.ToLowerInvariant();compiler_sha256=(Get-FileHash $hrCl -Algorithm SHA256).Hash.ToLowerInvariant();host_only=$true;driver_loaded=$false;hardware_operations=$false;stdout=$hrOutput}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$hrOut\host-tests.json"
 'GE8300_HOST_RESIDENCY_TEST_PASSED'
}finally{$env:TEMP=$hrOldTemp;$env:TMP=$hrOldTmp}
