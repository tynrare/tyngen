/*
 * File version: 1.1.0
 * Last updated: 2026-10-04
 * Purpose: Exercise MCP screenshot path validation and queued capture state.
 */
/* agent: gpt-6 | 2026-10-04 | Test MCP screenshot queue and path checks | v 1.1.0 | 41a7f2 */
#include "../lib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Check screenshot MCP queuing and reject paths outside workspace; return int. */
int main(void) {
    char workspace[] = "/tmp/aigen-ui-mcp-shot-XXXXXX";
    assert(mkdtemp(workspace));
    App *app = calloc(1, sizeof(*app));
    assert(app);
    snprintf(app->workspace, sizeof(app->workspace), "%s", workspace);
    char response[2048];
    AppDispatch(app, "{\"action\":\"ui.screenshot\",\"path\":\"screenshots/first.png\",\"detail\":\"current\"}", response, sizeof(response));
    assert(strstr(response, "\"pending\":true"));
    assert(app->screenshot_pending);
    assert(strstr(app->screenshot_path, "/screenshots/first.png"));
    AppDispatch(app, "{\"action\":\"ui.screenshot\",\"path\":\"screenshots/../outside.png\"}", response, sizeof(response));
    assert(strstr(response, "\"ok\":false"));
    AppDispatch(app, "{\"action\":\"ui.screenshot\",\"path\":\"screenshots/second.png\"}", response, sizeof(response));
    assert(strstr(response, "\"ok\":false"));
    AppDispatch(app, "{\"action\":\"ui.screenshot.status\"}", response, sizeof(response));
    assert(strstr(response, "\"pending\":true"));
    char directory[APP_PATH_CAP];
    snprintf(directory, sizeof(directory), "%s/screenshots", workspace);
    rmdir(directory);
    rmdir(workspace);
    free(app);
    return 0;
}

/* agent: gpt-6 | 2026-10-04 | Test MCP screenshot queue and path checks | v 1.1.0 | 41a7f2 */
