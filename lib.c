/*
 * File version: 3.43.0
 * Last updated: 2026-10-04
 * Purpose: Workbench persistence, processes, shared actions, and interface.
 * Flow ID APP-03
 * 1. User -> click the single Grid/Library header toggle -> close an open image viewer while keeping its view; otherwise alternate between grid and Library; from generation details it returns to grid; Settings overlays without changing this view.
 * 2. User -> click Library or grid plus -> prepare references and prompts; Generate -> preview and confirm immutable input snapshot with metadata.txt.
 * 3. Agent/MCP -> select home, library, or generation view -> update shared view state; run index selects generation details.
 * 4. Run -> emit process lines and all processor stages -> generation cards refresh live status; details show progress, stage previews when present, and full-width logs when no stages exist.
 * Flow ID APP-04
 * 1. User -> open a Library or generation image -> Library shows its filename; generation artifact labels stay hidden until hovering the overlay rail; dimensions/zoom tips, Open image, and transform reset stay in the frame footer; right-click closes it.
 * 2. Run -> save the original generated image then pixel outputs -> cards prefer the main pixel output, with the original image as fallback; viewer overlays a centered, tapered dot rail with final variants at top, stages in compact horizontal phase groups, and source at bottom; hover previews and labels the artifact, click selects it.
 * 3. Pixel process -> write output files -> UI rescans every 0.75s and replaces changed textures without leaving the generation view.
 * 4. User -> click generation trash -> confirm -> move the run directory to workspace/trash and release its textures.
 * 5. Run history or child process -> produce any decodable run image, even with nonzero status -> mark run complete and retain the image.
 * 6. Confirm run -> write immutable generic metadata.txt beside outputs -> start generation or pixel-only rerun.
 * 7. User -> zoom over image with wheel or keys -> keep the cursor's image point fixed; zoom outside image with keys -> zoom around center.
 * 8. Run -> expose a usable image while running -> generation details show that image automatically; Logs and Show image switch between image and diagnostics; the header shows run name/status/progress and the selected artifact name, omitting success status in image view.
 * 9. User -> hover a source/stage/output dot -> preview it with the current zoom and pan and show its label beside the dot grid; leave -> restore selection; click -> select it. The fine grid overlays the image's right edge, centers vertically, and tapers toward the middle; final outputs sit at top, stages follow pipeline phase in compact horizontal groups, and source sits at bottom.
 * Flow ID APP-05
 * 1. Import/create reference -> write directly to refs root -> Library displays a flat collection.
 * 2. App startup -> move supported files from legacy refs subfolders to collision-safe root names -> preserve exclusion state and remove empty groups only.
 * Flow ID APP-01
 * 1. User/agent -> choose home, Library, or generation details (APP-03) -> show one primary content view; header Settings button -> show profile configuration; prompt files live in Library.
 * 2. User -> left-click reference -> open viewer; right-click -> close viewer; exclude icon -> omit image or prompt from payload; trash icon -> confirm move item to workspace/trash; header Settings button -> edit profile configuration.
 * 3. Agent/MCP -> prepare then explicitly confirm -> immutable run starts generation then pixel processing.
 * 4. Child process -> report output and diagnostics -> generation cards and selected generation details reflect observed files.
 * 5. ENet/MCP -> dispatch the same action -> return current state or action result.
 * 6. User -> import TTF/OTF from any folder or select a stored font -> load 32px atlas; empty selection -> raylib default; persist per workspace.
 * 7. User -> plus card -> import image/text or create prompt; select the new ref before opening; text cards show filename/excerpt; select -> edit contents with UTF-8 caret, selection, clipboard and scrolling; Save or Ctrl+Enter persists; dirty close -> offer Save and close, Discard and close, or Cancel; Discard reloads saved text; filename header shows basename only and opens the same caret/selection editing controls; Enter renames while preserving extension; enabled text refs become repeated -p payloads and enabled images repeated -i payloads; trash -> confirm move and close editor if needed.
 * 8. User -> click any Settings text field -> edit with shared caret, selection, clipboard, and horizontal scrolling -> each changed value persists immediately; focus changes, Enter, and Escape end editing without discarding changes.
 * Invariant: reference trash moves require confirmation; trash is outside the scanned refs tree.
 * Invariant: the raylib default is active unless a stored custom font is selected; generation uses all enabled text refs, independently of editor selection.
 * Flow ID APP-02
 * 1. User -> click image or text reference filename -> edit basename while viewer stays open.
 * 2. User -> edit basename with caret/selection controls and press Enter -> save open text edits when needed, preserve file extension, reject empty names/collisions, rename reference and update workspace state.
 * 3. User -> press Escape -> discard filename edits; outside/right-click on add tile -> close its menu.
 * Invariant: an empty prompt collection stays empty; reference renames preserve extension and never overwrite files.
 */

/* agent: gpt-6.1-sol | 2026-10-04 | Write generic metadata for every generation | v 3.19.0 | 6c0bb8 */
#define ENET_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "enet.h"
#pragma GCC diagnostic pop
/* agent: gpt-6.1-sol | 2026-10-04 | Suppress ENET unused parameter warning | v 3.10.0 | f44c9c */
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
static void unload_run_artifacts(AppRun *run);

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

/* agent: gpt-6.1-sol | 2026-10-04 | Bound canonical workspace paths | v 3.10.0 | 04484d */
/* Resolve a selected regular file and reject paths outside the active workspace. */
static bool workspace_file(App *app, const char *input, char *resolved, size_t cap) {
    char *canonical=realpath(input,NULL);
    if (!canonical) return false;
    size_t resolved_size=strlen(canonical);
    if (resolved_size>=cap) { free(canonical); return false; }
    memcpy(resolved,canonical,resolved_size+1);
    free(canonical);
    if (!file_exists(resolved)) return false;
    size_t n=strlen(app->workspace);
    return !strncmp(resolved,app->workspace,n) && resolved[n]=='/';
}
/* agent: gpt-6.1-sol | 2026-10-04 | Bound canonical workspace paths | v 3.10.0 | 04484d */

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

/* Select a collision-free path in refs root for const char* name; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static bool reference_target_path(App *app, const char *name, char *target, size_t cap) {
    const char *dot = strrchr(name, '.');
    size_t stem = dot ? (size_t)(dot - name) : strlen(name);
    for (int suffix = 0; suffix < 1000; suffix++) {
        char candidate[512];
        if (!suffix) snprintf(candidate, sizeof(candidate), "%s", name);
        else snprintf(candidate, sizeof(candidate), "%.*s-%d%s", (int)stem, name, suffix, dot ? dot : "");
        int n = snprintf(target, cap, "%s/%s", app->refs_path, candidate);
        if (n < 0 || (size_t)n >= cap) return false;
        struct stat info;
        if (!lstat(target, &info)) continue;
        if (errno == ENOENT) return true;
        return false;
    }
    return false;
}

/* Flatten supported one-level legacy reference groups in App* app; return whether files moved. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static bool flatten_reference_groups(App *app) {
    DIR *root = opendir(app->refs_path);
    if (!root) return false;
    bool moved = false;
    struct dirent *entry;
    while ((entry = readdir(root))) {
        if (entry->d_name[0] == '.') continue;
        char folder[APP_PATH_CAP]; path_join(folder, sizeof(folder), app->refs_path, entry->d_name);
        struct stat info;
        if (stat(folder, &info) || !S_ISDIR(info.st_mode)) continue;
        DIR *group = opendir(folder);
        if (!group) continue;
        struct dirent *file;
        while ((file = readdir(group))) {
            if (file->d_name[0] == '.' || (!is_image_path(file->d_name) && !is_text_path(file->d_name))) continue;
            char old_path[APP_PATH_CAP], new_path[APP_PATH_CAP];
            path_join(old_path, sizeof(old_path), folder, file->d_name);
            if (stat(old_path, &info) || !S_ISREG(info.st_mode) || !reference_target_path(app, file->d_name, new_path, sizeof(new_path))) continue;
            if (rename(old_path, new_path) == 0) moved = true;
        }
        closedir(group);
        rmdir(folder);
    }
    closedir(root);
    return moved;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

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

/* Update AppRun* run from const char* process and line; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Show parsed generation process stages | v 3.13.0 | 461061 */
static void update_progress_label(AppRun *run, const char *process, const char *line) {
    const char *label = NULL;
    bool generation = process && strstr(process, "generation");
    if (strcasestr(line, "error") || strcasestr(line, "failed")) label = "Error reported";
    else if (strcasestr(line, "uploading")) label = "Uploading reference images";
    else if (strcasestr(line, "uploaded")) label = "References uploaded";
    else if (strcasestr(line, "waiting") || strcasestr(line, "queued")) label = "Waiting for image generation";
    else if (strcasestr(line, "download")) {
        const char *percent = strchr(line, '%');
        if (percent) {
            const char *digits = percent;
            while (digits > line && digits[-1] >= '0' && digits[-1] <= '9') digits--;
            if (digits < percent) {
                int value = atoi(digits);
                snprintf(run->current_step, sizeof(run->current_step), "Downloading image · %d%%", value);
                return;
            }
        }
        label = "Downloading generated image";
    } else if (strcasestr(line, "post-processing") || strcasestr(line, "pixel processing") || strcasestr(line, "stage-")) label = "Pixel processing";
    else if (strcasestr(line, "processing") || strcasestr(line, "generating")) label = generation ? "Generating image" : "Pixel processing";
    else if (strcasestr(line, "saved") || strcasestr(line, "wrote output")) label = "Generated image ready";
    else if (strcasestr(line, "starting")) label = generation ? "Starting image generation" : "Starting pixel processing";
    if (label) copy_text(run->current_step, sizeof(run->current_step), label);
    else if (generation) copy_text(run->current_step, sizeof(run->current_step), "Generating image");
    else copy_text(run->current_step, sizeof(run->current_step), "Pixel processing");
}
/* agent: gpt-6.1-sol | 2026-10-04 | Show parsed generation process stages | v 3.13.0 | 461061 */

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
    update_progress_label(run, process, line);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Preserve complete child process output | v 3.37.0 | c299a8 */
