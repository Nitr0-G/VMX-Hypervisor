#pragma once
#include <map>
#include <string>
#include <Windows.h>

#include "IOCTLRequests.hpp"
#include "../../Shared/CtlTypes.hpp"

namespace Trace
{
	bool StartTraceByLaunchArgs(std::map<std::wstring, std::wstring>& args);
	bool StartTraceByLaunch(std::wstring Path, std::wstring CmdArgs, Cr3GetMode Mode);

	bool EnableOn(uint64_t PhysicalAddress);
}