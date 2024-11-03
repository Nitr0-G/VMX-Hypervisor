#include "Driver/Inc/IOCTLHandlers.hpp"

extern volatile LONG KbHandlesCount;

namespace HyperVisor
{
    namespace TraceProcess
    {
        extern bool RepeatMtfExit;
        extern bool InMtfExit;

        extern PMV_VMM_TRACE_PROCESS_OUT Out;
    }
}

namespace IOCTLFuncs
{
    NTSTATUS FASTCALL MvVmmEnable(_In_ PIOCTL_INFO RequestInfo, _Out_ PSIZE_T ResponseLength)
    {
        UNREFERENCED_PARAMETER(RequestInfo);
        UNREFERENCED_PARAMETER(ResponseLength);
        return HyperVisor::Virtualize() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
    }

    NTSTATUS FASTCALL MvVmmDisable(_In_ PIOCTL_INFO RequestInfo, _Out_ PSIZE_T ResponseLength)
    {
        UNREFERENCED_PARAMETER(RequestInfo);
        UNREFERENCED_PARAMETER(ResponseLength);
        return HyperVisor::Devirtualize() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
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

    NTSTATUS FASTCALL MvTranslateProcessVirtualAddrToPhysicalAddr(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        if (
            RequestInfo->InputBufferSize != sizeof(MV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN) ||
            RequestInfo->OutputBufferSize != sizeof(MV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT)
            ) return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN>(RequestInfo->InputBuffer);
        auto Output = static_cast<PMV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT>(RequestInfo->OutputBuffer);

        if (!Input || !Output) return STATUS_INVALID_PARAMETER;

        PEPROCESS Process = Processes::Descriptors::GetEPROCESS(reinterpret_cast<HANDLE>(Input->ProcessId));

        Output->PhysicalAddress = reinterpret_cast<WdkTypes::PVOID>(
            PhysicalMemory::GetPhysicalAddress(
                reinterpret_cast<PVOID>(Input->VirtualAddress),
                reinterpret_cast<PEPROCESS>(Process)
            )
            );

        *ResponseLength = RequestInfo->OutputBufferSize;

        return Output->PhysicalAddress
            ? STATUS_SUCCESS
            : STATUS_UNSUCCESSFUL;
    }

    NTSTATUS FASTCALL MvVmmInterceptPage(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        UNREFERENCED_PARAMETER(ResponseLength);

        if (RequestInfo->InputBufferSize != sizeof(MV_VMM_INTERCEPT_PAGE_IN))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_VMM_INTERCEPT_PAGE_IN>(RequestInfo->InputBuffer);
        bool Status = HyperVisor::InterceptPage(
            Input->PhysicalAddress,
            Input->OnReadPhysicalAddress,
            Input->OnWritePhysicalAddress,
            Input->OnExecutePhysicalAddress,
            Input->OnExecuteReadPhysicalAddress,
            Input->OnExecuteWritePhysicalAddress
        );

        return Status ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
    }

    NTSTATUS FASTCALL MvVmmDeinterceptPage(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        UNREFERENCED_PARAMETER(ResponseLength);

        if (RequestInfo->InputBufferSize != sizeof(MV_VMM_DEINTERCEPT_PAGE_IN))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_VMM_DEINTERCEPT_PAGE_IN>(RequestInfo->InputBuffer);
        bool Status = HyperVisor::DeinterceptPage(Input->PhysicalAddress);

        return Status ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
    }

    NTSTATUS FASTCALL MvVmmTraceProcess(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        if (
            RequestInfo->InputBufferSize != sizeof(MV_VMM_TRACE_PROCESS_IN) ||
            RequestInfo->OutputBufferSize != sizeof(MV_VMM_TRACE_PROCESS_OUT)
            ) return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_VMM_TRACE_PROCESS_IN>(RequestInfo->InputBuffer);
        auto Output = static_cast<PMV_VMM_TRACE_PROCESS_OUT>(RequestInfo->OutputBuffer);

        if (!Input || !Output || !Input->Cr3 && !Input->AddrStart && !Input->AddrEnd) return STATUS_INVALID_PARAMETER;

        NTSTATUS Status = STATUS_SUCCESS;

        //if (!HyperVisor::TraceProcess::InMtfExit)
        //{
        //    //Status = HyperVisor::Trace(Input, Output) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        //}
        //else
        //{
        //    HyperVisor::TraceProcess::RepeatMtfExit = true;
        //    HyperVisor::TraceProcess::InMtfExit = false;
        //}

        for (;;)
        {
            if (HyperVisor::TraceProcess::InMtfExit) 
            { 
                RtlCopyMemory(Output, HyperVisor::TraceProcess::Out, sizeof(MV_VMM_TRACE_PROCESS_OUT));
                RtlZeroMemory(HyperVisor::TraceProcess::Out, sizeof(MV_VMM_TRACE_PROCESS_OUT));

                HyperVisor::TraceProcess::RepeatMtfExit = true;
                HyperVisor::TraceProcess::InMtfExit = false;
                
                break;
            }
        }

        *ResponseLength = RequestInfo->OutputBufferSize;
        return Status;
    }

    NTSTATUS FASTCALL MvVmmInitTraceProcess(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        if (
            RequestInfo->InputBufferSize != sizeof(MV_VMM_TRACE_PROCESS_IN)
            ) return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_VMM_TRACE_PROCESS_IN>(RequestInfo->InputBuffer);

        if (!Input || !Input->Cr3 && !Input->AddrStart && !Input->AddrEnd) return STATUS_INVALID_PARAMETER;

        NTSTATUS Status = HyperVisor::InitTrace(Input) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

        *ResponseLength = RequestInfo->OutputBufferSize;
        return Status;
    }

    NTSTATUS FASTCALL MvGetProcessCr3(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        if (
            RequestInfo->InputBufferSize != sizeof(MV_GET_PROCESS_CR3_IN) ||
            RequestInfo->OutputBufferSize != sizeof(MV_GET_PROCESS_CR3_OUT)
            ) return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_GET_PROCESS_CR3_IN>(RequestInfo->InputBuffer);
        auto Output = static_cast<PMV_GET_PROCESS_CR3_OUT>(RequestInfo->OutputBuffer);

        if (!Input || !Output || !Input->ProcessId) return STATUS_INVALID_PARAMETER;

        PEPROCESS Process = Processes::Descriptors::GetEPROCESS(reinterpret_cast<HANDLE>(Input->ProcessId));
        if (!Process) return STATUS_NOT_FOUND;

        Output->Cr3 = Cr3Getter::GetProcessCr3(Process, Input->Mode);

        *ResponseLength = RequestInfo->OutputBufferSize;
        return STATUS_SUCCESS;
    }

    NTSTATUS FASTCALL MvGetEprocess(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        if (
            RequestInfo->InputBufferSize != sizeof(MV_GET_EPROCESS_IN) ||
            RequestInfo->OutputBufferSize != sizeof(MV_GET_EPROCESS_OUT)
            ) return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_GET_EPROCESS_IN>(RequestInfo->InputBuffer);
        auto Output = static_cast<PMV_GET_EPROCESS_OUT>(RequestInfo->OutputBuffer);

        if (!Input || !Output || !Input->ProcessId) return STATUS_INVALID_PARAMETER;

        PEPROCESS Process = Processes::Descriptors::GetEPROCESS(reinterpret_cast<HANDLE>(Input->ProcessId));
        if (!Process) return STATUS_NOT_FOUND;

        Output->EPROCESS = (PVOID)Process;
        *ResponseLength = RequestInfo->OutputBufferSize;
        return STATUS_SUCCESS;
    }

    NTSTATUS FASTCALL MvNativeTranslateProcessVirtualAddrToPhysicalAddr(IN PIOCTL_INFO RequestInfo, OUT PSIZE_T ResponseLength)
    {
        if (
            RequestInfo->InputBufferSize != sizeof(MV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN) ||
            RequestInfo->OutputBufferSize != sizeof(MV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT)
            ) return STATUS_INFO_LENGTH_MISMATCH;

        auto Input = static_cast<PMV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN>(RequestInfo->InputBuffer);
        auto Output = static_cast<PMV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT>(RequestInfo->OutputBuffer);

        if (!Input || !Output || !Input->Cr3 && !Input->VirtualAddress) return STATUS_INVALID_PARAMETER;

        UINT64 PhysicalAddress = PhysicalMemory::NativeGetPhysicalAddress(Input->Cr3, Input->VirtualAddress);
        if (!PhysicalAddress) return STATUS_NOT_FOUND;

        Output->PhysicalAddress = PhysicalAddress;
        *ResponseLength = RequestInfo->OutputBufferSize;
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
        IOCTLFuncs::MvVmmInterceptPage,
        IOCTLFuncs::MvVmmDeinterceptPage,
        IOCTLFuncs::MvVmmTraceProcess,
        IOCTLFuncs::MvVmmInitTraceProcess,

        // Driver management:
        IOCTLFuncs::MvGetHandlesCount,

        // Physical memory:
        IOCTLFuncs::MvTranslateProcessVirtualAddrToPhysicalAddr,
        IOCTLFuncs::MvNativeTranslateProcessVirtualAddrToPhysicalAddr,

        // Processes & Threads:
        IOCTLFuncs::MvGetProcessCr3,
        IOCTLFuncs::MvGetEprocess
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