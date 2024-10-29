#pragma once
#include "GuestContext.hpp"
#include "include/HyperVisor/HyperVisor.hpp"

//namespace VmExit {
//    void InitHandlersTable();
//}
//
//void Interceptions(
//    _In_ Intel::IA32_VMX_BASIC VmxBasicInfo);
//
////Define in asm file(in my example)
//extern "C" void VmxVmmRun(_In_ void* InitialVmmStackPointer);

namespace HyperVisor {
    bool IsVirtualized();
    bool Virtualize();
    bool Devirtualize();
}