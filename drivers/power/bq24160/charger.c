/* SPDX-License-Identifier: MIT
 * Policy derived from UniXNU-a04br3 A04BR3ChargerCore.h (MIT), commit
 * 0420ce37f0241178a7276784bbc013b0dbba4c95. No platform I/O here.
 */
#include "charger.h"
#define SECOND_NS 1000000000ULL
#define MAX_GAP_NS (20ULL * SECOND_NS)

static enum a04_bq_result policy(const struct a04_bq_snapshot *s)
{
    unsigned status;
    if (s->valid != 0xffu || (s->raw[4] & 0xf8u) != 0x40u) return A04_BQ_POLICY;
    if ((s->raw[0] & 7u) || (s->raw[7] & 6u)) return A04_BQ_FAULT;
    if ((s->raw[0] & 8u) || (s->raw[2] & 0x7fu) != 0x0cu ||
        (s->raw[3] & 0xfeu) != 0xa4u || s->raw[5] != 0x51u ||
        (s->raw[7] & 0xe9u) != 0x28u || (s->raw[6] & 0x3fu) ||
        (s->raw[1] & 15u)) return A04_BQ_POLICY;
    status = (s->raw[0] >> 4) & 7u;
    if ((status == 3u && !(s->raw[1] & 0xc0u)) ||
        (status == 4u && !(s->raw[1] & 0x30u)) ||
        (status == 5u && (!(s->raw[1] & 0xc0u) || !(s->raw[1] & 0x30u))))
        return A04_BQ_OK;
    return A04_BQ_POLICY;
}
static int late(const struct a04_bq_owner *o, uint64_t now, uint64_t deadline)
{
    return !deadline || now >= deadline ||
        (o->kicks && (now < o->last_kick || now - o->last_kick >= MAX_GAP_NS));
}
enum a04_bq_result a04_bq_step(struct a04_bq_owner *o, const struct a04_bq_bus *b, uint64_t deadline)
{
    struct a04_bq_snapshot s = {{0}, 0, 0};
    enum a04_bq_result result;
    uint64_t end;
    unsigned reg, first;
    if (!o || !b || !b->now_ns || !b->read || !b->write_watchdog) return A04_BQ_INVALID;
    if (o->stopped) return A04_BQ_STOPPED;
    s.at = b->now_ns(b->context);
    if (late(o, s.at, deadline)) { o->stopped = 1; return A04_BQ_DEADLINE; }
    first = o->attempts++ == 0;
    for (reg = 0; reg < 8; ++reg) {
        if (b->read(b->context, (uint8_t)reg, &s.raw[reg])) {
            o->latest = s; if (first) o->first = s;
            o->stopped = 1; return A04_BQ_READ_ERROR;
        }
        s.valid |= 1u << reg;
        if (!reg) o->fault_history |= 1u << (s.raw[0] & 7u);
    }
    o->latest = s; if (first) o->first = s;
    result = policy(&s);
    end = b->now_ns(b->context);
    if (end < s.at || end - s.at >= SECOND_NS || late(o, end, deadline)) result = A04_BQ_DEADLINE;
    if (result != A04_BQ_OK) { o->stopped = 1; return result; }
    /* Never echo status/fault bits, or write any charge configuration. */
    if (b->write_watchdog(b->context, (uint8_t)(0x80u | (s.raw[0] & 8u)))) {
        o->stopped = 1; return A04_BQ_WRITE_ERROR;
    }
    end = b->now_ns(b->context);
    if (end < s.at || end - s.at >= SECOND_NS || late(o, end, deadline)) {
        o->stopped = 1; return A04_BQ_DEADLINE;
    }
    o->last_kick = end; ++o->kicks;
    return A04_BQ_OK;
}
