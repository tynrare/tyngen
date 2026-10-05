/*
 * File version: 3.98.0
 * Last updated: 2026-10-05
 * Purpose: Workbench persistence, processes, shared actions, and interface.
 * Flow ID APP-03
 * 1. User -> click the single Grid/Library header toggle -> close an open image viewer while keeping its view; otherwise alternate between grid and Library; from generation details it returns to grid; Settings overlays without changing this view.
 * 2. User -> click Library or grid plus -> prepare resources and prompts; Generate -> preview and confirm immutable input snapshot with metadata.txt.
 * 3. Agent/MCP -> select home, library, or generation view -> update shared view state; run index selects generation details.
 * 4. Run -> emit process lines and all processor stages -> generation cards refresh live status; details show progress, stage previews when present, and full-width logs when no stages exist; Controls remains available for stage-only runs.
 * Flow ID APP-04
 * 1. User -> open a Library or generation image -> Library shows its filename; generation artifact labels stay hidden until hovering the overlay rail; processor notes render at the original top-left over the image; dimensions/zoom tips, Open image, and transform reset stay in the frame footer; right-click closes it.
 * 2. Run -> save the original generated image then pixel outputs -> cards prefer the main pixel output, with the original image as fallback; viewer overlays a right-aligned dot rail with final variants at top, stages grouped by logical phase and sorted by descending numeric stage number from right to left, earlier higher-numbered stages lower and farther right, and source at bottom; stages 100-91 share the source/layout phase row; hover previews and labels the artifact, click selects it.
 * 3. Pixel process -> write output files -> UI rescans every 0.75s and replaces changed textures without leaving the generation view.
 * 4. User -> click generation trash -> confirm -> move the run directory to workspace/trash and release its textures.
 * 5. Run history or child process -> produce any decodable run image, even with nonzero status -> mark run complete and retain the image.
 * 6. Confirm run -> write immutable metadata.txt and used-resource paths -> start generation or pixel-only rerun.
 * 7. User -> zoom over image with wheel or keys -> keep the cursor's image point fixed; zoom outside image with keys -> zoom around center.
 * 8. Run -> expose a usable image while running -> generation details show that image automatically; Controls and Show image switch between image and diagnostics; the header shows run name/status/progress and the selected artifact name, omitting success status in image view.
 * 9. User -> hover a source/stage/output dot -> preview it with the current zoom and pan and show its label beside the dot grid; leave -> restore selection; click -> select it. The fine grid sits at the viewer's right edge and tapers toward the middle; final outputs sit at top, stages group by logical phase and sort by descending numeric stage number from right to left; earlier higher-numbered stages sit lower and farther right; stages 100-91 share the source/layout phase row; source sits at bottom. Sprite Inspect hides this global waterfall and provides its own sprite-bound version rail (APP-04.11).
 * 10. User -> open Controls -> inspect record and captured resources in-app; stopped run -> request stage cleanup -> confirm -> remove stage images and rescan; pixel rerun -> resolve its input from metadata and validate it before preview/confirmation; confirmation -> copy input to a new run or rerun in the existing directory; missing input -> refuse; full rerun -> reuse captured prompt/images; each rerun needs confirmation.
 * 11. User -> click footer Inspect toggle -> keep the current artifact and show sprite bounds plus a version rail; hover a version -> split the sprite bounds between the pinned and hovered per-sprite crops and label both in the info card; click pins it. Sprite settings stay out of the footer. Source maps to stage 100; final maps to processor output records.
 * Flow ID APP-05
 * 1. Import/create resource -> write supported image/text files directly to resources root -> Library displays a flat collection.
 * 2. App startup -> migrate refs files and legacy groups into resources with collision-safe names -> preserve exclusion state and remove empty groups only.
 * Flow ID APP-06
 * 1. User -> click draw in an image footer -> snapshot RGBA pixels and retain viewer transform; press samples one pixel and drag paints it with a clipped 1px stroke.
 * 2. User -> finish a stroke -> append pixel deltas as one undo step; undo/redo replay deltas; a new stroke clears redo history.
 * 3. User -> Save or Save as copy -> worker writes atomically while input is locked; AppUpdate refreshes artifacts and closes draw mode only after success; failure keeps edits open.
 * 4. User -> Save as copy -> name source-basename-copy.png with numeric collision suffix; generation copies live in extra/draw-copies/ and occupy their own waterfall row above final outputs.
 * 5. User -> draw Cancel or Escape -> leave the viewer open after discard; generation Close -> return to home after save/discard confirmation; other navigation offers the same close choices.
 * Invariant: draw copies persist in resources root or extra/draw-copies and are reloaded by startup scans.
 * Flow ID APP-07
 * 1. Startup -> migrate profiles.ini to individual .profile resources and restore active resource -> ensure one profile is enabled.
 * 2. User -> edit profile resource in its Settings view -> keep changed fields in memory and highlight them yellow; Save persists and refreshes baseline; Close or Text saves before leaving.
 * 3. User -> open profile resource as text -> parse and validate complete text before replacing settings; invalid text leaves prior settings intact.
 * 4. User -> add image/text/profile from Add resource -> write or create it in resources and show it with other resource cards.
 * Invariant: generation and pixel defaults belong to exactly one enabled profile resource; run overrides are run-local; APP-07 selection state is workspace-local; profile Settings edits are not persisted before Save, Close, or switching to Text.
 * Flow ID APP-08
 * 1. App -> queue reference and run image paths -> one worker decodes/resizes CPU images without graphics calls.
 * 2. Worker -> publish one bounded result -> AppUpdate uploads textures on the graphics thread and advances the queue.
 * 3. User -> open a run -> worker reads its process.log -> AppUpdate parses bounded history into the selected run.
 * 4. User -> open a card/view while work remains -> animated loading marker stays responsive until its texture arrives; stage previews request full-resolution images; stage-only runs keep Controls reachable.
 * 5. Cache -> measure reloadable GPU textures and evict oldest cold full images, then thumbnails, above 512 MiB; selected/card views queue evicted files again.
 * 6. App shutdown -> stop worker and free pending pixels -> unload textures on the main thread.
 * Invariant: worker owns decoded Image buffers until AppUpdate consumes them; raylib texture operations and cache eviction stay on the main thread.
 * Flow ID APP-01
 * 1. User/agent -> choose home, Library, or generation details (APP-03) -> show one primary content view; header Settings button -> show application settings only; prompt and profile resources live with images in Resources.
 * 2. User -> left-click resource -> open viewer/editor; right-click -> close viewer; exclude icon -> omit image or prompt from payload; profile enable control -> select exactly one profile; trash icon -> confirm move item to workspace/trash; profile editor Settings view -> edit profile configuration.
 * 3. Agent/MCP -> prepare then explicitly confirm -> immutable run starts generation then pixel processing.
 * 4. Child process -> report output and diagnostics -> generation cards and selected generation details reflect observed files.
 * 5. ENet/MCP -> dispatch the same action -> return current state or action result.
 * 6. User -> import TTF/OTF from any folder or select a stored font -> load 32px atlas; empty selection -> raylib default; persist per workspace.
 * 7. User -> Add resource -> import image/text, create text, or add profile; open text/profile cards -> edit contents with UTF-8 caret, selection, clipboard and scrolling; profile text is validated before applying; profile editor Settings view edits the same profile resource and commits through Save, Close, or Text; application Settings contains only application preferences; enable -> disable all other profiles; trash -> confirm move and close editor if needed.
 * 8. User -> edit profile Settings fields -> keep changes in memory and outline modified values in yellow; Save persists and refreshes the baseline; Close or Text saves before leaving; application settings remain immediate.
 * Invariant: resource trash moves require confirmation; trash is outside the scanned resources tree.
 * Invariant: the raylib default is active unless a stored custom font is selected; generation uses all enabled text refs, independently of editor selection.
 * Flow ID APP-02
 * 1. User -> click image, text, or profile resource filename -> edit basename while viewer stays open.
 * 2. User -> edit basename with caret/selection controls and press Enter -> save open text edits when needed, preserve file extension, reject empty names/collisions, rename resource and update workspace state.
 * 3. User -> press Escape -> discard filename edits; outside/right-click on add tile -> close its menu.
 * Invariant: an empty prompt collection stays empty; resource renames preserve extension and never overwrite files.
 * Flow ID APP-09 Pixelprocess conversion, inspection, edits, reruns, and export
 * 1. User -> convert a library image or confirm generation -> snapshot it as run/source.png and launch the runner with effective settings.
 * 2. Runner -> publish ordered main/debug/metadata artifacts and sprite records -> Controls shows process, record, resources, and metadata; the image inspector shows sprite versions and settings.
 * 3. User -> select a stage and toggle its overlay -> viewer resolves the matching emitted debug artifact; comparison uses available source/final artifacts.
 * 4. User -> select a sprite -> app records its ID; sprite-scoped pitch/phase overrides persist independently, while blob assignment writes ownership overrides.
 * 5. User -> edit stage settings or ownership -> mark draft dirty; Apply prepares a pixel-only run and confirmation copies the immutable source before processing.
 * 6. Runner -> validate cached source, settings, and executable inputs -> reuse matching stages or execute them; a failed revision leaves earlier run outputs intact.
 * 7. User -> export a run image -> verify run ownership and atomically copy it to the chosen PNG destination.
 * Invariants: generation model/aspect/options remain profile-owned; obsolete pixel profile keys are discarded; processing drafts and ownership corrections are run-local.
 */
/* agent: gpt-6 | 2026-10-05 | Fit previews to saved sprite bounds | v 3.97.0 | c80076 */
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* agent: gpt-6.1-sol | 2026-10-04 | Align profile toolbar and support filename editing | v 3.60.0 | a06e32 */

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
#include <pthread.h>
#include <stdatomic.h>
#include <float.h>

/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.50.0 | bca2b5 */
typedef struct { uint32_t index; Color before, after; } DrawChange;
typedef struct { size_t start, count; } DrawStroke;
typedef struct {
    char path[APP_PATH_CAP];
    int width, height;
    Color *pixels, *original;
    uint32_t *pixel_stamp;
    Texture2D texture;
    DrawChange *changes;
    size_t change_count, change_capacity;
    DrawStroke *strokes;
    size_t stroke_count, stroke_capacity, cursor;
    size_t active_start;
    Color stroke_color;
    int last_x, last_y;
    bool active;
    pthread_t save_thread;
    bool save_started, save_success;
    atomic_bool save_done;
    int save_kind;
    char save_target[APP_PATH_CAP];
} DrawState;
/* agent: gpt-6.1-sol | 2026-10-04 | Track drawing state and copied artifacts | v 3.50.0 | bca2b5 */
static void draw_session_free(DrawState *state);
static void draw_save_update(App *app);
static void draw_apply_close_action(App *app, int action);

#define ENET_CHANNEL 0
#define MCP_FRAME_CAP 32768
#define IMAGE_JOB_CAP 4096
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
/* agent: gpt-6.1-sol | 2026-10-04 | Declare workspace path helper early | v 3.55.0 | 273a50 */
static void path_join(char *dst, size_t cap, const char *a, const char *b);
/* agent: gpt-6.1-sol | 2026-10-04 | Declare workspace path helper early | v 3.55.0 | 273a50 */
/* agent: gpt-6.1-sol | 2026-10-04 | Declare profile saver before use | v 3.50.0 | 6a6121 */
/* agent: gpt-6.1-sol | 2026-10-04 | Persist generation profiles as resources | v 3.55.0 | 9d1e3c */
static bool save_profiles(App *app);

/* agent: gpt-6.1-sol | 2026-10-04 | Validate numeric profile settings | v 3.55.0 | c34683 */
/* Parse an integer profile setting within inclusive bounds; return bool. */
static bool parse_profile_number(const char *text, int minimum, int maximum, int *value) {
    char *end=NULL; errno=0; long parsed=strtol(text,&end,10);
    if(errno==ERANGE||end==text||*end||parsed<minimum||parsed>maximum)return false;
    *value=(int)parsed; return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Validate numeric profile settings | v 3.55.0 | c34683 */
/* agent: gpt-6.1-sol | 2026-10-04 | Declare profile saver before use | v 3.50.0 | 6a6121 */
static void unload_run_artifacts(AppRun *run);
static void copy_text(char *dst, size_t cap, const char *src);

/* agent: gpt-6.1-sol | 2026-10-04 | Queue image decode and upload work | v 3.55.0 | eb40fa */
typedef struct { char path[APP_PATH_CAP]; int thumb_size; bool pixelated, full, logs; } ImageJob;
typedef struct { char path[APP_PATH_CAP]; Image image, thumb; int width, height; bool pixelated, logs; char *log_data; } ImageResult;
typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t ready;
    ImageJob jobs[IMAGE_JOB_CAP];
    ImageResult result;
    int head, count;
    char active_path[APP_PATH_CAP];
    bool result_ready, working, active_full, active_logs, stopping;
} ImageLoader;

/* Decode images from void* ImageLoader without graphics calls; return void*. */
static void *image_loader_thread(void *opaque) {
    ImageLoader *loader = opaque;
    for (;;) {
        pthread_mutex_lock(&loader->mutex);
        while (!loader->stopping && (!loader->count || loader->result_ready))
            pthread_cond_wait(&loader->ready, &loader->mutex);
        if (loader->stopping) { pthread_mutex_unlock(&loader->mutex); break; }
        ImageJob job = loader->jobs[loader->head];
        loader->head = (loader->head + 1) % IMAGE_JOB_CAP; loader->count--;
        loader->working = true; loader->active_full = job.full; loader->active_logs = job.logs; copy_text(loader->active_path, sizeof(loader->active_path), job.path);
        pthread_mutex_unlock(&loader->mutex);

        ImageResult result = {0};
        copy_text(result.path, sizeof(result.path), job.path);
        result.pixelated = job.pixelated;
        if (job.logs) {
            result.logs = true;
            size_t cap = (size_t)APP_MAX_LOGS * (APP_TEXT_CAP + 128u) + 1u, used = 0;
            result.log_data = malloc(cap);
            if (result.log_data) {
                FILE *f = fopen(job.path, "r");
                char line[APP_TEXT_CAP + 128];
                for (int count = 0; f && count < APP_MAX_LOGS && fgets(line, sizeof(line), f); count++) {
                    size_t n = strlen(line); if (n > cap - used - 1) break;
                    memcpy(result.log_data + used, line, n); used += n;
                }
                if (f) fclose(f);
                result.log_data[used] = 0;
            }
        } else {
            Image source = LoadImage(job.path);
            if (source.data) {
                result.width = source.width; result.height = source.height;
                if (job.full) { result.image = source; result.thumb = ImageCopy(source); }
                else result.thumb = source;
                if (result.thumb.data) {
                    int w = result.thumb.width, h = result.thumb.height;
                    int tw = w > h ? job.thumb_size : (int)((float)job.thumb_size * w / h);
                    int th = h > w ? job.thumb_size : (int)((float)job.thumb_size * h / w);
                    if (tw < 1) tw = 1;
                    if (th < 1) th = 1;
                    if (job.pixelated) ImageResizeNN(&result.thumb, tw, th);
                    else ImageResize(&result.thumb, tw, th);
                }
            }
        }
        pthread_mutex_lock(&loader->mutex);
        loader->result = result; loader->result_ready = true; loader->working = false; loader->active_full = false; loader->active_logs = false; loader->active_path[0] = 0;
        pthread_cond_broadcast(&loader->ready);
        pthread_mutex_unlock(&loader->mutex);
    }
    return NULL;
}

/* Start the image worker owned by App* app; return bool. */
static bool image_loader_start(App *app) {
    ImageLoader *loader = calloc(1, sizeof(*loader));
    if (!loader) return false;
    pthread_mutex_init(&loader->mutex, NULL); pthread_cond_init(&loader->ready, NULL);
    if (pthread_create(&loader->thread, NULL, image_loader_thread, loader)) {
        pthread_cond_destroy(&loader->ready); pthread_mutex_destroy(&loader->mutex); free(loader); return false;
    }
    app->image_loader = loader;
    return true;
}

/* Stop and free the image worker owned by App* app; return void. */
static void image_loader_stop(App *app) {
    ImageLoader *loader = app->image_loader;
    if (!loader) return;
    pthread_mutex_lock(&loader->mutex); loader->stopping = true; pthread_cond_broadcast(&loader->ready); pthread_mutex_unlock(&loader->mutex);
    pthread_join(loader->thread, NULL);
    if (loader->result.image.data) UnloadImage(loader->result.image);
    if (loader->result.thumb.data) UnloadImage(loader->result.thumb);
    free(loader->result.log_data);
    pthread_cond_destroy(&loader->ready); pthread_mutex_destroy(&loader->mutex); free(loader);
    app->image_loader = NULL;
}

