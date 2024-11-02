#pragma once
#include <fltKernel.h>
#include "CommonApi/PteUtils.hpp"
#include "Shared/WdkTypes.hpp"


#define WINDOWS_1803 17134
#define WINDOWS_1809 17763
#define WINDOWS_1903 18362
#define WINDOWS_1909 18363
#define WINDOWS_2004 19041
#define WINDOWS_20H2 19569
#define WINDOWS_21H1 20180
#define WINDOWS_22H2 19045

class Cr3Getter {
private:
	static const unsigned long GetUserDirectoryTableBaseOffset();
	static const unsigned long long GetKernelDirBase();
public:
	static const unsigned long long GetProcessCr3(const PEPROCESS pProcess, Cr3GetMode Mode);
};