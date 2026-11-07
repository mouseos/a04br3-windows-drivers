#ifndef MT8167_USB20_KEYBOARD_RX_H
#define MT8167_USB20_KEYBOARD_RX_H
#include "ep0_pio.h"
/* Caller exclusively owns host endpoint 1 and has allocated an RX FIFO of at
 * least max_packet bytes. speed is MUSB RXTYPE speed bits 0x40/0x80/0xc0. */
int KbRxConfigure(const MT8167_USB20_IO *, unsigned address, unsigned endpoint, unsigned max_packet, unsigned interval, unsigned speed);
/* Generalized boot input: keyboard=8 bytes, mouse=3..8 (HID1.11 B).
 * Mouse extensions are consumed but only the mandatory three bytes escape. */
int KbRxConfigureBoot(const MT8167_USB20_IO *,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned protocol);
int KbRxCompleteBoot(const MT8167_USB20_IO *,unsigned char report[8],unsigned protocol,unsigned max_packet);
int KbRxRequest(const MT8167_USB20_IO *);
/* 1=8-byte boot report, 2=NAK, 0=still pending, -1=error; reset endpoint on error. */
int KbRxComplete(const MT8167_USB20_IO *, unsigned char report[8]);
#endif
