#include <ntddk.h>
#include "vtasm.h"
#include "vtsystem.h"
VOID Unload(PDRIVER_OBJECT driver)
{
    StopVirtualTechnology();
	DbgPrint("Driver Unload\n");
}

ULONG g_ret_esp;
ULONG g_ret_eip;

NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING reg)
{
	DbgPrint("Driver Load\n");
	driver->DriverUnload = Unload;
	__asm
	{
		pushad
		pushfd
		mov g_ret_esp,esp
		mov g_ret_eip,offset RET_EIP
	}
	
	StartVirtualTechnology();
	
//.............................
	__asm
	{
RET_EIP:
		popfd
		popad
	}
	Log("DriverEntry ret", 0);

	return STATUS_SUCCESS;
}

