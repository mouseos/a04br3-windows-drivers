/* OBSOLETE: do not install; UEFI DXE publishes the authoritative SSDT.
 * Optional A04BR3 MMA8652 diagnostic function. Do not include until the
 * I2C1 ownership and clock/pinmux handoff is implemented and validated.
 * The assigned MOU0010 is a board-specific ACPI ID, not a chip identity.
 * SPDX-License-Identifier: BSD-2-Clause-Patent */
DefinitionBlock ("Mma8652.aml", "SSDT", 2, "BENESS", "A04ACCL ", 1)
{
    Scope (_SB)
    {
        Device (ACC0)
        {
            Name (_HID, "MOU0010")
            Name (_UID, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x1100A000, 0x00001000)
            })
        }
    }
}
