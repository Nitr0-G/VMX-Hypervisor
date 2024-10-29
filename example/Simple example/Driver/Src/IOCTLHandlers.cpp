#include "Driver/Inc/IOCTLHandlers.hpp"

extern volatile LONG KbHandlesCount;

namespace IOCTLFuncs
{
    NTSTATUS FASTCALL MvVmmEnable(_In_ PIOCTL_INFO RequestInfo, _Out_ PSIZE_T ResponseLength)
    {
        UNREFERENCED_PARAMETER(RequestInfo);
        UNREFERENCED_PARAMETER(ResponseLength);
        return HyperVisor::Virtualize() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        //return STATUS_SUCCESS;
    }

    NTSTATUS FASTCALL MvVmmDisable(_In_ PIOCTL_INFO RequestInfo, _Out_ PSIZE_T ResponseLength)
    {
        UNREFERENCED_PARAMETER(RequestInfo);
        UNREFERENCED_PARAMETER(ResponseLength);
        return HyperVisor::Devirtualize() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        //return STATUS_SUCCESS;
    }

    NTSTATUS FASTCALL MvGetHandlesCount(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        if (RequestInfo->OutputBufferSize != sizeof(MV_GET_HANDLES_COUNT_OUT))
            return STATUS_INFO_LENGTH_MISMATCH;
        auto Output = reinterpret_cast<PMV_GET_HANDLES_COUNT_OUT>(RequestInfo->OutputBuffer);
        if (!Output) return STATUS_INVALID_PARAMETER;
        Output->HandlesCount = KbHandlesCount;
        *ResponseLength = sizeof(*Output);
        return STATUS_SUCCESS;
    }
}

NTSTATUS FASTCALL DispatchIOCTL(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
{
    using _CtlHandler = NTSTATUS(FASTCALL*)(IN PIOCTL_INFO, OUT PSIZE_T);
    static const _CtlHandler Handlers[] = {
        // Hypervisor:
        IOCTLFuncs::MvVmmEnable,
        IOCTLFuncs::MvVmmDisable,

        // Driver management:
        IOCTLFuncs::MvGetHandlesCount
    };

    USHORT Index = EXTRACT_CTL_CODE(RequestInfo->ControlCode) - CTL_BASE;
    return Index < sizeof(Handlers) / sizeof(Handlers[0])
        ? Handlers[Index](RequestInfo, ResponseLength)
        : STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS CallIoctlDispatcher(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
{
    ULONG ExceptionCode = 0;
    PEXCEPTION_POINTERS ExceptionPointers = NULL;
    NTSTATUS Status = DispatchIOCTL(RequestInfo, ResponseLength);
    if (Status == STATUS_UNSUCCESSFUL)
    {
        KdPrint((
            "[Marius-VMX]: STATUS_UNSUCCESSFUL catched in IOCTL handler!\r\n"
            "\tCode: 0x%X\r\n"
            "\tAddress: 0x%p\r\n"
            "\tCTL: 0x%X\r\n",
            ExceptionCode,
            ExceptionPointers->ExceptionRecord->ExceptionAddress,
            RequestInfo->ControlCode
            ));
    }

    return Status;
}

NTSTATUS DriverControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);

    IOCTL_INFO RequestInfo;
    RequestInfo.ControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;
    switch (EXTRACT_CTL_METHOD(RequestInfo.ControlCode)) {
    case METHOD_BUFFERED: {
        RequestInfo.InputBuffer = Irp->AssociatedIrp.SystemBuffer;
        RequestInfo.OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
        RequestInfo.InputBufferSize = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
        RequestInfo.OutputBufferSize = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
        Irp->IoStatus.Status = CallIoctlDispatcher(&RequestInfo, &Irp->IoStatus.Information);
        break;
    }
    case METHOD_NEITHER: {
        RequestInfo.InputBuffer = IrpStack->Parameters.DeviceIoControl.Type3InputBuffer;
        RequestInfo.OutputBuffer = Irp->UserBuffer;
        RequestInfo.InputBufferSize = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
        RequestInfo.OutputBufferSize = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
        Irp->IoStatus.Status = CallIoctlDispatcher(&RequestInfo, &Irp->IoStatus.Information);
        break;
    }
    case METHOD_IN_DIRECT:
    case METHOD_OUT_DIRECT: {
        RequestInfo.InputBuffer = Irp->AssociatedIrp.SystemBuffer;
        RequestInfo.OutputBuffer = Irp->MdlAddress ? MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority) : NULL;
        RequestInfo.InputBufferSize = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
        RequestInfo.OutputBufferSize = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
        Irp->IoStatus.Status = CallIoctlDispatcher(&RequestInfo, &Irp->IoStatus.Information);
        break;
    }
    default: {
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        KdPrint(("[Marius-VMX]: Unknown method of IRP!\r\n"));
    }
    }

    NTSTATUS Status = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}