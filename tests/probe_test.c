/*
 * File version: 1.4.0
 * Last updated: 2026-10-10
 * Purpose: Validate node-scoped setup save ownership.
 */
/* agent: GPT-6 | 2026-10-10 | Verify node scoped setup ownership | v 1.4.0 | 09dae6 */

#include "../lib.c"
#include <assert.h>

/* Write const char* JSON to const char* path; return void. */
static void write_probe_settings(const char *path,const char *json){FILE *f=fopen(path,"w");assert(f);assert(fputs(json,f)>=0);assert(fclose(f)==0);}

/* Exercise probe and correction settings without opening a window; return int status. */
int main(int argc,char **argv){
    assert(argc==2);assert(setenv("TYNGEN_PIXELPROCESS_RUNNER",argv[1],1)==0);
    char root[]="/tmp/tyngen-probes-XXXXXX";assert(mkdtemp(root));static App app;app.run_count=1;app.selected_run=0;AppRun *run=&app.runs[0];
    copy_text(run->dir,sizeof(run->dir),root);path_join(run->processing_settings_path,sizeof(run->processing_settings_path),root,"settings.json");path_join(run->source,sizeof(run->source),root,"source.png");
    write_probe_settings(run->source,"source");
    const char *json="{\"background_probes\":[],\"disabled_background_probes\":[],\"sprite_overrides\":[{\"id\":\"sprite-r000-c000\",\"x\":5,\"y\":7}],\"ownership_overrides\":{\"blob-4\":\"sprite-r000-c001\"}}";
    write_probe_settings(run->processing_settings_path,json);load_run_pixel_settings(run);
    assert(run->background_probe_count==0&&run->disabled_background_probe_count==0&&run->owner_override_count==1);
    assert(!strcmp(run->owner_components[0],"blob-4"));
    const char *pretty="{\"background_probes\": [\n {\"id\":\"manual-8-9\",\"x\":8,\"y\":9}\n ],\n\"disabled_background_probes\": [\"auto-r0-c1\"\n ],\n\"ownership_overrides\":{\"blob-4\":\"sprite-r000-c001\"},\"sprite_overrides\":[{\"id\":\"ignored\",\"x\":77,\"y\":88}]}";
    write_probe_settings(run->processing_settings_path,pretty);run->pixel_settings_loaded=false;load_run_pixel_settings(run);
    assert(run->background_probe_count==1&&run->disabled_background_probe_count==1&&run->owner_override_count==1);
    assert(run->background_probes[0].x==8&&!run->background_probes[0].automatic);
    char metadata[2048];snprintf(metadata,sizeof(metadata),"{\"probes\":[{\"id\":\"auto-r0-c0\",\"x\":10,\"y\":20,\"origin\":\"automatic\",\"region_bounds\":[0,0,30,40],\"extra\":\"%0300d\"}],\"other\":[{\"id\":\"wrong\",\"x\":99,\"y\":99}]}",0);
    AppBackgroundProbe probes[4];assert(parse_background_probes(metadata,"probes",probes,4)==1);assert(probes[0].automatic&&probes[0].x==10);
    run->generated.width=64;run->generated.height=32;
    /* agent: gpt-6 | 2026-10-09 | Exercise probe editor session boundary | v 1.1.0 | ea3c69 */
    char response[1024];GridEditState edit={0};edit.background=true;edit.run_index=0;app.grid_edit_state=&edit;AppDispatch(&app,"{\"action\":\"processing.probe.edit\",\"operation\":\"add\",\"x\":12,\"y\":13}",response,sizeof(response));assert(strstr(response,"true"));
    assert(run->background_probe_count==2);AppDispatch(&app,"{\"action\":\"processing.probe.edit\",\"operation\":\"add\",\"x\":64,\"y\":0}",response,sizeof(response));assert(strstr(response,"false")&&run->background_probe_count==2);AppDispatch(&app,"{\"action\":\"processing.probe.edit\",\"operation\":\"add\",\"x\":12,\"y\":13}",response,sizeof(response));assert(run->background_probe_count==2);
    AppDispatch(&app,"{\"action\":\"processing.probe.edit\",\"operation\":\"remove\",\"id\":\"manual-8-9\"}",response,sizeof(response));assert(run->background_probe_count==1);
    AppDispatch(&app,"{\"action\":\"processing.probe.edit\",\"operation\":\"remove\",\"id\":\"auto-r1-c1\"}",response,sizeof(response));assert(run->disabled_background_probe_count==2);
    edit.rows=3;edit.cols=4;edit.draft_settings=run->pixel_settings;
    edit.draft_settings.grid_rows=edit.rows;edit.draft_settings.grid_cols=edit.cols;edit.save_snapshot=setup_snapshot(&edit,run);
    assert(edit.save_snapshot.probe_count==1&&edit.save_snapshot.disabled_probe_count==2);
    run->background_probe_count=0;run->disabled_background_probe_count=0;
    grid_edit_restore_save_snapshot(run,&edit);
    assert(run->background_probe_count==1&&run->background_probes[0].x==12&&run->background_probes[0].y==13);
    assert(run->disabled_background_probe_count==2&&run->pixel_settings.grid_rows==3&&run->pixel_settings.grid_cols==4);
    run->pixel_settings.background_tolerance=31;run->pixel_settings.grid_cols=2;AppArtifact selected={0};selected.number=97;copy_text(selected.branch_id,sizeof(selected.branch_id),"primary");
    assert(request_branch_rerun(&app,run,&selected));assert(run->owner_override_count==1&&run->pixel_settings.grid_cols==2&&run->pixel_settings.background_tolerance==31);
    assert(write_pixel_settings_path(run,&run->pixel_settings,run->processing_settings_path));run->pixel_settings_loaded=false;load_run_pixel_settings(run);
    assert(run->background_probe_count==1&&run->background_probes[0].x==12&&run->disabled_background_probe_count==2&&run->owner_override_count==1);
    char manifest[APP_PATH_CAP],extra[APP_PATH_CAP],record[APP_TEXT_CAP];path_join(extra,sizeof(extra),root,"extra");assert(make_dirs(extra));path_join(manifest,sizeof(manifest),extra,"pixelprocess.manifest.json");
    write_probe_settings(run->processing_settings_path,"{\"background_probes\":[],\"ownership_overrides\":{\"blob-9\":\"sprite-r000-c001\"}}");snprintf(record,sizeof(record),"{\"selected_branch\":\"primary\",\"settings_path\":\"%s\"}",run->processing_settings_path);write_probe_settings(manifest,record);
    run->pixel_settings_dirty=true;reload_committed_pixel_settings(run);assert(!run->pixel_settings_dirty&&run->owner_override_count==1&&!strcmp(run->owner_components[0],"blob-9")&&run->background_probe_count==0);
    /* agent: GPT-6 | 2026-10-10 | Verify committed setup settings | v 1.3.0 | f752ec */
    GridEditState *committed=calloc(1,sizeof(*committed));assert(committed);committed->rows=4;committed->cols=7;committed->draft_settings=run->pixel_settings;committed->draft_settings.grid_rows=4;committed->draft_settings.grid_cols=7;committed->save_snapshot=setup_snapshot(committed,run);
    run->pixel_settings.grid_rows=0;run->pixel_settings.grid_cols=0;assert(!grid_edit_saved_snapshot_matches(run,committed));grid_edit_restore_save_snapshot(run,committed);assert(grid_edit_saved_snapshot_matches(run,committed));
    assert(write_pixel_settings_path(run,&run->pixel_settings,run->processing_settings_path));run->pixel_settings_loaded=false;load_run_pixel_settings(run);assert(grid_edit_saved_snapshot_matches(run,committed));
    GridEditState *finished=calloc(1,sizeof(*finished));assert(finished);finished->prior_run_settings.grid_rows=0;finished->prior_run_settings.grid_cols=0;app.grid_edit_state=finished;grid_edit_finish_save(&app,finished);assert(app.grid_edit_state==NULL&&run->pixel_settings.grid_rows==4&&run->pixel_settings.grid_cols==7);free(committed);
    GridEditState quant_edit={0};quant_edit.quantize=true;quant_edit.run_index=0;quant_edit.rows=4;quant_edit.cols=4;quant_edit.texture.id=1;quant_edit.baseline_settings=run->pixel_settings;quant_edit.draft_settings=run->pixel_settings;quant_edit.draft_settings.grid_rows=0;quant_edit.draft_settings.grid_cols=0;copy_text(quant_edit.draft_settings.palette_path,sizeof(quant_edit.draft_settings.palette_path),"test.palette");quant_edit.save_snapshot=setup_snapshot(&quant_edit,run);quant_edit.save_snapshot.settings=quant_edit.draft_settings;quant_edit.save_snapshot.rows=4;quant_edit.save_snapshot.cols=4;app.grid_edit_state=&quant_edit;
    grid_edit_restore_save_snapshot(run,&quant_edit);assert(run->pixel_settings.grid_rows==0&&run->pixel_settings.grid_cols==0);
    assert(!strcmp(grid_edit_save_target(&quant_edit),"palette_map"));quant_edit.draft_settings.sheet_colors++;assert(!strcmp(grid_edit_save_target(&quant_edit),"quantize"));assert(quant_edit.save_snapshot.settings.grid_rows==0&&quant_edit.save_snapshot.settings.grid_cols==0);
    assert(cleanup_remove_tree(root));puts("probe UI tests passed");return 0;
}
/* agent: GPT-6 | 2026-10-10 | Verify committed setup settings | v 1.3.0 | f752ec */
