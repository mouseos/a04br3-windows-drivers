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
if($m.target -cne 'MT6392\Buttons' -or $m.service -cne 'Mt6392Keys' -or $m.version -cnotin @('0.0.1.2','0.0.1.3','0.0.1.4') -or $m.source_dirty){throw 'Candidate identity mismatch'}
if($m.version -ceq '0.0.1.3' -and ($m.schema -cne 'mt8167-keypad13-incremental-v1' -or @($m.compiled).Count -ne 1 -or @($m.reused).Count -ne 2 -or $m.source_commit -cnotmatch '^[0-9a-f]{40}$')){throw 'Incremental 1.3 provenance mismatch'}
$keypad14=$null;$payload14Before=$null
if($m.version -ceq '0.0.1.4'){
 . (Join-Path $PSScriptRoot 'keypad14-signing-gate.ps1')
 $keypad14=Assert-Keypad14SigningInput $OutputDirectory $m $InputSha256 $CertificateThumbprint $SignTool $Inf2Cat
 $payload14Before=Get-Keypad14Payload (Join-Path $OutputDirectory 'package/mt8167-keypad.sys') 13312 $false
 if(!$m.PSObject.Properties['status']){$m|Add-Member -NotePropertyName status -NotePropertyValue 'unsigned-observation-candidate'}
}
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
 if($keypad14){
  $after=Get-Keypad14Payload "$p/mt8167-keypad.sys" $payload14Before.reference_bytes $true
  if($after.sha256 -cne $payload14Before.sha256){throw '1.4 non-signature payload changed'}
  $proof=@{schema='mt8167-keypad14-signature-binding-v1';unsigned_manifest_sha256=$keypad14.pins.unsigned_manifest_sha256;input_zip_sha256=$InputSha256.ToLowerInvariant();signing_source_commit=$keypad14.signing_source_commit;certificate_thumbprint=$CertificateThumbprint;before=$payload14Before;after=$after;non_signature_payload_unchanged=$true;Windows_verify_success=$true;CAT_membership_success=$true;new_compile=0;new_link=0;device_tested=$false;functional_fix_claimed=$false}
  $proof|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$OutputDirectory/signature-receipt.json"
  $m.signed=$true;$m.CAT_created=$true;$m.metadata_only=$false;$m.payload_changed=$true;$m.new_sign=2
  $m|Add-Member -NotePropertyName signing_source_commit -NotePropertyValue $keypad14.signing_source_commit
  $m|Add-Member -NotePropertyName signature_receipt_sha256 -NotePropertyValue (Get-FileHash "$OutputDirectory/signature-receipt.json" -Algorithm SHA256).Hash.ToLowerInvariant()
 }
 $m.status='cross-built-WDK81-signed-catalog-verified-not-device-tested'
 $m|Add-Member -NotePropertyName signer -NotePropertyValue $CertificateThumbprint
 $m|Add-Member -NotePropertyName outputs -NotePropertyValue @(Get-ChildItem $p -File|ForEach-Object {@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
 $m|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$OutputDirectory\signed-manifest.json"
 $signArchiveInputs=@($p,"$OutputDirectory/signed-manifest.json","$OutputDirectory/unsigned-manifest.json")
 if($keypad14){$signArchiveInputs+="$OutputDirectory/signature-receipt.json"}
 Compress-Archive -Path $signArchiveInputs -DestinationPath "$OutputDirectory/mt8167-keypad-signed.zip
 Get-FileHash "$OutputDirectory\mt8167-keypad-signed.zip" -Algorithm SHA256|Format-List
 Write-Output 'MT8167_KEYPAD_SIGNED_RETRIEVAL_READY_NOT_DEVICE_TESTED'
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
