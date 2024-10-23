#include "Ept.hpp"

static bool AreRangesIntersects(
    const VMX::MEMORY_RANGE& Range1,
    const VMX::MEMORY_RANGE& Range2)
{
    return Range1.First <= Range2.Last && Range1.Last >= Range2.First;
}

static bool MixMtrrTypes(
    Intel::MTRR_MEMORY_TYPE Type1,
    Intel::MTRR_MEMORY_TYPE Type2,
    __out Intel::MTRR_MEMORY_TYPE& Mixed)
{
    Mixed = Intel::MTRR_MEMORY_TYPE::Uncacheable;

    if (Type1 == Intel::MTRR_MEMORY_TYPE::Uncacheable || Type2 == Intel::MTRR_MEMORY_TYPE::Uncacheable)
    {
        Mixed = Intel::MTRR_MEMORY_TYPE::Uncacheable;
        return true;
    }

    if (Type1 == Type2)
    {
        Mixed = Type1;
        return true;
    }
    else
    {
        if ((Type1 == Intel::MTRR_MEMORY_TYPE::WriteThrough || Type1 == Intel::MTRR_MEMORY_TYPE::WriteBack)
            && (Type2 == Intel::MTRR_MEMORY_TYPE::WriteThrough || Type2 == Intel::MTRR_MEMORY_TYPE::WriteBack))
        {
            Mixed = Intel::MTRR_MEMORY_TYPE::WriteThrough;
            return true;
        }
    }

    return false; // Memory types are conflicting, returning Uncacheable
}

static Intel::MTRR_MEMORY_TYPE CalcMemoryTypeByFixedMtrr(
    Intel::MTRR_FIXED_GENERIC FixedMtrrGeneric,
    const VMX::MEMORY_RANGE& MtrrRange,
    const VMX::MEMORY_RANGE& PhysRange
) {
    bool Initialized = false;
    Intel::MTRR_MEMORY_TYPE MemType = Intel::MTRR_MEMORY_TYPE::Uncacheable;

    constexpr unsigned long long RangeBitsMask = 0b1111'1111;
    constexpr unsigned long long RangeBitsCount = 8;
    constexpr unsigned long long RangesCount = (sizeof(FixedMtrrGeneric) * 8) / RangeBitsCount;
    const unsigned long long SubrangeSize = (MtrrRange.Last - MtrrRange.First + 1) / RangeBitsCount;

    for (unsigned int i = 0; i < RangesCount; ++i)
    {
        VMX::MEMORY_RANGE Subrange;
        Subrange.First = MtrrRange.First + i * SubrangeSize;
        Subrange.Last = Subrange.First + SubrangeSize - 1;

        if (AreRangesIntersects(PhysRange, Subrange))
        {
            Intel::MTRR_MEMORY_TYPE SubrangeType = static_cast<Intel::MTRR_MEMORY_TYPE>((FixedMtrrGeneric.Value >> (i * RangeBitsCount)) & RangeBitsMask);
            if (Initialized)
            {
                bool MixingStatus = MixMtrrTypes(MemType, SubrangeType, OUT MemType);
                if (!MixingStatus)
                {
                    // Cache types are conflicting in overlapped regions, returning Uncacheable:
                    MemType = Intel::MTRR_MEMORY_TYPE::Uncacheable;
                }
            }
            else
            {
                MemType = SubrangeType;
                Initialized = true;
            }

            // If at least one range is Uncacheable - then
            // all overlapped ranges are Uncacheable:
            if (MemType == Intel::MTRR_MEMORY_TYPE::Uncacheable)
            {
                break;
            }
        }
    }

    return MemType;
}

static const VMX::MEMORY_RANGE FixedRanges[] = {
    { 0x00000, 0x7FFFF },
    { 0x80000, 0x9FFFF },
    { 0xA0000, 0xBFFFF },
    { 0xC0000, 0xC7FFF },
    { 0xC8000, 0xCFFFF },
    { 0xD0000, 0xD7FFF },
    { 0xD8000, 0xDFFFF },
    { 0xE0000, 0xE7FFF },
    { 0xE8000, 0xEFFFF },
    { 0xF0000, 0xF7FFF },
    { 0xF8000, 0xFFFFF },
};

