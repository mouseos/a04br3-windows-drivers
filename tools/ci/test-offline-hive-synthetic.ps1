$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'offline-hive-fixture'
if(Test-Path $base){throw 'fixture exists'}
New-Item -ItemType Directory -Path "$base\target","$base\ram" | Out-Null
if((Test-Path 'R:\') -or (Test-Path 'S:\')){throw 'fixture drives in use'}
$certs=@()
function From-Hex([string]$value){$result=New-Object byte[] ($value.Length/2);for($i=0;$i -lt $result.Length;$i++){$result[$i]=[Convert]::ToByte($value.Substring(2*$i,2),16)};return ,$result}
try {
 & subst.exe R: "$base\target";if($LASTEXITCODE){throw 'subst target failed'}
 & subst.exe S: "$base\ram";if($LASTEXITCODE){throw 'subst ram failed'}
 New-Item -ItemType Directory -Force 'R:\Windows\System32\config','R:\Windows\System32\drivers','S:\a04br3\driver-trust','S:\a04br3\update' | Out-Null
 foreach($n in @('mt8167-toprgu.sys','mt8167-shared.sys','bu21025.sys')){[IO.File]::WriteAllText("R:\Windows\System32\drivers\$n","fixture-$n")}
 $pin=@{'toprgu-sys'='mt8167-toprgu.sys';'shared-sys'='mt8167-shared.sys';'touch-sys'='bu21025.sys'}
 foreach($k in $pin.Keys){$hash=(Get-FileHash -Algorithm SHA256 "R:\Windows\System32\drivers\$($pin[$k])").Hash;[IO.File]::WriteAllBytes("S:\a04br3\driver-trust\$k.sha256",(From-Hex $hash))}
 foreach($kind in @('toprgu','shared','touch')){
  $c=New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=A04 Synthetic $kind" -CertStoreLocation 'Cert:\CurrentUser\My';$certs+= $c
  Export-Certificate -Cert $c -FilePath "S:\a04br3\driver-trust\$kind.cer" | Out-Null
  $hash=(Get-FileHash -Algorithm SHA256 "S:\a04br3\driver-trust\$kind.cer").Hash
  [IO.File]::WriteAllBytes("S:\a04br3\driver-trust\$kind-cert.sha256",(From-Hex $hash))
 }
 function Reg([string[]]$arguments){& reg.exe @arguments | Out-Null;if($LASTEXITCODE){throw "registry fixture command failed: $($arguments -join ' ')"}}
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\Select','/v','Current','/t','REG_DWORD','/d','1','/f')
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\ControlSet001\Services\Mt8167Toprgu','/v','Start','/t','REG_DWORD','/d','3','/f')
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\ControlSet001\Services\Mt8167Toprgu','/v','Type','/t','REG_DWORD','/d','1','/f')
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\ControlSet001\Services\Mt8167Toprgu','/v','ImagePath','/t','REG_EXPAND_SZ','/d','\SystemRoot\System32\drivers\mt8167-toprgu.sys','/f')
 Reg -arguments @('save','HKLM\SOFTWARE\A04FIXSYS','R:\Windows\System32\config\SYSTEM','/y');Reg -arguments @('delete','HKLM\SOFTWARE\A04FIXSYS','/f')
 foreach($store in @('Root','TrustedPublisher')){Reg -arguments @('add',"HKLM\SOFTWARE\A04FIXSW\Microsoft\SystemCertificates\$store\Certificates",'/f')}
 Reg -arguments @('save','HKLM\SOFTWARE\A04FIXSW','R:\Windows\System32\config\SOFTWARE','/y');Reg -arguments @('delete','HKLM\SOFTWARE\A04FIXSW','/f')
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-hive-apply.c").Replace('L"C:\\','L"R:\\').Replace('L"X:\\','L"S:\\')
 if($src.Contains('L"C:\\Windows') -or $src.Contains('L"X:\\a04br3')){throw 'fixture path substitution failed'}
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -DA04_HIVE_FIXTURE -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'fixture compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "$lib\crypt32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'fixture link failed'}
 foreach($n in @('mt8167-toprgu.sys','mt8167-shared.sys','bu21025.sys')){Copy-Item "R:\Windows\System32\drivers\$n" "S:\a04br3\update\$n"}
 & "$base\fixture.exe" --payload-check;if($LASTEXITCODE){throw 'valid payload rejected'}
 [IO.File]::AppendAllText('S:\a04br3\update\bu21025.sys','tampered')
 & "$base\fixture.exe" --payload-check;if($LASTEXITCODE -ne 3){throw 'tampered payload accepted'}
 Copy-Item 'R:\Windows\System32\drivers\bu21025.sys' 'S:\a04br3\update\bu21025.sys' -Force
 & "$base\fixture.exe" --check;if($LASTEXITCODE){throw 'valid inputs rejected'}
 & "$base\fixture.exe" --apply;if($LASTEXITCODE -ne 5){throw 'missing journal not rejected'}
 $env:A04_FIXTURE_JOURNAL='1'; & "$base\fixture.exe" --apply;if($LASTEXITCODE){throw 'hive apply failed'}
 Reg -arguments @('load','HKLM\A04VERIFY','R:\Windows\System32\config\SYSTEM')
 $q=& reg.exe query 'HKLM\A04VERIFY\ControlSet001\Services\Mt8167Toprgu' /v Start
 if($LASTEXITCODE -or ($q -join ' ') -notmatch '0x0'){throw 'service value mismatch'}
 Reg -arguments @('unload','HKLM\A04VERIFY')
 Reg -arguments @('load','HKLM\A04VERIFY','R:\Windows\System32\config\SOFTWARE')
 foreach($store in @('Root','TrustedPublisher')){foreach($c in $certs){& reg.exe query "HKLM\A04VERIFY\Microsoft\SystemCertificates\$store\Certificates\$($c.Thumbprint)" /v Blob | Out-Null;if($LASTEXITCODE){throw 'certificate blob missing'}}}
 Reg -arguments @('unload','HKLM\A04VERIFY')
 & "$base\fixture.exe" --apply;if($LASTEXITCODE){throw 'idempotent hive apply failed'}
 [IO.File]::WriteAllBytes('S:\a04br3\driver-trust\touch-sys.sha256',[byte[]]::new(32))
 & "$base\fixture.exe" --apply;if($LASTEXITCODE -ne 3){throw 'bad driver pin accepted'}
 Write-Host 'OFFLINE_HIVE_SYNTHETIC_PASS'
} finally {
 $ErrorActionPreference='Continue'
 Remove-Item Env:A04_FIXTURE_JOURNAL -ErrorAction SilentlyContinue
 foreach($name in @('A04VERIFY','A04TXSYS','A04TXSW')){& cmd.exe /c "reg unload HKLM\$name >nul 2>nul" | Out-Null}
 foreach($name in @('A04FIXSYS','A04FIXSW')){& cmd.exe /c "reg delete HKLM\SOFTWARE\$name /f >nul 2>nul" | Out-Null}
 foreach($c in $certs){Remove-Item -LiteralPath ("Cert:\CurrentUser\My\"+$c.Thumbprint) -Force -ErrorAction SilentlyContinue}
 & subst.exe R: /d; & subst.exe S: /d
}
$global:LASTEXITCODE=0
