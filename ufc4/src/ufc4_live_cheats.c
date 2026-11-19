/* Live UFC 4 controls use a writable flag prepared by the launch-time linker. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "ufc4_live_cheats.h"

static const unsigned char marker[]="UFC4LIVE_STAMINA1";
static volatile LONG *mask_address;
static SRWLOCK setup_guard=SRWLOCK_INIT;

static int readable_writable(DWORD protection){
    DWORD page=protection&0xff;
    return !(protection&PAGE_GUARD)&&(page==PAGE_READWRITE||page==PAGE_WRITECOPY||
                                     page==PAGE_EXECUTE_READWRITE||page==PAGE_EXECUTE_WRITECOPY);
}

static volatile LONG *find_mask(void){
    SYSTEM_INFO system;GetSystemInfo(&system);
    uintptr_t at=(uintptr_t)system.lpMinimumApplicationAddress;
    uintptr_t limit=(uintptr_t)system.lpMaximumApplicationAddress;
    uintptr_t found=0;unsigned matches=0;
    while(at<limit){
        MEMORY_BASIC_INFORMATION region;
        if(!VirtualQuery((void*)at,&region,sizeof(region)))break;
        uintptr_t end=(uintptr_t)region.BaseAddress+region.RegionSize;
        if(end<=at)break;
        if(region.State==MEM_COMMIT&&readable_writable(region.Protect)&&region.RegionSize>=4096){
            /* The linker puts the marker at +16 in its own writable page. */
            uintptr_t candidate=(uintptr_t)region.BaseAddress+16;
            unsigned char observed[sizeof(marker)-1];SIZE_T got=0;
            if(ReadProcessMemory(GetCurrentProcess(),(void*)candidate,observed,sizeof(observed),&got)&&
               got==sizeof(observed)&&!memcmp(observed,marker,sizeof(observed))){
                found=candidate;++matches;
            }
        }
        at=end;
    }
    if(matches!=1||found<16)return NULL;
    volatile LONG *mask=(volatile LONG*)(found-16);
    LONG value=InterlockedCompareExchange(mask,0,0);
    return value>=0&&value<=3?mask:NULL;
}

int ufc4_live_stamina_prepare(void){
    AcquireSRWLockExclusive(&setup_guard);
    if(!mask_address)mask_address=find_mask();
    int ready=mask_address!=NULL;
    ReleaseSRWLockExclusive(&setup_guard);
    return ready;
}

unsigned ufc4_live_stamina_mask(void){
    volatile LONG *mask=mask_address;
    return mask?(unsigned)InterlockedCompareExchange(mask,0,0)&3u:0;
}

int ufc4_live_stamina_toggle(int fighter,unsigned *selected){
    if(fighter<0||fighter>1||!selected||!ufc4_live_stamina_prepare())return 0;
    volatile LONG *mask=mask_address;
    LONG bit=1L<<fighter;
    for(;;){
        LONG old=InterlockedCompareExchange(mask,0,0);
        if(old<0||old>3)return 0;
        LONG next=old^bit;
        if(InterlockedCompareExchange(mask,next,old)==old){*selected=(unsigned)next;return 1;}
    }
}

#ifdef UFC4_TEST
__declspec(dllexport) int Ufc4TestStaminaPrepare(void){return ufc4_live_stamina_prepare();}
__declspec(dllexport) unsigned Ufc4TestStaminaToggle(int fighter){
    unsigned selected=0;return ufc4_live_stamina_toggle(fighter,&selected)?selected:~0u;
}
#endif
