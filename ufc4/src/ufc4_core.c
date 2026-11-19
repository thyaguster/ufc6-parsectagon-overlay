/* UFC 4 Vulkan overlay prototype. No game memory addresses are used. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <shellapi.h>
#include <objbase.h>
#include <Xinput.h>
#include "screenshot_folder.h"
#include "ufc4_cheats_config.h"

static volatile LONG open;
static int page, selected;
static unsigned short previous_buttons;
static int previous_f10, previous_f12, previous_up, previous_down, previous_enter, previous_escape;
static DWORD last_repeat;
static uint32_t panel_width=660, panel_height=440;
static volatile LONG screenshot_pending, screenshot_supported;
static wchar_t notice[180];
static DWORD notice_until;
static int button_style;
static int game_button_style;
static int icon_choice;
static int skip_movies;
static int preferences_loaded;
static unsigned cheats_mask;
static int cheats_ready;
enum {PAGE_MAIN,PAGE_SETTINGS,PAGE_ABOUT,PAGE_QUIT,PAGE_RESTART,PAGE_ICON_CONFIRM,PAGE_GAME_ICONS,PAGE_CHEATS};
static const wchar_t *main_items[]={L"Resume game",L"Take screenshot",L"Open screenshots folder",L"Restart game",L"Settings",L"Cheats",L"Quit game"};
static const wchar_t *settings_items[]={L"Overlay button icons",L"Game button icons",L"Skip intros",L"Back"};
static const wchar_t *game_icon_items[]={L"Xbox",L"PlayStation",L"Keyboard & mouse",L"Back"};
static const wchar_t *cheat_items[]={L"Infinite stamina: left fighter",L"Infinite stamina: right fighter",L"Back"};
static const wchar_t *choice_items[]={L"Cancel",L"Confirm"};
typedef DWORD (WINAPI *XInputStateFn)(DWORD,XINPUT_STATE*);
static XInputStateFn xinput_state;
static int movie_setting(int change);
static void message(const wchar_t *value){lstrcpynW(notice,value,180);notice_until=GetTickCount()+4000;}
static void settings_path(wchar_t *path,DWORD length){
    HMODULE module=NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)(uintptr_t)&settings_path,&module);
    if(!module||!GetModuleFileNameW(module,path,length)){*path=0;return;}
    wchar_t *slash=wcsrchr(path,L'\\');if(slash)lstrcpyW(slash+1,L"ufc4_actions.ini");else *path=0;
}
static void load_preferences(void){
    if(preferences_loaded)return;
    preferences_loaded=1;
    wchar_t path[MAX_PATH];settings_path(path,MAX_PATH);
    if(*path){
        button_style=GetPrivateProfileIntW(L"Appearance",L"ButtonStyle",0,path)?1:0;
        game_button_style=GetPrivateProfileIntW(L"Appearance",L"GameButtonStyle",1,path);
        if(game_button_style<0||game_button_style>2)game_button_style=1;
        skip_movies=GetPrivateProfileIntW(L"Menu",L"SkipStartupSplashes",movie_setting(-1)==1,path)!=0;
    }
}
static void save_preferences(void){
    wchar_t path[MAX_PATH];settings_path(path,MAX_PATH);
    if(*path){
        WritePrivateProfileStringW(L"Appearance",L"ButtonStyle",button_style?L"1":L"0",path);
        WritePrivateProfileStringW(L"Appearance",L"GameButtonStyle",game_button_style==2?L"2":game_button_style?L"1":L"0",path);
        WritePrivateProfileStringW(L"Menu",L"SkipStartupSplashes",skip_movies?L"1":L"0",path);
    }
}
/* sacredfire.ini is a plain key=value file, not a Windows sectioned INI. */
static int movie_setting(int change){
    wchar_t path[MAX_PATH];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if(!n||n>=MAX_PATH-34)return -1;
    lstrcatW(path,L"\\UFC4Native\\sacredfire.ini");
    HANDLE file=CreateFileW(path,change>=0?GENERIC_READ|GENERIC_WRITE:GENERIC_READ,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return -1;
    char data[16384];DWORD size=0;int value=-1;DWORD at=0;int found=0;
    if(ReadFile(file,data,sizeof(data),&size,NULL)&&size<sizeof(data)){
        const char key[]="skip_movies=";
        for(DWORD i=0;i+sizeof(key)<=size;i++){
            if((i==0||data[i-1]=='\n')&&!memcmp(data+i,key,sizeof(key)-1)&&
               (data[i+sizeof(key)-1]=='0'||data[i+sizeof(key)-1]=='1')&&
               (i+sizeof(key)==size||data[i+sizeof(key)]=='\r'||data[i+sizeof(key)]=='\n')){
                at=i+sizeof(key)-1;value=data[at]-'0';found++;
            }
        }
        if(found!=1)value=-1;
        if(value>=0&&change>=0&&value!=change){
            DWORD done=0;char digit=(char)('0'+change);
            if(SetFilePointer(file,(LONG)at,NULL,FILE_BEGIN)==INVALID_SET_FILE_POINTER||
               !WriteFile(file,&digit,1,&done,NULL)||done!=1||!FlushFileBuffers(file))value=-1;
            else value=change;
        }
    }
    CloseHandle(file);return value;
}
#ifdef UFC4_TEST
__declspec(dllexport) int Ufc4TestMovieSetting(int change){return movie_setting(change);}
__declspec(dllexport) int Ufc4TestSaveSkipMovies(int enabled){
    preferences_loaded=1;skip_movies=enabled;save_preferences();
    wchar_t path[MAX_PATH];settings_path(path,MAX_PATH);
    return GetPrivateProfileIntW(L"Menu",L"SkipStartupSplashes",-1,path);
}
__declspec(dllexport) void Ufc4TestCheatPath(const wchar_t *path){ufc4_cheats_test_path(path);}
__declspec(dllexport) int Ufc4TestCheatRead(void){unsigned mask=0;return ufc4_cheats_read(&mask)?(int)mask:-1;}
__declspec(dllexport) int Ufc4TestCheatToggle(int fighter){unsigned mask=0;return ufc4_cheats_toggle(fighter,&mask)?(int)mask:-1;}
#endif
static BOOL CALLBACK find_window(HWND window,LPARAM result){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid==GetCurrentProcessId()&&GetWindow(window,GW_OWNER)==NULL&&IsWindowVisible(window)){
        *(HWND*)result=window;return FALSE;
    }
    return TRUE;
}
static HWND game_window(void){HWND found=NULL;EnumWindows(find_window,(LPARAM)&found);return found;}
static void request_screenshot(void){
    if(!InterlockedCompareExchange(&screenshot_supported,0,0)){message(L"Screenshot capture is unavailable in this session.");return;}
    if(InterlockedCompareExchange(&screenshot_pending,1,0)==0)message(L"Saving screenshot...");
}
__declspec(dllexport) int Ufc4ScreenshotRequest(void){return InterlockedCompareExchange(&screenshot_pending,0,0)?1:0;}
__declspec(dllexport) void Ufc4ScreenshotCapability(BOOL capable){InterlockedExchange(&screenshot_supported,capable?1:0);}
__declspec(dllexport) void Ufc4ScreenshotResult(BOOL saved){
    InterlockedExchange(&screenshot_pending,0);
    message(saved?L"Screenshot saved to Pictures\\Screenshots.":L"Screenshot could not be saved.");
}
__declspec(dllexport) void Ufc4ScreenshotResultPath(BOOL saved,const wchar_t *path){
    (void)path;Ufc4ScreenshotResult(saved);
}
static void open_screenshots(void){
    PWSTR folder=menu_screenshot_directory();
    if(!folder){message(L"Pictures\\Screenshots is unavailable.");return;}
    HINSTANCE result=ShellExecuteW(NULL,L"open",folder,NULL,NULL,SW_SHOWNORMAL);
    CoTaskMemFree(folder);
    if((INT_PTR)result<=32)message(L"Could not open the screenshots folder.");
}
static void restart_game(int icon_style){
    HWND window=game_window();
    wchar_t helper[MAX_PATH],event_name[110],command[500];
    settings_path(helper,MAX_PATH);wchar_t *slash=wcsrchr(helper,L'\\');
    if(!window||!slash){message(L"Could not prepare game restart.");return;}
    lstrcpyW(slash+1,L"ufc4_actions_restart.exe");
    wsprintfW(event_name,L"Local\\UFC4ActionsRestart_%lu_%lu",GetCurrentProcessId(),GetTickCount());
    HANDLE ready=CreateEventW(NULL,TRUE,FALSE,event_name);
    if(!ready){message(L"Could not prepare game restart.");return;}
    if(icon_style<0)wsprintfW(command,L"\"%ls\" %lu \"%ls\"",helper,GetCurrentProcessId(),event_name);
    else wsprintfW(command,L"\"%ls\" %lu \"%ls\" %ls",helper,GetCurrentProcessId(),event_name,icon_style==2?L"keyboard":icon_style?L"ps":L"xbox");
    STARTUPINFOW start={.cb=sizeof(start)};PROCESS_INFORMATION process={0};
    BOOL launched=CreateProcessW(helper,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&start,&process);
    if(launched){CloseHandle(process.hThread);launched=WaitForSingleObject(ready,5000)==WAIT_OBJECT_0;CloseHandle(process.hProcess);}
    CloseHandle(ready);
    if(launched)PostMessageW(window,WM_CLOSE,0,0);
    else message(L"Could not start the restart helper.");
}

