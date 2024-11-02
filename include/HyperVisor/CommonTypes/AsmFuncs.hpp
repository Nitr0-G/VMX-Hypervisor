#pragma once
#include <sal.h>
#include "Segmentation.hpp"

// Defined in AsmFuncs.asm:
void _sldt(__out SEGMENT_SELECTOR* Selector);
void _str(__out SEGMENT_SELECTOR* TaskRegister);
void __invd();

/* VMX-only */ void __invept(unsigned int Type, __in void* Descriptor);
/* VMX-only */ void __invvpid(unsigned int Type, __in void* Descriptor);

// For the Hyper-V only:
unsigned long long __fastcall __hyperv_vmcall(
    unsigned long long Rcx,
    unsigned long long Rdx,
    unsigned long long R8,
    unsigned long long R9
);

unsigned long long __fastcall __kb_vmcall(
    unsigned long long Rcx,
    unsigned long long Rdx,
    unsigned long long R8,
    unsigned long long R9
);

namespace VMCALLS
{
    enum class VMCALL_INDEX
    {
        VmmCall,
        MtfEnable
    };

    unsigned long long VmmCall(unsigned long long(*Fn)(void* Arg), void* Arg, bool SwitchToCallerAddressSpace = false);
}