#include "IOCTLRequests.hpp"

namespace PhysicalMemory
{
    BOOL WINAPI MvTranslateProcessVirtualAddrToPhysicalAddr(
        IN UINT32 ProcessId,
        IN WdkTypes::PVOID VirtualAddress,
        OUT WdkTypes::PVOID* PhysicalAddress
    )
    {
        if (!PhysicalAddress) return FALSE;
        MV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN Input = {};
        MV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT Output = {};
        Input.ProcessId = ProcessId;
        Input.VirtualAddress = VirtualAddress;
        BOOL Status = MvSendRequest(Ctls::MvTranslateProcessVirtualAddrToPhysicalAddr, &Input, sizeof(Input), &Output, sizeof(Output));
        *PhysicalAddress = Output.PhysicalAddress;
        return Status;
    }

    BOOL WINAPI MvNativeTranslateProcessVirtualAddrToPhysicalAddr(
        IN UINT64 Cr3,
        IN WdkTypes::PVOID VirtualAddress,
        OUT WdkTypes::PVOID* PhysicalAddress
    )
    {
        if (!PhysicalAddress) return FALSE;
        MV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN Input = {};
        MV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT Output = {};
        Input.Cr3 = Cr3;
        Input.VirtualAddress = VirtualAddress;
        BOOL Status = MvSendRequest(Ctls::MvNativeTranslateProcessVirtualAddrToPhysicalAddr, &Input, sizeof(Input), &Output, sizeof(Output));
        *PhysicalAddress = Output.PhysicalAddress;
        return Status;
    }
}

namespace Process
{
    BOOL WINAPI MvGetProcessCr3(
        ULONG ProcessId,
        OUT PUINT64 Cr3,
        Cr3GetMode Mode
    )
    {
        if (!ProcessId) return FALSE;
        MV_GET_PROCESS_CR3_IN Input = {};
        MV_GET_PROCESS_CR3_OUT Output = {};
        Input.ProcessId = ProcessId;
        Input.Mode = Mode;
        BOOL Status = MvSendRequest(Ctls::MvGetProcessCr3, &Input, sizeof(Input), &Output, sizeof(Output));
        if (Cr3) *Cr3 = Output.Cr3;
        return Status;
    }

    BOOL WINAPI MvGetEProcess(
        ULONG ProcessId,
        OUT PVOID Eprocess
    )
    {
        if (!ProcessId) return FALSE;
        MV_GET_EPROCESS_IN Input = {};
        MV_GET_EPROCESS_OUT Output = {};
        Input.ProcessId = ProcessId;
        BOOL Status = MvSendRequest(Ctls::MvGetProcessCr3, &Input, sizeof(Input), &Output, sizeof(Output));
        Eprocess = Output.EPROCESS;
        return Status;
    }
}

namespace HyperVisor
{
    BOOL WINAPI MvVmmEnable()
    {
        return MvSendRequest(Ctls::MvVmmEnable);
    }

    BOOL WINAPI MvVmmDisable()
    {
        return MvSendRequest(Ctls::MvVmmDisable);
    }

    BOOL WINAPI MvVmmInterceptPage(
        IN OPTIONAL WdkTypes::PVOID64 PhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnReadPhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnWritePhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnExecutePhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnExecuteReadPhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnExecuteWritePhysicalAddress
    )
    {
        MV_VMM_INTERCEPT_PAGE_IN Input = {};
        Input.PhysicalAddress = PhysicalAddress;
        Input.OnReadPhysicalAddress = OnReadPhysicalAddress;
        Input.OnWritePhysicalAddress = OnWritePhysicalAddress;
        Input.OnExecutePhysicalAddress = OnExecutePhysicalAddress;
        Input.OnExecuteReadPhysicalAddress = OnExecuteReadPhysicalAddress;
        Input.OnExecuteWritePhysicalAddress = OnExecuteWritePhysicalAddress;
        return MvSendRequest(Ctls::MvVmmInterceptPage, &Input, sizeof(Input));
    }

    BOOL WINAPI MvVmmDeinterceptPage(
        IN OPTIONAL WdkTypes::PVOID64 PhysicalAddress
    )
    {
        MV_VMM_DEINTERCEPT_PAGE_IN Input = {};
        Input.PhysicalAddress = PhysicalAddress;
        return MvSendRequest(Ctls::MvVmmDeinterceptPage, &Input, sizeof(Input));
    }

    BOOL WINAPI MvVmmTraceProcess(
        UINT64 Cr3,
        PVOID AddrStart,
        PVOID AddrEnd,
        PMV_VMM_TRACE_PROCESS_OUT pOutput
    )
    {
        MV_VMM_TRACE_PROCESS_IN Input = {};
        MV_VMM_TRACE_PROCESS_OUT Output = {};
        Input.Cr3 = Cr3;
        Input.AddrStart = AddrStart;
        Input.AddrEnd = AddrEnd;
        BOOL Status = MvSendRequest(Ctls::MvVmmTraceProcess, &Input, sizeof(Input), &Output, sizeof(Output));
        pOutput = &Output;
        return Status;
    }

    BOOL WINAPI MvVmmInitTraceProcess(
        UINT64 Cr3,
        PVOID AddrStart,
        PVOID AddrEnd
    )
    {
        MV_VMM_TRACE_PROCESS_IN Input = {};
        MV_VMM_TRACE_PROCESS_OUT Output = {};
        Input.Cr3 = Cr3;
        Input.AddrStart = AddrStart;
        Input.AddrEnd = AddrEnd;
        BOOL Status = MvSendRequest(Ctls::MvVmmInitTraceProcess, &Input, sizeof(Input));
        return Status;
    }
}