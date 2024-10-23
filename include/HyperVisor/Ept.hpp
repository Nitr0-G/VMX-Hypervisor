#pragma once
#include "CommonTypes/PTE.hpp"
#include "CommonTypes/MSR.hpp"
#include "CommonTypes/AsmFuncs.hpp"
#include "CommonApi/MemoryUtils.hpp"
#include "CommonApi/PteUtils.hpp"

#include <fltkernel.h>
#include <unordered_map>

namespace VMX {
    // Extended-Page-Table Pointer:
    union EPTP {
        unsigned long long Value;
        struct {
            unsigned long long EptMemoryType : 3; // 0 = Uncacheable, 6 = Write-back, other values are reserved
            unsigned long long PageWalkLength : 3; // This value is 1 less than the EPT page-walk length
            unsigned long long AccessedAndDirtyFlagsSupport : 1; // Setting this control to 1 enables accessed and dirty flags for EPT (check IA32_VMX_EPT_VPID_CAP)
            unsigned long long EnforcementOfAccessRightsForSupervisorShadowStack : 1;
            unsigned long long Reserved0 : 4;
            unsigned long long EptPml4ePhysicalPfn : 52; // Maximum supported physical address width: CPUID(EAX = 0x80000008) -> EAX[7:0]
        } Bitmap;
    };
    static_assert(sizeof(EPTP) == sizeof(unsigned long long), "Size of EPTP != sizeof(unsigned long long)");

