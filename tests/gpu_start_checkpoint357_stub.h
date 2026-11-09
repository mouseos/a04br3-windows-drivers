/* SPDX-License-Identifier: MIT; host-only optional checkpoint mock. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "ntddk.h"
typedef void *PDEVICE_OBJECT;
#define A04_START_CHECKPOINT357_H
#define GEO357_WORDS 192u
struct geo_start357_record {
 ULONG schema,bytes,kind,build,first_failure_id,status,valid_flags,last_step;
 ULONGLONG begin_filetime,begin_interrupt,end_filetime,end_interrupt;
 ULONG words[GEO357_WORDS];
};

extern struct geo_start357_record recorded357;
extern void Test357Stored(void);
static void Geo357Begin(struct geo_start357_record*r,ULONG k){memset(r,0,sizeof(*r));r->kind=k;r->schema=1;r->bytes=sizeof(*r);r->build=357;}
static NTSTATUS Geo357Fail(struct geo_start357_record*r,ULONG id,NTSTATUS s){if(!r->first_failure_id)r->first_failure_id=id;r->last_step=id;return s;}
static void Geo357End(struct geo_start357_record*r,NTSTATUS s){r->status=s;}
static void Geo357Store(const struct geo_start357_record*r,int d,int p){(void)d;(void)p;recorded357=*r;Test357Stored();}
static size_t RtlCompareMemory(const void *a,const void *b,size_t n){size_t i=0;while(i<n && ((const unsigned char*)a)[i]==((const unsigned char*)b)[i])i++;return i;}
