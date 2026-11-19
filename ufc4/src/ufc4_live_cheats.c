/* Exact UFC 4 v28 stamina write hook. No persistent cheat configuration. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "exact_hooks.h"
#include "ufc4_live_cheats.h"

#if !defined(UFC4_TEST) && !defined(UFC4_EXPERIMENTAL_LIVE_STAMINA)
int ufc4_live_stamina_prepare(void){return 0;}
unsigned ufc4_live_stamina_mask(void){return 0;}
int ufc4_live_stamina_toggle(int fighter,unsigned *mask){(void)fighter;(void)mask;return 0;}
#else

static const unsigned char stamina_site[]={
    0xc5,0xfa,0x5d,0xc1, /* vminss xmm0,xmm0,xmm1 */
    0xc5,0xfa,0x11,0x00, /* vmovss [rax],xmm0 */
    0x49,0x8b,0x00,      /* mov rax,[r8] */
    0x48,0x3b,0x45,0xd0 /* cmp rax,[rbp-0x30] */
};
static volatile LONG stamina_mask;
static volatile LONG *detour_mask;
static int prepared; /* 0 unchecked, 1 installed, -1 unsupported */
static void *original_site;
static SRWLOCK setup_guard=SRWLOCK_INIT;

static void scan_bytes(const unsigned char *bytes,size_t length,uintptr_t base,unsigned char **found,unsigned *hits){
    for(size_t i=0;i+sizeof(stamina_site)<=length;++i){
        if(bytes[i]!=stamina_site[0]||memcmp(bytes+i,stamina_site,sizeof(stamina_site)))continue;
        unsigned char *site=(unsigned char*)(base+i);
        if(site==*found)continue;
        ++*hits;*found=site;
    }
}

static unsigned char *find_site(void){
    SYSTEM_INFO info;GetSystemInfo(&info);
    uintptr_t address=(uintptr_t)info.lpMinimumApplicationAddress;
    uintptr_t limit=(uintptr_t)info.lpMaximumApplicationAddress;
    unsigned char *found=NULL;unsigned hits=0;
    while(address<limit){
        MEMORY_BASIC_INFORMATION region;
        if(!VirtualQuery((void*)address,&region,sizeof(region)))break;
        uintptr_t next=(uintptr_t)region.BaseAddress+region.RegionSize;
        if(next<=address)break;
        DWORD protection=region.Protect&0xff;
        int executable=protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||
                       protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
        if(region.State==MEM_COMMIT&&executable&&!(region.Protect&PAGE_GUARD)){
            /* Overlap chunks so a signature at a chunk boundary is included. */
            for(uintptr_t at=address;at<next;){
                unsigned char chunk[65536+sizeof(stamina_site)-1];size_t size=(size_t)(next-at);
                if(size>sizeof(chunk))size=sizeof(chunk);
                SIZE_T got=0;
                if(ReadProcessMemory(GetCurrentProcess(),(void*)at,chunk,size,&got)&&got==size){
                    scan_bytes(chunk,size,at,&found,&hits);
                }
                at+=next-at>65536?65536:size;
            }
        }
        address=next;
    }
    return hits==1?found:NULL;
}

static void append(unsigned char *code,size_t *used,const unsigned char *bytes,size_t length){
    memcpy(code+*used,bytes,length);*used+=length;
}
static void jump_back(unsigned char *code,size_t *used,const void *target){
    static const unsigned char jump[]={0xff,0x25,0,0,0,0};
    append(code,used,jump,sizeof(jump));
    uintptr_t address=(uintptr_t)target;append(code,used,(const unsigned char*)&address,sizeof(address));
}
static void mask_disp(unsigned char *code,size_t field,size_t after){
    int32_t delta=(int32_t)((intptr_t)(code+4096)-(intptr_t)(code+after));
    memcpy(code+field,&delta,sizeof(delta));
}

