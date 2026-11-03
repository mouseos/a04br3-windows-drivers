/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "check-chkdsk.h"
static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + ('a'-'A')) : c; }
static unsigned matches(const char *data, size_t size, const char *needle)
{
    size_t i, j; unsigned count=0;
    for (i=0; i<size; ++i) {
        for (j=0; needle[j] && i+j<size && lower(data[i+j])==lower(needle[j]); ++j) {}
        if (!needle[j]) ++count;
    }
    return count;
}
int a04_chkdsk_expected(const char *data, size_t size)
{
    size_t i;
    if (!data || !size || size>1048576) return 0;
    for (i=0; i<size; ++i) if (!data[i]) return 0;
    return matches(data,size,"Error detected in index $I30 for file 14634.")==1 &&
           matches(data,size,"Error detected")==1 &&
           matches(data,size,"File verification completed.")==1 &&
           matches(data,size,"Index verification completed.")==1 &&
           matches(data,size,"Errors found.")==1 &&
           matches(data,size,"incorrect")==0 && matches(data,size,"corrupt")==0 &&
           matches(data,size,"unreadable")==0 && matches(data,size,"insufficient")==0;
}
