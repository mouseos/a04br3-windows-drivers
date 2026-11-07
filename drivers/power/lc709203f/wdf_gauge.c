/* SPDX-License-Identifier: MIT
 * Default-off LC709203F battery function driver. I2C2 must have one owner;
 * Optional BQ profiles write charger settings; no gauge settings are changed.
 * Clock/pin setup remains firmware-owned and is checked before each D0 use.
 */
#include <ntddk.h>
#include <wdf.h>
#include <batclass.h>
#include "battery_class.h"
#include "mt8167_i2c.h"
#include "../bq24160/charger.h"
#if A04_BQ24160_EXPERIMENTAL
#include "../bq24160/battery_status.h"
#endif
#include "../bq24160/mt8167_i2c.h"
#ifndef A04_BQ24160_EXPERIMENTAL
#define A04_BQ24160_EXPERIMENTAL 0
#endif
#ifndef A04_BQ_LINUX_PROFILE_BOOT
#define A04_BQ_LINUX_PROFILE_BOOT 0
#endif
#if A04_BQ_LINUX_PROFILE_BOOT
#include "../bq24160/cold_profile.h"
#endif
#ifndef A04_BQ_COLD_DIAGNOSTIC
#define A04_BQ_COLD_DIAGNOSTIC 0
#endif
#ifndef A04_BQ_D0_DEFAULT_DIAGNOSTIC
#define A04_BQ_D0_DEFAULT_DIAGNOSTIC 0
#endif
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
#include "../bq24160/d0_session.h"
#endif
#ifndef A04_BQ_STOPPED_GAUGE_DIAGNOSTIC
#define A04_BQ_STOPPED_GAUGE_DIAGNOSTIC 0
#endif
#ifndef A04_BQ_DEFAULT_EXIT_EXPERIMENTAL
#define A04_BQ_DEFAULT_EXIT_EXPERIMENTAL 0
#endif
#ifndef A04_BROKER_CLIENT_EXPERIMENTAL
#define A04_BROKER_CLIENT_EXPERIMENTAL 0
#endif
#ifndef A04_BROKER_OWNER_EXPERIMENTAL
#define A04_BROKER_OWNER_EXPERIMENTAL 0
#endif
#if A04_BROKER_CLIENT_EXPERIMENTAL && !A04_BQ24160_EXPERIMENTAL
#error Broker client build requires BQ experimental components
#endif
#if A04_BQ_LINUX_PROFILE_BOOT && !A04_BQ_COLD_DIAGNOSTIC
#error Linux profile boot requires checked cold diagnostic ownership
#endif
#if A04_BQ_COLD_DIAGNOSTIC && (!A04_BROKER_OWNER_EXPERIMENTAL || !A04_BQ_DEFAULT_EXIT_EXPERIMENTAL || A04_BQ_STOPPED_GAUGE_DIAGNOSTIC)
#error Cold diagnostic requires broker owner, DEFAULT exit, and no gauge-only fallback
#endif
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC && (!A04_BQ_COLD_DIAGNOSTIC || !A04_BQ_LINUX_PROFILE_BOOT || !A04_BQ_DEFAULT_EXIT_EXPERIMENTAL || !A04_BROKER_OWNER_EXPERIMENTAL || A04_BROKER_CLIENT_EXPERIMENTAL || A04_BQ_STOPPED_GAUGE_DIAGNOSTIC)
#error D0 DEFAULT diagnostic requires local cold Linux owner and exact DEFAULT exit
#endif
#if A04_BQ_STOPPED_GAUGE_DIAGNOSTIC && !A04_BROKER_OWNER_EXPERIMENTAL
#error Stopped-record gauge diagnostic requires broker owner build
#endif
#if A04_BQ_DEFAULT_EXIT_EXPERIMENTAL && !A04_BROKER_OWNER_EXPERIMENTAL
#error DEFAULT exit requires the broker owner
#endif
#if A04_BROKER_OWNER_EXPERIMENTAL && (!A04_BQ24160_EXPERIMENTAL || A04_BROKER_CLIENT_EXPERIMENTAL)
#error Broker owner requires BQ experimental and excludes broker client
#endif
#if A04_BROKER_CLIENT_EXPERIMENTAL || A04_BROKER_OWNER_EXPERIMENTAL
#include "../bq24160/broker_wdf.h"
#endif
#if A04_BROKER_OWNER_EXPERIMENTAL
#include "../bq24160/broker_lifecycle.h"
#include "../bq24160/admission.h"
#endif
#ifndef A04_BQHR_DIAGNOSTIC
#define A04_BQHR_DIAGNOSTIC 0
#endif
#ifndef A04_BQ_CLOCK_DIAGNOSTIC
#define A04_BQ_CLOCK_DIAGNOSTIC 0
#endif
#if A04_BQ_CLOCK_DIAGNOSTIC && !A04_BQHR_DIAGNOSTIC
#error Clock observation requires the BQHR diagnostic
#endif
#if A04_BQ_CLOCK_DIAGNOSTIC
#include "../bq24160/platform_clock_wdf.h"
#endif
#if A04_BQHR_DIAGNOSTIC || A04_BROKER_OWNER_EXPERIMENTAL
#include "../bq24160/bqhr_acpi.h"
#if A04_BQ_COLD_DIAGNOSTIC
#include "../bq24160/bqhr_diagnostic.h"
#endif
#endif
#ifndef A04_LC709203F_TRACE
#define A04_LC709203F_TRACE 0
#endif

#define I2C2_BASE 0x1100b000u
#define I2C2_SIZE 0x1000u

