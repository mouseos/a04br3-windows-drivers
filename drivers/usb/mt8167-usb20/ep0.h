#ifndef MT8167_USB20_EP0_H
#define MT8167_USB20_EP0_H

/* Hardware-independent control-transfer sequencing for the MT8167 MUSB EP0.
 * The caller owns serialization, FIFO access, deadlines and error recovery. */
typedef enum { EP0_IDLE, EP0_SETUP, EP0_IN, EP0_OUT, EP0_STATUS, EP0_DONE, EP0_FAILED } EP0_STAGE;
typedef enum { EP0_WAIT, EP0_SEND_SETUP, EP0_REQUEST_IN, EP0_SEND_OUT,
               EP0_STATUS_IN, EP0_STATUS_OUT, EP0_COMPLETE, EP0_ABORT } EP0_ACTION;
typedef struct {
    EP0_STAGE stage;
    unsigned total, actual, max_packet, pending;
    int direction_in;
} EP0_TRANSFER;

/* setup is the eight USB SETUP bytes; length must match wLength. */
EP0_ACTION Ep0Begin(EP0_TRANSFER *t, const unsigned char setup[8], unsigned length, unsigned max_packet);
/* Called only for a completed controller transaction. For IN, count is the
 * FIFO byte count; for OUT, the caller must have sent t->pending bytes. */
EP0_ACTION Ep0Advance(EP0_TRANSFER *t, unsigned count);
EP0_ACTION Ep0Fail(EP0_TRANSFER *t);
#endif
