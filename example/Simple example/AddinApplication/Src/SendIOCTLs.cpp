#include "SendIOCTLs.hpp"

namespace MvLoader
{
    extern HANDLE hDriver;
}

BOOL SendIOCTL(
    IN HANDLE hDevice,
    IN DWORD Ioctl,
    IN PVOID InputBuffer,
    IN ULONG InputBufferSize,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferSize,
    OPTIONAL OUT PDWORD BytesReturned,
    OPTIONAL IN DWORD Method
) {
    DWORD RawIoctl = CTL_CODE(0x8000, Ioctl, Method, FILE_ANY_ACCESS);
    DWORD Returned = 0;
    BOOL Status = DeviceIoControl(hDevice, RawIoctl, InputBuffer, InputBufferSize, OutputBuffer, OutputBufferSize, &Returned, NULL);
    if (BytesReturned) *BytesReturned = Returned;
    return Status;
}

BOOL SendRawIOCTL(
    IN HANDLE hDevice,
    IN DWORD Ioctl,
    IN PVOID InputBuffer,
    IN ULONG InputBufferSize,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferSize,
    OPTIONAL OUT PDWORD BytesReturned
) {
    DWORD Returned = 0;
    BOOL Status = DeviceIoControl(hDevice, Ioctl, InputBuffer, InputBufferSize, OutputBuffer, OutputBufferSize, &Returned, NULL);
    if (BytesReturned) *BytesReturned = Returned;
    return Status;
}

BOOL WINAPI MvSendRequest(
    Ctls::CtlIndices Index,
    IN PVOID Input,
    ULONG InputSize,
    OUT PVOID Output,
    ULONG OutputSize
) {
    return SendIOCTL(MvLoader::hDriver, CTL_BASE + Index, Input, InputSize, Output, OutputSize);
}