/*
 * File version: 3.42.0
 * Last updated: 2026-10-06
 * Purpose: Shared workbench and processing graph state.
 * Flow META-01 journals, per-sprite stage evidence, and screenshots: lib.c header; Flow APP-01 canonical steps 1-8 and APP-02: lib.c header; APP-03 navigation, APP-04 shared image browsing/trash/live run images/Controls toggle/artifact waterfall, APP-05 flat resources root, APP-06 drawing, APP-07 profile resources, and APP-08 async image loading: lib.c.
 * Flow APP-06 drawing session and save behavior: lib.c header; APP-07 resource/profile state and APP-08 async loading and texture cache: lib.c header.
 * Flow PIXEL-02 graph IDs, branch lanes, and artifact geometry: lib.c header; schema and execution: pixelprocess/common.hpp and runner.cpp.
 * Shared invariant: destructive resource moves require explicit confirmation; enabled text resources enter payload; editor selection is independent; exactly one profile resource is enabled.
 */

/* agent: gpt-6.1-sol | 2026-10-04 | Index header tabs and flat refs flow | v 3.9.0 | 948d14 */
/* agent: gpt-6.1-sol | 2026-10-04 | Remove optional artifact debug mode | v 3.12.0 | 729d0c */
/* agent: gpt-6.1-sol | 2026-10-04 | Track async asset and log loading | v 3.20.0 | d9535e */
/* agent: gpt-6.1-sol | 2026-10-04 | Track profile settings subview | v 3.21.0 | 0ff512 */
#ifndef AIGEN_LIB_H
#define AIGEN_LIB_H

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

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
#define APP_MAX_METADATA_EVENTS 128
#define APP_MAX_FONTS 32
#define APP_MAX_SPRITE_EVIDENCE 512
#define APP_MAX_SPRITE_BLOBS 128

typedef enum { VIEW_HOME, VIEW_LIBRARY, VIEW_GENERATION } AppView;
/* agent: gpt-6.1-sol | 2026-10-04 | Replace tab state with named views | v 3.5.0 | ffa161 */
typedef enum { RUN_IDLE, RUN_PREVIEW, RUN_RUNNING, RUN_SUCCEEDED, RUN_FAILED, RUN_CANCELED } RunState;
typedef enum { PROC_NONE, PROC_GENERATION, PROC_PIXEL } ProcessKind;
typedef enum { ACTION_NONE, ACTION_PREVIEW, ACTION_CONFIRM, ACTION_CANCEL, ACTION_SAVE_PROMPT,
               ACTION_REF_IMPORT, ACTION_REF_HIDE, ACTION_REF_EXCLUDE,
               ACTION_REF_DELETE, ACTION_REF_OPEN, ACTION_PROFILE_SELECT, ACTION_PROFILE_SAVE,
               ACTION_PROFILE_ADD, ACTION_PROFILE_DUPLICATE, ACTION_PROFILE_DELETE,
               ACTION_RUN_RERUN, ACTION_RUN_NOTE, ACTION_RUN_LOG_CLEAR,
               ACTION_RUN_OUTPUT_OPEN, ACTION_SET_FIELD, ACTION_STATE } ActionKind;

typedef struct {
    char pitch[32], phase_x[24], phase_y[24], cleanup_mode[16], sheet_anchor[24], pitch_precision[16];
    int background_tolerance, grid_cols, grid_rows, extraction_padding;
    int pitch_min, pitch_max, foreground_coverage, cleanup_tolerance, sheet_padding;
} AppPixelSettings;

/* agent: gpt-6.1-sol | 2026-10-04 | Store profile resource state | v 3.20.0 | f0cb7d */
typedef struct {
    char name[64], model[128], aspect[24];
    char path[APP_PATH_CAP];
    int slim_upload;
    char sets[APP_MAX_PROMPTS][192];
    int set_count;
    char prompts[APP_MAX_PROMPTS][512];
    int prompt_count;
    AppPixelSettings pixel;
} AppProfile;
/* agent: gpt-6.1-sol | 2026-10-04 | Store profile resource state | v 3.20.0 | f0cb7d */

