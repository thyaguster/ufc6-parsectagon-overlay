/* Live UFC 4 controls use a writable flag prepared by the launch-time linker. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "ufc4_live_cheats.h"

static const unsigned char marker[]="UFC4LIVE_STAMINA1";
static const unsigned char damage_marker[]="UFC4LIVE_NODAMAGE1";
static volatile LONG *mask_address;
static volatile LONG *damage_mask_address;
static LONG ko_start_hits;
static int ko_requested;
static uintptr_t ko_result_address;
#define KO_HEALTH_FIELDS 4
static uintptr_t ko_health_address[KO_HEALTH_FIELDS];
static LONG ko_health_original[KO_HEALTH_FIELDS],ko_health_primed[KO_HEALTH_FIELDS];
static int ko_health_saved;
#ifdef UFC4_TEST
static int ko_test_side=-1;
static volatile LONG ko_test_result[2];
#endif
static SRWLOCK setup_guard=SRWLOCK_INIT;

static int readable_writable(DWORD protection){
    DWORD page=protection&0xff;
    return !(protection&PAGE_GUARD)&&(page==PAGE_READWRITE||page==PAGE_WRITECOPY||
                                     page==PAGE_EXECUTE_READWRITE||page==PAGE_EXECUTE_WRITECOPY);
}

static volatile LONG *find_mask(const unsigned char *signature,SIZE_T signature_length){
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
            unsigned char observed[32];SIZE_T got=0;
            if(signature_length<=sizeof(observed)&&
               ReadProcessMemory(GetCurrentProcess(),(void*)candidate,observed,signature_length,&got)&&
               got==signature_length&&!memcmp(observed,signature,signature_length)){
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

static int read_guest(uintptr_t address,void *out,SIZE_T size){
    SIZE_T got=0;
    return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)address,out,size,&got)&&got==size;
}

static int ko_opponent_side(uintptr_t *world_out){
    const uintptr_t start=0x7007000000ULL,limit=0x7008000000ULL;
    unsigned char *buffer=(unsigned char*)HeapAlloc(GetProcessHeap(),0,1<<20);
    if(!buffer)return -1;
    int found=0,side=-1;
    for(uintptr_t at=start;at<limit;){
        MEMORY_BASIC_INFORMATION region;
        if(!VirtualQuery((void*)at,&region,sizeof(region)))break;
        uintptr_t end=(uintptr_t)region.BaseAddress+region.RegionSize;
        if(end<=at)break;
        if(region.State==MEM_COMMIT&&readable_writable(region.Protect)){
            for(uintptr_t cursor=at;cursor<end&&cursor<limit;){
                SIZE_T size=(SIZE_T)((end-cursor)>(1<<20)?(1<<20):(end-cursor));
                if(size>limit-cursor)size=(SIZE_T)(limit-cursor);
                if(read_guest(cursor,buffer,size)){
                    unsigned char *p=buffer,*last=buffer+size;
                    while(p<last){
                        p=(unsigned char*)memchr(p,0x05,(size_t)(last-p));
                        if(!p||last-p<4)break;
                        uint32_t key=0;memcpy(&key,p,4);
                        if(key==0xea9d2e05u){
                            uintptr_t address=cursor+(uintptr_t)(p-buffer),vtable=0,world=0,values=0;
                            unsigned char header[0x70],data[0x60];uint32_t count=0,a=~0u,b=~0u;
                            if(address>=8&&read_guest(address-8,header,sizeof(header))){
                                memcpy(&vtable,header+0x18,8);memcpy(&world,header+0x28,8);
                                memcpy(&count,header+0x50,4);memcpy(&values,header+0x58,8);
                                if(vtable>=0x7000000000ULL&&vtable<0x7100000000ULL&&
                                   world>=0x7000000000ULL&&world<0x7100000000ULL&&count==4&&
                                   read_guest(values,data,sizeof(data))){
                                    memcpy(&a,data+0x28,4);memcpy(&b,data+0x58,4);
                                    if((a==1&&b==0)||(a==0&&b==1)){++found;side=a==1?0:1;if(world_out)*world_out=world;}
                                }
                            }
                        }
                        ++p;
                    }
                }
                cursor+=size;
            }
        }
        at=end;
    }
    HeapFree(GetProcessHeap(),0,buffer);
    return found==1?side:-1;
}

static uintptr_t ko_match_result_address(uintptr_t expected_world){
    const uintptr_t start=0x7007000000ULL,limit=0x7008000000ULL;
    const uint32_t key=0xe784b061u; /* FNV-1 of Fighter_MatchResult */
    unsigned char *buffer=(unsigned char*)HeapAlloc(GetProcessHeap(),0,1<<20);
    if(!buffer)return 0;
    uintptr_t found=0;unsigned matches=0;
    for(uintptr_t at=start;at<limit;){
        MEMORY_BASIC_INFORMATION region;
        if(!VirtualQuery((void*)at,&region,sizeof(region)))break;
        uintptr_t end=(uintptr_t)region.BaseAddress+region.RegionSize;
        if(end<=at)break;
        if(region.State==MEM_COMMIT&&readable_writable(region.Protect)){
            for(uintptr_t cursor=at;cursor<end&&cursor<limit;){
                SIZE_T size=(SIZE_T)((end-cursor)>(1<<20)?(1<<20):(end-cursor));
                if(size>limit-cursor)size=(SIZE_T)(limit-cursor);
                if(read_guest(cursor,buffer,size)){
                    for(SIZE_T i=0;i+4<=size;++i){
                        uint32_t observed=0;memcpy(&observed,buffer+i,4);
                        if(observed!=key)continue;
                        uintptr_t address=cursor+i,vtable=0,world=0,values=0;
                        unsigned char header[0x70];uint32_t count=0,result[2]={~0u,~0u};
                        if(address<8||!read_guest(address-8,header,sizeof(header)))continue;
                        memcpy(&vtable,header+0x18,8);memcpy(&world,header+0x28,8);
                        memcpy(&count,header+0x50,4);memcpy(&values,header+0x58,8);
                        if(vtable<0x7000000000ULL||vtable>=0x7100000000ULL||
                           world!=expected_world||count!=4||!read_guest(values+0x24,result,8)||
                           result[0]!=0||result[1]!=0)continue;
                        found=values+0x24;++matches;
                    }
                }
                cursor+=size;
            }
        }
        at=end;
    }
    HeapFree(GetProcessHeap(),0,buffer);
    return matches==1?found:0;
}

