#pragma once

#define DECLARE_STRUCT(Name, Fields) \
    using Name = struct Fields;      \
    using P##Name = Name*

namespace Ctls
{
    enum CtlIndices {
        // Hypervisor:
        /* 0 */ MvVmmEnable,
        /* 1 */ MvVmmDisable,

        // Driver management:
        /* 3 */ MvGetHandlesCount
    };
}

DECLARE_STRUCT(MV_GET_HANDLES_COUNT_OUT, {
    unsigned long HandlesCount;
});