/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Identity check for the fixed board native22 fragment asset. No USC bytes
 * are distributed here, and a matching digest is not an execution approval. */
#include "native_copy_usc.h"
static uint32_t ror(uint32_t x,unsigned n){return (x>>n)|(x<<(32u-n));}
static const uint32_t k[64]={
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
/* Exactly 152 input bytes: two full blocks followed by 24 bytes, 0x80,
 * zero padding and the big-endian 1216-bit length. */
void a04_vendor_copy_usc_hash(const uint8_t input[152],uint8_t digest[32])
{
    uint32_t h[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    uint8_t block[64];uint32_t w[64];unsigned part,i,j;
    for(part=0;part<3;part++) {
        uint32_t a,b,c,d,e,f,g,z,t1,t2;
        for(i=0;i<64;i++) {j=part*64u+i;block[i]=j<152u?input[j]:(j==152u?0x80u:(j==190u?4u:(j==191u?0xc0u:0u)));}
        for(i=0;i<16;i++)w[i]=((uint32_t)block[4*i]<<24)|((uint32_t)block[4*i+1]<<16)|((uint32_t)block[4*i+2]<<8)|block[4*i+3];
        for(i=16;i<64;i++)w[i]=w[i-16]+(ror(w[i-15],7)^ror(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(ror(w[i-2],17)^ror(w[i-2],19)^(w[i-2]>>10));
        a=h[0];b=h[1];c=h[2];d=h[3];e=h[4];f=h[5];g=h[6];z=h[7];
        for(i=0;i<64;i++) {t1=z+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^(~e&g))+k[i]+w[i];t2=(ror(a,2)^ror(a,13)^ror(a,22))+((a&b)^(a&c)^(b&c));z=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
    }
    for(i=0;i<8;i++)for(j=0;j<4;j++)digest[4*i+j]=(uint8_t)(h[i]>>(24u-8u*j));
}
int a04_vendor_copy_usc_match(const uint8_t *data,size_t bytes)
{
    static const uint8_t expected[32]={0x5c,0x68,0x91,0x7f,0xaa,0x05,0x12,0x38,0xb2,0x36,0x74,0xb6,0x4c,0x6b,0x13,0x7e,0xf7,0xff,0x75,0x42,0x5d,0xbd,0xe9,0xdb,0xc5,0xe1,0xe5,0xa3,0x6e,0x02,0xc4,0xa3};
    uint8_t digest[32];unsigned i;uint32_t diff=0;
    if(!data || bytes!=152u)return 0;
    a04_vendor_copy_usc_hash(data,digest);for(i=0;i<32;i++)diff|=digest[i]^expected[i];return diff==0;
}
