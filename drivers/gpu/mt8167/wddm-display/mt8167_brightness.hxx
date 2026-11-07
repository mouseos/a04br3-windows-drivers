/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_BRIGHTNESS_HXX
#define A04_MT8167_BRIGHTNESS_HXX
extern "C" {
#include <ntddk.h>
#include <windef.h>
#include <wingdi.h>
#include <winddi.h>
#include <ntddvdeo.h>
#include <d3dkmddi.h>
#include <d3dkmthk.h>
#include <dispmprt.h>
}
#ifndef A04_MT8167_DISPLAY_BRIGHTNESS
#define A04_MT8167_DISPLAY_BRIGHTNESS 0
#endif
/* Only DISP_PWM0's assigned PnP window. No TOPCK/GPIO/MMSYS/PMIC lease.
 * Owner methods are serialized by the DOD present/power mutex. Published
 * interfaces retain an independent nonpaged context with its own mutex. */
class A04Mt8167Brightness {
    void *m_Context;
public:
    void Initialize() { m_Context=NULL; }
    NTSTATUS Start(PCM_RESOURCE_LIST resources);
    void Stop();
    void SetPower(BOOLEAN active);
    NTSTATUS QueryInterface(PQUERY_INTERFACE query);
};
#endif
