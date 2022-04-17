#include <ntddk.h>
#define PAGE_SIZE 0x1000

ULONG64* ept_PML4T;

static PVOID *pagesToFree;
static int index = 0;

void initEptPagesPool()
{
    pagesToFree = ExAllocatePoolWithTag(NonPagedPool, 12*1024*1024, 'ept');
    if(!pagesToFree)
        __asm int 3
    RtlZeroMemory(pagesToFree, 12*1024*1024);   //12Mb//
}

static ULONG64* AllocateOnePage()
{
    PVOID page;
    page = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, 'ept');
    if(!page)
        __asm int 3
    RtlZeroMemory(page, PAGE_SIZE);
    pagesToFree[index] = page;
    index++;
    return (ULONG64 *)page;
}

//extern PULONG test_data;
//PHYSICAL_ADDRESS hook_pa;
//ULONG64 *hook_ept_pt;

ULONG64 fake_page_pa;
ULONG64* hook_ept_PT;

void  AllocateFakedPage()
{
    PVOID page;
    page = AllocateOnePage();
    fake_page_pa = MmGetPhysicalAddress(page).QuadPart;
}

ULONG64* MyEptInitialization()
{
    ULONG64 *ept_PDPT, *ept_PDT, *ept_PT;
    PHYSICAL_ADDRESS FirstPtePA, FirstPdePA, FirstPdptePA;
    int a, b, c;


    initEptPagesPool();
    AllocateFakedPage();//�����ҳ��//
    ept_PML4T = AllocateOnePage();  //PML4E//
    ept_PDPT = AllocateOnePage();   //PDPTE//
    FirstPdptePA = MmGetPhysicalAddress(ept_PDPT);      
    *ept_PML4T = (FirstPdptePA.QuadPart) + 7;   //��Ȩ��//��ҳ�ĺ�12λ������//�ɶ���д��ִ��//
    for (a = 0; a < 4; a++)
    {
        ept_PDT = AllocateOnePage();
        FirstPdePA = MmGetPhysicalAddress(ept_PDT);
        *ept_PDPT = (FirstPdePA.QuadPart) + 7;
        ept_PDPT++;
        for (b = 0; b < 512; b++)
        {
            ept_PT = AllocateOnePage();
            FirstPtePA = MmGetPhysicalAddress(ept_PT);
            *ept_PDT = (FirstPtePA.QuadPart) + 7;
            ept_PDT++;
            for (c = 0; c < 512; c++)
            {
                *ept_PT  = ((a << 30) | (b << 21) | (c << 12) | 0x37) & 0xFFFFFFFF; //PTE�Ǽӵ�0x37// �ɶ���д��ִ�У��л���,�����еĵ�ַҪ���л��棬�������������ȫ���ó���0x37����Ȼ������Ӱ��CPU������//
                
                if( (0x541000 == ((a << 30) | (b << 21) | (c << 12)) & 0xffffffff))
                {
                    *ept_PT = ((a << 30) | (b << 21) | (c << 12) | 0x34) & 0xFFFFFFFF;//ֻ��ִ��//��ִ������ʱ������Host//
                    hook_ept_PT = ept_PT;
                }
                
                ept_PT++;
            }
        }
    }

    return ept_PML4T;
}

void MyEptFree()    //����ֱ��ͨ�������ַת�����ַ����ת,ʹ��ȫ������ķ�ʽ����¼�����ַ//
{
    int i;
    for (i = 0; i < index; i++) {
        ExFreePool(pagesToFree[i]);
    }
    ExFreePool(pagesToFree);
    index = 0;
}
