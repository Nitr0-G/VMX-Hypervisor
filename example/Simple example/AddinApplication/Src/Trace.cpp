#include "Trace.hpp"
#include <ASMFuncs.hpp>

bool Trace::StartTraceByLaunchArgs(std::map<std::wstring, std::wstring>& args)
{
	return args.count(L"-path") > 0 && args.count(L"-cmd_args") >= 0 && args.count(L"-mode") > 0;
}

//void* GetFuncPtr(const void* Func)
//{
//    if (!Func) return nullptr;
//    const auto* FuncDataPtr = reinterpret_cast<const unsigned char*>(Func);
//    if (*FuncDataPtr == 0xE9)
//    {
//        auto Offset = *reinterpret_cast<const int*>(FuncDataPtr + 1);
//        return const_cast<unsigned char*>((FuncDataPtr + 5) + Offset);
//    }
//    else
//    {
//        return const_cast<void*>(Func);
//    }
//}
//
//#pragma section(".hidden", read, execute, nopage)
//__declspec(code_seg(".hidden")) static void MtfEnableHiddenFunc()
//{
//    ASMmv_vmcall((uint64_t)1, 0, 0, 0);
//}
//
//bool Trace::EnableOn(uint64_t PhysicalAddress)
//{
//    constexpr unsigned int PageSize = 4096;
//
//    //volatile PBYTE Read = reinterpret_cast<PBYTE>(VirtualAlloc(NULL, PageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
//    //volatile PBYTE Write = reinterpret_cast<PBYTE>(VirtualAlloc(NULL, PageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
//    volatile PBYTE WriteExecute = reinterpret_cast<PBYTE>(VirtualAlloc(NULL, PageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
//    volatile PBYTE Execute = reinterpret_cast<PBYTE>(GetFuncPtr(MtfEnableHiddenFunc));
//
//    //__assume(Read != 0);
//    //__assume(Write != 0);
//    __assume(WriteExecute != 0);
//
//    //VirtualLock(Read, PageSize);
//    //VirtualLock(Write, PageSize);
//    VirtualLock(Execute, PageSize);
//    VirtualLock(WriteExecute, PageSize);
//
//    //memset(Read, 0xFF, PageSize);
//    //memset(Write, 0xEE, PageSize);
//    memcpy(WriteExecute, Execute, PageSize);
//
//    DWORD OldProtect = 0;
//    VirtualProtect(Execute, PageSize, PAGE_EXECUTE_READWRITE, &OldProtect);
//    *Execute = *Execute;
//
//    WdkTypes::PVOID64 ReadPa = 0, WritePa = 0, WriteExecutePa = 0, ExecutePa = 0;
//    //PhysicalMemory::MvNativeTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(Read), &ReadPa);//IOCTL  
//    //PhysicalMemory::MvNativeTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(Write), &WritePa);//IOCTL  
//    PhysicalMemory::MvNativeTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(WriteExecute), &WriteExecutePa);//IOCTL  
//    PhysicalMemory::MvNativeTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(Execute), &ExecutePa);//IOCTL  
//
//    printf("Original bytes: 0x%X\n", static_cast<unsigned int>(*Execute));
//
//    HyperVisor::MvVmmInterceptPage(PhysicalAddress, ReadPa, WritePa, ExecutePa, ExecutePa, WriteExecutePa);//IOCTL    
//    //printf("Bytes after intercept: 0x%X\n", static_cast<unsigned int>(*Execute));
//    //printf("Bytes after call: 0x%X\n", static_cast<unsigned int>(*Execute));
//    //printf("Write-interceptor: W:0x%X WX:0x%X\n", static_cast<unsigned int>(*Write), static_cast<unsigned int>(*WriteExecute));
//    //HyperVisor::MvVmmDeinterceptPage(ExecutePa);//IOCTL  
//
//    //printf("Bytes after deintercept: 0x%X\n", static_cast<unsigned int>(*Execute));
//
//    //VirtualUnlock(Execute, PageSize);
//    //VirtualUnlock(Write, PageSize);
//    //VirtualUnlock(Read, PageSize);
//
//    //VirtualFree(Write, 0, MEM_RELEASE);
//    //VirtualFree(Read, 0, MEM_RELEASE);
//
//    return true;
//}

void* GetFuncPtr(const void* Func)
{
    if (!Func) return nullptr;
    const auto* FuncDataPtr = reinterpret_cast<const unsigned char*>(Func);
    if (*FuncDataPtr == 0xE9)
    {
        auto Offset = *reinterpret_cast<const int*>(FuncDataPtr + 1);
        return const_cast<unsigned char*>((FuncDataPtr + 5) + Offset);
    }
    else
    {
        return const_cast<void*>(Func);
    }
}

