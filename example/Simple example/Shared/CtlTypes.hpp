#pragma once
#include "WdkTypes.hpp"

namespace Ctls
{
    enum CtlIndices {
        // Hypervisor:
        /* 0 */ MvVmmEnable,
        /* 1 */ MvVmmDisable,
        /* 2 */ MvVmmInterceptPage,
        /* 3 */ MvVmmDeinterceptPage,
        /* 4 */ MvVmmTraceProcess,

        // Driver management:
        /* 5 */ MvGetHandlesCount,

        // Physical memory:
        /* 6 */ MvTranslateProcessVirtualAddrToPhysicalAddr,
        /* 7 */ MvNativeTranslateProcessVirtualAddrToPhysicalAddr,

        // Processes & Threads:
        /* 8 */ MvGetProcessCr3,
        /* 9 */ MvGetEprocess,
    };
}

DECLARE_STRUCT(MV_GET_HANDLES_COUNT_OUT, {
    unsigned long HandlesCount;
});

DECLARE_STRUCT(MV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN, {
    WdkTypes::PEPROCESS Process;
    WdkTypes::PVOID VirtualAddress;
    });

DECLARE_STRUCT(MV_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT, {
    WdkTypes::PVOID PhysicalAddress;
    });

DECLARE_STRUCT(MV_VMM_INTERCEPT_PAGE_IN, {
    IN WdkTypes::PVOID64 PhysicalAddress;
    IN OPTIONAL WdkTypes::PVOID64 OnReadPhysicalAddress;
    IN OPTIONAL WdkTypes::PVOID64 OnWritePhysicalAddress;
    IN OPTIONAL WdkTypes::PVOID64 OnExecutePhysicalAddress;
    IN OPTIONAL WdkTypes::PVOID64 OnExecuteReadPhysicalAddress;
    IN OPTIONAL WdkTypes::PVOID64 OnExecuteWritePhysicalAddress;
    });

DECLARE_STRUCT(MV_VMM_DEINTERCEPT_PAGE_IN, {
    IN WdkTypes::PVOID64 PhysicalAddress;
    });

DECLARE_STRUCT(MV_VMM_TRACE_PROCESS_IN, {
    UINT64 Cr3;
    PVOID AddrStart;
    PVOID AddrEnd;
    });

DECLARE_STRUCT(MV_VMM_TRACE_PROCESS_OUT, {
    UINT8 Opcodes[MAX_INSTRUCTION_LENGTH];
    UINT32 ThreadId;
    UINT32 InstrLength;
    PVOID Rip;
    });

DECLARE_STRUCT(MV_GET_PROCESS_CR3_IN, {
    UINT64 ProcessId;
    Cr3GetMode Mode;
    });

DECLARE_STRUCT(MV_GET_PROCESS_CR3_OUT, {
    UINT64 Cr3;
    });

DECLARE_STRUCT(MV_GET_EPROCESS_IN, {
    UINT64 ProcessId;
    });

DECLARE_STRUCT(MV_GET_EPROCESS_OUT, {
    PVOID EPROCESS;
    });

DECLARE_STRUCT(MV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_IN, {
    UINT64 Cr3;
    UINT64 VirtualAddress;
    });

DECLARE_STRUCT(MV_NATIVE_TRANSLATE_PROCESS_VIRTUAL_ADDRESS_TO_PHYSICAL_ADDRESS_OUT, {
    UINT64 PhysicalAddress;
    });