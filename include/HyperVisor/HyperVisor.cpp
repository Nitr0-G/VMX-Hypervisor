#include "HyperVisor.hpp"

VMX::SHARED_VM_DATA g_Shared = {};

extern "C" NTSYSAPI VOID NTAPI RtlRestoreContext(__in PCONTEXT ContextRecord, __in_opt EXCEPTION_RECORD* ExceptionRecord);

size_t vmread(_In_ size_t field)
{
    size_t value = 0;
    __vmx_vmread(field, &value);
    return value;
}

CONTROLS_MASK GetControlMask::GetPinControlsMask(Intel::IA32_VMX_BASIC VmxBasic)
{
    CONTROLS_MASK Mask = {};
    if (VmxBasic.Bitmap.AnyVmxControlsThatDefaultToOneMayBeZeroed) {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_TRUE_PINBASED_CTLS));
    }
    else {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_PINBASED_CTLS));
    }

    return Mask;
}

CONTROLS_MASK GetControlMask::GetPrimaryControlsMask(Intel::IA32_VMX_BASIC VmxBasic)
{
    CONTROLS_MASK Mask = {};
    if (VmxBasic.Bitmap.AnyVmxControlsThatDefaultToOneMayBeZeroed) {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_TRUE_PROCBASED_CTLS));
    }
    else {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_PROCBASED_CTLS));
    }

    return Mask;
}

CONTROLS_MASK GetControlMask::GetVmexitControlsMask(Intel::IA32_VMX_BASIC VmxBasic)
{
    CONTROLS_MASK Mask = {};
    if (VmxBasic.Bitmap.AnyVmxControlsThatDefaultToOneMayBeZeroed) {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_TRUE_EXIT_CTLS));
    }
    else {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_EXIT_CTLS));
    }

    return Mask;
}

CONTROLS_MASK GetControlMask::GetVmentryControlsMask(Intel::IA32_VMX_BASIC VmxBasic)
{
    CONTROLS_MASK Mask = {};
    if (VmxBasic.Bitmap.AnyVmxControlsThatDefaultToOneMayBeZeroed) {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_TRUE_ENTRY_CTLS));
    }
    else {
        Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_ENTRY_CTLS));
    }

    return Mask;
}

CONTROLS_MASK GetControlMask::GetSecondaryControlsMask()
{
    CONTROLS_MASK Mask = {};
    Mask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_PROCBASED_CTLS2));
    return Mask;
}

CONTROLS_MASK HyperVisorVmx::GetCr0Mask()
{
    CONTROLS_MASK Mask = {};
    Mask.Bitmap.Allowed0Settings = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_CR0_FIXED0));
    Mask.Bitmap.Allowed1Settings = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_CR0_FIXED1));
    return Mask;
}

CONTROLS_MASK HyperVisorVmx::GetCr4Mask()
{
    CONTROLS_MASK Mask = {};
    Mask.Bitmap.Allowed0Settings = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_CR4_FIXED0));
    Mask.Bitmap.Allowed1Settings = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_CR4_FIXED1));
    return Mask;
}

PVOID HyperVisorVmx::AllocPhys(
    _In_ SIZE_T Size,
    _In_ MEMORY_CACHING_TYPE CachingType = MmCached,
    _In_ ULONG MaxPhysBits = 0)
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

void HyperVisorVmx::FreePhys(_In_ PVOID Memory)
{
    PhysicalMemory::FreePhysicalMemory(Memory);
}

CpuVendor HyperVisorVmx::GetCpuVendor()
{
    static CpuVendor CpuVendor; CPUID_REGS Regs;
    __cpuid(Regs.Raw, CPUID::Generic::CPUID_MAXIMUM_FUNCTION_NUMBER_AND_VENDOR_ID);
    if (Regs.Regs.Ebx == IntelEnc::IEbx && Regs.Regs.Edx == IntelEnc::IEdx && Regs.Regs.Ecx == IntelEnc::IEcx) { CpuVendor = CpuVendor::CpuIntel; }
    else if (Regs.Regs.Ebx == AmdEnc::AEbx && Regs.Regs.Edx == AmdEnc::AEdx && Regs.Regs.Ecx == AmdEnc::AEcx) { CpuVendor = CpuVendor::CpuAmd; }
    else { CpuVendor = CpuVendor::CpuUnknown; }
    return CpuVendor;
}

