/*
 * File version: 3.3.0
 * Last updated: 2026-10-04
 * Purpose: Workbench persistence, processes, shared actions, and interface.
 * Flow ID APP-01
 * 1. User/agent -> choose References, Progress, or Output -> show one primary content view; prompt files live in References.
 * 2. User -> left-click reference -> open viewer; right-click -> close viewer; exclude icon -> omit image or prompt from payload; trash icon -> confirm move item to workspace/trash; Details or Settings -> edit contextual actions or profile configuration.
 * 3. User/agent -> prepare then explicitly confirm -> immutable run starts generation then pixel processing.
 * 4. Child process -> report output and diagnostics -> Progress details and Output reflect observed files.
 * 5. ENet/MCP -> dispatch the same action -> return current state or action result.
 * 6. User -> import TTF/OTF from any folder or select a stored font -> load 32px atlas; empty selection -> raylib default; persist per workspace.
 * 7. User -> plus card -> import image/text or create prompt; text cards show filename/excerpt; select -> edit; Ctrl+Enter or close saves; enabled text refs become repeated -p payloads and enabled images repeated -i payloads; trash -> confirm move and close editor if needed.
 * Invariant: reference trash moves require confirmation; trash is outside the scanned refs tree.
 * Invariant: the raylib default is active unless a stored custom font is selected; generation uses all enabled text refs, independently of editor selection.
 * Flow ID APP-02
 * 1. User -> click image or text reference filename -> edit basename while viewer stays open.
 * 2. User -> press Enter -> save open text edits when needed, preserve file extension, reject empty names/collisions, rename reference and update workspace state.
 * 3. User -> press Escape -> discard filename edits; outside/right-click on add tile -> close its menu.
 * Invariant: an empty prompt collection stays empty; reference renames preserve extension and never overwrite files.
 */

/* agent: gpt-6.1-sol | 2026-10-04 | Support image and text renaming | v 3.3.0 | 204819 */
#define ENET_IMPLEMENTATION
#include "enet.h"
#include "lib.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ENET_CHANNEL 0
#define MCP_FRAME_CAP 32768
#define UI_BG (Color){24, 25, 29, 255}
#define UI_PANEL (Color){34, 36, 42, 255}
#define UI_PANEL_ALT (Color){43, 45, 53, 255}
#define UI_TEXT (Color){228, 230, 235, 255}
#define UI_MUTED (Color){153, 158, 168, 255}
#define UI_ACCENT (Color){115, 181, 255, 255}
#define UI_WARN (Color){240, 174, 92, 255}
#define UI_BAD (Color){240, 106, 115, 255}
#define UI_GOOD (Color){116, 206, 142, 255}

static bool s_mcp_mode;
static char s_action_response[APP_TEXT_CAP];
static Shader s_image_shader;
static Font s_ui_font;
static bool file_exists(const char *path);
static bool button(Rectangle rect, const char *label, bool active);

/* agent: gpt-6.1-sol | 2026-10-04 | Load standard sans UI font | v 3.3.0 | 65170e */
/* Load a selected TTF/OTF at a 32px atlas size; return Font or zero on failure. */
static Font load_ui_font(const char *path) {
    if (!path || !file_exists(path)) return (Font){0};
    Font font = LoadFontEx(path, 32, NULL, 0);
    if (font.texture.id) SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Load standard sans UI font | v 3.3.0 | 65170e */

/* agent: gpt-6.1-sol | 2026-10-04 | Render text with sans UI font | v 3.3.0 | fffba1 */
/* Draw readable UI text with int position/size and Color; return void. */
static void ui_draw_text(const char *text, int x, int y, int size, Color color) {
    if (s_ui_font.texture.id) DrawTextEx(s_ui_font, text, (Vector2){(float)x, (float)y}, (float)size, 0, color);
    else DrawText(text, x, y, size, color);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Render text with sans UI font | v 3.3.0 | fffba1 */

/* agent: gpt-6.1-sol | 2026-10-04 | Measure text with sans UI font | v 3.3.0 | da1d14 */
/* Measure UI text with int size; return pixel width as int. */
static int ui_measure_text(const char *text, int size) {
    if (s_ui_font.texture.id) return (int)MeasureTextEx(s_ui_font, text, (float)size, 0).x;
    return MeasureText(text, size);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Measure text with sans UI font | v 3.3.0 | da1d14 */

/* Extract a JSON scalar ID without converting its original numeric/string form. */
static bool json_raw(const char *json, const char *key, char *out, size_t cap) {
    char needle[128]; snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle); if (!p || !(p = strchr(p + strlen(needle), ':'))) return false;
    while (*++p == ' ' || *p == '\t') {}
    size_t n = 0;
    if (*p == '"') { out[n++] = *p++; while (*p && *p != '"' && n + 2 < cap) { if (*p == '\\' && p[1]) out[n++] = *p++; out[n++] = *p++; } if (*p == '"' && n + 1 < cap) out[n++] = *p; }
    else while (*p && *p != ',' && *p != '}' && n + 1 < cap) out[n++] = *p++;
    out[n] = 0; return n > 0;
}

/* Copy a bounded string into a fixed destination and terminate it. */
static void copy_text(char *dst, size_t cap, const char *src) {
    if (!cap) return;
    snprintf(dst, cap, "%s", src ? src : "");
}

/* Return true when a path names a regular file. */
static bool file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Create a directory and its parents; return false on an unusable path. */
static bool make_dirs(const char *path) {
    char tmp[APP_PATH_CAP];
    copy_text(tmp, sizeof(tmp), path);
    size_t n = strlen(tmp);
    if (!n || n >= sizeof(tmp)) return false;
    for (size_t i = 1; i < n; i++) {
        if (tmp[i] != '/') continue;
        tmp[i] = 0;
        if (mkdir(tmp, 0775) && errno != EEXIST) return false;
        tmp[i] = '/';
    }
    if (mkdir(tmp, 0775) && errno != EEXIST) return false;
    return true;
}

/* Join two path components while keeping the result bounded. */
static void path_join(char *dst, size_t cap, const char *a, const char *b) {
    snprintf(dst, cap, "%s/%s", a ? a : "", b ? b : "");
}

/* Read a workspace text file into a terminated buffer. */
static bool read_text(const char *path, char *dst, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(dst, 1, cap - 1, f);
    dst[n] = 0;
    fclose(f);
    return true;
}

/* Write text through a sibling temporary file to avoid partial profile edits. */
static bool write_text(const char *path, const char *text) {
    char tmp[APP_PATH_CAP];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    size_t n = strlen(text);
    bool ok = fwrite(text, 1, n, f) == n && fflush(f) == 0;
    if (fclose(f)) ok = false;
    if (ok && rename(tmp, path)) ok = false;
    if (!ok) unlink(tmp);
    return ok;
}

/* Copy a dropped reference into the active workspace without replacing a file. */
static bool copy_file_new(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }
    char buf[16384];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
    }
    if (ferror(in)) ok = false;
    if (fclose(out)) ok = false;
    if (fclose(in)) ok = false;
    if (!ok) unlink(dst);
    return ok;
}

/* Open one path using the desktop file handler without invoking a shell. */
static bool open_path(const char *path) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) { execlp("xdg-open", "xdg-open", path, (char *)NULL); _exit(127); }
    return true;
}

/* Choose a file using char* out, size_t cap, bool filters; return bool. */
static bool choose_file(char *out, size_t cap, bool image_only, bool font_only) {
    int pipes[2]; if (pipe(pipes)) return false;
    pid_t pid = fork(); if (pid < 0) { close(pipes[0]); close(pipes[1]); return false; }
    if (pid == 0) {
        close(pipes[0]); dup2(pipes[1],STDOUT_FILENO); close(pipes[1]);
        if (image_only) execlp("zenity","zenity","--file-selection","--file-filter=Images and prompts | *.png *.jpg *.jpeg *.webp *.bmp *.txt",(char *)NULL);
        else if (font_only) execlp("zenity","zenity","--file-selection","--file-filter=Fonts | *.ttf *.otf",(char *)NULL);
        else execlp("zenity","zenity","--file-selection","--file-filter=Text | *.txt",(char *)NULL);
        _exit(127);
    }
    close(pipes[1]); FILE *f = fdopen(pipes[0],"r"); if (!f) { close(pipes[0]); waitpid(pid,NULL,0); return false; }
    bool ok = fgets(out,(int)cap,f) != NULL; fclose(f); int status=0; waitpid(pid,&status,0);
    if (!ok || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return false;
    size_t n=strlen(out); while(n && (out[n-1]=='\n'||out[n-1]=='\r')) out[--n]=0;
    return n > 0;
}

/* Resolve a selected regular file and reject paths outside the active workspace. */
static bool workspace_file(App *app, const char *input, char *resolved, size_t cap) {
    if (!realpath(input,resolved) || !file_exists(resolved)) return false;
    size_t n=strlen(app->workspace);
    return !strncmp(resolved,app->workspace,n) && resolved[n]=='/';
}

/* Return true for supported raster reference file extensions. */
static bool is_image_path(const char *p) {
    const char *dot = strrchr(p, '.');
    return dot && (!strcasecmp(dot, ".png") || !strcasecmp(dot, ".jpg") ||
                   !strcasecmp(dot, ".jpeg") || !strcasecmp(dot, ".webp") ||
                   !strcasecmp(dot, ".bmp"));
}

/* agent: gpt-6.1-sol | 2026-10-04 | Add text prompts to references | v 3.3.0 | 89d8ea */
/* Return true for a .txt prompt path; return bool. */
static bool is_text_path(const char *p) {
    const char *dot = strrchr(p, '.');
    return dot && !strcasecmp(dot, ".txt");
}

/* agent: gpt-6.1-sol | 2026-10-04 | Cache prompt previews in reference cards | v 3.3.0 | b9e284 */
/* Cache a normalized 255-byte excerpt from const char *text into AppReference*; return void. */
static void cache_prompt_preview(AppReference *ref, const char *text) {
    size_t out = 0;
    for (size_t i = 0; text[i] && out + 1 < sizeof(ref->preview); i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c >= 32) ref->preview[out++] = (char)c;
    }
    ref->preview[out] = 0;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Cache prompt previews in reference cards | v 3.3.0 | b9e284 */

/* Extract one JSON string value using bounded unescaping. */
static bool json_string(const char *json, const char *key, char *out, size_t cap) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p || !(p = strchr(p + strlen(needle), ':'))) return false;
    while (*++p == ' ' || *p == '\t') {}
    if (*p++ != '"') return false;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') out[n++] = '\n';
            else if (*p == 'r') out[n++] = '\r';
            else if (*p == 't') out[n++] = '\t';
            else out[n++] = *p;
        } else out[n++] = *p;
        p++;
    }
    out[n] = 0;
    return true;
}

/* Escape a string for compact JSON responses. */
static void json_escape(const char *src, char *dst, size_t cap) {
    size_t n = 0;
    if (!cap) return;
    for (; *src && n + 2 < cap; src++) {
        unsigned char c = (unsigned char)*src;
        if (c == '"' || c == '\\') { dst[n++] = '\\'; dst[n++] = (char)c; }
        else if (c == '\n') { dst[n++] = '\\'; dst[n++] = 'n'; }
        else if (c == '\r') { dst[n++] = '\\'; dst[n++] = 'r'; }
        else if (c == '\t') { dst[n++] = '\\'; dst[n++] = 't'; }
        else if (c >= 32) dst[n++] = (char)c;
    }
    dst[n] = 0;
}

/* Set a concise message shown in the app and included in agent responses. */
static void set_message(App *app, const char *message) {
    copy_text(app->message, sizeof(app->message), message);
}

/* Append a timestamped process line to a run's bounded in-memory log. */
static void add_log(AppRun *run, const char *process, const char *line) {
    if (run->log_count >= APP_MAX_LOGS) {
        memmove(run->logs, run->logs + 1, sizeof(run->logs[0]) * (APP_MAX_LOGS - 1));
        run->log_count--;
    }
    AppLogLine *entry = &run->logs[run->log_count++];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(entry->time, sizeof(entry->time), "%H:%M:%S", &tm_now);
    copy_text(entry->process, sizeof(entry->process), process);
    copy_text(entry->text, sizeof(entry->text), line);
    char log_path[APP_PATH_CAP]; path_join(log_path, sizeof(log_path), run->dir, "process.log");
    FILE *f = fopen(log_path, "a"); if (f) { fprintf(f, "%s [%s] %s\n", entry->time, process, line); fclose(f); }
    if (strstr(line, "error") || strstr(line, "failed")) copy_text(run->current_step, sizeof(run->current_step), "error reported");
    else if (strstr(line, "stage-") || strstr(line, "decision ") || strstr(line, "post-processing"))
        copy_text(run->current_step, sizeof(run->current_step), line);
}

/* Parse a profile INI file, retaining only the known bounded settings. */
static void load_profiles(App *app) {
    FILE *f = fopen(app->profiles_path, "r");
    if (!f) return;
    char line[1024];
    AppProfile *p = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *v = line;
        while (*v == ' ' || *v == '\t') v++;
        size_t len = strlen(v);
        while (len && (v[len - 1] == '\n' || v[len - 1] == '\r')) v[--len] = 0;
        if (!*v || *v == '#' || *v == ';') continue;
        if (*v == '[') {
            if (app->profile_count >= APP_MAX_PROFILES) { p = NULL; continue; }
            p = &app->profiles[app->profile_count++];
            memset(p, 0, sizeof(*p));
            char *name = strstr(v, "profile:");
            if (name) { name += 8; char *end = strchr(name, ']'); if (end) *end = 0; copy_text(p->name, sizeof(p->name), name); }
            continue;
        }
        if (!p) continue;
        char *eq = strchr(v, '=');
        if (!eq) continue;
        *eq++ = 0;
        if (!strcmp(v, "model")) copy_text(p->model, sizeof(p->model), eq);
        else if (!strcmp(v, "aspect")) copy_text(p->aspect, sizeof(p->aspect), eq);
        else if (!strcmp(v, "layout")) copy_text(p->layout, sizeof(p->layout), eq);
        else if (!strcmp(v, "pixel_pitch")) copy_text(p->pixel_pitch, sizeof(p->pixel_pitch), eq);
        else if (!strcmp(v, "pitch_preset")) copy_text(p->pitch_preset, sizeof(p->pitch_preset), eq);
        else if (!strcmp(v, "pitch_group_layout")) copy_text(p->pitch_group_layout, sizeof(p->pitch_group_layout), eq);
        else if (!strcmp(v, "align_blobs")) copy_text(p->align_blobs, sizeof(p->align_blobs), eq);
        else if (!strcmp(v, "magenta")) copy_text(p->magenta, sizeof(p->magenta), eq);
        else if (!strcmp(v, "edge_alpha")) copy_text(p->edge_alpha, sizeof(p->edge_alpha), eq);
        else if (!strcmp(v, "slim_upload")) p->slim_upload = atoi(eq) != 0;
        else if (!strcmp(v, "rearrange")) p->rearrange = atoi(eq) != 0;
        else if (!strcmp(v, "padding")) p->padding = atoi(eq);
        else if (!strcmp(v, "tile_size") && p->tile_count < 8) p->tile_sizes[p->tile_count++] = atoi(eq);
        else if (!strcmp(v, "set") && p->set_count < APP_MAX_PROMPTS) copy_text(p->sets[p->set_count++], sizeof(p->sets[0]), eq);
        else if (!strcmp(v, "prompt") && p->prompt_count < APP_MAX_PROMPTS) copy_text(p->prompts[p->prompt_count++], sizeof(p->prompts[0]), eq);
    }
    fclose(f);
    if (app->profile_count) app->active_profile = 0;
}

/* Persist known profile fields in the workspace INI file. */
static bool save_profiles(App *app) {
    char out[32768];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof(out) - n, "; aigen workbench profiles | workspace-local\n");
    for (int i = 0; i < app->profile_count && n < sizeof(out) - 2048; i++) {
        AppProfile *p = &app->profiles[i];
        n += (size_t)snprintf(out + n, sizeof(out) - n,
            "\n[profile:%s]\nmodel=%s\naspect=%s\nlayout=%s\npixel_pitch=%s\npitch_preset=%s\npitch_group_layout=%s\nalign_blobs=%s\nmagenta=%s\nedge_alpha=%s\nslim_upload=%d\nrearrange=%d\npadding=%d\n",
            p->name, p->model, p->aspect, p->layout, p->pixel_pitch, p->pitch_preset,
            p->pitch_group_layout, p->align_blobs, p->magenta, p->edge_alpha, p->slim_upload, p->rearrange, p->padding);
        for (int k = 0; k < p->tile_count; k++) n += (size_t)snprintf(out + n, sizeof(out) - n, "tile_size=%d\n", p->tile_sizes[k]);
        for (int k = 0; k < p->set_count; k++) n += (size_t)snprintf(out + n, sizeof(out) - n, "set=%s\n", p->sets[k]);
        for (int k = 0; k < p->prompt_count; k++) n += (size_t)snprintf(out + n, sizeof(out) - n, "prompt=%s\n", p->prompts[k]);
    }
    return write_text(app->profiles_path, out);
}

/* Write the initial aigen1-equivalent profile when a workspace has none. */
static void seed_profile(App *app) {
    if (file_exists(app->profiles_path) && app->profile_count) return;
    AppProfile *p = &app->profiles[0];
    memset(p, 0, sizeof(*p));
    copy_text(p->name, sizeof(p->name), "Default");
    copy_text(p->model, sizeof(p->model), "openai/gpt-image-2.5-flare");
    copy_text(p->aspect, sizeof(p->aspect), "1:1");
    copy_text(p->layout, sizeof(p->layout), "auto");
    copy_text(p->pixel_pitch, sizeof(p->pixel_pitch), "auto");
    copy_text(p->pitch_preset, sizeof(p->pitch_preset), "balanced");
    copy_text(p->pitch_group_layout, sizeof(p->pitch_group_layout), "compact");
    copy_text(p->align_blobs, sizeof(p->align_blobs), "auto");
    copy_text(p->magenta, sizeof(p->magenta), "outside");
    copy_text(p->edge_alpha, sizeof(p->edge_alpha), "legacy");
    p->slim_upload = 1;
    p->rearrange = 1;
    p->padding = 0;
    p->tile_sizes[0] = 16; p->tile_sizes[1] = 32; p->tile_count = 2;
    p->sets[p->set_count][0] = 0; copy_text(p->sets[p->set_count++], sizeof(p->sets[0]), "background=auto");
    copy_text(p->sets[p->set_count++], sizeof(p->sets[0]), "output_format=png");
    copy_text(p->sets[p->set_count++], sizeof(p->sets[0]), "moderation=low");
    copy_text(p->sets[p->set_count++], sizeof(p->sets[0]), "quality=low");
    copy_text(p->prompts[p->prompt_count++], sizeof(p->prompts[0]), "4x4 spritesheet grid. No labels.");
    copy_text(p->prompts[p->prompt_count++], sizeof(p->prompts[0]), "do not copy reference images. only apply style");
    copy_text(p->prompts[p->prompt_count++], sizeof(p->prompts[0]), "magic pink background.");
    app->profile_count = 1;
    app->active_profile = 0;
    save_profiles(app);
}