/* UFC 4 imports SDL3 input directly. Intercept only its main executable's
   import slots so the menu can own input while it is visible. Window and
   quit events continue to reach the game. */
typedef int (*SdlPollEventFn)(void *);
typedef int (*SdlGamepadButtonFn)(void *,int);
typedef short (*SdlGamepadAxisFn)(void *,int);
typedef const unsigned char *(*SdlKeyboardStateFn)(int *);
static SdlPollEventFn real_poll_event;
static SdlGamepadButtonFn real_gamepad_button;
static SdlGamepadAxisFn real_gamepad_axis;
static SdlKeyboardStateFn real_keyboard_state;
static unsigned char empty_keys[2048];
static int hooks_installed;

static int menu_open(void){return InterlockedCompareExchange(&open,0,0)!=0;}
static int blocked_sdl_event(uint32_t type){
    return (type>=0x300&&type<=0x307) || /* keyboard and text */
           (type>=0x600&&type<=0x604) || /* joystick movement/buttons */
           (type>=0x650&&type<=0x652);   /* gamepad movement/buttons */
}
static int sdl_poll_event(void *event){
    if(!real_poll_event)return 0;
    if(!menu_open()||!event)return real_poll_event(event);
    for(int i=0;i<256;++i){
        if(!real_poll_event(event))return 0;
        if(!blocked_sdl_event(*(uint32_t*)event))return 1;
    }
    return 0;
}
static int sdl_gamepad_button(void *pad,int button){
    return menu_open()?0:(real_gamepad_button?real_gamepad_button(pad,button):0);
}
static short sdl_gamepad_axis(void *pad,int axis){
    return menu_open()?0:(real_gamepad_axis?real_gamepad_axis(pad,axis):0);
}
static const unsigned char *sdl_keyboard_state(int *count){
    const unsigned char *keys=real_keyboard_state?real_keyboard_state(count):NULL;
    if(!menu_open())return keys;
    if(count&&*count>(int)sizeof(empty_keys))*count=(int)sizeof(empty_keys);
    return empty_keys;
}
static void install_input_hooks(void){
    if(hooks_installed)return;
    HMODULE exe=GetModuleHandleW(NULL);
    if(!exe)return;
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)exe;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64*)((unsigned char*)exe+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE)return;
    IMAGE_DATA_DIRECTORY dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!dir.VirtualAddress)return;
    IMAGE_IMPORT_DESCRIPTOR *imports=(IMAGE_IMPORT_DESCRIPTOR*)((unsigned char*)exe+dir.VirtualAddress);
    int found=0;
    for(;imports->Name;++imports){
        const char *dll=(const char*)((unsigned char*)exe+imports->Name);
        if(lstrcmpiA(dll,"SDL3.dll"))continue;
        IMAGE_THUNK_DATA64 *names=(IMAGE_THUNK_DATA64*)((unsigned char*)exe+(imports->OriginalFirstThunk?imports->OriginalFirstThunk:imports->FirstThunk));
        IMAGE_THUNK_DATA64 *slots=(IMAGE_THUNK_DATA64*)((unsigned char*)exe+imports->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots){
            if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
            IMAGE_IMPORT_BY_NAME *entry=(IMAGE_IMPORT_BY_NAME*)((unsigned char*)exe+names->u1.AddressOfData);
            void *replacement=NULL;
            if(!lstrcmpA((char*)entry->Name,"SDL_PollEvent")){real_poll_event=(SdlPollEventFn)(uintptr_t)slots->u1.Function;replacement=sdl_poll_event;found|=1;}
            else if(!lstrcmpA((char*)entry->Name,"SDL_GetGamepadButton")){real_gamepad_button=(SdlGamepadButtonFn)(uintptr_t)slots->u1.Function;replacement=sdl_gamepad_button;found|=2;}
            else if(!lstrcmpA((char*)entry->Name,"SDL_GetGamepadAxis")){real_gamepad_axis=(SdlGamepadAxisFn)(uintptr_t)slots->u1.Function;replacement=sdl_gamepad_axis;found|=4;}
            else if(!lstrcmpA((char*)entry->Name,"SDL_GetKeyboardState")){real_keyboard_state=(SdlKeyboardStateFn)(uintptr_t)slots->u1.Function;replacement=sdl_keyboard_state;found|=8;}
            if(replacement){
                DWORD old;
                if(VirtualProtect(&slots->u1.Function,sizeof(slots->u1.Function),PAGE_READWRITE,&old)){
                    slots->u1.Function=(ULONGLONG)(uintptr_t)replacement;
                    DWORD ignored;VirtualProtect(&slots->u1.Function,sizeof(slots->u1.Function),old,&ignored);
                }
            }
        }
    }
    hooks_installed=(found==15);
}
#ifdef UFC4_TEST
__declspec(dllexport) void Ufc4TestOpen(void){open=1;page=0;selected=0;}
__declspec(dllexport) void Ufc4TestCheatPage(unsigned mask){open=1;page=PAGE_CHEATS;selected=0;cheats_mask=mask;cheats_ready=1;}
#endif

