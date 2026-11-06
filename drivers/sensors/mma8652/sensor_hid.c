/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows 8 sensor HID class: 3D accelerometer, interval and reporting state.
 * Usage definitions: HID sensor usage tables and Microsoft Sensor HID docs.
 */
#include "sensor_hid.h"
static const uint8_t descriptor[] = {
  0x05,0x20, 0x09,0x73, 0xa1,0x00, 0x85,0x01,
  /* Reporting state, array of no events / all events selectors. */
  0x0a,0x16,0x03, 0x15,0x00, 0x25,0x02, 0x75,0x08, 0x95,0x01,
  0xa1,0x02, 0x0a,0x40,0x08, 0x0a,0x41,0x08, 0x0a,0x42,0x08, 0xb1,0x00, 0xc0,
  /* Sensor state: unknown/ready/not available/no data/initializing/error. */
  0x0a,0x01,0x02, 0x15,0x00, 0x25,0x06, 0xa1,0x02,
  0x0a,0x00,0x08, 0x0a,0x01,0x08, 0x0a,0x02,0x08, 0x0a,0x03,0x08,
  0x0a,0x04,0x08, 0x0a,0x05,0x08, 0x0a,0x06,0x08, 0xb1,0x00, 0xc0,
  /* Absolute sensitivity in hundredths of g. */
  0x0a,0x52,0x14, 0x15,0x00, 0x26,0xff,0xff,
  0x75,0x10, 0x95,0x01, 0x55,0x0e, 0xb1,0x02,
  /* Interval in milliseconds. */
  0x0a,0x0e,0x03, 0x15,0x00, 0x27,0xff,0xff,0xff,0x7f,
  0x75,0x20, 0x95,0x01, 0x55,0x00, 0xb1,0x02,
  /* Minimum interval is read-only and fixed by our 50 Hz sampling. */
  0x0a,0x04,0x03, 0xb1,0x02,
  /* Integrated connection is required for Windows motion/orientation UX.
   * Keep this read-only field last to preserve the preceding byte offsets. */
  0x0a,0x09,0x03, 0x15,0x00, 0x25,0x02, 0x75,0x08, 0x95,0x01,
  0xa1,0x02, 0x0a,0x30,0x08, 0x0a,0x31,0x08, 0x0a,0x32,0x08, 0xb1,0x00, 0xc0,
  /* Input state and event (data updated). */
  0x0a,0x01,0x02, 0x15,0x00, 0x25,0x06, 0x75,0x08, 0x95,0x01,
  0xa1,0x02, 0x0a,0x00,0x08, 0x0a,0x01,0x08, 0x0a,0x02,0x08,
  0x0a,0x03,0x08, 0x0a,0x04,0x08, 0x0a,0x05,0x08, 0x0a,0x06,0x08,
  0x81,0x00, 0xc0,
  0x0a,0x02,0x02, 0x15,0x00, 0x25,0x05, 0xa1,0x02,
  0x0a,0x10,0x08, 0x0a,0x11,0x08, 0x0a,0x12,0x08,
  0x0a,0x13,0x08, 0x0a,0x14,0x08, 0x0a,0x15,0x08, 0x81,0x00, 0xc0,
  /* X/Y/Z in 0.01 g. Signed 16-bit, exponent -2. */
  0x0a,0x53,0x04, 0x0a,0x54,0x04, 0x0a,0x55,0x04,
  0x16,0x00,0x80, 0x26,0xff,0x7f, 0x75,0x10, 0x95,0x03, 0x55,0x0e,
  0x81,0x02, 0xc0
};
const uint8_t *a04_mma_hid_descriptor(size_t *length)
{ if (length) *length=sizeof(descriptor); return descriptor; }
static void put16(uint8_t *p, int value)
{ uint16_t v=(uint16_t)value; p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static int to_hundredths(int v)
{ return v>=0 ? (v*100+512)/1024 : -((-v*100+512)/1024); }
void a04_mma_hid_input(const struct a04_mma8652_sample *s,uint8_t out[A04_MMA_HID_INPUT_SIZE])
{
 out[0]=1; out[1]=A04_MMA_HID_STATE_READY; out[2]=A04_MMA_HID_EVENT_DATA_UPDATED;
 /* Stock board direction 7 is in the landscape board frame. Windows uses
  * the native portrait panel frame. Rotate the reported basis 90 degrees
  * clockwise: (board Y,-board X,board Z) = (-raw X,raw Y,-raw Z).
  * The 0.0.1.7 opposite rotation was reported upside down. Keep this
  * transform at the HID boundary; mma8652_read retains Linux board axes. */
 put16(out+3,to_hundredths(s->y)); put16(out+5,to_hundredths(-(int)s->x)); put16(out+7,to_hundredths(s->z));
}
void a04_mma_hid_get_feature(const struct a04_mma_hid_features *f,uint8_t out[A04_MMA_HID_FEATURE_SIZE])
{
 /* Windows named-array selectors are one-based (Microsoft HID Sensors
  * Usages, HID Selectors); zero-based values report UNKNOWN, not READY. */
 uint32_t v=f->interval_ms; out[0]=1; out[1]=(uint8_t)(f->reporting+1u); out[2]=A04_MMA_HID_STATE_READY;
 out[3]=(uint8_t)f->sensitivity; out[4]=(uint8_t)(f->sensitivity>>8);
 out[5]=(uint8_t)v; out[6]=(uint8_t)(v>>8); out[7]=(uint8_t)(v>>16); out[8]=(uint8_t)(v>>24);
 out[9]=20; out[10]=0; out[11]=0; out[12]=0;
 out[13]=1; /* PC integrated. */
}
int a04_mma_hid_set_feature(struct a04_mma_hid_features *f,const uint8_t *p,size_t n)
{
 uint32_t v;
 if (!f || !p || n!=A04_MMA_HID_FEATURE_SIZE || p[0]!=1 || p[1]<1 || p[1]>3) return -1;
 v=(uint32_t)p[5]|((uint32_t)p[6]<<8)|((uint32_t)p[7]<<16)|((uint32_t)p[8]<<24);
 if (v<20 || v>60000) return -1;
 /* Read-only state, minimum interval and connection fields are ignored on
  * SET_FEATURE; the host only controls reporting, sensitivity and interval. */
 f->reporting=(uint8_t)(p[1]-1u); f->sensitivity=(uint16_t)(p[3]|((uint16_t)p[4]<<8)); f->interval_ms=v; return 0;
}

static unsigned delta(int a,int b) { int d=a-b; return (unsigned)(d<0 ? -d : d); }
int a04_mma_hid_changed(const struct a04_mma_hid_features *f,
                        const struct a04_mma8652_sample *s,
                        const struct a04_mma8652_sample *last,int have_last)
{
 unsigned threshold;
 if (!f || !s || !last || !f->reporting) return 0;
 threshold=((unsigned)f->sensitivity*1024u+99u)/100u;
 if (!have_last || f->reporting==1 || !threshold) return 1;
 return delta(s->x,last->x)>=threshold || delta(s->y,last->y)>=threshold || delta(s->z,last->z)>=threshold;
}