void HyperVisorVmx::FreePrivateVmData(_In_ void* Private)
{
    auto* Data = reinterpret_cast<VMX::PRIVATE_VM_DATA*>(Private);
    delete Data->EptInterceptor;
    FreePhys(Private);
}

bool HyperVisorVmx::DevirtualizeProcessor(
    __out PVOID& PrivateVmData)
{
    PrivateVmData = NULL; CPUID_REGS Regs = {};
    __cpuid(Regs.Raw, CPUID_VMM_SHUTDOWN);
    if (Regs.Regs.Ecx != CPUID_VMM_SHUTDOWN) { return false; }// Processor not virtualized!

    // Processor is devirtualized now:
    //  Info.Eax -> PRIVATE_VM_DATA* Private LOW
    //  Info.Ebx -> Vmexit RIP
    //  Info.Ecx -> VMEXIT_SIGNATURE
    //  Info.Edx -> PRIVATE_VM_DATA* Private HIGH

    PrivateVmData = reinterpret_cast<void*>(
        (static_cast<UINT64>(Regs.Regs.Edx) << 32u) |
        (static_cast<UINT64>(Regs.Regs.Eax))
        );

    return true;
}

bool HyperVisorVmx::DevirtualizeAllProcessors()
{
    ULONG ProcessorsCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    void** PrivateVmDataArray = VirtualMemory::AllocArray<void*>(ProcessorsCount);

    struct Callable {
        static ULONG_PTR BroadCast(void* Arg, void* Class)
        {
            void** PrivateVmDataArray = reinterpret_cast<void**>(Arg);
            ULONG CurrentProcessor = KeGetCurrentProcessorNumber();
            void* PrivateVmData = NULL;
            bool Status = static_cast<HyperVisorVmx*>(Class)->DevirtualizeProcessor(OUT PrivateVmData);
            PrivateVmDataArray[CurrentProcessor] = PrivateVmData; // Data buffer to free
            return static_cast<ULONG_PTR>(Status);
        }
    };

    KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(Callable::BroadCast), reinterpret_cast<ULONG_PTR>(PrivateVmDataArray));

    CpuVendor vendor = GetCpuVendor();

    for (ULONG i = 0; i < ProcessorsCount; ++i)
    {
        if (PrivateVmDataArray[i])
        {
            if (vendor == CpuVendor::CpuIntel)
            {
                FreePrivateVmData(PrivateVmDataArray[i]);
            }
            else
            {
                FreePhys(PrivateVmDataArray[i]);
            }
        }
    }

    g_IsVirtualized = false;

    return true;
}

void HyperVisorVmx::InitMtrr(__out VMX::MTRR_INFO* MtrrInfo)
{
    *MtrrInfo = {};

    CPUID::FEATURE_INFORMATION Features = {};
    __cpuid(Features.Regs.Raw, CPUID::Intel::CPUID_FEATURE_INFORMATION);
    MtrrInfo->IsSupported = Features.Intel.MTRR;

    if (!MtrrInfo->IsSupported) return;

    CPUID::Intel::VIRTUAL_AND_PHYSICAL_ADDRESS_SIZES MaxAddrSizes = {};
    __cpuid(MaxAddrSizes.Regs.Raw, CPUID::Intel::CPUID_VIRTUAL_AND_PHYSICAL_ADDRESS_SIZES);
    MtrrInfo->MaxPhysAddrBits = MaxAddrSizes.Bitmap.PhysicalAddressBits;
    MtrrInfo->PhysAddrMask = ~MaskLow<unsigned long long>(static_cast<unsigned char>(MtrrInfo->MaxPhysAddrBits));

    MtrrInfo->EptVpidCap.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_EPT_VPID_CAP));
    MtrrInfo->MtrrCap.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRRCAP));
    MtrrInfo->MtrrDefType.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_DEF_TYPE));

    if (MtrrInfo->MtrrCap.Bitmap.FIX && MtrrInfo->MtrrDefType.Bitmap.FE)
    {
        // 512-Kbyte range:
        MtrrInfo->Fixed.Ranges.RangeFrom00000To7FFFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX64K_00000));

        // Two 128-Kbyte ranges:
        MtrrInfo->Fixed.Ranges.RangeFrom80000To9FFFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX16K_80000));
        MtrrInfo->Fixed.Ranges.RangeFromA0000ToBFFFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX16K_A0000));

        // Eight 32-Kbyte ranges:
        MtrrInfo->Fixed.Ranges.RangeFromC0000ToC7FFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_C0000));
        MtrrInfo->Fixed.Ranges.RangeFromC8000ToCFFFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_C8000));
        MtrrInfo->Fixed.Ranges.RangeFromD0000ToD7FFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_D0000));
        MtrrInfo->Fixed.Ranges.RangeFromD8000ToDFFFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_D8000));
        MtrrInfo->Fixed.Ranges.RangeFromE0000ToE7FFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_E0000));
        MtrrInfo->Fixed.Ranges.RangeFromE8000ToEFFFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_E8000));
        MtrrInfo->Fixed.Ranges.RangeFromF0000ToF7FFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_F0000));
        MtrrInfo->Fixed.Ranges.RangeFromF8000ToFFFFF.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_FIX4K_F8000));
    }

    for (unsigned i = 0; i < MtrrInfo->MtrrCap.Bitmap.VCNT; ++i)
    {
        if (i == ARRAYSIZE(MtrrInfo->Variable)) break;
        MtrrInfo->Variable[i].PhysBase.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_PHYSBASE0) + i * 2);
        MtrrInfo->Variable[i].PhysMask.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_MTRR_PHYSMASK0) + i * 2);
    }
}

