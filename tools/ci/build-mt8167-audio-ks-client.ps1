# One client EXE, no target CRT and no AudioSrv dependency. No device/runner operations.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt8167-first-playback\client'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'OutputDirectory must be inside this checkout'
}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
    $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
    $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector `
      -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" `
      -c (Join-Path $root 'tools\winpe\mt8167-audio-first-playback.c') -o (Join-Path $out 'client.obj')
    if($LASTEXITCODE){throw 'Direct KS client ARM32 compilation failed'}
    & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib `
      (Join-Path $out 'client.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'setupapi.lib') `
      (Join-Path $lib 'cfgmgr32.lib') "/out:$out\mt8167-audio-first-playback.exe"
    if($LASTEXITCODE){throw 'Direct KS client ARM32 link failed'}
    & llvm-readobj --file-headers --coff-imports (Join-Path $out 'mt8167-audio-first-playback.exe')
    if($LASTEXITCODE){throw 'KS client PE inspection failed'}
    Copy-Item (Join-Path $root 'tools\winpe\run-mt8167-audio-first-playback.cmd') $out
    $files=@('mt8167-audio-first-playback.exe','run-mt8167-audio-first-playback.cmd')
    @{source_commit=(& git -C $root rev-parse HEAD);diag_interface=11;audible_verified=$false;
      dependencies=@('kernel32.dll','setupapi.dll','cfgmgr32.dll','ksuser.dll (dynamic)');
      files=@($files|ForEach-Object{@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}})
    }|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 (Join-Path $out 'client-manifest.json')
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
