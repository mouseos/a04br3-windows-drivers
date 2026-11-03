# BU21025 sampling foundation

This component implements the controller command sequence and 12-bit ADC
decoding used by the BSD-licensed [A04BR3 iOS input implementation](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/A04BR3Input.cpp).
It accepts bus and delay callbacks; it does not access hardware by itself.
The caller must serialize transfers and provide I2C, power, GPIO and error
recovery. The existing board description identifies I2C address 0x48; no
address or per-device touch calibration is embedded here.

The optional `mt8167_adapter` connects this protocol to the experimental PIO
helper. A transfer failure latches the adapter in a failed state and no more
commands are sent until its owner has recovered the hardware. A mock test
exercises the combined path; this is not a Windows transport implementation.

`coordinates` maps 12-bit ADC values using bounds supplied at runtime. It
does not define a calibration source or require a user to supply values;
retrieving device-specific data automatically remains unimplemented.

`touch_hid` prepares an experimental single-contact HID descriptor and report
encoding. It requires the caller to supply accurate physical dimensions and
keeps the last contact position for the release report. Independent parsing
and host tests do not establish Windows 8.1 HID acceptance or certification;
the dimension source, contact detection, Windows HID/PnP driver and actual
device behavior remain to be verified.

`contact` evaluates the pressure equation used by the existing iOS port, but
requires a runtime policy for plate resistances and noise threshold. It does
not choose device-specific values, implement debouncing, or prove that the
policy accurately detects touch on this panel.

`pipeline` connects filtering, calibration and HID report encoding. It holds
position across noisy samples and can emit a release on a transport fault;
the eventual driver must serialize calls and retain reports until delivered.
It requires valid platform-provided calibration and pressure policy and does
not provide automatic retrieval of those values.

`device` connects controller sampling and the pipeline using a caller-supplied
pen state. When the pen is up it avoids I2C transfers; on a transfer failure it
latches the fault and produces a release when one was pending. The owner must
deliver reports in order and recover the bus before reinitializing. These
interfaces have only been exercised against host-side mocks.

This is not a Windows HID driver. Windows PnP resources, an I2C controller or
SPB transport, pen interrupts, pressure/contact filtering, calibration and
HID reports remain to be implemented and tested. Host and ARM32 compilation
tests do not demonstrate device operation or wake from sleep.

`tools/ci/build-touch-link-probe.ps1` builds a deliberately non-operational
ARM32 SYS using Windows 8.1 WDK, Clang and LLD. Its entry point always returns
`STATUS_NOT_SUPPORTED`: it only checks that the shared components link in a
kernel image, and must not be installed or treated as a working driver.
`tools/ci/build-kmdf-touch-probe.ps1` independently checks the Windows 8.1
KMDF 1.13 ARM entry and linking path. Its AddDevice callback rejects all
devices; it is unsigned, untested on hardware and not a HID implementation.
