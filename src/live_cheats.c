/* Session-only per-fighter control through a checked game binding-update hook. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <math.h>
#include "live_cheats.h"
#include "cheat_bindings.h"
#include "exact_hooks.h"
#include "next_hit_ko.h"
typedef struct {uintptr_t flag[2],hook[2]; const char *off[2],*on[2];} Profile;
static const Profile profiles[]={
    {{0xb444d78,0xb440610},{0x5A88787,0x5A887B8},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x76\xE8\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\x48\x8B\x47\x20\x8B\x4F\x58","\x48\x8B\x77\x60\x0F\xB7\x56\x1C\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x76\xE8\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\xE9\x3F\x02\x00\x00\x90\x90","\xE9\xC9\x12\x00\x00\x90\x90\x90\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"}},
    {{0xb452958,0xb44e1f0},{0x5A98F77,0x5A98FA8},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x36\xF7\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\x48\x8B\x47\x20\x8B\x4F\x58","\x48\x8B\x77\x60\x0F\xB7\x56\x1C\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x36\xF7\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\xE9\x3F\x02\x00\x00\x90\x90","\xE9\xC9\x12\x00\x00\x90\x90\x90\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"}},
};
typedef void (__attribute__((sysv_abi)) *BindingSyncFn)(void*,unsigned,unsigned);
static BindingSyncFn original_binding_sync;
static uintptr_t hooked_base;
static SRWLOCK binding_guard=SRWLOCK_INIT;
static CheatBindingCache binding_cache[8];
static volatile LONG binding_commands[2];
typedef struct {volatile LONG busy,command,known,enabled;volatile LONG64 tick;} Observation;
static Observation observations[2];
static volatile LONG64 fight_presence_tick,sparring_clock_obj,sparring_clock_value;
static volatile LONG64 sparring_elapsed_obj,sparring_elapsed_value;
static volatile LONG64 sparring_round_elapsed_obj,sparring_round_elapsed_value;
static volatile LONG64 sparring_round_remaining_obj,sparring_round_remaining_value;
static volatile LONG sparring_clock_paused,sparring_clock_scanning;
static volatile LONG sparring_clock_saved_bits;
static volatile LONG sparring_elapsed_saved_bits;
static volatile LONG sparring_round_elapsed_saved,sparring_round_remaining_saved;
static int read_memory(uintptr_t address,void *out,size_t size){SIZE_T got=0;return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)address,out,size,&got)&&got==size;}
static int readable_page(DWORD protection){
 DWORD page=protection&0xff;
 return !(protection&(PAGE_GUARD|PAGE_NOACCESS))&&
        (page==PAGE_READONLY||page==PAGE_READWRITE||page==PAGE_WRITECOPY||
         page==PAGE_EXECUTE_READ||page==PAGE_EXECUTE_READWRITE||page==PAGE_EXECUTE_WRITECOPY);
}
static uintptr_t sparring_clock_find_named_near(uintptr_t anchor,const char *field_name,size_t field_name_length,uintptr_t *object_out){
 static const uintptr_t field_vtable=0x909c227f0ULL;
 unsigned char anchor_header[0x68];uintptr_t world=0;
 if(!anchor||!read_memory(anchor,anchor_header,sizeof(anchor_header))||
    memcmp(anchor_header,&field_vtable,8))return 0;
 memcpy(&world,anchor_header+0x28,8);if(!world)return 0;
 uintptr_t low=anchor>0x1000000?anchor-0x1000000:0x10000,high=anchor+0x1000000;
 unsigned char *copy=(unsigned char*)HeapAlloc(GetProcessHeap(),0,1<<20);
 if(!copy)return 0;
 uintptr_t found=0,found_object=0;unsigned matches=0;
 for(uintptr_t at=low;at<high;){
  MEMORY_BASIC_INFORMATION region;if(!VirtualQuery((void*)at,&region,sizeof(region)))break;
  uintptr_t end=(uintptr_t)region.BaseAddress+region.RegionSize;if(end<=at)break;
  if(region.State==MEM_COMMIT&&readable_page(region.Protect))for(uintptr_t cursor=at;cursor<end&&cursor<high;){
   SIZE_T size=(SIZE_T)((end-cursor)>(1<<20)?(1<<20):(end-cursor));
   if(size>high-cursor)size=(SIZE_T)(high-cursor);
   if(read_memory(cursor,copy,size))for(SIZE_T offset=0;offset+8<=size;offset+=8){
    uintptr_t vt=0;memcpy(&vt,copy+offset,8);if(vt!=field_vtable)continue;
    uintptr_t object=cursor+offset,name_address=0,object_world=0,buffer=0;unsigned char header[0x68],data[0x24];
    uint32_t element_size=0;char name[48]={0};float current=0;
    if(!read_memory(object,header,sizeof(header)))continue;
    memcpy(&name_address,header+8,8);memcpy(&object_world,header+0x28,8);
    memcpy(&element_size,header+0x58,4);memcpy(&buffer,header+0x60,8);
    if(object_world!=world||element_size!=4||field_name_length>=sizeof(name)||
       !read_memory(name_address,name,field_name_length)||memcmp(name,field_name,field_name_length)||
       !read_memory(buffer,data,sizeof(data)))continue;
    uint16_t data_offset=0;memcpy(&data_offset,data+0x1c,2);
    if(data_offset!=32||!read_memory(buffer+data_offset,&current,4)||
       !isfinite(current)||current<=0.f||current>600.f)continue;
    found=buffer+data_offset;found_object=object;++matches;
   }
   cursor+=size;
  }
  at=end;
 }
 HeapFree(GetProcessHeap(),0,copy);
 if(matches!=1)return 0;
 if(object_out)*object_out=found_object;return found;
}
static uintptr_t sparring_clock_find_near(uintptr_t anchor,uintptr_t *object_out){
 return sparring_clock_find_named_near(anchor,"Training_TimeRemaining",23,object_out);
}
static int sparring_clock_valid_named(uintptr_t obj,uintptr_t value,const char *field_name,size_t field_name_length){
 static const uintptr_t field_vtable=0x909c227f0ULL;
 unsigned char header[0x68],data[0x20];uintptr_t vt=0,name_address=0,buffer=0;char name[48]={0};uint32_t element_size=0;
 if(!obj||!value||!read_memory(obj,header,sizeof(header)))return 0;
 memcpy(&vt,header,8);memcpy(&name_address,header+8,8);memcpy(&element_size,header+0x58,4);memcpy(&buffer,header+0x60,8);
 if(vt!=field_vtable||element_size!=4||field_name_length>=sizeof(name)||
    !read_memory(name_address,name,field_name_length)||memcmp(name,field_name,field_name_length)||
    !read_memory(buffer,data,sizeof(data)))return 0;
 uint16_t offset=0;memcpy(&offset,data+0x1c,2);return offset==32&&buffer+offset==value;
}
static int sparring_clock_valid(uintptr_t obj,uintptr_t value){return sparring_clock_valid_named(obj,value,"Training_TimeRemaining",23);}
static DWORD WINAPI sparring_clock_scan_worker(void *unused){
 (void)unused;
 uintptr_t anchor=0,object=0,value=0,elapsed_object=0,elapsed_value=0;
 uintptr_t round_elapsed_object=0,round_elapsed_value=0,round_remaining_object=0,round_remaining_value=0;
 if(read_memory(0x90b444c10ULL,&anchor,8)){
  value=sparring_clock_find_near(anchor,&object);
  elapsed_value=sparring_clock_find_named_near(anchor,"Timer_Training_Countdown_Elapsed",32,&elapsed_object);
  round_elapsed_value=sparring_clock_find_named_near(anchor,"ROUND_TIME_ELAPSED",18,&round_elapsed_object);
  round_remaining_value=sparring_clock_find_named_near(anchor,"ROUND_TIME_REMAINING",20,&round_remaining_object);
 }
 if(value){InterlockedExchange64(&sparring_clock_obj,(LONG64)object);InterlockedExchange64(&sparring_clock_value,(LONG64)value);}
 if(elapsed_value){InterlockedExchange64(&sparring_elapsed_obj,(LONG64)elapsed_object);InterlockedExchange64(&sparring_elapsed_value,(LONG64)elapsed_value);}
 if(round_elapsed_value){InterlockedExchange64(&sparring_round_elapsed_obj,(LONG64)round_elapsed_object);InterlockedExchange64(&sparring_round_elapsed_value,(LONG64)round_elapsed_value);}
 if(round_remaining_value){InterlockedExchange64(&sparring_round_remaining_obj,(LONG64)round_remaining_object);InterlockedExchange64(&sparring_round_remaining_value,(LONG64)round_remaining_value);}
 InterlockedExchange(&sparring_clock_scanning,0);
 return 0;
}
static void sparring_clock_hold_value(uintptr_t value,volatile LONG *saved_bits,int increasing){
 LONG current_bits=0;if(!read_memory(value,&current_bits,4))return;float cur=0;memcpy(&cur,&current_bits,4);
 if(!isfinite(cur)||cur<0.f||cur>600.f)return;
 LONG saved_value_bits=InterlockedCompareExchange(saved_bits,0,0);float saved=0;memcpy(&saved,&saved_value_bits,4);
 if(saved_value_bits==0||(increasing?cur+5.f<saved:cur>saved+5.f))InterlockedExchange(saved_bits,current_bits);
 else if(current_bits!=saved_value_bits)InterlockedExchange((volatile LONG*)value,saved_value_bits);
}
static void sparring_clock_hold_integer(uintptr_t value,volatile LONG *saved_value,int increasing){
 LONG current=0;if(!read_memory(value,&current,4)||current<0||current>3600)return;
 LONG saved=InterlockedCompareExchange(saved_value,-1,-1);
 if(saved<0||(increasing?current+5<saved:current>saved+5))InterlockedExchange(saved_value,current);
 else if(current!=saved)InterlockedExchange((volatile LONG*)value,saved);
}
static void sparring_clock_step(void){
 if(!InterlockedCompareExchange(&sparring_clock_paused,0,0)){InterlockedExchange(&sparring_clock_saved_bits,0);InterlockedExchange(&sparring_elapsed_saved_bits,0);InterlockedExchange(&sparring_round_elapsed_saved,-1);InterlockedExchange(&sparring_round_remaining_saved,-1);return;}
 uintptr_t obj=(uintptr_t)InterlockedCompareExchange64(&sparring_clock_obj,0,0);
 uintptr_t value=(uintptr_t)InterlockedCompareExchange64(&sparring_clock_value,0,0);
 uintptr_t elapsed_obj=(uintptr_t)InterlockedCompareExchange64(&sparring_elapsed_obj,0,0);
 uintptr_t elapsed_value=(uintptr_t)InterlockedCompareExchange64(&sparring_elapsed_value,0,0);
 uintptr_t round_elapsed_obj=(uintptr_t)InterlockedCompareExchange64(&sparring_round_elapsed_obj,0,0);
 uintptr_t round_elapsed_value=(uintptr_t)InterlockedCompareExchange64(&sparring_round_elapsed_value,0,0);
 uintptr_t round_remaining_obj=(uintptr_t)InterlockedCompareExchange64(&sparring_round_remaining_obj,0,0);
 uintptr_t round_remaining_value=(uintptr_t)InterlockedCompareExchange64(&sparring_round_remaining_value,0,0);
 int remaining_valid=sparring_clock_valid(obj,value);
 int elapsed_valid=sparring_clock_valid_named(elapsed_obj,elapsed_value,"Timer_Training_Countdown_Elapsed",32);
 int round_elapsed_valid=sparring_clock_valid_named(round_elapsed_obj,round_elapsed_value,"ROUND_TIME_ELAPSED",18);
 int round_remaining_valid=sparring_clock_valid_named(round_remaining_obj,round_remaining_value,"ROUND_TIME_REMAINING",20);
 if(!remaining_valid){
  InterlockedExchange64(&sparring_clock_obj,0);InterlockedExchange64(&sparring_clock_value,0);InterlockedExchange(&sparring_clock_saved_bits,0);
 }
 if(!elapsed_valid){
  InterlockedExchange64(&sparring_elapsed_obj,0);InterlockedExchange64(&sparring_elapsed_value,0);InterlockedExchange(&sparring_elapsed_saved_bits,0);
 }
 if(!round_elapsed_valid){
  InterlockedExchange64(&sparring_round_elapsed_obj,0);InterlockedExchange64(&sparring_round_elapsed_value,0);InterlockedExchange(&sparring_round_elapsed_saved,-1);
 }
 if(!round_remaining_valid){
  InterlockedExchange64(&sparring_round_remaining_obj,0);InterlockedExchange64(&sparring_round_remaining_value,0);InterlockedExchange(&sparring_round_remaining_saved,-1);
 }
 if(!remaining_valid||!elapsed_valid||!round_elapsed_valid||!round_remaining_valid){
  if(InterlockedCompareExchange(&sparring_clock_scanning,1,0)==0){
   HANDLE t=CreateThread(NULL,0,sparring_clock_scan_worker,NULL,0,NULL);
   if(t)CloseHandle(t);else InterlockedExchange(&sparring_clock_scanning,0);
  }
 }
 if(remaining_valid)sparring_clock_hold_value(value,&sparring_clock_saved_bits,0);
 if(elapsed_valid)sparring_clock_hold_value(elapsed_value,&sparring_elapsed_saved_bits,1);
 if(round_elapsed_valid)sparring_clock_hold_integer(round_elapsed_value,&sparring_round_elapsed_saved,1);
 if(round_remaining_valid)sparring_clock_hold_integer(round_remaining_value,&sparring_round_remaining_saved,0);
}
void live_cheats_sparring_clock_toggle(void){
 int next=!InterlockedCompareExchange(&sparring_clock_paused,0,0);
 InterlockedExchange(&sparring_clock_saved_bits,0);
 InterlockedExchange(&sparring_elapsed_saved_bits,0);
 InterlockedExchange(&sparring_round_elapsed_saved,-1);
 InterlockedExchange(&sparring_round_remaining_saved,-1);
 InterlockedExchange(&sparring_clock_paused,next);
 if(next)sparring_clock_step();
}
int live_cheats_sparring_clock_get(void){return (int)InterlockedCompareExchange(&sparring_clock_paused,0,0);}
void live_cheats_sparring_clock_tick(void){sparring_clock_step();}
static const unsigned char sync_signature[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18,0x48,0x8b,0x05,0x88,0xb5,0x9e,0x09,0x48,0x8b,0x00,0x48,0x89,0x45,0xd0};
static void __attribute__((sysv_abi)) hooked_binding_sync(void *binder,unsigned first,unsigned count){
    unsigned commands[2];CheatBindingFrame frame;memset(&frame,0,sizeof(frame));frame.player_one=-1;
    for(int i=0;i<2;++i)commands[i]=(unsigned)InterlockedCompareExchange(&binding_commands[i],0,0);
    if(TryAcquireSRWLockExclusive(&binding_guard)){
        if(ko.cancel){ko_release(3);ko.cancel=0;}
        CheatBindingCache *cache=&binding_cache[0];
        for(int i=0;i<8;++i)if(binding_cache[i].owner==(uintptr_t)binder){cache=&binding_cache[i];break;}else if(!binding_cache[i].owner)cache=&binding_cache[i];
        cheat_bindings_apply(cache,(uintptr_t)binder,first,count,commands,&frame);
        if(frame.fighters_ready)ko_observe((uintptr_t)binder,frame.player_one,GetTickCount64());
        ReleaseSRWLockExclusive(&binding_guard);
    }
    original_binding_sync(binder,first,count);
    if(frame.fighters_ready)InterlockedExchange64(&fight_presence_tick,(LONG64)GetTickCount64());
    unsigned known[2],enabled[2];cheat_bindings_observe(&frame,known,enabled);
    for(int i=0;i<2;++i)if((frame.count[i]||frame.player_one==0)&&InterlockedCompareExchange(&observations[i].busy,1,0)==0){
        InterlockedExchange(&observations[i].known,(LONG)known[i]);InterlockedExchange(&observations[i].enabled,(LONG)enabled[i]);
        InterlockedExchange64(&observations[i].tick,(LONG64)GetTickCount64());InterlockedExchange(&observations[i].command,(LONG)commands[i]);InterlockedExchange(&observations[i].busy,0);
    }
}
static int flag_handles_ready(uintptr_t base){
    const char *names[]={"Fighter_Disable_StaminaLoss","Fighter_Disable_HealthLoss"};
    for(int i=0;i<2;++i){uintptr_t object=0,name=0;char text[32]={0};
        if(!read_memory(base+profiles[1].flag[i],&object,8)||!read_memory(object+8,&name,8)||!read_memory(name,text,strlen(names[i])+1)||strcmp(text,names[i]))return 0;
    }
    return 1;
}
static int ensure_binding_hook(uintptr_t base){
    if(hooked_base)return hooked_base==base;
    unsigned char bytes[sizeof(sync_signature)];
    if(!read_memory(base+0x34cd10,bytes,sizeof(bytes))||memcmp(bytes,sync_signature,sizeof(bytes)))return 0;
    ExactHook hook={(unsigned char*)(base+0x34cd10),(void*)hooked_binding_sync,(void**)&original_binding_sync,sync_signature,17};
    const char *error=NULL;
    if(!install_exact_hooks(&hook,1,&error))return 0;
    hooked_base=base;return 1;
}
static unsigned publish(LiveCheats *s,int i,int blocked){
    LiveCheat *f=&s->flags[i];unsigned bits=s->target|((blocked?0:f->owned_mask)<<2)|((!blocked&&f->requested?s->target:0)<<4);
    unsigned old=(unsigned)InterlockedCompareExchange(&binding_commands[i],0,0);
    if((old&63)!=bits){old=((old+64)&~63u)|bits;InterlockedExchange(&binding_commands[i],(LONG)old);}
    return old;
}
void live_cheats_init(LiveCheats *s,uintptr_t guest_base){
    memset(s,0,sizeof(*s));s->base=guest_base;s->target=LIVE_PLAYER1;InterlockedExchange64(&fight_presence_tick,0);
    for(int i=0;i<2;++i){InterlockedExchange(&binding_commands[i],LIVE_PLAYER1);InterlockedExchange64(&observations[i].tick,0);}
}
void live_cheats_tick(LiveCheats *s){
    const Profile *p=&profiles[1];unsigned char code[32];int available=0,readable=0;
    /* Binding synchronization is verified for game code 01.005/01.006 only. */
    for(int i=0;i<2;++i)if(read_memory(s->base+p->hook[i],code,sizeof(code))){readable=1;if(!memcmp(code,p->off[i],32)||!memcmp(code,p->on[i],32))available=1;}
    int handles=flag_handles_ready(s->base);
    int ready=available&&handles&&ensure_binding_hook(s->base);
    for(int i=0;i<LIVE_COUNT;++i){
        LiveCheat *f=&s->flags[i];int blocked=-1;
        if(!ready)blocked=!handles?LIVE_WAIT:readable?LIVE_UNSUPPORTED:LIVE_WAIT;
        else if(!read_memory(s->base+p->hook[i],code,sizeof(code)))blocked=LIVE_WAIT;
        else if(memcmp(code,p->off[i],32))blocked=!memcmp(code,p->on[i],32)?LIVE_LAUNCHER:LIVE_UNSUPPORTED;
        if(!ready||blocked>=0){publish(s,i,1);f->status=blocked>=0?blocked:LIVE_WAIT;continue;}
        unsigned command=publish(s,i,0),required=s->target|f->owned_mask;
        if(InterlockedCompareExchange(&observations[i].busy,1,0)!=0)continue;
        unsigned observed=(unsigned)InterlockedCompareExchange(&observations[i].command,0,0);
        unsigned known=(unsigned)InterlockedCompareExchange(&observations[i].known,0,0),enabled=(unsigned)InterlockedCompareExchange(&observations[i].enabled,0,0);
        uint64_t last=(uint64_t)InterlockedCompareExchange64(&observations[i].tick,0,0);InterlockedExchange(&observations[i].busy,0);
        if(!last||GetTickCount64()-last>1000||observed!=command||(known&required)!=required){f->status=f->owned_mask&&!f->requested?LIVE_RESTORING:LIVE_WAIT;continue;}
        if(f->requested)f->status=(enabled&s->target)==s->target?LIVE_ON:LIVE_WAIT;
        else if(f->owned_mask&s->target)f->status=(enabled&s->target)?LIVE_RESTORING:LIVE_OFF;
        else f->status=(enabled&s->target)?LIVE_GAME:LIVE_OFF;
    }
}
int live_cheats_toggle(LiveCheats *s,int which){
    if(which<0||which>=LIVE_COUNT)return 0;
    live_cheats_tick(s);LiveCheat *f=&s->flags[which];
    if((hooked_base&&hooked_base!=s->base)||f->status==LIVE_LAUNCHER||f->status==LIVE_UNSUPPORTED)return 0;
    if(f->requested||f->status==LIVE_GAME){f->requested=0;f->owned_mask|=s->target;}
    else{f->requested=1;f->owned_mask|=s->target;}
    live_cheats_tick(s);return 1;
}
void live_cheats_stop(LiveCheats *s){InterlockedExchange(&sparring_clock_paused,0);InterlockedExchange(&sparring_clock_saved_bits,0);InterlockedExchange(&sparring_elapsed_saved_bits,0);InterlockedExchange(&sparring_round_elapsed_saved,-1);InterlockedExchange(&sparring_round_remaining_saved,-1);live_ko_poll(0);for(int i=0;i<2;++i)s->flags[i].requested=0;live_cheats_tick(s);}

