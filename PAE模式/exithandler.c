#include "exithandler.h"
#include "vtsystem.h"
#include "vtasm.h"

GUEST_REGS g_GuestRegs;

ULONG g_vmcall_arg;
ULONG g_stop_esp, g_stop_eip;
void HandleVmCall()
{
    if (g_vmcall_arg == 'EXIT')
    {
        Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart);
        Vmx_VmxOff();
        __asm {
            mov esp, g_stop_esp
            jmp g_stop_eip
        }
    }
    else {
        __asm int 3
    }
}

void HandleCPUID()
{
    if (g_GuestRegs.eax == 'Mini')  //调试用的，删了也没影响//
    {
        g_GuestRegs.ebx = 0x88888888;
        g_GuestRegs.ecx = 0x11111111;
        g_GuestRegs.edx = 0x12345678;
    }
    else Asm_CPUID(g_GuestRegs.eax, &g_GuestRegs.eax, &g_GuestRegs.ebx, &g_GuestRegs.ecx, &g_GuestRegs.edx);
}


void HandleCrAccess()
{
    ULONG		movcrControlRegister;
    ULONG		movcrAccessType;
    ULONG		movcrOperandType;
    ULONG		movcrGeneralPurposeRegister;    //通用寄存器的下表号//
    ULONG		movcrLMSWSourceData;
    ULONG		ExitQualification;

    ExitQualification = Vmx_VmRead(EXIT_QUALIFICATION);
    movcrControlRegister = (ExitQualification & 0x0000000F);
    movcrAccessType = ((ExitQualification & 0x00000030) >> 4);
    movcrOperandType = ((ExitQualification & 0x00000040) >> 6);
    movcrGeneralPurposeRegister = ((ExitQualification & 0x00000F00) >> 8);

    //进一步划分退出号//
    //具体可以查看Intel 手册的Table 27-3//
    if (movcrControlRegister != 3) {    // not for cr3
        __asm int 3
    }

    if (movcrAccessType == 0) {         // CR3 <-- reg32            //写cr3//
        Vmx_VmWrite(GUEST_CR3, *(PULONG)((ULONG)&g_GuestRegs + 4 * movcrGeneralPurposeRegister));
        
        Asm_SetCr3(Vmx_VmRead(GUEST_CR3));

        Vmx_VmWrite(GUEST_PDPTR0, MmGetPhysicalAddress((PVOID)0xc0600000).LowPart | 1);
        Vmx_VmWrite(GUEST_PDPTR0_HIGH, MmGetPhysicalAddress((PVOID)0xc0600000).HighPart);
        Vmx_VmWrite(GUEST_PDPTR1, MmGetPhysicalAddress((PVOID)0xc0601000).LowPart | 1);
        Vmx_VmWrite(GUEST_PDPTR1_HIGH, MmGetPhysicalAddress((PVOID)0xc0601000).HighPart);
        Vmx_VmWrite(GUEST_PDPTR2, MmGetPhysicalAddress((PVOID)0xc0602000).LowPart | 1);
        Vmx_VmWrite(GUEST_PDPTR2_HIGH, MmGetPhysicalAddress((PVOID)0xc0602000).HighPart);
        Vmx_VmWrite(GUEST_PDPTR3, MmGetPhysicalAddress((PVOID)0xc0603000).LowPart | 1);
        Vmx_VmWrite(GUEST_PDPTR3_HIGH, MmGetPhysicalAddress((PVOID)0xc0603000).HighPart);
    }
    else {                            // reg32 <-- CR3  //读cr3//
        *(PULONG)((ULONG)&g_GuestRegs + 4 * movcrGeneralPurposeRegister) = Vmx_VmRead(GUEST_CR3);
    }
}
static void  VMMEntryPointEbd(void)
{
    ULONG ExitReason;
    ULONG ExitInstructionLength;
    ULONG GuestResumeEIP;

    ExitReason = Vmx_VmRead(VM_EXIT_REASON);        //接收退出号//
    ExitInstructionLength = Vmx_VmRead(VM_EXIT_INSTRUCTION_LEN);//抛出退出号原因的代码字节长度//跳回去的时候加上这一长度即可//

    g_GuestRegs.eflags = Vmx_VmRead(GUEST_RFLAGS);
    g_GuestRegs.esp = Vmx_VmRead(GUEST_RSP);
    g_GuestRegs.eip = Vmx_VmRead(GUEST_RIP);

    switch (ExitReason)
    {
    case EXIT_REASON_CPUID:         //10号//
        HandleCPUID();
       //Log("EXIT_REASON_CPUID", 0)
            break;

    case EXIT_REASON_VMCALL:
        Log("EXIT_REASON_VMCALL", 0)
        HandleVmCall();
            break;

    case EXIT_REASON_CR_ACCESS:             //28退出号//
        HandleCrAccess();
        //Log("EXIT_REASON_CR_ACCESS", 0)
        break;

    default:
        Log("not handled reason: %p", ExitReason);
        __asm int 3
    }

    //Resume:
    GuestResumeEIP = g_GuestRegs.eip + ExitInstructionLength;
    Vmx_VmWrite(GUEST_RIP, GuestResumeEIP);
    Vmx_VmWrite(GUEST_RSP, g_GuestRegs.esp);
    Vmx_VmWrite(GUEST_RFLAGS, g_GuestRegs.eflags);
    
}


void __declspec(naked) VMMEntryPoint(void) //我们没有wei'huHost 的ebp，所以必须使用裸函数//...
{
    //下面内联汇编的功能是为了填充fs与gs基地址,这个方法不很严谨//
   ////保存从guest刚出来时候的信息////
    __asm
    {
        mov g_GuestRegs.eax, eax    
        mov g_GuestRegs.ecx, ecx
        mov g_GuestRegs.edx, edx
        mov g_GuestRegs.ebx, ebx
        mov g_GuestRegs.esp, esp
        mov g_GuestRegs.ebp, ebp
        mov g_GuestRegs.esi, esi
        mov g_GuestRegs.edi, edi






        mov ax, fs
        mov fs, ax

        mov ax, gs
        mov gs, ax
    }
   
    VMMEntryPointEbd();//在裸函数里面使用局部变量会在栈区分配空间，会导致ebp的索引出现错误,索所以我们干脆再封装一个函数,在这个函数中实现功能//
    //手动恢复寄存器// 执行完VMMEntryPoint后，恢复Guest模式的时候，Guest寄存器会优先从Guest域中读取信息，如果Guest域中没有某些信息，就需要我们手动去恢复了//
    __asm {
        mov  eax, g_GuestRegs.eax
        mov  ecx, g_GuestRegs.ecx
        mov  edx, g_GuestRegs.edx
        mov  ebx, g_GuestRegs.ebx
        mov  esp, g_GuestRegs.esp
        mov  ebp, g_GuestRegs.ebp
        mov  esi, g_GuestRegs.esi
        mov  edi, g_GuestRegs.edi

        //vmresume
        __emit 0x0f
        __emit 0x01
        __emit 0xc3
    }
    
     

}
