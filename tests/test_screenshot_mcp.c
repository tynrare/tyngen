/*
 * File version: 1.6.0
 * Last updated: 2026-10-05
 * Purpose: Exercise screenshot and pixelprocess action validation.
 */
/* agent: gpt-6 | 2026-10-05 | Test independent sprite lattice edits | v 1.6.0 | 8ddcdc */
/* agent: gpt-6 | 2026-10-04 | Test MCP screenshot queue and path checks | v 1.1.0 | 41a7f2 */
#include "../lib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Check screenshot queuing, pixel edits, and run-owned PNG export; return int. */
int main(void) {
    char workspace[] = "/tmp/aigen-ui-mcp-shot-XXXXXX";
    assert(mkdtemp(workspace));
    App *app = calloc(1, sizeof(*app));
    assert(app);
    snprintf(app->workspace, sizeof(app->workspace), "%s", workspace);
    app->run_count=1;app->selected_run=0;app->active_run=-1;app->runs[0].state=RUN_SUCCEEDED;
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
    AppDispatch(app,"{\"action\":\"processing.setting.set\",\"field\":\"background_tolerance\",\"value\":\"73\"}",response,sizeof(response));
    assert(strstr(response,"\"ok\":true")&&app->runs[0].pixel_settings.background_tolerance==73);
    AppDispatch(app,"{\"action\":\"processing.setting.set\",\"field\":\"background_tolerance\",\"value\":\"300\"}",response,sizeof(response));
    assert(strstr(response,"\"ok\":false"));
    app->runs[0].pixel_sprite_scope=true;
    snprintf(app->runs[0].selected_sprite_id,sizeof(app->runs[0].selected_sprite_id),"sprite-r000-c001");
    AppDispatch(app,"{\"action\":\"processing.setting.set\",\"field\":\"pitch\",\"value\":\"5.5\"}",response,sizeof(response));
    assert(strstr(response,"\"ok\":true")&&app->runs[0].sprite_override_count==1);
    snprintf(app->runs[0].selected_sprite_id,sizeof(app->runs[0].selected_sprite_id),"sprite-r000-c002");
    AppDispatch(app,"{\"action\":\"processing.setting.set\",\"field\":\"pitch\",\"value\":\"7\"}",response,sizeof(response));
    assert(strstr(response,"\"ok\":true")&&app->runs[0].sprite_override_count==2);
    assert(!strcmp(app->runs[0].sprite_overrides[0].settings.pitch,"5.5"));
    assert(!strcmp(app->runs[0].sprite_overrides[1].settings.pitch,"7"));
    AppDispatch(app,"{\"action\":\"run.panel\",\"value\":\"4\"}",response,sizeof(response));
    assert(strstr(response,"\"ok\":false"));
    char original[APP_PATH_CAP],exported[APP_PATH_CAP];snprintf(original,sizeof(original),"%s/source.png",workspace);snprintf(exported,sizeof(exported),"%s/export.png",workspace);
    FILE *image=fopen(original,"wb");assert(image);fwrite("png-data",1,8,image);fclose(image);snprintf(app->runs[0].generated.path,sizeof(app->runs[0].generated.path),"%s",original);
    setenv("AIGEN_DISABLE_PIXELPROCESS","1",1);
    char convert_request[APP_PATH_CAP*2];snprintf(convert_request,sizeof(convert_request),"{\"action\":\"pixel.convert\",\"path\":\"%s\"}",original);
    AppDispatch(app,convert_request,response,sizeof(response));assert(strstr(response,"pixel processor unavailable")&&app->run_count==1);
    unsetenv("AIGEN_DISABLE_PIXELPROCESS");
    /* agent: gpt-6.1-sol | 2026-10-05 | Test source named processor runner | v 1.5.0 | 398974 */
    char processor_dir[APP_PATH_CAP],fake_runner[APP_PATH_CAP];snprintf(processor_dir,sizeof(processor_dir),"%s/processor",workspace);assert(mkdir(processor_dir,0755)==0);snprintf(fake_runner,sizeof(fake_runner),"%s/runner",processor_dir);
    FILE *runner=fopen(fake_runner,"wb");assert(runner);fputs("#!/bin/sh\nexit 0\n",runner);fclose(runner);assert(chmod(fake_runner,0755)==0);setenv("AIGEN_PIXELPROCESS_DIR",processor_dir,1);
    /* agent: gpt-6.1-sol | 2026-10-05 | Test source named processor runner | v 1.5.0 | 398974 */
    AppDispatch(app,"{\"action\":\"processing.apply\"}",response,sizeof(response));assert(strstr(response,"\"confirmation_required\":true"));
    AppDispatch(app,"{\"action\":\"app.cancel\"}",response,sizeof(response));unsetenv("AIGEN_PIXELPROCESS_DIR");unlink(fake_runner);rmdir(processor_dir);
    char export_request[APP_PATH_CAP*3];snprintf(export_request,sizeof(export_request),"{\"action\":\"output.export\",\"path\":\"%s\",\"destination\":\"%s\"}",original,exported);
    AppDispatch(app,export_request,response,sizeof(response));assert(strstr(response,"\"ok\":true"));assert(access(exported,F_OK)==0);
    AppDispatch(app,"{\"action\":\"output.export\",\"path\":\"/etc/passwd\",\"destination\":\"/tmp/nope.png\"}",response,sizeof(response));assert(strstr(response,"\"ok\":false"));
    char directory[APP_PATH_CAP];
    snprintf(directory, sizeof(directory), "%s/screenshots", workspace);
    rmdir(directory);
    unlink(original);unlink(exported);rmdir(workspace);
    free(app);
    return 0;
}

/* agent: gpt-6 | 2026-10-05 | Test independent sprite lattice edits | v 1.6.0 | 8ddcdc */
/* agent: gpt-6.1-sol | 2026-10-05 | Test processor directory configuration | v 1.4.0 | 42a0fb */