typedef struct {
    WDFDEVICE device;
    WDFTIMER timer;
    WDFWAITLOCK class_lock;
    WDFWAITLOCK bus_lock;
#if A04_BROKER_CLIENT_EXPERIMENTAL
    WDFWAITLOCK broker_lock;
    struct a04_broker_wdf broker;
#endif
#if A04_BROKER_OWNER_EXPERIMENTAL
    struct a04_broker_endpoint owner_endpoint;
    struct a04_bq_broker owner_broker;
    struct a04_broker_lifecycle owner_lifecycle;
    uint64_t owner_lifetime, owner_epoch;
    WDFQUEUE owner_queue;
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
    struct a04_bq_d0_session d0_session;
    ULONG report_count;
#endif
#if A04_BQ_STOPPED_GAUGE_DIAGNOSTIC
    BOOLEAN owner_fallback;
#endif
#endif
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
    struct a04_bq_owner charger;
    struct a04_bq_bus charger_bus;
#endif
    PVOID class_handle;
    volatile UCHAR *i2c;
    struct a04_i2c_io io;
    struct a04_gauge_mt8167 adapter;
    struct a04_gauge_bus bus;
    struct a04_battery_class battery;
    volatile LONG online;
    BOOLEAN faulted;
#if A04_LC709203F_TRACE
    volatile ULONG *uart;
#endif
} GAUGE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GAUGE_CONTEXT, GaugeContext)
EVT_WDF_DRIVER_DEVICE_ADD GaugeAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE GaugePrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE GaugeRelease;
EVT_WDF_DEVICE_D0_ENTRY GaugeD0Entry;
EVT_WDF_DEVICE_D0_EXIT GaugeD0Exit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT GaugeClassInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP GaugeClassCleanup;
EVT_WDF_TIMER GaugePoll;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS GaugeIrp;

#if A04_LC709203F_TRACE
static void trace(GAUGE_CONTEXT *c, const char *message)
{
    ULONG n;
    if (!c->uart) return;
    while (*message) {
        for (n = 100000; n; --n)
            if (!(READ_REGISTER_ULONG(c->uart + 3) & 0x80u) &&
                (READ_REGISTER_ULONG(c->uart + 5) & 0x20u)) break;
        if (!n) return;
        WRITE_REGISTER_ULONG(c->uart, (ULONG)(UCHAR)*message++);
    }
}
static void trace_hex(GAUGE_CONTEXT *c, ULONG value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[9]; unsigned i;
    for (i = 0; i < 8; ++i) text[i] = digits[(value >> (28 - i * 4)) & 15];
    text[8] = 0; trace(c, text);
}
#else
#define trace(c, message) ((void)0)
#define trace_hex(c, value) ((void)0)
#endif

static uint16_t rd(void *context, unsigned offset)
{ GAUGE_CONTEXT *c = context; return READ_REGISTER_USHORT((volatile USHORT *)(c->i2c + offset)); }
static void wr(void *context, unsigned offset, uint16_t value)
{ GAUGE_CONTEXT *c = context; WRITE_REGISTER_USHORT((volatile USHORT *)(c->i2c + offset), value); }
static void delay(void *context, unsigned us)
{ UNREFERENCED_PARAMETER(context); KeStallExecutionProcessor(us); }
static uint64_t now_ns(void *context)
{ UNREFERENCED_PARAMETER(context); return KeQueryInterruptTime() * 100u; }
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
/* No Windows firmware handoff protocol exists yet. This gate is intentionally
 * closed even for the experimental build; a compile flag is not proof that
 * firmware stopped accessing I2C2 or that its last pet is recent. */
static int charger_handoff_is_qualified(void) { return 0; }
static uint64_t charger_now(void *context)
{
    struct a04_gauge_mt8167 *a = context;
    return a->monotonic_ns(a->io->context);
}
#endif
#if A04_BROKER_OWNER_EXPERIMENTAL && A04_LC709203F_TRACE
/* Report only cached data already acquired by the owner. In particular this
 * does not touch BQ after RESET, which would start its watchdog again. */
static void trace_owner_values(GAUGE_CONTEXT *c,const char *phase,
                               const struct a04_bq_owner *owner,ULONG life,ULONG fault)
{
    unsigned i;
    const struct a04_bq_snapshot *snapshot=&owner->latest;
    trace(c,"A04BAT OWNER phase=");trace(c,phase);
    trace(c," life=");trace_hex(c,life);
    trace(c," valid=");trace_hex(c,(ULONG)snapshot->valid);
    trace(c," regs=");
    for (i=0;i<8;++i) trace_hex(c,(ULONG)snapshot->raw[i]);
    trace(c," faults=");trace_hex(c,(ULONG)owner->fault_history);
    trace(c," kicks=");trace_hex(c,(ULONG)owner->kicks);
    trace(c," io_fault=");trace_hex(c,fault);
    trace(c," stopped=");trace_hex(c,(ULONG)owner->stopped);trace(c,"\n");
}
static void trace_owner(GAUGE_CONTEXT *c,const char *phase)
{ trace_owner_values(c,phase,&c->charger,(ULONG)c->owner_lifecycle.state,(ULONG)c->adapter.fault_reason); }

#else
#define trace_owner(c, phase) ((void)0)
#endif
#if A04_BQHR_DIAGNOSTIC
static void diagnose_bqhr(GAUGE_CONTEXT *c)
{
    NTSTATUS status;
    enum a04_bqhr_acpi_result r = a04_bqhr_evaluate(c->device, &status);
    if (r == A04_BQHR_ACPI_STOPPED) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "A04BAT BQHR STOPPED VERIFIED\n");
        trace(c, "A04BAT BQHR STOPPED VERIFIED\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                   "A04BAT BQHR FAILED result=%u status=%08lx\n", (unsigned)r, status);
        trace(c, "A04BAT BQHR FAILED\n");
    }
