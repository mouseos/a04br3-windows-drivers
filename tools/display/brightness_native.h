/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BRIGHTNESS_NATIVE_H
#define A04_BRIGHTNESS_NATIVE_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Fresh-process, fixed one-shot; initializes COM security itself. No arguments.
 * Return flags as brightness_sequence.h, plus 16 if log could not be created.
 * Flag value 4 covers root-file or bounded inherited-stdout failure. No Set after
 * header/tee failure; no arbitrary log path or new COM/WMI retry policy. */
DWORD A04RunBrightnessNativeProbe(void);
#ifdef __cplusplus
}
#endif
#endif
