/* SPDX-License-Identifier: MIT */
#ifndef GEO_UI_TEXTURE_ORACLE_H
#define GEO_UI_TEXTURE_ORACLE_H
/* Non-symmetric channels detect BGRA/RGBA swaps; alpha remains opaque. */
static unsigned ui_color(unsigned x,unsigned y) {
 unsigned r=(17+37*x+11*y)&255,g=(61+13*x+29*y)&255,b=(109+19*x+7*y)&255;
 return 0xff000000u|(r<<16)|(g<<8)|b;
}
static void ui_store(unsigned char *p,unsigned color,unsigned rgba) {
 p[0]=(unsigned char)(color>>(rgba?16:0));p[1]=(unsigned char)(color>>8);
 p[2]=(unsigned char)(color>>(rgba?0:16));p[3]=255;
}
/* Pixel centers; triangle at (80,60),(720,60),(400,540).
 * 0 excludes edge band; +1 tests texture color; -1 tests exterior black. */
static int ui_region(unsigned x,unsigned y) {
 int px=(int)(2*x+1),py=(int)(2*y+1);
 int l=3*(px-160)-2*(py-120),r=3*(1440-px)-2*(py-120),t=py-120;
 if(l>16&&r>16&&t>4)return 1;
 if(l< -16||r< -16||t< -4)return -1;
 return 0;
}
static int ui_matches(unsigned value,unsigned want) {
 unsigned c;for(c=0;c<4;c++){int d=(int)((value>>(8*c))&255)-(int)((want>>(8*c))&255);if(d< -2||d>2)return 0;}return 1;
}
#endif