#if A04_BQ_CLOCK_DIAGNOSTIC
    {
        struct a04_bq_clock_observation observation;
        enum a04_bq_clock_result result=a04_bq_clock_observe(c->device,&observation,&status);
        /* Candidate is only a numeric comparison; it never opens admission. */
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,
                   "A04BAT CLOCK result=%u qpc_before=%I64x qpc_after=%I64x frequency=%I64x\n",
                   (unsigned)result,observation.qpc_before,observation.qpc_after,observation.frequency);
        trace(c,"A04BAT CLOCK result=");trace_hex(c,(ULONG)result);
        trace(c," qpc_before=");trace_hex(c,(ULONG)(observation.qpc_before>>32));trace_hex(c,(ULONG)observation.qpc_before);
        trace(c," qpc_after=");trace_hex(c,(ULONG)(observation.qpc_after>>32));trace_hex(c,(ULONG)observation.qpc_after);
        trace(c," freq=");trace_hex(c,(ULONG)(observation.frequency>>32));trace_hex(c,(ULONG)observation.frequency);trace(c,"\n");
        trace(c,"A04BAT CLOCK record_state=");trace_hex(c,(ULONG)observation.record_state);
        trace(c," fw_last=");trace_hex(c,(ULONG)(observation.firmware_last>>32));trace_hex(c,(ULONG)observation.firmware_last);
        trace(c," fw_exit=");trace_hex(c,(ULONG)(observation.firmware_exit>>32));trace_hex(c,(ULONG)observation.firmware_exit);
        trace(c," fw_freq=");trace_hex(c,(ULONG)(observation.firmware_frequency>>32));trace_hex(c,(ULONG)observation.firmware_frequency);trace(c,"\n");
    }
#endif
}
#endif
#if A04_BROKER_CLIENT_EXPERIMENTAL
/* A future resolver must validate the owner, target lifetime, boot-local epoch,
 * PnP resource exclusion and the shared clock before returning success. */
static NTSTATUS broker_resolve(GAUGE_CONTEXT *c,WDFIOTARGET *target,uint64_t *epoch)
{ UNREFERENCED_PARAMETER(c);UNREFERENCED_PARAMETER(target);UNREFERENCED_PARAMETER(epoch);return STATUS_DEVICE_NOT_READY; }
#endif
#if A04_BROKER_OWNER_EXPERIMENTAL
/* These must be implemented from verified platform ownership evidence, not
 * flags or guessed epochs. No D0-off successor is currently established. */
static NTSTATUS owner_platform_start(GAUGE_CONTEXT *c,uint64_t *lifetime,uint64_t *epoch,
                                      uint64_t *counter,uint64_t *frequency)
{ UNREFERENCED_PARAMETER(c);UNREFERENCED_PARAMETER(lifetime);UNREFERENCED_PARAMETER(epoch);UNREFERENCED_PARAMETER(counter);UNREFERENCED_PARAMETER(frequency);return STATUS_DEVICE_NOT_READY; }
static NTSTATUS owner_prepare_handoff(GAUGE_CONTEXT *c)
{
    UCHAR record[128]; NTSTATUS status; uint64_t counter=0,frequency=0,now;
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
    {
        int entry=a04_bq_d0_begin(&c->d0_session);
        if (!entry) return STATUS_DEVICE_NOT_READY;
        if (entry==2) {
            uint64_t begin=now_ns(c);
            /* Only local endpoint storage exists in this profile; the timer
             * was stopped synchronously on exit. Do not replay boot BQHR or
             * reset a transport fault. Previous DEFAULT closed the endpoint. */
            if (c->adapter.faulted || c->owner_lifecycle.state!=A04_LIFE_STOPPED ||
                !c->owner_endpoint.closed || !c->charger.stopped ||
                begin>UINT64_MAX-10000000000ULL) return STATUS_DEVICE_NOT_READY;
            RtlZeroMemory(&c->charger,sizeof(c->charger));
            RtlZeroMemory(&c->owner_endpoint,sizeof(c->owner_endpoint));
            RtlZeroMemory(&c->owner_broker,sizeof(c->owner_broker));
            RtlZeroMemory(&c->owner_lifecycle,sizeof(c->owner_lifecycle));
            if (a04_bq_prepare_cold_diagnostic(&c->charger,begin,begin+10000000000ULL))
                return STATUS_DEVICE_NOT_READY;
            c->owner_lifetime=UINT64_MAX;c->owner_epoch=c->d0_session.generation;
            trace(c,"A04BAT D0 NEW LOCAL EPOCH=");trace_hex(c,(ULONG)c->owner_epoch);trace(c,"\n");
            return STATUS_SUCCESS;
        }
    }
#endif
    /* BQHR reads the firmware NVS copy only, not the charger. Stopped records
     * are explicitly non-adoptable, even when their checksum is valid. */
    {
        enum a04_bqhr_acpi_result record_status=a04_bqhr_receive(c->device,&status,record);
#if A04_BQ_STOPPED_GAUGE_DIAGNOSTIC
        if (record_status==A04_BQHR_ACPI_STOPPED) return STATUS_NOT_SUPPORTED;
#endif
#if A04_BQ_COLD_DIAGNOSTIC
        if (record_status==A04_BQHR_ACPI_STOPPED) {
            struct a04_bqhr_info info;
            uint64_t cold_now=now_ns(c);
            if (a04_bqhr_classify(record,sizeof(record),&info)!=A04_BQHR_STOPPED ||
                !a04_bqhr_is_cold_candidate(&info) ||
                cold_now>UINT64_MAX-10000000000ULL ||
                a04_bq_prepare_cold_diagnostic(&c->charger,cold_now,cold_now+10000000000ULL))
                return STATUS_DEVICE_NOT_READY;
            /* PnP exclusive mapping was checked by PrepareHardware. This is
             * an explicit experiment, not proof against firmware/other CPUs. */
            c->owner_lifetime=UINT64_MAX;
            #if A04_BQ_D0_DEFAULT_DIAGNOSTIC
            c->owner_epoch=c->d0_session.generation;
#else
            c->owner_epoch=cold_now ? cold_now : 1;
#endif
            trace(c,"A04BAT COLD OWNER EXPERIMENT\n");
            return STATUS_SUCCESS;
        }
#endif
        if (record_status!=A04_BQHR_ACPI_OTHER_STATE) return STATUS_DEVICE_NOT_READY;
    }
    /* Sample OS time first, then let the platform read the *same* physical
     * counter as firmware and establish ownership and continuity evidence. */
    now=now_ns(c);
    status=owner_platform_start(c,&c->owner_lifetime,&c->owner_epoch,&counter,&frequency);
    if (status!=STATUS_SUCCESS || a04_bq_admission_prepare(&c->charger,record,sizeof(record),now,
                    counter,frequency,c->owner_lifetime,c->owner_epoch)) {
        c->faulted=TRUE;return STATUS_DEVICE_NOT_READY;
    }
    return STATUS_SUCCESS;
}
static NTSTATUS owner_platform_successor(GAUGE_CONTEXT *c,struct a04_bq_owner **next,
                                         struct a04_owner_transfer **transfer,
                                         const struct a04_transfer_ops **ops,uint64_t *deadline)
{ UNREFERENCED_PARAMETER(c);UNREFERENCED_PARAMETER(next);UNREFERENCED_PARAMETER(transfer);UNREFERENCED_PARAMETER(ops);UNREFERENCED_PARAMETER(deadline);return STATUS_DEVICE_NOT_READY; }
#endif
static void notify(GAUGE_CONTEXT *c)
{
    WdfWaitLockAcquire(c->class_lock, NULL);
    if (c->class_handle) BatteryClassStatusNotify(c->class_handle);
    WdfWaitLockRelease(c->class_lock);
}
#if A04_BROKER_OWNER_EXPERIMENTAL
/* Queue callback is outside bus_lock. Latch failed shared ownership and
 * invalidate battery data before notifying BatteryClass. Never restart here. */
