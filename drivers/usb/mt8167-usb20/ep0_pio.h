#ifndef MT8167_USB20_EP0_PIO_H
#define MT8167_USB20_EP0_PIO_H
#include "ep0.h"
/* Caller must hold the controller lock and select the appropriate device,
 * speed and hub route before starting. Access callbacks use MUSB byte offsets. */
typedef struct {
    void *context;
    unsigned char (*read8)(void *, unsigned);
    unsigned short (*read16)(void *, unsigned);
    void (*write8)(void *, unsigned, unsigned char);
    void (*write16)(void *, unsigned, unsigned short);
} MT8167_USB20_IO;
typedef struct {
    EP0_TRANSFER transfer;
    MT8167_USB20_IO io;
    unsigned char *buffer;
    unsigned char setup[8];
} MT8167_USB20_EP0;
/* Requires an already initialized, host-mode controller; no power changes. */
EP0_ACTION Mt8167Ep0Start(MT8167_USB20_EP0 *, const MT8167_USB20_IO *, const unsigned char *, unsigned char *, unsigned, unsigned);
/* Invoke only after a confirmed EP0 interrupt. Caller clears interrupt source,
 * owns timeout/cancellation and must reset/flush EP0 after EP0_ABORT. */
EP0_ACTION Mt8167Ep0Interrupt(MT8167_USB20_EP0 *);
#endif
