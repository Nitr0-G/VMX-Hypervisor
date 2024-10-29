#pragma once
#include "Windows.h"
#include "../../Shared/CtlTypes.hpp"
#include "SendIOCTLs.hpp"

namespace MvLoader
{
    BOOL WINAPI MvLoadAsDriver(LPCWSTR DriverPath);
    BOOL WINAPI MvLoadAsFilter(LPCWSTR DriverPath, LPCWSTR Altitude);
    BOOL WINAPI MvUnload();
    BOOL WINAPI MvGetHandlesCount(OUT PULONG Count);
}