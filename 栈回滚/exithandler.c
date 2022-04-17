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

    ExitReason = Vmx_VmRead(VM_EXIT_REASON);       //��ȡ�����//
    Log("ExitReason", ExitReason);
    //ExitInstructionLength = Vmx_VmRead(VM_EXIT_INSTRUCTION_LEN);

    g_GuestRegs.eflags = Vmx_VmRead(GUEST_RFLAGS);
    g_GuestRegs.esp = Vmx_VmRead(GUEST_RSP);
    g_GuestRegs.eip = Vmx_VmRead(GUEST_RIP);
    Dbg_data = g_GuestRegs.eip;

    Log("g_GuestRegs.eip", g_GuestRegs.eip);

    //__asm int 3
    Vmx_VmxOff();   //�ر������//
    //ֱ������guestȥ�����ж�//
    ////�ָ��ֳ�//
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


void __declspec(naked) VMMEntryPoint(void) //����û��wei'huHost ��ebp�����Ա���ʹ���㺯��//...
{
    //�����������Ĺ�����Ϊ�����fs��gs����ַ,������������Ͻ�//
   ////�����guest����ʱ�����Ϣ////
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
   
    VMMEntryPointEbd();//���㺯������ʹ�þֲ���������ջ������ռ䣬�ᵼ��ebp���������ִ���,���������Ǹɴ��ٷ�װһ������,�����������ʵ�ֹ���//
}
