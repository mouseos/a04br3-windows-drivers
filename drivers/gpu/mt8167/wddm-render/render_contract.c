/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_binding.h"
#include "query_adapter.h"
/* All lifecycle/object/memory callbacks are installed by miniport.c. No
 * display callbacks, optional hardware capability, or success stubs. */
const DRIVER_INITIALIZATION_DATA geo_ge8300_render_contract={
 .Version=DXGKDDI_INTERFACE_VERSION,
 .DxgkDdiQueryAdapterInfo=GeoQueryAdapterInfo,
 .DxgkDdiPreemptCommand=GeoPreemptCommand
};
