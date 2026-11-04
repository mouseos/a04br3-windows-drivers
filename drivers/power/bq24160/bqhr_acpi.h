/* SPDX-License-Identifier: MIT */
#ifndef A04_BQHR_ACPI_H
#define A04_BQHR_ACPI_H
#include <ntddk.h>
#include <wdf.h>
enum a04_bqhr_acpi_result { A04_BQHR_ACPI_IOCTL_ERROR, A04_BQHR_ACPI_INVALID_OUTPUT,
    A04_BQHR_ACPI_INVALID_RECORD, A04_BQHR_ACPI_STOPPED, A04_BQHR_ACPI_OTHER_STATE };
enum a04_bqhr_acpi_result a04_bqhr_evaluate(WDFDEVICE device, NTSTATUS *io_status);
#endif