#if 0 /* Abandoned hidden-health priming experiment; kept out of production. */
static int ko_prime_health(uintptr_t world,int side){(void)world;(void)side;return 1;}
static void ko_restore_health(void){}
static void ko_forget_health(void){}
#else
static void ko_forget_health(void){
    memset(ko_health_address,0,sizeof(ko_health_address));
    memset(ko_health_original,0,sizeof(ko_health_original));
    memset(ko_health_primed,0,sizeof(ko_health_primed));
    ko_health_saved=0;
}

static void ko_restore_health(void){
    if(ko_health_saved)for(int i=0;i<KO_HEALTH_FIELDS;++i)if(ko_health_address[i])
        InterlockedCompareExchange((volatile LONG*)ko_health_address[i],
                                   ko_health_original[i],ko_health_primed[i]);
    ko_forget_health();
}

static int ko_binding_health(uintptr_t expected_world,int side,LONG expected[KO_HEALTH_FIELDS]){
    static const uint32_t keys[KO_HEALTH_FIELDS]={
        0xa2f4531du,0x184c8c43u,0x6bf8f6a0u,0x7a3755e1u
    };
    const uintptr_t start=0x7007000000ULL,limit=0x7008000000ULL;
    unsigned char *buffer=(unsigned char*)HeapAlloc(GetProcessHeap(),0,1<<20);
    unsigned found[KO_HEALTH_FIELDS]={0};
    if(!buffer)return 0;
    for(uintptr_t at=start;at<limit;){
        MEMORY_BASIC_INFORMATION region;
        if(!VirtualQuery((void*)at,&region,sizeof(region)))break;
        uintptr_t end=(uintptr_t)region.BaseAddress+region.RegionSize;
        if(end<=at)break;
        if(region.State==MEM_COMMIT&&readable_writable(region.Protect)){
            for(uintptr_t cursor=at;cursor<end&&cursor<limit;){
                SIZE_T size=(SIZE_T)((end-cursor)>(1<<20)?(1<<20):(end-cursor));
                if(size>limit-cursor)size=(SIZE_T)(limit-cursor);
                if(read_guest(cursor,buffer,size))for(SIZE_T i=0;i+4<=size;++i){
                    uint32_t observed=0;memcpy(&observed,buffer+i,4);int which=-1;
                    for(int n=0;n<KO_HEALTH_FIELDS;++n)if(observed==keys[n]){which=n;break;}
                    if(which<0)continue;
                    uintptr_t key_address=cursor+i,vtable=0,world=0,values=0;
                    unsigned char header[0x70];uint32_t count=0;
                    if(key_address<8||!read_guest(key_address-8,header,sizeof(header)))continue;
                    memcpy(&vtable,header+0x18,8);memcpy(&world,header+0x28,8);
                    memcpy(&count,header+0x50,4);memcpy(&values,header+0x58,8);
                    if(vtable<0x7000000000ULL||vtable>=0x7100000000ULL||
                       world!=expected_world||count!=4||
                       !read_guest(values+(uintptr_t)side*0x30+0x24,&expected[which],4))continue;
                    ++found[which];
                }
                cursor+=size;
            }
        }
        at=end;
    }
    HeapFree(GetProcessHeap(),0,buffer);
    for(int i=0;i<KO_HEALTH_FIELDS;++i)if(found[i]!=1)return 0;
    return 1;
}

