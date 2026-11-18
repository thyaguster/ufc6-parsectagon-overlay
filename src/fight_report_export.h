#ifndef UFC6_FIGHT_REPORT_EXPORT_H
#define UFC6_FIGHT_REPORT_EXPORT_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

enum {
    REPORT_STATE, REPORT_RESULT_TYPE, REPORT_ROUND, REPORT_ROUND_LENGTH,
    REPORT_ONLINE_RESULT, REPORT_ONLINE_RESULT_TYPE, REPORT_MATCH_TIME,
    REPORT_KNOCKDOWNS, REPORT_SIG_ATTEMPTED, REPORT_SIG_LANDED,
    REPORT_TOTAL_ATTEMPTED, REPORT_TOTAL_LANDED, REPORT_TD_ATTEMPTED,
    REPORT_TD_LANDED, REPORT_STUNS, REPORT_CONTROL, REPORT_FIELD_COUNT
};
typedef struct { uint32_t value[REPORT_FIELD_COUNT][2]; } FightReportRaw;

/* Returns -1 when the engine bindings are unavailable, otherwise the game's
   match state. This is a cheap read used to arm one automatic capture/fight. */
int fight_report_match_state(void);

int fight_report_format(const FightReportRaw *raw,const char *version,const char *engine,
                        const char *id,unsigned long long captured_at,
                        char *json,size_t capacity);
int fight_report_format_result(const FightReportRaw *raw,const char *version,const char *engine,
                               const char *id,unsigned long long captured_at,
                               char *json,size_t capacity);
enum {
    REPORT_UPLOAD_NONE, REPORT_UPLOAD_SAVED_LOCAL, REPORT_UPLOAD_WAITING,
    REPORT_UPLOAD_SUBMITTED, REPORT_UPLOAD_NEEDS_REVIEW
};
typedef struct {
    int available, has_stats, round, clock_seconds, upload_state;
    int red_kd, blue_kd, red_sig_l, red_sig_a, blue_sig_l, blue_sig_a;
    int red_tot_l, red_tot_a, blue_tot_l, blue_tot_a;
    int red_td_l, red_td_a, blue_td_l, blue_td_a;
    int red_stuns, blue_stuns, red_ctrl, blue_ctrl;
    wchar_t file_path[260], winner[64], method[48], capture_time[48], upload_error[160];
} FightReportSummary;

int fight_report_export(const wchar_t *game_version,int new_engine,
                        wchar_t *path,size_t path_count,wchar_t *error,size_t error_count);
int fight_report_export_result(const wchar_t *game_version,int new_engine,
                               wchar_t *path,size_t path_count,wchar_t *error,size_t error_count);
int fight_report_read_from_folder(const wchar_t *folder,FightReportSummary *out);
int fight_report_read_latest(FightReportSummary *out);
#endif
