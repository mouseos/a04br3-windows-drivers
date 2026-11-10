# SPDX-License-Identifier: BSD-2-Clause-Patent
param([Parameter(Mandatory=$true)][string]$InputTrio,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(Test-Path $OutputDirectory){throw 'New output required'}
New-Item -ItemType Directory "$OutputDirectory\clone"|Out-Null
Add-Type -Path @("$PSScriptRoot\OfflineSystemHiveNative.cs","$PSScriptRoot\PostOwnershipPolicy360.cs")
$expected=@{SYSTEM='7a1951bbbb624b7e6ee1150c0d07eca128eb30d7265bf0c081a4283583342077';'SYSTEM.LOG1'='5ae1b7643d83a3718c909b1afadbca7a7cbf8518533291849200496b7ac617a1';'SYSTEM.LOG2'='b9242da6b9f8d4944e7de763e9738c75be92c2fab6dd6eefe56e49e00d3b4721'}
foreach($n in $expected.Keys){if((Get-FileHash "$InputTrio\$n" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected[$n]){throw 'Immutable184 hash mismatch'};Copy-Item "$InputTrio\$n" "$OutputDirectory\clone\$n"}
$r=[ordered]@{schema=1;candidate='legacy offscreen policy restoration only';fresh_manifest_sha256='ce675a15cbef38ba91696d9f7f863e364de0fc44ee9f4a15824714c59ecd9775';before=$null;after=$null;negative_reapply_rejected=$false;negative_unchanged=$false;load_rc=-1;unload_rc=-1;privilege_restore_error=-1;originals_unchanged=$false;errors=@();hardware_success=$false}
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$priv=$null;$loaded=$false
try {
 $priv=New-Object MT8167OfflineHive.Privileges
 $r.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\clone\SYSTEM");if($r.load_rc){throw 'Load failed'};$loaded=$true
 $root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 $r.before=[MT8167PostPolicy360.Policy]::Read($root)
 $r.preserved_graphics_subtree_sha256=[MT8167PostPolicy360.Policy]::Apply($mount)
 $r.after=[MT8167PostPolicy360.Policy]::Read($root)
 try{[MT8167PostPolicy360.Policy]::Apply($mount)|Out-Null;throw 'Reapply incorrectly accepted'}catch{if($_.Exception.ToString() -notmatch 'One-shot DWORD1 baseline required'){throw};$r.negative_reapply_rejected=$true}
 $r.negative_unchanged=([MT8167PostPolicy360.Policy]::Read($root) -ceq '00000000' -and [MT8167PostPolicy360.Policy]::Snapshot($root) -ceq $r.preserved_graphics_subtree_sha256)
} catch {$r.errors+=@($_.Exception.ToString())} finally {
 if($root){$root.Dispose()};if($loaded){$r.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)}
 if($priv){$priv.Dispose();$r.privilege_restore_error=$priv.RestoreError}
 $r.originals_unchanged=$true;foreach($n in $expected.Keys){if((Get-FileHash "$InputTrio\$n" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected[$n]){$r.originals_unchanged=$false}}
 $r|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$OutputDirectory\receipt.json"
}
if($r.errors.Count -or $r.load_rc -or $r.unload_rc -or $r.privilege_restore_error -or !$r.originals_unchanged -or !$r.negative_reapply_rejected -or !$r.negative_unchanged){throw 'Policy360 native test failed'}
'POLICY360_NATIVE_CLONE_PASS'
