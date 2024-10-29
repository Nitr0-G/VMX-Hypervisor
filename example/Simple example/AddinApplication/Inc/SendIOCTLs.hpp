#pragma once
#include <Windows.h>
#include "../../Shared/IOCTLs.hpp"
#include "../../Shared/CtlTypes.hpp"

BOOL SendIOCTL(
    IN HANDLE hDevice,
    IN DWORD Ioctl,
    IN PVOID InputBuffer,
    IN ULONG InputBufferSize,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferSize,
    OPTIONAL OUT PDWORD BytesReturned = NULL,
    OPTIONAL IN DWORD Method = METHOD_NEITHER
);

BOOL SendRawIOCTL(
    IN HANDLE hDevice,
    IN DWORD Ioctl,
    IN PVOID InputBuffer,
    IN ULONG InputBufferSize,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferSize,
    OPTIONAL OUT PDWORD BytesReturned = NULL
);

BOOL WINAPI MvSendRequest(
    Ctls::CtlIndices Index,
    IN PVOID Input = NULL,
    ULONG InputSize = 0,
    OUT PVOID Output = NULL,
    ULONG OutputSize = 0
);