static void owner_broker_failed(void *context)
{
    GAUGE_CONTEXT *c=context;
    BOOLEAN changed=FALSE;
    WdfWaitLockAcquire(c->bus_lock,NULL);
    if (c->owner_endpoint.broker && c->owner_endpoint.broker->stopped) {
        a04_broker_lifecycle_stop(&c->owner_lifecycle);
        c->faulted=TRUE;InterlockedExchange(&c->online,0);
        changed=a04_battery_invalidate(&c->battery);
    }
    WdfWaitLockRelease(c->bus_lock);
    if (changed) notify(c);
}
#endif
static void poll(GAUGE_CONTEXT *c)
{
    struct a04_gauge_sample sample;
    BOOLEAN changed = FALSE;
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL && !A04_BROKER_OWNER_EXPERIMENTAL
    enum a04_bq_result result;
    uint64_t now;
#endif
    WdfWaitLockAcquire(c->bus_lock, NULL);
    if (!c->online) { WdfWaitLockRelease(c->bus_lock); return; }
#if A04_BROKER_CLIENT_EXPERIMENTAL
    {
        uint64_t now=now_ns(c);
        if (now>UINT64_MAX-2000000000ULL ||
            a04_broker_wdf_read(&c->broker,now+2000000000ULL,&sample)) {
            c->faulted=TRUE; InterlockedExchange(&c->online,0);
            changed=a04_battery_invalidate(&c->battery);
        } else changed=a04_battery_publish(&c->battery,&sample);
    }
#elif A04_BROKER_OWNER_EXPERIMENTAL
    {
        uint64_t now=now_ns(c),deadline;
        int error;
#if A04_BQ_STOPPED_GAUGE_DIAGNOSTIC
        if (c->owner_fallback) {
            error=a04_gauge_read_sample(&c->bus,&sample);
            if (!error) {
                changed=a04_battery_publish(&c->battery,&sample);
                trace(c,"A04BAT STOPPED GAUGE mv=");trace_hex(c,(ULONG)sample.voltage_mv);
                trace(c," pct=");trace_hex(c,(ULONG)sample.percent);trace(c,"\n");
            } else {
                changed=a04_battery_invalidate(&c->battery);
                trace(c,"A04BAT STOPPED GAUGE ERROR reason=");trace_hex(c,c->adapter.fault_reason);
                trace(c," command=");trace_hex(c,c->adapter.fault_command);
                trace(c," status=");trace_hex(c,c->adapter.fault_status);
                trace(c," fifo=");trace_hex(c,c->adapter.fault_fifo);trace(c,"\n");
                if (c->adapter.faulted) {c->faulted=TRUE;InterlockedExchange(&c->online,0);}
            }
            WdfWaitLockRelease(c->bus_lock);
            if (changed) notify(c);
            return;
        }
#endif
        if (c->owner_lifecycle.state==A04_LIFE_QUIESCING)
            error=a04_broker_lifecycle_maintain(&c->owner_lifecycle);
        else {
            deadline=now<=UINT64_MAX-2000000000ULL ? now+2000000000ULL : 0;
            if (deadline>c->owner_broker.lifetime) deadline=c->owner_broker.lifetime;
            error=deadline ? a04_broker_lifecycle_sample_local(&c->owner_lifecycle,deadline,&sample) : -1;
            if (!error) {
                /* The broker has accepted this cached BQ snapshot and kicked
                 * its watchdog. Linux reports STAT 3/4 as charging. */
                changed=a04_battery_publish_power(&c->battery,&sample,
                    BATTERY_POWER_ON_LINE | (a04_bq_status_charging(c->charger.latest.raw[0]) ? BATTERY_CHARGING : 0));
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
                if (c->report_count<3) {
                    ++c->report_count;
                    trace(c,"A04BAT POWER epoch=");trace_hex(c,(ULONG)c->owner_epoch);
                    trace(c," stat=");trace_hex(c,(c->charger.latest.raw[0]>>4)&7u);
                    trace(c," source=");trace_hex(c,c->charger.latest.raw[1]);
                    trace(c," mv=");trace_hex(c,sample.voltage_mv);
                    trace(c," pct=");trace_hex(c,sample.percent);
                    trace(c," gauge=PEC_VALID rate=UNKNOWN\n");
                }
#endif
            } else if (error>0) {
                /* Invalid gauge data does not invalidate a healthy shared bus. */
                changed=a04_battery_invalidate(&c->battery);
                error=0;
            }
        }
        if (error) {
            a04_broker_lifecycle_stop(&c->owner_lifecycle);
            trace_owner(c,"POLL_ERROR");
            c->faulted=TRUE;InterlockedExchange(&c->online,0);
            changed|=a04_battery_invalidate(&c->battery);
        }
    }
#else
#if A04_BQ24160_EXPERIMENTAL
    now = charger_now(&c->adapter);
    /* The first call requires a qualified firmware ownership handoff.
     * This experimental build has no validated handoff record, so it is
     * never eligible for deployment. See README before any target use. */
    result = a04_bq_step(&c->charger, &c->charger_bus,
                          now <= UINT64_MAX - 20000000000ULL ? now + 20000000000ULL : 0);
    if (result != A04_BQ_OK) {
        trace(c, "A04BAT CHARGER STOPPED\n");
        c->faulted = TRUE; InterlockedExchange(&c->online, 0);
        changed = a04_battery_invalidate(&c->battery);
        WdfWaitLockRelease(c->bus_lock);
        if (changed) notify(c);
        return;
    }
    /* Only accepted charger snapshots establish usable external input.
     * A failed or ambiguous sample invalidates the battery status entirely. */
#endif
    if (a04_gauge_read_sample(&c->bus, &sample)) {
        trace(c, "A04BAT SAMPLE FAILED reason="); trace_hex(c, c->adapter.fault_reason);
        trace(c, " command="); trace_hex(c, c->adapter.fault_command);
        trace(c, " status="); trace_hex(c, c->adapter.fault_status);
        trace(c, " fifo="); trace_hex(c, c->adapter.fault_fifo); trace(c, "\n");
        changed |= a04_battery_invalidate(&c->battery);
        /* A failed PEC/range check alone leaves the transport healthy.
         * Do not publish the gauge; the next periodic read can recover it.
         * A busy controller, NACK or timeout latches the shared bus fault. */
        if (c->adapter.faulted) {
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
            c->charger.stopped = 1;
#endif
            c->faulted = TRUE;
            InterlockedExchange(&c->online, 0);
        }
    } else {
        trace(c, "A04BAT SAMPLE VALID\n");
#if A04_BQ24160_EXPERIMENTAL
        {
            changed |= a04_battery_publish_power(&c->battery,&sample,
                BATTERY_POWER_ON_LINE | (a04_bq_status_charging(c->charger.latest.raw[0]) ? BATTERY_CHARGING : 0));
        }
#else
        changed |= a04_battery_publish(&c->battery, &sample);
#endif
    }
#endif
    WdfWaitLockRelease(c->bus_lock);
    if (changed) notify(c);
}
NTSTATUS GaugePrepare(WDFDEVICE device, WDFCMRESLIST raw, WDFCMRESLIST translated)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    ULONG i, found = 0;
    PHYSICAL_ADDRESS address;
