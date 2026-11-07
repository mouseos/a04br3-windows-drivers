/* MT8167 USB keyboard shared owner. Include only in the main owner's
 * mutually exclusive USB test firmware: replace any BEN0001 shared parent
 * and disable legacy nodes that own these ranges. This is an enabled PnP
 * node, not the offline prototype. No EDK2 common source changes required.
 * The parent creates MT8167\USB20 with MAC, SIF and GSIV104 requirements.
 * Main firmware must provide SPI72/GSIV104's board level-low routing. */
DefinitionBlock ("UsbKeyboard.aml", "SSDT", 2, "BENESS", "A04USB  ", 1)
{
    Scope (\_SB)
    {
        Device (USBO)
        {
            Name (_HID, "BEN0001")
            Name (_UID, Zero)
            Name (_STA, 0x0F)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10000000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10001000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10005000, 0x1000)
                Memory32Fixed (ReadWrite, 0x1000F000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10018000, 0x0710)
            })
        }
    }
}