    // Describes 512-GByte region:
    union EPT_PML4E {
        unsigned long long Value;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Reserved0 : 5; // Must be zero
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Ignored0 : 1;
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Ignored1 : 1;
            unsigned long long EptPdptePhysicalPfn : 40; // Maximum supported physical address width: CPUID(EAX = 0x80000008) -> EAX[7:0]
            unsigned long long Ignored2 : 12;
        } Page1Gb, Page2Mb, Page4Kb, Generic;
    };
    static_assert(sizeof(EPT_PML4E) == sizeof(unsigned long long), "Size of EPT_PML4E != sizeof(unsigned long long)");

    union EPT_PDPTE {
        unsigned long long Value;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Reserved0 : 4;
            unsigned long long LargePage : 1;
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Reserved1 : 1;
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Reserved2 : 53;
        } Generic;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Type : 3;
            unsigned long long IgnorePat : 1;
            unsigned long long LargePage : 1; // Must be 1 (otherwise, this entry references an EPT page directory)
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Dirty : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Ignored0 : 1;
            unsigned long long Reserved0 : 18; // Must be zero
            unsigned long long PagePhysicalPfn : 22; // Physical address of the 1-GByte page referenced by this entry
            unsigned long long Ignored1 : 8;
            unsigned long long SupervisorShadowStackAccess : 1; // Ignored if bit 7 of EPTP is 0
            unsigned long long Ignored2 : 2;
            unsigned long long SuppressVe : 1; // Ignored if "EPT-violation #VE" VM-execution control is 0
        } Page1Gb;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Reserved0 : 5; // Must be zero
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Ignored0 : 1;
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Ignored1 : 1;
            unsigned long long EptPdePhysicalPfn : 40;
            unsigned long long Ignored2 : 12;
        } Page2Mb, Page4Kb;
    };
    static_assert(sizeof(EPT_PDPTE) == sizeof(unsigned long long), "Size of EPT_PDPTE != sizeof(unsigned long long)");

    union EPT_PDE {
        unsigned long long Value;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Reserved0 : 4; // Must be zero
            unsigned long long LargePage : 1;
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Reserved1 : 1;
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Reserved2 : 53;
        } Generic;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Type : 3;
            unsigned long long IgnorePat : 1;
            unsigned long long LargePage : 1; // Must be 1 (otherwise, this entry references an EPT page directory)
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Dirty : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Ignored0 : 1;
            unsigned long long Reserved0 : 9; // Must be zero
            unsigned long long PagePhysicalPfn : 31; // Physical address of the 2-MByte page referenced by this entry
            unsigned long long Ignored1 : 8;
            unsigned long long SupervisorShadowStackAccess : 1; // Ignored if bit 7 of EPTP is 0
            unsigned long long Ignored2 : 2;
            unsigned long long SuppressVe : 1; // Ignored if "EPT-violation #VE" VM-execution control is 0
        } Page2Mb;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Reserved0 : 4; // Must be zero
            unsigned long long LargePage : 1; // // Must be 0 (otherwise, this entry references a 2-MByte page)
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Ignored0 : 1;
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Ignored1 : 1;
            unsigned long long EptPtePhysicalPfn : 40;
            unsigned long long Ignored : 12;
        } Page4Kb;
    };
    static_assert(sizeof(EPT_PDE) == sizeof(unsigned long long), "Size of EPT_PDE != sizeof(unsigned long long)");

    union EPT_PTE {
        unsigned long long Value;
        struct {
            unsigned long long ReadAccess : 1;
            unsigned long long WriteAccess : 1;
            unsigned long long ExecuteAccess : 1;
            unsigned long long Type : 3;
            unsigned long long IgnorePat : 1;
            unsigned long long Ignored0 : 1;
            unsigned long long Accessed : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long Dirty : 1; // Ignored if bit 6 of EPTP is 0
            unsigned long long UserModeExecuteAccess : 1; // Ignored if "mode-based execute control for EPT" VM-execution control is 0
            unsigned long long Ignored1 : 1;
            unsigned long long PagePhysicalPfn : 40;
            unsigned long long Ignored2 : 8;
            unsigned long long SupervisorShadowStackAccess : 1; // Ignored if bit 7 of EPTP is 0
            unsigned long long SubPageWritePermissions : 1; // Ignored if "sub-page write permissions for EPT" VM-execution control is 0
            unsigned long long Ignored3 : 1;
            unsigned long long SuppressVe : 1; // Ignored if "EPT-violation #VE" VM-execution control is 0
        } Page4Kb;
    };
    static_assert(sizeof(EPT_PTE) == sizeof(unsigned long long), "Size of EPT_PTE != sizeof(unsigned long long)");

    struct EPT_TABLES
    {
        DECLSPEC_ALIGN(PAGE_SIZE) VMX::EPT_PML4E Pml4e;
        DECLSPEC_ALIGN(PAGE_SIZE) VMX::EPT_PDPTE Pdpte[512];
        DECLSPEC_ALIGN(PAGE_SIZE) VMX::EPT_PDE Pde[512][512];
        DECLSPEC_ALIGN(PAGE_SIZE) VMX::EPT_PTE PteForFirstLargePage[2 * 1048576 / 4096];
    };

    struct EPT_ENTRIES
    {
        EPT_PML4E* Pml4e;
        EPT_PDPTE* Pdpte;
        EPT_PDE* Pde;
        EPT_PTE* Pte;
    };

    struct LARGE_PAGE_LAYOUT
    {
        EPT_PTE Pte[512];
    };

    struct LARGE_PAGE_DESCRIPTOR
    {
        EPT_PDE* Pde;
        EPT_PDE OriginalPde;
        LARGE_PAGE_LAYOUT* Layout;
    };

    struct PAGE_HANDLER
    {
        EPT_PTE OnRead;
        EPT_PTE OnWrite;
        EPT_PTE OnExecute;
        EPT_PTE OnExecuteRead;
        EPT_PTE OnExecuteWrite;
    };

    struct EPT_PTE_HANDLER
    {
        EPT_PTE* Pte;
        PAGE_HANDLER Handlers;
    };

    enum class INVEPT_TYPE : unsigned int {
        SingleContextInvalidation = 1,
        GlobalInvalidation = 2
    };

    struct INVEPT_DESCRIPTOR {
        unsigned long long Eptp;
        unsigned long long Reserved;
    };
    static_assert(sizeof(INVEPT_DESCRIPTOR) == 2 * sizeof(unsigned long long), "Size of INVEPT_DESCRIPTOR != 2 * sizeof(unsigned long long)");

    struct MTRR_INFO
    {
        UINT64 MaxPhysAddrBits;
        UINT64 PhysAddrMask;
        Intel::IA32_VMX_EPT_VPID_CAP EptVpidCap;
        Intel::IA32_MTRRCAP MtrrCap;
        Intel::IA32_MTRR_DEF_TYPE MtrrDefType;

        // For the first 1 megabyte of the physical address space:
        union
        {
            Intel::MTRR_FIXED_GENERIC Generic[11];
            struct {
                // 512-Kbyte range:
                Intel::IA32_MTRR_FIX64K RangeFrom00000To7FFFF;

                // Two 128-Kbyte ranges:
                Intel::IA32_MTRR_FIX16K RangeFrom80000To9FFFF;
                Intel::IA32_MTRR_FIX16K RangeFromA0000ToBFFFF;

                // Eight 32-Kbyte ranges:
                Intel::IA32_MTRR_FIX4K RangeFromC0000ToC7FFF;
                Intel::IA32_MTRR_FIX4K RangeFromC8000ToCFFFF;
                Intel::IA32_MTRR_FIX4K RangeFromD0000ToD7FFF;
                Intel::IA32_MTRR_FIX4K RangeFromD8000ToDFFFF;
                Intel::IA32_MTRR_FIX4K RangeFromE0000ToE7FFF;
                Intel::IA32_MTRR_FIX4K RangeFromE8000ToEFFFF;
                Intel::IA32_MTRR_FIX4K RangeFromF0000ToF7FFF;
                Intel::IA32_MTRR_FIX4K RangeFromF8000ToFFFFF;
            } Ranges;
        } Fixed;

        // For the memory above the first megabyte of the physical address space:
        struct
        {
            Intel::IA32_MTRR_PHYSBASE PhysBase;
            Intel::IA32_MTRR_PHYSMASK PhysMask;
        } Variable[10];

        bool IsSupported;
    };

    struct MEMORY_RANGE
    {
        unsigned long long First;
        unsigned long long Last;
    };

    enum class INVVPID_TYPE : unsigned int {
        IndividualAddressInvalidation = 0,
        SingleContextInvalidation = 1,
        AllContextsInvalidation = 2,
        SingleContextInvalidationExceptGlobal = 3
    };

    struct INVVPID_DESCRIPTOR {
        unsigned long long Vpid : 16;
        unsigned long long Reserved : 48;
        unsigned long long LinearAddress;
    };
    static_assert(sizeof(INVVPID_DESCRIPTOR) == 2 * sizeof(unsigned long long), "Size of INVVPID_DESCRIPTOR != 2 * sizeof(unsigned long long)");
}

