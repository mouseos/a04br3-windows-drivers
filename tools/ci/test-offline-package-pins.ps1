param([Parameter(Mandatory=$true)][string]$CandidateDirectory)
$ErrorActionPreference='Stop'
if(!$env:RUNNER_TEMP){throw 'RUNNER_TEMP required'}
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'package-pins-synthetic'
if(Test-Path $base){throw 'fixture exists'}
New-Item -ItemType Directory $base | Out-Null
$src="$root\tools\winpe\offline-package-pins.c"
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$arm="$kit\Lib\winv6.3\um\arm";$x64="$kit\Lib\winv6.3\um\x64"
& clang --target=thumbv7-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c $src -o "$base\arm.obj"
if($LASTEXITCODE){throw 'ARM compile'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$base\arm.obj" "$arm\kernel32.lib" "$arm\advapi32.lib" "/out:$base\offline-package-pins.exe"
if($LASTEXITCODE){throw 'ARM link'}
$head= & llvm-readobj --file-headers "$base\offline-package-pins.exe"
if($LASTEXITCODE -or ($head -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARM identity'}
if(Test-Path 'Q:\'){throw 'Q drive in use'}
New-Item -ItemType Directory -Force "$base\fixture\a04br3\update" | Out-Null
New-Item -ItemType Directory -Force "$base\fixture\a04br3\update\packages" | Out-Null
foreach($d in @('toprgu','parent','touch')){ Copy-Item -Recurse (Join-Path $CandidateDirectory $d) "$base\fixture\a04br3\update\packages\$d" }
[IO.File]::WriteAllText("$base\test.c",([IO.File]::ReadAllText($src).Replace('X:\\','Q:\\')))
& clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\test.c" -o "$base\test.obj"
if($LASTEXITCODE){throw 'x64 compile'}
& lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\test.obj" "$x64\kernel32.lib" "$x64\advapi32.lib" "/out:$base\test.exe"
if($LASTEXITCODE){throw 'x64 link'}
try {
 & subst Q: "$base\fixture";if($LASTEXITCODE){throw 'subst'}
 & "$base\test.exe";if($LASTEXITCODE){throw 'matching package rejected'}
 $names=@('toprgu\mt8167-toprgu.sys','toprgu\mt8167-toprgu.inf','toprgu\mt8167-toprgu.cat','parent\mt8167-shared.sys','parent\shared-parent.inf','parent\shared-parent.cat','touch\bu21025.sys','touch\bu21025.inf','touch\bu21025.cat')
 foreach($n in $names){
  $p="Q:\a04br3\update\packages\$n";$old=[IO.File]::ReadAllBytes($p)
  [IO.File]::AppendAllText($p,'alteration')
  & "$base\test.exe" | Out-Null;if($LASTEXITCODE -ne 3){throw "alteration accepted $n"}
  [IO.File]::WriteAllBytes($p,$old)
  Rename-Item $p ($p+'.missing'); & "$base\test.exe" | Out-Null;if($LASTEXITCODE -ne 3){throw "missing accepted $n"};Rename-Item ($p+'.missing') $p
 }
 & "$base\test.exe";if($LASTEXITCODE){throw 'restored input rejected'}
 Write-Host 'PACKAGE_PINS_WDK_ARM_AND_X64_SYNTHETIC_PASS'
} finally { & subst Q: /d }
Get-FileHash -Algorithm SHA256 "$base\offline-package-pins.exe"
