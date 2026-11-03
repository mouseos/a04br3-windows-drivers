/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "reset_command.h"
int mt8167_reset_consume(struct mt8167_reset_parser *parser, uint8_t byte)
{
    static const char command[] = "A04BR3!RST";
    if (!parser || parser->matched >= sizeof(command) - 1) return 0;
    if (byte == (uint8_t)command[parser->matched]) {
        if (++parser->matched == sizeof(command) - 1) {
            parser->matched = 0;
            return 1;
        }
    } else {
        parser->matched = byte == (uint8_t)command[0] ? 1u : 0u;
    }
    return 0;
}