static Intel::MTRR_MEMORY_TYPE GetMtrrMemoryType(
    __in const  VMX::MTRR_INFO* MtrrInfo, 
    unsigned long long PhysicalAddress,
    unsigned int PageSize)
{
    if (!MtrrInfo || !PageSize || !MtrrInfo->MtrrDefType.Bitmap.E)
        return  Intel::MTRR_MEMORY_TYPE::Uncacheable;

    constexpr unsigned long long FIRST_MEGABYTE = 0x100000ULL;

    VMX::MEMORY_RANGE PhysRange = {};
    PhysRange.First = PhysicalAddress;
    PhysRange.Last = PhysicalAddress + PageSize - 1;

    bool IsMemTypeInitialized = false;

    // Default type:
    Intel::MTRR_MEMORY_TYPE MemType = static_cast<Intel::MTRR_MEMORY_TYPE>(MtrrInfo->MtrrDefType.Bitmap.Type);

    if (PhysicalAddress < FIRST_MEGABYTE && MtrrInfo->MtrrCap.Bitmap.FIX && MtrrInfo->MtrrDefType.Bitmap.FE)
    {
        for (unsigned int i = 0; i < ARRAYSIZE(FixedRanges); ++i)
        {
            Intel::MTRR_FIXED_GENERIC MtrrFixedGeneric = {};
            MtrrFixedGeneric.Value = MtrrInfo->Fixed.Generic[i].Value;
            if (AreRangesIntersects(PhysRange, FixedRanges[i]))
            {
                Intel::MTRR_MEMORY_TYPE FixedMemType = CalcMemoryTypeByFixedMtrr(MtrrFixedGeneric, FixedRanges[i], PhysRange);
                if (FixedMemType == Intel::MTRR_MEMORY_TYPE::Uncacheable) return FixedMemType;
                if (IsMemTypeInitialized)
                {
                    bool IsMixed = MixMtrrTypes(MemType, FixedMemType, OUT MemType);
                    if (!IsMixed)
                    {
                        return  Intel::MTRR_MEMORY_TYPE::Uncacheable;
                    }
                }
                else
                {
                    IsMemTypeInitialized = true;
                    MemType = FixedMemType;
                }
            }
        }
    }

    for (unsigned int i = 0; i < MtrrInfo->MtrrCap.Bitmap.VCNT; ++i)
    {
        // If this entry is valid:
        if (!MtrrInfo->Variable[i].PhysMask.Bitmap.V) continue;

        unsigned long long MtrrPhysBase = PFN_TO_PAGE(MtrrInfo->Variable[i].PhysBase.Bitmap.PhysBasePfn);
        unsigned long long MtrrPhysMask = PFN_TO_PAGE(MtrrInfo->Variable[i].PhysMask.Bitmap.PhysMaskPfn) | MtrrInfo->PhysAddrMask;
        unsigned long long MaskedMtrrPhysBase = MtrrPhysBase & MtrrPhysMask;
        Intel::MTRR_MEMORY_TYPE VarMemType = Intel::MTRR_MEMORY_TYPE::Uncacheable;
        bool IsVarMemTypeInitialized = false;
        for (unsigned long long Page = PhysicalAddress; Page < PhysicalAddress + PageSize; Page += PAGE_SIZE)
        {
            if ((Page & MtrrPhysMask) == MaskedMtrrPhysBase)
            {
                auto PageMemType = static_cast<Intel::MTRR_MEMORY_TYPE>(MtrrInfo->Variable[i].PhysBase.Bitmap.Type);
                if (IsVarMemTypeInitialized)
                {
                    bool IsMixed = MixMtrrTypes(VarMemType, PageMemType, OUT VarMemType);
                    if (!IsMixed)
                    {
                        return  Intel::MTRR_MEMORY_TYPE::Uncacheable;
                    }
                }
                else
                {
                    VarMemType = PageMemType;
                    IsVarMemTypeInitialized = true;
                }

                if (VarMemType == Intel::MTRR_MEMORY_TYPE::Uncacheable)
                {
                    return  Intel::MTRR_MEMORY_TYPE::Uncacheable;
                }
            }
        }

        if (IsVarMemTypeInitialized)
        {
            if (VarMemType == Intel::MTRR_MEMORY_TYPE::Uncacheable)
            {
                return  Intel::MTRR_MEMORY_TYPE::Uncacheable;
            }

            if (IsMemTypeInitialized)
            {
                bool IsMixed = MixMtrrTypes(MemType, VarMemType, OUT MemType);
                if (!IsMixed)
                {
                    return Intel::MTRR_MEMORY_TYPE::Uncacheable;
                }
            }
            else
            {
                MemType = VarMemType;
                IsMemTypeInitialized = true;
            }
        }
    }

    return MemType;
}

