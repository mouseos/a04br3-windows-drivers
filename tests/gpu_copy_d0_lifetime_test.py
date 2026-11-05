#!/usr/bin/env python3
"""Exercise the actual provider D0Exit with synthetic owned DMA state."""
from pathlib import Path
import os, subprocess, tempfile
s=(Path(__file__).resolve().parents[1]/'drivers/gpu/mt8167/ge8300_provider.c').read_text()
start=s.index('NTSTATUS GpuD0Exit('); brace=s.index('{',start); depth=1; end=brace+1
while depth:
    depth+=(s[end]=='{')-(s[end]=='}'); end+=1
fn=s[start:end]
preamble=r'''
#include <assert.h>
#define A04_GE8300_COPY_SERVICE 1
#define A04_GE8300_RENDER_DIAGNOSTIC 0
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_BUSY (-1)
#define NT_SUCCESS(x) ((x)>=0)
#define UNREFERENCED_PARAMETER(x) (void)(x)
typedef int NTSTATUS; typedef unsigned ULONG; typedef void *WDFDEVICE;
typedef int WDF_POWER_DEVICE_STATE;
struct firmware { int processor_released; void (*trace)(void); };
struct memory { void *cpu, *adapter; };
struct linear { struct memory memory; };
typedef struct { int copy_online, referenced; void *dma; struct firmware firmware; struct linear linear; } GPU_CONTEXT;
static int stops, frees, refs;
static GPU_CONTEXT *GpuContext(WDFDEVICE d) { return (GPU_CONTEXT *)d; }
static void GpuPrint(const char *s) { (void)s; }
static void GpuHex(ULONG n) { (void)n; }
static void GpuTraceStage(void) {}
static int a04_gpu_firmware_shutdown(struct firmware *s) { ++stops; s->processor_released=0; return 0; }
static int a04_windows_linear_context_free(struct linear *c,struct firmware *s) { (void)c; (void)s; ++frees; return 0; }
static void WdfObjectDereference(void *p) { (void)p; ++refs; }
'''
main=r'''
int main(void) {
 GPU_CONTEXT c={0}; c.copy_online=1; c.referenced=1; c.firmware.processor_released=1;
 c.linear.memory.cpu=&c;
 assert(GpuD0Exit(&c,0)==STATUS_DEVICE_BUSY && !c.copy_online && c.firmware.processor_released && c.referenced && !stops && !frees && !refs);
 c.linear.memory.cpu=0; c.linear.memory.adapter=&c;
 assert(GpuD0Exit(&c,0)==STATUS_DEVICE_BUSY && !stops && !frees && !refs);
 c.linear.memory.adapter=0;
 assert(GpuD0Exit(&c,0)==STATUS_SUCCESS && stops==1 && frees==1 && refs==1 && !c.referenced);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c'; exe=Path(d)/'test'; c.write_text(preamble+fn+'\n'+main)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('actual D0Exit retains owned GPU DMA and firmware on busy paths')
