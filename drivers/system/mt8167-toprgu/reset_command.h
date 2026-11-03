/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_RESET_COMMAND_H
#define MT8167_RESET_COMMAND_H
#include <stdint.h>
struct mt8167_reset_parser { unsigned matched; };
int mt8167_reset_consume(struct mt8167_reset_parser *parser, uint8_t byte);
#endif
