#ifndef MT8167_USB20_GPIO22_H
#define MT8167_USB20_GPIO22_H
/* GPIO22 belongs to a shared pin controller. A board power provider must
 * hold exclusive pin ownership before these callbacks are used. */
typedef struct { void *context; unsigned (*read32)(void *,unsigned); void (*write32)(void *,unsigned,unsigned); } A04_GPIO_IO;
int A04Gpio22Prepare(const A04_GPIO_IO *);
int A04Gpio22Vbus(const A04_GPIO_IO *,int on);
#endif