/* Append const char *bytes of size_t length from int stream for AppRun* run; return void. */
static void consume_process_output(AppRun *run, int stream, const char *bytes, size_t length) {
    char *pending = stream ? run->stderr_pending : run->stdout_pending;
    size_t used = strlen(pending);
    const char *process = TextFormat("%s/%s", run->process_kind == PROC_GENERATION ? "generation" : "pixel", stream ? "stderr" : "stdout");
    for (size_t i = 0; i < length; i++) {
        char c = bytes[i];
        if (c == '\n') {
            if (used && pending[used - 1] == '\r') pending[--used] = 0;
            add_log(run, process, pending);
            used = 0; pending[0] = 0;
        } else if (used + 1 < APP_TEXT_CAP) {
            pending[used++] = c; pending[used] = 0;
        } else {
            add_log(run, process, pending);
            used = 0; pending[0] = 0;
            pending[used++] = c; pending[used] = 0;
        }
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Preserve complete child process output | v 3.37.0 | c299a8 */

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
/* Load exclusion flags for App* app, matching moved files by basename; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static void load_reference_state(App *app) {
    FILE *f = fopen(app->refs_state_path, "r");
    if (!f) return;
    char path[APP_PATH_CAP]; int legacy_hidden, excluded;
    while (fscanf(f, "%d %d %1023[^\n]\n", &legacy_hidden, &excluded, path) == 3) {
        (void)legacy_hidden;
        const char *name = strrchr(path, '/'); name = name ? name + 1 : path;
        bool matched = false;
        for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, path)) {
            app->refs[i].excluded = excluded != 0;
            matched = true;
            break;
        }
        if (!matched) for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].name, name)) { app->refs[i].excluded = excluded != 0; break; }
    }
    fclose(f);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

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

/* Move a completed run directory into workspace trash; return bool. */
static bool move_run_to_trash(App *app, const char *path) {
    char trash_dir[APP_PATH_CAP], target[APP_PATH_CAP];
    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    path_join(trash_dir, sizeof(trash_dir), app->workspace, "trash");
    if (!make_dirs(trash_dir)) return false;
    path_join(target, sizeof(target), trash_dir, base);
    struct stat info;
    if (!lstat(target, &info)) {
        bool available = false;
        for (int suffix = 1; suffix < 1000; suffix++) {
            char unique[128]; snprintf(unique, sizeof(unique), "%s-%d", base, suffix);
            path_join(target, sizeof(target), trash_dir, unique);
            if (lstat(target, &info) && errno == ENOENT) { available = true; break; }
        }
        if (!available) return false;
    } else if (errno != ENOENT) return false;
    return rename(path, target) == 0;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Move generations into workspace trash | v 3.13.0 | 30919b */

/* Release one trashed run record and its loaded artifacts; return bool. */
static bool trash_run_record(App *app, const char *path) {
    int index = -1;
    for (int i = 0; i < app->run_count; i++) if (!strcmp(app->runs[i].dir, path)) { index = i; break; }
    if (index < 0 || app->runs[index].state == RUN_RUNNING || !move_run_to_trash(app, path)) return false;
    unload_run_artifacts(&app->runs[index]);
    for (int i = index; i + 1 < app->run_count; i++) app->runs[i] = app->runs[i + 1];
    app->run_count--;
    memset(&app->runs[app->run_count], 0, sizeof(app->runs[0]));
    app->runs[app->run_count].pipe_fd = app->runs[app->run_count].err_fd = -1;
    if (app->selected_run == index) app->selected_run = app->run_count ? (index < app->run_count ? index : app->run_count - 1) : -1;
    else if (app->selected_run > index) app->selected_run--;
    if (app->active_run > index) app->active_run--;
    if (app->active_run == index) app->active_run = -1;
    app->view = VIEW_HOME; app->show_full_image = false;
    return true;
}

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

/* Copy const char* source into App* app's flat refs root; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static bool import_reference_file(App *app, const char *source) {
    if (!is_image_path(source) && !is_text_path(source)) return false;
    const char *base = strrchr(source,'/'); base = base ? base + 1 : source;
    char target[APP_PATH_CAP]; if (!reference_target_path(app, base, target, sizeof(target))) return false;
    if (!copy_file_new(source,target)) return false;
    add_reference(app,target); return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

static bool select_prompt_reference(App *app, AppReference *ref);

/* agent: gpt-6.1-sol | 2026-10-04 | Select new prompt before opening | v 3.5.0 | 595d6e */
/* Create an empty collision-safe prompt in App* app's refs root; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static bool create_prompt_file(App *app) {
    if (app->prompt_dirty) { set_message(app, "Save the current prompt before creating another"); return false; }
    char target[APP_PATH_CAP];
    if (!reference_target_path(app, "prompt.txt", target, sizeof(target)) || !write_text(target, "")) return false;
    add_reference(app, target);
    for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, target)) {
        if (!select_prompt_reference(app, &app->refs[i])) return false;
        app->selected_ref = i;
        app->view = VIEW_LIBRARY;
        app->show_full_image = true;
        save_reference_state(app);
        return true;
    }
    return false;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
/* agent: gpt-6.1-sol | 2026-10-04 | Select new prompt before opening | v 3.5.0 | 595d6e */

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
    app->prompt_caret = app->prompt_anchor = (int)strlen(content);
    app->prompt_scroll = 0;
    app->prompt_selecting = false;
    app->prompt_dirty = app->prompt_editor_focused = app->prompt_save_error = app->prompt_limit_error = app->prompt_close_pending = false;
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

/* Scan flat files in App* app's refs root; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
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
    }
    closedir(root);
    load_reference_state(app);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

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
    if (!write_text(app->prompt_path, app->prompt_edit)) { app->prompt_save_error = true; set_message(app, "Could not save prompt"); return false; }
    app->prompt_dirty = false;
    app->prompt_save_error = app->prompt_limit_error = false;
    for (int i = 0; i < app->ref_count; i++)
        if (app->refs[i].is_text && !strcmp(app->refs[i].path, app->prompt_path)) cache_prompt_preview(&app->refs[i], app->prompt_edit);
    set_message(app, "Prompt saved");
    return true;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Reload prompt when discarding edits | v 3.8.0 | a8bdc5 */
/* Replace dirty prompt with saved file text; return void. */
static void discard_open_prompt(App *app) {
    char saved[APP_TEXT_CAP];
    if (read_text(app->prompt_path, saved, sizeof(saved))) copy_text(app->prompt_edit, sizeof(app->prompt_edit), saved);
    else { app->prompt_edit[0]=0; set_message(app, "Prompt file could not be reloaded"); }
    app->prompt_dirty=app->prompt_save_error=app->prompt_limit_error=false;
    app->prompt_editor_focused=app->prompt_selecting=app->prompt_close_pending=false;
    app->prompt_caret=app->prompt_anchor=(int)strlen(app->prompt_edit);
    app->prompt_scroll=0;
    for(int i=0;i<app->ref_count;i++)
        if(app->refs[i].is_text&&!strcmp(app->refs[i].path,app->prompt_path))cache_prompt_preview(&app->refs[i],app->prompt_edit);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Reload prompt when discarding edits | v 3.8.0 | a8bdc5 */

enum { PROMPT_CLOSE_NONE, PROMPT_CLOSE_SAVE, PROMPT_CLOSE_DISCARD, PROMPT_CLOSE_CANCEL };

/* agent: gpt-6.1-sol | 2026-10-04 | Confirm unsaved prompt close choices | v 3.8.0 | 32e9a8 */
/* Draw the unsaved prompt choice dialog; return the selected close action as int. */
static int draw_prompt_close_dialog(void) {
    int w=GetScreenWidth(),h=GetScreenHeight();
    Rectangle panel={(w-520)*0.5f,(h-190)*0.5f,520,190};
    DrawRectangle(0,0,w,h,(Color){0,0,0,190});
    DrawRectangleRec(panel,UI_PANEL); DrawRectangleLinesEx(panel,2,UI_WARN);
    ui_draw_text("Unsaved prompt",(int)panel.x+22,(int)panel.y+24,21,UI_TEXT);
    ui_draw_text("Save changes before closing?",(int)panel.x+22,(int)panel.y+62,16,UI_MUTED);
    if(button((Rectangle){panel.x+20,panel.y+120,148,42},"Save & close",false))return PROMPT_CLOSE_SAVE;
    if(button((Rectangle){panel.x+186,panel.y+120,148,42},"Discard",false))return PROMPT_CLOSE_DISCARD;
    if(button((Rectangle){panel.x+352,panel.y+120,148,42},"Cancel",false))return PROMPT_CLOSE_CANCEL;
    return PROMPT_CLOSE_NONE;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Confirm unsaved prompt close choices | v 3.8.0 | 32e9a8 */

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

static void draw_reference_name_field(App *app, AppReference *ref, Rectangle box);
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

/* Write generic run metadata for App* app and AppRun* run; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Write generic metadata for every generation | v 3.19.0 | 6c0bb8 */
static bool write_run_metadata(App *app, AppRun *run, bool pixel_only) {
    char path[APP_PATH_CAP], image_path[APP_PATH_CAP];
    path_join(path, sizeof(path), run->dir, "metadata.txt");
    if (pixel_only) copy_text(image_path, sizeof(image_path), run->source);
    else path_join(image_path, sizeof(image_path), run->dir, "source.png");
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    time_t now = time(NULL); struct tm local_now; localtime_r(&now, &local_now);
    char created[64]; strftime(created, sizeof(created), "%Y-%m-%d %H:%M:%S %z", &local_now);
    AppProfile *profile = &app->snapshot_profile;
    bool ok = fprintf(file,
        "Generation: %s\nCreated: %s\nOperation: %s\nProfile: %s\nModel: %s\nAspect ratio: %s\n",
        run->id, created, pixel_only ? "Pixel processing rerun" : "Image generation", profile->name,
        profile->model, profile->aspect) >= 0;
    int reference_count = pixel_only ? 0 : app->snapshot_ref_count;
    if (ok && fprintf(file, "%s: %s\nOutput folder: %s\nImage references (%d):\n",
        pixel_only ? "Input image" : "Original generated image", image_path, run->dir, reference_count) < 0) ok = false;
    for (int i = 0; ok && i < reference_count; i++) {
        const char *path = app->snapshot_refs[i];
        const char *name = path ? strrchr(path, '/') : NULL;
        if (fprintf(file, "  %s\n", name ? name + 1 : path ? path : "") < 0) ok = false;
    }
    if (ok && fprintf(file, "Prompt:\n%s\n", run->resolved_prompt) < 0) ok = false;
    if (fclose(file)) ok = false;
    if (!ok) unlink(path);
    return ok;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Write generic metadata for every generation | v 3.19.0 | 6c0bb8 */

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

/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* Start processing for App* app and AppRun* run with every stage; return bool. */
static bool start_pixel(App *app, AppRun *run) {
    char processor[APP_PATH_CAP], python[APP_PATH_CAP], layout[64], pitch[64], preset[32], group[32];
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
    argv_push(argv, &argc, 128, "--emit"); argv_push(argv, &argc, 128, "all");
    argv[argc] = NULL;
    if (!spawn_process(run, app->workspace, argv, PROC_PIXEL)) return false;
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */

/* Scan actual files produced by the processor and collect stages and final outputs. */
static int artifact_order(const void *a, const void *b) {
    const AppArtifact *aa = a, *bb = b;
    bool amain = strstr(aa->name, "final.pixel") != NULL, bmain = strstr(bb->name, "final.pixel") != NULL;
    if (amain != bmain) return bmain - amain;
    int by_name = strcmp(aa->name, bb->name);
    if (by_name) return by_name;
    return bb->number - aa->number;
}
static int stage_order(const void *a, const void *b) {
    const AppArtifact *aa = a, *bb = b;
    return bb->number - aa->number;
}

/* Load an image artifact's full texture and thumbnail from const char* path; return bool. */
static bool load_artifact_image(AppArtifact *artifact, const char *path, const char *name, int number) {
    struct stat info;
    if (stat(path, &info) || !S_ISREG(info.st_mode)) return false;
    Image image = LoadImage(path);
    if (!image.data) return false;
    memset(artifact, 0, sizeof(*artifact));
    copy_text(artifact->name, sizeof(artifact->name), name);
    copy_text(artifact->path, sizeof(artifact->path), path);
    artifact->number = number; artifact->file_size = info.st_size;
    artifact->width = image.width; artifact->height = image.height;
    artifact->texture = LoadTexture(path);
    if (artifact->texture.id) SetTextureFilter(artifact->texture, TEXTURE_FILTER_POINT);
    int thumb_width = image.width > image.height ? 240 : (int)(240.0f * image.width / image.height);
    int thumb_height = image.height > image.width ? 240 : (int)(240.0f * image.height / image.width);
    if (thumb_width < 1) thumb_width = 1;
    if (thumb_height < 1) thumb_height = 1;
    ImageResize(&image, thumb_width, thumb_height);
    artifact->thumb = LoadTextureFromImage(image);
    UnloadImage(image);
    return artifact->texture.id != 0 || artifact->thumb.id != 0;
}

/* Release both textures in AppArtifact* artifact; return void. */
static void unload_artifact(AppArtifact *artifact) {
    if (artifact->texture.id) UnloadTexture(artifact->texture);
    if (artifact->thumb.id) UnloadTexture(artifact->thumb);
    memset(artifact, 0, sizeof(*artifact));
}
/* Release loaded images for AppRun* run; return void. */
static void unload_run_artifacts(AppRun *run) {
    unload_artifact(&run->generated);
    for (int i = 0; i < run->stage_count; i++) unload_artifact(&run->stages[i]);
    for (int i = 0; i < run->output_count; i++) unload_artifact(&run->outputs[i]);
    run->stage_count = run->output_count = 0;
}

/* Scan AppRun* run output folders and load discovered artifacts; return void. */
static void scan_artifacts(AppRun *run) {
    for (int i = 0; i < run->stage_count; i++) unload_artifact(&run->stages[i]);
    for (int i = 0; i < run->output_count; i++) unload_artifact(&run->outputs[i]);
    run->stage_count = 0; run->output_count = 0;
    if (!run->generated.texture.id && file_exists(run->source)) load_artifact_image(&run->generated, run->source, "Generated image", -1);
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
            } else if (strcmp(path, run->source) && run->output_count < APP_MAX_OUTPUTS) {
                a = &run->outputs[run->output_count++]; memset(a, 0, sizeof(*a));
            }
            if (!a) continue;
            char name[128]; copy_text(name, sizeof(name), de->d_name);
            if (!load_artifact_image(a, path, name, a->number)) { if (s) run->stage_count--; else run->output_count--; }
        }
        closedir(dir);
    }
    qsort(run->stages, (size_t)run->stage_count, sizeof(run->stages[0]), stage_order);
    qsort(run->outputs, (size_t)run->output_count, sizeof(run->outputs[0]), artifact_order);
}

/* agent: gpt-6.1-sol | 2026-10-04 | Separate live stage and final artifacts | v 3.37.0 | 99dbe2 */
/* Refresh stage and output images for AppRun* run while processing; return void. */
static void refresh_run_outputs(AppRun *run) {
    char selected_output_path[APP_PATH_CAP] = "", selected_stage_path[APP_PATH_CAP] = "";
    if (!run->stage_output_selected && run->selected_output >= 0 && run->selected_output < run->output_count)
        copy_text(selected_output_path, sizeof(selected_output_path), run->outputs[run->selected_output].path);
    if (run->stage_output_selected && run->selected_stage >= 0 && run->selected_stage < run->stage_count)
        copy_text(selected_stage_path, sizeof(selected_stage_path), run->stages[run->selected_stage].path);
    char folders[2][APP_PATH_CAP];
    copy_text(folders[0], sizeof(folders[0]), run->dir); path_join(folders[1], sizeof(folders[1]), run->dir, "extra");
    for (int folder = 0; folder < 2; folder++) {
        DIR *dir = opendir(folders[folder]); if (!dir) continue;
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (!is_image_path(entry->d_name)) continue;
            char path[APP_PATH_CAP]; path_join(path, sizeof(path), folders[folder], entry->d_name);
            if (!strcmp(path, run->source)) continue;
            struct stat info; if (stat(path, &info) || !S_ISREG(info.st_mode)) continue;
            const char *stage_name = strstr(entry->d_name, "stage-");
            AppArtifact *items = stage_name ? run->stages : run->outputs;
            int *item_count = stage_name ? &run->stage_count : &run->output_count;
            int item_limit = stage_name ? APP_MAX_STAGES : APP_MAX_OUTPUTS;
            int found = -1;
            for (int i = 0; i < *item_count; i++) if (!strcmp(items[i].path, path)) { found = i; break; }
            if (found >= 0 && items[found].file_size == info.st_size) continue;
            AppArtifact refreshed = {0};
            if (!load_artifact_image(&refreshed, path, entry->d_name, stage_name ? atoi(stage_name + 6) : 0)) continue;
            if (found >= 0) { unload_artifact(&items[found]); items[found] = refreshed; }
            else if (*item_count < item_limit) items[(*item_count)++] = refreshed;
            else unload_artifact(&refreshed);
        }
        closedir(dir);
    }
    qsort(run->outputs, (size_t)run->output_count, sizeof(run->outputs[0]), artifact_order);
    qsort(run->stages, (size_t)run->stage_count, sizeof(run->stages[0]), stage_order);
    if (selected_output_path[0]) for (int i = 0; i < run->output_count; i++) if (!strcmp(run->outputs[i].path, selected_output_path)) { run->selected_output = i; break; }
    if (selected_stage_path[0]) for (int i = 0; i < run->stage_count; i++) if (!strcmp(run->stages[i].path, selected_stage_path)) { run->selected_stage = i; break; }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Separate live stage and final artifacts | v 3.37.0 | 99dbe2 */

/* Keep the original generated image and processor artifacts discoverable after exit. */
static void update_run_artifacts(App *app, AppRun *run) {
    scan_artifacts(run);
    if (file_exists(run->source) && !strstr(run->source, ".pixel")) copy_text(run->primary, sizeof(run->primary), run->source);
    else if (run->output_count) copy_text(run->primary, sizeof(run->primary), run->outputs[0].path);
    (void)app;
}

/* Return whether AppRun* run contains any successfully loaded image; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static bool run_has_image(const AppRun *run) {
    return run->generated.texture.id || run->generated.thumb.id || run->output_count || run->stage_count;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

/* Return whether AppRun* run has a loaded image for the viewer; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */
static bool run_has_viewable_image(const AppRun *run) {
    if (!run) return false;
    if (run->generated.texture.id || run->generated.thumb.id) return true;
    for (int i = 0; i < run->output_count; i++)
        if (run->outputs[i].texture.id || run->outputs[i].thumb.id) return true;
    return false;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */

/* Return the first loaded pixel output, or original image, for AppRun* run; return int. */
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */
static int run_default_image_index(const AppRun *run) {
    if (!run) return -1;
    for (int i = 0; i < run->output_count; i++)
        if (run->outputs[i].texture.id || run->outputs[i].thumb.id) return i;
    return -1;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */

/* Load App* app's run folders, logs, and image artifacts; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static void load_run_history(App *app) {
    DIR *dir = opendir(app->runs_path); if (!dir) return;
    struct dirent *de;
    while ((de = readdir(dir)) && app->run_count < APP_MAX_RUNS) {
        if (de->d_name[0] == '.') continue;
        char path[APP_PATH_CAP]; path_join(path, sizeof(path), app->runs_path, de->d_name);
        struct stat st; if (stat(path, &st) || !S_ISDIR(st.st_mode)) continue;
        AppRun *run = &app->runs[app->run_count]; memset(run, 0, sizeof(*run)); run->pipe_fd = run->err_fd = -1; run->selected_output = -2;
        copy_text(run->id, sizeof(run->id), de->d_name); copy_text(run->dir, sizeof(run->dir), path);
        path_join(run->source, sizeof(run->source), path, "source.png");
        char logpath[APP_PATH_CAP]; path_join(logpath, sizeof(logpath), path, "process.log");
        FILE *f = fopen(logpath, "r"); char line[APP_TEXT_CAP + 128];
        while (f && fgets(line, sizeof(line), f)) {
            char tm[32] = "", proc[32] = "", message[APP_TEXT_CAP] = "";
            if (sscanf(line, "%31s [%31[^]]] %8191[^\n]", tm, proc, message) < 3) continue;
            if (run->log_count >= APP_MAX_LOGS) continue;
            AppLogLine *entry = &run->logs[run->log_count++]; copy_text(entry->time, sizeof(entry->time), tm); copy_text(entry->process, sizeof(entry->process), proc); copy_text(entry->text, sizeof(entry->text), message);
            update_progress_label(run, proc, message);
        }
        if (f) fclose(f);
        run->started = 0;
        run->state = RUN_FAILED;
        if (run->state == RUN_FAILED) copy_text(run->error, sizeof(run->error), "incomplete previous run; inspect retained files and logs");
        path_join(run->primary, sizeof(run->primary), path, "final.pixel.png");
        update_run_artifacts(app, run);
        if (run_has_image(run)) { run->state = RUN_SUCCEEDED; run->error[0] = 0; copy_text(run->current_step, sizeof(run->current_step), "image ready"); }
        app->run_count++;
    }
    closedir(dir);
    if (app->run_count) app->selected_run = app->run_count - 1;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

/* agent: gpt-6.1-sol | 2026-10-04 | Preserve complete child process output | v 3.37.0 | c299a8 */
/* Poll AppRun* run for App* app; continue processing or retain any image; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static void update_child(App *app, AppRun *run) {
    if (run->child <= 0) return;
    int fds[2] = {run->pipe_fd, run->err_fd};
    for (int stream = 0; stream < 2; stream++) if (fds[stream] >= 0) {
        char buf[2048]; ssize_t n = read(fds[stream], buf, sizeof(buf));
        while (n > 0) {
            consume_process_output(run, stream, buf, (size_t)n);
            n = read(fds[stream], buf, sizeof(buf));
        }
    }
    int status = 0; pid_t result = waitpid(run->child, &status, WNOHANG);
    if (result != run->child) return;
    if (run->stdout_pending[0]) { add_log(run, run->process_kind == PROC_GENERATION ? "generation/stdout" : "pixel/stdout", run->stdout_pending); run->stdout_pending[0] = 0; }
    if (run->stderr_pending[0]) { add_log(run, run->process_kind == PROC_GENERATION ? "generation/stderr" : "pixel/stderr", run->stderr_pending); run->stderr_pending[0] = 0; }
    if (run->pipe_fd >= 0) { close(run->pipe_fd); run->pipe_fd = -1; }
    if (run->err_fd >= 0) { close(run->err_fd); run->err_fd = -1; }
    ProcessKind finished = run->process_kind; run->child = 0; run->process_kind = PROC_NONE;
    run->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1);
    if (run->cancel_pending || run->exit_code == 130 || run->exit_code == 143) {
        run->state = RUN_CANCELED; run->finished = GetTime(); copy_text(run->current_step, sizeof(run->current_step), "canceled; partial files retained");
    } else if (run->exit_code != 0) {
        update_run_artifacts(app, run);
        run->finished = GetTime();
        if (run_has_image(run)) {
            run->state = RUN_SUCCEEDED; run->error[0] = 0;
            copy_text(run->current_step, sizeof(run->current_step), "image ready");
        } else {
            run->state = RUN_FAILED;
            snprintf(run->error, sizeof(run->error), "%s exited with status %d", finished == PROC_GENERATION ? "generation" : "pixel processing", run->exit_code);
            copy_text(run->current_step, sizeof(run->current_step), "failed");
        }
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
/* agent: gpt-6.1-sol | 2026-10-04 | Preserve complete child process output | v 3.37.0 | c299a8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

/* agent: gpt-6.1-sol | 2026-10-04 | Load UI font for desktop app | v 3.3.0 | a9fe70 */
/* Initialize App* app with const char *workspace and bool mcp_mode; flatten refs; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
bool AppInit(App *app, const char *workspace, bool mcp_mode) {
    memset(app, 0, sizeof(*app));
    app->view = VIEW_HOME; app->thumb_size = 104; app->image_zoom = 1.0f;
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
    bool refs_flattened = flatten_reference_groups(app);
    char child[APP_PATH_CAP];
    path_join(child, sizeof(child), app->workspace, "extra"); make_dirs(child);
    load_profiles(app); seed_profile(app); scan_refs(app);
    if (refs_flattened) save_reference_state(app);
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
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
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
        unload_run_artifacts(run);
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

/* Encode App* app's visible state in char* out with size_t cap; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Expose view and original output status | v 3.13.0 | b2a2eb */
static void state_json(App *app, char *out, size_t cap) {
    AppRun *run = get_run(app);
    char prompt[APP_TEXT_CAP], esc_prompt[APP_TEXT_CAP * 2], esc_step[512], esc_workspace[APP_PATH_CAP * 2], esc_message[1024];
    build_prompt(app, prompt, sizeof(prompt)); json_escape(prompt, esc_prompt, sizeof(esc_prompt));
    json_escape(run ? run->current_step : "idle", esc_step, sizeof(esc_step));
    json_escape(app->workspace, esc_workspace, sizeof(esc_workspace));
    json_escape(app->message, esc_message, sizeof(esc_message));
    const char *view = app->view == VIEW_LIBRARY ? "library" : app->view == VIEW_GENERATION ? "generation" : "home";
    int n = snprintf(out, cap, "{\"workspace\":\"%s\",\"view\":\"%s\",\"selected_run\":%d,\"profile\":\"%s\",\"prompt\":\"%s\",\"run_state\":%d,\"process\":\"%s\",\"current_step\":\"%s\",\"progress_percent\":null,\"message\":\"%s\",\"stages\":[",
        esc_workspace, view, app->selected_run, app->profile_count ? app->profiles[app->active_profile].name : "", esc_prompt,
        run ? run->state : RUN_IDLE, run ? (run->process_kind == PROC_GENERATION ? "generation" : run->process_kind == PROC_PIXEL ? "pixel" : "none") : "none", esc_step, esc_message);
    if (n < 0 || (size_t)n >= cap) return;
    if (run) for (int i = 0; i < run->stage_count; i++) {
        char name[256], path[APP_PATH_CAP * 2]; json_escape(run->stages[i].name, name, sizeof(name)); json_escape(run->stages[i].path, path, sizeof(path));
        n += snprintf(out + n, cap - (size_t)n, "%s{\"name\":\"%s\",\"number\":%d,\"path\":\"%s\"}", i ? "," : "", name, run->stages[i].number, path);
        if ((size_t)n >= cap) return;
    }
    n += snprintf(out + n, cap - (size_t)n, "],\"outputs\":[");
    if ((size_t)n >= cap) return;
    bool first_output = true;
    if (run && run->generated.path[0]) {
        char path[APP_PATH_CAP * 2]; json_escape(run->generated.path, path, sizeof(path));
        n += snprintf(out + n, cap - (size_t)n, "{\"name\":\"original\",\"path\":\"%s\",\"width\":%d,\"height\":%d}", path, run->generated.width, run->generated.height);
        first_output = false;
        if ((size_t)n >= cap) return;
    }
    if (run) for (int i = 0; i < run->output_count; i++) {
        char name[256], path[APP_PATH_CAP * 2]; json_escape(run->outputs[i].name, name, sizeof(name)); json_escape(run->outputs[i].path, path, sizeof(path));
        n += snprintf(out + n, cap - (size_t)n, "%s{\"name\":\"%s\",\"path\":\"%s\",\"width\":%d,\"height\":%d}", first_output ? "" : ",", name, path, run->outputs[i].width, run->outputs[i].height);
        first_output = false;
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
/* agent: gpt-6.1-sol | 2026-10-04 | Expose view and original output status | v 3.13.0 | b2a2eb */

/* agent: gpt-6.1-sol | 2026-10-04 | Route named view navigation | v 3.12.0 | 0b288d */
/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */
/* agent: gpt-6.1-sol | 2026-10-04 | Route reference trash actions | v 3.3.0 | e1c824 */
/* Apply char* request via App* app, write char* response with size_t response_size; return const char*. */
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Write generic metadata for every generation | v 3.19.0 | 6c0bb8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route generation image and logs controls | v 3.35.0 | 94dd5b */
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
    } else if (!strcmp(action, "view")) {
        char view[32]; json_string(request, "value", view, sizeof(view));
        bool valid = true;
        if (!strcmp(view, "home")) { app->view = VIEW_HOME; app->settings_view = false; }
        else if (!strcmp(view, "library")) { app->view = VIEW_LIBRARY; app->settings_view = false; }
        else if (!strcmp(view, "generation")) {
            char run_value[32]; json_string(request, "run", run_value, sizeof(run_value));
            int idx = *run_value ? atoi(run_value) : app->selected_run;
            if (idx >= 0 && idx < app->run_count) { app->selected_run = idx; app->view = VIEW_GENERATION; app->settings_view = false; app->show_run_logs = false; }
            else valid = false;
        } else valid = false;
        app->show_add_menu = false;
        snprintf(response, response_size, valid ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"unknown view or run\"}");
    } else if (!strcmp(action, "prompt.save")) {
        char prompt[APP_TEXT_CAP]; json_string(request, "text", prompt, sizeof(prompt));
        if (app->prompt_path[0] && write_text(app->prompt_path, prompt)) { copy_text(app->prompt_edit, sizeof(app->prompt_edit), prompt); app->prompt_dirty = app->prompt_editor_focused = app->prompt_save_error = app->prompt_limit_error = app->prompt_close_pending = false; app->prompt_caret=app->prompt_anchor=(int)strlen(app->prompt_edit); set_message(app, "Prompt saved"); snprintf(response, response_size, "{\"ok\":true}"); }
        else { snprintf(response, response_size, "{\"ok\":false,\"error\":\"prompt save failed\"}"); }
    } else if (!strcmp(action,"prompt.open")) {
        char resolved[APP_PATH_CAP];
        if (app->prompt_dirty) snprintf(response,response_size,"{\"ok\":false,\"error\":\"save prompt edits before switching files\"}");
        else if (!workspace_file(app,path,resolved,sizeof(resolved)) || !strstr(resolved,".txt")) snprintf(response,response_size,"{\"ok\":false,\"error\":\"prompt must be a text file inside the workspace\"}");
        else {
            copy_text(app->prompt_path,sizeof(app->prompt_path),resolved); app->prompt_dirty=app->prompt_editor_focused=app->prompt_save_error=app->prompt_limit_error=app->prompt_close_pending=false; app->prompt_scroll=0;
            if (!read_text(app->prompt_path,app->prompt_edit,sizeof(app->prompt_edit))) app->prompt_edit[0]=0;
            app->prompt_caret=app->prompt_anchor=(int)strlen(app->prompt_edit);
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
            app->preview_ready = true; app->rerun_pixel_only = false;
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
            if((size_t)n<response_size)n+=snprintf(response+n,response_size-(size_t)n,"],\"generation_options\":{\"model\":\"%s\",\"aspect\":\"%s\",\"set_count\":%d},\"postprocessing\":{\"layout\":\"%s\",\"pixel_pitch\":\"%s\",\"pitch_preset\":\"%s\",\"align_blobs\":\"%s\",\"magenta\":\"%s\",\"edge_alpha\":\"%s\",\"rearrange\":%s,\"tile_size_count\":%d,\"emit\":\"all\"},\"output_path\":\"%s\"}",model,app->snapshot_profile.aspect,app->snapshot_profile.set_count,layout,pitch,preset,align,magenta,alpha,app->snapshot_profile.rearrange?"true":"false",app->snapshot_profile.tile_count,escaped_output);
        }
    } else if (!strcmp(action, "generation.confirm") || !strcmp(action, "app.confirm")) {
        if (!strcmp(app->pending_action, "run.cancel")) {
            AppRun *run = get_run(app);
            if (run && run->child > 0) kill(-run->child, SIGINT);
            app->pending_action[0] = 0;
            snprintf(response, response_size, "{\"ok\":true}"); return response;
        }
        if (!strcmp(app->pending_action, "run.trash")) {
            char path[APP_PATH_CAP]; copy_text(path, sizeof(path), app->pending_path);
            app->pending_action[0] = app->pending_path[0] = 0;
            if (trash_run_record(app, path)) { set_message(app, "Generation moved to workspace/trash"); snprintf(response, response_size, "{\"ok\":true}"); }
            else { set_message(app, "Could not move generation to trash"); snprintf(response, response_size, "{\"ok\":false,\"error\":\"move generation to trash failed\"}"); }
            return response;
        }
        if (!strcmp(app->pending_action, "reference.trash")) {
            bool removing_open_prompt = !strcmp(app->pending_path, app->prompt_path);
            bool moved = move_reference_to_trash(app, app->pending_path);
            app->pending_action[0] = app->pending_path[0] = 0;
            if (moved) { scan_refs(app); if (removing_open_prompt) { app->prompt_path[0]=app->prompt_edit[0]=0; app->prompt_dirty=app->prompt_editor_focused=app->prompt_save_error=app->prompt_limit_error=app->prompt_close_pending=false; app->edit_field[0]=0; app->show_full_image=false; } app->selected_ref = -1; save_reference_state(app); set_message(app, "Item moved to workspace/trash"); snprintf(response, response_size, "{\"ok\":true}"); }
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
            AppRun *run = &app->runs[app->run_count]; memset(run, 0, sizeof(*run)); run->pipe_fd = run->err_fd = -1; run->selected_output = -2;
            copy_text(run->id, sizeof(run->id), app->snapshot_run_id); copy_text(run->dir, sizeof(run->dir), app->snapshot_run_dir); make_dirs(run->dir);
            copy_text(run->prompt, sizeof(run->prompt), app->snapshot_prompt); copy_text(run->resolved_prompt, sizeof(run->resolved_prompt), app->snapshot_resolved);
            run->follow_log = true;
            bool started = false;
            bool pixel_only = app->rerun_pixel_only && *app->rerun_source;
            if (pixel_only) copy_text(run->source, sizeof(run->source), app->rerun_source);
            bool metadata_written = write_run_metadata(app, run, pixel_only);
            if (pixel_only) {
                run->started = GetTime(); run->state = RUN_RUNNING;
                started = start_pixel(app, run);
            } else started = start_generation(app, run);
            app->rerun_pixel_only = false; app->rerun_source[0] = 0; app->preview_ready = false;
            if (started) { app->active_run = app->run_count; app->selected_run = app->run_count++; app->view = VIEW_HOME; app->settings_view = false; set_message(app, metadata_written ? "Run started" : "Run started; metadata could not be saved"); snprintf(response, response_size, "{\"ok\":true,\"run\":\"%s\"}", run->id); }
            else { run->state = RUN_FAILED; run->finished = GetTime(); if (!*run->error) copy_text(run->error, sizeof(run->error), "could not start configured process"); snprintf(response, response_size, "{\"ok\":false,\"error\":\"%s\"}", run->error); }
        }
    } else if (!strcmp(action, "app.cancel")) {
        if (!strcmp(app->pending_action, "reference.trash")) { app->pending_action[0] = app->pending_path[0] = 0; set_message(app, "Move to trash canceled"); }
        if (!strcmp(app->pending_action, "run.trash")) { app->pending_action[0] = app->pending_path[0] = 0; set_message(app, "Move generation to trash canceled"); }
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
        for(int i=0;i<app->ref_count;i++) if(!strcmp(path,app->refs[i].path)){found=true;if(!strcmp(action,"reference.select")){app->selected_ref=i;app->refs[i].selected=!app->refs[i].selected;}else{if(app->refs[i].is_text&&!select_prompt_reference(app,&app->refs[i]))break;app->selected_ref=i;app->view=VIEW_LIBRARY;app->show_full_image=true;app->image_zoom=1.0f;}}
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
            app->rerun_pixel_only = true; app->preview_ready = true;
            make_run_id(app, app->snapshot_run_id, sizeof(app->snapshot_run_id));
            path_join(app->snapshot_run_dir, sizeof(app->snapshot_run_dir), app->runs_path, app->snapshot_run_id);
            resolve_prompt(app, old->prompt, app->snapshot_resolved, sizeof(app->snapshot_resolved), NULL);
            app->view = VIEW_GENERATION; app->settings_view = false; snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}");
            set_message(app, "Pixel rerun preview ready; confirm to start");
        }
    } else if (!strcmp(action,"run.select")) {
        int idx=atoi(value); if(idx>=0&&idx<app->run_count){app->selected_run=idx;app->view=VIEW_GENERATION;app->settings_view=false;app->show_run_logs=false;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"run index out of range\"}");
    } else if (!strcmp(action,"output.select")) {
        /* agent: gpt-6.1-sol | 2026-10-04 | Select generated output from thumbnail | v 3.37.0 | c43da3 */
        AppRun *run=get_run(app); int idx=atoi(value); if(run&&!strcmp(value,"original")){run->selected_output=-1;run->stage_output_selected=false;app->view=VIEW_GENERATION;app->settings_view=false;app->show_run_logs=false;snprintf(response,response_size,"{\"ok\":true}");}else if(run&&idx>=0&&idx<run->output_count){run->selected_output=idx;run->stage_output_selected=false;app->view=VIEW_GENERATION;app->settings_view=false;app->show_run_logs=false;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"output index out of range\"}");
        /* agent: gpt-6.1-sol | 2026-10-04 | Select generated output from thumbnail | v 3.37.0 | c43da3 */
    } else if (!strcmp(action,"stage.select")) {
        AppRun *run=get_run(app); int number=atoi(value); bool found=false; if(run)for(int i=0;i<run->stage_count;i++)if(run->stages[i].number==number){run->selected_stage=i;app->view=VIEW_GENERATION;app->show_run_logs=true;found=true;break;}
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
        if (run) { if (run->generated.path[0] && !strcmp(path, run->generated.path)) allowed = true; for (int i=0;i<run->output_count;i++) if (!strcmp(path,run->outputs[i].path)) allowed = true; for (int i=0;i<run->stage_count;i++) if (!strcmp(path,run->stages[i].path)) allowed = true; }
        if (allowed && open_path(path)) snprintf(response,response_size,"{\"ok\":true}"); else snprintf(response,response_size,"{\"ok\":false,\"error\":\"path is not a selected run artifact\"}");
    }
    else if (!strcmp(action, "app.action")) snprintf(response, response_size, "{\"ok\":false,\"error\":\"unknown action\"}");
    else snprintf(response, response_size, "{\"ok\":false,\"error\":\"unknown action\"}");
    return response;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route generation image and logs controls | v 3.35.0 | 94dd5b */
/* agent: gpt-6.1-sol | 2026-10-04 | Write generic metadata for every generation | v 3.19.0 | 6c0bb8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route named view navigation | v 3.12.0 | 0b288d */
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
        if (run->state == RUN_RUNNING) {
            update_child(app, run);
            if (run->state == RUN_RUNNING && run->process_kind == PROC_PIXEL && GetTime() - run->artifact_scan_at >= 0.75) {
                refresh_run_outputs(run);
                run->artifact_scan_at = GetTime();
            }
        }
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

typedef enum { INPUT_FRAME_IDLE, INPUT_FRAME_ACTIVE, INPUT_FRAME_DIRTY } InputFrameState;

/* Draw a shared prompt, rename, or settings input frame with InputFrameState; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Share prompt and settings input frames | v 3.18.0 | c8a86b */
static void draw_advanced_input_frame(Rectangle bounds, InputFrameState state) {
    Color fill = state == INPUT_FRAME_ACTIVE ? UI_PANEL_ALT : UI_PANEL;
    Color outline = state == INPUT_FRAME_ACTIVE ? UI_ACCENT : state == INPUT_FRAME_DIRTY ? UI_WARN : UI_PANEL_ALT;
    DrawRectangleRec(bounds, fill);
    DrawRectangleLinesEx(bounds, 1, outline);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Share prompt and settings input frames | v 3.18.0 | c8a86b */

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

/* agent: gpt-6.1-sol | 2026-10-04 | Preserve editor font glyph spacing | v 3.7.0 | e4fe10 */
typedef struct { int start, end; } PromptVisualRow;
static PromptVisualRow s_prompt_rows[APP_TEXT_CAP];
static int s_prompt_row_count;
static char s_prompt_display[APP_TEXT_CAP * 4];
static int s_prompt_display_offsets[APP_TEXT_CAP];

/* Return the UTF-8 byte count at text offset; return 1 for invalid input. */
static int prompt_char_bytes(const char *text, int offset, int length) {
    unsigned char c = (unsigned char)text[offset];
    int bytes = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
    if (offset + bytes > length) return 1;
    for (int i = 1; i < bytes; i++) if (((unsigned char)text[offset+i] & 0xc0) != 0x80) return 1;
    return bytes;
}

/* Return the previous UTF-8 character boundary before offset; return int. */
static int prompt_prev_boundary(const char *text, int offset) {
    if (offset <= 0) return 0;
    offset--;
    while (offset > 0 && ((unsigned char)text[offset] & 0xc0) == 0x80) offset--;
    return offset;
}

/* Build wrapped prompt rows within float width; return int row count. */
static int prompt_layout(const char *text, int length, float width, int font_size) {
    int row = 0, start = 0, x = 0;
    for (int i = 0; i < length;) {
        int bytes = prompt_char_bytes(text, i, length);
        if (text[i] == '\n' || text[i] == '\r') {
            s_prompt_rows[row++] = (PromptVisualRow){start, i};
            i += bytes;
            if (text[i-1] == '\r' && i < length && text[i] == '\n') i++;
            start = i; x = 0;
            continue;
        }
        char glyph[8] = {0};
        memcpy(glyph, text+i, (size_t)bytes);
        int glyph_width = text[i] == '\t' ? ui_measure_text("    ", font_size) : ui_measure_text(glyph, font_size);
        if (x > 0 && x + glyph_width > width) {
            s_prompt_rows[row++] = (PromptVisualRow){start, i};
            start = i; x = 0;
        }
        x += glyph_width;
        i += bytes;
    }
    s_prompt_rows[row++] = (PromptVisualRow){start, length};
    return s_prompt_row_count = row;
}

/* Convert one source row to display text and map byte offsets; return int bytes. */
static int prompt_build_display(const char *text, PromptVisualRow row) {
    int display = 0;
    s_prompt_display_offsets[row.start] = 0;
    for (int i = row.start; i < row.end;) {
        int bytes = prompt_char_bytes(text, i, row.end);
        for (int j = 1; j < bytes; j++) s_prompt_display_offsets[i+j] = display;
        if (text[i] == '\t') {
            memcpy(s_prompt_display + display, "    ", 4);
            display += 4;
        } else {
            memcpy(s_prompt_display + display, text + i, (size_t)bytes);
            display += bytes;
        }
        i += bytes;
        s_prompt_display_offsets[i] = display;
    }
    s_prompt_display[display] = 0;
    return display;
}

/* Measure a display substring by byte offsets; return pixel width as int. */
static int prompt_measure_display(int start, int end, int font_size) {
    char saved = s_prompt_display[end];
    s_prompt_display[end] = 0;
    int width = ui_measure_text(s_prompt_display + start, font_size);
    s_prompt_display[end] = saved;
    return width;
}

/* Insert bounded UTF-8 bytes at selection and update dirty state; return bool. */
static bool prompt_insert(App *app, const char *insert, int insert_bytes) {
    int length = (int)strlen(app->prompt_edit);
    int lo = app->prompt_caret < app->prompt_anchor ? app->prompt_caret : app->prompt_anchor;
    int hi = app->prompt_caret > app->prompt_anchor ? app->prompt_caret : app->prompt_anchor;
    if (length - (hi-lo) + insert_bytes >= APP_TEXT_CAP) { app->prompt_limit_error = true; set_message(app, "Prompt limit reached"); return false; }
    memmove(app->prompt_edit+lo+insert_bytes, app->prompt_edit+hi, (size_t)(length-hi+1));
    memcpy(app->prompt_edit+lo, insert, (size_t)insert_bytes);
    app->prompt_caret = app->prompt_anchor = lo + insert_bytes;
    app->prompt_dirty = true;
    app->prompt_limit_error = false;
    return true;
}

/* Encode one raylib Unicode codepoint into UTF-8; return byte count. */
static int prompt_encode(int codepoint, char out[5]) {
    if (codepoint < 0x80) { out[0]=(char)codepoint; return 1; }
    if (codepoint < 0x800) { out[0]=(char)(0xc0|(codepoint>>6)); out[1]=(char)(0x80|(codepoint&63)); return 2; }
    if (codepoint < 0x10000) { out[0]=(char)(0xe0|(codepoint>>12)); out[1]=(char)(0x80|((codepoint>>6)&63)); out[2]=(char)(0x80|(codepoint&63)); return 3; }
    if (codepoint <= 0x10ffff) { out[0]=(char)(0xf0|(codepoint>>18)); out[1]=(char)(0x80|((codepoint>>12)&63)); out[2]=(char)(0x80|((codepoint>>6)&63)); out[3]=(char)(0x80|(codepoint&63)); return 4; }
    return 0;
}

/* Find the nearest caret byte offset at mouse position in the editor; return int. */
static int prompt_hit_test(const char *text, int length, Rectangle body, int scroll, Vector2 mouse) {
    int line_h = 28, row = (int)((mouse.y-body.y)/line_h)+scroll;
    if (row < 0) return 0;
    if (row >= s_prompt_row_count) return length;
    PromptVisualRow visual=s_prompt_rows[row];
    prompt_build_display(text,visual);
    int start = visual.start, end = visual.end;
    for (int i = start; i < end;) {
        int bytes = prompt_char_bytes(text, i, length);
        int left=prompt_measure_display(0,s_prompt_display_offsets[i],22);
        int right=prompt_measure_display(0,s_prompt_display_offsets[i+bytes],22);
        if (mouse.x-body.x < (left+right)*0.5f) return i;
        i += bytes;
    }
    return end;
}

/* Draw and handle the prompt editor for App* app in Rectangle bounds; return void. */
static void draw_prompt_editor(App *app, Rectangle rect) {
    char *text = app->prompt_edit;
    int length = (int)strlen(text), font_size = 22, line_h = 28;
    Rectangle body = {rect.x+14, rect.y+10, rect.width-28, rect.height-54};
    draw_advanced_input_frame(rect, app->prompt_dirty ? INPUT_FRAME_DIRTY : INPUT_FRAME_IDLE);
    prompt_layout(text, length, body.width, font_size);
    int visible = (int)(body.height/line_h);
    int caret_row = 0;
    for (int r=0; r<s_prompt_row_count; r++) if (app->prompt_caret >= s_prompt_rows[r].start && app->prompt_caret <= s_prompt_rows[r].end) { caret_row=r; if (app->prompt_caret == s_prompt_rows[r].end && r+1<s_prompt_row_count && s_prompt_rows[r+1].start==app->prompt_caret) caret_row=r+1; break; }
    if (caret_row < app->prompt_scroll) app->prompt_scroll=caret_row;
    if (caret_row >= app->prompt_scroll+visible) app->prompt_scroll=caret_row-visible+1;
    int max_scroll=s_prompt_row_count-visible;
    if(max_scroll<0)max_scroll=0;
    if(app->prompt_scroll>max_scroll)app->prompt_scroll=max_scroll;
    if (app->prompt_scroll < 0) app->prompt_scroll=0;
    if (!app->prompt_close_pending && CheckCollisionPointRec(GetMousePosition(), body) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        app->prompt_caret=prompt_hit_test(text,length,body,app->prompt_scroll,GetMousePosition());
        app->prompt_anchor=app->prompt_caret; app->prompt_selecting=true; app->prompt_editor_focused=true; app->edit_field[0]=0;
    }
    if(!app->prompt_close_pending && app->prompt_selecting && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) app->prompt_caret=prompt_hit_test(text,length,body,app->prompt_scroll,GetMousePosition());
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) app->prompt_selecting=false;
    if (!app->prompt_close_pending && CheckCollisionPointRec(GetMousePosition(), body)) { float wheel=GetMouseWheelMove(); if(wheel) app->prompt_scroll -= (int)wheel*3; }
    if (app->prompt_scroll < 0) app->prompt_scroll=0;
    if (app->prompt_scroll > max_scroll) app->prompt_scroll=max_scroll;
    int lo=app->prompt_caret<app->prompt_anchor?app->prompt_caret:app->prompt_anchor;
    int hi=app->prompt_caret>app->prompt_anchor?app->prompt_caret:app->prompt_anchor;
    for (int r=app->prompt_scroll; r<s_prompt_row_count && r<app->prompt_scroll+visible; r++) {
        int y=(int)body.y+(r-app->prompt_scroll)*line_h;
        PromptVisualRow visual=s_prompt_rows[r];
        prompt_build_display(text,visual);
        int caret_x=r==caret_row&&app->prompt_caret>=visual.start&&app->prompt_caret<=visual.end?prompt_measure_display(0,s_prompt_display_offsets[app->prompt_caret],font_size):-1;
        int select_start=lo>visual.start?lo:visual.start, select_end=hi<visual.end?hi:visual.end;
        if (select_end>select_start) {
            int x=prompt_measure_display(0,s_prompt_display_offsets[select_start],font_size);
            int width=prompt_measure_display(s_prompt_display_offsets[select_start],s_prompt_display_offsets[select_end],font_size);
            DrawRectangle((int)body.x+x,y,width,line_h,(Color){65,105,155,210});
        }
        ui_draw_text(s_prompt_display,(int)body.x,y,font_size,UI_TEXT);
        if (select_end>select_start) {
            int offset=s_prompt_display_offsets[select_start], end=s_prompt_display_offsets[select_end];
            char saved=s_prompt_display[end]; s_prompt_display[end]=0;
            ui_draw_text(s_prompt_display+offset,(int)body.x+prompt_measure_display(0,offset,font_size),y,font_size,UI_TEXT);
            s_prompt_display[end]=saved;
        }
        if (caret_x>=0 && app->prompt_editor_focused) DrawLine((int)body.x+caret_x,y,(int)body.x+caret_x,y+font_size,UI_ACCENT);
    }
    Rectangle save={rect.x+rect.width-104,rect.y+rect.height-38,90,28};
    const char *status=app->prompt_save_error?"Save failed":app->prompt_limit_error?"Limit reached":app->prompt_dirty?"Unsaved changes":"Saved";
    ui_draw_text(status,(int)rect.x+14,(int)save.y+6,14,app->prompt_save_error||app->prompt_limit_error?UI_BAD:app->prompt_dirty?UI_WARN:UI_MUTED);
    if (!app->prompt_editor_focused || app->prompt_close_pending) return;
    bool control=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
    if (control && IsKeyPressed(KEY_A)) { app->prompt_anchor=0; app->prompt_caret=length; }
    else if (control && (IsKeyPressed(KEY_C)||IsKeyPressed(KEY_X))) {
        if (hi>lo) { char saved=text[hi]; text[hi]=0; SetClipboardText(text+lo); text[hi]=saved; if(IsKeyPressed(KEY_X)){app->prompt_caret=app->prompt_anchor=lo;memmove(text+lo,text+hi,(size_t)(length-hi+1));app->prompt_dirty=true;} }
    } else if (control && IsKeyPressed(KEY_V)) {
        const char *clip=GetClipboardText(); if(clip){char paste[APP_TEXT_CAP];int used=0;for(int i=0;clip[i]&&used+4<APP_TEXT_CAP;i++){if(clip[i]=='\r')continue;paste[used++]=clip[i];}paste[used]=0;prompt_insert(app,paste,used);}
    } else if (IsKeyPressed(KEY_BACKSPACE)||IsKeyPressed(KEY_DELETE)) {
        if(lo==hi){if(IsKeyPressed(KEY_BACKSPACE))lo=prompt_prev_boundary(text,app->prompt_caret);else if(hi<length)hi+=prompt_char_bytes(text,hi,length);}
        if(hi>lo){memmove(text+lo,text+hi,(size_t)(length-hi+1));app->prompt_caret=app->prompt_anchor=lo;app->prompt_dirty=true;}
    } else if (IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_HOME)||IsKeyPressed(KEY_END)||IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_DOWN)) {
        int next=app->prompt_caret;
        if(IsKeyPressed(KEY_LEFT)) next=lo<hi?lo:prompt_prev_boundary(text,next);
        else if(IsKeyPressed(KEY_RIGHT)) next=lo<hi?hi:next+(next<length?prompt_char_bytes(text,next,length):0);
        else if(IsKeyPressed(KEY_HOME)) next=s_prompt_rows[caret_row].start;
        else if(IsKeyPressed(KEY_END)) next=s_prompt_rows[caret_row].end;
        else { int target=caret_row+(IsKeyPressed(KEY_UP)?-1:1); if(target>=0&&target<s_prompt_row_count){PromptVisualRow current=s_prompt_rows[caret_row];prompt_build_display(text,current);int px=prompt_measure_display(0,s_prompt_display_offsets[app->prompt_caret],font_size);PromptVisualRow dest=s_prompt_rows[target];prompt_build_display(text,dest);next=dest.start;while(next<dest.end){int bytes=prompt_char_bytes(text,next,length);int left=prompt_measure_display(0,s_prompt_display_offsets[next],font_size);int right=prompt_measure_display(0,s_prompt_display_offsets[next+bytes],font_size);if((left+right)*0.5f>=px)break;next+=bytes;}}}
        app->prompt_caret=next; if(!shift)app->prompt_anchor=next;
    }
    if(IsKeyPressed(KEY_ENTER)&&control) save_open_prompt(app);
    else if(IsKeyPressed(KEY_ENTER)){char newline='\n';prompt_insert(app,&newline,1);}
    int key=GetCharPressed();
    while(key>0){if(!control&&key>=32){char encoded[5]={0};int bytes=prompt_encode(key,encoded);if(bytes)prompt_insert(app,encoded,bytes);}key=GetCharPressed();}
}

/* agent: gpt-6.1-sol | 2026-10-04 | Preserve editor font glyph spacing | v 3.7.0 | e4fe10 */
/* agent: gpt-6.1-sol | 2026-10-04 | Edit reference names with caret selection | v 3.9.0 | a4903c */
/* Return the basename length before a non-leading extension; return int. */
static int reference_basename_length(const char *name) {
    const char *dot=strrchr(name,'.');
    return dot&&dot!=name?(int)(dot-name):(int)strlen(name);
}

/* Measure filename bytes up to end using the active UI font; return int pixels. */
static int filename_measure(char *text, int end) {
    char saved=text[end]; text[end]=0;
    int width=ui_measure_text(text,16);
    text[end]=saved;
    return width;
}

/* Find the closest UTF-8 filename boundary to mouse x; return byte offset. */
static int filename_hit_test(App *app, Rectangle box, float mouse_x) {
    int length=(int)strlen(app->field_buffer), x=(int)(mouse_x-box.x-9)+app->field_scroll;
    for(int i=0;i<length;){
        int bytes=prompt_char_bytes(app->field_buffer,i,length);
        int left=filename_measure(app->field_buffer,i),right=filename_measure(app->field_buffer,i+bytes);
        if(x<(left+right)/2)return i;
        i+=bytes;
    }
    return length;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Share settings input editing controls | v 3.32.0 | 35831f */
/* Insert const char* text of int bytes into App* app; bool basename rejects path separators; return bool. */
static bool singleline_insert(App *app, const char *insert, int bytes, bool basename) {
    int length=(int)strlen(app->field_buffer);
    int lo=app->field_caret<app->field_anchor?app->field_caret:app->field_anchor;
    int hi=app->field_caret>app->field_anchor?app->field_caret:app->field_anchor;
    if((size_t)(length-(hi-lo))+ (size_t)bytes>=sizeof(app->field_buffer)){set_message(app,"Text is too long");return false;}
    memmove(app->field_buffer+lo+bytes,app->field_buffer+hi,(size_t)(length-hi+1));
    for(int i=0;i<bytes;i++)if((!basename||(insert[i]!='/'&&insert[i]!='\\'))&&(unsigned char)insert[i]>=32)app->field_buffer[lo+i]=insert[i];
    app->field_caret=app->field_anchor=lo+bytes;
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Share settings input editing controls | v 3.32.0 | 35831f */

/* agent: gpt-6.1-sol | 2026-10-04 | Share settings input editing controls | v 3.32.0 | 35831f */
/* Draw and edit AppReference* ref in Rectangle box using App* app; return void. */
static void draw_reference_name_field(App *app, AppReference *ref, Rectangle box) {
    bool editing=!strcmp(app->edit_field,"reference.name");
    draw_advanced_input_frame(box, editing ? INPUT_FRAME_ACTIVE : INPUT_FRAME_IDLE);
    if(!editing){int n=reference_basename_length(ref->name);char shown[256];memcpy(shown,ref->name,(size_t)n);shown[n]=0;ui_draw_text(shown,(int)box.x+9,(int)box.y+8,16,UI_TEXT);}
    if(CheckCollisionPointRec(GetMousePosition(),box)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        if(!editing){int n=reference_basename_length(ref->name);memcpy(app->field_buffer,ref->name,(size_t)n);app->field_buffer[n]=0;app->field_scroll=0;copy_text(app->edit_field,sizeof(app->edit_field),"reference.name");app->prompt_editor_focused=false;editing=true;}
        app->field_caret=filename_hit_test(app,box,GetMousePosition().x);app->field_anchor=app->field_caret;app->field_selecting=true;
    }
    if(app->field_selecting&&IsMouseButtonDown(MOUSE_BUTTON_LEFT))app->field_caret=filename_hit_test(app,box,GetMousePosition().x);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))app->field_selecting=false;
    if(!editing)return;
    int length=(int)strlen(app->field_buffer),inner=(int)box.width-18;
    int caret_x=filename_measure(app->field_buffer,app->field_caret);
    if(caret_x-app->field_scroll>inner)app->field_scroll=caret_x-inner;
    if(caret_x<app->field_scroll)app->field_scroll=caret_x;
    if(app->field_scroll<0)app->field_scroll=0;
    Rectangle clip={box.x+8,box.y+2,box.width-16,box.height-4};
    BeginScissorMode((int)clip.x,(int)clip.y,(int)clip.width,(int)clip.height);
    int lo=app->field_caret<app->field_anchor?app->field_caret:app->field_anchor;
    int hi=app->field_caret>app->field_anchor?app->field_caret:app->field_anchor;
    if(hi>lo){int x=filename_measure(app->field_buffer,lo)-app->field_scroll;int width=filename_measure(app->field_buffer,hi)-filename_measure(app->field_buffer,lo);DrawRectangle((int)clip.x+x,(int)box.y+5,width,26,(Color){65,105,155,210});}
    ui_draw_text(app->field_buffer,(int)clip.x-app->field_scroll,(int)box.y+8,16,UI_TEXT);
    if(hi>lo){char saved=app->field_buffer[hi];app->field_buffer[hi]=0;ui_draw_text(app->field_buffer+lo,(int)clip.x+filename_measure(app->field_buffer,lo)-app->field_scroll,(int)box.y+8,16,UI_TEXT);app->field_buffer[hi]=saved;}
    DrawLine((int)clip.x+caret_x-app->field_scroll,(int)box.y+7,(int)clip.x+caret_x-app->field_scroll,(int)box.y+29,UI_ACCENT);
    EndScissorMode();
    bool control=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
    if(control&&IsKeyPressed(KEY_A)){app->field_anchor=0;app->field_caret=length;}
    else if(control&&(IsKeyPressed(KEY_C)||IsKeyPressed(KEY_X))){if(hi>lo){char saved=app->field_buffer[hi];app->field_buffer[hi]=0;SetClipboardText(app->field_buffer+lo);app->field_buffer[hi]=saved;if(IsKeyPressed(KEY_X)){memmove(app->field_buffer+lo,app->field_buffer+hi,(size_t)(length-hi+1));app->field_caret=app->field_anchor=lo;}}}
    else if(control&&IsKeyPressed(KEY_V)){const char *clip_text=GetClipboardText();if(clip_text){char pasted[sizeof(app->field_buffer)];int n=0;for(int i=0;clip_text[i]&&n+1<(int)sizeof(pasted);i++){unsigned char c=(unsigned char)clip_text[i];if(c>=32&&c!=127&&c!='/'&&c!='\\')pasted[n++]=clip_text[i];}pasted[n]=0;singleline_insert(app,pasted,n,true);}}
    else if(IsKeyPressed(KEY_BACKSPACE)||IsKeyPressed(KEY_DELETE)){if(lo==hi){if(IsKeyPressed(KEY_BACKSPACE))lo=prompt_prev_boundary(app->field_buffer,app->field_caret);else if(hi<length)hi+=prompt_char_bytes(app->field_buffer,hi,length);}if(hi>lo){memmove(app->field_buffer+lo,app->field_buffer+hi,(size_t)(length-hi+1));app->field_caret=app->field_anchor=lo;}}
    else if(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_HOME)||IsKeyPressed(KEY_END)){int next=app->field_caret;if(IsKeyPressed(KEY_LEFT))next=lo<hi?lo:prompt_prev_boundary(app->field_buffer,next);else if(IsKeyPressed(KEY_RIGHT))next=lo<hi?hi:next+(next<length?prompt_char_bytes(app->field_buffer,next,length):0);else next=IsKeyPressed(KEY_HOME)?0:length;app->field_caret=next;if(!shift)app->field_anchor=next;}
    int key=GetCharPressed();
    while(key>0){if(!control&&key>=32&&key!='/'&&key!='\\'){char encoded[5]={0};int n=prompt_encode(key,encoded);if(n)singleline_insert(app,encoded,n,true);}key=GetCharPressed();}
    if(IsKeyPressed(KEY_ENTER)){if(rename_reference_file(app,ref,app->field_buffer)){app->edit_field[0]=0;app->field_scroll=0;}}
    if(IsKeyPressed(KEY_ESCAPE)){app->edit_field[0]=0;app->field_scroll=0;}
}
/* agent: gpt-6.1-sol | 2026-10-04 | Share settings input editing controls | v 3.32.0 | 35831f */
/* agent: gpt-6.1-sol | 2026-10-04 | Edit reference names with caret selection | v 3.9.0 | a4903c */

/* agent: gpt-6.1-sol | 2026-10-04 | Autosave settings fields on edit | v 3.34.0 | 6704f1 */
/* Draw App* app's const char* field, label and value in Rectangle bounds; return void. */
static void draw_profile_input(App *app, const char *field, const char *label, const char *value, Rectangle rect) {
    ui_draw_text(label, (int)rect.x, (int)rect.y, 15, UI_MUTED);
    Rectangle box = {rect.x, rect.y + 22, rect.width, 38};
    bool editing = !strcmp(app->edit_field, field);
    draw_advanced_input_frame(box, editing ? INPUT_FRAME_ACTIVE : INPUT_FRAME_IDLE);
    if (CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (!editing) {
            copy_text(app->edit_field, sizeof(app->edit_field), field);
            copy_text(app->field_buffer, sizeof(app->field_buffer), value);
            app->field_caret = app->field_anchor = (int)strlen(app->field_buffer);
            app->field_scroll = 0;
            app->prompt_editor_focused = false;
        }
        editing = true;
        app->field_caret=filename_hit_test(app,box,GetMousePosition().x);
        app->field_anchor=app->field_caret;
        app->field_selecting=true;
    }
    if(editing&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!CheckCollisionPointRec(GetMousePosition(),box)){
        app->edit_field[0]=0;app->field_scroll=0;editing=false;
    }
    if(app->field_selecting&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&editing)app->field_caret=filename_hit_test(app,box,GetMousePosition().x);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))app->field_selecting=false;
    int length=(int)strlen(editing?app->field_buffer:value);
    int inner=(int)box.width-18;
    int caret_x=editing?filename_measure(app->field_buffer,app->field_caret):0;
    if(editing&&caret_x-app->field_scroll>inner)app->field_scroll=caret_x-inner;
    if(editing&&caret_x<app->field_scroll)app->field_scroll=caret_x;
    if(app->field_scroll<0)app->field_scroll=0;
    Rectangle clip={box.x+8,box.y+2,box.width-16,box.height-4};
    BeginScissorMode((int)clip.x,(int)clip.y,(int)clip.width,(int)clip.height);
    int lo=app->field_caret<app->field_anchor?app->field_caret:app->field_anchor;
    int hi=app->field_caret>app->field_anchor?app->field_caret:app->field_anchor;
    const char *shown=editing?app->field_buffer:value;
    int scroll=editing?app->field_scroll:0;
    if(editing&&hi>lo){int sx=filename_measure(app->field_buffer,lo)-scroll;int sw=filename_measure(app->field_buffer,hi)-filename_measure(app->field_buffer,lo);DrawRectangle((int)clip.x+sx,(int)box.y+5,sw,26,(Color){65,105,155,210});}
    ui_draw_text(shown,(int)clip.x-scroll,(int)box.y+8,16,UI_TEXT);
    if(editing&&hi>lo){char saved=app->field_buffer[hi];app->field_buffer[hi]=0;ui_draw_text(app->field_buffer+lo,(int)clip.x+filename_measure(app->field_buffer,lo)-scroll,(int)box.y+8,16,UI_TEXT);app->field_buffer[hi]=saved;}
    if(editing)DrawLine((int)clip.x+caret_x-app->field_scroll,(int)box.y+7,(int)clip.x+caret_x-app->field_scroll,(int)box.y+29,UI_ACCENT);
    EndScissorMode();
    if (!editing) return;
    char original[sizeof(app->field_buffer)];
    copy_text(original,sizeof(original),app->field_buffer);
    bool control=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
    lo=app->field_caret<app->field_anchor?app->field_caret:app->field_anchor;
    hi=app->field_caret>app->field_anchor?app->field_caret:app->field_anchor;
    if(control&&IsKeyPressed(KEY_A)){app->field_anchor=0;app->field_caret=length;}
    else if(control&&(IsKeyPressed(KEY_C)||IsKeyPressed(KEY_X))){if(hi>lo){char saved=app->field_buffer[hi];app->field_buffer[hi]=0;SetClipboardText(app->field_buffer+lo);app->field_buffer[hi]=saved;if(IsKeyPressed(KEY_X)){memmove(app->field_buffer+lo,app->field_buffer+hi,(size_t)(length-hi+1));app->field_caret=app->field_anchor=lo;}}}
    else if(control&&IsKeyPressed(KEY_V)){const char *clip=GetClipboardText();if(clip){char paste[sizeof(app->field_buffer)];int n=0;for(int i=0;clip[i]&&n+1<(int)sizeof(paste);i++){unsigned char c=(unsigned char)clip[i];if(c>=32&&c!=127)paste[n++]=clip[i];}paste[n]=0;singleline_insert(app,paste,n,false);}}
    else if(IsKeyPressed(KEY_BACKSPACE)||IsKeyPressed(KEY_DELETE)){if(lo==hi){if(IsKeyPressed(KEY_BACKSPACE))lo=prompt_prev_boundary(app->field_buffer,app->field_caret);else if(hi<length)hi+=prompt_char_bytes(app->field_buffer,hi,length);}if(hi>lo){memmove(app->field_buffer+lo,app->field_buffer+hi,(size_t)(length-hi+1));app->field_caret=app->field_anchor=lo;}}
    else if(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_HOME)||IsKeyPressed(KEY_END)){int next=app->field_caret;if(IsKeyPressed(KEY_LEFT))next=lo<hi?lo:prompt_prev_boundary(app->field_buffer,next);else if(IsKeyPressed(KEY_RIGHT))next=lo<hi?hi:next+(next<length?prompt_char_bytes(app->field_buffer,next,length):0);else next=IsKeyPressed(KEY_HOME)?0:length;app->field_caret=next;if(!shift)app->field_anchor=next;}
    int key = GetCharPressed();
    while (key > 0) {
        if(!control&&key>=32){char encoded[5]={0};int bytes=prompt_encode(key,encoded);if(bytes)singleline_insert(app,encoded,bytes,false);}
        key = GetCharPressed();
    }
    if(strcmp(original,app->field_buffer)){
        char ef[128], ev[APP_TEXT_CAP * 2], req[APP_TEXT_CAP * 2 + 256];
        json_escape(field, ef, sizeof(ef)); json_escape(app->field_buffer, ev, sizeof(ev));
        snprintf(req, sizeof(req), "{\"action\":\"profile.set\",\"field\":\"%s\",\"value\":\"%s\"}", ef, ev);
        AppDispatch(app, req, s_action_response, sizeof(s_action_response));
    }
    if (IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_ENTER)) { app->edit_field[0] = 0; app->field_scroll = 0; }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Autosave settings fields on edit | v 3.34.0 | 6704f1 */
