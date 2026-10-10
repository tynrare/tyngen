/*
 * File version: 1.1.0
 * Last updated: 2026-10-10
 * Purpose: Share stage setting ownership, cache inputs, and reset order.
 * agent: GPT-6 | 2026-10-10 | Separate palette stage ownership | v 1.1.0 | 645990
 */
#ifndef PIXELPROCESS_STAGE_POLICY_H
#define PIXELPROCESS_STAGE_POLICY_H
#include <stddef.h>
#include <string.h>

typedef struct {
    const char *id;
    int progress;
    const char *owned_settings;
    const char *consumed_settings;
} PixelStagePolicy;

static const PixelStagePolicy PIXEL_STAGE_POLICIES[] = {
    {"background",97,"background_tolerance,background_probes,disabled_background_probes","background_tolerance,grid_cols,grid_rows,background_probes,disabled_background_probes"},
    {"separators",96,"","background_tolerance,grid_cols,grid_rows,background_probes,disabled_background_probes,separator_evidence_v2"},
    {"ownership",94,"grid_cols,grid_rows,ownership_overrides","background_tolerance,grid_cols,grid_rows,ownership_overrides,ownership_lanes_v2"},
    {"diagnosis",93,"","grid_cols,grid_rows,ownership_overrides"},
    {"extract",91,"extraction_padding,cleanup_mode","background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,ownership_overrides"},
    {"lattice",80,"pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,min_sprite_cells,sprite_pitch_tolerance,sprite_overrides","background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,min_sprite_cells,sprite_pitch_tolerance,pitch_selector_version_v3,sprite_overrides,ownership_overrides"},
    {"reconstruct",60,"foreground_coverage","background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,min_sprite_cells,sprite_pitch_tolerance,foreground_coverage,sprite_overrides,ownership_overrides,reconstruction_copy_v2"},
    {"cleanup_preview",24,"cleanup_tolerance,cleanup_background_support","cleanup_mode,cleanup_tolerance,cleanup_background_support"},
    {"cleanup",23,"","cleanup_mode,cleanup_tolerance,cleanup_background_support"},
    {"outline_reconstruct",22,"outline_mode,outline_foreground_support,outline_color_delta,outline_color","outline_mode,outline_foreground_support,outline_color_delta,outline_color,cleanup_tolerance"},
    {"assemble",4,"sheet_anchor,sheet_padding","background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,foreground_coverage,cleanup_tolerance,outline_mode,outline_foreground_support,outline_color_delta,outline_color,sprite_overrides,ownership_overrides,sheet_anchor,sheet_padding"},
    {"quantize",2,"sheet_colors,sprite_colors,sprite_color_source","sheet_colors,sprite_colors,sprite_color_source,exact_palette_v2"},
    {"palette_map",1,"palette_path","palette_path"}
};
#define PIXEL_STAGE_POLICY_COUNT (sizeof(PIXEL_STAGE_POLICIES)/sizeof(PIXEL_STAGE_POLICIES[0]))

/* Return the owning stage progress for const char * setting; return int or 0. */
static inline int pixel_policy_progress(const char *setting) {
    if(!setting||!*setting)return 0;
    for(size_t i=0;i<PIXEL_STAGE_POLICY_COUNT;i++){
        const char *list=PIXEL_STAGE_POLICIES[i].owned_settings,*at=list;
        while(*at){while(*at==',')at++;const char *end=at;while(*end&&*end!=',')end++;
            if((size_t)(end-at)==strlen(setting)&&!strncmp(at,setting,(size_t)(end-at)))return PIXEL_STAGE_POLICIES[i].progress;
            at=end;
        }
    }
    return 0;
}

#endif
