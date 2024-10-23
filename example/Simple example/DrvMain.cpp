#include "GuestContext.hpp"
#include "include/HyperVisor/HyperVisor.hpp"

HyperVisorVmx objHyperVisorVmx;
extern VMX::SHARED_VM_DATA g_Shared;


void DriverUnload(_In_ PDRIVER_OBJECT DriverObj)
{
	UNREFERENCED_PARAMETER(DriverObj);
	KdPrint(("Sample driver Unload called\n"));
}

void InjectEvent(VMX::INTERRUPTION_TYPE Type, INTERRUPT_VECTOR Vector, bool DeliverErrorCode, unsigned int ErrorCode)
{
    VMX::VMENTRY_INTERRUPTION_INFORMATION Event = {};
    Event.Bitmap.VectorOfInterruptOrException = static_cast<unsigned int>(Vector) & 0xFF;
    Event.Bitmap.InterruptionType = static_cast<unsigned int>(Type) & 0b111;
    Event.Bitmap.DeliverErrorCode = DeliverErrorCode;
    Event.Bitmap.Valid = TRUE;
    __vmx_vmwrite(VMX::VMCS_FIELD_VMENTRY_INTERRUPTION_INFORMATION_FIELD, Event.Value);

    if (DeliverErrorCode)
    {
        __vmx_vmwrite(VMX::VMCS_FIELD_VMENTRY_EXCEPTION_ERROR_CODE, ErrorCode);
    }
}

void InjectMonitorTrapFlagVmExit()
{
    // It is a special case of events injection:
    VMX::VMENTRY_INTERRUPTION_INFORMATION Event = {};
    Event.Bitmap.VectorOfInterruptOrException = 0;
    Event.Bitmap.InterruptionType = static_cast<unsigned int>(VMX::INTERRUPTION_TYPE::OtherEvent);
    Event.Bitmap.Valid = TRUE;
    __vmx_vmwrite(VMX::VMCS_FIELD_VMENTRY_INTERRUPTION_INFORMATION_FIELD, Event.Value);
}

void EnableMonitorTrapFlag()
{
    VMX::PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS PrimaryControls = { static_cast<unsigned int>(vmread(VMX::VMCS_FIELD_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS)) };
    PrimaryControls.Bitmap.MonitorTrapFlag = TRUE;
    __vmx_vmwrite(VMX::VMCS_FIELD_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, PrimaryControls.Value);
}

void DisableMonitorTrapFlag()
{
    VMX::PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS PrimaryControls = { static_cast<unsigned int>(vmread(VMX::VMCS_FIELD_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS)) };
    PrimaryControls.Bitmap.MonitorTrapFlag = FALSE;
    __vmx_vmwrite(VMX::VMCS_FIELD_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, PrimaryControls.Value);
}

void DisableGuestInterrupts()
{
    RFLAGS Rflags = { vmread(VMX::VMCS_FIELD_GUEST_RFLAGS) };
    Rflags.Bitmap.Eflags.Bitmap.IF = FALSE;
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_RFLAGS, Rflags.Value);
}

void EnableGuestInterrupts()
{
    RFLAGS Rflags = { vmread(VMX::VMCS_FIELD_GUEST_RFLAGS) };
    Rflags.Bitmap.Eflags.Bitmap.IF = TRUE;
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_RFLAGS, Rflags.Value);
}

bool IsMsrAllowed(unsigned int MsrIndex)
{
    if (MsrIndex <= 0x1FFF || (MsrIndex >= 0xC0000000 && MsrIndex <= 0xC0001FFF))
    {
        // Intel MSR:
        return true;
    }
    else if (MsrIndex >= 0x40000000 && MsrIndex <= 0x4000109F)
    {
        // Hyper-V MSR:
        return true;
    }
    else
    {
        // Invalid MSR index:
        return false;
    }
}

// Returns NULL if RegNum is RSP:
unsigned long long* GetRegPtr(unsigned char RegNum, __in GuestContext* Context)
{
    switch (RegNum)
    {
    case 0:
        return &Context->Rax;
    case 1:
        return &Context->Rcx;
    case 2:
        return &Context->Rdx;
    case 3:
        return &Context->Rbx;
    case 4:
        // RSP (must be obtained by __vmx_vmread(VMCS_FIELD_GUEST_RSP)):
        return nullptr;
    case 5:
        return &Context->Rbp;
    case 6:
        return &Context->Rsi;
    case 7:
        return &Context->Rdi;
    case 8:
        return &Context->R8;
    case 9:
        return &Context->R9;
    case 10:
        return &Context->R10;
    case 11:
        return &Context->R11;
    case 12:
        return &Context->R12;
    case 13:
        return &Context->R13;
    case 14:
        return &Context->R14;
    case 15:
        return &Context->R15;
    default:
        return nullptr;
    }
}