/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
/* Draw a plus tile in Rectangle bounds with const char* label and bool active; return void. */
static void draw_plus_tile(Rectangle bounds, const char *label, bool active) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    Color tone = active || hovered ? UI_ACCENT : UI_PANEL_ALT;
    int x = (int)bounds.x, y = (int)bounds.y, tile = (int)bounds.width;
    DrawRectangleRec(bounds, UI_PANEL);
    DrawRectangleLinesEx(bounds, 1, tone);
    DrawCircleLines(x + tile / 2, y + tile / 2, 25, tone);
    DrawLine(x + tile / 2 - 12, y + tile / 2, x + tile / 2 + 12, y + tile / 2, tone);
    DrawLine(x + tile / 2, y + tile / 2 - 12, x + tile / 2, y + tile / 2 + 12, tone);
    ui_draw_text(label, x + 12, y + tile - 25, 14, UI_TEXT);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */

/* Draw the generation action in a library tile and return void. */
static void draw_generate_tile(Rectangle bounds, const char *label, bool active) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    Color tone = active || hovered ? UI_ACCENT : UI_PANEL_ALT;
    int x = (int)bounds.x, y = (int)bounds.y, tile = (int)bounds.width;
    int cx = x + tile / 2, cy = y + tile / 2;
    DrawRectangleRec(bounds, UI_PANEL);
    DrawRectangleLinesEx(bounds, 1, tone);
    DrawCircleLines(cx, cy, 25, tone);
    DrawTriangle((Vector2){(float)(cx - 6), (float)(cy - 11)},
                 (Vector2){(float)(cx - 6), (float)(cy + 11)},
                 (Vector2){(float)(cx + 12), (float)cy}, tone);
    ui_draw_text(label, x + 12, y + tile - 25, 14, UI_TEXT);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Add Generate as first library tile | v 3.43.0 | d9527c */

