#pragma once
#include "include/HyperVisor/HyperVisor.hpp"

#include "GuestContext.hpp"
#include "Shared/CtlTypes.hpp"

namespace HyperVisor {
    bool IsVirtualized();
    bool Virtualize();
    bool Devirtualize();
    bool InterceptPage(
        unsigned long long Pa,
        unsigned long long ReadPa,
        unsigned long long WritePa,
        unsigned long long ExecutePa,
        unsigned long long ExecuteReadPa,
        unsigned long long ExecuteWritePa
    );
    bool DeinterceptPage(
        unsigned long long Pa
    );
    bool Trace(
        PMV_VMM_TRACE_PROCESS_IN Input,
        PMV_VMM_TRACE_PROCESS_OUT Output
    );
}