int live_cheats_fight_ready(void){
 uint64_t last=(uint64_t)InterlockedCompareExchange64(&fight_presence_tick,0,0);
 return last&&GetTickCount64()-last<1000;
}

int live_ko_toggle(void){int ok=0;AcquireSRWLockExclusive(&binding_guard);
 uint64_t now=GetTickCount64();
 if(ko.cancel){ok=0;}
 else if(ko.requested||ko.armed){ko.requested=0;ko.cancel=1;ko.result=3;ok=1;}
 else if(hooked_base&&ko.candidate.valid&&ko_identity(&ko.candidate)&&ko_values(&ko.candidate)){
  int32_t value=-1;if(ko_read(ko.candidate.field[0][1-ko.candidate.player],&value,4)&&value==0){ko.requested=1;ko.result=1;ko.deadline=now+25000;ok=1;}
 }ReleaseSRWLockExclusive(&binding_guard);return ok;}
int live_ko_poll(int allowed){AcquireSRWLockExclusive(&binding_guard);uint64_t now=GetTickCount64();
 if((ko.armed||ko.requested)&&(!allowed||now>=ko.deadline)){ko.requested=0;ko.cancel=1;ko.result=3;}
 int result=ko.result;ReleaseSRWLockExclusive(&binding_guard);return result;}