/* Queue const char* path for App* app using int thumb size and bool filtering; return bool. */
static bool queue_image(App *app, const char *path, int thumb_size, bool pixelated, bool full) {
    ImageLoader *loader = app->image_loader;
    if (!loader || !path || !*path) return false;
    pthread_mutex_lock(&loader->mutex);
    if (loader->working && !loader->active_logs && !strcmp(loader->active_path, path) && (!full || loader->active_full)) { pthread_mutex_unlock(&loader->mutex); return true; }
    for (int i = 0; i < loader->count; i++) {
        ImageJob *queued = &loader->jobs[(loader->head + i) % IMAGE_JOB_CAP];
        if (!queued->logs && !strcmp(queued->path, path) && (!full || queued->full)) { pthread_mutex_unlock(&loader->mutex); return true; }
    }
    if (loader->result_ready && !loader->result.logs && !strcmp(loader->result.path, path) && (!full || loader->result.image.data)) { pthread_mutex_unlock(&loader->mutex); return true; }
    if (loader->count >= IMAGE_JOB_CAP) { pthread_mutex_unlock(&loader->mutex); return false; }
    int priority = full ? 3 : thumb_size >= 240 ? 2 : 1, pos = 0;
    while (pos < loader->count) {
        ImageJob *queued = &loader->jobs[(loader->head + pos) % IMAGE_JOB_CAP];
        int queued_priority = queued->full ? 3 : queued->thumb_size >= 240 ? 2 : 1;
        if (priority > queued_priority || (priority == 2 && queued_priority == 2 && strcmp(path, queued->path) > 0)) break;
        pos++;
    }
    for (int i = loader->count; i > pos; i--)
        loader->jobs[(loader->head + i) % IMAGE_JOB_CAP] = loader->jobs[(loader->head + i - 1) % IMAGE_JOB_CAP];
    int tail = (loader->head + pos) % IMAGE_JOB_CAP;
    copy_text(loader->jobs[tail].path, sizeof(loader->jobs[tail].path), path);
    loader->jobs[tail].thumb_size = thumb_size; loader->jobs[tail].pixelated = pixelated;
    loader->jobs[tail].full = full; loader->jobs[tail].logs = false;
    loader->count++; pthread_cond_signal(&loader->ready);
    pthread_mutex_unlock(&loader->mutex);
    return true;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
/* Queue AppRun* run's process.log for App* app; return bool. */
static bool queue_run_logs(App *app, AppRun *run) {
    ImageLoader *loader = app->image_loader;
    if (!loader) return false;
    char path[APP_PATH_CAP]; path_join(path, sizeof(path), run->dir, "process.log");
    pthread_mutex_lock(&loader->mutex);
    if ((loader->working && loader->active_logs && !strcmp(loader->active_path, path)) ||
        (loader->result_ready && loader->result.logs && !strcmp(loader->result.path, path))) {
        run->logs_loading = true; pthread_mutex_unlock(&loader->mutex); return true;
    }
    for (int i = 0; i < loader->count; i++) {
        ImageJob *queued = &loader->jobs[(loader->head + i) % IMAGE_JOB_CAP];
        if (queued->logs && !strcmp(queued->path, path)) { run->logs_loading = true; pthread_mutex_unlock(&loader->mutex); return true; }
    }
    if (loader->count >= IMAGE_JOB_CAP) { pthread_mutex_unlock(&loader->mutex); return false; }
    int pos = 0;
    while (pos < loader->count) {
        ImageJob *queued = &loader->jobs[(loader->head + pos) % IMAGE_JOB_CAP];
        if (!queued->full && !queued->logs && queued->thumb_size < 240) break;
        pos++;
    }
    for (int i = loader->count; i > pos; i--)
        loader->jobs[(loader->head + i) % IMAGE_JOB_CAP] = loader->jobs[(loader->head + i - 1) % IMAGE_JOB_CAP];
    int tail = (loader->head + pos) % IMAGE_JOB_CAP;
    loader->jobs[tail] = (ImageJob){0};
    copy_text(loader->jobs[tail].path, sizeof(loader->jobs[tail].path), path);
    loader->jobs[tail].logs = true; loader->count++; run->logs_loading = true;
    pthread_cond_signal(&loader->ready); pthread_mutex_unlock(&loader->mutex);
    return true;
}

/* Check whether App* app has queued, active, or completed image work; return bool. */
static bool image_loader_pending(App *app) {
    ImageLoader *loader = app->image_loader;
    if (!loader) return false;
    pthread_mutex_lock(&loader->mutex);
    bool pending = loader->count || loader->working || loader->result_ready;
    pthread_mutex_unlock(&loader->mutex);
    return pending;
}

/* Check whether const char* path remains queued or active in App* app. */
static bool image_loader_has_path(App *app, const char *path) {
    ImageLoader *loader = app->image_loader;
    if (!loader) return false;
    pthread_mutex_lock(&loader->mutex);
    bool found = (loader->working && !strcmp(loader->active_path, path)) ||
        (loader->result_ready && !strcmp(loader->result.path, path));
    for (int i = 0; !found && i < loader->count; i++)
        found = !strcmp(loader->jobs[(loader->head + i) % IMAGE_JOB_CAP].path, path);
    pthread_mutex_unlock(&loader->mutex);
    return found;
}

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

/* Draw a frame animated loading marker centered at int x,y; return void. */
static void draw_loading_marker(int x, int y, Color color) {
    float angle = (float)GetTime() * 240.0f;
    float radians = angle * DEG2RAD;
    DrawCircleLines(x, y, 8, UI_PANEL_ALT);
    DrawLineEx((Vector2){(float)x, (float)y}, (Vector2){x + cosf(radians) * 8, y + sinf(radians) * 8}, 2, color);
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

/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* Copy a bounded string into a fixed destination and terminate it. */
static void copy_text(char *dst, size_t cap, const char *src) {
    if (!cap) return;
    const char *text = src ? src : "";
    size_t length = strlen(text);
    if (length >= cap) length = cap - 1;
    memmove(dst, text, length);
    dst[length] = '\0';
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
    if (!cap) return;
    const char *left = a ? a : "", *right = b ? b : "";
    size_t left_len = strlen(left), right_len = strlen(right), used = 0;
    if (left_len >= cap) left_len = cap - 1;
    memcpy(dst, left, left_len);
    used = left_len;
    if (used + 1 < cap) dst[used++] = '/';
    size_t available = cap - used - 1;
    if (right_len > available) right_len = available;
    memcpy(dst + used, right, right_len);
    dst[used + right_len] = '\0';
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

/* Copy file from const char *src to new const char *dst; return bool. */
/* agent: gpt-6.1-sol | 2026-10-05 | Create copies without overwriting | v 3.73.0 | f72e7a */
static bool copy_file_new(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    FILE *out = fopen(dst, "wbx");
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
/* agent: gpt-6.1-sol | 2026-10-05 | Create copies without overwriting | v 3.73.0 | f72e7a */

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

/* Choose a new PNG export destination with overwrite confirmation; return bool. */
static bool choose_save_png(char *out,size_t cap) {
    int pipes[2];if(pipe(pipes))return false;pid_t pid=fork();if(pid<0){close(pipes[0]);close(pipes[1]);return false;}
    if(pid==0){close(pipes[0]);dup2(pipes[1],STDOUT_FILENO);close(pipes[1]);execlp("zenity","zenity","--file-selection","--save","--confirm-overwrite","--filename=pixel-art.png","--file-filter=PNG image | *.png",(char *)NULL);_exit(127);}
    close(pipes[1]);FILE*f=fdopen(pipes[0],"r");if(!f){close(pipes[0]);waitpid(pid,NULL,0);return false;}bool ok=fgets(out,(int)cap,f)!=NULL;fclose(f);int status=0;waitpid(pid,&status,0);if(!ok||!WIFEXITED(status)||WEXITSTATUS(status)!=0)return false;size_t n=strlen(out);while(n&&(out[n-1]=='\n'||out[n-1]=='\r'))out[--n]=0;return n>0;
}

/* Copy a file through a sibling temporary and replace destination atomically; return bool. */
static bool copy_file_atomic(const char *source,const char *destination) {
    char temporary[APP_PATH_CAP];if(snprintf(temporary,sizeof(temporary),"%s.export-%ld",destination,(long)getpid())<0)return false;
    int in=open(source,O_RDONLY),out=-1;if(in<0)return false;out=open(temporary,O_WRONLY|O_CREAT|O_TRUNC,0644);if(out<0){close(in);return false;}
    char buffer[65536];bool ok=true;for(;;){ssize_t n=read(in,buffer,sizeof(buffer));if(n==0)break;if(n<0){if(errno==EINTR)continue;ok=false;break;}ssize_t sent=0;while(sent<n){ssize_t m=write(out,buffer+sent,(size_t)(n-sent));if(m<0){if(errno==EINTR)continue;ok=false;break;}sent+=m;}if(!ok)break;}
    if(fsync(out))ok=false;
    close(in);
    if(close(out))ok=false;
    if(ok&&rename(temporary,destination)==0)return true;
    unlink(temporary);return false;
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

/* agent: gpt-6.1-sol | 2026-10-04 | Recognize profile resource files | v 3.55.0 | be1d28 */
/* Return true for a profile resource file; return bool. */
static bool is_profile_path(const char *p) {
    const char *dot=strrchr(p,'.');
    return dot && !strcasecmp(dot,".profile");
}
/* agent: gpt-6.1-sol | 2026-10-04 | Recognize profile resource files | v 3.55.0 | be1d28 */

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

/* agent: gpt-6.1-sol | 2026-10-04 | Migrate legacy refs into resources | v 3.55.0 | b5b4bf */
/* Move legacy refs files and one-level groups into resources; return whether files moved. */
static bool migrate_legacy_refs(App *app) {
    char old_root[APP_PATH_CAP]; path_join(old_root,sizeof(old_root),app->workspace,"refs");
    DIR *root=opendir(old_root); if(!root)return false;
    bool moved=false; struct dirent *entry;
    while((entry=readdir(root))) {
        if(entry->d_name[0]=='.')continue;
        char source[APP_PATH_CAP]; path_join(source,sizeof(source),old_root,entry->d_name);
        struct stat st; if(stat(source,&st))continue;
        if(S_ISREG(st.st_mode)) {
            if(!is_image_path(entry->d_name)&&!is_text_path(entry->d_name))continue;
            char target[APP_PATH_CAP]; if(reference_target_path(app,entry->d_name,target,sizeof(target))&&rename(source,target)==0)moved=true;
            continue;
        }
        if(!S_ISDIR(st.st_mode))continue;
        DIR *group=opendir(source); if(!group)continue; struct dirent *file;
        while((file=readdir(group))) {
            if(file->d_name[0]=='.'||(!is_image_path(file->d_name)&&!is_text_path(file->d_name)))continue;
            char from[APP_PATH_CAP],to[APP_PATH_CAP]; path_join(from,sizeof(from),source,file->d_name);
            if(!reference_target_path(app,file->d_name,to,sizeof(to)))continue;
            if(rename(from,to)==0)moved=true;
        }
        closedir(group); rmdir(source);
    }
    closedir(root); rmdir(old_root); return moved;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Migrate legacy refs into resources | v 3.55.0 | b5b4bf */

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
    else if (strcasestr(line, "uploading")) label = "Uploading resource images";
    else if (strcasestr(line, "uploaded")) label = "Resources uploaded";
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
    run->logs_loaded = true;
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

static bool save_profiles(App *app);

/* Parse one readable profile resource into AppProfile; return bool. */
/* Set safe pixel pipeline defaults for AppPixelSettings* settings; return void. */
static void pixel_settings_defaults(AppPixelSettings *settings) {
    memset(settings, 0, sizeof(*settings));
    settings->background_tolerance=24; settings->extraction_padding=1;
    settings->pitch_min=2; settings->pitch_max=16; settings->foreground_coverage=35;
    settings->cleanup_tolerance=55; settings->sheet_padding=1;
    copy_text(settings->pitch,sizeof(settings->pitch),"auto");
    copy_text(settings->phase_x,sizeof(settings->phase_x),"auto");copy_text(settings->phase_y,sizeof(settings->phase_y),"auto");
    copy_text(settings->pitch_precision,sizeof(settings->pitch_precision),"0.1");
    copy_text(settings->cleanup_mode,sizeof(settings->cleanup_mode),"outside");
    copy_text(settings->sheet_anchor,sizeof(settings->sheet_anchor),"center");
}

/* Set one supported processor default on AppProfile* profile; return bool. */
static bool set_pixel_profile_field(AppProfile *profile,const char *field,const char *value) {
    AppPixelSettings *p=&profile->pixel; int number=0;
    if(!strcmp(field,"background_tolerance")){if(!parse_profile_number(value,0,255,&number))return false;p->background_tolerance=number;}
    else if(!strcmp(field,"grid_cols")){if(!parse_profile_number(value,0,256,&number))return false;p->grid_cols=number;}
    else if(!strcmp(field,"grid_rows")){if(!parse_profile_number(value,0,256,&number))return false;p->grid_rows=number;}
    else if(!strcmp(field,"extraction_padding")){if(!parse_profile_number(value,0,64,&number))return false;p->extraction_padding=number;}
    else if(!strcmp(field,"pitch")){if(strcmp(value,"auto")){char *end=NULL;double n=strtod(value,&end);if(end==value||*end||!isfinite(n)||n<1||n>128)return false;}copy_text(p->pitch,sizeof(p->pitch),value);}
    else if(!strcmp(field,"phase_x")||!strcmp(field,"phase_y")){if(strcmp(value,"auto")){char *end=NULL;double n=strtod(value,&end);if(end==value||*end||!isfinite(n)||n< -100000||n>100000)return false;}copy_text(!strcmp(field,"phase_x")?p->phase_x:p->phase_y,24,value);}
    else if(!strcmp(field,"pitch_min")){if(!parse_profile_number(value,1,128,&number)||number>p->pitch_max)return false;p->pitch_min=number;}
    else if(!strcmp(field,"pitch_max")){if(!parse_profile_number(value,1,128,&number)||number<p->pitch_min)return false;p->pitch_max=number;}
    else if(!strcmp(field,"pitch_precision")){char *end=NULL;double n=strtod(value,&end);if(end==value||*end||!isfinite(n)||n<.01||n>4)return false;copy_text(p->pitch_precision,sizeof(p->pitch_precision),value);}
    else if(!strcmp(field,"foreground_coverage")){if(!parse_profile_number(value,0,100,&number))return false;p->foreground_coverage=number;}
    else if(!strcmp(field,"cleanup_mode")){if(strcmp(value,"outside")&&strcmp(value,"all")&&strcmp(value,"skip"))return false;copy_text(p->cleanup_mode,sizeof(p->cleanup_mode),value);}
    else if(!strcmp(field,"cleanup_tolerance")){if(!parse_profile_number(value,0,255,&number))return false;p->cleanup_tolerance=number;}
    else if(!strcmp(field,"sheet_anchor")){if(strcmp(value,"center")&&strcmp(value,"top_left")&&strcmp(value,"bottom_center")&&strcmp(value,"top_right"))return false;copy_text(p->sheet_anchor,sizeof(p->sheet_anchor),value);}
    else if(!strcmp(field,"sheet_padding")){if(!parse_profile_number(value,0,256,&number))return false;p->sheet_padding=number;}
    else return false;
    return true;
}

/* Parse a profile resource with generation settings and new pixel defaults; return bool. */
static bool parse_profile_text(const char *text, AppProfile *profile) {
    AppProfile parsed = {0};
    pixel_settings_defaults(&parsed.pixel);
    char lines[32768]; copy_text(lines, sizeof(lines), text);
    char *save = NULL;
    for (char *line = strtok_r(lines, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (!*line || *line == '#' || *line == ';') continue;
        char *eq = strchr(line, '=');
        if (!eq) return false;
        *eq++ = 0;
        char *end = line + strlen(line);
        while (end > line && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) *--end = 0;
        if (!strcmp(line, "name")) copy_text(parsed.name, sizeof(parsed.name), eq);
        else if (!strcmp(line, "model")) copy_text(parsed.model, sizeof(parsed.model), eq);
        else if (!strcmp(line, "aspect")) copy_text(parsed.aspect, sizeof(parsed.aspect), eq);
        else if (!strcmp(line, "background_tolerance") || !strcmp(line,"grid_cols") || !strcmp(line,"grid_rows") || !strcmp(line,"extraction_padding") || !strcmp(line,"pitch") || !strcmp(line,"phase_x") || !strcmp(line,"phase_y") || !strcmp(line,"pitch_min") || !strcmp(line,"pitch_max") || !strcmp(line,"pitch_precision") || !strcmp(line,"foreground_coverage") || !strcmp(line,"cleanup_mode") || !strcmp(line,"cleanup_tolerance") || !strcmp(line,"sheet_anchor") || !strcmp(line,"sheet_padding")) { if(!set_pixel_profile_field(&parsed,line,eq))return false; }
        else if (!strcmp(line, "layout") || !strcmp(line,"pixel_pitch") || !strcmp(line,"pitch_preset") || !strcmp(line,"pitch_group_layout") || !strcmp(line,"align_blobs") || !strcmp(line,"magenta") || !strcmp(line,"edge_alpha") || !strcmp(line,"rearrange") || !strcmp(line,"padding") || !strcmp(line,"tile_size")) { /* Legacy pixel settings are discarded. */ }
        else if (!strcmp(line, "slim_upload")) { if(!parse_profile_number(eq,0,1,&parsed.slim_upload))return false; }
        else if (!strcmp(line, "set") && parsed.set_count < APP_MAX_PROMPTS) copy_text(parsed.sets[parsed.set_count++], sizeof(parsed.sets[0]), eq);
        else if (!strcmp(line, "prompt") && parsed.prompt_count < APP_MAX_PROMPTS) copy_text(parsed.prompts[parsed.prompt_count++], sizeof(parsed.prompts[0]), eq);
        else return false;
    }
    if (!parsed.name[0] || !parsed.model[0]) return false;
    copy_text(parsed.path, sizeof(parsed.path), profile->path);
    *profile = parsed;
    return true;
}

/* Serialize one profile into readable resource text; return bool. */
static bool save_profile_resource(AppProfile *p) {
    char out[32768]; size_t n = 0;
    n += (size_t)snprintf(out+n,sizeof(out)-n,"name=%s\nmodel=%s\naspect=%s\nslim_upload=%d\nbackground_tolerance=%d\ngrid_cols=%d\ngrid_rows=%d\nextraction_padding=%d\npitch=%s\nphase_x=%s\nphase_y=%s\npitch_min=%d\npitch_max=%d\npitch_precision=%s\nforeground_coverage=%d\ncleanup_mode=%s\ncleanup_tolerance=%d\nsheet_anchor=%s\nsheet_padding=%d\n",p->name,p->model,p->aspect,p->slim_upload,p->pixel.background_tolerance,p->pixel.grid_cols,p->pixel.grid_rows,p->pixel.extraction_padding,p->pixel.pitch,p->pixel.phase_x,p->pixel.phase_y,p->pixel.pitch_min,p->pixel.pitch_max,p->pixel.pitch_precision,p->pixel.foreground_coverage,p->pixel.cleanup_mode,p->pixel.cleanup_tolerance,p->pixel.sheet_anchor,p->pixel.sheet_padding);
    for (int i=0;i<p->set_count&&n<sizeof(out)-256;i++) n+=(size_t)snprintf(out+n,sizeof(out)-n,"set=%s\n",p->sets[i]);
    for (int i=0;i<p->prompt_count&&n<sizeof(out)-600;i++) n+=(size_t)snprintf(out+n,sizeof(out)-n,"prompt=%s\n",p->prompts[i]);
    return p->path[0] && write_text(p->path,out);
}

/* Load individual profile resources, migrating the legacy INI when needed; return void. */
static void load_profiles(App *app) {
    DIR *root=opendir(app->refs_path); struct dirent *de; bool found=false;
    if (root) {
        while ((de=readdir(root))) if (is_profile_path(de->d_name)) {
            char path[APP_PATH_CAP],text[32768]; path_join(path,sizeof(path),app->refs_path,de->d_name);
            if (app->profile_count>=APP_MAX_PROFILES || !read_text(path,text,sizeof(text))) continue;
            AppProfile *p=&app->profiles[app->profile_count]; memset(p,0,sizeof(*p)); copy_text(p->path,sizeof(p->path),path);
            if (parse_profile_text(text,p)) { app->profile_count++; found=true; }
        }
        closedir(root);
    }
    if (!found && file_exists(app->profiles_path)) {
        FILE *f=fopen(app->profiles_path,"r"); AppProfile *p=NULL; char line[1024];
        if (f) {
            while (fgets(line,sizeof(line),f)) {
                char *v=line; while (*v==' '||*v=='\t')v++; size_t len=strlen(v); while(len&&(v[len-1]=='\n'||v[len-1]=='\r'))v[--len]=0;
                if (!*v||*v=='#'||*v==';')continue;
                if (*v=='[') { if(app->profile_count>=APP_MAX_PROFILES){p=NULL;continue;} p=&app->profiles[app->profile_count++]; memset(p,0,sizeof(*p)); pixel_settings_defaults(&p->pixel); char *name=strstr(v,"profile:"); if(name){name+=8;char *end=strchr(name,']');if(end)*end=0;copy_text(p->name,sizeof(p->name),name);} continue; }
                if(!p) continue;
                char *eq=strchr(v,'=');
                if(!eq) continue;
                *eq++=0;
                if(!strcmp(v,"model"))copy_text(p->model,sizeof(p->model),eq);else if(!strcmp(v,"aspect"))copy_text(p->aspect,sizeof(p->aspect),eq);else if(!strcmp(v,"slim_upload"))p->slim_upload=atoi(eq)!=0;else if(!strcmp(v,"set")&&p->set_count<APP_MAX_PROMPTS)copy_text(p->sets[p->set_count++],sizeof(p->sets[0]),eq);else if(!strcmp(v,"prompt")&&p->prompt_count<APP_MAX_PROMPTS)copy_text(p->prompts[p->prompt_count++],sizeof(p->prompts[0]),eq);
            }
            fclose(f);
        }
        save_profiles(app);
    }
    if (app->profile_count) app->active_profile=0;
    char active[APP_PATH_CAP];
    if (read_text(app->active_profile_path,active,sizeof(active))) {
        active[strcspn(active,"\r\n")]=0;
        for(int i=0;i<app->profile_count;i++) if(!strcmp(app->profiles[i].path,active)) app->active_profile=i;
    }
}

/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* Persist profile resources and the single enabled profile; return bool. */
static bool save_profiles(App *app) {
    bool ok=true;
    for(int i=0;i<app->profile_count;i++) {
        AppProfile *p=&app->profiles[i];
        if(!p->path[0]) {
            char candidate[APP_PATH_CAP]; bool available=false;
            for(int id=1;id<1000;id++) { char name[32]; snprintf(name,sizeof(name),"profile-%03d.profile",id); path_join(candidate,sizeof(candidate),app->refs_path,name); struct stat st; if(lstat(candidate,&st)&&errno==ENOENT){available=true;break;} }
            if(!available){ok=false;continue;} copy_text(p->path,sizeof(p->path),candidate);
        }
        if(!save_profile_resource(p))ok=false;
    }
    if(app->active_profile>=0&&app->active_profile<app->profile_count) ok=write_text(app->active_profile_path,app->profiles[app->active_profile].path)&&ok;
    for(int i=0;i<app->ref_count;i++) if(app->refs[i].is_profile) {
        for(int k=0;k<app->profile_count;k++) if(!strcmp(app->refs[i].path,app->profiles[k].path)) {
            app->refs[i].profile_index=k; app->refs[i].selected=k==app->active_profile;
            copy_text(app->refs[i].name,sizeof(app->refs[i].name),app->profiles[k].name); break;
        }
    }
    return ok;
}

/* Select exactly one profile resource and persist the workspace choice; return void. */
static void set_active_profile(App *app, int index) {
    if(index<0||index>=app->profile_count)return;
    app->active_profile=index;
    for(int i=0;i<app->ref_count;i++) if(app->refs[i].is_profile) app->refs[i].selected=app->refs[i].profile_index==index;
    save_profiles(app);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Persist generation profiles as resources | v 3.55.0 | 9d1e3c */

/* Write the initial aigen1-equivalent profile when a workspace has none. */
static void seed_profile(App *app) {
    if (app->profile_count) return;
    AppProfile *p = &app->profiles[0];
    memset(p, 0, sizeof(*p));
    copy_text(p->name, sizeof(p->name), "Default");
    copy_text(p->model, sizeof(p->model), "openai/gpt-image-2.5-flare");
    copy_text(p->aspect, sizeof(p->aspect), "1:1");
    pixel_settings_defaults(&p->pixel);
    p->slim_upload = 1;
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
/* agent: gpt-6.1-sol | 2026-10-04 | Show active profile resource state | v 3.55.0 | fa71b7 */
/* Draw two compact corner actions for an AppReference; return void. */
static void draw_reference_icons(AppReference *ref, Rectangle box) {
    Rectangle exclude = {box.x + box.width - 58, box.y + 6, 24, 24};
    Rectangle trash = {box.x + box.width - 28, box.y + 6, 22, 24};
    bool over_exclude = CheckCollisionPointRec(GetMousePosition(), exclude);
    bool over_trash = CheckCollisionPointRec(GetMousePosition(), trash);
    DrawRectangleRec(exclude, (Color){20, 22, 26, 225});
    DrawRectangleRec(trash, (Color){20, 22, 26, 225});
    DrawCircleLines((int)(exclude.x + 12), (int)(exclude.y + 12), 7, over_exclude ? UI_TEXT : UI_MUTED);
    bool enabled=ref->is_profile?ref->selected:!ref->excluded;
    if (!enabled) DrawLine((int)exclude.x + 5, (int)exclude.y + 19, (int)exclude.x + 19, (int)exclude.y + 5, UI_BAD);
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
/* agent: gpt-6.1-sol | 2026-10-04 | Show active profile resource state | v 3.55.0 | fa71b7 */

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

/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
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
            char unique[128]; snprintf(unique, sizeof(unique), "%.*s-%d", (int)sizeof(unique)-12, base, suffix);
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
/* Load const char* image path into App* asynchronously when available; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Load profile cards with resources | v 3.55.0 | 824258 */
/* agent: gpt-6.1-sol | 2026-10-04 | Queue workspace reference images | v 3.55.0 | b3fba2 */
static void add_reference(App *app, const char *path) {
    if (app->ref_count >= APP_MAX_REFS || (!is_image_path(path) && !is_text_path(path) && !is_profile_path(path))) return;
    for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, path)) return;
    AppReference *r = &app->refs[app->ref_count++];
    memset(r, 0, sizeof(*r));
    copy_text(r->path, sizeof(r->path), path);
    const char *base = strrchr(path, '/');
    copy_text(r->name, sizeof(r->name), base ? base + 1 : path);
    r->selected = true;
    r->is_text = is_text_path(path);
    r->is_profile = is_profile_path(path);
    r->profile_index = -1;
    if(r->is_profile) {
        for(int i=0;i<app->profile_count;i++) if(!strcmp(app->profiles[i].path,path)) { r->profile_index=i; r->selected=i==app->active_profile; copy_text(r->name,sizeof(r->name),app->profiles[i].name); break; }
        return;
    }
    if (r->is_text) {
        char content[APP_TEXT_CAP];
        if (read_text(path, content, sizeof(content))) cache_prompt_preview(r, content);
        return;
    }
    if (queue_image(app, path, 160, true, false)) { r->loading = true; return; }
    Image image = LoadImage(path);
    if (image.data) {
        r->width = image.width; r->height = image.height;
        r->full = LoadTextureFromImage(image);
        if (r->full.id) SetTextureFilter(r->full, TEXTURE_FILTER_POINT);
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
/* agent: gpt-6.1-sol | 2026-10-04 | Load profile cards with resources | v 3.55.0 | 824258 */

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
    app->profile_text_edit=false;
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
    set_message(app, "Resource imported");
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

/* Scan flat files in App* app's resources root; return void. */
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
/* agent: gpt-6.1-sol | 2026-10-04 | Edit profile resources as text | v 3.55.0 | 7d0fdd */
/* Save dirty prompt or profile text and apply it; return bool. */
static bool save_open_prompt(App *app) {
    if (!app->prompt_dirty) return true;
    if (app->profile_text_edit) {
        AppProfile parsed=app->profiles[app->active_profile];
        if(!parse_profile_text(app->prompt_edit,&parsed)) { app->prompt_save_error=true; set_message(app,"Profile text is invalid; settings were not changed"); return false; }
        app->profiles[app->active_profile]=parsed;
        if(!save_profiles(app)) { app->prompt_save_error=true; set_message(app,"Could not save profile resource"); return false; }
        app->prompt_dirty=false; app->prompt_save_error=app->prompt_limit_error=false;
        set_message(app,"Profile resource saved"); return true;
    }
    if (!write_text(app->prompt_path, app->prompt_edit)) { app->prompt_save_error = true; set_message(app, "Could not save prompt"); return false; }
    app->prompt_dirty = false;
    app->prompt_save_error = app->prompt_limit_error = false;
    for (int i = 0; i < app->ref_count; i++)
        if (app->refs[i].is_text && !strcmp(app->refs[i].path, app->prompt_path)) cache_prompt_preview(&app->refs[i], app->prompt_edit);
    set_message(app, "Prompt saved");
    return true;
}

/* Open a profile resource in the shared text editor; return bool. */
static bool open_profile_text(App *app, int index) {
    if(index<0||index>=app->profile_count)return false;
    if(app->prompt_dirty&&!save_open_prompt(app))return false;
    AppProfile *p=&app->profiles[index]; char text[32768];
    if(!read_text(p->path,text,sizeof(text)))return false;
    set_active_profile(app,index);
    copy_text(app->prompt_path,sizeof(app->prompt_path),p->path);
    copy_text(app->prompt_edit,sizeof(app->prompt_edit),text);
    app->prompt_dirty=app->prompt_editor_focused=app->prompt_save_error=app->prompt_limit_error=app->prompt_close_pending=false;
    app->prompt_caret=app->prompt_anchor=(int)strlen(app->prompt_edit); app->prompt_scroll=0;
    app->profile_text_edit=true; app->profile_settings_view=false; app->settings_view=false; app->show_full_image=true; app->view=VIEW_LIBRARY;
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Edit profile resources as text | v 3.55.0 | 7d0fdd */

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

/* agent: gpt-6.1-sol | 2026-10-04 | Support profile resource filename editing | v 3.60.0 | 1b3d96 */
/* agent: gpt-6.1-sol | 2026-10-04 | Align profile toolbar and support filename editing | v 3.60.0 | a06e32 */
/* Rename AppReference* ref from const char* entered for App* app; return bool. */
static bool rename_reference_file(App *app, AppReference *ref, const char *entered) {
    char name[256], directory[APP_PATH_CAP], target[APP_PATH_CAP];
    copy_text(name, sizeof(name), entered);
    size_t n = strlen(name);
    while (n && (name[n-1] == ' ' || name[n-1] == '\t')) name[--n] = 0;
    const char *file_name = strrchr(ref->path, '/'); file_name = file_name ? file_name + 1 : ref->path;
    const char *extension = strrchr(file_name, '.');
    if (!extension || !n || !strcmp(name, ".") || !strcmp(name, "..") || strchr(name, '/') || strchr(name, '\\')) { set_message(app, "Enter a valid resource filename"); return false; }
    const char *entered_extension = strrchr(name, '.');
    bool has_file_extension = entered_extension != NULL;
    if (has_file_extension && strcasecmp(entered_extension, extension)) { set_message(app, "Keep the original file extension"); return false; }
    if (!has_file_extension) {
        if (n + strlen(extension) >= sizeof(name)) { set_message(app, "Resource filename is too long"); return false; }
        strcat(name, extension);
    }
    if (!strcmp(file_name, name)) return true;
    const char *slash = strrchr(ref->path, '/');
    if (!slash) return false;
    size_t directory_len = (size_t)(slash - ref->path);
    if (directory_len >= sizeof(directory)) return false;
    memcpy(directory, ref->path, directory_len); directory[directory_len] = 0;
    path_join(target, sizeof(target), directory, name);
    struct stat st;
    if (!lstat(target, &st)) { set_message(app, "A file with that name already exists"); return false; }
    if (errno != ENOENT) { set_message(app, "Could not check resource filename"); return false; }
    bool open_document = !strcmp(ref->path, app->prompt_path);
    if (open_document && !save_open_prompt(app)) return false;
    if (rename(ref->path, target)) { set_message(app, "Could not rename resource"); return false; }
    copy_text(ref->path, sizeof(ref->path), target);
    copy_text(ref->name, sizeof(ref->name), name);
    if (ref->is_profile && ref->profile_index >= 0 && ref->profile_index < app->profile_count) {
        AppProfile *profile = &app->profiles[ref->profile_index];
        copy_text(profile->path, sizeof(profile->path), target);
        const char *dot = strrchr(name, '.');
        size_t stem_length = dot ? (size_t)(dot - name) : strlen(name);
        if (stem_length >= sizeof(profile->name)) stem_length = sizeof(profile->name) - 1;
        memcpy(profile->name, name, stem_length); profile->name[stem_length] = 0;
        save_profiles(app);
    }
    if (open_document) copy_text(app->prompt_path, sizeof(app->prompt_path), target);
    save_reference_state(app);
    set_message(app, "Resource renamed");
    return true;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Support profile resource filename editing | v 3.60.0 | 1b3d96 */
/* agent: gpt-6.1-sol | 2026-10-04 | Align profile toolbar and support filename editing | v 3.60.0 | a06e32 */

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
/* agent: gpt-6.1-sol | 2026-10-04 | Add generation run controls | v 3.53.0 | 522f7a */
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
    if (ok && fprintf(file, "%s: %s\nOutput folder: %s\nImage resources (%d):\n",
        pixel_only ? "Input image" : "Original generated image", image_path, run->dir, reference_count) < 0) ok = false;
    for (int i = 0; ok && i < reference_count; i++) {
        if (fprintf(file, "  %s\n", app->snapshot_refs[i]) < 0) ok = false;
    }
    if (ok && fprintf(file, "Resources used (%d):\n", run->used_resource_count) < 0) ok = false;
    for (int i = 0; ok && i < run->used_resource_count; i++) if (fprintf(file, "  %s\n", run->used_resources[i]) < 0) ok = false;
    if (ok && fprintf(file, "Prompt:\n%s\n", run->resolved_prompt) < 0) ok = false;
    if (fclose(file)) ok = false;
    if (!ok) unlink(path);
    return ok;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Add generation run controls | v 3.53.0 | 522f7a */

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

/* agent: gpt-6 | 2026-10-05 | Persist sprite override settings | v 3.88.0 | 6f1785 */
/* Write versioned effective pipeline settings for AppRun* run using AppPixelSettings* settings; return bool. */
static bool write_pixel_settings(AppRun *run,const AppPixelSettings *settings) {
    char path[APP_PATH_CAP];path_join(path,sizeof(path),run->dir,"pixelprocess.settings.json");
    FILE *f=fopen(path,"wb");if(!f)return false;
    const AppPixelSettings *p=settings;
    bool ok=fprintf(f,"{\"schema_version\":1,\"background_tolerance\":%d,\"grid_cols\":%d,\"grid_rows\":%d,\"extraction_padding\":%d,\"pitch\":\"%s\",\"phase_x\":\"%s\",\"phase_y\":\"%s\",\"min_pitch\":%d,\"max_pitch\":%d,\"pitch_precision\":%s,\"foreground_coverage\":%.3f,\"cleanup_mode\":\"%s\",\"cleanup_tolerance\":%d,\"sheet_anchor\":\"%s\",\"sheet_padding\":%d,\"sprite_overrides\":[",
        p->background_tolerance,p->grid_cols,p->grid_rows,p->extraction_padding,p->pitch,p->phase_x,p->phase_y,p->pitch_min,p->pitch_max,p->pitch_precision,(double)p->foreground_coverage/100.0,p->cleanup_mode,p->cleanup_tolerance,p->sheet_anchor,p->sheet_padding)>=0;
    for(int i=0;ok&&i<run->sprite_override_count;i++){
        AppSpriteSettings *item=&run->sprite_overrides[i];if(i)ok=fputc(',',f)!=EOF;
        if(ok)ok=fprintf(f,"{\"id\":\"%s\",\"pitch\":\"%s\",\"phase_x\":\"%s\",\"phase_y\":\"%s\"}",item->id,item->settings.pitch,item->settings.phase_x,item->settings.phase_y)>=0;
    }
    if(ok)ok=fprintf(f,"],\"ownership_overrides\":{")>=0;
    for(int i=0;ok&&i<run->owner_override_count;i++){if(i)ok=fputc(',',f)!=EOF;if(ok)ok=fprintf(f,"\"%s\":\"%s\"",run->owner_components[i],run->owner_sprites[i])>=0;}
    if(ok)ok=fprintf(f,"}}\n")>=0;
    if(fclose(f))ok=false;
    if(!ok)unlink(path);else copy_text(run->processing_settings_path,sizeof(run->processing_settings_path),path);
    return ok;
}
/* agent: gpt-6 | 2026-10-05 | Persist sprite override settings | v 3.88.0 | 6f1785 */

/* Find an executable named runner on PATH; return bool. */
static bool find_runner_on_path(char *out,size_t cap) {
    const char *path=getenv("PATH");if(!path)return false;
    for(const char *part=path;*part;){const char *end=strchr(part,':');size_t length=end?(size_t)(end-part):strlen(part);char folder[APP_PATH_CAP],candidate[APP_PATH_CAP];
        if(length<sizeof(folder)){memcpy(folder,part,length);folder[length]=0;if(!length)copy_text(folder,sizeof(folder),".");path_join(candidate,sizeof(candidate),folder,"runner");if(access(candidate,X_OK)==0){copy_text(out,cap,candidate);return true;}}
        if(!end) break;
        part=end+1;
    }
    return false;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Resolve source named processor runner | v 3.84.0 | 3dd7fa */
/* Resolve configured, adjacent, or PATH runner into char* out; return bool. */
static bool resolve_pixel_runner(char *out,size_t cap) {
    const char *disabled=getenv("AIGEN_DISABLE_PIXELPROCESS");if(disabled&&(!strcmp(disabled,"1")||!strcasecmp(disabled,"true")))return false;
    const char *configured=getenv("AIGEN_PIXELPROCESS_RUNNER");
    if(configured&&*configured){if(access(configured,X_OK)!=0)return false;copy_text(out,cap,configured);return true;}
    const char *directory=getenv("AIGEN_PIXELPROCESS_DIR");
    if(directory&&*directory){char candidate[APP_PATH_CAP];path_join(candidate,sizeof(candidate),directory,"runner");if(access(candidate,X_OK)==0){copy_text(out,cap,candidate);return true;}path_join(candidate,sizeof(candidate),directory,"bin/runner");if(access(candidate,X_OK)==0){copy_text(out,cap,candidate);return true;}return false;}
    char executable[APP_PATH_CAP];ssize_t length=readlink("/proc/self/exe",executable,sizeof(executable)-1);
    if(length>0&&(size_t)length<sizeof(executable)){executable[length]=0;char *slash=strrchr(executable,'/');if(slash){*slash=0;path_join(out,cap,executable,"pixelprocess/runner");if(access(out,X_OK)==0)return true;}}
    return find_runner_on_path(out,cap);
}
/* agent: gpt-6.1-sol | 2026-10-05 | Resolve source named processor runner | v 3.84.0 | 3dd7fa */

/* Start the pixelprocess runner for App* app and AppRun* run; return bool. */
static bool start_pixel(App *app, AppRun *run) {
    char *argv[12];int argc=0;char runner[APP_PATH_CAP];if(!resolve_pixel_runner(runner,sizeof(runner))){copy_text(run->error,sizeof(run->error),"pixel processor unavailable; configure AIGEN_PIXELPROCESS_DIR or AIGEN_PIXELPROCESS_RUNNER");return false;}
    if(!run->pixel_settings.pitch[0])run->pixel_settings=app->snapshot_profile.pixel;
    if(!write_pixel_settings(run,&run->pixel_settings)){copy_text(run->error,sizeof(run->error),"could not write pixelprocess settings");return false;}
    char settings_path[APP_PATH_CAP];copy_text(settings_path,sizeof(settings_path),run->processing_settings_path);
    argv_push(argv,&argc,12,runner);argv_push(argv,&argc,12,"--input");argv_push(argv,&argc,12,run->source);
    argv_push(argv,&argc,12,"--run-dir");argv_push(argv,&argc,12,run->dir);argv_push(argv,&argc,12,"--settings");argv_push(argv,&argc,12,settings_path);
    if(run->pixel_target[0]){argv_push(argv,&argc,12,"--target");argv_push(argv,&argc,12,run->pixel_target);}
    if(run->pixel_sprite[0]){argv_push(argv,&argc,12,"--sprite");argv_push(argv,&argc,12,run->pixel_sprite);}
    argv[argc] = NULL;
    if (!spawn_process(run, app->workspace, argv, PROC_PIXEL)) return false;
    copy_text(run->primary,sizeof(run->primary),run->source);
    run->state=RUN_RUNNING;run->started=GetTime();
    return true;
}

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

/* Queue/load AppArtifact* from App* and const char* path/name plus int number; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Queue run artifact image loads | v 3.55.0 | 2fdcae */
static bool load_artifact_image(App *app, AppArtifact *artifact, const char *path, const char *name, int number) {
    struct stat info;
    if (stat(path, &info) || !S_ISREG(info.st_mode)) return false;
    if (artifact->loading) return true;
    memset(artifact, 0, sizeof(*artifact));
    copy_text(artifact->name, sizeof(artifact->name), name);
    copy_text(artifact->path, sizeof(artifact->path), path);
    artifact->number = number; artifact->file_size = info.st_size;
    if (queue_image(app, path, 240, false, false)) {
        artifact->loading = true;
        return true;
    }
    Image image = LoadImage(path);
    if (!image.data) return false;
    artifact->width = image.width; artifact->height = image.height;
    artifact->texture = LoadTextureFromImage(image);
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
/* Release cached per-sprite preview textures and clear their identity in AppRun* run; return void. */
/* agent: gpt-6 | 2026-10-05 | Clear per-sprite preview cache | v 3.97.0 | 58bfb5 */
static void clear_sprite_version_previews(AppRun *run) {
    for (int i = 0; i < run->sprite_version_preview_count; i++) unload_artifact(&run->sprite_version_previews[i]);
    run->sprite_version_preview_count = 0;
    run->sprite_version_preview_id[0] = 0;
}
/* agent: gpt-6 | 2026-10-05 | Clear per-sprite preview cache | v 3.97.0 | 58bfb5 */
/* agent: gpt-6.1-sol | 2026-10-04 | Classify copies by dedicated folder | v 3.55.0 | 71bb3c */
/* Release loaded images for AppRun* run; return void. */
static void unload_run_artifacts(AppRun *run) {
    unload_artifact(&run->generated);
    for (int i = 0; i < run->stage_count; i++) unload_artifact(&run->stages[i]);
    for (int i = 0; i < run->debug_stage_count; i++) unload_artifact(&run->debug_stages[i]);
    for (int i = 0; i < run->output_count; i++) unload_artifact(&run->outputs[i]);
    for (int i = 0; i < run->draw_copy_count; i++) unload_artifact(&run->draw_copies[i]);
    clear_sprite_version_previews(run);
    run->stage_count = run->debug_stage_count = run->output_count = run->draw_copy_count = 0;
}

/* Scan AppRun* output folders and queue images using App* app; return void. */
/* Load new JSONL processing events into AppRun* run; return void. */
/* agent: gpt-6 | 2026-10-04 | Load incremental processor journal events | v 3.67.0 | 1c46fd */
static void load_processing_events(AppRun *run) {
    char path[APP_PATH_CAP];
    path_join(path, sizeof(path), run->dir, "extra/processing.events.jsonl");
    struct stat st;
    if (stat(path, &st)) { run->metadata_event_count = 0; run->metadata_offset = 0; run->metadata_size = 0; return; }
    if (st.st_size < run->metadata_offset || (st.st_size == run->metadata_size && st.st_mtime == run->metadata_mtime)) return;
    FILE *file = fopen(path, "r");
    if (!file) return;
    if (st.st_size < run->metadata_offset) { run->metadata_event_count = 0; run->metadata_offset = 0; }
    if (fseeko(file, run->metadata_offset, SEEK_SET)) { fclose(file); return; }
    char line[APP_TEXT_CAP], kind[24], message[APP_TEXT_CAP], stamp[40], variant[64], raw_number[32];
    while (fgets(line, sizeof(line), file)) {
        if (!strchr(line, '\n')) break;
        kind[0] = message[0] = stamp[0] = variant[0] = raw_number[0] = 0;
        json_string(line, "kind", kind, sizeof(kind));
        json_string(line, "message", message, sizeof(message));
        json_string(line, "timestamp_utc", stamp, sizeof(stamp));
        json_string(line, "variant_id", variant, sizeof(variant));
        int stage_number = json_raw(line, "stage_number", raw_number, sizeof(raw_number)) ? atoi(raw_number) : -1;
        run->metadata_offset = ftello(file);
        if (run->metadata_event_count == APP_MAX_METADATA_EVENTS) {
            memmove(run->metadata_events, run->metadata_events + 1, sizeof(run->metadata_events[0]) * (APP_MAX_METADATA_EVENTS - 1));
            run->metadata_event_count--;
        }
        AppMetadataEvent *event = &run->metadata_events[run->metadata_event_count++];
        event->stage_number = stage_number;
        copy_text(event->kind, sizeof(event->kind), kind);
        copy_text(event->time, sizeof(event->time), stamp);
        copy_text(event->variant_id, sizeof(event->variant_id), variant);
        copy_text(event->message, sizeof(event->message), message);
    }
    run->metadata_size = st.st_size;
    run->metadata_mtime = st.st_mtime;
    fclose(file);
}
/* agent: gpt-6 | 2026-10-04 | Load incremental processor journal events | v 3.67.0 | 1c46fd */

/* Read summary fields from AppRun* run's newest manifest; return void. */
/* agent: gpt-6 | 2026-10-04 | Read processor manifest summary | v 3.67.0 | 7a509c */
static void load_processor_manifest(AppRun *run) {
    char extra[APP_PATH_CAP], selected[APP_PATH_CAP] = "";
    path_join(extra, sizeof(extra), run->dir, "extra");
    DIR *dir = opendir(extra);
    if (!dir) return;
    struct dirent *entry;
    time_t selected_mtime = 0;
    int selected_rank = -1;
    while ((entry = readdir(dir))) {
        if (!strstr(entry->d_name, ".manifest.json")) continue;
        char candidate[APP_PATH_CAP]; struct stat st;
        path_join(candidate, sizeof(candidate), extra, entry->d_name);
        if (stat(candidate, &st)) continue;
        int rank = strstr(entry->d_name, ".partial.") ? 0 : 1;
        if (rank > selected_rank || (rank == selected_rank && st.st_mtime >= selected_mtime)) {
            copy_text(selected, sizeof(selected), candidate); selected_mtime = st.st_mtime; selected_rank = rank;
        }
    }
    closedir(dir);
    if (!selected[0]) { run->processor_summary[0] = 0; run->processor_manifest_path[0] = 0; return; }
    if (selected_mtime == run->manifest_mtime && !strcmp(selected, run->processor_manifest_path)) return;
    copy_text(run->processor_manifest_path, sizeof(run->processor_manifest_path), selected);
    run->manifest_mtime = selected_mtime;
    FILE *file = fopen(selected, "r");
    if (!file) return;
    char line[4096], version[48] = "legacy", digest[80] = "", state[48] = "unknown", revision[32] = "0";
    while (fgets(line, sizeof(line), file)) {
        json_string(line, "processor_version", version, sizeof(version));
        json_string(line, "source_fingerprint", digest, sizeof(digest));
        json_string(line, "run_state", state, sizeof(state));
        json_raw(line, "revision", revision, sizeof(revision));
    }
    fclose(file);
    snprintf(run->processor_summary, sizeof(run->processor_summary),
             "Pixelprocess %s · revision %s · %s · source FNV %.12s",
             version, revision, state, digest[0] ? digest : "not recorded");
}
/* agent: gpt-6 | 2026-10-04 | Read processor manifest summary | v 3.67.0 | 7a509c */

/* agent: gpt-6 | 2026-10-05 | Persist sprite override settings | v 3.88.0 | 6f1785 */
/* Load effective processing defaults from AppRun* run's saved JSON; return void. */
/* agent: gpt-6 | 2026-10-05 | Attach lattice settings to selection | v 3.88.0 | 2f70c2 */
/* Find or create AppRun* run's sprite settings by const char* id; return AppSpriteSettings*. */
static AppSpriteSettings *sprite_settings_for(AppRun *run, const char *id, bool create);

static void load_run_pixel_settings(AppRun *run) {
    if(run->pixel_settings_loaded)return;
    if(!run->pixel_settings.pitch[0])pixel_settings_defaults(&run->pixel_settings);
    run->sprite_settings=run->pixel_settings;
    char path[APP_PATH_CAP],json[APP_TEXT_CAP],value[128];path_join(path,sizeof(path),run->dir,"pixelprocess.settings.json");
    if(!read_text(path,json,sizeof(json))){
        char manifest[APP_PATH_CAP],data[APP_TEXT_CAP];path_join(manifest,sizeof(manifest),run->dir,"extra/pixelprocess.manifest.json");
        if(!read_text(manifest,data,sizeof(data))||!json_string(data,"settings_path",path,sizeof(path))||!read_text(path,json,sizeof(json))){run->pixel_settings_loaded=true;return;}
    }
    AppProfile parsed={0};parsed.pixel=run->pixel_settings;
    const char *strings[]={"pitch","phase_x","phase_y","cleanup_mode","sheet_anchor"};
    for(size_t i=0;i<sizeof(strings)/sizeof(strings[0]);i++)if(json_string(json,strings[i],value,sizeof(value)))set_pixel_profile_field(&parsed,strings[i],value);
    const char *numbers[]={"background_tolerance","grid_cols","grid_rows","extraction_padding","min_pitch","max_pitch","pitch_precision","foreground_coverage","cleanup_tolerance","sheet_padding"};
    const char *fields[]={"background_tolerance","grid_cols","grid_rows","extraction_padding","pitch_min","pitch_max","pitch_precision","foreground_coverage","cleanup_tolerance","sheet_padding"};
    for(size_t i=0;i<sizeof(numbers)/sizeof(numbers[0]);i++)if(json_raw(json,numbers[i],value,sizeof(value))){if(!strcmp(numbers[i],"foreground_coverage")){char *end=NULL;double n=strtod(value,&end);if(end!=value)set_pixel_profile_field(&parsed,fields[i],TextFormat("%d",(int)(n*100.0+.5)));}else set_pixel_profile_field(&parsed,fields[i],value);}
    run->pixel_settings=parsed.pixel;run->sprite_settings=parsed.pixel;
    const char *entry=strstr(json,"\"sprite_overrides\"");
    if(entry&&(entry=strchr(entry,'[')))while((entry=strchr(entry,'{'))&&run->sprite_override_count<APP_MAX_SPRITE_EVIDENCE){
        const char *end=strchr(entry,'}');if(!end)break;char record[512],id[32];size_t n=(size_t)(end-entry+1);if(n>=sizeof(record))break;memcpy(record,entry,n);record[n]=0;
        if(json_string(record,"id",id,sizeof(id))&&*id){AppSpriteSettings *item=sprite_settings_for(run,id,true);if(item){AppProfile override={0};override.pixel=run->pixel_settings;
            const char *keys[]={"pitch","phase_x","phase_y"};for(int k=0;k<3;k++)if(json_string(record,keys[k],value,sizeof(value)))set_pixel_profile_field(&override,keys[k],value);
            item->settings=override.pixel;
        }}
        entry=end+1;if(*entry==']')break;
    }
    copy_text(run->processing_settings_path,sizeof(run->processing_settings_path),path);
    run->pixel_settings_loaded=true;
}
/* agent: gpt-6 | 2026-10-05 | Persist sprite override settings | v 3.88.0 | 6f1785 */

/* Read whether const char* path has a non-thumbnail role in AppRun* run's manifest; return bool. */
/* agent: gpt-6.1-sol | 2026-10-05 | Stream role tagged artifact records | v 3.98.0 | 67400a */
static bool artifact_role_hidden(AppRun *run, const char *path) {
    char manifest[APP_PATH_CAP], line[2048], record[1024];
    path_join(manifest, sizeof(manifest), run->dir, "extra/pixelprocess.manifest.json");
    FILE *file = fopen(manifest, "r");
    if (!file) return false;
    bool in_artifacts = false, in_record = false, hidden = false;
    size_t used = 0;
    record[0] = 0;
    while (fgets(line, sizeof(line), file)) {
        if (!in_artifacts) {
            if (strstr(line, "\"artifacts\"")) in_artifacts = true;
            continue;
        }
        if (!in_record && strchr(line, ']')) break;
        if (!in_record && strchr(line, '{')) { in_record = true; used = 0; record[0] = 0; }
        if (in_record) {
            size_t n = strlen(line);
            if (n >= sizeof(record) - used) { in_record = false; continue; }
            memcpy(record + used, line, n + 1); used += n;
            if (strchr(line, '}')) {
                char item_path[APP_PATH_CAP], role[64];
                if (json_string(record, "path", item_path, sizeof(item_path)) &&
                    json_string(record, "role", role, sizeof(role)) && !strcmp(item_path, path) && !strcmp(role, "sprite_export")) { hidden = true; break; }
                in_record = false;
            }
        }
    }
    fclose(file);
    return hidden;
}
/* Scan run-local images for App* app and AppRun* run; return void. */
/* agent: gpt-6.1-sol | 2026-10-05 | Stream role tagged artifact records | v 3.98.0 | 67400a */
static void scan_artifacts(App *app, AppRun *run) {
    for (int i = 0; i < run->stage_count; i++) unload_artifact(&run->stages[i]);
    for (int i = 0; i < run->debug_stage_count; i++) unload_artifact(&run->debug_stages[i]);
    for (int i = 0; i < run->output_count; i++) unload_artifact(&run->outputs[i]);
    for (int i = 0; i < run->draw_copy_count; i++) unload_artifact(&run->draw_copies[i]);
    run->stage_count = 0; run->debug_stage_count = 0; run->output_count = 0; run->draw_copy_count = 0;
    /* agent: gpt-6.1-sol | 2026-10-04 | Preserve cached run thumbnails | v 3.61.0 | cdfc31 */
    if (!run->generated.texture.id && !run->generated.thumb.id && !run->generated.loading && file_exists(run->source)) load_artifact_image(app, &run->generated, run->source, "Generated image", -1);
    /* agent: gpt-6.1-sol | 2026-10-04 | Preserve cached run thumbnails | v 3.61.0 | cdfc31 */
    char folders[3][APP_PATH_CAP]; copy_text(folders[0],sizeof(folders[0]),run->dir); path_join(folders[1],sizeof(folders[1]),run->dir,"extra"); path_join(folders[2],sizeof(folders[2]),run->dir,"extra/draw-copies");
    for (int folder = 0; folder < 3; folder++) {
        DIR *dir = opendir(folders[folder]); if (!dir) continue;
        struct dirent *de;
        while ((de = readdir(dir))) {
            if (!is_image_path(de->d_name)) continue;
            char path[APP_PATH_CAP]; path_join(path, sizeof(path), folders[folder], de->d_name);
            AppArtifact *a = NULL;
            const char *s = folder == 2 ? NULL : strstr(de->d_name, "stage-");
            const char *debug = folder == 2 ? NULL : strstr(de->d_name, "stage-debug-");
            if (debug && run->debug_stage_count < APP_MAX_STAGES) {
                a=&run->debug_stages[run->debug_stage_count++];memset(a,0,sizeof(*a));a->number=atoi(debug+12);
            } else if (s && run->stage_count < APP_MAX_STAGES) {
                a = &run->stages[run->stage_count++]; memset(a, 0, sizeof(*a)); a->number = atoi(s + 6);
            } else if (folder == 2 && run->draw_copy_count < APP_MAX_OUTPUTS) {
                a = &run->draw_copies[run->draw_copy_count++]; memset(a, 0, sizeof(*a));
            } else if (strcmp(path, run->source) && run->output_count < APP_MAX_OUTPUTS) {
                if (artifact_role_hidden(run, path)) continue;
                a = &run->outputs[run->output_count++]; memset(a, 0, sizeof(*a));
            }
            if (!a) continue;
            char name[128]; copy_text(name, sizeof(name), de->d_name);
            if (!load_artifact_image(app, a, path, name, a->number)) {
                if (debug) run->debug_stage_count--;
                else if (s) run->stage_count--;
                else if (folder == 2) run->draw_copy_count--;
                else run->output_count--;
            }
        }
        closedir(dir);
    }
    qsort(run->stages, (size_t)run->stage_count, sizeof(run->stages[0]), stage_order);
    qsort(run->debug_stages, (size_t)run->debug_stage_count, sizeof(run->debug_stages[0]), stage_order);
    qsort(run->outputs, (size_t)run->output_count, sizeof(run->outputs[0]), artifact_order);
    qsort(run->draw_copies, (size_t)run->draw_copy_count, sizeof(run->draw_copies[0]), artifact_order);
    load_processing_events(run);
    time_t previous_manifest_mtime = run->manifest_mtime;
    load_processor_manifest(run);
    if (previous_manifest_mtime != run->manifest_mtime) clear_sprite_version_previews(run);
    load_run_pixel_settings(run);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Classify copies by dedicated folder | v 3.55.0 | 71bb3c */

/* agent: gpt-6.1-sol | 2026-10-04 | Separate live stage and final artifacts | v 3.37.0 | 99dbe2 */
/* Refresh outputs for AppRun* run through App* app while processing; return void. */
static void refresh_run_outputs(App *app, AppRun *run) {
    char selected_output_path[APP_PATH_CAP] = "", selected_stage_path[APP_PATH_CAP] = "", selected_copy_path[APP_PATH_CAP] = "";
    if (!run->stage_output_selected && run->selected_output >= 0 && run->selected_output < run->output_count)
        copy_text(selected_output_path, sizeof(selected_output_path), run->outputs[run->selected_output].path);
    if (run->stage_output_selected && run->selected_stage >= 0 && run->selected_stage < run->stage_count)
        copy_text(selected_stage_path, sizeof(selected_stage_path), run->stages[run->selected_stage].path);
    if (run->draw_copy_selected && run->selected_draw_copy >= 0 && run->selected_draw_copy < run->draw_copy_count)
        copy_text(selected_copy_path, sizeof(selected_copy_path), run->draw_copies[run->selected_draw_copy].path);
    char folders[3][APP_PATH_CAP];
    copy_text(folders[0], sizeof(folders[0]), run->dir); path_join(folders[1], sizeof(folders[1]), run->dir, "extra"); path_join(folders[2], sizeof(folders[2]), run->dir, "extra/draw-copies");
    for (int folder = 0; folder < 3; folder++) {
        DIR *dir = opendir(folders[folder]); if (!dir) continue;
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (!is_image_path(entry->d_name)) continue;
            char path[APP_PATH_CAP]; path_join(path, sizeof(path), folders[folder], entry->d_name);
            if (!strcmp(path, run->source)) continue;
            struct stat info; if (stat(path, &info) || !S_ISREG(info.st_mode)) continue;
            const char *debug_name = folder == 2 ? NULL : strstr(entry->d_name, "stage-debug-");
            const char *stage_name = folder == 2 || debug_name ? NULL : strstr(entry->d_name, "stage-");
            bool draw_copy = folder == 2;
            AppArtifact *items = debug_name ? run->debug_stages : stage_name ? run->stages : draw_copy ? run->draw_copies : run->outputs;
            int *item_count = debug_name ? &run->debug_stage_count : stage_name ? &run->stage_count : draw_copy ? &run->draw_copy_count : &run->output_count;
            int item_limit = debug_name || stage_name ? APP_MAX_STAGES : APP_MAX_OUTPUTS;
            int found = -1;
            for (int i = 0; i < *item_count; i++) if (!strcmp(items[i].path, path)) { found = i; break; }
            if (found >= 0 && items[found].file_size == info.st_size) continue;
            AppArtifact refreshed = {0};
            if (!load_artifact_image(app, &refreshed, path, entry->d_name, debug_name ? atoi(debug_name+12) : stage_name ? atoi(stage_name + 6) : 0)) continue;
            if (found >= 0) { unload_artifact(&items[found]); items[found] = refreshed; }
            else if (*item_count < item_limit) items[(*item_count)++] = refreshed;
            else unload_artifact(&refreshed);
        }
        closedir(dir);
    }
    qsort(run->outputs, (size_t)run->output_count, sizeof(run->outputs[0]), artifact_order);
    qsort(run->draw_copies, (size_t)run->draw_copy_count, sizeof(run->draw_copies[0]), artifact_order);
    qsort(run->stages, (size_t)run->stage_count, sizeof(run->stages[0]), stage_order);
    qsort(run->debug_stages,(size_t)run->debug_stage_count,sizeof(run->debug_stages[0]),stage_order);
    if (selected_output_path[0]) for (int i = 0; i < run->output_count; i++) if (!strcmp(run->outputs[i].path, selected_output_path)) { run->selected_output = i; break; }
    if (selected_stage_path[0]) for (int i = 0; i < run->stage_count; i++) if (!strcmp(run->stages[i].path, selected_stage_path)) { run->selected_stage = i; break; }
    if (selected_copy_path[0]) for (int i = 0; i < run->draw_copy_count; i++) if (!strcmp(run->draw_copies[i].path, selected_copy_path)) { run->selected_draw_copy = i; break; }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Separate live stage and final artifacts | v 3.37.0 | 99dbe2 */

/* Keep the original generated image and processor artifacts discoverable after exit. */
static void update_run_artifacts(App *app, AppRun *run) {
    scan_artifacts(app, run);
    if (file_exists(run->source) && !strstr(run->source, ".pixel")) copy_text(run->primary, sizeof(run->primary), run->source);
    else if (run->output_count) copy_text(run->primary, sizeof(run->primary), run->outputs[0].path);
    (void)app;
}

/* Find AppArtifact* by const char* path in App* app; return pointer or NULL. */
static AppArtifact *find_artifact(App *app, const char *path) {
    for (int r = 0; r < app->run_count; r++) {
        AppRun *run = &app->runs[r];
        if (!strcmp(run->generated.path, path)) return &run->generated;
        for (int i = 0; i < run->stage_count; i++) if (!strcmp(run->stages[i].path, path)) return &run->stages[i];
        for (int i = 0; i < run->debug_stage_count; i++) if (!strcmp(run->debug_stages[i].path,path)) return &run->debug_stages[i];
        for (int i = 0; i < run->output_count; i++) if (!strcmp(run->outputs[i].path, path)) return &run->outputs[i];
        for (int i = 0; i < run->draw_copy_count; i++) if (!strcmp(run->draw_copies[i].path, path)) return &run->draw_copies[i];
        for (int i = 0; i < run->sprite_version_preview_count; i++) if (!strcmp(run->sprite_version_previews[i].path, path)) return &run->sprite_version_previews[i];
    }
    return NULL;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
/* agent: gpt-6.1-sol | 2026-10-04 | Upload decoded textures on main thread | v 3.55.0 | c09b7f */
/* Consume one image result for App* app on the graphics thread; return void. */
static void image_loader_update(App *app) {
    ImageLoader *loader = app->image_loader;
    if (!loader) return;
    pthread_mutex_lock(&loader->mutex);
    if (!loader->result_ready) { pthread_mutex_unlock(&loader->mutex); return; }
    ImageResult result = loader->result;
    memset(&loader->result, 0, sizeof(loader->result)); loader->result_ready = false;
    pthread_cond_signal(&loader->ready);
    pthread_mutex_unlock(&loader->mutex);

    if (result.logs) {
        for (int r = 0; r < app->run_count; r++) {
            AppRun *run = &app->runs[r];
            char path[APP_PATH_CAP]; path_join(path, sizeof(path), run->dir, "process.log");
            if (strcmp(path, result.path)) continue;
            run->log_count = 0; run->current_step[0] = 0;
            const char *cursor = result.log_data ? result.log_data : "";
            while (*cursor && run->log_count < APP_MAX_LOGS) {
                char line[APP_TEXT_CAP + 128], tm[32] = "", proc[32] = "", message[APP_TEXT_CAP] = "";
                const char *end = strchr(cursor, '\n');
                size_t n = end ? (size_t)(end - cursor) : strlen(cursor);
                if (n >= sizeof(line)) n = sizeof(line) - 1;
                memcpy(line, cursor, n); line[n] = 0;
                if (sscanf(line, "%31s [%31[^]]] %8191[^\n]", tm, proc, message) >= 3) {
                    AppLogLine *entry = &run->logs[run->log_count++];
                    copy_text(entry->time, sizeof(entry->time), tm); copy_text(entry->process, sizeof(entry->process), proc); copy_text(entry->text, sizeof(entry->text), message);
                    update_progress_label(run, proc, message);
                }
                cursor += end ? n + 1 : n;
                if (!end) break;
            }
            run->logs_loaded = true; run->logs_loading = false;
            break;
        }
        free(result.log_data);
        return;
    }
    /* agent: gpt-6.1-sol | 2026-10-04 | Record texture cache use on upload | v 3.61.0 | 5c5b1d */
    if (result.thumb.data || result.image.data) {
        Texture2D full = result.image.data ? LoadTextureFromImage(result.image) : (Texture2D){0};
        Texture2D thumb = result.thumb.data ? LoadTextureFromImage(result.thumb) : (Texture2D){0};
        if (full.id) SetTextureFilter(full, TEXTURE_FILTER_POINT);
        if (thumb.id) SetTextureFilter(thumb, result.pixelated ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
        for (int i = 0; i < app->ref_count; i++) {
            AppReference *ref = &app->refs[i];
            if (!strcmp(ref->path, result.path)) {
                if (full.id) ref->full = full;
                bool keep_thumb = !ref->thumb.id;
                if (keep_thumb) ref->thumb = thumb; else if (thumb.id) UnloadTexture(thumb);
                ref->width = result.width; ref->height = result.height;
                if (full.id) ref->full_used_at = GetTime();
                if (keep_thumb && thumb.id) ref->thumb_used_at = GetTime();
                ref->loading = image_loader_has_path(app, result.path);
                full = (Texture2D){0}; thumb = (Texture2D){0};
                break;
            }
        }
        AppArtifact *artifact = find_artifact(app, result.path);
        if (artifact) {
            if (full.id) artifact->texture = full;
            bool keep_thumb = !artifact->thumb.id;
            if (keep_thumb) artifact->thumb = thumb; else if (thumb.id) UnloadTexture(thumb);
            artifact->width = result.width; artifact->height = result.height;
            if (full.id) artifact->full_used_at = GetTime();
            if (keep_thumb && thumb.id) artifact->thumb_used_at = GetTime();
            artifact->loading = image_loader_has_path(app, result.path);
            full = (Texture2D){0}; thumb = (Texture2D){0};
        }
        if (full.id) UnloadTexture(full);
        if (thumb.id) UnloadTexture(thumb);
    } else {
        for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, result.path)) app->refs[i].loading = false;
        AppArtifact *artifact = find_artifact(app, result.path);
        if (artifact) artifact->loading = false;
    }
    if (result.image.data) UnloadImage(result.image);
    if (result.thumb.data) UnloadImage(result.thumb);
    /* agent: gpt-6.1-sol | 2026-10-04 | Record texture cache use on upload | v 3.61.0 | 5c5b1d */
}

/* Return whether AppRun* run contains any successfully loaded image; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
static bool run_has_image(const AppRun *run) {
    return run->generated.texture.id || run->generated.thumb.id || run->output_count || run->draw_copy_count || run->stage_count;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */

/* Return whether AppRun* run has a loaded image for the viewer; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */
/* Return whether AppRun* run has any viewable image, including processor stages; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Expose stage-only runs in Controls | v 3.57.0 | 440367 */
static bool run_has_viewable_image(const AppRun *run) {
    if (!run) return false;
    if (run->generated.texture.id || run->generated.thumb.id || run->generated.loading) return true;
    for (int i = 0; i < run->output_count; i++)
        if (run->outputs[i].texture.id || run->outputs[i].thumb.id || run->outputs[i].loading) return true;
    for (int i = 0; i < run->draw_copy_count; i++)
        if (run->draw_copies[i].texture.id || run->draw_copies[i].thumb.id || run->draw_copies[i].loading) return true;
    for (int i = 0; i < run->stage_count; i++)
        if (run->stages[i].texture.id || run->stages[i].thumb.id || run->stages[i].loading) return true;
    return false;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Expose stage-only runs in Controls | v 3.57.0 | 440367 */
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */

/* Return the first loaded pixel output, or original image, for AppRun* run; return int. */
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */
static int run_default_image_index(const AppRun *run) {
    if (!run) return -1;
    for (int i = 0; i < run->output_count; i++)
        if (run->outputs[i].texture.id || run->outputs[i].thumb.id) return i;
    return -1;
}

/* Return the currently selected source, stage, output, or edited copy for AppRun* run; return artifact or NULL. */
static AppArtifact *run_selected_artifact(AppRun *run) {
    if (!run) return NULL;
    if (run->draw_copy_selected && run->selected_draw_copy >= 0 && run->selected_draw_copy < run->draw_copy_count)
        return &run->draw_copies[run->selected_draw_copy];
    if (run->stage_output_selected && run->selected_stage >= 0 && run->selected_stage < run->stage_count) {
        if(run->selected_stage_debug)for(int i=0;i<run->debug_stage_count;i++)if(run->debug_stages[i].number==run->stages[run->selected_stage].number)return &run->debug_stages[i];
        return &run->stages[run->selected_stage];
    }
    int choice = run->selected_output == -2 ? run_default_image_index(run) : run->selected_output;
    if (choice >= run->output_count || (choice >= 0 && !run->outputs[choice].texture.id && !run->outputs[choice].thumb.id))
        choice = run_default_image_index(run);
    return choice < 0 ? &run->generated : &run->outputs[choice];
}
/* agent: gpt-6.1-sol | 2026-10-04 | Classify copies by dedicated folder | v 3.55.0 | 71bb3c */
/* agent: gpt-6.1-sol | 2026-10-04 | Detect viewable generation images | v 3.35.0 | 3c3fad */

/* Load App* app's run folders, queue images, and read MCP logs; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
/* agent: gpt-6.1-sol | 2026-10-04 | Add generation run controls | v 3.53.0 | 522f7a */
/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
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
        char metadata_path[APP_PATH_CAP],metadata[APP_TEXT_CAP];path_join(metadata_path,sizeof(metadata_path),path,"metadata.txt");
        /* agent: gpt-6.1-sol | 2026-10-05 | Preserve run metadata parsing | v 3.68.0 | 1304a1 */
        if (read_text(metadata_path, metadata, sizeof(metadata))) {
            char *input = strstr(metadata, "Input image: ");
            if (input) {
                input += 13;
                char *end = strchr(input, '\n');
                if (end) *end = 0;
                if (file_exists(input)) copy_text(run->source, sizeof(run->source), input);
                if (end) *end = '\n';
            }
            char *resources = strstr(metadata, "Resources used (");
            if (resources && (resources = strchr(resources, '\n'))) {
                resources++;
                while (*resources && run->used_resource_count < APP_MAX_REFS) {
                    char *end = strchr(resources, '\n');
                    if (!end) break;
                    *end = 0;
                    if (strncmp(resources, "  ", 2)) { *end = '\n'; break; }
                    copy_text(run->used_resources[run->used_resource_count++], APP_PATH_CAP, resources + 2);
                    *end = '\n';
                    resources = end + 1;
                }
            }
            char *prompt = strstr(metadata, "Prompt:\n");
            if (prompt) copy_text(run->prompt, sizeof(run->prompt), prompt + 8);
        }
        /* agent: gpt-6.1-sol | 2026-10-05 | Preserve run metadata parsing | v 3.68.0 | 1304a1 */
        if (s_mcp_mode) {
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
            run->logs_loaded = true;
        }
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
/* agent: gpt-6.1-sol | 2026-10-04 | Add generation run controls | v 3.53.0 | 522f7a */
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
        char runner[APP_PATH_CAP];
        if (!resolve_pixel_runner(runner,sizeof(runner))) { run->state=RUN_SUCCEEDED;run->finished=GetTime();copy_text(run->current_step,sizeof(run->current_step),"image ready · pixel processor unavailable");set_message(app,"Generation complete; pixel processor unavailable"); }
        else if (!start_pixel(app, run)) { run->state = RUN_FAILED; run->finished = GetTime(); if (!*run->error) copy_text(run->error, sizeof(run->error), "Could not start pixel processor"); }
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
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* Initialize App* app with const char *workspace and bool mcp_mode; migrate workspace resources; return bool. */
/* agent: gpt-6.1-sol | 2026-10-04 | Initialize resource workspace migration | v 3.55.0 | 8ca4fb */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
/* agent: gpt-6.1-sol | 2026-10-04 | Wire queue through app lifecycle | v 3.55.0 | d0a0dc */
bool AppInit(App *app, const char *workspace, bool mcp_mode) {
    memset(app, 0, sizeof(*app));
    s_mcp_mode = mcp_mode;
    app->view = VIEW_HOME; app->thumb_size = 104; app->image_zoom = 1.0f;
    app->enet_port = 27881; app->selected_ref = 0; app->active_run = -1; app->selected_run = -1;
    app->runs[0].pipe_fd = -1; app->runs[0].err_fd = -1;
    copy_text(app->workspace, sizeof(app->workspace), workspace && *workspace ? workspace : "assets");
    if (!make_dirs(app->workspace)) { snprintf(app->message, sizeof(app->message), "Cannot create workspace: %.*s", (int)sizeof(app->message)-32, app->workspace); return false; }
    char absolute_workspace[APP_PATH_CAP];
    if (realpath(app->workspace, absolute_workspace)) copy_text(app->workspace, sizeof(app->workspace), absolute_workspace);
    if (!mcp_mode) image_loader_start(app);
    if (!mcp_mode) {
        s_ui_font = (Font){0};
        path_join(app->font_dir, sizeof(app->font_dir), GetApplicationDirectory(), "assets/fonts");
        make_dirs(app->font_dir);
        path_join(app->font_state_path, sizeof(app->font_state_path), app->workspace, "ui-font.path");
        scan_asset_fonts(app);
        load_saved_ui_font(app);
    }
    path_join(app->refs_path, sizeof(app->refs_path), app->workspace, "resources"); make_dirs(app->refs_path);
    path_join(app->runs_path, sizeof(app->runs_path), app->workspace, "runs"); make_dirs(app->runs_path);
    path_join(app->profiles_path, sizeof(app->profiles_path), app->workspace, "profiles.ini");
    path_join(app->active_profile_path,sizeof(app->active_profile_path),app->workspace,"active-profile.txt");
    path_join(app->refs_state_path, sizeof(app->refs_state_path), app->workspace, "references.ini");
    bool refs_flattened = migrate_legacy_refs(app) | flatten_reference_groups(app);
    char child[APP_PATH_CAP];
    path_join(child, sizeof(child), app->workspace, "extra"); make_dirs(child);
    load_profiles(app); seed_profile(app); scan_refs(app);
    if (refs_flattened) save_reference_state(app);
    s_image_shader = LoadShader("shaders.vs", "shaders.fs");
    load_run_history(app);
    app->selected_ref = 0; app->last_enet_fetch = GetTime();
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
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* agent: gpt-6.1-sol | 2026-10-04 | Initialize resource workspace migration | v 3.55.0 | 8ca4fb */
/* agent: gpt-6.1-sol | 2026-10-04 | Keep references in the workspace root | v 3.15.0 | 26269f */
/* agent: gpt-6.1-sol | 2026-10-04 | Load UI font for desktop app | v 3.3.0 | a9fe70 */

/* agent: gpt-6.1-sol | 2026-10-04 | Release loaded UI font | v 3.3.0 | 0bf8e7 */
/* agent: gpt-6.1-sol | 2026-10-04 | Release active drawing session | v 3.50.0 | 90dc18 */
/* Release resources owned by App* app; return void. */
void AppShutdown(App *app) {
    image_loader_stop(app);
    draw_session_free((DrawState *)app->draw_state);
    app->draw_state = NULL;
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
/* agent: gpt-6.1-sol | 2026-10-04 | Release active drawing session | v 3.50.0 | 90dc18 */
/* agent: gpt-6.1-sol | 2026-10-04 | Release loaded UI font | v 3.3.0 | 0bf8e7 */

/* Return the currently selected run when one exists. */
static AppRun *get_run(App *app) {
    if (app->selected_run >= 0 && app->selected_run < app->run_count) return &app->runs[app->selected_run];
    if (app->active_run >= 0 && app->active_run < app->run_count) return &app->runs[app->active_run];
    return NULL;
}

/* Find or create AppRun* run's sprite settings by const char* id; return AppSpriteSettings*. */
static AppSpriteSettings *sprite_settings_for(AppRun *run, const char *id, bool create) {
    for (int i = 0; i < run->sprite_override_count; i++)
        if (!strcmp(run->sprite_overrides[i].id, id)) return &run->sprite_overrides[i];
    if (!create || run->sprite_override_count >= APP_MAX_SPRITE_EVIDENCE) return NULL;
    AppSpriteSettings *item = &run->sprite_overrides[run->sprite_override_count++];
    memset(item, 0, sizeof(*item));
    copy_text(item->id, sizeof(item->id), id);
    item->settings = run->pixel_settings;
    return item;
}
/* agent: gpt-6 | 2026-10-05 | Attach lattice settings to selection | v 3.88.0 | 2f70c2 */

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
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* agent: gpt-6.1-sol | 2026-10-04 | Expose resource actions and tools | v 3.55.0 | 5d23c1 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route reference trash actions | v 3.3.0 | e1c824 */
/* Apply char* request via App* app, write char* response with size_t response_size; return const char*. */
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Write generic metadata for every generation | v 3.19.0 | 6c0bb8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route generation image and logs controls | v 3.35.0 | 94dd5b */
/* agent: gpt-6.1-sol | 2026-10-04 | Block navigation during drawing | v 3.50.0 | cbcdc5 */
const char *AppDispatch(App *app, const char *request, char *response, size_t response_size) {
    if (response_size) response[0] = 0;
    char action[64] = "", value[APP_TEXT_CAP] = "", path[APP_PATH_CAP] = "", destination[APP_PATH_CAP] = "";
    char detail[32] = "current";
    json_string(request, "action", action, sizeof(action));
    json_string(request, "value", value, sizeof(value));
    json_string(request, "path", path, sizeof(path));
    json_string(request, "destination", destination, sizeof(destination));
    json_string(request, "detail", detail, sizeof(detail));
    if (app->draw_state && (!strcmp(action,"view") || !strcmp(action,"resource.view") || !strcmp(action,"resource.select") || !strcmp(action,"resource.exclude") || !strcmp(action,"resource.trash") || !strcmp(action,"profile.select") || !strcmp(action,"profile.delete") || !strcmp(action,"reference.view") || !strcmp(action,"reference.trash") || !strcmp(action,"reference.delete") || !strcmp(action,"run.select") || !strcmp(action,"run.trash") || !strcmp(action,"output.select") || !strcmp(action,"stage.select"))) {
        snprintf(response, response_size, "{\"ok\":false,\"error\":\"finish or cancel draw mode first\"}");
        return response;
    }
    /* agent: gpt-6 | 2026-10-04 | Queue secure workspace screenshot capture | v 3.67.0 | e2576a */
    if (!strcmp(action, "ui.screenshot")) {
        char screenshots[APP_PATH_CAP], target[APP_PATH_CAP], root_real[APP_PATH_CAP], dir_real[APP_PATH_CAP];
        const char *name = !strncmp(path, "screenshots/", 12) ? path + 12 : path;
        struct stat existing; const char *extension = strrchr(name, '.');
        bool safe_name = *name && !strstr(name, "..") && !strchr(name, '/') && !strchr(name, '\\') && extension && !strcmp(extension, ".png") && strlen(name) < 180;
        path_join(screenshots, sizeof(screenshots), app->workspace, "screenshots");
        path_join(target, sizeof(target), screenshots, name);
        if (!safe_name || app->screenshot_pending || (!strcmp(detail, "expanded") && !get_run(app)) || !make_dirs(screenshots) || !realpath(app->workspace, root_real) || !realpath(screenshots, dir_real) || strncmp(dir_real, root_real, strlen(root_real)) || (dir_real[strlen(root_real)] && dir_real[strlen(root_real)] != '/') || (lstat(target, &existing) == 0)) {
            snprintf(response, response_size, "{\"ok\":false,\"error\":\"invalid screenshot path, active capture, or target already exists\"}");
        } else {
            copy_text(app->screenshot_path, sizeof(app->screenshot_path), target);
            copy_text(app->screenshot_view, sizeof(app->screenshot_view), app->view == VIEW_GENERATION ? "generation" : app->view == VIEW_LIBRARY ? "library" : "home");
            AppRun *shot_run = get_run(app);
            copy_text(app->screenshot_run_id, sizeof(app->screenshot_run_id), shot_run ? shot_run->id : "");
            AppArtifact *shot_artifact = shot_run ? run_selected_artifact(shot_run) : NULL;
            copy_text(app->screenshot_artifact, sizeof(app->screenshot_artifact), shot_artifact ? shot_artifact->name : "");
            copy_text(app->screenshot_detail, sizeof(app->screenshot_detail), detail);
            app->screenshot_previous_expanded = app->screenshot_expanded;
            app->screenshot_expanded = !strcmp(detail, "expanded");
            app->screenshot_pending = true; app->screenshot_error[0] = 0;
            char escaped[APP_PATH_CAP * 2]; json_escape(target, escaped, sizeof(escaped));
            snprintf(response, response_size, "{\"ok\":true,\"pending\":true,\"path\":\"%s\"}", escaped);
        }
    } else if (!strcmp(action, "ui.screenshot.status")) {
        char escaped[APP_PATH_CAP * 2], error[512]; json_escape(app->screenshot_path, escaped, sizeof(escaped)); json_escape(app->screenshot_error, error, sizeof(error));
        char run_id[128], artifact[512], view[64], detail_mode[64];
        json_escape(app->screenshot_run_id, run_id, sizeof(run_id)); json_escape(app->screenshot_artifact, artifact, sizeof(artifact));
        json_escape(app->screenshot_view, view, sizeof(view)); json_escape(app->screenshot_detail, detail_mode, sizeof(detail_mode));
        snprintf(response, response_size, "{\"ok\":%s,\"pending\":%s,\"path\":\"%s\",\"error\":\"%s\",\"view\":\"%s\",\"run\":\"%s\",\"artifact\":\"%s\",\"detail\":\"%s\"}", app->screenshot_error[0] ? "false" : "true", app->screenshot_pending ? "true" : "false", escaped, error, view, run_id, artifact, detail_mode);
    } else if (!strcmp(action, "state") || !strcmp(action, "status")) state_json(app, response, response_size);
    /* agent: gpt-6 | 2026-10-04 | Queue secure workspace screenshot capture | v 3.67.0 | e2576a */
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
    } else if (!strcmp(action, "resources.list")) {
        size_t n=(size_t)snprintf(response,response_size,"{\"resources\":[");
        for(int i=0;i<app->ref_count&&n<response_size;i++) {
            AppReference *r=&app->refs[i]; char name[512],file[APP_PATH_CAP*2];
            json_escape(r->name,name,sizeof(name));json_escape(r->path,file,sizeof(file));
            const char *type=r->is_profile?"profile":r->is_text?"text":"image";
            n+=(size_t)snprintf(response+n,response_size-n,"%s{\"name\":\"%s\",\"path\":\"%s\",\"type\":\"%s\",\"enabled\":%s,\"excluded\":%s}",i?",":"",name,file,type,r->is_profile?(r->profile_index==app->active_profile?"true":"false"):(r->excluded?"false":"true"),r->excluded?"true":"false");
        }
        if(n>=response_size)snprintf(response,response_size,"{\"ok\":false,\"error\":\"resource list exceeds response capacity\"}");
        else snprintf(response+n,response_size-n,"]}");
    } else if (!strcmp(action, "references.list")) {
        size_t n = (size_t)snprintf(response, response_size, "{\"references\":[");
        int listed=0;
        for (int i = 0; i < app->ref_count && n < response_size; i++) {
            if(app->refs[i].is_profile)continue;
            char name[512], file[APP_PATH_CAP * 2]; json_escape(app->refs[i].name, name, sizeof(name)); json_escape(app->refs[i].path, file, sizeof(file));
            n += (size_t)snprintf(response + n, response_size - n, "%s{\"name\":\"%s\",\"path\":\"%s\",\"excluded\":%s,\"selected\":%s}", listed++ ? "," : "", name, file, app->refs[i].excluded ? "true" : "false", app->refs[i].selected ? "true" : "false");
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
        if(app->profile_text_edit) { copy_text(app->prompt_edit,sizeof(app->prompt_edit),prompt); app->prompt_dirty=true; if(save_open_prompt(app)){app->prompt_editor_focused=app->prompt_close_pending=false;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"invalid profile text\"}"); }
        else if (app->prompt_path[0] && write_text(app->prompt_path, prompt)) { copy_text(app->prompt_edit, sizeof(app->prompt_edit), prompt); app->prompt_dirty = app->prompt_editor_focused = app->prompt_save_error = app->prompt_limit_error = app->prompt_close_pending = false; app->prompt_caret=app->prompt_anchor=(int)strlen(app->prompt_edit); set_message(app, "Text resource saved"); snprintf(response, response_size, "{\"ok\":true}"); }
        else { snprintf(response, response_size, "{\"ok\":false,\"error\":\"text resource save failed\"}"); }
    } else if (!strcmp(action,"prompt.open")) {
        char resolved[APP_PATH_CAP];
        if (app->prompt_dirty) snprintf(response,response_size,"{\"ok\":false,\"error\":\"save prompt edits before switching files\"}");
        else if (!workspace_file(app,path,resolved,sizeof(resolved)) || !strstr(resolved,".txt")) snprintf(response,response_size,"{\"ok\":false,\"error\":\"prompt must be a text file inside the workspace\"}");
        else {
            copy_text(app->prompt_path,sizeof(app->prompt_path),resolved); app->profile_text_edit=false; app->prompt_dirty=app->prompt_editor_focused=app->prompt_save_error=app->prompt_limit_error=app->prompt_close_pending=false; app->prompt_scroll=0;
            if (!read_text(app->prompt_path,app->prompt_edit,sizeof(app->prompt_edit))) app->prompt_edit[0]=0;
            app->prompt_caret=app->prompt_anchor=(int)strlen(app->prompt_edit);
            set_message(app,"Prompt opened for editing"); snprintf(response,response_size,"{\"ok\":true}");
        }
    } else if (!strcmp(action, "generation.preview")) {
        if (app->active_profile < 0 || app->active_profile >= app->profile_count) snprintf(response, response_size, "{\"ok\":false,\"error\":\"no profile\"}");
        else {
            char ep[APP_TEXT_CAP * 2], model[256], output[APP_PATH_CAP], escaped_output[APP_PATH_CAP * 2];
            app->snapshot_profile = app->profiles[app->active_profile];
            build_prompt(app, app->snapshot_prompt, sizeof(app->snapshot_prompt));
            app->snapshot_ref_count = 0;
            for (int i = 0; i < app->ref_count && app->snapshot_ref_count < APP_MAX_REFS; i++)
                if (!app->refs[i].is_text && !app->refs[i].is_profile && app->refs[i].selected && !app->refs[i].excluded)
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
            char profile_name[160], pitch[64], cleanup[64], anchor[64];
            json_escape(app->snapshot_profile.name,profile_name,sizeof(profile_name));
            json_escape(app->snapshot_profile.pixel.pitch,pitch,sizeof(pitch));
            json_escape(app->snapshot_profile.pixel.cleanup_mode,cleanup,sizeof(cleanup));
            json_escape(app->snapshot_profile.pixel.sheet_anchor,anchor,sizeof(anchor));
            int n=snprintf(response,response_size,"{\"ok\":true,\"confirmation_required\":true,\"profile\":\"%s\",\"prompt\":\"%s\",\"prompt_files\":[",profile_name,ep);
            bool first_prompt_file=true;
            for(int i=0;i<app->ref_count&&(size_t)n<response_size;i++) if(app->refs[i].is_text&&!app->refs[i].excluded){char source[APP_PATH_CAP*2];json_escape(app->refs[i].path,source,sizeof(source));n+=snprintf(response+n,response_size-(size_t)n,"%s\"%s\"",first_prompt_file?"":",",source);first_prompt_file=false;}
            if((size_t)n<response_size)n+=snprintf(response+n,response_size-(size_t)n,"],\"additional_prompt_count\":%d,\"references\":[",app->snapshot_profile.prompt_count);
            for(int i=0;i<app->snapshot_ref_count&&(size_t)n<response_size;i++){char ref[APP_PATH_CAP*2];json_escape(app->snapshot_refs[i],ref,sizeof(ref));n+=snprintf(response+n,response_size-(size_t)n,"%s\"%s\"",i?",":"",ref);}
            if((size_t)n<response_size)n+=snprintf(response+n,response_size-(size_t)n,"],\"resources\":[");
            bool first_resource=true;
            for(int i=0;i<app->ref_count&&(size_t)n<response_size;i++) {
                AppReference *resource=&app->refs[i];
                if(resource->is_profile||resource->excluded||(resource->is_text?!resource->path[0]:!resource->selected))continue;
                char resource_path[APP_PATH_CAP*2];json_escape(resource->path,resource_path,sizeof(resource_path));
                n+=snprintf(response+n,response_size-(size_t)n,"%s\"%s\"",first_resource?"":",",resource_path);first_resource=false;
            }
            AppPixelSettings *px=&app->snapshot_profile.pixel;
            if((size_t)n<response_size)n+=snprintf(response+n,response_size-(size_t)n,"],\"generation_options\":{\"model\":\"%s\",\"aspect\":\"%s\",\"set_count\":%d},\"postprocessing\":{\"background_tolerance\":%d,\"grid_cols\":%d,\"grid_rows\":%d,\"extraction_padding\":%d,\"pitch\":\"%s\",\"foreground_coverage_percent\":%d,\"cleanup_mode\":\"%s\",\"sheet_anchor\":\"%s\",\"sheet_padding\":%d},\"output_path\":\"%s\"}",model,app->snapshot_profile.aspect,app->snapshot_profile.set_count,px->background_tolerance,px->grid_cols,px->grid_rows,px->extraction_padding,pitch,px->foreground_coverage,cleanup,anchor,px->sheet_padding,escaped_output);
        }
    } else if (!strcmp(action, "generation.confirm") || !strcmp(action, "app.confirm")) {
        /* agent: gpt-6.1-sol | 2026-10-05 | Guard cleanup and add in-place pixel rerun | v 3.87.0 | 81ffd0 */
        if (!strcmp(app->pending_action, "run.stages.clear")) {
            AppRun *run=get_run(app); int removed=0;
            if(run&&run->state!=RUN_RUNNING){char extra[APP_PATH_CAP];path_join(extra,sizeof(extra),run->dir,"extra");DIR *dir=opendir(extra);if(dir){struct dirent *entry;while((entry=readdir(dir))){if(!strstr(entry->d_name,"stage-")||!is_image_path(entry->d_name))continue;char file[APP_PATH_CAP];path_join(file,sizeof(file),extra,entry->d_name);if(!unlink(file))removed++;}closedir(dir);}scan_artifacts(app,run);}
            app->pending_action[0]=0; set_message(app, TextFormat("Removed %d stage images",removed)); snprintf(response,response_size,"{\"ok\":%s,\"removed\":%d}",run&&run->state!=RUN_RUNNING?"true":"false",removed); return response;
        }
        if (!strcmp(app->pending_action, "run.pixel.inplace")) {
            AppRun *run=get_run(app); app->pending_action[0]=0;
            if(!run||run->state==RUN_RUNNING||!file_exists(run->source)) snprintf(response,response_size,"{\"ok\":false,\"error\":\"run or source is unavailable\"}");
            else { run->pixel_settings_saved=run->pixel_settings; run->pixel_settings_dirty=false; run->state=RUN_RUNNING; app->active_run=app->selected_run; if(start_pixel(app,run)){set_message(app,"Pixel rerun started in this run");snprintf(response,response_size,"{\"ok\":true}");}else{run->state=RUN_FAILED;run->finished=GetTime();app->active_run=-1;snprintf(response,response_size,"{\"ok\":false,\"error\":\"%s\"}",run->error);} }
            return response;
        }
        /* agent: gpt-6.1-sol | 2026-10-05 | Guard cleanup and add in-place pixel rerun | v 3.87.0 | 81ffd0 */
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
            else { app->pending_action[0] = app->pending_path[0] = 0; set_message(app, "Could not move resource to trash"); snprintf(response, response_size, "{\"ok\":false,\"error\":\"move to trash failed\"}"); }
            return response;
        }
        if (!strcmp(app->pending_action, "profile.delete")) {
            if (app->profile_count <= 1) snprintf(response, response_size, "{\"ok\":false,\"error\":\"keep at least one profile\"}");
            else { char removed[APP_PATH_CAP]; copy_text(removed,sizeof(removed),app->profiles[app->active_profile].path); if(!move_reference_to_trash(app,removed)){snprintf(response,response_size,"{\"ok\":false,\"error\":\"could not move profile to trash\"}");}else{for (int i = app->active_profile; i + 1 < app->profile_count; i++) app->profiles[i] = app->profiles[i + 1]; app->profile_count--; app->active_profile = 0; save_profiles(app); scan_refs(app); app->selected_ref=-1; app->show_full_image=false; snprintf(response, response_size, "{\"ok\":true}");} }
            app->pending_action[0] = 0; return response;
        }
        if (!app->preview_ready) { snprintf(response, response_size, "{\"ok\":false,\"error\":\"prepare a preview before confirming\"}"); return response; }
        if (app->run_count >= APP_MAX_RUNS || app->active_run >= 0) snprintf(response, response_size, "{\"ok\":false,\"error\":\"run active or run limit reached\"}");
        else {
            AppRun *run = &app->runs[app->run_count]; memset(run, 0, sizeof(*run)); run->pipe_fd = run->err_fd = -1; run->selected_output = -2;
            run->pixel_settings=app->snapshot_profile.pixel;run->pixel_settings_saved=run->pixel_settings;run->sprite_settings=run->pixel_settings;
            copy_text(run->id, sizeof(run->id), app->snapshot_run_id); copy_text(run->dir, sizeof(run->dir), app->snapshot_run_dir); make_dirs(run->dir);
            copy_text(run->prompt, sizeof(run->prompt), app->snapshot_prompt); copy_text(run->resolved_prompt, sizeof(run->resolved_prompt), app->snapshot_resolved);
            AppRun *rerun_parent=get_run(app);
            if(app->rerun_source[0]&&rerun_parent){run->used_resource_count=rerun_parent->used_resource_count;memcpy(run->used_resources,rerun_parent->used_resources,sizeof(run->used_resources));}
            else for(int i=0;i<app->ref_count&&run->used_resource_count<APP_MAX_REFS;i++) if(!app->refs[i].excluded&&!app->refs[i].is_profile&&(app->refs[i].is_text||app->refs[i].selected)) copy_text(run->used_resources[run->used_resource_count++],APP_PATH_CAP,app->refs[i].path);
            run->follow_log = true;
            bool started = false;
            bool pixel_only = app->rerun_pixel_only && *app->rerun_source;
        if(pixel_only&&rerun_parent){
                if(!rerun_parent->pixel_settings_dirty)run->pixel_settings=rerun_parent->pixel_settings;
                run->pixel_settings_saved=run->pixel_settings;run->sprite_settings=rerun_parent->sprite_settings;run->sprite_settings_override=rerun_parent->sprite_settings_override;
                run->sprite_override_count=rerun_parent->sprite_override_count;memcpy(run->sprite_overrides,rerun_parent->sprite_overrides,sizeof(run->sprite_overrides));
                run->pixel_settings_loaded=true;
                run->owner_override_count=rerun_parent->owner_override_count;memcpy(run->owner_components,rerun_parent->owner_components,sizeof(run->owner_components));memcpy(run->owner_sprites,rerun_parent->owner_sprites,sizeof(run->owner_sprites));
                copy_text(run->selected_sprite_id,sizeof(run->selected_sprite_id),rerun_parent->selected_sprite_id);
                copy_text(run->pixel_target,sizeof(run->pixel_target),rerun_parent->pixel_target);
            }
            bool source_ready = true;
            if (pixel_only) {
                char source_path[APP_PATH_CAP];
                path_join(source_path, sizeof(source_path), run->dir, "source.png");
                source_ready = copy_file_new(app->rerun_source, source_path);
                if (source_ready) copy_text(run->source, sizeof(run->source), source_path);
                else { copy_text(run->error, sizeof(run->error), "could not copy pixel rerun input into the new run"); rmdir(run->dir); }
            }
            bool metadata_written = source_ready && write_run_metadata(app, run, pixel_only);
            if (source_ready && pixel_only) {
                run->started = GetTime(); run->state = RUN_RUNNING;
                started = start_pixel(app, run);
            } else if (source_ready) started = start_generation(app, run);
            app->rerun_pixel_only = false; app->rerun_source[0] = 0; app->preview_ready = false;
            if (started) { app->active_run = app->run_count; app->selected_run = app->run_count++; app->view = VIEW_HOME; app->settings_view = false; set_message(app, metadata_written ? "Run started" : "Run started; metadata could not be saved"); snprintf(response, response_size, "{\"ok\":true,\"run\":\"%s\"}", run->id); }
            else { run->state = RUN_FAILED; run->finished = GetTime(); if (!*run->error) copy_text(run->error, sizeof(run->error), "could not start configured process"); snprintf(response, response_size, "{\"ok\":false,\"error\":\"%s\"}", run->error); }
        }
    } else if (!strcmp(action, "app.cancel")) {
        if (!strcmp(app->pending_action,"run.stages.clear")) { app->pending_action[0]=0; set_message(app,"Stage cleanup canceled"); }
        if (!strcmp(app->pending_action,"run.pixel.inplace")) { app->pending_action[0]=0; set_message(app,"Pixel rerun canceled"); }
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
    } else if (!strcmp(action, "resource.exclude") || !strcmp(action, "resource.trash") || !strcmp(action, "reference.exclude") || !strcmp(action, "reference.trash") || !strcmp(action, "reference.delete")) {
        json_string(request, "path", path, sizeof(path));
        for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, path)) {
            if (!strcmp(action, "resource.exclude") && app->refs[i].is_profile) set_active_profile(app,app->refs[i].profile_index);
            else if (!strcmp(action, "resource.exclude") || !strcmp(action, "reference.exclude")) app->refs[i].excluded = !app->refs[i].excluded;
            else if(app->refs[i].is_profile) { if(app->profile_count<=1){snprintf(response,response_size,"{\"ok\":false,\"error\":\"keep at least one profile\"}");return response;} set_active_profile(app,app->refs[i].profile_index);copy_text(app->pending_action,sizeof(app->pending_action),"profile.delete");set_message(app,"Move profile resource to trash pending; confirm or cancel");snprintf(response,response_size,"{\"ok\":true,\"confirmation_required\":true}");return response; }
            else { copy_text(app->pending_action, sizeof(app->pending_action), "reference.trash"); copy_text(app->pending_path, sizeof(app->pending_path), path); set_message(app, "Move resource to trash pending; confirm or cancel"); snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}"); return response; }
            save_reference_state(app); set_message(app, "Resource updated"); snprintf(response, response_size, "{\"ok\":true}"); return response;
        }
        snprintf(response, response_size, "{\"ok\":false,\"error\":\"reference not found\"}");
    } else if (!strcmp(action,"resource.select") || !strcmp(action,"resource.view") || !strcmp(action,"reference.select") || !strcmp(action,"reference.view")) {
        bool found=false;
        for(int i=0;i<app->ref_count;i++) if(!strcmp(path,app->refs[i].path)){found=true;if(!strcmp(action,"resource.select")||!strcmp(action,"reference.select")){app->selected_ref=i;if(app->refs[i].is_profile)set_active_profile(app,app->refs[i].profile_index);else app->refs[i].selected=!app->refs[i].selected;}else{if(app->refs[i].is_profile){if(!open_profile_text(app,app->refs[i].profile_index))break;}else if(app->refs[i].is_text&&!select_prompt_reference(app,&app->refs[i]))break;app->selected_ref=i;app->view=VIEW_LIBRARY;app->show_full_image=true;app->image_zoom=1.0f;}}
        snprintf(response,response_size,found?"{\"ok\":true}":"{\"ok\":false,\"error\":\"reference not found\"}");
    } else if (!strcmp(action,"references.size")) {
        int size=atoi(value); if(size>=56&&size<=216)app->thumb_size=size; snprintf(response,response_size,"{\"ok\":true}");
    } else if (!strcmp(action, "reference.import")) {
        if (!*path || !import_reference_file(app,path)) snprintf(response, response_size, "{\"ok\":false,\"error\":\"reference import failed\"}");
        else { save_reference_state(app); set_message(app, "Resource copied into workspace"); snprintf(response, response_size, "{\"ok\":true}"); }
    } else if (!strcmp(action,"pixel.convert")) {
        char source[APP_PATH_CAP];
        if(app->active_run>=0||app->run_count>=APP_MAX_RUNS)snprintf(response,response_size,"{\"ok\":false,\"error\":\"another run is active or the run list is full\"}");
        else if(!*path||!is_image_path(path)||!realpath(path,source)||!file_exists(source))snprintf(response,response_size,"{\"ok\":false,\"error\":\"select a readable image file\"}");
        else if(!resolve_pixel_runner((char[APP_PATH_CAP]){0},APP_PATH_CAP))snprintf(response,response_size,"{\"ok\":false,\"error\":\"pixel processor unavailable; configure AIGEN_PIXELPROCESS_DIR or AIGEN_PIXELPROCESS_RUNNER\"}");
        else if(app->active_profile<0||app->active_profile>=app->profile_count)snprintf(response,response_size,"{\"ok\":false,\"error\":\"no active profile defaults\"}");
        else {
            AppRun *run=&app->runs[app->run_count];memset(run,0,sizeof(*run));run->pipe_fd=run->err_fd=-1;run->selected_output=-2;
            app->snapshot_profile=app->profiles[app->active_profile];make_run_id(app,run->id,sizeof(run->id));path_join(run->dir,sizeof(run->dir),app->runs_path,run->id);make_dirs(run->dir);
            char target[APP_PATH_CAP];path_join(target,sizeof(target),run->dir,"source.png");
            copy_text(run->source,sizeof(run->source),target);run->pixel_settings=app->snapshot_profile.pixel;run->pixel_settings_saved=run->pixel_settings;run->sprite_settings=run->pixel_settings;run->follow_log=true;
            copy_text(run->used_resources[0],APP_PATH_CAP,source);run->used_resource_count=1;
            bool copied=copy_file_new(source,target),recorded=copied&&write_run_metadata(app,run,true),started=copied&&start_pixel(app,run);
            if(started){run->state=RUN_RUNNING;app->selected_run=app->run_count;app->active_run=app->run_count++;app->view=VIEW_HOME;set_message(app,recorded?"Image conversion started":"Conversion started; metadata could not be saved");snprintf(response,response_size,"{\"ok\":true,\"run\":\"%s\"}",run->id);}
            else {run->state=RUN_FAILED;run->finished=GetTime();if(!run->error[0])copy_text(run->error,sizeof(run->error),"could not start image conversion");snprintf(response,response_size,"{\"ok\":false,\"error\":\"%s\"}",run->error);}
        }
    } else if (!strcmp(action, "profile.select")) {
        int idx = atoi(value); if (idx >= 0 && idx < app->profile_count) set_active_profile(app,idx);
        snprintf(response, response_size, "{\"ok\":true}");
    } else if (!strcmp(action, "profile.add") || !strcmp(action, "profile.duplicate")) {
        if (app->profile_count >= APP_MAX_PROFILES) snprintf(response, response_size, "{\"ok\":false,\"error\":\"profile limit reached\"}");
        else { AppProfile *p = &app->profiles[app->profile_count]; *p = app->profiles[app->active_profile]; p->path[0]=0; snprintf(p->name, sizeof(p->name), "Profile %d", app->profile_count + 1); int added=app->profile_count++; set_active_profile(app,added); scan_refs(app); snprintf(response, response_size, "{\"ok\":true}"); }
    } /* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
    else if (!strcmp(action,"profile.prompt.add")) {
        AppProfile *p=&app->profiles[app->active_profile]; if(p->prompt_count>=APP_MAX_PROMPTS)snprintf(response,response_size,"{\"ok\":false,\"error\":\"prompt part limit reached\"}");else{app->active_prompt_part=p->prompt_count;snprintf(p->prompts[p->prompt_count++],sizeof(p->prompts[0]),"Describe the image.");if(app->profile_settings_view)app->profile_settings_dirty=true;else save_profiles(app);snprintf(response,response_size,"{\"ok\":true}");}
    } else if (!strcmp(action, "profile.delete")) {
        copy_text(app->pending_action, sizeof(app->pending_action), "profile.delete"); set_message(app, "Profile delete pending; confirm or cancel"); snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}");
    } else if (!strcmp(action, "profile.set")) {
        char field[64]; json_string(request, "field", field, sizeof(field)); AppProfile *p = &app->profiles[app->active_profile];
        if (!strcmp(field, "model")) copy_text(p->model, sizeof(p->model), value);
        else if (!strcmp(field, "aspect")) copy_text(p->aspect, sizeof(p->aspect), value);
        else if (!strcmp(field, "name")) copy_text(p->name, sizeof(p->name), value);
        else if (!strcmp(field, "slim_upload")) p->slim_upload = atoi(value) != 0;
        else if (!strncmp(field,"prompt.",7)) { int i=atoi(field+7); if(i<0||i>=p->prompt_count)copy_text(response,response_size,"{\"ok\":false,\"error\":\"prompt part index out of range\"}");else copy_text(p->prompts[i],sizeof(p->prompts[i]),value); }
        else if (!strncmp(field,"set.",4)) { int i=atoi(field+4); if(i<0||i>=APP_MAX_PROMPTS)copy_text(response,response_size,"{\"ok\":false,\"error\":\"model setting index out of range\"}");else{if(i>=p->set_count)p->set_count=i+1;copy_text(p->sets[i],sizeof(p->sets[i]),value);} }
        else if (!set_pixel_profile_field(p,field,value)) copy_text(response,response_size,"{\"ok\":false,\"error\":\"invalid or unsupported profile field\"}");
        else if (response[0]==0) copy_text(response,response_size,"{\"ok\":false,\"error\":\"unknown profile field\"}");
        if (response[0] == 0) { if(app->profile_settings_view)app->profile_settings_dirty=true;else save_profiles(app); snprintf(response, response_size, "{\"ok\":true}"); }
    } else if (!strcmp(action,"run.rerun.inplace.confirm")) {
        AppRun *run=get_run(app);
        if(!app->preview_ready||!app->rerun_pixel_only||!run||run->state==RUN_RUNNING||!file_exists(run->source)) snprintf(response,response_size,"{\"ok\":false,\"error\":\"pixel rerun preview or source is unavailable\"}");
        else {run->pixel_settings_saved=run->pixel_settings;run->pixel_settings_dirty=false;run->state=RUN_RUNNING;app->active_run=app->selected_run;app->preview_ready=false;app->rerun_pixel_only=false;app->rerun_source[0]=0;if(start_pixel(app,run)){set_message(app,"Pixel rerun started in this run");snprintf(response,response_size,"{\"ok\":true}");}else{run->state=RUN_FAILED;run->finished=GetTime();app->active_run=-1;snprintf(response,response_size,"{\"ok\":false,\"error\":\"%s\"}",run->error);}}
    /* agent: gpt-6.1-sol | 2026-10-05 | Offer pixel rerun location choices | v 3.90.0 | 186703 */
    /* agent: gpt-6.1-sol | 2026-10-05 | Keep pixel rerun source local | v 3.73.0 | aa74e8 */
    /* agent: gpt-6.1-sol | 2026-10-05 | Guard cleanup and add in-place pixel rerun | v 3.87.0 | 81ffd0 */
    } else if (!strcmp(action, "run.rerun")) {
        AppRun *old = get_run(app);
        if (!old || app->active_run >= 0) snprintf(response, response_size, "{\"ok\":false,\"error\":\"no completed run or another run active\"}");
        else if (strcmp(value,"full")!=0&&!resolve_pixel_runner((char[APP_PATH_CAP]){0},APP_PATH_CAP)) snprintf(response,response_size,"{\"ok\":false,\"error\":\"pixel processor unavailable; configure AIGEN_PIXELPROCESS_DIR or AIGEN_PIXELPROCESS_RUNNER\"}");
        else if (strcmp(value, "full") != 0 && !file_exists(old->source)) snprintf(response, response_size, "{\"ok\":false,\"error\":\"pixel rerun input is missing; original run was left unchanged\"}");
        /* agent: gpt-6.1-sol | 2026-10-05 | Clarify in-place rerun label | v 3.89.0 | 663800 */
        else if (!strcmp(value,"pixel-in-place")) { copy_text(app->pending_action,sizeof(app->pending_action),"run.pixel.inplace"); set_message(app,"Rerun pixels here? Existing stage outputs may be replaced"); snprintf(response,response_size,"{\"ok\":true,\"confirmation_required\":true}"); }
        else {
            app->snapshot_profile = app->profiles[app->active_profile];
            copy_text(app->snapshot_prompt, sizeof(app->snapshot_prompt), old->prompt);
            copy_text(app->rerun_source, sizeof(app->rerun_source), old->source);
            char rerun_mode[24]; json_string(request, "value", rerun_mode, sizeof(rerun_mode));
            app->rerun_pixel_only = strcmp(rerun_mode, "full") != 0;
            app->snapshot_ref_count = 0; app->snapshot_prompt_part_count = 1;
            copy_text(app->snapshot_prompt_parts[0], APP_TEXT_CAP, old->prompt);
            if (!app->rerun_pixel_only) for (int i=0;i<old->used_resource_count&&app->snapshot_ref_count<APP_MAX_REFS;i++)
                if (is_image_path(old->used_resources[i])) copy_text(app->snapshot_refs[app->snapshot_ref_count++], APP_PATH_CAP, old->used_resources[i]);
            app->preview_ready = true;
            make_run_id(app, app->snapshot_run_id, sizeof(app->snapshot_run_id));
            path_join(app->snapshot_run_dir, sizeof(app->snapshot_run_dir), app->runs_path, app->snapshot_run_id);
            resolve_prompt(app, old->prompt, app->snapshot_resolved, sizeof(app->snapshot_resolved), NULL);
            app->view = VIEW_GENERATION; app->settings_view = false; app->show_run_logs=true; snprintf(response, response_size, "{\"ok\":true,\"confirmation_required\":true}");
            set_message(app, app->rerun_pixel_only ? "Pixel rerun preview ready; confirm to start" : "Full generation rerun preview ready; confirm to start");
        }
    /* agent: gpt-6.1-sol | 2026-10-05 | Guard cleanup and add in-place pixel rerun | v 3.87.0 | 81ffd0 */
    /* agent: gpt-6.1-sol | 2026-10-05 | Keep pixel rerun source local | v 3.73.0 | aa74e8 */
    } else if (!strcmp(action,"run.select")) {
        int idx=atoi(value); if(idx>=0&&idx<app->run_count){app->selected_run=idx;app->view=VIEW_GENERATION;app->settings_view=false;app->show_run_logs=false;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"run index out of range\"}");
    } else if (!strcmp(action,"run.controls")) { app->view=VIEW_GENERATION;app->show_run_logs=true;app->settings_view=false;snprintf(response,response_size,"{\"ok\":true}");
    } else if (!strcmp(action,"run.panel")) { int panel=atoi(value);if(panel>=0&&panel<=3){app->run_panel=panel;app->view=VIEW_GENERATION;app->show_run_logs=true;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"panel index out of range\"}");
    } else if (!strcmp(action,"output.select")) {
        /* agent: gpt-6.1-sol | 2026-10-04 | Select generated output from thumbnail | v 3.37.0 | c43da3 */
        AppRun *run=get_run(app); int idx=atoi(value); if(run&&!strcmp(value,"original")){run->selected_output=-1;run->stage_output_selected=false;run->draw_copy_selected=false;app->view=VIEW_GENERATION;app->settings_view=false;app->show_run_logs=false;snprintf(response,response_size,"{\"ok\":true}");}else if(run&&idx>=0&&idx<run->output_count){run->selected_output=idx;run->stage_output_selected=false;run->draw_copy_selected=false;app->view=VIEW_GENERATION;app->settings_view=false;app->show_run_logs=false;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"output index out of range\"}");
        /* agent: gpt-6.1-sol | 2026-10-04 | Select generated output from thumbnail | v 3.37.0 | c43da3 */
    } else if (!strcmp(action,"stage.select")) {
        AppRun *run=get_run(app); int number=atoi(value); bool found=false; if(run)for(int i=0;i<run->stage_count;i++)if(run->stages[i].number==number){run->selected_stage=i;run->draw_copy_selected=false;run->stage_output_selected=true;app->view=VIEW_GENERATION;app->show_run_logs=true;found=true;break;}
        if(found)snprintf(response,response_size,"{\"ok\":true}");else snprintf(response,response_size,"{\"ok\":false,\"error\":\"stage not emitted\"}");
    } else if (!strcmp(action,"stage.debug")) {
        AppRun *run=get_run(app); if(run){run->selected_stage_debug=atoi(value)!=0;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"no run selected\"}");
    } else if (!strcmp(action,"processing.setting.set")) {
        AppRun *run=get_run(app); char field[64]; json_string(request,"field",field,sizeof(field));
        if(run&&run->state!=RUN_RUNNING)load_run_pixel_settings(run);
        if(!run||run->state==RUN_RUNNING)snprintf(response,response_size,"{\"ok\":false,\"error\":\"run is unavailable\"}");
        else if(run->pixel_sprite_scope&&strcmp(field,"pitch")&&strcmp(field,"phase_x")&&strcmp(field,"phase_y"))snprintf(response,response_size,"{\"ok\":false,\"error\":\"sprite scope supports pitch and phase only\"}");
        else { AppSpriteSettings *override=run->pixel_sprite_scope?sprite_settings_for(run,run->selected_sprite_id,true):NULL;
            AppPixelSettings *px=run->pixel_sprite_scope?(override?&override->settings:NULL):&run->pixel_settings; AppProfile temp={0}; if(px)temp.pixel=*px;
            if(px&&set_pixel_profile_field(&temp,field,value)){
                *px=temp.pixel;if(run->pixel_sprite_scope)run->sprite_settings_override=true;run->pixel_settings_dirty=true;
                if(run->pixel_sprite_scope||!strcmp(field,"pitch")||!strcmp(field,"phase_x")||!strcmp(field,"phase_y")||!strcmp(field,"pitch_min")||!strcmp(field,"pitch_max")||!strcmp(field,"pitch_precision"))copy_text(run->pixel_target,sizeof(run->pixel_target),"lattice");
                else if(!strcmp(field,"background_tolerance"))copy_text(run->pixel_target,sizeof(run->pixel_target),"background");
                else if(!strcmp(field,"grid_cols")||!strcmp(field,"grid_rows"))copy_text(run->pixel_target,sizeof(run->pixel_target),"ownership");
                else if(!strcmp(field,"extraction_padding")||!strcmp(field,"cleanup_mode"))copy_text(run->pixel_target,sizeof(run->pixel_target),"extract");
                else if(!strcmp(field,"foreground_coverage"))copy_text(run->pixel_target,sizeof(run->pixel_target),"reconstruct");
                else if(!strcmp(field,"cleanup_tolerance"))copy_text(run->pixel_target,sizeof(run->pixel_target),"cleanup");
                else if(!strcmp(field,"sheet_anchor")||!strcmp(field,"sheet_padding"))copy_text(run->pixel_target,sizeof(run->pixel_target),"assemble");
                snprintf(response,response_size,"{\"ok\":true}");
            }
            else snprintf(response,response_size,"{\"ok\":false,\"error\":\"invalid pixel setting\"}"); }
    } else if (!strcmp(action,"processing.scope")) {
        AppRun *run=get_run(app); if(run){load_run_pixel_settings(run);run->pixel_sprite_scope=!strcmp(value,"sprite");if(run->pixel_sprite_scope&&!run->selected_sprite_id[0])copy_text(run->selected_sprite_id,sizeof(run->selected_sprite_id),"sprite-r000-c000");AppSpriteSettings *override=run->pixel_sprite_scope?sprite_settings_for(run,run->selected_sprite_id,false):NULL;run->sprite_settings=override?override->settings:run->pixel_settings;snprintf(response,response_size,"{\"ok\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"no run selected\"}");
    } else if (!strcmp(action,"processing.apply")) {
        AppRun *run=get_run(app);if(run)load_run_pixel_settings(run);char runner[APP_PATH_CAP];if(!resolve_pixel_runner(runner,sizeof(runner)))snprintf(response,response_size,"{\"ok\":false,\"error\":\"pixel processor unavailable; configure AIGEN_PIXELPROCESS_DIR or AIGEN_PIXELPROCESS_RUNNER\"}");else if(run&&run->state!=RUN_RUNNING){app->snapshot_profile=app->profiles[app->active_profile];app->snapshot_profile.pixel=run->pixel_settings;run->pixel_settings_dirty=false;run->pixel_settings_saved=run->pixel_settings;copy_text(app->snapshot_prompt,sizeof(app->snapshot_prompt),run->prompt);copy_text(app->rerun_source,sizeof(app->rerun_source),run->source);app->snapshot_ref_count=0;app->snapshot_prompt_part_count=1;copy_text(app->snapshot_prompt_parts[0],APP_TEXT_CAP,run->prompt);app->preview_ready=true;app->rerun_pixel_only=true;make_run_id(app,app->snapshot_run_id,sizeof(app->snapshot_run_id));path_join(app->snapshot_run_dir,sizeof(app->snapshot_run_dir),app->runs_path,app->snapshot_run_id);app->view=VIEW_GENERATION;app->show_run_logs=true;snprintf(response,response_size,"{\"ok\":true,\"confirmation_required\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"run is unavailable\"}");
    } else if (!strcmp(action,"processing.owner.set")) {
        AppRun *run=get_run(app);char component[32],sprite[32];json_string(request,"component",component,sizeof(component));json_string(request,"sprite_id",sprite,sizeof(sprite));
        bool component_ok=!strncmp(component,"blob-",5)&&strlen(component)>5&&strspn(component+5,"0123456789")==strlen(component+5);
        bool sprite_ok=!strncmp(sprite,"sprite-r",8)&&strlen(sprite)==16&&sprite[11]=='-'&&sprite[12]=='c'&&strspn(sprite+8,"0123456789")==3&&strspn(sprite+13,"0123456789")==3;
        if(!run||run->state==RUN_RUNNING||!component_ok||!sprite_ok)snprintf(response,response_size,"{\"ok\":false,\"error\":\"invalid ownership override\"}");
        else {int i=0;for(;i<run->owner_override_count;i++)if(!strcmp(run->owner_components[i],component))break;if(i==run->owner_override_count&&i<APP_MAX_SPRITE_BLOBS)run->owner_override_count++;if(i>=APP_MAX_SPRITE_BLOBS)snprintf(response,response_size,"{\"ok\":false,\"error\":\"ownership override limit reached\"}");else{copy_text(run->owner_components[i],sizeof(run->owner_components[i]),component);copy_text(run->owner_sprites[i],sizeof(run->owner_sprites[i]),sprite);run->pixel_target[0]=0;copy_text(run->pixel_target,sizeof(run->pixel_target),"ownership");run->pixel_settings_dirty=true;snprintf(response,response_size,"{\"ok\":true}");}}
    } else if (!strcmp(action,"output.export")) {
        AppRun *run=get_run(app);bool allowed=false;if(run){if(run->generated.path[0]&&!strcmp(path,run->generated.path))allowed=true;for(int i=0;i<run->output_count;i++)if(!strcmp(path,run->outputs[i].path))allowed=true;for(int i=0;i<run->stage_count;i++)if(!strcmp(path,run->stages[i].path))allowed=true;for(int i=0;i<run->debug_stage_count;i++)if(!strcmp(path,run->debug_stages[i].path))allowed=true;}
        if(allowed&&destination[0]&&copy_file_atomic(path,destination))snprintf(response,response_size,"{\"ok\":true}");else snprintf(response,response_size,"{\"ok\":false,\"error\":\"source must be a run image and destination writable\"}");
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
    /* agent: gpt-6.1-sol | 2026-10-05 | Guard cleanup and add in-place pixel rerun | v 3.87.0 | 81ffd0 */
    else if (!strcmp(action, "run.stages.clear")) {
        AppRun *run=get_run(app); if(run&&run->state!=RUN_RUNNING){copy_text(app->pending_action,sizeof(app->pending_action),"run.stages.clear");set_message(app,"Stage cleanup pending; confirm or cancel");snprintf(response,response_size,"{\"ok\":true,\"confirmation_required\":true}");}else snprintf(response,response_size,"{\"ok\":false,\"error\":\"run is unavailable\"}");
    }
    /* agent: gpt-6.1-sol | 2026-10-05 | Guard cleanup and add in-place pixel rerun | v 3.87.0 | 81ffd0 */
    else if (!strcmp(action, "run.output.open")) {
        AppRun *run = get_run(app); bool allowed = false;
        if (run) { if (run->generated.path[0] && !strcmp(path, run->generated.path)) allowed = true; for (int i=0;i<run->output_count;i++) if (!strcmp(path,run->outputs[i].path)) allowed = true; for (int i=0;i<run->stage_count;i++) if (!strcmp(path,run->stages[i].path)) allowed = true; for (int i=0;i<run->debug_stage_count;i++) if (!strcmp(path,run->debug_stages[i].path)) allowed = true; }
        if (allowed && open_path(path)) snprintf(response,response_size,"{\"ok\":true}"); else snprintf(response,response_size,"{\"ok\":false,\"error\":\"path is not a selected run artifact\"}");
    }
    else if (!strcmp(action, "app.action")) snprintf(response, response_size, "{\"ok\":false,\"error\":\"unknown action\"}");
    else snprintf(response, response_size, "{\"ok\":false,\"error\":\"unknown action\"}");
    return response;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Expose resource actions and tools | v 3.55.0 | 5d23c1 */
/* agent: gpt-6.1-sol | 2026-10-04 | Block navigation during drawing | v 3.50.0 | cbcdc5 */
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

/* agent: gpt-6.1-sol | 2026-10-04 | Bound reloadable texture memory | v 3.61.0 | 432118 */
#define APP_TEXTURE_BUDGET (512u * 1024u * 1024u)
#define APP_TEXTURE_CACHE_CAP (APP_MAX_REFS * 2 + APP_MAX_RUNS * (APP_MAX_STAGES + APP_MAX_OUTPUTS * 2 + 1) * 2)
typedef struct { Texture2D *texture; double used_at; bool full; } CacheTexture;
static CacheTexture s_texture_cache[APP_TEXTURE_CACHE_CAP];

/* Compare const void* CacheTexture entries by full-texture priority and age; return int. */
static int texture_cache_order(const void *a, const void *b) {
    const CacheTexture *aa = a, *bb = b;
    if (aa->full != bb->full) return aa->full ? -1 : 1;
    return aa->used_at < bb->used_at ? -1 : aa->used_at > bb->used_at;
}

/* Add Texture2D* texture to reusable cache candidates and size_t bytes; return size_t. */
static size_t texture_cache_add(Texture2D *texture, double used_at, bool full, size_t count) {
    if (!texture->id || count >= APP_TEXTURE_CACHE_CAP) return count;
    s_texture_cache[count++] = (CacheTexture){texture, used_at, full};
    return count;
}

/* Evict stale reloadable textures from App* app above the 512 MiB budget; return void. */
static void texture_cache_enforce(App *app) {
    double now = GetTime();
    if (now < app->texture_cache_checked_at) return;
    app->texture_cache_checked_at = now + 0.5;
    size_t count = 0, bytes = 0;
    for (int i = 0; i < app->ref_count; i++) {
        AppReference *ref = &app->refs[i];
        if (ref->full.id) bytes += (size_t)ref->full.width * ref->full.height * 4;
        if (ref->thumb.id) bytes += (size_t)ref->thumb.width * ref->thumb.height * 4;
        count = texture_cache_add(&ref->full, ref->full_used_at, true, count);
        count = texture_cache_add(&ref->thumb, ref->thumb_used_at, false, count);
    }
    for (int r = 0; r < app->run_count; r++) {
        AppRun *run = &app->runs[r];
        AppArtifact *sets[] = {&run->generated, run->stages, run->debug_stages, run->outputs, run->draw_copies};
        int sizes[] = {1, run->stage_count, run->debug_stage_count, run->output_count, run->draw_copy_count};
        for (int set = 0; set < 5; set++) for (int i = 0; i < sizes[set]; i++) {
            AppArtifact *artifact = &sets[set][i];
            if (artifact->texture.id) bytes += (size_t)artifact->texture.width * artifact->texture.height * 4;
            if (artifact->thumb.id) bytes += (size_t)artifact->thumb.width * artifact->thumb.height * 4;
            count = texture_cache_add(&artifact->texture, artifact->full_used_at, true, count);
            count = texture_cache_add(&artifact->thumb, artifact->thumb_used_at, false, count);
        }
    }
    if (bytes <= APP_TEXTURE_BUDGET) return;
    qsort(s_texture_cache, count, sizeof(s_texture_cache[0]), texture_cache_order);
    for (size_t i = 0; i < count && bytes > APP_TEXTURE_BUDGET; i++) {
        CacheTexture *candidate = &s_texture_cache[i];
        Texture2D *texture = candidate->texture;
        if (!texture->id || candidate->used_at > now - 1.0) continue;
        size_t size = (size_t)texture->width * texture->height * 4;
        UnloadTexture(*texture);
        *texture = (Texture2D){0};
        bytes = size < bytes ? bytes - size : 0;
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Bound reloadable texture memory | v 3.61.0 | 432118 */

/* Poll App* app processes, ENet, image results, and selected run logs; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
/* agent: gpt-6.1-sol | 2026-10-04 | Reload evicted selected images | v 3.61.0 | 70ccac */
void AppUpdate(App *app) {
    image_loader_update(app);
    if (app->image_loader) texture_cache_enforce(app);
    draw_save_update(app);
    if (app->view == VIEW_GENERATION && app->selected_run >= 0 && app->selected_run < app->run_count) {
        AppRun *run = &app->runs[app->selected_run];
        /* agent: gpt-6.1-sol | 2026-10-04 | Reload evicted selected images | v 3.61.0 | 70ccac */
        if (!run->generated.texture.id && !run->generated.thumb.id && !run->generated.loading && run->generated.path[0] && queue_image(app, run->generated.path, 240, false, false)) run->generated.loading = true;
        if (!run->generated.path[0] && run->stage_count && !run->stages[0].texture.id && !run->stages[0].thumb.id && !run->stages[0].loading && queue_image(app, run->stages[0].path, 240, false, false)) run->stages[0].loading = true;
        /* agent: gpt-6.1-sol | 2026-10-04 | Reload evicted selected images | v 3.61.0 | 70ccac */
        if (!run->logs_loaded && !run->logs_loading) queue_run_logs(app, run);
    }
    poll_enet(app);
    for (int i = 0; i < app->run_count; i++) {
        AppRun *run = &app->runs[i];
        if (run->state == RUN_RUNNING) {
            update_child(app, run);
            if (run->state == RUN_RUNNING && run->process_kind == PROC_PIXEL && GetTime() - run->artifact_scan_at >= 0.75) {
                refresh_run_outputs(app, run);
                run->artifact_scan_at = GetTime();
            }
        }
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Reload evicted selected images | v 3.61.0 | 70ccac */

/* Draw a button and return true on a left click. */
static bool button(Rectangle rect, const char *label, bool active) {
    Vector2 m = GetMousePosition(); bool over = CheckCollisionPointRec(m, rect);
    DrawRectangleRec(rect, over ? UI_PANEL_ALT : UI_PANEL);
    DrawRectangleLinesEx(rect, 1, active ? UI_ACCENT : (over ? UI_TEXT : (Color){67, 70, 79, 255}));
    ui_draw_text(label, (int)rect.x + 10, (int)rect.y + 8, 16, active ? UI_ACCENT : UI_TEXT);
    return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

typedef enum { INPUT_FRAME_IDLE, INPUT_FRAME_ACTIVE, INPUT_FRAME_DIRTY, INPUT_FRAME_PROFILE_DIRTY } InputFrameState;

/* Draw a shared prompt, rename, or settings input frame with InputFrameState; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Share prompt and settings input frames | v 3.18.0 | c8a86b */
static void draw_advanced_input_frame(Rectangle bounds, InputFrameState state) {
    Color fill = state == INPUT_FRAME_ACTIVE ? UI_PANEL_ALT : UI_PANEL;
    Color outline = state == INPUT_FRAME_ACTIVE ? UI_ACCENT : state == INPUT_FRAME_DIRTY ? UI_WARN : state == INPUT_FRAME_PROFILE_DIRTY ? (Color){255,220,82,255} : UI_PANEL_ALT;
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

/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* Save pending App* app profile settings and refresh their baseline; return bool. */
static bool save_profile_settings(App *app) {
    if(!app->profile_settings_view)return false;
    if(!app->profile_settings_dirty)return true;
    if(!save_profiles(app)){set_message(app,"Could not save profile settings");return false;}
    app->profile_settings_saved=app->profiles[app->active_profile];
    app->profile_settings_dirty=false;
    set_message(app,"Profile settings saved");
    return true;
}

/* Compare a profile field value with App* app's last saved settings; return bool. */
static bool profile_input_changed(App *app, const char *field, const char *value) {
    AppProfile *saved=&app->profile_settings_saved;
    if(!strcmp(field,"model"))return strcmp(value,saved->model)!=0;
    if(!strcmp(field,"aspect"))return strcmp(value,saved->aspect)!=0;
    if(!strcmp(field,"pitch"))return strcmp(value,saved->pixel.pitch)!=0;
    if(!strcmp(field,"phase_x"))return strcmp(value,saved->pixel.phase_x)!=0;
    if(!strcmp(field,"phase_y"))return strcmp(value,saved->pixel.phase_y)!=0;
    if(!strcmp(field,"pitch_precision"))return strcmp(value,saved->pixel.pitch_precision)!=0;
    if(!strcmp(field,"cleanup_mode"))return strcmp(value,saved->pixel.cleanup_mode)!=0;
    if(!strcmp(field,"sheet_anchor"))return strcmp(value,saved->pixel.sheet_anchor)!=0;
    if(!strcmp(field,"background_tolerance"))return atoi(value)!=saved->pixel.background_tolerance;
    if(!strcmp(field,"grid_cols"))return atoi(value)!=saved->pixel.grid_cols;
    if(!strcmp(field,"grid_rows"))return atoi(value)!=saved->pixel.grid_rows;
    if(!strcmp(field,"extraction_padding"))return atoi(value)!=saved->pixel.extraction_padding;
    if(!strcmp(field,"pitch_min"))return atoi(value)!=saved->pixel.pitch_min;
    if(!strcmp(field,"pitch_max"))return atoi(value)!=saved->pixel.pitch_max;
    if(!strcmp(field,"foreground_coverage"))return atoi(value)!=saved->pixel.foreground_coverage;
    if(!strcmp(field,"cleanup_tolerance"))return atoi(value)!=saved->pixel.cleanup_tolerance;
    if(!strcmp(field,"sheet_padding"))return atoi(value)!=saved->pixel.sheet_padding;
    int index=-1;
    if(sscanf(field,"prompt.%d",&index)==1)return index<0||index>=APP_MAX_PROMPTS||index>=saved->prompt_count||strcmp(value,saved->prompts[index])!=0;
    if(sscanf(field,"set.%d",&index)==1)return index<0||index>=APP_MAX_PROMPTS||index>=saved->set_count||strcmp(value,saved->sets[index])!=0;
    return false;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */

static bool profile_input_changed(App *app, const char *field, const char *value);
static bool save_profile_settings(App *app);

/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* Draw App* app's const char* field, label and value in Rectangle bounds; return void. */
static void draw_profile_input(App *app, const char *field, const char *label, const char *value, Rectangle rect) {
    ui_draw_text(label, (int)rect.x, (int)rect.y, 15, UI_MUTED);
    Rectangle box = {rect.x, rect.y + 22, rect.width, 38};
    bool editing = !strcmp(app->edit_field, field);
    bool changed=app->profile_settings_view&&profile_input_changed(app,field,value);
    draw_advanced_input_frame(box, changed ? INPUT_FRAME_PROFILE_DIRTY : editing ? INPUT_FRAME_ACTIVE : INPUT_FRAME_IDLE);
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
        const char *action=!strncmp(field,"pixelrun.",9)?"processing.setting.set":"profile.set";
        snprintf(req, sizeof(req), "{\"action\":\"%s\",\"field\":\"%s\",\"value\":\"%s\"}",action, !strncmp(field,"pixelrun.",9)?field+9:ef, ev);
        AppDispatch(app, req, s_action_response, sizeof(s_action_response));
        if(app->profile_settings_view)app->profile_settings_dirty=true;
    }
    if (IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_ENTER)) { app->edit_field[0] = 0; app->field_scroll = 0; }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
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
/* agent: gpt-6.1-sol | 2026-10-04 | Show profiles among resources | v 3.55.0 | 0f4735 */

/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
/* Draw resource cards and Generate for App* app; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Block library input behind dialogs | v 3.66.0 | b66dd8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Add generate control to library | v 3.12.0 | 4e90c5 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
static void draw_references(App *app) {
    int w = GetScreenWidth(),h=GetScreenHeight();
    bool dialog_open = app->pending_action[0] != 0;
    Rectangle menu={80,132,220,144};
    bool menu_was_open=app->show_add_menu;
    int margin = 40, top = 120,bottom=h-44;
    int cols = w / 250; if (cols < 1) cols = 1;
    int gap = 20, tile = (w - margin * 2 - gap * (cols - 1)) / cols;
    if (tile > 260) tile = 260;
    int visible_count=app->ref_count+2;
    int rows=(visible_count+cols-1)/cols,max_scroll=rows*(tile+gap)-(bottom-top);if(max_scroll<0)max_scroll=0;
    Rectangle area={24,(float)top,(float)w-48,(float)(bottom-top)};
    if(!dialog_open&&CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel!=0)app->reference_scroll-=(int)(wheel*120);}
    if(app->reference_scroll<0)app->reference_scroll=0;
    if(app->reference_scroll>max_scroll)app->reference_scroll=max_scroll;
    BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    int visible = 0;
    int generate_x=margin,generate_y=top-app->reference_scroll;
    Rectangle generate_box={(float)generate_x,(float)generate_y,(float)tile,(float)tile};
    draw_generate_tile(generate_box,app->active_run>=0?"Run active":"Generate",false);
    if(!dialog_open&&app->active_run<0&&CheckCollisionPointRec(GetMousePosition(),generate_box)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        app->preview_ready=false;
        AppDispatch(app,"{\"action\":\"generation.preview\"}",s_action_response,sizeof(s_action_response));
        if(app->preview_ready)AppDispatch(app,"{\"action\":\"generation.confirm\"}",s_action_response,sizeof(s_action_response));
    }
    visible++;
    int add_x=margin+(visible%cols)*(tile+gap),add_y=top+(visible/cols)*(tile+gap)-app->reference_scroll;
    Rectangle add_box={(float)add_x,(float)add_y,(float)tile,(float)tile};
    draw_plus_tile(add_box,"Add resource",app->show_add_menu);
    if(!dialog_open&&CheckCollisionPointRec(GetMousePosition(),add_box)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!(app->show_add_menu&&CheckCollisionPointRec(GetMousePosition(),menu)))app->show_add_menu=!app->show_add_menu;
    visible++;
    for (int i = 0; i < app->ref_count; i++) {
        AppReference *r = &app->refs[i];
        int col = visible % cols, row = visible / cols;
        int x = margin + col * (tile + gap), y = top + row * (tile + gap)-app->reference_scroll; visible++;
        Rectangle box = {(float)x, (float)y, (float)tile, (float)tile};
        /* agent: gpt-6.1-sol | 2026-10-04 | Touch visible resource textures | v 3.61.0 | 77080c */
        if (CheckCollisionRecs(box, area)) {
            if (r->thumb.id) r->thumb_used_at = GetTime();
            else if (r->full.id) r->full_used_at = GetTime();
        }
        if (!r->is_text && !r->is_profile && !r->thumb.id && !r->loading && CheckCollisionRecs(box, area) && queue_image(app, r->path, 160, true, false)) r->loading = true;
        /* agent: gpt-6.1-sol | 2026-10-04 | Touch visible resource textures | v 3.61.0 | 77080c */
        DrawRectangleRec(box, UI_PANEL);
        if(r->is_profile) {
            DrawRectangleRec(box,UI_PANEL_ALT);
            ui_draw_text("GENERATION PROFILE",(int)box.x+14,(int)box.y+22,14,UI_ACCENT);
            ui_draw_text(TextFormat("Model: %.24s",r->profile_index>=0?app->profiles[r->profile_index].model:""),(int)box.x+14,(int)box.y+56,16,UI_TEXT);
            ui_draw_text(r->selected?"Enabled":"Click to edit",(int)box.x+14,(int)box.y+84,14,r->selected?UI_GOOD:UI_MUTED);
        } else if (r->is_text) {
            DrawRectangleRec(box,UI_PANEL_ALT);
            draw_wrapped_text(r->preview,(Rectangle){box.x+14,box.y+14,box.width-28,box.height-54},14,UI_TEXT);
        } else if (r->thumb.id) {
            float scale = fminf(box.width / r->thumb.width, box.height / r->thumb.height);
            Rectangle image = {box.x + (box.width - r->thumb.width * scale) * 0.5f, box.y + (box.height - r->thumb.height * scale) * 0.5f, r->thumb.width * scale, r->thumb.height * scale};
            DrawTexturePro(r->thumb, (Rectangle){0,0,(float)r->thumb.width,(float)r->thumb.height}, image, (Vector2){0,0}, 0, WHITE);
        } else if (r->loading) {
            draw_loading_marker(x + tile / 2, y + tile / 2 - 12, UI_ACCENT);
            ui_draw_text("Loading preview", x + 10, y + tile / 2 + 4, 13, UI_MUTED);
        }
        DrawRectangle((int)box.x,(int)(box.y+box.height-30),(int)box.width,30,(Color){20,22,26,225});
        ui_draw_text(TextFormat("%.30s",r->name),(int)box.x+8,(int)(box.y+box.height-24),14,UI_TEXT);
        draw_reference_icons(r, box);
        DrawRectangleLinesEx(box, app->selected_ref == i ? 3 : 1, app->selected_ref == i ? UI_ACCENT : UI_PANEL_ALT);
        if (!dialog_open && CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Rectangle exclude = {box.x + box.width - 58, box.y + 6, 24, 24};
            Rectangle trash = {box.x + box.width - 28, box.y + 6, 22, 24};
            if (!app->show_add_menu && CheckCollisionPointRec(GetMousePosition(), exclude)) {
                app->selected_ref=i;
                if(r->is_profile&&r->profile_index>=0)set_active_profile(app,r->profile_index);
                else {r->excluded = !r->excluded;save_reference_state(app);}
            }
            else if (!app->show_add_menu && CheckCollisionPointRec(GetMousePosition(), trash)) { app->selected_ref=i; if(r->is_profile&&r->profile_index>=0){set_active_profile(app,r->profile_index);AppDispatch(app,"{\"action\":\"profile.delete\"}",s_action_response,sizeof(s_action_response));}else{copy_text(app->pending_action, sizeof(app->pending_action), "reference.trash"); copy_text(app->pending_path, sizeof(app->pending_path), r->path);} }
            else if (!app->show_add_menu && r->is_profile) { app->selected_ref=i; if(r->profile_index>=0)open_profile_text(app,r->profile_index); }
            else if (!app->show_add_menu && r->is_text) { if (select_prompt_reference(app,r)) { app->selected_ref=i; app->profile_text_edit=false; app->show_full_image=true; } }
            else if (!app->show_add_menu) { app->selected_ref=i; app->show_full_image = true; app->image_zoom = 1; app->image_pan_x = app->image_pan_y = 0; }
        }
    }
    EndScissorMode();
    bool left_click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if(!dialog_open&&app->show_add_menu&&(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)||(left_click&&!CheckCollisionPointRec(GetMousePosition(),add_box)&&!CheckCollisionPointRec(GetMousePosition(),menu)))) app->show_add_menu=false;
    if(!dialog_open&&app->show_add_menu){
        DrawRectangleRec(menu,UI_PANEL);DrawRectangleLinesEx(menu,1,UI_ACCENT);
        if(menu_was_open&&button((Rectangle){menu.x+10,menu.y+8,menu.width-20,36},"Import image or text",false)){app->show_add_menu=false;import_reference_dialog(app);}
        if(menu_was_open&&button((Rectangle){menu.x+10,menu.y+50,menu.width-20,36},"New text resource",false)){app->show_add_menu=false;if(create_prompt_file(app))set_message(app,"Empty text resource created");}
        if(menu_was_open&&button((Rectangle){menu.x+10,menu.y+92,menu.width-20,36},"Add profile",false)){app->show_add_menu=false;AppDispatch(app,"{\"action\":\"profile.add\"}",s_action_response,sizeof(s_action_response));}
    }
    if (!dialog_open && IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        for (unsigned int i = 0; i < dropped.count; i++) if (is_image_path(dropped.paths[i]) || is_text_path(dropped.paths[i])) import_reference_file(app, dropped.paths[i]);
        UnloadDroppedFiles(dropped); save_reference_state(app); set_message(app, "Resources added");
    }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Block library input behind dialogs | v 3.66.0 | b66dd8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show profiles among resources | v 3.55.0 | 0f4735 */
/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Add generate control to library | v 3.12.0 | 4e90c5 */

/* Draw generation cards and route App* app to Library or selected details; return void. */
/* agent: gpt-6.1-sol | 2026-10-05 | Block generation card input behind dialogs | v 3.82.0 | 17ac38 */
/* agent: gpt-6.1-sol | 2026-10-04 | Draw live generation cards | v 3.12.0 | cac401 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Tone plus tile on hover | v 3.29.0 | 99ec42 */
/* agent: gpt-6.1-sol | 2026-10-04 | Unify library and generation plus tiles | v 3.33.0 | c630e9 */
static void draw_generation_grid(App *app) {
    bool dialog_open = app->pending_action[0] != 0;
    int w = GetScreenWidth(), h = GetScreenHeight(), margin = 40, top = 120, bottom = h - 44;
    int cols = w / 250; if (cols < 1) cols = 1;
    int gap = 20, tile = (w - margin * 2 - gap * (cols - 1)) / cols;
    if (tile > 260) tile = 260;
    int count = app->run_count + 1, rows = (count + cols - 1) / cols;
    int max_scroll = rows * (tile + gap) - (bottom - top); if (max_scroll < 0) max_scroll = 0;
    Rectangle area = {24, (float)top, (float)w - 48, (float)(bottom - top)};
    if (!dialog_open && CheckCollisionPointRec(GetMousePosition(), area)) app->reference_scroll -= (int)(GetMouseWheelMove() * 120);
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
            if (!dialog_open && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app->view = VIEW_LIBRARY; app->settings_view = false; }
            continue;
        }
        DrawRectangleRec(box, UI_PANEL);
        int index = app->run_count - card;
        AppRun *run = &app->runs[index];
        AppArtifact *preview = run->output_count ? &run->outputs[0] : run->generated.path[0] ? &run->generated : NULL;
        /* agent: gpt-6.1-sol | 2026-10-04 | Queue evicted cards for reload | v 3.61.0 | 24a8e6 */
        if (preview && !preview->thumb.id && !preview->loading && CheckCollisionRecs(box, area) && queue_image(app, preview->path, 240, false, false)) preview->loading = true;
        /* agent: gpt-6.1-sol | 2026-10-04 | Queue evicted cards for reload | v 3.61.0 | 24a8e6 */
        if (preview && (preview->thumb.id || preview->texture.id)) {
            Texture2D card_texture = preview->thumb.id ? preview->thumb : preview->texture;
            if (CheckCollisionRecs(box, area)) {
                if (preview->thumb.id) preview->thumb_used_at = GetTime();
                else preview->full_used_at = GetTime();
            }
            float scale = fminf(box.width / card_texture.width, (box.height - 42) / card_texture.height);
            Rectangle image = {box.x + (box.width - card_texture.width * scale) * .5f, box.y + (box.height - 42 - card_texture.height * scale) * .5f, card_texture.width * scale, card_texture.height * scale};
            DrawTexturePro(card_texture, (Rectangle){0, 0, (float)card_texture.width, (float)card_texture.height}, image, (Vector2){0, 0}, 0, WHITE);
        } else if (preview && preview->loading) {
            draw_loading_marker(x + tile / 2, y + (tile - 42) / 2, UI_ACCENT);
            ui_draw_text("Loading preview", x + 10, y + (tile - 42) / 2 + 14, 13, UI_MUTED);
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
        if (!dialog_open && CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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

/* agent: gpt-6.1-sol | 2026-10-04 | Add drawing session history | v 3.50.0 | 721428 */
/* Release pixel buffers and texture owned by DrawState* state; return void. */
static void draw_session_free(DrawState *state) {
    if (!state) return;
    if (state->save_started) pthread_join(state->save_thread, NULL);
    if (state->texture.id) UnloadTexture(state->texture);
    free(state->pixels); free(state->original); free(state->pixel_stamp);
    free(state->changes); free(state->strokes); free(state);
}

/* Open image at const char* path as an editable RGBA session on App* app; return bool. */
static bool draw_session_begin(App *app, const char *path) {
    if (!path || !*path || app->draw_state) return false;
    Image image = LoadImage(path);
    if (!image.data) { set_message(app, "Could not load image for drawing"); return false; }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    size_t count = (size_t)image.width * (size_t)image.height;
    DrawState *state = calloc(1, sizeof(*state));
    if (!state) { UnloadImage(image); return false; }
    atomic_init(&state->save_done, false);
    state->pixels = malloc(count * sizeof(Color));
    state->original = malloc(count * sizeof(Color));
    state->pixel_stamp = calloc(count, sizeof(uint32_t));
    if (!state->pixels || !state->original || !state->pixel_stamp) {
        UnloadImage(image); draw_session_free(state); set_message(app, "Not enough memory to draw on this image"); return false;
    }
    state->width = image.width; state->height = image.height;
    memcpy(state->pixels, image.data, count * sizeof(Color));
    memcpy(state->original, image.data, count * sizeof(Color));
    copy_text(state->path, sizeof(state->path), path);
    state->texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!state->texture.id) { draw_session_free(state); set_message(app, "Could not create drawing canvas"); return false; }
    SetTextureFilter(state->texture, TEXTURE_FILTER_POINT);
    app->draw_state = state;
    app->draw_close_pending = false;
    return true;
}

/* Reserve count entries in DrawChange* changes; return bool. */
static bool draw_reserve_changes(DrawState *state, size_t count) {
    if (count <= state->change_capacity) return true;
    size_t cap = state->change_capacity ? state->change_capacity * 2 : 1024;
    while (cap < count) cap *= 2;
    DrawChange *p = realloc(state->changes, cap * sizeof(*p));
    if (!p) return false;
    state->changes = p; state->change_capacity = cap;
    return true;
}

/* Reserve count stroke history entries for DrawState* state; return bool. */
static bool draw_reserve_strokes(DrawState *state, size_t count) {
    if (count <= state->stroke_capacity) return true;
    size_t cap = state->stroke_capacity ? state->stroke_capacity * 2 : 32;
    while (cap < count) cap *= 2;
    DrawStroke *p = realloc(state->strokes, cap * sizeof(*p));
    if (!p) return false;
    state->strokes = p; state->stroke_capacity = cap;
    return true;
}

/* Apply one sampled-color pixel to DrawState* state at image coordinates; return bool. */
static bool draw_paint_pixel(DrawState *state, int x, int y) {
    if (x < 0 || y < 0 || x >= state->width || y >= state->height) return false;
    uint32_t index = (uint32_t)((size_t)y * state->width + x);
    Color before = state->pixels[index];
    if (memcmp(&before, &state->stroke_color, sizeof(Color)) == 0) return false;
    size_t stamped = state->pixel_stamp[index] ? state->pixel_stamp[index] - 1 : SIZE_MAX;
    if (stamped >= state->active_start && stamped < state->change_count && state->changes[stamped].index == index) {
        size_t slot = stamped;
        state->changes[slot].after = state->stroke_color;
    } else {
        if (!draw_reserve_changes(state, state->change_count + 1)) return false;
        size_t slot = state->change_count++;
        state->changes[slot] = (DrawChange){index, before, state->stroke_color};
        state->pixel_stamp[index] = (uint32_t)(slot + 1);
    }
    state->pixels[index] = state->stroke_color;
    return true;
}

/* Start a sampled stroke on DrawState* state at pixel x/y; return bool. */
static bool draw_begin_stroke(DrawState *state, int x, int y) {
    if (x < 0 || y < 0 || x >= state->width || y >= state->height) return false;
    if (state->cursor < state->stroke_count) {
        state->change_count = state->cursor ? state->strokes[state->cursor - 1].start + state->strokes[state->cursor - 1].count : 0;
        state->stroke_count = state->cursor;
    }
    uint32_t index = (uint32_t)((size_t)y * state->width + x);
    state->stroke_color = state->pixels[index];
    state->active_start = state->change_count;
    state->last_x = x; state->last_y = y;
    state->active = true;
    draw_paint_pixel(state, x, y);
    return true;
}

/* Paint a line between image pixels x0/y0 and x1/y1; return bool. */
static bool draw_stroke_line(DrawState *state, int x0, int y0, int x1, int y1) {
    bool changed = false;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        changed |= draw_paint_pixel(state, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
    return changed;
}

/* Finish one changed drag and append it to DrawState* history; return bool. */
static bool draw_end_stroke(DrawState *state) {
    if (!state->active) return false;
    state->active = false;
    size_t count = state->change_count - state->active_start;
    if (!count) return false;
    if (!draw_reserve_strokes(state, state->cursor + 1)) return false;
    state->strokes[state->cursor++] = (DrawStroke){state->active_start, count};
    state->stroke_count = state->cursor;
    return true;
}

/* Undo or redo one stroke in DrawState* state using bool redo; return bool. */
static bool draw_history_step(DrawState *state, bool redo) {
    if (redo) {
        if (state->cursor >= state->stroke_count) return false;
        DrawStroke stroke = state->strokes[state->cursor++];
        for (size_t i = 0; i < stroke.count; i++) { DrawChange *c = &state->changes[stroke.start + i]; state->pixels[c->index] = c->after; }
        return true;
    }
    if (!state->cursor) return false;
    DrawStroke stroke = state->strokes[--state->cursor];
    for (size_t i = stroke.count; i > 0; i--) { DrawChange *c = &state->changes[stroke.start + i - 1]; state->pixels[c->index] = c->before; }
    return true;
}

/* Return whether DrawState* state differs from its original pixels; return bool. */
static bool draw_session_dirty(const DrawState *state) {
    return state && memcmp(state->pixels, state->original, (size_t)state->width * state->height * sizeof(Color)) != 0;
}

/* Return image destination in viewer int bounds after current zoom and pan; return Rectangle. */
static Rectangle image_view_rect(App *app, Texture2D texture, int x, int y, int width, int height) {
    float scale = fminf((float)width / texture.width, (float)height / texture.height) * app->image_zoom;
    float iw = texture.width * scale, ih = texture.height * scale;
    return (Rectangle){x + (width - iw) * 0.5f + app->image_pan_x, y + (height - ih) * 0.5f + app->image_pan_y, iw, ih};
}

/* agent: gpt-6.1-sol | 2026-10-05 | Map source and final sprite evidence | v 3.75.0 | 3e5619 */
/* Resolve the processor stage represented by AppArtifact* image; return int or -1. */
static int sprite_evidence_stage(AppArtifact *image) {
    if (!image) return -1;
    if (image->number > 0) return image->number;
    if (image->number < 0) return 100;
    return strstr(image->name, "final.pixel") ? 0 : -1;
}
/* agent: gpt-6.1-sol | 2026-10-05 | Map source and final sprite evidence | v 3.75.0 | 3e5619 */

/* agent: gpt-6.1-sol | 2026-10-05 | Load source and final sprite records | v 3.75.0 | cb9336 */
/* Load JSONL sprite records for AppRun* run and stage AppArtifact* image; return void. */
static void load_sprite_evidence(App *app, AppRun *run, AppArtifact *image) {
    if (!run || !image) return;
    int evidence_stage = sprite_evidence_stage(image);
    if (evidence_stage < 0) {
        app->sprite_evidence_count = 0;
        app->sprite_evidence_stage = -1;
        app->sprite_evidence_artifact[0] = 0;
        app->sprite_evidence_selected = -1;
        return;
    }
    if (app->sprite_evidence_count > 0 && app->sprite_evidence_stage == evidence_stage && !strcmp(app->sprite_evidence_artifact, image->path)) return;
    app->sprite_evidence_stage = evidence_stage;
    app->sprite_evidence_selected = -1;
    app->sprite_evidence_count = 0;
    copy_text(app->sprite_evidence_artifact, sizeof(app->sprite_evidence_artifact), image->path);
    char path[APP_PATH_CAP], line[32768], raw[128];
    path_join(path, sizeof(path), run->dir, "extra/processing.sprites.jsonl");
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    while (app->sprite_evidence_count < APP_MAX_SPRITE_EVIDENCE && fgets(line, sizeof(line), fp)) {
        if (!json_raw(line, "stage_number", raw, sizeof(raw)) || atoi(raw) != evidence_stage) continue;
        if (evidence_stage == 0) {
            char artifact[APP_PATH_CAP];
            if (!json_string(line, "artifact", artifact, sizeof(artifact)) || strcmp(artifact, image->path)) continue;
        }
        AppSpriteEvidence *item = &app->sprite_evidence[app->sprite_evidence_count];
        const char *bounds = strstr(line, "\"artifact_bounds\"");
        const char *array = bounds ? strchr(bounds, '[') : NULL;
        if (!array || sscanf(array, "[%d ,%d ,%d ,%d]", &item->x, &item->y, &item->width, &item->height) != 4) continue;
        json_raw(line, "row", raw, sizeof(raw)); item->row = atoi(raw);
        json_raw(line, "col", raw, sizeof(raw)); item->col = atoi(raw);
        const char *dimensions = strstr(line, "\"artifact_dimensions\"");
        const char *dimension_array = dimensions ? strchr(dimensions, '[') : NULL;
        if (dimension_array) (void)sscanf(dimension_array, "[%d ,%d]", &item->artifact_width, &item->artifact_height);
        const char *source_bounds=strstr(line,"\"source_bounds\""),*source_array=source_bounds?strchr(source_bounds,'['):NULL;
        if(source_array)(void)sscanf(source_array,"[%d ,%d ,%d ,%d]",&item->source_x,&item->source_y,&item->source_width,&item->source_height);
        const char *source_dimensions=strstr(line,"\"source_dimensions\""),*source_dims=source_dimensions?strchr(source_dimensions,'['):NULL;
        if(source_dims)(void)sscanf(source_dims,"[%d ,%d]",&item->source_artifact_width,&item->source_artifact_height);
        if(item->source_width<=0){item->source_x=item->x;item->source_y=item->y;item->source_width=item->width;item->source_height=item->height;item->source_artifact_width=item->artifact_width;item->source_artifact_height=item->artifact_height;}
        json_string(line, "sprite_id", item->id, sizeof(item->id));
        json_string(line, "timestamp_utc", item->time, sizeof(item->time));
        json_string(line, "summary", item->summary, sizeof(item->summary));
        app->sprite_evidence_count++;
    }
    fclose(fp);
    app->sprite_blob_count = 0;
    app->sprite_blob_sprite_id[0] = 0;
    app->sprite_blob_stage = -1;
}
/* agent: gpt-6.1-sol | 2026-10-05 | Load source and final sprite records | v 3.75.0 | cb9336 */

/* agent: gpt-6.1-sol | 2026-10-05 | Filter blobs by final artifact | v 3.75.0 | 4acf0a */
/* Load blob rectangles for AppSpriteEvidence* sprite from AppRun* run; return void. */
static void load_selected_sprite_blobs(App *app, AppRun *run, int stage, AppSpriteEvidence *sprite) {
    app->sprite_blob_count = 0;
    app->sprite_blob_stage = stage;
    copy_text(app->sprite_blob_sprite_id, sizeof(app->sprite_blob_sprite_id), sprite->id);
    char path[APP_PATH_CAP], line[32768], raw[64], id[32];
    path_join(path, sizeof(path), run->dir, "extra/processing.sprites.jsonl");
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    while (fgets(line, sizeof(line), fp)) {
        if (!json_raw(line, "stage_number", raw, sizeof(raw)) || atoi(raw) != stage) continue;
        if (stage == 0) {
            char artifact[APP_PATH_CAP];
            if (!json_string(line, "artifact", artifact, sizeof(artifact)) || strcmp(artifact, app->sprite_evidence_artifact)) continue;
        }
        if (!json_string(line, "sprite_id", id, sizeof(id)) || strcmp(id, sprite->id)) continue;
        bool components = stage == 10;
        const char *entry = strstr(line, components ? "\"components\":[" : "\"blobs\":[");
        const char *cursor = entry ? strchr(entry, '[') + 1 : NULL;
        while (cursor && *cursor && *cursor != ']' && app->sprite_blob_count < APP_MAX_SPRITE_BLOBS) {
            while (*cursor == ' ' || *cursor == ',' || *cursor == '\t') cursor++;
            if (*cursor != '{') break;
            const char *end = strchr(cursor, '}');
            if (!end) break;
            char object[1024]; size_t length = (size_t)(end - cursor + 1);
            if (length >= sizeof(object)) { cursor = end + 1; continue; }
            memcpy(object, cursor, length); object[length] = 0;
            AppSpriteBlob *blob = &app->sprite_blobs[app->sprite_blob_count];
            const char *bounds = strstr(object, components ? "\"bounds\"" : "\"artifact_bounds\"");
            const char *array = bounds ? strchr(bounds, '[') : NULL;
            if (array && sscanf(array, "[%d ,%d ,%d ,%d]", &blob->x, &blob->y, &blob->width, &blob->height) == 4) {
                json_string(object, components ? "component_id" : "blob_id", blob->id, sizeof(blob->id));
                json_raw(object, "pixels", raw, sizeof(raw)); blob->pixels = atoi(raw);
                const char *color = strstr(object, "\"average_color\"");
                const char *rgb = color ? strchr(color, '[') : NULL;
                if (rgb) (void)sscanf(rgb, "[%d ,%d ,%d]", &blob->color[0], &blob->color[1], &blob->color[2]);
                app->sprite_blob_count++;
            }
            cursor = end + 1;
        }
        break;
    }
    fclose(fp);
}
/* agent: gpt-6.1-sol | 2026-10-05 | Filter blobs by final artifact | v 3.75.0 | 4acf0a */

/* Return true if mouse position is over texture pixel x/y; return bool. */
static bool image_view_pixel(App *app, Texture2D texture, int x, int y, int width, int height, Vector2 mouse, int *px, int *py) {
    Rectangle rect = image_view_rect(app, texture, x, y, width, height);
    if (!CheckCollisionPointRec(mouse, (Rectangle){(float)x,(float)y,(float)width,(float)height}) || !CheckCollisionPointRec(mouse, rect)) return false;
    *px = (int)((mouse.x - rect.x) * texture.width / rect.width);
    *py = (int)((mouse.y - rect.y) * texture.height / rect.height);
    if (*px >= texture.width) *px = texture.width - 1;
    if (*py >= texture.height) *py = texture.height - 1;
    return *px >= 0 && *py >= 0;
}

/* Write DrawState* state to path through a sibling temporary file; return bool. */
static bool draw_export_atomic(DrawState *state, const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return false;
    char temp[APP_PATH_CAP];
    int temp_size = snprintf(temp, sizeof(temp), "%.*s.drawtmp-%ld%s", (int)(dot - path), path, (long)getpid(), dot);
    if (temp_size < 0 || (size_t)temp_size >= sizeof(temp)) return false;
    Image image = {state->pixels, state->width, state->height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if (!ExportImage(image, temp)) { unlink(temp); return false; }
    if (rename(temp, path)) { unlink(temp); return false; }
    return true;
}

/* agent: gpt-6.1-sol | 2026-10-04 | Run drawing saves asynchronously | v 3.55.0 | 3fbe9e */
/* Export DrawState* state on its worker thread and publish completion; return void*. */
static void *draw_save_worker(void *data) {
    DrawState *state = data;
    state->save_success = draw_export_atomic(state, state->save_target);
    atomic_store(&state->save_done, true);
    return NULL;
}

/* Start background export for DrawState* state to const char* path and int kind; return bool. */
static bool draw_save_start(App *app, const char *path, int kind) {
    DrawState *state = app->draw_state;
    if (!state || state->save_started || strlen(path) >= sizeof(state->save_target)) return false;
    copy_text(state->save_target, sizeof(state->save_target), path);
    state->save_kind = kind;
    atomic_store(&state->save_done, false);
    if (pthread_create(&state->save_thread, NULL, draw_save_worker, state)) { set_message(app, "Could not start image save"); return false; }
    state->save_started = true;
    set_message(app, "Saving image…");
    return true;
}

/* Choose unused source-name-copy PNG in const char* folder; return bool. */
static bool draw_unique_copy(char *out, size_t cap, const char *folder, const char *source) {
    const char *base = strrchr(source, '/'); base = base ? base + 1 : source;
    char stem[APP_PATH_CAP]; copy_text(stem, sizeof(stem), base);
    char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
    if (!*stem) copy_text(stem, sizeof(stem), "image");
    for (int i = 1; i < 10000; i++) {
        int n = i == 1 ? snprintf(out, cap, "%s/%s-copy.png", folder, stem) : snprintf(out, cap, "%s/%s-copy-%d.png", folder, stem, i);
        if (n < 0 || (size_t)n >= cap) return false;
        struct stat info;
        if (lstat(out, &info) != 0 && errno == ENOENT) return true;
    }
    return false;
}

/* Begin in-place async save for App* app and optional reference/run; return bool. */
static bool draw_save_in_place(App *app, AppReference *ref, AppRun *run) {
    DrawState *state = app->draw_state;
    if (!state || !draw_session_dirty(state)) return false;
    (void)ref; (void)run;
    return draw_save_start(app, state->path, 1);
}

/* Begin async copy save for App* app and optional reference/run; return bool. */
static bool draw_save_copy(App *app, AppReference *ref, AppRun *run) {
    DrawState *state = app->draw_state;
    if (!state || state->save_started) return false;
    char target[APP_PATH_CAP], folder[APP_PATH_CAP];
    if (ref) {
        if (app->ref_count >= APP_MAX_REFS) { set_message(app, "Resource limit reached"); return false; }
        copy_text(folder, sizeof(folder), app->refs_path);
    } else if (run) {
        if (run->draw_copy_count >= APP_MAX_OUTPUTS) { set_message(app, "Generation copy limit reached"); return false; }
        path_join(folder, sizeof(folder), run->dir, "extra/draw-copies");
        if (!make_dirs(folder)) { set_message(app, "Could not create copy folder"); return false; }
    } else return false;
    if (!draw_unique_copy(target, sizeof(target), folder, state->path)) { set_message(app, "Could not choose a filename for the copy"); return false; }
    return draw_save_start(app, target, ref ? 2 : 3);
}

/* Apply completed DrawState* save on the UI thread for App* app; return void. */
static void draw_save_update(App *app) {
    DrawState *state = app->draw_state;
    if (!state || !state->save_started || !atomic_load(&state->save_done)) return;
    pthread_join(state->save_thread, NULL); state->save_started = false;
    if (!state->save_success) { set_message(app, "Image save failed"); return; }
    int kind = state->save_kind, close_action = app->draw_close_action;
    char target[APP_PATH_CAP]; copy_text(target, sizeof(target), state->save_target);
    bool reference = kind == 2 || (kind == 1 && !strncmp(target, app->refs_path, strlen(app->refs_path)));
    if (reference) {
        scan_refs(app);
        for (int i = 0; i < app->ref_count; i++) if (!strcmp(app->refs[i].path, target)) { app->selected_ref = i; app->refs[i].selected = true; if (kind == 2) app->view = VIEW_LIBRARY; break; }
        if (kind == 2) save_reference_state(app);
    } else {
        for (int i = 0; i < app->run_count; i++) if (!strncmp(target, app->runs[i].dir, strlen(app->runs[i].dir))) {
            AppRun *run = &app->runs[i]; scan_artifacts(app, run);
            if (kind == 3) for (int j = 0; j < run->draw_copy_count; j++) if (!strcmp(run->draw_copies[j].path, target)) { run->selected_draw_copy = j; run->draw_copy_selected = true; run->stage_output_selected = false; }
            break;
        }
    }
    app->draw_state = NULL; app->draw_close_pending = false; draw_session_free(state);
    if (close_action) draw_apply_close_action(app, close_action);
    set_message(app, kind == 1 ? "Image saved" : "Image copy saved");
}
/* agent: gpt-6.1-sol | 2026-10-04 | Run drawing saves asynchronously | v 3.55.0 | 3fbe9e */

/* End DrawState* app session and optionally close its image viewer; return void. */
static void draw_session_discard(App *app, bool close_viewer) {
    draw_session_free((DrawState *)app->draw_state);
    app->draw_state = NULL; app->draw_close_pending = false; app->draw_close_action = 0;
    if (close_viewer) app->show_full_image = false;
}

/* Apply pending viewer action int action after a draw session closes; return void. */
static void draw_apply_close_action(App *app, int action) {
    app->draw_close_pending = false; app->draw_close_action = 0;
    if (action == 1) { app->show_full_image = false; app->edit_field[0] = 0; app->field_scroll = 0; }
    else if (action == 2) { app->settings_view = !app->settings_view; app->show_add_menu = false; app->show_full_image = false; app->edit_field[0] = 0; app->field_scroll = 0; }
    else if (action == 3) { app->view = VIEW_HOME; app->show_full_image = false; app->show_run_logs = false; }
}

/* Draw an unsaved draw-session close dialog; return selected prompt action. */
static int draw_close_dialog(void) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    Rectangle panel = {(w - 520) * 0.5f, (h - 190) * 0.5f, 520, 190};
    DrawRectangle(0, 0, w, h, (Color){0,0,0,190});
    DrawRectangleRec(panel, UI_PANEL); DrawRectangleLinesEx(panel, 2, UI_WARN);
    ui_draw_text("Unsaved drawing", (int)panel.x + 22, (int)panel.y + 24, 21, UI_TEXT);
    ui_draw_text("Save your pixel edits before closing?", (int)panel.x + 22, (int)panel.y + 62, 16, UI_MUTED);
    if (button((Rectangle){panel.x+20,panel.y+120,148,42}, "Save & close", false)) return PROMPT_CLOSE_SAVE;
    if (button((Rectangle){panel.x+186,panel.y+120,148,42}, "Discard", false)) return PROMPT_CLOSE_DISCARD;
    if (button((Rectangle){panel.x+352,panel.y+120,148,42}, "Cancel", false)) return PROMPT_CLOSE_CANCEL;
    return PROMPT_CLOSE_NONE;
}

/* Resolve a pending draw close choice for optional ref and run; return void. */
static void draw_handle_close_dialog(App *app, AppReference *ref, AppRun *run) {
    if (!app->draw_close_pending) return;
    DrawState *state = app->draw_state;
    if (state && state->save_started) { ui_draw_text("Saving image…", GetScreenWidth() / 2 - 55, GetScreenHeight() / 2, 16, UI_ACCENT); return; }
    int action = app->draw_close_action;
    if (!draw_session_dirty((DrawState *)app->draw_state)) {
        draw_session_discard(app, false); draw_apply_close_action(app, action); return;
    }
    int choice = draw_close_dialog();
    if (IsKeyPressed(KEY_ESCAPE)) choice = PROMPT_CLOSE_CANCEL;
    if (choice == PROMPT_CLOSE_SAVE) (void)draw_save_in_place(app, ref, run);
    else if (choice == PROMPT_CLOSE_DISCARD) { draw_session_discard(app, false); draw_apply_close_action(app, action); }
    else if (choice == PROMPT_CLOSE_CANCEL) { app->draw_close_pending = false; app->draw_close_action = 0; }
}
/* agent: gpt-6.1-sol | 2026-10-04 | Add drawing session history | v 3.50.0 | 721428 */
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

/* agent: gpt-6.1-sol | 2026-10-05 | Use execution-ordered processor stage IDs | v 3.72.0 | ce9186 */
/* agent: gpt-6 | 2026-10-05 | Hide generic diagnostic descriptions | v 3.88.0 | 08b004 */
/* Describe stage int number with its execution-ordered stage meaning; return const char*. */
static const char *stage_description(int number) {
    switch (number) {
        case 100: return "What: source pixels. Why: baseline for every measured change.";
        case 97: return "What: background mask. How: connected background and tolerance.";
        case 94: return "What: blob ownership. How: component to sprite-cell assignment.";
        case 91: return "What: extracted sprite crops. How: source bounds and padding.";
        case 80: return "What: pixel lattice. How: selected pitch, phase, and confidence.";
        case 77: return "";
        case 74: return "";
        case 71: return "";
        case 60: return "What: reconstructed sprites. Why: inspect logical pixels before expansion.";
        case 57: return "What: reconstructed output. How: shared pitch and local measurements.";
        case 50: return "What: uncertain edge pixels. Why: show candidates for alpha resolution.";
        case 47: return "What: edge support. How: neighboring alpha support scores.";
        case 44: return "What: alpha edits. How: per-sprite changes and topology guards.";
        case 35: return "What: uniform logical grid. Why: expose occupied cells before cleanup.";
        case 32: return "What: outside-magenta mask. How: classified source pixels by sprite.";
        case 29: return "What: cleanup support. Why: bound removal to supported pixels.";
        case 26: return "What: bounded cleanup. How: removed pixels and support mode.";
        case 23: return "What: magenta cleanup. How: supported removal per sprite.";
        case 10: return "What: connected components. How: owner sprite, bounds, and pixel counts.";
        case 7: return "What: boundary recovery. How: recovered lanes and unresolved pixels.";
        case 4: return "What: assembled spritesheet. How: preserved cells and anchor.";
        default: return "";
    }
}


/* agent: gpt-6.1-sol | 2026-10-04 | Remove generated output arrow controls | v 3.37.0 | f5e94d */
/* agent: gpt-6.1-sol | 2026-10-04 | Add reset transform image button | v 3.30.0 | 5bd415 */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Open images from viewer footer | v 3.35.0 | 63bf6c */
/* agent: gpt-6.1-sol | 2026-10-04 | Add generation draw close action | v 3.64.0 | 39b151 */
/* Draw AppArtifact* image, optional AppReference* ref and AppRun* run in int bounds using App* app; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Show loading state in image views | v 3.55.0 | 4420da */
/* Draw AppArtifact* image in App* app with optional AppReference* ref/AppRun* run and int bounds; return void. */
/* agent: gpt-6.1-sol | 2026-10-05 | Toggle stage details frame | v 3.74.0 | cdd038 */
/* Draw metadata for AppArtifact* image and AppRun* run before the image area; return void. */
/* agent: gpt-6.1-sol | 2026-10-05 | Show selected sprite notes in info frame | v 3.75.0 | f5509b */
/* agent: gpt-6 | 2026-10-05 | Show useful selected sprite facts | v 3.88.0 | 1be6f1 */
/* Return a short label for int processor stage number; return const char*. */
/* agent: gpt-6 | 2026-10-05 | Name sprite processing versions | v 3.97.0 | 6782ad */
static const char *sprite_version_label(int number) {
    switch (number) {
        case -1: case 100: return "Source";
        case 0: return "Final";
        case 97: return "Background";
        case 94: return "Ownership";
        case 91: return "Extract";
        case 80: return "Lattice";
        case 60: return "Reconstruct";
        case 23: return "Cleanup";
        default: return TextFormat("Stage %02d", number);
    }
}
/* agent: gpt-6 | 2026-10-05 | Name sprite processing versions | v 3.97.0 | 6782ad */
/* agent: gpt-6 | 2026-10-05 | Label selected and hovered versions | v 3.97.0 | 3db3d0 */
static void draw_image_metadata(App *app, AppArtifact *image, AppRun *run, Rectangle frame) {
    if (!image || !run || app->draw_state || !app->sprite_inspect_enabled) return;
    if (app->sprite_evidence_selected < 0 || app->sprite_evidence_selected >= app->sprite_evidence_count) return;
    AppSpriteEvidence *sprite = &app->sprite_evidence[app->sprite_evidence_selected];
    float card_w = fminf(frame.width - 20, 340.0f);
    Rectangle card = {frame.x + 12, frame.y + 12, card_w, 84};
    DrawRectangleRec(card, (Color){12, 14, 19, 232});
    DrawRectangleLinesEx(card, 1, UI_ACCENT);
    ui_draw_text(sprite->id, (int)card.x + 10, (int)card.y + 7, 14, UI_TEXT);
    ui_draw_text(TextFormat("row %d · column %d · %d × %d px", sprite->row, sprite->col, sprite->width, sprite->height),
                 (int)card.x + 10, (int)card.y + 28, 12, UI_MUTED);
    ui_draw_text(TextFormat("Selected: %s", sprite_version_label(app->sprite_version_number)),
                 (int)card.x + 10, (int)card.y + 48, 12, UI_TEXT);
    if (app->sprite_version_hovered >= 0 && app->sprite_version_hovered_number != app->sprite_version_number)
        ui_draw_text(TextFormat("Hover: %s", sprite_version_label(app->sprite_version_hovered_number)),
                     (int)card.x + 10, (int)card.y + 66, 12, UI_ACCENT);
}
/* agent: gpt-6 | 2026-10-05 | Label selected and hovered versions | v 3.97.0 | 3db3d0 */
/* agent: gpt-6 | 2026-10-05 | Show useful selected sprite facts | v 3.88.0 | 1be6f1 */

/* agent: gpt-6 | 2026-10-05 | Choose version crops per sprite | v 3.88.0 | 4c06d7 */
/* agent: gpt-6 | 2026-10-05 | Attach settings to pinned stage | v 3.88.0 | 592e97 */
/* Return AppRun* run's artifact at sprite version int index; return AppArtifact* or NULL. */
static AppArtifact *sprite_version_artifact(AppRun *run,int index) {
    if(index==0)return &run->generated;
    index--;
    for(int i=0;i<run->stage_count;i++)if(run->stages[i].number!=4){if(index==0)return &run->stages[i];index--;}
    return run->output_count&&index==0?&run->outputs[run->output_count-1]:NULL;
}
/* agent: gpt-6 | 2026-10-05 | Attach settings to pinned stage | v 3.88.0 | 592e97 */
/* Find AppRun* run's version index for AppArtifact* image; return int or -1. */
/* agent: gpt-6 | 2026-10-05 | Map selected artifact to version index | v 3.97.0 | 6160a7 */
static int sprite_version_index(AppRun *run,AppArtifact *image) {
    int count=1;
    if(image==&run->generated)return 0;
    for(int i=0;i<run->stage_count;i++)if(run->stages[i].number!=4){if(&run->stages[i]==image)return count;count++;}
    for(int i=0;i<run->output_count;i++)if(&run->outputs[i]==image)return count;
    return -1;
}
/* agent: gpt-6 | 2026-10-05 | Map selected artifact to version index | v 3.97.0 | 6160a7 */
/* Resolve a sprite-specific preview path from stage metadata in AppRun* run; return bool. */
/* agent: gpt-6 | 2026-10-05 | Map pipeline assets to sprites | v 3.97.0 | 57f166 */
static bool sprite_stage_preview_path(AppRun *run, int stage_number, const char *sprite_id, char *out, size_t out_size) {
    const char *folder = stage_number == 91 ? "crops" : stage_number == 60 ? "logical-sprites" : stage_number == 23 ? "clean-sprites" : NULL;
    if (!folder || !run->processor_manifest_path[0]) return false;
    char manifest[APP_TEXT_CAP];
    if (!read_text(run->processor_manifest_path, manifest, sizeof(manifest))) return false;
    char record[2048], metadata[APP_PATH_CAP], number[32];
    const char *key = manifest;
    while ((key = strstr(key, "\"number\""))) {
        const char *begin = key; while (begin > manifest && *begin != '{') begin--;
        const char *end = strchr(key, '}');
        if (*begin == '{' && end && end - begin < (ptrdiff_t)sizeof(record)) {
            size_t length = (size_t)(end - begin + 1);
            memcpy(record, begin, length); record[length] = 0;
            if (json_raw(record, "number", number, sizeof(number)) && atoi(number) == stage_number &&
                json_string(record, "metadata", metadata, sizeof(metadata))) break;
        }
        key += 8;
    }
    if (!key || !strchr(key, '}')) return false;
    char *slash = strrchr(metadata, '/'); if (!slash) return false; *slash = 0;
    char directory[APP_PATH_CAP]; path_join(directory, sizeof(directory), metadata, folder);
    int written = snprintf(out, out_size, "%s/%s.png", directory, sprite_id);
    return written > 0 && (size_t)written < out_size && file_exists(out);
}
/* agent: gpt-6 | 2026-10-05 | Map pipeline assets to sprites | v 3.97.0 | 57f166 */
/* Return cached sprite-specific AppArtifact* for a pipeline item or the original; return AppArtifact*. */
/* agent: gpt-6 | 2026-10-05 | Cache per-sprite pipeline versions | v 3.97.0 | 1ec90a */
static AppArtifact *sprite_version_view_artifact(AppRun *run, AppArtifact *artifact, AppSpriteEvidence *sprite) {
    char path[APP_PATH_CAP];
    if (!strcmp(run->sprite_version_preview_id, sprite->id))
        for (int i = 0; i < run->sprite_version_preview_count; i++)
            if (run->sprite_version_previews[i].number == artifact->number) return &run->sprite_version_previews[i];
    if (!sprite_stage_preview_path(run, artifact->number, sprite->id, path, sizeof(path))) return artifact;
    for (int i = 0; i < run->sprite_version_preview_count; i++)
        if (!strcmp(run->sprite_version_previews[i].path, path)) return &run->sprite_version_previews[i];
    if (run->sprite_version_preview_count >= APP_MAX_STAGES + 2) return artifact;
    AppArtifact *preview = &run->sprite_version_previews[run->sprite_version_preview_count++];
    memset(preview, 0, sizeof(*preview));
    copy_text(preview->path, sizeof(preview->path), path);
    copy_text(preview->name, sizeof(preview->name), artifact->name);
    preview->number = artifact->number;
    return preview;
}
/* agent: gpt-6 | 2026-10-05 | Cache per-sprite pipeline versions | v 3.97.0 | 1ec90a */
/* Draw viewer-style transparency checker cells clipped to Rectangle bounds; return void. */
/* agent: gpt-6 | 2026-10-05 | Show sprite alpha on checkerboard | v 3.97.0 | 9905ed */
static void draw_sprite_checkerboard(Rectangle bounds) {
    const int cell = 12;
    int start_x = (int)floorf(bounds.x / cell) * cell;
    int start_y = (int)floorf(bounds.y / cell) * cell;
    int screen_h = GetScreenHeight();
    for (int y = start_y; y < bounds.y + bounds.height; y += cell) {
        for (int x = start_x; x < bounds.x + bounds.width; x += cell) {
            int checker = ((int)floorf((float)x / cell) + (int)floorf((float)(screen_h - y) / cell)) & 1;
            Color color = checker ? (Color){48, 48, 48, 255} : (Color){31, 31, 31, 255};
            Rectangle tile = {fmaxf(bounds.x, (float)x), fmaxf(bounds.y, (float)y),
                              fminf(bounds.x + bounds.width, (float)(x + cell)) - fmaxf(bounds.x, (float)x),
                              fminf(bounds.y + bounds.height, (float)(y + cell)) - fmaxf(bounds.y, (float)y)};
            DrawRectangleRec(tile, color);
        }
    }
}
/* agent: gpt-6 | 2026-10-05 | Show sprite alpha on checkerboard | v 3.97.0 | 9905ed */
/* Draw AppArtifact* view's sprite crop inside Rectangle bounds without distortion; return void. */
/* agent: gpt-6 | 2026-10-05 | Fit version crops inside sprite bounds | v 3.97.0 | 4ae09f */
static void draw_sprite_version_crop(AppArtifact *artifact, AppArtifact *view, AppSpriteEvidence *sprite, Rectangle bounds) {
    Texture2D texture = view->texture.id ? view->texture : view->thumb;
    bool source_space = view == artifact && artifact->number != 0 && artifact->number != 4;
    float sx = source_space ? sprite->source_x : sprite->x, sy = source_space ? sprite->source_y : sprite->y;
    float sw = source_space ? sprite->source_width : sprite->width, sh = source_space ? sprite->source_height : sprite->height;
    float aw = source_space ? sprite->source_artifact_width : sprite->artifact_width;
    float ah = source_space ? sprite->source_artifact_height : sprite->artifact_height;
    Rectangle crop = view != artifact ? (Rectangle){0, 0, (float)texture.width, (float)texture.height} :
        (Rectangle){sx * texture.width / fmaxf(1, aw), sy * texture.height / fmaxf(1, ah),
                    sw * texture.width / fmaxf(1, aw), sh * texture.height / fmaxf(1, ah)};
    crop.x = fmaxf(0, crop.x); crop.y = fmaxf(0, crop.y);
    crop.width = fminf(crop.width, texture.width - crop.x); crop.height = fminf(crop.height, texture.height - crop.y);
    draw_sprite_checkerboard(bounds);
    if (!texture.id || crop.width <= 0 || crop.height <= 0) return;
    float scale = fminf(bounds.width / crop.width, bounds.height / crop.height);
    Rectangle destination = {bounds.x + (bounds.width - crop.width * scale) * .5f,
                             bounds.y + (bounds.height - crop.height * scale) * .5f,
                             crop.width * scale, crop.height * scale};
    DrawTexturePro(texture, crop, destination, (Vector2){0, 0}, 0, WHITE);
}
/* agent: gpt-6 | 2026-10-05 | Fit version crops inside sprite bounds | v 3.97.0 | 4ae09f */
/* agent: gpt-6 | 2026-10-05 | Pin the visible sprite version | v 3.88.0 | 85df6e */
/* Draw sprite crops from AppRun* run for AppSpriteEvidence* sprite inside Rectangle box, using Rectangle viewport; return void. */
/* agent: gpt-6 | 2026-10-05 | Compare pinned and hovered sprite versions | v 3.97.0 | 04afa2 */
static void draw_sprite_version_rail(App *app, AppRun *run, AppArtifact *current, AppSpriteEvidence *sprite, Rectangle viewport, Rectangle box) {
    AppArtifact *items[APP_MAX_STAGES + 2];
    AppArtifact *views[APP_MAX_STAGES + 2];
    int count=0;
    items[count++]=&run->generated;
    for(int i=0;i<run->stage_count&&count<APP_MAX_STAGES+1;i++)if(run->stages[i].number!=4)items[count++]=&run->stages[i];
    if(run->output_count&&count<APP_MAX_STAGES+2){AppArtifact *output=&run->outputs[run->output_count-1];for(int i=0;i<run->output_count;i++)if(&run->outputs[i]==current)output=current;items[count++]=output;}
    if(app->sprite_version_selected<0||app->sprite_version_selected>=count)app->sprite_version_selected=0;
    app->sprite_version_number=items[app->sprite_version_selected]->number;
    if (strcmp(run->sprite_version_preview_id, sprite->id)) {
        clear_sprite_version_previews(run);
        copy_text(run->sprite_version_preview_id, sizeof(run->sprite_version_preview_id), sprite->id);
    }
    Vector2 mouse=GetMousePosition();int hovered=-1;
    float rail_w=100.0f,rail_h=ceilf(count/3.0f)*34.0f;
    float rail_x=box.x-rail_w-8.0f,rail_y=box.y;
    if(rail_x<viewport.x+4.0f)rail_x=box.x+box.width+8.0f;
    if(rail_x+rail_w>viewport.x+viewport.width)rail_x=viewport.x+viewport.width-rail_w-4.0f;
    if(rail_y+rail_h>viewport.y+viewport.height)rail_y=viewport.y+viewport.height-rail_h;
    if(rail_y<viewport.y)rail_y=viewport.y;
    int cols=3;
    for(int i=0;i<count;i++){
        AppArtifact *artifact=items[i];
        AppArtifact *view=sprite_version_view_artifact(run,artifact,sprite);views[i]=view;
        if(!view->thumb.id&&!view->texture.id&&!view->loading&&view->path[0]&&queue_image(app,view->path,240,true,false))view->loading=true;
        int row=i/cols,col=i%cols;
        Rectangle tile={rail_x+col*34.0f+(row%2?10.0f:0.0f),rail_y+row*34.0f,32,32};
        bool over=CheckCollisionPointRec(mouse,tile);if(over)hovered=i;
        DrawRectangleRec(tile,(Color){12,14,19,220});
        Texture2D tex=view->thumb.id?view->thumb:view->texture;
        if(tex.id){
            bool source_space=view==artifact&&artifact->number!=0&&artifact->number!=4;
            float sx=source_space?sprite->source_x:sprite->x,sy=source_space?sprite->source_y:sprite->y;
            float sw=source_space?sprite->source_width:sprite->width,sh=source_space?sprite->source_height:sprite->height;
            float aw=source_space?sprite->source_artifact_width:sprite->artifact_width,ah=source_space?sprite->source_artifact_height:sprite->artifact_height;
            Rectangle crop=view!=artifact?(Rectangle){0,0,(float)tex.width,(float)tex.height}:(Rectangle){sx*tex.width/fmaxf(1,aw),sy*tex.height/fmaxf(1,ah),sw*tex.width/fmaxf(1,aw),sh*tex.height/fmaxf(1,ah)};
            crop.x=fmaxf(0,crop.x);crop.y=fmaxf(0,crop.y);crop.width=fminf(crop.width,tex.width-crop.x);crop.height=fminf(crop.height,tex.height-crop.y);
            if(crop.width>0&&crop.height>0){float scale=fminf(28.0f/crop.width,28.0f/crop.height);Rectangle dst={tile.x+(32-crop.width*scale)*.5f,tile.y+(32-crop.height*scale)*.5f,crop.width*scale,crop.height*scale};DrawTexturePro(tex,crop,dst,(Vector2){0,0},0,WHITE);}
        }
        DrawRectangleLinesEx(tile,app->sprite_version_selected==i?2:1,over||app->sprite_version_selected==i?UI_ACCENT:UI_PANEL_ALT);
        if(over&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))app->sprite_version_selected=i;
    }
    app->sprite_version_number=items[app->sprite_version_selected]->number;
    int preview=hovered>=0?hovered:app->sprite_version_selected;
    app->sprite_version_hovered=hovered;
    app->sprite_version_hovered_number=hovered>=0?items[hovered]->number:app->sprite_version_number;
    if(preview>=0&&preview<count){
        int selected=app->sprite_version_selected;
        AppArtifact *selected_view=views[selected],*hovered_view=views[preview];
        if(!selected_view->texture.id&&!selected_view->loading&&selected_view->path[0]&&queue_image(app,selected_view->path,240,true,true))selected_view->loading=true;
        if(!hovered_view->texture.id&&!hovered_view->loading&&hovered_view->path[0]&&queue_image(app,hovered_view->path,240,true,true))hovered_view->loading=true;
        if(hovered>=0&&hovered!=selected){
            draw_sprite_version_crop(items[selected],selected_view,sprite,box);
            BeginScissorMode((int)(box.x+box.width*.5f),(int)box.y,(int)ceilf(box.width*.5f),(int)box.height);
            draw_sprite_version_crop(items[hovered],hovered_view,sprite,box);
            EndScissorMode();
            DrawLine((int)(box.x+box.width*.5f),(int)box.y,(int)(box.x+box.width*.5f),(int)(box.y+box.height),UI_ACCENT);
        }else draw_sprite_version_crop(items[selected],selected_view,sprite,box);
        DrawRectangleLinesEx(box,1.5f,UI_ACCENT);
    }
}
/* agent: gpt-6 | 2026-10-05 | Compare pinned and hovered sprite versions | v 3.97.0 | 04afa2 */
/* agent: gpt-6 | 2026-10-05 | Choose version crops per sprite | v 3.88.0 | 4c06d7 */
/* agent: gpt-6.1-sol | 2026-10-05 | Draw info banner above sprite overlays | v 3.76.0 | 73ac1b */
/* agent: gpt-6 | 2026-10-05 | Fit previews to saved sprite bounds | v 3.97.0 | c80076 */
/* agent: gpt-6 | 2026-10-05 | Preserve selected version on inspect toggle | v 3.97.0 | 543997 */
static void draw_shared_image_view(App *app, AppArtifact *image, AppReference *ref, AppRun *run, int x, int y, int width, int height) {
    /* agent: gpt-6.1-sol | 2026-10-04 | Touch selected cache entries | v 3.61.0 | 4c0bb0 */
    if (ref && ref->full.id) ref->full_used_at = GetTime();
    else if (ref && ref->thumb.id) ref->thumb_used_at = GetTime();
    if (run && image && image->texture.id) {
        AppArtifact *target = find_artifact(app, image->path);
        if (target) target->full_used_at = GetTime();
    } else if (run && image && image->thumb.id) {
        AppArtifact *target = find_artifact(app, image->path);
        if (target) target->thumb_used_at = GetTime();
    }
    if (ref && !ref->full.id && !ref->loading && queue_image(app, ref->path, 160, true, true)) ref->loading = true;
    if (ref && image) image->loading = ref->loading;
    if (run && image && !image->texture.id) {
        AppArtifact *target = find_artifact(app, image->path);
        if (target && !target->loading && queue_image(app, target->path, 240, false, true)) target->loading = true;
        if (target) image->loading = target->loading;
    }
    /* agent: gpt-6.1-sol | 2026-10-04 | Touch selected cache entries | v 3.61.0 | 4c0bb0 */
    if (!image || (!image->texture.id && !image->thumb.id)) {
        if (image && image->loading) {
            draw_loading_marker(x + width / 2, y + height / 2 - 8, UI_ACCENT);
            ui_draw_text("Loading image", x + width / 2 - 48, y + height / 2 + 12, 14, UI_MUTED);
        } else ui_draw_text("Image is not available yet.", x, y, 18, UI_MUTED);
        return;
    }
    const char *path = image->path;
    if (ref) draw_reference_name_field(app, ref, (Rectangle){(float)x, (float)(y - 54), fminf(420, (float)width - 360), 36});
    else if (!run) ui_draw_text(image->name, x, y - 42, 17, UI_TEXT);
    DrawState *draw = (DrawState *)app->draw_state;
    if (run && !draw) load_sprite_evidence(app, run, image);
    bool sprite_selected=run&&!draw&&app->sprite_inspect_enabled&&app->sprite_evidence_selected>=0&&app->sprite_evidence_selected<app->sprite_evidence_count;
    if(sprite_selected){load_run_pixel_settings(run);AppArtifact *version=sprite_version_artifact(run,app->sprite_version_selected);if(version)app->sprite_version_number=version->number;
        AppSpriteEvidence *selected=&app->sprite_evidence[app->sprite_evidence_selected];copy_text(run->selected_sprite_id,sizeof(run->selected_sprite_id),selected->id);
        run->pixel_sprite_scope=app->sprite_version_number==80;AppSpriteSettings *override=run->pixel_sprite_scope?sprite_settings_for(run,selected->id,false):NULL;run->sprite_settings=override?override->settings:run->pixel_settings;
    }
    int frame_y = y, footer_h = 48;
    int frame_h = height;
    if (frame_h < footer_h + 1) frame_h = footer_h + 1;
    Rectangle frame = {(float)x, (float)frame_y, (float)width, (float)frame_h};
    DrawRectangleRec(frame, UI_PANEL);
    DrawRectangleLinesEx(frame, 1, UI_PANEL_ALT);
    int image_y = frame_y + 1, image_h = frame_h - footer_h - 2;
    if (image_h < 1) image_h = 1;
    Texture2D view_texture = draw ? draw->texture : image->texture.id ? image->texture : image->thumb;
    /* agent: gpt-6.1-sol | 2026-10-05 | Restore top-left image metadata overlay | v 3.71.0 | adb730 */
    draw_image_view(app, view_texture, x, image_y, width, image_h);
    if (run && !draw && app->sprite_inspect_enabled && app->sprite_evidence_count > 0) {
        Rectangle image_rect = image_view_rect(app, view_texture, x, image_y, width, image_h);
        Vector2 mouse = GetMousePosition();
        int hovered = -1;
        float smallest = FLT_MAX;
        for (int i = 0; i < app->sprite_evidence_count; i++) {
            AppSpriteEvidence *item = &app->sprite_evidence[i];
            float artifact_w = item->artifact_width > 0 ? (float)item->artifact_width : (float)view_texture.width;
            float artifact_h = item->artifact_height > 0 ? (float)item->artifact_height : (float)view_texture.height;
            Rectangle box = {image_rect.x + (float)item->x * image_rect.width / artifact_w,
                             image_rect.y + (float)item->y * image_rect.height / artifact_h,
                             (float)item->width * image_rect.width / artifact_w,
                             (float)item->height * image_rect.height / artifact_h};
            DrawRectangleLinesEx(box, 1.5f, app->sprite_evidence_selected == i ? UI_ACCENT : (Color){70, 220, 210, 220});
            ui_draw_text(item->id, (int)box.x + 3, (int)box.y + 3, 11, WHITE);
            if (CheckCollisionPointRec(mouse, box) && box.width * box.height < smallest) { smallest = box.width * box.height; hovered = i; }
        }
        if (hovered >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered != app->sprite_evidence_selected) {
            app->sprite_evidence_selected = hovered;
            app->sprite_version_selected=sprite_version_index(run,image);if(app->sprite_version_selected<0)app->sprite_version_selected=0;
            if(app->selected_run>=0&&app->selected_run<app->run_count){AppRun *selected_run=&app->runs[app->selected_run];copy_text(selected_run->selected_sprite_id,sizeof(selected_run->selected_sprite_id),app->sprite_evidence[hovered].id);AppSpriteSettings *override=sprite_settings_for(selected_run,selected_run->selected_sprite_id,false);selected_run->sprite_settings=override?override->settings:selected_run->pixel_settings;selected_run->pixel_sprite_scope=true;}
            load_selected_sprite_blobs(app, run, app->sprite_evidence_stage, &app->sprite_evidence[hovered]);
        }
        if (app->sprite_evidence_selected >= 0 && app->sprite_evidence_selected < app->sprite_evidence_count) {
            AppSpriteEvidence *sprite = &app->sprite_evidence[app->sprite_evidence_selected];
            if (app->sprite_blob_stage != app->sprite_evidence_stage || strcmp(app->sprite_blob_sprite_id, sprite->id))
                load_selected_sprite_blobs(app, run, app->sprite_evidence_stage, sprite);
            for (int i = 0; i < app->sprite_blob_count; i++) {
                AppSpriteBlob *blob = &app->sprite_blobs[i];
                float artifact_w = sprite->artifact_width > 0 ? (float)sprite->artifact_width : (float)view_texture.width;
                float artifact_h = sprite->artifact_height > 0 ? (float)sprite->artifact_height : (float)view_texture.height;
                Rectangle blob_box = {image_rect.x + (float)blob->x * image_rect.width / artifact_w,
                                      image_rect.y + (float)blob->y * image_rect.height / artifact_h,
                                      (float)blob->width * image_rect.width / artifact_w,
                                      (float)blob->height * image_rect.height / artifact_h};
                DrawRectangleRec(blob_box, (Color){255, 196, 0, 38});
                DrawRectangleLinesEx(blob_box, 2.5f, (Color){255, 196, 0, 255});
            }
            float selected_w=sprite->artifact_width>0?(float)sprite->artifact_width:(float)view_texture.width;
            float selected_h=sprite->artifact_height>0?(float)sprite->artifact_height:(float)view_texture.height;
            Rectangle selected_box={image_rect.x+(float)sprite->x*image_rect.width/selected_w,image_rect.y+(float)sprite->y*image_rect.height/selected_h,
                                    (float)sprite->width*image_rect.width/selected_w,(float)sprite->height*image_rect.height/selected_h};
            draw_sprite_version_rail(app,run,image,sprite,image_rect,selected_box);
        }
    }
    if (run && app->sprite_inspect_enabled) draw_image_metadata(app, image, run, (Rectangle){(float)x, (float)image_y, (float)width, (float)image_h});
    /* agent: gpt-6.1-sol | 2026-10-05 | Restore top-left image metadata overlay | v 3.71.0 | adb730 */
    if (image->loading) draw_loading_marker(x + width - 20, image_y + 18, UI_ACCENT);
    if (draw) {
        if (IsKeyPressed(KEY_ESCAPE) && !app->draw_close_pending) {
            app->draw_close_pending = true; app->draw_close_action = 0;
        }
        int px = 0, py = 0; Vector2 mouse = GetMousePosition();
        bool inside = image_view_pixel(app, view_texture, x, image_y, width, image_h, mouse, &px, &py);
        bool changed = false;
        if (!app->draw_close_pending && !draw->save_started && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside) changed = draw_begin_stroke(draw, px, py);
        else if (!app->draw_close_pending && !draw->save_started && draw->active && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && inside) {
            changed = draw_stroke_line(draw, draw->last_x, draw->last_y, px, py);
            draw->last_x = px; draw->last_y = py;
        }
        if (draw->active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            (void)draw_end_stroke(draw); changed = true;
        }
        if (changed) UpdateTexture(draw->texture, draw->pixels);
    }
    int footer_y = frame_y + frame_h - footer_h;
    int action_y=footer_y+6;
    DrawRectangle(x + 1, footer_y, width - 2, footer_h - 1, UI_PANEL);
    if (draw) {
        bool dirty = draw_session_dirty(draw);
        int control_x = x + width - (run ? 556 : 474);
        const char *tips = width < 850 ? "Drag: draw · Wheel: zoom · Middle: pan" : "Drag: draw  ·  Wheel: zoom  ·  Middle drag: pan";
        ui_draw_text(TextFormat("%d x %d  ·  %s", draw->width, draw->height, tips), x + 12, footer_y + 17, width < 850 ? 10 : 13, UI_MUTED);
        if (!draw->save_started && button((Rectangle){control_x,footer_y+6,78,36},"Undo",draw->cursor>0) && draw_history_step(draw,false)) UpdateTexture(draw->texture,draw->pixels);
        if (!draw->save_started && button((Rectangle){control_x+84,footer_y+6,78,36},"Redo",draw->cursor<draw->stroke_count) && draw_history_step(draw,true)) UpdateTexture(draw->texture,draw->pixels);
        if (!draw->save_started && button((Rectangle){control_x+168,footer_y+6,76,36},"Save",dirty) && dirty) (void)draw_save_in_place(app,ref,run);
        if (!draw->save_started && button((Rectangle){control_x+250,footer_y+6,126,36},"Save as copy",dirty) && dirty) (void)draw_save_copy(app,ref,run);
        if (draw->save_started) ui_draw_text("Saving…", control_x + 202, footer_y + 16, 14, UI_ACCENT);
        if (!draw->save_started && button((Rectangle){control_x+392,footer_y+6,82,36},"Cancel",false)) {
            app->draw_close_pending = true; app->draw_close_action = 0;
        }
        if (run && !draw->save_started && button((Rectangle){control_x+480,footer_y+6,66,36},"Close",false)) {
            if (dirty) { app->draw_close_pending = true; app->draw_close_action = 3; }
            else { draw_session_discard(app, false); app->view = VIEW_HOME; }
        }
    } else {
        ui_draw_text(TextFormat("%d x %d  ·  Wheel: zoom  ·  Middle drag: pan", image->width, image->height), x + 12, footer_y + 16, 14, UI_MUTED);
        if (run) {
            Rectangle export_button={(float)(x+width-280),(float)action_y,82,36};
            if(button(export_button,"Export",false)){char destination[APP_PATH_CAP],src[APP_PATH_CAP*2],dst[APP_PATH_CAP*2],request[APP_PATH_CAP*4+128];if(choose_save_png(destination,sizeof(destination))){json_escape(path,src,sizeof(src));json_escape(destination,dst,sizeof(dst));snprintf(request,sizeof(request),"{\"action\":\"output.export\",\"path\":\"%s\",\"destination\":\"%s\"}",src,dst);AppDispatch(app,request,s_action_response,sizeof(s_action_response));}}
            Rectangle inspect_toggle = {(float)(x + width - 180), (float)action_y, 36, 36};
            bool inspect_hover = CheckCollisionPointRec(GetMousePosition(), inspect_toggle);
            if (button(inspect_toggle, "", app->sprite_inspect_enabled)) {
                app->sprite_inspect_enabled = !app->sprite_inspect_enabled;
                if (app->sprite_inspect_enabled) {
                    int version = sprite_version_index(run, image);
                    if (version >= 0) app->sprite_version_selected = version;
                    app->sprite_version_hovered = -1;
                }
            }
            Color inspect_tone = app->sprite_inspect_enabled || inspect_hover ? UI_ACCENT : UI_TEXT;
            DrawCircleLines((int)inspect_toggle.x + 15, (int)inspect_toggle.y + 16, 7, inspect_tone);
            DrawLine((int)inspect_toggle.x + 20, (int)inspect_toggle.y + 21, (int)inspect_toggle.x + 27, (int)inspect_toggle.y + 28, inspect_tone);
            if (inspect_hover) ui_draw_text("Inspect sprites and pipeline stages", (int)inspect_toggle.x - 185, footer_y - 18, 12, UI_TEXT);
        } else if(ref&&!ref->is_text&&!ref->is_profile) {
            Rectangle convert_button={(float)(x+width-220),(float)action_y,112,36};
            if(button(convert_button,"Convert pixels",false)){char source[APP_PATH_CAP*2],request[APP_PATH_CAP*2+128];json_escape(path,source,sizeof(source));snprintf(request,sizeof(request),"{\"action\":\"pixel.convert\",\"path\":\"%s\"}",source);AppDispatch(app,request,s_action_response,sizeof(s_action_response));}
        }
        Rectangle draw_button = {(float)(x + width - 132), (float)action_y, 36, 36};
        bool draw_hovered = CheckCollisionPointRec(GetMousePosition(), draw_button);
        if (button(draw_button, "", false)) (void)draw_session_begin(app, path);
        Color draw_tone = draw_hovered ? UI_ACCENT : UI_TEXT;
        DrawLine((int)draw_button.x+10,(int)draw_button.y+25,(int)draw_button.x+24,(int)draw_button.y+11,draw_tone);
        DrawLine((int)draw_button.x+21,(int)draw_button.y+10,(int)draw_button.x+26,(int)draw_button.y+15,draw_tone);
        DrawLine((int)draw_button.x+9,(int)draw_button.y+27,(int)draw_button.x+15,(int)draw_button.y+25,draw_tone);
    }
    if (!draw) {
    Rectangle open_button = {(float)(x + width - 90), (float)action_y, 36, 36};
    bool open_hovered = CheckCollisionPointRec(GetMousePosition(), open_button);
    if (button(open_button, "", false)) open_path(path);
    Color open_tone = open_hovered ? UI_ACCENT : UI_TEXT;
    int ox = (int)(open_button.x + 10), oy = (int)(open_button.y + 10);
    DrawRectangleLines(ox, oy + 3, 13, 13, open_tone);
    DrawLine(ox + 7, oy + 9, ox + 17, oy - 1, open_tone);
    DrawLine(ox + 11, oy - 1, ox + 17, oy - 1, open_tone);
    DrawLine(ox + 17, oy - 1, ox + 17, oy + 5, open_tone);
    Rectangle reset_button = {(float)(x + width - 48), (float)action_y, 36, 36};
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
    }
    Rectangle image_area = {(float)x, (float)image_y, (float)width, (float)image_h};
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(GetMousePosition(), image_area)) {
        DrawState *active_draw = (DrawState *)app->draw_state;
        if (active_draw && draw_session_dirty(active_draw)) { app->draw_close_pending = true; app->draw_close_action = run ? 3 : 1; }
        else if (active_draw) { draw_session_discard(app, false); if (run) app->view = VIEW_HOME; else app->show_full_image = false; }
        else if (run) app->view = VIEW_HOME;
        else app->show_full_image = false;
    }
}
/* agent: gpt-6 | 2026-10-05 | Fit previews to saved sprite bounds | v 3.97.0 | c80076 */
/* agent: gpt-6 | 2026-10-05 | Preserve selected version on inspect toggle | v 3.97.0 | 543997 */
/* agent: gpt-6.1-sol | 2026-10-05 | Draw info banner above sprite overlays | v 3.76.0 | 73ac1b */
/* agent: gpt-6.1-sol | 2026-10-04 | Add generation draw close action | v 3.64.0 | 39b151 */
/* agent: gpt-6.1-sol | 2026-10-04 | Remove generated output arrow controls | v 3.37.0 | f5e94d */
/* agent: gpt-6.1-sol | 2026-10-04 | Open images from viewer footer | v 3.35.0 | 63bf6c */
/* agent: gpt-6.1-sol | 2026-10-04 | Add reset transform image button | v 3.30.0 | 5bd415 */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Move image details into frame footer | v 3.20.0 | 8869d6 */
/* agent: gpt-6.1-sol | 2026-10-04 | Share the reference and output viewer | v 3.13.0 | e45697 */

/* agent: gpt-6.1-sol | 2026-10-04 | Group stage 38 and reverse visual numbering | v 3.48.0 | 230919 */
/* agent: gpt-6.1-sol | 2026-10-04 | Run drawing saves asynchronously | v 3.55.0 | 3fbe9e */
typedef struct {
    AppArtifact *artifact;
    Rectangle hit, image;
    int kind, index;
} ArtifactDot;

/* agent: gpt-6.1-sol | 2026-10-05 | Group execution-ordered processor stages | v 3.72.0 | 9c9d64 */
/* Return the horizontal pipeline phase for int stage_number; return int. */
static int artifact_stage_group(int stage_number) {
    if (stage_number == 100 || stage_number == 97 || stage_number == 94 || stage_number == 91) return 0;
    if (stage_number == 80 || stage_number == 77 || stage_number == 74 || stage_number == 71 || stage_number == 60 || stage_number == 57) return 1;
    if (stage_number == 50 || stage_number == 47 || stage_number == 44 || stage_number == 35) return 2;
    if (stage_number == 32 || stage_number == 29 || stage_number == 26 || stage_number == 23) return 3;
    if (stage_number == 10 || stage_number == 7 || stage_number == 4) return 4;
    return -1;
}
/* agent: gpt-6.1-sol | 2026-10-05 | Group execution-ordered processor stages | v 3.72.0 | 9c9d64 */

/* agent: gpt-6.1-sol | 2026-10-04 | Order draw copy row above outputs | v 3.55.0 | ef35c7 */
/* Build clickable source, stage, draw-copy, and final dots for AppRun* run; return int count. */
static int layout_artifact_dots(AppRun *run, Rectangle bounds, ArtifactDot *dots, int capacity) {
    if (bounds.width <= 0 || bounds.height <= 0) return 0;
    int outputs = 0, copies = 0, count = 0;
    bool present[5] = {false};
    for (int i = 0; i < run->stage_count; i++) {
        int group = artifact_stage_group(run->stages[i].number);
        if (group >= 0) present[group] = true;
    }
    int groups = 0;
    for (int i = 0; i < 5; i++) groups += present[i];
    outputs = run->output_count;
    copies = run->draw_copy_count;
    int rows = (outputs > 0) + (copies > 0);
    rows += groups;
    bool has_source = run->generated.path[0] != 0;
    rows += has_source;
    if (!rows) return 0;
    if (capacity < outputs + copies + run->stage_count + has_source) return 0;
    int columns = outputs > copies ? outputs : copies;
    if (columns < 6) columns = 6;
    float grid_width = fminf((float)columns * 32.0f, fmaxf(0.0f, bounds.width));
    if (grid_width <= 0.0f) return 0;
    float cell = grid_width / columns;
    float grid_left = bounds.x + bounds.width - grid_width;
    float slot = fminf(24.0f, bounds.height / rows);
    if (slot < 0.1f) return 0;
    float used_height = slot * rows, top = bounds.y + (bounds.height - used_height) * 0.5f;
    float base_size = fminf(16.0f, fminf(slot * 0.84f, cell));
    int row = 0;
    if (copies) {
        int first_column = columns - copies, visible = 0;
        for (int i = 0; i < run->draw_copy_count; i++) {
            AppArtifact *artifact = &run->draw_copies[i];
            float size = fminf(base_size, cell * 0.5f);
            float cx = grid_left + (first_column + visible + 0.5f) * cell;
            float cy = top + slot * 0.5f;
            ArtifactDot *dot = &dots[count++];
            dot->artifact = artifact; dot->kind = 3; dot->index = i;
            dot->image = (Rectangle){cx - size * 0.5f, cy - size * 0.5f, size, size};
            dot->hit = (Rectangle){grid_left + (first_column + visible) * cell, top, cell, slot};
            visible++;
        }
        row++;
    }
    if (outputs) {
        int first_column = columns - outputs;
        int visible = 0;
        for (int i = 0; i < run->output_count; i++) {
            AppArtifact *artifact = &run->outputs[i];
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
    for (int group = 4; group >= 0; group--) {
        if (!present[group]) continue;
        int group_count = 0;
        for (int i = 0; i < run->stage_count; i++) if (artifact_stage_group(run->stages[i].number) == group) group_count++;
        int first_column = columns - group_count;
        float t = rows > 1 ? (float)row / (float)(rows - 1) : 0.5f;
        float size = base_size * (0.58f + 0.42f * fabsf(2.0f * t - 1.0f));
        float cy = top + (row + 0.5f) * slot;
        int within_group = 0;
        for (int i = run->stage_count - 1; i >= 0; i--) {
            AppArtifact *artifact = &run->stages[i];
            if (artifact_stage_group(artifact->number) != group || count >= capacity) continue;
            float cx = grid_left + (first_column + within_group + 0.5f) * cell;
            ArtifactDot *dot = &dots[count++];
            dot->artifact = artifact; dot->kind = 2; dot->index = i;
            dot->image = (Rectangle){cx - size * 0.5f, cy - size * 0.5f, size, size};
            float hit_height = fminf(slot, fmaxf(10.0f, size + 4.0f));
            dot->hit = (Rectangle){grid_left + (first_column + within_group) * cell, cy - hit_height * 0.5f, cell, hit_height};
            within_group++;
        }
        row++;
    }
    if (has_source && count < capacity) {
        float t = rows > 1 ? (float)row / (float)(rows - 1) : 1.0f;
        float size = base_size * (0.58f + 0.42f * fabsf(2.0f * t - 1.0f));
        float cx = grid_left + (columns - 0.5f) * cell, cy = top + (row + 0.5f) * slot;
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
/* agent: gpt-6.1-sol | 2026-10-04 | Reload artifact thumbnails on demand | v 3.61.0 | 58d3ca */
/* Draw/select artifact dots for App* app and AppRun* run, Rectangle bounds, ArtifactDot* dots, int capacity, int* hover_index and bool draw/allow_select; queue missing thumbnails; return int. */
static int draw_artifact_waterfall(App *app, AppRun *run, Rectangle bounds, ArtifactDot *dots, int capacity, int *hover_index, bool draw, bool allow_select) {
    int count = layout_artifact_dots(run, bounds, dots, capacity);
    Vector2 mouse = GetMousePosition();
    *hover_index = -1;
    for (int i = 0; i < count; i++) {
        AppArtifact *artifact = dots[i].artifact;
        if (!artifact->texture.id && !artifact->thumb.id && !artifact->loading && queue_image(app, artifact->path, 240, false, false)) artifact->loading = true;
    }
    for (int i = 0; i < count; i++) if (CheckCollisionPointRec(mouse, dots[i].hit)) {
        *hover_index = i;
        if (!draw && allow_select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ArtifactDot *dot = &dots[i];
            if (dot->kind == 0) { run->selected_output = -1; run->stage_output_selected = false; run->draw_copy_selected = false; }
            else if (dot->kind == 1) { run->selected_output = dot->index; run->stage_output_selected = false; run->draw_copy_selected = false; }
            else if (dot->kind == 2) { run->selected_stage = dot->index; run->stage_output_selected = true; run->draw_copy_selected = false; }
            else { run->selected_draw_copy = dot->index; run->draw_copy_selected = true; run->stage_output_selected = false; }
        }
        break;
    }
    if (draw) {
        for (int i = 0; i < count; i++) {
            AppArtifact *artifact = dots[i].artifact;
            if (artifact->thumb.id) artifact->thumb_used_at = GetTime();
            else if (artifact->texture.id) artifact->full_used_at = GetTime();
            bool selected = dots[i].kind == 0 ? (!run->stage_output_selected && !run->draw_copy_selected && run->selected_output < 0) :
                dots[i].kind == 1 ? (!run->stage_output_selected && !run->draw_copy_selected && run->selected_output == dots[i].index) :
                dots[i].kind == 2 ? (run->stage_output_selected && run->selected_stage == dots[i].index) :
                (run->draw_copy_selected && run->selected_draw_copy == dots[i].index);
            draw_artifact_dot(&dots[i], selected, i == *hover_index);
        }
        if (*hover_index >= 0) {
            AppArtifact *artifact = dots[*hover_index].artifact;
            if (artifact->thumb.id) artifact->thumb_used_at = GetTime();
            else if (artifact->texture.id) artifact->full_used_at = GetTime();
            int outputs = 0;
            for (int i = 0; i < run->output_count; i++) if (run->outputs[i].thumb.id || run->outputs[i].texture.id) outputs++;
            for (int i = 0; i < run->draw_copy_count; i++) if (run->draw_copies[i].thumb.id || run->draw_copies[i].texture.id) outputs++;
            int columns = outputs > 6 ? outputs : 6;
            float grid_width = fminf((float)columns * 32.0f, fmaxf(0.0f, bounds.width));
            float grid_left = bounds.x + bounds.width - grid_width;
            draw_artifact_dot_popup(&dots[*hover_index], bounds, grid_left);
        }
    }
    return count;
}
/* agent: gpt-6.1-sol | 2026-10-04 | Reload artifact thumbnails on demand | v 3.61.0 | 58d3ca */

/* Draw App* app's run artifacts at int x/y/width/height; hide global waterfall during sprite inspection; return void. */
/* agent: gpt-6 | 2026-10-05 | Hide global waterfall in sprite inspector | v 3.93.0 | 782a38 */
static void draw_output_page(App *app, int x, int y, int width, int height) {
    AppRun *run = get_run(app);
    if (!run) { ui_draw_text("No generation selected.", x, y, 18, UI_MUTED); return; }
    ArtifactDot dots[APP_MAX_STAGES + APP_MAX_OUTPUTS * 2 + 1];
    float rail_w = fminf(300.0f, fmaxf(110.0f, width * 0.34f));
    Rectangle bounds = {(float)(x + width - rail_w), (float)(y + 4), rail_w, (float)(height - 58)};
    int hover_index = -1, count = 0;
    if (!app->sprite_inspect_enabled)
        count = draw_artifact_waterfall(app, run, bounds, dots, APP_MAX_STAGES + APP_MAX_OUTPUTS * 2 + 1, &hover_index, false, !app->draw_state);
    if (run->stage_output_selected && (run->selected_stage < 0 || run->selected_stage >= run->stage_count)) run->stage_output_selected = false;
    AppArtifact *selected;
    if (run->draw_copy_selected && run->selected_draw_copy >= 0 && run->selected_draw_copy < run->draw_copy_count) selected = &run->draw_copies[run->selected_draw_copy];
    else if (run->stage_output_selected) selected = &run->stages[run->selected_stage];
    else {
        int choice = run->selected_output == -2 ? run_default_image_index(run) : run->selected_output;
        if (choice >= run->output_count || (choice >= 0 && !run->outputs[choice].texture.id && !run->outputs[choice].thumb.id)) choice = run_default_image_index(run);
        run->selected_output = choice;
        selected = choice < 0 ? &run->generated : &run->outputs[choice];
    }
    AppArtifact *image = !app->draw_state && hover_index >= 0 ? dots[hover_index].artifact : selected;
    draw_shared_image_view(app, image, NULL, run, x, y, width, height);
    if (count > 0 && !app->draw_state && !app->sprite_inspect_enabled) draw_artifact_waterfall(app, run, bounds, dots, APP_MAX_STAGES + APP_MAX_OUTPUTS * 2 + 1, &hover_index, true, false);
    draw_handle_close_dialog(app, NULL, run);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Order draw copy row above outputs | v 3.55.0 | ef35c7 */
/* agent: gpt-6.1-sol | 2026-10-04 | Group stage 38 and reverse visual numbering | v 3.48.0 | 230919 */
/* agent: gpt-6.1-sol | 2026-10-04 | Share the reference and output viewer | v 3.13.0 | e45697 */

/* agent: gpt-6.1-sol | 2026-10-04 | Show full run diagnostics on progress | v 3.11.0 | 8bbd56 */
/* agent: gpt-6.1-sol | 2026-10-04 | Use full width for run logs | v 3.36.0 | 2fb83a */
/* agent: gpt-6 | 2026-10-05 | Remove pixel controls panel | v 3.88.0 | 5a9c3f */
/* agent: gpt-6.1-sol | 2026-10-04 | Open full process and generation records | v 3.37.0 | d29792 */
/* agent: gpt-6 | 2026-10-05 | Remove pixel controls panel | v 3.88.0 | 5a9c3f */
/* Draw run status, stages, and log for App* app in int bounds; return void. */
/* agent: gpt-6.1-sol | 2026-10-05 | List selected-stage sprite evidence | v 3.72.0 | 34d14c */
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
        AppArtifact *debug_stage=NULL;for(int di=0;di<run->debug_stage_count;di++)if(run->debug_stages[di].number==stage->number){debug_stage=&run->debug_stages[di];break;}
        AppArtifact *shown_stage=run->selected_stage_debug&&debug_stage?debug_stage:stage;
        if (shown_stage->texture.id) shown_stage->full_used_at = GetTime();
        else if (shown_stage->thumb.id) shown_stage->thumb_used_at = GetTime();
        load_sprite_evidence(app, run, stage);
        ui_draw_text(TextFormat("%02d | %s",stage->number,stage->name),x+124,stage_y+42,16,UI_TEXT);
        /* agent: gpt-6.1-sol | 2026-10-04 | Load full resolution stage previews | v 3.61.0 | 0e49cd */
        if (!shown_stage->texture.id && !shown_stage->loading && queue_image(app, shown_stage->path, 240, false, true)) shown_stage->loading = true;
        /* agent: gpt-6.1-sol | 2026-10-04 | Load full resolution stage previews | v 3.61.0 | 0e49cd */
        /* agent: gpt-6.1-sol | 2026-10-04 | Show stage image loading state | v 3.57.0 | fb1f81 */
        if (shown_stage->texture.id) draw_image_view(app,shown_stage->texture,x,stage_y+82,stage_w,stage_h-132);
        else if (shown_stage->thumb.id) draw_image_view(app,shown_stage->thumb,x,stage_y+82,stage_w,stage_h-132);
        else if (shown_stage->loading) draw_loading_marker(x+stage_w/2,stage_y+stage_h/2-20,UI_ACCENT);
        else ui_draw_text("Stage image unavailable",x+8,stage_y+100,14,UI_MUTED);
        /* agent: gpt-6.1-sol | 2026-10-04 | Show stage image loading state | v 3.57.0 | fb1f81 */
        const char *description=stage_description(stage->number);
        if(*description)draw_wrapped_text(description,(Rectangle){(float)x,(float)(stage_y+stage_h-42),(float)stage_w,32},14,UI_MUTED);
        if(button((Rectangle){x,stage_y+stage_h,150,36},"Open stage",false))open_path(stage->path);
        if(debug_stage&&button((Rectangle){x+164,stage_y+stage_h,148,36},run->selected_stage_debug?"Show stage":"Show overlay",false))run->selected_stage_debug=!run->selected_stage_debug;
    }
    int log_x=has_stages?x+stage_w+44:x,log_w=has_stages?width-stage_w-44:width,log_y=stage_y,log_h=stage_h+36;
    /* agent: gpt-6.1-sol | 2026-10-04 | Add generation run controls | v 3.53.0 | 522f7a */
    ui_draw_text("Controls",log_x,log_y,18,UI_TEXT);
    /* agent: gpt-6 | 2026-10-04 | Add processor metadata controls tab | v 3.67.0 | 0ee559 */
    float diag_button_w = fminf(130.0f, fmaxf(1.0f, (log_w - 15) / 4.0f));
    Rectangle logs_tab={(float)log_x,(float)(log_y+30),diag_button_w,32}, record_tab={(float)(log_x+diag_button_w+5),(float)(log_y+30),diag_button_w,32}, resources_tab={(float)(log_x+(diag_button_w+5)*2),(float)(log_y+30),diag_button_w,32}, metadata_tab={(float)(log_x+(diag_button_w+5)*3),(float)(log_y+30),diag_button_w,32};
    if(button(logs_tab,"Process log",app->run_panel==0))app->run_panel=0;
    if(button(record_tab,"Gen record",app->run_panel==1))app->run_panel=1;
    if(button(resources_tab,"Resources",app->run_panel==2))app->run_panel=2;
    if(button(metadata_tab,"Metadata",app->run_panel==3))app->run_panel=3;
    if(app->preview_ready){
        ui_draw_text(app->rerun_pixel_only?"Pixel rerun ready":"Full generation rerun ready",log_x+log_w-385,log_y-1,13,UI_WARN);
        if(app->rerun_pixel_only){
            /* agent: gpt-6.1-sol | 2026-10-05 | Offer pixel rerun location choices | v 3.90.0 | 186703 */
            if(button((Rectangle){(float)(log_x+log_w-370),(float)(log_y-6),112,32},"New run",false))AppDispatch(app,"{\"action\":\"generation.confirm\"}",s_action_response,sizeof(s_action_response));
            if(button((Rectangle){(float)(log_x+log_w-250),(float)(log_y-6),112,32},"Rerun here",false))AppDispatch(app,"{\"action\":\"run.rerun.inplace.confirm\"}",s_action_response,sizeof(s_action_response));
        }else if(button((Rectangle){(float)(log_x+log_w-250),(float)(log_y-6),112,32},"Confirm rerun",false))AppDispatch(app,"{\"action\":\"generation.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){(float)(log_x+log_w-130),(float)(log_y-6),92,32},"Cancel",false))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }else{
        if(run->state!=RUN_RUNNING&&button((Rectangle){(float)(log_x+log_w-518),(float)(log_y-6),124,32},"Clean stages",false))AppDispatch(app,"{\"action\":\"run.stages.clear\"}",s_action_response,sizeof(s_action_response));
        if(run->state!=RUN_RUNNING&&button((Rectangle){(float)(log_x+log_w-388),(float)(log_y-6),124,32},"Rerun pixels",false))AppDispatch(app,"{\"action\":\"run.rerun\",\"value\":\"pixel\"}",s_action_response,sizeof(s_action_response));
        if(run->state!=RUN_RUNNING&&button((Rectangle){(float)(log_x+log_w-128),(float)(log_y-6),128,32},"Rerun generation",false))AppDispatch(app,"{\"action\":\"run.rerun\",\"value\":\"full\"}",s_action_response,sizeof(s_action_response));
    }
    Rectangle area={(float)log_x,(float)(log_y+68),(float)log_w,(float)(log_h-68)};
    DrawRectangleRec(area,UI_PANEL);BeginScissorMode((int)area.x,(int)area.y,(int)area.width,(int)area.height);
    if(app->run_panel==0){
        if(CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel>0)run->log_scroll+=3;else if(wheel<0&&run->log_scroll>0)run->log_scroll-=3;if(run->log_scroll<0)run->log_scroll=0;}
        int line_h=20,max_lines=(int)(area.height/line_h),start=run->log_count-max_lines-run->log_scroll;if(start<0)start=0;
        for(int i=start,row=0;i<run->log_count&&row<max_lines;i++,row++){AppLogLine *line=&run->logs[i];ui_draw_text(TextFormat("%s [%s] %s",line->time,line->process,line->text),(int)area.x+10,(int)area.y+6+row*line_h,14,UI_TEXT);}
    } else if(app->run_panel==1){
        char record_path[APP_PATH_CAP],record[APP_TEXT_CAP];path_join(record_path,sizeof(record_path),run->dir,"generation.record.txt");
        if(!read_text(record_path,record,sizeof(record)))copy_text(record,sizeof(record),"Generation record is not available yet.");
        int row=0;for(char *line=strtok(record,"\n");line&&row<(int)(area.height/21);line=strtok(NULL,"\n"),row++)ui_draw_text(line,(int)area.x+10,(int)area.y+6+row*21,14,UI_TEXT);
    } else if (app->run_panel == 2) {
        ui_draw_text("Resources captured for this run",(int)area.x+10,(int)area.y+8,15,UI_MUTED);
        if(CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel>0)run->log_scroll+=2;else if(wheel<0&&run->log_scroll>0)run->log_scroll-=2;if(run->log_scroll<0)run->log_scroll=0;}
        int visible_resources=(int)((area.height-40)/36);if(visible_resources<1)visible_resources=1;
        int first_resource=run->used_resource_count-visible_resources-run->log_scroll;if(first_resource<0)first_resource=0;
        for(int i=first_resource,row=0;i<run->used_resource_count&&i<first_resource+visible_resources;i++,row++){
            const char *path=run->used_resources[i],*base=strrchr(path,'/');base=base?base+1:path;
            ui_draw_text(base,(int)area.x+12,(int)area.y+34+row*34,14,UI_TEXT);
            bool previewed=false;
            for(int j=0;j<app->ref_count;j++)if(!strcmp(app->refs[j].path,path)){if(!app->refs[j].is_text&&app->refs[j].thumb.id){Rectangle src={0,0,(float)app->refs[j].thumb.width,(float)app->refs[j].thumb.height},dst={(float)(area.x+area.width-46),(float)(area.y+29+row*34),28,28};DrawTexturePro(app->refs[j].thumb,src,dst,(Vector2){0,0},0,WHITE);previewed=true;}else if(app->refs[j].is_text){char text[APP_TEXT_CAP];if(read_text(path,text,sizeof(text))){text[strcspn(text,"\r\n")]=0;ui_draw_text(text,(int)area.x+12,(int)area.y+49+row*34,12,UI_MUTED);}previewed=true;}break;}
            if(!previewed&&is_image_path(base))ui_draw_text("image file",(int)area.x+area.width-88,(int)area.y+36+row*34,12,UI_MUTED);
        }
        if(!run->used_resource_count)ui_draw_text("No external resources recorded",(int)area.x+10,(int)area.y+36,14,UI_MUTED);
    } else if(app->run_panel==3) {
        char journal_path[APP_PATH_CAP], sprite_path[APP_PATH_CAP];
        path_join(journal_path, sizeof(journal_path), run->dir, "extra/processing.events.jsonl");
        path_join(sprite_path, sizeof(sprite_path), run->dir, "extra/processing.sprites.jsonl");
        if (run->processor_summary[0]) draw_wrapped_text(run->processor_summary, (Rectangle){area.x + 10, area.y + 4, area.width - 146, 42}, 11, UI_MUTED);
        else ui_draw_text(TextFormat("Processor evidence · %d recent events", run->metadata_event_count), (int)area.x + 10, (int)area.y + 6, 14, UI_MUTED);
        if (button((Rectangle){area.x + area.width - 126, area.y + 2, 116, 28}, "Open manifest", file_exists(run->processor_manifest_path))) open_path(run->processor_manifest_path);
        if (button((Rectangle){area.x + area.width - 126, area.y + 34, 116, 28}, "Open journal", file_exists(journal_path))) open_path(journal_path);
        if (button((Rectangle){area.x + area.width - 126, area.y + 66, 116, 28}, "Open sprite data", file_exists(sprite_path))) open_path(sprite_path);
        if(CheckCollisionPointRec(GetMousePosition(),area)){float wheel=GetMouseWheelMove();if(wheel>0)run->log_scroll+=2;else if(wheel<0&&run->log_scroll>0)run->log_scroll-=2;if(run->log_scroll<0)run->log_scroll=0;}
        int stage_filter=run->selected_stage>=0&&run->selected_stage<run->stage_count?run->stages[run->selected_stage].number:-1;
        ui_draw_text(stage_filter > 0 ? TextFormat("Stage %d · %d sprite records", stage_filter, app->sprite_evidence_count) : "Select a stage to inspect per-sprite evidence", (int)area.x+10, (int)area.y+48, 13, UI_MUTED);
        int max_records=(int)((area.height-140)/35), shown=0;
        int skip=run->log_scroll; if(skip<0)skip=0;
        for(int i=skip;i<app->sprite_evidence_count&&shown<max_records;i++){
            AppSpriteEvidence *item=&app->sprite_evidence[i];
            ui_draw_text(TextFormat("%s · [%d,%d %dx%d] · %s · %.110s",item->id,item->x,item->y,item->width,item->height,item->time,item->summary),(int)area.x+10,(int)area.y+104+shown*35,12,UI_TEXT);
            shown++;
        }
        if(stage_filter>0&&!app->sprite_evidence_count)ui_draw_text("No per-sprite evidence was recorded for this stage.",(int)area.x+10,(int)area.y+108,14,UI_MUTED);
        else if(stage_filter<0&&!run->metadata_event_count)ui_draw_text("Processor metadata is not available for this run.",(int)area.x+10,(int)area.y+108,14,UI_MUTED);

    }
    EndScissorMode();
    /* agent: gpt-6.1-sol | 2026-10-05 | List selected-stage sprite evidence | v 3.72.0 | 34d14c */
    /* agent: gpt-6 | 2026-10-04 | Add processor metadata controls tab | v 3.67.0 | 0ee559 */
    /* agent: gpt-6.1-sol | 2026-10-04 | Add generation run controls | v 3.53.0 | 522f7a */
}
/* agent: gpt-6.1-sol | 2026-10-04 | Open full process and generation records | v 3.37.0 | d29792 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show full run diagnostics on progress | v 3.11.0 | 8bbd56 */
/* agent: gpt-6.1-sol | 2026-10-04 | Use full width for run logs | v 3.36.0 | 2fb83a */

/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* Draw profile fields and processor settings for App* app; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
static void draw_profile_settings(App *app) {
    int w=GetScreenWidth(),h=GetScreenHeight(),content_h=1680;
    Rectangle viewport={24,106,(float)w-48,(float)h-144};
    if(CheckCollisionPointRec(GetMousePosition(),viewport)){float wheel=GetMouseWheelMove();if(wheel!=0)app->settings_scroll-=(int)(wheel*56);}
    int max_scroll=content_h-(int)viewport.height;if(max_scroll<0)max_scroll=0;if(app->settings_scroll<0)app->settings_scroll=0;if(app->settings_scroll>max_scroll)app->settings_scroll=max_scroll;
    BeginScissorMode((int)viewport.x,(int)viewport.y,(int)viewport.width,(int)viewport.height);
    int x=48,off=app->settings_scroll; float gap=24,cw=(w-96-gap*2)/3.0f; AppProfile *p=&app->profiles[app->active_profile];
    int y=120-off; ui_draw_text("Generation",x,y,20,UI_TEXT);
    ui_draw_text("Scroll for more settings",x+320,y+8,15,UI_MUTED);
    draw_profile_input(app,"model","Image model",p->model,(Rectangle){x,y+40,cw,62});
    draw_profile_input(app,"aspect","Aspect ratio",p->aspect,(Rectangle){x+cw+gap,y+40,cw,62});
    int row=y+40;
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
        copy_text(p->sets[p->set_count],sizeof(p->sets[0]),"");p->set_count++;app->profile_settings_dirty=true;
    }
    int pixel_y=set_y+100+set_rows*62; ui_draw_text("Pixel processing",x,pixel_y,20,UI_TEXT);
    int fields_y=pixel_y+38;
    AppPixelSettings *px=&p->pixel;
    draw_profile_input(app,"background_tolerance","Background tolerance",TextFormat("%d",px->background_tolerance),(Rectangle){x,fields_y,cw,62});
    draw_profile_input(app,"grid_cols","Grid columns (0 auto)",TextFormat("%d",px->grid_cols),(Rectangle){x+cw+gap,fields_y,cw,62});
    draw_profile_input(app,"grid_rows","Grid rows (0 auto)",TextFormat("%d",px->grid_rows),(Rectangle){x+2*(cw+gap),fields_y,cw,62});
    draw_profile_input(app,"extraction_padding","Sprite crop padding",TextFormat("%d",px->extraction_padding),(Rectangle){x,fields_y+72,cw,62});
    draw_profile_input(app,"pitch","Pitch (auto or fraction)",px->pitch,(Rectangle){x+cw+gap,fields_y+72,cw,62});
    draw_profile_input(app,"cleanup_mode","Magenta cleanup",px->cleanup_mode,(Rectangle){x+2*(cw+gap),fields_y+72,cw,62});
    draw_profile_input(app,"phase_x","Phase X (auto or source px)",px->phase_x,(Rectangle){x,fields_y+144,cw,62});
    draw_profile_input(app,"phase_y","Phase Y (auto or source px)",px->phase_y,(Rectangle){x+cw+gap,fields_y+144,cw,62});
    draw_profile_input(app,"pitch_precision","Pitch precision",px->pitch_precision,(Rectangle){x+2*(cw+gap),fields_y+144,cw,62});
    draw_profile_input(app,"pitch_min","Minimum pitch",TextFormat("%d",px->pitch_min),(Rectangle){x,fields_y+216,cw,62});
    draw_profile_input(app,"pitch_max","Maximum pitch",TextFormat("%d",px->pitch_max),(Rectangle){x+cw+gap,fields_y+216,cw,62});
    draw_profile_input(app,"foreground_coverage","Foreground coverage %",TextFormat("%d",px->foreground_coverage),(Rectangle){x+2*(cw+gap),fields_y+216,cw,62});
    draw_profile_input(app,"cleanup_tolerance","Cleanup tolerance",TextFormat("%d",px->cleanup_tolerance),(Rectangle){x,fields_y+288,cw,62});
    draw_profile_input(app,"sheet_anchor","Sheet anchor",px->sheet_anchor,(Rectangle){x+cw+gap,fields_y+288,cw,62});
    draw_profile_input(app,"sheet_padding","Sheet padding",TextFormat("%d",px->sheet_padding),(Rectangle){x+2*(cw+gap),fields_y+288,cw,62});
    ui_draw_text("Slim upload",x,fields_y+360,15,UI_MUTED);
    if(button((Rectangle){x,fields_y+382,cw,38},p->slim_upload?"On":"Off",p->slim_upload)){p->slim_upload=!p->slim_upload;app->profile_settings_dirty=true;}
    if(p->slim_upload!=app->profile_settings_saved.slim_upload)DrawRectangleLinesEx((Rectangle){x,fields_y+382,cw,38},2,(Color){255,220,82,255});
    EndScissorMode();
    int track_x=w-14,track_y=(int)viewport.y,track_h=(int)viewport.height;
    DrawRectangle(track_x,track_y,5,track_h,UI_PANEL);
    int thumb_h=(int)(track_h*((float)viewport.height/content_h));if(thumb_h<36)thumb_h=36;
    int thumb_y=track_y+(max_scroll?(track_h-thumb_h)*app->settings_scroll/max_scroll:0);
    DrawRectangle(track_x,thumb_y,5,thumb_h,UI_ACCENT);
    int right=GetScreenWidth();
    if(button((Rectangle){right-440,66,100,38},"Text",false)&&save_profile_settings(app))open_profile_text(app,app->active_profile);
    if(button((Rectangle){right-320,66,90,38},"Save",false))save_profile_settings(app);
    if(button((Rectangle){right-220,66,88,38},"Close",false)&&save_profile_settings(app)){app->profile_settings_view=false;app->profile_text_edit=false;app->show_full_image=false;app->edit_field[0]=0;app->field_scroll=0;}
}
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* Draw application settings for App* app; return void. */
static void draw_settings(App *app) {
    ui_draw_text("Application settings",48,76,17,UI_ACCENT);
    draw_font_settings(app,48,132);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */

/* agent: gpt-6.1-sol | 2026-10-04 | Use shared reference name editing | v 3.3.0 | 930e92 */
/* agent: gpt-6.1-sol | 2026-10-04 | Route viewer by named view | v 3.12.0 | aed5ea */
/* agent: gpt-6.1-sol | 2026-10-04 | Confirm unsaved prompt close choices | v 3.8.0 | 32e9a8 */
/* Draw the shared image viewer or prompt editor for App* app; return void. */
/* agent: gpt-6.1-sol | 2026-10-04 | Place section controls in header band | v 3.25.0 | 749a93 */
/* agent: gpt-6.1-sol | 2026-10-04 | Reduce gap below header | v 3.21.0 | 4ccf31 */
/* agent: gpt-6.1-sol | 2026-10-04 | Handle draw save and close flow | v 3.50.0 | 8c3bc7 */
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
static void draw_modal_view(App *app) {
    if (!app->show_full_image) return;
    if (app->view == VIEW_LIBRARY && app->selected_ref >= 0 && app->selected_ref < app->ref_count && (app->refs[app->selected_ref].is_text || app->refs[app->selected_ref].is_profile)) {
        AppReference *ref = &app->refs[app->selected_ref];
        bool pending_before=app->prompt_close_pending;
        bool profile=ref->is_profile;
        bool editing_name = !strcmp(app->edit_field, "reference.name");
        if(profile) {
            if(!pending_before)draw_reference_name_field(app,ref,(Rectangle){48,66,420,36});
            if(!strcmp(app->edit_field,"reference.name"))ui_draw_text("Enter: rename · Esc: cancel",480,77,14,UI_MUTED);
            if(!pending_before&&button((Rectangle){GetScreenWidth()-440,66,100,38},"Edit settings",false)) {
                if(!app->prompt_dirty||save_open_prompt(app)){app->profile_text_edit=false;app->show_full_image=false;app->settings_view=false;app->profile_settings_saved=app->profiles[app->active_profile];app->profile_settings_dirty=false;app->profile_settings_view=true;}
            }
        } else {
            if(!pending_before)draw_reference_name_field(app, ref, (Rectangle){48,66,420,36});
            if(!strcmp(app->edit_field,"reference.name"))ui_draw_text("Enter: rename · Esc: cancel",480,77,14,UI_MUTED);
        }
        draw_prompt_editor(app, (Rectangle){32, 176, GetScreenWidth()-64, GetScreenHeight()-226});
        int w=GetScreenWidth();
        bool save_requested=button((Rectangle){w-320,66,90,38},"Save",false);
        bool close_button=button((Rectangle){w-220,66,88,38},"Close",false);
        if(!app->prompt_close_pending && save_requested)save_open_prompt(app);
        if(!app->prompt_close_pending) {
            bool close_requested=close_button||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
            if(IsKeyPressed(KEY_ESCAPE)){if(editing_name)app->edit_field[0]=0;else close_requested=true;}
            if(close_requested){
                if(app->prompt_dirty)app->prompt_close_pending=true;
                else {app->show_full_image=false;app->edit_field[0]=0;app->profile_text_edit=false;}
            }
        }
        if(app->prompt_close_pending){
            int choice=draw_prompt_close_dialog();
            if(pending_before&&IsKeyPressed(KEY_ESCAPE))choice=PROMPT_CLOSE_CANCEL;
            if(choice==PROMPT_CLOSE_SAVE&&save_open_prompt(app)){app->prompt_close_pending=false;app->show_full_image=false;app->edit_field[0]=0;app->profile_text_edit=false;}
            else if(choice==PROMPT_CLOSE_DISCARD){discard_open_prompt(app);app->show_full_image=false;app->edit_field[0]=0;app->profile_text_edit=false;}
            else if(choice==PROMPT_CLOSE_CANCEL)app->prompt_close_pending=false;
        }
        return;
    }
    AppArtifact image = {0};
    AppReference *ref = NULL;
    AppRun *run = NULL;
    if (app->view == VIEW_LIBRARY && app->selected_ref >= 0 && app->selected_ref < app->ref_count) {
        ref = &app->refs[app->selected_ref];
        copy_text(image.name, sizeof(image.name), ref->name); copy_text(image.path, sizeof(image.path), ref->path);
        image.texture = ref->full; image.thumb = ref->thumb; image.width = ref->width; image.height = ref->height; image.loading = ref->loading;
    }
    else {
        run = get_run(app);
        if (run) {
            AppArtifact *selected = run_selected_artifact(run);
            if (selected) image = *selected;
            draw_shared_image_view(app, &image, NULL, run, 48, 120, GetScreenWidth()-96, GetScreenHeight()-164);
        }
    }
    if (ref && !ref->is_text) {
        draw_shared_image_view(app, &image, ref, NULL, 48, 120, GetScreenWidth()-96, GetScreenHeight()-164);
        if (!app->draw_state && IsKeyPressed(KEY_ESCAPE)) { app->show_full_image = false; app->edit_field[0] = 0; }
    }
    draw_handle_close_dialog(app, ref, run);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Edit profiles in settings or text | v 3.55.0 | 5f9d57 */
/* agent: gpt-6.1-sol | 2026-10-04 | Handle draw save and close flow | v 3.50.0 | 8c3bc7 */
/* agent: gpt-6.1-sol | 2026-10-04 | Align profile toolbar and support filename editing | v 3.60.0 | a06e32 */
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
    return run_selected_artifact(run);
}
/* agent: gpt-6.1-sol | 2026-10-04 | Show run header state and artifact | v 3.42.0 | d9ca89 */

/* agent: gpt-6.1-sol | 2026-10-04 | Handle draw save and close flow | v 3.50.0 | 8c3bc7 */
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
/* agent: gpt-6.1-sol | 2026-10-04 | Show animated asset loading state | v 3.55.0 | c74352 */
/* agent: gpt-6.1-sol | 2026-10-04 | Upload decoded textures on main thread | v 3.55.0 | c09b7f */
/* agent: gpt-6.1-sol | 2026-10-04 | Show loading state in image views | v 3.55.0 | 4420da */
void AppDraw(App *app) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    ClearBackground(UI_BG);
    bool dirty_prompt = app->show_full_image && app->view == VIEW_LIBRARY && app->selected_ref >= 0 && app->selected_ref < app->ref_count && app->refs[app->selected_ref].is_text && app->prompt_dirty;
    ui_draw_text("AIGEN", 40, 22, 26, UI_ACCENT);
    Rectangle view_button = {130,18,36,36};
    bool show_grid_icon = app->view != VIEW_LIBRARY;
    bool view_clicked = button(view_button, "", !app->settings_view) && !dirty_prompt && !app->draw_close_pending;
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
    bool settings_clicked=button(settings_button,"",app->settings_view) && !dirty_prompt && !app->draw_close_pending;
    for(int i=0;i<3;i++){
        int sy=(int)settings_button.y+10+i*8,left=(int)settings_button.x+8,right=(int)settings_button.x+28;
        DrawLine(left,sy,right,sy,app->settings_view?UI_ACCENT:UI_TEXT);
        int knob_x=left+(i==1?13:i==2?7:5);
        DrawCircle(knob_x,sy,3,UI_PANEL);DrawCircleLines(knob_x,sy,3,app->settings_view?UI_ACCENT:UI_TEXT);
    }
    if (view_clicked) {
        if (app->draw_state) {
            DrawState *state = (DrawState *)app->draw_state;
            if (draw_session_dirty(state)) { app->draw_close_pending = true; app->draw_close_action = 1; }
            else { draw_session_discard(app, false); app->show_full_image = false; app->edit_field[0] = 0; app->field_scroll = 0; }
        }
        else if (app->profile_settings_view) { if(save_profile_settings(app)){app->profile_settings_view = false; app->view = VIEW_LIBRARY; app->edit_field[0] = 0; app->field_scroll = 0;} }
        else if (app->settings_view) { app->settings_view = false; app->edit_field[0] = 0; app->field_scroll = 0; }
        else if (app->show_full_image) {
            app->show_full_image = false; app->edit_field[0] = 0; app->field_scroll = 0;
        }
        else {
            app->view = app->view == VIEW_HOME ? VIEW_LIBRARY : VIEW_HOME;
            app->show_add_menu = false;
        }
    }
    if(settings_clicked){
        if (app->draw_state) {
            if (draw_session_dirty((DrawState *)app->draw_state)) { app->draw_close_pending = true; app->draw_close_action = 2; }
            else { draw_session_discard(app, false); app->settings_view=!app->settings_view;app->show_add_menu=false;app->show_full_image=false; }
        } else if(!app->profile_settings_view||save_profile_settings(app)){app->profile_settings_view=false;app->settings_view=!app->settings_view;app->show_add_menu=false;app->show_full_image=false;if(!app->settings_view){app->edit_field[0]=0;app->field_scroll=0;} }
    }
    ui_draw_text(app->workspace, 222, 29, 15, UI_MUTED);
    if (!app->show_full_image) {
        if (app->profile_settings_view) ui_draw_text("Profile settings",40,76,17,UI_ACCENT);
        else if (app->settings_view) ui_draw_text("Settings", 40, 76, 17, UI_ACCENT);
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
                if (button(logs_button, app->show_run_logs ? "Show image" : "Controls", false)) app->show_run_logs = !app->show_run_logs;
            }
        }
    }
    AppRun *active = app->active_run >= 0 ? &app->runs[app->active_run] : NULL;
    ui_draw_text(active ? TextFormat("%s | %s", active->id, active->state == RUN_RUNNING ? "WORKING" : active->state == RUN_FAILED ? "FAILED" : active->state == RUN_SUCCEEDED ? "DONE" : "CANCELED") : "READY", w - 330, 28, 15, active && active->state == RUN_FAILED ? UI_BAD : UI_GOOD);
    if (image_loader_pending(app)) {
        draw_loading_marker(w - 54, 36, UI_ACCENT);
        ui_draw_text("Loading", w - 148, 29, 13, UI_MUTED);
    }
    DrawLine(28, 112, w - 28, 112, UI_PANEL_ALT);
    if (app->show_full_image) draw_modal_view(app);
    else if(app->profile_settings_view) draw_profile_settings(app);
    else if(app->settings_view) draw_settings(app);
    else if(app->view==VIEW_LIBRARY) draw_references(app);
    else if(app->view==VIEW_GENERATION) {
        AppRun *run = get_run(app);
        if (run && run_has_viewable_image(run) && !app->show_run_logs) draw_output_page(app,48,120,w-96,h-170);
        else draw_progress_page(app,48,120,w-96,h-170);
    } else draw_generation_grid(app);
    DrawRectangle(0, h - 28, w, 28, UI_PANEL);
    ui_draw_text(app->message,40,h-22,13,UI_MUTED);
    if (!app->show_full_image && (!strcmp(app->pending_action,"run.stages.clear") || !strcmp(app->pending_action,"run.pixel.inplace"))) {
        bool cleaning=!strcmp(app->pending_action,"run.stages.clear");
        Rectangle panel={(w-470)*0.5f,(h-190)*0.5f,470,190};
        DrawRectangleRec(panel,UI_PANEL);DrawRectangleLinesEx(panel,2,UI_WARN);
        ui_draw_text(cleaning?"Remove processor stage images?":"Rerun pixels here?",(int)panel.x+20,(int)panel.y+26,20,UI_TEXT);
        ui_draw_text(cleaning?"This removes stage previews from this run.":"Existing stage and output files may be replaced.",(int)panel.x+20,(int)panel.y+62,15,UI_MUTED);
        /* agent: gpt-6.1-sol | 2026-10-05 | Clarify in-place rerun label | v 3.89.0 | 663800 */
        if(button((Rectangle){panel.x+20,panel.y+118,150,40},cleaning?"Remove stages":"Rerun pixels here",false))AppDispatch(app,"{\"action\":\"app.confirm\"}",s_action_response,sizeof(s_action_response));
        if(button((Rectangle){panel.x+190,panel.y+118,120,40},"Cancel",false)||IsKeyPressed(KEY_ESCAPE))AppDispatch(app,"{\"action\":\"app.cancel\"}",s_action_response,sizeof(s_action_response));
    }
    if (!app->show_full_image && !strcmp(app->pending_action, "profile.delete")) {
        Rectangle panel = {(w - 440) * 0.5f, (h - 180) * 0.5f, 440, 180};
        DrawRectangleRec(panel, UI_PANEL); DrawRectangleLinesEx(panel, 2, UI_WARN);
        ui_draw_text("Move this profile resource to trash?", (int)panel.x + 20, (int)panel.y + 24, 21, UI_TEXT);
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
/* Save a queued full-window screenshot after AppDraw has rendered its annotations; return void. */
/* agent: gpt-6 | 2026-10-04 | Capture queued MCP screenshot frame | v 3.67.0 | c177d3 */
void AppAfterDraw(App *app) {
    if (!app || !app->screenshot_pending) return;
    Image capture = LoadImageFromScreen();
    bool saved = capture.data && ExportImage(capture, app->screenshot_path);
    if (capture.data) UnloadImage(capture);
    app->screenshot_pending = false;
    app->screenshot_expanded = app->screenshot_previous_expanded != 0;
    if (!saved) copy_text(app->screenshot_error, sizeof(app->screenshot_error), "Could not save screenshot PNG");
}
/* agent: gpt-6 | 2026-10-04 | Capture queued MCP screenshot frame | v 3.67.0 | c177d3 */
/* agent: gpt-6.1-sol | 2026-10-04 | Handle draw save and close flow | v 3.50.0 | 8c3bc7 */
/* agent: gpt-6.1-sol | 2026-10-04 | Stage profile settings edits until save | v 3.65.0 | 39ca33 */
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
/* agent: gpt-6.1-sol | 2026-10-04 | Expose resource actions and tools | v 3.55.0 | 5d23c1 */
/* Return the project tool list with the shared confirmation contract as const char*. */
/* agent: gpt-6 | 2026-10-04 | Expose processor screenshot MCP actions | v 3.67.0 | 9e2246 */
const char *AppMcpToolsJson(void) {
    return "{\"tools\":["
      "{\"name\":\"app_status\",\"description\":\"Read workspace, profile, active run, log tail, stages, and outputs\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"ui_screenshot\",\"description\":\"Capture the full rendered app window as a workspace PNG; detail may expand generation metadata overlays\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"detail\":{\"type\":\"string\",\"enum\":[\"current\",\"expanded\"]}},\"required\":[\"path\"]}},"
      "{\"name\":\"ui_screenshot_status\",\"description\":\"Read completion status for the last queued screenshot\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"ui_view\",\"description\":\"Show home, library, or generation view; generation accepts an optional run index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"},\"run\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"prompt_get\",\"description\":\"Read the currently open prompt file, including unsaved UI edits\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"prompt_open\",\"description\":\"Open a text resource for editing inside the workspace\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"prompt_save\",\"description\":\"Save prompt text to the currently open workspace file\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}},"
      "{\"name\":\"profiles_list\",\"description\":\"List workspace generation profiles\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"resources_list\",\"description\":\"List image, text, and profile resources in the workspace\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_add\",\"description\":\"Add a copy of the active profile\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_duplicate\",\"description\":\"Duplicate the active profile\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_prompt_add\",\"description\":\"Add an ordered profile prompt fragment\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_delete\",\"description\":\"Request deletion of the active profile; confirm separately\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"profile_select\",\"description\":\"Select a profile by list index\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"profile_set\",\"description\":\"Edit a supported profile field\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"field\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}},\"required\":[\"field\",\"value\"]}},"
      "{\"name\":\"references_list\",\"description\":\"Legacy alias for listing resource items\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"resource_import\",\"description\":\"Copy an image or text resource into the workspace resources folder\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"resource_manage\",\"description\":\"Exclude a resource or request moving it to workspace/trash\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"exclude\",\"trash\"]},\"path\":{\"type\":\"string\"}},\"required\":[\"action\",\"path\"]}},"
      "{\"name\":\"resource_select\",\"description\":\"Enable a profile or toggle resource inclusion\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"resource_view\",\"description\":\"Open a text or profile resource for editing, or view an image\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"resources_size\",\"description\":\"Set resource thumbnail size from 56 through 216\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"reference_import\",\"description\":\"Legacy alias for resource_import\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"reference_manage\",\"description\":\"Legacy alias for managing resources\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"exclude\",\"trash\"]},\"path\":{\"type\":\"string\"}},\"required\":[\"action\",\"path\"]}},"
      "{\"name\":\"reference_select\",\"description\":\"Legacy alias for resource selection\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"reference_view\",\"description\":\"Legacy alias for opening a resource\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"references_size\",\"description\":\"Legacy alias for resources_size\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"generation_preview\",\"description\":\"Prepare immutable prompt, resources, model, options, output path, and all-stage processing; does not start generation\",\"inputSchema\":{\"type\":\"object\"}},"
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
      "{\"name\":\"pixel_convert\",\"description\":\"Convert a selected workspace image into a pixelprocess run\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
      "{\"name\":\"processing_setting_set\",\"description\":\"Edit an effective pixel setting on the selected run\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"field\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}},\"required\":[\"field\",\"value\"]}},"
      "{\"name\":\"processing_scope\",\"description\":\"Set processing scope to whole image or selected sprite\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\",\"enum\":[\"whole\",\"sprite\"]}},\"required\":[\"value\"]}},"
      "{\"name\":\"processing_apply\",\"description\":\"Prepare an effective-settings rerun for confirmation\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"stage_debug\",\"description\":\"Show the selected stage or its debug overlay\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\",\"enum\":[\"0\",\"1\"]}},\"required\":[\"value\"]}},"
      "{\"name\":\"processing_owner_set\",\"description\":\"Assign a detected blob to a sprite cell before rerunning\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"component\":{\"type\":\"string\"},\"sprite_id\":{\"type\":\"string\"}},\"required\":[\"component\",\"sprite_id\"]}},"
      "{\"name\":\"output_export\",\"description\":\"Export a selected run artifact to a destination PNG\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"destination\":{\"type\":\"string\"}},\"required\":[\"path\",\"destination\"]}},"
      "{\"name\":\"run_rerun\",\"description\":\"Prepare a new-run pixel rerun or full rerun; pixel-in-place requests a confirmed rerun in the selected run\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\",\"enum\":[\"pixel\",\"pixel-in-place\",\"full\"]}}}},"
      "{\"name\":\"run_controls\",\"description\":\"Show the selected run's processing controls\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_panel_select\",\"description\":\"Select one Controls tab by index: process, record, resources, metadata\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}},"
      "{\"name\":\"run_stages_clear\",\"description\":\"Request removal of stage images for the selected completed run; confirm separately\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_logs_clear\",\"description\":\"Clear the visible log view and retain process.log\",\"inputSchema\":{\"type\":\"object\"}},"
      "{\"name\":\"run_note\",\"description\":\"Save a note beside the selected run\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}}]}";
}
/* agent: gpt-6 | 2026-10-04 | Expose processor screenshot MCP actions | v 3.67.0 | 9e2246 */
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-04 | Expose trash instead of delete | v 3.3.0 | 84c62f */

/* Handle MCP requests for const char* workspace and int port; return int status. */
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
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
            char name[128] = "", action[64] = "", value[APP_TEXT_CAP] = "", path[APP_PATH_CAP] = "", field[64] = "", text[APP_TEXT_CAP] = "", run_value[32] = "", detail[32] = "current", destination[APP_PATH_CAP] = "", component[32] = "", sprite_id[32] = "";
            json_string(line, "name", name, sizeof(name));
            if (!strcmp(name,"app_status")) copy_text(action,sizeof(action),"state");
            else if (!strcmp(name,"ui_screenshot")) { copy_text(action,sizeof(action),"ui.screenshot"); json_string(line,"path",path,sizeof(path)); json_string(line,"detail",detail,sizeof(detail)); }
            else if (!strcmp(name,"ui_screenshot_status")) copy_text(action,sizeof(action),"ui.screenshot.status");
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
            else if (!strcmp(name,"resources_list")) copy_text(action,sizeof(action),"resources.list");
            else if (!strcmp(name,"references_list")) copy_text(action,sizeof(action),"references.list");
            else if (!strcmp(name,"resource_import")) { copy_text(action,sizeof(action),"reference.import"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"resource_select")) { copy_text(action,sizeof(action),"resource.select"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"resource_view")) { copy_text(action,sizeof(action),"resource.view"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"resources_size")) { copy_text(action,sizeof(action),"references.size"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"resource_manage")) { json_string(line,"action",action,sizeof(action)); if (!strncmp(action,"resource.",9)) {} else { char kind[32]; copy_text(kind,sizeof(kind),action); snprintf(action,sizeof(action),"resource.%s",kind); } json_string(line,"path",path,sizeof(path)); }
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
            else if (!strcmp(name,"run_rerun")) { copy_text(action,sizeof(action),"run.rerun"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"run_controls")) copy_text(action,sizeof(action),"run.controls");
            else if (!strcmp(name,"run_panel_select")) {copy_text(action,sizeof(action),"run.panel");json_string(line,"value",value,sizeof(value));}
            else if (!strcmp(name,"run_stages_clear")) copy_text(action,sizeof(action),"run.stages.clear");
            else if (!strcmp(name,"run_logs_clear")) copy_text(action,sizeof(action),"run.logs.clear");
            else if (!strcmp(name,"run_select")) { copy_text(action,sizeof(action),"run.select"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"output_select")) { copy_text(action,sizeof(action),"output.select"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"stage_select")) { copy_text(action,sizeof(action),"stage.select"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"stage_compare")) { copy_text(action,sizeof(action),"stage.compare"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"viewer_zoom")) { copy_text(action,sizeof(action),"viewer.zoom"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"run_output_open")) { copy_text(action,sizeof(action),"run.output.open"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"pixel_convert")) { copy_text(action,sizeof(action),"pixel.convert"); json_string(line,"path",path,sizeof(path)); }
            else if (!strcmp(name,"processing_setting_set")) { copy_text(action,sizeof(action),"processing.setting.set"); json_string(line,"field",field,sizeof(field)); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"processing_scope")) { copy_text(action,sizeof(action),"processing.scope"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"processing_apply")) copy_text(action,sizeof(action),"processing.apply");
            else if (!strcmp(name,"stage_debug")) { copy_text(action,sizeof(action),"stage.debug"); json_string(line,"value",value,sizeof(value)); }
            else if (!strcmp(name,"processing_owner_set")) { copy_text(action,sizeof(action),"processing.owner.set"); json_string(line,"component",component,sizeof(component)); json_string(line,"sprite_id",sprite_id,sizeof(sprite_id)); }
            else if (!strcmp(name,"output_export")) { copy_text(action,sizeof(action),"output.export"); json_string(line,"path",path,sizeof(path)); json_string(line,"destination",destination,sizeof(destination)); }
            else if (!strcmp(name,"run_note")) { copy_text(action,sizeof(action),"run.note"); json_string(line,"text",text,sizeof(text)); }
            char request[MCP_FRAME_CAP], esc[APP_TEXT_CAP * 2], escpath[APP_PATH_CAP * 2], escfield[128], escvalue[APP_TEXT_CAP * 2], escaction[128], escdetail[64], escdest[APP_PATH_CAP*2], esccomponent[96], escsprite[96];
            json_escape(text,esc,sizeof(esc)); json_escape(path,escpath,sizeof(escpath)); json_escape(field,escfield,sizeof(escfield));
            json_escape(value,escvalue,sizeof(escvalue)); json_escape(action,escaction,sizeof(escaction));
            json_escape(detail, escdetail, sizeof(escdetail));
            json_escape(destination,escdest,sizeof(escdest));json_escape(component,esccomponent,sizeof(esccomponent));json_escape(sprite_id,escsprite,sizeof(escsprite));
            char escrun[64]; json_escape(run_value, escrun, sizeof(escrun));
            if (!strcmp(action, "ui.screenshot")) snprintf(request, sizeof(request), "{\"action\":\"ui.screenshot\",\"path\":\"%s\",\"detail\":\"%s\"}", escpath, escdetail);
            else if(!strcmp(action,"processing.owner.set"))snprintf(request,sizeof(request),"{\"action\":\"processing.owner.set\",\"component\":\"%s\",\"sprite_id\":\"%s\"}",esccomponent,escsprite);
            else if(!strcmp(action,"output.export"))snprintf(request,sizeof(request),"{\"action\":\"output.export\",\"path\":\"%s\",\"destination\":\"%s\"}",escpath,escdest);
            else if (!strcmp(action, "view")) snprintf(request, sizeof(request), "{\"action\":\"view\",\"value\":\"%s\",\"run\":\"%s\"}", escvalue, escrun);
            else snprintf(request,sizeof(request),"{\"action\":\"%.*s\",\"value\":\"%.*s\",\"path\":\"%.*s\",\"field\":\"%.*s\",\"text\":\"%.*s\"}",(int)sizeof(escaction)-1,escaction,4096,escvalue,2048,escpath,(int)sizeof(escfield)-1,escfield,12000,esc);
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
/* agent: gpt-6.1-sol | 2026-10-05 | Decouple shell from pixel processor | v 3.80.0 | 00df75 */
/* agent: gpt-6.1-sol | 2026-10-04 | Expose resource actions and tools | v 3.55.0 | 5d23c1 */
/* agent: gpt-6.1-sol | 2026-10-04 | Always emit complete pixel artifacts | v 3.41.0 | d1e8c8 */

/* 2026-10-03: core functions are documented above; APP-01 defines the shared run flow. */
/* agent: gpt-6.1-sol | 2026-10-04 | Support image and text renaming | v 3.3.0 | 204819 */
/* agent: gpt-6.1-sol | 2026-10-04 | Queue image decode and upload work | v 3.55.0 | eb40fa */
/* agent: gpt-6.1-sol | 2026-10-04 | Queue workspace reference images | v 3.55.0 | b3fba2 */
/* agent: gpt-6.1-sol | 2026-10-04 | Queue run artifact image loads | v 3.55.0 | 2fdcae */
/* agent: gpt-6.1-sol | 2026-10-04 | Wire queue through app lifecycle | v 3.55.0 | d0a0dc */
/* agent: gpt-6.1-sol | 2026-10-04 | Show animated asset loading state | v 3.55.0 | c74352 */
/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
/* agent: gpt-6.1-sol | 2026-10-04 | Defer historical process logs | v 3.55.0 | af851b */
/* agent: gpt-6.1-sol | 2026-10-04 | Upload decoded textures on main thread | v 3.55.0 | c09b7f */
/* agent: gpt-6.1-sol | 2026-10-04 | Show loading state in image views | v 3.55.0 | 4420da */
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
/* agent: gpt-6.1-sol | 2026-10-05 | Remove compiler warnings | v 3.82.0 | df869b */