/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
/* Draw library references and Generate for App* app; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Add generate control to library | v 3.12.0 | 4e90c5 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
static void draw_references(App *app) {
    int w = GetScreenWidth(),h=GetScreenHeight();
    Rectangle menu={80,132,220,100};
    bool menu_was_open=app->show_add_menu;
    int margin = 40, top = 120,bottom=h-44;
    int cols = w / 250; if (cols < 1) cols = 1;
    int gap = 20, tile = (w - margin * 2 - gap * (cols - 1)) / cols;
    if (tile > 260) tile = 260;
    int visible_count=app->ref_count+2;
    int rows=(visible_count+cols-1)/cols,max_scroll=rows*(tile+gap)-(bottom-top);if(max_scroll<0)max_scroll=0;
    Rectangle area={24,(float)top,(float)w-48,(float)(bottom-top)};
    if(CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel!=0)app->reference_scroll-=(int)(wheel*120);}
    if(app->reference_scroll<0)app->reference_scroll=0;
    if(app->reference_scroll>max_scroll)app->reference_scroll=max_scroll;
    BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    int visible = 0;
    int generate_x=margin,generate_y=top-app->reference_scroll;
    Rectangle generate_box={(float)generate_x,(float)generate_y,(float)tile,(float)tile};
    draw_generate_tile(generate_box,app->active_run>=0?"Run active":"Generate",false);
    if(app->active_run<0&&CheckCollisionPointRec(GetMousePosition(),generate_box)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        app->preview_ready=false;
        AppDispatch(app,"{\"action\":\"generation.preview\"}",s_action_response,sizeof(s_action_response));
        if(app->preview_ready)AppDispatch(app,"{\"action\":\"generation.confirm\"}",s_action_response,sizeof(s_action_response));
    }
    visible++;
    int add_x=margin+(visible%cols)*(tile+gap),add_y=top+(visible/cols)*(tile+gap)-app->reference_scroll;
    Rectangle add_box={(float)add_x,(float)add_y,(float)tile,(float)tile};
    draw_plus_tile(add_box,"Add reference",app->show_add_menu);
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
/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Add generate control to library | v 3.12.0 | 4e90c5 */

