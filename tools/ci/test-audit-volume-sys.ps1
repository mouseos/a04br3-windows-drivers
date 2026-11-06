$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$base=Join-Path $env:RUNNER_TEMP 'a04-audit-volume-sys-fixture'
if(Test-Path $base){throw 'fixture exists'}
New-Item -ItemType Directory -Force "$base\target\Windows\System32\drivers" | Out-Null
if(Test-Path 'R:\'){throw 'R: occupied'}
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst failed'}
 foreach($name in @('mt8167-toprgu.sys','mt8167-shared.sys','bu21025.sys')){[IO.File]::WriteAllText("R:\Windows\System32\drivers\$name","fixture-$name")}
 $src=[IO.File]::ReadAllText("$root\tools\winpe\audit-volume-sys.c")
 $src=$src.Replace('L"C:\\Windows','L"R:\\Windows')
 $start=$src.IndexOf('void mainCRTStartup(void)')
 if($start -lt 0){throw 'entry not found'}
 $src=$src.Substring(0,$start)+@'
void mainCRTStartup(void){HCRYPTPROV p=0;int ok=1;if(!CryptAcquireContextW(&p,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))ExitProcess(3);if(!filehash(p,L"R:\\Windows\\System32\\drivers\\mt8167-toprgu.sys","AUDIT_TOPRGU_SHA256"))ok=0;if(!filehash(p,L"R:\\Windows\\System32\\drivers\\mt8167-shared.sys","AUDIT_SHARED_SHA256"))ok=0;if(!filehash(p,L"R:\\Windows\\System32\\drivers\\bu21025.sys","AUDIT_BU21025_SHA256"))ok=0;CryptReleaseContext(p,0);ExitProcess(ok?0:4);}
'@
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'link failed'}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE){throw 'hash fixture failed'}
 foreach($item in @(@('AUDIT_TOPRGU_SHA256','mt8167-toprgu.sys'),@('AUDIT_SHARED_SHA256','mt8167-shared.sys'),@('AUDIT_BU21025_SHA256','bu21025.sys'))){$expected=(Get-FileHash -Algorithm SHA256 "R:\Windows\System32\drivers\$($item[1])").Hash;if(!($lines -contains "$($item[0])=$expected")){throw 'digest mismatch'}}
 Remove-Item 'R:\Windows\System32\drivers\bu21025.sys'
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 4 -or !($lines -match 'AUDIT_BU21025_SHA256=OPEN_ERROR_')){throw 'missing file not distinguished'}
 Write-Host 'AUDIT_VOLUME_SYS_HASH_FIXTURE_PASS (volume identity and dirty query not tested)'
} finally { & subst R: /d | Out-Null }
