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
    AllocateFakedPage();//制造假页面//
    ept_PML4T = AllocateOnePage();  //PML4E//
    ept_PDPT = AllocateOnePage();   //PDPTE//
    FirstPdptePA = MmGetPhysicalAddress(ept_PDPT);      
    *ept_PML4T = (FirstPdptePA.QuadPart) + 7;   //加权限//分页的后12位的属性//可读可写可执行//
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
                *ept_PT  = ((a << 30) | (b << 21) | (c << 12) | 0x37) & 0xFFFFFFFF; //PTE是加的0x37// 可读可写可执行，有缓存,由于有的地址要求有缓存，方便起见，这里全设置成了0x37，虽然这样会影响CPU的性能//
                
                if( (0x541000 == ((a << 30) | (b << 21) | (c << 12)) & 0xffffffff))
                {
                    *ept_PT = ((a << 30) | (b << 21) | (c << 12) | 0x34) & 0xFFFFFFFF;//只可执行//当执行它的时候会进入Host//
                    hook_ept_PT = ept_PT;
                }
                
                ept_PT++;
            }
        }
    }

    return ept_PML4T;
}

void MyEptFree()    //由于直接通过物理地址转虚拟地址不好转,使用全局数组的方式，记录虚拟地址//
{
    int i;
    for (i = 0; i < index; i++) {
        ExFreePool(pagesToFree[i]);
    }
    ExFreePool(pagesToFree);
    index = 0;
}