#pragma section(".hidden", read, execute, nopage)
__declspec(code_seg(".hidden")) unsigned int HiddenFunc()
{
    printf("Called from hidden func!\n");
    for (unsigned int i = 0; i < 100; ++i)
    {
        volatile BYTE* Self = reinterpret_cast<PBYTE>(GetFuncPtr(HiddenFunc));
        *Self = 0x55;
    }
    return 0x1EE7C0DE;
}

VOID TestHvPageInterception()
{
    constexpr unsigned int PageSize = 4096;

    volatile PBYTE Read = reinterpret_cast<PBYTE>(VirtualAlloc(NULL, PageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    volatile PBYTE Write = reinterpret_cast<PBYTE>(VirtualAlloc(NULL, PageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    volatile PBYTE WriteExecute = reinterpret_cast<PBYTE>(VirtualAlloc(NULL, PageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    volatile PBYTE Execute = reinterpret_cast<PBYTE>(GetFuncPtr(HiddenFunc));

    __assume(Read != 0);
    __assume(Write != 0);
    __assume(WriteExecute != 0);

    VirtualLock(Read, PageSize);
    VirtualLock(Write, PageSize);
    VirtualLock(Execute, PageSize);
    VirtualLock(WriteExecute, PageSize);

    memset(Read, 0xFF, PageSize);
    memset(Write, 0xEE, PageSize);
    memcpy(WriteExecute, Execute, PageSize);

    DWORD OldProtect = 0;
    VirtualProtect(Execute, PageSize, PAGE_EXECUTE_READWRITE, &OldProtect);
    *Execute = *Execute;

    WdkTypes::PVOID64 ReadPa = 0, WritePa = 0, WriteExecutePa = 0, ExecutePa = 0;
    PhysicalMemory::MvTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(Read), &ReadPa);//IOCTL  
    PhysicalMemory::MvTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(Write), &WritePa);//IOCTL  
    PhysicalMemory::MvTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(WriteExecute), &WriteExecutePa);//IOCTL  
    PhysicalMemory::MvTranslateProcessVirtualAddrToPhysicalAddr(NULL, reinterpret_cast<WdkTypes::PVOID>(Execute), &ExecutePa);//IOCTL  

    printf("Original bytes: 0x%X\n", static_cast<unsigned int>(*Execute));

    HyperVisor::MvVmmInterceptPage(ExecutePa, ReadPa, WritePa, ExecutePa, ExecutePa, WriteExecutePa);//IOCTL    
    printf("Bytes after intercept: 0x%X\n", static_cast<unsigned int>(*Execute));
    *Execute = 0x40;
    HiddenFunc();
    printf("Bytes after call: 0x%X\n", static_cast<unsigned int>(*Execute));
    printf("Write-interceptor: W:0x%X WX:0x%X\n", static_cast<unsigned int>(*Write), static_cast<unsigned int>(*WriteExecute));
    HyperVisor::MvVmmDeinterceptPage(ExecutePa);//IOCTL  

    printf("Bytes after deintercept: 0x%X\n", static_cast<unsigned int>(*Execute));

    VirtualUnlock(Execute, PageSize);
    VirtualUnlock(Write, PageSize);
    VirtualUnlock(Read, PageSize);

    VirtualFree(Write, 0, MEM_RELEASE);
    VirtualFree(Read, 0, MEM_RELEASE);
}


bool Trace::StartTraceByLaunch(std::wstring Path, std::wstring CmdArgs, Cr3GetMode Mode)
{
	if (Path.empty()) { return false; }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    BOOL result = CreateProcessW(Path.c_str(),
        const_cast<wchar_t*>(CmdArgs.c_str()),
        NULL,
        NULL,
        FALSE,
        CREATE_SUSPENDED | DETACHED_PROCESS,
        NULL,
        NULL,
        &si,
        &pi);

    if (!result) {
        DWORD error = GetLastError();
        return false;
    }

    uint64_t Cr3 = {};
    Process::MvGetProcessCr3(pi.dwProcessId, &Cr3, Mode);

    //uint64_t PhysicalAddress = {};
    //PhysicalMemory::MvNativeTranslateProcessVirtualAddrToPhysicalAddr(Cr3, 0x15B284020, &PhysicalAddress);

    //EnableOn(PhysicalAddress);
    //Process::
    //ResumeThread(pi.hThread);
    TestHvPageInterception();
    for (;;)
    {
        PMV_VMM_TRACE_PROCESS_OUT Output = nullptr;
        HyperVisor::MvVmmTraceProcess(Cr3, (PVOID)0x15B284020, (PVOID)0x15B28402F, Output);

        printf("%p\n", Output->Rip);

        if (Output->Rip == (PVOID)0x15B28402F)
        {
            break;
        }
    }
    //HyperVisor::MvVmmDeinterceptPage(PhysicalAddress);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}