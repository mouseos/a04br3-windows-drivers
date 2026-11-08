# Driver-only ARM32 package. No device IO, user-mode probe, or SDK rebuild.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$ExpectedSourceCommit,
 [Parameter(Mandatory=$true)][string]$InputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if($ExpectedSourceCommit -cnotmatch '^[0-9a-f]{40}$'){throw 'Explicit source commit required'}
if(Test-Path (Join-Path $root '.git')){
 if((& git -C $root rev-parse HEAD) -cne $ExpectedSourceCommit -or @(& git -C $root status --porcelain).Count){throw 'Clean pinned source required'}
}else{
 if((Get-Content "$InputDirectory\source-commit.txt" -Raw).Trim() -cne $ExpectedSourceCommit){throw 'Archive commit mismatch'}
}
$sourcePins=Get-Content "$InputDirectory\source-inputs.json" -Raw|ConvertFrom-Json
foreach($p in $sourcePins.PSObject.Properties){
 if($p.Name -match '(^|[\\/])\.\.([\\/]|$)' -or [IO.Path]::IsPathRooted($p.Name)){throw 'Relative source input required'}
 if((Get-FileHash (Join-Path $root $p.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $p.Value){throw 'Source byte mismatch'}
}
if(Test-Path $OutputDirectory){throw 'Fresh driver output required'}
$pkg=Join-Path $OutputDirectory 'package';New-Item -ItemType Directory $pkg|Out-Null
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include"
$wdfInc="$inc\wdf\kmdf\1.13";$wdfLib="$kit\Lib\wdf\kmdf\arm\1.13";$kmLib="$kit\Lib\winv6.3\km\arm"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or (Get-Date) -lt $cert.NotBefore -or (Get-Date) -gt $cert.NotAfter){throw 'Existing BCBF private signer required'}
$objects=@();$commands=@()
foreach($name in @('mma8652','mt8167_i2c','sensor_hid','wdf_mma8652')){
 $src=Join-Path $root "drivers\sensors\mma8652\$name.c";$obj=Join-Path $OutputDirectory "$name.obj"
 $args=@('--target=thumbv7-pc-windows-msvc','-w','-fms-extensions','-ffreestanding','-fno-stack-protector','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DMMA8652_UART_TRACE=1',"-I$inc\km","-I$inc\shared","-I$inc\um","-I$wdfInc",'-c',$src,'-o',$obj)
 & clang @args;if($LASTEXITCODE){throw "Required driver TU failed: $name"};$commands+=@(@{tool='clang';args=$args});$objects+=$obj
}
$obj=Join-Path $OutputDirectory 'pio.obj';$args=@('--target=thumbv7-pc-windows-msvc','-ffreestanding','-fno-stack-protector','-c',"$root\drivers\bus\mt8167-i2c\pio.c",'-o',$obj)
& clang @args;if($LASTEXITCODE){throw 'Required I2C PIO TU failed'};$commands+=@(@{tool='clang';args=$args});$objects+=$obj
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -property installationPath
$vc=Get-ChildItem "$vs\VC\Tools\MSVC" -Directory|Sort-Object Name -Descending|Where-Object{Test-Path "$($_.FullName)\bin\Hostx64\x64\link.exe"}|Select-Object -First 1
if(!$vc){throw 'MSVC linker required'};$link="$($vc.FullName)\bin\Hostx64\x64\link.exe"
& $link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects "$wdfLib\wdfdriverentry.lib" "$wdfLib\wdfldr.lib" "$kmLib\ntoskrnl.lib" "$kmLib\hal.lib" "$kmLib\bufferoverflowfastfailk.lib" "$kmLib\libcntpr.lib" "/out:$pkg\mma8652.sys"
if($LASTEXITCODE){throw 'Driver link failed'}
Copy-Item "$root\drivers\sensors\mma8652\mma8652.inf" "$pkg\mma8652.inf"
if(!(Select-String -LiteralPath "$pkg\mma8652.inf" -SimpleMatch 'DriverVer=11/06/2026,0.0.1.10')){throw 'Exact 1.10 INF required'}
$sign="$kit\bin\x86\signtool.exe"
& $sign sign /fd SHA256 /sha1 $thumb /s My "$pkg\mma8652.sys";if($LASTEXITCODE){throw 'BCBF SYS signing failed'}
& "$kit\bin\x86\Inf2Cat.exe" "/driver:$pkg" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'ARM81 catalog failed'}
& $sign sign /fd SHA256 /sha1 $thumb /s My "$pkg\mma8652.cat";if($LASTEXITCODE){throw 'BCBF CAT signing failed'}
$sigs=[ordered]@{}
foreach($n in @('mma8652.sys','mma8652.cat')){
 $s=Get-AuthenticodeSignature "$pkg\$n"
 if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw 'BCBF signature verification failed'}
 $sigs[$n]=@{status=[string]$s.Status;signer_thumbprint=$s.SignerCertificate.Thumbprint}
}
foreach($n in @('mma8652.sys','mma8652.inf')){& $sign verify /pa /v /c "$pkg\mma8652.cat" "$pkg\$n";if($LASTEXITCODE){throw 'Signed CAT member verification failed'}}
function Contains-Bytes([byte[]]$whole,[byte[]]$part){
 for($i=0;$i -le $whole.Length-$part.Length;$i++){$ok=$true;for($j=0;$j -lt $part.Length;$j++){if($whole[$i+$j] -ne $part[$j]){$ok=$false;break}};if($ok){return $true}}
 return $false
}
$descriptor=[IO.File]::ReadAllBytes("$InputDirectory\hid-descriptor.bin")
$baseline="$InputDirectory\captured-upright18.sys"
if((Get-FileHash $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -cne '44760ee15335cbbc9cbf6277e68b7645f9bddfd6ae8ba93cb276a074d96158e1'){throw 'Captured 1.8 input mismatch'}
if(!(Contains-Bytes ([IO.File]::ReadAllBytes($baseline)) $descriptor) -or !(Contains-Bytes ([IO.File]::ReadAllBytes("$pkg\mma8652.sys")) $descriptor)){throw 'Exact unchanged HID descriptor bytes missing'}
Export-Certificate -Cert $cert -FilePath "$pkg\mt8167-development.cer"|Out-Null
$hashes=[ordered]@{};foreach($n in @('mma8652.sys','mma8652.inf','mma8652.cat','mt8167-development.cer')){$hashes[$n]=@{sha256=(Get-FileHash "$pkg\$n" -Algorithm SHA256).Hash.ToLowerInvariant();bytes=(Get-Item "$pkg\$n").Length}}
[ordered]@{schema='mma8652-panel180-110-signed-v1';source_commit=$ExpectedSourceCommit;source_files=$sourcePins;driver_version='0.0.1.10';driverver='11/06/2026,0.0.1.10';signer_thumbprint=$thumb;signature_status='Valid';signatures=$sigs;catalog_members_verified=@('mma8652.sys','mma8652.inf');uart_trace=1;commands=$commands;files=$hashes;descriptor_sha256=(Get-FileHash "$InputDirectory\hid-descriptor.bin" -Algorithm SHA256).Hash.ToLowerInvariant();descriptor_bytes=$descriptor.Length;descriptor_preserved_in_baseline_and_candidate=$true;board_to_hid=@('-Y','X','Z');rebuilt_reason='Old 1.7 has lower DriverVer and another signing identity; fresh 1.10 compile and BCBF signatures are required';runtime_verified=$false;device_verified=$false}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
Compress-Archive "$pkg\*" "$OutputDirectory\package.zip"
'MMA_PANEL180_110_BUILD_SIGN_CAT_COMPLETE'
