/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_USB20_BOOT_HID_H
#define MT8167_USB20_BOOT_HID_H
#include "keyboard.h"
#define A04_BOOT_HID_REPORT_BYTES 9u
#if A04_MUSB_BOOT_MOUSE_EXPERIMENT
#define A04_HID_INPUT_BYTES A04_BOOT_HID_REPORT_BYTES
#else
#define A04_HID_INPUT_BYTES 8u
#endif
/* Controller capability descriptor: two Windows TLCs, not the peripheral's
 * report descriptor or identity. ID1=keyboard, ID2=three-button relative mouse.
 * Both input reports are padded to nine bytes; physical USB reports have no ID. */
const unsigned char *A04BootHidDescriptor(unsigned output,unsigned *length);
int A04BootHidEncode(unsigned protocol,const unsigned char boot[8],unsigned char windows[9]);
/* Current button state only. Never replay an old relative displacement. */
int A04BootHidMouseSnapshot(unsigned buttons,unsigned char windows[9]);
int A04BootHidKeyboardOutput(const KB_ENUM *,const unsigned char *,unsigned length,
 unsigned report_id,KB_CONTROL *,unsigned char *led);
#endif
