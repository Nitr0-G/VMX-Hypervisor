#pragma once
#include "Shared/IOCTLs.hpp"
#include "Driver/Inc/Hpv.hpp"
#include "Shared/CtlTypes.hpp"
#include <fltKernel.h>

typedef struct _IOCTL_INFO {
    PVOID InputBuffer;
    PVOID OutputBuffer;
    ULONG InputBufferSize;
    ULONG OutputBufferSize;
    ULONG ControlCode;
} IOCTL_INFO, * PIOCTL_INFO;

NTSTATUS FASTCALL DispatchIOCTL(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength);
NTSTATUS DriverControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);