#if A04_BROKER_CLIENT_EXPERIMENTAL
    /* No proven owner, timebase or non-overlapping PnP resources yet. Do not
     * map I2C2 or attempt target discovery merely because this was built. */
    UNREFERENCED_PARAMETER(c); UNREFERENCED_PARAMETER(raw); UNREFERENCED_PARAMETER(translated);
    return STATUS_DEVICE_NOT_READY;
#endif
    if (WdfCmResourceListGetCount(raw) != WdfCmResourceListGetCount(translated))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    for (i = 0; i < WdfCmResourceListGetCount(translated); ++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(translated, i);
        PCM_PARTIAL_RESOURCE_DESCRIPTOR r = WdfCmResourceListGetDescriptor(raw, i);
        if (!d || !r || d->Type != r->Type) return STATUS_DEVICE_CONFIGURATION_ERROR;
        if (d && d->Type == CmResourceTypeMemory) {
            if (r->u.Memory.Start.QuadPart != I2C2_BASE ||
                r->u.Memory.Length != I2C2_SIZE || d->u.Memory.Length != I2C2_SIZE ||
                r->ShareDisposition != CmResourceShareDeviceExclusive ||
                d->ShareDisposition != CmResourceShareDeviceExclusive || found)
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            address = d->u.Memory.Start; ++found;
        }
    }
    if (found != 1) return STATUS_DEVICE_CONFIGURATION_ERROR;
    c->i2c = MmMapIoSpace(address, I2C2_SIZE, MmNonCached);
    if (!c->i2c) return STATUS_INSUFFICIENT_RESOURCES;
