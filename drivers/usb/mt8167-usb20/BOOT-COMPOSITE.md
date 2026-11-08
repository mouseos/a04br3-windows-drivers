# Direct boot keyboard + mouse, default off

USB1009 selected only one boot interface. A direct composite HID device with
both boot keyboard and boot mouse silently lost mouse input. The explicit
`A04_MUSB_BOOT_COMPOSITE_EXPERIMENT=1` profile now selects one of each,
executes SET_PROTOCOL(boot) against both actual interface numbers and routes
independent interrupt-IN requests through MUSB RX1/RX2. Existing report IDs
use the Windows kbdhid/mouhid top-level collections. This remains a direct
boot HID transport, not a standard Windows USB host controller/root hub.

The descriptor traversal rejects duplicate boot protocols, duplicate
interface numbers, endpoint aliasing and inconsistent interface counts.
FIFO allocations are EP0 bytes0..63, RX1 bytes64..127 and RX2 bytes128..191;
RX IRQ bits1/2, target endpoint and bInterval are independent. INDEX access
remains under the controller lock. NAK/idle on one channel does not stop the
other; disconnect, reset, D0 exit and fault cancel both pending requests.
The existing LED/control path targets the keyboard interface. GET_REPORT
mouse snapshot requires an actual mouse report, and does not inject motion.

Register facts: CT3 kernel4.4 drivers/misc/mediatek/usb20/musb_host.c,
context save/restore and RX endpoint setup use per-endpoint INDEX, RXTYPE,
RXINTERVAL, RXMAXP and multipoint RXFUNCADDR/RXHUBADDR. FIFO register units
are eight bytes, per existing controller allocation. No Linux code copied.

Parent1101, power admission, ID41, GPIO22/34, VUSB/PHY sequencing and host
session health remain unchanged. In particular GPIO/MAC readback is not
proof of physical 5V. run115 establishes selected USB1009 bytes but its
metadata guard prevented CM status collection; that run does not identify a
power failure or establish enumeration. Linux/XNU source alone does not
justify overriding role/source safety. This candidate does not fix or claim
real USB power. No hubs, bulk/storage, arbitrary HID report protocols or
standard Windows USB root-hub/URB support are advertised.

Build only the six changed TUs from the pinned USB1009 object cache via
`tools/ci/build-mt8167-usb-composite-incremental.ps1`. Seven linked objects
are reused byte-for-byte; shared parent/audio/SDK are not rebuilt. Requires
real cached WDK8.1 KM/KMDF1.13 ARM imports and the existing signing identity.

`tests/usb/boot_composite_test.c` exercises both SET_PROTOCOL requests,
indexed RX routing, NAK isolation, idle/wrap and both requests cancelled on
disconnect, plus ambiguous descriptor rejection. ASan/UBSan and the existing
WDF callback/power regression are host checks, not real-device success.