/* agent: gpt-6.1-sol | 2026-10-04 | Store prompt payload snapshot state | v 3.0.0 | 739150 */
/* agent: gpt-6.1-sol | 2026-10-04 | Store profile resource state | v 3.20.0 | f0cb7d */
typedef struct {
    char path[APP_PATH_CAP], name[256], preview[256];
    bool excluded, selected, is_text, is_profile;
    bool loading;
    int profile_index;
    Texture2D thumb, full;
    int width, height;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
    double full_used_at, thumb_used_at;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
} AppReference;
/* agent: gpt-6.1-sol | 2026-10-04 | Store profile resource state | v 3.20.0 | f0cb7d */
/* agent: gpt-6.1-sol | 2026-10-04 | Replace hidden refs with trash state | v 3.0.0 | 1296fd */

typedef struct {
    char text[APP_TEXT_CAP];
    int length;
    char time[32], process[20];
} AppLogLine;

/* agent: gpt-6 | 2026-10-04 | Store processor metadata events and capture state | v 3.25.0 | 8407c6 */
typedef struct {
    int stage_number;
    char kind[24], time[40], variant_id[64], message[768];
} AppMetadataEvent;
/* agent: gpt-6 | 2026-10-04 | Store processor metadata events and capture state | v 3.25.0 | 8407c6 */

/* agent: gpt-6.1-sol | 2026-10-05 | Cache stage sprites and inspector state | v 3.28.0 | 3edb49 */
/* agent: gpt-6 | 2026-10-05 | Record source crop coordinates | v 3.33.0 | f87c25 */
typedef struct {
    int row, col, x, y, width, height, artifact_width, artifact_height;
    int source_x, source_y, source_width, source_height, source_artifact_width, source_artifact_height;
    char id[32], time[40], summary[512];
} AppSpriteEvidence;
/* agent: gpt-6 | 2026-10-05 | Record source crop coordinates | v 3.33.0 | f87c25 */

typedef struct {
    char id[32];
    int x, y, width, height, pixels, color[3];
} AppSpriteBlob;
/* agent: gpt-6.1-sol | 2026-10-05 | Cache selected sprite blob details | v 3.28.0 | c830ce */

/* agent: gpt-6 | 2026-10-05 | Store per-sprite lattice edits | v 3.33.0 | 25c87a */
typedef struct {
    char id[32];
    AppPixelSettings settings;
} AppSpriteSettings;
/* agent: gpt-6 | 2026-10-05 | Store per-sprite lattice edits | v 3.33.0 | 25c87a */

/* agent: gpt-6.1-sol | 2026-10-06 | Store branch-aware artifact identity | v 3.41.0 | 116f90 */
/* agent: gpt-6.1-sol | 2026-10-06 | Store graph node execution state | v 3.41.0 | 74a936 */
typedef struct {
    char name[128], path[APP_PATH_CAP];
    char node_id[96], branch_id[64], role[24], sprite_dir[APP_PATH_CAP];
    char state[24];
    int number, progress;
    Texture2D texture, thumb;
    int width, height;
    off_t file_size;
    bool loading;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
    double full_used_at, thumb_used_at;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
} AppArtifact;

