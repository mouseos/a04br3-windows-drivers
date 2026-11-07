#ifndef MT8167_USB20_KEYBOARD_H
#define MT8167_USB20_KEYBOARD_H
#define A04_USB_CONFIG_MAX 512u
#ifndef A04_MUSB_BOOT_MOUSE_EXPERIMENT
#define A04_MUSB_BOOT_MOUSE_EXPERIMENT 0
#endif
#define A04_BOOT_KEYBOARD 1u
#define A04_BOOT_MOUSE 2u
/* One directly attached boot interface: keyboard, or opt-in mouse. No hubs
 * or concurrent interrupt-IN interfaces. Fields come from device descriptors. */
typedef enum { KB_DEVICE8, KB_ADDRESS, KB_DEVICE18, KB_CONFIG9, KB_CONFIG,
 KB_SET_CONFIG, KB_PROTOCOL, KB_READY, KB_ERROR } KB_STAGE;
typedef struct {
 KB_STAGE stage; unsigned max_packet0, config_length, configuration, interface_number;
 unsigned endpoint, endpoint_packet, interval, vid, pid, protocol;
} KB_ENUM;
typedef struct { unsigned char setup[8]; unsigned length, address, max_packet0; } KB_CONTROL;
void KbEnumStart(KB_ENUM *);
/* Returns one setup packet for the current stage; the caller serializes it,
 * waits at least 2 ms after SET_ADDRESS, and supplies its completion bytes. */
int KbNextControl(const KB_ENUM *, KB_CONTROL *);
int KbControlComplete(KB_ENUM *, const unsigned char *, unsigned);
/* Windows HID boot keyboard descriptor; reports are eight bytes with no ID. */
const unsigned char *KbHidDescriptor(unsigned *length);
/* Accept the one-byte unnumbered packet or its zero-ID-prefixed form.
 * Validates the entire LED report before constructing a SET_REPORT. */
int KbOutputControl(const KB_ENUM *, const unsigned char *report,
 unsigned length, unsigned report_id, KB_CONTROL *, unsigned char *led);
int KbCopyInputReport(unsigned char *report, unsigned length,
 const unsigned char input[8]);
const unsigned char *KbHidOutputDescriptor(unsigned *length);
#endif
