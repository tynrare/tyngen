/*
 * File version: 1.0.0
 * Last updated: 2026-10-07
 * Purpose: Verify run cleanup retention, backups, manifests, and pruning.
 */
/* agent: gpt-6.1-sol | 2026-10-07 | Verify cleanup retention and backup pruning | v 1.0.0 | 73b688 */
#include "../lib.c"
#include <assert.h>

/* Write text to path after creating its parent directories; return void. */
static void write_test_file(const char *path, const char *text) {
    char parent[APP_PATH_CAP]; copy_text(parent, sizeof(parent), path);
    char *slash = strrchr(parent, '/'); assert(slash); *slash = 0; assert(make_dirs(parent));
    FILE *file = fopen(path, "wb"); assert(file); assert(fwrite(text, 1, strlen(text), file) == strlen(text)); assert(fclose(file) == 0);
}

/* Count files under path recursively; return int. */
static int count_test_files(const char *path) {
    DIR *dir = opendir(path); if (!dir) return 0;
    int count = 0; struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[APP_PATH_CAP]; path_join(child, sizeof(child), path, entry->d_name); struct stat st;
        assert(lstat(child, &st) == 0);
        count += S_ISDIR(st.st_mode) ? count_test_files(child) : 1;
    }
    closedir(dir); return count;
}

/* Create one run source and return void. */
static void make_test_source(AppRun *run, const char *root, const char *id) {
    copy_text(run->id, sizeof(run->id), id); path_join(run->dir, sizeof(run->dir), root, id); assert(make_dirs(run->dir));
    char path[APP_PATH_CAP]; path_join(path, sizeof(path), run->dir, "source.png"); write_test_file(path, "source");
}

/* Verify Artifacts, Full, disabled backups, manifest inventory, and cap pruning; return int. */
int main(void) {
    char root[] = "/tmp/tyngen-cleanup-test-XXXXXX"; assert(mkdtemp(root));
    static App app, loaded; static AppRun artifacts_run, no_backup_run, oversized_run;
    copy_text(app.workspace, sizeof(app.workspace), root);
    path_join(app.cleanup_settings_path, sizeof(app.cleanup_settings_path), root, "cleanup.settings");
    app.cleanup_backups_enabled = false; app.cleanup_backup_limit = 2LL * 1024 * 1024 * 1024; assert(save_cleanup_preferences(&app));
    copy_text(loaded.cleanup_settings_path, sizeof(loaded.cleanup_settings_path), app.cleanup_settings_path); load_cleanup_preferences(&loaded);
    assert(!loaded.cleanup_backups_enabled && loaded.cleanup_backup_limit == 2LL * 1024 * 1024 * 1024);
    app.cleanup_backups_enabled = true; app.cleanup_backup_limit = 1024LL * 1024 * 1024;

    make_test_source(&artifacts_run, root, "run-artifacts");
    char path[APP_PATH_CAP];
    path_join(path, sizeof(path), artifacts_run.dir, "pixelprocess.settings.json"); write_test_file(path, "settings");
    path_join(path, sizeof(path), artifacts_run.dir, "process.log"); write_test_file(path, "log");
    path_join(path, sizeof(path), artifacts_run.dir, "extra/pixelprocess/revision-01/effective-settings.json"); write_test_file(path, "revision settings");
    path_join(path, sizeof(path), artifacts_run.dir, "extra/pixelprocess/pins/pin.json"); write_test_file(path, "pin");
    path_join(path, sizeof(path), artifacts_run.dir, "extra/pixelprocess/revision-01/stage-10-test.png"); write_test_file(path, "stage");
    assert(cleanup_run_files(&app, &artifacts_run, false) == 2);
    path_join(path, sizeof(path), artifacts_run.dir, "pixelprocess.settings.json"); assert(file_exists(path));
    path_join(path, sizeof(path), artifacts_run.dir, "extra/pixelprocess/revision-01/effective-settings.json"); assert(file_exists(path));
    path_join(path, sizeof(path), artifacts_run.dir, "extra/pixelprocess/pins/pin.json"); assert(file_exists(path));
    path_join(path, sizeof(path), artifacts_run.dir, "process.log"); assert(!file_exists(path));
    path_join(path, sizeof(path), root, "backups/manifest.json"); char manifest[8192]; assert(read_text(path, manifest, sizeof(manifest)));
    assert(strstr(manifest, "run-artifacts") && strstr(manifest, "process.log") && strstr(manifest, "\"mode\":\"artifacts\""));

    assert(cleanup_run_files(&app, &artifacts_run, true) == 3); assert(count_test_files(artifacts_run.dir) == 1);
    path_join(path, sizeof(path), artifacts_run.dir, "source.png"); assert(file_exists(path));

    app.cleanup_backups_enabled = false; make_test_source(&no_backup_run, root, "run-no-backup");
    path_join(path, sizeof(path), no_backup_run.dir, "note.txt"); write_test_file(path, "note");
    assert(cleanup_run_files(&app, &no_backup_run, true) == 1); assert(count_test_files(no_backup_run.dir) == 1);
    path_join(path, sizeof(path), root, "backups/manifest.json"); assert(read_text(path, manifest, sizeof(manifest)));
    assert(strstr(manifest, "run-no-backup") && strstr(manifest, "\"backed_up\":false"));

    app.cleanup_backups_enabled = true; app.cleanup_backup_limit = 64LL * 1024 * 1024;
    make_test_source(&oversized_run, root, "run-oversized"); path_join(path, sizeof(path), oversized_run.dir, "large.bin");
    FILE *large = fopen(path, "wb"); assert(large); assert(ftruncate(fileno(large), 65LL * 1024 * 1024) == 0); assert(fclose(large) == 0);
    assert(cleanup_run_files(&app, &oversized_run, true) == 1);
    path_join(path, sizeof(path), root, "backups/manifest.json"); assert(read_text(path, manifest, sizeof(manifest)));
    assert(strstr(manifest, "run-oversized") && !strstr(manifest, "run-artifacts") && !strstr(manifest, "run-no-backup"));
    assert(cleanup_remove_tree(root));
    puts("cleanup tests passed"); return 0;
}
/* agent: gpt-6.1-sol | 2026-10-07 | Verify cleanup retention and backup pruning | v 1.0.0 | 73b688 */