/* agent: gpt-6.1-sol | 2026-10-04 | Track run artifact selection and output buffers | v 3.11.0 | 7d27ec */
/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.15.0 | d5f7dd */
/* agent: gpt-6.1-sol | 2026-10-04 | Track per-run resource snapshot | v 3.19.0 | 50a237 */
typedef struct {
    char id[64], dir[APP_PATH_CAP], source[APP_PATH_CAP], primary[APP_PATH_CAP];
    char prompt[APP_TEXT_CAP], resolved_prompt[APP_TEXT_CAP], error[512], current_step[256];
    char used_resources[APP_MAX_REFS][APP_PATH_CAP];
    int used_resource_count;
    char note[APP_TEXT_CAP];
    char stdout_pending[APP_TEXT_CAP], stderr_pending[APP_TEXT_CAP];
    RunState state;
    ProcessKind process_kind;
    pid_t child;
    int pipe_fd, err_fd, exit_code;
    double started, finished;
    bool cancel_pending, follow_log;
    bool logs_loaded, logs_loading;
    char pending_signal[32];
    AppArtifact stages[APP_MAX_STAGES];
    int stage_count, selected_stage;
    /* agent: gpt-6.1-sol | 2026-10-06 | Cache typed final sprite previews | v 3.42.0 | c7a014 */
    /* Cache loaded crops by sprite, stage, and typed final output. */
    AppArtifact sprite_version_previews[APP_MAX_STAGES + APP_MAX_OUTPUTS + 1];
    int sprite_version_preview_count;
    char sprite_version_preview_id[32];
    /* agent: gpt-6.1-sol | 2026-10-06 | Cache typed final sprite previews | v 3.42.0 | c7a014 */
    AppArtifact debug_stages[APP_MAX_STAGES];
    int debug_stage_count;
    bool selected_stage_debug;
    AppPixelSettings pixel_settings, pixel_settings_saved, sprite_settings;
    bool pixel_settings_dirty, sprite_settings_override, pixel_sprite_scope, pixel_settings_loaded;
    char selected_sprite_id[32], pixel_target[24], pixel_sprite[32], processing_settings_path[APP_PATH_CAP];
    AppSpriteSettings sprite_overrides[APP_MAX_SPRITE_EVIDENCE];
    int sprite_override_count;
    char owner_components[APP_MAX_SPRITE_BLOBS][32], owner_sprites[APP_MAX_SPRITE_BLOBS][32];
    int owner_override_count;
    bool stage_output_selected;
    bool draw_copy_selected;
    int selected_draw_copy;
    AppArtifact generated;
    AppArtifact outputs[APP_MAX_OUTPUTS];
    int output_count, selected_output;
    AppArtifact draw_copies[APP_MAX_OUTPUTS];
    int draw_copy_count;
    double artifact_scan_at;
    AppLogLine logs[APP_MAX_LOGS];
    int log_count, log_scroll;
    AppMetadataEvent metadata_events[APP_MAX_METADATA_EVENTS];
    int metadata_event_count;
    off_t metadata_offset;
    off_t metadata_size;
    time_t metadata_mtime;
    char processor_summary[1024], processor_manifest_path[APP_PATH_CAP];
    /* agent: gpt-6.1-sol | 2026-10-06 | Track nanosecond graph refresh | v 3.41.0 | c93c64 */
    long long manifest_mtime;
} AppRun;
/* agent: gpt-6.1-sol | 2026-10-04 | Track per-run resource snapshot | v 3.19.0 | 50a237 */
/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.15.0 | d5f7dd */
/* agent: gpt-6.1-sol | 2026-10-04 | Track run artifact selection and output buffers | v 3.11.0 | 7d27ec */

