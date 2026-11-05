#include "ep0.h"

static EP0_ACTION status_phase(EP0_TRANSFER *t)
{
    t->stage = EP0_STATUS;
    t->pending = 0;
    return t->total && t->direction_in ? EP0_STATUS_OUT : EP0_STATUS_IN;
}

static EP0_ACTION next_out(EP0_TRANSFER *t)
{
    unsigned left = t->total - t->actual;
    t->pending = left < t->max_packet ? left : t->max_packet;
    return EP0_SEND_OUT;
}

EP0_ACTION Ep0Begin(EP0_TRANSFER *t, const unsigned char setup[8], unsigned length, unsigned max_packet)
{
    unsigned requested;
    if (!t || !setup) return EP0_ABORT;
    requested = (unsigned)setup[6] | ((unsigned)setup[7] << 8);
    t->stage = EP0_FAILED;
    t->actual = t->pending = 0;
    if (requested != length || (max_packet != 8 && max_packet != 16 &&
        max_packet != 32 && max_packet != 64)) return EP0_ABORT;
    t->total = length;
    t->max_packet = max_packet;
    t->direction_in = (setup[0] & 0x80) != 0;
    t->stage = EP0_SETUP;
    return EP0_SEND_SETUP;
}

EP0_ACTION Ep0Fail(EP0_TRANSFER *t)
{
    if (t) t->stage = EP0_FAILED;
    return EP0_ABORT;
}

EP0_ACTION Ep0Advance(EP0_TRANSFER *t, unsigned count)
{
    unsigned remaining;
    if (!t) return EP0_ABORT;
    switch (t->stage) {
    case EP0_SETUP:
        if (count) return Ep0Fail(t);
        if (!t->total) return status_phase(t);
        t->stage = t->direction_in ? EP0_IN : EP0_OUT;
        return t->direction_in ? EP0_REQUEST_IN : next_out(t);
    case EP0_IN:
        remaining = t->total - t->actual;
        if (count > t->max_packet || count > remaining) return Ep0Fail(t);
        t->actual += count;
        if (count < t->max_packet || t->actual == t->total) return status_phase(t);
        return EP0_REQUEST_IN;
    case EP0_OUT:
        if (count != t->pending) return Ep0Fail(t);
        t->actual += count;
        if (t->actual == t->total) return status_phase(t);
        return next_out(t);
    case EP0_STATUS:
        if (count) return Ep0Fail(t);
        t->stage = EP0_DONE;
        return EP0_COMPLETE;
    default:
        return Ep0Fail(t);
    }
}