/* agent: gpt-6.1-sol | 2026-10-04 | Read exclusion preferences | v 3.3.0 | e7f5ef */
/* Load legacy hidden and current exclude flags for App* app; return void. */
static void load_reference_state(App *app) {
    FILE *f = fopen(app->refs_state_path, "r");
    if (!f) return;
    char path[APP_PATH_CAP]; int legacy_hidden, excluded;
    while (fscanf(f, "%d %d %1023[^\n]\n", &legacy_hidden, &excluded, path) == 3) {
        (void)legacy_hidden;
        for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, path)) {
            app->refs[i].excluded = excluded != 0;
        }
    }
    fclose(f);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Read exclusion preferences | v 3.3.0 | e7f5ef */

/* agent: gpt-6.1-sol | 2026-10-04 | Write exclusion preferences | v 3.3.0 | 3eb24c */
/* Persist only generation exclusion flags for App* app; return void. */
static void save_reference_state(App *app) {
    FILE *f = fopen(app->refs_state_path, "w");
    if (!f) return;
    for (int i = 0; i < app->ref_count; i++) fprintf(f, "0 %d %s\n", app->refs[i].excluded, app->refs[i].path);
    fclose(f);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Write exclusion preferences | v 3.3.0 | 3eb24c */

/* agent: gpt-6.1-sol | 2026-10-04 | Draw reference corner actions | v 3.3.0 | fd5ac8 */
/* Draw two compact corner actions for an AppReference; return void. */
static void draw_reference_icons(AppReference *ref, Rectangle box) {
    Rectangle exclude = {box.x + box.width - 58, box.y + 6, 24, 24};
    Rectangle trash = {box.x + box.width - 28, box.y + 6, 22, 24};
    bool over_exclude = CheckCollisionPointRec(GetMousePosition(), exclude);
    bool over_trash = CheckCollisionPointRec(GetMousePosition(), trash);
    DrawRectangleRec(exclude, (Color){20, 22, 26, 225});
    DrawRectangleRec(trash, (Color){20, 22, 26, 225});
    DrawCircleLines((int)(exclude.x + 12), (int)(exclude.y + 12), 7, over_exclude ? UI_TEXT : UI_MUTED);
    if (ref->excluded) DrawLine((int)exclude.x + 5, (int)exclude.y + 19, (int)exclude.x + 19, (int)exclude.y + 5, UI_BAD);
    else {
        DrawLine((int)exclude.x + 5, (int)exclude.y + 12, (int)exclude.x + 10, (int)exclude.y + 17, UI_GOOD);
        DrawLine((int)exclude.x + 10, (int)exclude.y + 17, (int)exclude.x + 19, (int)exclude.y + 7, UI_GOOD);
    }
    DrawLine((int)trash.x + 5, (int)trash.y + 8, (int)trash.x + 17, (int)trash.y + 8, over_trash ? UI_BAD : UI_MUTED);
    DrawLine((int)trash.x + 7, (int)trash.y + 10, (int)trash.x + 8, (int)trash.y + 19, over_trash ? UI_BAD : UI_MUTED);
    DrawLine((int)trash.x + 15, (int)trash.y + 10, (int)trash.x + 14, (int)trash.y + 19, over_trash ? UI_BAD : UI_MUTED);
    DrawLine((int)trash.x + 8, (int)trash.y + 19, (int)trash.x + 14, (int)trash.y + 19, over_trash ? UI_BAD : UI_MUTED);
    DrawLine((int)trash.x + 9, (int)trash.y + 5, (int)trash.x + 13, (int)trash.y + 5, over_trash ? UI_BAD : UI_MUTED);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Draw reference corner actions | v 3.3.0 | fd5ac8 */

/* agent: gpt-6.1-sol | 2026-10-04 | Move references into trash | v 3.3.0 | fc93f0 */
/* Move a reference into workspace/trash with a collision-safe name; return bool. */
static bool move_reference_to_trash(App *app, const char *path) {
    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    char trash_dir[APP_PATH_CAP], target[APP_PATH_CAP];
    path_join(trash_dir, sizeof(trash_dir), app->workspace, "trash");
    if (!make_dirs(trash_dir)) return false;
    path_join(target, sizeof(target), trash_dir, base);
    struct stat st;
    if (lstat(target, &st) == 0) {
        const char *dot = strrchr(base, '.'); size_t stem = dot ? (size_t)(dot - base) : strlen(base);
        bool available = false;
        for (int n = 1; n < 1000; n++) {
            char unique[256]; snprintf(unique, sizeof(unique), "%.*s-%d%s", (int)stem, base, n, dot ? dot : "");
            path_join(target, sizeof(target), trash_dir, unique);
            if (lstat(target, &st) && errno == ENOENT) { available = true; break; }
        }
        if (!available) return false;
    }
    if (link(path, target)) return false;
    if (unlink(path)) { unlink(target); return false; }
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Move references into trash | v 3.3.0 | fc93f0 */

/* agent: gpt-6.1-sol | 2026-10-04 | Load pixelated reference thumbnails | v 3.3.0 | d5c68f */
/* Load full and pixelated fitted textures from const char *path into App*; return void. */
static void add_reference(App *app, const char *path) {
    if (app->ref_count >= APP_MAX_REFS || (!is_image_path(path) && !is_text_path(path))) return;
    for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, path)) return;
    AppReference *r = &app->refs[app->ref_count++];
    memset(r, 0, sizeof(*r));
    copy_text(r->path, sizeof(r->path), path);
    const char *base = strrchr(path, '/');
    copy_text(r->name, sizeof(r->name), base ? base + 1 : path);
    r->selected = true;
    r->is_text = is_text_path(path);
    if (r->is_text) {
        char content[APP_TEXT_CAP];
        if (read_text(path, content, sizeof(content))) cache_prompt_preview(r, content);
        return;
    }
    r->full = LoadTexture(path);
    if (r->full.id) SetTextureFilter(r->full,TEXTURE_FILTER_POINT);
    Image image = LoadImage(path);
    if (image.data) {
        r->width = image.width; r->height = image.height;
        int thumb_w = image.width > image.height ? 160 : (int)(160.0f * image.width / image.height);
        int thumb_h = image.height > image.width ? 160 : (int)(160.0f * image.height / image.width);
        if (thumb_w < 1) thumb_w = 1;
        if (thumb_h < 1) thumb_h = 1;
        ImageResizeNN(&image, thumb_w, thumb_h);
        r->thumb = LoadTextureFromImage(image);
        if (r->thumb.id) SetTextureFilter(r->thumb, TEXTURE_FILTER_POINT);
        UnloadImage(image);
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Load pixelated reference thumbnails | v 3.3.0 | d5c68f */

/* Copy an image or text prompt into refs/imported; return bool. */
static bool import_reference_file(App *app, const char *source) {
    if (!is_image_path(source) && !is_text_path(source)) return false;
    const char *base = strrchr(source,'/'); base = base ? base + 1 : source;
    char folder[APP_PATH_CAP], target[APP_PATH_CAP]; path_join(folder,sizeof(folder),app->refs_path,"imported"); make_dirs(folder);
    path_join(target,sizeof(target),folder,base);
    if (file_exists(target)) {
        const char *dot=strrchr(base,'.'); size_t stem=dot?(size_t)(dot-base):strlen(base);
        for (int n=1;n<1000;n++) {
            char unique[256]; snprintf(unique,sizeof(unique),"%.*s-%d%s",(int)stem,base,n,dot?dot:"");
            path_join(target,sizeof(target),folder,unique); if (!file_exists(target)) break;
        }
    }
    if (!copy_file_new(source,target)) return false;
    add_reference(app,target); return true;
}

static bool select_prompt_reference(App *app, AppReference *ref);

/* Create an empty collision-safe prompt in refs/imported; return bool. */
static bool create_prompt_file(App *app) {
    if (app->prompt_dirty) { set_message(app, "Save the current prompt before creating another"); return false; }
    char folder[APP_PATH_CAP], target[APP_PATH_CAP];
    path_join(folder, sizeof(folder), app->refs_path, "imported");
    if (!make_dirs(folder)) return false;
    path_join(target, sizeof(target), folder, "prompt.txt");
    for (int i = 1; file_exists(target) && i < 1000; i++) {
        char name[64]; snprintf(name, sizeof(name), "prompt-%d.txt", i);
        path_join(target, sizeof(target), folder, name);
    }
    if (file_exists(target) || !write_text(target, "")) return false;
    add_reference(app, target);
    for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, target)) {
        if (!select_prompt_reference(app, &app->refs[i])) return false;
        app->show_full_image = true;
        return true;
    }
    return false;
}

/* Open a text reference for editing in App* app; return bool. */
static bool select_prompt_reference(App *app, AppReference *ref) {
    if (!ref || !ref->is_text) return false;
    if (app->prompt_dirty) {
        if (!strcmp(app->prompt_path, ref->path)) return true;
        set_message(app, "Save the current prompt before switching"); return false;
    }
    char content[APP_TEXT_CAP];
    if (!read_text(ref->path, content, sizeof(content))) { set_message(app, "Could not read prompt file"); return false; }
    copy_text(app->prompt_path, sizeof(app->prompt_path), ref->path);
    copy_text(app->prompt_edit, sizeof(app->prompt_edit), content);
    app->prompt_dirty = app->prompt_editor_focused = false;
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Add text prompts to references | v 3.3.0 | 89d8ea */

/* Open image/text import dialog and append the copied reference; return void. */
static void import_reference_dialog(App *app) {
    char chosen[APP_PATH_CAP];
    if (!choose_file(chosen, sizeof(chosen), true, false) || !import_reference_file(app, chosen)) return;
    int imported = app->ref_count - 1;
    if (imported >= 0 && app->refs[imported].is_text && !select_prompt_reference(app, &app->refs[imported])) return;
    app->selected_ref = imported;
    save_reference_state(app);
    set_message(app, "Reference imported");
}

/* agent: gpt-6.1-sol | 2026-10-04 | Add importable UI font settings | v 3.3.0 | 240208 */
/* Return true for supported TTF or OTF paths; return bool. */
static bool is_font_path(const char *path) {
    const char *dot = strrchr(path, '.');
    return dot && (!strcasecmp(dot, ".ttf") || !strcasecmp(dot, ".otf"));
}

/* Return a stored font path's basename as const char *. */
static const char *font_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Scan project assets/fonts for stored fonts in App* app; return void. */
static void scan_asset_fonts(App *app) {
    app->font_count = 0;
    DIR *dir = opendir(app->font_dir);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) && app->font_count < APP_MAX_FONTS) {
        if (entry->d_name[0] == '.' || !is_font_path(entry->d_name)) continue;
        char path[APP_PATH_CAP]; path_join(path, sizeof(path), app->font_dir, entry->d_name);
        struct stat st;
        if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
        copy_text(app->font_paths[app->font_count++], sizeof(app->font_paths[0]), path);
    }
    closedir(dir);
    for (int i = 1; i < app->font_count; i++) {
        char item[APP_PATH_CAP]; copy_text(item, sizeof(item), app->font_paths[i]);
        int j = i;
        while (j > 0 && strcasecmp(font_basename(app->font_paths[j - 1]), font_basename(item)) > 0) {
            copy_text(app->font_paths[j], sizeof(app->font_paths[j]), app->font_paths[j - 1]); j--;
        }
        copy_text(app->font_paths[j], sizeof(app->font_paths[j]), item);
    }
}

/* Copy a TTF/OTF into assets/fonts; return its stored path through out. */
static bool import_asset_font(App *app, const char *source, char *out, size_t cap) {
    if (!is_font_path(source)) return false;
    const char *base = font_basename(source);
    char target[APP_PATH_CAP]; path_join(target, sizeof(target), app->font_dir, base);
    struct stat st;
    if (lstat(target, &st) == 0) {
        const char *dot = strrchr(base, '.'); size_t stem = dot ? (size_t)(dot - base) : strlen(base);
        bool available = false;
        for (int i = 1; i < 1000; i++) {
            char name[256]; snprintf(name, sizeof(name), "%.*s-%d%s", (int)stem, base, i, dot ? dot : "");
            path_join(target, sizeof(target), app->font_dir, name);
            if (lstat(target, &st) && errno == ENOENT) { available = true; break; }
        }
        if (!available) return false;
    }
    if (!copy_file_new(source, target)) return false;
    copy_text(out, cap, target);
    return true;
}

/* Persist a selected stored font, or empty text for raylib default; return void. */
static void save_ui_font_choice(App *app) {
    const char *name = app->selected_font >= 0 && app->selected_font < app->font_count
        ? font_basename(app->font_paths[app->selected_font]) : "";
    write_text(app->font_state_path, name);
}

/* Select stored font index or raylib default for App* app; return bool. */
static bool select_ui_font(App *app, int index) {
    if (index < -1 || index >= app->font_count) return false;
    Font next = {0};
    if (index >= 0) {
        next = load_ui_font(app->font_paths[index]);
        if (!next.texture.id) return false;
    }
    if (s_ui_font.texture.id) UnloadFont(s_ui_font);
    s_ui_font = next;
    app->selected_font = index;
    copy_text(app->ui_font_name, sizeof(app->ui_font_name), index >= 0 ? font_basename(app->font_paths[index]) : "Raylib default");
    save_ui_font_choice(app);
    set_message(app, index >= 0 ? "UI font selected" : "Raylib default font selected");
    return true;
}

/* Load a stored UI font name from the workspace for App* app; return void. */
static void load_saved_ui_font(App *app) {
    char name[256]; app->selected_font = -1;
    if (!read_text(app->font_state_path, name, sizeof(name))) { copy_text(app->ui_font_name, sizeof(app->ui_font_name), "Raylib default"); return; }
    size_t length = strlen(name); while (length && (name[length - 1] == '\n' || name[length - 1] == '\r')) name[--length] = 0;
    for (int i = 0; i < app->font_count; i++) if (!strcmp(font_basename(app->font_paths[i]), name)) {
        Font font = load_ui_font(app->font_paths[i]);
        if (!font.texture.id) break;
        s_ui_font = font; app->selected_font = i;
        copy_text(app->ui_font_name, sizeof(app->ui_font_name), font_basename(app->font_paths[i]));
        return;
    }
    copy_text(app->ui_font_name, sizeof(app->ui_font_name), "Raylib default");
}