#if A04_LC709203F_TRACE
    { PHYSICAL_ADDRESS uart; uart.QuadPart = 0x11005000u;
      c->uart = MmMapIoSpace(uart, 0x1000u, MmNonCached); }
#endif
    trace(c, "A04BAT PREPARE\n");
    return STATUS_SUCCESS;
}
NTSTATUS GaugeRelease(WDFDEVICE device, WDFCMRESLIST translated)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(translated);
    if (c->i2c) { MmUnmapIoSpace((PVOID)c->i2c, I2C2_SIZE); c->i2c = NULL; }
#if A04_LC709203F_TRACE
    if (c->uart) { MmUnmapIoSpace((PVOID)c->uart, 0x1000u); c->uart = NULL; }
#endif
    return STATUS_SUCCESS;
}
NTSTATUS GaugeD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(previous);
#if A04_BROKER_CLIENT_EXPERIMENTAL
    {
        WDFIOTARGET target=NULL;
        uint64_t epoch=0;
        NTSTATUS status=broker_resolve(c,&target,&epoch);
        if (!NT_SUCCESS(status) || c->faulted || !target || !epoch) return STATUS_DEVICE_NOT_READY;
        if (a04_broker_wdf_init(&c->broker,target,c->broker_lock,A04_BROKER_INTERNAL_IOCTL,epoch))
            return STATUS_DEVICE_NOT_READY;
        InterlockedExchange(&c->online,1);
        poll(c);
        if (!c->online) { a04_broker_wdf_stop(&c->broker); return STATUS_DEVICE_HARDWARE_ERROR; }
        WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(30000));
        return STATUS_SUCCESS;
    }
#else
    if (!c->i2c || c->faulted) return STATUS_DEVICE_NOT_READY;
#if A04_BQHR_DIAGNOSTIC
    diagnose_bqhr(c);
#endif
#if A04_BROKER_OWNER_EXPERIMENTAL
    {
        NTSTATUS admission=owner_prepare_handoff(c);
#if A04_BQ_STOPPED_GAUGE_DIAGNOSTIC
        c->owner_fallback=(admission==STATUS_NOT_SUPPORTED);
        if (c->owner_fallback) trace(c,"A04BAT STOPPED GAUGE ONLY NO BQ\n");
        else
#endif
        if (!NT_SUCCESS(admission)) {
            trace(c,"A04BAT OWNER ADMISSION REFUSED\n");
            return STATUS_DEVICE_NOT_READY;
        }
    }
#elif A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
    if (!charger_handoff_is_qualified()) return STATUS_DEVICE_NOT_READY;
#endif
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
    c->report_count=0;
#endif
    trace(c, "A04BAT D0 ENTRY\n");
    c->io.context = c; c->io.read16 = rd; c->io.write16 = wr; c->io.delay_us = delay;
    c->adapter.io = &c->io; c->adapter.monotonic_ns = now_ns; c->adapter.faulted = 0;
    c->bus.context = &c->adapter; c->bus.read_word_pec = a04_gauge_mt8167_read;
    /* Only accept pre-initialized, idle polling-mode controller state. */
    if ((rd(c, 0x24) & 1u) || !rd(c, 0x20) || (rd(c, 0x48) & 1u) ||
        rd(c, 0x40) != 3u || rd(c, 0x60) != 0u) {
#if A04_BROKER_OWNER_EXPERIMENTAL
        c->charger.stopped=1;c->faulted=TRUE;
#endif
        trace(c, "A04BAT I2C NOT READY\n"); return STATUS_DEVICE_NOT_READY;
    }
    trace(c, "A04BAT I2C READY\n");
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
    c->charger_bus.context = &c->adapter;
    c->charger_bus.now_ns = charger_now;
    c->charger_bus.read = a04_bq_mt8167_read;
    c->charger_bus.write_watchdog = a04_bq_mt8167_kick;
#endif
#if A04_BROKER_OWNER_EXPERIMENTAL
#if A04_BQ_STOPPED_GAUGE_DIAGNOSTIC
    if (c->owner_fallback) {
        InterlockedExchange(&c->online,1);
        poll(c);
        if (!c->online) return STATUS_DEVICE_HARDWARE_ERROR;
        WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(30000));
        return STATUS_SUCCESS;
    }
#endif
    {
        NTSTATUS status;
#if A04_BQ_LINUX_PROFILE_BOOT
        struct a04_bq_cold_profile_result profile;
        int configured;
#endif
        WdfWaitLockAcquire(c->bus_lock,NULL);
        /* Arm maintenance before publishing the endpoint: an internal IOCTL
         * may immediately pet the BQ once it can acquire this same lock. */
        InterlockedExchange(&c->online,1);
        WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(10000));
#if A04_BQ_LINUX_PROFILE_BOOT
        {
            struct a04_bq_recovery_io io={&c->adapter,charger_now,a04_bq_mt8167_read,a04_bq_mt8167_recovery_write};
            configured=a04_bq_cold_profile(&c->charger,&io,&profile);
            if(configured) {
                c->faulted=TRUE;InterlockedExchange(&c->online,0);
                WdfWaitLockRelease(c->bus_lock);
                DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,
                    "A04BAT PROFILE FAILED stage=%u valid=%02x attempted=%d\n",
                    profile.stage,profile.before.valid,profile.write_attempted);
                trace(c,"A04BAT PROFILE FAILED stage=");trace_hex(c,profile.stage);
                trace(c," valid=");trace_hex(c,profile.before.valid);
                trace(c," attempted=");trace_hex(c,profile.write_attempted);trace(c,"\n");
                WdfTimerStop(c->timer,TRUE);WdfTimerStop(c->timer,FALSE);
                return STATUS_DEVICE_HARDWARE_ERROR;
            }
        }
