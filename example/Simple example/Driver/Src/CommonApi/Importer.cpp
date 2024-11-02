#include <fltKernel.h>
#include "Driver/Inc/CommonApi/Importer.hpp"

namespace Importer {
    _IRQL_requires_max_(PASSIVE_LEVEL)
    PVOID NTAPI GetKernelProcAddress(LPCWSTR SystemRoutineName) {
        UNICODE_STRING Name;
        RtlInitUnicodeString(&Name, SystemRoutineName);
        return MmGetSystemRoutineAddress(&Name);
    }
}