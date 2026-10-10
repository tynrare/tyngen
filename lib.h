/*
 * File version: 3.75.0
 * Last updated: 2026-10-10
 * Purpose: Shared workbench and processing graph state.
 * Flow META-01 journals, per-sprite stage evidence, and screenshots: lib.c header; Flow APP-01 canonical steps 1-8 and APP-02: lib.c header; APP-03 navigation, APP-04 shared image browsing/trash/live run images/Controls toggle/artifact waterfall, APP-05 resource order/tags, APP-06 drawing, APP-07 profile resources, and APP-08 async image loading: lib.c.
 * Flow APP-06 drawing session and save behavior: lib.c header; APP-07 resource/profile state and APP-08 async loading and texture cache: lib.c header.
 * Flow RIGHT-CLICK-01 active UI actions close newest-first; history is transient App state.
 * Flow PIXEL-02 graph IDs, branch lanes, and artifact geometry: lib.c header; schema and execution: pixelprocess/common.hpp and runner.cpp.
 * Flow BPROBE-01 shared probe contract: AppRun probe state and classification stage_background.cpp; BPROBE-UI-01 session state and controls: lib.c header.
 * Flow APP-04 step 2 connectors use recorded branch parent/fork metadata; node placement remains fixed.
 * Flow MANUAL-GRID-01 diagnosis editing and saves: lib.c header; processor preview and stage flow: pixelprocess/runner.cpp.
 * Flow CLEANUP-01 actions and UI: lib.c header.
 * Flow SPRITE-SELECTION-01 transient selection order and animation controls: lib.c header.
 * Shared invariant: destructive resource moves require explicit confirmation; visible enabled text resources enter payload; editor selection is independent; exactly one profile resource is enabled.
 * agent: gpt-6 | 2026-10-09 | Remove probe toggle state from shared header | v 3.65.0 | 73ba5c
 * agent: gpt-6.1-sol | 2026-10-07 | Track failed sprite evidence | v 3.53.0 | 395d9d
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Add outline settings state | v 3.66.0 | 33f654 */
/* agent: gpt-6.1-sol | 2026-10-09 | Rename shared header guard | v 3.57.0 | 679c2c */

/* agent: gpt-6.1-sol | 2026-10-04 | Index header tabs and flat refs flow | v 3.9.0 | 948d14 */
/* agent: gpt-6.1-sol | 2026-10-04 | Remove optional artifact debug mode | v 3.12.0 | 729d0c */
/* agent: gpt-6.1-sol | 2026-10-04 | Track async asset and log loading | v 3.20.0 | d9535e */
/* agent: gpt-6.1-sol | 2026-10-04 | Track profile settings subview | v 3.21.0 | 0ff512 */
#ifndef TYNGEN_LIB_H
#define TYNGEN_LIB_H

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
/* agent: gpt-6 | 2026-10-09 | Track newest right-click action history | v 3.68.0 | 9f30d7 */
#define APP_MAX_ACTION_HISTORY 16

