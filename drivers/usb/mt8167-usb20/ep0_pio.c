#include "ep0_pio.h"
#define INDEX 0x0e
#define CSR0 0x12
#define COUNT0 0x18
#define FIFO0 0x20
#define RX_READY 0x0001
#define TX_READY 0x0002
#define STALL 0x0004
#define SETUP 0x0008
#define ERROR_BIT 0x0010
#define REQUEST 0x0020
#define STATUS 0x0040
#define NAK_TIMEOUT 0x0080
#define DIS_PING 0x0800

static EP0_ACTION issue(MT8167_USB20_EP0 *e, EP0_ACTION action)
{
    unsigned i;
    unsigned short csr;
    switch (action) {
    case EP0_SEND_SETUP:
        for (i=0; i<8; ++i) e->io.write8(e->io.context,FIFO0,e->setup[i]);
        csr = DIS_PING | SETUP | TX_READY; break;
    case EP0_REQUEST_IN: csr = REQUEST; break;
    case EP0_SEND_OUT:
        for (i=0; i<e->transfer.pending; ++i)
            e->io.write8(e->io.context,FIFO0,e->buffer[e->transfer.actual+i]);
        csr = TX_READY; break;
    case EP0_STATUS_IN: csr = STATUS | REQUEST; break;
    case EP0_STATUS_OUT: csr = STATUS | TX_READY; break;
    default: return action;
    }
    e->io.write16(e->io.context,CSR0,csr);
    return action;
}
/* NAK is flow control, not a completed/failed data transaction. Re-arm the
 * resident token without refilling FIFO, reading IN data, advancing EP0 or
 * extending the caller's absolute URB deadline. No setup retry: USB SETUP
 * must be accepted; an unexpected setup NAK stays a protocol failure.
 * CT3 musb_h_ep0_irq distinguishes H_NAKTIMEOUT from RXSTALL/H_ERROR;
 * mtk_musb_reg.h defines these same CSR0 commands/WZC bits. */
static EP0_ACTION nak_wait(MT8167_USB20_EP0 *e, unsigned short csr)
{
    unsigned short token;
    if ((csr & RX_READY) || e->transfer.stage==EP0_SETUP) {
        e->fault=MT8167_EP0_FAULT_PROTOCOL;return Ep0Fail(&e->transfer);
    }
    switch(e->transfer.stage) {
    case EP0_IN: token=REQUEST;break;
    case EP0_OUT: token=TX_READY;break;
    case EP0_STATUS:
        token=STATUS | (e->transfer.total && e->transfer.direction_in ? TX_READY : REQUEST);break;
    default:
        e->fault=MT8167_EP0_FAULT_PROTOCOL;return Ep0Fail(&e->transfer);
    }
    /* Clear timeout and an outstanding request before issuing the same
     * transaction. Crucially do not call issue() for an OUT data packet. */
    e->io.write16(e->io.context,CSR0,0);
    e->io.write16(e->io.context,CSR0,token);
    return EP0_WAIT;
}
EP0_ACTION Mt8167Ep0Start(MT8167_USB20_EP0 *e, const MT8167_USB20_IO *io,
                         const unsigned char *setup, unsigned char *buffer,
                         unsigned length, unsigned max_packet)
{
    unsigned i;
    EP0_ACTION action;
    if (!e || !io || !setup || !io->read8 || !io->read16 || !io->write8 ||
        !io->write16 || (length && !buffer)) return EP0_ABORT;
    e->io=*io; e->buffer=buffer;e->fault=MT8167_EP0_FAULT_NONE;
    for (i=0; i<8; ++i) e->setup[i]=setup[i];
    action=Ep0Begin(&e->transfer,setup,length,max_packet);
    if (action==EP0_ABORT) return action;
    io->write8(io->context,INDEX,0);
    return issue(e,action);
}
EP0_ACTION Mt8167Ep0Interrupt(MT8167_USB20_EP0 *e)
{
    unsigned short csr;
    unsigned count=0, i;
    EP0_ACTION action;
    if (!e || !e->io.read8 || !e->io.read16 || !e->io.write8 || !e->io.write16)
        return EP0_ABORT;
    e->io.write8(e->io.context,INDEX,0);
    csr=e->io.read16(e->io.context,CSR0);
    if (csr & STALL) {e->fault=MT8167_EP0_FAULT_STALL;return Ep0Fail(&e->transfer);}
    if (csr & ERROR_BIT) {e->fault=MT8167_EP0_FAULT_PROTOCOL;return Ep0Fail(&e->transfer);}
    if (csr & NAK_TIMEOUT) return nak_wait(e,csr);
    if (e->transfer.stage==EP0_IN) {
        if (!(csr & RX_READY)) return EP0_WAIT;
        count=e->io.read8(e->io.context,COUNT0);
        if (count>e->transfer.max_packet || count>e->transfer.total-e->transfer.actual)
            {e->fault=MT8167_EP0_FAULT_PROTOCOL;return Ep0Fail(&e->transfer);}
        for (i=0; i<count; ++i)
            e->buffer[e->transfer.actual+i]=e->io.read8(e->io.context,FIFO0);
    } else if (e->transfer.stage==EP0_OUT) {
        if (csr & TX_READY) return EP0_WAIT;
        count=e->transfer.pending;
    } else if (e->transfer.stage==EP0_SETUP || e->transfer.stage==EP0_STATUS) {
        if (csr & (TX_READY|REQUEST)) return EP0_WAIT;
    } else {e->fault=MT8167_EP0_FAULT_PROTOCOL;return Ep0Fail(&e->transfer);}
    action=Ep0Advance(&e->transfer,count);
    if(action==EP0_ABORT)e->fault=MT8167_EP0_FAULT_PROTOCOL;
    return issue(e,action);
}
