#include "CppSupport/CppSupport.hpp"
//;-
#include "Driver/Inc/IOCTLHandlers.hpp"

/////////////////////////////////////////////////////////////////////////DRIVER INIT SECTION
namespace {
    UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\Marius-VMX");
    UNICODE_STRING DeviceLink = RTL_CONSTANT_STRING(L"\\DosDevices\\Marius-VMX");
    PDEVICE_OBJECT DeviceInstance = NULL;
    PFLT_FILTER FilterHandle = NULL;
}

namespace HyperVisorManagement {
    static bool NeedToRevirtualizeOnWake = false;
}

volatile LONG KbHandlesCount = 0;
static PVOID PowerCallbackRegistration = NULL;

NTSTATUS DrvUnsupported(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID OnDriverCreate(
    PDEVICE_OBJECT DeviceObject,
    PFLT_FILTER FilterHandle,
    PIRP Irp,
    PIO_STACK_LOCATION IrpStack
) 
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(FilterHandle);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(IrpStack);
    InterlockedIncrement(&KbHandlesCount);
}

VOID OnDriverCleanup(
    PDEVICE_OBJECT DeviceObject,
    PFLT_FILTER FilterHandle,
    PIRP Irp,
    PIO_STACK_LOCATION IrpStack
) 
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(FilterHandle);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(IrpStack);
}

VOID OnDriverClose(
    PDEVICE_OBJECT DeviceObject,
    PFLT_FILTER FilterHandle,
    PIRP Irp,
    PIO_STACK_LOCATION IrpStack
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(FilterHandle);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(IrpStack);
    InterlockedDecrement(&KbHandlesCount);
}

VOID OnDriverLoad(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT DeviceObject,
    PFLT_FILTER FilterHandle,
    PUNICODE_STRING RegistryPath
) 
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(FilterHandle);
    UNREFERENCED_PARAMETER(RegistryPath);
}

VOID OnDriverUnload(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT DeviceObject
) 
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(DeviceObject);
    HyperVisor::Devirtualize(); // Devirtualize processor if it is in virtualized state
}

_Function_class_(DRIVER_DISPATCH)
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
_Dispatch_type_(IRP_MJ_CLEANUP)
static NTSTATUS DriverStub(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
    PAGED_CODE();
    PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
    switch (IrpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        OnDriverCreate(DeviceObject, FilterHandle, Irp, IrpStack);
        break;
    case IRP_MJ_CLEANUP:
        OnDriverCleanup(DeviceObject, FilterHandle, Irp, IrpStack);
        break;
    case IRP_MJ_CLOSE:
        OnDriverClose(DeviceObject, FilterHandle, Irp, IrpStack);
        break;
    }
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
static NTSTATUS DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    PAGED_CODE();

    OnDriverUnload(DriverObject, DeviceInstance);

    __crt_deinit(); // Global objects destroying

    if (PowerCallbackRegistration)
        ExUnregisterCallback(PowerCallbackRegistration);

    IoDeleteSymbolicLink(&DeviceLink);
    IoDeleteDevice(DeviceInstance);

    KdPrint(("[Kernel-Bridge]: Successfully unloaded!\r\n"));
    return STATUS_SUCCESS;
}

VOID OnSystemSleep()
{
    if (HyperVisor::IsVirtualized()) {
        HyperVisorManagement::NeedToRevirtualizeOnWake = true;
        HyperVisor::Devirtualize();
    }
    else {
        HyperVisorManagement::NeedToRevirtualizeOnWake = false;
    }
}

VOID OnSystemWake()
{
    if (HyperVisorManagement::NeedToRevirtualizeOnWake)
        HyperVisor::Virtualize();
}

static VOID PowerCallback(PVOID CallbackContext, PVOID Argument1, PVOID Argument2)
{
    UNREFERENCED_PARAMETER(CallbackContext);
    if (reinterpret_cast<SIZE_T>(Argument1) != PO_CB_SYSTEM_STATE_LOCK) return;

    if (Argument2) {
        OnSystemWake();
    }
    else {
        OnSystemSleep();
    }
}

extern "C" NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject, 
    _In_ PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    // Initialization of POOL_NX_OPTIN:
    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

    __crt_init(); // Global objects initialization

    //for (UINT64 Index = 0; Index < IRP_MJ_MAXIMUM_FUNCTION; Index++) { DriverObject->MajorFunction[Index] = DrvUnsupported; }

    DriverObject->DriverUnload = reinterpret_cast<PDRIVER_UNLOAD>(DriverUnload);
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverStub;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = DriverStub;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverStub;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverControl;

    NTSTATUS Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceInstance);

    if (!NT_SUCCESS(Status)) {
        KdPrint(("[Marius-VMX]: IoCreateDevice Error!\r\n"));
        return Status;
    }

    Status = IoCreateSymbolicLink(&DeviceLink, &DeviceName);

    if (!NT_SUCCESS(Status)) {
        KdPrint(("[Marius-VMX]: IoCreateSymbolicLink Error!\r\n"));
        IoDeleteDevice(DeviceInstance);
        return Status;
    }

    // Registering the power callback to handle sleep/resume for support in VMM:
    PCALLBACK_OBJECT PowerCallbackObject = NULL;
    UNICODE_STRING PowerObjectName = RTL_CONSTANT_STRING(L"\\Callback\\PowerState");
    OBJECT_ATTRIBUTES PowerObjectAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(&PowerObjectName, OBJ_CASE_INSENSITIVE);
    Status = ExCreateCallback(&PowerCallbackObject, &PowerObjectAttributes, FALSE, TRUE);
    if (NT_SUCCESS(Status)) {
        PowerCallbackRegistration = ExRegisterCallback(PowerCallbackObject, PowerCallback, NULL);
        ObDereferenceObject(PowerCallbackObject);
        if (!PowerCallbackRegistration) {
            KdPrint(("[Marius-VMX]: Unable to register the power callback!\r\n"));
        }
    }
    else {
        KdPrint(("[Marius-VMX]: Unable to create the power callback!\r\n"));
    }

    OnDriverLoad(DriverObject, DeviceInstance, FilterHandle, RegistryPath);

    KdPrint(("[Marius-VMX]: Successfully loaded!\r\n"));
    return STATUS_SUCCESS;
}