unsigned long long HyperVisorVmx::ExtractSegmentBaseAddress(_In_ const SEGMENT_DESCRIPTOR_LONG* SegmentDescriptor)
{
    if (SegmentDescriptor->Generic.System == 0)
    {
        // System segment (16 bytes):
        auto* Descriptor = reinterpret_cast<const SYSTEM_SEGMENT_DESCRIPTOR_LONG*>(SegmentDescriptor);
        return (static_cast<unsigned long long>(Descriptor->Bitmap.BaseAddressHighest) << 32)
            | (static_cast<unsigned long long>(Descriptor->Bitmap.BaseAddressHigh) << 24)
            | (static_cast<unsigned long long>(Descriptor->Bitmap.BaseAddressMiddle) << 16)
            | (static_cast<unsigned long long>(Descriptor->Bitmap.BaseAddressLow));
    }
    else
    {
        // User segment (8 bytes):
        auto* Descriptor = reinterpret_cast<const USER_SEGMENT_DESCRIPTOR_LONG*>(SegmentDescriptor);
        return (static_cast<unsigned long long>(Descriptor->Generic.BaseAddressHigh) << 24)
            | (static_cast<unsigned long long>(Descriptor->Generic.BaseAddressMiddle) << 16)
            | (static_cast<unsigned long long>(Descriptor->Generic.BaseAddressLow));
    }
}

void HyperVisorVmx::ParseSegmentInfo(
    _In_ const SEGMENT_DESCRIPTOR_LONG* Gdt,
    _In_ const SEGMENT_DESCRIPTOR_LONG* Ldt,
    _In_ unsigned short Selector,
    __out VMX::SEGMENT_INFO* Info
) {
    *Info = {};

    SEGMENT_SELECTOR SegmentSelector;
    SegmentSelector.Value = Selector;

    auto* SegmentDescriptor = SegmentSelector.Bitmap.TableIndicator == 0
        ? reinterpret_cast<const SEGMENT_DESCRIPTOR_LONG*>(&Gdt[SegmentSelector.Bitmap.SelectorIndex])
        : reinterpret_cast<const SEGMENT_DESCRIPTOR_LONG*>(&Ldt[SegmentSelector.Bitmap.SelectorIndex]);

    Info->BaseAddress = ExtractSegmentBaseAddress(SegmentDescriptor);
    Info->Limit = GetSegmentLimit(Selector);

    Info->AccessRights.Bitmap.SegmentType = SegmentDescriptor->Generic.Type;
    Info->AccessRights.Bitmap.S = SegmentDescriptor->Generic.System;
    Info->AccessRights.Bitmap.DPL = SegmentDescriptor->Generic.Dpl;
    Info->AccessRights.Bitmap.P = SegmentDescriptor->Generic.Present;
    Info->AccessRights.Bitmap.AVL = SegmentDescriptor->Generic.Available;
    Info->AccessRights.Bitmap.L = SegmentDescriptor->Generic.LongMode;
    Info->AccessRights.Bitmap.DB = SegmentDescriptor->Generic.System == 1 // If it is a user segment descriptor:
        ? reinterpret_cast<const USER_SEGMENT_DESCRIPTOR_LONG*>(SegmentDescriptor)->Generic.DefaultOperandSize
        : 0; // The DefaultOperandSize is not applicable to system segments and marked as reserved!
    Info->AccessRights.Bitmap.G = SegmentDescriptor->Generic.Granularity;
    Info->AccessRights.Bitmap.SegmentUnusable = static_cast<unsigned int>(!Info->AccessRights.Bitmap.P);

    Info->Selector = Selector;
}

