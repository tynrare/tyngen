/*
 * File version: 2.5.0
 * Last updated: 2026-10-09
 * Purpose: Launch Tyngen, hidden screenshot capture, or MCP bridge.
 * Flow APP-01 is implemented by the shared dispatch and process loop in lib.c; META-01 calls AppAfterDraw after rendering queued screenshots.
 * Flow MCP-HIDDEN-01: 1. Agent -> run --hidden workspace -> hidden UI loop binds loopback ENet. 2. Codex -> MCP --mcp -> connects to that loop. 3. Agent -> ui_screenshot -> next drawn frame is saved in workspace screenshots/.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Rename window and control setting | v 2.5.0 | 888593 */

/* agent: gpt-6.1-sol | 2026-10-03 | Allocate app state on heap | v 2.1.0 | 5307e7 */
#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read the loopback ENet port from the environment or keep the project default. */
static int get_port(void) {
    const char *value = getenv("TYNGEN_ENET_PORT");
    int port = value ? atoi(value) : 27881;
    return port > 0 && port <= 65535 ? port : 27881;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Launch persistent hidden agent controlled app | v 2.4.0 | 6427f2 */
/* Start MCP, hidden UI, hidden capture, or desktop UI; return process status. */
int main(int argc, char **argv) {
    bool mcp_mode = argc > 1 && !strcmp(argv[1], "--mcp");
    bool screenshot_mode = argc > 1 && !strcmp(argv[1], "--screenshot");
    bool hidden_mode = argc > 1 && !strcmp(argv[1], "--hidden");
    const char *screenshot_path = screenshot_mode && argc > 2 ? argv[2] : NULL;
    const char *workspace = mcp_mode ? (argc > 2 ? argv[2] : "assets") :
        screenshot_mode ? (argc > 3 ? argv[3] : "assets") :
        hidden_mode ? (argc > 2 ? argv[2] : "assets") : (argc > 1 ? argv[1] : "assets");
    int port = get_port();
    if (mcp_mode) return AppMcpClient(workspace, port);
    if (screenshot_mode && !screenshot_path) {
        fprintf(stderr, "usage: %s --screenshot <png-path> [workspace]\n", argv[0]);
        return 2;
    }

    SetConfigFlags((screenshot_mode || hidden_mode) ? FLAG_WINDOW_HIDDEN : FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Tyngen Workbench");
    if (!IsWindowReady()) {
        fprintf(stderr, "could not initialize raylib window/context\n");
        return 1;
    }
    SetTargetFPS(60);
    App *app = calloc(1, sizeof(*app));
    if (!app) return 1;
    if (!AppInit(app, workspace, false)) {
        TraceLog(LOG_ERROR, "%s", app->message);
        free(app);
        CloseWindow();
        return 1;
    }
    do {
        AppUpdate(app);
        BeginDrawing();
        AppDraw(app);
/* agent: gpt-6 | 2026-10-04 | Capture MCP screenshots after drawing | v 2.2.0 | 97237d */
        AppAfterDraw(app);
/* agent: gpt-6 | 2026-10-04 | Capture MCP screenshots after drawing | v 2.2.0 | 97237d */
        bool saved = true;
        if (screenshot_mode) {
            Image frame = LoadImageFromScreen();
            saved = frame.data && ExportImage(frame, screenshot_path);
            if (frame.data) UnloadImage(frame);
        }
        EndDrawing();
        if (!saved) TraceLog(LOG_ERROR, "Could not save screenshot: %s", screenshot_path);
        if (screenshot_mode) {
            AppShutdown(app);
            free(app);
            CloseWindow();
            return saved ? 0 : 1;
        }
    } while (!WindowShouldClose());
    AppShutdown(app);
    free(app);
    CloseWindow();
    return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Launch persistent hidden agent controlled app | v 2.4.0 | 6427f2 */

/* agent: gpt-6.1-sol | 2026-10-03 | Allocate app state on heap | v 2.1.0 | 5307e7 */
