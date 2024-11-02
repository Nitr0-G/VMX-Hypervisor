#pragma once
#include <Windows.h>

#include "SendIOCTLs.hpp"
#include "../../Shared/CtlTypes.hpp"

namespace PhysicalMemory
{
    BOOL WINAPI MvTranslateProcessVirtualAddrToPhysicalAddr(
        IN OPTIONAL WdkTypes::PEPROCESS Process,
        IN WdkTypes::PVOID VirtualAddress,
        OUT WdkTypes::PVOID* PhysicalAddress
    );

    BOOL WINAPI MvNativeTranslateProcessVirtualAddrToPhysicalAddr(
        IN UINT64 Cr3,
        IN WdkTypes::PVOID VirtualAddress,
        OUT WdkTypes::PVOID* PhysicalAddress
    );
}

namespace Process
{
    BOOL WINAPI MvGetProcessCr3(
        ULONG ProcessId,
        OUT OPTIONAL PUINT64 Cr3,
        Cr3GetMode Mode
    );
}

namespace HyperVisor
{
    BOOL WINAPI MvVmmEnable();

    BOOL WINAPI MvVmmDisable();

    BOOL WINAPI MvVmmInterceptPage(
        IN OPTIONAL WdkTypes::PVOID64 PhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnReadPhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnWritePhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnExecutePhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnExecuteReadPhysicalAddress,
        IN OPTIONAL WdkTypes::PVOID64 OnExecuteWritePhysicalAddress
    );

    BOOL WINAPI MvVmmDeinterceptPage(
        IN OPTIONAL WdkTypes::PVOID64 PhysicalAddress
    );

    BOOL WINAPI MvVmmTraceProcess(
        UINT64 Cr3,
        PVOID AddrStart,
        PVOID AddrEnd,
        PMV_VMM_TRACE_PROCESS_OUT pOutput
    );
}