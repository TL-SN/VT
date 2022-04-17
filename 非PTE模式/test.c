#include <ntddk.h>
#include "vtasm.h"
#include "vtsystem.h"
void MyEptFree();
VOID Unload(PDRIVER_OBJECT driver)
{
    StopVirtualTechnology();
	MyEptFree();//释放非分页内存//
	DbgPrint("Driver Unload\n");
}

ULONG g_ret_esp;
ULONG g_ret_eip;

extern void initEptPagesPool();
extern ULONG64* MyEptInitialization();
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING reg)
{
	ULONG64* ept_PML4T;
	DbgPrint("Driver Load\n");
	driver->DriverUnload = Unload;

	ept_PML4T = MyEptInitialization();
	Log("ept_PML4T", ept_PML4T);
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

