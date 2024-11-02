#include <iostream>
#include <sstream>
#include <Windows.h>
#include <map>

#include "SelfLoader.hpp"
#include "IOCTLRequests.hpp"
#include "Trace.hpp"


static std::map<std::wstring, std::wstring> ParseArguments(std::wstring Command)
{
    std::wstringstream ss(Command);
    std::wstring token;
    std::map<std::wstring, std::wstring> args;

    // Считываем аргументы из строки
    while (ss >> token)
    {
        if (token[0] == L'-')  // Если токен начинается с '-', это ключ
        {
            std::wstring key = token;
            if (ss >> token)  // Попробуем считать значение
            {
                args[key] = token;  // Сохраняем пару ключ-значение
            }
            else
            {
                args[key] = L"";  // Если значения нет, сохраняем пустую строку
            }
        }
    }

    return args;
}

void HvLifeCycle()
{
    while (true)
    {
        std::wstring Command;
        std::getline(std::wcin, Command);

        if (Command == L"exit")
        {
            // HyperVisor::MvVmmDisable();
            std::wcout << L"VMX VMM disabled!\r\n";
            break;
        }
        else if (Command.find(L"Trace") == 0)
        {
            // Убираем команду "Trace" перед парсингом
            std::wstring argsString = Command.substr(5);  // Убираем "Trace "

            // Парсим аргументы
            auto args = ParseArguments(argsString);

            // Проверяем наличие необходимых аргументов
            if (Trace::StartTraceByLaunchArgs(args))
            {
                Trace::StartTraceByLaunch(args[L"-path"], args[L"-cmd_args"], (Cr3GetMode)std::stoi(args[L"-mode"]));
            }
            else
            {
                std::wcout << L"Invalid command format. Use: Trace -path <path> -mode <mode>\n";
            }
        }
        else
        {
            std::wcout << L"Unknown command!\n";
        }
    }
}


void HvEntry()
{
    if (HyperVisor::MvVmmEnable())
    {
        printf("VMX VMM enabled!\r\n");

        printf(
            "Commands:\n"
            "  exit: disable the hypervisor and exit\n"
        );

        HvLifeCycle();
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
        HvEntry();
    }
    else
    {
        std::wcout << L"Unable to load driver! LastError: 0x" << std::hex << GetLastError() << std::endl;
    }

    std::wcout << L"Press any key to exit..." << std::endl;
    std::cin.get();

	return 1;
}