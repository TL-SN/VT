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

void __declspec(naked) GuestEntry()     //guest入口//
{
    //加载段选择子与基址//
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

    //1、Guest state fields
    
    Vmx_VmWrite(GUEST_CR0, Asm_GetCr0());
    Vmx_VmWrite(GUEST_CR3, Asm_GetCr3());
    Vmx_VmWrite(GUEST_CR4, Asm_GetCr4());

    Vmx_VmWrite(GUEST_DR7, 0x400);//Dr7调试寄存器默认就是0x400//
    Vmx_VmWrite(GUEST_RFLAGS, Asm_GetEflags() & ~0x200);    //eflag寄存器关中断//cli

    Vmx_VmWrite(GUEST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);   
    Vmx_VmWrite(GUEST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
    Vmx_VmWrite(GUEST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
    Vmx_VmWrite(GUEST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
    Vmx_VmWrite(GUEST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
    Vmx_VmWrite(GUEST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
    Vmx_VmWrite(GUEST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

    Vmx_VmWrite(GUEST_ES_AR_BYTES, 0x10000); //Intel手册第三卷 Table 24-2 把这些段选择子设为不可用的段选择子需要第16位置1//   
    Vmx_VmWrite(GUEST_FS_AR_BYTES, 0x10000);                               //防止CPU当作可用，而加载错误的属性//
    Vmx_VmWrite(GUEST_DS_AR_BYTES, 0x10000);                               //防止CPU当作可用，而加载错误的属性//
    Vmx_VmWrite(GUEST_SS_AR_BYTES, 0x10000);                               //防止CPU当作可用，而加载错误的属性//
    Vmx_VmWrite(GUEST_GS_AR_BYTES, 0x10000);                               //防止CPU当作可用，而加载错误的属性//
    Vmx_VmWrite(GUEST_LDTR_AR_BYTES, 0x10000);                             //防止CPU当作可用，而加载错误的属性//

    Vmx_VmWrite(GUEST_CS_AR_BYTES, 0xc09b);     //直接读的1号段描述符//
    Vmx_VmWrite(GUEST_CS_BASE, 0);
    Vmx_VmWrite(GUEST_CS_LIMIT, 0xffffffff);

    Vmx_VmWrite(GUEST_TR_AR_BYTES, 0x008b); //5号//
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

    Vmx_VmWrite(GUEST_RSP, ((ULONG)g_VMXCPU.pStack) + 0x1000);     //Guest 临时栈  //上半4kb给guest使用//
    Vmx_VmWrite(GUEST_RIP, (ULONG)GuestEntry);                     // 客户机的入口点

    Vmx_VmWrite(VMCS_LINK_POINTER, 0xffffffff);                     //不使用shadow vmcs，都置成f//
    Vmx_VmWrite(VMCS_LINK_POINTER_HIGH, 0xffffffff);                //不使用shadow vmcs，都置成f//



    //2、Host state fields
    Vmx_VmWrite(HOST_CR0, Asm_GetCr0());
    Vmx_VmWrite(HOST_CR3, Asm_GetCr3());
    Vmx_VmWrite(HOST_CR4, Asm_GetCr4());

    Vmx_VmWrite(HOST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);    //Intel规定后三位必须为0//
    Vmx_VmWrite(HOST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
    Vmx_VmWrite(HOST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
    Vmx_VmWrite(HOST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
    Vmx_VmWrite(HOST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
    Vmx_VmWrite(HOST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
    Vmx_VmWrite(HOST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

    Vmx_VmWrite(HOST_TR_BASE, 0x80042000);      //tr寄存器在5号段选择子上，直接查，为0x80042000//   直接填充为tr的基地址//

    Vmx_VmWrite(HOST_GDTR_BASE, GdtBase);
    Vmx_VmWrite(HOST_IDTR_BASE, IdtBase);

    Vmx_VmWrite(HOST_IA32_SYSENTER_CS, Asm_ReadMsr(MSR_IA32_SYSENTER_CS) & 0xFFFFFFFF);
    Vmx_VmWrite(HOST_IA32_SYSENTER_ESP, Asm_ReadMsr(MSR_IA32_SYSENTER_ESP) & 0xFFFFFFFF);
    Vmx_VmWrite(HOST_IA32_SYSENTER_EIP, Asm_ReadMsr(MSR_IA32_SYSENTER_EIP) & 0xFFFFFFFF); // KiFastCallEntry

    Vmx_VmWrite(HOST_RSP, ((ULONG)g_VMXCPU.pStack) + 0x2000);     //Host 临时栈    //为后面的中断单独分配一个栈，防止线程切换的时候影响到后续程序的执行// 栈顶
    Vmx_VmWrite(HOST_RIP, (ULONG)VMMEntryPoint);                  //这里定义我们的VMM处理程序入口

     
    //3、vm-control fields
    //  3.1、vm execution control
     Vmx_VmWrite(PIN_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PINBASED_CTLS));
     Vmx_VmWrite(CPU_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PROCBASED_CTLS));
    //  3.2、vm exit control
     Vmx_VmWrite(VM_EXIT_CONTROLS, VmxAdjustControls(0, MSR_IA32_VMX_EXIT_CTLS));
    //  3.3、vm entry control
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
    uCr4.VMXE = 1;      //第一步，开锁完成//

    Asm_SetCr4(*((PULONG)&uCr4));
    g_VMXCPU.pVMXONRegion = ExAllocatePoolWithTag(NonPagedPool,0x1000,'tlsn');//最多分配4k，还要满足4k大小对齐//那这里直接分配4k//
 
    RtlZeroMemory(g_VMXCPU.pVMXONRegion, 0x1000);
    *(PULONG)g_VMXCPU.pVMXONRegion = 1;// //

    g_VMXCPU.pVMXONRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMXONRegion);//virtual add to phy add//
 

    Vmx_VmxOn(g_VMXCPU.pVMXONRegion_PA.LowPart,g_VMXCPU.pVMXONRegion_PA.HighPart);//第二步，开启VmxOn模式//

    *((PULONG)&uEflags) = Asm_GetEflags();  
    if(uEflags.CF != 0)
    {
        Log("Error: VMXON指令调用失败!", 0);
        ExFreePool(g_VMXCPU.pVMXONRegion);

        return STATUS_UNSUCCESSFUL;
    }
    Log("vmcon success", 0);

    //为虚拟机分配4kb//
    g_VMXCPU.pVMCSRegion = ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'vmcs');//最多分配4k，还要满足4k大小对齐//那这里直接分配4k//
    RtlZeroMemory(g_VMXCPU.pVMCSRegion, 0x1000);
    *(PULONG)g_VMXCPU.pVMCSRegion = 1;//

    g_VMXCPU.pVMCSRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMCSRegion);//virtual add to phy add//


    Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart); //第三步拔电源//

    //第四步，选中机器 (我们可能有很多虚拟机，我们要进行选择)//
    Vmx_VmPtrld(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart);

    g_VMXCPU.pStack = ExAllocatePoolWithTag(NonPagedPool, 0x2000, 'stck');  //栈//
    RtlZeroMemory(g_VMXCPU.pStack, 0x2000);

    Log("host_stack: ", g_VMXCPU.pStack);
    //第五步,设置vmcs(vmwrite)  ,装机//设置大量寄存器信息//
    SetupVMCS();  

    //第六步...
    Vmx_VmLaunch(); //执行成功，直接跳走，不再向下执行,执行失败，EFLAG的CF或ZF位置1//       VM - instruction错误字段将会包含错误代码//

    Log("ERROR: VmLaunch指令调用失败！！！！！！",Vmx_VmRead(VM_INSTRUCTION_ERROR));    //从错误字段中读取错误号// Intel第三卷 30-4中可以查看错误号//

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
    Vmx_VmCall();       //关柜门 这一指令，guest主机是没有权限执行的//
    __asm
    {
   STOP_EIP:
        popfd
        popad
    }
    *((PULONG)&uCr4) = Asm_GetCr4();
    uCr4.VMXE = 0;      //关锁//
    Asm_SetCr4(*((PULONG)&uCr4));

    ExFreePool(g_VMXCPU.pVMXONRegion);//在内核分配的内存需要我们自己手动释放//
    ExFreePool(g_VMXCPU.pVMCSRegion);
    ExFreePool(g_VMXCPU.pStack);

    DbgPrint("VT Stop");
    return STATUS_SUCCESS;
}

BOOLEAN IsVTEnabled()  //判断主机能不能开启vt//
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
        Log("ERROR: 这个CPU不支持VT!",0);
        return FALSE;
    }

    // 2. MSR
    *((PULONG)&msr) = (ULONG)Asm_ReadMsr(MSR_IA32_FEATURE_CONTROL);
    if (msr.Lock!=1)
    {
        Log("ERROR:VT指令未被锁定!",0);
        return FALSE;
    }

    // 3. CR0 CR4
    *((PULONG)&uCr0) = Asm_GetCr0();
    *((PULONG)&uCr4) = Asm_GetCr4();

    if (uCr0.PE != 1 || uCr0.PG!=1 || uCr0.NE!=1)
    {
        Log("ERROR:这个CPU没有开启VT!",0);
        return FALSE;
    }

    if (uCr4.VMXE == 1)
    {
        Log("ERROR:这个CPU已经开启了VT!",0);
        Log("可能是别的驱动已经占用了VT，你必须关闭它后才能开启。",0);
        return FALSE;
    }


    Log("SUCCESS:这个CPU支持VT!",0);
    return TRUE;
}
