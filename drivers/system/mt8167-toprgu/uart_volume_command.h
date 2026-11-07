/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_UART_VOLUME_COMMAND_H
#define MT8167_UART_VOLUME_COMMAND_H
#include <stdint.h>
/* One request per device lifetime; no string or argument is passed to Windows. */
struct mt8167_volume_parser { unsigned matched, fired; };
static int mt8167_volume_consume(struct mt8167_volume_parser *p,uint8_t byte)
{
    static const char command[]="A04BR3!VOL";
    if(!p || p->fired || p->matched>=sizeof(command)-1) return 0;
    if(byte==(uint8_t)command[p->matched]) {
        if(++p->matched==sizeof(command)-1) { p->matched=0;p->fired=1;return 1; }
    } else p->matched=byte==(uint8_t)command[0] ? 1u : 0u;
    return 0;
}
#endif
