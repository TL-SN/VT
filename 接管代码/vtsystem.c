#include "vtsystem.h"
#include "vtasm.h"
#include "exithandler.h"
VMX_CPU g_VMXCPU;

extern ULONG g_ret_esp;
extern ULONG g_ret_eip;
static ULONG  VmxAdjustControls(ULONG Ctl, ULONG Msr)
{
    LARGE_INTEGER MsrValue;
    MsrValue.QuadPart = Asm_ReadMsr(Msr);
    Ctl &= MsrValue.HighPart;     /* bit == 0 in high word ==> must be zero */
    Ctl |= MsrValue.LowPart;      /* bit == 1 in low word  ==> must be one  */
    return Ctl;
}

void __declspec(naked) GuestEntry()     //guest���//
{
    //���ض�ѡ�������ַ//
    __asm {
        mov ax, es
        mov es, ax

        mov ax, ds
        mov ds, ax

        mov ax, fs
        mov fs, ax

        mov ax, gs
        mov gs, ax

        mov ax, ss
        mov ss, ax
    }
    __asm
    {
        mov esp,g_ret_esp
        jmp g_ret_eip
    }
}


void SetupVMCS()
{
    ULONG GdtBase, IdtBase;
    GdtBase = Asm_GetGdtBase();
    IdtBase = Asm_GetIdtBase();

    //1��Guest state fields
    
    Vmx_VmWrite(GUEST_CR0, Asm_GetCr0());
    Vmx_VmWrite(GUEST_CR3, Asm_GetCr3());
    Vmx_VmWrite(GUEST_CR4, Asm_GetCr4());

    Vmx_VmWrite(GUEST_DR7, 0x400);//Dr7���ԼĴ���Ĭ�Ͼ���0x400//
    Vmx_VmWrite(GUEST_RFLAGS, Asm_GetEflags() & ~0x200);    //eflag�Ĵ������ж�//cli

    Vmx_VmWrite(GUEST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);   
    Vmx_VmWrite(GUEST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
    Vmx_VmWrite(GUEST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
    Vmx_VmWrite(GUEST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
    Vmx_VmWrite(GUEST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
    Vmx_VmWrite(GUEST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
    Vmx_VmWrite(GUEST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

    Vmx_VmWrite(GUEST_ES_AR_BYTES, 0x10000); //Intel�ֲ������ Table 24-2 ����Щ��ѡ������Ϊ�����õĶ�ѡ������Ҫ��16λ��1//   
    Vmx_VmWrite(GUEST_FS_AR_BYTES, 0x10000);                               //��ֹCPU�������ã������ش��������//
    Vmx_VmWrite(GUEST_DS_AR_BYTES, 0x10000);                               //��ֹCPU�������ã������ش��������//
    Vmx_VmWrite(GUEST_SS_AR_BYTES, 0x10000);                               //��ֹCPU�������ã������ش��������//
    Vmx_VmWrite(GUEST_GS_AR_BYTES, 0x10000);                               //��ֹCPU�������ã������ش��������//
    Vmx_VmWrite(GUEST_LDTR_AR_BYTES, 0x10000);                             //��ֹCPU�������ã������ش��������//

    Vmx_VmWrite(GUEST_CS_AR_BYTES, 0xc09b);     //ֱ�Ӷ���1�Ŷ�������//
    Vmx_VmWrite(GUEST_CS_BASE, 0);
    Vmx_VmWrite(GUEST_CS_LIMIT, 0xffffffff);

    Vmx_VmWrite(GUEST_TR_AR_BYTES, 0x008b); //5��//
    Vmx_VmWrite(GUEST_TR_BASE, 0x80042000);
    Vmx_VmWrite(GUEST_TR_LIMIT, 0x20ab);


    Vmx_VmWrite(GUEST_GDTR_BASE, GdtBase);
    Vmx_VmWrite(GUEST_GDTR_LIMIT, Asm_GetGdtLimit());
    Vmx_VmWrite(GUEST_IDTR_BASE, IdtBase);
    Vmx_VmWrite(GUEST_IDTR_LIMIT, Asm_GetIdtLimit());

    //Vmx_VmWrite(GUEST_IA32_DEBUGCTL, Asm_ReadMsr(MSR_IA32_DEBUGCTL) & 0xFFFFFFFF);    
    //Vmx_VmWrite(GUEST_IA32_DEBUGCTL_HIGH, Asm_ReadMsr(MSR_IA32_DEBUGCTL) >> 32);  

    Vmx_VmWrite(GUEST_SYSENTER_CS, Asm_ReadMsr(MSR_IA32_SYSENTER_CS) & 0xFFFFFFFF);
    Vmx_VmWrite(GUEST_SYSENTER_ESP, Asm_ReadMsr(MSR_IA32_SYSENTER_ESP) & 0xFFFFFFFF);
    Vmx_VmWrite(GUEST_SYSENTER_EIP, Asm_ReadMsr(MSR_IA32_SYSENTER_EIP) & 0xFFFFFFFF); // KiFastCallEntry

    Vmx_VmWrite(GUEST_RSP, ((ULONG)g_VMXCPU.pStack) + 0x1000);     //Guest ��ʱջ  //�ϰ�4kb��guestʹ��//
    Vmx_VmWrite(GUEST_RIP, (ULONG)GuestEntry);                     // �ͻ�������ڵ�

    Vmx_VmWrite(VMCS_LINK_POINTER, 0xffffffff);                     //��ʹ��shadow vmcs�����ó�f//
    Vmx_VmWrite(VMCS_LINK_POINTER_HIGH, 0xffffffff);                //��ʹ��shadow vmcs�����ó�f//



    //2��Host state fields
    Vmx_VmWrite(HOST_CR0, Asm_GetCr0());
    Vmx_VmWrite(HOST_CR3, Asm_GetCr3());
    Vmx_VmWrite(HOST_CR4, Asm_GetCr4());

    Vmx_VmWrite(HOST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);    //Intel�涨����λ����Ϊ0//
    Vmx_VmWrite(HOST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
    Vmx_VmWrite(HOST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
    Vmx_VmWrite(HOST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
    Vmx_VmWrite(HOST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
    Vmx_VmWrite(HOST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
    Vmx_VmWrite(HOST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

    Vmx_VmWrite(HOST_TR_BASE, 0x80042000);      //tr�Ĵ�����5�Ŷ�ѡ�����ϣ�ֱ�Ӳ飬Ϊ0x80042000//   ֱ�����Ϊtr�Ļ���ַ//

    Vmx_VmWrite(HOST_GDTR_BASE, GdtBase);
    Vmx_VmWrite(HOST_IDTR_BASE, IdtBase);

    Vmx_VmWrite(HOST_IA32_SYSENTER_CS, Asm_ReadMsr(MSR_IA32_SYSENTER_CS) & 0xFFFFFFFF);
    Vmx_VmWrite(HOST_IA32_SYSENTER_ESP, Asm_ReadMsr(MSR_IA32_SYSENTER_ESP) & 0xFFFFFFFF);
    Vmx_VmWrite(HOST_IA32_SYSENTER_EIP, Asm_ReadMsr(MSR_IA32_SYSENTER_EIP) & 0xFFFFFFFF); // KiFastCallEntry

    Vmx_VmWrite(HOST_RSP, ((ULONG)g_VMXCPU.pStack) + 0x2000);     //Host ��ʱջ    //Ϊ������жϵ�������һ��ջ����ֹ�߳��л���ʱ��Ӱ�쵽���������ִ��// ջ��
    Vmx_VmWrite(HOST_RIP, (ULONG)VMMEntryPoint);                  //���ﶨ�����ǵ�VMM����������

     
    //3��vm-control fields
    //  3.1��vm execution control
     Vmx_VmWrite(PIN_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PINBASED_CTLS));
     Vmx_VmWrite(CPU_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PROCBASED_CTLS));
    //  3.2��vm exit control
     Vmx_VmWrite(VM_EXIT_CONTROLS, VmxAdjustControls(0, MSR_IA32_VMX_EXIT_CTLS));
    //  3.3��vm entry control
    Vmx_VmWrite(VM_ENTRY_CONTROLS, VmxAdjustControls(0, MSR_IA32_VMX_ENTRY_CTLS));
}     
NTSTATUS StartVirtualTechnology()
{
    _CR4 uCr4;
    _EFLAGS uEflags;
    if(!IsVTEnabled())
    {
        return STATUS_UNSUCCESSFUL;
    }
    *((PULONG)&uCr4) = Asm_GetCr4();
    uCr4.VMXE = 1;      //��һ�����������//

    Asm_SetCr4(*((PULONG)&uCr4));
    g_VMXCPU.pVMXONRegion = ExAllocatePoolWithTag(NonPagedPool,0x1000,'tlsn');//������4k����Ҫ����4k��С����//������ֱ�ӷ���4k//
 
    RtlZeroMemory(g_VMXCPU.pVMXONRegion, 0x1000);
    *(PULONG)g_VMXCPU.pVMXONRegion = 1;// //

    g_VMXCPU.pVMXONRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMXONRegion);//virtual add to phy add//
 

    Vmx_VmxOn(g_VMXCPU.pVMXONRegion_PA.LowPart,g_VMXCPU.pVMXONRegion_PA.HighPart);//�ڶ���������VmxOnģʽ//

    *((PULONG)&uEflags) = Asm_GetEflags();  
    if(uEflags.CF != 0)
    {
        Log("Error: VMXONָ�����ʧ��!", 0);
        ExFreePool(g_VMXCPU.pVMXONRegion);

        return STATUS_UNSUCCESSFUL;
    }
    Log("vmcon success", 0);

    //Ϊ���������4kb//
    g_VMXCPU.pVMCSRegion = ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'vmcs');//������4k����Ҫ����4k��С����//������ֱ�ӷ���4k//
    RtlZeroMemory(g_VMXCPU.pVMCSRegion, 0x1000);
    *(PULONG)g_VMXCPU.pVMCSRegion = 1;//

    g_VMXCPU.pVMCSRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMCSRegion);//virtual add to phy add//


    Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart); //�������ε�Դ//

    //���Ĳ���ѡ�л��� (���ǿ����кܶ������������Ҫ����ѡ��)//
    Vmx_VmPtrld(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart);

    g_VMXCPU.pStack = ExAllocatePoolWithTag(NonPagedPool, 0x2000, 'stck');  //ջ//
    RtlZeroMemory(g_VMXCPU.pStack, 0x2000);

    Log("host_stack: ", g_VMXCPU.pStack);
    //���岽,����vmcs(vmwrite)  ,װ��//���ô����Ĵ�����Ϣ//
    SetupVMCS();  

    //������...
    Vmx_VmLaunch(); //ִ�гɹ���ֱ�����ߣ���������ִ��,ִ��ʧ�ܣ�EFLAG��CF��ZFλ��1//       VM - instruction�����ֶν�������������//

    Log("ERROR: VmLaunchָ�����ʧ�ܣ�����������",Vmx_VmRead(VM_INSTRUCTION_ERROR));    //�Ӵ����ֶ��ж�ȡ�����// Intel������ 30-4�п��Բ鿴�����//

    return STATUS_SUCCESS;
}

extern ULONG g_vmcall_arg;
extern ULONG g_stop_esp, g_stop_eip;;
NTSTATUS StopVirtualTechnology()
{
    _CR4 uCr4;
    g_vmcall_arg = 'EXIT';

    __asm
    {
        pushad
        pushfd
        mov g_stop_esp,esp
        mov g_stop_eip,offset STOP_EIP

    }
    Vmx_VmCall();       //�ع��� ��һָ�guest������û��Ȩ��ִ�е�//
    __asm
    {
   STOP_EIP:
        popfd
        popad
    }
    *((PULONG)&uCr4) = Asm_GetCr4();
    uCr4.VMXE = 0;      //����//
    Asm_SetCr4(*((PULONG)&uCr4));

    ExFreePool(g_VMXCPU.pVMXONRegion);//���ں˷�����ڴ���Ҫ�����Լ��ֶ��ͷ�//
    ExFreePool(g_VMXCPU.pVMCSRegion);
    ExFreePool(g_VMXCPU.pStack);

    DbgPrint("VT Stop");
    return STATUS_SUCCESS;
}

BOOLEAN IsVTEnabled()  //�ж������ܲ��ܿ���vt//
{
    ULONG       uRet_EAX, uRet_ECX, uRet_EDX, uRet_EBX;
    _CPUID_ECX  uCPUID;
    _CR0        uCr0;
    _CR4    uCr4;
    IA32_FEATURE_CONTROL_MSR msr;
    //1. CPUID
    Asm_CPUID(1, &uRet_EAX, &uRet_EBX, &uRet_ECX, &uRet_EDX);
    *((PULONG)&uCPUID) = uRet_ECX;

    if (uCPUID.VMX != 1)
    {
        Log("ERROR: ���CPU��֧��VT!",0);
        return FALSE;
    }

    // 2. MSR
    *((PULONG)&msr) = (ULONG)Asm_ReadMsr(MSR_IA32_FEATURE_CONTROL);
    if (msr.Lock!=1)
    {
        Log("ERROR:VTָ��δ������!",0);
        return FALSE;
    }

    // 3. CR0 CR4
    *((PULONG)&uCr0) = Asm_GetCr0();
    *((PULONG)&uCr4) = Asm_GetCr4();

    if (uCr0.PE != 1 || uCr0.PG!=1 || uCr0.NE!=1)
    {
        Log("ERROR:���CPUû�п���VT!",0);
        return FALSE;
    }

    if (uCr4.VMXE == 1)
    {
        Log("ERROR:���CPU�Ѿ�������VT!",0);
        Log("�����Ǳ�������Ѿ�ռ����VT�������ر�������ܿ�����",0);
        return FALSE;
    }


    Log("SUCCESS:���CPU֧��VT!",0);
    return TRUE;
}
