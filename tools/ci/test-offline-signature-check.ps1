$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-signature-fixture';if(Test-Path $base){throw 'fixture exists'}
if(Test-Path 'R:\'){throw 'fixture drive busy'}
New-Item -ItemType Directory -Force $base | Out-Null
try {
 & subst R: $base;if($LASTEXITCODE){throw 'subst failed'}
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-signature-check.c").Replace('L"X:\\a04br3\\update\\mt8167-toprgu.sys"','L"R:\\toprgu.exe"').Replace('L"X:\\a04br3\\update\\bu21025.sys"','L"R:\\touch.exe"')
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\wintrust.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'link failed'}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 2 -or !($lines -contains 'SIGNATURE_PRECHECK_REJECTED_NO_UPDATE')){throw 'missing inputs accepted'}
 $signed=Join-Path $kit 'bin\x86\signtool.exe'
 $sig=Get-AuthenticodeSignature $signed
 if($sig.Status -ne 'Valid' -or $sig.SignatureType -ne 'Authenticode'){throw 'fixture executable is not authenticode trusted'}
 Copy-Item $signed 'R:\toprgu.exe';Copy-Item "$base\fixture.exe" 'R:\touch.exe'
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 2 -or !($lines -contains 'SIGNATURE_PRECHECK_REJECTED_NO_UPDATE')){throw 'unsigned touch accepted'}
 Copy-Item $signed 'R:\touch.exe' -Force
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 3 -or !($lines -contains 'SIGNATURE_WINPE_GENERIC_ONLY_RT_CI_POLICY_UNPROVEN_NO_UPDATE')){throw "generic trust or RT refusal failed: $($lines -join ' | ')"}
 Write-Host 'OFFLINE_SIGNATURE_TRUST_AND_FAIL_CLOSED_PASS (RT CI policy not tested)'
} finally {
 & subst R: /d | Out-Null
}
