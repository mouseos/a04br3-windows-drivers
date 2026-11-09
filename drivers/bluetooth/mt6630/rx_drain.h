/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded broker -> pending BTHX READ pump. No bus access/owner in this layer.
 * deliver preserves packets without a matching/large-enough read request.
 * pull returns 1 packet appended, 0 broker empty, -1 actual transport fault.
 */
#ifndef MT6630_RX_DRAIN_H
#define MT6630_RX_DRAIN_H
#define MT6630_RX_DRAIN_BUDGET 32u
enum mt6630_rx_drain_result {MT6630_RX_EMPTY,MT6630_RX_FULL,MT6630_RX_BUDGET,MT6630_RX_FAULT};
struct mt6630_rx_drain_io {
 void *context;
 void (*deliver)(void *context);
 int (*room)(void *context);
 int (*pull)(void *context);
};
static enum mt6630_rx_drain_result mt6630_rx_drain(const struct mt6630_rx_drain_io *io)
{
 unsigned n;int got;
 /* Free slots for already pending standard reads before destructive READ. */
 io->deliver(io->context);
 for(n=0;n<MT6630_RX_DRAIN_BUDGET;n++){
  if(!io->room(io->context))return MT6630_RX_FULL;
  got=io->pull(io->context);
  if(got<0)return MT6630_RX_FAULT;
  if(!got)return MT6630_RX_EMPTY;
  io->deliver(io->context);
 }
 return MT6630_RX_BUDGET;
}
#endif
