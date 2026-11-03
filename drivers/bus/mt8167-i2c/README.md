# MT8167 I2C PIO foundation

This is a callback-based one- or two-byte polling transfer, derived from the
BSD-licensed [A04BR3 iOS input code](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/A04BR3Input.cpp).
It provides neither a Windows I2C/SPB controller nor PnP resources. The caller
must own and serialize the entire controller, configure the clock, pinmux,
power and timing, and handle bus errors and timeouts. The helper deliberately
does not reset a shared controller after errors.

It is a building block for the BU21025 protocol; it has not been run on Windows
or tested on a device. Interrupts, DMA, longer transfers, concurrent clients,
power transitions and Windows driver packaging are not implemented.
