/* Process-local Vulkan layer bootstrap for sacredfire-game.exe. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <wchar.h>
#include <string.h>
#include <stdint.h>
static HMODULE system_winmm;
static int read_intro_preference(const wchar_t *bin){
    wchar_t ini[MAX_PATH];wsprintfW(ini,L"%ls\\ufc4_actions.ini",bin);
    HANDLE file=CreateFileW(ini,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return -1;
    int found=-1;
    char data[16384];DWORD size=0;
    if(ReadFile(file,data,sizeof(data),&size,NULL)&&size<sizeof(data)){
        const char key[]="SkipStartupSplashes=";
        for(DWORD i=0;i+sizeof(key)<=size;i++){
            if((i==0||data[i-1]=='\n')&&!memcmp(data+i,key,sizeof(key)-1)&&
               (data[i+sizeof(key)-1]=='0'||data[i+sizeof(key)-1]=='1')&&
               (i+sizeof(key)==size||data[i+sizeof(key)]=='\r'||data[i+sizeof(key)]=='\n'))
                found=data[i+sizeof(key)-1]-'0';
        }
    }
    CloseHandle(file);return found;
}
#ifdef UFC4_TEST
static void apply_intro_preference(const wchar_t *bin){
    int found=read_intro_preference(bin);
    if(found>=0)SetEnvironmentVariableW(L"BB_SKIP_MOVIES",found?L"1":NULL);
}
#endif
typedef struct{uint64_t offset;uint32_t size;const unsigned char *original,*patched;unsigned char current[6500];} StartupRecord;
static int seek_read(HANDLE file,uint64_t offset,void *data,DWORD size){
    LARGE_INTEGER pos;pos.QuadPart=(LONGLONG)offset;DWORD got=0;
    return SetFilePointerEx(file,pos,NULL,FILE_BEGIN)&&ReadFile(file,data,size,&got,NULL)&&got==size;
}
static int seek_write(HANDLE file,uint64_t offset,const void *data,DWORD size){
    LARGE_INTEGER pos;pos.QuadPart=(LONGLONG)offset;DWORD got=0;
    return SetFilePointerEx(file,pos,NULL,FILE_BEGIN)&&WriteFile(file,data,size,&got,NULL)&&got==size;
}
/* Returns -1 on mismatch/I/O failure, 0 when already synced, 1 when changed. */
static int sync_startup_assets(const wchar_t *archive_path,const wchar_t *bundle_path,int enabled){
    HANDLE bundle=CreateFileW(bundle_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(bundle==INVALID_HANDLE_VALUE)return -1;
    DWORD length=GetFileSize(bundle,NULL),got=0;
    unsigned char bytes[32768];
    int valid=length!=INVALID_FILE_SIZE&&length<=sizeof(bytes)&&length>=12&&
              ReadFile(bundle,bytes,length,&got,NULL)&&got==length;
    CloseHandle(bundle);
    if(!valid||memcmp(bytes,"UFC4ST01",8)||memcmp(bytes+8,"\3\0\0\0",4))return -1;
    const uint64_t offsets[]={1777552,1850720,4374240};
    const uint32_t sizes[]={2916,2862,6427};
    StartupRecord records[3];size_t cursor=12;
    for(int i=0;i<3;i++){
        if(cursor+12>length)return -1;
        memcpy(&records[i].offset,bytes+cursor,8);cursor+=8;
        memcpy(&records[i].size,bytes+cursor,4);cursor+=4;
        if(records[i].offset!=offsets[i]||records[i].size!=sizes[i]||cursor+2*sizes[i]>length)return -1;
        records[i].original=bytes+cursor;cursor+=sizes[i];
        records[i].patched=bytes+cursor;cursor+=sizes[i];
    }
    if(cursor!=length)return -1;
    HANDLE archive=CreateFileW(archive_path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                               NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(archive==INVALID_HANDLE_VALUE)return -1;
    LARGE_INTEGER size;
    if(!GetFileSizeEx(archive,&size)||size.QuadPart<10000000){CloseHandle(archive);return -1;}
    int result=0;
    for(int i=0;i<3;i++){
        StartupRecord *r=&records[i];
        if(!seek_read(archive,r->offset,r->current,r->size)||
           (memcmp(r->current,r->original,r->size)&&memcmp(r->current,r->patched,r->size))){
            CloseHandle(archive);return -1;
        }
    }
    int changed[3]={0};
    for(int i=0;i<3;i++){
        StartupRecord *r=&records[i];const unsigned char *desired=enabled?r->patched:r->original;
        if(!memcmp(r->current,desired,r->size))continue;
        if(!seek_write(archive,r->offset,desired,r->size))goto rollback;
        changed[i]=1;result=1;
    }
    if(result&&!FlushFileBuffers(archive))goto rollback;
    for(int i=0;i<3;i++){
        StartupRecord *r=&records[i];const unsigned char *desired=enabled?r->patched:r->original;
        unsigned char check[6500];
        if(!seek_read(archive,r->offset,check,r->size)||memcmp(check,desired,r->size))goto rollback;
    }
    CloseHandle(archive);return result;
rollback:
    for(int i=0;i<3;i++)if(changed[i])seek_write(archive,records[i].offset,records[i].current,records[i].size);
    FlushFileBuffers(archive);CloseHandle(archive);return -1;
}
static int sync_startup_preference(const wchar_t *bin,int enabled){
    wchar_t list[MAX_PATH],bundle[MAX_PATH],archive_path[1024];
    wsprintfW(list,L"%ls\\ufc4_buttonart_archive.txt",bin);
    wsprintfW(bundle,L"%ls\\ufc4_startup_flow.bin",bin);
    HANDLE file=CreateFileW(list,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return -1;
    char path[1024];DWORD size=0;
    int good=ReadFile(file,path,sizeof(path)-1,&size,NULL)&&size>0&&size<sizeof(path);
    CloseHandle(file);if(!good)return -1;
    while(size&&(path[size-1]=='\r'||path[size-1]=='\n'||path[size-1]==' '))size--;
    path[size]=0;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,archive_path,1024))return -1;
    return sync_startup_assets(archive_path,bundle,enabled);
}
#ifdef UFC4_TEST
__declspec(dllexport) void Ufc4TestApplyIntroPreference(const wchar_t *bin){apply_intro_preference(bin);}
__declspec(dllexport) int Ufc4TestSyncStartupAssets(const wchar_t *archive,const wchar_t *bundle,int enabled){return sync_startup_assets(archive,bundle,enabled);}
#endif
static void enable_layer(HMODULE self){
    wchar_t exe[MAX_PATH],path[MAX_PATH],value[4096];
    if(!GetModuleFileNameW(NULL,exe,MAX_PATH)||!wcsstr(exe,L"sacredfire-game.exe"))return;
    if(!GetModuleFileNameW(self,path,MAX_PATH))return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;*slash=0;
    wchar_t required[MAX_PATH];wsprintfW(required,L"%ls\\ufc4_actions_layer.json",path);
    if(GetFileAttributesW(required)==INVALID_FILE_ATTRIBUTES)return;
    DWORD n=GetEnvironmentVariableW(L"VK_LAYER_PATH",value,4096);
    if(!n)SetEnvironmentVariableW(L"VK_LAYER_PATH",path);
    else if(n<4096&&!wcsstr(value,path)){wchar_t merged[4096];wsprintfW(merged,L"%ls;%ls",path,value);SetEnvironmentVariableW(L"VK_LAYER_PATH",merged);}
    n=GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",value,4096);
    if(!n)SetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",L"VK_LAYER_UFC4_actions");
    else if(n<4096&&!wcsstr(value,L"VK_LAYER_UFC4_actions")){wchar_t merged[4096];wsprintfW(merged,L"VK_LAYER_UFC4_actions;%ls",value);SetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",merged);}
    int preference=read_intro_preference(path);
    if(preference>=0){
        SetEnvironmentVariableW(L"BB_SKIP_MOVIES",preference?L"1":NULL);
        sync_startup_preference(path,preference);
    }
}
MMRESULT WINAPI ProxyTimeBeginPeriod(UINT period){
    if(!system_winmm){wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);lstrcatW(path,L"\\winmm.dll");system_winmm=LoadLibraryW(path);}
    typedef MMRESULT (WINAPI *Fn)(UINT);Fn original=system_winmm?(Fn)GetProcAddress(system_winmm,"timeBeginPeriod"):NULL;
    return original?original(period):TIMERR_NOCANDO;
}
MMRESULT WINAPI ProxyTimeEndPeriod(UINT period){
    if(!system_winmm){wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);lstrcatW(path,L"\\winmm.dll");system_winmm=LoadLibraryW(path);}
    typedef MMRESULT (WINAPI *Fn)(UINT);Fn original=system_winmm?(Fn)GetProcAddress(system_winmm,"timeEndPeriod"):NULL;
    return original?original(period):TIMERR_NOCANDO;
}
BOOL WINAPI DllMain(HMODULE self,DWORD reason,LPVOID reserved){(void)reserved;if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(self);enable_layer(self);}return TRUE;}
