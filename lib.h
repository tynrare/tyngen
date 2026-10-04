/*
 * File version: 3.4.0
 * Last updated: 2026-10-04
 * Purpose: Shared workbench state and action interface.
 * Flow APP-01 canonical steps 1-7 and APP-02: lib.c header; APP-01 step 2 owns reference gestures/trash; step 6 owns fonts; step 7 owns text prompts; APP-02 owns prompt rename and popup dismissal.
 * Shared invariant: destructive reference moves require explicit confirmation; enabled text refs enter payload; editor selection is independent.
 */

/* agent: gpt-6.1-sol | 2026-10-04 | Remove transient details view state | v 3.4.0 | cb703b */
#ifndef AIGEN_LIB_H
#define AIGEN_LIB_H

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define APP_TEXT_CAP 8192
#define APP_PATH_CAP 1024
#define APP_MAX_PROFILES 12
#define APP_MAX_PROMPTS 6
#define APP_MAX_REFS 128
#define APP_MAX_PROMPT_PARTS (APP_MAX_REFS + APP_MAX_PROMPTS)
#define APP_MAX_RUNS 64
#define APP_MAX_LOGS 512
#define APP_MAX_STAGES 64
#define APP_MAX_OUTPUTS 64
#define APP_MAX_FONTS 32

typedef enum { TAB_REFERENCES, TAB_OUTPUT, TAB_PROGRESS, TAB_COUNT } AppTab;
typedef enum { RUN_IDLE, RUN_PREVIEW, RUN_RUNNING, RUN_SUCCEEDED, RUN_FAILED, RUN_CANCELED } RunState;
typedef enum { PROC_NONE, PROC_GENERATION, PROC_PIXEL } ProcessKind;
typedef enum { ACTION_NONE, ACTION_PREVIEW, ACTION_CONFIRM, ACTION_CANCEL, ACTION_SAVE_PROMPT,
               ACTION_SELECT_TAB, ACTION_REF_IMPORT, ACTION_REF_HIDE, ACTION_REF_EXCLUDE,
               ACTION_REF_DELETE, ACTION_REF_OPEN, ACTION_PROFILE_SELECT, ACTION_PROFILE_SAVE,
               ACTION_PROFILE_ADD, ACTION_PROFILE_DUPLICATE, ACTION_PROFILE_DELETE,
               ACTION_RUN_RERUN, ACTION_RUN_NOTE, ACTION_RUN_LOG_CLEAR,
               ACTION_RUN_OUTPUT_OPEN, ACTION_SET_FIELD, ACTION_STATE } ActionKind;

typedef struct {
    char name[64], model[128], aspect[24], layout[32], pixel_pitch[32];
    char pitch_preset[24], pitch_group_layout[24], align_blobs[24], magenta[24], edge_alpha[24];
    int slim_upload, rearrange, padding, tile_sizes[8], tile_count;
    char sets[APP_MAX_PROMPTS][192];
    int set_count;
    char prompts[APP_MAX_PROMPTS][512];
    int prompt_count;
} AppProfile;

/* agent: gpt-6.1-sol | 2026-10-04 | Store prompt payload snapshot state | v 3.0.0 | 739150 */
typedef struct {
    char path[APP_PATH_CAP], name[256], preview[256];
    bool excluded, selected, is_text;
    Texture2D thumb, full;
    int width, height;
} AppReference;
/* agent: gpt-6.1-sol | 2026-10-04 | Replace hidden refs with trash state | v 3.0.0 | 1296fd */

typedef struct {
    char text[APP_TEXT_CAP];
    int length;
    char time[32], process[20];
} AppLogLine;

typedef struct {
    char name[128], path[APP_PATH_CAP];
    int number;
    Texture2D texture, thumb;
    int width, height;
} AppArtifact;