/* Draw generation cards and route App* app to Library or selected details; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Draw live generation cards | v 3.12.0 | cac401 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Tone plus tile on hover | v 3.29.0 | 99ec42 */
/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
static void draw_generation_grid(App *app) {
    int w = GetScreenWidth(), h = GetScreenHeight(), margin = 40, top = 120, bottom = h - 44;
    int cols = w / 250; if (cols < 1) cols = 1;
    int gap = 20, tile = (w - margin * 2 - gap * (cols - 1)) / cols;
    if (tile > 260) tile = 260;
    int count = app->run_count + 1, rows = (count + cols - 1) / cols;
    int max_scroll = rows * (tile + gap) - (bottom - top); if (max_scroll < 0) max_scroll = 0;
    Rectangle area = {24, (float)top, (float)w - 48, (float)(bottom - top)};
    if (CheckCollisionPointRec(GetMousePosition(), area)) app->reference_scroll -= (int)(GetMouseWheelMove() * 120);
    if (app->reference_scroll < 0) app->reference_scroll = 0;
    if (app->reference_scroll > max_scroll) app->reference_scroll = max_scroll;
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    for (int card = 0; card < count; card++) {
        int x = margin + card % cols * (tile + gap);
        int y = top + card / cols * (tile + gap) - app->reference_scroll;
        Rectangle box = {(float)x, (float)y, (float)tile, (float)tile};
        if (card == 0) {
            bool hovered = CheckCollisionPointRec(GetMousePosition(), box);
            draw_plus_tile(box,"New generation",false);
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app->view = VIEW_LIBRARY; app->settings_view = false; }
            continue;
        }
        DrawRectangleRec(box, UI_PANEL);
        int index = app->run_count - card;
        AppRun *run = &app->runs[index];
        AppArtifact *preview = run->output_count ? &run->outputs[0] : run->generated.thumb.id ? &run->generated : NULL;
        if (preview && preview->thumb.id) {
            float scale = fminf(box.width / preview->thumb.width, (box.height - 42) / preview->thumb.height);
            Rectangle image = {box.x + (box.width - preview->thumb.width * scale) * .5f, box.y + (box.height - 42 - preview->thumb.height * scale) * .5f, preview->thumb.width * scale, preview->thumb.height * scale};
            DrawTexturePro(preview->thumb, (Rectangle){0, 0, (float)preview->thumb.width, (float)preview->thumb.height}, image, (Vector2){0, 0}, 0, WHITE);
        }
        Color tone = run->state == RUN_RUNNING ? UI_ACCENT : run->state == RUN_FAILED ? UI_BAD : run->state == RUN_SUCCEEDED ? UI_GOOD : UI_WARN;
        const char *state = run->state == RUN_RUNNING ? "RUNNING" : run->state == RUN_SUCCEEDED ? "COMPLETE" : run->state == RUN_FAILED ? "FAILED" : "CANCELED";
        DrawRectangle(x, y + tile - 42, tile, 42, (Color){20, 22, 26, 240});
        ui_draw_text(TextFormat("%.23s  %s", run->id, state), x + 8, y + tile - 36, 13, tone);
        double elapsed = run->started > 0 ? ((run->state == RUN_RUNNING ? GetTime() : run->finished) - run->started) : 0;
        ui_draw_text(TextFormat("%.22s  ·  %.0fs", run->current_step, elapsed), x + 8, y + tile - 18, 12, UI_MUTED);
        if (run->state == RUN_RUNNING) {
            float phase = fmodf((float)GetTime() * 0.8f, 2.0f); if (phase > 1) phase = 2 - phase;
            DrawRectangle(x + 8 + (int)((tile - 24) * phase), y + tile - 3, 16, 2, UI_ACCENT);
        }
        Rectangle trash = {box.x + box.width - 28, box.y + 6, 22, 24};
        bool trash_hover = CheckCollisionPointRec(GetMousePosition(), trash);
        DrawRectangleRec(trash, (Color){20, 22, 26, 225});
        DrawLine((int)trash.x + 5, (int)trash.y + 8, (int)trash.x + 17, (int)trash.y + 8, trash_hover ? UI_BAD : UI_MUTED);
        DrawLine((int)trash.x + 7, (int)trash.y + 10, (int)trash.x + 8, (int)trash.y + 19, trash_hover ? UI_BAD : UI_MUTED);
        DrawLine((int)trash.x + 15, (int)trash.y + 10, (int)trash.x + 14, (int)trash.y + 19, trash_hover ? UI_BAD : UI_MUTED);
        DrawLine((int)trash.x + 8, (int)trash.y + 19, (int)trash.x + 14, (int)trash.y + 19, trash_hover ? UI_BAD : UI_MUTED);
        DrawLine((int)trash.x + 9, (int)trash.y + 5, (int)trash.x + 13, (int)trash.y + 5, trash_hover ? UI_BAD : UI_MUTED);
        DrawRectangleLinesEx(box, app->selected_run == index ? 2 : 1, app->selected_run == index ? UI_ACCENT : UI_PANEL_ALT);
        if (CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (trash_hover) {
                if (run->state == RUN_RUNNING) set_message(app, "Stop the generation before moving it to trash");
                else { copy_text(app->pending_action, sizeof(app->pending_action), "run.trash"); copy_text(app->pending_path, sizeof(app->pending_path), run->dir); }
            } else { app->selected_run = index; app->view = VIEW_GENERATION; app->settings_view = false; app->show_run_logs = false; }
        }
    }
    EndScissorMode();
}
/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
/* agent: gpt-6.1-sol | 2026-10-04 | Tone plus tile on hover | v 3.29.0 | 99ec42 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Draw live generation cards | v 3.12.0 | cac401 */
/* agent: gpt-6.1-sol | 2026-10-04 | Capture plus menu clicks exclusively | v 3.3.0 | 7ce4da */
/* agent: gpt-6.1-sol | 2026-10-04 | Simplify primary views and group advanced controls | v 3.3.0 | 3d4f2f */

