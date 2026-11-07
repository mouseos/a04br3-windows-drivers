/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HI556_WINDOWS_OWNER_CLIENT_H
#define A04_HI556_WINDOWS_OWNER_CLIENT_H
#include "../../bus/mt8167-i2c/shared_identity_interface.h"
/* PASSIVE; caller serializes open/identify/close. open retains a kernel
 * interface reference, not a mapping or a user handle. Unknown/ambiguous
 * providers fail closed; identity still requires a real power interface. */
NTSTATUS a04_hi556_owner_open(A04_I2C1_IDENTITY_INTERFACE *);
void a04_hi556_owner_close(A04_I2C1_IDENTITY_INTERFACE *);
NTSTATUS a04_hi556_owner_identify(A04_I2C1_IDENTITY_INTERFACE *,const A04_HI556_POWER_INTERFACE *,A04_HI556_IDENTITY_RESULT *);
NTSTATUS a04_hi556_camera_open(A04_I2C1_CAMERA_INTERFACE *);
void a04_hi556_camera_close(A04_I2C1_CAMERA_INTERFACE *);
#endif
