/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "reset_command.h"
int main(void)
{
    struct mt8167_reset_parser p = {0};
    const char *text = "noise A04BR3!RSX A04BR3!RST";
    unsigned hits = 0; size_t i;
    for (i=0; i<strlen(text); ++i) hits += (unsigned)mt8167_reset_consume(&p,(unsigned char)text[i]);
    assert(hits == 1 && p.matched == 0);
    text = "AA04BR3!RSTA04BR3!RST"; hits = 0;
    for (i=0; i<strlen(text); ++i) hits += (unsigned)mt8167_reset_consume(&p,(unsigned char)text[i]);
    assert(hits == 2);
    return 0;
}
