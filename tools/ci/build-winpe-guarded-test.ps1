$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm\kernel32.lib'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$out = Join-Path $env:RUNNER_TEMP 'guarded-write-test'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" "-I$root\drivers\storage\mt8167-msdc" -c "$root\tools\winpe\guarded-write-test.c" -o "$out\test.obj"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 WinPE test compilation failed' }
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c "$root\drivers\storage\mt8167-msdc\gpt_guard.c" -o "$out\gpt.obj"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 GPT guard compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\test.obj" "$out\gpt.obj" $lib "/out:$out\guarded-write-test.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 WinPE test link failed' }
& llvm-readobj --file-headers --coff-imports "$out\guarded-write-test.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 WinPE test inspection failed' }
Write-Host "Experimental executable: $out\guarded-write-test.exe"