static int ko_prime_health(uintptr_t expected_world,int side){
    static const unsigned offsets[KO_HEALTH_FIELDS]={0x158,0x160,0x168,0x170};
    static const unsigned char signature[]={0x00,0x00,0x20,0x41,0x00,0x00,0xb4,0x42};
    const uintptr_t start=0x7007000000ULL,limit=0x7008000000ULL;
    unsigned char *buffer=(unsigned char*)HeapAlloc(GetProcessHeap(),0,1<<20);
    if(!buffer||side<0||side>1)return 0;
    LONG expected[KO_HEALTH_FIELDS]={0};
    if(!ko_binding_health(expected_world,side,expected)){HeapFree(GetProcessHeap(),0,buffer);return 0;}
    uintptr_t actor=0;unsigned matches=0;
    for(uintptr_t at=start;at<limit;){
        MEMORY_BASIC_INFORMATION region;
        if(!VirtualQuery((void*)at,&region,sizeof(region)))break;
        uintptr_t end=(uintptr_t)region.BaseAddress+region.RegionSize;
        if(end<=at)break;
        if(region.State==MEM_COMMIT&&readable_writable(region.Protect)){
            for(uintptr_t cursor=at;cursor<end&&cursor<limit;){
                SIZE_T size=(SIZE_T)((end-cursor)>(1<<20)?(1<<20):(end-cursor));
                if(size>limit-cursor)size=(SIZE_T)(limit-cursor);
                if(read_guest(cursor,buffer,size))for(SIZE_T i=0;i+sizeof(signature)<=size;++i){
                    if(memcmp(buffer+i,signature,sizeof(signature)))continue;
                    uintptr_t candidate=cursor+i-0x150;unsigned char state[0x180];
                    if(cursor+i<0x150||!read_guest(candidate,state,sizeof(state)))continue;
                    uint32_t candidate_side=~0u,header=0;float stamina=0,prefix_a=0,prefix_b=0;
                    memcpy(&candidate_side,state+0x114,4);memcpy(&header,state+0x140,4);
                    memcpy(&stamina,state,4);memcpy(&prefix_a,state+0x150,4);
                    memcpy(&prefix_b,state+0x154,4);
                    if(candidate_side!=(uint32_t)side||header!=112||prefix_a!=10.0f||
                       prefix_b!=90.0f||!(stamina>=0.0f&&stamina<=100.0f))continue;
                    int valid=1;
                    for(int n=0;n<KO_HEALTH_FIELDS;++n){float current=0,visible=0;
                        memcpy(&current,state+offsets[n],4);memcpy(&visible,state+offsets[n]+4,4);
                        LONG current_bits=0;memcpy(&current_bits,state+offsets[n],4);
                        if(current_bits!=expected[n]||
                           !(current>0.0f&&current<=100.0f&&visible>=0.0f&&visible<=100.0f)){valid=0;break;}
                    }
                    if(valid){actor=candidate;++matches;}
                }
                cursor+=size;
            }
        }
        at=end;
    }
    HeapFree(GetProcessHeap(),0,buffer);
    if(matches!=1){ko_forget_health();return 0;}
    LONG one=0x3f800000L;
    for(int i=0;i<KO_HEALTH_FIELDS;++i){
        uintptr_t field=actor+offsets[i];LONG original=InterlockedCompareExchange((volatile LONG*)field,0,0);
        float current=0;memcpy(&current,&original,4);
        ko_health_address[i]=field;ko_health_original[i]=original;
        ko_health_primed[i]=current>1.0f?one:original;
        if(InterlockedCompareExchange((volatile LONG*)field,ko_health_primed[i],original)!=original){
            ko_health_saved=1;ko_restore_health();return 0;
        }
    }
    ko_health_saved=1;return 1;
}
#endif