// Exit action for the SvmVmexitHandler/VmxVmexitHandler:
enum class VMM_STATUS : bool
{
	VMM_SHUTDOWN = false, // Devirtualize the current logical processor
	VMM_CONTINUE = true   // Continue execution in the virtualized environment
};

using FnVmexitHandler = VMM_STATUS(*)(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction);

namespace Supplementation
{
    static PVOID AllocPhys(SIZE_T Size, MEMORY_CACHING_TYPE CachingType = MmCached, ULONG MaxPhysBits = 0)
    {
        PVOID64 HighestAcceptableAddress = MaxPhysBits
            ? reinterpret_cast<PVOID64>((1ULL << MaxPhysBits) - 1)
            : reinterpret_cast<PVOID64>((1ULL << 48) - 1);

        PVOID Memory = PhysicalMemory::AllocPhysicalMemorySpecifyCache(
            0,
            HighestAcceptableAddress,
            0,
            Size,
            CachingType
        );
        if (Memory) RtlSecureZeroMemory(Memory, Size);
        return Memory;
    }

    static VOID FreePhys(PVOID Memory)
    {
        PhysicalMemory::FreePhysicalMemory(Memory);
    }

    namespace FastPhys
    {
        // As is from ntoskrnl.exe disassembly (VirtualAddress may be unaligned):
        inline static unsigned long long MiGetPteAddress(unsigned long long VirtualAddress)
        {
            return 0xFFFFF680'00000000ull + ((VirtualAddress >> 9ull) & 0x7FFFFFFFF8ull);
        }

        // To fixup differences between different kernels:
        static const unsigned long long g_PteCorrective = []() -> unsigned long long
            {
                unsigned long long TestVa = reinterpret_cast<unsigned long long>(&g_PteCorrective);

                /* Manual traversal to obtain a valid PTE pointer in system memory */

                VIRTUAL_ADDRESS Va = { TestVa };

                auto Pml4ePhys = PFN_TO_PAGE(CR3{ __readcr3() }.x64.Bitmap.PML4) + Va.x64.NonPageSize.Generic.PageMapLevel4Offset * sizeof(PML4E);
                const PML4E* Pml4e = reinterpret_cast<const PML4E*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(Pml4ePhys) }));

                auto PdpePhys = PFN_TO_PAGE(Pml4e->x64.Generic.PDP) + Va.x64.NonPageSize.Generic.PageDirectoryPointerOffset * sizeof(PDPE);
                const PDPE* Pdpe = reinterpret_cast<const PDPE*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(PdpePhys) }));

                auto PdePhys = PFN_TO_PAGE(Pdpe->x64.NonPageSize.Generic.PD) + Va.x64.NonPageSize.Generic.PageDirectoryOffset * sizeof(PDE);
                const PDE* Pde = reinterpret_cast<const PDE*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(PdePhys) }));

                auto PtePhys = PFN_TO_PAGE(Pde->x64.Page4Kb.PT) + Va.x64.NonPageSize.Page4Kb.PageTableOffset * sizeof(PTE);
                const PTE* Pte = reinterpret_cast<const PTE*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(PtePhys) }));

                /* Then get a PTE pointer by MiGetPteAddress and calculate a difference */

                unsigned long long PteByMi = MiGetPteAddress(TestVa & 0xFFFFFFFFFFFFF000ull);

                return reinterpret_cast<unsigned long long>(Pte) - PteByMi;
            }();

        inline unsigned long long GetPhysAddressFast4KbUnsafe(unsigned long long Va)
        {
            return PFN_TO_PAGE(reinterpret_cast<const PTE*>(MiGetPteAddress(Va) + g_PteCorrective)->x64.Page4Kb.PhysicalPageFrameNumber) + (Va & 0xFFF);
        }

        unsigned long long GetPhysAddressFast4Kb(unsigned long long Cr3, unsigned long long VirtualAddress)
        {
            VIRTUAL_ADDRESS Va = { VirtualAddress };

            auto Pml4ePhys = PFN_TO_PAGE(CR3{ Cr3 }.x64.Bitmap.PML4) + Va.x64.NonPageSize.Generic.PageMapLevel4Offset * sizeof(PML4E);
            const PML4E* Pml4e = reinterpret_cast<const PML4E*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(Pml4ePhys) }));

            auto PdpePhys = PFN_TO_PAGE(Pml4e->x64.Generic.PDP) + Va.x64.NonPageSize.Generic.PageDirectoryPointerOffset * sizeof(PDPE);
            const PDPE* Pdpe = reinterpret_cast<const PDPE*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(PdpePhys) }));

            auto PdePhys = PFN_TO_PAGE(Pdpe->x64.NonPageSize.Generic.PD) + Va.x64.NonPageSize.Generic.PageDirectoryOffset * sizeof(PDE);
            const PDE* Pde = reinterpret_cast<const PDE*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(PdePhys) }));

            auto PtePhys = PFN_TO_PAGE(Pde->x64.Page4Kb.PT) + Va.x64.NonPageSize.Page4Kb.PageTableOffset * sizeof(PTE);
            const PTE* Pte = reinterpret_cast<const PTE*>(MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = static_cast<long long>(PtePhys) }));

            return PFN_TO_PAGE(Pte->x64.Page4Kb.PhysicalPageFrameNumber) + Va.x64.NonPageSize.Page4Kb.PageOffset;
        }
    }
}

