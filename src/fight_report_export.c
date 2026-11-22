/* Read-only post-fight report. Guest addresses are validated before reading. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "fight_report_export.h"

typedef struct { const char *name; uint64_t slot; } ReportField;
static const ReportField fields[REPORT_FIELD_COUNT]={
    {"Global_MatchState",0x90b444c10ULL},
    {"Global_MatchResultType",0x90b444be8ULL},
    {"Global_RoundNum",0x90b444f08ULL},
    {"Global_RoundLength",0x90b444ee0ULL},
    {"Online_MatchResult",0x90b46a820ULL},
    {"Online_MatchResultType",0x90b46a870ULL},
    {"Online_MatchTimeSecs",0x90b46a8c0ULL},
    {"Online_KnockdownsLanded",0x90b46a988ULL},
    {"Online_AdvanceStrikesLanded",0x90b46ac30ULL},
    {"Online_SignificantStrikesThrown",0x90b46a960ULL},
    {"Online_StrikesLanded",0x90b46a9b0ULL},
    {"Online_StrikesThrown",0x90b46a9d8ULL},
    {"Online_TakedownsAttempted",0x90b46aa78ULL},
    {"Online_TakedownsLanded",0x90b46aaa0ULL},
    {"Online_StunsLanded",0x90b46ac80ULL},
    {"Online_Control_Duration",0x90b46aca8ULL}
};
static const GUID documents_id={0xfdd39ad0,0x238f,0x46af,{0xad,0xb4,0x6c,0x85,0x48,0x03,0x69,0xc7}};
static int read_exact(uintptr_t address,void *out,size_t length){
    SIZE_T read=0;
    return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)address,out,length,&read)&&read==length;
}
static int report_read_count(FightReportRaw *raw,int count){
    uint64_t world=0;
    for(int i=0;i<count;++i){
        uintptr_t object=0,name_address=0,buffer_address=0;
        unsigned char header[0x68],buffer[40];
        char name[96];
        if(!read_exact((uintptr_t)fields[i].slot,&object,sizeof(object))||
           !read_exact(object,header,sizeof(header)))return 0;
        uint64_t vtable=0,actual_world=0;
        uint32_t element_size=0;
        memcpy(&vtable,header,8);
        memcpy(&name_address,header+8,8);
        memcpy(&actual_world,header+0x28,8);
        memcpy(&element_size,header+0x58,4);
        memcpy(&buffer_address,header+0x60,8);
        if(vtable!=0x909c227f0ULL||element_size!=4||!name_address||
           !read_exact(name_address,name,sizeof(name)))return 0;
        name[sizeof(name)-1]=0;
        if(strcmp(name,fields[i].name))return 0;
        if(world&&world!=actual_world)return 0;
        world=actual_world;
        if(!read_exact(buffer_address,buffer,sizeof(buffer)))return 0;
        uint16_t data_offset=0;memcpy(&data_offset,buffer+0x1c,2);
        if(data_offset!=32)return 0;
        memcpy(raw->value[i],buffer+0x20,8);
    }
    return world!=0;
}
static int report_read(FightReportRaw *raw){return report_read_count(raw,REPORT_FIELD_COUNT);}
int fight_report_match_state(void){
    FightReportRaw raw={0};
    if(!report_read_count(&raw,REPORT_STATE+1))return -1;
    return raw.value[REPORT_STATE][0]<=3?(int)raw.value[REPORT_STATE][0]:-1;
}
static float game_float(uint32_t bits){float value;memcpy(&value,&bits,4);return value;}
static int valid_stat(const FightReportRaw *r,int field,int fighter){
    return r->value[field][fighter]<=9999;
}
static int report_result_valid(const FightReportRaw *r,int *winner,int *round,int *seconds){
    if(r->value[REPORT_STATE][0]!=3||
       r->value[REPORT_RESULT_TYPE][0]!=r->value[REPORT_ONLINE_RESULT_TYPE][0])return 0;
    uint32_t blue=r->value[REPORT_ONLINE_RESULT][0],red=r->value[REPORT_ONLINE_RESULT][1];
    if(blue==2&&red==1)*winner=1;
    else if(blue==1&&red==2)*winner=0;
    else return 0; /* Draw codes require a separate in-game validation. */
    uint32_t round_index=r->value[REPORT_ROUND][0];
    float length=game_float(r->value[REPORT_ROUND_LENGTH][0]);
    if(round_index>=12||!isfinite(length)||length<30||length>1800)return 0;
    float remaining=(float)r->value[REPORT_MATCH_TIME][0]-(float)round_index*length;
    if(!isfinite(remaining)||remaining<0||remaining>length)return 0;
    *round=(int)round_index+1;*seconds=(int)(length-remaining+0.5f);
    return 1;
}
static int report_valid(const FightReportRaw *r,int *winner,int *round,int *seconds){
    if(!report_result_valid(r,winner,round,seconds))return 0;
    const int stats[]={REPORT_KNOCKDOWNS,REPORT_SIG_ATTEMPTED,REPORT_SIG_LANDED,
        REPORT_TOTAL_ATTEMPTED,REPORT_TOTAL_LANDED,REPORT_TD_ATTEMPTED,REPORT_TD_LANDED,REPORT_STUNS};
    for(int fighter=0;fighter<2;++fighter){
        for(size_t i=0;i<sizeof(stats)/sizeof(stats[0]);++i)if(!valid_stat(r,stats[i],fighter))return 0;
        if(r->value[REPORT_SIG_LANDED][fighter]>r->value[REPORT_SIG_ATTEMPTED][fighter]||
           r->value[REPORT_TOTAL_LANDED][fighter]>r->value[REPORT_TOTAL_ATTEMPTED][fighter]||
           r->value[REPORT_TD_LANDED][fighter]>r->value[REPORT_TD_ATTEMPTED][fighter])return 0;
        float control=game_float(r->value[REPORT_CONTROL][fighter]);
        if(!isfinite(control)||control<0||control>9999)return 0;
    }
    return 1;
}
static int format_stats(const FightReportRaw *r,int fighter,char *out,size_t count){
    float control=game_float(r->value[REPORT_CONTROL][fighter]);
    int length=snprintf(out,count,
        "{\"knockdowns\":%u,\"stuns\":%u,\"significantStrikesAttempted\":%u,"
        "\"significantStrikesLanded\":%u,\"totalStrikesAttempted\":%u,"
        "\"totalStrikesLanded\":%u,\"takedownsAttempted\":%u,\"takedownsLanded\":%u,"
        "\"controlTimeSeconds\":%d}",
        r->value[REPORT_KNOCKDOWNS][fighter],r->value[REPORT_STUNS][fighter],
        r->value[REPORT_SIG_ATTEMPTED][fighter],r->value[REPORT_SIG_LANDED][fighter],
        r->value[REPORT_TOTAL_ATTEMPTED][fighter],r->value[REPORT_TOTAL_LANDED][fighter],
        r->value[REPORT_TD_ATTEMPTED][fighter],r->value[REPORT_TD_LANDED][fighter],
        (int)(control+0.5f));
    return length>0&&(size_t)length<count;
}
int fight_report_format_result(const FightReportRaw *raw,const char *version,const char *engine,
                               const char *id,unsigned long long captured_at,
                               char *json,size_t capacity){
    int winner=0,round=0,seconds=0;
    if(!raw||!version||!engine||!id||!json||
       !report_result_valid(raw,&winner,&round,&seconds))return 0;
    uint32_t method=raw->value[REPORT_RESULT_TYPE][0];
    int length=snprintf(json,capacity,
        "{\"schema\":1,\"id\":\"%s\",\"reportStage\":\"result\",\"fightStatisticsComplete\":false,"
        "\"gameVersion\":\"%s\",\"engine\":\"%s\",\"capturedAt\":%llu,"
        "\"result\":{\"winnerCorner\":\"%s\",\"method\":%s,\"gameMethodCode\":%u,"
        "\"round\":%d,\"clockSeconds\":%d},\"fighters\":["
        "{\"corner\":\"red\",\"name\":null},{\"corner\":\"blue\",\"name\":null}]}\n",
        id,version,engine,captured_at,winner?"red":"blue",
        method==0?"\"Knockout\"":method==1?"\"TKO\"":method==2?"\"Submission\"":"null",
        method,round,seconds);
    return length>0&&(size_t)length<capacity;
}
int fight_report_format(const FightReportRaw *raw,const char *version,const char *engine,
                        const char *id,unsigned long long captured_at,
                        char *json,size_t capacity){
    int winner=0,round=0,seconds=0;char red[384],blue[384];
    if(!raw||!version||!engine||!id||!json||
       !report_valid(raw,&winner,&round,&seconds)||
       !format_stats(raw,1,red,sizeof(red))||!format_stats(raw,0,blue,sizeof(blue)))return 0;
    /* Names are left null until the game's fighter-name binding is verified. */
    int length=snprintf(json,capacity,
        "{\"schema\":1,\"id\":\"%s\",\"reportStage\":\"result\",\"fightStatisticsComplete\":false,"
        "\"gameVersion\":\"%s\",\"engine\":\"%s\","
        "\"capturedAt\":%llu,\"result\":{\"winnerCorner\":\"%s\",\"method\":%s,\"gameMethodCode\":%u,"
        "\"round\":%d,\"clockSeconds\":%d},\"fighters\":["
        "{\"corner\":\"red\",\"name\":null,\"stats\":%s},"
        "{\"corner\":\"blue\",\"name\":null,\"stats\":%s}]}\n",
        id,version,engine,captured_at,winner? "red":"blue",
        raw->value[REPORT_RESULT_TYPE][0]==0?"\"Knockout\"":
        raw->value[REPORT_RESULT_TYPE][0]==1?"\"TKO\"":
        raw->value[REPORT_RESULT_TYPE][0]==2?"\"Submission\"":"null",
        raw->value[REPORT_RESULT_TYPE][0],
        round,seconds,red,blue);
    return length>0&&(size_t)length<capacity;
}
static int make_directory(const wchar_t *path){
    return CreateDirectoryW(path,NULL)||GetLastError()==ERROR_ALREADY_EXISTS;
}
static int report_folder(const wchar_t *documents,wchar_t *folder,size_t count,int create){
    wchar_t parent[MAX_PATH],game[MAX_PATH];
    int ok=swprintf(parent,MAX_PATH,L"%ls\\UFC Parsectagon Overlay",documents)>0&&
           swprintf(game,MAX_PATH,L"%ls\\UFC6",parent)>0&&
           swprintf(folder,count,L"%ls\\Fight Reports",game)>0;
    return ok&&(!create||(make_directory(parent)&&make_directory(game)&&make_directory(folder)));
}
static void migrate_report_files(const wchar_t *source,const wchar_t *destination){
    wchar_t pattern[MAX_PATH],from[MAX_PATH],to[MAX_PATH];
    if(swprintf(pattern,MAX_PATH,L"%ls\\fight-*",source)<=0)return;
    WIN32_FIND_DATAW data;HANDLE find=FindFirstFileW(pattern,&data);
    if(find==INVALID_HANDLE_VALUE)return;
    do{
        if((data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||
           swprintf(from,MAX_PATH,L"%ls\\%ls",source,data.cFileName)<=0||
           swprintf(to,MAX_PATH,L"%ls\\%ls",destination,data.cFileName)<=0)continue;
        if(GetFileAttributesW(to)==INVALID_FILE_ATTRIBUTES)
            MoveFileExW(from,to,MOVEFILE_WRITE_THROUGH);
    }while(FindNextFileW(find,&data));
    FindClose(find);
}
static void migrate_old_report_folders(const wchar_t *documents,const wchar_t *destination){
    wchar_t old[MAX_PATH];
    if(swprintf(old,MAX_PATH,L"%ls\\UFC 6 Parsectagon Overlay\\Fight Reports",documents)>0)
        migrate_report_files(old,destination);
    if(swprintf(old,MAX_PATH,L"%ls\\Parsectagon\\Fight Reports",documents)>0)
        migrate_report_files(old,destination);
}
static void set_error(wchar_t *out,size_t count,const wchar_t *message){
    if(out&&count)lstrcpynW(out,message,(int)count);
}
int fight_report_export(const wchar_t *game_version,int new_engine,
                        wchar_t *path,size_t path_count,wchar_t *error,size_t error_count){
    FightReportRaw first={0},second={0};
    if(!report_read(&first)){set_error(error,error_count,L"Fight report unavailable for this game version.");return 0;}
    Sleep(120);
    if(!report_read(&second)||memcmp(&first,&second,sizeof(first))){
        set_error(error,error_count,L"Fight data changed during capture. Try again.");return 0;
    }
    char version[24]="unknown";
    if(game_version&&game_version[0]){
        int i=0;for(;i<22&&game_version[i];++i){
            wchar_t c=game_version[i];
            if(!((c>=L'0'&&c<=L'9')||c==L'.'))break;
            version[i]=(char)c;
        }
        if(i&&i<23)version[i]=0;
    }
    GUID guid;if(FAILED(CoCreateGuid(&guid))){set_error(error,error_count,L"Could not create a report ID.");return 0;}
    char id[40];snprintf(id,sizeof(id),"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned long)guid.Data1,guid.Data2,guid.Data3,
        guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],
        guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    FILETIME ft;GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER timestamp;
    timestamp.LowPart=ft.dwLowDateTime;timestamp.HighPart=ft.dwHighDateTime;
    unsigned long long milliseconds=(timestamp.QuadPart-116444736000000000ULL)/10000ULL;
    char json[2048];
    if(!fight_report_format(&second,version,new_engine?"ps5-pro":"original-parsec",
                            id,milliseconds,json,sizeof(json))){
        set_error(error,error_count,L"Open a finished fight's Statistics screen, then try again.");return 0;
    }
    PWSTR documents=NULL;
    if(FAILED(SHGetKnownFolderPath(&documents_id,KF_FLAG_CREATE,NULL,&documents))||!documents){
        set_error(error,error_count,L"Windows Documents folder is unavailable.");return 0;
    }
    wchar_t folder[MAX_PATH],target[MAX_PATH];
    int ok=report_folder(documents,folder,MAX_PATH,1);
    if(ok)migrate_old_report_folders(documents,folder);
    CoTaskMemFree(documents);
    if(!ok){
        set_error(error,error_count,L"Could not create Documents\\UFC Parsectagon Overlay\\UFC6\\Fight Reports.");return 0;
    }
    SYSTEMTIME now;GetLocalTime(&now);
    wchar_t short_id[9];
    for(int i=0;i<8;++i)short_id[i]=(wchar_t)(unsigned char)id[i];
    short_id[8]=0;
    if(swprintf(target,MAX_PATH,L"%ls\\fight-%04u%02u%02u-%02u%02u%02u-%ls.json",folder,
                now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,short_id)<=0){
        set_error(error,error_count,L"Could not name the fight report.");return 0;
    }
    HANDLE file=CreateFileW(target,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){set_error(error,error_count,L"Could not create the fight report file.");return 0;}
    DWORD written=0,size=(DWORD)strlen(json);
    ok=WriteFile(file,json,size,&written,NULL)&&written==size&&FlushFileBuffers(file);
    CloseHandle(file);
    if(!ok){DeleteFileW(target);set_error(error,error_count,L"Could not save the fight report.");return 0;}
    if(path&&path_count)lstrcpynW(path,target,(int)path_count);
    if(error&&error_count)error[0]=0;
    return 1;
}

/* Persist the smallest useful result first. Only seven verified bindings are
   needed, so this stage is independent of the later Fight Statistics table. */
int fight_report_export_result(const wchar_t *game_version,int new_engine,
                               wchar_t *path,size_t path_count,wchar_t *error,size_t error_count){
    FightReportRaw first={0},second={0};
    if(!report_read_count(&first,REPORT_MATCH_TIME+1)){
        set_error(error,error_count,L"Finished-fight result is not ready.");return 0;
    }
    Sleep(10);
    if(!report_read_count(&second,REPORT_MATCH_TIME+1)||memcmp(&first,&second,sizeof(first))){
        set_error(error,error_count,L"Fight result changed during capture.");return 0;
    }
    char version[24]="unknown";
    if(game_version&&game_version[0]){
        int i=0;for(;i<22&&game_version[i];++i){
            wchar_t c=game_version[i];
            if(!((c>=L'0'&&c<=L'9')||c==L'.'))break;
            version[i]=(char)c;
        }
        if(i&&i<23)version[i]=0;
    }
    GUID guid;if(FAILED(CoCreateGuid(&guid))){set_error(error,error_count,L"Could not create a result ID.");return 0;}
    char id[40];snprintf(id,sizeof(id),"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned long)guid.Data1,guid.Data2,guid.Data3,
        guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],
        guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    FILETIME ft;GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER timestamp;
    timestamp.LowPart=ft.dwLowDateTime;timestamp.HighPart=ft.dwHighDateTime;
    unsigned long long milliseconds=(timestamp.QuadPart-116444736000000000ULL)/10000ULL;
    char json[1024];
    if(!fight_report_format_result(&second,version,new_engine?"ps5-pro":"original-parsec",
                                   id,milliseconds,json,sizeof(json))){
        set_error(error,error_count,L"Finished-fight result is not ready.");return 0;
    }
    PWSTR documents=NULL;
    if(FAILED(SHGetKnownFolderPath(&documents_id,KF_FLAG_CREATE,NULL,&documents))||!documents){
        set_error(error,error_count,L"Windows Documents folder is unavailable.");return 0;
    }
    wchar_t folder[MAX_PATH],target[MAX_PATH];
    int ok=report_folder(documents,folder,MAX_PATH,1);
    if(ok)migrate_old_report_folders(documents,folder);
    CoTaskMemFree(documents);
    if(!ok){
        set_error(error,error_count,L"Could not create Documents\\UFC Parsectagon Overlay\\UFC6\\Fight Reports.");return 0;
    }
    wchar_t short_id[9];for(int i=0;i<8;++i)short_id[i]=(wchar_t)(unsigned char)id[i];short_id[8]=0;
    if(swprintf(target,MAX_PATH,L"%ls\\fight-result-%ls.json",folder,short_id)<=0){
        set_error(error,error_count,L"Could not name the result file.");return 0;
    }
    HANDLE file=CreateFileW(target,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){set_error(error,error_count,L"Could not create the result file.");return 0;}
    DWORD written=0,size=(DWORD)strlen(json);
    ok=WriteFile(file,json,size,&written,NULL)&&written==size&&FlushFileBuffers(file);
    CloseHandle(file);
    if(!ok){DeleteFileW(target);set_error(error,error_count,L"Could not save the result.");return 0;}
    if(path&&path_count)lstrcpynW(path,target,(int)path_count);
    if(error&&error_count)error[0]=0;
    return 1;
}

static int json_find_int(const char *src,const char *key,int *value){
    if(!src||!key||!value)return 0;
    const char *p=strstr(src,key);
    if(!p)return 0;
    p+=strlen(key);
    while(*p==' '||*p=='\t'||*p==':')++p;
    if(*p!='-'&&(*p<'0'||*p>'9'))return 0;
    *value=(int)strtol(p,NULL,10);
    return 1;
}
static int json_find_ull(const char *src,const char *key,unsigned long long *value){
    if(!src||!key||!value)return 0;
    const char *p=strstr(src,key);
    if(!p)return 0;
    p+=strlen(key);
    while(*p==' '||*p=='\t'||*p==':')++p;
    if(*p<'0'||*p>'9')return 0;
    *value=strtoull(p,NULL,10);
    return 1;
}
static int json_find_str(const char *src,const char *key,char *out,size_t count){
    if(!src||!key||!out||!count)return 0;
    out[0]=0;
    const char *p=strstr(src,key);
    if(!p)return 0;
    p+=strlen(key);
    while(*p==' '||*p=='\t'||*p==':')++p;
    if(*p!='"')return 0;
    ++p;
    size_t i=0;
    while(*p&&*p!='"'&&i+1<count){
        if(*p=='\\'&&p[1]){++p;}
        out[i++]=*p++;
    }
    out[i]=0;
    return i>0;
}
static void parse_fighter_stats(const char *stats,int *kd,int *stuns,int *sig_a,int *sig_l,int *tot_a,int *tot_l,int *td_a,int *td_l,int *ctrl){
    json_find_int(stats,"\"knockdowns\"",kd);
    json_find_int(stats,"\"stuns\"",stuns);
    json_find_int(stats,"\"significantStrikesAttempted\"",sig_a);
    json_find_int(stats,"\"significantStrikesLanded\"",sig_l);
    json_find_int(stats,"\"totalStrikesAttempted\"",tot_a);
    json_find_int(stats,"\"totalStrikesLanded\"",tot_l);
    json_find_int(stats,"\"takedownsAttempted\"",td_a);
    json_find_int(stats,"\"takedownsLanded\"",td_l);
    json_find_int(stats,"\"controlTimeSeconds\"",ctrl);
}
static int read_small_utf8(const wchar_t *path,char *buf,size_t cap){
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return 0;
    DWORD got=0;BOOL ok=ReadFile(f,buf,(DWORD)(cap-1),&got,NULL);
    CloseHandle(f);
    if(!ok)return 0;
    buf[got]=0;
    return 1;
}
static void format_capture_time(unsigned long long ms,const FILETIME *fallback,wchar_t *out,size_t count){
    FILETIME ft={0},local_ft={0};SYSTEMTIME st={0};
    if(ms>0){
        ULARGE_INTEGER ui;
        ui.QuadPart=ms*10000ULL+116444736000000000ULL;
        ft.dwLowDateTime=ui.LowPart;ft.dwHighDateTime=ui.HighPart;
    }else if(fallback){
        ft=*fallback;
    }
    if((ft.dwLowDateTime||ft.dwHighDateTime)&&FileTimeToLocalFileTime(&ft,&local_ft)&&FileTimeToSystemTime(&local_ft,&st)){
        swprintf(out,count,L"%04u-%02u-%02u %02u:%02u:%02u",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    }else lstrcpynW(out,L"Unknown",(int)count);
}
int fight_report_read_from_folder(const wchar_t *folder,FightReportSummary *out){
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    lstrcpyW(out->winner,L"No report captured");
    lstrcpyW(out->method,L"—");
    lstrcpyW(out->capture_time,L"—");
    if(!folder||!folder[0])return 0;

    wchar_t pattern[MAX_PATH];
    if(swprintf(pattern,MAX_PATH,L"%ls\\*.json",folder)<=0)return 0;
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW(pattern,&fd);
    if(h==INVALID_HANDLE_VALUE)return 0;

    wchar_t best_name[MAX_PATH]=L"";
    FILETIME best_time={0};
    do{
        if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        size_t len=wcslen(fd.cFileName);
        if(len>12&&_wcsicmp(fd.cFileName+len-12,L".upload.json")==0)continue;
        if(!best_name[0]||CompareFileTime(&fd.ftLastWriteTime,&best_time)>0){
            lstrcpynW(best_name,fd.cFileName,MAX_PATH);
            best_time=fd.ftLastWriteTime;
        }
    }while(FindNextFileW(h,&fd));
    FindClose(h);
    if(!best_name[0])return 0;

    if(swprintf(out->file_path,260,L"%ls\\%ls",folder,best_name)<=0)return 0;
    char json[8192];
    if(!read_small_utf8(out->file_path,json,sizeof(json)))return 0;
    out->available=1;

    char corner[32]="",method[48]="";
    json_find_str(json,"\"winnerCorner\"",corner,sizeof(corner));
    if(_stricmp(corner,"red")==0)lstrcpyW(out->winner,L"Red corner");
    else if(_stricmp(corner,"blue")==0)lstrcpyW(out->winner,L"Blue corner");
    else lstrcpyW(out->winner,L"Unknown");

    if(json_find_str(json,"\"method\"",method,sizeof(method)))MultiByteToWideChar(CP_UTF8,0,method,-1,out->method,48);
    else lstrcpyW(out->method,L"Unknown");

    json_find_int(json,"\"round\"",&out->round);
    json_find_int(json,"\"clockSeconds\"",&out->clock_seconds);

    unsigned long long captured_ms=0;
    json_find_ull(json,"\"capturedAt\"",&captured_ms);
    format_capture_time(captured_ms,&best_time,out->capture_time,48);

    const char *red_p=strstr(json,"\"corner\":\"red\"");
    const char *blue_p=strstr(json,"\"corner\":\"blue\"");
    const char *red_stats=red_p?strstr(red_p,"\"stats\""):NULL;
    const char *blue_stats=blue_p?strstr(blue_p,"\"stats\""):NULL;
    if(red_stats&&blue_stats&&(!blue_p||red_stats<blue_p)){
        out->has_stats=1;
        parse_fighter_stats(red_stats,&out->red_kd,&out->red_stuns,&out->red_sig_a,&out->red_sig_l,&out->red_tot_a,&out->red_tot_l,&out->red_td_a,&out->red_td_l,&out->red_ctrl);
        parse_fighter_stats(blue_stats,&out->blue_kd,&out->blue_stuns,&out->blue_sig_a,&out->blue_sig_l,&out->blue_tot_a,&out->blue_tot_l,&out->blue_td_a,&out->blue_td_l,&out->blue_ctrl);
    }

    wchar_t marker[MAX_PATH],err_path[MAX_PATH];
    swprintf(marker,MAX_PATH,L"%ls.upload.json",out->file_path);
    swprintf(err_path,MAX_PATH,L"%ls.upload-error.txt",out->file_path);
    char marker_buf[1024],err_buf[512];
    if(read_small_utf8(marker,marker_buf,sizeof(marker_buf))){
        char status_str[32]="";
        json_find_str(marker_buf,"\"status\"",status_str,sizeof(status_str));
        if(_stricmp(status_str,"needs_review")==0||strstr(marker_buf,"\"needsReview\":true")||strstr(marker_buf,"\"needsReview\": true")){
            out->upload_state=REPORT_UPLOAD_NEEDS_REVIEW;
            lstrcpyW(out->upload_error,L"Flagged for review by server.");
        }else if(_stricmp(status_str,"uploaded")==0){
            out->upload_state=REPORT_UPLOAD_SUBMITTED;
        }else{
            out->upload_state=REPORT_UPLOAD_WAITING;
        }
    }else if(read_small_utf8(err_path,err_buf,sizeof(err_buf))){
        out->upload_state=REPORT_UPLOAD_NEEDS_REVIEW;
        size_t elen=strlen(err_buf);
        while(elen>0&&(err_buf[elen-1]=='\r'||err_buf[elen-1]=='\n'))err_buf[--elen]=0;
        MultiByteToWideChar(CP_UTF8,0,err_buf,-1,out->upload_error,160);
    }else{
        out->upload_state=REPORT_UPLOAD_SAVED_LOCAL;
    }
    return 1;
}
int fight_report_read_latest(FightReportSummary *out){
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    PWSTR documents=NULL;
    if(FAILED(SHGetKnownFolderPath(&documents_id,0,NULL,&documents))||!documents)return 0;
    wchar_t folder[MAX_PATH];
    int ok=report_folder(documents,folder,MAX_PATH,1);
    if(ok)migrate_old_report_folders(documents,folder);
    if(ok&&fight_report_read_from_folder(folder,out)){CoTaskMemFree(documents);return 1;}
    /* Older captures remain readable after the output folder is renamed. */
    ok=swprintf(folder,MAX_PATH,L"%ls\\UFC 6 Parsectagon Overlay\\Fight Reports",documents)>0;
    if(ok&&fight_report_read_from_folder(folder,out)){CoTaskMemFree(documents);return 1;}
    ok=swprintf(folder,MAX_PATH,L"%ls\\Parsectagon\\Fight Reports",documents)>0;
    CoTaskMemFree(documents);
    return ok?fight_report_read_from_folder(folder,out):0;
}
