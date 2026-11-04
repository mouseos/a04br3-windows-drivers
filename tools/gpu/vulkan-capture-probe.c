/* SPDX-License-Identifier: MIT */
#include <stdarg.h>
#include <stdio.h>
static unsigned failures;
void probe_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr);
}
void probe_pass(void) {}
void probe_fail(void) { ++failures; }
int probe_vulkan(const char *library);
int main(int argc, char **argv)
{
    int result;
    if (argc != 2) return 2;
    result = probe_vulkan(argv[1]);
    return result || failures ? 1 : 0;
}