namespace Ept {
    void DbgPrintMtrrEptCacheLayout(
        __in const VMX::EPT_TABLES* Ept,
        __in const VMX::MTRR_INFO* MtrrInfo);

    void InitializeEptTables(
        __in const VMX::MTRR_INFO* MtrrInfo,
        __out VMX::EPT_TABLES* Ept,
        __out VMX::EPTP* Eptp);

    bool GetEptEntries(
        unsigned long long PhysicalAddress,
        const VMX::EPT_TABLES& Ept,
        __out VMX::EPT_ENTRIES& Entries);

    void BuildPtesForPde(
        __in const VMX::EPT_PDE& Pde,
        __out VMX::LARGE_PAGE_LAYOUT& Ptes);

    void BuildPageHandler(
        Intel::MTRR_MEMORY_TYPE CacheType,
        UINT64 ReadPa,
        UINT64 WritePa,
        UINT64 ExecutePa,
        UINT64 ExecuteReadPa,
        UINT64 ExecuteWritePa,
        __out VMX::PAGE_HANDLER& Handler);
}

class EptHandler final
{
private:
    VMX::EPT_TABLES* m_Ept;
    std::unordered_map<uint64_t, VMX::LARGE_PAGE_DESCRIPTOR> m_PageDescriptors; // Guest PA (aligned by 2Mb) -> 4Kb PTEs describing a 2Mb page
    std::unordered_map<uint64_t, VMX::EPT_PTE_HANDLER> m_Handlers; // Guest PA (aligned by 4Kb) -> Page handlers (EPT entries for RWX)
    VMX::INVEPT_DESCRIPTOR m_InveptDescriptor;

    struct
    {
        const void* Rip;
        VMX::EPT_PTE* Pte;
        VMX::EPT_PTE PendingPrevEntry;
    } m_PendingHandler;

    constexpr static unsigned int PageSize = 4096;
    constexpr static unsigned int LargePageSize = 2 * 1048576;

    inline void invept()
    {
        __invept((unsigned int)VMX::INVEPT_TYPE::SingleContextInvalidation, &m_InveptDescriptor);
    }
    
    PVOID AllocPhys(
        _In_ SIZE_T Size,
        _In_ MEMORY_CACHING_TYPE CachingType,
        _In_ ULONG MaxPhysBits);

    void FreePhys(_In_ PVOID Memory);

public:
    EptHandler(__in VMX::EPT_TABLES* Ept)
        : m_Ept(Ept)
        , m_PendingHandler({})
        , m_InveptDescriptor({})
    {}

    ~EptHandler()
    {
        for (auto& [_, Desc] : m_PageDescriptors)
        {
            *Desc.Pde = Desc.OriginalPde;
            FreePhys(Desc.Layout);
        }
    }

    void CompleteInitialization(VMX::EPTP Eptp);

    void InterceptPage(
        unsigned long long Pa,
        unsigned long long ReadPa,
        unsigned long long WritePa,
        unsigned long long ExecutePa,
        unsigned long long ExecuteReadPa,
        unsigned long long ExecuteWritePa
    );

    void DeinterceptPage(unsigned long long Pa);

    bool HandleRead(unsigned long long Pa);

    bool HandleWrite(unsigned long long Pa, const void* NextInstruction);

    bool HandleExecute(unsigned long long Pa);

    bool HandleExecuteRead(unsigned long long Pa, const void* NextInstruction);

    bool HandleExecuteWrite(unsigned long long Pa, const void* NextInstruction);

    bool CompletePendingHandler(const void* Rip);
};