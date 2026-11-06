# ARM32 Native WLAN scan probe. No deployment, signing, profile or key input.
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm';$out=Join-Path $root 'build\mt6630-runtime\wlan-probe'
New-Item -ItemType Directory -Force $out | Out-Null
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try {
 $env:TMP=$out;$env:TEMP=$out
 & clang --target=thumbv7-pc-windows-msvc -O2 -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -D_WINSOCK_DEPRECATED_NO_WARNINGS=1 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\mt6630-wlan-probe.c" -o "$out\probe.obj"
 if($LASTEXITCODE){throw 'Native WLAN probe compile failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\probe.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$out\mt6630-wlan-probe.exe"
 if($LASTEXITCODE){throw 'Native WLAN probe link failed'}
 Write-Output "Unsigned Native WLAN API probe: $out. WlanSvc/wlanapi.dll are required on target; missing components are explicitly reported."
} finally {$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