bool HyperVisorVmx::VirtualizeProcessor()
{
    using namespace PhysicalMemory;

    volatile LONG IsVirtualized = FALSE;
    InterlockedExchange(&IsVirtualized, FALSE);

    volatile LONG ContextHasBeenRestored = FALSE;
    InterlockedExchange(&ContextHasBeenRestored, FALSE);

    volatile unsigned int CurrentProcessor = KeGetCurrentProcessorNumber();
    unsigned int Vpid = CurrentProcessor + 1;

    CONTEXT Context = {};
    Context.ContextFlags = CONTEXT_ALL;
    RtlCaptureContext(&Context);

    if (InterlockedCompareExchange(&IsVirtualized, TRUE, TRUE) == TRUE)
    {
        if (InterlockedCompareExchange(&ContextHasBeenRestored, FALSE, FALSE) == FALSE)
        {
            InterlockedExchange(&ContextHasBeenRestored, TRUE);
            RtlRestoreContext(&Context, NULL);
        }

        g_Shared.Processors[CurrentProcessor].Status = true;
        _mm_sfence();

        return true;
    }

    Intel::IA32_VMX_BASIC VmxBasicInfo = { __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_VMX_BASIC)) };

    VMX::PRIVATE_VM_DATA* Private = g_Shared.Processors[CurrentProcessor].VmData;
    if (!Private)
    {
        return false;
    }

    Private->Vmxon.RevisionId.Bitmap.VmcsRevisionId = VmxBasicInfo.Bitmap.VmcsRevision;
    Private->Vmcs.RevisionId.Bitmap.VmcsRevisionId = VmxBasicInfo.Bitmap.VmcsRevision;

    void* VmxonPa = GetPhysicalAddress(&Private->Vmxon);
    Private->VmmStack.Layout.InitialStack.Shared = &g_Shared;
    Private->VmmStack.Layout.InitialStack.VmcsPa = GetPhysicalAddress(&Private->Vmcs);
    Private->VmmStack.Layout.InitialStack.Private = Private;

    CR0 Cr0 = { __readcr0() };
    Cr0 = GetControlMask::ApplyMask(Cr0, GetCr0Mask());
    __writecr0(Cr0.Value);

    // Enable the VMX instructions set:
    CR4 Cr4 = { __readcr4() };
    Cr4.x64.Bitmap.VMXE = TRUE;
    Cr4.x64.Bitmap.PCIDE = TRUE;
    Cr4 = GetControlMask::ApplyMask(Cr4, GetCr4Mask());
    __writecr4(Cr4.Value);

    unsigned char VmxStatus = 0;

    // Entering the VMX root-mode:
    VmxStatus = __vmx_on(reinterpret_cast<unsigned long long*>(&VmxonPa));
    if (VmxStatus != 0)
    {
        return false;
    }

    // Resetting the guest VMCS:
    VmxStatus = __vmx_vmclear(reinterpret_cast<unsigned long long*>(&Private->VmmStack.Layout.InitialStack.VmcsPa));
    if (VmxStatus != 0)
    {
        __vmx_off();
        return false;
    }

    // Loading the VMCS as current for the processor:
    VmxStatus = __vmx_vmptrld(reinterpret_cast<unsigned long long*>(&Private->VmmStack.Layout.InitialStack.VmcsPa));
    if (VmxStatus != 0)
    {
        __vmx_off();
        return false;
    }

    __vmx_vmwrite(VMX::VMCS_FIELD_VMCS_LINK_POINTER_FULL, 0xFFFFFFFFFFFFFFFFULL);
    __vmx_vmwrite(VMX::VMCS_FIELD_VIRTUAL_PROCESSOR_IDENTIFIER, Vpid);

    /* CR0 was already read above */
    __vmx_vmwrite(VMX::VMCS_FIELD_CR0_READ_SHADOW, Cr0.x64.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_CR0, Cr0.x64.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_CR0, Cr0.x64.Value);

    CR3 Cr3 = { __readcr3() };
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_CR3, Cr3.x64.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_CR3, g_Shared.KernelCr3);

    /* CR4 was already read above */
    CR4 Cr4Mask = Cr4;
    __vmx_vmwrite(VMX::VMCS_FIELD_CR4_READ_SHADOW, Cr4.x64.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_CR4, Cr4.x64.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_CR4, Cr4.x64.Value);

    DR7 Dr7 = { __readdr(7) };
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_DR7, Dr7.x64.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_IA32_DEBUGCTL_FULL, __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_DEBUGCTL)));

    __vmx_vmwrite(VMX::VMCS_FIELD_ADDRESS_OF_MSR_BITMAPS_FULL, reinterpret_cast<UINT64>(GetPhysicalAddress(Private->MsrBitmap.MsrBitmap)));

    DESCRIPTOR_TABLE_REGISTER_LONG Gdtr = {}, Idtr = {};
    _sgdt(&Gdtr);
    __sidt(&Idtr);
    Private->Gdtr = Gdtr;
    Private->Idtr = Idtr;

    SEGMENT_SELECTOR Tr = {}, Ldtr = {};
    _sldt(&Ldtr);
    _str(&Tr);

    const auto* Gdt = reinterpret_cast<const SEGMENT_DESCRIPTOR_LONG*>(Gdtr.BaseAddress);
    __assume(Gdt != nullptr);
    const auto* LdtDescriptorInGdt = reinterpret_cast<const SEGMENT_DESCRIPTOR_LONG*>(&Gdt[Ldtr.Bitmap.SelectorIndex]);
    const auto* Ldt = reinterpret_cast<const SEGMENT_DESCRIPTOR_LONG*>(ExtractSegmentBaseAddress(LdtDescriptorInGdt));

    // These fields must be zeroed in host state selector values:
    constexpr unsigned short RPL_MASK = 0b11; // Requested privilege level
    constexpr unsigned short TI_MASK = 0b100; // Table indicator
    constexpr unsigned short HOST_SELECTOR_MASK = TI_MASK | RPL_MASK;

    VMX::SEGMENT_INFO SegmentInfo = {};

    ParseSegmentInfo(Gdt, Ldt, Context.SegEs, OUT & SegmentInfo);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_ES_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_ES_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_ES_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_ES_BASE, SegmentInfo.BaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_ES_SELECTOR, SegmentInfo.Selector & ~HOST_SELECTOR_MASK);

    ParseSegmentInfo(Gdt, Ldt, Context.SegCs, OUT & SegmentInfo);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_CS_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_CS_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_CS_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_CS_BASE, SegmentInfo.BaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_CS_SELECTOR, SegmentInfo.Selector & ~HOST_SELECTOR_MASK);

    ParseSegmentInfo(Gdt, Ldt, Context.SegSs, OUT & SegmentInfo);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_SS_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_SS_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_SS_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_SS_BASE, SegmentInfo.BaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_SS_SELECTOR, SegmentInfo.Selector & ~HOST_SELECTOR_MASK);

    ParseSegmentInfo(Gdt, Ldt, Context.SegDs, OUT & SegmentInfo);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_DS_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_DS_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_DS_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_DS_BASE, SegmentInfo.BaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_DS_SELECTOR, SegmentInfo.Selector & ~HOST_SELECTOR_MASK);

    ParseSegmentInfo(Gdt, Ldt, Context.SegFs, OUT & SegmentInfo);
    unsigned long long FsBaseAddress = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_FS_BASE));
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_FS_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_FS_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_FS_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_FS_BASE, FsBaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_FS_SELECTOR, SegmentInfo.Selector & ~HOST_SELECTOR_MASK);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_FS_BASE, FsBaseAddress);

    ParseSegmentInfo(Gdt, Ldt, Context.SegGs, OUT & SegmentInfo);
    unsigned long long GsBaseAddress = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_GS_BASE));
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_GS_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_GS_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_GS_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_GS_BASE, GsBaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_GS_SELECTOR, SegmentInfo.Selector & ~HOST_SELECTOR_MASK);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_GS_BASE, GsBaseAddress);

    ParseSegmentInfo(Gdt, Ldt, Ldtr.Value, OUT & SegmentInfo);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_LDTR_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_LDTR_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_LDTR_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_LDTR_BASE, SegmentInfo.BaseAddress);

    ParseSegmentInfo(Gdt, Ldt, Tr.Value, OUT & SegmentInfo);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_TR_SELECTOR, SegmentInfo.Selector);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_TR_LIMIT, SegmentInfo.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_TR_ACCESS_RIGHTS, SegmentInfo.AccessRights.Value);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_TR_BASE, SegmentInfo.BaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_TR_SELECTOR, SegmentInfo.Selector & ~HOST_SELECTOR_MASK);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_TR_BASE, SegmentInfo.BaseAddress);

    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_GDTR_LIMIT, Gdtr.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_GDTR_BASE, Gdtr.BaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_GDTR_BASE, Gdtr.BaseAddress);

    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_IDTR_LIMIT, Idtr.Limit);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_IDTR_BASE, Idtr.BaseAddress);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_IDTR_BASE, Idtr.BaseAddress);

    VMX::EPTP Eptp = {};
    Ept::InitializeEptTables(IN g_Shared.Processors[CurrentProcessor].MtrrInfo, OUT & Private->Ept, OUT & Eptp);
    __vmx_vmwrite(VMX::VMCS_FIELD_EPT_POINTER_FULL, Eptp.Value);
    Private->EptInterceptor->CompleteInitialization(Eptp);
    //////////////////////////////////////////////////////////////////////////////////////////////////////CONTROLS
    Interceptions = reinterpret_cast<_Interceptions>(PInterceptions);
    Interceptions(VmxBasicInfo);
    //////////////////////////////////////////////////////////////////////////////////////////////////////CONTROLS
    unsigned long long SysenterCs = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_SYSENTER_CS));
    unsigned long long SysenterEsp = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_SYSENTER_ESP));
    unsigned long long SysenterEip = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_SYSENTER_EIP));

    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_IA32_SYSENTER_ESP, SysenterEsp);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_IA32_SYSENTER_EIP, SysenterEip);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_RSP, Context.Rsp);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_RIP, Context.Rip);
    __vmx_vmwrite(VMX::VMCS_FIELD_GUEST_RFLAGS, Context.EFlags);

    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_IA32_SYSENTER_CS, SysenterCs);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_IA32_SYSENTER_ESP, SysenterEsp);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_IA32_SYSENTER_EIP, SysenterEip);
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_RSP, reinterpret_cast<unsigned long long>(&Private->VmmStack.Layout.InitialStack));
    __vmx_vmwrite(VMX::VMCS_FIELD_HOST_RIP, reinterpret_cast<unsigned long long>(VmxVmmRun));

    InterlockedExchange(&IsVirtualized, TRUE);

    __vmx_vmlaunch();

    // If we're here - something went wrong:
    g_Shared.Processors[CurrentProcessor].Error = static_cast<VMX::VM_INSTRUCTION_ERROR>(vmread(VMX::VMCS_FIELD_VM_INSTRUCTION_ERROR));

    __vmx_off();

    return false;
}