static unsigned char *make_detour(unsigned char *site){
    unsigned char *code=VirtualAlloc(NULL,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!code)return NULL;
    size_t n=0,branch_empty,branch_right,branch_decide,branch_off,branch_after,right,decide,off,after;
    static const unsigned char begin[]={0xc5,0xfa,0x5d,0xc1};
    append(code,&n,begin,sizeof(begin));
    static const unsigned char empty[]={0x83,0x3d,0,0,0,0,0,0x74,0};
    size_t empty_at=n;append(code,&n,empty,sizeof(empty));mask_disp(code,empty_at+2,empty_at+7);branch_empty=n-1;
    static const unsigned char side[]={0x83,0xb8,0x18,0x01,0x00,0x00,0x00,0x75,0};
    append(code,&n,side,sizeof(side));branch_right=n-1;
    static const unsigned char left[]={0xf6,0x05,0,0,0,0,0x01,0xeb,0};
    size_t left_at=n;append(code,&n,left,sizeof(left));mask_disp(code,left_at+2,left_at+7);branch_decide=n-1;
    right=n;
    static const unsigned char right_test[]={0xf6,0x05,0,0,0,0,0x02};
    append(code,&n,right_test,sizeof(right_test));mask_disp(code,right+2,right+7);
    decide=n;
    static const unsigned char choice[]={0x74,0,0xc5,0xfa,0x11,0x08,0xeb,0};
    append(code,&n,choice,sizeof(choice));branch_off=decide+1;branch_after=n-1;
    off=n;
    static const unsigned char normal[]={0xc5,0xfa,0x11,0x00};
    append(code,&n,normal,sizeof(normal));
    after=n;
    static const unsigned char tail[]={0x49,0x8b,0x00,0x48,0x3b,0x45,0xd0};
    append(code,&n,tail,sizeof(tail));jump_back(code,&n,site+sizeof(stamina_site));
    code[branch_empty]=(unsigned char)(off-(branch_empty+1));
    code[branch_right]=(unsigned char)(right-(branch_right+1));
    code[branch_decide]=(unsigned char)(decide-(branch_decide+1));
    code[branch_off]=(unsigned char)(off-(branch_off+1));
    code[branch_after]=(unsigned char)(after-(branch_after+1));
    DWORD old=0;
    if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return NULL;}
    FlushInstructionCache(GetCurrentProcess(),code,n);
    return code;
}

#ifdef UFC4_TEST
__declspec(dllexport) int Ufc4TestStaminaDetour(unsigned char *out,size_t size){
    if(!out||size<96)return 0;
    unsigned char *code=make_detour((unsigned char*)(uintptr_t)0x100000);
    if(!code)return 0;
    memcpy(out,code,96);VirtualFree(code,0,MEM_RELEASE);return 1;
}
__declspec(dllexport) int Ufc4TestStaminaRun(unsigned mask,int side){
    unsigned char *site=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!site)return -1;
    memcpy(site,stamina_site,sizeof(stamina_site));site[sizeof(stamina_site)]=0xc3;
    DWORD old=0;
    if(!VirtualProtect(site,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(site,0,MEM_RELEASE);return -1;}
    unsigned char *code=make_detour(site);
    if(!code){VirtualFree(site,0,MEM_RELEASE);return -1;}
    *(volatile LONG*)(code+4096)=(LONG)mask;
    struct {float value;unsigned char padding[0x114];int fighter_side;} fighter={0};
    fighter.value=9.0f;fighter.fighter_side=side;
    float candidate=5.0f,previous=9.0f;
    uintptr_t after=0;void *entry=code;float *target=&fighter.value;uintptr_t *after_ptr=&after;
    __asm__ volatile(
        "mov %[entry], %%r11\n\t"
        "mov %[target], %%rax\n\t"
        "mov %[after], %%r8\n\t"
        "vmovss %[candidate], %%xmm0\n\t"
        "vmovss %[previous], %%xmm1\n\t"
        "call *%%r11\n\t"
        :
        : [entry]"m"(entry),[target]"m"(target),[after]"m"(after_ptr),
          [candidate]"m"(candidate),[previous]"m"(previous)
        : "rax","r8","r11","xmm0","xmm1","cc","memory");
    int result=(int)fighter.value;
    VirtualFree(code,0,MEM_RELEASE);VirtualFree(site,0,MEM_RELEASE);
    return result;
}
#endif

int ufc4_live_stamina_prepare(void){
    AcquireSRWLockExclusive(&setup_guard);
    if(!prepared){
        unsigned char *site=find_site();
        unsigned char *detour=site?make_detour(site):NULL;
        if(detour){
            ExactHook hook={site,detour,&original_site,stamina_site,sizeof(stamina_site)};
            const char *error=NULL;
            if(install_exact_hooks(&hook,1,&error)){
                detour_mask=(volatile LONG*)(detour+4096);
                prepared=1;
            }
            else VirtualFree(detour,0,MEM_RELEASE);
        }
        if(!prepared)prepared=-1;
    }
    int ready=prepared==1;
    ReleaseSRWLockExclusive(&setup_guard);return ready;
}
unsigned ufc4_live_stamina_mask(void){return (unsigned)InterlockedCompareExchange(&stamina_mask,0,0)&3u;}
int ufc4_live_stamina_toggle(int fighter,unsigned *mask){
    if(fighter<0||fighter>1||!mask||!ufc4_live_stamina_prepare())return 0;
    LONG bit=1L<<fighter;
    for(;;){
        LONG old=InterlockedCompareExchange(&stamina_mask,0,0);
        LONG next=old^bit;
        if(InterlockedCompareExchange(&stamina_mask,next,old)==old){
            InterlockedExchange(detour_mask,next);*mask=(unsigned)next&3u;return 1;
        }
    }
}
#ifdef UFC4_TEST
__declspec(dllexport) int Ufc4TestStaminaPrepare(void){return ufc4_live_stamina_prepare();}
__declspec(dllexport) unsigned Ufc4TestStaminaToggle(int fighter){
    unsigned mask=0;return ufc4_live_stamina_toggle(fighter,&mask)?mask:~0u;
}
#endif
#endif
