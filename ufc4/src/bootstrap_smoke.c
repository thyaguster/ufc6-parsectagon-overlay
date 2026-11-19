#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
int main(void){
    HMODULE bridge=LoadLibraryW(L"winmm.dll");
    if(!bridge){puts("FAIL: bridge did not load");return 1;}
    wchar_t layer[512]={0};GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",layer,512);
    if(!wcsstr(layer,L"VK_LAYER_UFC4_actions")){puts("FAIL: Vulkan layer not activated");return 1;}
    typedef unsigned int (WINAPI *TimeBeginPeriod)(unsigned int);
    TimeBeginPeriod call=(TimeBeginPeriod)GetProcAddress(bridge,"timeBeginPeriod");
    TimeBeginPeriod end=(TimeBeginPeriod)GetProcAddress(bridge,"timeEndPeriod");
    if(!call||!end||call(1)!=0||end(1)!=0){puts("FAIL: WINMM forwarding");return 1;}
    puts("PASS: per-game Vulkan bootstrap and WINMM forwarding");return 0;
}
