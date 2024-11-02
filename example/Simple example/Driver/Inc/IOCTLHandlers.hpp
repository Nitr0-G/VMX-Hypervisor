#pragma once
#include "Driver/Inc/CommonApi/ProcessesUtils.hpp"
#include "Driver/Inc/Hpv.hpp"
#include "Driver/Inc/Cr3Get.hpp"
#include "Shared/IOCTLs.hpp"
#include "Shared/CtlTypes.hpp"
#include "CommonApi/MemoryUtils.hpp"
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