namespace VmExit {
    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS EmptyHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Private);
        UNREFERENCED_PARAMETER(Context);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        __debugbreak();

        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS CpuidHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        //UNREFERENCED_PARAMETER(RepeatInstruction);

        CPUID_REGS Regs = {};
		int Function = static_cast<int>(Context->Rax);
		int SubLeaf = static_cast<int>(Context->Rcx);
		__cpuidex(Regs.Raw, Function, SubLeaf);

		if (Function == CPUID_VMM_SHUTDOWN) 
        { 
            Rip += vmread(VMX::VMCS_FIELD_VMEXIT_INSTRUCTION_LENGTH);

            size_t Rsp = vmread(VMX::VMCS_FIELD_GUEST_RSP);

            Context->Rax = reinterpret_cast<UINT64>(Private) & MAXUINT32; // Low part
            Context->Rbx = Rip; // Guest RIP
            Context->Rcx = Rsp; // Guest RSP
            Context->Rdx = reinterpret_cast<UINT64>(Private) >> 32; // High part

            _lgdt(&Private->Gdtr);
            __lidt(&Private->Idtr);

            CR3 Cr3 = {};
            Cr3.Value = vmread(VMX::VMCS_FIELD_GUEST_CR3);
            __writecr3(Cr3.Value);

            RFLAGS Rflags = {};
            Rflags.Value = vmread(VMX::VMCS_FIELD_GUEST_RFLAGS);
            __writeeflags(Rflags.Value);

            __vmx_off();

            RepeatInstruction = true;
        }
		else 
		{
			Context->Rax = Regs.Regs.Eax;
			Context->Rbx = Regs.Regs.Ebx;
			Context->Rcx = Regs.Regs.Ecx;
			Context->Rdx = Regs.Regs.Edx;
		}

        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS XsetbvHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Private);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        _xsetbv(static_cast<unsigned int>(Context->Rcx), (Context->Rdx << 32u) | Context->Rax);
        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS EptViolationHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Context);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        VMX::EXIT_QUALIFICATION Info = { vmread(VMX::VMCS_FIELD_EXIT_QUALIFICATION) };
        unsigned long long AccessedPa = vmread(VMX::VMCS_FIELD_GUEST_PHYSICAL_ADDRESS_FULL);

        if (!(Info.EptViolations.GuestPhysicalReadable || Info.EptViolations.GuestPhysicalExecutable))
        {
            // The page is neither readable nor executable (acts as not present):
            InjectEvent(VMX::INTERRUPTION_TYPE::HardwareException, INTERRUPT_VECTOR::GeneralProtection, true, 0);
            return VMM_STATUS::VMM_CONTINUE;
        }

        bool Handled = false;

        if (Info.EptViolations.AccessedRead)
        {
            unsigned long long RipPa;
            if (AddressRange::IsUserAddress(reinterpret_cast<void*>(Rip)))
            {
                __writecr3(vmread(VMX::VMCS_FIELD_GUEST_CR3));
                RipPa = Supplementation::FastPhys::GetPhysAddressFast4KbUnsafe(Rip);
                __writecr3(g_Shared.KernelCr3);
            }
            else
            {
                RipPa = Supplementation::FastPhys::GetPhysAddressFast4KbUnsafe(Rip);
            }

            if (ALIGN_DOWN_BY(AccessedPa, PAGE_SIZE) == ALIGN_DOWN_BY(RipPa, PAGE_SIZE))
            {
                unsigned long long InstructionLength = vmread(VMX::VMCS_FIELD_VMEXIT_INSTRUCTION_LENGTH);
                Handled = Private->EptInterceptor->HandleExecuteRead(AccessedPa, reinterpret_cast<void*>(Rip + InstructionLength));
                if (Handled)
                {
                    // Perform a single step:
                    EnableMonitorTrapFlag();
                    DisableGuestInterrupts();
                }
            }
            else
            {
                Handled = Private->EptInterceptor->HandleRead(AccessedPa);
            }
        }
        else if (Info.EptViolations.AccessedWrite)
        {
            unsigned long long InstructionLength = vmread(VMX::VMCS_FIELD_VMEXIT_INSTRUCTION_LENGTH);

            unsigned long long RipPa;
            if (AddressRange::IsUserAddress(reinterpret_cast<void*>(Rip)))
            {
                __writecr3(vmread(VMX::VMCS_FIELD_GUEST_CR3));
                RipPa = Supplementation::FastPhys::GetPhysAddressFast4KbUnsafe(Rip);
                __writecr3(g_Shared.KernelCr3);
            }
            else
            {
                RipPa = Supplementation::FastPhys::GetPhysAddressFast4KbUnsafe(Rip);
            }

            if (ALIGN_DOWN_BY(AccessedPa, PAGE_SIZE) == ALIGN_DOWN_BY(RipPa, PAGE_SIZE))
            {
                Handled = Private->EptInterceptor->HandleExecuteWrite(AccessedPa, reinterpret_cast<void*>(Rip + InstructionLength));
            }
            else
            {
                Handled = Private->EptInterceptor->HandleWrite(AccessedPa, reinterpret_cast<void*>(Rip + InstructionLength));
            }

            if (Handled)
            {
                // Perform a single step:
                EnableMonitorTrapFlag();
                DisableGuestInterrupts();
            }
        }
        else if (Info.EptViolations.AccessedExecute)
        {
            Handled = Private->EptInterceptor->HandleExecute(AccessedPa);
        }

        if (Handled)
        {
            RepeatInstruction = true;
        }
        else
        {
            InjectEvent(VMX::INTERRUPTION_TYPE::HardwareException, INTERRUPT_VECTOR::GeneralProtection, true, 0);
        }

        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS EptMisconfigurationHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Context);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        unsigned long long FailedPagePa = vmread(VMX::VMCS_FIELD_GUEST_PHYSICAL_ADDRESS_FULL);

        VMX::EPT_ENTRIES EptEntries = {};
        Ept::GetEptEntries(FailedPagePa, Private->Ept, EptEntries);

        UNREFERENCED_PARAMETER(EptEntries);

        __debugbreak();

        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS MonitorTrapFlagHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Context);

        Private->EptInterceptor->CompletePendingHandler(reinterpret_cast<void*>(Rip));
        DisableMonitorTrapFlag();
        EnableGuestInterrupts();
        RepeatInstruction = true;
        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS ExceptionOrNmiHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Private);
        UNREFERENCED_PARAMETER(Context);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS VmcallHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Private);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        if (Context->R10 == HyperSign)
        {
            switch (static_cast<VMCALLS::VMCALL_INDEX>(Context->Rcx))
            {
            case VMCALLS::VMCALL_INDEX::VmmCall:
            {
                unsigned long long(*Fn)(void* Arg) = reinterpret_cast<decltype(Fn)>(Context->Rdx);
                void* Arg = reinterpret_cast<void*>(Context->R8);
                bool SwitchToCallerAddressSpace = Context->R9 != 0;

                unsigned long long Cr3 = 0;
                if (SwitchToCallerAddressSpace)
                {
                    Cr3 = __readcr3();
                    __writecr3(vmread(VMX::VMCS_FIELD_GUEST_CR3));
                }

                Context->Rax = Fn(Arg);

                if (SwitchToCallerAddressSpace)
                {
                    __writecr3(Cr3);
                }
                break;
            }
            default:
            {
                Context->Rax = HyperSign;
            }
            }
        }
        else
        {
            HyperV::HYPERCALL_INPUT_VALUE InputValue = { Context->Rcx };
            switch (static_cast<HyperV::HYPERCALL_CODE>(InputValue.Bitmap.CallCode))
            {
            case HyperV::HYPERCALL_CODE::HvSwitchVirtualAddressSpace:
            case HyperV::HYPERCALL_CODE::HvFlushVirtualAddressSpace:
            case HyperV::HYPERCALL_CODE::HvFlushVirtualAddressList:
            case HyperV::HYPERCALL_CODE::HvCallFlushVirtualAddressSpaceEx:
            case HyperV::HYPERCALL_CODE::HvCallFlushVirtualAddressListEx:
            {
                VMX::INVVPID_DESCRIPTOR InvvpidDesc = {};
                __invvpid((unsigned int)VMX::INVVPID_TYPE::AllContextsInvalidation, &InvvpidDesc);
                break;
            }
            case HyperV::HYPERCALL_CODE::HvCallFlushGuestPhysicalAddressSpace:
            case HyperV::HYPERCALL_CODE::HvCallFlushGuestPhysicalAddressList:
            {
                // Acts as __invept():
                VMX::INVEPT_DESCRIPTOR Desc = {};
                Desc.Eptp = vmread(VMX::VMCS_FIELD_EPT_POINTER_FULL);
                __invept((unsigned int)VMX::INVEPT_TYPE::GlobalInvalidation, &Desc);
                break;
            }
            }

            // It is a Hyper-V hypercall - passing through:
            Context->Rax = __hyperv_vmcall(Context->Rcx, Context->Rdx, Context->R8, Context->R9);
        }

        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS RdmsrHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Private);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        unsigned long Msr = static_cast<unsigned long>(Context->Rcx);
        if (IsMsrAllowed(Msr))
        {
            // Allowed MSR:
            LARGE_INTEGER Value = {};
            Value.QuadPart = __readmsr(Msr);
            Context->Rdx = Value.HighPart;
            Context->Rax = Value.LowPart;
        }
        else
        {
            // It is unknown MSR (not Intel nor Hyper-V), returning 0:
            Context->Rdx = 0;
            Context->Rax = 0;
        }
        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS WrmsrHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Private);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        unsigned int Msr = static_cast<int>(Context->Rcx);
        if (IsMsrAllowed(Msr))
        {
            unsigned long long Value = (static_cast<unsigned long long>(Context->Rdx) << 32u)
                | (static_cast<unsigned long long>(Context->Rax));
            __writemsr(Msr, Value);
        }
        return VMM_STATUS::VMM_CONTINUE;
    }

    _IRQL_requires_same_
    _IRQL_requires_min_(DISPATCH_LEVEL)
    static VMM_STATUS VmxRelatedHandler(__inout VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context, unsigned long long Rip, __inout_opt bool& RepeatInstruction)
    {
        UNREFERENCED_PARAMETER(Private);
        UNREFERENCED_PARAMETER(Context);
        UNREFERENCED_PARAMETER(Rip);
        UNREFERENCED_PARAMETER(RepeatInstruction);

        RFLAGS Rflags = {};
        Rflags.Value = vmread(VMX::VMCS_FIELD_GUEST_RFLAGS);
        Rflags.Bitmap.Eflags.Bitmap.CF = TRUE;
        __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_RFLAGS, Rflags.Value);

        return VMM_STATUS::VMM_CONTINUE;
    }

    // It is large enough to contain all possible handlers (the last handler has number 68 (EXIT_REASON_TPAUSE)):
    static FnVmexitHandler HandlersTable[72] = {};

    void InsertHandler(VMX::VMX_EXIT_REASON ExitReason, FnVmexitHandler Handler)
    {
        HandlersTable[static_cast<unsigned int>(ExitReason)] = Handler;
    }

    void InitHandlersTable()
    {
        for (auto i = 0u; i < ARRAYSIZE(HandlersTable); ++i)
        {
            HandlersTable[i] = EmptyHandler;
        }

        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_CPUID, CpuidHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_XSETBV, XsetbvHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_EPT_VIOLATION, EptViolationHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_EPT_MISCONFIGURATION, EptMisconfigurationHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_MONITOR_TRAP_FLAG, MonitorTrapFlagHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_EXCEPTION_OR_NMI, ExceptionOrNmiHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMCALL, VmcallHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_RDMSR, RdmsrHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_WRMSR, WrmsrHandler);

        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMCLEAR, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMLAUNCH, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMPTRLD, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMPTRST, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMREAD, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMWRITE, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMXOFF, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_VMXON, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_INVVPID, VmxRelatedHandler);
        InsertHandler(VMX::VMX_EXIT_REASON::EXIT_REASON_INVEPT, VmxRelatedHandler);

        _mm_sfence();
    }
}

