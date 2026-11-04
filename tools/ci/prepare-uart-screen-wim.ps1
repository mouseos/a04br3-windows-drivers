param(
 [Parameter(Mandatory=$true)][string]$SourceWim,
 [Parameter(Mandatory=$true)][string]$OutputWim,
 [Parameter(Mandatory=$true)][string]$DriverPackage,
 [Parameter(Mandatory=$true)][string]$MountDirectory,
 [Parameter(Mandatory=$true)][string]$SourceSha256,
 [Parameter(Mandatory=$true)][string]$SysSha256,
 [Parameter(Mandatory=$true)][string]$ExeSha256,
 [switch]$FullResolution
)
$ErrorActionPreference='Stop'
$exe=Join-Path $DriverPackage 'uart-screen.exe'
if(!(Test-Path $exe -PathType Leaf) -or (Get-FileHash $exe -Algorithm SHA256).Hash -ne $ExeSha256) { throw 'screen EXE mismatch' }
$sig=Get-AuthenticodeSignature $exe
$cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2((Join-Path $DriverPackage 'mt8167-toprgu.cer'))
if(!$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -ne $cert.Thumbprint -or $sig.Status -notin @('Valid','UnknownError')) { throw 'screen EXE signature mismatch' }
$firstMount=$MountDirectory+'-first'
& (Join-Path $PSScriptRoot 'prepare-toprgu-early-wim.ps1') -SourceWim $SourceWim -OutputWim $OutputWim -DriverPackage $DriverPackage -MountDirectory $firstMount -SourceSha256 $SourceSha256 -SysSha256 $SysSha256
if(Test-Path $MountDirectory) { throw 'Second mount already exists' }
New-Item -ItemType Directory -Path $MountDirectory | Out-Null
$mounted=$false
try {
 & dism.exe /Mount-Image "/ImageFile:$OutputWim" /Index:1 "/MountDir:$MountDirectory" | Out-Host
 if($LASTEXITCODE) { throw 'WIM mount failed' };$mounted=$true
 $dst=Join-Path $MountDirectory 'a04br3\uart-screen.exe';if(Test-Path $dst) { throw 'screen EXE already exists' }
 Copy-Item -LiteralPath $exe -Destination $dst
 if((Get-FileHash $dst -Algorithm SHA256).Hash -ne $ExeSha256) { throw 'screen EXE copy mismatch' }
 $start=Join-Path $MountDirectory 'Windows\System32\startnet.cmd'
 $text=[IO.File]::ReadAllText($start,[Text.Encoding]::ASCII)
 if(([regex]::Matches($text,'(?m)^echo MMA8652_TEST_FINISHED\s*$')).Count -ne 1) { throw 'unexpected startnet; refusing modification' }
 $mode=if($FullResolution) { " -f" } else { "" }
 $append="`r`necho UART_SCREEN_CAPTURE_START`r`nX:\a04br3\uart-screen.exe$mode`r`necho UART_SCREEN_CAPTURE_EXIT=%errorlevel%`r`n"
 [IO.File]::WriteAllText($start,$text+$append,[Text.Encoding]::ASCII)
 & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Commit | Out-Host
 if($LASTEXITCODE) { throw 'WIM commit failed' };$mounted=$false
 Get-FileHash -Algorithm SHA256 $OutputWim | Format-List
} finally { if($mounted) { & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Discard | Out-Host } }
