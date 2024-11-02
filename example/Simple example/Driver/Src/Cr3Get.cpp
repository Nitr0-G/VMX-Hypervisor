#include "Driver/Inc/Cr3Get.hpp"

extern "C" unsigned long long __readcr3();

const unsigned long Cr3Getter::GetUserDirectoryTableBaseOffset()
{
	RTL_OSVERSIONINFOW ver = { 0 };
	RtlGetVersion(&ver);
	switch (ver.dwBuildNumber)
	{
	case WINDOWS_1803:
		return 0x0278;
		break;
	case WINDOWS_1809:
		return 0x0278;
		break;
	case WINDOWS_1903:
		return 0x0280;
		break;
	case WINDOWS_1909:
		return 0x0280;
		break;
	case WINDOWS_2004:
		return 0x0388;
		break;
	case WINDOWS_20H2:
		return 0x0388;
		break;
	case WINDOWS_21H1:
		return 0x0388;
		break;
	case WINDOWS_22H2:
		return 0x0388;
		break;
	default:
		return 0x0388;
	}
}

const unsigned long long Cr3Getter::GetKernelDirBase()
{
	PUCHAR process = (PUCHAR)PsGetCurrentProcess();
#ifdef _AMD64_
	ULONG_PTR cr3 = *(PULONG_PTR)(process + 0x28);
#else 
	ULONG_PTR cr3 = *(PULONG_PTR)(process + 0x18);
#endif
	return cr3;
}

const unsigned long long Cr3Getter::GetProcessCr3(const PEPROCESS pProcess, Cr3GetMode Mode)
{
	if (Mode == Cr3GetMode::MvBareMetal)
	{
		PUCHAR process = (PUCHAR)pProcess;
#ifdef _AMD64_
		ULONG_PTR process_dirbase = *(PULONG_PTR)(process + 0x28);
#else 
		ULONG_PTR process_dirbase = *(PULONG_PTR)(process + 0x18);
#endif
		if (process_dirbase == 0)
		{
			unsigned long UserDirOffset = GetUserDirectoryTableBaseOffset();

			ULONG_PTR process_userdirbase = *(PULONG_PTR)(process + UserDirOffset);
			return process_userdirbase;
		}
		return process_dirbase;
	}
	else if (Mode == Cr3GetMode::MvKernelApiBased)
	{
		KAPC_STATE ApcState;
		KeStackAttachProcess(pProcess, &ApcState);
		SIZE_T process_dirbase = __readcr3();
		KeUnstackDetachProcess(&ApcState);

		return process_dirbase;
	}

	return NULL;
}