#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ufc4_cheats_config.h"

#define MAX_CONFIG 16384
#define MAX_IDS 64
#define MAX_ID 80
static const char *stamina_ids[]={"ufc4_stamina_left","ufc4_stamina_right"};
#ifdef UFC4_TEST
static wchar_t test_path[MAX_PATH];
void ufc4_cheats_test_path(const wchar_t *path){lstrcpynW(test_path,path?path:L"",MAX_PATH);}
#endif

static int config_path(wchar_t path[MAX_PATH]){
#ifdef UFC4_TEST
    if(test_path[0]){lstrcpynW(path,test_path,MAX_PATH);return 1;}
#endif
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if(!n||n>=MAX_PATH-wcslen(L"\\UFC4Native\\cheats.json"))return 0;
    lstrcatW(path,L"\\UFC4Native\\cheats.json");return 1;
}
static int pack_available(void){
#ifdef UFC4_TEST
    if(test_path[0])return 1;
#endif
    wchar_t path[MAX_PATH];HMODULE module=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(uintptr_t)&pack_available,&module)||
       !GetModuleFileNameW(module,path,MAX_PATH))return 0;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return 0;
    if((size_t)(slash-path)+wcslen(L"\\..\\cheats\\ufc4-cheats.json")>=MAX_PATH)return 0;
    lstrcpyW(slash,L"\\..\\cheats\\ufc4-cheats.json");
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    char data[4097]={0};DWORD got=0;int ok=ReadFile(file,data,4096,&got,NULL)&&got<4096;
    CloseHandle(file);
    return ok&&strstr(data,"\"ufc4_stamina_left\"")&&strstr(data,"\"ufc4_stamina_right\"");
}
static int read_config(const wchar_t *path,char data[MAX_CONFIG+1],size_t *length){
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    LARGE_INTEGER size;DWORD got=0;
    int ok=GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<=MAX_CONFIG&&
           ReadFile(file,data,(DWORD)size.QuadPart,&got,NULL)&&got==(DWORD)size.QuadPart;
    CloseHandle(file);
    if(!ok)return 0;
    *length=(size_t)got;data[got]=0;return 1;
}
static int id_char(unsigned char ch){return (ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
    (ch>='0'&&ch<='9')||ch=='_'||ch=='-';}
static size_t spaces(const char *data,size_t length,size_t i){while(i<length&&(data[i]==' '||data[i]=='\r'||data[i]=='\n'||data[i]=='\t'))++i;return i;}
static size_t string_end(const char *data,size_t length,size_t start){
    if(start>=length||data[start]!='"')return 0;
    for(size_t i=start+1;i<length;++i){
        if(data[i]=='\\'){++i;continue;}
        if(data[i]=='"')return i+1;
    }
    return 0;
}
static int parse_enabled(const char *data,size_t length,size_t *begin,size_t *end,
                         char ids[MAX_IDS][MAX_ID],int *count){
    size_t first=spaces(data,length,0);if(first>=length||data[first]!='{')return 0;
    int depth=0,brackets=0,found=0;*count=0;
    for(size_t i=first;i<length;++i){
        if(data[i]=='"'){
            size_t after=string_end(data,length,i);if(!after)return 0;
            if(depth==1&&brackets==0&&after-i==9&&!memcmp(data+i,"\"enabled\"",9)){
                size_t colon=spaces(data,length,after),a=spaces(data,length,colon+1);
                if(colon>=length||data[colon]!=':'||a>=length||data[a]!='['||found++)return 0;
                *begin=a;size_t j=spaces(data,length,a+1);
                while(j<length&&data[j]!=']'){
                    size_t stop=string_end(data,length,j);
                    if(!stop||stop-j<3||stop-j>MAX_ID+1||*count>=MAX_IDS)return 0;
                    size_t id_length=stop-j-2;
                    for(size_t k=0;k<id_length;++k)if(!id_char((unsigned char)data[j+1+k]))return 0;
                    memcpy(ids[*count],data+j+1,id_length);ids[*count][id_length]=0;
                    for(int k=0;k<*count;++k)if(!strcmp(ids[k],ids[*count]))return 0;
                    ++*count;j=spaces(data,length,stop);
                    if(j<length&&data[j]==','){j=spaces(data,length,j+1);if(j>=length||data[j]==']')return 0;}
                    else if(j>=length||data[j]!=']')return 0;
                }
                if(j>=length)return 0;*end=j+1;i=j;continue;
            }
            i=after-1;continue;
        }
        if(data[i]=='{')++depth;
        else if(data[i]=='}'){if(--depth<0)return 0;}
        else if(data[i]=='[')++brackets;
        else if(data[i]==']'){if(--brackets<0)return 0;}
    }
    return found==1&&depth==0&&brackets==0;
}
static unsigned mask_for(char ids[MAX_IDS][MAX_ID],int count){
    unsigned mask=0;for(int i=0;i<count;++i)for(int j=0;j<2;++j)
        if(!strcmp(ids[i],stamina_ids[j]))mask|=1u<<j;
    return mask;
}
int ufc4_cheats_read(unsigned *enabled){
    wchar_t path[MAX_PATH];char data[MAX_CONFIG+1],ids[MAX_IDS][MAX_ID];size_t length=0,begin=0,end=0;int count=0;
    if(!enabled||!pack_available()||!config_path(path)||!read_config(path,data,&length)||
       !parse_enabled(data,length,&begin,&end,ids,&count))return 0;
    *enabled=mask_for(ids,count);return 1;
}
int ufc4_cheats_toggle(int fighter,unsigned *enabled){
    if(fighter<0||fighter>1||!enabled)return 0;
    wchar_t path[MAX_PATH],temp[MAX_PATH],backup[MAX_PATH];char data[MAX_CONFIG+1],ids[MAX_IDS][MAX_ID];
    size_t length=0,begin=0,end=0;int count=0;
    if(!pack_available()||!config_path(path)||!read_config(path,data,&length)||
       !parse_enabled(data,length,&begin,&end,ids,&count))return 0;
    int found=-1;for(int i=0;i<count;++i)if(!strcmp(ids[i],stamina_ids[fighter]))found=i;
    if(found>=0){for(int i=found;i<count-1;++i)memcpy(ids[i],ids[i+1],MAX_ID);--count;}
    else{if(count>=MAX_IDS)return 0;lstrcpynA(ids[count++],stamina_ids[fighter],MAX_ID);}
    char output[MAX_CONFIG+128];size_t used=0;
    if(begin>sizeof(output))return 0;memcpy(output,data,begin);used=begin;output[used++]='[';
    for(int i=0;i<count;++i){size_t n=strlen(ids[i]);
        if(used+n+4>=sizeof(output))return 0;
        if(i){output[used++]=',';output[used++]=' ';}
        output[used++]='"';memcpy(output+used,ids[i],n);used+=n;output[used++]='"';
    }
    output[used++]=']';if(used+length-end>sizeof(output))return 0;
    memcpy(output+used,data+end,length-end);used+=length-end;
    if(wcslen(path)+48>=MAX_PATH)return 0;
    swprintf(temp,MAX_PATH,L"%ls.ufc4-%lu-%llu.tmp",path,GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    swprintf(backup,MAX_PATH,L"%ls.ufc4-overlay.bak",path);
    HANDLE file=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    DWORD written=0;int ok=WriteFile(file,output,(DWORD)used,&written,NULL)&&written==used&&FlushFileBuffers(file);
    CloseHandle(file);
    if(ok&&!CopyFileW(path,backup,TRUE)&&GetLastError()!=ERROR_FILE_EXISTS)ok=0;
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok){DeleteFileW(temp);return 0;}
    *enabled=mask_for(ids,count);return 1;
}