typedef enum {
    UI_ACTION_GENERATION_DETAIL, UI_ACTION_MENU, UI_ACTION_FIELD, UI_ACTION_PREVIEW, UI_ACTION_CONFIRM,
    UI_ACTION_PROMPT_CLOSE, UI_ACTION_DRAW_CLOSE, UI_ACTION_GRID_EDITOR,
    UI_ACTION_DRAW_EDITOR, UI_ACTION_PROFILE_EDITOR, UI_ACTION_IMAGE_VIEWER,
    UI_ACTION_SETTINGS, UI_ACTION_CONTROLS, UI_ACTION_COMPARE,
    UI_ACTION_SPRITE_INSPECT, UI_ACTION_SPRITE_SELECT
} AppUiAction;
/* agent: gpt-6 | 2026-10-09 | Track newest right-click action history | v 3.68.0 | 9f30d7 */

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
    char sprite_color_source[16], palette_path[APP_PATH_CAP];
    int background_tolerance, grid_cols, grid_rows, extraction_padding;
    int pitch_min, pitch_max, foreground_coverage, cleanup_tolerance, cleanup_background_support, outline_foreground_support, outline_color_delta, sheet_padding;
    char outline_mode[16], outline_color[16];
    int min_sprite_cells, sprite_pitch_tolerance;
    int sheet_colors, sprite_colors;
    /* agent: gpt-6.1-sol | 2026-10-09 | Remove unused alternate pitch setting | v 3.58.0 | 45b3c8 */
} AppPixelSettings;
/* agent: gpt-6.1-sol | 2026-10-09 | Remove unused alternate pitch setting | v 3.58.0 | 45b3c8 */

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
/* agent: gpt-6.1-sol | 2026-10-09 | Add generic tags to resources | v 3.62.0 | fb9b0d */
typedef struct {
    char path[APP_PATH_CAP], name[256], preview[256];
    bool excluded, selected, is_text, is_profile, is_palette;
    /* agent: gpt-6 | 2026-10-09 | Store cached palette card colors | v 3.69.0 | f4ffba */
    Color palette_colors[64];
    int palette_color_count;
    bool palette_text_view;
    char tags[8][32];
    int tag_count, library_order;
    bool loading;
    int profile_index;
    Texture2D thumb, full;
    int width, height;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
    double full_used_at, thumb_used_at;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
} AppReference;
/* agent: gpt-6.1-sol | 2026-10-09 | Add generic tags to resources | v 3.62.0 | fb9b0d */
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
/* agent: gpt-6.1-sol | 2026-10-07 | Track failed sprite evidence | v 3.53.0 | 395d9d */
typedef struct {
    int row, col, x, y, width, height, artifact_width, artifact_height;
    int source_x, source_y, source_width, source_height, source_artifact_width, source_artifact_height;
    /* agent: gpt-6 | 2026-10-09 | Track selected sprites for export | v 3.64.0 | 025a62 */
    bool failed, export_selected;
    char id[32], time[40], summary[512];
} AppSpriteEvidence;
/* agent: gpt-6 | 2026-10-09 | Track selected sprites for export | v 3.64.0 | 025a62 */

/* agent: gpt-6.1-sol | 2026-10-10 | Cache per-stage inspector bounds | v 3.73.0 | adf500 */
typedef struct {
    int x, y, width, height, artifact_width, artifact_height;
    bool available;
} AppSpriteVersionEvidence;
/* agent: gpt-6.1-sol | 2026-10-10 | Cache per-stage inspector bounds | v 3.73.0 | adf500 */
/* agent: gpt-6.1-sol | 2026-10-07 | Track failed sprite evidence | v 3.53.0 | 395d9d */
/* agent: gpt-6 | 2026-10-05 | Record source crop coordinates | v 3.33.0 | f87c25 */

typedef struct {
    char id[32];
    int x, y, width, height, pixels, color[3];
} AppSpriteBlob;
/* agent: gpt-6.1-sol | 2026-10-05 | Cache selected sprite blob details | v 3.28.0 | c830ce */

/* Source-pixel background probe and automatic suppression state. */
typedef struct { char id[32]; int x, y; bool automatic; } AppBackgroundProbe;
/* agent: gpt-6.1-sol | 2026-10-09 | Store editable background probe state | v 3.60.0 | 3d5b93 */

/* agent: gpt-6 | 2026-10-05 | Store per-sprite lattice edits | v 3.33.0 | 25c87a */
typedef struct {
    char id[32];
    AppPixelSettings settings;
} AppSpriteSettings;
/* agent: gpt-6 | 2026-10-05 | Store per-sprite lattice edits | v 3.33.0 | 25c87a */

/* agent: gpt-6.1-sol | 2026-10-06 | Store branch-aware artifact identity | v 3.41.0 | 116f90 */
/* agent: gpt-6.1-sol | 2026-10-06 | Store graph node execution state | v 3.41.0 | 74a936 */
/* agent: gpt-6.1-sol | 2026-10-06 | Persist parent identity for draw edit nodes | v 3.45.0 | 81cdf5 */
/* agent: gpt-6.1-sol | 2026-10-09 | Store processor stage timing | v 3.63.0 | 6e848e */
typedef struct {
    char name[128], path[APP_PATH_CAP];
    /* agent: GPT-6 | 2026-10-09 | Store pinned sprite selection state | v 3.71.0 | c42712 */
    char node_id[96], branch_id[64], role[24], sprite_dir[APP_PATH_CAP];
    char parent_node_id[96];
    char state[24], missing_sprite_ids[256];
    int number, progress;
    double elapsed_seconds, timing_updated_at;
    Texture2D texture, thumb;
    int width, height;
    off_t file_size;
    bool loading;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
    double full_used_at, thumb_used_at;
    /* agent: gpt-6.1-sol | 2026-10-04 | Track texture cache recency | v 3.22.0 | dd84dd */
} AppArtifact;
/* agent: GPT-6 | 2026-10-09 | Store pinned sprite selection state | v 3.71.0 | c42712 */
/* agent: gpt-6.1-sol | 2026-10-09 | Store processor stage timing | v 3.63.0 | 6e848e */

