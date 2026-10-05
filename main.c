/*
 * File version: 2.2.0
 * Last updated: 2026-10-04
 * Purpose: Launch the raylib workbench or its MCP stdio bridge.
 * Flow APP-01 is implemented by the shared dispatch and process loop in lib.c; META-01 calls AppAfterDraw after rendering queued screenshots.
 */

/* agent: gpt-6.1-sol | 2026-10-03 | Allocate app state on heap | v 2.1.0 | 5307e7 */
#include "lib.h"

#include <stdlib.h>
#include <string.h>

/* Read the loopback ENet port from the environment or keep the project default. */
static int get_port(void) {
    const char *value = getenv("AIGEN_ENET_PORT");
    int port = value ? atoi(value) : 27881;
    return port > 0 && port <= 65535 ? port : 27881;
}

/* Start MCP bridge mode for Codex, otherwise open the desktop UI. */
int main(int argc, char **argv) {
    bool mcp_mode = argc > 1 && !strcmp(argv[1], "--mcp");
    const char *workspace = mcp_mode ? (argc > 2 ? argv[2] : "assets") : (argc > 1 ? argv[1] : "assets");
    int port = get_port();
    if (mcp_mode) return AppMcpClient(workspace, port);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Aigen Workbench");
    SetTargetFPS(60);
    App *app = calloc(1, sizeof(*app));
    if (!app) return 1;
    if (!AppInit(app, workspace, false)) {
        TraceLog(LOG_ERROR, "%s", app->message);
        free(app);
        CloseWindow();
        return 1;
    }
    while (!WindowShouldClose()) {
        AppUpdate(app);
        BeginDrawing();
        AppDraw(app);
/* agent: gpt-6 | 2026-10-04 | Capture MCP screenshots after drawing | v 2.2.0 | 97237d */
        AppAfterDraw(app);
/* agent: gpt-6 | 2026-10-04 | Capture MCP screenshots after drawing | v 2.2.0 | 97237d */
        EndDrawing();
    }
    AppShutdown(app);
    free(app);
    CloseWindow();
    return 0;
}

/* agent: gpt-6.1-sol | 2026-10-03 | Allocate app state on heap | v 2.1.0 | 5307e7 */
