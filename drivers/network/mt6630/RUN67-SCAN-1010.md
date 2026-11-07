# run67: scan rejection after successful Native initialization

Observed: parent1015 + WiFi1009, Initialize success/General attrs0/Native attrs0,
FW and calibration successful, Code0, one standard WLAN interface. Scan request
accepted by WLAN service, failure event C0010011. This does not prove firmware
scan-command submission. No runtime checkpoint was present in build1009.

The control gate returns ADAPTER_NOT_READY before scan() if software NIC power
is off or radio.failed is set. Poll/idle reconciliation can set radio.failed
between successful Initialize and a later scan. Exact source is unresolved;
this revision does NOT claim a scan fix or change radio semantics/timeouts.

Build1010 corrects this evidence gap: bounded separate Scan/RuntimeFailure
records, parented to the actual Initialize generation. First runtime fault is
immutable per adapter, separate from at most four scan-entry/exit snapshots.
Records contain only software flags, backend status, credits, observed WHISR/
WASR, last command CID/sequence and OID/status. No extra register reads, no
payload/SSID/MAC/key/calibration data. Writer runs only at PASSIVE_LEVEL.
Logging errors never change OID result or allow failed hardware to run.

Failure phases: 1 poll, 2 RF wake, 3 idle transition, 4 control gate/result.
Scan phases1/2 are entry/return. A successful pre-scan snapshot has not submitted
any firmware command. DriverEntry/Initialize record formats/keys are retained.

Linux sources used for analysis: CT3 gen3 nic/hal and mt6630_reg.h WHISR
FW_OWN_BACK(bit7), ABNORMAL(bit6), TX/RX bits. Existing strict status handling
and power transitions remain unchanged until actual first-failure evidence.
XNU station polling is compared as a reference, not proof of this boot's state.

No device access/testing in this revision. Parent1015/GPU/brightness untouched.

## Native1011: legitimate ownership-return notification

Linux gen3 nic/nic.c nicProcessIST allows WHIER_FW_OWN_BACK_INT_EN in the
interrupt mask. mt6630_reg.h declares WHISR_FW_OWN_BACK_INT bit7. The Windows
station poll accepted only bits0..2 and converted normal bit7 into permanent
radio.failed, causing later scan to return ADAPTER_NOT_READY. Fix the accepted
mask to0x87. Unknown/reserved bits, ABNORMAL and firmware assert still fail.
No credits/ownership/scan completion are inferred from the notification.
WHISR read-clear status is acknowledged exactly once through the bridge.
A real station-poll test verifies two notification-only polls preserve credits
and ownership and do not poison radio; reserved/fault bits remain rejected.

This is a source-proven rejection bug, but run67 did not record runtime WHISR,
so it is still not proof that bit7 caused that particular failure. The first
runtime-failure/scan records remain necessary to assess the next main-owner
trial. No scan success is claimed before real firmware SCAN_DONE.