#endif
        status=a04_broker_lifecycle_start(&c->owner_lifecycle,&c->owner_endpoint,&c->owner_broker,
                    &c->charger,&c->charger_bus,&c->bus,now_ns(c),c->owner_lifetime,c->owner_epoch) ? STATUS_DEVICE_NOT_READY : STATUS_SUCCESS;
        if (!NT_SUCCESS(status)) {c->charger.stopped=1;c->faulted=TRUE;InterlockedExchange(&c->online,0);}
        WdfWaitLockRelease(c->bus_lock);
#if A04_BQ_LINUX_PROFILE_BOOT
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,
            "A04BAT PROFILE stage=%u owner=%08lx usb_ma=100\n",profile.stage,(ULONG)status);
#endif
        if (!NT_SUCCESS(status)) { WdfTimerStop(c->timer,TRUE);WdfTimerStop(c->timer,FALSE);return status; }
    }
#endif
    InterlockedExchange(&c->online, 1);
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL && !A04_BROKER_OWNER_EXPERIMENTAL
    /* Arm before any charger transaction. D0Exit synchronously cancels it. */
    WdfTimerStart(c->timer, WDF_REL_TIMEOUT_IN_MS(10000));
#endif
    poll(c);
    if (!c->online) {
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
        WdfTimerStop(c->timer, TRUE);
        WdfTimerStop(c->timer, FALSE);
#endif
        return STATUS_DEVICE_HARDWARE_ERROR;
    }
#if A04_BQ_COLD_DIAGNOSTIC && A04_LC709203F_TRACE
    {
        struct a04_bq_owner copy;
        ULONG life, fault;
        /* Copy under the I2C lock, but print outside it: slow UART must not
         * block the watchdog worker. No additional charger transaction. */
        WdfWaitLockAcquire(c->bus_lock,NULL);
        copy=c->charger;life=(ULONG)c->owner_lifecycle.state;fault=(ULONG)c->adapter.fault_reason;
        WdfWaitLockRelease(c->bus_lock);
        trace_owner_values(c,"INITIAL_CACHED",&copy,life,fault);
    }
#endif
#if !A04_BQ24160_EXPERIMENTAL || A04_BROKER_CLIENT_EXPERIMENTAL
    WdfTimerStart(c->timer, WDF_REL_TIMEOUT_IN_MS(30000));
#endif
    return STATUS_SUCCESS;
#endif
}
NTSTATUS GaugeD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    UNREFERENCED_PARAMETER(target);
#if A04_BROKER_OWNER_EXPERIMENTAL
    /* Close admission under the same lock while the D0 timer can still run. */
    WdfWaitLockAcquire(c->bus_lock,NULL);
    if (c->owner_lifecycle.state==A04_LIFE_LIVE)
        (void)a04_broker_lifecycle_quiesce(&c->owner_lifecycle);
    WdfWaitLockRelease(c->bus_lock);
#endif
    InterlockedExchange(&c->online, 0);
    WdfTimerStop(c->timer, TRUE);
    /* The callback may have rearmed immediately before observing offline. */
    WdfTimerStop(c->timer, FALSE);
    WdfWaitLockAcquire(c->bus_lock, NULL);
#if A04_BROKER_OWNER_EXPERIMENTAL
    if (c->owner_lifecycle.state==A04_LIFE_QUIESCING) {
        struct a04_bq_owner *next=NULL;
        struct a04_owner_transfer *transfer=NULL;
        const struct a04_transfer_ops *ops=NULL;
        uint64_t deadline=0;
        NTSTATUS status=owner_platform_successor(c,&next,&transfer,&ops,&deadline);
        if (!NT_SUCCESS(status) || !next || !transfer || !ops || !deadline) {
#if A04_BQ_DEFAULT_EXIT_EXPERIMENTAL
            /* Timer stopped, queue quiesced, bus lock held. Do not read BQ
             * after RESET: I2C access starts its watchdog again. */
            int result;
            result=a04_broker_lifecycle_default(&c->owner_lifecycle,
                                                    a04_bq_mt8167_reset_default,&c->adapter);
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
            a04_bq_d0_default_result(&c->d0_session,result);
#endif
            trace(c, result ? "A04BAT DEFAULT UNCONFIRMED\n" : "A04BAT DEFAULT WRITE OK\n");
            trace_owner(c,"POST_RESET_CACHED");
#else
            a04_broker_lifecycle_stop(&c->owner_lifecycle);
#endif
        } else if (a04_broker_lifecycle_handoff(&c->owner_lifecycle,next,transfer,ops,deadline))
            a04_broker_lifecycle_stop(&c->owner_lifecycle);
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
        c->faulted=!a04_bq_d0_can_resume(&c->d0_session);
#else
        c->faulted=TRUE;
#endif
    }
#endif
#if A04_BROKER_CLIENT_EXPERIMENTAL
    a04_broker_wdf_stop(&c->broker);
#endif
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
    /* BQ13 stays terminal. The local D0 diagnostic may create a fresh epoch
     * only after this object's exact DEFAULT write completed successfully. */
    if (c->charger.attempts || c->charger.kicks || c->charger.handoff_pending) {
        c->charger.stopped = 1;
#if A04_BQ_D0_DEFAULT_DIAGNOSTIC
        if (!a04_bq_d0_can_resume(&c->d0_session)) c->faulted=TRUE;
#else
        c->faulted = TRUE;
#endif
    }
