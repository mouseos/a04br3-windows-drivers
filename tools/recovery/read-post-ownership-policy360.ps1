# SPDX-License-Identifier: BSD-2-Clause-Patent
# Root is the already-loaded private SYSTEM clone; never opens a live control set.
function Read-PostOwnershipPolicy360($Root) {
 $k=[MT8167OfflineHive.Native]::OpenPath($Root,'ControlSet001\Control\GraphicsDrivers',$false)
 if(!$k){throw 'Missing GraphicsDrivers'}
 try{$raw=[MT8167OfflineHive.Native]::ReadFixedRawValue($k,'DisableAutoAcpiPostDeivce',4,4)}finally{$k.Dispose()}
 [ordered]@{schema=1;name='DisableAutoAcpiPostDeivce';controlset='ControlSet001';raw=$raw;standard_auto_acpi_post_enabled=($raw -ceq '00000000');post_ownership_success_proven=$false}
}