/* Draw font controls for App* app at int x/y; return void. */
static void draw_font_settings(App *app, int x, int y) {
    ui_draw_text("UI font", x, y + 7, 16, UI_MUTED);
    if (button((Rectangle){x + 82, y, 128, 38}, "Import font...", false)) {
        char source[APP_PATH_CAP], stored[APP_PATH_CAP];
        if (choose_file(source, sizeof(source), false, true)) {
            if (import_asset_font(app, source, stored, sizeof(stored))) {
                scan_asset_fonts(app);
                for (int i = 0; i < app->font_count; i++) if (!strcmp(app->font_paths[i], stored)) {
                    if (!select_ui_font(app, i)) { select_ui_font(app, -1); set_message(app, "Could not load imported font"); }
                    break;
                }
            } else set_message(app, "Could not import font");
        }
    }
    bool has_fonts = app->font_count > 0;
    if (button((Rectangle){x + 220, y, 40, 38}, "<", has_fonts) && has_fonts) {
        int next = app->selected_font < 0 ? app->font_count - 1 : (app->selected_font + app->font_count - 1) % app->font_count;
        select_ui_font(app, next);
    }
    ui_draw_text(app->ui_font_name[0] ? TextFormat("%.22s", app->ui_font_name) : "No asset fonts", x + 270, y + 8, 15, UI_TEXT);
    if (button((Rectangle){x + 470, y, 40, 38}, ">", has_fonts) && has_fonts) {
        int next = app->selected_font < 0 ? 0 : (app->selected_font + 1) % app->font_count;
        select_ui_font(app, next);
    }
    if (button((Rectangle){x + 520, y, 132, 38}, "Raylib default", app->selected_font < 0)) select_ui_font(app, -1);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Add importable UI font settings | v 3.3.0 | 240208 */

/* Scan workspace reference files and one directory level of named groups. */
static void scan_refs(App *app) {
    for (int i = 0; i < app->ref_count; i++) { if (app->refs[i].thumb.id) UnloadTexture(app->refs[i].thumb); if (app->refs[i].full.id) UnloadTexture(app->refs[i].full); }
    app->ref_count = 0;
    DIR *root = opendir(app->refs_path);
    if (!root) return;
    struct dirent *de;
    while ((de = readdir(root))) {
        if (de->d_name[0] == '.') continue;
        char path[APP_PATH_CAP]; path_join(path, sizeof(path), app->refs_path, de->d_name);
        struct stat st;
        if (stat(path, &st)) continue;
        if (S_ISREG(st.st_mode)) add_reference(app, path);
        else if (S_ISDIR(st.st_mode)) {
            DIR *sub = opendir(path);
            if (!sub) continue;
            struct dirent *child;
            while ((child = readdir(sub))) {
                if (child->d_name[0] == '.') continue;
                char child_path[APP_PATH_CAP]; path_join(child_path, sizeof(child_path), path, child->d_name);
                add_reference(app, child_path);
            }
            closedir(sub);
        }
    }
    closedir(root);
    load_reference_state(app);
}

/* Resolve only named prompt tokens and preserve all other brace text. */
static void resolve_prompt(App *app, const char *source, char *out, size_t cap, AppRun *run) {
    size_t n = 0;
    for (size_t i = 0; source[i] && n + 1 < cap;) {
        const char *replacement = NULL;
        size_t token = 0;
        if (!strncmp(source + i, "{workspace}", 11)) { replacement = app->workspace; token = 11; }
        else if (!strncmp(source + i, "{reference_count}", 17)) { static char count[24]; snprintf(count, sizeof(count), "%d", app->snapshot_ref_count); replacement = count; token = 17; }
        else if (!strncmp(source + i, "{reference_files}", 17)) {
            static char files[4096]; files[0] = 0;
            for (int k = 0; k < app->snapshot_ref_count; k++) {
                if (*files) strncat(files, ", ", sizeof(files) - strlen(files) - 1);
                strncat(files, app->snapshot_refs[k], sizeof(files) - strlen(files) - 1);
            }
            replacement = files; token = 17;
        } else if (!strncmp(source + i, "{model}", 7)) { replacement = app->snapshot_profile.model; token = 7; }
        else if (!strncmp(source + i, "{active_process}", 16)) { replacement = run ? run->current_step : "idle"; token = 16; }
        else if (!strncmp(source + i, "{output_path}", 13)) { replacement = run ? run->primary : "pending"; token = 13; }
        if (replacement) {
            size_t m = strlen(replacement); if (m > cap - n - 1) m = cap - n - 1;
            memcpy(out + n, replacement, m); n += m; i += token;
        } else out[n++] = source[i++];
    }
    out[n] = 0;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Combine enabled prompt references | v 3.3.0 | f86085 */
/* Build enabled text refs and profile fragments into prompt output; return void. */
static void build_prompt(App *app, char *out, size_t cap) {
    if (!cap) return;
    out[0] = 0;
    size_t n = 0;
    app->snapshot_prompt_part_count = 0;
    char content[APP_TEXT_CAP];
    for (int i = 0; i < app->ref_count && app->snapshot_prompt_part_count < APP_MAX_PROMPT_PARTS; i++) {
        AppReference *ref = &app->refs[i];
        if (!ref->is_text || ref->excluded) continue;
        const char *part = NULL;
        if (app->prompt_dirty && !strcmp(ref->path, app->prompt_path)) part = app->prompt_edit;
        else if (read_text(ref->path, content, sizeof(content))) part = content;
        if (!part || !part[0]) continue;
        copy_text(app->snapshot_prompt_parts[app->snapshot_prompt_part_count++], APP_TEXT_CAP, part);
        if (n) {
            if (n + 1 >= cap) continue;
            out[n++] = ' '; out[n] = 0;
        }
        size_t len = strlen(part), room = cap - n - 1;
        if (len > room) len = room;
        memcpy(out + n, part, len); n += len; out[n] = 0;
    }
    AppProfile *p = &app->profiles[app->active_profile];
    for (int i = 0; i < p->prompt_count && app->snapshot_prompt_part_count < APP_MAX_PROMPT_PARTS; i++) {
        copy_text(app->snapshot_prompt_parts[app->snapshot_prompt_part_count++], APP_TEXT_CAP, p->prompts[i]);
        if (n + 1 < cap) n += (size_t)snprintf(out + n, cap - n, "%s%s", n ? " " : "", p->prompts[i]);
    }
}

/* agent: gpt-6.1-sol | 2026-10-04 | Save open prompt and refresh previews | v 3.3.0 | 550f70 */
/* Save dirty open prompt and refresh its cached card excerpt; return bool. */
static bool save_open_prompt(App *app) {
    if (!app->prompt_dirty) return true;
    if (!write_text(app->prompt_path, app->prompt_edit)) { set_message(app, "Could not save prompt"); return false; }
    app->prompt_dirty = false;
    for (int i = 0; i < app->ref_count; i++)
        if (app->refs[i].is_text && !strcmp(app->refs[i].path, app->prompt_path)) cache_prompt_preview(&app->refs[i], app->prompt_edit);
    set_message(app, "Prompt saved");
    return true;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Rename references of either type | v 3.3.0 | f5dfde */
/* Rename AppReference* ref from const char* entered for App* app; return bool. */
static bool rename_reference_file(App *app, AppReference *ref, const char *entered) {
    char name[256], directory[APP_PATH_CAP], target[APP_PATH_CAP];
    copy_text(name, sizeof(name), entered);
    size_t n = strlen(name);
    while (n && (name[n-1] == ' ' || name[n-1] == '\t')) name[--n] = 0;
    const char *extension = strrchr(ref->name, '.');
    if (!extension || !n || !strcmp(name, ".") || !strcmp(name, "..") || strchr(name, '/') || strchr(name, '\\')) { set_message(app, "Enter a valid reference filename"); return false; }
    const char *entered_extension = strrchr(name, '.');
    bool has_file_extension = is_text_path(name) || is_image_path(name);
    if (has_file_extension && strcasecmp(entered_extension, extension)) { set_message(app, "Keep the original file extension"); return false; }
    if (!has_file_extension) {
        if (n + strlen(extension) >= sizeof(name)) { set_message(app, "Reference filename is too long"); return false; }
        strcat(name, extension);
    }
    if (!strcmp(ref->name, name)) return true;
    const char *slash = strrchr(ref->path, '/');
    if (!slash) return false;
    size_t directory_len = (size_t)(slash - ref->path);
    if (directory_len >= sizeof(directory)) return false;
    memcpy(directory, ref->path, directory_len); directory[directory_len] = 0;
    path_join(target, sizeof(target), directory, name);
    struct stat st;
    if (!lstat(target, &st)) { set_message(app, "A file with that name already exists"); return false; }
    if (errno != ENOENT) { set_message(app, "Could not check reference filename"); return false; }
    bool open_document = !strcmp(ref->path, app->prompt_path);
    if (open_document && !save_open_prompt(app)) return false;
    if (rename(ref->path, target)) { set_message(app, "Could not rename reference"); return false; }
    copy_text(ref->path, sizeof(ref->path), target);
    copy_text(ref->name, sizeof(ref->name), name);
    if (open_document) copy_text(app->prompt_path, sizeof(app->prompt_path), target);
    save_reference_state(app);
    set_message(app, "Reference renamed");
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Rename references of either type | v 3.3.0 | f5dfde */

/* agent: gpt-6.1-sol | 2026-10-04 | Edit names in both reference viewers | v 3.3.0 | 7d873a */
/* Draw and edit filename for App* app and AppReference* ref; return void. */
static void draw_reference_name_field(App *app, AppReference *ref) {
    Rectangle box = {36, 16, fmaxf(180, fminf(520, (float)GetScreenWidth()-190)), 36};
    bool editing = !strcmp(app->edit_field, "reference.name");
    DrawRectangleRec(box, UI_PANEL); DrawRectangleLinesEx(box, 1, editing ? UI_ACCENT : UI_PANEL_ALT);
    if (!editing) ui_draw_text(ref->name, (int)box.x+9, (int)box.y+8, 16, UI_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        copy_text(app->edit_field, sizeof(app->edit_field), "reference.name");
        copy_text(app->field_buffer, sizeof(app->field_buffer), ref->name);
        app->prompt_editor_focused = false;
        editing = true;
    }
    if (!editing) return;
    ui_draw_text(app->field_buffer, (int)box.x+9, (int)box.y+8, 16, UI_TEXT);
    int key = GetCharPressed();
    while (key > 0) {
        size_t n = strlen(app->field_buffer);
        if (key >= 32 && key <= 125 && key != '/' && key != '\\' && n + 1 < sizeof(app->field_buffer)) { app->field_buffer[n] = (char)key; app->field_buffer[n+1] = 0; }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) { size_t n = strlen(app->field_buffer); if (n) app->field_buffer[n-1] = 0; }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) app->field_buffer[0] = 0;
    if (IsKeyPressed(KEY_ENTER)) {
        if (rename_reference_file(app, ref, app->field_buffer)) app->edit_field[0] = 0;
    }
    if (IsKeyPressed(KEY_ESCAPE)) app->edit_field[0] = 0;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Edit names in both reference viewers | v 3.3.0 | 7d873a */
/* agent: gpt-6.1-sol | 2026-10-04 | Save open prompt and refresh previews | v 3.3.0 | 550f70 */

/* Find the first run identifier not already present in the workspace. */
static void make_run_id(App *app, char *out, size_t cap) {
    time_t now = time(NULL); struct tm tm_now; localtime_r(&now, &tm_now);
    strftime(out, cap, "run_%Y%m%d_%H%M%S", &tm_now);
    char path[APP_PATH_CAP]; path_join(path, sizeof(path), app->runs_path, out);
    for (int suffix = 1; access(path, F_OK) == 0; suffix++) {
        snprintf(out, cap, "run_%04ld_%d", (long)now, suffix);
        path_join(path, sizeof(path), app->runs_path, out);
    }
}

/* Start one bounded argv child and capture merged stdout/stderr without a shell. */
static bool spawn_process(AppRun *run, const char *cwd, char *const argv[], ProcessKind kind) {
    int outpipe[2], errpipe[2];
    if (pipe(outpipe)) return false;
    if (pipe(errpipe)) { close(outpipe[0]); close(outpipe[1]); return false; }
    pid_t pid = fork();
    if (pid < 0) { close(outpipe[0]); close(outpipe[1]); close(errpipe[0]); close(errpipe[1]); return false; }
    if (pid == 0) {
        setpgid(0, 0); close(outpipe[0]); close(errpipe[0]);
        dup2(outpipe[1], STDOUT_FILENO); dup2(errpipe[1], STDERR_FILENO); close(outpipe[1]); close(errpipe[1]);
        if (chdir(cwd)) _exit(126);
        execvp(argv[0], argv);
        dprintf(STDERR_FILENO, "could not start %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    setpgid(pid, pid); close(outpipe[1]); close(errpipe[1]);
    int flags = fcntl(outpipe[0], F_GETFL, 0); fcntl(outpipe[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(errpipe[0], F_GETFL, 0); fcntl(errpipe[0], F_SETFL, flags | O_NONBLOCK);
    run->child = pid; run->pipe_fd = outpipe[0]; run->err_fd = errpipe[0]; run->process_kind = kind;
    copy_text(run->current_step, sizeof(run->current_step), kind == PROC_GENERATION ? "starting generation" : "starting pixel processing");
    return true;
}

/* Append one argv item when fixed storage has enough room. */
static void argv_push(char **argv, int *argc, int max, char *value) {
    if (*argc < max - 1) argv[(*argc)++] = value;
}

/* Start the image-generation process from the immutable preview snapshot. */
static bool start_generation(App *app, AppRun *run) {
    char *argv[1024]; int argc = 0;
    char prompt_parts[APP_MAX_PROMPT_PARTS][APP_TEXT_CAP];
    char output[APP_PATH_CAP], rec[APP_PATH_CAP];
    snprintf(output, sizeof(output), "%s/source.png", run->dir);
    snprintf(rec, sizeof(rec), "%s/generation.record.txt", run->dir);
    copy_text(run->primary, sizeof(run->primary), output);
    AppProfile *p = &app->snapshot_profile;
    argv_push(argv, &argc, 1024, "replicate-gen");
    argv_push(argv, &argc, 1024, "-m"); argv_push(argv, &argc, 1024, p->model);
    argv_push(argv, &argc, 1024, "-o"); argv_push(argv, &argc, 1024, output);
    if (*p->aspect) { argv_push(argv, &argc, 1024, "-a"); argv_push(argv, &argc, 1024, p->aspect); }
    for (int i = 0; i < p->set_count; i++) { argv_push(argv, &argc, 1024, "--set"); argv_push(argv, &argc, 1024, p->sets[i]); }
    for (int i = 0; i < app->snapshot_prompt_part_count; i++) {
        char *part = prompt_parts[i];
        resolve_prompt(app, app->snapshot_prompt_parts[i], part, APP_TEXT_CAP, run);
        if (!*part) continue;
        argv_push(argv, &argc, 1024, "-p"); argv_push(argv, &argc, 1024, part);
    }
    for (int i = 0; i < app->snapshot_ref_count; i++) {
        argv_push(argv, &argc, 1024, "-i"); argv_push(argv, &argc, 1024, app->snapshot_refs[i]);
    }
    if (p->slim_upload) argv_push(argv, &argc, 1024, "--slim-upload");
    argv_push(argv, &argc, 1024, "--record-file"); argv_push(argv, &argc, 1024, rec);
    argv[argc] = NULL;
    if (!spawn_process(run, app->workspace, argv, PROC_GENERATION)) return false;
    copy_text(run->source, sizeof(run->source), output);
    run->state = RUN_RUNNING;
    run->started = GetTime();
    return true;
}

/* Locate the installed processor beside the resolved replicate-gen executable. */
static bool locate_processor(char *out, size_t cap) {
    const char *path = getenv("PATH");
    if (!path) return false;
    char paths[4096]; copy_text(paths, sizeof(paths), path);
    char *save = NULL;
    for (char *dir = strtok_r(paths, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        char candidate[APP_PATH_CAP]; path_join(candidate, sizeof(candidate), dir, "replicate-gen");
        if (access(candidate, X_OK)) continue;
        char resolved[APP_PATH_CAP];
        if (!realpath(candidate, resolved)) continue;
        char *slash = strrchr(resolved, '/'); if (!slash) continue; *slash = 0;
        path_join(out, cap, resolved, "process_ai_pixel_2.py");
        return file_exists(out);
    }
    return false;
}

/* Start pixel processing after generation, keeping artifacts inside this run. */
static bool start_pixel(App *app, AppRun *run) {
    char processor[APP_PATH_CAP], python[APP_PATH_CAP], emit[16], layout[64], pitch[64], preset[32], group[32];
    char magenta[32], alpha[32], output[APP_PATH_CAP], padding[32], tile[32];
    if (!locate_processor(processor, sizeof(processor))) {
        copy_text(run->error, sizeof(run->error), "process_ai_pixel_2.py was not found beside replicate-gen");
        return false;
    }
    copy_text(python,sizeof(python),"python3");
    char processor_dir[APP_PATH_CAP], candidate_python[APP_PATH_CAP]; copy_text(processor_dir,sizeof(processor_dir),processor);
    char *slash=strrchr(processor_dir,'/'); if(slash){*slash=0;path_join(candidate_python,sizeof(candidate_python),processor_dir,".venv/bin/python");if(access(candidate_python,X_OK)==0)copy_text(python,sizeof(python),candidate_python);}
    char *argv[128]; int argc = 0; AppProfile *p = &app->snapshot_profile;
    snprintf(output, sizeof(output), "%s/final.pixel.png", run->dir);
    snprintf(padding, sizeof(padding), "%d", p->padding);
    copy_text(emit, sizeof(emit), run->debug ? "all" : "final");
    copy_text(layout, sizeof(layout), p->layout[0] ? p->layout : "auto");
    copy_text(pitch, sizeof(pitch), p->pixel_pitch[0] ? p->pixel_pitch : "auto");
    copy_text(preset, sizeof(preset), p->pitch_preset[0] ? p->pitch_preset : "best");
    copy_text(group, sizeof(group), p->pitch_group_layout[0] ? p->pitch_group_layout : "compact");
    copy_text(magenta, sizeof(magenta), p->magenta[0] ? p->magenta : "outside");
    copy_text(alpha, sizeof(alpha), p->edge_alpha[0] ? p->edge_alpha : "legacy");
    argv_push(argv, &argc, 128, python); argv_push(argv, &argc, 128, processor);
    argv_push(argv, &argc, 128, "-i"); argv_push(argv, &argc, 128, run->source);
    argv_push(argv, &argc, 128, "--root"); argv_push(argv, &argc, 128, app->workspace);
    argv_push(argv, &argc, 128, "--out-dir"); argv_push(argv, &argc, 128, run->dir);
    argv_push(argv, &argc, 128, "--extra-dir");
    char extra[APP_PATH_CAP]; path_join(extra, sizeof(extra), run->dir, "extra"); argv_push(argv, &argc, 128, extra);
    argv_push(argv, &argc, 128, "--output"); argv_push(argv, &argc, 128, output);
    argv_push(argv, &argc, 128, "--layout"); argv_push(argv, &argc, 128, layout);
    argv_push(argv, &argc, 128, "--pixel-pitch"); argv_push(argv, &argc, 128, pitch);
    argv_push(argv, &argc, 128, "--pixel-pitch-preset"); argv_push(argv, &argc, 128, preset);
    argv_push(argv, &argc, 128, "--pixel-pitch-group-layout"); argv_push(argv, &argc, 128, group);
    char align[32]; copy_text(align, sizeof(align), p->align_blobs[0] ? p->align_blobs : "auto");
    argv_push(argv, &argc, 128, "--align-blobs"); argv_push(argv, &argc, 128, align);
    argv_push(argv, &argc, 128, "--magenta"); argv_push(argv, &argc, 128, magenta);
    argv_push(argv, &argc, 128, "--edge-alpha-mode"); argv_push(argv, &argc, 128, alpha);
    argv_push(argv, &argc, 128, "--padding"); argv_push(argv, &argc, 128, padding);
    if (p->rearrange) argv_push(argv, &argc, 128, "--rearrange");
    for (int i = 0; i < p->tile_count; i++) { snprintf(tile, sizeof(tile), "%d", p->tile_sizes[i]); argv_push(argv, &argc, 128, "--tile-size"); argv_push(argv, &argc, 128, strdup(tile)); }
    argv_push(argv, &argc, 128, "--emit"); argv_push(argv, &argc, 128, emit);
    argv[argc] = NULL;
    if (!spawn_process(run, app->workspace, argv, PROC_PIXEL)) return false;
    return true;
}

/* Scan actual files produced by the processor and collect stages and final outputs. */
static int artifact_order(const void *a, const void *b) {
    const AppArtifact *aa = a, *bb = b;
    return bb->number - aa->number;
}

/* Scan both run output folders, sorting stages by processor execution order. */
static void scan_artifacts(AppRun *run) {
    for (int i = 0; i < run->stage_count; i++) {
        if (run->stages[i].texture.id) UnloadTexture(run->stages[i].texture);
        if (run->stages[i].thumb.id) UnloadTexture(run->stages[i].thumb);
    }
    for (int i = 0; i < run->output_count; i++) {
        if (run->outputs[i].texture.id) UnloadTexture(run->outputs[i].texture);
        if (run->outputs[i].thumb.id) UnloadTexture(run->outputs[i].thumb);
    }
    run->stage_count = 0; run->output_count = 0;
    char folders[2][APP_PATH_CAP]; copy_text(folders[0],sizeof(folders[0]),run->dir); path_join(folders[1],sizeof(folders[1]),run->dir,"extra");
    for (int folder = 0; folder < 2; folder++) {
        DIR *dir = opendir(folders[folder]); if (!dir) continue;
        struct dirent *de;
        while ((de = readdir(dir))) {
            if (!is_image_path(de->d_name)) continue;
            char path[APP_PATH_CAP]; path_join(path, sizeof(path), folders[folder], de->d_name);
            AppArtifact *a = NULL;
            const char *s = strstr(de->d_name, "stage-");
            if (s && run->stage_count < APP_MAX_STAGES) {
                a = &run->stages[run->stage_count++]; memset(a, 0, sizeof(*a)); a->number = atoi(s + 6);
            } else if (strstr(de->d_name, ".pixel") && run->output_count < APP_MAX_OUTPUTS) {
                a = &run->outputs[run->output_count++]; memset(a, 0, sizeof(*a));
            }
            if (!a) continue;
            copy_text(a->name, sizeof(a->name), de->d_name); copy_text(a->path, sizeof(a->path), path);
            Image image = LoadImage(path);
            if (image.data) {
                a->width = image.width; a->height = image.height;
                a->texture = LoadTexture(path);
                if (a->texture.id) SetTextureFilter(a->texture,TEXTURE_FILTER_POINT);
                ImageResize(&image, 240, (int)(240.0f * image.height / (float)image.width));
                a->thumb = LoadTextureFromImage(image); UnloadImage(image);
            }
        }
        closedir(dir);
    }
    qsort(run->stages, (size_t)run->stage_count, sizeof(run->stages[0]), artifact_order);
}

/* Keep generated source and actual processor artifacts discoverable after exit. */
static void update_run_artifacts(App *app, AppRun *run) {
    scan_artifacts(run);
    if (file_exists(run->source) && !strstr(run->source, ".pixel")) copy_text(run->primary, sizeof(run->primary), run->source);
    else if (run->output_count) copy_text(run->primary, sizeof(run->primary), run->outputs[0].path);
    (void)app;
}

/* Load completed and partial run folders, logs, manifests, and emitted images. */
static void load_run_history(App *app) {
    DIR *dir = opendir(app->runs_path); if (!dir) return;
    struct dirent *de;
    while ((de = readdir(dir)) && app->run_count < APP_MAX_RUNS) {
        if (de->d_name[0] == '.') continue;
        char path[APP_PATH_CAP]; path_join(path, sizeof(path), app->runs_path, de->d_name);
        struct stat st; if (stat(path, &st) || !S_ISDIR(st.st_mode)) continue;
        AppRun *run = &app->runs[app->run_count]; memset(run, 0, sizeof(*run)); run->pipe_fd = run->err_fd = -1;
        copy_text(run->id, sizeof(run->id), de->d_name); copy_text(run->dir, sizeof(run->dir), path);
        path_join(run->source, sizeof(run->source), path, "source.png");
        char logpath[APP_PATH_CAP]; path_join(logpath, sizeof(logpath), path, "process.log");
        FILE *f = fopen(logpath, "r"); char line[APP_TEXT_CAP + 128];
        while (f && fgets(line, sizeof(line), f)) {
            char tm[32] = "", proc[32] = "", message[APP_TEXT_CAP] = "";
            if (sscanf(line, "%31s [%31[^]]] %8191[^\n]", tm, proc, message) < 3) continue;
            if (run->log_count >= APP_MAX_LOGS) continue;
            AppLogLine *entry = &run->logs[run->log_count++]; copy_text(entry->time, sizeof(entry->time), tm); copy_text(entry->process, sizeof(entry->process), proc); copy_text(entry->text, sizeof(entry->text), message);
            if (strstr(message, "stage-") || strstr(message, "decision ") || strstr(message, "post-processing")) copy_text(run->current_step, sizeof(run->current_step), message);
        }
        if (f) fclose(f);
        run->started = 0;
        run->state = RUN_FAILED;
        if (run->state == RUN_FAILED) copy_text(run->error, sizeof(run->error), "incomplete previous run; inspect retained files and logs");
        path_join(run->primary, sizeof(run->primary), path, "final.pixel.png");
        update_run_artifacts(app, run);
        char final_output[APP_PATH_CAP]; path_join(final_output, sizeof(final_output), path, "final.pixel.png");
        if (file_exists(final_output)) { run->state = RUN_SUCCEEDED; run->error[0] = 0; copy_text(run->primary, sizeof(run->primary), final_output); }
        app->run_count++;
    }
    closedir(dir);
    if (app->run_count) app->selected_run = app->run_count - 1;
}

/* Poll one child and begin the postprocessor after successful generation. */
static void update_child(App *app, AppRun *run) {
    if (run->child <= 0) return;
    int fds[2] = {run->pipe_fd, run->err_fd};
    for (int stream = 0; stream < 2; stream++) if (fds[stream] >= 0) {
        char buf[2048]; ssize_t n = read(fds[stream], buf, sizeof(buf) - 1);
        while (n > 0) {
            buf[n] = 0; char *save = NULL;
            for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
                add_log(run, TextFormat("%s/%s", run->process_kind == PROC_GENERATION ? "generation" : "pixel", stream ? "stderr" : "stdout"), line);
            n = read(fds[stream], buf, sizeof(buf) - 1);
        }
    }
    int status = 0; pid_t result = waitpid(run->child, &status, WNOHANG);
    if (result != run->child) return;
    if (run->pipe_fd >= 0) { close(run->pipe_fd); run->pipe_fd = -1; }
    if (run->err_fd >= 0) { close(run->err_fd); run->err_fd = -1; }
    ProcessKind finished = run->process_kind; run->child = 0; run->process_kind = PROC_NONE;
    run->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1);
    if (run->cancel_pending || run->exit_code == 130 || run->exit_code == 143) {
        run->state = RUN_CANCELED; run->finished = GetTime(); copy_text(run->current_step, sizeof(run->current_step), "canceled; partial files retained");
    } else if (run->exit_code != 0) {
        run->state = RUN_FAILED; run->finished = GetTime();
        snprintf(run->error, sizeof(run->error), "%s exited with status %d", finished == PROC_GENERATION ? "generation" : "pixel processing", run->exit_code);
        copy_text(run->current_step, sizeof(run->current_step), "failed");
    } else if (finished == PROC_GENERATION) {
        update_run_artifacts(app, run);
        if (!start_pixel(app, run)) { run->state = RUN_FAILED; run->finished = GetTime(); if (!*run->error) copy_text(run->error, sizeof(run->error), "Could not start pixel processor"); }
    } else {
        run->state = RUN_SUCCEEDED; run->finished = GetTime();
        copy_text(run->current_step, sizeof(run->current_step), "done");
        update_run_artifacts(app, run);
    }
    if (run->state != RUN_RUNNING) app->active_run = -1;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Load UI font for desktop app | v 3.3.0 | a9fe70 */
/* Initialize App* app with const char *workspace and bool mcp_mode; return bool. */
bool AppInit(App *app, const char *workspace, bool mcp_mode) {
    memset(app, 0, sizeof(*app));
    app->tab = TAB_REFERENCES; app->thumb_size = 104; app->image_zoom = 1.0f;
    app->enet_port = 27881; app->selected_ref = 0; app->active_run = -1; app->selected_run = -1;
    app->runs[0].pipe_fd = -1; app->runs[0].err_fd = -1;
    copy_text(app->workspace, sizeof(app->workspace), workspace && *workspace ? workspace : "assets");
    if (!make_dirs(app->workspace)) { snprintf(app->message, sizeof(app->message), "Cannot create workspace: %s", app->workspace); return false; }
    char absolute_workspace[APP_PATH_CAP];
    if (realpath(app->workspace, absolute_workspace)) copy_text(app->workspace, sizeof(app->workspace), absolute_workspace);
    if (!mcp_mode) {
        s_ui_font = (Font){0};
        path_join(app->font_dir, sizeof(app->font_dir), GetApplicationDirectory(), "assets/fonts");
        make_dirs(app->font_dir);
        path_join(app->font_state_path, sizeof(app->font_state_path), app->workspace, "ui-font.path");
        scan_asset_fonts(app);
        load_saved_ui_font(app);
    }
    path_join(app->refs_path, sizeof(app->refs_path), app->workspace, "refs"); make_dirs(app->refs_path);
    path_join(app->runs_path, sizeof(app->runs_path), app->workspace, "runs"); make_dirs(app->runs_path);
    path_join(app->profiles_path, sizeof(app->profiles_path), app->workspace, "profiles.ini");
    path_join(app->refs_state_path, sizeof(app->refs_state_path), app->workspace, "references.ini");
    char child[APP_PATH_CAP];
    const char *defaults[] = {"misc", "recycle", "superpowers-rpg", "imported"};
    for (int i = 0; i < 4; i++) { path_join(child, sizeof(child), app->refs_path, defaults[i]); make_dirs(child); }
    path_join(child, sizeof(child), app->workspace, "extra"); make_dirs(child);
    load_profiles(app); seed_profile(app); scan_refs(app);
    s_image_shader = LoadShader("shaders.vs", "shaders.fs");
    load_run_history(app);
    app->selected_ref = 0; app->last_enet_fetch = GetTime(); s_mcp_mode = mcp_mode;
    if (!mcp_mode) {
        if (enet_initialize() != 0) { set_message(app, "ENet unavailable; local UI remains active"); return true; }
        ENetAddress address; enet_address_set_host(&address, "127.0.0.1"); address.port = (enet_uint16)app->enet_port;
        ENetHost *host = enet_host_create(&address, 8, 2, 0, 0);
        if (host) app->enet_host = host;
        else set_message(app, "ENet bind failed; local UI remains active");
    }
    if (!*app->message) set_message(app, "Ready");
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Load UI font for desktop app | v 3.3.0 | a9fe70 */

/* agent: gpt-6.1-sol | 2026-10-04 | Release loaded UI font | v 3.3.0 | 0bf8e7 */
/* Release resources owned by App* app; return void. */
void AppShutdown(App *app) {
    for (int i = 0; i < app->ref_count; i++) { if (app->refs[i].thumb.id) UnloadTexture(app->refs[i].thumb); if (app->refs[i].full.id) UnloadTexture(app->refs[i].full); }
    for (int r = 0; r < app->run_count; r++) {
        AppRun *run = &app->runs[r];
        if (run->child > 0) { kill(-run->child, SIGINT); waitpid(run->child, NULL, 0); }
        if (run->pipe_fd >= 0) close(run->pipe_fd);
        if (run->err_fd >= 0) close(run->err_fd);
        for (int i = 0; i < run->stage_count; i++) { if (run->stages[i].texture.id) UnloadTexture(run->stages[i].texture); if (run->stages[i].thumb.id) UnloadTexture(run->stages[i].thumb); }
        for (int i = 0; i < run->output_count; i++) { if (run->outputs[i].texture.id) UnloadTexture(run->outputs[i].texture); if (run->outputs[i].thumb.id) UnloadTexture(run->outputs[i].thumb); }
    }
    if (s_image_shader.id) UnloadShader(s_image_shader);
    if (s_ui_font.texture.id) UnloadFont(s_ui_font);
    if (app->enet_host) enet_host_destroy((ENetHost *)app->enet_host);
    if (!s_mcp_mode) enet_deinitialize();
}
/* agent: gpt-6.1-sol | 2026-10-04 | Release loaded UI font | v 3.3.0 | 0bf8e7 */

/* Return the currently selected run when one exists. */
static AppRun *get_run(App *app) {
    if (app->selected_run >= 0 && app->selected_run < app->run_count) return &app->runs[app->selected_run];
    if (app->active_run >= 0 && app->active_run < app->run_count) return &app->runs[app->active_run];
    return NULL;
}

/* Encode the visible status and current run state for agent queries. */
static void state_json(App *app, char *out, size_t cap) {
    AppRun *run = get_run(app);
    char prompt[APP_TEXT_CAP], esc_prompt[APP_TEXT_CAP * 2], esc_step[512], esc_workspace[APP_PATH_CAP * 2], esc_message[1024];
    build_prompt(app, prompt, sizeof(prompt)); json_escape(prompt, esc_prompt, sizeof(esc_prompt));
    json_escape(run ? run->current_step : "idle", esc_step, sizeof(esc_step));
    json_escape(app->workspace, esc_workspace, sizeof(esc_workspace));
    json_escape(app->message, esc_message, sizeof(esc_message));
    int n = snprintf(out, cap, "{\"workspace\":\"%s\",\"tab\":%d,\"profile\":\"%s\",\"prompt\":\"%s\",\"run_state\":%d,\"process\":\"%s\",\"current_step\":\"%s\",\"progress_percent\":null,\"message\":\"%s\",\"stages\":[",
        esc_workspace, app->tab, app->profile_count ? app->profiles[app->active_profile].name : "", esc_prompt,
        run ? run->state : RUN_IDLE, run ? (run->process_kind == PROC_GENERATION ? "generation" : run->process_kind == PROC_PIXEL ? "pixel" : "none") : "none", esc_step, esc_message);
    if (n < 0 || (size_t)n >= cap) return;
    if (run) for (int i = 0; i < run->stage_count; i++) {
        char name[256], path[APP_PATH_CAP * 2]; json_escape(run->stages[i].name, name, sizeof(name)); json_escape(run->stages[i].path, path, sizeof(path));
        n += snprintf(out + n, cap - (size_t)n, "%s{\"name\":\"%s\",\"number\":%d,\"path\":\"%s\"}", i ? "," : "", name, run->stages[i].number, path);
        if ((size_t)n >= cap) return;
    }
    n += snprintf(out + n, cap - (size_t)n, "],\"outputs\":[");
    if ((size_t)n >= cap) return;
    if (run) for (int i = 0; i < run->output_count; i++) {
        char name[256], path[APP_PATH_CAP * 2]; json_escape(run->outputs[i].name, name, sizeof(name)); json_escape(run->outputs[i].path, path, sizeof(path));
        n += snprintf(out + n, cap - (size_t)n, "%s{\"name\":\"%s\",\"path\":\"%s\",\"width\":%d,\"height\":%d}", i ? "," : "", name, path, run->outputs[i].width, run->outputs[i].height);
        if ((size_t)n >= cap) return;
    }
    n += snprintf(out + n, cap - (size_t)n, "],\"log_tail\":[");
    if ((size_t)n >= cap) return;
    if (run) {
        int first = run->log_count > 8 ? run->log_count - 8 : 0;
        for (int i = first; i < run->log_count; i++) {
            char message[2048], process[48], timestamp[48];
            json_escape(run->logs[i].text, message, sizeof(message)); json_escape(run->logs[i].process, process, sizeof(process)); json_escape(run->logs[i].time, timestamp, sizeof(timestamp));
            n += snprintf(out + n, cap - (size_t)n, "%s{\"time\":\"%s\",\"stream\":\"%s\",\"text\":\"%s\"}", i == first ? "" : ",", timestamp, process, message);
            if ((size_t)n >= cap) return;
        }
    }
    snprintf(out + n, cap - (size_t)n, "]}");
}

/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */
/* agent: gpt-6.1-sol | 2026-10-04 | Route reference trash actions | v 3.3.0 | e1c824 */
/* Apply a request through App* app; return the serialized result as const char*. */
const char *AppDispatch(App *app, const char *request, char *response, size_t response_size) {
    if (response_size) response[0] = 0;
    char action[64] = "", value[APP_TEXT_CAP] = "", path[APP_PATH_CAP] = "";
    json_string(request, "action", action, sizeof(action));
    json_string(request, "value", value, sizeof(value));
    json_string(request, "path", path, sizeof(path));
    if (!strcmp(action, "state") || !strcmp(action, "status")) state_json(app, response, response_size);
    else if (!strcmp(action, "run.inspect")) state_json(app, response, response_size);
    else if (!strcmp(action, "prompt.get")) {
        char prompt[APP_TEXT_CAP], escaped[APP_TEXT_CAP * 2], file[APP_PATH_CAP * 2];
        prompt[0] = 0;
        if (app->prompt_dirty) copy_text(prompt,sizeof(prompt),app->prompt_edit); else read_text(app->prompt_path, prompt, sizeof(prompt));
        json_escape(prompt, escaped, sizeof(escaped)); json_escape(app->prompt_path, file, sizeof(file));
        snprintf(response, response_size, "{\"path\":\"%s\",\"text\":\"%s\",\"dirty\":%s}", file, escaped, app->prompt_dirty ? "true" : "false");
    } else if (!strcmp(action, "profile.list")) {
        size_t n = (size_t)snprintf(response, response_size, "{\"profiles\":[");
        for (int i = 0; i < app->profile_count && n < response_size; i++) {
            char name[160]; json_escape(app->profiles[i].name, name, sizeof(name));
            n += (size_t)snprintf(response + n, response_size - n, "%s{\"index\":%d,\"name\":\"%s\",\"active\":%s}", i ? "," : "", i, name, i == app->active_profile ? "true" : "false");
        }
        if (n >= response_size) snprintf(response, response_size, "{\"ok\":false,\"error\":\"profile list exceeds response capacity\"}");
        else snprintf(response + n, response_size - n, "]}");
    } else if (!strcmp(action, "references.list")) {
        size_t n = (size_t)snprintf(response, response_size, "{\"references\":[");
        for (int i = 0; i < app->ref_count && n < response_size; i++) {
            char name[512], file[APP_PATH_CAP * 2]; json_escape(app->refs[i].name, name, sizeof(name)); json_escape(app->refs[i].path, file, sizeof(file));
            n += (size_t)snprintf(response + n, response_size - n, "%s{\"name\":\"%s\",\"path\":\"%s\",\"excluded\":%s,\"selected\":%s}", i ? "," : "", name, file, app->refs[i].excluded ? "true" : "false", app->refs[i].selected ? "true" : "false");
        }
        if (n >= response_size) snprintf(response, response_size, "{\"ok\":false,\"error\":\"reference list exceeds response capacity\"}");
        else snprintf(response + n, response_size - n, "]}");
    } else if (!strcmp(action, "tab")) {
        int tab = atoi(value); if (tab >= 0 && tab < TAB_COUNT) { app->tab = (AppTab)tab; app->details_view = app->settings_view = false; app->show_add_menu = false; }
        snprintf(response, response_size, "{\"ok\":true}");
    } else if (!strcmp(action, "prompt.save")) {
        char prompt[APP_TEXT_CAP]; json_string(request, "text", prompt, sizeof(prompt));
        if (app->prompt_path[0] && write_text(app->prompt_path, prompt)) { copy_text(app->prompt_edit, sizeof(app->prompt_edit), prompt); app->prompt_dirty = app->prompt_editor_focused = false; set_message(app, "Prompt saved"); snprintf(response, response_size, "{\"ok\":true}"); }
        else { snprintf(response, response_size, "{\"ok\":false,\"error\":\"prompt save failed\"}"); }
    } else if (!strcmp(action,"prompt.open")) {
        char resolved[APP_PATH_CAP];
        if (app->prompt_dirty) snprintf(response,response_size,"{\"ok\":false,\"error\":\"save prompt edits before switching files\"}");
        else if (!workspace_file(app,path,resolved,sizeof(resolved)) || !strstr(resolved,".txt")) snprintf(response,response_size,"{\"ok\":false,\"error\":\"prompt must be a text file inside the workspace\"}");
        else {
            copy_text(app->prompt_path,sizeof(app->prompt_path),resolved); app->prompt_dirty=app->prompt_editor_focused=false;
            if (!read_text(app->prompt_path,app->prompt_edit,sizeof(app->prompt_edit))) app->prompt_edit[0]=0;
            set_message(app,"Prompt opened for editing"); snprintf(response,response_size,"{\"ok\":true}");
        }
    } else if (!strcmp(action, "generation.preview")) {
        if (app->active_profile < 0 || app->active_profile >= app->profile_count) snprintf(response, response_size, "{\"ok\":false,\"error\":\"no profile\"}");
        else if (!strcmp(app->profiles[app->active_profile].edge_alpha, "weighted") && strcmp(app->profiles[app->active_profile].magenta, "skip")) snprintf(response, response_size, "{\"ok\":false,\"error\":\"weighted edge alpha requires magenta skip\"}");
        else {
            char ep[APP_TEXT_CAP * 2], model[256], output[APP_PATH_CAP], escaped_output[APP_PATH_CAP * 2];
            app->snapshot_profile = app->profiles[app->active_profile];
            build_prompt(app, app->snapshot_prompt, sizeof(app->snapshot_prompt));
            app->snapshot_ref_count = 0;
            for (int i = 0; i < app->ref_count && app->snapshot_ref_count < APP_MAX_REFS; i++)
                if (!app->refs[i].is_text && app->refs[i].selected && !app->refs[i].excluded)
                    copy_text(app->snapshot_refs[app->snapshot_ref_count++], APP_PATH_CAP, app->refs[i].path);
            app->snapshot_debug = app->debug_stages;
            make_run_id(app, app->snapshot_run_id, sizeof(app->snapshot_run_id));
            path_join(app->snapshot_run_dir, sizeof(app->snapshot_run_dir), app->runs_path, app->snapshot_run_id);
            path_join(output, sizeof(output), app->snapshot_run_dir, "final.pixel.png");
            AppRun preview; memset(&preview, 0, sizeof(preview));
            copy_text(preview.current_step, sizeof(preview.current_step), "generation");
            copy_text(preview.primary, sizeof(preview.primary), output);
            resolve_prompt(app, app->snapshot_prompt, app->snapshot_resolved, sizeof(app->snapshot_resolved), &preview);
            json_escape(app->snapshot_resolved, ep, sizeof(ep));
            json_escape(app->snapshot_profile.model, model, sizeof(model));
            json_escape(output, escaped_output, sizeof(escaped_output));
            app->preview_ready = true; app->rerun_pixel_only = false; app->tab = TAB_REFERENCES; app->details_view = true; app->settings_view = false;
            set_message(app, "Request preview ready; confirm to start");
            char profile_name[160], layout[64], pitch[64], preset[64], align[64], magenta[64], alpha[64];
            json_escape(app->snapshot_profile.name,profile_name,sizeof(profile_name));
            json_escape(app->snapshot_profile.layout,layout,sizeof(layout)); json_escape(app->snapshot_profile.pixel_pitch,pitch,sizeof(pitch));
            json_escape(app->snapshot_profile.pitch_preset,preset,sizeof(preset)); json_escape(app->snapshot_profile.align_blobs,align,sizeof(align));
            json_escape(app->snapshot_profile.magenta,magenta,sizeof(magenta)); json_escape(app->snapshot_profile.edge_alpha,alpha,sizeof(alpha));
            int n=snprintf(response,response_size,"{\"ok\":true,\"confirmation_required\":true,\"profile\":\"%s\",\"prompt\":\"%s\",\"prompt_files\":[",profile_name,ep);
            bool first_prompt_file=true;
            for(int i=0;i<app->ref_count&&(size_t)n<response_size;i++) if(app->refs[i].is_text&&!app->refs[i].excluded){char source[APP_PATH_CAP*2];json_escape(app->refs[i].path,source,sizeof(source));n+=snprintf(response+n,response_size-(size_t)n,"%s\"%s\"",first_prompt_file?"":",",source);first_prompt_file=false;}
            if((size_t)n<response_size)n+=snprintf(response+n,response_size-(size_t)n,"],\"additional_prompt_count\":%d,\"references\":[",app->snapshot_profile.prompt_count);
            for(int i=0;i<app->snapshot_ref_count&&(size_t)n<response_size;i++){char ref[APP_PATH_CAP*2];json_escape(app->snapshot_refs[i],ref,sizeof(ref));n+=snprintf(response+n,response_size-(size_t)n,"%s\"%s\"",i?",":"",ref);}
            if((size_t)n<response_size)n+=snprintf(response+n,response_size-(size_t)n,"],\"generation_options\":{\"model\":\"%s\",\"aspect\":\"%s\",\"set_count\":%d},\"postprocessing\":{\"layout\":\"%s\",\"pixel_pitch\":\"%s\",\"pitch_preset\":\"%s\",\"align_blobs\":\"%s\",\"magenta\":\"%s\",\"edge_alpha\":\"%s\",\"rearrange\":%s,\"tile_size_count\":%d,\"emit\":\"%s\"},\"output_path\":\"%s\"}",model,app->snapshot_profile.aspect,app->snapshot_profile.set_count,layout,pitch,preset,align,magenta,alpha,app->snapshot_profile.rearrange?"true":"false",app->snapshot_profile.tile_count,app->snapshot_debug?"all":"final",escaped_output);
        }
    } else if (!strcmp(action, "generation.confirm") || !strcmp(action, "app.confirm")) {
        if (!strcmp(app->pending_action, "run.cancel")) {
            AppRun *run = get_run(app);
            if (run && run->child > 0) kill(-run->child, SIGINT);
            app->pending_action[0] = 0;
            snprintf(response, response_size, "{\"ok\":true}"); return response;
        }
        if (!strcmp(app->pending_action, "reference.trash")) {
            bool removing_open_prompt = !strcmp(app->pending_path, app->prompt_path);
            bool moved = move_reference_to_trash(app, app->pending_path);
            app->pending_action[0] = app->pending_path[0] = 0;
            if (moved) { scan_refs(app); if (removing_open_prompt) { app->prompt_path[0]=app->prompt_edit[0]=0; app->prompt_dirty=app->prompt_editor_focused=false; app->edit_field[0]=0; app->show_full_image=false; } app->selected_ref = -1; save_reference_state(app); set_message(app, "Item moved to workspace/trash"); snprintf(response, response_size, "{\"ok\":true}"); }
            else { app->pending_action[0] = app->pending_path[0] = 0; set_message(app, "Could not move reference to trash"); snprintf(response, response_size, "{\"ok\":false,\"error\":\"move to trash failed\"}"); }
            return response;
        }
        if (!strcmp(app->pending_action, "profile.delete")) {
            if (app->profile_count <= 1) snprintf(response, response_size, "{\"ok\":false,\"error\":\"keep at least one profile\"}");
            else { for (int i = app->active_profile; i + 1 < app->profile_count; i++) app->profiles[i] = app->profiles[i + 1]; app->profile_count--; app->active_profile = 0; save_profiles(app); snprintf(response, response_size, "{\"ok\":true}"); }
            app->pending_action[0] = 0; return response;
        }
        if (!app->preview_ready) { snprintf(response, response_size, "{\"ok\":false,\"error\":\"prepare a preview before confirming\"}"); return response; }
        if (app->run_count >= APP_MAX_RUNS || app->active_run >= 0) snprintf(response, response_size, "{\"ok\":false,\"error\":\"run active or run limit reached\"}");
        else {
            AppRun *run = &app->runs[app->run_count]; memset(run, 0, sizeof(*run)); run->pipe_fd = run->err_fd = -1;
            copy_text(run->id, sizeof(run->id), app->snapshot_run_id); copy_text(run->dir, sizeof(run->dir), app->snapshot_run_dir); make_dirs(run->dir);
            copy_text(run->prompt, sizeof(run->prompt), app->snapshot_prompt); copy_text(run->resolved_prompt, sizeof(run->resolved_prompt), app->snapshot_resolved);
            run->debug = app->snapshot_debug; run->follow_log = true;
            bool started = false;
            if (app->rerun_pixel_only && *app->rerun_source) {
                copy_text(run->source, sizeof(run->source), app->rerun_source);
                run->started = GetTime(); run->state = RUN_RUNNING;
                started = start_pixel(app, run);
            } else started = start_generation(app, run);
            app->rerun_pixel_only = false; app->rerun_source[0] = 0; app->preview_ready = false;
            if (started) { app->active_run = app->run_count; app->selected_run = app->run_count++; app->tab = TAB_PROGRESS; app->details_view = app->settings_view = false; set_message(app, "Run started"); snprintf(response, response_size, "{\"ok\":true,\"run\":\"%s\"}", run->id); }
            else { run->state = RUN_FAILED; run->finished = GetTime(); if (!*run->error) copy_text(run->error, sizeof(run->error), "could not start configured process"); snprintf(response, response_size, "{\"ok\":false,\"error\":\"%s\"}", run->error); }
        }
    } else if (!strcmp(action, "app.cancel")) {
        if (!strcmp(app->pending_action, "reference.trash")) { app->pending_action[0] = app->pending_path[0] = 0; set_message(app, "Move to trash canceled"); }
        if (!strcmp(app->pending_action, "run.cancel")) { app->pending_action[0] = 0; AppRun *run = get_run(app); if (run) run->cancel_pending = false; set_message(app, "Cancellation dismissed"); }
        if (!strcmp(app->pending_action, "profile.delete")) { app->pending_action[0] = 0; set_message(app, "Profile delete canceled"); }
        app->snapshot_profile.model[0] = 0; app->rerun_pixel_only = false; app->rerun_source[0] = 0; app->preview_ready = false;
        snprintf(response, response_size, "{\"ok\":true}");
    } else if (!strcmp(action, "generation.cancel")) {
        AppRun *run = get_run(app);
        if (run && run->state == RUN_RUNNING) { copy_text(app->pending_action, sizeof(app->pending_action), "run.cancel"); run->cancel_pending = true; set_message(app, "Cancel pending; confirm to interrupt child"); snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}"); }
        else snprintf(response, response_size, "{\"ok\":false,\"error\":\"no active run\"}");
    } else if (!strcmp(action, "reference.exclude") || !strcmp(action, "reference.trash") || !strcmp(action, "reference.delete")) {
        json_string(request, "path", path, sizeof(path));
        for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, path)) {
            if (!strcmp(action, "reference.exclude")) app->refs[i].excluded = !app->refs[i].excluded;
            else { copy_text(app->pending_action, sizeof(app->pending_action), "reference.trash"); copy_text(app->pending_path, sizeof(app->pending_path), path); set_message(app, "Move reference to trash pending; confirm or cancel"); snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}"); return response; }
            save_reference_state(app); set_message(app, "Reference updated"); snprintf(response, response_size, "{\"ok\":true}"); return response;
        }
        snprintf(response, response_size, "{\"ok\":false,\"error\":\"reference not found\"}");
    } else if (!strcmp(action,"reference.select") || !strcmp(action,"reference.view")) {
        bool found=false;
        for(int i=0;i<app->ref_count;i++) if(!strcmp(path,app->refs[i].path)){found=true;if(!strcmp(action,"reference.select")){app->selected_ref=i;app->refs[i].selected=!app->refs[i].selected;}else{if(app->refs[i].is_text&&!select_prompt_reference(app,&app->refs[i]))break;app->selected_ref=i;app->tab=TAB_REFERENCES;app->show_full_image=true;app->image_zoom=1.0f;}}
        snprintf(response,response_size,found?"{\"ok\":true}":"{\"ok\":false,\"error\":\"reference not found\"}");
    } else if (!strcmp(action,"references.size")) {
        int size=atoi(value); if(size>=56&&size<=216)app->thumb_size=size; snprintf(response,response_size,"{\"ok\":true}");
    } else if (!strcmp(action, "reference.import")) {
        if (!*path || !import_reference_file(app,path)) snprintf(response, response_size, "{\"ok\":false,\"error\":\"reference import failed\"}");
        else { save_reference_state(app); set_message(app, "Reference copied into workspace"); snprintf(response, response_size, "{\"ok\":true}"); }
    } else if (!strcmp(action, "profile.select")) {
        int idx = atoi(value); if (idx >= 0 && idx < app->profile_count) app->active_profile = idx;
        snprintf(response, response_size, "{\"ok\":true}");
    } else if (!strcmp(action, "profile.add") || !strcmp(action, "profile.duplicate")) {
        if (app->profile_count >= APP_MAX_PROFILES) snprintf(response, response_size, "{\"ok\":false,\"error\":\"profile limit reached\"}");
        else { AppProfile *p = &app->profiles[app->profile_count]; *p = app->profiles[app->active_profile]; snprintf(p->name, sizeof(p->name), "Profile %d", app->profile_count + 1); app->active_profile = app->profile_count++; save_profiles(app); snprintf(response, response_size, "{\"ok\":true}"); }
    } else if (!strcmp(action,"profile.prompt.add")) {
        AppProfile *p=&app->profiles[app->active_profile]; if(p->prompt_count>=APP_MAX_PROMPTS)snprintf(response,response_size,"{\"ok\":false,\"error\":\"prompt part limit reached\"}");else{app->active_prompt_part=p->prompt_count;snprintf(p->prompts[p->prompt_count++],sizeof(p->prompts[0]),"Describe the image.");save_profiles(app);snprintf(response,response_size,"{\"ok\":true}");}
    } else if (!strcmp(action,"generation.debug")) {
        app->debug_stages = !app->debug_stages;
        snprintf(response,response_size,"{\"ok\":true,\"debug\":%s}",app->debug_stages?"true":"false");
    } else if (!strcmp(action, "profile.delete")) {
        copy_text(app->pending_action, sizeof(app->pending_action), "profile.delete"); set_message(app, "Profile delete pending; confirm or cancel"); snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}");
    } else if (!strcmp(action, "profile.set")) {
        char field[64]; json_string(request, "field", field, sizeof(field)); AppProfile *p = &app->profiles[app->active_profile];
        if (!strcmp(field, "model")) copy_text(p->model, sizeof(p->model), value);
        else if (!strcmp(field, "aspect")) copy_text(p->aspect, sizeof(p->aspect), value);
        else if (!strcmp(field, "layout")) copy_text(p->layout, sizeof(p->layout), value);
        else if (!strcmp(field, "pixel_pitch")) copy_text(p->pixel_pitch, sizeof(p->pixel_pitch), value);
        else if (!strcmp(field, "pitch_preset")) copy_text(p->pitch_preset, sizeof(p->pitch_preset), value);
        else if (!strcmp(field, "pitch_group_layout")) copy_text(p->pitch_group_layout, sizeof(p->pitch_group_layout), value);
        else if (!strcmp(field, "align_blobs")) copy_text(p->align_blobs, sizeof(p->align_blobs), value);
        else if (!strcmp(field, "magenta")) copy_text(p->magenta, sizeof(p->magenta), value);
        else if (!strcmp(field, "edge_alpha")) copy_text(p->edge_alpha, sizeof(p->edge_alpha), value);
        else if (!strcmp(field, "name")) copy_text(p->name, sizeof(p->name), value);
        else if (!strcmp(field, "padding")) p->padding = atoi(value);
        else if (!strcmp(field, "rearrange")) p->rearrange = atoi(value) != 0;
        else if (!strcmp(field, "slim_upload")) p->slim_upload = atoi(value) != 0;
        else if (!strncmp(field,"prompt.",7)) { int i=atoi(field+7); if(i<0||i>=p->prompt_count)copy_text(response,response_size,"{\"ok\":false,\"error\":\"prompt part index out of range\"}");else copy_text(p->prompts[i],sizeof(p->prompts[i]),value); }
        else if (!strncmp(field,"set.",4)) { int i=atoi(field+4); if(i<0||i>=APP_MAX_PROMPTS)copy_text(response,response_size,"{\"ok\":false,\"error\":\"model setting index out of range\"}");else{if(i>=p->set_count)p->set_count=i+1;copy_text(p->sets[i],sizeof(p->sets[i]),value);} }
        else if (!strncmp(field,"tile_size.",10)) { int i=atoi(field+10),size=atoi(value);if(i<0||i>=8||size<1||size>1024)copy_text(response,response_size,"{\"ok\":false,\"error\":\"invalid tile size\"}");else{if(i>=p->tile_count)p->tile_count=i+1;p->tile_sizes[i]=size;} }
        else copy_text(response, response_size, "{\"ok\":false,\"error\":\"unknown profile field\"}");
        if (response[0] == 0) { save_profiles(app); snprintf(response, response_size, "{\"ok\":true}"); }
    } else if (!strcmp(action, "run.rerun")) {
        AppRun *old = get_run(app);
        if (!old || app->active_run >= 0) snprintf(response, response_size, "{\"ok\":false,\"error\":\"no completed run or another run active\"}");
        else {
            app->snapshot_profile = app->profiles[app->active_profile];
            copy_text(app->snapshot_prompt, sizeof(app->snapshot_prompt), old->prompt);
            copy_text(app->rerun_source, sizeof(app->rerun_source), old->source);
            app->rerun_pixel_only = true; app->snapshot_debug = true; app->preview_ready = true;
            make_run_id(app, app->snapshot_run_id, sizeof(app->snapshot_run_id));
            path_join(app->snapshot_run_dir, sizeof(app->snapshot_run_dir), app->runs_path, app->snapshot_run_id);
            resolve_prompt(app, old->prompt, app->snapshot_resolved, sizeof(app->snapshot_resolved), NULL);
            app->tab = TAB_PROGRESS; app->details_view = true; app->settings_view = false; snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}");
            set_message(app, "Pixel rerun preview ready; confirm to start");
        }
    } else if (!strcmp(action,"run.select")) {
        int idx=atoi(value); if(idx>=0&&idx<app->run_count){app->selected_run=idx;app->tab=TAB_PROGRESS;app->details_view=app->settings_view=false;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"run index out of range\"}");
    } else if (!strcmp(action,"output.select")) {
        AppRun *run=get_run(app); int idx=atoi(value); if(run&&idx>=0&&idx<run->output_count){run->selected_output=idx;app->tab=TAB_OUTPUT;app->details_view=app->settings_view=false;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"output index out of range\"}");
    } else if (!strcmp(action,"stage.select")) {
        AppRun *run=get_run(app); int number=atoi(value); bool found=false; if(run)for(int i=0;i<run->stage_count;i++)if(run->stages[i].number==number){run->selected_stage=i;app->tab=TAB_PROGRESS;found=true;break;}
        if(found)snprintf(response,response_size,"{\"ok\":true}");else snprintf(response,response_size,"{\"ok\":false,\"error\":\"stage not emitted\"}");
    } else if (!strcmp(action,"stage.compare")) {
        AppRun *run=get_run(app); char mode[32]; json_string(request,"value",mode,sizeof(mode));
        if(run){app->show_compare=true;app->compare_mode=!strcmp(mode,"final")?1:!strcmp(mode,"previous")?2:0;app->compare_stage=run->selected_stage+1;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"no run selected\"}");
    } else if (!strcmp(action,"viewer.zoom")) {
        app->image_zoom+=atof(value); if(app->image_zoom<0.1f)app->image_zoom=0.1f;if(app->image_zoom>16.0f)app->image_zoom=16.0f;snprintf(response,response_size,"{\"ok\":true}");
    } else if (!strcmp(action, "run.note")) {
        AppRun *run = get_run(app); char note[APP_TEXT_CAP]; json_string(request, "text", note, sizeof(note));
        if (run) { copy_text(run->note, sizeof(run->note), note); char p[APP_PATH_CAP]; path_join(p, sizeof(p), run->dir, "note.txt"); write_text(p, note); snprintf(response, response_size, "{\"ok\":true}"); }
        else snprintf(response, response_size, "{\"ok\":false,\"error\":\"no run selected\"}");
    } else if (!strcmp(action, "run.logs.clear")) { AppRun *run = get_run(app); if (run) run->log_count = 0; snprintf(response, response_size, "{\"ok\":true,\"underlying_log_retained\":true}"); }
    else if (!strcmp(action, "run.output.open")) {
        AppRun *run = get_run(app); bool allowed = false;
        if (run) { for (int i=0;i<run->output_count;i++) if (!strcmp(path,run->outputs[i].path)) allowed = true; for (int i=0;i<run->stage_count;i++) if (!strcmp(path,run->stages[i].path)) allowed = true; }
        if (allowed && open_path(path)) snprintf(response,response_size,"{\"ok\":true}"); else snprintf(response,response_size,"{\"ok\":false,\"error\":\"path is not a selected run artifact\"}");
    }
    else if (!strcmp(action, "app.action")) snprintf(response, response_size, "{\"ok\":false,\"error\":\"unknown action\"}");
    else snprintf(response, response_size, "{\"ok\":false,\"error\":\"unknown action\"}");
    return response;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Route reference trash actions | v 3.3.0 | e1c824 */
/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */

/* Receive bounded command datagrams from local MCP clients and return results. */
static void poll_enet(App *app) {
    if (!app->enet_host || GetTime() - app->last_enet_fetch < 0.01) return;
    app->last_enet_fetch = GetTime();
    ENetEvent event;
    while (enet_host_service((ENetHost *)app->enet_host, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) app->enet_peer = event.peer;
        else if (event.type == ENET_EVENT_TYPE_DISCONNECT) { if (app->enet_peer == event.peer) app->enet_peer = NULL; }
        else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            char request[MCP_FRAME_CAP], response[MCP_FRAME_CAP];
            size_t n = event.packet->dataLength; if (n >= sizeof(request)) n = sizeof(request) - 1;
            memcpy(request, event.packet->data, n); request[n] = 0;
            AppDispatch(app, request, response, sizeof(response));
            ENetPacket *packet = enet_packet_create(response, strlen(response) + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event.peer, ENET_CHANNEL, packet); enet_host_flush((ENetHost *)app->enet_host);
            enet_packet_destroy(event.packet);
        }
    }
}

/* Poll run process state, output stages, and ENet commands. */
void AppUpdate(App *app) {
    poll_enet(app);
    for (int i = 0; i < app->run_count; i++) {
        AppRun *run = &app->runs[i];
        if (run->state == RUN_RUNNING) update_child(app, run);
    }
}

/* Draw a button and return true on a left click. */
static bool button(Rectangle rect, const char *label, bool active) {
    Vector2 m = GetMousePosition(); bool over = CheckCollisionPointRec(m, rect);
    DrawRectangleRec(rect, over ? UI_PANEL_ALT : UI_PANEL);
    DrawRectangleLinesEx(rect, 1, active ? UI_ACCENT : (over ? UI_TEXT : (Color){67, 70, 79, 255}));
    ui_draw_text(label, (int)rect.x + 10, (int)rect.y + 8, 16, active ? UI_ACCENT : UI_TEXT);
    return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */
/* Draw const char* text in Rectangle bounds at int size with Color tint; return void. */
static void draw_wrapped_text(const char *text, Rectangle rect, int font_size, Color color) {
    char line[2048]; size_t used=0; int y=(int)rect.y;
    const char *p=text?text:"";
    while(*p&&y+font_size<=rect.y+rect.height){
        if(*p=='\r'){p++;continue;}
        if(*p=='\n'){line[used]=0;if(used)ui_draw_text(line,(int)rect.x,y,font_size,color);used=0;y+=font_size+6;p++;continue;}
        if((*p==' '||*p=='\t')&&!used){p++;continue;}
        if(used+1>=sizeof(line)){line[used]=0;ui_draw_text(line,(int)rect.x,y,font_size,color);used=0;y+=font_size+6;}
        line[used++]=*p++;line[used]=0;
        if(ui_measure_text(line,font_size)<=(int)rect.width)continue;
        size_t split=used;
        while(split&&line[split-1]!=' ')split--;
        if(split){
            size_t keep=split-1,tail=used-split;
            line[keep]=0;if(keep)ui_draw_text(line,(int)rect.x,y,font_size,color);
            memmove(line,line+split,tail);used=tail;line[used]=0;y+=font_size+6;
        }else if(used>1){
            char last=line[used-1];line[--used]=0;ui_draw_text(line,(int)rect.x,y,font_size,color);
            line[0]=last;used=1;line[1]=0;y+=font_size+6;
        }
    }
    if(used&&y+font_size<=rect.y+rect.height){line[used]=0;ui_draw_text(line,(int)rect.x,y,font_size,color);}
}

/* Draw the prompt editor for App* app in Rectangle bounds; return void. */
static void draw_prompt_editor(App *app, Rectangle rect) {
    char *content = app->prompt_edit;
    if (!app->prompt_dirty && !read_text(app->prompt_path, content, sizeof(app->prompt_edit))) content[0] = 0;
    DrawRectangleRec(rect, UI_PANEL); DrawRectangleLinesEx(rect, 1, app->prompt_dirty ? UI_WARN : UI_PANEL_ALT);
    draw_wrapped_text(content,(Rectangle){rect.x+14,rect.y+12,rect.width-28,rect.height-24},22,UI_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app->prompt_dirty = true; app->prompt_editor_focused = true; app->edit_field[0] = 0; }
    if (app->prompt_editor_focused) {
        int key = GetCharPressed();
        while (key > 0) {
            size_t n = strlen(content);
            if (key >= 32 && key <= 125 && n + 1 < sizeof(app->prompt_edit)) { content[n] = (char)key; content[n + 1] = 0; }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) { size_t n = strlen(content); if (n) content[n - 1] = 0; }
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) content[0] = 0;
        if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_CONTROL)) save_open_prompt(app);
        if (IsKeyPressed(KEY_ENTER) && !IsKeyDown(KEY_LEFT_CONTROL)) { size_t n = strlen(content); if (n + 1 < sizeof(app->prompt_edit)) { content[n] = '\n'; content[n + 1] = 0; } }
    }
}

/* Draw one editable profile field for App* app from const char* values and Rectangle bounds; return void. */
static void draw_profile_input(App *app, const char *field, const char *label, const char *value, Rectangle rect) {
    ui_draw_text(label, (int)rect.x, (int)rect.y, 15, UI_MUTED);
    Rectangle box = {rect.x, rect.y + 22, rect.width, 38};
    bool editing = !strcmp(app->edit_field, field);
    DrawRectangleRec(box, editing ? UI_PANEL_ALT : UI_PANEL);
    DrawRectangleLinesEx(box, 1, editing ? UI_ACCENT : UI_PANEL_ALT);
    if (CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        copy_text(app->edit_field, sizeof(app->edit_field), field);
        copy_text(app->field_buffer, sizeof(app->field_buffer), value);
        editing = true;
    }
    ui_draw_text(editing ? app->field_buffer : value,(int)box.x+9,(int)box.y+10,17,UI_TEXT);
    if (!editing) return;
    int key = GetCharPressed();
    while (key > 0) {
        size_t n = strlen(app->field_buffer);
        if (key >= 32 && key <= 125 && n + 1 < sizeof(app->field_buffer)) { app->field_buffer[n] = (char)key; app->field_buffer[n + 1] = 0; }
        key = GetCharPressed();
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) app->field_buffer[0] = 0;
    if (IsKeyPressed(KEY_BACKSPACE)) { size_t n = strlen(app->field_buffer); if (n) app->field_buffer[n - 1] = 0; }
    if (IsKeyPressed(KEY_ESCAPE)) app->edit_field[0] = 0;
    if (IsKeyPressed(KEY_ENTER)) {
        char ef[128], ev[APP_TEXT_CAP * 2], req[APP_TEXT_CAP * 2 + 256];
        json_escape(field, ef, sizeof(ef)); json_escape(app->field_buffer, ev, sizeof(ev));
        snprintf(req, sizeof(req), "{\"action\":\"profile.set\",\"field\":\"%s\",\"value\":\"%s\"}", ef, ev);
        AppDispatch(app, req, s_action_response, sizeof(s_action_response)); app->edit_field[0] = 0;
    }
}

/* agent: gpt-6.1-sol | 2026-10-04 | Capture plus menu clicks exclusively | v 3.3.0 | 7ce4da */
/* Draw reference thumbnails and actions for App* app; return void. */
static void draw_references(App *app) {
    int w = GetScreenWidth(),h=GetScreenHeight();
    Rectangle menu={80,132,220,100};
    bool menu_was_open=app->show_add_menu;
    int margin = 40, top = 126,bottom=h-44;
    int cols = w / 250; if (cols < 1) cols = 1;
    int gap = 20, tile = (w - margin * 2 - gap * (cols - 1)) / cols;
    if (tile > 260) tile = 260;
    int visible_count=app->ref_count+1;
    int rows=(visible_count+cols-1)/cols,max_scroll=rows*(tile+gap)-(bottom-top);if(max_scroll<0)max_scroll=0;
    Rectangle area={24,(float)top,(float)w-48,(float)(bottom-top)};
    if(CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel!=0)app->reference_scroll-=(int)(wheel*120);}
    if(app->reference_scroll<0)app->reference_scroll=0;
    if(app->reference_scroll>max_scroll)app->reference_scroll=max_scroll;
    BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    int visible = 0;
    int add_x=margin,add_y=top-app->reference_scroll;
    Rectangle add_box={(float)add_x,(float)add_y,(float)tile,(float)tile};
    DrawRectangleRec(add_box,UI_PANEL); DrawRectangleLinesEx(add_box,1,app->show_add_menu?UI_ACCENT:UI_PANEL_ALT);
    DrawCircleLines(add_x+tile/2,add_y+tile/2,25,UI_ACCENT);
    DrawLine(add_x+tile/2-12,add_y+tile/2,add_x+tile/2+12,add_y+tile/2,UI_ACCENT);
    DrawLine(add_x+tile/2,add_y+tile/2-12,add_x+tile/2,add_y+tile/2+12,UI_ACCENT);
    if(CheckCollisionPointRec(GetMousePosition(),add_box)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!(app->show_add_menu&&CheckCollisionPointRec(GetMousePosition(),menu)))app->show_add_menu=!app->show_add_menu;
    visible++;
    for (int i = 0; i < app->ref_count; i++) {
        AppReference *r = &app->refs[i];
        int col = visible % cols, row = visible / cols;
        int x = margin + col * (tile + gap), y = top + row * (tile + gap)-app->reference_scroll; visible++;
        Rectangle box = {(float)x, (float)y, (float)tile, (float)tile};
        DrawRectangleRec(box, UI_PANEL);
        if (r->is_text) {
            DrawRectangleRec(box,UI_PANEL_ALT);
            draw_wrapped_text(r->preview,(Rectangle){box.x+14,box.y+14,box.width-28,box.height-54},14,UI_TEXT);
        } else if (r->thumb.id) {
            float scale = fminf(box.width / r->thumb.width, box.height / r->thumb.height);
            Rectangle image = {box.x + (box.width - r->thumb.width * scale) * 0.5f, box.y + (box.height - r->thumb.height * scale) * 0.5f, r->thumb.width * scale, r->thumb.height * scale};
            DrawTexturePro(r->thumb, (Rectangle){0,0,(float)r->thumb.width,(float)r->thumb.height}, image, (Vector2){0,0}, 0, WHITE);
        }
        DrawRectangle((int)box.x,(int)(box.y+box.height-30),(int)box.width,30,(Color){20,22,26,225});
        ui_draw_text(TextFormat("%.30s",r->name),(int)box.x+8,(int)(box.y+box.height-24),14,UI_TEXT);
        draw_reference_icons(r, box);
        DrawRectangleLinesEx(box, app->selected_ref == i ? 3 : 1, app->selected_ref == i ? UI_ACCENT : UI_PANEL_ALT);
        if (CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Rectangle exclude = {box.x + box.width - 58, box.y + 6, 24, 24};
            Rectangle trash = {box.x + box.width - 28, box.y + 6, 22, 24};
            if (!app->show_add_menu && CheckCollisionPointRec(GetMousePosition(), exclude)) { app->selected_ref=i; r->excluded = !r->excluded; save_reference_state(app); }
            else if (!app->show_add_menu && CheckCollisionPointRec(GetMousePosition(), trash)) { app->selected_ref=i; copy_text(app->pending_action, sizeof(app->pending_action), "reference.trash"); copy_text(app->pending_path, sizeof(app->pending_path), r->path); }
            else if (!app->show_add_menu && r->is_text) { if (select_prompt_reference(app,r)) { app->selected_ref=i; app->show_full_image=true; } }
            else if (!app->show_add_menu) { app->selected_ref=i; app->show_full_image = true; app->image_zoom = 1; app->image_pan_x = app->image_pan_y = 0; }
        }
    }
    EndScissorMode();
    bool left_click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if(app->show_add_menu&&(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)||(left_click&&!CheckCollisionPointRec(GetMousePosition(),add_box)&&!CheckCollisionPointRec(GetMousePosition(),menu)))) app->show_add_menu=false;
    if(app->show_add_menu){
        DrawRectangleRec(menu,UI_PANEL);DrawRectangleLinesEx(menu,1,UI_ACCENT);
        if(menu_was_open&&button((Rectangle){menu.x+10,menu.y+10,menu.width-20,36},"Import reference",false)){app->show_add_menu=false;import_reference_dialog(app);}
        if(menu_was_open&&button((Rectangle){menu.x+10,menu.y+54,menu.width-20,36},"New prompt",false)){app->show_add_menu=false;if(create_prompt_file(app))set_message(app,"Empty prompt created");}
    }
    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        for (unsigned int i = 0; i < dropped.count; i++) if (is_image_path(dropped.paths[i]) || is_text_path(dropped.paths[i])) import_reference_file(app, dropped.paths[i]);
        UnloadDroppedFiles(dropped); save_reference_state(app); set_message(app, "References added");
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Capture plus menu clicks exclusively | v 3.3.0 | 7ce4da */
/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */

/* agent: gpt-6.1-sol | 2026-10-04 | Fix image viewer zoom scaling | v 3.3.0 | c3c385 */
/* Draw Texture2D at pointer-controlled scale in App* app; return void. */
static void draw_image_view(App *app, Texture2D texture, int x, int y, int width, int height) {
    if (!texture.id) { ui_draw_text("No image selected", x + 20, y + 24, 18, UI_MUTED); return; }
    Rectangle area = {(float)x,(float)y,(float)width,(float)height};
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) app->image_zoom *= 1.15f;
    if (IsKeyPressed(KEY_MINUS)) app->image_zoom /= 1.15f;
    if (CheckCollisionPointRec(GetMousePosition(),area)) { float wheel=GetMouseWheelMove(); if(wheel>0)app->image_zoom*=1.12f; else if(wheel<0)app->image_zoom/=1.12f; }
    if (app->image_zoom < 0.1f) app->image_zoom = 0.1f;
    if (app->image_zoom > 16.0f) app->image_zoom = 16.0f;
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) { Vector2 d = GetMouseDelta(); app->image_pan_x += d.x; app->image_pan_y += d.y; }
    float z = app->image_zoom;
    float scale = fminf((float)width / texture.width, (float)height / texture.height) * z;
    float iw = texture.width * scale, ih = texture.height * scale;
    Rectangle dst = {x + (width - iw) * 0.5f + app->image_pan_x, y + (height - ih) * 0.5f + app->image_pan_y, iw, ih};
    DrawRectangle(x, y, width, height, (Color){27,28,32,255});
    if (s_image_shader.id) BeginShaderMode(s_image_shader);
    DrawTexturePro(texture, (Rectangle){0,0,(float)texture.width,(float)texture.height}, dst, (Vector2){0,0}, 0, WHITE);
    if (s_image_shader.id) EndShaderMode();
    ui_draw_text(TextFormat("%dx%d  %.2fx  wheel/+- zoom, middle drag pan", texture.width, texture.height, z), x + 8, y + height - 23, 13, UI_MUTED);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Fix image viewer zoom scaling | v 3.3.0 | c3c385 */

/* Draw previous/next controls for retained run history. */
static void draw_run_picker(App *app, int x, int y) {
    if (!app->run_count) return;
    if (button((Rectangle){x,y,32,28}, "<", false) && app->selected_run > 0) app->selected_run--;
    if (button((Rectangle){x+38,y,32,28}, ">", false) && app->selected_run + 1 < app->run_count) app->selected_run++;
    ui_draw_text(TextFormat("%d/%d", app->selected_run + 1, app->run_count), x + 78, y + 6, 14, UI_MUTED);
}

/* Describe only the processor stages present in the current run. */
static const char *stage_description(int number) {
    switch (number) {
        case 40: return "Source baseline; compare with every later stage.";
        case 36: return "Layout boundaries, detection method, confidence, and grid.";
        case 35: return "Phase diagnostics only; not a processing mode.";
        case 34: return "Blob placement counts and confidence.";
        case 33: return "Detected or forced pixel pitch and confidence.";
        case 32: return "Fitted sprite grid linked to pitch and lattice decisions.";
        case 37: return "Lattice phase fit diagnostics.";
        case 39: return "Lattice phase shift diagnostics.";
        case 31: return "Reconstructed sprites before expansion.";
        case 30: return "Shared pitch result and measured pitch.";
        case 28: return "Weighted edge-alpha candidates.";
        case 27: return "Weighted edge-alpha support values.";
        case 26: return "Weighted alpha edits; requires magenta skip.";
        case 29: return "Regularized logical grid.";
        case 25: return "Outside-magenta mask when classification ran.";
        case 24: return "Source support used by bounded cleanup.";
        case 22: return "Support-scoped magenta cleanup.";
        case 20: return "Magenta cleanup result.";
        case 38: return "Blob ownership during rearrangement.";
        case 9: return "Recovered and unresolved boundary pixels.";
        case 10: return "Trim and repack bounds.";
        default: return "Processor diagnostic artifact.";
    }
}

/* Change only the profile field that is meaningful for the selected stage. */
static bool adjust_stage_option(AppProfile *p, int stage) {
    if (stage == 36) copy_text(p->layout,sizeof(p->layout),!strcmp(p->layout,"auto")?"4x4":"auto");
    else if (stage == 34 || stage == 38) copy_text(p->align_blobs,sizeof(p->align_blobs),!strcmp(p->align_blobs,"auto")?"on":!strcmp(p->align_blobs,"on")?"off":"auto");
    else if (stage == 33 || stage == 30) copy_text(p->pixel_pitch,sizeof(p->pixel_pitch),!strcmp(p->pixel_pitch,"auto")?"6":"auto");
    else if (stage == 26 || stage == 27 || stage == 28) { copy_text(p->edge_alpha,sizeof(p->edge_alpha),!strcmp(p->edge_alpha,"weighted")?"legacy":"weighted"); if (!strcmp(p->edge_alpha,"weighted")) copy_text(p->magenta,sizeof(p->magenta),"skip"); }
    else if (stage == 20 || stage == 22 || stage == 24 || stage == 25) copy_text(p->magenta,sizeof(p->magenta),!strcmp(p->magenta,"outside")?"all":!strcmp(p->magenta,"all")?"skip":"outside");
    else if (stage == 10) p->rearrange = !p->rearrange;
    else return false;
    return true;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */
/* Draw one final image for App* app; return void. */
static void draw_output(App *app) {
    AppRun *run = get_run(app); int w = GetScreenWidth(), h = GetScreenHeight();
    Texture2D texture = {0};
    if (run && run->selected_output >= 0 && run->selected_output < run->output_count) texture = run->outputs[run->selected_output].texture;
    draw_image_view(app, texture, 40, 116, w - 80, h - 156);
}

/* Draw the run status view for App* app; return void. */
static void draw_progress(App *app) {
    AppRun *run = get_run(app); int w = GetScreenWidth();
    const char *state = !run ? "Ready" : run->state == RUN_RUNNING ? "Working" : run->state == RUN_SUCCEEDED ? "Complete" : run->state == RUN_FAILED ? "Failed" : "Canceled";
    Color tone = !run || run->state == RUN_RUNNING ? UI_ACCENT : run->state == RUN_SUCCEEDED ? UI_GOOD : UI_BAD;
    ui_draw_text(state, 48, 132, 30, tone);
    const char *description = !run ? "Create a request from the Prompt page." : run->state == RUN_RUNNING ? run->current_step : run->state == RUN_SUCCEEDED ? "Output is ready on the Output page." : run->state == RUN_FAILED ? "Run ended with an issue. Open Details for the log." : "Run canceled.";
    draw_wrapped_text(description,(Rectangle){48,184,(float)w-96,54},22,UI_TEXT);
    if (!run) return;
    ui_draw_text(run->id, 48, 250, 17, UI_MUTED);
    float progress = run->state == RUN_SUCCEEDED ? 1.0f : 0.0f;
    Rectangle bar = {48, 296, (float)w - 96, 16};
    DrawRectangleRec(bar, UI_PANEL);
    if (progress > 0) DrawRectangleRec((Rectangle){bar.x,bar.y,bar.width*progress,bar.height}, tone);
    else if (run->state == RUN_RUNNING) {
        float segment=fminf(220,bar.width),phase=fmodf((float)GetTime()*0.45f,2.0f);if(phase>1)phase=2-phase;
        DrawRectangleRec((Rectangle){bar.x+(bar.width-segment)*phase,bar.y,segment,bar.height},tone);
    }
    DrawRectangleLinesEx(bar, 1, UI_PANEL_ALT);
    if (run->state == RUN_RUNNING) ui_draw_text(TextFormat("Running for %.0f seconds", GetTime() - run->started), 48, 332, 16, UI_MUTED);
}

/* Draw prompt details for App* app at int x/y with int width; return void. */
static void draw_prompt_details(App *app, int x, int y, int width) {
    int h = GetScreenHeight();
    ui_draw_text("Prompt file", x, y, 20, UI_TEXT);
    ui_draw_text(app->prompt_path,x,y+44,17,UI_MUTED);
    if (button((Rectangle){x+width-184,y+30,184,42}, "Open prompt file", false)) {
        char chosen[APP_PATH_CAP], escaped[APP_PATH_CAP*2], req[APP_PATH_CAP*2+128];
        if (choose_file(chosen,sizeof(chosen),false,false)) { json_escape(chosen,escaped,sizeof(escaped)); snprintf(req,sizeof(req),"{\"action\":\"prompt.open\",\"path\":\"%s\"}",escaped); AppDispatch(app,req,s_action_response,sizeof(s_action_response)); }
    }
    if (!app->preview_ready) {
        ui_draw_text("Generation", x, y+112, 20, UI_TEXT);
        ui_draw_text(TextFormat("Profile: %s  |  Model: %s",app->profiles[app->active_profile].name,app->profiles[app->active_profile].model),x,y+150,17,UI_MUTED);
        if (button((Rectangle){x,y+194,176,44}, "Prepare generation", false)) AppDispatch(app,"{\"action\":\"generation.preview\"}",s_action_response,sizeof(s_action_response));
        if (app->prompt_dirty) ui_draw_text("Unsaved prompt | Ctrl+Enter saves",x+198,y+208,16,UI_WARN);
        return;
    }
    Rectangle panel = {x, y+108, (float)width, (float)fmaxf(220,h-y-184)};
    DrawRectangleRec(panel,UI_PANEL); DrawRectangleLinesEx(panel,1,UI_ACCENT);
    ui_draw_text("Review request",x+18,y+122,20,UI_ACCENT);
    ui_draw_text(TextFormat("%s  |  %d references  |  %s",app->snapshot_profile.model,app->snapshot_ref_count,app->snapshot_profile.aspect),x+18,y+156,16,UI_MUTED);
    draw_wrapped_text(app->snapshot_resolved,(Rectangle){x+18,y+190,(float)width-36,panel.height-270},18,UI_TEXT);
    int actions_y=(int)(panel.y+panel.height-58);
    if (button((Rectangle){x+18,actions_y,136,42},"Start generation",false)) AppDispatch(app,"{\"action\":\"generation.confirm\"}",s_action_response,sizeof(s_action_response));
    if (button((Rectangle){x+170,actions_y,112,42},"Cancel",false)) AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
}

/* agent: gpt-6.1-sol | 2026-10-04 | Add prompt controls in details | v 3.3.0 | 5830e8 */
/* Draw reference details for App* app at int x/y with int width/height; return void. */
static void draw_reference_details(App *app, int x, int y, int width, int height) {
    int list_w = (int)(width * 0.54f), cols = list_w / 150; if (cols < 1) cols = 1;
    int tile = (list_w - (cols-1)*14) / cols; if (tile > 150) tile = 150;
    int matches=app->ref_count;
    int top=y,view_h=height,rows=(matches+cols-1)/cols,max_scroll=rows*(tile+14)-view_h;if(max_scroll<0)max_scroll=0;
    Rectangle list={(float)x,(float)top,(float)list_w,(float)view_h};
    if(CheckCollisionPointRec(GetMousePosition(),list)){float wheel=GetMouseWheelMove();if(wheel!=0)app->reference_scroll-=(int)(wheel*120);}
    if(app->reference_scroll<0)app->reference_scroll=0;
    if(app->reference_scroll>max_scroll)app->reference_scroll=max_scroll;
    BeginScissorMode((int)list.x,(int)list.y,(int)list.width,(int)list.height);
    int count = 0;
    for (int i=0;i<app->ref_count;i++) {
        AppReference *ref=&app->refs[i];
        int col=count%cols,row=count/cols,tx=x+col*(tile+14),ty=top+row*(tile+14)-app->reference_scroll; count++;
        Rectangle box={(float)tx,(float)ty,(float)tile,(float)tile};
        DrawRectangleRec(box,UI_PANEL);
        if(ref->is_text){DrawRectangleRec(box,UI_PANEL_ALT);draw_wrapped_text(ref->preview,(Rectangle){box.x+8,box.y+8,box.width-16,box.height-38},11,UI_TEXT);}
        else if(ref->thumb.id){float scale=fminf(box.width/ref->thumb.width,box.height/ref->thumb.height);Rectangle image={box.x+(box.width-ref->thumb.width*scale)*0.5f,box.y+(box.height-ref->thumb.height*scale)*0.5f,ref->thumb.width*scale,ref->thumb.height*scale};DrawTexturePro(ref->thumb,(Rectangle){0,0,(float)ref->thumb.width,(float)ref->thumb.height},image,(Vector2){0,0},0,WHITE);}
        DrawRectangle(tx,ty+tile-24,tile,24,(Color){20,22,26,225});ui_draw_text(TextFormat("%.20s",ref->name),tx+5,ty+tile-20,11,UI_TEXT);
        draw_reference_icons(ref,box);
        DrawRectangleLinesEx(box,app->selected_ref==i?3:1,app->selected_ref==i?UI_ACCENT:UI_PANEL_ALT);
        if(CheckCollisionPointRec(GetMousePosition(),box)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            Rectangle exclude={box.x+box.width-58,box.y+6,24,24};Rectangle trash={box.x+box.width-28,box.y+6,22,24};
            if(CheckCollisionPointRec(GetMousePosition(),exclude)){app->selected_ref=i;ref->excluded=!ref->excluded;save_reference_state(app);}
            else if(CheckCollisionPointRec(GetMousePosition(),trash)){app->selected_ref=i;copy_text(app->pending_action,sizeof(app->pending_action),"reference.trash");copy_text(app->pending_path,sizeof(app->pending_path),ref->path);}
            else if(ref->is_text){if(select_prompt_reference(app,ref)){app->selected_ref=i;app->show_full_image=true;}}
            else{app->selected_ref=i;app->show_full_image=true;app->image_zoom=1;app->image_pan_x=app->image_pan_y=0;}
        }
    }
    EndScissorMode();
    int sx=x+list_w+36,sw=width-list_w-36;
    if(app->selected_ref<0||app->selected_ref>=app->ref_count){ui_draw_text("Select a reference thumbnail.",sx,y+8,18,UI_MUTED);return;}
    AppReference *ref=&app->refs[app->selected_ref];
    ui_draw_text(ref->name,sx,y,20,UI_TEXT);
    ui_draw_text(ref->is_text?"Prompt file":TextFormat("%d x %d",ref->width,ref->height),sx,y+34,16,UI_MUTED);
    draw_wrapped_text(ref->path,(Rectangle){sx,y+66,(float)sw,70},14,UI_MUTED);
    int by=y+158;
    if(ref->is_text){
        if(button((Rectangle){sx,by,(float)fminf(sw,180),40},"Edit prompt",false)&&select_prompt_reference(app,ref))app->show_full_image=true;
        if(button((Rectangle){sx,by+52,(float)fminf(sw,180),40},ref->excluded?"Enable prompt":"Disable prompt",false)){ref->excluded=!ref->excluded;save_reference_state(app);}
        if(button((Rectangle){sx,by+104,(float)fminf(sw,180),40},"Move to trash",false)){copy_text(app->pending_action,sizeof(app->pending_action),"reference.trash");copy_text(app->pending_path,sizeof(app->pending_path),ref->path);}
    }else{
        if(button((Rectangle){sx,by,(float)fminf(sw,180),40},ref->excluded?"Include in generation":"Exclude from generation",false)){ref->excluded=!ref->excluded;save_reference_state(app);}
        if(button((Rectangle){sx,by+52,(float)fminf(sw,180),40},"Move to trash",false)){copy_text(app->pending_action,sizeof(app->pending_action),"reference.trash");copy_text(app->pending_path,sizeof(app->pending_path),ref->path);}
        if(button((Rectangle){sx,by+104,160,40},"View full size",false)){app->show_full_image=true;app->image_zoom=1;app->image_pan_x=app->image_pan_y=0;}
    }
    (void)height;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Add prompt controls in details | v 3.3.0 | 5830e8 */

/* Draw output details for App* app at int x/y with int width; return void. */
static void draw_output_details(App *app, int x, int y, int width) {
    AppRun *run=get_run(app);
    if(!run){ui_draw_text("No output is available yet.",x,y,20,UI_MUTED);return;}
    ui_draw_text("Run",x,y,18,UI_TEXT); draw_run_picker(app,x+90,y-4);
    ui_draw_text("Output",x,y+76,18,UI_TEXT);
    if(run->output_count){
        if(button((Rectangle){x,y+112,52,40},"<",false)&&run->selected_output>0)run->selected_output--;
        if(button((Rectangle){x+62,y+112,52,40},">",false)&&run->selected_output+1<run->output_count)run->selected_output++;
        int idx=run->selected_output>=0&&run->selected_output<run->output_count?run->selected_output:0;
        AppArtifact *out=&run->outputs[idx];
        ui_draw_text(out->name,x+132,y+108,18,UI_TEXT); ui_draw_text(TextFormat("%d x %d",out->width,out->height),x+132,y+138,16,UI_MUTED);
        draw_wrapped_text(out->path,(Rectangle){x,y+176,(float)width,54},14,UI_MUTED);
        if(button((Rectangle){x,y+246,160,42},"Open output file",false))open_path(out->path);
    } else ui_draw_text("This run has no final output yet.",x,y+112,18,UI_MUTED);
}

/* Draw progress details for App* app at int x/y with int width/height; return void. */
static void draw_progress_details(App *app, int x, int y, int width, int height) {
    AppRun *run=get_run(app);
    if(!run){ui_draw_text("No run details yet.",x,y,20,UI_MUTED);return;}
    draw_run_picker(app,x,y-4);
    ui_draw_text(TextFormat("%s | %s",run->id,run->state==RUN_RUNNING?"Working":run->state==RUN_SUCCEEDED?"Complete":run->state==RUN_FAILED?"Failed":"Canceled"),x+160,y,18,run->state==RUN_FAILED?UI_BAD:UI_TEXT);
    ui_draw_text(run->current_step,x+160,y+30,15,UI_MUTED);
    if(run->state==RUN_RUNNING&&button((Rectangle){x+width-170,y-8,170,40},"Cancel run",false)){copy_text(app->pending_action,sizeof(app->pending_action),"run.cancel");run->cancel_pending=true;}
    if(run->cancel_pending){
        ui_draw_text("Stop the active process?",x+160,y+62,15,UI_WARN);
        if(button((Rectangle){x+width-274,y+54,120,38},"Stop",false))AppDispatch(app,"{\"action\":\"app.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){x+width-138,y+54,120,38},"Continue",false))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }
    int stage_w=(int)fminf(520,width*0.42f), stage_y=y+126, stage_h=height-236;
    ui_draw_text("Processor stages",x,stage_y,18,UI_TEXT);
    if(run->stage_count){
        if(run->selected_stage<0||run->selected_stage>=run->stage_count)run->selected_stage=0;
        AppArtifact *stage=&run->stages[run->selected_stage];
        if(button((Rectangle){x,stage_y+34,50,36},"<",false)&&run->selected_stage>0)run->selected_stage--;
        if(button((Rectangle){x+60,stage_y+34,50,36},">",false)&&run->selected_stage+1<run->stage_count)run->selected_stage++;
        stage=&run->stages[run->selected_stage]; ui_draw_text(TextFormat("%02d | %s",stage->number,stage->name),x+124,stage_y+42,16,UI_TEXT);
        draw_image_view(app,stage->texture,x,stage_y+82,stage_w,stage_h-128);
        draw_wrapped_text(stage_description(stage->number),(Rectangle){x,stage_y+stage_h-36,(float)stage_w,32},14,UI_MUTED);
        if(button((Rectangle){x,stage_y+stage_h+2,150,38},"Open stage",false))open_path(stage->path);
        if(button((Rectangle){x+162,stage_y+stage_h+2,170,38},"Adjust related setting",false)&&adjust_stage_option(&app->profiles[app->active_profile],stage->number))save_profiles(app);
    }else ui_draw_text("Stages appear when the processor emits them.",x,stage_y+36,16,UI_MUTED);
    int log_x=x+stage_w+44,log_w=width-stage_w-44,log_y=stage_y,log_h=stage_h+42;
    ui_draw_text("Run log",log_x,log_y,18,UI_TEXT);
    Rectangle area={(float)log_x,(float)(log_y+32),(float)log_w,(float)(log_h-32)};
    DrawRectangleRec(area,UI_PANEL); BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    if(CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel>0)run->log_scroll+=3;else if(wheel<0&&run->log_scroll>0)run->log_scroll-=3;if(run->log_scroll<0)run->log_scroll=0;}
    int line_h=20,max_lines=(int)(area.height/line_h),start=run->log_count-max_lines-run->log_scroll;if(start<0)start=0;
    for(int i=start,row=0;i<run->log_count&&row<max_lines;i++,row++){
        AppLogLine *line=&run->logs[i]; ui_draw_text(TextFormat("%s [%s] %s",line->time,line->process,line->text),(int)area.x+10,(int)area.y+6+row*line_h,14,UI_TEXT);
    }
    EndScissorMode();
}

/* Draw profile fields and processor settings for App* app; return void. */
static void draw_settings(App *app) {
    int w=GetScreenWidth(),h=GetScreenHeight(),content_h=1435;
    Rectangle viewport={24,106,(float)w-48,(float)h-144};
    if(CheckCollisionPointRec(GetMousePosition(),viewport)){float wheel=GetMouseWheelMove();if(wheel!=0)app->settings_scroll-=(int)(wheel*56);}
    int max_scroll=content_h-(int)viewport.height;if(max_scroll<0)max_scroll=0;if(app->settings_scroll<0)app->settings_scroll=0;if(app->settings_scroll>max_scroll)app->settings_scroll=max_scroll;
    BeginScissorMode((int)viewport.x,(int)viewport.y,(int)viewport.width,(int)viewport.height);
    int x=48,off=app->settings_scroll; float gap=24,cw=(w-96-gap*2)/3.0f; AppProfile *p=&app->profiles[app->active_profile];
    int y=126-off; ui_draw_text("Profile",x,y,20,UI_TEXT);
    ui_draw_text("Scroll for more settings",x+320,y+8,15,UI_MUTED);
    if(button((Rectangle){x+112,y-4,42,38},"<",false)&&app->profile_count>1)app->active_profile=(app->active_profile+app->profile_count-1)%app->profile_count;
    if(button((Rectangle){x+164,y-4,42,38},">",false)&&app->profile_count>1)app->active_profile=(app->active_profile+1)%app->profile_count;
    p=&app->profiles[app->active_profile]; ui_draw_text(p->name,x+220,y+7,17,UI_ACCENT);
    if(button((Rectangle){x+cw*2-80,y-4,110,38},"Add profile",false))AppDispatch(app,"{\"action\":\"profile.add\"}",s_action_response,sizeof(s_action_response));
    if(button((Rectangle){x+cw*2+42,y-4,142,38},"Duplicate",false))AppDispatch(app,"{\"action\":\"profile.duplicate\"}",s_action_response,sizeof(s_action_response));
    if(button((Rectangle){x+cw*2+194,y-4,112,38},"Remove",false))AppDispatch(app,"{\"action\":\"profile.delete\"}",s_action_response,sizeof(s_action_response));
    draw_font_settings(app,x,y+48);
    int row=y+112;
    draw_profile_input(app,"name","Name",p->name,(Rectangle){x,row,cw,62});
    draw_profile_input(app,"model","Image model",p->model,(Rectangle){x+cw+gap,row,cw,62});
    draw_profile_input(app,"aspect","Aspect ratio",p->aspect,(Rectangle){x+2*(cw+gap),row,cw,62});
    ui_draw_text("Prompt additions",x,row+82,18,UI_TEXT);
    int prompt_rows=(APP_MAX_PROMPTS+1)/2;
    for(int i=0;i<p->prompt_count;i++){
        int col=i%2,pr=i/2; float pw=(w-120-gap)/2.0f;
        draw_profile_input(app,TextFormat("prompt.%d",i),TextFormat("Prompt %d",i+1),p->prompts[i],(Rectangle){x+col*(pw+gap),row+116+pr*62,pw,60});
    }
    if(p->prompt_count<APP_MAX_PROMPTS&&button((Rectangle){x,row+116+prompt_rows*62,176,38},"Add prompt addition",false))AppDispatch(app,"{\"action\":\"profile.prompt.add\"}",s_action_response,sizeof(s_action_response));
    int set_y=row+174+prompt_rows*62;
    ui_draw_text("Image model options",x,set_y,18,UI_TEXT);
    for(int i=0;i<p->set_count;i++){
        int col=i%2,sr=i/2;float pw=(w-120-gap)/2.0f;
        draw_profile_input(app,TextFormat("set.%d",i),TextFormat("Option %d",i+1),p->sets[i],(Rectangle){x+col*(pw+gap),set_y+34+sr*62,pw,60});
    }
    int set_rows=(p->set_count+1)/2;
    if(p->set_count<APP_MAX_PROMPTS&&button((Rectangle){x,set_y+42+set_rows*62,176,38},"Add model option",false)){
        copy_text(p->sets[p->set_count],sizeof(p->sets[0]),"");p->set_count++;save_profiles(app);
    }
    int pixel_y=set_y+100+set_rows*62; ui_draw_text("Pixel processing",x,pixel_y,20,UI_TEXT);
    int fields_y=pixel_y+38;
    draw_profile_input(app,"layout","Layout",p->layout,(Rectangle){x,fields_y,cw,62});
    draw_profile_input(app,"pixel_pitch","Pixel pitch",p->pixel_pitch,(Rectangle){x+cw+gap,fields_y,cw,62});
    draw_profile_input(app,"pitch_preset","Pitch preset",p->pitch_preset,(Rectangle){x+2*(cw+gap),fields_y,cw,62});
    draw_profile_input(app,"pitch_group_layout","Pitch grouping",p->pitch_group_layout,(Rectangle){x,fields_y+72,cw,62});
    draw_profile_input(app,"align_blobs","Blob alignment",p->align_blobs,(Rectangle){x+cw+gap,fields_y+72,cw,62});
    draw_profile_input(app,"magenta","Magenta cleanup",p->magenta,(Rectangle){x+2*(cw+gap),fields_y+72,cw,62});
    draw_profile_input(app,"edge_alpha","Edge alpha",p->edge_alpha,(Rectangle){x,fields_y+144,cw,62});
    ui_draw_text("Rearrange",x+cw+gap,fields_y+144,15,UI_MUTED);
    if(button((Rectangle){x+cw+gap,fields_y+166,cw,38},p->rearrange?"On":"Off",p->rearrange)){p->rearrange=!p->rearrange;save_profiles(app);}
    ui_draw_text("Slim upload",x+2*(cw+gap),fields_y+144,15,UI_MUTED);
    if(button((Rectangle){x+2*(cw+gap),fields_y+166,cw,38},p->slim_upload?"On":"Off",p->slim_upload)){p->slim_upload=!p->slim_upload;save_profiles(app);}
    draw_profile_input(app,"padding","Output padding",TextFormat("%d",p->padding),(Rectangle){x,fields_y+216,cw,62});
    ui_draw_text("Diagnostic stages",x+cw+gap,fields_y+216,15,UI_MUTED);
    if(button((Rectangle){x+cw+gap,fields_y+238,cw,38},app->debug_stages?"On":"Off",app->debug_stages))AppDispatch(app,"{\"action\":\"generation.debug\"}",s_action_response,sizeof(s_action_response));
    ui_draw_text("Tile sizes",x,fields_y+282,18,UI_TEXT);
    int tile_y=fields_y+314,tw=(int)((w-120-gap*3)/4.0f);
    for(int i=0;i<p->tile_count&&i<8;i++)draw_profile_input(app,TextFormat("tile_size.%d",i),TextFormat("Tile %d",i+1),TextFormat("%d",p->tile_sizes[i]),(Rectangle){x+(i%4)*(tw+gap),tile_y+(i/4)*70,(float)tw,60});
    if(p->tile_count<8&&button((Rectangle){x,tile_y+148,164,38},"Add tile size",false)){p->tile_sizes[p->tile_count++]=16;save_profiles(app);}
    EndScissorMode();
    int track_x=w-14,track_y=(int)viewport.y,track_h=(int)viewport.height;
    DrawRectangle(track_x,track_y,5,track_h,UI_PANEL);
    int thumb_h=(int)(track_h*((float)viewport.height/content_h));if(thumb_h<36)thumb_h=36;
    int thumb_y=track_y+(max_scroll?(track_h-thumb_h)*app->settings_scroll/max_scroll:0);
    DrawRectangle(track_x,thumb_y,5,thumb_h,UI_ACCENT);
}

/* Draw the Details view for App* app using int screen bounds; return void. */
static void draw_details(App *app) {
    int w=GetScreenWidth(),h=GetScreenHeight(),x=48,y=128,width=w-96,height=h-178;
    if(app->preview_ready)draw_prompt_details(app,x,y,width);
    else if(app->tab==TAB_REFERENCES)draw_reference_details(app,x,y,width,height);
    else if(app->tab==TAB_OUTPUT)draw_output_details(app,x,y,width);
    else draw_progress_details(app,x,y,width,height);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */

/* agent: gpt-6.1-sol | 2026-10-04 | Use shared reference name editing | v 3.3.0 | 930e92 */
/* Draw the full-size Texture2D viewer for App* app; return void. */
static void draw_modal_view(App *app) {
    if (!app->show_full_image) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0,0,0,220});
    if (app->tab == TAB_REFERENCES && app->selected_ref >= 0 && app->selected_ref < app->ref_count && app->refs[app->selected_ref].is_text) {
        AppReference *ref = &app->refs[app->selected_ref];
        bool editing_name = !strcmp(app->edit_field, "reference.name");
        draw_reference_name_field(app, ref);
        ui_draw_text("Click filename to rename; Ctrl+Enter or close to save", 36, 58, 14, UI_MUTED);
        draw_prompt_editor(app, (Rectangle){32, 82, GetScreenWidth()-64, GetScreenHeight()-132});
        bool close_requested = button((Rectangle){GetScreenWidth()-138,18,106,34}, "Close", false) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        if (IsKeyPressed(KEY_ESCAPE)) { if (editing_name) app->edit_field[0] = 0; else close_requested = true; }
        if (close_requested && save_open_prompt(app)) { app->show_full_image = false; app->edit_field[0] = 0; }
        return;
    }
    Texture2D tex = {0};
    AppReference *ref = NULL;
    if (app->tab == TAB_REFERENCES && app->selected_ref >= 0 && app->selected_ref < app->ref_count) {
        ref = &app->refs[app->selected_ref];
        tex = ref->full;
    }
    else {
        AppRun *run = get_run(app);
        if (run && app->tab == TAB_PROGRESS && run->selected_stage < run->stage_count) tex = run->stages[run->selected_stage].texture;
        else if (run && run->selected_output < run->output_count) tex = run->outputs[run->selected_output].texture;
    }
    int view_y=64, view_height=GetScreenHeight()-112;
    if (ref && !ref->is_text) {
        bool editing_name = !strcmp(app->edit_field, "reference.name");
        draw_reference_name_field(app, ref);
        ui_draw_text("Click filename to rename", 36, 58, 14, UI_MUTED);
        view_y=82; view_height=GetScreenHeight()-130;
        draw_image_view(app, tex, 32, view_y, GetScreenWidth() - 64, view_height);
        bool close_requested = button((Rectangle){GetScreenWidth()-138,18,106,34}, "Close", false) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        if (IsKeyPressed(KEY_ESCAPE)) { if (editing_name) app->edit_field[0] = 0; else close_requested = true; }
        if (close_requested) { app->show_full_image = false; app->edit_field[0] = 0; }
        return;
    }
    draw_image_view(app, tex, 32, view_y, GetScreenWidth() - 64, view_height);
    if (button((Rectangle){GetScreenWidth()-138,18,106,34}, "Close", false) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE)) app->show_full_image = false;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Use shared reference name editing | v 3.3.0 | 930e92 */

/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */
/* agent: gpt-6.1-sol | 2026-10-04 | Show trash confirmation dialog | v 3.3.0 | d68489 */
/* Draw the workbench for App* app; return void. */
void AppDraw(App *app) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    ClearBackground(UI_BG);
    if (app->show_full_image) { draw_modal_view(app); return; }
    ui_draw_text("AIGEN", 40, 22, 26, UI_ACCENT);
    ui_draw_text(app->workspace, 148, 29, 15, UI_MUTED);
    AppRun *active = app->active_run >= 0 ? &app->runs[app->active_run] : NULL;
    ui_draw_text(active ? TextFormat("%s | %s", active->id, active->state == RUN_RUNNING ? "WORKING" : active->state == RUN_FAILED ? "FAILED" : active->state == RUN_SUCCEEDED ? "DONE" : "CANCELED") : "READY", w - 330, 28, 15, active && active->state == RUN_FAILED ? UI_BAD : UI_GOOD);
    const char *tabs[] = {"References", "Output", "Progress"};
    bool compact=w<1100; float tab_w=compact?100:146,tab_gap=8,detail_w=compact?96:136,settings_w=compact?112:154;
    const char *compact_tabs[]={"Refs","Output","Progress"};
    for (int i = 0; i < TAB_COUNT; i++) if (button((Rectangle){40 + i * (tab_w+tab_gap), 62, tab_w, 40}, compact?compact_tabs[i]:tabs[i], !app->details_view && !app->settings_view && app->tab == (AppTab)i)) { app->tab = (AppTab)i; app->details_view = app->settings_view = app->show_add_menu = false; }
    bool details_open=app->details_view||app->settings_view;
    float settings_x=w-40-settings_w,details_x=details_open?settings_x-8-detail_w:w-40-detail_w;
    if (button((Rectangle){details_x,62,detail_w,40},"Details",app->details_view)){app->details_view=true;app->settings_view=false;app->show_add_menu=false;}
    if (details_open&&button((Rectangle){settings_x,62,settings_w,40},"Settings",app->settings_view)){app->settings_view=true;app->details_view=false;app->show_add_menu=false;}
    DrawLine(28, 112, w - 28, 112, UI_PANEL_ALT);
    if(app->settings_view) draw_settings(app);
    else if(app->details_view) draw_details(app);
    else if(app->tab==TAB_REFERENCES) draw_references(app);
    else if(app->tab==TAB_OUTPUT) draw_output(app);
    else draw_progress(app);
    DrawRectangle(0, h - 28, w, 28, UI_PANEL);
    ui_draw_text(app->message,40,h-22,13,UI_MUTED);
    if (!app->preview_ready && !app->show_full_image && app->settings_view && !strcmp(app->pending_action, "profile.delete")) {
        Rectangle panel = {(w - 440) * 0.5f, (h - 180) * 0.5f, 440, 180};
        DrawRectangleRec(panel, UI_PANEL); DrawRectangleLinesEx(panel, 2, UI_WARN);
        ui_draw_text("Remove this profile?", (int)panel.x + 20, (int)panel.y + 24, 21, UI_TEXT);
        ui_draw_text("You cannot remove the only profile.", (int)panel.x + 20, (int)panel.y + 62, 16, UI_MUTED);
        if(button((Rectangle){panel.x+20,panel.y+112,132,40},"Remove",false))AppDispatch(app,"{\"action\":\"app.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){panel.x+168,panel.y+112,132,40},"Cancel",false)||IsKeyPressed(KEY_ESCAPE))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }
    if (!strcmp(app->pending_action, "reference.trash")) {
        Rectangle panel = {(w - 440) * 0.5f, (h - 190) * 0.5f, 440, 190};
        DrawRectangleRec(panel, UI_PANEL); DrawRectangleLinesEx(panel, 2, UI_WARN);
        ui_draw_text("Move this item to workspace/trash?", (int)panel.x + 20, (int)panel.y + 28, 19, UI_TEXT);
        if(button((Rectangle){panel.x+20,panel.y+118,160,40},"Move to trash",false))AppDispatch(app,"{\"action\":\"app.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){panel.x+194,panel.y+118,120,40},"Cancel",false)||IsKeyPressed(KEY_ESCAPE))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }
    if (app->tab == TAB_OUTPUT && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) app->show_full_image = true;
    if (app->show_full_image) draw_modal_view(app);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Show trash confirmation dialog | v 3.3.0 | d68489 */
/* agent: gpt-6.1-sol | 2026-10-04 | Add reference gestures and trash actions | v 3.3.0 | 4845a9 */

/* Return monotonic seconds for MCP timeouts without initializing raylib. */
static double monotonic_seconds(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

/* Connect or reconnect an MCP stdio process to the active GUI listener. */
static ENetPeer *mcp_connect(ENetHost *host, ENetAddress address) {
    ENetPeer *peer = enet_host_connect(host, &address, 2, 0);
    if (!peer) return NULL;
    ENetEvent event;
    for (int i = 0; i < 100; i++) {
        if (enet_host_service(host, &event, 20) <= 0) continue;
        if (event.type == ENET_EVENT_TYPE_CONNECT) return peer;
        if (event.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(event.packet);
    }
    enet_peer_reset(peer);
    return NULL;
}

/* Wait for an ENet response to one serialized MCP request. */
static bool mcp_exchange(ENetHost *host, ENetPeer *peer, const char *request, char *response, size_t cap) {
    ENetPacket *packet = enet_packet_create(request, strlen(request) + 1, ENET_PACKET_FLAG_RELIABLE);
    if (enet_peer_send(peer, ENET_CHANNEL, packet)) return false;
    enet_host_flush(host);
    double until = monotonic_seconds() + 5.0; ENetEvent event;
    while (monotonic_seconds() < until) {
        if (enet_host_service(host, &event, 20) <= 0) continue;
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            size_t n = event.packet->dataLength; if (n >= cap) n = cap - 1; memcpy(response, event.packet->data, n); response[n] = 0;
            enet_packet_destroy(event.packet); return true;
        }
    }
    return false;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Expose trash instead of delete | v 3.3.0 | 84c62f */
/* Return the project tool list with the shared confirmation contract. */
const char *AppMcpToolsJson(void) {
    return "{\"tools\":["
      "{\"name\":\"app_status\",\"description\":\"Read workspace, profile, active run, log tail, stages, and outputs\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"ui_tab\",\"description\":\"Select References, Output, or Progress tab by index 0-2\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"prompt_get\",\"description\":\"Read the currently open prompt file, including unsaved UI edits\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"prompt_open\",\"description\":\"Open a text reference for editing inside the workspace\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"prompt_save\",\"description\":\"Save prompt text to the currently open workspace file\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}},"
      "{\"name\":\"profiles_list\",\"description\":\"List workspace generation profiles\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_add\",\"description\":\"Add a copy of the active profile\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_duplicate\",\"description\":\"Duplicate the active profile\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_prompt_add\",\"description\":\"Add an ordered profile prompt fragment\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_delete\",\"description\":\"Request deletion of the active profile; confirm separately\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_select\",\"description\":\"Select a profile by list index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"profile_set\",\"description\":\"Edit a supported profile field\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"field\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}},\"required\":[\"field\",\"value\"]}},"
      "{\"name\":\"references_list\",\"description\":\"List workspace references and generation exclusion state\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"reference_import\",\"description\":\"Copy an image path into workspace refs/imported\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"reference_manage\",\"description\":\"Exclude a reference or request moving it to workspace/trash\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"exclude\",\"trash\"]},\"path\":{\"type\":\"string\"}},\"required\":[\"action\",\"path\"]}},"
      "{\"name\":\"reference_select\",\"description\":\"Toggle whether a listed reference is included\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"reference_view\",\"description\":\"Open a listed reference in the full-size viewer\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"references_size\",\"description\":\"Set reference thumbnail size from 56 through 216\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"generation_preview\",\"description\":\"Prepare immutable prompt, references, model, options, and output path; does not start generation\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"generation_debug_toggle\",\"description\":\"Toggle diagnostic pixel stages for the next generation\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"generation_confirm\",\"description\":\"Confirm the current preview and start generation\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"generation_cancel\",\"description\":\"Request child interruption; confirm separately\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"app_confirm\",\"description\":\"Confirm a prepared generation, cancellation, or deletion\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"app_cancel\",\"description\":\"Cancel a pending preview or destructive action\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_select\",\"description\":\"Select retained run by list index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"run_inspect\",\"description\":\"Read run status, timestamped log tail, emitted stages, and final outputs\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"output_select\",\"description\":\"Select final output variant by list index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"stage_select\",\"description\":\"Select an actually emitted processor stage by stage number\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"stage_compare\",\"description\":\"Compare selected stage with source, previous available stage, or final output\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\",\"enum\":[\"source\",\"previous\",\"final\"]}},\"required\":[\"value\"]}},"
      "{\"name\":\"viewer_zoom\",\"description\":\"Adjust the selected image view scale\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"run_output_open\",\"description\":\"Open a selected run artifact with the desktop file handler\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"run_rerun\",\"description\":\"Prepare a processor-only debug rerun using current profile values\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_logs_clear\",\"description\":\"Clear the visible log view and retain process.log\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_note\",\"description\":\"Save a note beside the selected run\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}}]}";
}
/* agent: gpt-6.1-sol | 2026-10-04 | Expose trash instead of delete | v 3.3.0 | 84c62f */

/* Handle MCP JSON-RPC requests on stdio and forward tool actions to the GUI. */
int AppMcpClient(const char *workspace, int port) {
    (void)workspace;
    if (enet_initialize()) return 2;
    ENetHost *host = enet_host_create(NULL, 1, 2, 0, 0);
    if (!host) { enet_deinitialize(); return 2; }
    ENetAddress address; enet_address_set_host(&address, "127.0.0.1"); address.port = (enet_uint16)port;
    ENetPeer *peer = NULL;
    char line[MCP_FRAME_CAP], id[64], method[128], response[MCP_FRAME_CAP];
    while (fgets(line, sizeof(line), stdin)) {
        id[0] = method[0] = 0; json_raw(line, "id", id, sizeof(id)); json_string(line, "method", method, sizeof(method));
        if (!strcmp(method, "initialize")) {
            printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"protocolVersion\":\"2025-03-26\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"aigen-workbench\",\"version\":\"1.0.0\"}}}\n", id[0] ? id : "null"); fflush(stdout);
        } else if (!strcmp(method, "notifications/initialized")) continue;
        else if (!strcmp(method, "tools/list")) {
            printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}\n", id[0] ? id : "null", AppMcpToolsJson()); fflush(stdout);
        } else if (!strcmp(method, "tools/call")) {
            char name[128] = "", action[64] = "", value[APP_TEXT_CAP] = "", path[APP_PATH_CAP] = "", field[64] = "", text[APP_TEXT_CAP] = "";
            json_string(line, "name", name, sizeof(name));
            if (!strcmp(name,"app_status")) copy_text(action,sizeof(action),"state");
            else if (!strcmp(name,"prompt_get")) copy_text(action,sizeof(action),"prompt.get");
            else if (!strcmp(name,"prompt_open")) { copy_text(action,sizeof(action),"prompt.open"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"prompt_save")) { copy_text(action,sizeof(action),"prompt.save"); json_string(line,"text",text,sizeof(text)); }
            else if (!strcmp(name,"generation_preview")) copy_text(action,sizeof(action),"generation.preview");
            else if (!strcmp(name,"generation_debug_toggle")) copy_text(action,sizeof(action),"generation.debug");
            else if (!strcmp(name,"generation_confirm")) copy_text(action,sizeof(action),"generation.confirm");
            else if (!strcmp(name,"generation_cancel")) copy_text(action,sizeof(action),"generation.cancel");
            else if (!strcmp(name,"profiles_list")) copy_text(action,sizeof(action),"profile.list");
            else if (!strcmp(name,"profile_add")) copy_text(action,sizeof(action),"profile.add");
            else if (!strcmp(name,"profile_duplicate")) copy_text(action,sizeof(action),"profile.duplicate");
            else if (!strcmp(name,"profile_prompt_add")) copy_text(action,sizeof(action),"profile.prompt.add");
            else if (!strcmp(name,"profile_delete")) copy_text(action,sizeof(action),"profile.delete");
            else if (!strcmp(name,"references_list")) copy_text(action,sizeof(action),"references.list");
            else if (!strcmp(name,"reference_import")) { copy_text(action,sizeof(action),"reference.import"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"reference_select")) { copy_text(action,sizeof(action),"reference.select"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"reference_view")) { copy_text(action,sizeof(action),"reference.view"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"references_size")) { copy_text(action,sizeof(action),"references.size"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"ui_tab")) { copy_text(action,sizeof(action),"tab"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"profile_select")) { copy_text(action,sizeof(action),"profile.select"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"profile_set")) { copy_text(action,sizeof(action),"profile.set"); json_string(line,"field",field,sizeof(field)); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"reference_manage")) { json_string(line,"action",action,sizeof(action)); if (!strncmp(action,"reference.",10)) {} else { char kind[32]; copy_text(kind,sizeof(kind),action); snprintf(action,sizeof(action),"reference.%s",kind); } json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"app_confirm")) copy_text(action,sizeof(action),"app.confirm");
            else if (!strcmp(name,"app_cancel")) copy_text(action,sizeof(action),"app.cancel");
            else if (!strcmp(name,"run_inspect")) copy_text(action,sizeof(action),"run.inspect");
            else if (!strcmp(name,"run_rerun")) copy_text(action,sizeof(action),"run.rerun");
            else if (!strcmp(name,"run_logs_clear")) copy_text(action,sizeof(action),"run.logs.clear");
            else if (!strcmp(name,"run_select")) { copy_text(action,sizeof(action),"run.select"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"output_select")) { copy_text(action,sizeof(action),"output.select"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"stage_select")) { copy_text(action,sizeof(action),"stage.select"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"stage_compare")) { copy_text(action,sizeof(action),"stage.compare"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"viewer_zoom")) { copy_text(action,sizeof(action),"viewer.zoom"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"run_output_open")) { copy_text(action,sizeof(action),"run.output.open"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"run_note")) { copy_text(action,sizeof(action),"run.note"); json_string(line,"text",text,sizeof(text)); }
            char request[MCP_FRAME_CAP], esc[APP_TEXT_CAP * 2], escpath[APP_PATH_CAP * 2], escfield[128], escvalue[APP_TEXT_CAP * 2], escaction[128];
            json_escape(text,esc,sizeof(esc)); json_escape(path,escpath,sizeof(escpath)); json_escape(field,escfield,sizeof(escfield));
            json_escape(value,escvalue,sizeof(escvalue)); json_escape(action,escaction,sizeof(escaction));
            snprintf(request,sizeof(request),"{\"action\":\"%s\",\"value\":\"%s\",\"path\":\"%s\",\"field\":\"%s\",\"text\":\"%s\"}",escaction,escvalue,escpath,escfield,esc);
            if (!peer || peer->state != ENET_PEER_STATE_CONNECTED) { peer = mcp_connect(host, address); }
            bool ok = peer && mcp_exchange(host,peer,request,response,sizeof(response));
            if (!ok) snprintf(response,sizeof(response),"{\"ok\":false,\"error\":\"GUI app is not connected on loopback ENet\"}");
            char escaped_response[MCP_FRAME_CAP * 2]; json_escape(response,escaped_response,sizeof(escaped_response));
            bool tool_error = !ok || strstr(response, "\"ok\":false") != NULL;
            printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}],\"isError\":%s}}\n",id[0]?id:"null",escaped_response,tool_error?"true":"false"); fflush(stdout);
        } else if (!strcmp(method,"ping")) { printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{}}\n",id[0]?id:"null"); fflush(stdout); }
        else if (*id) { printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}\n",id); fflush(stdout); }
    }
    if (peer) enet_peer_disconnect(peer,0);
    enet_host_destroy(host); enet_deinitialize();
    return 0;
}

/* 2026-10-03: core functions are documented above; APP-01 defines the shared run flow. */
/* agent: gpt-6.1-sol | 2026-10-04 | Support image and text renaming | v 3.3.0 | 204819 */
