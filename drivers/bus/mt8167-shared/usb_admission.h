/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USB_ADMISSION_H
#define A04_USB_ADMISSION_H
/* Acquire v1 semantic extension: positive, side-effect-free admission waits.
 * All negative results remain failures. Unknown positive values fail closed. */
#define A04_USB_WAIT_DEVICE_ROLE 1
#define A04_USB_WAIT_EXTERNAL_VBUS 2
static __inline int A04UsbAdmissionWait(int result)
{ return result==A04_USB_WAIT_DEVICE_ROLE || result==A04_USB_WAIT_EXTERNAL_VBUS; }
static __inline int A04UsbAdmission(unsigned input)
{
 if(input&(1u<<9))return A04_USB_WAIT_DEVICE_ROLE;
 if(!(input&(1u<<2)))return A04_USB_WAIT_EXTERNAL_VBUS;
 return 0;
}
#endif