void Ept::DbgPrintMtrrEptCacheLayout(
    __in const VMX::EPT_TABLES* Ept, 
    __in const VMX::MTRR_INFO* MtrrInfo)
{
    auto MemTypeToStr = [](Intel::MTRR_MEMORY_TYPE MemType) -> const char*
        {
            switch (MemType)
            {
            case Intel::MTRR_MEMORY_TYPE::Uncacheable: return "Uncacheable (0)";
            case Intel::MTRR_MEMORY_TYPE::WriteCombining: return "WriteCombining (1)";
            case Intel::MTRR_MEMORY_TYPE::WriteThrough: return "WriteThrough (4)";
            case Intel::MTRR_MEMORY_TYPE::WriteProtected: return "WriteProtected (5)";
            case Intel::MTRR_MEMORY_TYPE::WriteBack: return "WriteBack (6)";
            default:
                return "Unknown";
            }
        };

    Intel::MTRR_MEMORY_TYPE CurrentRangeType = Intel::MTRR_MEMORY_TYPE::Uncacheable;
    unsigned long long RangeBeginning = 0;
    for (unsigned int i = 0; i < _ARRAYSIZE(Ept->Pdpte); ++i)
    {
        for (unsigned int j = 0; j < _ARRAYSIZE(Ept->Pde[i]); ++j)
        {
            if (i == 0 && j == 0)
            {
                for (unsigned int k = 0; k < _ARRAYSIZE(Ept->PteForFirstLargePage); ++k)
                {
                    auto Page = Ept->PteForFirstLargePage[k].Page4Kb;
                    Intel::MTRR_MEMORY_TYPE MemType = static_cast<Intel::MTRR_MEMORY_TYPE>(Page.Type);
                    unsigned long long PagePa = Page.PagePhysicalPfn * PAGE_SIZE;
                    if (MemType != CurrentRangeType)
                    {
                        if ((PagePa - RangeBeginning) > 0)
                        {
                            DbgPrint("Physical range [%p..%p]: %s\r\n", reinterpret_cast<void*>(RangeBeginning), reinterpret_cast<void*>(PagePa - 1), MemTypeToStr(CurrentRangeType));
                        }
                        CurrentRangeType = MemType;
                        RangeBeginning = PagePa;
                    }
                }
            }
            else
            {
                constexpr unsigned long long PageSize = 2 * 1048576; // 2 Mb

                auto Page = Ept->Pde[i][j].Page2Mb;
                Intel::MTRR_MEMORY_TYPE MemType = static_cast<Intel::MTRR_MEMORY_TYPE>(Page.Type);
                unsigned long long PagePa = Page.PagePhysicalPfn * PageSize;
                if (MemType != CurrentRangeType)
                {
                    if ((PagePa - RangeBeginning) > 0)
                    {
                        DbgPrint("Physical range [%p..%p]: %s\r\n", reinterpret_cast<void*>(RangeBeginning), reinterpret_cast<void*>(PagePa - 1), MemTypeToStr(CurrentRangeType));
                    }
                    CurrentRangeType = MemType;
                    RangeBeginning = PagePa;
                }
            }
        }
    }

    DbgPrint("Physical range [%p..%p]: %s\r\n", reinterpret_cast<void*>(RangeBeginning), reinterpret_cast<void*>(512ull * 1024ull * 1048576ull - 1ull), MemTypeToStr(CurrentRangeType));

    DbgPrint("EptVpidCap      : 0x%I64X\r\n", MtrrInfo->EptVpidCap.Value);
    DbgPrint("MaxPhysAddrBits : 0x%I64X\r\n", MtrrInfo->MaxPhysAddrBits);
    DbgPrint("MtrrCap         : 0x%I64X\r\n", MtrrInfo->MtrrCap.Value);
    DbgPrint("MtrrDefType     : 0x%I64X\r\n", MtrrInfo->MtrrDefType.Value);
    DbgPrint("PhysAddrMask    : 0x%I64X\r\n", MtrrInfo->PhysAddrMask);
    DbgPrint("MTRR.Fixed [00000..7FFFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFrom00000To7FFFF.Value);
    DbgPrint("MTRR.Fixed [80000..9FFFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFrom80000To9FFFF.Value);
    DbgPrint("MTRR.Fixed [A0000..BFFFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromA0000ToBFFFF.Value);
    DbgPrint("MTRR.Fixed [C0000..C7FFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromC0000ToC7FFF.Value);
    DbgPrint("MTRR.Fixed [C8000..CFFFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromC8000ToCFFFF.Value);
    DbgPrint("MTRR.Fixed [D0000..D7FFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromD0000ToD7FFF.Value);
    DbgPrint("MTRR.Fixed [D8000..DFFFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromD8000ToDFFFF.Value);
    DbgPrint("MTRR.Fixed [E0000..E7FFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromE0000ToE7FFF.Value);
    DbgPrint("MTRR.Fixed [E8000..EFFFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromE8000ToEFFFF.Value);
    DbgPrint("MTRR.Fixed [F0000..F7FFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromF0000ToF7FFF.Value);
    DbgPrint("MTRR.Fixed [F8000..FFFFF]: 0x%I64X\r\n", MtrrInfo->Fixed.Ranges.RangeFromF8000ToFFFFF.Value);

    for (unsigned int i = 0; i < _ARRAYSIZE(MtrrInfo->Variable); ++i)
    {
        DbgPrint("MTRR.Variable[%u]: Base: 0x%I64X, Mask: 0x%I64X\r\n", i, MtrrInfo->Variable[i].PhysBase.Value, MtrrInfo->Variable[i].PhysMask.Value);
    }
}

void Ept::InitializeEptTables(
    __in const VMX::MTRR_INFO* MtrrInfo,
    __out VMX::EPT_TABLES* Ept,
    __out VMX::EPTP* Eptp)
{
    using namespace PhysicalMemory;

    memset(Ept, 0, sizeof(VMX::EPT_TABLES));
    memset(Eptp, 0, sizeof(VMX::EPTP));

    PVOID64 Pml4ePhys = GetPhysicalAddress(&Ept->Pml4e);
    Eptp->Bitmap.EptMemoryType = static_cast<unsigned char>(Intel::MTRR_MEMORY_TYPE::WriteBack);
    Eptp->Bitmap.PageWalkLength = 3;
    Eptp->Bitmap.AccessedAndDirtyFlagsSupport = FALSE;
    Eptp->Bitmap.EptPml4ePhysicalPfn = PAGE_TO_PFN(reinterpret_cast<UINT64>(Pml4ePhys));

    PVOID64 PdptePhys = GetPhysicalAddress(Ept->Pdpte);
    Ept->Pml4e.Page2Mb.ReadAccess = TRUE;
    Ept->Pml4e.Page2Mb.WriteAccess = TRUE;
    Ept->Pml4e.Page2Mb.ExecuteAccess = TRUE;
    Ept->Pml4e.Page2Mb.EptPdptePhysicalPfn = PAGE_TO_PFN(reinterpret_cast<UINT64>(PdptePhys));

    for (unsigned int i = 0; i < _ARRAYSIZE(Ept->Pdpte); ++i)
    {
        PVOID64 PdePhys = GetPhysicalAddress(Ept->Pde[i]);
        Ept->Pdpte[i].Page2Mb.ReadAccess = TRUE;
        Ept->Pdpte[i].Page2Mb.WriteAccess = TRUE;
        Ept->Pdpte[i].Page2Mb.ExecuteAccess = TRUE;
        Ept->Pdpte[i].Page2Mb.EptPdePhysicalPfn = PAGE_TO_PFN(reinterpret_cast<UINT64>(PdePhys));

        for (unsigned int j = 0; j < _ARRAYSIZE(Ept->Pde[i]); ++j)
        {
            if (i == 0 && j == 0)
            {
                PVOID64 PtePhys = GetPhysicalAddress(Ept->PteForFirstLargePage);
                Ept->Pde[i][j].Page4Kb.ReadAccess = TRUE;
                Ept->Pde[i][j].Page4Kb.WriteAccess = TRUE;
                Ept->Pde[i][j].Page4Kb.ExecuteAccess = TRUE;
                Ept->Pde[i][j].Page4Kb.EptPtePhysicalPfn = PAGE_TO_PFN(reinterpret_cast<UINT64>(PtePhys));

                for (unsigned int k = 0; k < _ARRAYSIZE(Ept->PteForFirstLargePage); ++k)
                {
                    Intel::MTRR_MEMORY_TYPE MemType = Intel::MTRR_MEMORY_TYPE::Uncacheable;
                    if (MtrrInfo->IsSupported)
                    {
                        MemType = GetMtrrMemoryType(MtrrInfo, PFN_TO_PAGE(static_cast<unsigned long long>(k)), PAGE_SIZE);
                    }

                    Ept->PteForFirstLargePage[k].Page4Kb.ReadAccess = TRUE;
                    Ept->PteForFirstLargePage[k].Page4Kb.WriteAccess = TRUE;
                    Ept->PteForFirstLargePage[k].Page4Kb.ExecuteAccess = TRUE;
                    Ept->PteForFirstLargePage[k].Page4Kb.Type = static_cast<unsigned char>(MemType);
                    Ept->PteForFirstLargePage[k].Page4Kb.PagePhysicalPfn = k;
                }
            }
            else
            {
                unsigned long long PagePfn = i * _ARRAYSIZE(Ept->Pde[i]) + j;
                constexpr unsigned long long LargePageSize = 2 * 1048576; // 2 Mb

                Intel::MTRR_MEMORY_TYPE MemType = Intel::MTRR_MEMORY_TYPE::Uncacheable;
                if (MtrrInfo->IsSupported)
                {
                    MemType = GetMtrrMemoryType(MtrrInfo, PFN_TO_LARGE_PAGE(PagePfn), LargePageSize);
                }

                Ept->Pde[i][j].Page2Mb.ReadAccess = TRUE;
                Ept->Pde[i][j].Page2Mb.WriteAccess = TRUE;
                Ept->Pde[i][j].Page2Mb.ExecuteAccess = TRUE;
                Ept->Pde[i][j].Page2Mb.Type = static_cast<unsigned char>(MemType);
                Ept->Pde[i][j].Page2Mb.LargePage = TRUE;
                Ept->Pde[i][j].Page2Mb.PagePhysicalPfn = PagePfn;
            }
        }
    }
}

bool Ept::GetEptEntries(
    unsigned long long PhysicalAddress,
    const VMX::EPT_TABLES& Ept,
    __out VMX::EPT_ENTRIES& Entries)
{
    VIRTUAL_ADDRESS Addr;
    Addr.x64.Value = PhysicalAddress;

    // Our EPT supports only one PML4E (512 Gb of the physical address space):
    if (Addr.x64.Generic.PageMapLevel4Offset > 0)
    {
        __stosq(reinterpret_cast<unsigned long long*>(&Entries), 0, sizeof(Entries) / sizeof(unsigned long long));
        return false;
    }

    auto PdpteIndex = Addr.x64.NonPageSize.Generic.PageDirectoryPointerOffset;
    auto PdeIndex = Addr.x64.NonPageSize.Generic.PageDirectoryOffset;

    Entries.Pml4e = const_cast<VMX::EPT_PML4E*>(&Ept.Pml4e);
    Entries.Pdpte = const_cast<VMX::EPT_PDPTE*>(&Ept.Pdpte[PdpteIndex]);
    Entries.Pde = const_cast<VMX::EPT_PDE*>(&Ept.Pde[PdpteIndex][PdeIndex]);
    if (Entries.Pde->Generic.LargePage)
    {
        Entries.Pte = reinterpret_cast<VMX::EPT_PTE*>(NULL);
    }
    else
    {
        PVOID64 PtPhys = reinterpret_cast<PVOID64>(PFN_TO_PAGE(Entries.Pde->Page4Kb.EptPtePhysicalPfn));
        PVOID PtVa = PhysicalMemory::GetVirtualForPhysical(PtPhys);
        Entries.Pte = &((reinterpret_cast<VMX::EPT_PTE*>(PtVa))[Addr.x64.NonPageSize.Page4Kb.PageTableOffset]);
    }

    return true;
}

void Ept::BuildPtesForPde(
    __in const VMX::EPT_PDE& Pde,
    __out VMX::LARGE_PAGE_LAYOUT& Ptes)
{
    for (unsigned int i = 0; i < ARRAYSIZE(Ptes.Pte); ++i)
    {
        Ptes.Pte[i].Page4Kb.ReadAccess = TRUE;
        Ptes.Pte[i].Page4Kb.WriteAccess = TRUE;
        Ptes.Pte[i].Page4Kb.ExecuteAccess = TRUE;
        Ptes.Pte[i].Page4Kb.Type = Pde.Page2Mb.Type;
        Ptes.Pte[i].Page4Kb.PagePhysicalPfn = PAGE_TO_PFN(PFN_TO_LARGE_PAGE(Pde.Page2Mb.PagePhysicalPfn)) + i;
    }
}

void Ept::BuildPageHandler(
    Intel::MTRR_MEMORY_TYPE CacheType, 
    UINT64 ReadPa, 
    UINT64 WritePa,
    UINT64 ExecutePa,
    UINT64 ExecuteReadPa,
    UINT64 ExecuteWritePa,
    __out VMX::PAGE_HANDLER& Handler)
{
    Handler.OnRead.Value = 0;
    Handler.OnWrite.Value = 0;
    Handler.OnExecute.Value = 0;
    Handler.OnExecuteRead.Value = 0;
    Handler.OnExecuteWrite.Value = 0;

    if (ReadPa)
    {
        Handler.OnRead.Page4Kb.ReadAccess = TRUE;
        Handler.OnRead.Page4Kb.Type = static_cast<unsigned long long>(CacheType);
        Handler.OnRead.Page4Kb.PagePhysicalPfn = PAGE_TO_PFN(ReadPa);
    }

    if (WritePa)
    {
        // We're unable to make a write-only page:
        Handler.OnWrite.Page4Kb.ReadAccess = TRUE; // Without it we will get the EPT_MISCONFIGURATION error
        Handler.OnWrite.Page4Kb.WriteAccess = TRUE;
        Handler.OnWrite.Page4Kb.Type = static_cast<unsigned long long>(CacheType);
        Handler.OnWrite.Page4Kb.PagePhysicalPfn = PAGE_TO_PFN(WritePa);
    }

    if (ExecutePa)
    {
        Handler.OnExecute.Page4Kb.ExecuteAccess = TRUE;
        Handler.OnExecute.Page4Kb.Type = static_cast<unsigned long long>(CacheType);
        Handler.OnExecute.Page4Kb.PagePhysicalPfn = PAGE_TO_PFN(ExecutePa);
    }

    if (ExecuteReadPa)
    {
        Handler.OnExecuteRead.Page4Kb.ReadAccess = TRUE;
        Handler.OnExecuteRead.Page4Kb.ExecuteAccess = TRUE;
        Handler.OnExecuteRead.Page4Kb.Type = static_cast<unsigned long long>(CacheType);
        Handler.OnExecuteRead.Page4Kb.PagePhysicalPfn = PAGE_TO_PFN(ExecuteReadPa);
    }

    if (ExecuteWritePa)
    {
        Handler.OnExecuteWrite.Page4Kb.ReadAccess = TRUE;
        Handler.OnExecuteWrite.Page4Kb.WriteAccess = TRUE;
        Handler.OnExecuteWrite.Page4Kb.ExecuteAccess = TRUE;
        Handler.OnExecuteWrite.Page4Kb.Type = static_cast<unsigned long long>(CacheType);
        Handler.OnExecuteWrite.Page4Kb.PagePhysicalPfn = PAGE_TO_PFN(ExecuteWritePa);
    }

    if (ReadPa && (ReadPa == WritePa))
    {
        Handler.OnRead.Page4Kb.WriteAccess = TRUE;
        Handler.OnWrite.Page4Kb.ReadAccess = TRUE;
    }

    if (WritePa && (WritePa == ExecutePa))
    {
        Handler.OnWrite.Page4Kb.ExecuteAccess = TRUE;
        Handler.OnExecute.Page4Kb.WriteAccess = TRUE;
    }

    if (ExecutePa && (ReadPa == ExecutePa))
    {
        Handler.OnRead.Page4Kb.ExecuteAccess = TRUE;
        Handler.OnExecute.Page4Kb.ReadAccess = TRUE;
    }

    if (ExecuteReadPa && (ExecuteReadPa == ExecuteWritePa))
    {
        Handler.OnExecuteRead.Page4Kb.WriteAccess = TRUE;
    }
}

PVOID EptHandler::AllocPhys(
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

void EptHandler::FreePhys(_In_ PVOID Memory)
{
	PhysicalMemory::FreePhysicalMemory(Memory);
}

void EptHandler::CompleteInitialization(VMX::EPTP Eptp)
{
	m_InveptDescriptor.Eptp = Eptp.Value;
}

void EptHandler::InterceptPage(
    unsigned long long Pa,
    unsigned long long ReadPa,
    unsigned long long WritePa,
    unsigned long long ExecutePa,
    unsigned long long ExecuteReadPa,
    unsigned long long ExecuteWritePa
) {
    unsigned long long Pa4Kb = ALIGN_DOWN_BY(Pa, PageSize);
    auto HandlerEntry = m_Handlers.find(Pa4Kb);
    if (HandlerEntry != m_Handlers.end())
    {
        VMX::PAGE_HANDLER Handler = {};
        Ept::BuildPageHandler(static_cast<Intel::MTRR_MEMORY_TYPE>(HandlerEntry->second.Handlers.OnRead.Page4Kb.Type), ReadPa, WritePa, ExecutePa, ExecuteReadPa, ExecuteWritePa, OUT Handler);
        HandlerEntry->second.Handlers = Handler;
        *HandlerEntry->second.Pte = Handler.OnRead;
        invept();
        return;
    }

    unsigned long long Pa2Mb = ALIGN_DOWN_BY(Pa, LargePageSize);
    auto DescriptorEntry = m_PageDescriptors.find(Pa2Mb);
    if (DescriptorEntry == m_PageDescriptors.end())
    {
        VMX::EPT_ENTRIES EptEntries;
        Ept::GetEptEntries(Pa2Mb, *m_Ept, OUT EptEntries);

        VMX::LARGE_PAGE_DESCRIPTOR Desc = {
            .Pde = EptEntries.Pde,
            .OriginalPde = *EptEntries.Pde,
            .Layout = reinterpret_cast<VMX::LARGE_PAGE_LAYOUT*>(AllocPhys(sizeof(VMX::LARGE_PAGE_LAYOUT)))
        };

        Ept::BuildPtesForPde(Desc.OriginalPde, OUT * Desc.Layout);

        DescriptorEntry = m_PageDescriptors.emplace(Pa2Mb, Desc).first;
    }

    auto& Descriptor = DescriptorEntry->second;
    auto* Pde = &Descriptor.Pde->Page4Kb;
    Pde->LargePage = FALSE;
    Pde->Reserved0 = 0;
    Pde->EptPtePhysicalPfn = PAGE_TO_PFN(reinterpret_cast<uint64_t>(PhysicalMemory::GetPhysicalAddress(Descriptor.Layout)));

    unsigned long long PteIndex = (Pa - Pa2Mb) / PageSize;
    VMX::EPT_PTE* Pte = &Descriptor.Layout->Pte[PteIndex];

    VMX::EPT_PTE_HANDLER Handler = {};
    Ept::BuildPageHandler(static_cast<Intel::MTRR_MEMORY_TYPE>(DescriptorEntry->second.OriginalPde.Page2Mb.Type), ReadPa, WritePa, ExecutePa, ExecuteReadPa, ExecuteWritePa, OUT Handler.Handlers);
    Handler.Pte = Pte;
    m_Handlers.emplace(Pa4Kb, Handler);

    *Pte = Handler.Handlers.OnRead;
    invept();
}

void EptHandler::DeinterceptPage(unsigned long long Pa)
{
    unsigned long long Pa4Kb = ALIGN_DOWN_BY(Pa, PageSize);
    auto HandlerEntry = m_Handlers.find(Pa4Kb);
    if (HandlerEntry != m_Handlers.end())
    {
        m_Handlers.erase(HandlerEntry);
    }

    if (m_Handlers.empty())
    {
        unsigned long long Pa2Mb = ALIGN_DOWN_BY(Pa, LargePageSize);
        auto DescriptorEntry = m_PageDescriptors.find(Pa2Mb);
        if (DescriptorEntry != m_PageDescriptors.end())
        {
            auto& Desc = DescriptorEntry->second;
            *Desc.Pde = Desc.OriginalPde;
            FreePhys(Desc.Layout);
            m_PageDescriptors.erase(DescriptorEntry);
            invept();
        }
    }
}

bool EptHandler::HandleRead(unsigned long long Pa)
{
    unsigned long long Pa4Kb = ALIGN_DOWN_BY(Pa, PageSize);
    auto Handler = m_Handlers.find(Pa4Kb);
    if (Handler == m_Handlers.end())
    {
        return false;
    }

    auto& Entry = Handler->second;
    auto PteEntry = Entry.Handlers.OnRead;
    if (!PteEntry.Value)
    {
        return false;
    }

    *Entry.Pte = PteEntry;
    invept();

    return true;
}

bool EptHandler::HandleWrite(
    unsigned long long Pa, 
    const void* NextInstruction)
{
    unsigned long long Pa4Kb = ALIGN_DOWN_BY(Pa, PageSize);
    auto Handler = m_Handlers.find(Pa4Kb);
    if (Handler == m_Handlers.end())
    {
        return false;
    }

    auto& HandlerEntry = Handler->second;
    if (!HandlerEntry.Handlers.OnWrite.Value)
    {
        return false;
    }

    auto* Pte = HandlerEntry.Pte;

    m_PendingHandler.Rip = NextInstruction;
    m_PendingHandler.Pte = Pte;
    m_PendingHandler.PendingPrevEntry = *Pte;

    *Pte = HandlerEntry.Handlers.OnWrite;
    invept();

    return true;
}

bool EptHandler::HandleExecute(unsigned long long Pa)
{
    unsigned long long Pa4Kb = ALIGN_DOWN_BY(Pa, PageSize);
    auto Handler = m_Handlers.find(Pa4Kb);
    if (Handler == m_Handlers.end())
    {
        return false;
    }

    auto& Entry = Handler->second;
    auto PteEntry = Entry.Handlers.OnExecute;
    if (!PteEntry.Value)
    {
        return false;
    }

    *Entry.Pte = PteEntry;
    invept();

    return true;
}

bool EptHandler::HandleExecuteRead(
    unsigned long long Pa, 
    const void* NextInstruction)
{
    unsigned long long Pa4Kb = ALIGN_DOWN_BY(Pa, PageSize);
    auto Handler = m_Handlers.find(Pa4Kb);
    if (Handler == m_Handlers.end())
    {
        return false;
    }

    auto& HandlerEntry = Handler->second;
    if (!HandlerEntry.Handlers.OnExecuteRead.Value)
    {
        return false;
    }

    auto* Pte = HandlerEntry.Pte;

    m_PendingHandler.Rip = NextInstruction;
    m_PendingHandler.Pte = Pte;
    m_PendingHandler.PendingPrevEntry = *Pte;

    *Pte = HandlerEntry.Handlers.OnExecuteRead;
    invept();

    return true;
}

bool EptHandler::HandleExecuteWrite(
    unsigned long long Pa,
    const void* NextInstruction)
{
    unsigned long long Pa4Kb = ALIGN_DOWN_BY(Pa, PageSize);
    auto Handler = m_Handlers.find(Pa4Kb);
    if (Handler == m_Handlers.end())
    {
        return false;
    }

    auto& HandlerEntry = Handler->second;
    if (!HandlerEntry.Handlers.OnExecuteWrite.Value)
    {
        return false;
    }

    auto* Pte = HandlerEntry.Pte;

    m_PendingHandler.Rip = NextInstruction;
    m_PendingHandler.Pte = Pte;
    m_PendingHandler.PendingPrevEntry = *Pte;

    *Pte = HandlerEntry.Handlers.OnExecuteWrite;
    invept();

    return true;
}

bool EptHandler::CompletePendingHandler(const void* Rip)
{
    if (m_PendingHandler.Rip != Rip)
    {
        return false;
    }

    *m_PendingHandler.Pte = m_PendingHandler.PendingPrevEntry;
    m_PendingHandler.Rip = NULL;

    invept();

    return true;
}