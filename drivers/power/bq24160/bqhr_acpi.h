/* SPDX-License-Identifier: MIT */
#ifndef A04_BQHR_ACPI_H
#define A04_BQHR_ACPI_H
#include <ntddk.h>
#include <wdf.h>
enum a04_bqhr_acpi_result { A04_BQHR_ACPI_IOCTL_ERROR, A04_BQHR_ACPI_INVALID_OUTPUT,
    A04_BQHR_ACPI_INVALID_RECORD, A04_BQHR_ACPI_STOPPED, A04_BQHR_ACPI_OTHER_STATE };
/* Output is cleared on failure. Successful transport and a valid seal do not
 * prove ownership, freshness, counter continuity, or permission to access BQ. */
enum a04_bqhr_acpi_result a04_bqhr_receive(WDFDEVICE device, NTSTATUS *io_status,
                                           UCHAR record[128]);
enum a04_bqhr_acpi_result a04_bqhr_evaluate(WDFDEVICE device, NTSTATUS *io_status);
#endif