bool HyperVisorVmx::VirtualizeAllProcessors()
{
    if (g_IsVirtualized) return true;

    // Virtualizing each processor:
    struct Callback_ {
        static bool inForEachCpu(void* Arg, unsigned int ProcessorNumber) {
            UNREFERENCED_PARAMETER(ProcessorNumber);
            return static_cast<HyperVisorVmx*>(Arg)->VirtualizeProcessor();
        }
        static bool inSystemContext(void* Arg) {
            UNREFERENCED_PARAMETER(Arg);

            g_Shared.KernelCr3 = __readcr3();

            // Determining the max phys size:
            CPUID::Intel::VIRTUAL_AND_PHYSICAL_ADDRESS_SIZES MaxAddrSizes = {};
            __cpuid(MaxAddrSizes.Regs.Raw, CPUID::Intel::CPUID_VIRTUAL_AND_PHYSICAL_ADDRESS_SIZES);

            // Initializing MTRRs shared between all processors:
            VMX::MTRR_INFO MtrrInfo;
            memset(&MtrrInfo, 0, sizeof(MtrrInfo));
            static_cast<HyperVisorVmx*>(Arg)->InitMtrr(&MtrrInfo);

            ULONG ProcessorsCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
            g_Shared.Processors = VirtualMemory::AllocArray<VMX::VCPU_INFO>(ProcessorsCount);
            for (ULONG i = 0; i < ProcessorsCount; ++i)
            {
                auto Proc = &g_Shared.Processors[i];
                Proc->VmData = reinterpret_cast<VMX::PRIVATE_VM_DATA*>(static_cast<HyperVisorVmx*>(Arg)->AllocPhys(sizeof(VMX::PRIVATE_VM_DATA), MmCached, MaxAddrSizes.Bitmap.PhysicalAddressBits));
                if (!g_Shared.Processors[i].VmData)
                {
                    for (ULONG j = 0; j < ProcessorsCount; ++j)
                    {
                        if (g_Shared.Processors[j].VmData)
                        {
                            static_cast<HyperVisorVmx*>(Arg)->FreePhys(g_Shared.Processors[j].VmData);
                        }
                    }
                    VirtualMemory::FreePoolMemory(g_Shared.Processors);
                    g_Shared.Processors = NULL;
                    return false;
                }
                Proc->MtrrInfo = &MtrrInfo;
                Proc->VmData->EptInterceptor = new EptHandler(&Proc->VmData->Ept);
            }

            //////////////////////////////////////////VM_EXIT
            static_cast<HyperVisorVmx*>(Arg)->InitHandlersTable = reinterpret_cast<_InitHandlersTable>(static_cast<HyperVisorVmx*>(Arg)->PInitHandlersTable);
            static_cast<HyperVisorVmx*>(Arg)->InitHandlersTable();
            //InitHandlersTable();
            //////////////////////////////////////////VM_EXIT

            Callable::ForEachCpu(&Callback_::inForEachCpu, Arg);

            bool Status = true;
            for (ULONG i = 0; i < ProcessorsCount; ++i)
            {
                Status &= g_Shared.Processors[i].Status;
                if (!Status)
                {
                    break;
                }
            }

            if (Status)
            {
                Ept::DbgPrintMtrrEptCacheLayout(&g_Shared.Processors[0].VmData->Ept, g_Shared.Processors[0].MtrrInfo);
            }
            else
            {
                static_cast<HyperVisorVmx*>(Arg)->DevirtualizeAllProcessors();
                VirtualMemory::FreePoolMemory(g_Shared.Processors);
                g_Shared.Processors = NULL;
            }

            return Status;
        }
    };

    bool Status = Callable::CallInSystemContext(&Callback_::inSystemContext, this);

    g_IsVirtualized = Status;

    return Status;
}

bool HyperVisorVmx::IsVmxSupported()
{
    CPUID_REGS Regs = {};

    // Check the 'GenuineIntel' vendor name:
    __cpuid(Regs.Raw, CPUID::Generic::CPUID_MAXIMUM_FUNCTION_NUMBER_AND_VENDOR_ID);
    if (Regs.Regs.Ebx != 'uneG' || Regs.Regs.Edx != 'Ieni' || Regs.Regs.Ecx != 'letn') return false;

    // Support by processor:
    __cpuid(Regs.Raw, CPUID::Intel::CPUID_FEATURE_INFORMATION);
    if (!reinterpret_cast<CPUID::FEATURE_INFORMATION*>(&Regs)->Intel.VMX) return false;

    // Check the VMX is locked in BIOS:
    Intel::IA32_FEATURE_CONTROL MsrFeatureControl = {};
    MsrFeatureControl.Value = __readmsr(static_cast<unsigned long>(Intel::INTEL_MSR::IA32_FEATURE_CONTROL));

    if (MsrFeatureControl.Bitmap.LockBit == FALSE) return false;

    return true;
}