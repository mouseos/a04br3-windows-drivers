# BU21025 sampling foundation

This component implements the controller command sequence and 12-bit ADC
decoding used by the BSD-licensed [A04BR3 iOS input implementation](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/A04BR3Input.cpp).
It accepts bus and delay callbacks; it does not access hardware by itself.
The caller must serialize transfers and provide I2C, power, GPIO and error
recovery. The existing board description identifies I2C address 0x48; no
address or per-device touch calibration is embedded here.

This is not a Windows HID driver. Windows PnP resources, an I2C controller or
SPB transport, pen interrupts, pressure/contact filtering, calibration and
HID reports remain to be implemented and tested. Host and ARM32 compilation
tests do not demonstrate device operation or wake from sleep.
