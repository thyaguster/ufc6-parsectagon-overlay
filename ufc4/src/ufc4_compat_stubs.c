/* UFC 4 compatibility & live-feature adapter for the shared Parsectagon Overlay UI. */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ufc4_live_cheats.h"
#include "live_cheats.h"
#include "career_cheats.h"
#include "fight_report_export.h"
#include "game_icons.h"
#include "game_version.h"
#include "restart_client.h"
#include "avatar_readback.h"
#include "photo_mode.h"
#include "replay_controls.h"
#include "audio_focus.h"
#include "startup_options.h"
#include "startup_prompts.h"
#include "progression_notice.h"
#include "instant_replay_gate.h"
#include "exact_hooks.h"
#include "performance_settings.h"
#include "photo_effects.h"

/* --- Live Cheats (backed by ufc4_live_cheats.c) --- */
static int u4_stam_ready, u4_dmg_ready, u4_ko_ready, u4_dlc_unlocked;
static uintptr_t u4_guest_base = (uintptr_t)0x800000000ULL;

static int ufc4_patch_guest(uintptr_t addr, const unsigned char *sig, size_t sig_len, size_t patch_off, const unsigned char *patch, size_t patch_len) {
    unsigned char buf[32];
    SIZE_T got = 0;
    if (sig_len > sizeof(buf) || !ReadProcessMemory(GetCurrentProcess(), (const void *)addr, buf, sig_len, &got) || got != sig_len) return 0;
    if (memcmp(buf + patch_off, patch, patch_len) == 0) return 1;
    if (memcmp(buf, sig, sig_len) != 0) return 0;
    DWORD old_prot = 0;
    if (!VirtualProtect((void *)(addr + patch_off), patch_len, PAGE_EXECUTE_READWRITE, &old_prot)) return 0;
    memcpy((void *)(addr + patch_off), patch, patch_len);
    VirtualProtect((void *)(addr + patch_off), patch_len, old_prot, &old_prot);
    FlushInstructionCache(GetCurrentProcess(), (const void *)(addr + patch_off), patch_len);
    return 1;
}

static void ufc4_unlock_dlc_fighters(void) {
    if (u4_dlc_unlocked) return;
    static const unsigned char sig_fn[18]  = {0x4c,0x8d,0x2d,0x58,0xee,0x42,0x01,0x84,0xc0,0x4c,0x89,0xbd,0xb8,0xfd,0xff,0xff,0x74,0x68};
    static const unsigned char pat_fn[2]   = {0xeb,0x68};
    static const unsigned char sig_dl[16]  = {0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xec,0x20,0x4c};
    static const unsigned char pat_dl[3]   = {0xb0,0x01,0xc3};
    static const unsigned char sig_car[16] = {0x48,0x8b,0x78,0x10,0x8b,0x70,0x18,0x48,0x8b,0x07,0xff,0x90,0x00,0x01,0x00,0x00};
    static const unsigned char pat_car[6]  = {0xb8,0x01,0x00,0x00,0x00,0x90};
    static const unsigned char sig_itm[7]  = {0x48,0x83,0x7b,0x50,0x00,0x74,0x4a};
    static const unsigned char pat_itm[7]  = {0xb0,0x01,0xeb,0x29,0x90,0x90,0x90};
    int ok1 = ufc4_patch_guest(u4_guest_base + 0x00ac9e28ULL, sig_fn, sizeof(sig_fn), 16, pat_fn, sizeof(pat_fn));
    int ok2 = ufc4_patch_guest(u4_guest_base + 0x00a920c0ULL, sig_dl, sizeof(sig_dl), 0, pat_dl, sizeof(pat_dl));
    int ok3 = ufc4_patch_guest(u4_guest_base + 0x008db890ULL, sig_car, sizeof(sig_car), 10, pat_car, sizeof(pat_car));
    int ok4 = ufc4_patch_guest(u4_guest_base + 0x00a42278ULL, sig_itm, sizeof(sig_itm), 0, pat_itm, sizeof(pat_itm));
    if (ok1 && ok2 && ok3 && ok4) u4_dlc_unlocked = 1;
}