/* agent: gpt-6 | 2026-10-07 | Cache branch origin metadata | v 3.52.0 | 2bbeec */
typedef struct {
    char id[64], parent[64], fork_step[48], label[128];
    bool provider_only;
} AppGraphBranch;
/* agent: gpt-6 | 2026-10-07 | Cache branch origin metadata | v 3.52.0 | 2bbeec */

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
    AppGraphBranch graph_branches[APP_MAX_STAGES];
    int graph_branch_count;
    /* agent: gpt-6.1-sol | 2026-10-06 | Cache edited sprite waterfall nodes | v 3.45.0 | 31d50e */
    /* Cache loaded crops by sprite, graph stage, edit node, and final output. */
    AppArtifact sprite_version_previews[APP_MAX_STAGES + APP_MAX_OUTPUTS * 2 + 1];
    int sprite_version_preview_count;
    char sprite_version_preview_id[32];
    /* agent: gpt-6.1-sol | 2026-10-10 | Cache per-stage inspector bounds | v 3.73.0 | adf500 */
    AppSpriteVersionEvidence sprite_version_evidence[APP_MAX_STAGES + APP_MAX_OUTPUTS * 2 + 1];
    int sprite_version_evidence_count;
    char sprite_version_evidence_id[32];
    long long sprite_version_evidence_mtime, sprite_version_evidence_size;
    /* agent: gpt-6.1-sol | 2026-10-10 | Cache per-stage inspector bounds | v 3.73.0 | adf500 */
    /* agent: gpt-6.1-sol | 2026-10-06 | Cache edited sprite waterfall nodes | v 3.45.0 | 31d50e */
    AppArtifact debug_stages[APP_MAX_STAGES];
    int debug_stage_count;
    bool selected_stage_debug;
    AppPixelSettings pixel_settings, pixel_settings_saved, sprite_settings;
    bool pixel_settings_dirty, sprite_settings_override, pixel_sprite_scope, pixel_settings_loaded;
    char selected_sprite_id[32], pixel_target[24], pixel_sprite[32], processing_settings_path[APP_PATH_CAP];
    char pixel_branch_id[64], pixel_save_mode[16];
    /* agent: gpt-6.1-sol | 2026-10-07 | Track pixel rerun intent | v 3.48.0 | 7e8fd4 */
    bool pixel_force_fresh, pixel_branch_revision;
    /* agent: gpt-6 | 2026-10-07 | Track graph revision preconditions | v 3.56.0 | a3c284 */
    int pixel_expected_revision;
    bool pixel_expected_revision_set;
    /* agent: gpt-6 | 2026-10-07 | Track graph revision preconditions | v 3.56.0 | a3c284 */
    /* agent: gpt-6.1-sol | 2026-10-07 | Track pixel rerun intent | v 3.48.0 | 7e8fd4 */
    AppSpriteSettings sprite_overrides[APP_MAX_SPRITE_EVIDENCE];
    int sprite_override_count;
    char owner_components[APP_MAX_SPRITE_BLOBS][32], owner_sprites[APP_MAX_SPRITE_BLOBS][32];
    int owner_override_count;
    AppBackgroundProbe background_probes[APP_MAX_SPRITE_BLOBS];
    int background_probe_count;
    char disabled_background_probes[APP_MAX_SPRITE_BLOBS][32];
    int disabled_background_probe_count;
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
/* agent: gpt-6 | 2026-10-09 | Track operation identity in run state | v 3.72.0 | d8e5ba */
/* agent: gpt-6 | 2026-10-09 | Store live operation refresh state | v 3.69.0 | bea75d */
    off_t live_event_offset;
    double live_event_scan_at;
    bool live_operation_active, live_refresh_needed;
    char live_operation_id[80];
    /* agent: gpt-6 | 2026-10-09 | Store live operation refresh state | v 3.69.0 | bea75d */
    off_t metadata_size;
    time_t metadata_mtime;
    char processor_summary[1024], processor_manifest_path[APP_PATH_CAP];
    /* agent: gpt-6 | 2026-10-07 | Track graph history selection | v 3.55.0 | ee0103 */
    char history_paths[32][APP_PATH_CAP];
    int history_count, history_selected;
    bool history_read_only, history_truncated;
    /* agent: gpt-6 | 2026-10-07 | Track graph history selection | v 3.55.0 | ee0103 */
    /* agent: gpt-6.1-sol | 2026-10-06 | Track nanosecond graph refresh | v 3.41.0 | c93c64 */
    long long manifest_mtime;
} AppRun;
/* agent: gpt-6.1-sol | 2026-10-04 | Track per-run resource snapshot | v 3.19.0 | 50a237 */
/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.15.0 | d5f7dd */
/* agent: gpt-6.1-sol | 2026-10-04 | Track run artifact selection and output buffers | v 3.11.0 | 7d27ec */

