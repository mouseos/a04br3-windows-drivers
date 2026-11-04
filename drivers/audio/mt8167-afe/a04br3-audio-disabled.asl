/* Standalone proposal only: not integrated with firmware. */
DefinitionBlock ("", "SSDT", 2, "A04BR3", "AUDOFF", 1)
{
    External (\_SB, DeviceObj)
    Scope (\_SB)
    {
        Device (AUD0)
        {
            Name (_HID, "MOU0A01") // project-local, not a silicon identifier
            Name (_UID, Zero)
            Name (_STA, Zero)       // never enumerate this unvalidated proposal
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x11140000, 0x1000)
                Memory32Fixed (ReadWrite, 0x11141000, 0x9000)
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) { 152 }
            })
        }
    }
}
