/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BRIGHTNESS_SEQUENCE_H
#define A04_BRIGHTNESS_SEQUENCE_H
/* Internal source helper, not an external command/query interface. */
struct a04_brightness_ops {
 void *context;
 int (*snapshot)(void *,unsigned *); /* Original and 30/70 must be supported. */
 int (*set)(void *,unsigned);
 int (*read)(void *,unsigned *);
 int (*record)(void *,const char *,unsigned,unsigned,int);
 void (*pause)(void *);
};
/* Bits: 1=test failed, 2=restore failed, 4=log failed, 8=preflight failed. */
unsigned a04_brightness_once(const struct a04_brightness_ops *);
#endif