/* agent: gpt-6.1-sol | 2026-10-04 | Anchor image zoom at cursor | v 3.29.0 | 0617bb */
/* Draw Texture2D texture in Rectangle bounds using App* app; return void. */
static void draw_image_view(App *app, Texture2D texture, int x, int y, int width, int height) {
    if (!texture.id) { ui_draw_text("No image selected", x + 20, y + 24, 18, UI_MUTED); return; }
    Rectangle area = {(float)x,(float)y,(float)width,(float)height};
    Vector2 mouse = GetMousePosition();
    bool over = CheckCollisionPointRec(mouse, area);
    float previous_zoom = app->image_zoom, zoom = previous_zoom;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) zoom *= 1.15f;
    if (IsKeyPressed(KEY_MINUS)) zoom /= 1.15f;
    if (over) { float wheel=GetMouseWheelMove(); if(wheel>0)zoom*=1.12f; else if(wheel<0)zoom/=1.12f; }
    zoom = fminf(16.0f, fmaxf(0.1f, zoom));
    if (over && previous_zoom > 0.0f && zoom != previous_zoom) {
        float factor = zoom / previous_zoom;
        float dx = mouse.x - ((float)x + width * 0.5f), dy = mouse.y - ((float)y + height * 0.5f);
        app->image_pan_x = dx - (dx - app->image_pan_x) * factor;
        app->image_pan_y = dy - (dy - app->image_pan_y) * factor;
    }
    app->image_zoom = zoom;
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) { Vector2 d = GetMouseDelta(); app->image_pan_x += d.x; app->image_pan_y += d.y; }
    float z = app->image_zoom;
    float scale = fminf((float)width / texture.width, (float)height / texture.height) * z;
    float iw = texture.width * scale, ih = texture.height * scale;
    Rectangle dst = {x + (width - iw) * 0.5f + app->image_pan_x, y + (height - ih) * 0.5f + app->image_pan_y, iw, ih};
    DrawRectangle(x, y, width, height, (Color){27,28,32,255});
    if (s_image_shader.id) BeginShaderMode(s_image_shader);
    DrawTexturePro(texture, (Rectangle){0,0,(float)texture.width,(float)texture.height}, dst, (Vector2){0,0}, 0, WHITE);
    if (s_image_shader.id) EndShaderMode();
}
/* agent: gpt-6.1-sol | 2026-10-04 | Anchor image zoom at cursor | v 3.29.0 | 0617bb */

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

/* agent: gpt-6.1-sol | 2026-10-04 | Remove generated output arrow controls | v 3.37.0 | f5e94d */
/* agent: gpt-6.1-sol | 2026-10-04 | Add reset transform image button | v 3.30.0 | 5bd415 */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Open images from viewer footer | v 3.35.0 | 63bf6c */
/* Draw AppArtifact* image, optional AppReference* ref and AppRun* run in int bounds using App* app; return void. */
static void draw_shared_image_view(App *app, AppArtifact *image, AppReference *ref, AppRun *run, int x, int y, int width, int height) {
    if (!image || (!image->texture.id && !image->thumb.id)) { ui_draw_text("Image is not available yet.", x, y, 18, UI_MUTED); return; }
    const char *path = image->path;
    if (ref) draw_reference_name_field(app, ref, (Rectangle){(float)x, (float)(y - 54), fminf(420, (float)width - 360), 36});
    else if (!run) ui_draw_text(image->name, x, y - 42, 17, UI_TEXT);
    int frame_y = y, footer_h = 48;
    int frame_h = height;
    if (frame_h < footer_h + 1) frame_h = footer_h + 1;
    Rectangle frame = {(float)x, (float)frame_y, (float)width, (float)frame_h};
    DrawRectangleRec(frame, UI_PANEL);
    DrawRectangleLinesEx(frame, 1, UI_PANEL_ALT);
    int image_y = frame_y + 1, image_h = frame_h - footer_h - 2;
    if (image_h < 1) image_h = 1;
    Texture2D view_texture = image->texture.id ? image->texture : image->thumb;
    draw_image_view(app, view_texture, x, image_y, width, image_h);
    int footer_y = frame_y + frame_h - footer_h;
    DrawRectangle(x + 1, footer_y, width - 2, footer_h - 1, UI_PANEL);
    ui_draw_text(TextFormat("%d x %d  ·  Wheel: zoom  ·  Middle drag: pan", image->width, image->height), x + 12, footer_y + 16, 14, UI_MUTED);
    Rectangle open_button = {(float)(x + width - 90), (float)(footer_y + 6), 36, 36};
    bool open_hovered = CheckCollisionPointRec(GetMousePosition(), open_button);
    if (button(open_button, "", false)) open_path(path);
    Color open_tone = open_hovered ? UI_ACCENT : UI_TEXT;
    int ox = (int)(open_button.x + 10), oy = (int)(open_button.y + 10);
    DrawRectangleLines(ox, oy + 3, 13, 13, open_tone);
    DrawLine(ox + 7, oy + 9, ox + 17, oy - 1, open_tone);
    DrawLine(ox + 11, oy - 1, ox + 17, oy - 1, open_tone);
    DrawLine(ox + 17, oy - 1, ox + 17, oy + 5, open_tone);
    Rectangle reset_button = {(float)(x + width - 48), (float)(footer_y + 6), 36, 36};
    bool reset_hovered = CheckCollisionPointRec(GetMousePosition(), reset_button);
    if (button(reset_button, "", false)) {
        app->image_zoom = 1.0f; app->image_pan_x = 0.0f; app->image_pan_y = 0.0f;
    }
    Color reset_tone = reset_hovered ? UI_ACCENT : UI_TEXT;
    int cx = (int)(reset_button.x + reset_button.width * 0.5f), cy = (int)(reset_button.y + reset_button.height * 0.5f);
    DrawLine(cx - 8, cy - 5, cx - 8, cy - 9, reset_tone); DrawLine(cx - 8, cy - 9, cx - 4, cy - 9, reset_tone);
    DrawLine(cx + 8, cy - 5, cx + 8, cy - 9, reset_tone); DrawLine(cx + 8, cy - 9, cx + 4, cy - 9, reset_tone);
    DrawLine(cx - 8, cy + 5, cx - 8, cy + 9, reset_tone); DrawLine(cx - 8, cy + 9, cx - 4, cy + 9, reset_tone);
    DrawLine(cx + 8, cy + 5, cx + 8, cy + 9, reset_tone); DrawLine(cx + 8, cy + 9, cx + 4, cy + 9, reset_tone);
    DrawCircleLines(cx, cy, 3, reset_tone);
    Rectangle image_area = {(float)x, (float)image_y, (float)width, (float)image_h};
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(GetMousePosition(), image_area)) {
        if (run) app->view = VIEW_HOME;
        else app->show_full_image = false;
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Remove generated output arrow controls | v 3.37.0 | f5e94d */
/* agent: gpt-6.1-sol | 2026-10-04 | Open images from viewer footer | v 3.35.0 | 63bf6c */
/* agent: gpt-6.1-sol | 2026-10-04 | Add reset transform image button | v 3.30.0 | 5bd415 */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Move image details into frame footer | v 3.20.0 | 8869d6 */
/* agent: gpt-6.1-sol | 2026-10-04 | Share the reference and output viewer | v 3.13.0 | e45697 */

/* agent: gpt-6.1-sol | 2026-10-04 | Align artifact dots on a compact grid | v 3.40.0 | 15e2d2 */
typedef struct {
    AppArtifact *artifact;
    Rectangle hit, image;
    int kind, index;
} ArtifactDot;

static const int artifact_pipeline_order[] = {40,36,35,34,33,32,37,39,31,30,29,28,27,26,25,24,22,20,38,9,10};

/* Return the pixel pipeline order for int stage_number; return int. */
static int artifact_stage_rank(int stage_number) {
    for (int i = 0; i < (int)(sizeof(artifact_pipeline_order) / sizeof(artifact_pipeline_order[0])); i++) if (artifact_pipeline_order[i] == stage_number) return i;
    return (int)(sizeof(artifact_pipeline_order) / sizeof(artifact_pipeline_order[0]));
}

/* Return the horizontal pipeline phase for int stage_number; return int. */
static int artifact_stage_group(int stage_number) {
    if (stage_number == 40 || stage_number == 36 || stage_number == 35 || stage_number == 34) return 0;
    if (stage_number == 33 || stage_number == 32 || stage_number == 37 || stage_number == 39 || stage_number == 31 || stage_number == 30) return 1;
    if (stage_number == 28 || stage_number == 27 || stage_number == 26 || stage_number == 29) return 2;
    if (stage_number == 25 || stage_number == 24 || stage_number == 22 || stage_number == 20) return 3;
    if (stage_number == 38 || stage_number == 9 || stage_number == 10) return 4;
    return -1;
}

/* Build clickable source, stage, and final dots for AppRun* run in Rectangle bounds; return int count. */
static int layout_artifact_dots(AppRun *run, Rectangle bounds, ArtifactDot *dots, int capacity) {
    if (bounds.width <= 0 || bounds.height <= 0) return 0;
    int outputs = 0, count = 0;
    bool present[5] = {false};
    for (int i = 0; i < run->stage_count; i++) {
        int group = artifact_stage_group(run->stages[i].number);
        if (group >= 0) present[group] = true;
    }
    int groups = 0;
    for (int i = 0; i < 5; i++) groups += present[i];
    for (int i = 0; i < run->output_count; i++) if (run->outputs[i].thumb.id || run->outputs[i].texture.id) outputs++;
    int rows = outputs > 0 ? 1 : 0;
    rows += groups;
    bool has_source = run->generated.thumb.id || run->generated.texture.id;
    rows += has_source;
    if (!rows) return 0;
    if (capacity < outputs + run->stage_count + has_source) return 0;
    int columns = outputs > 6 ? outputs : 6;
    float grid_width = fminf((float)columns * 16.0f, fmaxf(0.0f, bounds.width - 4.0f));
    if (grid_width <= 0.0f) return 0;
    float cell = grid_width / columns;
    float grid_left = bounds.x + bounds.width - grid_width - 2.0f;
    float slot = fminf(24.0f, bounds.height / rows);
    if (slot < 0.1f) return 0;
    float used_height = slot * rows, top = bounds.y + (bounds.height - used_height) * 0.5f;
    float base_size = fminf(8.0f, fminf(slot * 0.42f, cell * 0.5f));
    int row = 0;
    if (outputs) {
        int first_column = (columns - outputs) / 2;
        int visible = 0;
        for (int i = 0; i < run->output_count; i++) {
            AppArtifact *artifact = &run->outputs[i];
            if (!artifact->thumb.id && !artifact->texture.id) continue;
            float size = fminf(base_size, cell * 0.5f);
            float cx = grid_left + (first_column + visible + 0.5f) * cell;
            float cy = top + slot * 0.5f;
            ArtifactDot *dot = &dots[count++];
            dot->artifact = artifact; dot->kind = 1; dot->index = i;
            dot->image = (Rectangle){cx - size * 0.5f, cy - size * 0.5f, size, size};
            dot->hit = (Rectangle){grid_left + (first_column + visible) * cell, top, cell, slot};
            visible++;
        }
        row++;
    }
    for (int group = 0; group < 5; group++) {
        if (!present[group]) continue;
        int group_count = 0;
        for (int i = 0; i < run->stage_count; i++) if (artifact_stage_group(run->stages[i].number) == group) group_count++;
        int first_column = (columns - group_count) / 2;
        float t = rows > 1 ? (float)row / (float)(rows - 1) : 0.5f;
        float size = base_size * (0.58f + 0.42f * fabsf(2.0f * t - 1.0f));
        float cy = top + (row + 0.5f) * slot;
        int within_group = 0;
        for (int rank = 0; rank < (int)(sizeof(artifact_pipeline_order) / sizeof(artifact_pipeline_order[0])); rank++) {
            for (int i = 0; i < run->stage_count; i++) {
                AppArtifact *artifact = &run->stages[i];
                if (artifact_stage_group(artifact->number) != group || artifact_stage_rank(artifact->number) != rank || count >= capacity) continue;
                float cx = grid_left + (first_column + within_group + 0.5f) * cell;
                ArtifactDot *dot = &dots[count++];
                dot->artifact = artifact; dot->kind = 2; dot->index = i;
                dot->image = (Rectangle){cx - size * 0.5f, cy - size * 0.5f, size, size};
                float hit_height = fminf(slot, fmaxf(10.0f, size + 4.0f));
                dot->hit = (Rectangle){grid_left + (first_column + within_group) * cell, cy - hit_height * 0.5f, cell, hit_height};
                within_group++;
            }
        }
        row++;
    }
    if (has_source && count < capacity) {
        float t = rows > 1 ? (float)row / (float)(rows - 1) : 1.0f;
        float size = base_size * (0.58f + 0.42f * fabsf(2.0f * t - 1.0f));
        float cx = grid_left + (columns / 2 + 0.5f) * cell, cy = top + (row + 0.5f) * slot;
        ArtifactDot *dot = &dots[count++];
        dot->artifact = &run->generated; dot->kind = 0; dot->index = 0;
        dot->image = (Rectangle){cx - size * 0.5f, cy - size * 0.5f, size, size};
        float hit_size = fminf(slot, fmaxf(8.0f, size + 4.0f));
        dot->hit = (Rectangle){cx - cell * 0.5f, cy - hit_size * 0.5f, cell, hit_size};
    }
    return count;
}

/* Draw ArtifactDot* dot as a tiny image and selection ring; return void. */
static void draw_artifact_dot(const ArtifactDot *dot, bool selected, bool hovered) {
    Vector2 center = {dot->image.x + dot->image.width * 0.5f, dot->image.y + dot->image.height * 0.5f};
    float radius = fmaxf(0.45f, dot->image.width * 0.5f + 0.35f);
    DrawCircleV(center, radius, (Color){20, 21, 24, 225});
    Texture2D texture = dot->artifact->thumb.id ? dot->artifact->thumb : dot->artifact->texture;
    if (texture.id && dot->image.width >= 1.0f) {
        float scale = fminf(dot->image.width / texture.width, dot->image.height / texture.height);
        float iw = texture.width * scale, ih = texture.height * scale;
        Rectangle image = {center.x - iw * 0.5f, center.y - ih * 0.5f, iw, ih};
        DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width, (float)texture.height}, image, (Vector2){0, 0}, 0, WHITE);
    }
    DrawCircleLines((int)center.x, (int)center.y, radius, selected ? UI_ACCENT : hovered ? UI_TEXT : UI_PANEL_ALT);
}