/* agent: gpt-6.1-sol | 2026-10-04 | Track pending prompt close decision | v 3.2.0 | 9e2977 */
/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.15.0 | d5f7dd */
/* agent: gpt-6.1-sol | 2026-10-04 | Store profile resource state | v 3.20.0 | f0cb7d */
/* agent: gpt-6.1-sol | 2026-10-09 | Track library manifest and drag state | v 3.62.0 | 92f93f */
typedef struct {
    char workspace[APP_PATH_CAP], prompt_path[APP_PATH_CAP], profiles_path[APP_PATH_CAP], active_profile_path[APP_PATH_CAP];
    char refs_path[APP_PATH_CAP], runs_path[APP_PATH_CAP], library_manifest_path[APP_PATH_CAP];
    char message[512], edit_field[32], field_buffer[512], prompt_edit[APP_TEXT_CAP];
    char font_dir[APP_PATH_CAP], font_state_path[APP_PATH_CAP], ui_font_name[256];
    char font_paths[APP_MAX_FONTS][APP_PATH_CAP];
    /* agent: gpt-6.1-sol | 2026-10-07 | Store cleanup preferences in app state | v 3.49.0 | cba857 */
    char cleanup_settings_path[APP_PATH_CAP];
    long long cleanup_backup_limit;
    int font_count, selected_font;
    int prompt_caret, prompt_anchor, prompt_scroll;
    int field_caret, field_anchor, field_scroll;
    bool field_selecting;
    bool cleanup_backups_enabled;
    /* agent: gpt-6 | 2026-10-09 | Store transient action history state | v 3.68.0 | 5913d5 */
    AppUiAction action_history[APP_MAX_ACTION_HISTORY];
    int action_history_count;
    /* agent: gpt-6 | 2026-10-09 | Store transient action history state | v 3.68.0 | 5913d5 */
    /* agent: gpt-6.1-sol | 2026-10-07 | Store cleanup preferences in app state | v 3.49.0 | cba857 */
    AppView view;
    AppProfile profiles[APP_MAX_PROFILES];
    int profile_count, active_profile, profile_edit_index;
    int active_prompt_part, active_set_part;
    AppReference refs[APP_MAX_REFS];
    int ref_count, selected_ref, thumb_size;
    AppRun runs[APP_MAX_RUNS];
/* agent: GPT-6 | 2026-10-10 | Allow concurrent independent generation and processing | v 3.75.0 | e2e5fb */
    int run_count, selected_run;
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
    void *grid_edit_state;
/* agent: gpt-6.1-sol | 2026-10-04 | Track unsaved profile settings baseline | v 3.24.0 | 8baf3b */
    bool settings_view, profile_settings_view, profile_settings_dirty, show_add_menu, show_run_logs;
    bool show_hidden_resources, reference_drag_active, reference_dragging;
    int reference_drag_index;
    Vector2 reference_drag_start;
    AppProfile profile_settings_saved;
    int run_panel;
    AppSpriteEvidence sprite_evidence[APP_MAX_SPRITE_EVIDENCE];
    int sprite_evidence_count, sprite_evidence_stage, sprite_evidence_selected;
    /* agent: gpt-6 | 2026-10-05 | Track selected and hovered sprite stages | v 3.36.0 | 76dcfd */
    int sprite_version_selected, sprite_version_number;
    int sprite_version_hovered, sprite_version_hovered_number;
    /* agent: gpt-6 | 2026-10-05 | Track selected and hovered sprite stages | v 3.36.0 | 76dcfd */
    bool sprite_version_split;
    /* agent: gpt-6.1-sol | 2026-10-06 | Track inherited sprite evidence paths | v 3.45.0 | 0a5287 */
    char sprite_evidence_artifact[APP_PATH_CAP];
    char sprite_evidence_data_artifact[APP_PATH_CAP];
    /* agent: gpt-6.1-sol | 2026-10-05 | Cache stage inspection state | v 3.29.0 | c50d50 */
    bool sprite_inspect_enabled;
    /* agent: gpt-6 | 2026-10-09 | Remove probe toggle state from shared header | v 3.65.0 | 73ba5c */
    /* agent: gpt-6 | 2026-10-09 | Store sprite selection mode | v 3.64.0 | 6f0a76 */
    /* agent: GPT-6 | 2026-10-10 | Track ordered sprite animation | v 3.74.0 | 916a56 */
    bool sprite_select_enabled;
    int sprite_selection_order[APP_MAX_SPRITE_EVIDENCE], sprite_selection_count;
    bool sprite_selection_animation;
    int sprite_selection_fps, sprite_selection_frame, sprite_selection_scroll;
    double sprite_selection_frame_started;
    float sprite_animation_zoom, sprite_animation_pan_x, sprite_animation_pan_y;
    /* agent: GPT-6 | 2026-10-10 | Track ordered sprite animation | v 3.74.0 | 916a56 */
    char sprite_selection_parent_node_id[96], sprite_selection_source_node_id[96];
    char sprite_selection_branch_id[64], sprite_selection_source_role[24];
    int sprite_selection_revision, sprite_selection_stage;
    /* agent: gpt-6 | 2026-10-09 | Remove probe toggle state from shared header | v 3.65.0 | 73ba5c */
    /* agent: gpt-6 | 2026-10-09 | Store sprite selection mode | v 3.64.0 | 6f0a76 */
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
    /* agent: gpt-6.1-sol | 2026-10-07 | Store cleanup worker state | v 3.51.0 | 9361bc */
    void *cleanup_worker;
    /* agent: gpt-6.1-sol | 2026-10-07 | Store cleanup worker state | v 3.51.0 | 9361bc */
} App;
/* agent: gpt-6.1-sol | 2026-10-09 | Track profile editing separately from activation | v 3.68.0 | 9107de */
/* agent: gpt-6.1-sol | 2026-10-09 | Track library manifest and drag state | v 3.62.0 | 92f93f */
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
/* agent: GPT-6 | 2026-10-10 | Allow concurrent independent generation and processing | v 3.75.0 | e2e5fb */
/* agent: gpt-6.1-sol | 2026-10-05 | Store processor settings and stage debug state | v 3.32.0 | dfd43b */
/* agent: gpt-6 | 2026-10-04 | Read processing events and queue screenshots | v 3.25.0 | 660017 */
/* agent: gpt-6.1-sol | 2026-10-04 | Remove transient details view state | v 3.4.0 | cb703b */
/* agent: gpt-6.1-sol | 2026-10-04 | Track async asset and log loading | v 3.20.0 | d9535e */
/* agent: gpt-6.1-sol | 2026-10-06 | Store branch-aware artifact identity | v 3.41.0 | 116f90 */
/* agent: gpt-6.1-sol | 2026-10-06 | Store graph node execution state | v 3.41.0 | 74a936 */
/* agent: gpt-6.1-sol | 2026-10-06 | Track nanosecond graph refresh | v 3.41.0 | c93c64 */
/* agent: gpt-6.1-sol | 2026-10-06 | Persist parent identity for draw edit nodes | v 3.45.0 | 81cdf5 */
/* agent: gpt-6.1-sol | 2026-10-06 | Cache edited sprite waterfall nodes | v 3.45.0 | 31d50e */
/* agent: gpt-6.1-sol | 2026-10-06 | Track inherited sprite evidence paths | v 3.45.0 | 0a5287 */
/* agent: gpt-6.1-sol | 2026-10-09 | Store editable background probe state | v 3.60.0 | 3d5b93 */
/* agent: gpt-6.1-sol | 2026-10-09 | Add outline settings state | v 3.66.0 | 33f654 */
/* agent: gpt-6 | 2026-10-09 | Track operation identity in run state | v 3.72.0 | d8e5ba */
