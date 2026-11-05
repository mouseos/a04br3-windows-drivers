#ifndef MT8167_USB20_CONTROLLER_H
#define MT8167_USB20_CONTROLLER_H
#include "ep0_pio.h"
typedef struct { MT8167_USB20_IO io; unsigned (*read32)(void *,unsigned); void (*write32)(void *,unsigned,unsigned); } MT8167_USB20_CONTROLLER;
typedef struct { unsigned usb, tx, rx; } MT8167_USB20_EVENTS;
/* Requires exclusive controller ownership, clocks on and at least 128 bytes of
 * dynamic FIFO RAM; initializes only EP0 and EP1 RX. Caller serializes INDEX. */
int Mt8167UsbFifoInit(const MT8167_USB20_CONTROLLER *,unsigned fifo_bytes);
int Mt8167UsbInterruptEnable(const MT8167_USB20_CONTROLLER *);
void Mt8167UsbInterruptDisable(const MT8167_USB20_CONTROLLER *);
/* Read and acknowledge only enabled interrupt sources; call with IRQ lock. */
int Mt8167UsbInterruptCapture(const MT8167_USB20_CONTROLLER *,MT8167_USB20_EVENTS *);
#endif