#endif
    WdfWaitLockRelease(c->bus_lock);
    if (a04_battery_invalidate(&c->battery)) notify(c);
    return STATUS_SUCCESS;
}
VOID GaugePoll(WDFTIMER timer)
{
    GAUGE_CONTEXT *c = GaugeContext((WDFDEVICE)WdfTimerGetParentObject(timer));
    poll(c);
#if A04_BQ24160_EXPERIMENTAL && !A04_BROKER_CLIENT_EXPERIMENTAL
#if A04_BROKER_OWNER_EXPERIMENTAL && A04_BQ_STOPPED_GAUGE_DIAGNOSTIC
    if (c->online && c->owner_fallback) WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(30000));
    else
#endif
    if (c->online) WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_MS(10000));
#else
    if (c->online) WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_MS(30000));
#endif
}
NTSTATUS GaugeClassInit(WDFDEVICE device)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    BATTERY_MINIPORT_INFO_V1_1 info;
    NTSTATUS status;
    RtlZeroMemory(&info, sizeof(info));
    info.MajorVersion = BATTERY_CLASS_MAJOR_VERSION;
    info.MinorVersion = BATTERY_CLASS_MINOR_VERSION_1;
    info.Context = &c->battery;
    info.QueryTag = a04_battery_query_tag;
    info.QueryInformation = a04_battery_query_information;
    info.SetInformation = a04_battery_set_information;
    info.QueryStatus = a04_battery_query_status;
    info.SetStatusNotify = a04_battery_set_notify;
    info.DisableStatusNotify = a04_battery_disable_notify;
    info.Pdo = WdfDeviceWdmGetPhysicalDevice(device);
    info.Fdo = WdfDeviceWdmGetDeviceObject(device);
    WdfWaitLockAcquire(c->class_lock, NULL);
    status = BatteryClassInitializeDevice((PBATTERY_MINIPORT_INFO)&info, &c->class_handle);
    WdfWaitLockRelease(c->class_lock);
    trace(c, NT_SUCCESS(status) ? "A04BAT CLASS STARTED\n" : "A04BAT CLASS FAILED\n");
    return status;
}
VOID GaugeClassCleanup(WDFDEVICE device)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    WdfWaitLockAcquire(c->class_lock, NULL);
    if (c->class_handle) { BatteryClassUnload(c->class_handle); c->class_handle = NULL; }
    WdfWaitLockRelease(c->class_lock);
}
NTSTATUS GaugeIrp(WDFDEVICE device, PIRP irp)
{
    GAUGE_CONTEXT *c = GaugeContext(device);
    NTSTATUS status = STATUS_NOT_SUPPORTED;
    WdfWaitLockAcquire(c->class_lock, NULL);
    if (c->class_handle) status = BatteryClassIoctl(c->class_handle, irp);
    WdfWaitLockRelease(c->class_lock);
    if (status == STATUS_NOT_SUPPORTED) {
        IoSkipCurrentIrpStackLocation(irp);
        status = WdfDeviceWdmDispatchPreprocessedIrp(device, irp);
    }
    return status;
}
NTSTATUS GaugeAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES attributes, child;
    WDF_TIMER_CONFIG timer;
    WDFDEVICE device;
    GAUGE_CONTEXT *c;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = GaugePrepare; pnp.EvtDeviceReleaseHardware = GaugeRelease;
    pnp.EvtDeviceD0Entry = GaugeD0Entry; pnp.EvtDeviceD0Exit = GaugeD0Exit;
    pnp.EvtDeviceSelfManagedIoInit = GaugeClassInit; pnp.EvtDeviceSelfManagedIoCleanup = GaugeClassCleanup;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    status = WdfDeviceInitAssignWdmIrpPreprocessCallback(init, GaugeIrp, IRP_MJ_DEVICE_CONTROL, NULL, 0);
    if (!NT_SUCCESS(status)) return status;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, GAUGE_CONTEXT);
    attributes.ExecutionLevel = WdfExecutionLevelPassive;
    status = WdfDeviceCreate(&init, &attributes, &device);
    if (!NT_SUCCESS(status)) return status;
    c = GaugeContext(device); c->device = device;
    WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject = device;
    status = WdfWaitLockCreate(&child, &c->class_lock);
    if (!NT_SUCCESS(status)) return status;
    status = WdfWaitLockCreate(&child, &c->bus_lock);
    if (!NT_SUCCESS(status)) return status;
#if A04_BROKER_CLIENT_EXPERIMENTAL
    status = WdfWaitLockCreate(&child, &c->broker_lock);
    if (!NT_SUCCESS(status)) return status;
#endif
    status = WdfWaitLockCreate(&child, &c->battery.lock);
    if (!NT_SUCCESS(status)) return status;
    /* KMDF passive-level timers cannot be periodic. */
    WDF_TIMER_CONFIG_INIT(&timer, GaugePoll);
    /* D0Exit synchronously stops this timer; do not serialize it under a
     * device callback lock that D0Exit may already hold. */
    timer.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject = device; child.ExecutionLevel = WdfExecutionLevelPassive;
    status=WdfTimerCreate(&timer, &child, &c->timer);
    if (!NT_SUCCESS(status)) return status;
#if A04_BROKER_OWNER_EXPERIMENTAL && !A04_BQ_D0_DEFAULT_DIAGNOSTIC
    /* Register only an unbound, non-operational endpoint. It rejects every
     * request until a separately proven owner is adopted and initialized. */
    status=a04_broker_wdf_register_receiver(device,&c->owner_endpoint,c->bus_lock,
                                             owner_broker_failed,c,&c->owner_queue);
    if (!NT_SUCCESS(status)) return status;
#endif
    return STATUS_SUCCESS;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, GaugeAdd);
    return WdfDriverCreate(driver, registry, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}