static int edge(int down,int *previous){int pressed=down&&!*previous;*previous=down;return pressed;}
static int row_count(void){return page==PAGE_MAIN?7:page==PAGE_GAME_ICONS?4:page==PAGE_SETTINGS?4:page==PAGE_CHEATS?3:page==PAGE_QUIT||page==PAGE_RESTART||page==PAGE_ICON_CONFIRM?2:1;}
static void poll_input(void){
    load_preferences();
    if(!xinput_state){HMODULE x=GetModuleHandleW(L"XInput1_4.dll");if(!x)x=LoadLibraryW(L"XInput1_4.dll");if(x)xinput_state=(XInputStateFn)GetProcAddress(x,"XInputGetState");}
    XINPUT_STATE state={0};unsigned short buttons=0;
    if(xinput_state&&xinput_state(0,&state)==ERROR_SUCCESS)buttons=state.Gamepad.wButtons;
    int f10=edge((GetAsyncKeyState(VK_F10)&0x8000)!=0,&previous_f10);
    int f12=edge((GetAsyncKeyState(VK_F12)&0x8000)!=0,&previous_f12);
    if(f12)request_screenshot();
    int chord=(buttons&(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_START))==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_START);
    int old_chord=(previous_buttons&(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_START))==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_START);
    if(f10||(chord&&!old_chord)){InterlockedExchange(&open,!menu_open());page=PAGE_MAIN;selected=0;}
    int up=edge((GetAsyncKeyState(VK_UP)&0x8000)!=0,&previous_up)||((buttons&XINPUT_GAMEPAD_DPAD_UP)&&!(previous_buttons&XINPUT_GAMEPAD_DPAD_UP));
    int down=edge((GetAsyncKeyState(VK_DOWN)&0x8000)!=0,&previous_down)||((buttons&XINPUT_GAMEPAD_DPAD_DOWN)&&!(previous_buttons&XINPUT_GAMEPAD_DPAD_DOWN));
    int accept=edge((GetAsyncKeyState(VK_RETURN)&0x8000)!=0,&previous_enter)||((buttons&XINPUT_GAMEPAD_A)&&!(previous_buttons&XINPUT_GAMEPAD_A));
    int back=edge((GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0,&previous_escape)||((buttons&XINPUT_GAMEPAD_B)&&!(previous_buttons&XINPUT_GAMEPAD_B));
    previous_buttons=buttons;
    if(!menu_open())return;
    if(back){if(page==PAGE_ICON_CONFIRM){page=PAGE_GAME_ICONS;selected=icon_choice;}
        else if(page==PAGE_GAME_ICONS){page=PAGE_SETTINGS;selected=1;}
        else if(page!=PAGE_MAIN){page=PAGE_MAIN;selected=0;}
        else{InterlockedExchange(&open,0);selected=0;}return;}
    DWORD now=GetTickCount();
    if((up||down)&&now-last_repeat>105){selected=(selected+(down?1:row_count()-1))%row_count();last_repeat=now;}
    if(!accept)return;
    if(page==PAGE_MAIN){
        switch(selected){
        case 0:InterlockedExchange(&open,0);break;
        case 1:request_screenshot();break;
        case 2:open_screenshots();break;
        case 3:page=PAGE_RESTART;selected=0;break;
        case 4:page=PAGE_SETTINGS;selected=0;break;
        case 5:page=PAGE_CHEATS;selected=0;cheats_ready=ufc4_cheats_read(&cheats_mask);break;
        case 6:page=PAGE_QUIT;selected=0;break;
        }
    }else if(page==PAGE_SETTINGS){
        if(selected==0){button_style=!button_style;save_preferences();}
        else if(selected==1){page=PAGE_GAME_ICONS;selected=game_button_style;}
        else if(selected==2){
            skip_movies=!skip_movies;
            if(movie_setting(skip_movies)<0)message(L"Overlay setting saved; SacredFire config could not be updated.");
            else message(L"Intro video setting saved for the next launch.");
            save_preferences();
        }
        else{page=PAGE_MAIN;selected=0;}
    }else if(page==PAGE_CHEATS){
        if(selected==2){page=PAGE_MAIN;selected=5;}
        else if(!cheats_ready)message(L"PORTHEON stamina cheats or settings are unavailable.");
        else if(ufc4_cheats_toggle(selected,&cheats_mask))message(L"Stamina choice saved for the next launch.");
        else message(L"Could not save the stamina cheat choice.");
    }else if(page==PAGE_GAME_ICONS){
        if(selected==3){page=PAGE_SETTINGS;selected=1;}
        else if(selected!=game_button_style){icon_choice=selected;page=PAGE_ICON_CONFIRM;selected=0;}
    }else if(page==PAGE_ICON_CONFIRM){
        int confirmed=selected==1;
        page=PAGE_GAME_ICONS;selected=icon_choice;
        if(confirmed)restart_game(icon_choice);
    }else if(page==PAGE_QUIT||page==PAGE_RESTART){
        int action=page;
        int confirmed=selected==1;
        page=PAGE_MAIN;selected=0;
        if(action==PAGE_RESTART&&confirmed)restart_game(-1);
        else if(action==PAGE_QUIT&&confirmed){HWND window=game_window();if(window)PostMessageW(window,WM_CLOSE,0,0);else message(L"Could not find the UFC 4 window.");}
    }else{page=PAGE_MAIN;selected=0;}
}

