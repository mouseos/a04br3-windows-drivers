# MT6630 radio arrival observation

run167 diag1 observed root/child Problem0 and successful transport capabilities,
but BluetoothFindFirstRadio returned ERROR_NO_MORE_ITEMS (259). The status client
ended after 235ms/one sample because only NOT_READY and NOT_FOUND were retryable.
This is an observer wait bug, not evidence of a failed capabilities predicate.

Microsoft documents ERROR_NO_MORE_ITEMS as "No Bluetooth radios found":
https://learn.microsoft.com/en-us/windows/win32/api/bluetoothapis/nf-bluetoothapis-bluetoothfindfirstradio

Keep 259 as a retryable *absence*, never success. Preserve one process,
35s elapsed / 141 sample limits, 250ms sleeps, 45s caller timeout, early exit on
actual terminal CM problems or other errors. At the limit, 259 remains failure.
No driver, HCI command, registration, RF operation, power policy or frozen media
changes. The longer observation does not guarantee radio arrival or fix 9F/3.

BT2017/provider1026 remain unchanged. Runtime flags0x7ff, missing0,
commands=acked=128, transport_result0 establish the recorded transport snapshot;
they do not establish a public radio or RF success. New observation is for a later
main-approved normal batch; do not repeat run167 just to exercise this client.