void live_cheats_init(LiveCheats *state, uintptr_t guest_base) {
    if (guest_base) u4_guest_base = guest_base;
    if (state) memset(state, 0, sizeof(*state));
    ufc4_unlock_dlc_fighters();
    u4_stam_ready = ufc4_live_stamina_prepare();
    u4_dmg_ready = ufc4_live_damage_prepare();
    u4_ko_ready = ufc4_live_ko_prepare();
}
void live_cheats_tick(LiveCheats *state) {
    if (!state) return;
    ufc4_unlock_dlc_fighters();
    if (!u4_stam_ready) u4_stam_ready = ufc4_live_stamina_prepare();
    if (!u4_dmg_ready) u4_dmg_ready = ufc4_live_damage_prepare();
    if (!u4_ko_ready) u4_ko_ready = ufc4_live_ko_prepare();
    state->flags[LIVE_STAMINA].status = !u4_stam_ready ? LIVE_UNSUPPORTED : (ufc4_live_stamina_mask() & 1u) ? LIVE_ON : LIVE_OFF;
    state->flags[LIVE_STAMINA].requested = (state->flags[LIVE_STAMINA].status == LIVE_ON);
    state->flags[LIVE_HEALTH].status = !u4_dmg_ready ? LIVE_UNSUPPORTED : (ufc4_live_damage_mask() & 2u) ? LIVE_ON : LIVE_OFF;
    state->flags[LIVE_HEALTH].requested = (state->flags[LIVE_HEALTH].status == LIVE_ON);
}
int live_cheats_toggle(LiveCheats *state, int which) {
    if (which == LIVE_STAMINA) {
        if (!u4_stam_ready) u4_stam_ready = ufc4_live_stamina_prepare();
        unsigned mask = 0;
        if (!u4_stam_ready || !ufc4_live_stamina_toggle(0, &mask)) return 0;
        if (state) live_cheats_tick(state);
        return 1;
    }
    if (which == LIVE_HEALTH) {
        if (!u4_dmg_ready) u4_dmg_ready = ufc4_live_damage_prepare();
        unsigned mask = 0;
        if (!u4_dmg_ready || !ufc4_live_damage_toggle(&mask)) return 0;
        if (state) live_cheats_tick(state);
        return 1;
    }
    return 0;
}
int live_cheats_fight_ready(void) { return u4_stam_ready || u4_dmg_ready; }
int live_ko_toggle(void) {
    if (!u4_ko_ready) u4_ko_ready = ufc4_live_ko_prepare();
    unsigned armed = 0;
    return u4_ko_ready && ufc4_live_ko_toggle(&armed);
}
int live_ko_poll(int allowed) {
    (void)allowed;
    ufc4_live_ko_start_window();
    return ufc4_live_ko_poll();
}
void live_cheats_stop(LiveCheats *state) { (void)state; }
void live_cheats_sparring_clock_toggle(void) {}
int live_cheats_sparring_clock_get(void) { return 0; }
void live_cheats_sparring_clock_tick(void) {}

/* --- Career Cheats (unavailable in UFC 4) --- */
void career_cheats_init(uintptr_t base) { (void)base; }
void career_cheats_tick(void) {}
void career_cheats_cancel(void) {}
void career_cheats_snapshot(CareerSnapshot *out) { if (out) memset(out, 0, sizeof(*out)); }
void career_cheats_diagnostics(CareerDiagnostics *out) { if (out) memset(out, 0, sizeof(*out)); }
const char *career_cheats_diagnostic_name(int reason) { (void)reason; return "unavailable_ufc4"; }
int career_cheats_add(int kind, uint32_t amount) { (void)kind; (void)amount; return 0; }
int career_cheats_restore_fitness(void) { return 0; }
int career_cheats_restore_days(void) { return 0; }
int career_cheats_protect_longevity(int enabled) { (void)enabled; return 0; }
int career_cheats_training_multiplier(int value) { (void)value; return 0; }
int career_cheats_move_xp_multiplier(int value) { (void)value; return 0; }
int career_cheats_grade_a(void) { return 0; }
int career_cheats_max_moves(void) { return 0; }
int career_cheats_max_moves_group(int group) { (void)group; return 0; }

/* --- Fight Reports (backed by ufc4_fight_report_watcher.exe) --- */
static void ufc4_module_dir(wchar_t *dir, DWORD count) {
    HMODULE mod = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(uintptr_t)&ufc4_module_dir, &mod);
    if (!mod || !GetModuleFileNameW(mod, dir, count)) { *dir = 0; return; }
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *slash = 0; else *dir = 0;
}
static volatile LONG u4_watcher_started;
void ufc4_start_report_watcher(void) {
    if (InterlockedCompareExchange(&u4_watcher_started, 1, 0) != 0) return;
    wchar_t dir[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH + 8];
    ufc4_module_dir(dir, MAX_PATH);
    if (!*dir) return;
    swprintf(exe, MAX_PATH, L"%ls\\ufc4_fight_report_watcher.exe", dir);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) return;
    swprintf(cmd, MAX_PATH + 8, L"\"%ls\"", exe);
    STARTUPINFOW si = { .cb = sizeof(si) }; PROCESS_INFORMATION pi = { 0 };
    if (CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}
