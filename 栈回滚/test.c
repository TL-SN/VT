#include <ntddk.h>
#include "vtasm.h"
#include "vtsystem.h"
VOID Unload(PDRIVER_OBJECT driver)
{
    StopVirtualTechnology();
	DbgPrint("Driver Unload\n");
}

ULONG Dbg_data[4];

NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING reg)
{
	DbgPrint("Driver Load\n");

	Dbg_data[0] = 0x1433223;
	Dbg_data[1] = 0xabababab;
    StartVirtualTechnology();
	driver->DriverUnload = Unload;
	return STATUS_SUCCESS;
}

