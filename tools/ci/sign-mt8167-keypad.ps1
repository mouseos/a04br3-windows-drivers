# 既存署名hostで専用outputへ署名する。driver install/実機操作/鍵exportなし。
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$InputZip,
 [Parameter(Mandatory=$true)][string]$InputSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$Inf2Cat,
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if((Get-FileHash $InputZip -Algorithm SHA256).Hash.ToLowerInvariant() -cne $InputSha256.ToLowerInvariant()){throw 'Input SHA mismatch'}
if(Test-Path $OutputDirectory){throw 'Use dedicated fresh output'}
New-Item -ItemType Directory $OutputDirectory|Out-Null
Expand-Archive $InputZip $OutputDirectory
$m=Get-Content "$OutputDirectory\unsigned-manifest.json" -Raw|ConvertFrom-Json
if($m.target -cne 'MT6392\Buttons' -or $m.service -cne 'Mt6392Keys' -or $m.version -cne '0.0.1.1' -or $m.source_dirty){throw 'Candidate identity mismatch'}
foreach($f in $m.unsigned_outputs){
 if($f.path -cnotin @('package/mt8167-keypad.sys','package/mt8167-keypad.inf')){throw 'Unexpected package input'}
 if((Get-FileHash (Join-Path $OutputDirectory $f.path) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $f.sha256){throw 'Unsigned package mismatch'}
}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 New-Item -ItemType Directory "$OutputDirectory\tmp"|Out-Null
 $env:TEMP="$OutputDirectory\tmp";$env:TMP=$env:TEMP
 $p="$OutputDirectory\package"
 & $SignTool sign /fd SHA256 /sha1 $CertificateThumbprint /s My "$p\mt8167-keypad.sys"
 if($LASTEXITCODE){throw 'SYS sign failed'}
 & $Inf2Cat "/driver:$p" /os:6_3_ARM
 if($LASTEXITCODE){throw 'CAT creation failed'}
 & $SignTool sign /fd SHA256 /sha1 $CertificateThumbprint /s My "$p\mt8167-keypad.cat"
 if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($n in @('mt8167-keypad.sys','mt8167-keypad.cat')){
  & $SignTool verify /pa /q "$p\$n"
  if($LASTEXITCODE -or (Get-AuthenticodeSignature "$p\$n").SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Signature mismatch'}
 }
 foreach($n in @('mt8167-keypad.sys','mt8167-keypad.inf')){
  & $SignTool verify /pa /q /c "$p\mt8167-keypad.cat" "$p\$n"
  if($LASTEXITCODE){throw 'CAT membership failed'}
 }
 $b=[IO.File]::ReadAllBytes("$p\mt8167-keypad.sys");$at=[BitConverter]::ToInt32($b,0x3c)
 if([BitConverter]::ToUInt16($b,$at+4) -ne 0x1c4){throw 'Not ARMNT'}
 $m.status='cross-built-WDK81-signed-catalog-verified-not-device-tested'
 $m|Add-Member -NotePropertyName signer -NotePropertyValue $CertificateThumbprint
 $m|Add-Member -NotePropertyName outputs -NotePropertyValue @(Get-ChildItem $p -File|ForEach-Object {@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
 $m|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$OutputDirectory\signed-manifest.json"
 Compress-Archive -Path $p,"$OutputDirectory\signed-manifest.json","$OutputDirectory\unsigned-manifest.json" -DestinationPath "$OutputDirectory\mt8167-keypad-signed.zip"
 Get-FileHash "$OutputDirectory\mt8167-keypad-signed.zip" -Algorithm SHA256|Format-List
 Write-Output 'MT8167_KEYPAD_SIGNED_RETRIEVAL_READY_NOT_DEVICE_TESTED'
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
