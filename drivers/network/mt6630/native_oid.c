/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Scan is completed only after the command has been submitted; a separate
 * NDIS_STATUS_DOT11_SCAN_CONFIRM follows the matching firmware event.
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/network/oid-dot11-scan-request
 */
#include <ntddk.h>
#include "native_oid.h"
NDIS_STATUS a04_native_oid_dispatch(struct a04_native_oid_context *ctx,PNDIS_OID_REQUEST request)
{
 NDIS_STATUS status;ULONG needed=0;
 if(!ctx||!ctx->bridge||!request)return NDIS_STATUS_INVALID_DATA;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return NDIS_STATUS_NOT_SUPPORTED;
 if(request->RequestType!=NdisRequestSetInformation || request->DATA.SET_INFORMATION.Oid!=OID_DOT11_SCAN_REQUEST)
  return NDIS_STATUS_NOT_SUPPORTED;
 request->DATA.SET_INFORMATION.BytesRead=0;request->DATA.SET_INFORMATION.BytesNeeded=0;
 status=a04_native_scan_start(ctx->bridge,request->DATA.SET_INFORMATION.InformationBuffer,
  request->DATA.SET_INFORMATION.InformationBufferLength,&ctx->policy,&needed);
 if(status==NDIS_STATUS_SUCCESS)request->DATA.SET_INFORMATION.BytesRead=request->DATA.SET_INFORMATION.InformationBufferLength;
 else if(status==NDIS_STATUS_INVALID_LENGTH)request->DATA.SET_INFORMATION.BytesNeeded=needed;
 return status;
}
