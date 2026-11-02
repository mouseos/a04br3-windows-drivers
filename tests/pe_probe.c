/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Format probe only: deliberately refuses driver initialization. */
long DriverEntry(void *driver, void *registry)
{
    (void)driver;
    (void)registry;
    return (long)0xc00000bbU;
}
