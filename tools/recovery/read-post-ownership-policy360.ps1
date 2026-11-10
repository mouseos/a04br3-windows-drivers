# SPDX-License-Identifier: BSD-2-Clause-Patent
# Root is the already-loaded private SYSTEM clone; never opens a live control set.
function Read-PostOwnershipPolicy360($Root) {
 $raw=[MT8167PostPolicy360.Policy]::Read($Root)
 [ordered]@{schema=1;name='DisableAutoAcpiPostDeivce';controlset='ControlSet001';raw=$raw;standard_auto_acpi_post_enabled=($raw -ceq '00000000');post_ownership_success_proven=$false}
}