void Interceptions(
	_In_ Intel::IA32_VMX_BASIC VmxBasicInfo)
{
	VMX::PIN_BASED_VM_EXECUTION_CONTROLS PinControls = {};
	PinControls = GetControlMask::ApplyMask(PinControls, GetControlMask::GetPinControlsMask(VmxBasicInfo));
	__vmx_vmwrite(VMX::VMCS_FIELD_PIN_BASED_VM_EXECUTION_CONTROLS, PinControls.Value);

	VMX::PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS PrimaryControls = {};
	PrimaryControls.Bitmap.UseMsrBitmaps = TRUE;
	PrimaryControls.Bitmap.ActivateSecondaryControls = TRUE;
	PrimaryControls = GetControlMask::ApplyMask(PrimaryControls, GetControlMask::GetPrimaryControlsMask(VmxBasicInfo));
	__vmx_vmwrite(VMX::VMCS_FIELD_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, PrimaryControls.Value);

	VMX::VMEXIT_CONTROLS VmexitControls = {};
	VmexitControls.Bitmap.SaveDebugControls = TRUE;
	VmexitControls.Bitmap.HostAddressSpaceSize = TRUE;
	VmexitControls = GetControlMask::ApplyMask(VmexitControls, GetControlMask::GetVmexitControlsMask(VmxBasicInfo));
	__vmx_vmwrite(VMX::VMCS_FIELD_VMEXIT_CONTROLS, VmexitControls.Value);

	VMX::VMENTRY_CONTROLS VmentryControls = {};
	VmentryControls.Bitmap.LoadDebugControls = TRUE;
	VmentryControls.Bitmap.Ia32ModeGuest = TRUE;
	VmentryControls = GetControlMask::ApplyMask(VmentryControls, GetControlMask::GetVmentryControlsMask(VmxBasicInfo));
	__vmx_vmwrite(VMX::VMCS_FIELD_VMENTRY_CONTROLS, VmentryControls.Value);

	VMX::SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS SecondaryControls = {};
	SecondaryControls.Bitmap.EnableEpt = TRUE;
	SecondaryControls.Bitmap.EnableRdtscp = TRUE;
	SecondaryControls.Bitmap.EnableVpid = TRUE;
	SecondaryControls.Bitmap.EnableInvpcid = TRUE;
	SecondaryControls.Bitmap.EnableVmFunctions = TRUE;
	SecondaryControls.Bitmap.EptViolation = FALSE;
	SecondaryControls.Bitmap.EnableXsavesXrstors = TRUE;
	SecondaryControls = GetControlMask::ApplyMask(SecondaryControls, GetControlMask::GetSecondaryControlsMask());
	__vmx_vmwrite(VMX::VMCS_FIELD_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, SecondaryControls.Value);
}

