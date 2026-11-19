/* Restart handoff: wait for a normal game exit, then use PORTHEON's UFC 4 launcher. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int sync_button_art(const wchar_t *bin,const wchar_t *style){
    wchar_t helper[MAX_PATH],command[MAX_PATH*2];
    wsprintfW(helper,L"%ls\\ufc4_keycap_sync.exe",bin);
    wsprintfW(command,L"\"%ls\" \"%ls\" \"%ls\"",helper,bin,style);
    STARTUPINFOW start={.cb=sizeof(start)};PROCESS_INFORMATION process={0};
    if(!CreateProcessW(helper,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,bin,&start,&process))return 0;
    CloseHandle(process.hThread);
    DWORD waited=WaitForSingleObject(process.hProcess,120000),code=1;
    int ok=waited==WAIT_OBJECT_0&&GetExitCodeProcess(process.hProcess,&code)&&code==0;
    CloseHandle(process.hProcess);
    return ok;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR arguments,int show){
    (void)instance;(void)previous;(void)arguments;(void)show;
    int argc=0;LPWSTR *args=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!args||(argc!=3&&argc!=4)){if(args)LocalFree(args);return 2;}
    wchar_t icon_style[16]={0};
    if(argc==4){
        if(lstrcmpiW(args[3],L"ps")&&lstrcmpiW(args[3],L"xbox")&&lstrcmpiW(args[3],L"keyboard")){LocalFree(args);return 2;}
        lstrcpynW(icon_style,args[3],16);
    }
    wchar_t *end=NULL;unsigned long pid=wcstoul(args[1],&end,10);
    if(!pid||*end){LocalFree(args);return 2;}
    wchar_t path[MAX_PATH],game[MAX_PATH],launcher[MAX_PATH],directory[MAX_PATH];
    if(!GetModuleFileNameW(NULL,path,MAX_PATH)){LocalFree(args);return 2;}
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash){LocalFree(args);return 2;}
    *slash=0;
    wsprintfW(game,L"%ls\\sacredfire-game.exe",path);
    lstrcpynW(directory,path,MAX_PATH);
    slash=wcsrchr(directory,L'\\');if(!slash){LocalFree(args);return 2;}
    *slash=0;
    wsprintfW(launcher,L"%ls\\UFC4.exe",directory);
    HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,(DWORD)pid);
    if(!process){LocalFree(args);return 3;}
    wchar_t actual[MAX_PATH];DWORD size=MAX_PATH;
    if(!QueryFullProcessImageNameW(process,0,actual,&size)||lstrcmpiW(actual,game)){
        CloseHandle(process);LocalFree(args);return 3;
    }
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,args[2]);
    if(!ready){CloseHandle(process);LocalFree(args);return 3;}
    SetEvent(ready);CloseHandle(ready);LocalFree(args);
    DWORD wait=WaitForSingleObject(process,60000);CloseHandle(process);
    if(wait!=WAIT_OBJECT_0)return 4;
    if(*icon_style&&!sync_button_art(path,icon_style)){
        MessageBoxW(NULL,L"Could not apply the UFC 4 game prompts. The prepared artwork or game archive did not match.",L"UFC 4 Actions Overlay",MB_OK|MB_ICONERROR);
        *icon_style=0;
    }
    if(*icon_style){
        wchar_t ini[MAX_PATH];wsprintfW(ini,L"%ls\\ufc4_actions.ini",path);
        WritePrivateProfileStringW(L"Appearance",L"GameButtonStyle",!lstrcmpiW(icon_style,L"keyboard")?L"2":!lstrcmpiW(icon_style,L"ps")?L"1":L"0",ini);
    }
    wchar_t command[MAX_PATH+20];wsprintfW(command,L"\"%ls\" --play",launcher);
    STARTUPINFOW start={.cb=sizeof(start)};PROCESS_INFORMATION launched={0};
    if(!CreateProcessW(launcher,command,NULL,NULL,FALSE,0,NULL,directory,&start,&launched))return 5;
    CloseHandle(launched.hThread);CloseHandle(launched.hProcess);
    return 0;
}
