#pragma once 
#include "CommonTypes/CPUID.hpp"
#include "CommonTypes/Hyper-V.hpp"
#include "CommonTypes/MSR.hpp"
#include "CommonTypes/Segmentation.hpp"
#include "CommonTypes/Registers.hpp"
#include "CommonTypes/VMX.hpp"
#include "CommonTypes/Exception.hpp"
#include "CommonTypes/Interrupts.hpp"
#include "CommonApi/Callable.hpp"
#include "CommonApi/MemoryUtils.hpp"

#include <fltKernel.h>
#include <intrin.h>

extern "C" typedef void(*_VmxVmmRun)(
	_In_ void* InitialVmmStackPointer);

extern "C" typedef void(*_Interceptions)(
	_In_ Intel::IA32_VMX_BASIC VmxBasicInfo);

extern "C" typedef void(*_InitHandlersTable)();

enum IntelEnc : unsigned int { IEbx = 'uneG', IEdx = 'Ieni', IEcx = 'letn' };
enum AmdEnc : unsigned int { AEbx = 'htuA', AEdx = 'itne', AEcx = 'DMAc' };

enum class CpuVendor { CpuUnknown, CpuIntel, CpuAmd};

// Magic value, defined by hypervisor, triggers #VMEXIT and VMM shutdown:
constexpr unsigned int HyperSign = 0x1EE7C0DE;
constexpr unsigned int CPUID_VMM_SHUTDOWN = HyperSign;

namespace GetControlMask {
	template <typename T>
	T ApplyMask(_Inout_ T VmxControlReg, _In_ CONTROLS_MASK Mask)
	{
		VmxControlReg.Value &= Mask.Bitmap.Allowed1Settings;
		VmxControlReg.Value |= Mask.Bitmap.Allowed0Settings;
		return VmxControlReg;
	}

	CONTROLS_MASK GetSecondaryControlsMask();
	CONTROLS_MASK GetVmentryControlsMask(Intel::IA32_VMX_BASIC VmxBasic);
	CONTROLS_MASK GetVmexitControlsMask(Intel::IA32_VMX_BASIC VmxBasic);
	CONTROLS_MASK GetPrimaryControlsMask(Intel::IA32_VMX_BASIC VmxBasic);
	CONTROLS_MASK GetPinControlsMask(Intel::IA32_VMX_BASIC VmxBasic);
}

size_t vmread(_In_ size_t field);

class HyperVisorVmx {
private:
	CpuVendor GetCpuVendor();
	bool VirtualizeProcessor();
	bool DevirtualizeProcessor(__out PVOID& PrivateVmData);

	PVOID AllocPhys(
		_In_ SIZE_T Size,
		_In_ MEMORY_CACHING_TYPE CachingType,
		_In_ ULONG MaxPhysBits);

	void ParseSegmentInfo(
		_In_ const SEGMENT_DESCRIPTOR_LONG* Gdt,
		_In_ const SEGMENT_DESCRIPTOR_LONG* Ldt,
		_In_ unsigned short Selector,
		__out VMX::SEGMENT_INFO* Info
	);

	unsigned long long ExtractSegmentBaseAddress(_In_ const SEGMENT_DESCRIPTOR_LONG* SegmentDescriptor);

	void FreePhys(_In_ PVOID Memory);

	void FreePrivateVmData(_In_ void* Private);

	void InitMtrr(__out VMX::MTRR_INFO* MtrrInfo);

	// E.g.: MaskLow<char>(5) -> 0b00011111:
	template <typename T>
	constexpr T MaskLow(_In_ unsigned char SignificantBits)
	{
		return static_cast<T>((1ULL << SignificantBits) - 1);
	}

	// E.g.: MaskHigh<char>(3) -> 0b11100000:
	template <typename T>
	constexpr T MaskHigh(_In_ unsigned char SignificantBits)
	{
		return MaskLow<T>(SignificantBits) << ((sizeof(T) * 8) - SignificantBits);
	}

	CONTROLS_MASK GetCr4Mask();
	CONTROLS_MASK GetCr0Mask();

private:
	static inline volatile bool g_IsVirtualized;
	_VmxVmmRun VmxVmmRun; _Interceptions Interceptions;
public:
	_InitHandlersTable InitHandlersTable;
	PVOID PVmxVmmRun = nullptr, PInterceptions = nullptr, PInitHandlersTable = nullptr;
public:
	bool DevirtualizeAllProcessors();
	bool VirtualizeAllProcessors();
	bool IsVmxSupported();
};