/* Return an artifact label for ArtifactDot* dot in char out[256]; return void. */
static void artifact_dot_label(const ArtifactDot *dot, char out[256]) {
    if (dot->kind == 2) {
        const char *name = dot->artifact->name;
        if (!strncmp(name, "stage-", 6)) {
            const char *separator = strchr(name + 6, '-');
            if (separator) name = separator + 1;
        }
        snprintf(out, 256, "Stage %02d: %s", dot->artifact->number, name);
    } else if (dot->kind == 0) snprintf(out, 256, "Source · %s", dot->artifact->name);
    else copy_text(out, 256, dot->artifact->name);
}

/* Draw a hover label beside ArtifactDot* dot outside the grid; return void. */
static void draw_artifact_dot_popup(const ArtifactDot *dot, Rectangle bounds, float grid_left) {
    char label[256]; artifact_dot_label(dot, label);
    int popup_w = (int)fminf(280.0f, grid_left - 16.0f);
    if (popup_w < 60) return;
    float popup_h = 28.0f;
    float x = grid_left - popup_w - 8.0f;
    float y = fminf(bounds.y + bounds.height - popup_h, fmaxf(bounds.y, dot->image.y + dot->image.height * 0.5f - popup_h * 0.5f));
    Rectangle popup = {x, y, (float)popup_w, popup_h};
    DrawRectangleRec(popup, (Color){20, 21, 24, 245});
    DrawRectangleLinesEx(popup, 1, UI_ACCENT);
    BeginScissorMode((int)x + 7, (int)y + 4, popup_w - 14, 20);
    ui_draw_text(label, (int)x + 8, (int)y + 7, 13, UI_TEXT);
    EndScissorMode();
}

/* Draw run artifact overlays for AppRun* run; set hovered dot index and return count. */
static int draw_artifact_waterfall(AppRun *run, Rectangle bounds, ArtifactDot *dots, int capacity, int *hover_index, bool draw) {
    int count = layout_artifact_dots(run, bounds, dots, capacity);
    Vector2 mouse = GetMousePosition();
    *hover_index = -1;
    for (int i = 0; i < count; i++) if (CheckCollisionPointRec(mouse, dots[i].hit)) {
        *hover_index = i;
        if (!draw && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ArtifactDot *dot = &dots[i];
            if (dot->kind == 0) { run->selected_output = -1; run->stage_output_selected = false; }
            else if (dot->kind == 1) { run->selected_output = dot->index; run->stage_output_selected = false; }
            else { run->selected_stage = dot->index; run->stage_output_selected = true; }
        }
        break;
    }
    if (draw) {
        for (int i = 0; i < count; i++) {
            bool selected = dots[i].kind == 0 ? (!run->stage_output_selected && run->selected_output < 0) :
                dots[i].kind == 1 ? (!run->stage_output_selected && run->selected_output == dots[i].index) :
                (run->stage_output_selected && run->selected_stage == dots[i].index);
            draw_artifact_dot(&dots[i], selected, i == *hover_index);
        }
        if (*hover_index >= 0) {
            int outputs = 0;
            for (int i = 0; i < run->output_count; i++) if (run->outputs[i].thumb.id || run->outputs[i].texture.id) outputs++;
            int columns = outputs > 6 ? outputs : 6;
            float grid_width = fminf((float)columns * 16.0f, fmaxf(0.0f, bounds.width - 4.0f));
            float grid_left = bounds.x + bounds.width - grid_width - 2.0f;
            draw_artifact_dot_popup(&dots[*hover_index], bounds, grid_left);
        }
    }
    return count;
}