typedef struct {
    char id[64], dir[APP_PATH_CAP], source[APP_PATH_CAP], primary[APP_PATH_CAP];
    char prompt[APP_TEXT_CAP], resolved_prompt[APP_TEXT_CAP], error[512], current_step[256];
    char note[APP_TEXT_CAP];
    char stdout_pending[2048], stderr_pending[2048];
    RunState state;
    ProcessKind process_kind;
    pid_t child;
    int pipe_fd, err_fd, exit_code;
    double started, finished;
    bool cancel_pending, debug, follow_log;
    char pending_signal[32];
    AppArtifact stages[APP_MAX_STAGES];
    int stage_count, selected_stage;
    AppArtifact outputs[APP_MAX_OUTPUTS];
    int output_count, selected_output;
    AppLogLine logs[APP_MAX_LOGS];
    int log_count, log_scroll;
} AppRun;

/* agent: gpt-6.1-sol | 2026-10-04 | Track pending prompt close decision | v 3.2.0 | 9e2977 */
typedef struct {
    char workspace[APP_PATH_CAP], prompt_path[APP_PATH_CAP], profiles_path[APP_PATH_CAP];
    char refs_path[APP_PATH_CAP], runs_path[APP_PATH_CAP], refs_state_path[APP_PATH_CAP];
    char message[512], edit_field[32], field_buffer[512], prompt_edit[APP_TEXT_CAP];
    char font_dir[APP_PATH_CAP], font_state_path[APP_PATH_CAP], ui_font_name[256];
    char font_paths[APP_MAX_FONTS][APP_PATH_CAP];
    int font_count, selected_font;
    int prompt_caret, prompt_anchor, prompt_scroll;
    int field_caret, field_anchor, field_scroll;
    bool field_selecting;
    AppTab tab;
    AppProfile profiles[APP_MAX_PROFILES];
    int profile_count, active_profile;
    int active_prompt_part, active_set_part;
    AppReference refs[APP_MAX_REFS];
    int ref_count, selected_ref, thumb_size;
    AppRun runs[APP_MAX_RUNS];
    int run_count, active_run, selected_run;
    AppProfile snapshot_profile;
    char snapshot_prompt[APP_TEXT_CAP], snapshot_resolved[APP_TEXT_CAP];
    char snapshot_prompt_parts[APP_MAX_PROMPT_PARTS][APP_TEXT_CAP];
    int snapshot_prompt_part_count;
    char snapshot_refs[APP_MAX_REFS][APP_PATH_CAP], snapshot_run_id[64], snapshot_run_dir[APP_PATH_CAP];
    int snapshot_ref_count;
    bool prompt_dirty, prompt_editor_focused, prompt_save_error, prompt_limit_error, prompt_selecting, prompt_close_pending, show_full_image, show_compare, show_profile_editor, debug_stages, snapshot_debug, preview_ready;
    bool settings_view, show_add_menu;
    int compare_mode, compare_stage, settings_scroll, reference_scroll;
    char pending_action[64], pending_path[APP_PATH_CAP], rerun_source[APP_PATH_CAP];
    bool rerun_pixel_only;
    float image_zoom, image_pan_x, image_pan_y;
    int enet_port;
    void *enet_host, *enet_peer;
    double last_enet_fetch;
} App;
/* agent: gpt-6.1-sol | 2026-10-04 | Remove transient details view state | v 3.4.0 | cb703b */

/* App initialization: initialize raylib-independent workspace and ENet state. */
bool AppInit(App *app, const char *workspace, bool mcp_mode);
/* Release textures, child pipes, and network state owned by the app. */
void AppShutdown(App *app);
/* Poll child processes, ENet events, and workspace artifacts once per frame. */
void AppUpdate(App *app);
/* Draw the workbench UI using the current raylib frame. */
void AppDraw(App *app);
/* Dispatch UI, ENet, or MCP command text through the shared action handlers. */
const char *AppDispatch(App *app, const char *request, char *response, size_t response_size);
/* Run the MCP stdio client which forwards tool actions to a running GUI over ENet. */
int AppMcpClient(const char *workspace, int port);
/* Return an MCP tool listing and instructions as a JSON document. */
const char *AppMcpToolsJson(void);

#endif
/* agent: gpt-6.1-sol | 2026-10-04 | Remove transient details view state | v 3.4.0 | cb703b */