static const GUID u4_docs_guid = {0xfdd39ad0,0x238f,0x46af,{0xad,0xb4,0x6c,0x85,0x48,0x03,0x69,0xc7}};
int fight_report_match_state(void) { return -1; }
int fight_report_format(const FightReportRaw *raw, const char *version, const char *engine, const char *id, unsigned long long captured_at, char *json, size_t capacity) {
    (void)raw; (void)version; (void)engine; (void)id; (void)captured_at; (void)json; (void)capacity; return 0;
}
int fight_report_format_result(const FightReportRaw *raw, const char *version, const char *engine, const char *id, unsigned long long captured_at, char *json, size_t capacity) {
    (void)raw; (void)version; (void)engine; (void)id; (void)captured_at; (void)json; (void)capacity; return 0;
}
int fight_report_read_from_folder(const wchar_t *folder, FightReportSummary *out) {
    if (!folder || !out) return 0;
    memset(out, 0, sizeof(*out));
    wchar_t pattern[MAX_PATH];
    swprintf(pattern, MAX_PATH, L"%ls\\ufc4-*.json", folder);
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    FILETIME best = { 0 }; wchar_t best_name[MAX_PATH] = { 0 };
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (CompareFileTime(&fd.ftLastWriteTime, &best) > 0) {
            best = fd.ftLastWriteTime;
            lstrcpynW(best_name, fd.cFileName, MAX_PATH);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!best_name[0]) return 0;
    swprintf(out->file_path, 260, L"%ls\\%ls", folder, best_name);
    out->available = 1;
    wchar_t marker[MAX_PATH];
    swprintf(marker, MAX_PATH, L"%ls.upload.json", out->file_path);
    if (GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES) out->upload_state = REPORT_UPLOAD_SUBMITTED;
    else {
        swprintf(marker, MAX_PATH, L"%ls.upload-error.txt", out->file_path);
        if (GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES) out->upload_state = REPORT_UPLOAD_NEEDS_REVIEW;
        else out->upload_state = REPORT_UPLOAD_SAVED_LOCAL;
    }
    return 1;
}
int fight_report_read_latest(FightReportSummary *out) {
    PWSTR docs = NULL; wchar_t folder[MAX_PATH];
    if (FAILED(SHGetKnownFolderPath(&u4_docs_guid, 0, NULL, &docs)) || !docs) return 0;
    swprintf(folder, MAX_PATH, L"%ls\\UFC Parsectagon Overlay\\UFC4\\Fight Reports", docs);
    CoTaskMemFree(docs);
    return fight_report_read_from_folder(folder, out);
}
int fight_report_export(const wchar_t *game_version, int new_engine, wchar_t *path, size_t path_count, wchar_t *error, size_t error_count) {
    (void)game_version; (void)new_engine;
    wchar_t dir[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH + 48];
    ufc4_module_dir(dir, MAX_PATH);
    swprintf(exe, MAX_PATH, L"%ls\\ufc4_fight_report_watcher.exe", dir);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
        if (error && error_count) lstrcpynW(error, L"ufc4_fight_report_watcher.exe not found.", (int)error_count);
        return 0;
    }
    swprintf(cmd, MAX_PATH + 48, L"\"%ls\" --once --pid %lu", exe, GetCurrentProcessId());
    STARTUPINFOW si = { .cb = sizeof(si) }; PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        if (error && error_count) lstrcpynW(error, L"Could not start fight report exporter.", (int)error_count);
        return 0;
    }
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (code != 0) {
        if (error && error_count) lstrcpynW(error, L"Open a finished UFC 4 fight to export.", (int)error_count);
        return 0;
    }
    FightReportSummary s = { 0 };
    if (fight_report_read_latest(&s) && path && path_count) lstrcpynW(path, s.file_path, (int)path_count);
    return 1;
}
int fight_report_export_result(const wchar_t *game_version, int new_engine, wchar_t *path, size_t path_count, wchar_t *error, size_t error_count) {
    return fight_report_export(game_version, new_engine, path, path_count, error, error_count);
}

