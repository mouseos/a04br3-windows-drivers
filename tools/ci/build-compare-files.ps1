param([ValidateSet('arm','x64')] [string]$Arch = 'arm')
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit "Lib\winv6.3\um\$Arch"
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$temp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $temp "a04-compare-$Arch"
New-Item -ItemType Directory -Force $out | Out-Null
$target = if ($Arch -eq 'arm') { 'thumbv7-pc-windows-msvc' } else { 'x86_64-pc-windows-msvc' }
$define = if ($Arch -eq 'arm') { '-D_ARM_' } else { '-D_AMD64_' }
& clang "--target=$target" -w -fms-extensions $define -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\compare-files.c" -o "$out\compare.obj"
if ($LASTEXITCODE -ne 0) { throw 'Compare compilation failed' }
& lld-link /subsystem:console "/machine:$Arch" /entry:mainCRTStartup /nodefaultlib "$out\compare.obj" (Join-Path $lib 'kernel32.lib') "/out:$out\compare-files.exe"
if ($LASTEXITCODE -ne 0) { throw 'Compare link failed' }
Write-Output "$out\compare-files.exe"
