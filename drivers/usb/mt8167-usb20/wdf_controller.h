#ifndef MT8167_USB20_WDF_CONTROLLER_H
#define MT8167_USB20_WDF_CONTROLLER_H
#include <ntddk.h>
#include <wdf.h>
#include "controller.h"
typedef struct A04_USB_WDF A04_USB_WDF;
typedef void (*A04_USB_EVENT_CALLBACK)(void *,MT8167_USB20_EVENTS);
NTSTATUS A04UsbWdfCreate(WDFDEVICE,A04_USB_EVENT_CALLBACK,void *,A04_USB_WDF **);
NTSTATUS A04UsbWdfPrepare(A04_USB_WDF *,WDFCMRESLIST,WDFCMRESLIST);
void A04UsbWdfRelease(A04_USB_WDF *);
/* Only after exclusive board power/clock ownership and PHY preparation. */
NTSTATUS A04UsbWdfArm(A04_USB_WDF *);
void A04UsbWdfDisarm(A04_USB_WDF *);
const MT8167_USB20_CONTROLLER *A04UsbWdfController(A04_USB_WDF *);
unsigned char A04UsbWdfPhyRead(A04_USB_WDF *,unsigned);
void A04UsbWdfPhyWrite(A04_USB_WDF *,unsigned,unsigned char);
#endif