/* --- Game Icons, Restart & Startup (UFC 4 native helpers) --- */
static int u4_icon_style = 1;
void game_icons_init(HMODULE module) { (void)module; game_icons_check(); }
void game_icons_check(void) {
    wchar_t dir[MAX_PATH], ini[MAX_PATH]; ufc4_module_dir(dir, MAX_PATH);
    swprintf(ini, MAX_PATH, L"%ls\\ufc4-quit-menu.ini", dir);
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) swprintf(ini, MAX_PATH, L"%ls\\ufc4_actions.ini", dir);
    u4_icon_style = (int)GetPrivateProfileIntW(L"Appearance", L"GameButtonStyle", 1, ini);
    if (u4_icon_style < 0 || u4_icon_style > 2) u4_icon_style = 1;
}
int game_icons_status(wchar_t *message, size_t count) {
    if (message && count) lstrcpynW(message, L"Applies Xbox, PlayStation, or Keyboard prompts on restart.", (int)count);
    return u4_icon_style == 0 ? GAME_ICONS_XBOX : u4_icon_style == 2 ? GAME_ICONS_KEYBOARD : GAME_ICONS_PS;
}
static BOOL u4_launch_restart(HWND game, int icon_style) {
    wchar_t dir[MAX_PATH], helper[MAX_PATH], ev[110], cmd[500];
    ufc4_module_dir(dir, MAX_PATH);
    if (!game || !*dir) return FALSE;
    swprintf(helper, MAX_PATH, L"%ls\\ufc4_actions_restart.exe", dir);
    wsprintfW(ev, L"Local\\UFC4ActionsRestart_%lu_%lu", GetCurrentProcessId(), GetTickCount());
    HANDLE ready = CreateEventW(NULL, TRUE, FALSE, ev);
    if (!ready) return FALSE;
    if (icon_style < 0) wsprintfW(cmd, L"\"%ls\" %lu \"%ls\"", helper, GetCurrentProcessId(), ev);
    else wsprintfW(cmd, L"\"%ls\" %lu \"%ls\" %ls", helper, GetCurrentProcessId(), ev, icon_style == 2 ? L"keyboard" : icon_style == 0 ? L"xbox" : L"ps");
    STARTUPINFOW si = { .cb = sizeof(si) }; PROCESS_INFORMATION pi = { 0 };
    BOOL ok = CreateProcessW(helper, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (ok) { CloseHandle(pi.hThread); ok = WaitForSingleObject(ready, 5000) == WAIT_OBJECT_0; CloseHandle(pi.hProcess); }
    CloseHandle(ready);
    if (ok) PostMessageW(game, WM_CLOSE, 0, 0);
    return ok;
}
BOOL game_icons_apply(HWND game, int style) { return u4_launch_restart(game, style); }
BOOL game_icons_edit_bindings(HWND game) { (void)game; return FALSE; }
BOOL menu_restart_game(HWND game, HMODULE module) { (void)module; return u4_launch_restart(game, -1); }
BOOL menu_shutdown_pc(HWND game, HMODULE module) { (void)game; (void)module; return FALSE; }
BOOL menu_reboot_pc(HWND game, HMODULE module, BOOL auto_launch) { (void)game; (void)module; (void)auto_launch; return FALSE; }
void startup_assets_check(void) {}
int ufc4_sync_skip_movies(int change) {
    wchar_t path[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (!n || n >= MAX_PATH - 34) return -1;
    lstrcatW(path, L"\\UFC4Native\\sacredfire.ini");
    HANDLE file = CreateFileW(path, change >= 0 ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return -1;
    char data[16384]; DWORD size = 0; int value = -1; DWORD at = 0; int found = 0;
    if (ReadFile(file, data, sizeof(data), &size, NULL) && size < sizeof(data)) {
        const char key[] = "skip_movies=";
        for (DWORD i = 0; i + sizeof(key) <= size; i++) {
            if ((i == 0 || data[i - 1] == '\n') && !memcmp(data + i, key, sizeof(key) - 1) &&
                (data[i + sizeof(key) - 1] == '0' || data[i + sizeof(key) - 1] == '1') &&
                (i + sizeof(key) == size || data[i + sizeof(key)] == '\r' || data[i + sizeof(key)] == '\n')) {
                at = i + sizeof(key) - 1; value = data[at] - '0'; found++;
            }
        }
        if (found != 1) value = -1;
        if (value >= 0 && change >= 0 && value != change) {
            DWORD done = 0; char digit = (char)('0' + (change ? 1 : 0));
            if (SetFilePointer(file, (LONG)at, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
                !WriteFile(file, &digit, 1, &done, NULL) || done != 1 || !FlushFileBuffers(file)) value = -1;
            else value = change ? 1 : 0;
        }
    }
    CloseHandle(file);
    return value;
}
int startup_assets_status(wchar_t *message, size_t count) {
    if (message && count) lstrcpynW(message, L"Not needed in UFC 4 (use Skip intros in Settings).", (int)count);
    return 0;
}
BOOL startup_assets_apply(HWND game, int enabled) { (void)game; (void)enabled; return FALSE; }

/* --- Stubs for UFC 6-only features --- */
GameVersion game_version_detect(void) { return (GameVersion){1, 0, L"28.00"}; }
int game_version_parse_json(const char *json, size_t length, GameVersion *out) { (void)json; (void)length; if (out) *out = game_version_detect(); return 1; }
int avatar_readback_mode(const wchar_t *engine_dir) { (void)engine_dir; return AVATAR_READBACK_OFF; }
BOOL avatar_readback_set(const wchar_t *engine_dir, int mode, wchar_t *error, size_t count) {
    (void)engine_dir; (void)mode;
    if (error && count) lstrcpynW(error, L"Not available in UFC 4.", (int)count);
    return FALSE;
}
void photo_mode_prepare(void) {}
int photo_mode_start(int fight_ready) { (void)fight_ready; return PHOTO_START_NO_FIGHT; }
void photo_mode_command(unsigned command) { (void)command; }
void photo_mode_set_fov(int degrees) { (void)degrees; }
void photo_mode_poll(int focused, int menu_open, const PhotoInput *input) { (void)focused; (void)menu_open; (void)input; }
int photo_mode_blocks_input(void) { return 0; }
int photo_mode_stop_pending(void) { return 0; }
void photo_mode_snapshot(PhotoSnapshot *out) { if (out) memset(out, 0, sizeof(*out)); }
void photo_mode_fpv_set(int target) { (void)target; }
int photo_mode_fpv_get(void) { return 0; }
void photo_mode_fpv_adjust(float p, float h, float f, int fov, float g) { (void)p; (void)h; (void)f; (void)fov; (void)g; }
void photo_mode_fpv_get_tune(float *p, float *h, float *f, int *fov, float *g) { if(p)*p=0;if(h)*h=0;if(f)*f=0;if(fov)*fov=90;if(g)*g=0.65f; }
int photo_mode_fpv_diag(void) { return 0; }
int replay_prepare(void) { return 0; }
void replay_offer_decisions(int enabled) { (void)enabled; }
unsigned replay_decision_events(void) { return 0; }
void replay_poll(int focused, int photo_active) { (void)focused; (void)photo_active; }
void replay_snapshot(ReplaySnapshot *out) { if (out) memset(out, 0, sizeof(*out)); }
void replay_request_coverage(void) {}
int replay_toggle_pause(void) { return 0; }
int replay_toggle_range(void) { return 0; }
int replay_set_speed(int index) { (void)index; return 0; }
int replay_step(int direction) { (void)direction; return 0; }
int replay_step_ms(int direction, int milliseconds) { (void)direction; (void)milliseconds; return 0; }
int audio_focus_prepare(void) { return 0; }
void audio_focus_poll(int enabled, int focused) { (void)enabled; (void)focused; }
int startup_options_tick(StartupOptions *options, uintptr_t guest, int short_movie, int skip_splashes) { (void)options; (void)guest; (void)short_movie; (void)skip_splashes; return 0; }
int startup_prompt_can_confirm(int id, const StartupPromptContext *context) { (void)id; (void)context; return 0; }
int startup_prompt_identify(const unsigned char *pixels, uint32_t width, uint32_t height) { (void)pixels; (void)width; (void)height; return 0; }
int progression_notice_prime_engine(void) { return 0; }
int progression_notice_prepare(void) { return 0; }
void progression_notice_set(int enabled) { (void)enabled; }
int progression_notice_state(void) { return 0; }
const char *progression_notice_error(void) { return ""; }
void progression_notice_counts(unsigned long *calls, unsigned long *suppressed, unsigned long *unmatched) {
    if (calls) *calls = 0; if (suppressed) *suppressed = 0; if (unmatched) *unmatched = 0;
}
int instant_replay_gate_tick(InstantReplayGate *gate, uintptr_t guest_base, int enabled) { (void)gate; (void)guest_base; (void)enabled; return 0; }
int install_exact_hooks(ExactHook *hooks, size_t count, const char **error) { (void)hooks; (void)count; if (error) *error = "none"; return 0; }
int performance_load(const wchar_t *engine_dir, PerformanceSettings *out) { (void)engine_dir; if (out) memset(out, 0, sizeof(*out)); return 0; }
int performance_save(const wchar_t *bin, const PerformanceSettings *expected, const PerformanceSettings *desired) { (void)bin; (void)expected; (void)desired; return 0; }
int apply_parsec_preset(int preset_id) { (void)preset_id; return 0; }
void version_proxy_initialize(HMODULE module) { (void)module; }

/* --- UFC 4 SDL3 IAT Input Blocker --- */
typedef int (*SdlPollEventFn)(void *);
typedef int (*SdlGamepadButtonFn)(void *, int);
typedef short (*SdlGamepadAxisFn)(void *, int);
typedef const unsigned char *(*SdlKeyboardStateFn)(int *);
static SdlPollEventFn real_poll_event;
static SdlGamepadButtonFn real_gamepad_button;
static SdlGamepadAxisFn real_gamepad_axis;
static SdlKeyboardStateFn real_keyboard_state;
static int (*u4_menu_open_fn)(void);
static volatile LONG u4_sdl_hooked;

static int hook_poll_event(void *event) {
    if (!u4_menu_open_fn || !u4_menu_open_fn() || !event) return real_poll_event(event);
    while (real_poll_event(event)) {
        uint32_t type = *(uint32_t *)event;
        if (type == 0x100 || (type >= 0x200 && type < 0x300)) return 1;
    }
    return 0;
}
static int hook_gamepad_button(void *pad, int button) {
    return (u4_menu_open_fn && u4_menu_open_fn()) ? 0 : (real_gamepad_button ? real_gamepad_button(pad, button) : 0);
}
static short hook_gamepad_axis(void *pad, int axis) {
    return (u4_menu_open_fn && u4_menu_open_fn()) ? 0 : (real_gamepad_axis ? real_gamepad_axis(pad, axis) : 0);
}
static const unsigned char *hook_keyboard_state(int *numkeys) {
    static unsigned char blocked[512];
    const unsigned char *keys = real_keyboard_state ? real_keyboard_state(numkeys) : NULL;
    if (!u4_menu_open_fn || !u4_menu_open_fn()) return keys;
    return blocked;
}
void ufc4_install_sdl_hooks(int (*is_open)(void)) {
    u4_menu_open_fn = is_open;
    if (InterlockedCompareExchange(&u4_sdl_hooked, 1, 0) != 0) return;
    HMODULE exe = GetModuleHandleW(NULL);
    if (!exe) return;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)exe;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((unsigned char *)exe + dos->e_lfanew);
    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return;
    for (IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)((unsigned char *)exe + rva); imp->Name; ++imp) {
        const char *dll = (const char *)((unsigned char *)exe + imp->Name);
        if (_stricmp(dll, "SDL3.dll")) continue;
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)((unsigned char *)exe + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        IMAGE_THUNK_DATA *slots = (IMAGE_THUNK_DATA *)((unsigned char *)exe + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const char *name = (const char *)((IMAGE_IMPORT_BY_NAME *)((unsigned char *)exe + names->u1.AddressOfData))->Name;
            void *hook = NULL; void **save = NULL;
            if (!strcmp(name, "SDL_PollEvent")) { hook = (void *)hook_poll_event; save = (void **)&real_poll_event; }
            else if (!strcmp(name, "SDL_GetGamepadButton")) { hook = (void *)hook_gamepad_button; save = (void **)&real_gamepad_button; }
            else if (!strcmp(name, "SDL_GetGamepadAxis")) { hook = (void *)hook_gamepad_axis; save = (void **)&real_gamepad_axis; }
            else if (!strcmp(name, "SDL_GetKeyboardState")) { hook = (void *)hook_keyboard_state; save = (void **)&real_keyboard_state; }
            if (hook && !*save) {
                DWORD old = 0;
                if (VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &old)) {
                    *save = (void *)(uintptr_t)slots->u1.Function;
                    slots->u1.Function = (ULONG_PTR)hook;
                    VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), old, &old);
                }
            }
        }
    }
}
