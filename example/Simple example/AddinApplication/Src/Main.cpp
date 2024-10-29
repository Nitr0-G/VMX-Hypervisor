#include <iostream>
#include <Windows.h>

#include "SendIOCTLs.hpp"
#include "SelfLoader.hpp"
#include "../../Shared/CtlTypes.hpp"

namespace HyperVisor
{
    BOOL WINAPI MvVmmEnable()
    {
        return MvSendRequest(Ctls::MvVmmEnable);
    }

    BOOL WINAPI MvVmmDisable()
    {
        return MvSendRequest(Ctls::MvVmmDisable);
    }

    void LifeCycle()
    {
        while (true)
        {
            std::wstring Command;
            std::wcin >> Command;
            if (Command == L"exit")
            {
                HyperVisor::MvVmmDisable();
                printf("VMX VMM disabled!\r\n");

                break;
            }
            else
            {
                printf("Unknown command!\n");
            }
        }
    }

    void Entry()
    {
        if (HyperVisor::MvVmmEnable())
        {
            printf("VMX VMM enabled!\r\n");

            printf(
                "Commands:\n"
                "  exit: disable the hypervisor and exit\n"
            );

            LifeCycle();
        }
    }
}

std::wstring GetCurrentFolder()
{
    WCHAR Path[MAX_PATH] = {};
    DWORD PathLength = ARRAYSIZE(Path);
    BOOL Status = QueryFullProcessImageName(GetCurrentProcess(), 0, Path, &PathLength);

    if (!Status || PathLength == 0) {
        return L"";
    }

    // Find the last occurrence of the backslash
    const wchar_t* lastSlash = wcsrchr(Path, L'\\');

    if (lastSlash != nullptr) {
        // Truncate the string at the last slash
        return std::wstring(Path, static_cast<size_t>(lastSlash - Path + 1));
    }
    else {
        // No slashes found, return the entire path
        return std::wstring(Path, PathLength);
    }
}

int main()
{
    std::wstring CurrentFolder = GetCurrentFolder();
    if (CurrentFolder.empty())
    {
        printf("Unable to determine current directory!\n");
        return 0;
    }

    printf("[MariusVMX]: PID: %i, TID: %i\r\n", GetCurrentProcessId(), GetCurrentThreadId());

    if (MvLoader::MvLoadAsDriver(
        std::wstring(CurrentFolder + L"VMX-Hypervisor.sys").c_str()
    )) 
    {
        HyperVisor::Entry();
    }
    else
    {
        std::wcout << L"Unable to load driver! LastError: 0x" << std::hex << GetLastError() << std::endl;
    }

    std::wcout << L"Press any key to exit..." << std::endl;
    std::cin.get();

	return 1;
}