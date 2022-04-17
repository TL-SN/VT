#include "exithandler.h"
#include "vtsystem.h"
#include "vtasm.h"

GUEST_REGS g_GuestRegs;
extern ULONG Dbg_data;
static void  VMMEntryPointEbd(void)
{
    ULONG ExitReason;
    //ULONG ExitInstructionLength;
    //ULONG GuestResumeEIP;

    ExitReason = Vmx_VmRead(VM_EXIT_REASON);       //读取错误号//
    Log("ExitReason", ExitReason);
    //ExitInstructionLength = Vmx_VmRead(VM_EXIT_INSTRUCTION_LEN);

    g_GuestRegs.eflags = Vmx_VmRead(GUEST_RFLAGS);
    g_GuestRegs.esp = Vmx_VmRead(GUEST_RSP);
    g_GuestRegs.eip = Vmx_VmRead(GUEST_RIP);
    Dbg_data = g_GuestRegs.eip;

    Log("g_GuestRegs.eip", g_GuestRegs.eip);

    //__asm int 3
    Vmx_VmxOff();   //关闭虚拟机//
    //直接跳回guest去处理中断//
    ////恢复现场//
    __asm
    {
        mov  eax, g_GuestRegs.eax   
        mov  ecx, g_GuestRegs.ecx
        mov  edx, g_GuestRegs.edx
        mov  ebx, g_GuestRegs.ebx
        mov  esp, g_GuestRegs.esp
        mov  ebp, g_GuestRegs.ebp
        mov  esi, g_GuestRegs.esi
        mov  edi, g_GuestRegs.edi
    
        push g_GuestRegs.eflags
        popfd

        jmp g_GuestRegs.eip
    }
}


void __declspec(naked) VMMEntryPoint(void) //我们没有wei'huHost 的ebp，所以必须使用裸函数//...
{
    //下面内联汇编的功能是为了填充fs与gs基地址,这个方法不很严谨//
   ////保存从guest出来时候的信息////
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
}