int ufc4_live_stamina_prepare(void){
    AcquireSRWLockExclusive(&setup_guard);
    if(!mask_address)mask_address=find_mask(marker,sizeof(marker)-1);
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

int ufc4_live_damage_prepare(void){
    AcquireSRWLockExclusive(&setup_guard);
    if(!damage_mask_address)damage_mask_address=find_mask(damage_marker,sizeof(damage_marker)-1);
    int ready=damage_mask_address!=NULL;
    ReleaseSRWLockExclusive(&setup_guard);
    return ready;
}

unsigned ufc4_live_damage_mask(void){
    volatile LONG *mask=damage_mask_address;
    return mask?(unsigned)InterlockedCompareExchange(mask,0,0)&2u:0;
}

int ufc4_live_damage_toggle(unsigned *selected){
    if(!selected||!ufc4_live_damage_prepare())return 0;
    volatile LONG *mask=damage_mask_address;
    for(;;){
        LONG old=InterlockedCompareExchange(mask,0,0);
        if(old<0||old>3)return 0;
        LONG next=old^2L;
        if(InterlockedCompareExchange(mask,next,old)==old){*selected=(unsigned)next;return 1;}
    }
}

int ufc4_live_ko_prepare(void){
    if(!ufc4_live_damage_prepare())return 0;
    volatile LONG *base=damage_mask_address;
    float strength=0.0f;
    memcpy(&strength,(const void*)(base+152),sizeof(strength));
    LONG flag=InterlockedCompareExchange(base+150,0,0);
    return strength==-1000000.0f&&(flag==0||flag==1);
}

int ufc4_live_ko_poll(void){
    if(!ko_requested)return 0;
    volatile LONG *base=damage_mask_address;
    if(!base){ko_requested=0;return 0;}
    LONG flag=InterlockedCompareExchange(base+150,0,0);
    if(flag==0){
        ko_requested=0;
        ko_result_address=0;
        return InterlockedCompareExchange(base+151,0,0)>ko_start_hits?2:0;
    }
    uint32_t result[2]={0,0};
    if(ko_result_address&&read_guest(ko_result_address,result,sizeof(result))&&
       ((result[0]==2&&result[1]==1)||(result[0]==1&&result[1]==2))){
        InterlockedCompareExchange(base+150,0,1);
        ko_requested=0;ko_result_address=0;
        return InterlockedCompareExchange(base+151,0,0)>ko_start_hits?2:3;
    }
    return 1;
}

void ufc4_live_ko_start_window(void){
    /* Kept for ABI compatibility. UFC 4 remains armed until a result/cancel. */
}

int ufc4_live_ko_toggle(unsigned *armed){
    if(!armed||!ufc4_live_ko_prepare())return 0;
    (void)ufc4_live_ko_poll();
    volatile LONG *base=damage_mask_address;
    if(ko_requested){
        if(InterlockedCompareExchange(base+150,0,1)!=1)return 0;
        ko_requested=0;ko_result_address=0;*armed=0;return 1;
    }
    int side;
#ifdef UFC4_TEST
    uintptr_t world=0;
    side=ko_test_side>=0?ko_test_side:ko_opponent_side(&world);
    ko_result_address=ko_test_side>=0?(uintptr_t)&ko_test_result:ko_match_result_address(world);
#else
    uintptr_t world=0;
    side=ko_opponent_side(&world);
    ko_result_address=side>=0?ko_match_result_address(world):0;
#endif
    if(side<0||!ko_result_address)return 0;
    ko_start_hits=InterlockedCompareExchange(base+151,0,0);
    InterlockedExchange(base+153,side);
    if(InterlockedCompareExchange(base+150,1,0)!=0)return 0;
    ko_requested=1;*armed=1;return 1;
}

#ifdef UFC4_TEST
__declspec(dllexport) int Ufc4TestStaminaPrepare(void){return ufc4_live_stamina_prepare();}
__declspec(dllexport) unsigned Ufc4TestStaminaToggle(int fighter){
    unsigned selected=0;return ufc4_live_stamina_toggle(fighter,&selected)?selected:~0u;
}
__declspec(dllexport) int Ufc4TestDamagePrepare(void){return ufc4_live_damage_prepare();}
__declspec(dllexport) unsigned Ufc4TestDamageToggle(void){
    unsigned selected=0;return ufc4_live_damage_toggle(&selected)?selected:~0u;
}
__declspec(dllexport) int Ufc4TestKoPrepare(void){return ufc4_live_ko_prepare();}
__declspec(dllexport) void Ufc4TestKoSide(int side){ko_test_side=side;}
__declspec(dllexport) unsigned Ufc4TestKoToggle(void){
    unsigned armed=0;return ufc4_live_ko_toggle(&armed)?armed:~0u;
}
__declspec(dllexport) int Ufc4TestKoPoll(void){return ufc4_live_ko_poll();}
__declspec(dllexport) void Ufc4TestKoStartWindow(void){ufc4_live_ko_start_window();}
__declspec(dllexport) void Ufc4TestKoMatchState(int state){
    ko_test_result[0]=state==3?2:0;ko_test_result[1]=state==3?1:0;
}
#endif
