/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "check-chkdsk.h"
#define BASE "File verification completed.\r\nError detected in index $I30 for file 14634.\r\nIndex verification completed.\r\nErrors found.\r\n"
int main(void)
{
    const char good[] = BASE;
    const char duplicate[] = BASE "Error detected in index $I30 for file 42.\r\n";
    const char corrupted[] = BASE "The volume bitmap is incorrect.\r\n";
    const char incomplete[] = "Error detected in index $I30 for file 14634.\r\nErrors found.\r\n";
    char nul[sizeof(good)];
    assert(a04_chkdsk_expected(good, sizeof(good)-1));
    assert(!a04_chkdsk_expected(duplicate, sizeof(duplicate)-1));
    assert(!a04_chkdsk_expected(corrupted, sizeof(corrupted)-1));
    assert(!a04_chkdsk_expected(incomplete, sizeof(incomplete)-1));
    memcpy(nul, good, sizeof(good)); nul[5]=0;
    assert(!a04_chkdsk_expected(nul, sizeof(nul)-1));
    assert(!a04_chkdsk_expected(NULL, 10));
    puts("CHKDSK output gate tests passed");
    return 0;
}
