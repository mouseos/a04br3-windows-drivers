/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BRIGHTNESS_LOG_H
#define A04_BRIGHTNESS_LOG_H
#define A04_BRIGHTNESS_STDOUT_LIMIT 16384u
#define A04_BRIGHTNESS_LOG_FRAGMENT_LIMIT 1024u
/* Single-threaded fixed client. Each callback attempts one exact write;
 * the root callback additionally flushes its owned file. Stdout is borrowed.
 * Content is bounded; synchronous OS WriteFile is not a hard-timeout API. */
struct a04_brightness_log {
 void *context;
 int (*root)(void *,const char *,unsigned);
 int (*stdout_write)(void *,const char *,unsigned);
 unsigned stdout_bytes;
 unsigned failed; /* 1=root failure, 2=stdout failure/budget exhausted */
};
int a04_brightness_log_write(struct a04_brightness_log *,const char *,unsigned);
#endif
