/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_PRIVACY_H
#define A04_NATIVE_PRIVACY_H
/* DOT11_PRIVACY_EXEMPTION.usEtherType contains network-order bytes,
 * unlike the action/packet-type USHORTs. Preserve the NDIS struct in storage
 * and query replies; decode only this actual field for protocol comparisons.
 * Byte access avoids host-endian and alignment assumptions.
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/windot11/ns-windot11-dot11_privacy_exemption
 */
static inline unsigned a04_native_privacy_protocol(const void *ether_type_field)
{
 const unsigned char *p=(const unsigned char *)ether_type_field;
 return ((unsigned)p[0]<<8)|p[1];
}
#endif