/* Draw App* app's run artifacts at int x/y/width/height; return void. */
static void draw_output_page(App *app, int x, int y, int width, int height) {
    AppRun *run = get_run(app);
    if (!run) { ui_draw_text("No generation selected.", x, y, 18, UI_MUTED); return; }
    ArtifactDot dots[APP_MAX_STAGES + APP_MAX_OUTPUTS + 1];
    float rail_w = fminf(300.0f, fmaxf(110.0f, width * 0.34f));
    Rectangle bounds = {(float)(x + width - rail_w), (float)(y + 4), rail_w, (float)(height - 58)};
    int hover_index = -1;
    int count = draw_artifact_waterfall(run, bounds, dots, APP_MAX_STAGES + APP_MAX_OUTPUTS + 1, &hover_index, false);
    if (run->stage_output_selected && (run->selected_stage < 0 || run->selected_stage >= run->stage_count)) run->stage_output_selected = false;
    AppArtifact *selected;
    if (run->stage_output_selected) selected = &run->stages[run->selected_stage];
    else {
        int choice = run->selected_output == -2 ? run_default_image_index(run) : run->selected_output;
        if (choice >= run->output_count || (choice >= 0 && !run->outputs[choice].texture.id && !run->outputs[choice].thumb.id)) choice = run_default_image_index(run);
        run->selected_output = choice;
        selected = choice < 0 ? &run->generated : &run->outputs[choice];
    }
    AppArtifact *image = hover_index >= 0 ? dots[hover_index].artifact : selected;
    draw_shared_image_view(app, image, NULL, run, x, y, width, height);
    if (count > 0) draw_artifact_waterfall(run, bounds, dots, APP_MAX_STAGES + APP_MAX_OUTPUTS + 1, &hover_index, true);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Align artifact dots on a compact grid | v 3.40.0 | 15e2d2 */
/* agent: gpt-6.1-sol | 2026-10-04 | Share the reference and output viewer | v 3.13.0 | e45697 */

/* agent: gpt-6.1-sol | 2026-10-04 | Show full run diagnostics on progress | v 3.11.0 | 8bbd56 */
/* agent: gpt-6.1-sol | 2026-10-04 | Use full width for run logs | v 3.36.0 | 2fb83a */
/* agent: gpt-6.1-sol | 2026-10-04 | Open full process and generation records | v 3.37.0 | d29792 */
/* Draw run status, stages, and log for App* app in int bounds; return void. */
static void draw_progress_page(App *app, int x, int y, int width, int height) {
    AppRun *run=get_run(app);
    if(!run){ui_draw_text("No runs yet.",x,y,20,UI_MUTED);return;}
    ui_draw_text(run->current_step,x,y+18,15,UI_MUTED);
    if(run->state==RUN_RUNNING&&button((Rectangle){x+width-170,y-8,170,40},"Cancel run",false)){copy_text(app->pending_action,sizeof(app->pending_action),"run.cancel");run->cancel_pending=true;}
    if(run->state==RUN_RUNNING)ui_draw_text(TextFormat("Running for %.0f seconds",GetTime()-run->started),x,y+48,14,UI_MUTED);
    if(run->state==RUN_FAILED&&run->error[0])draw_wrapped_text(run->error,(Rectangle){(float)x,(float)y+48,(float)width,38},14,UI_BAD);
    if(run->cancel_pending){
        ui_draw_text("Stop the active process?",x+160,y+58,15,UI_WARN);
        if(button((Rectangle){x+width-274,y+50,120,38},"Stop",false))AppDispatch(app,"{\"action\":\"app.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){x+width-138,y+50,120,38},"Continue",false))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }
    bool has_stages=run->stage_count>0;
    int stage_w=has_stages?(int)fminf(520,width*0.42f):0,stage_y=y+104,stage_h=height-140;
    if(stage_h<(has_stages?160:120))stage_h=has_stages?160:120;
    if(has_stages){
        ui_draw_text("Processor stages",x,stage_y,18,UI_TEXT);
        if(run->selected_stage<0||run->selected_stage>=run->stage_count)run->selected_stage=0;
        if(button((Rectangle){x,stage_y+34,50,36},"<",false)&&run->selected_stage>0)run->selected_stage--;
        if(button((Rectangle){x+60,stage_y+34,50,36},">",false)&&run->selected_stage+1<run->stage_count)run->selected_stage++;
        AppArtifact *stage=&run->stages[run->selected_stage];
        ui_draw_text(TextFormat("%02d | %s",stage->number,stage->name),x+124,stage_y+42,16,UI_TEXT);
        draw_image_view(app,stage->texture,x,stage_y+82,stage_w,stage_h-132);
        draw_wrapped_text(stage_description(stage->number),(Rectangle){(float)x,(float)(stage_y+stage_h-42),(float)stage_w,32},14,UI_MUTED);
        if(button((Rectangle){x,stage_y+stage_h,150,36},"Open stage",false))open_path(stage->path);
        if(button((Rectangle){x+162,stage_y+stage_h,174,36},"Adjust related setting",false)&&adjust_stage_option(&app->profiles[app->active_profile],stage->number))save_profiles(app);
    }
    int log_x=has_stages?x+stage_w+44:x,log_w=has_stages?width-stage_w-44:width,log_y=stage_y,log_h=stage_h+36;
    ui_draw_text("Run log",log_x,log_y,18,UI_TEXT);
    float diag_button_w = fminf(138.0f, fmaxf(1.0f, (log_w - 12) * 0.5f));
    Rectangle open_log = {(float)(log_x + log_w - diag_button_w * 2 - 12), (float)(log_y - 6), diag_button_w, 34};
    Rectangle open_record = {(float)(log_x + log_w - diag_button_w), (float)(log_y - 6), diag_button_w, 34};
    if (button(open_log, "Process log", false)) { char path[APP_PATH_CAP]; path_join(path, sizeof(path), run->dir, "process.log"); open_path(path); }
    if (button(open_record, "Gen record", false)) { char path[APP_PATH_CAP]; path_join(path, sizeof(path), run->dir, "generation.record.txt"); open_path(path); }
    Rectangle area={(float)log_x,(float)(log_y+32),(float)log_w,(float)(log_h-32)};
    DrawRectangleRec(area,UI_PANEL);BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    if(CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel>0)run->log_scroll+=3;else if(wheel<0&&run->log_scroll>0)run->log_scroll-=3;if(run->log_scroll<0)run->log_scroll=0;}
    int line_h=20,max_lines=(int)(area.height/line_h),start=run->log_count-max_lines-run->log_scroll;if(start<0)start=0;
    for(int i=start,row=0;i<run->log_count&&row<max_lines;i++,row++){AppLogLine *line=&run->logs[i];ui_draw_text(TextFormat("%s [%s] %s",line->time,line->process,line->text),(int)area.x+10,(int)area.y+6+row*line_h,14,UI_TEXT);}
    EndScissorMode();
}
/* agent: gpt-6.1-sol | 2026-10-04 | Open full process and generation records | v 3.37.0 | d29792 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show full run diagnostics on progress | v 3.11.0 | 8bbd56 */
/* agent: gpt-6.1-sol | 2026-10-04 | Use full width for run logs | v 3.36.0 | 2fb83a */

/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* Draw profile fields and processor settings for App* app; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
static void draw_settings(App *app) {
    int w=GetScreenWidth(),h=GetScreenHeight(),content_h=1435;
    Rectangle viewport={24,106,(float)w-48,(float)h-144};
    if(CheckCollisionPointRec(GetMousePosition(),viewport)){float wheel=GetMouseWheelMove();if(wheel!=0)app->settings_scroll-=(int)(wheel*56);}
    int max_scroll=content_h-(int)viewport.height;if(max_scroll<0)max_scroll=0;if(app->settings_scroll<0)app->settings_scroll=0;if(app->settings_scroll>max_scroll)app->settings_scroll=max_scroll;
    BeginScissorMode((int)viewport.x,(int)viewport.y,(int)viewport.width,(int)viewport.height);
    int x=48,off=app->settings_scroll; float gap=24,cw=(w-96-gap*2)/3.0f; AppProfile *p=&app->profiles[app->active_profile];
    int y=120-off; ui_draw_text("Profile",x,y,20,UI_TEXT);
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
    ui_draw_text("Processor artifacts",x+cw+gap,fields_y+216,15,UI_MUTED);
    ui_draw_text("All stages saved",x+cw+gap,fields_y+246,16,UI_TEXT);
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
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */

/* agent: gpt-6.1-sol | 2026-10-04 | Use shared reference name editing | v 3.3.0 | 930e92 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route viewer by named view | v 3.12.0 | aed5ea */
/* agent: gpt-6.1-sol | 2026-10-04 | Confirm unsaved prompt close choices | v 3.8.0 | 32e9a8 */
/* Draw the shared image viewer or prompt editor for App* app; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
static void draw_modal_view(App *app) {
    if (!app->show_full_image) return;
    if (app->view == VIEW_LIBRARY && app->selected_ref >= 0 && app->selected_ref < app->ref_count && app->refs[app->selected_ref].is_text) {
        AppReference *ref = &app->refs[app->selected_ref];
        bool pending_before=app->prompt_close_pending;
        bool editing_name = !strcmp(app->edit_field, "reference.name");
        if(!pending_before)draw_reference_name_field(app, ref, (Rectangle){48,66,420,36});
        if(!strcmp(app->edit_field,"reference.name"))ui_draw_text("Enter: rename · Esc: cancel",480,77,14,UI_MUTED);
        draw_prompt_editor(app, (Rectangle){32, 176, GetScreenWidth()-64, GetScreenHeight()-226});
        int w=GetScreenWidth();
        bool save_requested=button((Rectangle){w-250,66,100,38},"Save",false);
        bool close_button=button((Rectangle){w-138,66,106,38},"Close",false);
        if(!app->prompt_close_pending && save_requested)save_open_prompt(app);
        if(!app->prompt_close_pending) {
            bool close_requested=close_button||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
            if(IsKeyPressed(KEY_ESCAPE)){if(editing_name)app->edit_field[0]=0;else close_requested=true;}
            if(close_requested){
                if(app->prompt_dirty)app->prompt_close_pending=true;
                else {app->show_full_image=false;app->edit_field[0]=0;}
            }
        }
        if(app->prompt_close_pending){
            int choice=draw_prompt_close_dialog();
            if(pending_before&&IsKeyPressed(KEY_ESCAPE))choice=PROMPT_CLOSE_CANCEL;
            if(choice==PROMPT_CLOSE_SAVE&&save_open_prompt(app)){app->prompt_close_pending=false;app->show_full_image=false;app->edit_field[0]=0;}
            else if(choice==PROMPT_CLOSE_DISCARD){discard_open_prompt(app);app->show_full_image=false;app->edit_field[0]=0;}
            else if(choice==PROMPT_CLOSE_CANCEL)app->prompt_close_pending=false;
        }
        return;
    }
    AppArtifact image = {0};
    AppReference *ref = NULL;
    if (app->view == VIEW_LIBRARY && app->selected_ref >= 0 && app->selected_ref < app->ref_count) {
        ref = &app->refs[app->selected_ref];
        copy_text(image.name, sizeof(image.name), ref->name); copy_text(image.path, sizeof(image.path), ref->path);
        image.texture = ref->full; image.width = ref->width; image.height = ref->height;
    }
    else {
        AppRun *run = get_run(app);
        if (run) {
            int choice = run->selected_output == -2 ? run_default_image_index(run) : run->selected_output;
            if (choice >= run->output_count || (choice >= 0 && !run->outputs[choice].texture.id && !run->outputs[choice].thumb.id)) choice = run_default_image_index(run);
            if (choice >= 0 && choice < run->output_count) image = run->outputs[choice];
            else image = run->generated;
            draw_shared_image_view(app, &image, NULL, run, 48, 120, GetScreenWidth()-96, GetScreenHeight()-164);
        }
    }
    if (ref && !ref->is_text) {
        draw_shared_image_view(app, &image, ref, NULL, 48, 120, GetScreenWidth()-96, GetScreenHeight()-164);
        if (IsKeyPressed(KEY_ESCAPE)) { app->show_full_image = false; app->edit_field[0] = 0; }
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route viewer by named view | v 3.12.0 | aed5ea */
/* agent: gpt-6.1-sol | 2026-10-04 | Confirm unsaved prompt close choices | v 3.8.0 | 32e9a8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Use shared reference name editing | v 3.3.0 | 930e92 */

/* agent: gpt-6.1-sol | 2026-10-04 | Remove details and consolidate run views | v 3.11.0 | b3d2d3 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show trash confirmation dialog | v 3.3.0 | d68489 */
/* agent: gpt-6.1-sol | 2026-10-04 | Draw named generation views | v 3.12.0 | 862302 */
/* Draw App* app with section labels, viewer controls and navigation; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Show live images and run logs | v 3.35.0 | 47e875 */
/* agent: gpt-6.1-sol | 2026-10-04 | Autosave settings fields on edit | v 3.34.0 | 6704f1 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show current view on toggle icon | v 3.28.0 | ffe4cb */
/* agent: gpt-6.1-sol | 2026-10-04 | Close viewer before switching views | v 3.26.0 | 1a00ab */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Fix toggle state and header spacing | v 3.23.0 | fba3ad */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show run header state and artifact | v 3.42.0 | d9ca89 */
/* Resolve the selected output or stage for AppRun* run; return artifact or NULL. */
static AppArtifact *generation_header_artifact(AppRun *run) {
    if (!run) return NULL;
    if (run->stage_output_selected && run->selected_stage >= 0 && run->selected_stage < run->stage_count)
        return &run->stages[run->selected_stage];
    int choice = run->selected_output == -2 ? run_default_image_index(run) : run->selected_output;
    if (choice >= run->output_count || (choice >= 0 && !run->outputs[choice].texture.id && !run->outputs[choice].thumb.id))
        choice = run_default_image_index(run);
    return choice < 0 ? &run->generated : &run->outputs[choice];
}
/* agent: gpt-6.1-sol | 2026-10-04 | Show run header state and artifact | v 3.42.0 | d9ca89 */

void AppDraw(App *app) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    ClearBackground(UI_BG);
    bool dirty_prompt = app->show_full_image && app->view == VIEW_LIBRARY && app->selected_ref >= 0 && app->selected_ref < app->ref_count && app->refs[app->selected_ref].is_text && app->prompt_dirty;
    ui_draw_text("AIGEN", 40, 22, 26, UI_ACCENT);
    Rectangle view_button = {130,18,36,36};
    bool show_grid_icon = app->view != VIEW_LIBRARY;
    bool view_clicked = button(view_button, "", !app->settings_view) && !dirty_prompt;
    Color view_tone = !app->settings_view ? UI_ACCENT : UI_TEXT;
    if (show_grid_icon) {
        for (int row = 0; row < 2; row++) for (int col = 0; col < 2; col++)
            DrawRectangleLinesEx((Rectangle){view_button.x+9+col*10,view_button.y+9+row*10,7,7},1,view_tone);
    } else {
        DrawLine((int)view_button.x+8,(int)view_button.y+10,(int)view_button.x+17,(int)view_button.y+12,view_tone);
        DrawLine((int)view_button.x+8,(int)view_button.y+10,(int)view_button.x+8,(int)view_button.y+26,view_tone);
        DrawLine((int)view_button.x+8,(int)view_button.y+26,(int)view_button.x+17,(int)view_button.y+28,view_tone);
        DrawLine((int)view_button.x+17,(int)view_button.y+12,(int)view_button.x+17,(int)view_button.y+28,view_tone);
        DrawLine((int)view_button.x+17,(int)view_button.y+12,(int)view_button.x+27,(int)view_button.y+10,view_tone);
        DrawLine((int)view_button.x+27,(int)view_button.y+10,(int)view_button.x+27,(int)view_button.y+26,view_tone);
        DrawLine((int)view_button.x+27,(int)view_button.y+26,(int)view_button.x+17,(int)view_button.y+28,view_tone);
    }
    Rectangle settings_button={172,18,36,36};
    bool settings_clicked=button(settings_button,"",app->settings_view) && !dirty_prompt;
    for(int i=0;i<3;i++){
        int sy=(int)settings_button.y+10+i*8,left=(int)settings_button.x+8,right=(int)settings_button.x+28;
        DrawLine(left,sy,right,sy,app->settings_view?UI_ACCENT:UI_TEXT);
        int knob_x=left+(i==1?13:i==2?7:5);
        DrawCircle(knob_x,sy,3,UI_PANEL);DrawCircleLines(knob_x,sy,3,app->settings_view?UI_ACCENT:UI_TEXT);
    }
    if (view_clicked) {
        if (app->settings_view) { app->settings_view = false; app->edit_field[0] = 0; app->field_scroll = 0; }
        else if (app->show_full_image) {
            app->show_full_image = false; app->edit_field[0] = 0; app->field_scroll = 0;
        }
        else {
            app->view = app->view == VIEW_HOME ? VIEW_LIBRARY : VIEW_HOME;
            app->show_add_menu = false;
        }
    }
    if(settings_clicked){app->settings_view=!app->settings_view;app->show_add_menu=false;app->show_full_image=false;if(!app->settings_view){app->edit_field[0]=0;app->field_scroll=0;}}
    ui_draw_text(app->workspace, 222, 29, 15, UI_MUTED);
    if (!app->show_full_image) {
        if (app->settings_view) ui_draw_text("Settings", 40, 76, 17, UI_ACCENT);
        else if (app->view == VIEW_HOME) ui_draw_text("Generations", 40, 76, 17, UI_ACCENT);
        else if (app->view == VIEW_LIBRARY) ui_draw_text("Files", 40, 76, 17, UI_ACCENT);
        else {
            AppRun *run = get_run(app);
            if (run) {
                bool image_view = run_has_viewable_image(run) && !app->show_run_logs;
                const char *status = run->state == RUN_RUNNING ? "Working" : run->state == RUN_FAILED ? "Failed" : run->state == RUN_CANCELED ? "Canceled" : "Complete";
                bool show_status = !image_view || run->state != RUN_SUCCEEDED;
                AppArtifact *artifact = image_view ? generation_header_artifact(run) : NULL;
                const char *image_name = artifact && artifact->name[0] ? artifact->name : "Image";
                const char *header = image_view ? (show_status ? TextFormat("%s  ·  %s  ·  %s", run->id, image_name, status) : TextFormat("%s  ·  %s", run->id, image_name)) : TextFormat("%s  ·  %s", run->id, status);
                Color header_tone = run->state == RUN_FAILED ? UI_BAD : run->state == RUN_CANCELED ? UI_WARN : show_status && run->state == RUN_SUCCEEDED ? UI_GOOD : UI_ACCENT;
                ui_draw_text(header, 40, 76, 17, header_tone);
                if (run->state == RUN_RUNNING) {
                    int bar_x = 40 + MeasureText(header, 17) + 18;
                    int bar_right = w - 210;
                    if (bar_x < bar_right - 24) {
                        Rectangle progress = {(float)bar_x, 80, (float)(bar_right - bar_x), 10};
                        DrawRectangleRec(progress, UI_PANEL);
                        float phase = fmodf((float)GetTime() * 0.45f, 2.0f); if (phase > 1) phase = 2 - phase;
                        float segment = fminf(100, progress.width);
                        DrawRectangleRec((Rectangle){progress.x + (progress.width - segment) * phase, progress.y, segment, progress.height}, UI_ACCENT);
                        DrawRectangleLinesEx(progress, 1, UI_PANEL_ALT);
                    }
                }
            } else ui_draw_text("No generation selected", 40, 76, 17, UI_MUTED);
            if (run && run_has_viewable_image(run)) {
                Rectangle logs_button = {(float)(w - 190), 66, 142, 40};
                if (button(logs_button, app->show_run_logs ? "Show image" : "Logs", false)) app->show_run_logs = !app->show_run_logs;
            }
        }
    }
    AppRun *active = app->active_run >= 0 ? &app->runs[app->active_run] : NULL;
    ui_draw_text(active ? TextFormat("%s | %s", active->id, active->state == RUN_RUNNING ? "WORKING" : active->state == RUN_FAILED ? "FAILED" : active->state == RUN_SUCCEEDED ? "DONE" : "CANCELED") : "READY", w - 330, 28, 15, active && active->state == RUN_FAILED ? UI_BAD : UI_GOOD);
    DrawLine(28, 112, w - 28, 112, UI_PANEL_ALT);
    if (app->show_full_image) draw_modal_view(app);
    else if(app->settings_view) draw_settings(app);
    else if(app->view==VIEW_LIBRARY) draw_references(app);
    else if(app->view==VIEW_GENERATION) {
        AppRun *run = get_run(app);
        if (run && run_has_viewable_image(run) && !app->show_run_logs) draw_output_page(app,48,120,w-96,h-170);
        else draw_progress_page(app,48,120,w-96,h-170);
    } else draw_generation_grid(app);
    DrawRectangle(0, h - 28, w, 28, UI_PANEL);
    ui_draw_text(app->message,40,h-22,13,UI_MUTED);
    if (!app->show_full_image && app->settings_view && !strcmp(app->pending_action, "profile.delete")) {
        Rectangle panel = {(w - 440) * 0.5f, (h - 180) * 0.5f, 440, 180};
        DrawRectangleRec(panel, UI_PANEL); DrawRectangleLinesEx(panel, 2, UI_WARN);
        ui_draw_text("Remove this profile?", (int)panel.x + 20, (int)panel.y + 24, 21, UI_TEXT);
        ui_draw_text("You cannot remove the only profile.", (int)panel.x + 20, (int)panel.y + 62, 16, UI_MUTED);
        if(button((Rectangle){panel.x+20,panel.y+112,132,40},"Remove",false))AppDispatch(app,"{\"action\":\"app.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){panel.x+168,panel.y+112,132,40},"Cancel",false)||IsKeyPressed(KEY_ESCAPE))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }
    if (!strcmp(app->pending_action, "reference.trash") || !strcmp(app->pending_action, "run.trash")) {
        Rectangle panel = {(w - 440) * 0.5f, (h - 190) * 0.5f, 440, 190};
        DrawRectangleRec(panel, UI_PANEL); DrawRectangleLinesEx(panel, 2, UI_WARN);
        ui_draw_text(!strcmp(app->pending_action, "run.trash") ? "Move this generation to workspace/trash?" : "Move this item to workspace/trash?", (int)panel.x + 20, (int)panel.y + 28, 19, UI_TEXT);
        if(button((Rectangle){panel.x+20,panel.y+118,160,40},"Move to trash",false))AppDispatch(app,"{\"action\":\"app.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){panel.x+194,panel.y+118,120,40},"Cancel",false)||IsKeyPressed(KEY_ESCAPE))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Show live images and run logs | v 3.35.0 | 47e875 */
/* agent: gpt-6.1-sol | 2026-10-04 | Autosave settings fields on edit | v 3.34.0 | 6704f1 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show current view on toggle icon | v 3.28.0 | ffe4cb */
/* agent: gpt-6.1-sol | 2026-10-04 | Close viewer before switching views | v 3.26.0 | 1a00ab */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Fix toggle state and header spacing | v 3.23.0 | fba3ad */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Use home and library navigation icons | v 3.17.0 | e080c6 */
/* agent: gpt-6.1-sol | 2026-10-04 | Draw named generation views | v 3.12.0 | 862302 */
/* agent: gpt-6.1-sol | 2026-10-04 | Remove details and consolidate run views | v 3.11.0 | b3d2d3 */
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

/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Expose trash instead of delete | v 3.3.0 | 84c62f */
/* Return the project tool list with the shared confirmation contract as const char*. */
const char *AppMcpToolsJson(void) {
    return "{\"tools\":["
      "{\"name\":\"app_status\",\"description\":\"Read workspace, profile, active run, log tail, stages, and outputs\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"ui_view\",\"description\":\"Show home, library, or generation view; generation accepts an optional run index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"},\"run\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
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
      "{\"name\":\"reference_import\",\"description\":\"Copy an image or prompt directly into the workspace refs folder\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"reference_manage\",\"description\":\"Exclude a reference or request moving it to workspace/trash\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"exclude\",\"trash\"]},\"path\":{\"type\":\"string\"}},\"required\":[\"action\",\"path\"]}},"
      "{\"name\":\"reference_select\",\"description\":\"Toggle whether a listed reference is included\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"reference_view\",\"description\":\"Open a listed reference in the full-size viewer\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"references_size\",\"description\":\"Set reference thumbnail size from 56 through 216\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"generation_preview\",\"description\":\"Prepare immutable prompt, references, model, options, output path, and all-stage processing; does not start generation\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"generation_confirm\",\"description\":\"Confirm the current preview and start generation\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"generation_cancel\",\"description\":\"Request child interruption; confirm separately\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"app_confirm\",\"description\":\"Confirm a prepared generation, cancellation, or deletion\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"app_cancel\",\"description\":\"Cancel a pending preview or destructive action\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_select\",\"description\":\"Select retained run by list index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"run_inspect\",\"description\":\"Read run status, timestamped log tail, emitted stages, and final outputs\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"output_select\",\"description\":\"Select the original generated image or a pixel output by list index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"stage_select\",\"description\":\"Select an actually emitted processor stage by stage number\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"stage_compare\",\"description\":\"Compare selected stage with source, previous available stage, or final output\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\",\"enum\":[\"source\",\"previous\",\"final\"]}},\"required\":[\"value\"]}},"
      "{\"name\":\"viewer_zoom\",\"description\":\"Adjust the selected image view scale\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"run_output_open\",\"description\":\"Open a selected run artifact with the desktop file handler\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"run_rerun\",\"description\":\"Prepare a processor-only rerun using current profile values and all emitted stages\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_logs_clear\",\"description\":\"Clear the visible log view and retain process.log\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_note\",\"description\":\"Save a note beside the selected run\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}}]}";
}
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Expose trash instead of delete | v 3.3.0 | 84c62f */

/* Handle MCP requests for const char* workspace and int port; return int status. */
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
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
            char name[128] = "", action[64] = "", value[APP_TEXT_CAP] = "", path[APP_PATH_CAP] = "", field[64] = "", text[APP_TEXT_CAP] = "", run_value[32] = "";
            json_string(line, "name", name, sizeof(name));
            if (!strcmp(name,"app_status")) copy_text(action,sizeof(action),"state");
            else if (!strcmp(name,"prompt_get")) copy_text(action,sizeof(action),"prompt.get");
            else if (!strcmp(name,"prompt_open")) { copy_text(action,sizeof(action),"prompt.open"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"prompt_save")) { copy_text(action,sizeof(action),"prompt.save"); json_string(line,"text",text,sizeof(text)); }
            else if (!strcmp(name,"generation_preview")) copy_text(action,sizeof(action),"generation.preview");
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
            else if (!strcmp(name,"ui_view")) { copy_text(action,sizeof(action),"view"); json_string(line,"value",value,sizeof(value)); json_string(line,"run",run_value,sizeof(run_value)); }
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
            char escrun[64]; json_escape(run_value, escrun, sizeof(escrun));
            if (!strcmp(action, "view")) snprintf(request, sizeof(request), "{\"action\":\"view\",\"value\":\"%s\",\"run\":\"%s\"}", escvalue, escrun);
            else snprintf(request,sizeof(request),"{\"action\":\"%s\",\"value\":\"%s\",\"path\":\"%s\",\"field\":\"%s\",\"text\":\"%s\"}",escaction,escvalue,escpath,escfield,esc);
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
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */

/* 2026-10-03: core functions are documented above; APP-01 defines the shared run flow. */
/* agent: gpt-6.1-sol | 2026-10-04 | Support image and text renaming | v 3.3.0 | 204819 */