/* agent: gpt-6.1-sol | 2026-10-04 | Track pending prompt close decision | v 3.2.0 | 9e2977 */
/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.15.0 | d5f7dd */
/* agent: gpt-6.1-sol | 2026-10-04 | Store profile resource state | v 3.20.0 | f0cb7d */
typedef struct {
    char workspace[APP_PATH_CAP], prompt_path[APP_PATH_CAP], profiles_path[APP_PATH_CAP], active_profile_path[APP_PATH_CAP];
    char refs_path[APP_PATH_CAP], runs_path[APP_PATH_CAP], refs_state_path[APP_PATH_CAP];
    char message[512], edit_field[32], field_buffer[512], prompt_edit[APP_TEXT_CAP];
    char font_dir[APP_PATH_CAP], font_state_path[APP_PATH_CAP], ui_font_name[256];
    char font_paths[APP_MAX_FONTS][APP_PATH_CAP];
    int font_count, selected_font;
    int prompt_caret, prompt_anchor, prompt_scroll;
    int field_caret, field_anchor, field_scroll;
    bool field_selecting;
    AppView view;
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
    bool prompt_dirty, prompt_editor_focused, prompt_save_error, prompt_limit_error, prompt_selecting, prompt_close_pending, profile_text_edit, show_full_image, show_compare, show_profile_editor, preview_ready;
    void *draw_state;
    bool draw_close_pending;
    int draw_close_action;
/* agent: gpt-6.1-sol | 2026-10-04 | Track unsaved profile settings baseline | v 3.24.0 | 8baf3b */
    bool settings_view, profile_settings_view, profile_settings_dirty, show_add_menu, show_run_logs;
    AppProfile profile_settings_saved;
    int run_panel;
    AppSpriteEvidence sprite_evidence[APP_MAX_SPRITE_EVIDENCE];
    int sprite_evidence_count, sprite_evidence_stage, sprite_evidence_selected;
    /* agent: gpt-6 | 2026-10-05 | Track selected and hovered sprite stages | v 3.36.0 | 76dcfd */
    int sprite_version_selected, sprite_version_number;
    int sprite_version_hovered, sprite_version_hovered_number;
    /* agent: gpt-6 | 2026-10-05 | Track selected and hovered sprite stages | v 3.36.0 | 76dcfd */
    bool sprite_version_split;
    char sprite_evidence_artifact[APP_PATH_CAP];
    /* agent: gpt-6.1-sol | 2026-10-05 | Cache stage inspection state | v 3.29.0 | c50d50 */
    bool sprite_inspect_enabled;
    /* agent: gpt-6.1-sol | 2026-10-05 | Cache stage inspection state | v 3.29.0 | c50d50 */
    AppSpriteBlob sprite_blobs[APP_MAX_SPRITE_BLOBS];
    int sprite_blob_count, sprite_blob_stage;
    char sprite_blob_sprite_id[32];
    int compare_mode, compare_stage, settings_scroll, reference_scroll;
    char pending_action[64], pending_path[APP_PATH_CAP], rerun_source[APP_PATH_CAP];
    char screenshot_path[APP_PATH_CAP], screenshot_error[256], screenshot_view[24];
    char screenshot_run_id[64], screenshot_artifact[256], screenshot_detail[24];
    bool screenshot_pending, screenshot_expanded;
    int screenshot_previous_expanded;
    bool rerun_pixel_only;
    float image_zoom, image_pan_x, image_pan_y;
    int enet_port;
    void *enet_host, *enet_peer;
    double last_enet_fetch;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
    double texture_cache_checked_at;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
    void *image_loader;
} App;
/* agent: gpt-6.1-sol | 2026-10-05 | Cache stage inspection state | v 3.29.0 | c50d50 */
/* agent: gpt-6.1-sol | 2026-10-04 | Track unsaved profile settings baseline | v 3.24.0 | 8baf3b */
/* agent: gpt-6.1-sol | 2026-10-04 | Track profile settings subview | v 3.21.0 | 0ff512 */
/* agent: gpt-6.1-sol | 2026-10-04 | Store profile resource state | v 3.20.0 | f0cb7d */
/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.15.0 | d5f7dd */
/* agent: gpt-6.1-sol | 2026-10-04 | Index header tabs and flat refs flow | v 3.9.0 | 948d14 */

/* App initialization: initialize raylib-independent workspace and ENet state. */
bool AppInit(App *app, const char *workspace, bool mcp_mode);
/* Release textures, child pipes, and network state owned by the app. */
void AppShutdown(App *app);
/* Poll App* app processes, ENet, artifacts, and APP-08 results; return void. */
void AppUpdate(App *app);
/* Draw the workbench UI using the current raylib frame. */
void AppDraw(App *app);
/* Dispatch UI, ENet, or MCP command text through the shared action handlers. */
const char *AppDispatch(App *app, const char *request, char *response, size_t response_size);
/* Run the MCP stdio client which forwards tool actions to a running GUI over ENet. */
int AppMcpClient(const char *workspace, int port);
/* Return an MCP tool listing and instructions as a JSON document. */
const char *AppMcpToolsJson(void);
/* Save any MCP screenshot queued by AppDispatch after the next complete AppDraw. */
void AppAfterDraw(App *app);

#endif
/* agent: gpt-6.1-sol | 2026-10-05 | Store processor settings and stage debug state | v 3.32.0 | dfd43b */
/* agent: gpt-6 | 2026-10-04 | Read processing events and queue screenshots | v 3.25.0 | 660017 */
/* agent: gpt-6.1-sol | 2026-10-04 | Remove transient details view state | v 3.4.0 | cb703b */
/* agent: gpt-6.1-sol | 2026-10-04 | Track async asset and log loading | v 3.20.0 | d9535e */
/* agent: gpt-6.1-sol | 2026-10-06 | Store branch-aware artifact identity | v 3.41.0 | 116f90 */
/* agent: gpt-6.1-sol | 2026-10-06 | Store graph node execution state | v 3.41.0 | 74a936 */
/* agent: gpt-6.1-sol | 2026-10-06 | Track nanosecond graph refresh | v 3.41.0 | c93c64 */
/* agent: gpt-6.1-sol | 2026-10-06 | Cache typed final sprite previews | v 3.42.0 | c7a014 */