_IRQL_requires_same_
_IRQL_requires_min_(HIGH_LEVEL)
extern "C" VMM_STATUS VmxVmExitHandler(VMX::PRIVATE_VM_DATA* Private, __inout GuestContext* Context)
{
    /* Interrupts are locked */

    unsigned long long Rip = vmread(VMX::VMCS_FIELD_GUEST_RIP);

    VMX::EXIT_REASON ExitReason = {};
    ExitReason.Value = static_cast<unsigned int>(vmread(VMX::VMCS_FIELD_EXIT_REASON));

    bool RepeatInstruction = false;

    VMM_STATUS Status = VmExit::HandlersTable[ExitReason.Bitmap.BasicExitReason](Private, Context, Rip, RepeatInstruction);

    if (!RepeatInstruction)
    {
        // Go to the next instruction:
        Rip += vmread(VMX::VMCS_FIELD_VMEXIT_INSTRUCTION_LENGTH);
        __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_RIP, Rip);
    }

    return Status;
}

//Define in asm file(in my example)
extern "C" void VmxVmmRun(_In_ void* InitialVmmStackPointer);

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObj, PUNICODE_STRING RegisterPath)
{
	UNREFERENCED_PARAMETER(RegisterPath);

	objHyperVisorVmx.PInterceptions = &Interceptions;
    objHyperVisorVmx.PInitHandlersTable = &VmExit::InitHandlersTable;
	objHyperVisorVmx.PVmxVmmRun = &VmxVmmRun;

	if (objHyperVisorVmx.IsVmxSupported()) { objHyperVisorVmx.VirtualizeAllProcessors(); }
	DriverObj->DriverUnload = DriverUnload;

	return STATUS_SUCCESS;
}
