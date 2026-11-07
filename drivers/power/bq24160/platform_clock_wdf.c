/* SPDX-License-Identifier: MIT */
#include "platform_clock_wdf.h"
#include "bqhr_acpi.h"
static uint64_t get64(const UCHAR *p)
{ uint64_t v=0;unsigned i;for(i=0;i<8;++i)v|=(uint64_t)p[i]<<(i*8);return v; }
enum a04_bq_clock_result a04_bq_clock_observe(WDFDEVICE device,struct a04_bq_clock_observation *out,NTSTATUS *io_status)
{
    LARGE_INTEGER first,second,freq1,freq2;
    UCHAR record[128]; uint64_t interrupt_ticks;
    enum a04_bqhr_acpi_result transport;
    if (!out) return A04_CLOCK_INVALID;
    RtlZeroMemory(out,sizeof(*out));
    /* Deliberately sample the OS clock before both QPC reads. */
    interrupt_ticks=KeQueryInterruptTime();
    if (interrupt_ticks>UINT64_MAX/100u) return A04_CLOCK_INVALID;
    first=KeQueryPerformanceCounter(&freq1);
    transport=a04_bqhr_receive(device,io_status,record);
    second=KeQueryPerformanceCounter(&freq2);
    if (transport!=A04_BQHR_ACPI_STOPPED && transport!=A04_BQHR_ACPI_OTHER_STATE)
        return A04_CLOCK_INVALID;
    if (first.QuadPart<0 || second.QuadPart<0 || freq1.QuadPart<=0 || freq2.QuadPart<=0)
        return A04_CLOCK_INVALID;
    out->qpc_before=(uint64_t)first.QuadPart;out->qpc_after=(uint64_t)second.QuadPart;
    out->frequency=(uint64_t)freq1.QuadPart;
    out->record_state=get64(record+16);out->firmware_last=get64(record+24);
    out->firmware_frequency=get64(record+32);out->firmware_exit=get64(record+40);
    return a04_bq_clock_compare(record,sizeof(record),interrupt_ticks*100u,out->qpc_before,out->qpc_after,
                                out->frequency,(uint64_t)freq2.QuadPart);
}
