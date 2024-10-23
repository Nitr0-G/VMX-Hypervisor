#include "AsmFuncs.hpp"

extern "C" void ASMsldt(SEGMENT_SELECTOR* Selector);
extern "C" void ASMstr(__out SEGMENT_SELECTOR* TaskRegister);
extern "C" void ASMinvd();

/* VMX-only */ extern "C" void ASMinvept(unsigned int Type, __in void* Descriptor);
/* VMX-only */ extern "C" void ASMinvvpid(unsigned int Type, __in void* Descriptor);

// For the Hyper-V only:
extern "C" unsigned long long __fastcall ASMhyperv_vmcall(
    unsigned long long Rcx,
    unsigned long long Rdx,
    unsigned long long R8,
    unsigned long long R9
);

extern "C" unsigned long long __fastcall ASMkb_vmcall(
    unsigned long long Rcx,
    unsigned long long Rdx,
    unsigned long long R8,
    unsigned long long R9
);

void _sldt(__out SEGMENT_SELECTOR* Selector)
{
    return ASMsldt(Selector);
}

void _str(__out SEGMENT_SELECTOR* TaskRegister)
{
    return ASMstr(TaskRegister);
}

void __invd()
{
    return ASMinvd();
}

void __invept(unsigned int Type, __in void* Descriptor)
{
    return ASMinvept(Type, Descriptor);
}

void __invvpid(unsigned int Type, __in void* Descriptor)
{
    return ASMinvvpid(Type, Descriptor);
}

unsigned long long __fastcall __hyperv_vmcall(
    unsigned long long Rcx,
    unsigned long long Rdx,
    unsigned long long R8,
    unsigned long long R9)
{
    return ASMhyperv_vmcall(Rcx, Rdx, R8, R9);
}

unsigned long long __fastcall __kb_vmcall(
    unsigned long long Rcx,
    unsigned long long Rdx,
    unsigned long long R8,
    unsigned long long R9)
{
    return ASMkb_vmcall(Rcx, Rdx, R8, R9);
}


unsigned long long VMCALLS::VmmCall(unsigned long long(*Fn)(void* Arg), void* Arg, bool SwitchToCallerAddressSpace)
{
    return __kb_vmcall(
        static_cast<unsigned long long>(VMCALL_INDEX::VmmCall),
        reinterpret_cast<unsigned long long>(Fn),
        reinterpret_cast<unsigned long long>(Arg),
        static_cast<unsigned long long>(SwitchToCallerAddressSpace)
    );
}