__declspec(dllexport) void Ufc4MenuExtent(uint32_t screen_width,uint32_t screen_height,uint32_t *width,uint32_t *height){
    panel_width=screen_width<760?screen_width-24:700;
    panel_height=screen_height<560?screen_height-24:500;
    *width=panel_width;*height=panel_height;
}
__declspec(dllexport) BOOL Ufc4RenderMenu(void *destination,uint32_t bytes){
    install_input_hooks();
    poll_input();
    if(!menu_open())return FALSE;
    if(!destination)return TRUE;
    if(bytes<panel_width*panel_height*4)return FALSE;
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=(LONG)panel_width;
    info.bmiHeader.biHeight=-(LONG)panel_height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);void *pixels=NULL;
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,NULL,0);ReleaseDC(NULL,screen);
    if(!dc||!bitmap||!pixels){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return FALSE;}
    HGDIOBJ old=SelectObject(dc,bitmap);
    RECT full={0,0,(LONG)panel_width,(LONG)panel_height};HBRUSH bg=CreateSolidBrush(RGB(12,17,27));FillRect(dc,&full,bg);DeleteObject(bg);
    RECT stripe={0,0,8,(LONG)panel_height};HBRUSH accent=CreateSolidBrush(RGB(231,55,68));FillRect(dc,&stripe,accent);DeleteObject(accent);
    HFONT title=CreateFontW(-31,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HFONT body=CreateFontW(-20,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(242,245,248));SelectObject(dc,title);
    const wchar_t *heading=page==PAGE_MAIN?L"UFC 4 Actions Overlay":page==PAGE_SETTINGS?L"Settings":page==PAGE_CHEATS?L"Cheats":page==PAGE_GAME_ICONS?L"Game button icons":page==PAGE_QUIT?L"Quit UFC 4?":page==PAGE_RESTART?L"Restart UFC 4?":page==PAGE_ICON_CONFIRM?L"Change game button icons?":L"About";
    TextOutW(dc,34,24,heading,(int)wcslen(heading));SelectObject(dc,body);SetTextColor(dc,RGB(175,190,207));
    const wchar_t *sub=page==PAGE_MAIN?L"Quick actions while UFC 4 is running":page==PAGE_SETTINGS?L"Prompts and startup videos":page==PAGE_CHEATS?L"Stamina applies next launch. Other cheats are under investigation.":page==PAGE_GAME_ICONS?L"Choose the prompts shown by UFC 4.":page==PAGE_QUIT?L"Unsaved game progress may be lost.":page==PAGE_RESTART?L"The game will close and relaunch through PORTHEON.":page==PAGE_ICON_CONFIRM?L"The game will close and restart. Save progress first.":L"Native Vulkan prototype for UFC 4";
    TextOutW(dc,35,69,sub,(int)wcslen(sub));
    if(page==PAGE_ICON_CONFIRM){
        const wchar_t *target=icon_choice==2?L"New game prompts: Keyboard & mouse":icon_choice?L"New game prompts: PlayStation":L"New game prompts: Xbox";
        TextOutW(dc,35,89,target,(int)wcslen(target));
    }
    const wchar_t **items=page==PAGE_MAIN?main_items:page==PAGE_SETTINGS?settings_items:page==PAGE_CHEATS?cheat_items:page==PAGE_GAME_ICONS?game_icon_items:page==PAGE_QUIT||page==PAGE_RESTART||page==PAGE_ICON_CONFIRM?choice_items:NULL;
    int count=row_count();
    if(items){
        int step=page==PAGE_MAIN?(panel_height<480?35:42):57;
        for(int i=0;i<count;++i){int y=113+i*step;RECT r={30,y,(LONG)panel_width-30,y+(page==PAGE_MAIN?step-4:42)};HBRUSH brush=CreateSolidBrush(i==selected?RGB(40,66,67):RGB(27,36,49));FillRect(dc,&r,brush);DeleteObject(brush);
            SetTextColor(dc,i==selected?RGB(127,246,193):RGB(235,241,247));TextOutW(dc,47,y+9,items[i],(int)wcslen(items[i]));
            if(page==PAGE_SETTINGS&&i==0){const wchar_t *value=button_style?L"PlayStation":L"Xbox";TextOutW(dc,(int)panel_width-190,y+9,value,(int)wcslen(value));}
            if(page==PAGE_SETTINGS&&i==1){const wchar_t *value=game_button_style==2?L"Keyboard & mouse":game_button_style?L"PlayStation":L"Xbox";TextOutW(dc,(int)panel_width-225,y+9,value,(int)wcslen(value));}
            if(page==PAGE_SETTINGS&&i==2){const wchar_t *value=skip_movies?L"On":L"Off";TextOutW(dc,(int)panel_width-145,y+9,value,(int)wcslen(value));}
            if(page==PAGE_GAME_ICONS&&i==game_button_style){const wchar_t *value=L"Current";TextOutW(dc,(int)panel_width-150,y+9,value,(int)wcslen(value));}
            if(page==PAGE_CHEATS&&i<2){const wchar_t *value=!cheats_ready?L"Unavailable":(cheats_mask&(1u<<i))?L"Next launch: On":L"Next launch: Off";TextOutW(dc,(int)panel_width-195,y+9,value,(int)wcslen(value));}
        }
    }else{SetTextColor(dc,RGB(235,241,247));
        const wchar_t *first=L"Screenshots, restart, quit and input are now ported.";
        const wchar_t *second=L"Stamina cheats use PORTHEON; Photo Mode needs UFC 4 work.";
        TextOutW(dc,35,135,first,(int)wcslen(first));
        TextOutW(dc,35,178,second,(int)wcslen(second));
    }
    SetTextColor(dc,RGB(163,177,194));
    const wchar_t *footer=button_style?L"D-pad: move    Cross: select    Circle: back    F10: close    F12: screenshot":L"D-pad: move    A: select    B: back    F10: close    F12: screenshot";
    TextOutW(dc,35,(int)panel_height-72,footer,(int)wcslen(footer));
    if(notice_until>GetTickCount()){SetTextColor(dc,RGB(127,246,193));TextOutW(dc,35,(int)panel_height-39,notice,(int)wcslen(notice));}
    SelectObject(dc,old);memcpy(destination,pixels,panel_width*panel_height*4);
    unsigned char *p=destination;for(uint32_t i=3;i<panel_width*panel_height*4;i+=4)p[i]=255;
    DeleteObject(title);DeleteObject(body);DeleteObject(bitmap);DeleteDC(dc);return TRUE;
}
